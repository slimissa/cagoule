# CAGOULE v2.5.1 — Architecture

## Data-Flow Diagram

```
                          ┌─────────────────────────────────────────────────────────┐
                          │                   CAGOULE v2.5.1                         │
                          │    Cryptographie Algébrique Géométrique par Ondes        │
                          │                 et Logique Entrelacée                    │
                          └─────────────────────────────────────────────────────────┘

   password ────►  Argon2id KDF  ────►  k_master (64 bytes)
        │              │                        │
        │         (RFC 9106)                    ├──► HKDF("CAGOULE_PRIME_SEL_V25") ──► Mersenne Pool Index ──► p = 2^64 - k
        │                                       │
        │                                       ├──► HKDF("CAGOULE_N") ──► n (block size)
        │                                       │
        │                                       ├──► generate_mu(p) ──► µ ∈ Z/pZ or Fp²
        │                                       │
        │                                       ├──► HKDF("CAGOULE_DELTA") ──► S-Box Feistel (rk0, rk1)
        │                                       │
        │                                       ├──► HKDF("CAGOULE_NODE_*") ──► Vandermonde Nodes
        │                                       │
        │                                       ├──► HKDF("CAGOULE_ENC") ──► k_stream (ChaCha20-Poly1305)
        │                                       │
        │                                       ├──► ζ(2n) → HKDF ──► 64 Round Keys (Z/pZ)
        │                                       │
        │                                       └──► HKDF("CAGOULE_Z_SHIFT_V25") ──► z_offset[16]
        │
        ▼
  ┌──────────────────────────────────────────────────────────────────────┐
  │                        ENCRYPTION PIPELINE                            │
  │                                                                      │
  │  plaintext                                                           │
  │     │                                                                │
  │     ▼                                                                │
  │  PKCS7 Pad (→ multiple of 16 bytes)                                  │
  │     │                                                                │
  │     ▼                                                                │
  │  ┌─────────────────────┐                                             │
  │  │  Z-Domain Shifting   │  byte[i] = (byte[i] + z_offset[i%16]) % 256│
  │  │  (v2.5.1, C-layer)  │                                             │
  │  └─────────┬───────────┘                                             │
  │            │                                                         │
  │            ▼                                                         │
  │  ┌─────────────────────────────────────────────────────────┐        │
  │  │              ALGEBRAIC LAYER (C + AVX2)                  │        │
  │  │                                                          │        │
  │  │  ┌──────────┐    ┌───────────┐    ┌──────────────────┐   │        │
  │  │  │ CBC Add  │───►│ Vandermonde│───►│  Feistel S-Box   │   │        │
  │  │  │ (mod p)  │    │ 16×16 Mul │    │  2-Round Network │   │        │
  │  │  │          │    │ (mod p)   │    │  (P32_PRIME)     │   │        │
  │  │  └──────────┘    └───────────┘    └────────┬─────────┘   │        │
  │  │                                            │              │        │
  │  │                     ┌──────────────────────┘              │        │
  │  │                     ▼                                     │        │
  │  │              ┌──────────────┐                              │        │
  │  │              │ Round Key Add│  block[i] += rk[bi % 64]    │        │
  │  │              │   (mod p)    │                              │        │
  │  │              └──────────────┘                              │        │
  │  │                                                          │        │
  │  │  Optimizations (v2.5.1):                                 │        │
  │  │  • Mersenne-64 primes (p = 2^64 - k)                     │        │
  │  │  • mulmod_mersenne64x4 (13 instr vs Barrett 22)          │        │
  │  │  • Option A — Dual Accumulator (even/odd split)          │        │
  │  │  • Pipeline4 — 4-way parallel decrypt (v2.4.0)           │        │
  │  └──────────────────────────────────────────────────────────┘        │
  │            │                                                         │
  │            ▼                                                         │
  │  ┌─────────────────────┐                                             │
  │  │  ChaCha20-Poly1305   │  AEAD Encrypt (RFC 8439)                   │
  │  │  (k_stream, nonce)  │                                             │
  │  └─────────┬───────────┘                                             │
  │            │                                                         │
  │            ▼                                                         │
  │  CGL1 Wire Format:  MAGIC | VERSION | SALT | NONCE | CT | TAG       │
  └──────────────────────────────────────────────────────────────────────┘

                          ┌─────────────────────────────────────────────────────────┐
                          │                   DECRYPTION PIPELINE                    │
                          │                                                          │
                          │  CGL1 → Parse → AEAD Decrypt → Round Key Remove →        │
                          │  Inverse S-Box → Inverse Matrix → CBC Subtract →          │
                          │  Z-Domain Unshift → PKCS7 Unpad → plaintext               │
                          └─────────────────────────────────────────────────────────┘
```

