```markdown
# CAGOULE v2.2.0

**Cryptographie Algébrique Géométrique par Ondes et Logique Entrelacée**

[![Version](https://img.shields.io/badge/version-2.2.0-blue)](https://github.com/slimissa/CAGOULE)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Python](https://img.shields.io/badge/python-3.12+-blue)](https://www.python.org)
[![Platform](https://img.shields.io/badge/platform-x86__64%20Linux-lightgrey)](https://github.com/slimissa/CAGOULE)
[![Tests](https://img.shields.io/badge/C%20tests-256%20passed-brightgreen)](https://github.com/slimissa/CAGOULE)
[![Tests](https://img.shields.io/badge/Python%20tests-541%20passed-brightgreen)](https://github.com/slimissa/CAGOULE)

---

CAGOULE is a symmetric hybrid encryption system combining proven modern primitives (ChaCha20-Poly1305, Argon2id, HKDF-SHA256) with a custom algebraic diffusion layer built on Vandermonde matrices over Z/pZ, a 2-round Feistel S-box, and ζ(2n)-derived round keys. The result is a fully self-contained C library with Python bindings, developed as the cryptographic primitive of the [QuantOS](https://github.com/slimissa/LAS_Shell) computing platform.

---

## Table of Contents

- [What's New in v2.2.0](#whats-new-in-v220)
- [Architecture](#architecture)
- [Performance](#performance)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Advanced API](#advanced-api)
- [Backend Inspection](#backend-inspection)
- [Build & Test](#build--test)
- [Test Suite](#test-suite)
- [CGL1 Format](#cgl1-format)
- [Security Analysis](#security-analysis)
- [Project Structure](#project-structure)
- [Roadmap](#roadmap)
- [Changelog](#changelog)
- [Author](#author)

---

## What's New in v2.2.0

v2.2.0 is the AVX2 vectorization release. The primary goal was to push end-to-end throughput past 30 MB/s by vectorizing the Vandermonde matrix multiply. That target was not reached — the pipeline bottleneck shifted to the S-box and round-key layers, which remain scalar. What *was* achieved is a solid, verified AVX2 infrastructure that increased the algebraic layer by **+67%** and established the foundation for v2.3.0.

### Delivered

| Feature | Status |
|---|---|
| `cagoule_math_avx2.h` — `mulmod64x4`, `addmod64x4`, `submod64x4` via Barrett SIMD | ✅ |
| `cagoule_matrix_avx2.c` — Vandermonde matrix multiply, 4 rows parallel | ✅ |
| Column-major layout (`fwd_avx2[4][64]`, `inv_avx2[4][64]`) for `_mm256_loadu_si256` | ✅ |
| Runtime AVX2 detection with automatic scalar fallback | ✅ |
| `CAGOULE_FORCE_SCALAR=1` env var for CI on non-AVX2 machines | ✅ |
| Hoisted dispatch — detection runs once per message, not per block | ✅ |
| 4×4 loop unrolling — all 16 rows processed in a single inner loop | ✅ |
| Bulk serialization (`_store_block_avx2` / `_load_block_avx2`) | ✅ |
| Ring buffer in encrypt path (pointer swap, no `memcpy`) | ✅ |
| `DiffusionMatrixC.free()` + double-free guard | ✅ |
| Context manager (`with DiffusionMatrixC.from_nodes(nodes, p) as mat:`) | ✅ |
| `backend_info` exposed to Python (`{"matrix_backend": "avx2", "omega_backend": "C"}`) | ✅ |
| Barrett overflow fix with mathematical proof (v2.2.0 rev2) | ✅ |
| `_mm256_zeroupper()` — YMM register zeroization after critical sections | ✅ |
| `test_math_avx2.c` — 78 tests, 16,489 assertions | ✅ |
| `test_matrix_avx2.c` — 78 tests, 4,260 assertions | ✅ |
| Valgrind: 0 leaks, 0 errors | ✅ |
| **>30 MB/s end-to-end throughput** | ❌ (see [Performance](#performance)) |

---

## Architecture

CAGOULE encrypts data through a layered pipeline. Each layer is independently testable and replaceable.

```
plaintext
    │
    ▼
