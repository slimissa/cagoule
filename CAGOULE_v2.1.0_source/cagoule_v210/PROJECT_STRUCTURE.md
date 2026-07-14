# Structure du projet — CAGOULE v2.1.0

```
CAGOULE/
│
├── cagoule/                          ← Package Python principal
│   ├── __init__.py                   ← API publique + __backend__, __omega_backend__
│   ├── __version__.py                ← version = "2.1.0"
│   │
│   ├── cipher.py                     ← encrypt(), encrypt_with_params()
│   ├── decipher.py            [v2.1] ← decrypt(), decrypt_with_params()
│   │                                    Fix test_mauvais_mdp
│   │                                    CagouleAuthError enrichi (.reason/.hint/.ct_size)
│   │                                    CagouleFormatError enrichi (.field/.data_size)
│   │
│   ├── params.py              [v2.1] ← CagouleParams.derive() — KDF + dérivation
│   │                                    Attribut fast_mode mémorisé (fix re-dérivation)
│   │                                    Round keys via omega.py v2.1 (backend C)
│   │
│   ├── omega.py               [v2.1] ← ζ(2n) → round keys
│   │                                    Backend C : cagoule_omega_* (libcagoule.so ≥ v2.1)
│   │                                    Fallback Python : mpmath (chargement conditionnel)
│   │                                    OMEGA_BACKEND, _OMEGA_C_SYMBOLS_OK
│   │
│   ├── _binding.py            [v2.1] ← Chargeur ctypes libcagoule.so
│   │                                    Structures CagouleMatrixC, CagouleSBox64C
│   │                                    Signatures CBC encrypt/decrypt
│   │
│   ├── format.py                     ← Parser/serializer format CGL1
│   │                                    Constantes : MAGIC, OVERHEAD, HEADER_SIZE, TAG_SIZE
│   │
│   ├── fp2.py                        ← Arithmétique Fp² (extension quadratique)
│   ├── mu.py                         ← Générateur d'élément primitif µ de Z/pZ ou Fp²
│   ├── matrix.py                     ← DiffusionMatrix — wrapper Python de CagouleMatrixC
│   ├── sbox.py                       ← SBox — wrapper Python de CagouleSBox64C
│   ├── utils.py                      ← secure_zeroize(), SensitiveBuffer
│   ├── logger.py                     ← Configuration logging CAGOULE
│   ├── kat_vectors.json              ← Vecteurs KAT chiffrement (générés par regenerate_kat.py)
│   │
│   └── c/                            ← Noyau C — libcagoule.so
│       ├── Makefile           [v2.1] ← cible make test_omega, make check-openssl, -lcrypto
│       │
│       ├── include/
│       │   ├── cagoule_math.h        ← addmod64, submod64, mulmod64 (arithmétique mod)
│       │   ├── cagoule_matrix.h      ← Vandermonde 16×16 sur Z/pZ
│       │   ├── cagoule_sbox.h        ← S-box Feistel 32 bits
│       │   ├── cagoule_cipher.h      ← Pipeline CBC-like (encrypt/decrypt)
│       │   └── cagoule_omega.h [NEW] ← ζ(2n) → round keys via HKDF-SHA256/OpenSSL
│       │
│       ├── src/
│       │   ├── cagoule_matrix.c      ← Matrice Vandermonde/Cauchy (build, mul, inv)
│       │   ├── cagoule_sbox.c        ← Feistel 2 rondes + cycle-walking
│       │   ├── cagoule_cipher.c      ← CBC-like 1 appel ctypes, validation b<256
│       │   └── cagoule_omega.c [NEW] ← ζ table, Fourier, HKDF/OpenSSL, zeroize
│       │
│       └── tests/
│           ├── test_math.c           ←  33 tests arithmétique modulaire
│           ├── test_matrix.c         ←  52 tests matrice Vandermonde
│           ├── test_sbox.c           ←  48 tests S-box Feistel
│           ├── test_cipher.c         ←  45 tests pipeline CBC
│           └── test_omega.c    [NEW] ←  78 tests omega (ζ, Fourier, round keys, blocs)
│                                         ──────
│                                         256 tests C total (178 v2.0 + 78 v2.1)
│
├── tests/                            ← Tests Python (pytest)
│   ├── conftest.py                   ← Fixtures : fast_params, normal_params
│   ├── test_cipher.py         [v2.1] ← Roundtrip, AuthError, FormatError
│   │                                    test_mauvais_mdp : xfail → PASS ✅
│   │                                    test_auth_error_enrichi (nouveaux attributs)
│   │                                    test_fast_mode_stored
│   ├── test_omega.py          [NEW]  ←  62 tests omega Python
│   │                                    §1. ζ(2n) et Fourier (12 tests)
│   │                                    §2. generate_round_keys (18 tests)
│   │                                    §3. apply/remove_round_key (10 tests)
│   │                                    §4. Compatibilité C ↔ Python bit-à-bit (8 tests)
│   │                                    §5. Backend et cache (8 tests)
│   │                                    §6. Intégration CagouleParams (6 tests)
│   │
│   ├── test_format.py                ← Tests format CGL1
│   ├── test_fp2.py                   ← Tests Fp²
│   ├── test_kat.py                   ← Known-Answer Tests chiffrement
│   ├── test_matrix.py                ← Tests matrice Python
│   ├── test_mu.py                    ← Tests générateur µ
│   ├── test_sbox.py                  ← Tests S-box Python
│   ├── test_nist.py                  ← Tests statistiques NIST (2 skip timeout)
│   └── kat_omega_vectors.json [NEW]  ← Vecteurs KAT omega (regenerate_kat.py --omega)
│
├── CHANGELOG.md               [v2.1] ← Historique complet
├── regenerate_kat.py          [v2.1] ← --omega, --all, --check flags
├── pyproject.toml             [v2.1] ← version=2.1.0, mpmath → optional [fallback]
└── README.md                         ← Documentation publique GitHub
```

