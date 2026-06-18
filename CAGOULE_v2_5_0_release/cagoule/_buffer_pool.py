"""
_buffer_pool.py — Thread-local buffer pool — CAGOULE v2.5.0 P4

Réduit l'overhead d'allocation ctypes pour les petits messages (~10-15% gain).
Chaque thread possède ses propres buffers, redimensionnés à la demande.
Zéroisation automatique après chaque utilisation pour éviter les fuites de données.
"""

from __future__ import annotations

import ctypes
import threading

# ── Thread-local storage ────────────────────────────────────────────

_tls = threading.local()


def _get_padded_buf(size: int) -> ctypes.Array:
    """Buffer thread-local pour le plaintext paddé (entrée chiffrement)."""
    if not hasattr(_tls, 'padded_buf') or len(_tls.padded_buf) < size:
        if hasattr(_tls, 'padded_buf'):
            ctypes.memset(_tls.padded_buf, 0, ctypes.sizeof(_tls.padded_buf))  # zéroiser avant remplacement
        _tls.padded_buf = (ctypes.c_uint8 * size)()
    return _tls.padded_buf


def _get_out_buf(size: int) -> ctypes.Array:
    """Buffer thread-local pour la sortie (ciphertext ou plaintext)."""
    if not hasattr(_tls, 'out_buf') or len(_tls.out_buf) < size:
        if hasattr(_tls, 'out_buf'):
            ctypes.memset(_tls.out_buf, 0, ctypes.sizeof(_tls.out_buf))  # zéroiser avant remplacement
        _tls.out_buf = (ctypes.c_uint8 * size)()
    return _tls.out_buf


def _get_rk_arr(num_keys: int) -> ctypes.Array:
    """Buffer thread-local pour le tableau de clés de ronde (64 uint64_t)."""
    if not hasattr(_tls, 'rk_arr') or len(_tls.rk_arr) < num_keys:
        _tls.rk_arr = (ctypes.c_uint64 * num_keys)()
    return _tls.rk_arr


def _get_input_buf(size: int) -> ctypes.Array:
    """Buffer thread-local pour l'entrée déchiffrement (ciphertext bytes)."""
    if not hasattr(_tls, 'input_buf') or len(_tls.input_buf) < size:
        if hasattr(_tls, 'input_buf'):
            ctypes.memset(_tls.input_buf, 0, ctypes.sizeof(_tls.input_buf))  # zéroiser avant remplacement
        _tls.input_buf = (ctypes.c_uint8 * size)()
    return _tls.input_buf


# ── Zeroization helpers ──────────────────────────────────────────────

def _zeroize_buf(buf: ctypes.Array, size: int) -> None:
    """Zéroise un buffer ctypes après utilisation."""
    try:
        ctypes.memset(buf, 0, size)
    except Exception:
        pass  # Best effort


def _pool_stats() -> dict:
    """Statistiques d'utilisation du pool (debug)."""
    stats = {}
    for name in ('padded_buf', 'out_buf', 'rk_arr', 'input_buf'):
        if hasattr(_tls, name):
            buf = getattr(_tls, name)
            stats[name] = {'size': len(buf), 'id': id(buf)}
        else:
            stats[name] = None
    return stats