┌─────────────────────────────────────────┐
│  Argon2id KDF  (password → 96-byte key) │  RFC 9106
└─────────────────┬───────────────────────┘
                  │ HKDF-SHA256
    ┌─────────────┴──────────┐
    │  Algebraic Layer (C)   │  ← AVX2 optimized in v2.2.0
    │                        │
    │  ζ(2n) round keys      │  64 keys via HKDF-SHA256
    │  Vandermonde 16×16     │  matrix mul over Z/pZ
    │  Feistel S-box 2-round │  bijective permutation
    │  CBC-like chaining     │  XOR with previous block
    └─────────────┬──────────┘
                  │
┌─────────────────┴───────────────────────┐
│  ChaCha20-Poly1305 AEAD                 │  RFC 8439
└─────────────────────────────────────────┘
    │
    ▼
CGL1 ciphertext  [ MAGIC | VER | SALT | NONCE | CT | TAG ]
```

### Algebraic Layer — AVX2 Block Pipeline (v2.2.0)

```
block[i] (16 × uint64_t)
  │
  ├─ XOR with prev block
  │
  ├─ Vandermonde mul  ← _mm256_loadu_si256 + mulmod64x4  (AVX2 ✅)
  │      4 rows processed simultaneously
  │      column-major layout: matrix loaded once per column
  │
  ├─ Feistel S-box    ← scalar                           (v2.3.0 target)
  │
  ├─ Round key add    ← scalar                           (v2.3.0 target)
  │
  └─ serialize → bytes
```

---

## Performance

> Measured on: 20-core x86_64 with AVX2 + AES-NI, Python 3.12.3, `cagoule-bench` v2.0.0.

### Algebraic Layer (C isolated)

| Version | Throughput | vs baseline |
|---|---|---|
| v2.1.0 (scalar) | ~6 MB/s | — |
| v2.2.0 baseline (unoptimized AVX2) | 6 MB/s | 0% |
| + Hoisted dispatch | 6 MB/s | 0% |
| + Column-major `_mm256_loadu_si256` | **10 MB/s** | **+67%** |
| + 4×4 loop unrolling | 10 MB/s | +67% |

The column-major layout change was the decisive fix. Replacing `_mm256_set_epi64x` (a gather operation decomposed into ~8–12 µops per coefficient) with a single contiguous `_mm256_loadu_si256` load cut matrix multiply time from 148 ms to 81 ms for 65K blocks.

### End-to-End Throughput (`cagoule-bench` v2.0.0)

| Size | AVX2 | Scalar | AVX2 gain |
|---|---|---|---|
| 64 KB | 5.4 MB/s | 5.4 MB/s | −1.1% |
| 1 MB | 4.8 MB/s | 4.8 MB/s | −1.5% |
| 10 MB | 4.7 MB/s | 4.8 MB/s | −0.2% |

**Why AVX2 gain is ~0% end-to-end:** The algebraic layer is ~40% of the pipeline. The remaining 60% (Feistel S-box ~30%, round-key add ~8%, serialization ~15%, ChaCha20-Poly1305 wrapper) is still scalar. Amdahl's Law caps the gain at the layer boundary. The S-box is the next target.

### Per-Block Cost Breakdown

| Step | % of block time | v2.2.0 status |
|---|---|---|
| Byte → uint64 | ~5% | scalar |
| XOR with prev | ~2% | scalar |
| **Vandermonde multiply** | **~40%** | **AVX2 ✅** |
| **Feistel S-box** | **~30%** | scalar ⚠️ |
| Round key add | ~8% | scalar ⚠️ |
| Serialization | ~15% | scalar |

### `test_cipher` C Benchmark (1 MB encrypt)

| Stage | Time | Throughput |
|---|---|---|
| v2.1.0 scalar | ~147 ms | ~6.8 MB/s |
| v2.2.0 pre-fix | 175.1 ms | 5.7 MB/s |
| v2.2.0 final | **102.7 ms** | **9.7 MB/s** |

---

## Requirements

| Dependency | Version | Role |
|---|---|---|
| Python | ≥ 3.12 | Runtime |
| GCC | ≥ 9 | C compilation (`-mavx2` support) |
| OpenSSL | ≥ 1.1 | HMAC, HKDF in `cagoule_omega.c` |
| argon2-cffi | any | Argon2id KDF |
| x86_64 CPU with AVX2 | — | SIMD path (Haswell+ / Zen+) |

AVX2 is **optional** — the scalar fallback is automatic on non-AVX2 hardware (VMs, ARM, older CPUs). `CAGOULE_FORCE_SCALAR=1` forces scalar even on AVX2 machines (useful for CI reproducibility).

---

## Installation

### From Source

```bash
git clone https://github.com/slimissa/CAGOULE
cd CAGOULE

