Update the header documentation (cagoule_cipher.h) to accurately reflect the byte-level implementation. The current header comment is misleading.

Add a roadmap item for v2.5.1 or v3.0.0: "Port Z-Domain Shifting to mod-p algebraic layer." This would use addmod64x4 on the 16 uint64_t elements after _load_plain and before the matrix multiplication. It would be cleaner and potentially faster.

Consider renaming the parameter from z_offset to something that distinguishes the two approaches, like z_byte_offset vs z_field_offset, to avoid confusion when the algebraic version is eventually implemented.


This is an excellent question that shows you're thinking like a true engineering leader. Let me provide a concrete, actionable roadmap to elevate each A/A- grade to A+.

---

## Roadmap to A+ Across All Categories

---

### 1. Security: A → A+

**Current State**: Constant-time operations, secure zeroization, versioned domain separation. This is already very strong, but A+ cryptographic code requires formal verification or external validation.

**Actions to Reach A+**:

| # | Action | Effort | Impact |
|---|--------|--------|--------|
| 1 | **Add `ctgrind`/`dudect` constant-time tests** for `mulmod_mersenne64x4`, `_cmpgt_epu64`, and the S-box reduction. These tools empirically verify that execution time does not vary with secret data. | Medium | Eliminates timing side-channel risk |
| 2 | **Add test vectors from an independent implementation** (e.g., a Rust or Go reference that implements the same algorithm from the spec). This catches implementation-specific bugs. | High | Catches spec ambiguity |
| 3 | **Document the IND-CPA/IND-CCA2 security model** in a `SECURITY.md` file. State explicitly: "CAGOULE v2.5.0 aims for IND-CCA2 security under the assumption that..." | Low | Clarifies security boundaries for auditors |
| 4 | **Run the NIST Statistical Test Suite** on ciphertext output and publish the results. Demonstrates that ciphertext is indistinguishable from random. | Low-Medium | Empirical security evidence |

**Most impactful single action**: #1 (constant-time testing). It transforms "we believe it's constant-time" into "we have empirical evidence it's constant-time."

---

### 2. Maintainability: A → A+

**Current State**: Excellent comments, clear architecture, version annotations. To reach A+, you need structured documentation that a newcomer could use to onboard in under an hour.

**Actions to Reach A+**:

| # | Action | Effort | Impact |
|---|--------|--------|--------|
| 1 | **Add an `ARCHITECTURE.md`** with a data-flow diagram showing: password → Argon2id → HKDF → round keys + z_offset → CBC pipeline with Z-Domain → ChaCha20-Poly1305 → ciphertext. | Medium | Visual onboarding |
| 2 | **Document every `static inline` function's register budget** in the AVX2 headers. Example: `_feistel_pass_avx2: uses 6 YMM temporaries + 2 constants = 8 YMM`. | Low | Prevents future spill regressions |
| 3 | **Add a `CONTRIBUTING.md`** explaining: how to add a new Mersenne prime, how to add a new test suite, the Git commit convention, and the release checklist. | Low | Enables external contributors |
| 4 | **Create a one-line `make doc` target** that runs Doxygen on the headers. The comments already have Doxygen-style annotations. | Low | Professional documentation output |

**Most impactful single action**: #1 (ARCHITECTURE.md). A single diagram showing how data flows through the system is worth more than all the inline comments combined for a new developer.

---

### 3. Portability: A- → A+

**Current State**: AVX2 conditional compilation, scalar fallbacks. The weakness is the hard dependency on x86_64 intrinsics and `__uint128_t`.

**Actions to Reach A+**:

| # | Action | Effort | Impact |
|---|--------|--------|--------|
| 1 | **Add an ARM NEON backend** for `mulmod64x4` and the S-box. The 16×16 matrix can use `uint64x2_t` with NEON's `PMULL` instruction. | High | Enables Apple Silicon, AWS Graviton, mobile |
| 2 | **Replace `__uint128_t` with a portable multi-precision fallback** using two `uint64_t` values. This enables MSVC and non-GCC/Clang compilers. | Medium | Windows/MSVC compatibility |
| 3 | **Add a CI matrix** that tests on: Ubuntu x86_64, Ubuntu ARM64 (via QEMU), macOS x86_64, macOS ARM64. GitHub Actions supports all of these. | Medium | Prevents platform regressions |
| 4 | **Add a `cmake` build option** alongside the existing `Makefile`. This is the industry standard for cross-platform builds. | Medium | Unlocks Windows, embedded Linux, etc. |

