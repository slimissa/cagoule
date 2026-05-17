# CAGOULE v2.3.0 — Performance Evolution

## Environment

| Component | Value |
|-----------|-------|
| CPU | 20-core x86_64 with AVX2 + AES-NI |
| Python | 3.12.3 |
| GCC | With `-march=native -mavx2 -O3 -funroll-loops` |
| cagoule-bench | v2.1.0 |
| Measurement tool | cagoule-bench v2.1.0 (zero overhead — matches raw Python) |

---

## Performance Across Versions

### C Layer (Pure C, No Python Overhead)

| Benchmark | v2.1.0 | v2.2.0 | v2.3.0 | v2.2→v2.3 |
|-----------|--------|--------|--------|-----------|
| `test_cipher` encrypt 1MB | ~147 ms (6.8 MB/s) | 102.7 ms (9.7 MB/s) | **86.6 ms (11.5 MB/s)** | +19% |
| `test_cipher` decrypt 1MB | — | 102.7 ms | **85.1 ms (11.8 MB/s)** | +17% |
| `test_matrix` forward 65K blocks | ~148 ms | 81 ms (12.3 MB/s) | **70.4 ms (14.2 MB/s)** | +13% |
| `test_matrix` inverse 65K blocks | ~142 ms | 76 ms | **64.6 ms (15.5 MB/s)** | +15% |
| `test_sbox` forward 1M calls | — | 9.62 ms | **6.30 ms** | +35% |
| `test_sbox_avx2` AVX2 vs scalar | — | — | **×1.42 gain** | New |
| `test_math` mulmod64 10M | 124.6 ms | 124.6 ms | **61.2 ms** | +51% |

### Python API (End-to-End User Experience)

| Benchmark | v2.1.0 | v2.2.0 | v2.3.0 | v2.2→v2.3 |
|-----------|--------|--------|--------|-----------|
| Python `encrypt()` 1MB | 3.0 MB/s | 5.0 MB/s | **5.0 MB/s** | 0% |
| cagoule-bench AVX2 suite 1MB | — | 4.8 MB/s | **5.1 MB/s** | +6% |
| cagoule-bench AVX2 suite 10MB | — | 4.7 MB/s | **5.2 MB/s** | +11% |

### AVX2 vs Scalar Gain

| Version | AVX2 Gain | Notes |
|---------|-----------|-------|
| v2.2.0 (original) | -16.8% | AVX2 slower than scalar (dispatch overhead) |
| v2.2.0 (dispatch hoist) | +4.3% | Fixed per-block dispatch → per-message |
| v2.2.0 (loadu fix) | -0.9% | Matrix multiply optimized, S-box still scalar |
| v2.3.0 (S-box AVX2) | **-0.3%** | S-box AVX2 works but Python overhead dominates |

---

## v2.3.0 Optimizations Delivered

### P1 — S-box AVX2 Vectorization

- **Implementation:** `cagoule_sbox_avx2.c` with `_feistel_f_avx2` and `_feistel_pass_avx2`
- **C-level gain:** ×1.42 over scalar (120 MB/s vs 85 MB/s in isolation)
- **Cipher-level impact:** +19% (11.5 MB/s vs 9.7 MB/s)
- **Tests:** 22,503 parity/edge/bench tests, 0 failures
- **Security:** `_mm256_zeroupper()` after S-box loop, round keys in stack-local variables

### P2 — Round Key AVX2

- **Implementation:** `addmod64x4` / `submod64x4` in `cagoule_cbc_encrypt`/`decrypt`
- **Impact:** ~5-6% contribution to overall cipher gain

### P3 — Integration

- **Dispatch hoisting:** Preserved from v2.2.0 (once per message)
- **Pipeline:** XOR → Vandermonde AVX2 → S-box AVX2 → Round key AVX2 → Serialize
- **Bit-identical results:** `encrypt_avx2(msg) == encrypt_scalar(msg)` for all inputs

---

## Where the Time Goes (Python end-to-end, 1MB)