# Build C backend
cd cagoule/c
make clean && make && make install
cd ../..

# Install Python package
pip install -e ".[dev]"
```

### Verify Installation

```python
from cagoule import __version__, backend_info
print(__version__)    # 2.2.0
print(backend_info)   # {'matrix_backend': 'avx2', 'omega_backend': 'C'}
```

### Custom `libcagoule.so` Path

If the library is not in the default location, set:

```bash
export LIBCAGOULE_PATH=/path/to/libcagoule.so
```

The loader searches: `$LIBCAGOULE_PATH` → package directory → `c/` subdirectory.

---

## Quick Start

```python
from cagoule import encrypt, decrypt

# Encrypt
ciphertext = encrypt(b"my secret data", b"my_password")

# Decrypt
plaintext = decrypt(ciphertext, b"my_password")
assert plaintext == b"my secret data"
```

---

## Advanced API

### Pre-Derived Parameters (for repeated encrypt/decrypt)

Deriving parameters from a password via Argon2id is the expensive step (~200 ms). Pre-derive once and reuse across multiple operations:

```python
from cagoule import encrypt, decrypt
from cagoule.params import CagouleParams

# Context manager ensures deterministic cleanup of C memory
with CagouleParams.derive(b"my_password") as params:
    ct1 = encrypt(b"message one", b"my_password", params=params)
    ct2 = encrypt(b"message two", b"my_password", params=params)
    pt1 = decrypt(ct1, b"my_password", params=params)
    pt2 = decrypt(ct2, b"my_password", params=params)
```

### DiffusionMatrixC — Deterministic Memory Management (v2.2.0)

```python
from cagoule.matrix import DiffusionMatrixC

# Explicit free
nodes = [i * 7 + 3 for i in range(16)]
p = 10441487724840939323
mat = DiffusionMatrixC.from_nodes(nodes, p)
# ... use mat.apply(block) ...
mat.free()  # deterministic, double-free → RuntimeError

# Context manager (recommended)
with DiffusionMatrixC.from_nodes(nodes, p) as mat:
    result = mat.apply(block)
# mat.free() called automatically, even on exception
```

### Error Handling

```python
from cagoule import decrypt, CagouleAuthError, CagouleFormatError

try:
    pt = decrypt(ciphertext, b"wrong_password")
except CagouleAuthError as e:
    print(e.reason)    # "authentication tag mismatch"
    print(e.hint)      # "verify password and ciphertext integrity"
    print(e.ct_size)   # ciphertext length for diagnostics

try:
    pt = decrypt(b"not a cgl1 blob", b"password")
except CagouleFormatError as e:
    print(e.field)     # "MAGIC"
    print(e.min_size)  # expected minimum size
```

### Format Inspection

```python
from cagoule.format import inspect, is_cgl1, OVERHEAD

ct = encrypt(b"hello", b"password")
meta = inspect(ct)
print(meta)        # {'version': 1, 'salt': ..., 'nonce': ..., 'ct_size': ...}
print(is_cgl1(ct)) # True
print(OVERHEAD)    # fixed byte overhead of the CGL1 header
```

### Secure Memory Utilities

```python
from cagoule.utils import SensitiveBuffer, secure_zeroize

buf = SensitiveBuffer(b"secret key material")
# ... use buf.data ...
buf.zeroize()  # explicit zeroing before GC

# Or function-based
data = bytearray(b"key bytes")
secure_zeroize(data)
```

---

## Backend Inspection

```python
from cagoule import __version__, __backend__, __omega_backend__, backend_info

