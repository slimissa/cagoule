# CAGOULE v3.0.0 — Architecture

## Data-Flow Diagram

```
                          ┌─────────────────────────────────────────────────────────┐
                          │                   CAGOULE v3.0.0                        │
                          │    Cryptographie Algébrique Géométrique par Ondes       │
                          │                 et Logique Entrelacée                   │
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
        │                                       ├──► HKDF("CAGOULE_Z_SHIFT_V25") ──► z_offset[16]
        │                                       │
        │                                       └──► HKDF("CAGOULE_CTR_V30") ──► IV_CTR (8 bytes, v3.0.0)
        │
        ▼
  ┌──────────────────────────────────────────────────────────────────────┐
  │                    CBC MODE (v0x01, legacy)                           │
  │                                                                      │
  │  plaintext → PKCS7 Pad → Z-Domain Shift → CBC Add →                  │
  │  Vandermonde → Feistel S-Box → Round Key Add →                       │
  │  ChaCha20-Poly1305 AEAD → CGL1 v0x01                                 │
  └──────────────────────────────────────────────────────────────────────┘

  ┌──────────────────────────────────────────────────────────────────────┐
  │                    CTR MODE (v0x02, v3.0.0)                           │
  │                                                                      │
  │  plaintext (arbitrary length, no padding)                            │
  │     │                                                                │
  │     ▼                                                                │
  │  ┌─────────────────────┐                                             │
  │  │  Z-Domain Shifting   │  byte[i] = (byte[i] + z_offset[i%16]) % 256│
  │  │  (v2.5.1, C-layer)  │                                             │
  │  └─────────┬───────────┘                                             │
  │            │                                                         │
  │            ▼                                                         │
  │  ┌─────────────────────────────────────────────────────────┐        │
  │  │           CTR KEYSTREAM PIPELINE (C + AVX2)             │        │
  │  │                                                          │        │
  │  │  counter_block = IV(8) ‖ bi(8)   (bi = block index)     │        │
  │  │       │                                                  │        │
  │  │       ▼                                                  │        │
  │  │  ┌───────────┐    ┌───────────┐    ┌──────────────┐     │        │
  │  │  │Vandermonde│───►│  Feistel  │───►│ Round Key Add│     │        │
  │  │  │ 16×16 Mul │    │  S-Box    │    │  (mod p)     │     │        │
  │  │  └───────────┘    └───────────┘    └──────┬───────┘     │        │
  │  │                                            │              │        │
  │  │                     keystream[j] = out[j] & 0xFF         │        │
  │  │                                                          │        │
  │  │  ciphertext[j] = (plaintext[j] + zo_byte[j]) ^ ks[j]    │        │
  │  │                                                          │        │
  │  │  Optimizations (v3.0.0):                                 │        │
  │  │  • 4-block simultaneous pipeline (ILP maximal)           │        │
  │  │  • No inter-block dependency (CTR mode)                  │        │
  │  │  • |CT| == |PT| (no PKCS7 padding)                       │        │
  │  │  • encrypt == decrypt (CTR symmetry)                     │        │
  │  └──────────────────────────────────────────────────────────┘        │
  │            │                                                         │
  │            ▼                                                         │
  │  ┌─────────────────────┐                                             │
  │  │  ChaCha20-Poly1305   │  AEAD Encrypt (RFC 8439)                   │
  │  │  (k_stream, nonce)  │                                             │
  │  └─────────┬───────────┘                                             │
  │            │                                                         │
  │            ▼                                                         │
  │  CGL1 Wire Format:  MAGIC | VERSION=0x02 | SALT | NONCE | CT | TAG  │
  └──────────────────────────────────────────────────────────────────────┘

                          ┌─────────────────────────────────────────────────────────┐
                          │                   DECRYPTION PIPELINE                    │
                          │                                                          │
                          │  CGL1 → Parse → AEAD Decrypt →                           │
                          │  VERSION 0x01 → CBC: Inverse S-Box → Inverse Matrix →    │
                          │                  CBC Subtract → Z-Domain Unshift →       │
                          │                  PKCS7 Unpad → plaintext                 │
                          │  VERSION 0x02 → CTR: Keystream gen → XOR+Z-Unshift →    │
                          │                  plaintext (symmetric)                   │
                          └─────────────────────────────────────────────────────────┘
```

---

## Layer Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    PYTHON PUBLIC API                         │
│  encrypt() → CTR (v0x02) | encrypt_cbc() → CBC (v0x01)     │
│  decrypt() → auto-dispatch v0x01/v0x02                      │
│  encrypt_bulk() / decrypt_bulk() / migrate_cbc_to_ctr()     │
│  CagouleParams.derive() / CagouleParams.zeroize()           │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│                  PYTHON CRYPTOGRAPHIC MODULES                │
│  cipher.py · decipher.py · cipher_ctr.py · decipher_ctr.py  │
│  params.py · format.py · omega.py · matrix.py · sbox.py     │
│  mu.py · fp2.py · _binding.py (ctypes) · _buffer_pool.py    │
└────────────────────────┬────────────────────────────────────┘
                         │ ctypes
