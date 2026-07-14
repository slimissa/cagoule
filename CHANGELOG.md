# CAGOULE Changelog

---

## v3.0.1 — Security Patch (2026-07-13)

This release closes 10 bugs identified during an independent empirical audit
of v3.0.0 + the v3.1.0 feature branch. All fixes are verified by re-running
the original exploit or test scenario, not just by code inspection.

**713 Python tests pass. 7 C test binaries pass (468,857+ assertions). 0 failures.**

### Critical Fixes

**Bug 2 — CTR two-time-pad via shared `params=` (3 iterations to close)**

Root cause: IV was derived from `k_master` alone — shared `params` meant
shared IV across all messages, enabling keystream reuse. Three fix attempts
were required:
- Attempt 1: dual-path (`msg_salt` for bulk, none for single) — single-message
  path untouched; bulk decrypt side never updated.
- Attempt 2: unified `header_salt` formula — correct on the encrypt side but
  broke the CGL1 invariant: fresh `os.urandom()` written to header ≠ `params.salt`
  that produced `k_master`/`k_stream`, making cross-session decryption impossible.
- **Final fix (this release):** IV bound to the ChaCha20 **nonce** (12 bytes,
  already random per message, already in the CGL1 header):
  `IV = HKDF(k_master, b'CAGOULE_CTR_V30' + nonce, 8)`.
  Header salt = `params.salt` in shared mode, preserving the CGL1 invariant
  `(password, header_salt) → k_master/k_stream` fully reproducible cross-session.
  Added `test_cross_session_roundtrip_no_params_object` — the test that would
  have caught every previous broken fix immediately.

**Bug 6 — Python fallback S-box completely unkeyed for production primes**

`SBoxPython.from_delta` silently used `x³ mod p` (ignoring `delta` entirely)
for all primes ≥ `_LARGE_PRIME_THRESHOLD`. In a pure-Python deployment without
`libcagoule.so`, the nonlinear layer contributed zero key material.
Fix: full port of the 2-round Feistel with cycle-walking, bit-exact against C
across all 8 Mersenne-64 primes × 4 deltas × 500 values (16,000 comparisons,
0 mismatches). Two bugs found during porting: XOR (not addition) for half-block
combination, and correct uint32 masking per the C implementation.

### High Severity Fixes

**Bug 3 — `CagouleParams.__reduce__` leaked `k_master` in plaintext via pickle**

`ProcessPoolExecutor` (recommended in the docstring) pickles arguments through
OS IPC pipes. `k_master` appeared verbatim in the pickle blob. Fix: `__reduce__`
now raises `TypeError` with an explicit explanation and the safe alternative.
Companion fix: `cipher.py` docstring updated to stop recommending `ProcessPoolExecutor`.

**Bug 7 — `Fp2Element.sqrt()` wrong formula + deeper structural finding**

Two formula attempts failed (`(p²+1)//4`, `(p²+p)//4`) because `p ≡ 1 mod 4`
for all production primes. The real finding: `Z/pZ[t]/(t²+t+1)` is only a field
when `p ≡ 2 mod 3`. Two of the 8 production primes (k=189, k=279) are not fields
under this construction — `sqrt()` cannot apply. Fix: Tonelli-Shanks generalized
to `Fp²`, with explicit `ArithmeticError` for non-field primes. Confirmed inert
in production (`mu.py` already avoids those 2 primes via strategy A). Test rewritten
to verify against brute-force ground truth (120/120, 0 mismatches) rather than
silently accepting `ArithmeticError` as a pass.

### Medium Severity Fixes

**Bug 1 — `_buffer_pool.py::_get_rk_arr` missing memset on grow path**

The other 3 pool functions (`_get_padded_buf`, `_get_out_buf`, `_get_input_buf`)
zeroize the old buffer before replacing it on a grow request. `_get_rk_arr` did
not, leaving stale round-key material from a previous (possibly different-password)
operation in the unused tail. Currently inert (C layer only reads `nk` explicit
elements), but inconsistent with the file's own declared security model. Fixed.

**Bug 5 — `omega.py` mpmath gate structurally unreachable**

`mpmath` was imported only inside `if not _OMEGA_C_SYMBOLS_OK` — meaning on any
normal build with `libcagoule.so`, `_mpmath_available` stayed `False` permanently,
silently skipping all 8 `TestBitExactCompatibility` tests. These tests verify C↔Python
bit-exact agreement for ζ(2n) round-key derivation — the one place a real
cross-backend divergence would matter. Fix: unconditional import.

### Low Severity / Code Quality Fixes

**Bug 4 — `migrate_cbc_to_ctr()` zeroizes a copy, not the original**

Not fixable without `decrypt_into(buf)` API (deferred to v3.2.0). `bytes` is
immutable in CPython — any zeroize call on `bytearray(plaintext)` touches a copy.
The docstring now documents this limitation honestly rather than implying secure wipe.

**Bug 8 — `_parse_cgl1` rejected version 0x02 with a generic error**

The function should reject 0x02 (it is the CBC-only parser). The bug was the
quality of the rejection: no indication of what went wrong or where to look.
Fix: explicit message "version CTR received in CBC parser — use `decrypt()` or
`decrypt_ctr()`."

**Bug 9 — `mu.py` dead functions with missing collision avoidance**

`generate_vandermonde_nodes` and `generate_cauchy_beta`: both have zero callers,
both lack the deduplication that `params._derive_nodes` and `matrix.py` implement.
Fix: both now raise `NotImplementedError` with documentation of the bug and
redirection to the correct production functions.

**Bug 10 — `sbox_analysis_report` tested the wrong S-box**

The corrected report tested `x³+cx` on small primes (p < 100), not the production
Feistel construction. Fix: report regenerated against `SBoxC` (real C Feistel).
Added explicit statistical detection floor: 50,000 samples has zero power to detect
biases in the `~2⁻⁶⁴` probability regime that matters cryptographically. IND-CPA
status: NON PROVED — formal analysis required.

### Wire Format Compatibility

v3.0.1 CTR ciphertexts (CGL1 v0x02) are **not compatible** with v3.0.0 CTR
ciphertexts. The IV formula changed (now includes the ChaCha20 nonce). CBC
ciphertexts (CGL1 v0x01) are unaffected.

---

## v3.0.0 — CTR Mode Release (2026-05-28)

Initial public release with CTR mode pipeline, AVX2 4x keystream, CGL1 v0x02
format, auto-dispatch decrypt, and Python CTR layer with C backend + fallback.
