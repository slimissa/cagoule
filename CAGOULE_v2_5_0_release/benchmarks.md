(venv) slim@slim:~/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench$ cagoule-bench run --suite encryption --iterations 20 --warmup 3 --format console
  → Config chargée depuis : /home/slim/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench/cagoule_bench.toml

────────────────────────────────────────────────────────────── cagoule-bench v2.0.0 ──────────────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 2.5.0  matrix: scalar  omega: C
  Suites: encryption  Iterations: 20  Warmup: 3  Tag: default

  ✓ encryption — 30 benchmarks

──────────────────────────────────────────────────────── Terminé en 125.3s — 30 résultats ────────────────────────────────────────────────────────


╭───────────────────────────────────────────────────────────── CAGOULE-BENCH v2.0.0 ─────────────────────────────────────────────────────────────╮
│ cagoule-bench v2.0.0  |  x86_64  |  3.12.3  |  matrix: scalar  omega: C  |  2026-05-25 15:16 UTC                                               │
╰────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  ENCRYPTION SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
╭────────────────────┬────────────────────┬─────────────┬───────────┬─────────┬──────────┬───────┬───────────╮
│ Test               │ Algorithm          │  Throughput │ Mean (ms) │ ±Stddev │ p95 (ms) │   CV% │  Mem Peak │
├────────────────────┼────────────────────┼─────────────┼───────────┼─────────┼──────────┼───────┼───────────┤
│ encrypt-1KB        │ CAGOULE            │    4.2 MB/s │     0.231 │  ±0.007 │    0.249 │  3.0% │   0.07 MB │
│ decrypt-1KB        │ CAGOULE            │    0.0 MB/s │   179.601 │  ±7.699 │  192.326 │  4.3% │   0.04 MB │
│ encrypt-1KB        │ AES-256-GCM        │  155.2 MB/s │     0.006 │  ±0.001 │    0.007 │  9.7% │   0.00 MB │
│ decrypt-1KB        │ AES-256-GCM        │  172.7 MB/s │     0.006 │  ±0.000 │    0.006 │  2.2% │   0.00 MB │
│ encrypt-1KB        │ ChaCha20-Poly1305  │  160.5 MB/s │     0.006 │  ±0.000 │    0.007 │  5.7% │   0.00 MB │
│ decrypt-1KB        │ ChaCha20-Poly1305  │  160.5 MB/s │     0.006 │  ±0.000 │    0.007 │  4.3% │   0.00 MB │
│ encrypt-8KB        │ CAGOULE            │    4.5 MB/s │     1.731 │  ±0.135 │    2.165 │  7.8% │   0.57 MB │
│ decrypt-8KB        │ CAGOULE            │    0.0 MB/s │   184.560 │ ±10.503 │  206.237 │  5.7% │   0.21 MB │
│ encrypt-8KB        │ AES-256-GCM        │  706.4 MB/s │     0.011 │  ±0.000 │    0.012 │  2.1% │   0.02 MB │
│ decrypt-8KB        │ AES-256-GCM        │  792.4 MB/s │     0.010 │  ±0.003 │    0.023 │ 32.6% │   0.02 MB │
│ encrypt-8KB        │ ChaCha20-Poly1305  │  602.0 MB/s │     0.013 │  ±0.000 │    0.013 │  1.0% │   0.02 MB │
│ decrypt-8KB        │ ChaCha20-Poly1305  │  602.6 MB/s │     0.013 │  ±0.003 │    0.024 │ 19.3% │   0.02 MB │
│ encrypt-64KB       │ CAGOULE            │    4.5 MB/s │    13.792 │  ±0.556 │   15.041 │  4.0% │   4.56 MB │
│ decrypt-64KB       │ CAGOULE            │    0.3 MB/s │   190.489 │  ±5.652 │  201.334 │  3.0% │   1.57 MB │
│ encrypt-64KB       │ AES-256-GCM        │ 2344.9 MB/s │     0.027 │  ±0.002 │    0.034 │  6.9% │   0.13 MB │
│ decrypt-64KB       │ AES-256-GCM        │ 2494.6 MB/s │     0.025 │  ±0.000 │    0.027 │  1.5% │   0.13 MB │
│ encrypt-64KB       │ ChaCha20-Poly1305  │ 1025.3 MB/s │     0.061 │  ±0.002 │    0.064 │  2.5% │   0.13 MB │
│ decrypt-64KB       │ ChaCha20-Poly1305  │ 1021.6 MB/s │     0.061 │  ±0.002 │    0.066 │  2.7% │   0.13 MB │
│ encrypt-1MB        │ CAGOULE            │    4.3 MB/s │   231.934 │  ±8.606 │  240.804 │  3.7% │  73.00 MB │
│ decrypt-1MB        │ CAGOULE            │    4.1 MB/s │   245.405 │  ±7.024 │  271.143 │  2.9% │  25.01 MB │
│ encrypt-1MB        │ AES-256-GCM        │ 2224.9 MB/s │     0.449 │  ±0.045 │    0.551 │ 10.0% │   2.00 MB │
│ decrypt-1MB        │ AES-256-GCM        │ 2451.9 MB/s │     0.408 │  ±0.008 │    0.425 │  2.0% │   2.00 MB │
│ encrypt-1MB        │ ChaCha20-Poly1305  │ 1084.1 MB/s │     0.922 │  ±0.025 │    0.953 │  2.7% │   2.00 MB │
│ decrypt-1MB        │ ChaCha20-Poly1305  │  911.3 MB/s │     1.097 │  ±0.109 │    1.272 │ 10.0% │   2.00 MB │
│ encrypt-10MB       │ CAGOULE            │    4.5 MB/s │  2206.950 │ ±29.405 │ 2297.078 │  1.3% │ 730.01 MB │
│ decrypt-10MB       │ CAGOULE            │    9.6 MB/s │  1038.886 │  ±9.550 │ 1068.956 │  0.9% │ 250.01 MB │
│ encrypt-10MB       │ AES-256-GCM        │ 2353.2 MB/s │     4.250 │  ±0.118 │    4.499 │  2.8% │  20.00 MB │
│ decrypt-10MB       │ AES-256-GCM        │ 1965.6 MB/s │     5.088 │  ±0.294 │    5.888 │  5.8% │  20.00 MB │
│ encrypt-10MB       │ ChaCha20-Poly1305  │ 1031.4 MB/s │     9.696 │  ±0.596 │   10.494 │  6.1% │  20.00 MB │
│ decrypt-10MB       │ ChaCha20-Poly1305  │ 1010.7 MB/s │     9.895 │  ±0.579 │   10.989 │  5.9% │  20.00 MB │
╰────────────────────┴────────────────────┴─────────────┴───────────┴─────────┴──────────┴───────┴───────────╯

