# CAGOULE v3.0.1

**Cryptographie Algébrique Géométrique par Ondes et Logique Entrelacée**

[![Version](https://img.shields.io/badge/version-3.0.1-blue)](https://github.com/slimissa/CAGOULE)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Python](https://img.shields.io/badge/python-3.12+-blue)](https://www.python.org)
[![Platform](https://img.shields.io/badge/platform-x86__64%20Linux-lightgrey)](https://github.com/slimissa/CAGOULE)
[![C Tests](https://img.shields.io/badge/C%20tests-468%2C857%2B-brightgreen)](https://github.com/slimissa/CAGOULE)
[![Python Tests](https://img.shields.io/badge/Python%20tests-713-brightgreen)](https://github.com/slimissa/CAGOULE)
[![Security](https://img.shields.io/badge/IND--CPA-conjectured%2C%20unproven-orange)](SECURITY.md)

---

> ⚠️ **Research-grade software.** CAGOULE is not a production replacement for
> AES-GCM or ChaCha20-Poly1305. IND-CPA security of the algebraic layer is
> conjectured but not formally proven. See [SECURITY.md](SECURITY.md).

---

## What is CAGOULE?

CAGOULE is a hybrid symmetric encryption system combining:

- **Standardized cryptographic primitives** (Argon2id, HKDF-SHA256, ChaCha20-Poly1305)
  that provide standard, battle-tested security guarantees
- **A novel algebraic diffusion layer** (Vandermonde/Cauchy matrix over Z/pZ,
  2-round Feistel S-box with ζ(2n)-derived round keys, dynamic Mersenne-64 prime pool)
  that is the subject of ongoing research

The algebraic layer is fully implemented in C with AVX2 vectorisation and a
bit-exact Python fallback. The outer ChaCha20-Poly1305 AEAD layer ensures
confidentiality and integrity regardless of the algebraic layer's security status.

---

## What's New in v3.0.1

v3.0.1 is a security patch release. **10 bugs were identified and fixed** during
an independent empirical audit. The most severe:

| Bug | Impact | Fix |
|---|---|---|
| CTR two-time-pad via shared `params=` | Keystream reuse, full plaintext recovery | IV now bound to per-message ChaCha20 nonce |
| Python S-box completely unkeyed | Zero key material from nonlinear layer in pure-Python deployments | Full Feistel port, bit-exact vs C |
| `k_master` leaked via pickle | Master key exposed in `ProcessPoolExecutor` IPC pipes | `__reduce__` now raises `TypeError` |
| `Fp2.sqrt()` wrong formula + non-field primes | Silent wrong results | Tonelli-Shanks generalized + field precondition |

See [CHANGELOG.md](CHANGELOG.md) for the complete list.

**Wire format note:** v3.0.1 CTR ciphertexts are not compatible with v3.0.0 CTR
ciphertexts (IV formula changed). CBC ciphertexts (v0x01) are unaffected.

---

## Architecture

```
Password ──► Argon2id ──► k_master
                              │
                    ┌─────────┴──────────────────┐
                    │                            │
               HKDF-SHA256                  HKDF-SHA256
                    │                            │
                    ▼                            ▼
            Algebraic layer               k_stream (32B)
       (Vandermonde matrix,                     │
        Feistel S-box,                          ▼
        ζ(2n) round keys,           ChaCha20-Poly1305 AEAD
        CTR mode)                   (confidentiality + integrity)
                    │                            │
                    └──────────────┬─────────────┘
                                   │
                              CGL1 v0x02
                  MAGIC|VERSION|SALT|NONCE|CT_AEAD|TAG
```

The algebraic layer operates over a password-derived Mersenne-64 prime, producing
a ciphertext that is then encrypted by ChaCha20-Poly1305. Even if the algebraic
layer were broken, ChaCha20-Poly1305 provides standard IND-CCA2 security.

---

## Quick Start

### Build the C backend (required for full performance)

```bash
cd cagoule/c
make clean && make all
# libcagoule.so is placed in cagoule/c/ and cagoule/
```

Dependencies: `gcc`, `libssl-dev`, `libargon2-dev`

```bash
# Ubuntu/Debian
sudo apt-get install gcc libssl-dev libargon2-dev
```

### Install Python dependencies

```bash
pip install cryptography argon2-cffi
# Optional: for Python fallback and cross-backend tests
pip install mpmath
```

### Basic usage

```python
from cagoule import encrypt, decrypt

# Encrypt (CTR mode, CGL1 v0x02)
ciphertext = encrypt(b"secret message", b"my password")

# Decrypt (auto-dispatches v0x01 CBC / v0x02 CTR)
plaintext = decrypt(ciphertext, b"my password")

# CBC mode (v0x01) — still available explicitly
from cagoule import encrypt_cbc, decrypt_cbc
ct_cbc = encrypt_cbc(b"secret message", b"my password")

# KDF amortization — share params across multiple encryptions
from cagoule import CagouleParams
from cagoule.cipher_ctr import encrypt_ctr

params = CagouleParams.derive(b"my password")
try:
    ct1 = encrypt_ctr(b"message one", b"my password", params=params)
    ct2 = encrypt_ctr(b"message two", b"my password", params=params)
    # Each ciphertext is independently decryptable — no shared state required
finally:
    params.zeroize()
```

### Performance

```
C encrypt 1MB (CTR, single core):  ~22.7 MB/s
C decrypt 1MB (CTR, single core):  ~22.3 MB/s
```

Measured on this hardware. FUSE context-switch overhead not included.
See `cagoule/c/Makefile` target `bench` for full benchmark suite.

---

## Project Structure

```
CAGOULE/
├── cagoule/                    # Python package
│   ├── __init__.py             # Public API
│   ├── params.py               # CagouleParams — KDF + key schedule
│   ├── cipher.py               # CBC encrypt (CGL1 v0x01)
│   ├── cipher_ctr.py           # CTR encrypt (CGL1 v0x02) ← MAIN PATH
│   ├── decipher.py             # CBC decrypt
│   ├── decipher_ctr.py         # CTR decrypt
│   ├── sbox.py                 # S-box (C backend + Python Feistel fallback)
│   ├── matrix.py               # Vandermonde/Cauchy diffusion matrix
│   ├── omega.py                # ζ(2n) round-key derivation
│   ├── fp2.py                  # Fp² arithmetic (mu generation)
│   ├── mu.py                   # μ parameter selection
│   ├── _binding.py             # ctypes bindings to libcagoule.so
│   ├── _buffer_pool.py         # Thread-local buffer pool
│   └── c/                      # C backend
│       ├── Makefile
│       ├── src/
│       │   ├── cagoule_math.c          # Modular arithmetic
│       │   ├── cagoule_matrix.c        # Diffusion matrix (AVX2)
│       │   ├── cagoule_sbox.c          # S-box (Feistel, AVX2)
│       │   ├── cagoule_cipher.c        # CBC pipeline
│       │   ├── cagoule_ctr.c           # CTR pipeline (AVX2 4x)
│       │   ├── cagoule_omega.c         # Round-key derivation
│       │   ├── cagoule_kdf.c           # Argon2id / HKDF wrappers
│       │   ├── cagoule_params.c        # C-side params
│       │   ├── cagoule_api.c           # High-level streaming API
│       │   └── cagoule_stream.c        # Streaming chunked API
│       ├── include/                    # Public headers
│       ├── tests/                      # C test sources
│       └── fuzz/                       # libFuzzer harness
├── tests/                      # Python test suite (713 tests)
├── README.md
├── CHANGELOG.md
├── SECURITY.md
├── ARCHITECTURE.md
├── SECURITY.md
├── pyproject.toml
├── regenerate_kat.py
├── sbox_analysis_report.md
└── sbox_analysis_report.json
```

---

## Running Tests

```bash
# Python suite
python3 -m pytest tests/ -q

# C suite
cd cagoule/c
make tests
./test_math && ./test_matrix && ./test_sbox && ./test_cipher && \
./test_omega && ./test_ctr && ./test_format

# Fuzzing (requires clang)
make fuzz
./fuzz_cipher -max_len=65536 -runs=1000000

# KAT verification
python3 regenerate_kat.py --check
```

---

## Security Status

| Property | Status |
|---|---|
| ChaCha20-Poly1305 AEAD integrity | ✅ Standard, proven |
| Argon2id KDF | ✅ Standard, proven |
| CTR keystream uniqueness | ✅ Fixed in v3.0.1 (nonce-bound IV) |
| Python fallback S-box keyed | ✅ Fixed in v3.0.1 (Feistel port) |
| Algebraic layer IND-CPA | ⚠️ Conjectured, unproven — research ongoing |
| Formal security reduction | ❌ Open research problem |

See [SECURITY.md](SECURITY.md) for the full threat model and known limitations.

---

## License

MIT — see [LICENSE](LICENSE) file.

## Author

Slim Issa (LASS) — CTO, QuantOS  
Kairouan, Tunisia
