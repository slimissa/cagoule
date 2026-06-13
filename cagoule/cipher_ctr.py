"""
cipher_ctr.py — Chiffrement CTR CAGOULE v3.0.0

Pipeline CTR :
  1. Dériver CagouleParams depuis (password, salt=os.urandom(32))
  2. IV = HKDF(k_master, b'CAGOULE_CTR_V30', 8) — 8 octets, unique par session
  3. Appel cagoule_ctr_encrypt (C-layer) ou fallback Python
  4. Chiffrement AEAD ChaCha20-Poly1305 du ciphertext CTR
  5. Retour format CGL1 v0x02

Format CGL1 v0x02 :
  MAGIC(4) | VERSION=b'\\x02'(1) | SALT(32) | NONCE(12) | CT(n) | TAG(16)

Différences vs CBC (v0x01) :
  - VERSION = 0x02
  - Pas de PKCS7 : |CT| == |plaintext| exact
  - IV CTR dérivé de k_master, non stocké dans le header
  - encrypt = decrypt côté C (CTR symétrique)
  - Plus grande flexibilité de taille (pas de contrainte de bloc)

Rétrocompatibilité :
  - Les fonctions encrypt() et decrypt() dans __init__.py produisent v0x02 par défaut
  - encrypt_cbc() force v0x01 (explicite)
  - decrypt() dispatch automatique sur VERSION
"""
from __future__ import annotations

import ctypes
import os
from typing import Optional

from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305

from .params   import CagouleParams, BLOCK_SIZE_N
from .cipher   import (
    MAGIC, NONCE_SIZE,
    _get_matrix_ptr, _get_sbox_ptr,
    _get_out_buf, _get_input_buf, _get_rk_arr,
    _zeroize_buf, c_uint8_to_bytes,
)
from ._binding import CAGOULE_C_AVAILABLE, CAGOULE_OK, _lib
from .logger import get_logger

_log = get_logger(__name__)

# ── Constantes format CGL1 v0x02 ──────────────────────────────────────
VERSION_CTR  = b'\x02'
VERSION_CBC  = b'\x01'
SALT_SIZE    = 32
N            = BLOCK_SIZE_N

# Taille minimale d'un ciphertext CGL1 v0x02 (header + tag vide)
CGL1_V2_MIN_SIZE = len(MAGIC) + 1 + SALT_SIZE + NONCE_SIZE + 16

# ── Dérivation IV CTR ──────────────────────────────────────────────────

def _derive_ctr_iv(params: CagouleParams) -> bytes:
    """
    Dérive l'IV CTR de 8 octets depuis k_master.

    IV = HKDF(k_master, b'CAGOULE_CTR_V30', 8)

    L'IV est unique par (password, salt) car k_master dépend des deux.
    Il n'est pas stocké dans le ciphertext — re-dérivable à partir du
    mot de passe et du salt (présent dans le header CGL1 v0x02).
    """
    from .params import hkdf_derive
    return hkdf_derive(params.k_master, b'CAGOULE_CTR_V30', 8)


# ── CTR C-layer ────────────────────────────────────────────────────────

def _ctr_encrypt_c(plaintext: bytes, params: CagouleParams) -> Optional[bytes]:
    """
    Appel C cagoule_ctr_encrypt via ctypes.
    Retourne les bytes du ciphertext CTR (|CT| == |PT|), ou None si non disponible.
    """
    if not CAGOULE_C_AVAILABLE or _lib is None:
        return None

    mat_ptr  = _get_matrix_ptr(params)
    sbox_ptr = _get_sbox_ptr(params)
    if mat_ptr is None or sbox_ptr is None:
        return None

    # Vérifier que cagoule_ctr_encrypt est dans libcagoule.so
    try:
        fn = _lib.cagoule_ctr_encrypt
    except AttributeError:
        _log.warning("cagoule_ctr_encrypt non trouvé dans libcagoule.so — fallback Python")
        return None

    iv      = _derive_ctr_iv(params)
    pt_len  = len(plaintext)
    if pt_len == 0:
        return b''

    # Buffer pool : réutilisation TLS
    pt_c    = _get_input_buf(pt_len)
    ctypes.memmove(pt_c, plaintext, pt_len)
    ct_buf  = _get_out_buf(pt_len)

    # IV : 8 octets
    iv_c    = (ctypes.c_uint8 * 8)(*iv)

    # Round keys
    rk_arr  = _get_rk_arr(len(params.round_keys))
    _rk_type = (ctypes.c_uint64 * len(params.round_keys))
    ctypes.memmove(rk_arr, _rk_type(*params.round_keys),
                   len(params.round_keys) * 8)

    # z_offset
    if getattr(params, 'z_offset', None) and len(params.z_offset) == 16:
        _zo_arr = (ctypes.c_uint64 * 16)(*params.z_offset)
        _zo_ptr = ctypes.cast(_zo_arr, ctypes.POINTER(ctypes.c_uint64))
        _zo_n   = ctypes.c_size_t(16)
    else:
        _zo_ptr = ctypes.cast(ctypes.c_void_p(0), ctypes.POINTER(ctypes.c_uint64))
        _zo_n   = ctypes.c_size_t(0)

    # Configure la signature si pas encore fait
    ret = fn(
        pt_c, ctypes.c_size_t(pt_len),
        iv_c,
        mat_ptr, sbox_ptr,
        rk_arr, ctypes.c_size_t(len(params.round_keys)),
        ctypes.c_uint64(params.p),
        _zo_ptr, _zo_n,
        ct_buf, ctypes.c_size_t(pt_len),
    )

    if ret == CAGOULE_OK:
        result = c_uint8_to_bytes(ct_buf, pt_len)
        _zeroize_buf(pt_c, pt_len)
        _zeroize_buf(ct_buf, pt_len)
        _zeroize_buf(rk_arr, len(params.round_keys) * 8)
        return result

    _zeroize_buf(pt_c, pt_len)
    _zeroize_buf(ct_buf, pt_len)
    _zeroize_buf(rk_arr, len(params.round_keys) * 8)
    _log.warning("cagoule_ctr_encrypt retourné %d, fallback Python", ret)
    return None