---

## Flux de dérivation des paramètres (v2.1.0)

```
password + salt (32B aléatoire)
      │
      ▼
   Argon2id (t=3, m=64 MiB, p=1)        ← ~180 ms
   └─ fallback Scrypt (N=2^17)
      │
      ▼ K_master (64B)
      │
      ├─ HKDF → n (taille bloc)
      ├─ HKDF → p (premier 64 bits, nextprime)
      ├─ HKDF → µ (élément primitif Z/pZ ou Fp²)
      ├─ HKDF → δ (graine S-box Feistel)
      ├─ HKDF → nodes[0..15] (nœuds matrice Vandermonde)
      ├─ HKDF → K_stream (clé ChaCha20, 32B)
      │
      └─ omega.py v2.1.0 ──────────────────────────────────┐
              │                                              │
              │  si libcagoule.so ≥ v2.1    │  sinon        │
              ▼                             ▼               │
         cagoule_omega_generate_round_keys()  _py_generate_round_keys()
              │                             │               │
              │  ζ(2n) table C              │  mpmath.zeta  │
              │  Fourier (double)           │  mpmath power │
              │  HKDF via libcrypto         │  cryptography │
              └─────────────────────────────┘               │
                          │                                  │
                          ▼                                  │
                   round_keys[0..63] ∈ [0, p) ──────────────┘
```

## Règle de dérivation decrypt() v2.1.0

```
decrypt(ct, password, params=P)
│
├─ password non-vide ?
│   └── OUI → re-dérive TOUJOURS depuis (password, salt_cgl1, fast_mode=P.fast_mode)
│              ← FIX test_mauvais_mdp : mauvais mdp → k_stream erroné → AuthError ✅
│
├─ password vide + params=P ?
│   └── Utilise P tel quel (usage interne : KAT, benchmark, decrypt_with_params)
│       Vérifie P.salt == salt_cgl1 → sinon AuthError(params_mismatch)
│
└─ password vide + params=None → CagouleError
```

## Comptage des tests v2.1.0

| Suite         | Fichier              | Tests | Statut          |
|---------------|----------------------|-------|-----------------|
| C — math      | test_math.c          |  33   | ✅ 33/33        |
| C — matrix    | test_matrix.c        |  52   | ✅ 52/52        |
| C — sbox      | test_sbox.c          |  48   | ✅ 48/48        |
| C — cipher    | test_cipher.c        |  45   | ✅ 45/45        |
| **C — omega** | **test_omega.c**     | **78**| ✅ **78/78**    |
| **C total**   |                      |**256**| ✅ **256/256**  |
| Py — cipher   | test_cipher.py       |  87   | ✅ 87/87 (fix!) |
| **Py — omega**| **test_omega.py**    | **62**| ✅ **62/62**    |
| Py — format   | test_format.py       |  31   | ✅ 31/31        |
| Py — fp2      | test_fp2.py          |  44   | ✅ 44/44        |
| Py — kat      | test_kat.py          |  96   | ✅ 96/96        |
| Py — matrix   | test_matrix.py       |  73   | ✅ 73/73        |
| Py — mu       | test_mu.py           |  58   | ✅ 58/58        |
| Py — sbox     | test_sbox.py         | 105   | ✅ 105/105      |
| Py — nist     | test_nist.py         |   4   | ⏭ 2 skip       |
| **Py total**  |                      |**560**| ✅ **557/560**  |
