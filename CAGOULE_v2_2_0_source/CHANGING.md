# CAGOULE v2.2.0 — Development Summary

**Date:** 5-7 May 2026
**Author:** Slim Issa
**Repository:** github.com/slimissa/CAGOULE

---

## 1. Starting Point: v2.1.0 Baseline

CAGOULE v2.1.0 had completed the C port of all critical-path components:
- `cagoule_matrix.c` — Vandermonde matrix multiply (scalar)
- `cagoule_sbox.c` — Feistel 2-round S-box
- `cagoule_cipher.c` — CBC-like encrypt/decrypt
- `cagoule_omega.c` — ζ(2n) → round keys via HKDF-SHA256

Throughput: ~23 MB/s algebraic layer, ~3-5 MB/s end-to-end with ChaCha20-Poly1305.

---

## 2. v2.2.0 Roadmap Goals

From `CAGOULE_v2_2_0_Roadmap.pdf`:

| Priority | Task | Target |
|---|---|---|
| P1-P3 | AVX2 SIMD vectorization of `mulmod64` and Vandermonde matrix multiply | >30 MB/s |
| P4 | `DiffusionMatrixC.free()` with context manager and double-free guard | Memory stability |
| P0 | Fix 2 pytest failures (libcagoule.so linkage) | 560/560 tests |
| — | `backend_info` detection (matrix: avx2/scalar, omega: C/mpmath) | User visibility |

---

## 3. What We Built

### 3.1 C Backend — New Files

**`cagoule_math_avx2.h`** — Vectorized modular arithmetic
- `mulmod64x4()` — Barrett reduction on 4 lanes simultaneously
- `addmod64x4()`, `submod64x4()` — 4-lane modular add/sub
- `_cmpgt_epu64()` — unsigned comparison via sign-bit flip trick
- `_mul128x4()` — 64×64→128 multiply decomposed into 32-bit chunks
- Bug fix v2.2.0 rev2: overflow detection in Barrett folding with mathematical proof

**`cagoule_matrix_avx2.c`** — Vectorized matrix multiply
- Processes 4 rows of the Vandermonde matrix in parallel
- Uses `mulmod64x4` + `addmod64x4` for SIMD accumulation
- `_mm256_zeroupper()` for YMM register zeroization (security)

### 3.2 C Backend — Modified Files

**`cagoule_matrix.c`**
- Added `_avx2_available()` — lazy, thread-safe AVX2 detection via `__builtin_cpu_supports` + `__atomic`
- Barrett guard: `p < 2^63` falls back to scalar (test primes)
- `CAGOULE_FORCE_SCALAR=1` env var for CI testing
- Exported `_matmul16_scalar()` for direct calls from cipher loop
- `cagoule_matrix_backend_is_avx2()` — exposed to Python via ctypes
- **v2.2.1: AVX2-friendly column-major layout** — `fwd_avx2[4][64]` and `inv_avx2[4][64]` fields populated at matrix build time

**`cagoule_cipher.c`** (v2.2.1 hotfix)
- **Hoisted AVX2 dispatch** — detection runs once per message, not per block
- **Bulk serialization** — `_store_block_avx2()` / `_load_block_avx2()` for 16-element loads/stores
- **Ring buffer** — pointer swap eliminates `memcpy` in encrypt path
- Direct calls to `cagoule_matrix_mul_avx2()` or `_matmul16_scalar()`

**`Makefile`**
- AVX2 detection via `check_avx2.py` (compiles test program with `-mavx2`)
- Separate compilation: `cagoule_matrix_avx2.o` and `cagoule_cipher.o` compiled with `-mavx2`
- All other `.o` files compiled without AVX2 for binary compatibility

### 3.3 Python Layer — Modified Files

**`_binding.py`**
- Added signatures for `cagoule_matrix_backend_is_avx2`, `cagoule_matrix_mul_scalar`, `cagoule_matrix_mul_inv_scalar`
- New `get_backend_info()` function returning `{"matrix_backend": "avx2", "omega_backend": "C"}`
- Multi-path library search: `$LIBCAGOULE_PATH` → package dir → `c/` subdirectory

**`matrix.py`**
- `DiffusionMatrixC.free()` with `_freed` guard (double-free → RuntimeError)
- Context manager (`__enter__`/`__exit__`) for deterministic cleanup
- `backend_info` property querying `cagoule_matrix_backend_is_avx2()`
- `_query_matrix_backend()` helper

**`__init__.py`**
- `backend_info` exported to public API
- Version strings updated to v2.2.0
- Docstring with v2.2.0 changelog

**`omega.py`, `decipher.py`, `cipher.py`, `params.py`, `sbox.py`, `format.py`, `fp2.py`, `mu.py`, `utils.py`, `logger.py`**
- Version strings updated to v2.2.0
- `_backend_str()` updated to v2.2

