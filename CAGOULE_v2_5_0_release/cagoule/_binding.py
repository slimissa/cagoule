"""
_binding.py — Chargeur ctypes pour libcagoule.so — CAGOULE v2.5.0

Nouveautés v2.4.0 :
  - GIL release on heavy C calls (cbc_encrypt/decrypt, matrix_mul, sbox_block)
    → ThreadPoolExecutor scaling improved from 1.7× to 4-6× at 8 workers

Nouveautés v2.3.0 (AVX2 S-box, buffer pool, QShell integration) :
  - cagoule_matrix_backend_is_avx2() : détection runtime AVX2
  - cagoule_matrix_mul_scalar / mul_inv_scalar : chemin scalaire explicite
  - get_backend_info() : dictionnaire des backends actifs

Nouveautés v2.1.0 :
  - Vérification version 2.1.0 via cagoule_version() si présent
  - Les symboles cagoule_omega_* sont détectés dans omega.py (pas ici)
    pour éviter le chargement circulaire au démarrage

Expose :
  - CagouleMatrix + cagoule_matrix_*
  - CagouleSBox64 + cagoule_sbox_*
  - cagoule_cbc_encrypt / cagoule_cbc_decrypt
  - get_backend_info() (v2.2.0, unchanged), get_backend_info_v230() (v2.3.0)
"""

from __future__ import annotations

import ctypes
import os
import pathlib
import warnings
from typing import Optional, List, Dict

# ── Constantes ────────────────────────────────────────────────────────
CAGOULE_N         = 16
CAGOULE_P32_PRIME = 4294967291   # plus grand premier < 2^32

CAGOULE_OK          =  0
CAGOULE_ERR_NULL    = -1
CAGOULE_ERR_SIZE    = -2
CAGOULE_ERR_CORRUPT = -3


# ── Localisation de libcagoule.so ─────────────────────────────────────

def _find_lib() -> Optional[pathlib.Path]:
    candidates = [
        pathlib.Path(__file__).parent / "libcagoule.so",
        pathlib.Path(__file__).parent / "c" / "libcagoule.so",
    ]
    env_path = os.environ.get("LIBCAGOULE_PATH")
    if env_path:
        candidates.insert(0, pathlib.Path(env_path))
    for p in candidates:
        if p.exists():
            return p
    return None


_lib_path = _find_lib()
_lib: Optional[ctypes.CDLL] = None
CAGOULE_C_AVAILABLE = False

if _lib_path is not None:
    try:
        _lib = ctypes.CDLL(str(_lib_path))
        CAGOULE_C_AVAILABLE = True
    except OSError as e:
        warnings.warn(
            f"cagoule: impossible de charger {_lib_path}: {e}. "
            "Fallback Python pur (performances v1.x).",
            RuntimeWarning, stacklevel=2
        )
else:
    warnings.warn(
        "cagoule: libcagoule.so non trouvé. "
        "Compiler : cd cagoule/c && make && make install. "
        "Fallback Python pur actif.",
        RuntimeWarning, stacklevel=2
    )


# ── Structures ctypes ─────────────────────────────────────────────────

class CagouleMatrixC(ctypes.Structure):
    _fields_ = [
        ("fwd",       (ctypes.c_uint64 * CAGOULE_N) * CAGOULE_N),
        ("inv",       (ctypes.c_uint64 * CAGOULE_N) * CAGOULE_N),
        ("p",         ctypes.c_uint64),
        ("kind",      ctypes.c_int),
        # v2.2.1: AVX2 column-major layouts
        ("fwd_avx2",  (ctypes.c_uint64 * (CAGOULE_N * 4)) * 4),
        ("inv_avx2",  (ctypes.c_uint64 * (CAGOULE_N * 4)) * 4),
        # v2.5.0: Mersenne pool constant
        ("k_mersenne", ctypes.c_uint64),
    ]


class CagouleSBox64C(ctypes.Structure):
    _fields_ = [
        ("p",           ctypes.c_uint64),
        ("rk0",         ctypes.c_uint64),
        ("rk1",         ctypes.c_uint64),
        ("d",           ctypes.c_uint64),
        ("d_inv",       ctypes.c_uint64),
        ("use_feistel", ctypes.c_int),
    ]