**Most impactful single action**: #3 (CI matrix). Even without a full NEON backend, testing the scalar fallback on ARM64 in CI ensures the code *works* everywhere, even if it's not fast everywhere.

**Pragmatic approach**: Start with #3 and #4 (CI matrix + CMake). These are infrastructure wins. Add #1 (NEON backend) as a v3.0.0 feature when the CTR mode roadmap item is addressed.

---

### 4. Error Handling: A → A+

**Current State**: Explicit error codes, null checks, bounded loops. To reach A+, you need systematic fuzz testing and consistent error recovery semantics.

**Actions to Reach A+**:

| # | Action | Effort | Impact |
|---|--------|--------|--------|
| 1 | **Add a fuzz harness** using libFuzzer or AFL++ that feeds random bytes to `cagoule_cbc_encrypt` / `_decrypt` and verifies: no crashes, no memory leaks (via ASAN), and `decrypt(encrypt(m)) == m`. | Medium | Finds edge cases no human would test |
| 2 | **Make `CAGOULE_ERR_CORRUPT` carry diagnostic information.** Currently it signals corruption but doesn't say *where*. Add an `out_error_offset` parameter or a separate `cagoule_get_last_error()` function. | Low | Debugging aid for users |
| 3 | **Add bounds-checked accessors** for all array parameters. The `num_keys <= CAGOULE_OMEGA_MAX_KEYS` check is good. Add similar for `n_blocks`, `salt_len`, and the output buffer size before any processing starts. | Low | Prevents buffer overflows |
| 4 | **Test error paths explicitly.** The test suite should have cases that trigger every `CAGOULE_ERR_*` code: NULL pointers, zero-size inputs, oversized parameters, and deliberately corrupted ciphertexts. | Low-Medium | Full error coverage |

**Most impactful single action**: #1 (fuzz harness). A few hours of fuzzing will find bugs that months of manual testing would miss. Combined with AddressSanitizer and UndefinedBehaviorSanitizer, this is the gold standard for error handling validation.

---

### 5. Prioritized Action Plan (80/20 Rule)

If you have limited time, here's the order that delivers maximum improvement per hour invested:

| Priority | Action | Category | Effort |
|----------|--------|----------|--------|
| **1** | Write `ARCHITECTURE.md` with data-flow diagram | Maintainability | 2-3 hours |
| **2** | Add CI matrix (x86_64, ARM64, macOS) | Portability | 2-3 hours |
| **3** | Add fuzz harness with ASAN/UBSAN | Error Handling | 3-4 hours |
| **4** | Add constant-time empirical tests (dudect) | Security | 4-5 hours |
| **5** | Document register budgets in AVX2 headers | Maintainability | 1-2 hours |
| **6** | Add CMake build alongside Makefile | Portability | 3-4 hours |
| **7** | Write `SECURITY.md` with threat model | Security | 2-3 hours |
| **8** | ARM NEON backend (v3.0.0 roadmap item) | Portability | 20-30 hours |

**Total for A+ across all categories (excluding NEON)**: ~20-25 hours of focused work.

---

### Summary of Target State

| Category | Current | Target | Key Enabler |
|----------|---------|--------|-------------|
| Security | A | **A+** | Constant-time empirical validation (dudect) |
| Maintainability | A | **A+** | ARCHITECTURE.md with visual data-flow diagram |
| Portability | A- | **A+** | Multi-platform CI matrix + CMake build |
| Error Handling | A | **A+** | Fuzz harness with ASAN/UBSAN integration |

With these additions, CAGOULE v2.5.0 would not just be an excellent research implementation—it would be a model of how cryptographic software *should* be engineered, suitable for citation as a reference implementation in academic papers and for security audits.


## Roadmap: Every Test File to A+ (No Code, Just Plan)

---

### 1. `test_cipher.c`: A- → A+

| Gap | Root Cause | Fix | Effort | New Assertions |
|-----|-----------|-----|--------|----------------|
| v2.5.0 Coverage: C | All encrypt/decrypt calls pass `NULL, 0` for `z_offset` | Add `test_z_domain_shifting()` that encrypts/decrypts with non-zero `z_offset[16]`, verifies roundtrip, verifies output differs from non-shifted path, and tests all-zero z_offset edge case | 20 min | 6 |