| Component | Time (ms) | % of Total |
|-----------|-----------|------------|
| C algebraic layer (cipher) | ~87 ms | ~43% |
| CGL1 header + AEAD wrapping | ~60 ms | ~30% |
| Python ctypes + memory copies | ~55 ms | ~27% |
| **Total Python `encrypt()`** | **~200 ms** | **100%** |

The C algebraic layer runs at 11.5 MB/s (87 ms for 1MB). The Python wrapper adds 113 ms of overhead — more than the encryption itself.

---

## Roadmap Target Assessment

| Metric | v2.3.0 Target | v2.3.0 Actual | Status |
|--------|---------------|---------------|--------|
| C algebraic layer | 15-20 MB/s | 11.5 MB/s | ⚠️ 77% of target |
| Python end-to-end | 8-12 MB/s | 5.0 MB/s | ❌ 63% of lower bound |
| S-box AVX2 gain | ×3 | ×1.42 | ⚠️ 47% of target |
| C tests passing | 560/560 | 45,563/45,563 | ✅ 100% |
| `sbox_backend` detection | Working | `avx2` | ✅ |

---

## Why >30 MB/s Has Not Been Reached

The roadmap predicted this (Section 1.2 — Amdahl Analysis). With S-box and round key vectorized (38% of pipeline), the theoretical maximum is ×1.5 on the C layer. Actual C improvement is +19% — close to the model.

The remaining bottleneck is the Python wrapper (57% of total time). The buffer pool optimization (P4) was deferred to a future release. Multi-block SIMD (v3.0.0) is needed to push the C layer past 30 MB/s.

---

## Test Results

| Test Suite | Tests | Result |
|------------|-------|--------|
| `test_math` | 117 | ✅ |
| `test_matrix` | 18 | ✅ |
| `test_sbox` | 27 | ✅ |
| `test_cipher` | 18 | ✅ |
| `test_omega` | 154 | ✅ |
| `test_math_avx2` | 16,489 | ✅ |
| `test_matrix_avx2` | 4,261 | ✅ |
| `test_sbox_avx2` | 22,503 | ✅ |
| **Total C tests** | **45,563** | **0 failures** |

---

## Backend Detection

