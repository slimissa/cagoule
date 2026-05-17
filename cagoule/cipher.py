"""
cipher.py — Chiffrement CAGOULE v2.4.0

Changements v2.4.0 :
  - Intégration AVX2 complète : S-box Feistel, round-key add/sub,
    CBC XOR via addmod64x4/submod64x4 dans le backend C
  - Dispatch S-box AVX2 hissé (une détection par message)
  - _cbc_decrypt_py : lève ValueError si octet hors [0,256) (cohérence C)
  - Docstring et commentaires mis à jour

Changements v2.1.0 :
  - decrypt() et decrypt_with_params() déplacés dans decipher.py
  - _build_aad / _parse_cgl1 / _cbc_decrypt exportés pour decipher.py
  - API 100% compatible v2.0.x

Optimisations actives (inchangées depuis v2.0) :
  - _cbc_encrypt() : un seul appel ctypes pour tout le message
  - _cbc_decrypt() : idem, ratio decrypt/encrypt ≈ 0.56 (Feistel symétrique)
"""

from __future__ import annotations

import ctypes
import os

from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305

from .params import CagouleParams, BLOCK_SIZE_N
from .omega import apply_round_key, remove_round_key

from ._buffer_pool import (
    _get_padded_buf, _get_out_buf, _get_rk_arr, _get_input_buf,
    _zeroize_buf,
)
from ._binding import (
    CAGOULE_C_AVAILABLE, _lib,
    list_to_uint64_array, bytes_to_c_uint8, c_uint8_to_bytes,
    cagoule_p_bytes,
    CAGOULE_OK, CAGOULE_ERR_CORRUPT,
)
from .logger import get_logger

_log = get_logger(__name__)

# ── Format CGL1 ───────────────────────────────────────────────────────
MAGIC      = b'CGL1'
VERSION    = b'\x01'
NONCE_SIZE = 12


# ══════════════════════════════════════════════════════════════════════
#  §1. PKCS7
# ══════════════════════════════════════════════════════════════════════

def pkcs7_pad(data: bytes, block_size: int) -> bytes:
    pad_len = block_size - (len(data) % block_size)
    return data + bytes([pad_len] * pad_len)


def pkcs7_unpad(data: bytes, block_size: int) -> bytes:
    if not data:
        raise ValueError("Données vides — padding PKCS7 invalide")
    pad_len = data[-1]
    if pad_len == 0 or pad_len > block_size:
        raise ValueError(f"Longueur de padding PKCS7 invalide : {pad_len}")
    if data[-pad_len:] != bytes([pad_len] * pad_len):
        raise ValueError("Padding PKCS7 corrompu")
    return data[:-pad_len]


# ══════════════════════════════════════════════════════════════════════
#  §2. Sérialisation Z/pZ ↔ bytes
# ══════════════════════════════════════════════════════════════════════

def elements_to_bytes(elements: list[int], p_bytes: int) -> bytes:
    return b''.join(e.to_bytes(p_bytes, 'big') for e in elements)


def bytes_to_elements(data: bytes, p_bytes: int) -> list[int]:
    if len(data) % p_bytes != 0:
        raise ValueError(
            f"Taille données ({len(data)}) non multiple de p_bytes ({p_bytes})"
        )
    return [int.from_bytes(data[i:i + p_bytes], 'big')
            for i in range(0, len(data), p_bytes)]


# ══════════════════════════════════════════════════════════════════════
#  §3. Helpers C — récupération des pointeurs
# ══════════════════════════════════════════════════════════════════════

def _get_matrix_ptr(params: CagouleParams):
    diff = params.diffusion
    if diff is None:
        return None
    if hasattr(diff, 'get_ptr') and callable(diff.get_ptr):
        return diff.get_ptr()
    if hasattr(diff, '_ptr') and diff._ptr:
        return diff._ptr
    return None


def _get_sbox_ptr(params: CagouleParams):
    sbox = params.sbox
    if sbox is None:
        return None
    if hasattr(sbox, 'get_ptr') and callable(sbox.get_ptr):
        return sbox.get_ptr()
    if hasattr(sbox, '_s') and sbox._s:
        return ctypes.byref(sbox._s)
    return None


# ══════════════════════════════════════════════════════════════════════
#  §4. Pipeline CBC-like — chiffrement
# ══════════════════════════════════════════════════════════════════════