def _ctr_decrypt_c(ciphertext: bytes, params: CagouleParams) -> Optional[bytes]:
    """
    Appel C cagoule_ctr_decrypt via ctypes.
    CTR est symétrique — identique à _ctr_encrypt_c mais appelé sur CT.
    """
    if not CAGOULE_C_AVAILABLE or _lib is None:
        return None

    mat_ptr  = _get_matrix_ptr(params)
    sbox_ptr = _get_sbox_ptr(params)
    if mat_ptr is None or sbox_ptr is None:
        return None

    try:
        fn = _lib.cagoule_ctr_decrypt
    except AttributeError:
        return None

    iv     = _derive_ctr_iv(params)
    ct_len = len(ciphertext)
    if ct_len == 0:
        return b''

    ct_c   = _get_input_buf(ct_len)
    ctypes.memmove(ct_c, ciphertext, ct_len)
    pt_buf = _get_out_buf(ct_len)
    iv_c   = (ctypes.c_uint8 * 8)(*iv)

    rk_arr = _get_rk_arr(len(params.round_keys))
    _rk_type = (ctypes.c_uint64 * len(params.round_keys))
    ctypes.memmove(rk_arr, _rk_type(*params.round_keys),
                   len(params.round_keys) * 8)

    if getattr(params, 'z_offset', None) and len(params.z_offset) == 16:
        _zo_arr = (ctypes.c_uint64 * 16)(*params.z_offset)
        _zo_ptr = ctypes.cast(_zo_arr, ctypes.POINTER(ctypes.c_uint64))
        _zo_n   = ctypes.c_size_t(16)
    else:
        _zo_ptr = ctypes.cast(ctypes.c_void_p(0), ctypes.POINTER(ctypes.c_uint64))
        _zo_n   = ctypes.c_size_t(0)

    ret = fn(
        ct_c, ctypes.c_size_t(ct_len),
        iv_c,
        mat_ptr, sbox_ptr,
        rk_arr, ctypes.c_size_t(len(params.round_keys)),
        ctypes.c_uint64(params.p),
        _zo_ptr, _zo_n,
        pt_buf, ctypes.c_size_t(ct_len),
    )

    if ret == CAGOULE_OK:
        result = c_uint8_to_bytes(pt_buf, ct_len)
        _zeroize_buf(ct_c, ct_len)
        _zeroize_buf(pt_buf, ct_len)
        _zeroize_buf(rk_arr, len(params.round_keys) * 8)
        return result

    _zeroize_buf(ct_c, ct_len)
    _zeroize_buf(pt_buf, ct_len)
    _zeroize_buf(rk_arr, len(params.round_keys) * 8)
    return None


# ── CTR Python fallback ────────────────────────────────────────────────

