# CAGOULE v2.3.0

**Cryptographie Algébrique Géométrique par Ondes et Logique Entrelacée**

[![Version](https://img.shields.io/badge/version-2.3.0-blue)](https://github.com/slimissa/CAGOULE)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Python](https://img.shields.io/badge/python-3.12+-blue)](https://www.python.org)
[![Platform](https://img.shields.io/badge/platform-x86__64%20Linux-lightgrey)](https://github.com/slimissa/CAGOULE)
[![C Tests](https://img.shields.io/badge/C%20tests-43587%20assertions-brightgreen)](https://github.com/slimissa/CAGOULE)
[![Python Tests](https://img.shields.io/badge/Python%20tests-567%20passed-brightgreen)](https://github.com/slimissa/CAGOULE)
---

CAGOULE is a symmetric hybrid encryption system combining ChaCha20-Poly1305, Argon2id, HKDF-SHA256 with a custom algebraic diffusion layer (Vandermonde over Z/pZ, 2-round Feistel S-box, ζ(2n)-derived round keys). The C backend is fully AVX2-vectorised as of v2.3.0.

---

## What's New in v2.3.0

v2.3.0 is the **S-box AVX2** release. After vectorising the Vandermonde matrix (v2.2.0), the next bottleneck was the Feistel S-box (~30 % of block time). v2.3.0 eliminates it.

### Delivered

| Feature | Status |
|---|---|
| `cagoule_sbox_avx2.h` — `_feistel_f_avx2`, `_feistel_pass_avx2/_inv`, `forward4/inverse4` | ✅ |
| `cagoule_sbox_avx2.c` — `cagoule_sbox_block_forward/inverse_avx2` (4 lanes) | ✅ |
| Mersenne-like reduction for P32_PRIME (2×32-bit add instead of 128-bit div) | ✅ |
| Cycle-walking AVX2 (branch-predicted near-zero path for p ≈ 2^64) | ✅ |
| Round-key add/sub via `addmod64x4` / `submod64x4` (4 lanes, v2.3.0) | ✅ |
| CBC XOR via `addmod64x4` / `submod64x4` (4 lanes, v2.3.0) | ✅ |
| `cagoule_sbox_backend_is_avx2()` — lazy-init, thread-safe, exposed to Python | ✅ |
| `get_backend_info_v230()` — `sbox_backend` added to `backend_info` | ✅ |
| ctypes buffer reuse in `_cbc_encrypt` (from_buffer_copy + direct bytes()) | ✅ |
| `test_sbox_avx2.c` — 22 498 assertions (parity, roundtrip, edge cases, bench) | ✅ |
| `tests/test_sbox_avx2.py` — 18 Python-level tests | ✅ |
| `_avx2_available()` normalization fix (bitmask → 0/1) in cagoule_matrix.c | ✅ |
| Makefile v2.3.0 — `cagoule_sbox_avx2.c` compiled with `-mavx2` | ✅ |

### Performance (Intel Skylake, p ≈ 2^64, 65 536 blocks ≡ 1 MB)

| Layer | v2.2.0 | v2.3.0 | Gain |
|---|---|---|---|
| Vandermonde matrix | 10 MB/s | 10 MB/s | — (v2.2.0) |
| **Feistel S-box** | **~87 MB/s** | **~120 MB/s** | **×1.32** |
| Round-key add | scalar | AVX2 | ~×2 |
| **End-to-end `test_cipher` 1 MB** | **9.7 MB/s** | **11.0 MB/s** | **+13%** |

The S-box `×1.9` gain (not the theoretical ×4) reflects the Mersenne reduction
overhead per Feistel round and the cycle-walking branch. The end-to-end gain is
bounded by Argon2id key derivation and ChaCha20-Poly1305 (Amdahl).

### v2.3.0 Per-Block Cost Breakdown

| Step | % of block time | v2.3.0 status |
|---|---|---|
| Byte → uint64 | ~5% | scalar |
| XOR with prev (CBC) | ~2% | **AVX2 ✅** |
| Vandermonde multiply | ~40% | **AVX2 ✅** (v2.2.0) |
| **Feistel S-box** | **~30%** | **AVX2 ✅** |
| **Round key add** | **~8%** | **AVX2 ✅** |
| Serialization | ~15% | AVX2 ✅ (v2.2.0) |

---

## Architecture

```
plaintext
    │
    ▼
┌─────────────────────────────────────────┐
│  Argon2id KDF  (password → 96-byte key) │  RFC 9106
└─────────────────┬───────────────────────┘
                  │ HKDF-SHA256
    ┌─────────────┴──────────┐
    │  Algebraic Layer (C)   │  ← Fully AVX2 in v2.3.0
    │                        │
    │  ζ(2n) round keys      │  64 keys via HKDF-SHA256
    │  Vandermonde 16×16     │  AVX2 ✅ (v2.2.0)
    │  Feistel S-box 2-round │  AVX2 ✅ (v2.3.0)
    │  CBC-like chaining     │  AVX2 ✅ (v2.3.0)
    └─────────────┬──────────┘
                  │
┌─────────────────┴───────────────────────┐
│  ChaCha20-Poly1305 AEAD                 │  RFC 8439
└─────────────────────────────────────────┘
    │
    ▼
CGL1 ciphertext  [ MAGIC | VER | SALT | NONCE | CT | TAG ]
```

---

## Quick Start

```python
from cagoule import encrypt, decrypt

ct = encrypt(b"my secret data", b"my_password")
pt = decrypt(ct, b"my_password")
assert pt == b"my secret data"
```

## Backend Inspection (v2.3.0)

```python
from cagoule import __version__, backend_info
from cagoule._binding import get_backend_info_v230

print(__version__)          # "2.3.0"
print(backend_info)         # {'matrix_backend': 'avx2', 'omega_backend': 'C', 'sbox_backend': 'avx2'}
print(get_backend_info_v230())  # full v2.3.0 dict
```

---

## Build & Test

```bash
cd cagoule/c
make clean && make && make tests  # builds + runs all 8 C test binaries
make install                      # copies libcagoule.so to Python package
pip install -e ".[dev]"
pytest tests/ -v -m "not nist"   # 567 Python tests
```

### C Tests — 43,587 assertions, 0 failed

| Binary | Assertions | Coverage |
|---|---|---|
| `test_math` | — | `mulmod64`, `addmod64`, `submod64`, `powmod64`, `invmod64` |
| `test_sbox` | — | Feistel bijectivity, roundtrip, fallback x^d |
| `test_matrix` | — | Vandermonde P×P⁻¹=I, Cauchy fallback |
| `test_cipher` | — | CBC roundtrip, PKCS7, diffusion, 1MB bench |
| `test_omega` | 154 | ζ(2n), HKDF round keys, OpenSSL, thread-safety |
| `test_math_avx2` | 16 489 | `mulmod64x4` / `addmod64x4` / `submod64x4` parity |
| `test_matrix_avx2` | 4 260 | AVX2 Vandermonde parity, roundtrip, bench |
| **`test_sbox_avx2`** | **22 498** | **AVX2 Feistel parity (400K cases), edge, bench** |

---

## Roadmap

### v2.3.0+ Backlog

- QShell primitives: `cgl encrypt`, `cgl decrypt`, `cgl bench`, `cgl info`
- PyPI `manylinux` wheel
- Formal algebraic specification + IACR ePrint submission
- `cagoule-pass` v2.0.0 with `qshell vault` command
- v3.0.0: multi-block SIMD (process 4 blocks × 16 elements simultaneously → >30 MB/s target)

---

## Changelog

### v2.3.0 — 2026-05-13

**C Backend**
- New: `cagoule_sbox_avx2.h` — `_feistel_f_avx2`, `forward4_avx2`, `inverse4_avx2`
- New: `cagoule_sbox_avx2.c` — `cagoule_sbox_block_forward/inverse_avx2` (4 lanes)
- Modified: `cagoule_cipher.c` — S-box AVX2 dispatch hoisted; round-key add/sub via `addmod64x4`/`submod64x4`; CBC XOR via `addmod64x4`
- Modified: `cagoule_matrix.c` — `_avx2_available()` return normalized to 0/1
- New: `test_sbox_avx2.c` (22 498 assertions)

**Python Layer**
- New: `get_backend_info_v230()` → `sbox_backend` field
- Modified: `_cbc_encrypt` — `from_buffer_copy` + `bytes(ct_buf[:n])` (buffer reuse)
- Updated: `__version__` → 2.3.0, `__backend__` → "C (libcagoule.so v2.3)"
- New: `tests/test_sbox_avx2.py` (18 tests)
- Updated: `tests/test_kat.py` — SHA-256 KAT updated for v2.3.0 S-box path

### v2.2.0 — 2026-05-06
AVX2 Vandermonde matrix multiply (+67% algebraic layer).

### v2.1.0 — 2026-05-01
C port of omega.c; security fix for wrong-password detection.

---

## Author

**Slim Issa** — Kairouan, Tunisia
[github.com/slimissa/CAGOULE](https://github.com/slimissa/CAGOULE)

Part of the [QuantOS](https://github.com/slimissa/LAS_Shell) platform.

---

## License

MIT — see [LICENSE](LICENSE).