---

### 2. `test_cipher_pipeline4.c`: A → A+

| Gap | Root Cause | Fix | Effort | New Assertions |
|-----|-----------|-----|--------|----------------|
| v2.5.0 Coverage: C | No Z-Domain Shifting in pipeline4 tests | Add `test_z_domain_pipeline4()` that runs pipeline4 (8 blocks) + residual (9 blocks) + larger sizes (16, 17 blocks) with non-zero `z_offset`, verifying roundtrip for each | 15 min | 12 |

---

### 3. `test_math.c`: B+ → A+

| Gap | Root Cause | Fix | Effort | New Assertions |
|-----|-----------|-----|--------|----------------|
| v2.5.0 Coverage: C | No `cagoule_mersenne_k()` tests | Add `test_mersenne_pool()` that verifies lookup returns correct `k` for all 8 Mersenne primes and returns 0 for non-pool primes (P, 97, 2) | 10 min | 11 |
| Edge Cases: B+ | Missing overflow/underflow for addmod/submod | Add 4 assertions: `addmod(P-1, P-1, P)`, `addmod(P-1, 2, P)`, `submod(0, P-1, P)`, `submod(1, P-1, P)` | 5 min | 4 |
| Code Quality: A | Version string says v2.4.0 | Change header printf to v2.5.0 | 1 min | 0 |

---

### 4. `test_math_avx2.c`: A → A+

| Gap | Root Cause | Fix | Effort | New Assertions |
|-----|-----------|-----|--------|----------------|
| v2.5.0 Coverage: D | No `mulmod_mersenne64x4` parity tests | Add `test_mersenne_parity()`: 512 random cases × 8 Mersenne primes × 4 lanes, comparing `mulmod_mersenne64x4` vs scalar `mulmod64` | 20 min | 16,384 |
| v2.5.0 Coverage: D | No Mersenne edge cases | Add `test_mersenne_edge_cases()`: same 8 cases from `test_edge_cases` (0,0; 0,p-1; p-1,0; p-1,p-1; 1,p-1; p-1,1; p/2,p/2; 1,1) applied to all 8 Mersenne primes via `mulmod_mersenne64x4` | 10 min | 256 |
| v2.5.0 Coverage: D | No Mersenne benchmark | Add Mersenne throughput measurement in `bench_comparison()`, comparing `mulmod_mersenne64x4` vs `mulmod64x4` Barrett to validate the -41% instruction reduction claim | 10 min | 2 |

---

### 5. `test_mersenne.c`: A+ (Already)

| Status | Why |
|--------|-----|
| **A+ Maintained** | 4,000,032 assertions cover all 8 primes with random parity + algebraic edge cases. Roadmap P0 requirement (500K per prime) exceeded 8×. No changes needed. |

---

### 6. `test_matrix.c`: A- → A+

| Gap | Root Cause | Fix | Effort | New Assertions |
|-----|-----------|-----|--------|----------------|
| v2.5.0 Coverage: C+ | No tests with Mersenne pool primes | Add loop in `test_roundtrip` or a new `test_mersenne_matrix()` that builds matrices with all 8 Mersenne primes, verifies `k_mersenne > 0`, runs `cagoule_matrix_verify`, and checks roundtrip | 15 min | 24 |
| Edge Cases: A | Missing `n != CAGOULE_N` invalid test | Add `CHECK(cagoule_matrix_build(nodes, 8, P) == NULL, "n=8 → NULL")` to `test_invalid_params` | 2 min | 1 |

---

### 7. `test_matrix_avx2.c`: A → A+

| Gap | Root Cause | Fix | Effort | New Assertions |
|-----|-----------|-----|--------|----------------|
| v2.5.0 Coverage: C+ | No parity tests with Mersenne primes | Add `test_mersenne_parity_avx2()`: for all 8 Mersenne primes, build matrix, verify `k_mersenne > 0`, run 100 random vectors through AVX2 dispatch vs scalar path for both forward and inverse | 15 min | 25,600 |
| v2.5.0 Coverage: C+ | No Mersenne roundtrip test | Add `test_mersenne_roundtrip_avx2()`: verify P×P⁻¹=I for all 8 Mersenne primes using the AVX2 dispatch path | 10 min | 2,048 |

