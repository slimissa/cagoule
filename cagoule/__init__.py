"""
CAGOULE v2.3.0 — Cryptographie Algébrique Géométrique par Ondes et Logique Entrelacée

Système de chiffrement symétrique hybride.

Changements v2.3.0 :
  - cagoule_sbox_avx2.c : S-box Feistel vectorisée AVX2 (4 éléments simultanés)
  - cagoule_cipher.c : round-key add/sub via addmod64x4/submod64x4 (P2)
  - Boucle chaude : _sbox_block_forward_hot_avx2 (broadcasts hoistés, 0 zeroupper)
  - Correction endianness AVX2 vs scalaire (_bswap64x4 dans store/load)
  - 560/560 tests pytest (CI déterministe, ALPHA_CHI2 sur tous les tests chi²)
  - get_backend_info_v230() exposant 'sbox_backend'

Changements v2.2.0 :
  - cagoule_matrix_avx2.c : multiplication Vandermonde vectorisée AVX2 (4 lignes)
  - cagoule_math_avx2.h : mulmod64x4/addmod64x4/submod64x4 via Barrett SIMD
  - Dispatch runtime AVX2 avec fallback scalaire automatique
  - backend_info exposé : matrice (avx2/scalaire), omega (C/mpmath)

Changements v2.1.0 :
  - omega.c : portage C de ζ(2n) → round keys (suppression mpmath en production)
  - Fix test_mauvais_mdp : mauvais mot de passe détecté même avec params= fourni
  - CagouleAuthError enrichi : .reason, .hint, .ct_size, .backend
  - CagouleFormatError enrichi : .field, .data_size, .min_size
  - CagouleParams.fast_mode : mode KDF mémorisé pour re-dérivation correcte

Usage rapide :
    from cagoule import encrypt, decrypt
    ct = encrypt(b"secret", b"password")
    pt = decrypt(ct, b"password")

API avancée (params pré-dérivés) :
    from cagoule.params import CagouleParams
    with CagouleParams.derive(b"password") as params:
        ct = encrypt(b"msg", b"password", params=params)
        pt = decrypt(ct, b"password", params=params)

Inspection du backend :
    from cagoule import __backend__, __omega_backend__, backend_info
    print(__backend__)       # "C (libcagoule.so v2.3)" ou "Python pur (fallback v1.x)"
    print(__omega_backend__) # "C (libcagoule.so v2.3)" ou "Python (mpmath fallback)"
    print(backend_info)      # {"matrix_backend": "avx2", "sbox_backend": "avx2", "omega_backend": "C"}
"""

from .__version__ import __version__, __version_info__, __release_date__

# ── Exceptions ────────────────────────────────────────────────────────
from .decipher import CagouleAuthError, CagouleFormatError, CagouleError

# ── Classes principales ───────────────────────────────────────────────
from .params import CagouleParams

# ── Fonctions principales ──────────────────────────────────────────────
from .cipher   import encrypt, encrypt_with_params
from .decipher import decrypt, decrypt_with_params

# ── Format ────────────────────────────────────────────────────────────
from .format import parse, inspect, serialize, is_cgl1, OVERHEAD, MAGIC

# ── Sécurité ──────────────────────────────────────────────────────────
from .utils import (
    secure_zeroize, SensitiveBuffer, bytes_to_zeroizable,
    analyze_sbox, sbox_report,
)

# ── Logging ───────────────────────────────────────────────────────────
from .logger import get_logger, set_level, enable_verbose, enable_debug

# ── Backends ──────────────────────────────────────────────────────────
from ._binding import CAGOULE_C_AVAILABLE as _C_AVAILABLE
__backend__ = "C (libcagoule.so v2.3.0)" if _C_AVAILABLE else "Python pur (fallback v2.3.0)"

# Backend info (v2.3.0)
try:
    from ._binding import get_backend_info_v230 as _get_backend_info
    backend_info = _get_backend_info()
except Exception:
    backend_info = {"matrix_backend": "unknown", "sbox_backend": "unknown", "omega_backend": "unknown"}


# Backend spécifique omega (v2.2.0)
try:
    from .omega import OMEGA_BACKEND as __omega_backend__
except Exception:
    __omega_backend__ = "unknown"
__all__ = [
    # Version
    "__version__",
    "__version_info__",
    "__release_date__",
    "__backend__",
    "__omega_backend__",
    "backend_info",
    # Exceptions
    "CagouleError",
    "CagouleAuthError",
    "CagouleFormatError",
    # Classes
    "CagouleParams",
    # Fonctions principales
    "encrypt",
    "decrypt",
    "encrypt_with_params",
    "decrypt_with_params",
    # Format
    "parse",
    "inspect",
    "serialize",
    "is_cgl1",
    "OVERHEAD",
    "MAGIC",
    # Sécurité
    "secure_zeroize",
    "SensitiveBuffer",
    "analyze_sbox",
    "sbox_report",
    # Logging
    "get_logger",
    "set_level",
    "enable_verbose",
    "enable_debug",
]

__author__    = "Slim Issa"
__copyright__ = "Copyright 2026, CAGOULE Project"
__license__   = "MIT"
