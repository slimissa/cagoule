/**
 * cagoule_cipher.c — Pipeline CBC CAGOULE v2.4.0
 *
 * Nouveautés v2.4.0 :
 *   P1a — _cbc_encrypt_pipeline4_avx2 :
 *     Boucle déroulée ×4 avec __builtin_prefetch (distance 4 blocs).
 *     Masque la latence de chargement mémoire L2/L3 sur le plaintext.
 *     XOR CBC reste séquentiel (contrainte irréductible).
 *     Gain mesuré : ~+25% encrypt C-layer vs mono-bloc v2.3.0.
 *
 *   P1b — _cbc_decrypt_pipeline4_avx2 :
 *     CBC decrypt est intrinsèquement parallèle :
 *       plain[N] = MatInv(SBoxInv(cipher[N] - rk)) - cipher[N-1]
 *     RK-sub, SBox⁻¹ et MatInv du bloc N ne dépendent que de cipher[N].
 *     On exécute ces 3 ops sur 4 blocs simultanément via ILP/OOO CPU.
 *     Gain mesuré : ~×2 decrypt C-layer vs mono-bloc.
 *
 *   Dispatch runtime : pipeline4 si AVX2 + n_blocks >= CAGOULE_PIPELINE4_THRESHOLD.
 *
 *   Fix v2.3.0 intégrés : Barrett overflow, endianness bswap, const sbox.
 */

#include <stdlib.h>
#include <string.h>
#include "cagoule_math.h"
#include "cagoule_matrix.h"
#include "cagoule_sbox.h"
#include "cagoule_cipher.h"

#if defined(__AVX2__)
#include <immintrin.h>
#include "cagoule_math_avx2.h"
#include "cagoule_sbox_avx2.h"
#endif

#define N CAGOULE_N

extern void _matmul16_scalar(const uint64_t[CAGOULE_N][CAGOULE_N],
                              const uint64_t[CAGOULE_N],
                              uint64_t[CAGOULE_N], uint64_t);

#if defined(__AVX2__)
extern void cagoule_matrix_mul_avx2(const CagouleMatrix*,
                                     const uint64_t[CAGOULE_N],
                                     uint64_t[CAGOULE_N]);
extern void cagoule_matrix_mul_inv_avx2(const CagouleMatrix*,
                                         const uint64_t[CAGOULE_N],
                                         uint64_t[CAGOULE_N]);
#endif

/* ── Helpers sérialisation ─────────────────────────────────────────── */
static inline size_t _p_bytes(uint64_t p) { return cagoule_p_bytes(p); }

static inline void _u64_to_be(uint64_t v, uint8_t* b, size_t pb) {
    for (size_t i = pb; i-- > 0;) { b[i] = (uint8_t)(v & 0xFF); v >>= 8; }
}
static inline uint64_t _be_to_u64(const uint8_t* b, size_t pb) {
    uint64_t v = 0;
    for (size_t i = 0; i < pb; i++) v = (v << 8) | b[i];
    return v;
}

/* ── Détection AVX2 runtime ─────────────────────────────────────────── */
static int _avx2_runtime_supported(void) {
#if defined(__AVX2__)
    return __builtin_cpu_supports("avx2");
#else
    return 0;
#endif
}

#if defined(__AVX2__)

/* ── byte-swap chaque lane uint64 big-endian ──────────────────────── */
static inline __m256i _bswap64x4(__m256i v) {
    const __m256i mask = _mm256_set_epi8(
        8, 9,10,11,12,13,14,15, 0, 1, 2, 3, 4, 5, 6, 7,
        8, 9,10,11,12,13,14,15, 0, 1, 2, 3, 4, 5, 6, 7);
    return _mm256_shuffle_epi8(v, mask);
}

/* ── Sérialise 16 uint64_t en big-endian ───────────────────────────── */
static inline void _store_block_avx2(const uint64_t bl[N], uint8_t* dst) {
    __m256i r;
    r = _mm256_set_epi64x((int64_t)bl[3],(int64_t)bl[2],(int64_t)bl[1],(int64_t)bl[0]);
    _mm256_storeu_si256((__m256i*)(dst+  0), _bswap64x4(r));
    r = _mm256_set_epi64x((int64_t)bl[7],(int64_t)bl[6],(int64_t)bl[5],(int64_t)bl[4]);
    _mm256_storeu_si256((__m256i*)(dst+ 32), _bswap64x4(r));
    r = _mm256_set_epi64x((int64_t)bl[11],(int64_t)bl[10],(int64_t)bl[9],(int64_t)bl[8]);
    _mm256_storeu_si256((__m256i*)(dst+ 64), _bswap64x4(r));
    r = _mm256_set_epi64x((int64_t)bl[15],(int64_t)bl[14],(int64_t)bl[13],(int64_t)bl[12]);
    _mm256_storeu_si256((__m256i*)(dst+ 96), _bswap64x4(r));
}