---

### 8. `test_omega.c`: A+ (Already)

| Status | Why |
|--------|-----|
| **A+ Maintained** | 89 tests across 8 sections. Covers mathematical correctness, determinism, sensitivity, error handling, performance, and thread safety. Version string correctly says v2.5.0. No changes needed. |

---

### 9. `test_sbox.c`: A- → A+

| Gap | Root Cause | Fix | Effort | New Assertions |
|-----|-----------|-----|--------|----------------|
| Edge Cases: B+ | No cycle-walking stress test | Add test with prime just below power of 2 (e.g., `2^32 - 5`) where cycle-walking probability is non-negligible; verify all outputs < p and roundtrip works | 10 min | 2 |
| Edge Cases: B+ | No zero round key test | Add test with `rk0=0, rk1=0` passed to `cagoule_sbox_init`; verify they are upgraded to 1 and S-box still works (bijective + roundtrip) | 5 min | 2 |
| v2.5.0 Coverage: A- | Only uses one Mersenne prime implicitly | Add explicit test with `P = 18446744073709551557ULL` (Mersenne pool prime) verifying Feistel path works correctly | 5 min | 2 |
| v2.5.0 Coverage: A- | Missing `cagoule_sbox_backend_is_avx2` test | Add `CHECK(cagoule_sbox_backend_is_avx2() == 0 \|\| cagoule_sbox_backend_is_avx2() == 1, "backend_is_avx2 returns bool")` | 2 min | 1 |

---

### 10. `test_sbox_avx2.c`: A+ (Already)

| Status | Why |
|--------|-----|
| **A+ Maintained** | ~22,501 assertions with cross-product design (4 primes × 4 deltas × 100 iterations). Includes one Mersenne prime. Edge cases cover 8 values × forward + inverse + mixed lanes. Backend detection tested. Benchmark with anti-optimization. Minor enhancement: add remaining 7 Mersenne primes to test prime list (optional, current coverage already excellent). |

---

## Summary: Effort and Impact

| Test File | Current Grade | Target Grade | Effort | New Assertions |
|-----------|--------------|--------------|--------|----------------|
| `test_cipher.c` | A- | **A+** | 20 min | 6 |
| `test_cipher_pipeline4.c` | A | **A+** | 15 min | 12 |
| `test_math.c` | B+ | **A+** | 16 min | 15 |
| `test_math_avx2.c` | A | **A+** | 40 min | 16,642 |
| `test_mersenne.c` | A+ | **A+** | — | — |
| `test_matrix.c` | A- | **A+** | 17 min | 25 |
| `test_matrix_avx2.c` | A | **A+** | 25 min | 27,648 |
| `test_omega.c` | A+ | **A+** | — | — |
| `test_sbox.c` | A- | **A+** | 22 min | 7 |
| `test_sbox_avx2.c` | A+ | **A+** | — | — |

| Metric | Current | After |
|--------|---------|-------|
| **Total effort** | — | **~2.5 hours** |
| **New assertions** | — | **~44,355** |
| **Total assertions** | ~4,043,660 | **~4,088,015** |
| **Files at A+** | 3/10 | **10/10** |
| **Lowest grade** | B+ | **A+** |

---

## The Two-Hour Path to Perfection

**Priority order** (maximum grade impact per minute):

1. **`test_math_avx2.c`** (40 min): The D in v2.5.0 coverage is the single biggest gap. Adding Mersenne parity tests is the most important missing validation in the entire suite.

2. **`test_matrix_avx2.c`** (25 min): Validates the Mersenne fast path through the full matrix multiplication, closing the integration testing gap.

3. **`test_cipher.c` + `test_cipher_pipeline4.c`** (35 min): Closes the Z-Domain Shifting gap, the other headline v2.5.0 feature.

4. **`test_sbox.c`** (22 min): Edge case hardening for cycle-walking and zero keys.

5. **`test_math.c` + `test_matrix.c`** (33 min): Minor additions for v2.5.0 awareness and edge case completion.

**After 2.5 hours: every file at A+, total assertion count exceeds 4 million, and the entire v2.5.0 feature set is comprehensively validated.**


A	3	test_sbox_avx2.py (version strings), fp2.py (sqrt edge case), cagoule/cipher.c (Z-Domain doc)
A-	1	test_cipher.c
