"""
params.py — Dérivation complète des paramètres CAGOULE v1.5

Pipeline :
    Password + Salt
        │
        ▼
    KDF (Argon2id si dispo, sinon Scrypt) → K_master (512 bits)
        │
        ├── HKDF('N')     → n ∈ [4, 65536]
        ├── HKDF('P')     → p = nextprime(64 bits) [Phase 1]
        ├── generate_mu(p) → µ (Z/pZ ou Fp²)
        ├── HKDF('DELTA') → δ → SBox.from_delta(δ, p)
        └── HKDF('ENC')   → K_stream (256 bits)

Requis par : cipher.py, decipher.py
"""

from __future__ import annotations

import os
import struct
from typing import Optional

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives.kdf.scrypt import Scrypt

from .fp2 import Fp2Element
from .mu import generate_mu, MuResult
from .sbox import SBox
from .matrix import DiffusionMatrix
from .logger import get_logger
_log = get_logger(__name__)


# ------------------------------------------------------------------ #
#  Cache pour benchmark (opt-in uniquement)                           #
# ------------------------------------------------------------------ #

# ATTENTION : Ce cache garde les paramètres en mémoire non zéroïsés.
#            À utiliser UNIQUEMENT pour les benchmarks et tests.
# NOTE: Pas de threading.Lock pour rester picklable (compatible ProcessPoolExecutor)
_BENCHMARK_CACHE: dict = {}


# ------------------------------------------------------------------ #
#  Constantes Phase 1                                                  #
# ------------------------------------------------------------------ #

SALT_SIZE      = 32      # octets — sel Argon2id/Scrypt
K_MASTER_SIZE  = 64      # octets — 512 bits
K_STREAM_SIZE  = 32      # octets — 256 bits
P_SEED_BYTES   = 8       # octets — graine pour nextprime (→ p ≈ 2⁶⁴)
BLOCK_SIZE_N   = 16      # taille de bloc interne FIXE Phase 1 (16 éléments)
NUM_ROUND_KEYS = 64      # nombre de round keys

# Paramètres Scrypt par défaut
# Production : n=2**17, r=8 (~128 MB, ~1s)
# Tests       : n=2**14, r=8 (~16 MB, rapide)
_SCRYPT_N_PROD = 2 ** 17
_SCRYPT_N_TEST = 2 ** 14


# ------------------------------------------------------------------ #
#  KDF principal (Argon2id si dispo, Scrypt sinon)                     #
# ------------------------------------------------------------------ #

def _kdf_argon2id(password: bytes, salt: bytes) -> bytes:
    """Argon2id (t=3, m=64MB, p=1) → K_master 512 bits."""
    from argon2.low_level import hash_secret_raw, Type
    return hash_secret_raw(
        secret=password,
        salt=salt,
        time_cost=3,
        memory_cost=65536,     # 64 MB
        parallelism=1,
        hash_len=K_MASTER_SIZE,
        type=Type.ID,
    )


def _kdf_scrypt(password: bytes, salt: bytes,
                scrypt_n: int = _SCRYPT_N_PROD) -> bytes:
    """
    Scrypt → K_master 512 bits. Fallback si argon2-cffi absent.
    Production : n=2^17 (~128 MB). Tests : n=2^14 (~16 MB).
    """
    kdf = Scrypt(salt=salt, length=K_MASTER_SIZE, n=scrypt_n, r=8, p=1)
    return kdf.derive(password)


def derive_k_master(password: bytes, salt: bytes,
                    fast_mode: bool = False) -> bytes:
    """
    Dérive K_master (512 bits) depuis password + salt.
    Utilise Argon2id si disponible, sinon Scrypt.
    fast_mode=True réduit la mémoire (tests uniquement).
    """
    if len(salt) != SALT_SIZE:
        raise ValueError(f"Salt doit faire {SALT_SIZE} octets, reçu {len(salt)}")
    try:
        return _kdf_argon2id(password, salt)
    except ImportError:
        n = _SCRYPT_N_TEST if fast_mode else _SCRYPT_N_PROD
        return _kdf_scrypt(password, salt, scrypt_n=n)


