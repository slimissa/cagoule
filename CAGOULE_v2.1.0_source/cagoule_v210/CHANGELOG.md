# CHANGELOG — CAGOULE

## [2.1.0] — 2026-04-28

### Nouveautés

#### omega.c — Portage C de ζ(2n) → round keys
- `cagoule_omega_generate_round_keys()` : implémentation C complète
  - Table ζ(2n) précalculée pour n ≤ 32 (précision double, vérifié vs mpmath)
  - Coefficients de Fourier cₖ = (2/π)(−1)ᵏ/k^(2n) en virgule flottante double
  - HKDF-SHA256 natif via libcrypto (OpenSSL ≥ 1.1), sans Python
  - Réduction modulaire 256 bits → uint64 via Horner + `__uint128_t`
- `cagoule_omega_zeta_2n()`, `cagoule_omega_fourier_coeff()` : API C publique
- `cagoule_omega_block_add_rk()`, `cagoule_omega_block_sub_rk()` : add/sub mod p
- `cagoule_omega_openssl_available()` : détection OpenSSL à l'exécution
- Résultats **bit-à-bit identiques** à mpmath pour n ≤ 32

#### omega.py — Backend C + fallback mpmath
- Détection automatique des symboles `cagoule_omega_*` dans libcagoule.so
- `OMEGA_BACKEND` : variable publique indiquant le backend actif
- `mpmath` n'est plus importé en production (chargement conditionnel)
- `_OMEGA_C_SYMBOLS_OK` : flag interne pour les tests de compatibilité

#### Fix test_mauvais_mdp (v2.0.1 anticipé dans v2.1.0)
- **Symptôme** : `decrypt(ct, wrong_password, params=correct_params)` ne levait
  pas `CagouleAuthError` car `params.k_stream` (correct) était utilisé directement
- **Cause** : quand `params=` était fourni, `password` était silencieusement ignoré
- **Fix** : si `password` est non-vide, `decrypt()` re-dérive **toujours** les
  paramètres depuis `(password, salt_cgl1, fast_mode=params.fast_mode)`
  → mauvais mdp → k_master erroné → k_stream erroné → `InvalidTag` → `CagouleAuthError`
- `CagouleParams.fast_mode` : nouvel attribut mémorisé pour la re-dérivation correcte
- `decrypt_with_params(ct, params)` → passe `password=b''` pour usage interne (KAT)

#### Enrichissement des exceptions
- `CagouleAuthError` : nouveaux attributs `.reason`, `.ct_size`, `.backend`, `.hint`
  - `str(CagouleAuthError)` : message multi-lignes avec diagnostic complet
  - `repr(CagouleAuthError)` : format compact pour les logs
- `CagouleFormatError` : nouveaux attributs `.field`, `.data_size`, `.min_size`
  - `_detect_bad_field()` : identifie le champ CGL1 invalide (magic/version/salt/nonce/tag)

#### Refactoring cipher.py / decipher.py
- `decrypt()` et `decrypt_with_params()` déplacés de `cipher.py` vers `decipher.py`
  (isolation du fix et évitement du couplage circulaire)
- `cipher.py` exporte uniquement : `encrypt`, `encrypt_with_params`, et les
  helpers internes (`_build_aad`, `_parse_cgl1`, `_cbc_decrypt`) pour `decipher.py`

#### Makefile C — v2.1.0
- Nouvelle cible `make test_omega` (78 tests C)
- Nouvelle cible `make check-openssl` : vérifie pkg-config + HMAC fonctionnel
- `make tests` : 256 tests total (178 v2.0 + 78 omega)
- `-lcrypto` ajouté automatiquement (via pkg-config ou direct)
- Détection `__uint128_t` au `make` → erreur explicite si absent

#### regenerate_kat.py — v2.1.0
- Nouveau flag `--omega` : génère `tests/kat_omega_vectors.json`
- Nouveau flag `--all` : régénère chiffrement + omega
- Nouveau flag `--check` : vérifie les vecteurs sans écraser

### Performance (estimée)
- Dérivation des paramètres (omega) : **−40% à −60%** (suppression mpmath)
- Chiffrement/déchiffrement : inchangé (déjà C depuis v2.0)

### Sécurité
- Aucune vulnérabilité supplémentaire introduite
- `secure_zeroize()` appliquée aux buffers `key_material` dans `omega.c`

### Tests
| Suite              | v2.0.0      | v2.1.0      | Delta     |
|--------------------|-------------|-------------|-----------|
| C — math           | 33/33       | 33/33       | —         |
| C — matrix         | 52/52       | 52/52       | —         |
| C — sbox           | 48/48       | 48/48       | —         |
| C — cipher         | 45/45       | 45/45       | —         |
| **C — omega**      | —           | **78/78**   | +78       |
| **C total**        | **178/178** | **256/256** | **+78**   |
| Py — cipher        | 87/87       | 87/87 ✅    | test_mauvais_mdp fixé |
| **Py — omega**     | —           | **62/62**   | +62       |
| Py — autres        | 408/411     | 408/411     | —         |
| **Py total**       | **495/498** | **557/560** | **+62**   |

### Dépendances
- `mpmath` rétrogradée de dépendance obligatoire → optionnelle (`[fallback]`)
- `openssl` (libcrypto) : nouvelle dépendance C (déjà présente sur Linux)
- `argon2-cffi`, `cryptography` : inchangées

### Breaking changes
- Aucun — API 100% compatible v2.0.x et v1.x

---

## [2.0.0] — 2026-04-27

### Nouveautés
- Portage C complet : `cagoule_matrix.c`, `cagoule_sbox.c`, `cagoule_cipher.c`
- S-box Feistel 32 bits (ratio decrypt/encrypt : 7.8× → ~1×)
- Pipeline CBC-like en C (1 appel ctypes vs 65 536)
- Fallback Python pur automatique si libcagoule.so absent
- 5 corrections de sécurité (CAGOULE_P32_PRIME, addmod64, Cauchy, b<256, zeroize)
- 178 tests C, 495 tests Python

---

## [1.5.0] — 2026-03-15

### Baseline Python pur
- KDF : Argon2id + HKDF-SHA256
- Chiffrement : ChaCha20-Poly1305 + couche algébrique Python
- Format CGL1 (MAGIC + VERSION + SALT + NONCE + CT + TAG)
- cagoule-pass v1.0.0