---

## Layer Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    PYTHON PUBLIC API                         │
│  encrypt() / decrypt() / encrypt_bulk() / decrypt_bulk()    │
│  CagouleParams.derive() / CagouleParams.zeroize()           │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│                  PYTHON CRYPTOGRAPHIC MODULES                │
│  cipher.py · decipher.py · params.py · format.py            │
│  omega.py · matrix.py · sbox.py · mu.py · fp2.py            │
│  _binding.py (ctypes) · _buffer_pool.py (P4)                │
└────────────────────────┬────────────────────────────────────┘
                         │ ctypes
┌────────────────────────▼────────────────────────────────────┐
│                   C SHARED LIBRARY                           │
│                     libcagoule.so                            │
│                                                              │
│  ┌──────────┐  ┌───────────┐  ┌────────────┐  ┌─────────┐  │
│  │  cipher  │  │  matrix   │  │    sbox    │  │  omega  │  │
│  │ CBC pipe │  │ Vandermonde│  │  Feistel   │  │ ζ(2n)→RK│  │
│  │ Z-Domain │  │ + Inverse │  │  AVX2 SBox │  │  HKDF   │  │
│  │ Pipeline4│  │ + Cauchy  │  │            │  │ OpenSSL │  │
│  └──────────┘  └───────────┘  └────────────┘  └─────────┘  │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐    │
│  │  math (scalar)  │  math_avx2 (Barrett + Mersenne)   │    │
│  └──────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## Key Design Decisions

### 1. Mersenne-64 Prime Pool (v2.5.1)

| Prime | k | p = 2^64 - k |
|-------|---|--------------|
| P_M59 | 59 | 18446744073709551557 |
| P_M83 | 83 | 18446744073709551533 |
| P_M95 | 95 | 18446744073709551521 |
| P_M179 | 179 | 18446744073709551437 |
| P_M189 | 189 | 18446744073709551427 |
| P_M257 | 257 | 18446744073709551359 |
| P_M279 | 279 | 18446744073709551337 |
| P_M323 | 323 | 18446744073709551293 |

- **Selection**: HKDF("CAGOULE_PRIME_SEL_V25")[0] % 8
- **Advantage**: `a*b mod p = hi*k + lo` — no division needed
- **Instruction count**: ~13 (Mersenne) vs ~22 (Barrett) per multiplication

### 2. Option A — Dual Accumulator

```
v2.4.0:  acc += M[j] * v[j]   for j = 0..15   → depth 16 chain
v2.5.1:  acc_a += M[j] * v[j]  for j even     → depth 8 chain
         acc_b += M[j] * v[j]  for j odd      → depth 8 chain
         acc = acc_a + acc_b                   → merge at end
```

- **Register budget**: ~13 YMM (Mersenne) vs 16+ YMM (Barrett)
- **CPU ILP**: Two independent chains execute in parallel

### 3. Z-Domain Shifting

- **Operation**: `byte[i] = (byte[i] + z_offset[i%16] % 256) % 256`
- **Location**: C-layer (pre-encryption) for performance
- **Derivation**: `z_offset = HKDF(k_master, "CAGOULE_Z_SHIFT_V25", 128) % p`
- **Security**: Prevents DDT precomputation attacks on the algebraic layer

### 4. Feistel S-Box Symmetry

- **2-round Feistel** on 32-bit halves
- **Round function**: `f(x, rk) = (x * rk) % P32_PRIME` where `P32_PRIME = 2^32 - 5`
- **Key property**: `decrypt_cost ≈ encrypt_cost` (ratio ≈ 1.0×)
- **v1.x ratio was 7.8×** — the Feistel design eliminated the asymmetry

### 5. Dual-Path Architecture

| Layer | C Backend | Python Fallback |
|-------|-----------|-----------------|
| Matrix | `cagoule_matrix_mul` (AVX2) | `_matmul16_scalar` |
| S-Box | `cagoule_sbox_forward` (Feistel AVX2) | `x^d mod p` |
| Omega | `cagoule_omega_generate_round_keys` | `mpmath.zeta()` |
| Cipher | `cagoule_cbc_encrypt` (pipeline4) | `_cbc_encrypt_py` |

