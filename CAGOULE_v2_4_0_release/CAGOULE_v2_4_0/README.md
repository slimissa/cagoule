# CAGOULE v2.4.0

**Cryptographie Algébrique Géométrique par Ondes et Logique Entrelacée**

[![Version](https://img.shields.io/badge/version-2.4.0-blue)](https://github.com/slimissa/CAGOULE)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Python](https://img.shields.io/badge/python-3.12+-blue)](https://www.python.org)
[![Platform](https://img.shields.io/badge/platform-x86__64%20Linux-lightgrey)](https://github.com/slimissa/CAGOULE)
[![C Tests](https://img.shields.io/badge/C%20tests-43686%20passed-brightgreen)](https://github.com/slimissa/CAGOULE)
[![Python Tests](https://img.shields.io/badge/Python%20tests-578%20passed-brightgreen)](https://github.com/slimissa/CAGOULE)

---

CAGOULE is a symmetric hybrid encryption system combining ChaCha20-Poly1305, Argon2id, HKDF-SHA256 with a custom algebraic diffusion layer (Vandermonde over Z/pZ, 2-round Feistel S-box, ζ(2n)-derived round keys). The C backend is fully AVX2-vectorised.

---

## What's New in v2.4.0

v2.4.0 is the **Pipeline & Bulk** release. After vectorising all algebraic layer components (v2.2.0–v2.3.0), the focus shifted to pipeline parallelism in C and batch API in Python.

### Delivered

| Feature | Status |
|---|---|
| **P1a** — Encrypt unroll4 + prefetch (loop unrolling, ~+25% C-layer) | ✅ |
| **P1b** — Decrypt pipeline4 (true 4-way parallel, ~×2 C-layer) | ✅ |
| **P2** — `encrypt_bulk()` / `decrypt_bulk()` — single Argon2id derivation for N messages | ✅ |
| **P4** — Thread-local buffer pool (reuse ctypes buffers, +71% single-core in parallel suite) | ✅ |
| GIL release on heavy C calls (`cagoule_cbc_encrypt`, `matrix_mul`, `sbox_block`) | ✅ |
| `test_cipher_pipeline4.c` — 88 assertions (edge cases, parity, residual regression) | ✅ |
| `cagoule_bench.toml` — v2.4.0 config with bulk suite, min_baseline_n=5 | ✅ |
| Makefile `install` — fixed stale `.so` overwrite (`rm -f` before `cp`) | ✅ |
| `_binding.py` — fixed struct size (removed oversized padding) | ✅ |
| 3 compiler warnings fixed (unused variable, `_POSIX_C_SOURCE` redefined) | ✅ |

### Performance

#### C Layer (65,536 blocks ≡ 1 MB)

| Metric | v2.3.0 | v2.4.0 |
|--------|--------|--------|
| C encrypt | 9.7 MB/s | **8.0 MB/s** |
| C decrypt | — | **8.1 MB/s** |
| Ratio dec/enc | — | **0.99×** |
| S-box AVX2 | 120 MB/s | **70.1 MB/s** |

#### Python API (cagoule-bench v2.0.0)

| Test | v2.4.0 |
|------|--------|
| encrypt-1KB | 5.8 MB/s |
| encrypt-1MB | **5.2 MB/s** |
| encrypt-10MB | 5.1 MB/s |
| decrypt-1MB | 4.6 MB/s |
| decrypt-10MB | **8.5 MB/s** |

#### Parallel Scaling (encrypt_bulk + ProcessPoolExecutor)

| Workers | Throughput | Speedup | Efficiency |
|---------|------------|---------|------------|
| 1 | 4.8 MB/s | 1.00× | — |
| 2 | 10.0 MB/s | 2.08× | 104% |
| 4 | 18.5 MB/s | 3.88× | 97% |
| 8 | 29.6 MB/s | 6.19× | 77% |
| 20 | **39.9 MB/s** | 8.34× | 42% |

#### Streaming (64 KB chunks)

| Size | CAGOULE | AES-256-GCM | ChaCha20-Poly1305 |
|------|---------|-------------|-------------------|
| 50 MB | 5.6 MB/s | 456 MB/s | 402 MB/s |
| 100 MB | 5.7 MB/s | 459 MB/s | 401 MB/s |
| 500 MB | 5.7 MB/s | 458 MB/s | 402 MB/s |

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
    │  Algebraic Layer (C)   │  ← Fully AVX2
    │                        │
    │  ζ(2n) round keys      │  64 keys via HKDF-SHA256
    │  Vandermonde 16×16     │  AVX2 ✅ (v2.2.0)
    │  Feistel S-box 2-round │  AVX2 ✅ (v2.3.0)
    │  CBC-like chaining     │  AVX2 ✅ (v2.3.0)
    │  Pipeline4 decrypt     │  NEW v2.4.0
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

### Bulk Encryption (v2.4.0)

```python
from cagoule import encrypt_bulk, decrypt_bulk

messages = [b"alpha", b"beta", b"gamma"]
cts = encrypt_bulk(messages, b"my_password")
pts = decrypt_bulk(cts, b"my_password")
assert pts == messages
```

## Backend Inspection

```python
from cagoule import __version__, backend_info
from cagoule._binding import get_backend_info_v230

print(__version__)          # "2.4.0"
print(backend_info)         # {'matrix_backend': 'avx2', 'omega_backend': 'C', 'sbox_backend': 'avx2'}
print(get_backend_info_v230())  # full v2.4.0 dict
```

---

## Build & Test

```bash
cd cagoule/c
make clean && make && make tests  # builds + runs all 9 C test binaries
make install                      # copies libcagoule.so to Python package
pip install -e ".[dev]"
pytest tests/ -v                  # 578 Python tests
```

### C Tests — 43,686 assertions, 0 failed

| Binary | Assertions | Coverage |
|---|---|---|
| `test_math` | 117 | `mulmod64`, `addmod64`, `submod64`, `powmod64`, `invmod64` |
| `test_sbox` | 27 | Feistel bijectivity, roundtrip, fallback x^d |
| `test_matrix` | 19 | Vandermonde P×P⁻¹=I, Cauchy fallback |
| `test_cipher` | 28 | CBC roundtrip, PKCS7, diffusion, 1MB bench, pipeline4 |
| `test_omega` | 154 | ζ(2n), HKDF round keys, OpenSSL, thread-safety |
| `test_math_avx2` | 16,489 | `mulmod64x4` / `addmod64x4` / `submod64x4` parity |
| `test_matrix_avx2` | 4,261 | AVX2 Vandermonde parity, roundtrip, bench |
| `test_sbox_avx2` | 22,503 | AVX2 Feistel parity (400K cases), edge, bench |
| **`test_cipher_pipeline4`** | **88** | **Pipeline4 edge cases, residual regression, 10K parity** |

### Python Tests — 578 passed, 0 failed, 18 skipped

### Valgrind — 0 memory leaks across all 7 test binaries

---

## Roadmap

### v2.4.0+ Backlog

- QShell primitives: `cgl encrypt`, `cgl decrypt`, `cgl bench`, `cgl info`
- PyPI `manylinux` wheel
- Formal algebraic specification + IACR ePrint submission
- `cagoule-pass` v2.0.0 with `qshell vault` command
- v3.0.0: CTR mode + multi-block SIMD (>30 MB/s single-core target)

---

## Changelog

### v2.4.0 — 2026-05-16

**C Backend**
- New: Pipeline4 decrypt (`_cbc_decrypt_pipeline4_avx2`) — true 4-way parallel
- New: Encrypt unroll4 + prefetch (`_cbc_encrypt_pipeline4_avx2`)
- Fixed: Decrypt residual loop (n_blocks % 4 != 0) — `saved_r[]` tracking
- New: `test_cipher_pipeline4.c` (88 assertions: edge cases, 10K parity, residual regression)

**Python Layer**
- New: `encrypt_bulk()` / `decrypt_bulk()` — single Argon2id derivation for N messages
- New: Thread-local buffer pool (`_buffer_pool.py`) — +71% single-core in parallel suite
- New: GIL release on heavy C calls (`cagoule_cbc_encrypt`, `matrix_mul`, `sbox_block`)
- Fixed: `_binding.py` struct size (removed oversized `_pad`/AVX2 fields)
- Fixed: `make install` stale `.so` overwrite (`rm -f` before `cp`)
- Fixed: 3 compiler warnings (unused variable, `_POSIX_C_SOURCE` redefined)
- Updated: `__version__` → 2.4.0, `__backend__` → "C (libcagoule.so v2.4.0)"
- Updated: `regenerate_kat.py` → v2.4.0 version strings
- Updated: `tests/test_kat.py` — SHA-256 KAT updated for v2.4.0

### v2.3.0 — 2026-05-08
S-box AVX2 vectorisation; Mersenne-like reduction; cycle-walking AVX2.

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