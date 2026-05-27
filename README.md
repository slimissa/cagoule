# CAGOULE v2.5.4

**Cryptographie Algébrique Géométrique par Ondes et Logique Entrelacée**

[![Version](https://img.shields.io/badge/version-2.5.7-blue)](https://github.com/slimissa/cagoule)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Python](https://img.shields.io/badge/python-3.12+-blue)](https://www.python.org)
[![Platform](https://img.shields.io/badge/platform-x86__64%20%7C%20AMD64%20%7C%20ARM64-lightgrey)](https://github.com/slimissa/cagoule)
[![C Tests](https://img.shields.io/badge/C%20tests-4,088,031%20passed-brightgreen)](https://github.com/slimissa/cagoule)
[![Python Tests](https://img.shields.io/badge/Python%20tests-578%20passed-brightgreen)](https://github.com/slimissa/cagoule)
[![CI](https://img.shields.io/badge/CI-multi--arch%20(x86__64%20%2B%20ARM64)-blue)](https://github.com/slimissa/cagoule/actions)

---

CAGOULE is a symmetric hybrid encryption system combining ChaCha20-Poly1305, Argon2id, HKDF-SHA256 with a custom algebraic diffusion layer (Vandermonde over Z/pZ, 2-round Feistel S-box, ζ(2n)-derived round keys). The C backend is fully AVX2-vectorised.

**Platform Support**: Intel x86-64 ✅ · AMD Ryzen/EPYC ✅ · ARM64 (QEMU) ✅ · Apple Silicon ✅

---

## What's New in v2.5.4

v2.5.7 is the culmination of the v2.5.x **Mersenne Acceleration** cycle, adding security hardening, fuzz testing, threat modeling, and multi-arch CI to the core v2.5.0 performance improvements.

### Core v2.5.0 Features

| Feature | Status |
|---|---|
| **P0** — Mersenne-64 prime pool (8 primes, HKDF-selected) | ✅ |
| **P0** — `mulmod_mersenne64x4` AVX2 (13 instructions vs 22 Barrett) | ✅ |
| **P1** — Option A dual accumulator (even/odd split, depth 8 vs 16) | ✅ |
| **P2** — Z-Domain Shifting in C-layer (+32% Python e2e) | ✅ |
| **P3** — `encrypt_bulk()` / `decrypt_bulk()` — single Argon2id derivation | ✅ |
| **P4** — Thread-local buffer pool (+71% single-core parallel) | ✅ |
| GIL release on heavy C calls | ✅ |
| `test_mersenne.c` — 4,000,032 assertions across all 8 primes | ✅ |
| ARCHITECTURE.md — complete data-flow diagram and design decisions | ✅ |

### v2.5.1–v2.5.3: Stability & Coverage

| Feature | Version |
|---|---|
| AVX2 runtime detection fix (`/proc/cpuinfo`, 4096-byte buffer) | v2.5.1 |
| Z-Domain Shifting tests in `test_cipher.c` (10 assertions) | v2.5.1 |
| Mersenne pool lookup tests in `test_math.c` (30 assertions) | v2.5.1 |
| Mersenne AVX2 parity tests in `test_math_avx2.c` (+16,544 assertions) | v2.5.2 |
| Mersenne matrix parity tests in `test_matrix_avx2.c` (+27,656 assertions) | v2.5.2 |
| Z-Domain pipeline4 tests in `test_cipher_pipeline4.c` (+21 assertions) | v2.5.2 |
| Zero round-key + Mersenne S-box tests in `test_sbox.c` (+20 assertions) | v2.5.2 |
| Mersenne pool roundtrip tests in `test_matrix.c` (+33 assertions) | v2.5.2 |
| Z-Domain doc fix, version strings, Mersenne benchmark suite | v2.5.3 |

### v2.5.4: Security Hardening

| Priority | Feature | Version |
|----------|---------|---------|
| **P0** | Z-Domain inline — no malloc in encrypt hot path | v2.5.4 |
| **P1** | dudect constant-time empirical validation | v2.5.4 |
| **P2** | libFuzzer harness — 500K clean runs, 0 crashes | v2.5.5 |
| **P3** | SECURITY.md — complete threat model and security policy | v2.5.6 |
| **P4** | CI multi-arch matrix (x86_64 native + ARM64 QEMU) | v2.5.7 |

---

## Performance

### Python API (cagoule-bench v2.0.0, 30 iterations)

| Test | v2.4.0 | v2.5.x | Improvement |
|------|--------|--------|-------------|
| encrypt-1KB | 5.8 MB/s | **7.1 MB/s** | **+22%** |
| encrypt-8KB | — | **8.1 MB/s** | — |
| encrypt-1MB | 5.2 MB/s | **6.9 MB/s** | **+33%** |
| encrypt-10MB | 5.1 MB/s | **6.8 MB/s** | **+33%** |
| decrypt-1MB | 4.6 MB/s | **6.0 MB/s** | **+30%** |
| decrypt-10MB | 8.5 MB/s | **14.4 MB/s** | **+69%** |

### C Layer (65,536 blocks ≡ 1 MB)

| Metric | v2.4.0 | v2.5.x |
|--------|--------|--------|
| C encrypt | 8.0 MB/s | **10.8 MB/s** |
| C decrypt | 8.1 MB/s | **10.8 MB/s** |
| Ratio dec/enc | 0.99× | **1.00×** |
| Matrix forward | ~1400 ns/bloc | **47 ms total** |
| S-box AVX2 | 70.1 MB/s | **87.9 MB/s** |

### Parallel Scaling (encrypt_bulk + ProcessPoolExecutor)

| Workers | Throughput | Speedup | Efficiency |
|---------|------------|---------|------------|
| 1 | ~3.8 MB/s | 1.00× | — |
| 2 | **8.1 MB/s** | 2.15× | 107% |
| 4 | **14.4 MB/s** | 3.81× | 95% |
| 8 | **23.7 MB/s** | 6.26× | 78% |
| 16 | **29.2 MB/s** | 7.72× | 48% |

### Streaming (64 KB chunks)

| Size | CAGOULE | AES-256-GCM | ChaCha20-Poly1305 |
|------|---------|-------------|-------------------|
| 50 MB | **7.9 MB/s** | 456 MB/s | 403 MB/s |
| 100 MB | **7.9 MB/s** | 457 MB/s | 402 MB/s |
| 500 MB | **7.8 MB/s** | 458 MB/s | 403 MB/s |

### KDF (Argon2id)

| Configuration | Latency | OWASP |
|---------------|---------|-------|
| t=3, m=64MB, p=1 | 113.7 ms | ✅ Production |
| t=3, m=64MB, p=4 | 51.4 ms | ✅ Multi-core |
| t=5, m=128MB, p=4 | 152.2 ms | ✅ High security |

---

## Platform Support

| Platform | SIMD | Status | CI Validated |
|----------|------|--------|--------------|
| **Intel x86-64** | AVX2 | ✅ Full acceleration | ✅ GitHub Actions |
| **AMD Ryzen/EPYC** | AVX2 (Zen 2+) | ✅ Full acceleration | ⚠️ Same arch, not separately tested |
| **ARM64 (Apple M1-M3)** | NEON (future) | ✅ Scalar fallback | ✅ QEMU in CI |
| **ARM64 (AWS Graviton)** | NEON (future) | ✅ Scalar fallback | ✅ QEMU in CI |
| **ARM64 (Raspberry Pi 5)** | NEON (future) | ✅ Scalar fallback | ✅ QEMU in CI |

---

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for the complete data-flow diagram and design decisions.

### Mersenne-64 Prime Pool (v2.5.0)

| Prime | k | p = 2^64 − k |
|-------|---|--------------|
| P_M59 | 59 | 18446744073709551557 |
| P_M83 | 83 | 18446744073709551533 |
| P_M95 | 95 | 18446744073709551521 |
| P_M179 | 179 | 18446744073709551437 |
| P_M189 | 189 | 18446744073709551427 |
| P_M257 | 257 | 18446744073709551359 |
| P_M279 | 279 | 18446744073709551337 |
| P_M323 | 323 | 18446744073709551293 |

- **Selection**: `HKDF(k_master, "CAGOULE_PRIME_SEL_V25")[0] % 8`
- **Advantage**: `a×b mod p = hi×k + lo` — 13 instructions vs 22 for Barrett
- **Option A**: Dual accumulator (even/odd split) reduces dependency chain from depth 16 to 8

### Z-Domain Shifting (v2.5.0, inline since v2.5.4)

- **Operation**: `byte[i] = (byte[i] + z_offset[i%16]) % 256` (applied inline in C-layer)
- **Derivation**: `z_offset = HKDF(k_master, "CAGOULE_Z_SHIFT_V25", 128) % p`
- **Security**: Prevents DDT precomputation attacks on the algebraic layer
- **Performance**: Zero allocation — applied per-block during load (v2.5.4)

---

## Security

See [SECURITY.md](SECURITY.md) for the complete threat model, known limitations, side-channel considerations, and vulnerability reporting process.

| Validation | Result |
|-----------|--------|
| **Constant-time C layer** | ✅ Empirically validated (dudect) |
| **Fuzz testing** | ✅ 500K iterations, 0 crashes (libFuzzer + ASAN/UBSAN) |
| **Memory safety** | ✅ Valgrind clean — 0 errors, 0 leaks across all 7 test binaries |
| **C test assertions** | ✅ 4,088,031 passed, 0 failed |
| **Python tests** | ✅ 578 passed, 0 failed, 20 skipped |

---

## Quick Start

```python
from cagoule import encrypt, decrypt

ct = encrypt(b"my secret data", b"my_password")
pt = decrypt(ct, b"my_password")
assert pt == b"my secret data"
```

### Bulk Encryption (v2.4.0+)

```python
from cagoule import encrypt_bulk, decrypt_bulk

messages = [b"alpha", b"beta", b"gamma"]
cts = encrypt_bulk(messages, b"my_password")
pts = decrypt_bulk(cts, b"my_password")
assert pts == messages
```

### Backend Inspection

```python
from cagoule import __version__, backend_info

print(__version__)          # "2.5.5"
print(backend_info)         # {'matrix_backend': 'avx2', 'omega_backend': 'C', 'sbox_backend': 'avx2'}
```

---

## Build & Test

```bash
cd cagoule/c
make clean && make && make tests  # builds + runs all 10 C test binaries
make valgrind                     # memory leak detection (7 binaries)
make install                      # copies libcagoule.so to Python package
make fuzz                         # libFuzzer harness (requires clang)
pip install -e ".[dev]"
pytest tests/ -v                  # 578 Python tests
```

### C Tests — 4,088,031 assertions, 0 failed

| Binary | Assertions | Coverage |
|---|---|---|
| `test_mersenne` | **4,000,032** | Mersenne-64 pool: 500K parity per prime |
| `test_math` | **147** | mulmod64, addmod64, submod64, powmod64, invmod64, Mersenne pool lookup |
| `test_sbox` | **47** | Feistel bijectivity, roundtrip, fallback x^d, zero-rk, Mersenne S-box |
| `test_matrix` | **52** | Vandermonde P×P⁻¹=I, Cauchy fallback, Mersenne pool roundtrip |
| `test_cipher` | **38** | CBC roundtrip, PKCS7, diffusion, Z-Domain Shifting, pipeline4, 1MB bench |
| `test_omega` | 154 | ζ(2n), HKDF round keys, OpenSSL, thread-safety |
| `test_math_avx2` | **33,033** | mulmod64x4/addmod64x4/submod64x4 parity, Mersenne parity + edge cases |
| `test_matrix_avx2` | **31,917** | AVX2 Vandermonde parity, Mersenne pool parity + roundtrip |
| `test_sbox_avx2` | 22,503 | AVX2 Feistel parity (400K cases), edge, bench |
| `test_cipher_pipeline4` | **109** | Pipeline4 edge cases, residual regression, Z-Domain + pipeline4, 10K parity |

### Python Tests — 578 passed, 0 failed, 20 skipped

### Valgrind — 0 memory leaks across all 7 test binaries

### libFuzzer — 500,000 iterations, 0 crashes (v2.5.5)

---

## Documentation

| Document | Content |
|----------|---------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Data-flow diagram, Mersenne pool, design decisions |
| [SECURITY.md](SECURITY.md) | Threat model, constant-time validation, known limitations, vulnerability reporting |

---

## Roadmap

### v2.5.x Backlog

- Las_shell primitives: `cgl encrypt`, `cgl decrypt`, `cgl bench`, `cgl info`
- PyPI `manylinux` wheel
- Formal algebraic specification + IACR ePrint submission
- `cagoule-pass` v2.0.0 with `Lass_shell vault` command

### v3.0.0

- CTR mode + multi-block SIMD (>30 MB/s single-core target)
- ARM NEON backend for native Apple Silicon / Graviton acceleration
- Production dudect (isolated CPU, 1M+ measurements, Bonferroni correction)

---

## Changelog

### v2.5.4 — 2026-05-26
- **P0**: Z-Domain inline — eliminated malloc in encrypt hot path
- **P1**: dudect constant-time empirical validation
- **P2**: libFuzzer harness — 500K iterations, 0 crashes, 0 memory leaks
- **P3**: SECURITY.md — complete threat model and security policy
- **P4**: CI multi-arch matrix (x86_64 native + ARM64 via QEMU)

### v2.5.3 — 2026-05-26
- Z-Domain doc fix in `cagoule_cipher.h`
- Version string fixes in `cagoule_math_avx2.h`
- Mersenne benchmark suite in `cagoule_bench.toml`

### v2.5.2 — 2026-05-26
- +44,281 test assertions across all suites
- Mersenne AVX2 parity, Mersenne matrix parity, Z-Domain pipeline4 tests
- Zero round-key + Mersenne S-box tests, Mersenne pool roundtrip tests

### v2.5.1 — 2026-05-26
- AVX2 runtime detection fix (`/proc/cpuinfo` with 4096-byte buffer)
- Z-Domain Shifting tests in `test_cipher.c`
- Mersenne pool lookup tests in `test_math.c`

### v2.5.0 — 2026-05-25
- Mersenne-64 prime pool (8 primes, HKDF selection)
- `mulmod_mersenne64x4` AVX2 (~13 instructions vs Barrett ~22)
- Option A dual accumulator (even/odd split, depth 8 vs 16)
- Z-Domain Shifting in C-layer (byte-level whitening)
- `test_mersenne.c` — 4,000,032 assertions across all 8 primes
- ARCHITECTURE.md — complete data-flow diagram and design decisions

### v2.4.0 — 2026-05-16
Pipeline4 decrypt, encrypt_bulk API, thread-local buffer pool, GIL release.

### v2.3.0 — 2026-05-08
S-box AVX2 vectorisation; Mersenne-like reduction; cycle-walking AVX2.

### v2.2.0 — 2026-05-06
AVX2 Vandermonde matrix multiply (+67% algebraic layer).

### v2.1.0 — 2026-05-01
C port of omega.c; security fix for wrong-password detection.

---

## Author

**Slim Issa** — Kairouan, Tunisia
[github.com/slimissa/cagoule](https://github.com/slimissa/cagoule)

Part of the [QuantOS](https://github.com/slimissa/LAS_Shell) platform.

---

## License

MIT — see [LICENSE](LICENSE).