def _encrypt_block_py(block_ints, prev_cipher, params, round_key):
    p, N = params.p, BLOCK_SIZE_N
    v = [(block_ints[j] + prev_cipher[j]) % p for j in range(N)]
    w = params.diffusion.apply(v)
    u = params.sbox.forward_block(w)
    return apply_round_key(u, round_key, p)


def _cbc_encrypt_py(padded: bytes, n_blocks: int,
                    params: CagouleParams, p_bytes: int) -> bytes:
    N = BLOCK_SIZE_N
    p = params.p
    round_keys = params.round_keys
    num_rk = len(round_keys)
    prev_cipher = [0] * N
    cipher_elems = []
    for idx, i in enumerate(range(0, len(padded), N)):
        block_ints = [b % p for b in padded[i:i + N]]
        rk = round_keys[idx % num_rk]
        c = _encrypt_block_py(block_ints, prev_cipher, params, rk)
        cipher_elems.extend(c)
        prev_cipher = c
    return elements_to_bytes(cipher_elems, p_bytes)


def _cbc_encrypt(message_bytes: bytes, params: CagouleParams) -> bytes:
    """Chiffrement CBC-like. Backend C si disponible (v2.0+)."""
    N = BLOCK_SIZE_N
    padded   = pkcs7_pad(message_bytes, N)
    n_blocks = len(padded) // N
    p_bytes  = cagoule_p_bytes(params.p)

    if CAGOULE_C_AVAILABLE and _lib is not None:
        mat_ptr  = _get_matrix_ptr(params)
        sbox_ptr = _get_sbox_ptr(params)
        if mat_ptr is not None and sbox_ptr is not None:
            ct_size   = n_blocks * N * p_bytes
            # P4: Reuse thread-local buffers instead of allocating new ones
            padded_c  = _get_padded_buf(len(padded))
            ctypes.memmove(padded_c, padded, len(padded))
            ct_buf    = _get_out_buf(ct_size)
            # Copy round keys into reusable buffer
            rk_arr    = _get_rk_arr(len(params.round_keys))
            for i, k in enumerate(params.round_keys):
                rk_arr[i] = k
            ret = _lib.cagoule_cbc_encrypt(
                padded_c, ctypes.c_size_t(n_blocks),
                ct_buf,   ctypes.c_size_t(ct_size),
                mat_ptr, sbox_ptr,
                rk_arr, ctypes.c_size_t(len(params.round_keys)),
                ctypes.c_uint64(params.p),
            )
            if ret == CAGOULE_OK:
                result = bytes(ct_buf[:ct_size])
                # P4: Zeroize plaintext buffer + round keys (BUG v2.3.0 fix)
                _zeroize_buf(padded_c, len(padded))
                _zeroize_buf(rk_arr, len(rk_arr) * 8)  # zeroize full allocated TLS buffer
                return result
            # P4: Zeroize on error path too
            _zeroize_buf(padded_c, len(padded))
            _zeroize_buf(rk_arr, len(rk_arr) * 8)  # zeroize full allocated TLS buffer
            _log.warning("cagoule_cbc_encrypt retourné %d, fallback Python", ret)

    return _cbc_encrypt_py(padded, n_blocks, params, p_bytes)


# ══════════════════════════════════════════════════════════════════════
#  §5. Pipeline CBC-like — déchiffrement
# ══════════════════════════════════════════════════════════════════════

def _decrypt_block_py(cipher_ints, prev_cipher, params, round_key):
    p, N = params.p, BLOCK_SIZE_N
    u = remove_round_key(cipher_ints, round_key, p)
    w = params.sbox.inverse_block(u)
    v = params.diffusion.apply_inv(w)
    return [(v[j] - prev_cipher[j]) % p for j in range(N)]


def _cbc_decrypt_py(t_message_bytes: bytes, params: CagouleParams) -> bytes:
    p_bytes  = cagoule_p_bytes(params.p)
    N        = BLOCK_SIZE_N
    p        = params.p
    round_keys = params.round_keys
    num_rk   = len(round_keys)
    n_blocks = len(t_message_bytes) // (N * p_bytes)
    prev_cipher = [0] * N
    plain_elems = []
    for idx in range(n_blocks):
        chunk = t_message_bytes[idx * N * p_bytes:(idx + 1) * N * p_bytes]
        cipher_ints = bytes_to_elements(chunk, p_bytes)
        rk = round_keys[idx % num_rk]
        pt = _decrypt_block_py(cipher_ints, prev_cipher, params, rk)
        for j, val in enumerate(pt):
            if val > 255:
                raise ValueError(
                    f"Octet hors domaine [0, 256) après déchiffrement algébrique "
                    f"(bloc {idx}, position {j}, valeur {val}). "
                    f"Ciphertext corrompu ou paramètres incorrects."
                )
        plain_elems.extend(pt)
        prev_cipher = cipher_ints
    return bytes(plain_elems)

