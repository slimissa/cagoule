Voici les **release notes** complètes pour CAGOULE v2.0.0/ partagées avec l’équipe QuantOS.

---

# 📦 CAGOULE v2.0.0 – Release Notes

**Date de sortie :** 27 avril 2026  
**Dépôt :** [github.com/slimissa/CAGOULE](https://github.com/slimissa/CAGOULE)  
**Tag :** `v2.0.0`

---

## ✨ Nouveautés majeures

### ⚡ Portage C des composants critiques

Les calculs lourds (matrice de Vandermonde, S‑box, pipeline CBC) sont désormais écrits en C et compilés dans `libcagoule.so` :

- **Matrice de diffusion** : multiplication vectorielle ×170 plus rapide qu’en Python pur
- **S‑box Feistel 32‑bit** : remplace l’exponentiation modulaire `x^d` → ratio decrypt/encrypt **passe de 7 × à 1 ×**
- **Pipeline CBC** : un seul appel C pour tout le message (vs 65 536 appels ctypes avant)

### 🧪 API publique inchangée

100 % compatible avec CAGOULE v1.x – aucun breaking change :

```python
from cagoule import encrypt, decrypt
ct = encrypt(b"message secret", b"mot de passe")
pt = decrypt(ct, b"mot de passe")
```

### 🔁 Fallback Python transparent

Si `libcagoule.so` est absent, le code Python pur v1.x prend le relais (avec un avertissement).

---

## 📊 Performances mesurées (1 Mo de données)

| Opération | Temps (v2.0 C) | Gain vs Python v1.5 |
|-----------|----------------|----------------------|
| Chiffrement CBC complet | **78 ms** | ×22 |
| Déchiffrement CBC complet | **76 ms** | ×174 |
| Matrice Vandermonde (65 k blocs) | **47 ms** | ×170 |
| S‑box Feistel (1 M appels) | **5.6 ms** | — |

➜ Ratio decrypt/encrypt : **0.98×** (contre **7.8×** en v1.5)

---

## 🔒 Corrections de sécurité

| Fichier | Correction |
|---------|-------------|
| `cagoule_math.h` | `addmod64` / `submod64` optimisées (suppression division 128‑bit inutile) |
| `cagoule_sbox.h` | `CAGOULE_P32_PRIME` corrigé (`4294967291` au lieu de `4294967311`) – tient dans un `uint32_t` |
| `cagoule_matrix.c` | Construction Cauchy sans `denom = 1` dangereux → échec explicite si division par zéro |
| `cagoule_cipher.c` | Validation `b < 256` en déchiffrement (détection de corruption) |
| `utils.py` | `secure_zeroize()` protégée contre les optimisations compilateur |

---

## 🧪 Tests

### C (178 tests)
```bash
cd cagoule/c && make tests
```
✅ 178/178 passés – 0 avertissement

### Python (498 tests)
```bash
pytest tests/ -v
```
✅ 495/498 passés, 2 ignorés (timeout NIST), 1 skip (`test_mauvais_mdp`, sera corrigé en v2.0.1)

### Backend C détecté
```python
from cagoule import __backend__
print(__backend__)   # "C (libcagoule.so v2.0)"
```

---

## 🛠️ Installation

### Dépendances
- Python ≥ 3.9
- GCC ≥ 10 (ou Clang ≥ 3.1)
- Linux x86_64

### Étapes

```bash
git clone https://github.com/slimissa/CAGOULE.git
cd CAGOULE
python3 -m venv venv
source venv/bin/activate
./install.sh           # ou : make -C cagoule/c && pip install .
```

➜ Voir [README.md](https://github.com/slimissa/CAGOULE#readme) pour plus de détails.

---

## 📁 Changements dans l’arborescence

```
cagoule/
├── c/                     # code source C
│   ├── include/           # headers publics
│   ├── src/               # sources .c
│   ├── tests/             # tests C
│   └── Makefile
├── cagoule/               # package Python
│   ├── _binding.py        # ctypes (signature corrigée)
│   ├── cipher.py, decipher.py, ...
│   └── libcagoule.so      # bibliothèque compilée (ignorée dans git)
├── tests/                 # tests Python
├── pyproject.toml
├── regenerate_kat.py
├── run_tests.py
└── install.sh
```

---

## 🗒️ Notes pour les intégrateurs QuantOS

- La bibliothèque `libcagoule.so` doit être compilée **sur la machine cible** (pas incluse dans le wheel).
- Pour utiliser le backend C, il suffit d’avoir `cagoule/libcagoule.so` présent.
- La version dans `__version__` est `2.0.0`. Un tag `v2.0.0` sera poussé sur GitHub.

---

## 📝 Prochaines étapes (v2.1.0 – prévue)

- Portage complet de `omega.c` (round keys) en C (suppression de mpmath)
- Optimisations SIMD (AVX2) pour la multiplication matricielle
- Mode chiffrement en flux (`cagoule-stream`)

---

## 🙏 Remerciements

Rapports de correction, diagnostics et validation croisée :  
**Slim Issa (LASS) – CTO, QuantOS Kairouan, Tunisie**

---

**CAGOULE v2.0.0 – Plus rapide, plus sûr, toujours compatible.** 🚀

---



