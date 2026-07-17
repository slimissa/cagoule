# Security Policy — CAGOULE v3.1.0

## Security Advisory — v3.1.0 Patch

v3.0.1 fixes two vulnerabilities present in v3.0.0 that affect confidentiality:

**CAGOULE-2026-001 (Critical): CTR two-time-pad via shared `params=`**
Calling `encrypt_ctr(message, password, params=<shared>)` with the same
`CagouleParams` object for multiple messages produced identical algebraic
keystreams, enabling full plaintext recovery. Fixed in v3.0.1 by binding the
IV to the per-message ChaCha20 nonce rather than `k_master` alone. The outer
ChaCha20-Poly1305 layer masked this from ciphertext observers, but was not
protection against an attacker who obtained the shared `params` object.

**CAGOULE-2026-002 (High): Python fallback S-box unkeyed**
In deployments without `libcagoule.so` (pure-Python mode), `SBoxPython` used
`x³ mod p` regardless of the password-derived `delta` parameter. The nonlinear
algebraic layer contributed zero key material. Fixed by porting the full Feistel
construction to Python, bit-exact against C.

Upgrade to v3.0.1. v3.0.0 CTR ciphertexts cannot be decrypted by v3.0.1
(IV formula changed). CBC ciphertexts (v0x01) are unaffected.

---

## Table of Contents

