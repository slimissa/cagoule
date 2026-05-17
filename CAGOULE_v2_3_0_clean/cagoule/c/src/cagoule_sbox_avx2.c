/**
 * cagoule_sbox_avx2.c — S-box Feistel vectorisée AVX2 — CAGOULE v2.3.0
 *
 * Implémente cagoule_sbox_block_forward_avx2 / _inverse_avx2
 * traitant CAGOULE_N (16) éléments en 4 passes de 4 lanes.
 *
 * Algorithme : voir cagoule_sbox_avx2.h
 *
 * Gains attendus (mesurés sur Intel Skylake) :
 *   S-box scalaire  : ~20 ms / MB
 *   S-box AVX2      : ~5  ms / MB   (+75%, ×4 théorique)
 *
 * Ce fichier est compilé avec -mavx2 séparément du reste (Makefile).
 * Les fonctions sont appelées depuis cagoule_cipher.c via dispatch runtime.
 */

#include "cagoule_sbox.h"
#include "cagoule_math.h"

#if defined(__AVX2__)
#include <immintrin.h>
#include "cagoule_sbox_avx2.h"

#define N CAGOULE_N   /* 16 */

/* ── cagoule_sbox_block_forward_avx2 ────────────────────────────────
 *
 * Applique la S-box Feistel forward à n éléments (n doit être multiple
 * de 4 pour le chemin vectoriel ; le reste est traité scalaire).
 */
void cagoule_sbox_block_forward_avx2(const CagouleSBox64* s,
                                      const uint64_t* in,
                                      uint64_t* out,
                                      size_t n)
{
    /* _sbox_block_forward_hot_avx2 traite les groupes de 4 avec broadcasts
     * hoistés et sans zeroupper interne — le zeroupper final est ici,
     * émis UNE SEULE FOIS pour l'ensemble de l'appel.
     * Quand cette fonction est appelée depuis cagoule_cbc_encrypt, la version
     * cipher.c appellera directement _sbox_block_forward_hot_avx2 pour éviter
     * le zeroupper inter-blocs. */
    _sbox_block_forward_hot_avx2(s, in, out, n);
    _mm256_zeroupper();
}

/* ── cagoule_sbox_block_inverse_avx2 ────────────────────────────────
 *
 * Applique la S-box Feistel inverse à n éléments.
 */
void cagoule_sbox_block_inverse_avx2(const CagouleSBox64* s,
                                      const uint64_t* in,
                                      uint64_t* out,
                                      size_t n)
{
    _sbox_block_inverse_hot_avx2(s, in, out, n);
    _mm256_zeroupper();
}

/* ── Détection runtime (exposé à cagoule_cipher.c) ────────────────── */
/* Délègue à cagoule_matrix_backend_is_avx2() qui utilise _avx2_available()
 * (lazy-init, thread-safe via __atomic) — cohérence avec v2.2.0 */
extern int cagoule_matrix_backend_is_avx2(void);
int cagoule_sbox_backend_is_avx2(void) {
    return cagoule_matrix_backend_is_avx2();
}

#else  /* Fallback si AVX2 non disponible à la compilation */

void cagoule_sbox_block_forward_avx2(const CagouleSBox64* s,
                                      const uint64_t* in,
                                      uint64_t* out,
                                      size_t n)
{
    for (size_t i = 0; i < n; i++)
        out[i] = cagoule_sbox_forward(s, in[i]);
}

void cagoule_sbox_block_inverse_avx2(const CagouleSBox64* s,
                                      const uint64_t* in,
                                      uint64_t* out,
                                      size_t n)
{
    for (size_t i = 0; i < n; i++)
        out[i] = cagoule_sbox_inverse(s, in[i]);
}

int cagoule_sbox_backend_is_avx2(void) { return 0; }

#endif /* __AVX2__ */