def _cbc_decrypt(t_message_bytes: bytes, params: CagouleParams) -> bytes:
    """Déchiffrement CBC-like. Backend C si disponible (v2.0+)."""
    p_bytes  = cagoule_p_bytes(params.p)
    N        = BLOCK_SIZE_N
    n_blocks = len(t_message_bytes) // (N * p_bytes)

    if CAGOULE_C_AVAILABLE and _lib is not None:
        mat_ptr  = _get_matrix_ptr(params)
        sbox_ptr = _get_sbox_ptr(params)
        if mat_ptr is not None and sbox_ptr is not None:
            # P4: Reuse thread-local buffers instead of allocating new ones
            ct_size  = len(t_message_bytes)
            ct_c     = _get_input_buf(ct_size)
            ctypes.memmove(ct_c, t_message_bytes, ct_size)
            pt_size  = n_blocks * N
            pt_buf   = _get_out_buf(pt_size)
            # Copy round keys into reusable buffer
            rk_arr   = _get_rk_arr(len(params.round_keys))
            for i, k in enumerate(params.round_keys):
                rk_arr[i] = k
            ret = _lib.cagoule_cbc_decrypt(
                ct_c,    ctypes.c_size_t(n_blocks),
                pt_buf,  ctypes.c_size_t(pt_size),
                mat_ptr, sbox_ptr,
                rk_arr, ctypes.c_size_t(len(params.round_keys)),
                ctypes.c_uint64(params.p),
            )
            if ret == CAGOULE_OK:
                result = c_uint8_to_bytes(pt_buf, pt_size)
                # P4: Zeroize plaintext output + ciphertext + round keys (BUG v2.3.0 fix)
                _zeroize_buf(pt_buf, pt_size)
                _zeroize_buf(ct_c, ct_size)
                _zeroize_buf(rk_arr, len(rk_arr) * 8)  # zeroize full allocated TLS buffer
                return result
            if ret == CAGOULE_ERR_CORRUPT:
                _zeroize_buf(pt_buf, pt_size)
                _zeroize_buf(ct_c, ct_size)
                _zeroize_buf(rk_arr, len(rk_arr) * 8)  # zeroize full allocated TLS buffer
                raise ValueError(f"b hors domaine [0,256) détecté (code {ret})")
            # P4: Zeroize on unknown error too
            _zeroize_buf(ct_c, ct_size)
            _zeroize_buf(rk_arr, len(rk_arr) * 8)  # zeroize full allocated TLS buffer
            _log.warning("cagoule_cbc_decrypt retourné %d, fallback Python", ret)

    return _cbc_decrypt_py(t_message_bytes, params)


# ══════════════════════════════════════════════════════════════════════
#  §6. Format CGL1
# ══════════════════════════════════════════════════════════════════════

def _build_aad(salt: bytes) -> bytes:
    return MAGIC + VERSION + salt


def _serialize_cgl1(salt: bytes, nonce: bytes, ct_tag: bytes) -> bytes:
    return MAGIC + VERSION + salt + nonce + ct_tag


def _parse_cgl1(data: bytes) -> tuple[bytes, bytes, bytes]:
    min_size = 4 + 1 + 32 + 12 + 16   # 65 octets minimum
    if len(data) < min_size:
        raise ValueError(f"Message trop court : {len(data)} < {min_size}")
    magic, version = data[0:4], data[4:5]
    if magic != MAGIC:
        raise ValueError(f"Magic invalide : {magic!r}")
    if version != VERSION:
        raise ValueError(f"Version non supportée : 0x{data[4]:02x}")
    return data[5:37], data[37:49], data[49:]   # salt, nonce, ct+tag


# ══════════════════════════════════════════════════════════════════════
#  §7. API publique
# ══════════════════════════════════════════════════════════════════════