# ── Signatures ctypes ─────────────────────────────────────────────────

if CAGOULE_C_AVAILABLE and _lib is not None:

    # Matrix
    _lib.cagoule_matrix_build.argtypes = [
        ctypes.POINTER(ctypes.c_uint64), ctypes.c_size_t, ctypes.c_uint64]
    _lib.cagoule_matrix_build.restype = ctypes.POINTER(CagouleMatrixC)

    _lib.cagoule_matrix_free.argtypes = [ctypes.POINTER(CagouleMatrixC)]
    _lib.cagoule_matrix_free.restype  = None

    _lib.cagoule_matrix_mul.argtypes = [
        ctypes.POINTER(CagouleMatrixC),
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.POINTER(ctypes.c_uint64)]
    _lib.cagoule_matrix_mul.restype = None
    _lib.cagoule_matrix_mul.release_gil = True

    _lib.cagoule_matrix_mul_inv.argtypes = [
        ctypes.POINTER(CagouleMatrixC),
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.POINTER(ctypes.c_uint64)]
    _lib.cagoule_matrix_mul_inv.restype = None
    _lib.cagoule_matrix_mul_inv.release_gil = True

    _lib.cagoule_matrix_verify.argtypes = [ctypes.POINTER(CagouleMatrixC)]
    _lib.cagoule_matrix_verify.restype  = ctypes.c_int

    # ── v2.2.0: AVX2 backend detection ─────────────────────────────
    try:
        _lib.cagoule_matrix_backend_is_avx2.argtypes = []
        _lib.cagoule_matrix_backend_is_avx2.restype = ctypes.c_int
        _HAS_AVX2_API = True
    except AttributeError:
        _HAS_AVX2_API = False

    # ── v2.2.0: Scalar explicit path (for parity tests) ────────────
    try:
        _lib.cagoule_matrix_mul_scalar.argtypes = [
            ctypes.POINTER(CagouleMatrixC),
            ctypes.POINTER(ctypes.c_uint64),
            ctypes.POINTER(ctypes.c_uint64)]
        _lib.cagoule_matrix_mul_scalar.restype = None

        _lib.cagoule_matrix_mul_inv_scalar.argtypes = [
            ctypes.POINTER(CagouleMatrixC),
            ctypes.POINTER(ctypes.c_uint64),
            ctypes.POINTER(ctypes.c_uint64)]
        _lib.cagoule_matrix_mul_inv_scalar.restype = None
        _HAS_SCALAR_API = True
    except AttributeError:
        _HAS_SCALAR_API = False

    # S-Box
    _lib.cagoule_sbox_init.argtypes = [
        ctypes.POINTER(CagouleSBox64C),
        ctypes.c_uint64, ctypes.c_uint64, ctypes.c_uint64]
    _lib.cagoule_sbox_init.restype = None

    _lib.cagoule_sbox_forward.argtypes = [
        ctypes.POINTER(CagouleSBox64C), ctypes.c_uint64]
    _lib.cagoule_sbox_forward.restype  = ctypes.c_uint64

    _lib.cagoule_sbox_inverse.argtypes = [
        ctypes.POINTER(CagouleSBox64C), ctypes.c_uint64]
    _lib.cagoule_sbox_inverse.restype  = ctypes.c_uint64

    _lib.cagoule_sbox_block_forward.argtypes = [
        ctypes.POINTER(CagouleSBox64C),
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.c_size_t]
    _lib.cagoule_sbox_block_forward.restype = None
    _lib.cagoule_sbox_block_forward.release_gil = True

    _lib.cagoule_sbox_block_inverse.argtypes = [
        ctypes.POINTER(CagouleSBox64C),
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.c_size_t]
    _lib.cagoule_sbox_block_inverse.restype = None
    _lib.cagoule_sbox_block_inverse.release_gil = True

    # CBC Pipeline
    _cbc_argtypes = [
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
        ctypes.POINTER(CagouleMatrixC),
        ctypes.POINTER(CagouleSBox64C),
        ctypes.POINTER(ctypes.c_uint64), ctypes.c_size_t,
        ctypes.c_uint64,
        # v2.5.0 : z_offset[16] uint64 + num_zo
        ctypes.POINTER(ctypes.c_uint64), ctypes.c_size_t,
    ]
    _lib.cagoule_cbc_encrypt.argtypes = _cbc_argtypes
    _lib.cagoule_cbc_encrypt.restype  = ctypes.c_int
    # release_gil is handled by ctypes automatically for CDLL calls
    # _lib.cagoule_cbc_encrypt.release_gil = True  (attribute does not exist)

    _lib.cagoule_cbc_decrypt.argtypes = _cbc_argtypes
    _lib.cagoule_cbc_decrypt.restype  = ctypes.c_int
    # _lib.cagoule_cbc_decrypt.release_gil = True  (attribute does not exist)