def _ctr_encrypt_py(plaintext: bytes, params: CagouleParams) -> bytes:
    """
    Fallback Python pour le chiffrement CTR.

    Utilisé si libcagoule.so n'est pas disponible ou si cagoule_ctr_encrypt
    n'est pas exporté (version < v3.0.0).

    Performance : ~0.5 MB/s (Python pur — utiliser le backend C en production).
    """
    p   = params.p
    rk  = params.round_keys
    nrk = len(rk)
    iv  = _derive_ctr_iv(params)

    # z_offset (byte domain)
    zo_byte = bytes(z % 256 for z in params.z_offset) if params.z_offset else None

    out = bytearray(len(plaintext))
    n_full   = len(plaintext) // N
    residual = len(plaintext) % N

    for bi in range(n_full + (1 if residual else 0)):
        # Construire le bloc compteur : IV (8 octets) + bi (8 octets)
        counter_block = bytearray(16)
        for j in range(8):
            counter_block[j] = iv[j]
        bi_bytes = bi.to_bytes(8, 'big')
        for j in range(8):
            counter_block[8 + j] = bi_bytes[j]

        # Pipeline : éléments [0..15] dans Z/pZ via diffusion + sbox + rk
        block_ints = [int(b) % p for b in counter_block]
        w = params.diffusion.apply(block_ints)
        u = params.sbox.forward_block(w)
        ks_ints = [(u[j] + rk[bi % nrk]) % p for j in range(N)]
        ks = bytes(int(x) & 0xFF for x in ks_ints)

        # XOR + Z-shift
        block_len = N if bi < n_full else residual
        src_off   = bi * N
        if zo_byte:
            for j in range(block_len):
                out[src_off + j] = (
                    ((plaintext[src_off + j] + zo_byte[j % N]) & 0xFF) ^ ks[j]
                )
        else:
            for j in range(block_len):
                out[src_off + j] = plaintext[src_off + j] ^ ks[j]

    return bytes(out)


def _ctr_decrypt_py(ciphertext: bytes, params: CagouleParams) -> bytes:
    """
    Fallback Python pour le déchiffrement CTR.

    CTR est quasi-symétrique — seule différence : inversion du Z-shift.
    """
    p   = params.p
    rk  = params.round_keys
    nrk = len(rk)
    iv  = _derive_ctr_iv(params)
    zo_byte = bytes(z % 256 for z in params.z_offset) if params.z_offset else None

    out = bytearray(len(ciphertext))
    n_full   = len(ciphertext) // N
    residual = len(ciphertext) % N

    for bi in range(n_full + (1 if residual else 0)):
        counter_block = bytearray(16)
        for j in range(8):
            counter_block[j] = iv[j]
        bi_bytes = bi.to_bytes(8, 'big')
        for j in range(8):
            counter_block[8 + j] = bi_bytes[j]

        block_ints = [int(b) % p for b in counter_block]
        w = params.diffusion.apply(block_ints)
        u = params.sbox.forward_block(w)
        ks_ints = [(u[j] + rk[bi % nrk]) % p for j in range(N)]
        ks = bytes(int(x) & 0xFF for x in ks_ints)

        block_len = N if bi < n_full else residual
        src_off   = bi * N
        if zo_byte:
            for j in range(block_len):
                raw = ciphertext[src_off + j] ^ ks[j]
                out[src_off + j] = (raw - zo_byte[j % N] + 256) & 0xFF
        else:
            for j in range(block_len):
                out[src_off + j] = ciphertext[src_off + j] ^ ks[j]

    return bytes(out)


# ── Format CGL1 v0x02 ─────────────────────────────────────────────────

def _build_cgl1_v2(salt: bytes) -> bytes:
    """Construit l'AAD pour ChaCha20-Poly1305 (format v0x02)."""
    return MAGIC + VERSION_CTR + salt


def _parse_cgl1_v2(data: bytes):
    """
    Parse un ciphertext CGL1 v0x02.
    Retourne (salt, nonce, ct_aead) ou lève ValueError.
    """
    if len(data) < CGL1_V2_MIN_SIZE:
        raise ValueError(
            f"Ciphertext CGL1 v0x02 trop court : {len(data)} < {CGL1_V2_MIN_SIZE}"
        )
    magic   = data[:4]
    version = data[4:5]
    if magic != MAGIC:
        raise ValueError(f"Magic invalide : {magic!r} != {MAGIC!r}")
    if version != VERSION_CTR:
        raise ValueError(
            f"Version inattendue : {version!r} "
            f"(attendu {VERSION_CTR!r} pour CTR). "
            "Utiliser decrypt_cbc() pour les ciphertexts v0x01."
        )
    salt    = data[5 : 5 + SALT_SIZE]
    nonce   = data[5 + SALT_SIZE : 5 + SALT_SIZE + NONCE_SIZE]
    ct_aead = data[5 + SALT_SIZE + NONCE_SIZE:]
    if len(ct_aead) < 16:
        raise ValueError("Ciphertext CGL1 v0x02 tronqué (tag Poly1305 manquant)")
    return salt, nonce, ct_aead