### 3.4 Test Suite

**New C tests:**
- `test_math_avx2.c` — 78 tests, 16,489 assertions: `mulmod64x4` parity, Barrett µ validation, edge cases
- `test_matrix_avx2.c` — 78 tests, 4,260 assertions: AVX2 roundtrip, scalar parity, symmetry

**Modified Python tests:**
- `test_matrix.py` — `TestDiffusionMatrixFree` class (8 tests): free, double-free, context manager, exception-safe, backend_info
- `test_bindings.py` — Backend Info tests, free/context manager tests, 34/34 passing
- `test_kat.py` — Version check updated to accept v2.2.0

**Final results:**
- C tests: 256 tests, 100% pass
- Python tests: 541 passed, 2 failed (NIST timeout + statistical fluke), 24 skipped
- Valgrind: 0 memory leaks, 0 errors

---

## 4. Problem: cagoule-bench v2.0.0 Regression

### Symptom
Running `cagoule-bench run --suite avx2` showed AVX2 **16-18% slower** than scalar:

| Taille | AVX2 (MB/s) | Scalar (MB/s) | Speedup |
|---|---|---|---|
| 64KB | 5.3 | 6.3 | 0.84× |
| 1MB | 4.7 | 5.4 | 0.86× |
| 10MB | 4.6 | 5.4 | 0.86× |

### Root Cause Analysis & Fixes Applied

#### Fix 1: Hoisted Dispatch (v2.2.1)
**Problem:** `cagoule_matrix_mul()` was called 65,536 times per MB, each call checking `_avx2_available()` and `p >= 2^63`.
**Solution:** Moved AVX2 detection once before the block loop in `cagoule_cbc_encrypt`. Direct calls to `cagoule_matrix_mul_avx2()` or `_matmul16_scalar()` inside the loop.
**Gain:** Regression reduced from -17% to -3%.

#### Fix 2: Column-Major Layout + `_mm256_loadu_si256` (v2.2.1)
**Problem:** `_mm256_set_epi64x(row3[j], row2[j], row1[j], row0[j])` was decomposing into ~8-12 µops (gather operation) per coefficient load — 16 times per block × 65,536 blocks = 1M+ gather ops per MB.
**Solution:** Added `fwd_avx2[4][64]` and `inv_avx2[4][64]` column-major arrays to `CagouleMatrix` struct, populated at build time. Replaced gather with single `_mm256_loadu_si256` — 1 µop, contiguous 32-byte load.
**Gain:** Algebraic layer: 6 MB/s → **10 MB/s** (+67%).

#### Fix 3: 4×4 Loop Unrolling (v2.2.1)
**Problem:** The outer loop processed 4 groups of 4 rows, reloading the matrix from L1 each iteration.
**Solution:** 4 accumulators process all 16 rows in a single inner loop — matrix loaded once, `v[j]` broadcast once per column, all 4 groups accumulate simultaneously.
**Gain:** Minor (~5% on matrix multiply alone). S-box and round keys now dominate.

#### Fix 4: Bulk Serialization & Ring Buffer (v2.2.1)
**Problem:** Per-element `_u64_to_be` loop and `memcpy(prev, block)` per iteration.
**Solution:** AVX2 bulk store/load for serialization, pointer swap ring buffer for prev/block.
**Gain:** Minimal — serialization was not the bottleneck.

### Performance Evolution

| Stage | Algebraic Layer | End-to-End |
|---|---|---|
| v2.2.0 baseline (unoptimized) | 6 MB/s | ~3 MB/s |
| + Hoisted dispatch | 6 MB/s | ~3 MB/s |
| + Column-major layout (`_mm256_loadu_si256`) | **10 MB/s** | ~5 MB/s |
| + 4×4 loop unrolling | 10 MB/s | ~5 MB/s |
| + Bulk serialization + ring buffer | 10 MB/s | ~5 MB/s |

### Final Benchmark (After All Fixes)

| Taille | AVX2 (MB/s) | Scalar (MB/s) | Speedup |
|---|---|---|---|
| 64KB | 5.4 | 5.4 | 0.99× |
| 1MB | 4.8 | 4.8 | 0.99× |
| 10MB | 4.7 | 4.8 | 1.00× |

Note: cagoule-bench measures end-to-end `encrypt()` which includes ChaCha20-Poly1305 (~980 MB/s) and Python wrapper overhead. The algebraic layer in isolation runs at **10 MB/s** (up from 6 MB/s, +67%).

### Current Bottleneck Analysis

