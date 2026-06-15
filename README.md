# CAGOULE v3.0.0

**Cryptographie Algébrique Géométrique par Ondes et Logique Entrelacée**

[![Version](https://img.shields.io/badge/version-3.0.0-blue)](https://github.com/slimissa/CAGOULE)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Python](https://img.shields.io/badge/python-3.12+-blue)](https://www.python.org)
[![Platform](https://img.shields.io/badge/platform-x86__64%20Linux-lightgrey)](https://github.com/slimissa/CAGOULE)
[![C Tests](https://img.shields.io/badge/C%20tests-4%2C088%2C123%2B-brightgreen)](https://github.com/slimissa/CAGOULE)
[![Python Tests](https://img.shields.io/badge/Python%20tests-578%2B-brightgreen)](https://github.com/slimissa/CAGOULE)

---

CAGOULE is a symmetric hybrid encryption system combining ChaCha20-Poly1305, Argon2id, HKDF-SHA256 with a custom algebraic diffusion layer (Vandermonde over Z/pZ, 2-round Feistel S-box, ζ(2n)-derived round keys). The C backend is fully AVX2-vectorised with Mersenne-64 prime pool.

---

## What's New in v3.0.0

v3.0.0 is the **CTR Mode** release. The v2.5.x cycle completed the AVX2 C-layer (10.8 MB/s). The bottleneck shifted to the CBC mode itself: each block depends on the previous one, preventing any inter-block parallelism. CTR eliminates this constraint.

### Delivered

| Feature | Status |
|---|---|
| **cagoule_ctr.c** — CTR keystream pipeline (scalar + AVX2 4x) | ✅ |
| **cagoule_ctr_encrypt_4x** — 4 independent keystreams simultaneously | ✅ |
| **Format CGL1 v0x02** — `|CT| == |PT|`, no PKCS7 | ✅ |
| **VERSION dispatch** — decrypt() auto-routes v0x01/v0x02 | ✅ |
| **cipher_ctr.py** — Python CTR layer (C backend + fallback) | ✅ |
| **decipher_ctr.py** — Decrypt CTR + dispatch | ✅ |
| **migrate_cbc_to_ctr()** — Migration utility | ✅ |
| **test_ctr.c** — 350K+ assertions (keystream, roundtrip, 4x parity, all 8 primes) | ✅ |
| **encrypt() default → CTR** — API change from v2.5.x | ✅ |
| **encrypt_cbc() / decrypt_cbc()** — CBC still available explicitly | ✅ |

### API Changes from v2.5.x

```python
# v3.0.0: encrypt() defaults to CTR (CGL1 v0x02)
from cagoule import encrypt, decrypt

ct = encrypt(b"secret", b"password")   # CTR — |ct| close to |pt|
pt = decrypt(ct, b"password")          # auto-dispatches v0x01 / v0x02

# CBC still available explicitly
from cagoule import encrypt_cbc, decrypt_cbc
ct_cbc = encrypt_cbc(b"secret", b"password")   # CGL1 v0x01

# Migration
from cagoule import migrate_cbc_to_ctr
ct_ctr = migrate_cbc_to_ctr(ct_cbc, b"password")
```

### Performance (projected — C-layer)

| Metric | v2.5.x (CBC) | v3.0.0 (CTR) | Mechanism |
|--------|-------------|-------------|-----------|
| C encrypt 1MB | 10.8 MB/s | **>25 MB/s** | No inter-block dependency |
| Python e2e 1MB | 6.9 MB/s | **>15 MB/s** | CTR × 4-block pipeline |
| Parallel 20 cores | ~40 MB/s | **>80 MB/s** | CTR × ProcessPool |

---

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for data-flow diagrams and design decisions.

### CGL1 Format Versions

| VERSION | Mode | CT size | PKCS7 | Since |
|---------|------|---------|-------|-------|
| `0x01` | CBC | `ceil(|PT|/16) × 16 × p_bytes` | Yes | v2.0.0 |
| `0x02` | CTR | `|PT|` exact | **No** | v3.0.0 |

### CTR Pipeline

```
IV = HKDF(k_master, "CAGOULE_CTR_V30", 8)

For block bi:
  counter_block = [IV[0..7], bi[0..7]]  ← 16 bytes, 1 byte per uint64
  keystream_bi  = sbox(matrix(counter_block) + rk[bi%nk]) & 0xFF  [16 bytes]
  ct[bi*16 + j] = (pt[bi*16+j] + zo_byte[j]) ^ keystream_bi[j]
```

4 independent keystreams computed simultaneously (ILP) in `cagoule_ctr_encrypt_4x`.

---

## Quick Start

```python
from cagoule import encrypt, decrypt

ct = encrypt(b"my secret data", b"my_password")
pt = decrypt(ct, b"my_password")
assert pt == b"my secret data"
```

### Bulk Encryption

```python
from cagoule import encrypt_bulk, decrypt_bulk

messages = [b"alpha", b"beta", b"gamma"]
cts = encrypt_bulk(messages, b"my_password")  # single Argon2id derivation
pts = decrypt_bulk(cts, b"my_password")
assert pts == messages
```

---

## Build & Test

```bash
cd cagoule/c
make clean && make && make tests
make test-ctr          # CTR-specific suite
make install
pip install -e ".[dev]"
pytest tests/ -v
```

### C Tests

| Binary | Assertions | Coverage |
|---|---|---|
| `test_mersenne` | 4,000,032 | Mersenne-64 pool: 500K parity per prime |
| `test_ctr` | **350K+** | Keystream, roundtrip, 4x parity, 8 primes, Z-domain |
| `test_math_avx2` | 16,553 | Mersenne + Barrett parity |
| `test_matrix_avx2` | 27,778 | Mersenne matrix parity + roundtrip |
| `test_sbox_avx2` | 22,503 | Feistel AVX2 parity |
| `test_constant_time` | 8 | dudect Mersenne + Barrett |
| *(+ 6 others)* | ~21,249 | math, cipher, matrix, sbox, omega, pipeline4 |

---

## Changelog

### v3.0.0 — 2026-05-28

**C Backend**
- New: `cagoule_ctr.c` — CTR keystream pipeline (scalar + AVX2)
- New: `cagoule_ctr_encrypt` / `cagoule_ctr_decrypt` — symmetric CTR API
- New: `cagoule_ctr_encrypt_4x` — 4-block simultaneous keystream
- New: `cagoule_ctr_keystream` — raw keystream generation
- New: `cagoule_ctr.h` — full documented header
- New: `test_ctr.c` — 350K+ assertions (10 suites)

**Python Layer**
- New: `cipher_ctr.py` — CTR encrypt layer (C backend + Python fallback)
- New: `decipher_ctr.py` — CTR decrypt + CGL1 VERSION dispatch
- Changed: `encrypt()` now defaults to CTR (CGL1 v0x02)
- Changed: `decrypt()` auto-dispatches v0x01/v0x02 by VERSION field
- New: `encrypt_cbc()` / `decrypt_cbc()` — explicit CBC API
- New: `migrate_cbc_to_ctr()` — migration utility
- New: `encrypt_bulk()` / `decrypt_bulk()` — bulk CTR
- Updated: `__version__` → 3.0.0

**Format**
- New: CGL1 v0x02 — CTR format, no PKCS7, `|CT| == |PT|`
- Backward compatible: v0x01 (CBC) still fully supported

### v2.5.4 — 2026-05-26
Z-Domain malloc eliminated, dudect CT validation, libFuzzer harness, CI ARM64.

### v2.5.3 — 2026-05-24
Documentation fixes (cagoule_cipher.h, math_avx2.h, bench.toml + mersenne suite).

### v2.5.2 — 2026-05-23
+44,405 assertions: Mersenne AVX2, matrix parity, pipeline4 z_offset, sbox.

### v2.5.1 — 2026-05-21
AVX2 detection fix (__builtin_cpu_supports), Z-domain + Mersenne tests.

### v2.5.0 — 2026-05-18
Mersenne-64 pool, mulmod_mersenne64x4, Option A dual accumulator, Z-Domain Shifting, encrypt_bulk, buffer pool.

---

## Author

**Slim Issa** — Kairouan, Tunisia
[github.com/slimissa/CAGOULE](https://github.com/slimissa/CAGOULE)

Part of the [QuantOS](https://github.com/slimissa/LAS_Shell) platform.

---

## License

MIT — see [LICENSE](LICENSE).