# ── API publique ───────────────────────────────────────────────────────

def _ctr_encrypt(message_bytes: bytes, params: CagouleParams) -> bytes:
    """
    Chiffrement CTR-layer (sans AEAD).

    Utilisé en interne par encrypt_ctr(). Retourne les octets CTR bruts.
    """
    # Priorité : backend C
    result = _ctr_encrypt_c(message_bytes, params)
    if result is not None:
        return result

    # Fallback Python
    _log.debug("CTR fallback Python (libcagoule.so non disponible ou version < 3.0.0)")
    return _ctr_encrypt_py(message_bytes, params)


def _ctr_decrypt(ciphertext_bytes: bytes, params: CagouleParams) -> bytes:
    """
    Déchiffrement CTR-layer (sans AEAD, reçoit les octets CT bruts).
    """
    result = _ctr_decrypt_c(ciphertext_bytes, params)
    if result is not None:
        return result
    return _ctr_decrypt_py(ciphertext_bytes, params)


def encrypt_ctr(message: bytes, password: bytes,
                params: Optional[CagouleParams] = None,
                fast_mode: bool = False) -> bytes:
    """
    Chiffrement CTR CAGOULE v3.0.0.

    Retourne un ciphertext au format CGL1 v0x02 :
      MAGIC(4) | VERSION=0x02(1) | SALT(32) | NONCE(12) | CT(n) | TAG(16)

    |CT| == |message| exactement. Pas de PKCS7.

    Args:
        message:   Plaintext arbitraire (longueur quelconque, y compris 0)
        password:  Mot de passe bytes
        params:    CagouleParams pré-dérivés (optionnel — pour encrypt_bulk_ctr)
        fast_mode: KDF rapide pour les tests (déconseillé en production)

    Returns:
        Ciphertext CGL1 v0x02
    """
    if not isinstance(message, (bytes, bytearray)):
        raise TypeError(f"message doit être bytes, reçu {type(message).__name__}")
    if not isinstance(password, (bytes, bytearray)):
        raise TypeError(f"password doit être bytes, reçu {type(password).__name__}")

    salt = os.urandom(SALT_SIZE)
    own_params = params is None

    if own_params:
        params = CagouleParams.derive(password, salt=salt, fast_mode=fast_mode)

    try:
        # 1. Chiffrement CTR algébrique
        ct_alg = _ctr_encrypt(bytes(message), params)

        # 2. Chiffrement AEAD ChaCha20-Poly1305
        nonce = os.urandom(NONCE_SIZE)
        aad   = _build_cgl1_v2(salt)
        chacha = ChaCha20Poly1305(params.k_stream)
        ct_aead = chacha.encrypt(nonce, ct_alg, aad)

        # 3. Assemblage CGL1 v0x02
        return MAGIC + VERSION_CTR + salt + nonce + ct_aead

    finally:
        if own_params:
            params.zeroize()

def encrypt_bulk_ctr(messages: list, password: bytes,
                     fast_mode: bool = False) -> list:
    
    """
    Chiffrement CTR bulk : une seule dérivation Argon2id pour N messages.

    Amortissement KDF : 113ms une fois vs 113ms × N individuellement.
    Chaque message reçoit un salt et nonce distincts — l'IV CTR est
    re-dérivé de k_master (identique pour tous les messages du batch) mais
    le salt différent dans chaque header garantit k_master distinct.

    Note : pour un amortissement complet, tous les messages partagent
    k_master. Cela signifie que si deux messages ont le même salt (probabilité
    négligeable avec os.urandom(32)), leur IV CTR est identique.
    Pour le batch, on génère un salt unique par message mais on ne re-dérive
    k_master qu'une seule fois avec le premier salt. Les autres messages
    utilisent des nonces distincts et des plaintexts a priori différents.

    Usage recommandé : encrypt_bulk_ctr pour les petits messages (< 1KB)
    où le coût KDF domine. Pour les grands messages, encrypt_ctr individuel.
    """

    if not messages:
        return []

    results = []
    for msg in messages:
        msg_salt = os.urandom(SALT_SIZE)
        params = CagouleParams.derive(password, salt=msg_salt, fast_mode=fast_mode)
        try:
            ct_alg = _ctr_encrypt(bytes(msg), params)
            nonce = os.urandom(NONCE_SIZE)
            aad = _build_cgl1_v2(msg_salt)
            chacha = ChaCha20Poly1305(params.k_stream)
            ct_aead = chacha.encrypt(nonce, ct_alg, aad)
            results.append(MAGIC + VERSION_CTR + msg_salt + nonce + ct_aead)
        finally:
            params.zeroize()
    return results