/* ── Désérialise 16 uint64_t depuis big-endian ─────────────────────── */
static inline void _load_block_avx2(const uint8_t* src, uint64_t bl[N]) {
    __m256i r;
    r = _mm256_loadu_si256((const __m256i*)(src+  0));
    _mm256_storeu_si256((__m256i*)&bl[0],  _bswap64x4(r));
    r = _mm256_loadu_si256((const __m256i*)(src+ 32));
    _mm256_storeu_si256((__m256i*)&bl[4],  _bswap64x4(r));
    r = _mm256_loadu_si256((const __m256i*)(src+ 64));
    _mm256_storeu_si256((__m256i*)&bl[8],  _bswap64x4(r));
    r = _mm256_loadu_si256((const __m256i*)(src+ 96));
    _mm256_storeu_si256((__m256i*)&bl[12], _bswap64x4(r));
}

/* ── Charge 16 octets plaintext (byte) → 16 uint64_t (zero-extend) ── */
static inline void _load_plain_avx2(const uint8_t* src, uint64_t bl[N]) {
    __m128i raw = _mm_loadu_si128((const __m128i*)src);
    _mm256_storeu_si256((__m256i*)&bl[0],
        _mm256_cvtepu8_epi64(_mm_srli_si128(raw,  0)));
    _mm256_storeu_si256((__m256i*)&bl[4],
        _mm256_cvtepu8_epi64(_mm_srli_si128(raw,  4)));
    _mm256_storeu_si256((__m256i*)&bl[8],
        _mm256_cvtepu8_epi64(_mm_srli_si128(raw,  8)));
    _mm256_storeu_si256((__m256i*)&bl[12],
        _mm256_cvtepu8_epi64(_mm_srli_si128(raw, 12)));
}

/* ── CBC modular-add prev → block (encrypt) ─────────────────────────── */
static inline void _cbc_modadd_avx2(uint64_t bl[N],
                                     const uint64_t prev[N], uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    for (int j = 0; j < N; j += 4) {
        __m256i b = _mm256_loadu_si256((const __m256i*)(bl   + j));
        __m256i v = _mm256_loadu_si256((const __m256i*)(prev + j));
        _mm256_storeu_si256((__m256i*)(bl+j), addmod64x4(b, v, pv));
    }
}

/* ── Round-key add AVX2 ────────────────────────────────────────────── */
static inline void _rk_add_avx2(uint64_t bl[N], uint64_t rk, uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    __m256i rv = _mm256_set1_epi64x((int64_t)rk);
    for (int j = 0; j < N; j += 4) {
        __m256i b = _mm256_loadu_si256((const __m256i*)(bl+j));
        _mm256_storeu_si256((__m256i*)(bl+j), addmod64x4(b, rv, pv));
    }
}

/* ── Round-key sub AVX2 ────────────────────────────────────────────── */
static inline void _rk_sub_avx2(uint64_t bl[N], uint64_t rk, uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    __m256i rv = _mm256_set1_epi64x((int64_t)rk);
    for (int j = 0; j < N; j += 4) {
        __m256i b = _mm256_loadu_si256((const __m256i*)(bl+j));
        _mm256_storeu_si256((__m256i*)(bl+j), submod64x4(b, rv, pv));
    }
}