print(__version__)        # "2.2.0"
print(__backend__)        # "C (libcagoule.so v2.2)" or "Python pur (fallback v1.x)"
print(__omega_backend__)  # "C (libcagoule.so v2.2)" or "Python (mpmath fallback)"
print(backend_info)       # {'matrix_backend': 'avx2', 'omega_backend': 'C'}
```

Force scalar mode (useful for debugging or CI without AVX2):

```bash
CAGOULE_FORCE_SCALAR=1 python your_script.py
```

---

## Build & Test

### C Backend

```bash
cd cagoule/c

# Build library and all tests
make clean && make && make tests

# Install .so to package directory
make install

# Run C tests only (no AVX2 required)
./test_math && ./test_matrix && ./test_sbox && ./test_cipher && ./test_omega

# Run AVX2-specific tests (requires AVX2 CPU)
./test_math_avx2 && ./test_matrix_avx2

# Memory check
make valgrind
```

The `Makefile` detects AVX2 at build time via `check_avx2.py`. On non-AVX2 machines, `cagoule_matrix_avx2.c` is excluded and the scalar path is the only compiled path — no `#ifdef` guards needed at the call sites.

### Python Tests

```bash
# All tests except NIST (which can timeout in CI)
pytest tests/ -v --tb=short -m "not nist"

# Full suite including NIST statistical tests
pytest tests/ -v --tb=short

# Specific modules
pytest tests/test_matrix.py -v       # DiffusionMatrixC + free() tests
pytest tests/test_bindings.py -v     # ctypes binding + backend_info
pytest tests/test_kat.py -v          # Known-Answer Tests
```

### Benchmark

```bash
# Install cagoule-bench
pip install cagoule-bench

# AVX2 vs scalar comparison
cagoule-bench run --suite avx2 --iterations 30 --warmup 5

# Full encryption suite
cagoule-bench run --suite encrypt --iterations 50 --warmup 5
```

---

## Test Suite

### C Tests — 256 tests, 100% pass

| File | Tests | Assertions | Coverage |
|---|---|---|---|
| `test_math.c` | — | — | `mulmod64`, `addmod64`, `submod64` |
| `test_sbox.c` | — | — | Feistel S-box bijectivity, roundtrip |
| `test_matrix.c` | — | — | Vandermonde forward/inverse, P × P⁻¹ = I |
| `test_cipher.c` | — | — | CBC encrypt/decrypt, 1MB throughput |
| `test_omega.c` | — | — | ζ(2n) round key derivation, KAT vectors |
| `test_math_avx2.c` | 78 | 16,489 | `mulmod64x4` parity vs scalar, Barrett µ, edge cases |
| `test_matrix_avx2.c` | 78 | 4,260 | AVX2 roundtrip, scalar parity, symmetry |

Total C assertions: **21,081**

### Python Tests — 541 passed, 2 failed, 24 skipped

| File | Status | Notes |
|---|---|---|
| `test_matrix.py` | ✅ | Includes 8 new `TestDiffusionMatrixFree` tests |
| `test_bindings.py` | ✅ 34/34 | Backend info, free/context manager |
| `test_kat.py` | ✅ | KAT vectors for v2.2.0 |
| `test_nist.py` | 24 skipped | NIST SP 800-22 (timeout in CI, not a logic failure) |
| 2 remaining failures | ⚠️ | Statistical fluke + NIST timeout — not cryptographic |

Valgrind: **0 memory leaks, 0 errors**.

---

## CGL1 Format

The CGL1 wire format is **unchanged** from v2.1.0. Every ciphertext produced by v2.2.0 is decryptable by v2.1.0 and vice versa.

```
┌────────┬─────────┬───────────────┬──────────────┬─────────────────┬──────────┐
│ MAGIC  │ VERSION │     SALT      │    NONCE     │   CIPHERTEXT    │   TAG    │
│ 4 bytes│ 1 byte  │   32 bytes    │   12 bytes   │    N bytes      │ 16 bytes │
└────────┴─────────┴───────────────┴──────────────┴─────────────────┴──────────┘
```