---

## Memory Management

```
Allocation:    cagoule_matrix_build() → calloc()
               ↓
Usage:         cagoule_matrix_mul() → AVX2 or scalar
               ↓
Cleanup:       1. free()       — explicit, preferred
               2. __exit__()   — context manager (with statement)
               3. __del__()    — GC fallback (logs errors)
```

- **Double-free guard**: `_freed` flag prevents corruption
- **Buffer pool**: Thread-local `ctypes` buffers reused across calls (P4)
- **Zeroization**: Sensitive buffers zeroed via `ctypes.memset` after use

---

## Thread Safety

| Component | Mechanism |
|-----------|-----------|
| AVX2 detection | `__atomic_load/store` (lock-free lazy init) |
| Buffer pool | `threading.local()` |
| Omega round keys | Stack allocation per call |
| Encryption | GIL release on heavy C calls |

---

## CGL1 Wire Format

```
Offset  Size  Field
─────────────────────
  0      4     MAGIC    = b'CGL1'
  4      1     VERSION  = 0x01
  5     32     SALT     (Argon2id salt)
 37     12     NONCE    (ChaCha20-Poly1305 nonce)
 49     CT     CIPHERTEXT + TAG (Poly1305 tag = last 16 bytes)
─────────────────────
OVERHEAD = 65 bytes (49 header + 16 tag)
```

---

## Performance Characteristics

| Operation | Throughput | Notes |
|-----------|-----------|-------|
| C encrypt (1 MB) | ~6-11 MB/s | Depends on AVX2 availability |
| C decrypt (1 MB) | ~6-11 MB/s | Ratio ≈ 1.0× (Feistel symmetry) |
| S-box Feistel | ~70-120 MB/s | AVX2 vectorized |
| Matrix multiply | ~75 ms/MB | Vandermonde 16×16 |
| Round keys (64) | ~0.26 ms | HKDF-SHA256 via OpenSSL |
| Parallel (20 cores) | ~40 MB/s | ProcessPoolExecutor |
| vs AES-256-GCM | ~80× slower | CAGOULE is a research cipher |

---

## Test Coverage

| Suite | Assertions | Focus |
|-------|-----------|-------|
| C tests (10 binaries) | 4,043,718 | Unit + parity + AVX2 validation |
| Python tests (pytest) | 579 tests | Integration + KAT + NIST |
| Valgrind | 7 binaries | Memory leak detection |
| `test_mersenne` | 4,000,032 | Mersenne-64 pool (v2.5.1 headline) |
| `test_kat` | 20 tests | Non-regression via SHA-256 pinning |

---

## Build System

```
make              → libcagoule.so (AVX2 if available)
make tests        → All 10 C test binaries
make valgrind     → Memory leak detection
make debug        → ASan + UBSan build
make install      → Copy to Python package
make sysinfo      → Compiler/flags/features summary
```

- **AVX2 detection**: `check_avx2.py` tests compiler support
- **Per-file AVX2**: Only 3 files compiled with `-mavx2`
- **Base CFLAGS**: `-march=native -mno-avx -mno-avx2` prevents auto-vectorization in scalar files

---

## Version History

| Version | Date | Key Features |
|---------|------|-------------|
| v2.5.1 | 2026-05-25 | Mersenne-64 pool, Option A dual accumulator, Z-Domain Shifting (C-layer) |
| v2.4.0 | 2026-05-16 | Pipeline4, encrypt_bulk API, GIL release, thread-local buffer pool |
| v2.3.0 | 2026-05-08 | S-box AVX2, Mersenne-like reduction, cycle-walking AVX2 |
| v2.2.0 | 2026-05-06 | AVX2 Vandermonde matrix multiply |
| v2.1.0 | 2026-05-01 | C port of omega.c, wrong-password detection fix |
| v2.0.0 | — | Feistel S-box (1:1 decrypt/encrypt ratio) |
| v1.x | — | Original x^d S-box (7.8× decrypt/encrypt ratio) |

---

## Author

**Slim Issa** — Kairouan, Tunisia  
Part of the QuantOS platform  
github.com/slimissa/CAGOULE

**License**: MIT