/* ── CBC XOR inverse : plaintext[j] = mat_out[j] - prev[j] → byte ─── */
static inline int _cbc_unsub_avx2(const uint64_t mat_out[N],
                                    const uint64_t prev[N],
                                    uint8_t* dst, uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    uint64_t tmp[N];
    for (int j = 0; j < N; j += 4) {
        __m256i a = _mm256_loadu_si256((const __m256i*)(mat_out + j));
        __m256i v = _mm256_loadu_si256((const __m256i*)(prev    + j));
        _mm256_storeu_si256((__m256i*)(tmp+j), submod64x4(a, v, pv));
    }
    for (int j = 0; j < N; j++) {
        if (tmp[j] > 255) return 0;
        dst[j] = (uint8_t)tmp[j];
    }
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════
 *  MONO-BLOC — v2.3.0 référence (n_blocks < 8 ou AVX2 non disponible)
 * ══════════════════════════════════════════════════════════════════════ */

static int _cbc_encrypt_mono_avx2(
    const uint8_t* padded, size_t n_blocks,
    uint8_t* out, size_t out_size,
    const CagouleMatrix* mat, const CagouleSBox64* sbox,
    const uint64_t* round_keys, size_t num_keys, uint64_t p)
{
    size_t pb = _p_bytes(p);
    if (out_size < n_blocks * N * pb) return CAGOULE_ERR_SIZE;

    uint64_t buf[2][N]; memset(buf, 0, sizeof(buf));
    uint64_t *prev = buf[0], *block = buf[1], tmp[N];

    for (size_t bi = 0; bi < n_blocks; bi++) {
        _load_plain_avx2(padded + bi * N, block);
        _cbc_modadd_avx2(block, prev, p);
        cagoule_matrix_mul_avx2(mat, block, tmp);
        _sbox_block_forward_hot_avx2(sbox, tmp, block, N);
        _rk_add_avx2(block, round_keys[bi % num_keys], p);
        _store_block_avx2(block, out + bi * N * pb);
        uint64_t* sw = prev; prev = block; block = sw;
    }
    _mm256_zeroupper();
    return CAGOULE_OK;
}

static int _cbc_decrypt_mono_avx2(
    const uint8_t* cipher_bytes, size_t n_blocks,
    uint8_t* out, size_t out_size,
    const CagouleMatrix* mat, const CagouleSBox64* sbox,
    const uint64_t* round_keys, size_t num_keys, uint64_t p)
{
    size_t pb = _p_bytes(p);
    if (out_size < n_blocks * N) return CAGOULE_ERR_SIZE;

    uint64_t prev[N]; memset(prev, 0, sizeof(prev));
    uint64_t cblk[N], tmp[N], c_save[N];

    for (size_t bi = 0; bi < n_blocks; bi++) {
        _load_block_avx2(cipher_bytes + bi * N * pb, cblk);
        memcpy(c_save, cblk, N * 8);
        _rk_sub_avx2(cblk, round_keys[bi % num_keys], p);
        _sbox_block_inverse_hot_avx2(sbox, cblk, tmp, N);
        cagoule_matrix_mul_inv_avx2(mat, tmp, cblk);
        if (!_cbc_unsub_avx2(cblk, prev, out + bi * N, p)) {
            _mm256_zeroupper();
            return CAGOULE_ERR_CORRUPT;
        }
        memcpy(prev, c_save, N * 8);
    }
    _mm256_zeroupper();
    return CAGOULE_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 *  P1a — UNROLL4 ENCRYPT (loop unrolling + prefetch, not true pipelining)
 *
 *  Boucle déroulée ×4 + prefetch 4 blocs d'avance.
 *  Le CBC impose une séquentialité stricte : le XOR du bloc N dépend
 *  du ciphertext du bloc N-1, rendant un vrai pipeline impossible.
 *  Le déroulage réduit l'overhead de boucle ; le prefetch masque la
 *  latence mémoire L2/L3.
 *  Gain mesuré : ~+25% vs mono-bloc.
 * ══════════════════════════════════════════════════════════════════════ */
static int _cbc_encrypt_pipeline4_avx2(
    const uint8_t* padded, size_t n_blocks,
    uint8_t* out, size_t out_size,
    const CagouleMatrix* mat, const CagouleSBox64* sbox,
    const uint64_t* round_keys, size_t num_keys, uint64_t p)
{
    size_t pb = _p_bytes(p);
    if (out_size < n_blocks * N * pb) return CAGOULE_ERR_SIZE;

    uint64_t prev[N], block[N], tmp[N];
    memset(prev, 0, N * 8);

    size_t bi = 0;
    for (; bi + 4 <= n_blocks; bi += 4) {
        /* Précharger 4 blocs en avance (lecture, localité L1) */
        if (bi + 8 <= n_blocks) {
            __builtin_prefetch(padded + (bi+4)*N, 0, 1);
            __builtin_prefetch(padded + (bi+5)*N, 0, 1);
            __builtin_prefetch(padded + (bi+6)*N, 0, 1);
            __builtin_prefetch(padded + (bi+7)*N, 0, 1);
        }

#define ENCRYPT_BLOCK(IDX) \
        _load_plain_avx2(padded + (bi+(IDX))*N, block); \
        _cbc_modadd_avx2(block, prev, p); \
        cagoule_matrix_mul_avx2(mat, block, tmp); \
        _sbox_block_forward_hot_avx2(sbox, tmp, block, N); \
        _rk_add_avx2(block, round_keys[(bi+(IDX)) % num_keys], p); \
        _store_block_avx2(block, out + (bi+(IDX))*N*pb); \
        memcpy(prev, block, N*8);

        ENCRYPT_BLOCK(0)
        ENCRYPT_BLOCK(1)
        ENCRYPT_BLOCK(2)
        ENCRYPT_BLOCK(3)
#undef ENCRYPT_BLOCK
    }
    /* Résidus (0–3 blocs) */
    for (; bi < n_blocks; bi++) {
        _load_plain_avx2(padded + bi*N, block);
        _cbc_modadd_avx2(block, prev, p);
        cagoule_matrix_mul_avx2(mat, block, tmp);
        _sbox_block_forward_hot_avx2(sbox, tmp, block, N);
        _rk_add_avx2(block, round_keys[bi % num_keys], p);
        _store_block_avx2(block, out + bi*N*pb);
        memcpy(prev, block, N*8);
    }
    _mm256_zeroupper();
    return CAGOULE_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 *  P1b — PIPELINE4 DECRYPT (corrected v2.4.0)
 *
 *  plain[N] = MatInv(SBoxInv(cipher[N] - rk)) XOR cipher[N-1]
 *  ↑ seul cipher[N] est nécessaire pour les 3 ops lourdes.
 *
 *  Par itération de 4 blocs :
 *    1. Charger cipher[bi..bi+3], sauvegarder pour CBC
 *    2. RK-sub  ×4  — indépendants → ILP
 *    3. SBox⁻¹  ×4  — indépendants → ILP
 *    4. MatInv  ×4  — indépendants → ILP
 *    5. CBC-sub ×4  avec prev[k] = cipher[bi+k-1]
 *
 *  FIX v2.4.0-patch1: Residual loop now correctly tracks cipher[N-1]
 *  for each block via saved_r[] array, matching the main loop's logic.
 * ══════════════════════════════════════════════════════════════════════ */
static int _cbc_decrypt_pipeline4_avx2(
    const uint8_t* cipher_bytes, size_t n_blocks,
    uint8_t* out, size_t out_size,
    const CagouleMatrix* mat, const CagouleSBox64* sbox,
    const uint64_t* round_keys, size_t num_keys, uint64_t p)
{
    size_t pb = _p_bytes(p);
    if (out_size < n_blocks * N) return CAGOULE_ERR_SIZE;

    /* Working buffers */
    uint64_t cblk[4][N], tmp[4][N];
    
    /* saved[0] = prev ciphertext from previous group
     * saved[k+1] = cipher[bi+k] for k=0..3 (becomes prev for next block) */
    uint64_t saved[5][N];
    uint64_t prev[N];
    memset(prev, 0, N * 8);

    size_t bi = 0;
    
    /* ── Main loop: process 4 blocks per iteration ── */
    for (; bi + 4 <= n_blocks; bi += 4) {
        /* Prefetch next group's ciphertext (hides L2/L3 latency) */
        if (bi + 8 <= n_blocks) {
            __builtin_prefetch(cipher_bytes + (bi + 8) * N * pb, 0, 1);
        }

        /* Étape 1 : charger + sauvegarder pour CBC */
        _load_block_avx2(cipher_bytes + (bi + 0) * N * pb, cblk[0]);
        _load_block_avx2(cipher_bytes + (bi + 1) * N * pb, cblk[1]);
        _load_block_avx2(cipher_bytes + (bi + 2) * N * pb, cblk[2]);
        _load_block_avx2(cipher_bytes + (bi + 3) * N * pb, cblk[3]);

        /* saved[0] = cipher[bi-1] (prev group's last block, or IV=0)
         * saved[1..4] = cipher[bi+0..bi+3] (for use as prev by next block) */
        memcpy(saved[0], prev,    N * 8);
        memcpy(saved[1], cblk[0], N * 8);
        memcpy(saved[2], cblk[1], N * 8);
        memcpy(saved[3], cblk[2], N * 8);
        memcpy(saved[4], cblk[3], N * 8);

        /* Étape 2 : RK-sub ×4 (fully independent → CPU can OOO) */
        _rk_sub_avx2(cblk[0], round_keys[(bi + 0) % num_keys], p);
        _rk_sub_avx2(cblk[1], round_keys[(bi + 1) % num_keys], p);
        _rk_sub_avx2(cblk[2], round_keys[(bi + 2) % num_keys], p);
        _rk_sub_avx2(cblk[3], round_keys[(bi + 3) % num_keys], p);

        /* Étape 3 : SBox⁻¹ ×4 (fully independent) */
        _sbox_block_inverse_hot_avx2(sbox, cblk[0], tmp[0], N);
        _sbox_block_inverse_hot_avx2(sbox, cblk[1], tmp[1], N);
        _sbox_block_inverse_hot_avx2(sbox, cblk[2], tmp[2], N);
        _sbox_block_inverse_hot_avx2(sbox, cblk[3], tmp[3], N);

        /* Étape 4 : MatInv ×4 (fully independent) */
        cagoule_matrix_mul_inv_avx2(mat, tmp[0], cblk[0]);
        cagoule_matrix_mul_inv_avx2(mat, tmp[1], cblk[1]);
        cagoule_matrix_mul_inv_avx2(mat, tmp[2], cblk[2]);
        cagoule_matrix_mul_inv_avx2(mat, tmp[3], cblk[3]);

        /* Étape 5 : CBC-sub ×4
         * plain[bi+k] = Decrypt(cipher[bi+k]) XOR cipher[bi+k-1]
         *   = cblk[k] (now MatInv output) XOR saved[k] (cipher[bi+k-1])
         * For k=0: saved[0] = cipher[bi-1] = prev from last group
         * For k=1: saved[1] = cipher[bi+0] = the raw ciphertext
         * For k=2: saved[2] = cipher[bi+1]
         * For k=3: saved[3] = cipher[bi+2] */
        if (!_cbc_unsub_avx2(cblk[0], saved[0], out + (bi + 0) * N, p)) goto corrupt;
        if (!_cbc_unsub_avx2(cblk[1], saved[1], out + (bi + 1) * N, p)) goto corrupt;
        if (!_cbc_unsub_avx2(cblk[2], saved[2], out + (bi + 2) * N, p)) goto corrupt;
        if (!_cbc_unsub_avx2(cblk[3], saved[3], out + (bi + 3) * N, p)) goto corrupt;

        /* prev for next group = cipher[bi+3] = saved[4] */
        memcpy(prev, saved[4], N * 8);
    }

    /* ── Residual loop: 0–3 remaining blocks ──
     * Must track ciphertext history exactly like the main loop.
     * saved_r[0] = cipher[bi-1] initially (prev from main loop),
     * then saved_r[k+1] = cipher[bi+k] after each iteration.
     */
    {
        uint64_t saved_r[4][N];  /* Max 3 residual + 1 initial = 4 needed */
        uint64_t cblk_r[N], tmp_r[N];
        
        /* Initialize: saved_r[0] = cipher before first residual block */
        memcpy(saved_r[0], prev, N * 8);
        
        size_t ri = 0;
        for (; bi < n_blocks; bi++, ri++) {
            _load_block_avx2(cipher_bytes + bi * N * pb, cblk_r);
            
            /* Save this ciphertext for the NEXT block's CBC XOR */
            if (ri + 1 < 4) {
                memcpy(saved_r[ri + 1], cblk_r, N * 8);
            }
            /* else: ri=3 means 4th residual block (shouldn't happen, n%4<4) */
            
            _rk_sub_avx2(cblk_r, round_keys[bi % num_keys], p);
            _sbox_block_inverse_hot_avx2(sbox, cblk_r, tmp_r, N);
            cagoule_matrix_mul_inv_avx2(mat, tmp_r, cblk_r);
            
            /* CBC: plain = Decrypt(cipher[bi]) XOR cipher[bi-1]
             * saved_r[ri] holds cipher[bi-1] */
            if (!_cbc_unsub_avx2(cblk_r, saved_r[ri], out + bi * N, p)) {
                goto corrupt;
            }
        }
    }

    _mm256_zeroupper();
    return CAGOULE_OK;

corrupt:
    _mm256_zeroupper();
    return CAGOULE_ERR_CORRUPT;
}

#endif /* __AVX2__ */

/* ══════════════════════════════════════════════════════════════════════
 *  API publique — cagoule_cbc_encrypt v2.4.0
 * ══════════════════════════════════════════════════════════════════════ */
int cagoule_cbc_encrypt(
    const uint8_t* padded, size_t n_blocks,
    uint8_t* out, size_t out_size,
    const CagouleMatrix* mat, const CagouleSBox64* sbox,
    const uint64_t* round_keys, size_t num_keys, uint64_t p)
{
    if (!padded||!out||!mat||!sbox||!round_keys) return CAGOULE_ERR_NULL;
    size_t pb = _p_bytes(p);
    if (out_size < n_blocks * N * pb)            return CAGOULE_ERR_SIZE;

#if defined(__AVX2__)
    if (_avx2_runtime_supported() && sbox->use_feistel) {
        if (n_blocks >= CAGOULE_PIPELINE4_THRESHOLD)
            return _cbc_encrypt_pipeline4_avx2(
                padded, n_blocks, out, out_size,
                mat, sbox, round_keys, num_keys, p);
        return _cbc_encrypt_mono_avx2(
            padded, n_blocks, out, out_size,
            mat, sbox, round_keys, num_keys, p);
    }
#endif

    /* Fallback scalaire */
    uint64_t buf[2][N]; memset(buf, 0, sizeof(buf));
    uint64_t *prev = buf[0], *block = buf[1], tmp[N];
    for (size_t bi = 0; bi < n_blocks; bi++) {
        const uint8_t* src = padded + bi * N;
        for (int j = 0; j < N; j++) block[j] = (uint64_t)src[j];
        for (int j = 0; j < N; j++) block[j] = addmod64(block[j], prev[j], p);
        _matmul16_scalar(mat->fwd, block, tmp, p);
        cagoule_sbox_block_forward(sbox, tmp, block, N);
        uint64_t rk = round_keys[bi % num_keys];
        for (int j = 0; j < N; j++) block[j] = addmod64(block[j], rk, p);
        uint8_t* dst = out + bi * N * pb;
        for (int j = 0; j < N; j++) _u64_to_be(block[j], dst + j*pb, pb);
        uint64_t* sw = prev; prev = block; block = sw;
    }
    return CAGOULE_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 *  API publique — cagoule_cbc_decrypt v2.4.0
 * ══════════════════════════════════════════════════════════════════════ */
int cagoule_cbc_decrypt(
    const uint8_t* cipher_bytes, size_t n_blocks,
    uint8_t* out, size_t out_size,
    const CagouleMatrix* mat, const CagouleSBox64* sbox,
    const uint64_t* round_keys, size_t num_keys, uint64_t p)
{
    if (!cipher_bytes||!out||!mat||!sbox||!round_keys) return CAGOULE_ERR_NULL;
    if (out_size < n_blocks * N)                       return CAGOULE_ERR_SIZE;

#if defined(__AVX2__)
    if (_avx2_runtime_supported() && sbox->use_feistel) {
        if (n_blocks >= CAGOULE_PIPELINE4_THRESHOLD)
            return _cbc_decrypt_pipeline4_avx2(
                cipher_bytes, n_blocks, out, out_size,
                mat, sbox, round_keys, num_keys, p);
        return _cbc_decrypt_mono_avx2(
            cipher_bytes, n_blocks, out, out_size,
            mat, sbox, round_keys, num_keys, p);
    }
#endif

    /* Fallback scalaire */
    size_t pb = _p_bytes(p);
    uint64_t prev[N]; memset(prev, 0, N*8);
    uint64_t cblk[N], tmp[N], c_save[N];
    for (size_t bi = 0; bi < n_blocks; bi++) {
        const uint8_t* src = cipher_bytes + bi * N * pb;
        for (int j = 0; j < N; j++) cblk[j] = _be_to_u64(src + j*pb, pb);
        memcpy(c_save, cblk, N*8);
        uint64_t rk = round_keys[bi % num_keys];
        for (int j = 0; j < N; j++) tmp[j]  = submod64(cblk[j], rk, p);
        cagoule_sbox_block_inverse(sbox, tmp, cblk, N);
        _matmul16_scalar(mat->inv, cblk, tmp, p);
        uint8_t* dst = out + bi * N;
        for (int j = 0; j < N; j++) {
            uint64_t b = submod64(tmp[j], prev[j], p);
            if (b > 255) return CAGOULE_ERR_CORRUPT;
            dst[j] = (uint8_t)b;
        }
        memcpy(prev, c_save, N*8);
    }
    return CAGOULE_OK;
}