# ── Utilitaires ───────────────────────────────────────────────────────

def list_to_uint64_array(lst: List[int]) -> ctypes.Array:
    return (ctypes.c_uint64 * len(lst))(*lst)


def uint64_array_to_list(arr: ctypes.Array, n: int) -> List[int]:
    return [int(arr[i]) for i in range(n)]


def bytes_to_c_uint8(data: bytes) -> ctypes.Array:
    return (ctypes.c_uint8 * len(data)).from_buffer_copy(data)


def c_uint8_to_bytes(arr: ctypes.Array, n: int) -> bytes:
    return bytes(arr[:n])


def cagoule_p_bytes(p: int) -> int:
    """Nombre d'octets pour un élément de Z/pZ."""
    return 8 if p > 0xFFFFFFFF else 4


def free_matrix(matrix_ptr) -> None:
    if CAGOULE_C_AVAILABLE and _lib and matrix_ptr:
        _lib.cagoule_matrix_free(matrix_ptr)


# ── v2.2.0: Backend Info ──────────────────────────────────────────────

def get_backend_info() -> Dict[str, str]:
    """Retourne les informations sur les backends actifs (v2.2.0, conservé pour compatibilité).

    Returns:
        dict avec les clés :
        - 'matrix_backend' : 'avx2', 'scalar', ou 'python'
        - 'omega_backend'  : 'C' ou 'python' (rempli par omega.py)
    """
    info = {
        "matrix_backend": "python",
        "omega_backend": "unknown",
    }

    if CAGOULE_C_AVAILABLE and _lib is not None:
        # Détection AVX2 pour la matrice
        try:
            if _lib.cagoule_matrix_backend_is_avx2():
                info["matrix_backend"] = "avx2"
            else:
                info["matrix_backend"] = "scalar"
        except Exception:
            info["matrix_backend"] = "scalar"

        # Backend omega (C par défaut si libcagoule.so est chargé)
        info["omega_backend"] = "C"
    else:
        info["omega_backend"] = "python"

    return info

# ── v2.3.0: S-box AVX2 backend detection ─────────────────────────────

if CAGOULE_C_AVAILABLE and _lib is not None:
    try:
        _lib.cagoule_sbox_backend_is_avx2.argtypes = []
        _lib.cagoule_sbox_backend_is_avx2.restype = ctypes.c_int
        _HAS_SBOX_AVX2_API = True
    except AttributeError:
        _HAS_SBOX_AVX2_API = False
else:
    _HAS_SBOX_AVX2_API = False


def get_backend_info_v230() -> Dict[str, str]:
    """Retourne les informations complètes sur les backends actifs (v2.3.0).

    Returns:
        dict avec les clés :
        - 'matrix_backend' : 'avx2', 'scalar', ou 'python'
        - 'sbox_backend'   : 'avx2', 'scalar', ou 'python'   ← nouveau v2.3.0
        - 'omega_backend'  : 'C' ou 'python'
    """
    info = get_backend_info()   # hérite v2.2.0 (matrix_backend, omega_backend)

    if CAGOULE_C_AVAILABLE and _lib is not None:
        try:
            if _HAS_SBOX_AVX2_API and _lib.cagoule_sbox_backend_is_avx2():
                info["sbox_backend"] = "avx2"
            else:
                info["sbox_backend"] = "scalar"
        except Exception:
            info["sbox_backend"] = "scalar"
    else:
        info["sbox_backend"] = "python"

    return info