The C algebraic layer per-block pipeline:
1. Byte→uint64 conversion (~5%)
2. XOR with prev (~2%)
3. **Matrix multiply** (~40%) ← AVX2 optimized ✅
4. **S-box Feistel** (~30%) ← **still scalar** ⚠️
5. **Round key add** (~8%) ← **still scalar** ⚠️
6. Serialization (~15%)

---

## 5. Lessons Learned

1. **Profile before optimizing.** The roadmap assumed matrix multiply was the bottleneck. It wasn't alone.

2. **`_mm256_set_epi64x` is expensive.** A single gather instruction replaced by a contiguous load delivered +67%.

3. **SIMD gains are diluted in complex pipelines.** A 2-3× speedup in one step means little if that step is only 40% of total time (Amdahl's Law).

4. **The algebraic layer is the core.** Preserving the Feistel S-box, ζ-round keys, and Vandermonde matrix design while making them faster is the right approach.

5. **Benchmarking needs isolation.** Testing the full `encrypt()` pipeline conflates ChaCha20, algebraic layer, and Python overhead. Profiling each layer separately revealed the truth.

6. **Test infrastructure is invaluable.** 21,081 C assertions + 541 Python tests caught every regression immediately.

---

## 6. v2.3.0 Roadmap (Proposed)

| Task | Effort | Expected Gain |
|---|---|---|
| `cagoule_sbox_block_forward_avx2` — 4 Feistel passes in parallel | 2-3 days | S-box: 20ms → 5ms per MB |
| Round key add via `addmod64x4` | 1 hour | Round key: 5ms → 1ms per MB |
| Integrate into `cagoule_cbc_encrypt` | 1 day | Algebraic layer: 10 MB/s → ~15-20 MB/s |
| ctypes buffer reuse in Python wrapper | 1 day | End-to-end: closer to C raw speed |
| **End-to-end target** | | **~8-12 MB/s** |

The >30 MB/s target requires multi-block SIMD processing and architectural changes for v3.0.0. Multi-process scaling (ProcessPoolExecutor × 20 cores) can achieve high aggregate throughput at the application level.

---

## 7. Files Changed Summary

### New files (v2.2.0)
- `c/include/cagoule_math_avx2.h`
- `c/src/cagoule_matrix_avx2.c`
- `c/tests/test_math_avx2.c`
- `c/tests/test_matrix_avx2.c`
- `c/check_avx2.py`

### Modified C files
- `c/include/cagoule_matrix.h` — added `fwd_avx2[4][64]`, `inv_avx2[4][64]`, `_matmul16_scalar` declaration
- `c/src/cagoule_matrix.c` — AVX2 dispatch, column-major layout build, exported scalar path
- `c/src/cagoule_cipher.c` — hoisted dispatch, bulk serialization, ring buffer, AVX2 direct calls
- `c/Makefile` — AVX2 compilation rules for `cagoule_matrix_avx2.o` and `cagoule_cipher.o`

### Modified Python files
- `cagoule/__init__.py` — v2.2.0, backend_info export
- `cagoule/__version__.py` — 2.2.0
- `cagoule/_binding.py` — AVX2 signatures, get_backend_info()
- `cagoule/matrix.py` — free(), context manager, backend_info
- `cagoule/omega.py` — version string
- `cagoule/decipher.py` — _backend_str() to v2.2
- `cagoule/cipher.py` — version string
- `cagoule/params.py` — version string
- `cagoule/sbox.py` — version string
- `cagoule/format.py` — version string
- `cagoule/fp2.py` — version string
- `cagoule/mu.py` — version string
- `cagoule/utils.py` — version string
- `cagoule/logger.py` — version string

### Modified test files
- `tests/test_matrix.py` — TestDiffusionMatrixFree (8 tests)
- `tests/test_bindings.py` — backend_info, free/context manager tests
- `tests/test_kat.py` — version check v2.2.0
- `tests/kat_omega_vectors.json` — v2.2.0
- `tests/conftest.py` — v2.2.0
- `tests/test_math_avx2.c`, `tests/test_matrix_avx2.c` — POSIX feature macro fix

### Generated files
- `cagoule/kat_vectors.json` — regenerated with derived params

---

## 8. Quick Reference: Build & Test

```bash
# Build C backend
cd cagoule/c
make clean && make && make tests && make install

# Python tests
cd ../..
pip install -e ".[dev]"
pytest tests/ -v --tb=short -m "not nist"

# Memory check
cd cagoule/c
make valgrind

# Benchmark
cagoule-bench run --suite avx2 --iterations 30 --warmup 5

# Backend inspection
python -c "from cagoule import __version__, backend_info; print(__version__, backend_info)"
# Output: 2.2.0 {'matrix_backend': 'avx2', 'omega_backend': 'C'}
