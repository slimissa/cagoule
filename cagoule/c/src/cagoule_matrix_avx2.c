/**
 * cagoule_matrix_avx2.c — Multiplication Vandermonde vectorisée AVX2
 *                          CAGOULE v2.5.0
 *
 * Nouveautés v2.5.0 :
 *
 *   A. Mersenne dispatch :
 *      Si CagouleMatrix.k_mersenne > 0 → mulmod_mersenne64x4.
 *      Sinon → mulmod64x4 Barrett (compatibilité tests scalaires).
 *      Gain Mersenne : ~13 instructions/mul vs ~22 Barrett (−41%).
 *
 *   B. Option A — Double accumulateur indépendant :
 *      Passe A : j pair   (0,2,4,...,14) → acc0a..acc3a, profondeur 8.
 *      Passe B : j impair (1,3,5,...,15) → acc0b..acc3b, profondeur 8.
 *      Merge   : result = addmod(accXa, accXb).
 *      Budget registres : 4 acc actifs + p,k,FLIP (3 consts) + ~6 temps Mersenne
 *                       = ~13 YMM dans les 16 disponibles. Zéro spill critique.
 *
 *   Résultats bit-à-bit identiques au scalaire — validés par test_matrix_avx2.c.
 */

#include "cagoule_matrix.h"
#include "cagoule_math.h"

#if defined(__AVX2__)

#include "cagoule_math_avx2.h"
#include <immintrin.h>

/* ── Macros d'accumulation ────────────────────────────────────────── */

#define ACCUM_MERSENNE(a0,a1,a2,a3, mat,j_, pv,kv) do {           \
    __m256i _vj = _mm256_set1_epi64x((int64_t)v[(j_)]);           \
    (a0) = addmod64x4((a0), mulmod_mersenne64x4(                   \
        _mm256_loadu_si256((const __m256i*)&(mat)[0][(j_)*4]),     \
        _vj, pv, kv), pv);                                         \
    (a1) = addmod64x4((a1), mulmod_mersenne64x4(                   \
        _mm256_loadu_si256((const __m256i*)&(mat)[1][(j_)*4]),     \
        _vj, pv, kv), pv);                                         \
    (a2) = addmod64x4((a2), mulmod_mersenne64x4(                   \
        _mm256_loadu_si256((const __m256i*)&(mat)[2][(j_)*4]),     \
        _vj, pv, kv), pv);                                         \
    (a3) = addmod64x4((a3), mulmod_mersenne64x4(                   \
        _mm256_loadu_si256((const __m256i*)&(mat)[3][(j_)*4]),     \
        _vj, pv, kv), pv);                                         \
} while(0)

#define ACCUM_BARRETT(a0,a1,a2,a3, mat,j_, pv,mu_) do {           \
    __m256i _vj = _mm256_set1_epi64x((int64_t)v[(j_)]);           \
    (a0) = addmod64x4((a0), mulmod64x4(                            \
        _mm256_loadu_si256((const __m256i*)&(mat)[0][(j_)*4]),     \
        _vj, pv, mu_), pv);                                        \
    (a1) = addmod64x4((a1), mulmod64x4(                            \
        _mm256_loadu_si256((const __m256i*)&(mat)[1][(j_)*4]),     \
        _vj, pv, mu_), pv);                                        \
    (a2) = addmod64x4((a2), mulmod64x4(                            \
        _mm256_loadu_si256((const __m256i*)&(mat)[2][(j_)*4]),     \
        _vj, pv, mu_), pv);                                        \
    (a3) = addmod64x4((a3), mulmod64x4(                            \
        _mm256_loadu_si256((const __m256i*)&(mat)[3][(j_)*4]),     \
        _vj, pv, mu_), pv);                                        \
} while(0)

#define STORE4(vec, out, base) do {                                 \
    uint64_t _t[4];                                                 \
    _mm256_storeu_si256((__m256i*)_t, (vec));                       \
    (out)[(base)+0]=_t[0];(out)[(base)+1]=_t[1];                   \
    (out)[(base)+2]=_t[2];(out)[(base)+3]=_t[3];                   \
} while(0)