1. [Scope and Purpose](#1-scope-and-purpose)
2. [Cryptographic Primitives](#2-cryptographic-primitives)
3. [Security Model](#3-security-model)
4. [Threat Model — What CAGOULE Protects Against](#4-threat-model--what-cagoule-protects-against)
5. [Out of Scope — What CAGOULE Does NOT Protect Against](#5-out-of-scope--what-cagoule-does-not-protect-against)
6. [Known Limitations](#6-known-limitations)
7. [Side-Channel Considerations](#7-side-channel-considerations)
8. [Key Material Lifecycle](#8-key-material-lifecycle)
9. [Version Compatibility and Breaking Changes](#9-version-compatibility-and-breaking-changes)
10. [Reporting a Vulnerability](#10-reporting-a-vulnerability)

---

## 1. Scope and Purpose

CAGOULE is a **research-grade symmetric encryption system** designed as part of the QuantOS platform. It is not a production replacement for AES-GCM or ChaCha20-Poly1305. Its primary purpose is to explore a novel algebraic diffusion layer (Vandermonde over Z/pZ, Feistel S-box, ζ(2n)-derived round keys) wrapped by standardized cryptographic primitives.

This document defines the threat model, the security properties CAGOULE provides, and the explicit limitations that any deployer must understand before use.

---

## 2. Cryptographic Primitives

### 2.1 Standardized Primitives

| Primitive | Role | Standard | Implementation |
|---|---|---|---|
| Argon2id | Password-based KDF | RFC 9106 | `argon2-cffi` |
| HKDF-SHA256 | Key derivation and domain separation | RFC 5869 | OpenSSL (C-layer) |
| ChaCha20-Poly1305 | AEAD streaming encryption | RFC 8439 | `cryptography` (Python) |

These primitives are **not modified** and provide their standard security guarantees. Their parameter choices in CAGOULE are:

- **Argon2id (production)**: `t=3, m=64MB, p=1` — OWASP-compliant, ~114ms on a single core
- **Argon2id (multi-core)**: `t=3, m=64MB, p=4` — same security, ~51ms
- **HKDF**: distinct domain labels per derived key material (`"CAGOULE_N"`, `"CAGOULE_DELTA"`, `"CAGOULE_ENC"`, `"CAGOULE_PRIME_SEL_V25"`, `"CAGOULE_Z_SHIFT_V25"`, `"CAGOULE_CTR_V30"`, `"CAGOULE_NODE_*"`)

### 2.2 CAGOULE Algebraic Layer

| Component | Construction | Security Role |
|---|---|---|
| Vandermonde 16×16 matrix | Defined over Z/pZ — Mersenne-64 prime (v2.5.x) | Diffusion — MDS-like structure |
| 2-round Feistel S-box | `f(x, rk) = (x × rk) % P32_PRIME`, P32_PRIME = 2³² − 5 | Confusion |
| Round keys (64) | ζ(2n) → HKDF-SHA256 | Key schedule |
| Mersenne-64 prime pool | 8 primes of form p = 2⁶⁴ − k (k < 2¹⁰), HKDF-selected | Structural diversity of the field |
| Z-Domain Shifting | `byte[i] = (byte[i] + z_offset[i%16] % 256) % 256` — byte-level whitening | Pre-computation defense |
| CTR Mode (v3.0.0) | Counter mode with 4-block SIMD keystream pipeline | No inter-block dependency, streaming-friendly |

**Important**: the algebraic layer has **not been formally analyzed or peer-reviewed**. Its security properties are claimed by design but not proven. See section 6.

---

## 3. Security Model

### 3.1 Assumptions

CAGOULE's security rests on the following assumptions, all of which must hold for the system to be secure:

1. **Password strength**: the security of the entire system reduces to the entropy of the user-supplied password after Argon2id. A weak password nullifies all other protections.

2. **Attacker has no access to key material in memory**: CAGOULE does not defend against an attacker who can read process memory at encryption time.

3. **Attacker cannot observe timing below the granularity of a Python function call**: the C-layer arithmetic is constant-time (see section 7), but the Python wrapper is not.

4. **The underlying standardized primitives are secure**: if ChaCha20-Poly1305, Argon2id, or HKDF-SHA256 are broken, CAGOULE provides no additional protection.

5. **The random number generator is secure**: Argon2id salts and ChaCha20 nonces are generated via `os.urandom()`. Any RNG compromise breaks freshness guarantees.

6. **CTR nonce uniqueness** (v3.0.0): the CTR IV is derived from `k_master` via HKDF. Two encryptions with the same password and same salt would produce the same keystream. Each `encrypt()` call generates a fresh random salt, guaranteeing unique (password, salt) pairs.

### 3.2 Security Goals

| Goal | Mechanism | Status |
|---|---|---|
| Confidentiality | ChaCha20 encryption of algebraically-transformed plaintext | ✅ Provided |
| Integrity and authenticity | Poly1305 authentication tag (16 bytes) | ✅ Provided |
| Wrong-password detection | AEAD tag verification before plaintext is returned | ✅ Provided |
| Nonce uniqueness | 96-bit random nonce per `encrypt()` call | ✅ Provided |
| Salt uniqueness | 256-bit random salt per `encrypt()` call | ✅ Provided |
| CTR IV uniqueness | HKDF-derived from k_master + random salt | ✅ Provided (v3.0.0) |
| Forward secrecy | ❌ Not provided — same password re-derives same material |
| Key rotation | ❌ Not built in — application responsibility |

---

## 4. Threat Model — What CAGOULE Protects Against

### 4.1 Passive attacker with ciphertext access

An attacker who can read CGL1-format ciphertexts but does not know the password gains:

- **Nothing about the plaintext**: ChaCha20 stream cipher provides IND-CPA security; the algebraic layer adds diffusion before AEAD encryption.
- **Nothing about the key material**: `k_master`, round keys, Mersenne prime index, `z_offset`, and CTR IV are all derived via HKDF from an Argon2id-hardened password. Brute-forcing the password is the only viable attack path.
- **No field structure information**: the Mersenne prime index is determined by `HKDF(k_master, "CAGOULE_PRIME_SEL_V25")[0] % 8` — unknown to the attacker, preventing field-specific precomputation.
- **No keystream reuse**: CTR mode uses a unique IV derived from `k_master` combined with a random salt per encryption. Keystream blocks are never reused across messages.

### 4.2 DDT precomputation attacks on the algebraic layer

A classical differential cryptanalysis approach requires building the Difference Distribution Table (DDT) of the Vandermonde matrix over a known field Z/pZ. CAGOULE counters this with two complementary mechanisms:

- **Mersenne prime pool** (v2.5.x): the attacker cannot know which of the 8 primes is in use without the password. Precomputing DDTs for all 8 fields simultaneously multiplies the attacker's work by 8.
- **Z-Domain Shifting** (v2.5.x): `z_offset[16]` is derived from `k_master` and applied as byte-level whitening before the algebraic layer. Even with the correct field, the attacker does not know the starting point of the algebraic transformation. This is functionally equivalent to DES-X key whitening.

### 4.3 Ciphertext forgery

The Poly1305 authentication tag prevents any attacker from producing a valid ciphertext for a message they did not encrypt. Any bitflip in the ciphertext or header produces a tag mismatch. CAGOULE rejects forged ciphertexts before decrypting, so no plaintext oracle is available.

### 4.4 Replay attacks at the application level

Each `encrypt()` call generates an independent 256-bit salt and 96-bit nonce. Two encryptions of the same plaintext with the same password produce different ciphertexts. Note: CAGOULE does not implement sequence numbers or replay detection — this is the application's responsibility.

### 4.5 Brute-force attacks on the password

Argon2id with `t=3, m=64MB, p=1` requires approximately 114ms and 64MB of RAM per attempt on the target hardware. This makes GPU/ASIC password cracking expensive. An attacker with a 10,000-GPU cluster would need:

- ~1.14 ms per attempt per GPU (amortized, highly optimized)
- Against a 72-bit entropy password: > 2⁷² / (10⁴ × ~876 attempts/s) ≈ astronomical time

Weak passwords (dictionary words, short PINs) remain vulnerable regardless of Argon2id parameters.

---

## 5. Out of Scope — What CAGOULE Does NOT Protect Against

### 5.1 Compromised endpoint

If the attacker controls the machine running CAGOULE (keylogger, memory dump, process injection), all security guarantees are void. CAGOULE does not implement secure enclaves or TEE protection.

### 5.2 Side-channel attacks via Python layer

The Python wrapper (`cipher.py`, `decipher.py`, `cipher_ctr.py`, `decipher_ctr.py`, `params.py`) is **not constant-time**. Python object construction, ctypes dispatch, and AEAD operations have data-dependent timing at the microsecond scale. An attacker capable of sub-millisecond timing measurements on the Python API may extract information. The C algebraic layer is constant-time (see section 7), but this protection does not extend to the full API.

### 5.3 Key management

CAGOULE encrypts and decrypts data. It does not:
- Store, rotate, or revoke keys
- Implement key agreement protocols
- Provide multi-party encryption
- Support key derivation from hardware tokens

Key management is entirely the application's responsibility.

### 5.4 Metadata

CGL1 format reveals:
- The ciphertext **length** (and therefore approximate plaintext length; for CTR v0x02: exact plaintext length since there is no padding)
- The **version byte** (`0x01` for CBC, `0x02` for CTR v3.0.0)
- The **magic** (`CGL1`)
- That the data was encrypted with CAGOULE

Traffic analysis, timing of encryption operations, and ciphertext length analysis are not protected.

### 5.5 Nonce reuse in encrypt_bulk()

In v3.0.0, `encrypt_bulk()` defaults to CTR mode. It derives a fresh `CagouleParams` per message (each with a unique salt), guaranteeing unique keystreams. However, if `os.urandom()` is compromised (predictable RNG), salt collisions become possible.

### 5.6 Forward secrecy

Re-encrypting data with the same password re-derives the same `k_master`, Mersenne prime, and `z_offset`. An attacker who obtains the password retroactively can decrypt all past ciphertexts encrypted with that password. CAGOULE provides no forward secrecy.

### 5.7 Formal cryptanalysis

The CAGOULE algebraic layer (Vandermonde diffusion, Feistel S-box, ζ(2n) round keys) has **not undergone formal peer review or public cryptanalysis**. No proof of security beyond the informal design arguments in ARCHITECTURE.md exists. Users requiring provably secure constructions should use AES-GCM or XChaCha20-Poly1305.

---

## 6. Known Limitations

### 6.1 Performance vs. standard primitives

CAGOULE is approximately **80× slower** than AES-256-GCM at 1MB on the same hardware in CBC mode (~6.9 MB/s vs ~2669 MB/s). CTR mode improves this to ~19.7 MB/s (~135× slower). This is a fundamental consequence of the custom algebraic layer being implemented without hardware acceleration. It is a research trade-off, not a bug.

### 6.2 Version compatibility

- v2.5.x is **not compatible** with v2.4.x ciphertexts (Mersenne prime pool changed the field Z/pZ).
- v3.0.0 introduces CTR mode (CGL1 v0x02). CBC ciphertexts (v0x01) from v2.5.x remain decryptable in v3.0.0 via automatic VERSION dispatch.
- CTR and CBC ciphertexts are **not interchangeable** — `decrypt()` dispatches automatically based on the VERSION byte.
- A migration utility is provided: `migrate_cbc_to_ctr(ciphertext_cbc, password)`.

### 6.3 x86-64 Linux only

The AVX2 backend (`mulmod_mersenne64x4`, `cagoule_matrix_avx2`, `cagoule_sbox_avx2`) targets x86-64 with GCC on Linux. ARM NEON, Apple Silicon, and Windows are not supported. Python fallbacks are available but are significantly slower.

### 6.4 ~~Memory allocation in C hot path~~ (RESOLVED in v2.5.4)

~~When Z-Domain Shifting is active, `cagoule_cbc_encrypt` performs a `malloc` for the shifted buffer.~~

**Resolved in v2.5.4**: The malloc was eliminated. Z-Domain Shifting is now applied inline using a stack-allocated `zo_byte[16]` array. No heap allocation occurs in the encryption hot path. This applies to both CBC and CTR modes.

### 6.5 ~~The `malloc` hot-path failure mode~~ (RESOLVED in v2.5.4)

**Resolved**: Since the malloc was eliminated in v2.5.4, this failure mode no longer exists.

### 6.6 2-round Feistel algebraic degree

The S-box uses a 2-round Feistel network with degree-1 round functions. The overall algebraic degree is limited. A 3-round variant is under consideration for future releases to increase the security margin.

---

Add this section under **6. Known Limitations**:

### 6.7 AAD does not include the nonce

The AEAD authenticated additional data in CGL1 v0x02 is:

```
AAD = MAGIC(4) || VERSION(1) || SALT(32)
```

The ChaCha20-Poly1305 nonce (12 bytes, offset 37-48 in the CGL1 header) is **not**
included in the AAD. It IS included as the ChaCha20-Poly1305 nonce input itself,
so modifying the header nonce bits changes the AEAD keystream and causes
tag verification to fail. This provides incidental authentication of the nonce.

However, this protection relies on the nonce being the AEAD nonce — a property
that does not hold in the streaming chunk-index scheme (v3.1.0 `cagoule_stream.c`,
Feature 4) or in future wire formats where the nonce may be carried separately.

**Mitigation**: In v0x02 (ChaCha20-Poly1305), nonce modification is detected
by AEAD tag failure. In v0x03 (Poly1305-only, experimental), the AAD already
includes `MAGIC || VERSION || SALT` and the VERSION byte prevents cross-mode
confusion.

**Plan**: Add nonce to AAD in v3.2.0:
AAD = MAGIC(4) || VERSION(1) || SALT(32) || NONCE(12)
This will be a wire-format-breaking change and will ship with a VERSION byte bump.


## 7. Side-Channel Considerations

### 7.1 Constant-time operations in the C layer

The following operations in `libcagoule.so` are implemented without data-dependent branching:

| Operation | Mechanism | File |
|---|---|---|
| `mulmod_mersenne64x4` | Bitmask reduction, no `DIV`, no `if` on data | `cagoule_math_avx2.h` |
| `addmod64x4` / `submod64x4` | Masked conditional subtract via `_mm256_cmpgt_epi64` + XOR flip | `cagoule_math_avx2.h` |
| CTR keystream generation | Same primitives as CBC, no inter-block branches | `cagoule_ctr.c` |
| Poly1305 tag comparison | `secrets.compare_digest()` in Python | `decipher.py`, `decipher_ctr.py` |
| `mulmod64` (scalar) | No branch on operands | `cagoule_math.c` |

Unsigned comparison in AVX2 is implemented via MSB flip (`XOR 0x8000000000000000`), since `_mm256_cmpgt_epi64` is signed-only:

```c
// Constant-time unsigned a > b detection:
__m256i flip = _mm256_set1_epi64x(0x8000000000000000ULL);
__m256i gt   = _mm256_cmpgt_epi64(
    _mm256_xor_si256(a, flip),
    _mm256_xor_si256(b, flip));
// gt = 0xFF..FF if a > b (unsigned), 0x00..00 otherwise — no branch
```

### 7.2 Operations that are NOT constant-time

- **Argon2id**: memory-hard by design; timing is proportional to `m_cost`. Not a leak of plaintext.
- **PKCS7 unpadding** (CBC only): Python-level byte inspection. Not a padding oracle in isolation, but do not expose error messages that distinguish "bad padding" from "bad tag". CTR mode has no padding, eliminating this concern.
- **Python wrapper**: all Python-level code. Data-dependent timing at microsecond scale.
- **`omega.py` (ζ computation)**: `mpmath` fallback is not constant-time. The C backend (`cagoule_omega_generate_round_keys` via OpenSSL HKDF) is deterministic and cache-friendly.
- **Cycle-walking** in S-box: probability < 2^-54 for Mersenne primes. Statistically undetectable.

### 7.3 Valgrind status

All 12 C test binaries (including CTR) pass Valgrind with zero memory errors and zero leaks in the v3.0.0 release. Valgrind does not detect timing side-channels — it verifies memory safety only.

### 7.4 Fuzzing status

The libFuzzer harness has been exercised for 1,000,000 runs on both CBC and CTR code paths with AddressSanitizer and UndefinedBehaviorSanitizer enabled. Zero crashes detected.

---

## 8. Key Material Lifecycle

```
password  ──►  Argon2id  ──►  k_master (64 bytes)
                                    │
        ┌───────────────────────────┼───────────────────────────┐
        │                           │                           │
  HKDF(k_master)              HKDF(k_master)              HKDF(k_master)
  "CAGOULE_ENC"               "CAGOULE_DELTA"             "CAGOULE_Z_SHIFT_V25"
        │                           │                           │
  k_stream (32B)               rk0, rk1 (S-box)            z_offset[16]
  (ChaCha20 key)                                           (Z-Domain Shift)
        │
  HKDF(k_master)
  "CAGOULE_CTR_V31"     ← v3.1.0
        │
  IV_CTR (8 bytes)

ZEROIZATION
  CagouleParams.zeroize()  →  secure_zeroize(k_master, round_keys, z_offset)
  Context manager (__exit__)  →  automatic zeroize on scope exit
  Destructor (__del__)  →  GC fallback (not reliable — use context manager)
```

**Rules for deployers:**

- Always use `with CagouleParams.derive(password) as p:` to guarantee zeroization.
- Do not cache `CagouleParams` objects beyond their encryption session.
- `encrypt_bulk()` derives per-message params (v3.0.0) and zeroizes at function exit.
- `k_master` is never written to disk or included in the CGL1 ciphertext output.
- The CTR IV is derived from `k_master`, not stored in the ciphertext header.

---

## 9. Version Compatibility and Breaking Changes

| From | To | Compatible? | Notes |
|---|---|---|---|
| v1.x | v2.x | ❌ No | Feistel S-box replaced x^d — vault incompatibility |
| v2.0–v2.4 | v2.5.x | ❌ No | Mersenne prime pool changed field Z/pZ |
| v2.5.x | v3.0.0 | ✅ Yes* | CBC (v0x01) retained. CTR (v0x02) is new format. |
| v2.5.0 | v2.5.1 | ✅ Yes | AVX2 detection fix only |
| v2.5.1 | v2.5.2 | ✅ Yes | Tests only — no cryptographic change |
| v2.5.2 | v2.5.3 | ✅ Yes | Documentation fixes only |
| v2.5.3 | v2.5.4 | ✅ Yes | Z-Domain malloc eliminated, security hardening |

*CBC ciphertexts (v0x01) from v2.5.x decrypt correctly in v3.0.0 via automatic VERSION dispatch.
CTR ciphertexts (v0x02) are new in v3.0.0 and cannot be decrypted by earlier versions.
Use `migrate_cbc_to_ctr()` to convert CBC ciphertexts to CTR format.

The CGL1 wire format (`MAGIC | VERSION | SALT | NONCE | CT | TAG`) is stable.
VERSION 0x01 = CBC, VERSION 0x02 = CTR (v3.0.0).
Breaking changes will increment the minor version and will be announced with migration guidance.

---

## 10. Reporting a Vulnerability

**Do not open a public GitHub issue for security vulnerabilities.**

Report security issues privately to:

**Slim Issa** — [github.com/slimissa](https://github.com/slimissa)

Please include in your report:

- CAGOULE version affected
- A description of the vulnerability and its security impact
- Steps to reproduce or a proof-of-concept (if applicable)
- Whether you believe the vulnerability is in the algebraic layer, the standardized primitives, the CTR mode, or the Python wrapper

Expected response time: **72 hours**.

### Scope of accepted reports

| Category | In scope |
|---|---|
| Incorrect constant-time implementation in `mulmod_mersenne64x4` or comparison functions | ✅ |
| Memory safety bugs in `libcagoule.so` (buffer overflow, use-after-free) | ✅ |
| Authentication bypass or tag forgery | ✅ |
| Wrong-password acceptance | ✅ |
| Nonce, salt, or CTR IV reuse | ✅ |
| Key material leakage into ciphertext or logs | ✅ |
| Algebraic weaknesses in the Vandermonde / Feistel construction | ✅ |
| CTR keystream prediction or reuse | ✅ (v3.0.0) |
| Slower-than-expected performance | ❌ — not a security issue |
| Incompatibility with non-Linux platforms | ❌ — known limitation |
| Python wrapper timing side-channels | ⚠️ Accepted but lower priority — documented limitation |

---

## Appendix — Quick Security Reference

```
✅ CAGOULE v3.0.0 provides:
   Confidentiality    — ChaCha20 + algebraic diffusion layer (CBC or CTR)
   Integrity          — Poly1305 authentication tag (16 bytes)
   Wrong-password     — AEAD tag check before plaintext return
   Nonce freshness    — 96-bit os.urandom() per encrypt()
   KDF hardening      — Argon2id t=3, m=64MB (OWASP-compliant)
   Field diversity    — 8 Mersenne-64 primes, HKDF-selected
   Whitening          — z_offset[16] byte-level key whitening
   CTR mode           — No padding, streaming-friendly, 4-block SIMD pipeline
   CBC→CTR migration  — migrate_cbc_to_ctr() utility
   Auto-dispatch      — decrypt() handles both v0x01 (CBC) and v0x02 (CTR)

❌ CAGOULE does NOT provide:
   Forward secrecy
   Key management / rotation / revocation
   Protection against compromised endpoints
   Constant-time Python API
   Formal security proof of the algebraic layer
   ARM / Apple Silicon / Windows support
   Metadata concealment (ciphertext length visible)
```

---

*CAGOULE — Cryptographie Algébrique Géométrique par Ondes et Logique Entrelacée*
*Slim Issa — Kairouan, Tunisia — Part of the QuantOS platform*
*License: MIT*
