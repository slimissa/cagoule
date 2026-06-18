(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$ cagoule-bench run --suite ctr --iterations 10 --warmup 1 --format console
  → Config chargée depuis : /home/slim/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2/cagoule_bench.toml

────────────────────────────────────────────────────────────── cagoule-bench v2.0.0 ──────────────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 3.0.0  matrix: avx2  omega: C
  Suites: ctr  Iterations: 10  Warmup: 1  Tag: default

  ✓ ctr — 41 benchmarks

──────────────────────────────────────────────────────── Terminé en 718.3s — 41 résultats ────────────────────────────────────────────────────────


╭───────────────────────────────────────────────────────────── CAGOULE-BENCH v2.0.0 ─────────────────────────────────────────────────────────────╮
│ cagoule-bench v2.0.0  |  x86_64  |  3.12.3  |  matrix: ?  omega: ?  |  2026-06-14 15:47 UTC                                                    │
╰────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  CTR SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
╭────────────────────────┬──────────────────────────┬───────────┬────────────╮
│ Name                   │ Algorithm                │ Mean (ms) │ Throughput │
├────────────────────────┼──────────────────────────┼───────────┼────────────┤
│ ctr-encrypt-1KB        │ CAGOULE-CTR              │     0.067 │  14.6 MB/s │
│ cbc-encrypt-1KB        │ CAGOULE-CBC              │     0.319 │   3.1 MB/s │
│ ctr-decrypt-1KB        │ CAGOULE-CTR              │     0.185 │   5.3 MB/s │
│ cbc-decrypt-1KB        │ CAGOULE-CBC              │   118.634 │   0.0 MB/s │
│ ctr-encrypt-8KB        │ CAGOULE-CTR              │     0.373 │  20.9 MB/s │
│ cbc-encrypt-8KB        │ CAGOULE-CBC              │     1.000 │   7.8 MB/s │
│ ctr-decrypt-8KB        │ CAGOULE-CTR              │     0.376 │  20.8 MB/s │
│ cbc-decrypt-8KB        │ CAGOULE-CBC              │   119.467 │   0.1 MB/s │
│ ctr-encrypt-64KB       │ CAGOULE-CTR              │     2.800 │  22.3 MB/s │
│ cbc-encrypt-64KB       │ CAGOULE-CBC              │     7.725 │   8.1 MB/s │
│ ctr-decrypt-64KB       │ CAGOULE-CTR              │     2.797 │  22.3 MB/s │
│ cbc-decrypt-64KB       │ CAGOULE-CBC              │   119.385 │   0.5 MB/s │
│ ctr-encrypt-1MB        │ CAGOULE-CTR              │    45.378 │  22.0 MB/s │
│ cbc-encrypt-1MB        │ CAGOULE-CBC              │   145.054 │   6.9 MB/s │
│ ctr-decrypt-1MB        │ CAGOULE-CTR              │    44.825 │  22.3 MB/s │
│ cbc-decrypt-1MB        │ CAGOULE-CBC              │   166.668 │   6.0 MB/s │
│ ctr-encrypt-10MB       │ CAGOULE-CTR              │   469.742 │  21.3 MB/s │
│ cbc-encrypt-10MB       │ CAGOULE-CBC              │  1485.971 │   6.7 MB/s │
│ ctr-decrypt-10MB       │ CAGOULE-CTR              │   469.705 │  21.3 MB/s │
│ cbc-decrypt-10MB       │ CAGOULE-CBC              │   690.249 │  14.5 MB/s │
│ ctr-auto-128B          │ CAGOULE-CTR-auto         │     0.032 │   3.8 MB/s │
│ ctr-auto-4KB           │ CAGOULE-CTR-auto         │     0.490 │   8.0 MB/s │
│ ctr-auto-64KB          │ CAGOULE-CTR-auto         │     2.803 │  22.3 MB/s │
│ ctr-auto-1MB           │ CAGOULE-CTR-auto         │    45.444 │  22.0 MB/s │
│ ctr-sym-encrypt-64KB   │ CAGOULE-CTR-symmetry-enc │     2.802 │  22.3 MB/s │
│ ctr-sym-decrypt-64KB   │ CAGOULE-CTR-symmetry-dec │     2.803 │  22.3 MB/s │
│ ctr-sym-encrypt-1MB    │ CAGOULE-CTR-symmetry-enc │    45.250 │  22.1 MB/s │
│ ctr-sym-decrypt-1MB    │ CAGOULE-CTR-symmetry-dec │    44.976 │  22.2 MB/s │
│ migrate-cbc-ctr-1KB    │ CAGOULE-migrate          │   237.480 │   0.0 MB/s │
│ migrate-cbc-ctr-64KB   │ CAGOULE-migrate          │   249.136 │   0.3 MB/s │
│ migrate-cbc-ctr-1MB    │ CAGOULE-migrate          │   353.073 │   2.8 MB/s │
│ bulk-ctr-1msgs         │ CAGOULE-bulk-CTR         │   118.074 │   0.5 MB/s │
│ individual-ctr-1msgs   │ CAGOULE-individual-CTR   │   122.170 │   0.5 MB/s │
│ bulk-ctr-5msgs         │ CAGOULE-bulk-CTR         │   596.575 │   0.5 MB/s │
│ individual-ctr-5msgs   │ CAGOULE-individual-CTR   │   587.676 │   0.5 MB/s │
│ bulk-ctr-10msgs        │ CAGOULE-bulk-CTR         │  1190.376 │   0.5 MB/s │
│ individual-ctr-10msgs  │ CAGOULE-individual-CTR   │  1204.107 │   0.5 MB/s │
│ bulk-ctr-50msgs        │ CAGOULE-bulk-CTR         │  5969.964 │   0.5 MB/s │
│ individual-ctr-50msgs  │ CAGOULE-individual-CTR   │  6042.054 │   0.5 MB/s │
│ bulk-ctr-100msgs       │ CAGOULE-bulk-CTR         │ 11835.390 │   0.5 MB/s │
│ individual-ctr-100msgs │ CAGOULE-individual-CTR   │ 11852.289 │   0.5 MB/s │
╰────────────────────────┴──────────────────────────┴───────────┴────────────╯

✓ Pas de régression 
  0 benchmarks OK vs historique (N≥5).
  → Historique : run_id=8c799d9c... sauvegardé dans .cagoule_bench/history.db
(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$


(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$ cagoule-bench run --suite ctr --iterations 10 --warmup 2 --format console
  → Config chargée depuis : 
/home/slim/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2/cagoule_bench.toml

────────────────────────────────────────────────────── cagoule-bench v2.0.0 ───────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 3.0.0  matrix: avx2  omega: C
  Suites: ctr  Iterations: 10  Warmup: 2  Tag: default

  ✓ ctr — 41 benchmarks

──────────────────────────────────────────────── Terminé en 433.0s — 41 résultats ─────────────────────────────────────────────────


╭───────────────────────────────────────────────────── CAGOULE-BENCH v2.2.0 ──────────────────────────────────────────────────────╮
│ cagoule-bench v2.2.0  |  x86_64  |  3.12.3  |  matrix: ?  omega: ?  |  2026-06-14 16:21 UTC                                     │
╰─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  CTR SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

CTR vs CBC — Throughput Comparison
╭──────┬────────────┬────────────┬─────────┬────────╮
│ Size │ CTR (MB/s) │ CBC (MB/s) │ Speedup │ CTR ms │
├──────┼────────────┼────────────┼─────────┼────────┤
│ 10MB │       21.3 │        6.8 │    ×3.1 │ 469.46 │
│ 1KB  │       14.4 │        3.8 │    ×3.8 │   0.07 │
│ 1MB  │       22.2 │        7.0 │    ×3.2 │  45.09 │
│ 64KB │       22.3 │        8.0 │    ×2.8 │   2.80 │
│ 8KB  │       18.9 │        7.6 │    ×2.5 │   0.41 │
╰──────┴────────────┴────────────┴─────────┴────────╯
CTR target: >15 MB/s Python e2e

CTR Pipeline 4x — Auto-dispatch
                                    
  Size      Throughput   Mean (ms)  
 ────────────────────────────────── 
  128B 4x     3.1 MB/s        0.04  
  4KB 4x     20.1 MB/s        0.19  
  64KB 4x    22.5 MB/s        2.78  
  1MB 4x     22.3 MB/s       44.93  
                                    
4x pipeline activates for messages ≥ 128 bytes (8 blocks)

CTR Symmetry — encrypt = decrypt
                                                  
  Size   Encrypt (MB/s)   Decrypt (MB/s)   Ratio  
 ──────────────────────────────────────────────── 
  64KB             22.1             22.1   1.00×  
  1MB              22.3             22.3   1.00×  
                                                  

CBC → CTR Migration Cost
                                 
  Size   Time (ms)   Throughput  
 ─────────────────────────────── 
  1KB        229.6     0.0 MB/s  
  64KB       237.7     0.3 MB/s  
  1MB        350.1     2.9 MB/s  
                                 

Bulk CTR — KDF Amortization
╭──────────┬───────────┬─────────────────┬───────┬───────────╮
│ Messages │ Bulk (ms) │ Individual (ms) │  Gain │ KDF calls │
├──────────┼───────────┼─────────────────┼───────┼───────────┤
│        1 │     117.4 │             2.8 │ 0.02× │  1 vs 1   │
│        5 │     596.3 │            14.1 │ 0.02× │  1 vs 5   │
│       10 │    1206.2 │            28.3 │ 0.02× │  1 vs 10  │
│       50 │    5952.1 │           140.5 │ 0.02× │  1 vs 50  │
│      100 │   11843.6 │           280.7 │ 0.02× │ 1 vs 100  │
╰──────────┴───────────┴─────────────────┴───────┴───────────╯
Bulk: 1 Argon2id derivation shared across N messages

✓ Pas de régression 
  0 benchmarks OK vs historique (N≥5).
  → Historique : run_id=1c6fa069... sauvegardé dans .cagoule_bench/history.db
(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$ 

(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$ cagoule-bench run --suite encryption --iterations 10 --warmup 2 --format console
  → Config chargée depuis : 
/home/slim/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2/cagoule_bench.toml

────────────────────────────────────────────────────── cagoule-bench v2.0.0 ───────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 3.0.0  matrix: avx2  omega: C
  Suites: encryption  Iterations: 10  Warmup: 2  Tag: default

  ✓ encryption — 40 benchmarks

───────────────────────────────────────────────── Terminé en 70.0s — 40 résultats ─────────────────────────────────────────────────


╭───────────────────────────────────────────────────── CAGOULE-BENCH v2.2.0 ──────────────────────────────────────────────────────╮
│ cagoule-bench v2.2.0  |  x86_64  |  3.12.3  |  matrix: avx2  omega: C  |  2026-06-14 16:24 UTC                                  │
╰─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  ENCRYPTION SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
╭────────────────────┬────────────────────┬─────────────┬───────────┬─────────┬──────────┬───────┬───────────╮
│ Test               │ Algorithm          │  Throughput │ Mean (ms) │ ±Stddev │ p95 (ms) │   CV% │  Mem Peak │
├────────────────────┼────────────────────┼─────────────┼───────────┼─────────┼──────────┼───────┼───────────┤
│ encrypt-1KB        │ CAGOULE-CBC        │    6.8 MB/s │     0.143 │  ±0.004 │    0.148 │  2.5% │   0.07 MB │
│ decrypt-1KB        │ CAGOULE-CBC        │    0.0 MB/s │   116.970 │  ±3.324 │  124.968 │  2.8% │   0.04 MB │
│ encrypt-1KB        │ CAGOULE-CTR        │    7.8 MB/s │     0.125 │  ±0.003 │    0.132 │  2.3% │   0.01 MB │
│ decrypt-1KB        │ CAGOULE-CTR        │    6.1 MB/s │     0.161 │  ±0.008 │    0.177 │  5.0% │   0.01 MB │
│ encrypt-1KB        │ AES-256-GCM        │  134.9 MB/s │     0.007 │  ±0.004 │    0.019 │ 59.4% │   0.00 MB │
│ decrypt-1KB        │ AES-256-GCM        │  235.3 MB/s │     0.004 │  ±0.000 │    0.005 │  5.8% │   0.00 MB │
│ encrypt-1KB        │ ChaCha20-Poly1305  │  133.1 MB/s │     0.007 │  ±0.000 │    0.008 │  3.8% │   0.00 MB │
│ decrypt-1KB        │ ChaCha20-Poly1305  │  161.2 MB/s │     0.006 │  ±0.000 │    0.007 │  5.0% │   0.00 MB │
│ encrypt-8KB        │ CAGOULE-CBC        │    8.1 MB/s │     0.969 │  ±0.002 │    0.975 │  0.2% │   0.57 MB │
│ decrypt-8KB        │ CAGOULE-CBC        │    0.1 MB/s │   114.386 │  ±0.258 │  115.018 │  0.2% │   0.21 MB │
│ encrypt-8KB        │ CAGOULE-CTR        │   13.0 MB/s │     0.603 │  ±0.090 │    0.728 │ 15.0% │   0.07 MB │
│ decrypt-8KB        │ CAGOULE-CTR        │   12.1 MB/s │     0.646 │  ±0.031 │    0.703 │  4.8% │   0.09 MB │
│ encrypt-8KB        │ AES-256-GCM        │  932.2 MB/s │     0.008 │  ±0.000 │    0.009 │  4.5% │   0.02 MB │
│ decrypt-8KB        │ AES-256-GCM        │  874.5 MB/s │     0.009 │  ±0.000 │    0.010 │  3.6% │   0.02 MB │
│ encrypt-8KB        │ ChaCha20-Poly1305  │  543.7 MB/s │     0.014 │  ±0.002 │    0.020 │ 14.1% │   0.02 MB │
│ decrypt-8KB        │ ChaCha20-Poly1305  │  636.2 MB/s │     0.012 │  ±0.000 │    0.013 │  2.0% │   0.02 MB │
│ encrypt-64KB       │ CAGOULE-CBC        │    8.0 MB/s │     7.776 │  ±0.019 │    7.799 │  0.2% │   4.56 MB │
│ decrypt-64KB       │ CAGOULE-CBC        │    0.5 MB/s │   117.604 │  ±0.475 │  118.556 │  0.4% │   1.57 MB │
│ encrypt-64KB       │ CAGOULE-CTR        │   22.4 MB/s │     2.794 │  ±0.007 │    2.805 │  0.3% │   0.57 MB │
│ decrypt-64KB       │ CAGOULE-CTR        │   22.4 MB/s │     2.792 │  ±0.020 │    2.840 │  0.7% │   0.69 MB │
│ encrypt-64KB       │ AES-256-GCM        │ 1427.4 MB/s │     0.044 │  ±0.002 │    0.049 │  5.0% │   0.13 MB │
│ decrypt-64KB       │ AES-256-GCM        │ 1794.9 MB/s │     0.035 │  ±0.002 │    0.041 │  6.2% │   0.13 MB │
│ encrypt-64KB       │ ChaCha20-Poly1305  │  936.5 MB/s │     0.067 │  ±0.002 │    0.072 │  2.5% │   0.13 MB │
│ decrypt-64KB       │ ChaCha20-Poly1305  │ 1049.3 MB/s │     0.060 │  ±0.000 │    0.060 │  0.3% │   0.13 MB │
│ encrypt-1MB        │ CAGOULE-CBC        │    6.9 MB/s │   144.287 │  ±1.855 │  147.239 │  1.3% │  73.00 MB │
│ decrypt-1MB        │ CAGOULE-CBC        │    6.0 MB/s │   165.894 │  ±2.196 │  169.811 │  1.3% │  25.01 MB │
│ encrypt-1MB        │ CAGOULE-CTR        │   22.2 MB/s │    44.987 │  ±0.383 │   45.807 │  0.9% │   9.01 MB │
│ decrypt-1MB        │ CAGOULE-CTR        │   22.3 MB/s │    44.811 │  ±0.231 │   45.437 │  0.5% │  11.00 MB │
│ encrypt-1MB        │ AES-256-GCM        │ 4134.5 MB/s │     0.242 │  ±0.001 │    0.245 │  0.6% │   2.00 MB │
│ decrypt-1MB        │ AES-256-GCM        │ 4174.7 MB/s │     0.240 │  ±0.001 │    0.242 │  0.5% │   2.00 MB │
│ encrypt-1MB        │ ChaCha20-Poly1305  │ 1257.3 MB/s │     0.795 │  ±0.133 │    0.983 │ 16.7% │   2.00 MB │
│ decrypt-1MB        │ ChaCha20-Poly1305  │ 1859.5 MB/s │     0.538 │  ±0.003 │    0.545 │  0.5% │   2.00 MB │
│ encrypt-10MB       │ CAGOULE-CBC        │    6.7 MB/s │  1485.096 │  ±8.776 │ 1500.090 │  0.6% │ 730.01 MB │
│ decrypt-10MB       │ CAGOULE-CBC        │   14.5 MB/s │   689.647 │  ±2.085 │  692.398 │  0.3% │ 250.01 MB │
│ encrypt-10MB       │ CAGOULE-CTR        │   21.3 MB/s │   469.449 │  ±1.007 │  472.266 │  0.2% │  90.01 MB │
│ decrypt-10MB       │ CAGOULE-CTR        │   21.1 MB/s │   473.075 │  ±1.029 │  474.929 │  0.2% │ 110.01 MB │
│ encrypt-10MB       │ AES-256-GCM        │ 3340.0 MB/s │     2.994 │  ±0.033 │    3.047 │  1.1% │  20.00 MB │
│ decrypt-10MB       │ AES-256-GCM        │ 3017.9 MB/s │     3.314 │  ±0.051 │    3.371 │  1.6% │  20.00 MB │
│ encrypt-10MB       │ ChaCha20-Poly1305  │ 1542.6 MB/s │     6.482 │  ±0.375 │    7.259 │  5.8% │  20.00 MB │
│ decrypt-10MB       │ ChaCha20-Poly1305  │ 1546.6 MB/s │     6.466 │  ±0.260 │    7.189 │  4.0% │  20.00 MB │
╰────────────────────┴────────────────────┴─────────────┴───────────┴─────────┴──────────┴───────┴───────────╯

Overhead — CAGOULE vs standards
                                                        
  Test           vs AES-256-GCM   vs ChaCha20-Poly1305  
 ────────────────────────────────────────────────────── 
  decrypt-10MB           -99.5%                 -99.1%  
  decrypt-1KB           -100.0%                -100.0%  
  decrypt-1MB            -99.9%                 -99.7%  
  decrypt-64KB          -100.0%                 -99.9%  
  decrypt-8KB           -100.0%                -100.0%  
  encrypt-10MB           -99.8%                 -99.6%  
  encrypt-1KB            -94.9%                 -94.9%  
  encrypt-1MB            -99.8%                 -99.4%  
  encrypt-64KB           -99.4%                 -99.1%  
  encrypt-8KB            -99.1%                 -98.5%  
                                                        

✓ Pas de régression 
  0 benchmarks OK vs historique (N≥5).
  → Historique : run_id=8e635a0b... sauvegardé dans .cagoule_bench/history.db
(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$ 

(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$ cagoule-bench run --suite avx2 --iterations 20 --warmup 3 --format console
  → Config chargée depuis : 
/home/slim/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2/cagoule_bench.toml

────────────────────────────────────────────────────── cagoule-bench v2.0.0 ───────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 3.0.0  matrix: avx2  omega: C
  Suites: avx2  Iterations: 20  Warmup: 3  Tag: default

  ✓ avx2 — 6 benchmarks

───────────────────────────────────────────────── Terminé en 32.5s — 6 résultats ──────────────────────────────────────────────────


╭───────────────────────────────────────────────────── CAGOULE-BENCH v2.2.0 ──────────────────────────────────────────────────────╮
│ cagoule-bench v2.2.0  |  x86_64  |  3.12.3  |  matrix: avx2  omega: C  |  2026-06-14 16:27 UTC                                  │
╰─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  AVX2 SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

CAGOULE v2.2.0 — Vectorisation AVX2 vs Scalaire
  matrix_backend: avx2  omega_backend: C  AVX2 actif: ✓ OUI
╭────────┬─────────────┬───────────────┬─────────┬────────┬─────────┬───────────╮
│ Taille │ AVX2 (MB/s) │ Scalar (MB/s) │ Speedup │   Gain │ AVX2 ms │ Scalar ms │
├────────┼─────────────┼───────────────┼─────────┼────────┼─────────┼───────────┤
│   64KB │        22.3 │          22.7 │   0.98x │ +-2.1% │    2.81 │      2.75 │
│    1MB │        22.2 │          22.3 │   1.00x │ +-0.3% │   45.06 │     44.93 │
│   10MB │        20.9 │          21.5 │   0.97x │ +-2.8% │  478.18 │    465.20 │
╰────────┴─────────────┴───────────────┴─────────┴────────┴─────────┴───────────╯

  Gain moyen AVX2 : -1.7%  (objectif roadmap v2.2.0 : ≥ +25%)
Note: CAGOULE_FORCE_SCALAR=1 utilisé pour mesurer le chemin scalaire

✓ Pas de régression 
  0 benchmarks OK vs historique (N≥5).
  → Historique : run_id=70b3e010... sauvegardé dans .cagoule_bench/history.db
(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$ 

(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$ cagoule-bench run --suite streaming --iterations 1 --warmup 0 --format console
  → Config chargée depuis : 
/home/slim/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2/cagoule_bench.toml

────────────────────────────────────────────────────── cagoule-bench v2.0.0 ───────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 3.0.0  matrix: avx2  omega: C
  Suites: streaming  Iterations: 1  Warmup: 10  Tag: default

  ✓ streaming — 12 benchmarks

──────────────────────────────────────────────── Terminé en 254.7s — 12 résultats ─────────────────────────────────────────────────


╭───────────────────────────────────────────────────── CAGOULE-BENCH v2.2.0 ──────────────────────────────────────────────────────╮
│ cagoule-bench v2.2.0  |  x86_64  |  3.12.3  |  matrix: ?  omega: ?  |  2026-06-14 16:32 UTC                                     │
╰─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  STREAMING SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
╭────────────────────────┬───────────────────┬────────────┬───────────┬───────┬──────────╮
│ Test                   │ Algorithm         │ Throughput │ Mean (ms) │ Chunk │ RAM eff. │
├────────────────────────┼───────────────────┼────────────┼───────────┼───────┼──────────┤
│ stream-encrypt-50MB    │ AES-256-GCM       │ 459.6 MB/s │       109 │ 64 KB │ O(chunk) │
│ stream-encrypt-50MB    │ ChaCha20-Poly1305 │ 403.1 MB/s │       124 │ 64 KB │ O(chunk) │
│ stream-encrypt-50MB    │ CAGOULE-CBC       │  21.8 MB/s │      2294 │ 64 KB │ O(chunk) │
│ stream-encrypt-50MB    │ CAGOULE-CTR       │  21.9 MB/s │      2287 │ 64 KB │ O(chunk) │
│ stream-encrypt-100MB   │ AES-256-GCM       │ 459.0 MB/s │       218 │ 64 KB │ O(chunk) │
│ stream-encrypt-100MB   │ ChaCha20-Poly1305 │ 402.8 MB/s │       248 │ 64 KB │ O(chunk) │
│ stream-encrypt-100MB   │ CAGOULE-CBC       │  21.8 MB/s │      4593 │ 64 KB │ O(chunk) │
│ stream-encrypt-100MB   │ CAGOULE-CTR       │  21.7 MB/s │      4604 │ 64 KB │ O(chunk) │
│ stream-encrypt-500MB   │ AES-256-GCM       │ 457.3 MB/s │      1093 │ 64 KB │ O(chunk) │
│ stream-encrypt-500MB   │ ChaCha20-Poly1305 │ 396.4 MB/s │      1261 │ 64 KB │ O(chunk) │
│ stream-encrypt-500MB   │ CAGOULE-CBC       │  21.7 MB/s │     23044 │ 64 KB │ O(chunk) │
│ stream-encrypt-500MB   │ CAGOULE-CTR       │  21.7 MB/s │     23065 │ 64 KB │ O(chunk) │
╰────────────────────────┴───────────────────┴────────────┴───────────┴───────┴──────────╯
Streaming: lecture chunked → chiffrement → sortie — RAM = O(chunk) idéalement

✓ Pas de régression 
  0 benchmarks OK vs historique (N≥5).
  → Historique : run_id=fbd46da7... sauvegardé dans .cagoule_bench/history.db
(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$


(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0$ cd cagoule-bench-v2
(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$ git add bench/suites/ctr_suite.py
git commit -m "Fix bulk CTR: true KDF amortization vs individual full KDF"
git push origin main
cagoule-bench run --suite ctr --iterations 5 --warmup 2 --format console
[main 9ba0533] Fix bulk CTR: true KDF amortization vs individual full KDF
 1 file changed, 2 insertions(+), 2 deletions(-)
Énumération des objets: 9, fait.
Décompte des objets: 100% (9/9), fait.
Compression par delta en utilisant jusqu'à 20 fils d'exécution
Compression des objets: 100% (5/5), fait.
Écriture des objets: 100% (5/5), 502 octets | 502.00 KiO/s, fait.
Total 5 (delta 4), réutilisés 0 (delta 0), réutilisés du paquet 0 (depuis 0)
remote: Resolving deltas: 100% (4/4), completed with 4 local objects.
To https://github.com/slimissa/cagoule-bench-v2
   8b7132c..9ba0533  main -> main
  → Config chargée depuis : 
/home/slim/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2/cagoule_bench.toml

────────────────────────────────────────────────────── cagoule-bench v2.2.0 ───────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 3.0.0  matrix: avx2  omega: C
  Suites: ctr  Iterations: 5  Warmup: 2  Tag: default

  ✓ ctr — 41 benchmarks

──────────────────────────────────────────────── Terminé en 308.3s — 41 résultats ─────────────────────────────────────────────────


╭───────────────────────────────────────────────────── CAGOULE-BENCH v2.2.0 ──────────────────────────────────────────────────────╮
│ cagoule-bench v2.2.0  |  x86_64  |  3.12.3  |  matrix: ?  omega: ?  |  2026-06-15 21:54 UTC                                     │
╰─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  CTR SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

CTR vs CBC — Throughput Comparison
╭──────┬────────────┬────────────┬─────────┬────────╮
│ Size │ CTR (MB/s) │ CBC (MB/s) │ Speedup │ CTR ms │
├──────┼────────────┼────────────┼─────────┼────────┤
│ 10MB │       21.4 │        6.8 │    ×3.1 │ 468.38 │
│ 1KB  │       13.5 │        3.0 │    ×4.4 │   0.07 │
│ 1MB  │       22.1 │        7.0 │    ×3.1 │  45.21 │
│ 64KB │       22.3 │        8.1 │    ×2.7 │   2.80 │
│ 8KB  │       14.2 │        6.9 │    ×2.0 │   0.55 │
╰──────┴────────────┴────────────┴─────────┴────────╯
CTR target: >15 MB/s Python e2e

CTR Pipeline 4x — Auto-dispatch
                                    
  Size      Throughput   Mean (ms)  
 ────────────────────────────────── 
  128B 4x     2.4 MB/s        0.05  
  4KB 4x     11.1 MB/s        0.35  
  64KB 4x    22.4 MB/s        2.79  
  1MB 4x     21.9 MB/s       45.61  
                                    
4x pipeline activates for messages ≥ 128 bytes (8 blocks)

CTR Symmetry — encrypt = decrypt
                                                  
  Size   Encrypt (MB/s)   Decrypt (MB/s)   Ratio  
 ──────────────────────────────────────────────── 
  64KB             12.3             19.9   1.62×  
  1MB              21.2             22.0   1.04×  
                                                  

CBC → CTR Migration Cost
                                 
  Size   Time (ms)   Throughput  
 ─────────────────────────────── 
  1KB        230.1     0.0 MB/s  
  64KB       238.6     0.3 MB/s  
  1MB        347.7     2.9 MB/s  
                                 

Bulk CTR — KDF Amortization
╭──────────┬───────────┬─────────────────┬────────┬───────────╮
│ Messages │ Bulk (ms) │ Individual (ms) │   Gain │ KDF calls │
├──────────┼───────────┼─────────────────┼────────┼───────────┤
│        1 │       2.8 │           116.9 │ 42.07× │  1 vs 1   │
│        5 │      13.9 │           590.7 │ 42.40× │  1 vs 5   │
│       10 │      28.0 │          1186.6 │ 42.42× │  1 vs 10  │
│       50 │     140.1 │          5983.7 │ 42.70× │  1 vs 50  │
│      100 │     281.4 │         11857.7 │ 42.14× │ 1 vs 100  │
╰──────────┴───────────┴─────────────────┴────────┴───────────╯
Bulk: 1 Argon2id derivation shared across N messages

✗ RÉGRESSION DÉTECTÉE 
    RÉGRESSION ctr/ctr-encrypt-1KB/CAGOULE-CTR: baseline_avg=14.7 → current=13.5 MB/s (-8.4% < seuil -5%) [N=5]
    RÉGRESSION ctr/cbc-encrypt-1KB/CAGOULE-CBC: baseline_avg=4.5 → current=3.0 MB/s (-32.8% < seuil -5%) [N=5]
    RÉGRESSION ctr/ctr-decrypt-1KB/CAGOULE-CTR: baseline_avg=8.7 → current=6.1 MB/s (-29.9% < seuil -5%) [N=5]
    RÉGRESSION ctr/ctr-encrypt-8KB/CAGOULE-CTR: baseline_avg=15.0 → current=14.2 MB/s (-5.3% < seuil -5%) [N=5]
    RÉGRESSION ctr/ctr-decrypt-8KB/CAGOULE-CTR: baseline_avg=13.5 → current=9.9 MB/s (-26.8% < seuil -5%) [N=5]
    RÉGRESSION ctr/ctr-auto-128B/CAGOULE-CTR-auto: baseline_avg=2.6 → current=2.4 MB/s (-6.1% < seuil -5%) [N=5]
    RÉGRESSION ctr/ctr-auto-4KB/CAGOULE-CTR-auto: baseline_avg=15.2 → current=11.1 MB/s (-27.1% < seuil -5%) [N=5]
    RÉGRESSION ctr/ctr-sym-encrypt-64KB/CAGOULE-CTR-symmetry-enc: baseline_avg=22.5 → current=12.3 MB/s (-45.2% < seuil -5%) [N=5]
    RÉGRESSION ctr/ctr-sym-decrypt-64KB/CAGOULE-CTR-symmetry-dec: baseline_avg=22.4 → current=19.9 MB/s (-11.1% < seuil -5%) [N=5]
    RÉGRESSION ctr/ctr-sym-encrypt-1MB/CAGOULE-CTR-symmetry-enc: baseline_avg=22.4 → current=21.2 MB/s (-5.5% < seuil -5%) [N=5]
    RÉGRESSION ctr/individual-ctr-1msgs/CAGOULE-individual-CTR: baseline_avg=22.3 → current=0.5 MB/s (-97.6% < seuil -5%) [N=5]
    RÉGRESSION ctr/individual-ctr-5msgs/CAGOULE-individual-CTR: baseline_avg=22.4 → current=0.5 MB/s (-97.6% < seuil -5%) [N=5]
    RÉGRESSION ctr/individual-ctr-10msgs/CAGOULE-individual-CTR: baseline_avg=22.5 → current=0.5 MB/s (-97.7% < seuil -5%) [N=5]
    RÉGRESSION ctr/individual-ctr-50msgs/CAGOULE-individual-CTR: baseline_avg=22.5 → current=0.5 MB/s (-97.7% < seuil -5%) [N=5]
    RÉGRESSION ctr/individual-ctr-100msgs/CAGOULE-individual-CTR: baseline_avg=22.5 → current=0.5 MB/s (-97.7% < seuil -5%) [N=5]
  → Historique : run_id=5ea349d6... sauvegardé dans .cagoule_bench/history.db
(.venv) slim@slim:~/Documents/Cagoule/cagoule/CAGOULE_v3_0_0_release/CAGOULE_v3_0_0/cagoule-bench-v2$ 
