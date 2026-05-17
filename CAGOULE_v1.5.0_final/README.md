# CAGOULE v1.6.0

[![Python Version](https://img.shields.io/badge/python-3.9%2B-blue)](https://www.python.org/downloads/)
[![Tests](https://img.shields.io/badge/tests-247%2F247-brightgreen)](https://github.com/slimissa/CAGOULE/tree/master/tests)
[![NIST](https://img.shields.io/badge/NIST_SP_800--22-14%2F14-brightgreen)](https://csrc.nist.gov/publications/detail/sp/800-22/rev-1a/final)
[![PyPI](https://img.shields.io/badge/PyPI-1.6.0-blue)](https://pypi.org/project/cagoule/)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![GitHub Actions](https://github.com/slimissa/CAGOULE/actions/workflows/tests.yml/badge.svg)](https://github.com/slimissa/CAGOULE/actions/workflows/tests.yml)

**Cryptographie Algébrique Géométrique par Ondes et Logique Entrelacée**

Système de chiffrement symétrique hybride fusionnant des primitives cryptographiques modernes avec des structures mathématiques issues du **Concours Général Sénégalais 2025** (CGS2025).

> ⚠️ **Usage académique.** CAGOULE est un projet de recherche et d'expérimentation cryptographique. Ne pas utiliser en production sans audit de sécurité indépendant.

---

## Qu'est-ce que CAGOULE ?

CAGOULE chiffre en **deux couches successives** :

```
Plaintext
  │
  ▼  PKCS7 pad → blocs de 16 octets → Z/pZ  (p ≈ 2⁶⁴, premier dérivé du mot de passe)
  │
  ┌─────────────────────────────── COUCHE 1 : CBC-like interne ───────────────────────────────┐
  │  1. v = m_i + prev_cipher   mod p   ← CBC mixing                                         │
  │  2. w = P × v               mod p   ← diffusion Vandermonde 16×16                        │
  │  3. u = S-box(w) = w^d      mod p   ← confusion algébrique (fallback x^d, d=3 typique)  │
  │  4. c = u + round_key_Ω     mod p   ← clé de ronde dérivée de ζ(2n)                     │
  └────────────────────────────────────────────────────────────────────────────────────────────┘
  │
  ┌─────────────────────────────── COUCHE 2 : AEAD (RFC 8439) ────────────────────────────────┐
  │  ChaCha20-Poly1305  — nonce 96 bits aléatoire — tag Poly1305 128 bits                    │
  └────────────────────────────────────────────────────────────────────────────────────────────┘
  │
  ▼  Format CGL1 : Magic(4) | Version(1) | Salt(32) | Nonce(12) | CT | Tag(16)
```

La **couche interne** exploite la théorie des groupes, les matrices de Vandermonde, les extensions quadratiques Fp², et la fonction zêta de Riemann comme source de clés de ronde. La **couche externe** (ChaCha20-Poly1305) garantit la confidentialité et l'authenticité selon les standards IETF.

> **Note sur la S-box :** La S-box cubique `x³ + cx` ne s'applique qu'aux petits premiers (p < 100). Pour p ≈ 2⁶⁴ (cas de production), le fallback `x^d` (généralement d=3) est systématiquement utilisé. La sécurité finale repose sur ChaCha20-Poly1305 en couche externe. Voir la section [Architecture interne](#architecture-interne) pour les détails.

---

## Nouveautés v1.6.0

La v1.6.0 est la version de stabilisation post-benchmarking. Elle apporte des optimisations mesurées expérimentalement via [cagoule-bench](https://github.com/slimissa/cagoule-bench) et corrige la documentation de la S-box.

### Performance mesurée (cagoule-bench v1.0.0, x86_64, Python 3.12.3)

| Opération | Algorithme | Throughput | Temps moyen |
|-----------|------------|------------|-------------|
| Encrypt 1MB | **CAGOULE** | **0.6 MB/s** | **1714 ms** |
| Decrypt 1MB | **CAGOULE** | **0.1 MB/s** | **13 347 ms** |
| Encrypt 1MB | AES-256-GCM | 3 711 MB/s | 0.27 ms |
| Decrypt 1MB | AES-256-GCM | 3 797 MB/s | 0.26 ms |
| Encrypt 1MB | ChaCha20-Poly1305 | 1 812 MB/s | 0.55 ms |
| Decrypt 1MB | ChaCha20-Poly1305 | 864 MB/s | 1.16 ms |

CAGOULE est **~6 000× plus lent qu'AES en chiffrement** et **~38 000× en déchiffrement**. Ce surcoût est attendu pour une implémentation Python pure d'un schéma algébrique expérimental. Le portage C (Phase 2) est la prochaine étape.

### Nouvelles fonctionnalités

- **`CagouleParams.derive_for_benchmark()`** — Factory avec cache opt-in pour les benchmarks. Dérive les paramètres une seule fois avec un sel fixe, évitant 150 ré-dérivations Argon2id. Gain mesuré : ×4 à ×60 selon la configuration.
- **`CagouleParams.clear_benchmark_cache()`** — Vide et zéroïse le cache de benchmark. À appeler en fin de session.
- **`CagouleParams.__reduce__()`** — Support pickle pour compatibilité `ProcessPoolExecutor`. Permet d'utiliser CAGOULE dans des suites de benchmarks parallèles.

### Corrections documentaires

- **S-box clarifiée** — `sbox.py` documente explicitement que `x^d` (fallback) est systématiquement actif pour p ≥ 100, donc pour tout p de production (p ≈ 2⁶⁴). La S-box cubique `x³ + cx` est réservée aux tests sur petits premiers. Le diagramme d'architecture est mis à jour en conséquence.
- `find_valid_c()` retourne directement `(None, -1)` pour p ≥ 100 sans itérer inutilement.

### Correctifs v1.5.x portés

- Imports absolus → relatifs dans 6 modules (`ImportError` corrigé à l'installation pip)
- Cache `_FALLBACK_CACHE` dans `sbox.py` — `pow(d, -1, p-1)` calculé une fois (×40 plus rapide)
- Paramètre `params` optionnel dans `decrypt()` — évite la re-dérivation KDF (−96%)
- Déroulage de boucle `matrix_vec_mul_mod_optimized` pour n=16 (−40% déchiffrement)

---

## Installation

```bash
pip install cagoule
```

### Dépendances

| Paquet | Version | Rôle |
|--------|---------|------|
| `cryptography` | ≥ 42.0 | ChaCha20-Poly1305, HKDF-SHA256, Scrypt |
| `mpmath` | ≥ 1.3 | Précision arbitraire pour ζ(2n) |
| `argon2-cffi` | ≥ 23.0 | KDF Argon2id — fallback Scrypt si absent |

---

## Utilisation rapide

### API Python

```python
from cagoule import encrypt, decrypt, CagouleAuthError

# Chiffrement
ciphertext = encrypt(b"Message secret", b"mon_mot_de_passe")

# Déchiffrement
plaintext = decrypt(ciphertext, b"mon_mot_de_passe")
assert plaintext == b"Message secret"

# Mauvais mot de passe → exception typée
try:
    decrypt(ciphertext, b"mauvais")
except CagouleAuthError:
    print("Authentification échouée")
```

### API avancée — réutilisation des paramètres

```python
from cagoule.params import CagouleParams
from cagoule import encrypt, decrypt

# Dériver les paramètres une seule fois (Argon2id ~300 ms)
with CagouleParams.derive(b"password", fast_mode=False) as params:
    ct1 = encrypt(b"Message 1", b"password", params=params)
    ct2 = encrypt(b"Message 2", b"password", params=params)
    pt1 = decrypt(ct1, b"password", params=params)
    pt2 = decrypt(ct2, b"password", params=params)
# params.zeroize() est appelé automatiquement ici
```

### API benchmark — cache opt-in

```python
from cagoule.params import CagouleParams
from cagoule import encrypt, decrypt

# ⚠️ UNIQUEMENT pour les benchmarks et tests — clés non zéroïsées en mémoire
BENCH_SALT = b'\xca\xf0' * 16  # 32 octets fixes pour reproductibilité

params = CagouleParams.derive_for_benchmark(
    b"benchmark_password",
    fast_mode=False,
    salt=BENCH_SALT,
)

# Chiffrer/déchiffrer 1000× sans re-dériver les paramètres
for _ in range(1000):
    ct = encrypt(plaintext, b"benchmark_password", params=params)

# Nettoyer en fin de session
CagouleParams.clear_benchmark_cache()
```

### Nettoyage mémoire sécurisé

```python
from cagoule.utils import secure_zeroize, SensitiveBuffer

key = bytearray(b"clé_secrète")
secure_zeroize(key)

with SensitiveBuffer(32) as buf:
    buf[:] = os.urandom(32)
    # utiliser buf...
# buf est zéroïsé automatiquement
```

### Logging

```bash
CAGOULE_LOG_LEVEL=DEBUG python3 mon_script.py
cagoule encrypt fichier.txt -p "mdp" --verbose
```

### CLI

```bash
cagoule encrypt secret.txt -p "MonMotDePasse"
cagoule decrypt secret.txt.cgl1 -p "MonMotDePasse"
cagoule inspect secret.txt.cgl1
cagoule bench -n 3
cagoule version
```

---

## Architecture interne

### Paramètres dérivés depuis le mot de passe

| Paramètre | Dérivation | Rôle |
|-----------|-----------|------|
| `n` | HKDF → [4, 65536] | Paramètre secret de ζ(2n) |
| `p` | HKDF → nextprime(≈2⁶⁴) | Corps de travail Z/pZ |
| `µ` | Racine de x⁴+x²+1 | Nœud de la matrice Vandermonde |
| `d` | HKDF → S-box x^d | Exposant de confusion (fallback) |
| `K_stream` | HKDF → 256 bits | Clé ChaCha20 |
| Round keys | ζ(2n) → Fourier → HKDF | 64 clés de ronde mod p |

### S-box : comportement réel

La S-box `x³ + cx mod p` est théoriquement définie mais **inopérante pour p ≥ 100** car la vérification de bijectivité est désactivée à ce seuil. En production avec p ≈ 2⁶⁴, le fallback `x^d` est **systématiquement actif** :

```
p < 100   → S-box cubique x³ + cx (tests sur petits premiers uniquement)
p ≥ 100   → Fallback x^d, d = plus petit impair avec gcd(d, p-1) = 1
             Typiquement d = 3 → f(x) = x³ mod p
```

`x^d` est bijectif sur Z/pZ (Fermat), et `d_inv = d⁻¹ mod (p-1)` est mis en cache une fois. La faiblesse connue de `x^d` (homomorphisme multiplicatif `f(ax) = aᵈ·f(x)`) est mitigée par la couche ChaCha20-Poly1305 en couche externe.

### Génération de µ

| Stratégie | Condition | Méthode |
|-----------|-----------|---------|
| **A** | p ≡ 1 mod 3 | Tonelli-Shanks dans Z/pZ sur x²±x+1=0 |
| **C** | p ≡ 2 mod 3 | µ = t dans Fp² = Z/pZ[t]/(t²+t+1), avec t³=1 ✓ |

### Constantes mathématiques CGS2025

| Constante | Formule | Valeur |
|-----------|---------|--------|
| ρ | (1+√5)/2 | 1.61803398874989... |
| β | (8π/81)(55ρ+34) | 34.21... |
| **Ω = ζ(8)** | **π⁸/9450** | **1.00407735619794...** |
| x₀ | 3π | 9.42477796076937... |
| δ | \|(Z/13Z)*\| | 12 |

---

## Format binaire CGL1

| Offset | Taille | Champ |
|--------|--------|-------|
| 0–3 | 4 oct. | Magic `CGL1` |
| 4 | 1 oct. | Version `0x01` |
| 5–36 | 32 oct. | Salt (inclus dans AAD) |
| 37–48 | 12 oct. | Nonce ChaCha20 |
| 49–N | variable | Ciphertext |
| N–N+16 | 16 oct. | Tag Poly1305 |

**Overhead fixe : 65 octets.**

---

## Structure du projet

```
cagoule/
├── pyproject.toml
├── README.md
├── scripts/
│   └── benchmark.py
├── tests/
│   ├── test_cipher.py
│   ├── test_format.py
│   ├── test_fp2.py
│   ├── test_kat.py
│   ├── test_matrix.py
│   ├── test_mu.py
│   ├── test_nist.py
│   └── test_sbox.py
├── run_tests.py
└── cagoule/
    ├── __init__.py
    ├── cipher.py
    ├── cli.py
    ├── constants.py
    ├── decipher.py
    ├── format.py
    ├── fp2.py
    ├── kat_vectors.json
    ├── logger.py
    ├── matrix.py
    ├── mu.py
    ├── omega.py
    ├── params.py       ← derive_for_benchmark(), clear_benchmark_cache(), __reduce__()
    ├── sbox.py         ← fallback x^d documenté, find_valid_c() optimisé
    └── utils.py
```

---

## Tests

```bash
python3 run_tests.py

pytest --cov=cagoule

pytest tests/test_nist.py -v

python3 scripts/benchmark.py --quick --no-kdf
```

### Résultats v1.6.0

```
════════════════════════════════════════════════════════════
  CAGOULE v1.6.0 — Résultats des tests
════════════════════════════════════════════════════════════
  ✅ 247 passés   ❌ 0 échoués   Total : 247   ⏱ ~1.1s
════════════════════════════════════════════════════════════
```

---

## Sécurité

### Garanties
- **KDF mémoire-dur** : Argon2id (t=3, m=64 MB, p=1) — résistant aux attaques GPU/ASIC.
- **AEAD** : ChaCha20-Poly1305 RFC 8439 — confidentialité + authenticité.
- **Nettoyage mémoire** : `secure_zeroize()` + `ctypes.memset` sur K_master, K_stream et round keys.
- **Validation statistique** : 14 tests NIST SP 800-22 passés sur la sortie ChaCha20.

### Limites connues
- La S-box `x^d` sur Z/pZ possède l'homomorphisme multiplicatif `f(ax) = aᵈ·f(x)`. Cela est intentionnellement mitigé par ChaCha20-Poly1305.
- La couche Z/pZ n'a pas été auditée par un cryptographe indépendant. Usage académique exclusivement.
- En Python, les objets `str` et `bytes` sont immuables — le GC peut conserver des copies de mots de passe.
- CAGOULE Python pur est ~6 000× plus lent qu'AES-256-GCM. Pas adapté aux flux de données en temps réel.

---

## Performances attendues

Ces mesures ont été produites par [cagoule-bench v1.0.0](https://github.com/slimissa/cagoule-bench) sur x86_64 / Python 3.12.3 / Ubuntu, paramètres pré-dérivés avec `derive_for_benchmark()`.

```
┌──────────────────────────────────────────────────────────────────────┐
│  cagoule-bench v1.0.0 — x86_64 / Python 3.12.3                      │
├──────────────────────────────────────────────────────────────────────┤
│  ENCRYPTION BENCHMARK (1 MB, 3 itérations, params pré-dérivés)       │
│  CAGOULE encrypt    ~0.6 MB/s   1714 ms  ±7 ms    196 MB RAM         │
│  CAGOULE decrypt    ~0.1 MB/s  13347 ms  ±636 ms   81 MB RAM         │
│  AES-256-GCM enc  ~3711 MB/s   0.269 ms  ±0.01 ms   2 MB RAM         │
│  ChaCha20 enc     ~1812 MB/s   0.552 ms  ±0.01 ms   2 MB RAM         │
│                                                                        │
│  Overhead CAGOULE vs AES    : ~−100% (couche algébrique Python pur)  │
│  Ratio déchiffrement/chiffr.: 7.8× (S-box inverse + diffusion inv.)  │
└──────────────────────────────────────────────────────────────────────┘
```

Le ratio 7.8× entre déchiffrement et chiffrement est une anomalie identifiée. Les causes probables sont le coût de `apply_inverse()` (16 777 216 multiplications modulaires pour 1MB) et de `pow(y, d_inv, p)` appelé une fois par octet de plaintext. L'optimisation de ces deux points est la priorité haute pour v1.7.0.

---

## Roadmap

| Phase | Description | Statut |
|-------|-------------|--------|
| Phase 1 | Implémentation Python complète | ✅ Terminé (v1.6.0) |
| Phase 2 | Portage C des parties critiques (Z/pZ, S-box, matrice) | 🔜 En cours |
| Phase 3 | LLVM IR via QLang | 🗓 Planifié |
| Phase 4 | Intégration QuantOS | 🗓 Planifié |

---

## Applications

- **[cagoule-pass](https://github.com/slimissa/cagoule-pass)** — Gestionnaire de mots de passe CLI chiffré avec CAGOULE
- **[cagoule-bench](https://github.com/slimissa/cagoule-bench)** — Suite de benchmarking académique

---

## Changelog

### v1.6.0 (Avril 2026) — Stabilisation post-benchmarking

**Nouvelles API**
- `CagouleParams.derive_for_benchmark()` — cache opt-in pour benchmarks, gain ×4 à ×60
- `CagouleParams.clear_benchmark_cache()` — nettoyage explicite du cache
- `CagouleParams.__reduce__()` + `_reconstruct()` — support pickle / ProcessPoolExecutor

**Corrections**
- `sbox.py` : documentation corrigée — `x^d` est la S-box effective en production, pas `x³+cx`
- `find_valid_c()` : retour anticipé `(None, -1)` pour p ≥ 100
- Diagramme d'architecture mis à jour (S-box `w^d` au lieu de `w³+cw`)

### v1.5.0 (Avril 2026) — Version finale Phase 1
- Cache `_FALLBACK_CACHE` dans `sbox.py` (×40 S-box inverse)
- Paramètre `params` dans `decrypt()` (−96% KDF)
- 14 tests NIST SP 800-22
- `secure_zeroize()`, `SensitiveBuffer`, logging structuré
- KAT v1.5 mis à jour

### v1.2.x / v1.1.0 (Avril 2026)
- Publication PyPI, CI GitHub Actions, 162 tests de base

---

## Auteur

**Slim Issa** — [github.com/slimissa](https://github.com/slimissa)

Projet académique inspiré des mathématiques du Concours Général Sénégalais 2025.

---

## Licence

MIT — voir [LICENSE](LICENSE)