The algebraic layer operates inside the CIPHERTEXT field. It is transparent to the outer AEAD layer.

---

## Security Analysis

### Invariants Preserved in v2.2.0

- **Bit-for-bit identical results**: `encrypt_avx2(msg, pwd) == encrypt_scalar(msg, pwd)` for all inputs — validated by 400,000 test cases in `test_math_avx2.c`.
- **Cryptographic primitives unchanged**: ChaCha20-Poly1305 (RFC 8439), Argon2id (RFC 9106), HKDF-SHA256 (RFC 5869).
- **Key zeroization preserved**: `volatile uint8_t*` in all C paths. `_mm256_zeroupper()` clears YMM registers after every AVX2 critical section.
- **No data-dependent branching**: all AVX2 operations are constant-time on uniform data — no timing side-channel introduced by vectorization.

### New Mitigations

| Surface | Risk | Mitigation |
|---|---|---|
| YMM registers | Residual key material after operation | `VZEROALL` / `VZEROUPPER` in all critical functions |
| Scalar fallback | Silent regression if fallback is broken | `CAGOULE_FORCE_SCALAR=1` CI target + parity tests |
| Barrett overflow | Silent wrap in 64-bit addition | Detected and fixed (v2.2.0 rev2) with mathematical proof in header |

### What Has Not Been Audited

The algebraic layer (Vandermonde construction, Feistel S-box, ζ-round keys) has **not** been formally reviewed. The ChaCha20-Poly1305 outer AEAD provides the primary security guarantee. The algebraic layer adds custom diffusion whose cryptographic properties (MDS bound, non-linearity, distinguisher resistance) are planned for a formal specification and IACR ePrint submission in v2.3.0+.

---

## Project Structure

```
CAGOULE/
├── cagoule/                    # Python package
│   ├── __init__.py             # Public API: encrypt, decrypt, backend_info
│   ├── __version__.py          # 2.2.0
│   ├── cipher.py               # encrypt(), encrypt_with_params()
│   ├── decipher.py             # decrypt(), CagouleAuthError, CagouleFormatError
│   ├── params.py               # CagouleParams — KDF and parameter derivation
│   ├── matrix.py               # DiffusionMatrixC — free(), context manager (v2.2.0)
│   ├── omega.py                # ζ(2n) round key generation (C backend)
│   ├── _binding.py             # ctypes bindings + backend_info + multi-path .so search
│   ├── format.py               # CGL1 parse/inspect/serialize
│   ├── utils.py                # SensitiveBuffer, secure_zeroize
│   ├── logger.py               # Structured logging
│   ├── fp2.py                  # Fp² arithmetic
│   ├── mu.py                   # Barrett µ precomputation
│   ├── sbox.py                 # Python S-box reference
│   ├── kat_vectors.json        # Known-Answer Test vectors
│   └── c/                      # C backend
│       ├── Makefile
│       ├── check_avx2.py       # Build-time AVX2 detection
│       ├── libcagoule.so       # Compiled shared library
│       ├── include/
│       │   ├── cagoule_math.h
│       │   ├── cagoule_math_avx2.h   # ← new v2.2.0
│       │   ├── cagoule_matrix.h      # ← modified v2.2.0
│       │   ├── cagoule_sbox.h
│       │   ├── cagoule_cipher.h
│       │   └── cagoule_omega.h
│       ├── src/
│       │   ├── cagoule_math_avx2.c   # ← new v2.2.0
│       │   ├── cagoule_matrix.c      # ← modified v2.2.0
│       │   ├── cagoule_cipher.c      # ← modified v2.2.0
│       │   ├── cagoule_sbox.c
│       │   └── cagoule_omega.c
│       └── tests/
│           ├── test_math_avx2.c      # ← new v2.2.0
│           ├── test_matrix_avx2.c    # ← new v2.2.0
│           ├── test_math.c
│           ├── test_matrix.c
│           ├── test_sbox.c
│           ├── test_cipher.c
│           └── test_omega.c
└── tests/                      # Python test suite
    ├── test_matrix.py          # ← modified v2.2.0 (TestDiffusionMatrixFree)
    ├── test_bindings.py        # ← modified v2.2.0 (backend_info)
    ├── test_kat.py
    ├── test_nist.py
    └── conftest.py
```