def encrypt(plaintext: bytes | str, password: bytes | str,
            salt: bytes | None = None,
            params: CagouleParams | None = None,
            fast_mode: bool = False) -> bytes:
    """
    Chiffre plaintext avec CAGOULE v2.4.0.

    Args:
        plaintext  : Message à chiffrer (bytes ou str UTF-8).
        password   : Mot de passe (bytes ou str).
        salt       : Sel 32 octets (None = aléatoire).
        params     : Paramètres pré-calculés (None = dérivés depuis password+salt).
        fast_mode  : KDF rapide (tests uniquement).

    Returns:
        Bytes au format CGL1 (MAGIC + VERSION + SALT + NONCE + CT + TAG).
    """
    if isinstance(plaintext, str):
        plaintext = plaintext.encode('utf-8')
    if isinstance(password, str):
        password = password.encode('utf-8')

    own_params = False
    if params is None:
        params     = CagouleParams.derive(password, salt, fast_mode=fast_mode)
        own_params = True

    salt  = params.salt
    nonce = os.urandom(NONCE_SIZE)

    try:
        t_message        = _cbc_encrypt(plaintext, params)
        aad              = _build_aad(salt)
        aead             = ChaCha20Poly1305(params.k_stream)
        ciphertext_w_tag = aead.encrypt(nonce, t_message, aad)
        return _serialize_cgl1(salt, nonce, ciphertext_w_tag)
    finally:
        if own_params:
            params.zeroize()


def encrypt_with_params(plaintext: bytes | str,
                        params: CagouleParams) -> bytes:
    """
    Chiffre avec des paramètres déjà dérivés.
    Usage : KAT, benchmarks, ProcessPoolExecutor.
    """
    if isinstance(plaintext, str):
        plaintext = plaintext.encode('utf-8')
    salt  = params.salt
    nonce = os.urandom(NONCE_SIZE)
    t_message        = _cbc_encrypt(plaintext, params)
    aad              = _build_aad(salt)
    aead             = ChaCha20Poly1305(params.k_stream)
    ciphertext_w_tag = aead.encrypt(nonce, t_message, aad)
    return _serialize_cgl1(salt, nonce, ciphertext_w_tag)


# ══════════════════════════════════════════════════════════════════════
#  §8. API bulk — P2 v2.4.0
#  Une seule dérivation Argon2id pour N messages.
#  Fraction GIL-holding chute de ~46% à ~10%.
# ══════════════════════════════════════════════════════════════════════

def encrypt_bulk(
    messages: list,
    password,
    salt: bytes | None = None,
    fast_mode: bool = False,
) -> list:
    """
    Chiffre N messages avec une seule dérivation Argon2id.

    Args:
        messages   : Liste de bytes ou str à chiffrer.
        password   : Mot de passe commun (bytes ou str).
        salt       : Sel 32 octets (None = aléatoire, partagé pour tous les messages).
        fast_mode  : KDF rapide (tests uniquement).

    Returns:
        Liste de bytes au format CGL1, même ordre que messages.

    Note sécurité :
        Le sel Argon2id est partagé entre les N messages — acceptable pour
        une session unique avec le même mot de passe.
        Pour des sessions distinctes, dériver un CagouleParams par session.
    """
    if not messages:
        return []
    if isinstance(password, str):
        password = password.encode('utf-8')

    params = CagouleParams.derive(password, salt, fast_mode=fast_mode)
    try:
        return [encrypt_with_params(msg, params) for msg in messages]
    finally:
        params.zeroize()


def decrypt_bulk(
    ciphertexts: list,
    password,
    fast_mode: bool = False,
) -> list:
    """
    Déchiffre N messages CGL1 avec une seule dérivation Argon2id par sel unique.

    Groupe les ciphertexts par sel (salt) et effectue une seule dérivation
    par groupe. Si tous les ciphertexts partagent le même sel (produits par
    encrypt_bulk), une seule dérivation est effectuée.

    Args:
        ciphertexts : Liste de bytes CGL1.
        password    : Mot de passe (bytes ou str).
        fast_mode   : KDF rapide (tests uniquement).

    Returns:
        Liste de plaintext bytes, même ordre que ciphertexts.
    """
    from .decipher import decrypt_with_params, _parse_cgl1_salt

    if not ciphertexts:
        return []
    if isinstance(password, str):
        password = password.encode('utf-8')

    # Grouper par sel → une seule dérivation par groupe
    from collections import defaultdict
    groups = defaultdict(list)   # salt → [(original_index, ciphertext)]
    for idx, ct in enumerate(ciphertexts):
        salt = _parse_cgl1_salt(ct)
        groups[salt].append((idx, ct))

    results = [None] * len(ciphertexts)
    for salt, items in groups.items():
        params = CagouleParams.derive(password, salt, fast_mode=fast_mode)
        try:
            for orig_idx, ct in items:
                results[orig_idx] = decrypt_with_params(ct, params)
        finally:
            params.zeroize()

    return results