Overhead — CAGOULE vs standards
                                                        
  Test           vs AES-256-GCM   vs ChaCha20-Poly1305  
 ────────────────────────────────────────────────────── 
  decrypt-10MB           -99.5%                 -99.0%  
  decrypt-1KB           -100.0%                -100.0%  
  decrypt-1MB            -99.8%                 -99.6%  
  decrypt-64KB          -100.0%                -100.0%  
  decrypt-8KB           -100.0%                -100.0%  
  encrypt-10MB           -99.8%                 -99.6%  
  encrypt-1KB            -97.3%                 -97.4%  
  encrypt-1MB            -99.8%                 -99.6%  
  encrypt-64KB           -99.8%                 -99.6%  
  encrypt-8KB            -99.4%                 -99.3%  
                                                        

✗ RÉGRESSION DÉTECTÉE 
    RÉGRESSION encryption/encrypt-1KB/CAGOULE: baseline_avg=5.1 → current=4.2 MB/s (-17.5% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-1KB/CAGOULE: baseline_avg=0.0 → current=0.0 MB/s (-26.5% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-1KB/AES-256-GCM: baseline_avg=188.7 → current=155.2 MB/s (-17.7% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-1KB/AES-256-GCM: baseline_avg=234.9 → current=172.7 MB/s (-26.5% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-1KB/ChaCha20-Poly1305: baseline_avg=189.2 → current=160.5 MB/s (-15.2% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-8KB/CAGOULE: baseline_avg=5.3 → current=4.5 MB/s (-14.9% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-8KB/CAGOULE: baseline_avg=0.1 → current=0.0 MB/s (-31.3% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-8KB/AES-256-GCM: baseline_avg=842.2 → current=706.4 MB/s (-16.1% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-8KB/AES-256-GCM: baseline_avg=1037.9 → current=792.4 MB/s (-23.7% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-8KB/ChaCha20-Poly1305: baseline_avg=668.9 → current=602.6 MB/s (-9.9% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-64KB/CAGOULE: baseline_avg=5.4 → current=4.5 MB/s (-16.5% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-64KB/CAGOULE: baseline_avg=0.5 → current=0.3 MB/s (-30.4% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-1MB/CAGOULE: baseline_avg=5.0 → current=4.3 MB/s (-13.0% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-1MB/AES-256-GCM: baseline_avg=3316.4 → current=2224.9 MB/s (-32.9% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-1MB/AES-256-GCM: baseline_avg=3449.0 → current=2451.9 MB/s (-28.9% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-1MB/ChaCha20-Poly1305: baseline_avg=1688.1 → current=1084.1 MB/s (-35.8% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-1MB/ChaCha20-Poly1305: baseline_avg=1656.3 → current=911.3 MB/s (-45.0% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-10MB/CAGOULE: baseline_avg=4.8 → current=4.5 MB/s (-5.6% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-10MB/AES-256-GCM: baseline_avg=3202.2 → current=2353.2 MB/s (-26.5% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-10MB/AES-256-GCM: baseline_avg=2722.7 → current=1965.6 MB/s (-27.8% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-10MB/ChaCha20-Poly1305: baseline_avg=1456.9 → current=1031.4 MB/s (-29.2% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-10MB/ChaCha20-Poly1305: baseline_avg=1420.6 → current=1010.7 MB/s (-28.9% < seuil -5%) [N=5]
  → Historique : run_id=41ab7b42... sauvegardé dans .cagoule_bench/history.db




(venv) slim@slim:~/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench$ cagoule-bench info

──────────────────────────────────────────────────── cagoule-bench v2.0.0 — Environment Info ─────────────────────────────────────────────────────

Système
                                                             
  Key         Value                                          
 ─────────────────────────────────────────────────────────── 
  Platform    Linux-6.17.0-22-generic-x86_64-with-glibc2.39  
  Machine     x86_64                                         
  Python      3.12.3                                         
  CPU count   20                                             
  AES-NI      ✓                                              
  AVX2        ✓                                              
                                                             

CAGOULE
                                                   
  Key               Value                          
 ───────────────────────────────────────────────── 
  cagoule version   2.5.0                          
  matrix_backend    scalar                         
  omega_backend     C                              
  CGL1 format       inchangé (v2.2.0 rétrocompat)  
                                                   

Dépendances
                              
  Package        Status       
 ──────────────────────────── 
  cryptography   ✓ installed  
  argon2-cffi    ✓ installed  
  psutil         ✓ installed  
  rich           ✓ installed  
  click          ✓ installed  
  jinja2         ✓ installed  
                              

(venv) slim@slim:~/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench$ cagoule-bench run --suite encryption --iterations 30 --warmup 3 --format console
  → Config chargée depuis : /home/slim/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench/cagoule_bench.toml

────────────────────────────────────────────────────────────── cagoule-bench v2.0.0 ──────────────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 2.5.0  matrix: scalar  omega: C
  Suites: encryption  Iterations: 30  Warmup: 3  Tag: default

  ✓ encryption — 30 benchmarks

──────────────────────────────────────────────────────── Terminé en 111.7s — 30 résultats ────────────────────────────────────────────────────────


╭───────────────────────────────────────────────────────────── CAGOULE-BENCH v2.0.0 ─────────────────────────────────────────────────────────────╮
│ cagoule-bench v2.0.0  |  x86_64  |  3.12.3  |  matrix: scalar  omega: C  |  2026-05-25 17:16 UTC                                               │
╰────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  ENCRYPTION SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
╭────────────────────┬────────────────────┬─────────────┬───────────┬─────────┬──────────┬───────┬───────────╮
│ Test               │ Algorithm          │  Throughput │ Mean (ms) │ ±Stddev │ p95 (ms) │   CV% │  Mem Peak │
├────────────────────┼────────────────────┼─────────────┼───────────┼─────────┼──────────┼───────┼───────────┤
│ encrypt-1KB        │ CAGOULE            │    7.1 MB/s │     0.138 │  ±0.003 │    0.143 │  2.0% │   0.07 MB │
│ decrypt-1KB        │ CAGOULE            │    0.0 MB/s │   114.849 │  ±1.837 │  120.488 │  1.6% │   0.04 MB │
│ encrypt-1KB        │ AES-256-GCM        │  177.5 MB/s │     0.006 │  ±0.002 │    0.008 │ 45.3% │   0.00 MB │
│ decrypt-1KB        │ AES-256-GCM        │  216.8 MB/s │     0.005 │  ±0.000 │    0.005 │  3.8% │   0.00 MB │
│ encrypt-1KB        │ ChaCha20-Poly1305  │  166.8 MB/s │     0.006 │  ±0.000 │    0.006 │  3.7% │   0.00 MB │
│ decrypt-1KB        │ ChaCha20-Poly1305  │  209.9 MB/s │     0.005 │  ±0.000 │    0.005 │  2.8% │   0.00 MB │
│ encrypt-8KB        │ CAGOULE            │    8.1 MB/s │     0.967 │  ±0.012 │    1.007 │  1.3% │   0.57 MB │
│ decrypt-8KB        │ CAGOULE            │    0.1 MB/s │   118.699 │  ±3.225 │  123.414 │  2.7% │   0.21 MB │
│ encrypt-8KB        │ AES-256-GCM        │  769.8 MB/s │     0.010 │  ±0.000 │    0.011 │  3.9% │   0.02 MB │
│ decrypt-8KB        │ AES-256-GCM        │  745.8 MB/s │     0.010 │  ±0.003 │    0.010 │ 28.9% │   0.02 MB │
│ encrypt-8KB        │ ChaCha20-Poly1305  │  515.8 MB/s │     0.015 │  ±0.002 │    0.020 │ 11.8% │   0.02 MB │
│ decrypt-8KB        │ ChaCha20-Poly1305  │  656.3 MB/s │     0.012 │  ±0.001 │    0.013 │  8.9% │   0.02 MB │
│ encrypt-64KB       │ CAGOULE            │    8.0 MB/s │     7.796 │  ±0.169 │    7.871 │  2.2% │   4.56 MB │
│ decrypt-64KB       │ CAGOULE            │    0.5 MB/s │   119.886 │  ±2.121 │  123.051 │  1.8% │   1.57 MB │
│ encrypt-64KB       │ AES-256-GCM        │ 1367.6 MB/s │     0.046 │  ±0.002 │    0.049 │  3.9% │   0.13 MB │
│ decrypt-64KB       │ AES-256-GCM        │ 1839.8 MB/s │     0.034 │  ±0.002 │    0.039 │  5.8% │   0.13 MB │
│ encrypt-64KB       │ ChaCha20-Poly1305  │ 1012.8 MB/s │     0.062 │  ±0.003 │    0.066 │  4.8% │   0.13 MB │
│ decrypt-64KB       │ ChaCha20-Poly1305  │ 1323.5 MB/s │     0.047 │  ±0.003 │    0.051 │  5.5% │   0.13 MB │
│ encrypt-1MB        │ CAGOULE            │    6.9 MB/s │   144.502 │  ±1.798 │  146.945 │  1.2% │  73.00 MB │
│ decrypt-1MB        │ CAGOULE            │    6.0 MB/s │   167.621 │  ±5.271 │  175.330 │  3.1% │  25.01 MB │
│ encrypt-1MB        │ AES-256-GCM        │ 2993.7 MB/s │     0.334 │  ±0.103 │    0.544 │ 30.8% │   2.00 MB │
│ decrypt-1MB        │ AES-256-GCM        │ 4157.2 MB/s │     0.241 │  ±0.002 │    0.243 │  0.9% │   2.00 MB │
│ encrypt-1MB        │ ChaCha20-Poly1305  │ 1783.4 MB/s │     0.561 │  ±0.038 │    0.665 │  6.7% │   2.00 MB │
│ decrypt-1MB        │ ChaCha20-Poly1305  │ 1586.5 MB/s │     0.630 │  ±0.144 │    0.890 │ 22.9% │   2.00 MB │
│ encrypt-10MB       │ CAGOULE            │    6.8 MB/s │  1479.784 │  ±9.191 │ 1507.292 │  0.6% │ 730.01 MB │
│ decrypt-10MB       │ CAGOULE            │   14.4 MB/s │   694.591 │  ±7.722 │  711.551 │  1.1% │ 250.01 MB │
│ encrypt-10MB       │ AES-256-GCM        │ 3738.1 MB/s │     2.675 │  ±0.033 │    2.751 │  1.2% │  20.00 MB │
│ decrypt-10MB       │ AES-256-GCM        │ 3199.2 MB/s │     3.126 │  ±0.195 │    3.324 │  6.2% │  20.00 MB │
│ encrypt-10MB       │ ChaCha20-Poly1305  │ 1719.8 MB/s │     5.815 │  ±0.106 │    6.068 │  1.8% │  20.00 MB │
│ decrypt-10MB       │ ChaCha20-Poly1305  │ 1584.6 MB/s │     6.311 │  ±0.228 │    6.766 │  3.6% │  20.00 MB │
╰────────────────────┴────────────────────┴─────────────┴───────────┴─────────┴──────────┴───────┴───────────╯

Overhead — CAGOULE vs standards
                                                        
  Test           vs AES-256-GCM   vs ChaCha20-Poly1305  
 ────────────────────────────────────────────────────── 
  decrypt-10MB           -99.5%                 -99.1%  
  decrypt-1KB           -100.0%                -100.0%  
  decrypt-1MB            -99.9%                 -99.6%  
  decrypt-64KB          -100.0%                -100.0%  
  decrypt-8KB           -100.0%                -100.0%  
  encrypt-10MB           -99.8%                 -99.6%  
  encrypt-1KB            -96.0%                 -95.8%  
  encrypt-1MB            -99.8%                 -99.6%  
  encrypt-64KB           -99.4%                 -99.2%  
  encrypt-8KB            -99.0%                 -98.4%  
                                                        

✗ RÉGRESSION DÉTECTÉE 
    RÉGRESSION encryption/encrypt-8KB/AES-256-GCM: baseline_avg=833.0 → current=769.8 MB/s (-7.6% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-8KB/AES-256-GCM: baseline_avg=1015.9 → current=745.8 MB/s (-26.6% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-8KB/ChaCha20-Poly1305: baseline_avg=623.4 → current=515.8 MB/s (-17.3% < seuil -5%) [N=5]
    RÉGRESSION encryption/encrypt-64KB/AES-256-GCM: baseline_avg=2023.1 → current=1367.6 MB/s (-32.4% < seuil -5%) [N=5]
    RÉGRESSION encryption/decrypt-64KB/AES-256-GCM: baseline_avg=1951.9 → current=1839.8 MB/s (-5.7% < seuil -5%) [N=5]
  → Historique : run_id=40fa8bab... sauvegardé dans .cagoule_bench/history.db



(venv) slim@slim:~/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench$ cagoule-bench run --suite avx2 --iterations 30 --warmup 3 --format console
  → Config chargée depuis : /home/slim/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench/cagoule_bench.toml

────────────────────────────────────────────────────────────── cagoule-bench v2.0.0 ──────────────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 2.5.0  matrix: scalar  omega: C
  Suites: avx2  Iterations: 30  Warmup: 3  Tag: default

  ✓ avx2 — 6 benchmarks

──────────────────────────────────────────────────────── Terminé en 134.7s — 6 résultats ─────────────────────────────────────────────────────────


╭───────────────────────────────────────────────────────────── CAGOULE-BENCH v2.0.0 ─────────────────────────────────────────────────────────────╮
│ cagoule-bench v2.0.0  |  x86_64  |  3.12.3  |  matrix: scalar  omega: C  |  2026-05-25 17:20 UTC                                               │
╰────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  AVX2 SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

CAGOULE v2.2.0 — Vectorisation AVX2 vs Scalaire
  matrix_backend: scalar  omega_backend: C  AVX2 actif: ✗ NON (fallback scalaire)
╭────────┬─────────────┬───────────────┬─────────┬────────┬─────────┬───────────╮
│ Taille │ AVX2 (MB/s) │ Scalar (MB/s) │ Speedup │   Gain │ AVX2 ms │ Scalar ms │
├────────┼─────────────┼───────────────┼─────────┼────────┼─────────┼───────────┤
│   64KB │         8.1 │           8.2 │   0.99x │ +-0.5% │    7.70 │      7.66 │
│    1MB │         6.7 │           6.7 │   1.00x │ +-0.1% │  150.34 │    150.27 │
│   10MB │         6.7 │           6.7 │   1.00x │  +0.1% │ 1494.89 │   1496.42 │
╰────────┴─────────────┴───────────────┴─────────┴────────┴─────────┴───────────╯

  Gain moyen AVX2 : -0.2%  (objectif roadmap v2.2.0 : ≥ +25%)
Note: CAGOULE_FORCE_SCALAR=1 utilisé pour mesurer le chemin scalaire

✓ Pas de régression 
  6 benchmarks OK vs historique (N≥5).
  → Historique : run_id=246bbd2e... sauvegardé dans .cagoule_bench/history.db


(venv) slim@slim:~/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench$ cagoule-bench run --suite kdf --suite memory --suite streaming --iterations 3 --warmup 1 --format consolerations=50)
  → Config chargée depuis : /home/slim/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench/cagoule_bench.toml

────────────────────────────────────────────────────────────── cagoule-bench v2.0.0 ──────────────────────────────────────────────────────────────
  Platform: x86_64  Python: 3.12.3  CAGOULE: 2.5.0  matrix: scalar  omega: C
  Suites: kdf, memory, streaming  Iterations: 3  Warmup: 1  Tag: default

  ✓ kdf — 33 benchmarks
  ✓ memory — 4 benchmarks
  ✓ streaming — 9 benchmarks

──────────────────────────────────────────────────────── Terminé en 540.3s — 46 résultats ────────────────────────────────────────────────────────


╭───────────────────────────────────────────────────────────── CAGOULE-BENCH v2.0.0 ─────────────────────────────────────────────────────────────╮
│ cagoule-bench v2.0.0  |  x86_64  |  3.12.3  |  matrix: ?  omega: ?  |  2026-05-25 17:35 UTC                                                    │
╰────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  KDF SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Argon2id — Grille de paramètres
╭───┬────────┬───┬───────────┬─────────┬──────────┬───────┬────────────┬───────╮
│ t │ m_cost │ p │ Mean (ms) │ ±Stddev │ Peak RAM │ Score │ GPU-resist │ OWASP │
├───┼────────┼───┼───────────┼─────────┼──────────┼───────┼────────────┼───────┤
│ 1 │ 16 MB  │ 1 │       7.9 │    ±0.1 │   0.0 MB │ 14.0  │    14.0    │   ✗   │
│ 1 │ 16 MB  │ 2 │       6.2 │    ±0.1 │   0.0 MB │ 15.0  │    15.0    │   ✗   │
│ 1 │ 16 MB  │ 4 │       3.7 │    ±0.3 │   0.0 MB │ 16.0  │    16.0    │   ✗   │
│ 1 │ 64 MB  │ 1 │      52.0 │    ±0.5 │   0.0 MB │ 16.0  │    16.0    │   ✗   │
│ 1 │ 64 MB  │ 2 │      44.5 │    ±6.0 │   0.0 MB │ 17.0  │    17.0    │   ✗   │
│ 1 │ 64 MB  │ 4 │      26.4 │    ±1.4 │   0.0 MB │ 18.0  │    18.0    │   ✗   │
│ 1 │ 128 MB │ 1 │     107.9 │    ±1.2 │   0.0 MB │ 17.0  │    17.0    │   ✗   │
│ 1 │ 128 MB │ 2 │      76.6 │    ±4.1 │   0.0 MB │ 18.0  │    18.0    │   ✗   │
│ 1 │ 128 MB │ 4 │      51.0 │    ±1.4 │   0.0 MB │ 19.0  │    19.0    │   ✗   │
│ 3 │ 16 MB  │ 1 │      21.4 │    ±0.3 │   0.0 MB │ 15.6  │    14.0    │   ✗   │
│ 3 │ 16 MB  │ 2 │      17.1 │    ±0.7 │   0.0 MB │ 16.6  │    15.0    │   ✗   │
│ 3 │ 16 MB  │ 4 │       9.6 │    ±0.1 │   0.0 MB │ 17.6  │    16.0    │   ✗   │
│ 3 │ 64 MB  │ 1 │     113.7 │    ±0.2 │   0.0 MB │ 17.6  │    16.0    │   ✓   │
│ 3 │ 64 MB  │ 2 │      86.4 │    ±1.3 │   0.0 MB │ 18.6  │    17.0    │   ✓   │
│ 3 │ 64 MB  │ 4 │      51.4 │    ±0.3 │   0.0 MB │ 19.6  │    18.0    │   ✓   │
│ 3 │ 128 MB │ 1 │     241.6 │    ±3.2 │   0.0 MB │ 18.6  │    17.0    │   ✓   │
│ 3 │ 128 MB │ 2 │     171.4 │    ±4.4 │   0.0 MB │ 19.6  │    18.0    │   ✓   │
│ 3 │ 128 MB │ 4 │     102.9 │    ±2.8 │   0.0 MB │ 20.6  │    19.0    │   ✓   │
│ 5 │ 16 MB  │ 1 │      35.9 │    ±1.6 │   0.0 MB │ 16.3  │    14.0    │   ✗   │
│ 5 │ 16 MB  │ 2 │      29.5 │    ±0.7 │   0.0 MB │ 17.3  │    15.0    │   ✗   │
│ 5 │ 16 MB  │ 4 │      16.4 │    ±0.4 │   0.0 MB │ 18.3  │    16.0    │   ✗   │
│ 5 │ 64 MB  │ 1 │     179.2 │    ±3.8 │   0.0 MB │ 18.3  │    16.0    │   ✓   │
│ 5 │ 64 MB  │ 2 │     139.7 │    ±5.4 │   0.0 MB │ 19.3  │    17.0    │   ✓   │
│ 5 │ 64 MB  │ 4 │      77.4 │    ±1.7 │   0.0 MB │ 20.3  │    18.0    │   ✓   │
│ 5 │ 128 MB │ 1 │     379.8 │   ±10.5 │   0.0 MB │ 19.3  │    17.0    │   ✓   │
│ 5 │ 128 MB │ 2 │     271.1 │    ±7.1 │   0.0 MB │ 20.3  │    18.0    │   ✓   │
│ 5 │ 128 MB │ 4 │     152.2 │    ±3.6 │   0.0 MB │ 21.3  │    19.0    │   ✓   │
╰───┴────────┴───┴───────────┴─────────┴──────────┴───────┴────────────┴───────╯

scrypt
╭─────────┬───┬───┬───────────┬───────────────┬───────┬───────╮
│       N │ r │ p │ Mean (ms) │ Théorique RAM │ Score │ OWASP │
├─────────┼───┼───┼───────────┼───────────────┼───────┼───────┤
│  16,384 │ 8 │ 1 │      28.3 │       16.0 MB │ 17.0  │   ✗   │
│  65,536 │ 8 │ 1 │       0.0 │       64.0 MB │ 17.0  │   ✓   │
│ 131,072 │ 8 │ 2 │       0.0 │      256.0 MB │ 17.0  │   ✓   │
╰─────────┴───┴───┴───────────┴───────────────┴───────┴───────╯

PBKDF2-SHA256 (référence)
                                          
  Iterations   Mean (ms)   Score   OWASP  
 ──────────────────────────────────────── 
     100,000        21.1   16.6      ✗    
     300,000        62.5   18.2      ✗    
     600,000       125.2   19.2      ✓    
                                          

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  MEMORY SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
╭───────────────┬──────────┬──────────┬────────────┬───────────┬────────╮
│    Vault Size │ Peak RAM │ MB/entry │ Build (ms) │ Entries/s │ Fragm. │
├───────────────┼──────────┼──────────┼────────────┼───────────┼────────┤
│    10 entries │  0.01 MB │  0.00105 │        0.0 │    417798 │   1.4% │
│   100 entries │  0.10 MB │  0.00102 │        0.5 │    202345 │   0.1% │
│ 1,000 entries │  1.02 MB │  0.00102 │        2.3 │    429904 │   0.0% │
│     0 entries │  0.00 MB │  0.00000 │        0.0 │         0 │   0.0% │
╰───────────────┴──────────┴──────────┴────────────┴───────────┴────────╯

Cache Analysis  Cold: 0.450ms  Hot: 0.048ms  Speedup: 9.4x

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  STREAMING SUITE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
╭────────────────────────┬───────────────────┬────────────┬───────────┬───────┬──────────╮
│ Test                   │ Algorithm         │ Throughput │ Mean (ms) │ Chunk │ RAM eff. │
├────────────────────────┼───────────────────┼────────────┼───────────┼───────┼──────────┤
│ stream-encrypt-50MB    │ AES-256-GCM       │ 455.9 MB/s │       110 │ 64 KB │ O(chunk) │
│ stream-encrypt-50MB    │ ChaCha20-Poly1305 │ 403.2 MB/s │       124 │ 64 KB │ O(chunk) │
│ stream-encrypt-50MB    │ CAGOULE           │   7.9 MB/s │      6332 │ 64 KB │ O(total) │
│ stream-encrypt-100MB   │ AES-256-GCM       │ 456.6 MB/s │       219 │ 64 KB │ O(chunk) │
│ stream-encrypt-100MB   │ ChaCha20-Poly1305 │ 401.5 MB/s │       249 │ 64 KB │ O(chunk) │
│ stream-encrypt-100MB   │ CAGOULE           │   7.9 MB/s │     12705 │ 64 KB │ O(chunk) │
│ stream-encrypt-500MB   │ AES-256-GCM       │ 458.3 MB/s │      1091 │ 64 KB │ O(chunk) │
│ stream-encrypt-500MB   │ ChaCha20-Poly1305 │ 403.1 MB/s │      1240 │ 64 KB │ O(chunk) │
│ stream-encrypt-500MB   │ CAGOULE           │   7.8 MB/s │     63832 │ 64 KB │ O(chunk) │
╰────────────────────────┴───────────────────┴────────────┴───────────┴───────┴──────────╯
Streaming: lecture chunked → chiffrement → sortie — RAM = O(chunk) idéalement

✓ Pas de régression 
  0 benchmarks OK vs historique (N≥5).
  → Historique : run_id=8de05714... sauvegardé dans .cagoule_bench/history.db
(venv) slim@slim:~/Documents/Cagoule/cagoule-bench/cagoule-bench-v2.0.0/cagoule-bench$