/* ── cagoule_matrix_mul_avx2 — Mersenne + Option A ─────────────────  */
void cagoule_matrix_mul_avx2(const CagouleMatrix* m,
                               const uint64_t v[CAGOULE_N],
                               uint64_t out[CAGOULE_N])
{
    const uint64_t p = m->p;
    __m256i p_vec = _mm256_set1_epi64x((int64_t)p);

    __m256i a0a,a1a,a2a,a3a;   /* Passe A — j pair  */
    __m256i a0b,a1b,a2b,a3b;   /* Passe B — j impair */
    a0a=a1a=a2a=a3a=_mm256_setzero_si256();
    a0b=a1b=a2b=a3b=_mm256_setzero_si256();

    if (m->k_mersenne > 0) {
        __m256i k_vec = _mm256_set1_epi64x((int64_t)m->k_mersenne);
        for (int j = 0; j < CAGOULE_N; j += 2)
            ACCUM_MERSENNE(a0a,a1a,a2a,a3a, m->fwd_avx2, j, p_vec, k_vec);
        for (int j = 1; j < CAGOULE_N; j += 2)
            ACCUM_MERSENNE(a0b,a1b,a2b,a3b, m->fwd_avx2, j, p_vec, k_vec);
    } else {
        uint64_t mu = cagoule_barrett_mu(p);
        for (int j = 0; j < CAGOULE_N; j += 2)
            ACCUM_BARRETT(a0a,a1a,a2a,a3a, m->fwd_avx2, j, p_vec, mu);
        for (int j = 1; j < CAGOULE_N; j += 2)
            ACCUM_BARRETT(a0b,a1b,a2b,a3b, m->fwd_avx2, j, p_vec, mu);
    }

    STORE4(addmod64x4(a0a,a0b,p_vec), out,  0);
    STORE4(addmod64x4(a1a,a1b,p_vec), out,  4);
    STORE4(addmod64x4(a2a,a2b,p_vec), out,  8);
    STORE4(addmod64x4(a3a,a3b,p_vec), out, 12);
    _mm256_zeroupper();
}

/* ── cagoule_matrix_mul_inv_avx2 — identique, matrice inverse ─────── */
void cagoule_matrix_mul_inv_avx2(const CagouleMatrix* m,
                                   const uint64_t v[CAGOULE_N],
                                   uint64_t out[CAGOULE_N])
{
    const uint64_t p = m->p;
    __m256i p_vec = _mm256_set1_epi64x((int64_t)p);

    __m256i a0a,a1a,a2a,a3a;
    __m256i a0b,a1b,a2b,a3b;
    a0a=a1a=a2a=a3a=_mm256_setzero_si256();
    a0b=a1b=a2b=a3b=_mm256_setzero_si256();

    if (m->k_mersenne > 0) {
        __m256i k_vec = _mm256_set1_epi64x((int64_t)m->k_mersenne);
        for (int j = 0; j < CAGOULE_N; j += 2)
            ACCUM_MERSENNE(a0a,a1a,a2a,a3a, m->inv_avx2, j, p_vec, k_vec);
        for (int j = 1; j < CAGOULE_N; j += 2)
            ACCUM_MERSENNE(a0b,a1b,a2b,a3b, m->inv_avx2, j, p_vec, k_vec);
    } else {
        uint64_t mu = cagoule_barrett_mu(p);
        for (int j = 0; j < CAGOULE_N; j += 2)
            ACCUM_BARRETT(a0a,a1a,a2a,a3a, m->inv_avx2, j, p_vec, mu);
        for (int j = 1; j < CAGOULE_N; j += 2)
            ACCUM_BARRETT(a0b,a1b,a2b,a3b, m->inv_avx2, j, p_vec, mu);
    }

    STORE4(addmod64x4(a0a,a0b,p_vec), out,  0);
    STORE4(addmod64x4(a1a,a1b,p_vec), out,  4);
    STORE4(addmod64x4(a2a,a2b,p_vec), out,  8);
    STORE4(addmod64x4(a3a,a3b,p_vec), out, 12);
    _mm256_zeroupper();
}

#undef ACCUM_MERSENNE
#undef ACCUM_BARRETT
#undef STORE4

#endif /* __AVX2__ */