┌────────────────────────▼────────────────────────────────────┐
│                   C SHARED LIBRARY                           │
│                     libcagoule.so                            │
│                                                              │
│  ┌──────────┐  ┌───────────┐  ┌────────────┐  ┌─────────┐  │
│  │  cipher  │  │  matrix   │  │    sbox    │  │  omega  │  │
│  │ CBC pipe │  │ Vandermonde│  │  Feistel   │  │ ζ(2n)→RK│  │
│  │ CTR pipe │  │ + Inverse │  │  AVX2 SBox │  │  HKDF   │  │
│  │ Z-Domain │  │ + Cauchy  │  │            │  │ OpenSSL │  │
│  │ Pipeline4│  │            │  │            │  │         │  │
│  └──────────┘  └───────────┘  └────────────┘  └─────────┘  │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐    │
│  │  math (scalar)  │  math_avx2 (Barrett + Mersenne)   │    │
│  └──────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## Key Design Decisions

### 1. Mersenne-64 Prime Pool (v3.0.0)

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

### 3. CTR Mode (v3.0.0)

- **Counter block**: IV(8 bytes) ‖ block_index(8 bytes, big-endian)
- **Keystream**: counter_block → matrix → sbox → round_key_add → byte_extract
- **4-block pipeline**: 4 independent keystreams computed simultaneously
- **No inter-block dependency**: ILP maximal, streaming-friendly
- **No padding**: |CT| == |PT| exact
- **Symmetric**: encrypt == decrypt at C-layer (only Z-shift direction differs)
- **IV derivation**: `HKDF(k_master, "CAGOULE_CTR_V30", 8)` — not stored in header

### 4. Z-Domain Shifting

- **Operation**: `byte[i] = (byte[i] + z_offset[i%16] % 256) % 256`
- **Location**: C-layer (pre-encryption) for performance
- **Derivation**: `z_offset = HKDF(k_master, "CAGOULE_Z_SHIFT_V25", 128) % p`
- **Security**: Prevents DDT precomputation attacks on the algebraic layer

### 5. Feistel S-Box Symmetry

- **2-round Feistel** on 32-bit halves
- **Round function**: `f(x, rk) = (x * rk) % P32_PRIME` where `P32_PRIME = 2^32 - 5`
- **Key property**: `decrypt_cost ≈ encrypt_cost` (ratio ≈ 1.0×)
- **v1.x ratio was 7.8×** — the Feistel design eliminated the asymmetry

### 6. Dual-Path Architecture

| Layer | C Backend | Python Fallback |
|-------|-----------|-----------------|
| Matrix | `cagoule_matrix_mul` (AVX2) | `_matmul16_scalar` |
| S-Box | `cagoule_sbox_forward` (Feistel AVX2) | `x^d mod p` |
| Omega | `cagoule_omega_generate_round_keys` | `mpmath.zeta()` |
| CBC | `cagoule_cbc_encrypt` (pipeline4) | `_cbc_encrypt_py` |
| CTR | `cagoule_ctr_encrypt` (4-block SIMD) | `_ctr_encrypt_py` |

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
  4      1     VERSION  = 0x01 (CBC) or 0x02 (CTR, v3.0.0)
  5     32     SALT     (Argon2id salt)
 37     12     NONCE    (ChaCha20-Poly1305 nonce)
 49     CT     CIPHERTEXT + TAG (Poly1305 tag = last 16 bytes)
─────────────────────
OVERHEAD = 65 bytes (49 header + 16 tag)

CTR (v0x02): |CT| == |plaintext| (no padding)
CBC (v0x01): |CT| is padded to 16-byte boundary (PKCS7)
```

---

## Performance Characteristics

| Operation | Throughput | Notes |
|-----------|-----------|-------|
| CTR encrypt (1 MB) | ~19.7 MB/s | v3.0.0, 4-block SIMD pipeline |
| CBC encrypt (1 MB) | ~6-11 MB/s | Depends on AVX2 availability |
| CBC decrypt (1 MB) | ~6-11 MB/s | Ratio ≈ 1.0× (Feistel symmetry) |
| S-box Feistel | ~70-120 MB/s | AVX2 vectorized |
| Matrix multiply | ~75 ms/MB | Vandermonde 16×16 |
| Round keys (64) | ~0.26 ms | HKDF-SHA256 via OpenSSL |
| Parallel (20 cores) | ~40 MB/s | ProcessPoolExecutor |
| vs AES-256-GCM | ~80× slower | CAGOULE is a research cipher |

---

## Test Coverage

| Suite | Assertions | Focus |
|-------|-----------|-------|
| C tests (12 binaries) | 4,576,891 | Unit + parity + AVX2 + CTR validation |
| Python tests (pytest) | 579+ tests | Integration + KAT + NIST + CTR |
| Valgrind | 8 binaries | Memory leak detection |
| `test_mersenne` | 4,000,032 | Mersenne-64 pool (v2.5.1 headline) |
| `test_ctr` | 468,850 | CTR mode (v3.0.0) |
| `test_kat` | 20 tests | Non-regression via SHA-256 pinning |
| libFuzzer | 1M runs | CBC + CTR (v3.0.0) |

---

## Build System

```
make              → libcagoule.so (AVX2 if available)
make tests        → All 12 C test binaries
make test-ctr     → CTR tests (v3.0.0)
make test-avx2    → AVX2 tests (Mersenne + matrix + S-box)
make valgrind     → Memory leak detection (8 binaries)
make fuzz         → libFuzzer 1M runs (CBC + CTR)
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
| v3.0.0 | 2026-05-28 | CTR mode, 4-block SIMD pipeline, CGL1 v0x02, 19.7 MB/s, encrypt/decrypt dispatch |
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