# ------------------------------------------------------------------ #
#  HKDF-SHA256 helper                                                  #
# ------------------------------------------------------------------ #

def hkdf_derive(key_material: bytes, info: bytes, length: int) -> bytes:
    """
    HKDF-SHA256 (RFC 5869).

    key_material : matériau source (K_master ou autre)
    info         : contexte de dérivation (ex: b'CAGOULE_N')
    length       : nombre d'octets à produire
    """
    hkdf = HKDF(
        algorithm=hashes.SHA256(),
        length=length,
        salt=None,
        info=info,
    )
    return hkdf.derive(key_material)


def hkdf_int(key_material: bytes, info: bytes, length: int) -> int:
    """HKDF-SHA256 → entier (big-endian)."""
    raw = hkdf_derive(key_material, info, length)
    return int.from_bytes(raw, 'big')


# ------------------------------------------------------------------ #
#  Nombres premiers (Miller-Rabin)                                     #
# ------------------------------------------------------------------ #

def _is_prime_miller_rabin(n: int) -> bool:
    """
    Test de primalité Miller-Rabin déterministe pour n < 3.3 × 10^24.
    Témoins suffisants pour tous les entiers < 3.3 × 10^24.
    """
    if n < 2:
        return False
    if n in (2, 3, 5, 7, 11, 13):
        return True
    if n % 2 == 0:
        return False

    # n-1 = 2^r × d
    r, d = 0, n - 1
    while d % 2 == 0:
        r += 1
        d //= 2

    # Témoins déterministes (suffisants pour n < 3.3×10^24)
    witnesses = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37]

    for a in witnesses:
        if a >= n:
            continue
        x = pow(a, d, n)
        if x == 1 or x == n - 1:
            continue
        for _ in range(r - 1):
            x = x * x % n
            if x == n - 1:
                break
        else:
            return False
    return True


def nextprime(n: int) -> int:
    """Retourne le plus petit premier ≥ n."""
    if n <= 2:
        return 2
    # Rendre impair
    candidate = n if n % 2 != 0 else n + 1
    while not _is_prime_miller_rabin(candidate):
        candidate += 2
    return candidate


# ------------------------------------------------------------------ #
#  Classe CagouleParams — tous les paramètres d'une session           #
# ------------------------------------------------------------------ #