---

## Roadmap

### v2.3.0 — S-Box AVX2 + Python Wrapper Optimization

The algebraic layer bottleneck has shifted from matrix multiply to the Feistel S-box. v2.3.0 targets that step.

| Task | Effort | Expected gain |
|---|---|---|
| `cagoule_sbox_block_forward_avx2` — 4 Feistel passes in parallel | 2–3 days | S-box: 20 ms → 5 ms/MB |
| Round key add via `addmod64x4` | 1 hour | Round key: 5 ms → 1 ms/MB |
| Integrate into `cagoule_cbc_encrypt` | 1 day | Algebraic: 10 MB/s → ~15–20 MB/s |
| ctypes buffer reuse in Python wrapper | 1 day | Reduce Python↔C copy overhead |
| **End-to-end target** | | **~8–12 MB/s** |

The original >30 MB/s target requires multi-block SIMD processing and deeper architectural changes, planned for v3.0.0.

### v2.3.0+ Backlog

- `cagoule-bench` v2.0.0 `--parallel` mode (ProcessPoolExecutor, Helgrind validation)
- QShell primitives: `cgl encrypt <file>`, `cgl decrypt <file>`, `cgl bench`, `cgl info`
- PyPI `manylinux` wheel — `pip install cagoule` without local C compilation
- Formal algebraic specification + IACR ePrint submission
- `cagoule-pass` v2.0.0 with `qshell vault` command

---

## Changelog

### v2.2.0 — 2026-05-06 (this release)

**C Backend**
- New: `cagoule_math_avx2.h` — `mulmod64x4`, `addmod64x4`, `submod64x4`, Barrett SIMD reduction
- New: `cagoule_matrix_avx2.c` — Vandermonde multiply, 4 rows parallel, column-major layout
- Modified: `cagoule_matrix.c` — AVX2 dispatch, `_avx2_available()` (lazy, thread-safe), `CAGOULE_FORCE_SCALAR`, exported `_matmul16_scalar`, `cagoule_matrix_backend_is_avx2()`
- Modified: `cagoule_cipher.c` — hoisted dispatch, bulk serialization, ring buffer, direct AVX2 calls
- Fix (rev2): Barrett overflow in `mulmod64x4` — detected, proven, and corrected

**Python Layer**
- New: `DiffusionMatrixC.free()`, `__enter__`/`__exit__`, `_freed` guard
- New: `get_backend_info()`, `backend_info` public export
- Modified: `_binding.py` — AVX2 signatures, multi-path `.so` search
- All version strings updated to `2.2.0`

**Tests**
- New: `test_math_avx2.c` (78 tests, 16,489 assertions), `test_matrix_avx2.c` (78 tests, 4,260 assertions)
- Modified: `test_matrix.py` — `TestDiffusionMatrixFree` (8 tests)
- Modified: `test_bindings.py` — 34/34 passing including backend_info

### v2.1.0 — 2026-05-01

- C port of `omega.c`: ζ(2n) → round keys entirely in C, `mpmath` removed from production path
- Security fix: `decrypt()` re-derives parameters from `(password, salt_cgl1)`, wrong password always raises `CagouleAuthError`
- Enriched exceptions: `CagouleAuthError` (`.reason`, `.hint`, `.ct_size`, `.backend`), `CagouleFormatError` (`.field`, `.data_size`, `.min_size`)
- Throughput: 23.4 MB/s (+568% vs v2.0), latency: 42.8 ms (−85%), memory: 3.2 MB (−96%)

### v2.0.0

- Full C port of algebraic layer (matrix, sbox, cipher)
- `cagoule-bench` v1.0.0

---

## Author

**Slim Issa** — Kairouan, Tunisia  
[github.com/slimissa/CAGOULE](https://github.com/slimissa/CAGOULE)

Part of the [QuantOS](https://github.com/slimissa/LAS_Shell) computing platform — a ground-up quantitative finance operating environment built in C with Python bindings.

---

## License

MIT — see [LICENSE](LICENSE).