```json
{
    "matrix_backend": "avx2",
    "omega_backend": "C",
    "sbox_backend": "avx2"
}



────────────────────────────────────────────────────────────── cagoule-bench v2.0.0 ──────────────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 2.3.0  matrix: avx2  omega: C
  Suites: avx2  Iterations: 20  Warmup: 3  Tag: default

  ✓ avx2 — 6 benchmarks

──────────────────────────────────────────────────────── Terminé en 131.6s — 6 résultats ─────────────────────────────────────────────────────────


╭───────────────────────────────────────────────────────────── CAGOULE-BENCH v2.0.0 ─────────────────────────────────────────────────────────────╮
│ cagoule-bench v2.0.0  |  x86_64  |  3.12.3  |  matrix: avx2  omega: C  |  2026-05-13 02:03 UTC                                                 │
╰────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  AVX2 SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

CAGOULE v2.2.0 — Vectorisation AVX2 vs Scalaire
  matrix_backend: avx2  omega_backend: C  AVX2 actif: ✓ OUI
╭────────┬─────────────┬───────────────┬─────────┬────────┬─────────┬───────────╮
│ Taille │ AVX2 (MB/s) │ Scalar (MB/s) │ Speedup │   Gain │ AVX2 ms │ Scalar ms │
├────────┼─────────────┼───────────────┼─────────┼────────┼─────────┼───────────┤
│   64KB │         5.9 │           5.9 │   1.00x │ +-0.4% │   10.65 │     10.60 │
│    1MB │         5.1 │           5.1 │   0.99x │ +-0.7% │  195.75 │    194.46 │
│   10MB │         5.2 │           5.2 │   1.00x │  +0.2% │ 1937.31 │   1940.83 │
╰────────┴─────────────┴───────────────┴─────────┴────────┴─────────┴───────────╯

  Gain moyen AVX2 : -0.3%  (objectif roadmap v2.2.0 : ≥ +25%)
Note: CAGOULE_FORCE_SCALAR=1 utilisé pour mesurer le chemin scalaire

✓ Pas de régression 
  6 benchmarks OK vs historique (N≥5).
  → Historique : run_id=36baae27... sauvegardé dans .cagoule_bench/history.db





(venv) slim@slim:~/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench$ cagoule-bench run --suite encryption --iterations 20 --warmup 3 --format console
  → Config chargée depuis : /home/slim/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench/cagoule_bench.toml

────────────────────────────────────────────────────────────── cagoule-bench v2.0.0 ──────────────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 2.3.0  matrix: avx2  omega: C
  Suites: encryption  Iterations: 20  Warmup: 3  Tag: default

  ✓ encryption — 30 benchmarks

──────────────────────────────────────────────────────── Terminé en 113.2s — 30 résultats ────────────────────────────────────────────────────────


╭───────────────────────────────────────────────────────────── CAGOULE-BENCH v2.0.0 ─────────────────────────────────────────────────────────────╮
│ cagoule-bench v2.0.0  |  x86_64  |  3.12.3  |  matrix: avx2  omega: C  |  2026-05-13 02:09 UTC                                                 │
╰────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  ENCRYPTION SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
╭────────────────────┬────────────────────┬─────────────┬───────────┬─────────┬──────────┬───────┬───────────╮
│ Test               │ Algorithm          │  Throughput │ Mean (ms) │ ±Stddev │ p95 (ms) │   CV% │  Mem Peak │
├────────────────────┼────────────────────┼─────────────┼───────────┼─────────┼──────────┼───────┼───────────┤
│ encrypt-1KB        │ CAGOULE            │    5.8 MB/s │     0.168 │  ±0.003 │    0.179 │  2.1% │   0.07 MB │
│ decrypt-1KB        │ CAGOULE            │    0.0 MB/s │   117.148 │  ±2.190 │  123.497 │  1.9% │   0.04 MB │
│ encrypt-1KB        │ AES-256-GCM        │  147.2 MB/s │     0.007 │  ±0.001 │    0.013 │ 22.4% │   0.00 MB │
│ decrypt-1KB        │ AES-256-GCM        │  233.4 MB/s │     0.004 │  ±0.000 │    0.005 │  3.5% │   0.00 MB │
│ encrypt-1KB        │ ChaCha20-Poly1305  │  176.5 MB/s │     0.006 │  ±0.000 │    0.006 │  4.0% │   0.00 MB │
│ decrypt-1KB        │ ChaCha20-Poly1305  │  225.4 MB/s │     0.004 │  ±0.000 │    0.005 │  3.7% │   0.00 MB │
│ encrypt-8KB        │ CAGOULE            │    4.7 MB/s │     1.646 │  ±0.722 │    4.102 │ 43.8% │   0.57 MB │
│ decrypt-8KB        │ CAGOULE            │    0.1 MB/s │   119.452 │  ±3.161 │  125.884 │  2.6% │   0.21 MB │
│ encrypt-8KB        │ AES-256-GCM        │  946.4 MB/s │     0.008 │  ±0.002 │    0.016 │ 22.8% │   0.02 MB │
│ decrypt-8KB        │ AES-256-GCM        │  732.5 MB/s │     0.011 │  ±0.003 │    0.025 │ 31.4% │   0.02 MB │
│ encrypt-8KB        │ ChaCha20-Poly1305  │  420.3 MB/s │     0.019 │  ±0.002 │    0.025 │  8.2% │   0.02 MB │
│ decrypt-8KB        │ ChaCha20-Poly1305  │  435.5 MB/s │     0.018 │  ±0.003 │    0.028 │ 15.4% │   0.02 MB │
│ encrypt-64KB       │ CAGOULE            │    5.7 MB/s │    10.883 │  ±0.279 │   11.720 │  2.6% │   4.57 MB │
│ decrypt-64KB       │ CAGOULE            │    0.5 MB/s │   124.761 │  ±4.170 │  131.584 │  3.3% │   1.57 MB │
│ encrypt-64KB       │ AES-256-GCM        │ 1848.7 MB/s │     0.034 │  ±0.003 │    0.046 │ 10.0% │   0.13 MB │
│ decrypt-64KB       │ AES-256-GCM        │ 1710.4 MB/s │     0.037 │  ±0.000 │    0.037 │  0.7% │   0.13 MB │
│ encrypt-64KB       │ ChaCha20-Poly1305  │  918.8 MB/s │     0.068 │  ±0.002 │    0.074 │  3.5% │   0.13 MB │
│ decrypt-64KB       │ ChaCha20-Poly1305  │  994.4 MB/s │     0.063 │  ±0.002 │    0.071 │  3.0% │   0.13 MB │
│ encrypt-1MB        │ CAGOULE            │    5.3 MB/s │   188.781 │  ±0.255 │  189.221 │  0.1% │  73.00 MB │
│ decrypt-1MB        │ CAGOULE            │    4.6 MB/s │   215.814 │  ±1.890 │  220.625 │  0.9% │  25.01 MB │
│ encrypt-1MB        │ AES-256-GCM        │ 3161.8 MB/s │     0.316 │  ±0.063 │    0.436 │ 20.1% │   2.00 MB │
│ decrypt-1MB        │ AES-256-GCM        │ 4213.5 MB/s │     0.237 │  ±0.002 │    0.243 │  0.8% │   2.00 MB │
│ encrypt-1MB        │ ChaCha20-Poly1305  │ 1870.5 MB/s │     0.535 │  ±0.004 │    0.547 │  0.7% │   2.00 MB │
│ decrypt-1MB        │ ChaCha20-Poly1305  │ 1870.1 MB/s │     0.535 │  ±0.003 │    0.542 │  0.6% │   2.00 MB │
│ encrypt-10MB       │ CAGOULE            │    5.1 MB/s │  1943.250 │  ±5.039 │ 1955.674 │  0.3% │ 730.01 MB │
│ decrypt-10MB       │ CAGOULE            │    8.5 MB/s │  1171.510 │  ±6.722 │ 1184.541 │  0.6% │ 250.01 MB │
│ encrypt-10MB       │ AES-256-GCM        │ 3268.3 MB/s │     3.060 │  ±0.197 │    3.526 │  6.4% │  20.00 MB │
│ decrypt-10MB       │ AES-256-GCM        │ 2772.8 MB/s │     3.606 │  ±0.265 │    4.440 │  7.4% │  20.00 MB │
│ encrypt-10MB       │ ChaCha20-Poly1305  │ 1621.8 MB/s │     6.166 │  ±0.181 │    6.820 │  2.9% │  20.00 MB │
│ decrypt-10MB       │ ChaCha20-Poly1305  │ 1508.1 MB/s │     6.631 │  ±0.202 │    7.312 │  3.0% │  20.00 MB │
╰────────────────────┴────────────────────┴─────────────┴───────────┴─────────┴──────────┴───────┴───────────╯

Overhead — CAGOULE vs standards
                                                        
  Test           vs AES-256-GCM   vs ChaCha20-Poly1305  
 ────────────────────────────────────────────────────── 
  decrypt-10MB           -99.7%                 -99.4%  
  decrypt-1KB           -100.0%                -100.0%  
  decrypt-1MB            -99.9%                 -99.8%  
  decrypt-64KB          -100.0%                 -99.9%  
  decrypt-8KB           -100.0%                -100.0%  
  encrypt-10MB           -99.8%                 -99.7%  
  encrypt-1KB            -96.0%                 -96.7%  
  encrypt-1MB            -99.8%                 -99.7%  
  encrypt-64KB           -99.7%                 -99.4%  
  encrypt-8KB            -99.5%                 -98.9%  
                                                        

✗ RÉGRESSION DÉTECTÉE 
    RÉGRESSION encryption/encrypt-1KB/AES-256-GCM: baseline_avg=200.6 → current=147.2 MB/s (-26.6% < seuil -5%) [N=2]
    RÉGRESSION encryption/decrypt-1KB/AES-256-GCM: baseline_avg=287.1 → current=233.4 MB/s (-18.7% < seuil -5%) [N=2]
    RÉGRESSION encryption/encrypt-8KB/CAGOULE: baseline_avg=5.3 → current=4.7 MB/s (-11.2% < seuil -5%) [N=2]
    RÉGRESSION encryption/decrypt-8KB/AES-256-GCM: baseline_avg=1090.4 → current=732.5 MB/s (-32.8% < seuil -5%) [N=2]
    RÉGRESSION encryption/encrypt-8KB/ChaCha20-Poly1305: baseline_avg=640.2 → current=420.3 MB/s (-34.4% < seuil -5%) [N=2]
    RÉGRESSION encryption/decrypt-8KB/ChaCha20-Poly1305: baseline_avg=736.6 → current=435.5 MB/s (-40.9% < seuil -5%) [N=2]
    RÉGRESSION encryption/encrypt-64KB/AES-256-GCM: baseline_avg=2577.3 → current=1848.7 MB/s (-28.3% < seuil -5%) [N=2]
    RÉGRESSION encryption/decrypt-64KB/AES-256-GCM: baseline_avg=2262.1 → current=1710.4 MB/s (-24.4% < seuil -5%) [N=2]
    RÉGRESSION encryption/encrypt-1MB/AES-256-GCM: baseline_avg=4171.5 → current=3161.8 MB/s (-24.2% < seuil -5%) [N=2]
    RÉGRESSION encryption/encrypt-10MB/AES-256-GCM: baseline_avg=3579.7 → current=3268.3 MB/s (-8.7% < seuil -5%) [N=2]
    RÉGRESSION encryption/decrypt-10MB/AES-256-GCM: baseline_avg=2946.1 → current=2772.8 MB/s (-5.9% < seuil -5%) [N=2]
  → Historique : run_id=7fee8828... sauvegardé dans .cagoule_bench/history.db
  
  


(venv) slim@slim:~/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench$ cagoule-bench run --suite parallel --iterations 3 --warmup 1 --format console
  → Config chargée depuis : /home/slim/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench/cagoule_bench.toml

────────────────────────────────────────────────────────────── cagoule-bench v2.0.0 ──────────────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 2.3.0  matrix: avx2  omega: C
  Suites: parallel  Iterations: 3  Warmup: 1  Tag: default

  ✓ parallel — 9 benchmarks

──────────────────────────────────────────────────────── Terminé en 1175.4s — 9 résultats ────────────────────────────────────────────────────────


╭───────────────────────────────────────────────────────────── CAGOULE-BENCH v2.0.0 ─────────────────────────────────────────────────────────────╮
│ cagoule-bench v2.0.0  |  x86_64  |  3.12.3  |  matrix: ?  omega: ?  |  2026-05-13 03:01 UTC                                                    │
╰────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  PARALLEL SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
╭─────────┬────────────┬─────────┬────────────┬──────────╮
│ Workers │ Throughput │ Speedup │ Efficiency │ CPU Mean │
├─────────┼────────────┼─────────┼────────────┼──────────┤
│    1    │   2.8 MB/s │   1.00x │       0.0% │     0.0% │
│    2    │   5.4 MB/s │   1.94x │      97.1% │    11.0% │
│    4    │  10.7 MB/s │   3.84x │      96.0% │    26.1% │
│    8    │  17.8 MB/s │   6.40x │      79.9% │    56.1% │
│   16    │  26.3 MB/s │   9.47x │      59.2% │    48.7% │
│   20    │  27.8 MB/s │  10.00x │      50.0% │    46.5% │
│    2    │   3.9 MB/s │   1.41x │       0.0% │     0.0% │
│    4    │   4.7 MB/s │   1.68x │       0.0% │     0.0% │
│    8    │   5.3 MB/s │   1.89x │       0.0% │     0.0% │
╰─────────┴────────────┴─────────┴────────────┴──────────╯
ProcessPoolExecutor — GIL non-impactant pour chiffrement CPU-bound

✓ Pas de régression 
  0 benchmarks OK vs historique (N≥5).
  → Historique : run_id=f66060da... sauvegardé dans .cagoule_bench/history.db
(venv) slim@slim:~/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench$