class CagouleParams:
    """
    Paramètres CAGOULE dérivés d'un mot de passe + sel.

    Attributs :
        salt       : sel aléatoire (SALT_SIZE octets)
        k_master   : clé maître (K_MASTER_SIZE octets)
        n          : taille de bloc ∈ [4, 65536]
        p          : nombre premier de travail (≈ 2⁶⁴ en Phase 1)
        p_bytes    : nombre d'octets pour sérialiser un élément de Z/pZ
        mu         : µ (MuResult)
        sbox       : S-box (SBox)
        diffusion  : matrice de diffusion (DiffusionMatrix)
        k_stream   : clé ChaCha20 (K_STREAM_SIZE octets)
        round_keys : liste de NUM_ROUND_KEYS valeurs entières mod p
    """

    def __init__(self) -> None:
        self.salt:       bytes          = b''
        self.k_master:   bytes          = b''
        self.n:          int            = BLOCK_SIZE_N
        self.p:          int            = 0
        self.p_bytes:    int            = 8
        self.mu:         MuResult | None = None
        self.sbox:       SBox | None    = None
        self.diffusion:  DiffusionMatrix | None = None
        self.k_stream:   bytes          = b''
        self.round_keys: list[int]      = []

    @classmethod
    def derive(cls, password: bytes | str, salt: bytes | None = None,
               timeout_mu: float = 5.0,
               fast_mode: bool = False) -> CagouleParams:
        """
        Dérive tous les paramètres CAGOULE depuis password + salt.

        password  : mot de passe utilisateur (str ou bytes)
        salt      : sel 32 octets (None = généré aléatoirement)
        timeout_mu: timeout en secondes pour la stratégie A de generate_mu
        fast_mode : True = paramètres KDF réduits pour les tests
        """
        if isinstance(password, str):
            password = password.encode('utf-8')
        if salt is None:
            salt = os.urandom(SALT_SIZE)
        if len(salt) != SALT_SIZE:
            raise ValueError(f"Salt doit faire {SALT_SIZE} octets")

        params = cls()
        params.salt = salt

        # ── K_master ─────────────────────────────────────────────── #
        params.k_master = derive_k_master(password, salt, fast_mode=fast_mode)
        _log.debug("K_master dérivé (%d octets, fast_mode=%s)", len(params.k_master), fast_mode)

        # ── n_zeta ∈ [4, 65536] : paramètre de ζ(2n) ────────────── #
        # NOTE : n_zeta est distinct du BLOCK_SIZE_N=16 (taille de bloc fixe)
        n_raw = hkdf_int(params.k_master, b'CAGOULE_N', 2)
        params.n = (n_raw % (65536 - 4 + 1)) + 4   # [4, 65536] — pour ζ(2n)

        # ── p = nextprime(64 bits) — Phase 1 ─────────────────────── #
        p_seed = hkdf_int(params.k_master, b'CAGOULE_P', P_SEED_BYTES)
        # Forcer p_seed dans [2^63, 2^64) pour avoir p ≈ 2^64
        p_seed = p_seed | (1 << 63)
        params.p = nextprime(p_seed)
        params.p_bytes = (params.p.bit_length() + 7) // 8
        _log.debug("p = %d (%d bits)", params.p, params.p.bit_length())
        _log.debug("n_zeta = %d", params.n)

        # ── µ (Stratégie A→C) ─────────────────────────────────────── #
        params.mu = generate_mu(params.p, timeout_s=timeout_mu)
        _log.info("µ — stratégie %s (dans_Fp2=%s)", params.mu.strategy, params.mu.in_fp2)

        # ── S-box (δ → c) ────────────────────────────────────────── #
        delta = hkdf_int(params.k_master, b'CAGOULE_DELTA', 8) % params.p
        params.sbox = SBox.from_delta(delta, params.p)

        # ── Matrice de diffusion (nœuds Vandermonde) ─────────────── #
        nodes = _derive_vandermonde_nodes(
            params.k_master, params.mu, BLOCK_SIZE_N, params.p  # 16x16 matrice
        )
        params.diffusion = DiffusionMatrix.from_nodes(nodes, params.p)

        # ── K_stream — clé ChaCha20 ───────────────────────────────── #
        params.k_stream = hkdf_derive(params.k_master, b'CAGOULE_ENC', K_STREAM_SIZE)

        # ── Round keys ────────────────────────────────────────────── #
        from .omega import generate_round_keys
        params.round_keys = generate_round_keys(params.n, params.salt, params.p)

        return params

    @classmethod
    def derive_for_benchmark(
        cls,
        password: bytes,
        fast_mode: bool = False,
        salt: Optional[bytes] = None,
    ) -> "CagouleParams":
        """
        Version benchmark avec cache optionnel.
        À utiliser UNIQUEMENT pour les tests de performance.
        
        Args:
            password: Mot de passe utilisateur
            fast_mode: Si True, utilise Scrypt au lieu d'Argon2id
            salt: Sel fixe (optionnel). S'il n'est pas fourni, un sel fixe
                  par défaut est utilisé pour la reproductibilité.
        
        Returns:
            Paramètres CAGOULE dérivés
        
        Warning:
            Ce cache garde les paramètres en mémoire. Ne pas utiliser en production.
            Les clés ne sont pas automatiquement zéroïsées.
            
        Note:
            Ce cache n'utilise pas de verrou thread-safe pour rester picklable
            avec ProcessPoolExecutor. Pour une utilisation multi-thread,
            préférer derive() sans cache.
        """
        # Sel fixe par défaut pour reproductibilité
        if salt is None:
            salt = b'\xca\xf0' * 16  # 32 octets fixes
        
        cache_key = (password, fast_mode, salt)
        
        # Pas de verrou ici pour rester picklable
        # Le cache est simplement un dictionnaire, l'utilisateur doit garantir
        # qu'il n'y a pas d'accès concurrents problématiques.
        if cache_key not in _BENCHMARK_CACHE:
            _BENCHMARK_CACHE[cache_key] = cls.derive(
                password, salt=salt, fast_mode=fast_mode
            )
        return _BENCHMARK_CACHE[cache_key]

    @classmethod
    def clear_benchmark_cache(cls) -> None:
        """
        Vide et zéroïse le cache de benchmark.
        Appeler à la fin des benchmarks pour libérer la mémoire.
        """
        for params in _BENCHMARK_CACHE.values():
            if hasattr(params, 'zeroize'):
                params.zeroize()
        _BENCHMARK_CACHE.clear()

    def zeroize(self) -> None:
        """
        Écrase toutes les données cryptographiques sensibles en mémoire.

        À appeler après usage pour minimiser la surface d'exposition.
        Note : en Python, le GC peut avoir copié ces valeurs — best-effort.
        """
        from .utils import secure_zeroize
        if self.k_master:
            _buf = bytearray(self.k_master)
            secure_zeroize(_buf)
            self.k_master = b""
        if self.k_stream:
            _buf = bytearray(self.k_stream)
            secure_zeroize(_buf)
            self.k_stream = b""
        if self.round_keys:
            for i in range(len(self.round_keys)):
                self.round_keys[i] = 0
            self.round_keys = []
        self.sbox = None
        self.diffusion = None
        self.mu = None

    def __enter__(self) -> "CagouleParams":
        return self

    def __exit__(self, *args) -> None:
        self.zeroize()

    def __reduce__(self):
        """
        Permet la sérialisation pickle pour ProcessPoolExecutor.
        Évite de sérialiser les objets non picklables (SBox, DiffusionMatrix, etc.)
        en les recréant à partir des attributs essentiels.
        """
        # Créer un tuple des attributs essentiels pour la reconstruction
        args = (
            self.salt,
            self.k_master,
            self.n,
            self.p,
            self.p_bytes,
            self.mu,
            # sbox, diffusion, k_stream, round_keys seront recréés
        )
        return (self._reconstruct, args)

    @classmethod
    def _reconstruct(cls, salt, k_master, n, p, p_bytes, mu):
        """Reconstruit un objet CagouleParams après désérialisation."""
        params = cls()
        params.salt = salt
        params.k_master = k_master
        params.n = n
        params.p = p
        params.p_bytes = p_bytes
        params.mu = mu
        
        # Recréer les objets dérivés
        delta = hkdf_int(k_master, b'CAGOULE_DELTA', 8) % p
        params.sbox = SBox.from_delta(delta, p)
        
        nodes = _derive_vandermonde_nodes(k_master, mu, BLOCK_SIZE_N, p)
        params.diffusion = DiffusionMatrix.from_nodes(nodes, p)
        
        params.k_stream = hkdf_derive(k_master, b'CAGOULE_ENC', K_STREAM_SIZE)
        
        from .omega import generate_round_keys
        params.round_keys = generate_round_keys(n, salt, p)
        
        return params

    def __repr__(self) -> str:
        return (
            f"CagouleParams(n={self.n}, p={self.p}, "
            f"mu_strategy={self.mu.strategy!r}, "
            f"sbox={self.sbox!r}, "
            f"diffusion={self.diffusion!r})"
        )


# ------------------------------------------------------------------ #
#  Génération des nœuds Vandermonde                                    #
# ------------------------------------------------------------------ #

def _derive_vandermonde_nodes(k_master: bytes, mu: MuResult,
                               n: int, p: int) -> list[int]:
    """
    Dérive N nœuds distincts pour la matrice de Vandermonde.

    α₀ = µ mod p (ou µ.a si µ ∈ Fp²)
    αᵢ = HKDF(K_master, b'NODE_i', 8) % p  pour i=1..N-1

    Garantit la distinction par incrémentation en cas de collision.
    """
    if mu.in_fp2:
        alpha0 = mu.mu.a % p
    else:
        alpha0 = int(mu.mu) % p

    nodes = [alpha0]
    seen = {alpha0}

    for i in range(1, n):
        info = f"CAGOULE_NODE_{i}".encode()
        raw = hkdf_int(k_master, info, 8) % p
        # Garantir la distinction
        while raw in seen:
            raw = (raw + 1) % p
        nodes.append(raw)
        seen.add(raw)

    return nodes