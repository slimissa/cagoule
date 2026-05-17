#include <stdlib.h>
#include <string.h>
#include "cagoule_math.h"
#include "cagoule_matrix.h"
#include "cagoule_sbox.h"
#include "cagoule_cipher.h"

#if defined(__AVX2__)
#include <immintrin.h>
#include "cagoule_math_avx2.h"
#endif

#define N CAGOULE_N

/* ── Prototypes forward pour les chemins directs ──────────────────── */
#if defined(__AVX2__)
extern void cagoule_matrix_mul_avx2(const CagouleMatrix* m,
                                     const uint64_t v[CAGOULE_N],
                                     uint64_t out[CAGOULE_N]);
extern void cagoule_matrix_mul_inv_avx2(const CagouleMatrix* m,
                                         const uint64_t v[CAGOULE_N],
                                         uint64_t out[CAGOULE_N]);
#endif

/* ── Helpers ──────────────────────────────────────────────────────── */

static inline size_t _p_bytes(uint64_t p) {
    return cagoule_p_bytes(p);
}

static inline void _u64_to_be(uint64_t val, uint8_t* buf, size_t p_bytes) {
    for (size_t i = p_bytes; i-- > 0;) {
        buf[i] = (uint8_t)(val & 0xFF);
        val >>= 8;
    }
}

static inline uint64_t _be_to_u64(const uint8_t* buf, size_t p_bytes) {
    uint64_t val = 0;
    for (size_t i = 0; i < p_bytes; i++)
        val = (val << 8) | buf[i];
    return val;
}

/* ── Scalar fallback (exposed from cagoule_matrix.c) ──────────────── */
extern void _matmul16_scalar(const uint64_t mat[CAGOULE_N][CAGOULE_N],
                              const uint64_t v[CAGOULE_N],
                              uint64_t out[CAGOULE_N],
                              uint64_t p);

/* ── AVX2 bulk serialization helpers ───────────────────────────────── */
#if defined(__AVX2__)
static inline void _store_block_avx2(const uint64_t block[N], uint8_t* dst) {
    _mm256_storeu_si256((__m256i*)(dst +  0), _mm256_set_epi64x(
        (int64_t)block[3], (int64_t)block[2], (int64_t)block[1], (int64_t)block[0]));
    _mm256_storeu_si256((__m256i*)(dst + 32), _mm256_set_epi64x(
        (int64_t)block[7], (int64_t)block[6], (int64_t)block[5], (int64_t)block[4]));
    _mm256_storeu_si256((__m256i*)(dst + 64), _mm256_set_epi64x(
        (int64_t)block[11], (int64_t)block[10], (int64_t)block[9], (int64_t)block[8]));
    _mm256_storeu_si256((__m256i*)(dst + 96), _mm256_set_epi64x(
        (int64_t)block[15], (int64_t)block[14], (int64_t)block[13], (int64_t)block[12]));
}

static inline void _load_block_avx2(const uint8_t* src, uint64_t block[N]) {
    __m256i r0 = _mm256_loadu_si256((__m256i*)(src +  0));
    __m256i r1 = _mm256_loadu_si256((__m256i*)(src + 32));
    __m256i r2 = _mm256_loadu_si256((__m256i*)(src + 64));
    __m256i r3 = _mm256_loadu_si256((__m256i*)(src + 96));
    _mm256_storeu_si256((__m256i*)&block[0], r0);
    _mm256_storeu_si256((__m256i*)&block[4], r1);
    _mm256_storeu_si256((__m256i*)&block[8], r2);
    _mm256_storeu_si256((__m256i*)&block[12], r3);
}
#endif

/* ══════════════════════════════════════════════════════════════════════
 *  cagoule_cbc_encrypt — AVX2 dispatch hoisted + bulk serialization
 * ══════════════════════════════════════════════════════════════════════ */

int cagoule_cbc_encrypt(
    const uint8_t*       padded,
    size_t               n_blocks,
    uint8_t*             out,
    size_t               out_size,
    const CagouleMatrix* mat,
    CagouleSBox64*       sbox,
    const uint64_t*      round_keys,
    size_t               num_keys,
    uint64_t             p)
{
    size_t p_bytes = _p_bytes(p);
    size_t out_needed = n_blocks * N * p_bytes;

    if (!padded || !out || !mat || !sbox || !round_keys)
        return CAGOULE_ERR_NULL;
    if (out_size < out_needed)
        return CAGOULE_ERR_SIZE;

    /* ── Hoist AVX2 dispatch once per message ───────────────────────── */
    int use_avx2 = 0;
#if defined(__AVX2__)
    if (__builtin_cpu_supports("avx2") && p >= ((uint64_t)1 << 63))
        use_avx2 = 1;
#endif

    /* ── Ring buffer for prev/block (no memcpy) ─────────────────────── */
    uint64_t buf[2][N] = {{0}};
    uint64_t *prev  = buf[0];
    uint64_t *block = buf[1];
    uint64_t tmp[N];

    for (size_t bi = 0; bi < n_blocks; bi++) {
        const uint8_t* src = padded + bi * N;

        for (int j = 0; j < N; j++)
            block[j] = (uint64_t)src[j];

        for (int j = 0; j < N; j++)
            block[j] = addmod64(block[j], prev[j], p);

        /* ── Direct matrix multiply (no per-block dispatch) ──────────── */
#if defined(__AVX2__)
        if (use_avx2)
            cagoule_matrix_mul_avx2(mat, block, tmp);
        else
            _matmul16_scalar(mat->fwd, block, tmp, p);
#else
        _matmul16_scalar(mat->fwd, block, tmp, p);
#endif

        cagoule_sbox_block_forward(sbox, tmp, block, N);

        uint64_t rk = round_keys[bi % num_keys];
        for (int j = 0; j < N; j++)
            block[j] = addmod64(block[j], rk, p);

        /* ── Bulk serialization or scalar fallback ───────────────────── */
        uint8_t* dst = out + bi * N * p_bytes;
#if defined(__AVX2__)
        if (p_bytes == 8 && use_avx2)
            _store_block_avx2(block, dst);
        else
#endif
            for (int j = 0; j < N; j++)
                _u64_to_be(block[j], dst + j * p_bytes, p_bytes);

        /* ── Swap prev and block pointers (ring buffer) ──────────────── */
        uint64_t *swap = prev;
        prev  = block;
        block = swap;
    }

    return CAGOULE_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 *  cagoule_cbc_decrypt — AVX2 dispatch hoisted + bulk deserialization
 * ══════════════════════════════════════════════════════════════════════ */

int cagoule_cbc_decrypt(
    const uint8_t*       cipher_bytes,
    size_t               n_blocks,
    uint8_t*             out,
    size_t               out_size,
    const CagouleMatrix* mat,
    CagouleSBox64*       sbox,
    const uint64_t*      round_keys,
    size_t               num_keys,
    uint64_t             p)
{
    size_t p_bytes = _p_bytes(p);
    size_t pt_needed = n_blocks * N;

    if (!cipher_bytes || !out || !mat || !sbox || !round_keys)
        return CAGOULE_ERR_NULL;
    if (out_size < pt_needed)
        return CAGOULE_ERR_SIZE;

    /* ── Hoist AVX2 dispatch once per message ───────────────────────── */
    int use_avx2 = 0;
#if defined(__AVX2__)
    if (__builtin_cpu_supports("avx2") && p >= ((uint64_t)1 << 63))
        use_avx2 = 1;
#endif

    /* ── Ring buffer for prev/cipher_block (no memcpy) ──────────────── */
    uint64_t buf[2][N] = {{0}};
    uint64_t *prev         = buf[0];
    uint64_t *cipher_block = buf[1];
    uint64_t tmp[N];

    for (size_t bi = 0; bi < n_blocks; bi++) {
        const uint8_t* src = cipher_bytes + bi * N * p_bytes;

        /* ── Bulk deserialization or scalar fallback ─────────────────── */
#if defined(__AVX2__)
        if (p_bytes == 8 && use_avx2)
            _load_block_avx2(src, cipher_block);
        else
#endif
            for (int j = 0; j < N; j++)
                cipher_block[j] = _be_to_u64(src + j * p_bytes, p_bytes);

        /* ── Save ciphertext for next block's prev ──────────────────── */
        uint64_t c_save[N];
        memcpy(c_save, cipher_block, N * sizeof(uint64_t));

        uint64_t rk = round_keys[bi % num_keys];
        for (int j = 0; j < N; j++)
            tmp[j] = submod64(cipher_block[j], rk, p);

        cagoule_sbox_block_inverse(sbox, tmp, cipher_block, N);

        /* ── Direct matrix multiply (no per-block dispatch) ──────────── */
#if defined(__AVX2__)
        if (use_avx2)
            cagoule_matrix_mul_inv_avx2(mat, cipher_block, tmp);
        else
            _matmul16_scalar(mat->inv, cipher_block, tmp, p);
#else
        _matmul16_scalar(mat->inv, cipher_block, tmp, p);
#endif

        uint8_t* dst = out + bi * N;
        for (int j = 0; j < N; j++) {
            uint64_t b = submod64(tmp[j], prev[j], p);
            if (b > 255)
                return CAGOULE_ERR_CORRUPT;
            dst[j] = (uint8_t)b;
        }

        /* ── Update prev from saved ciphertext ──────────────────────── */
        memcpy(prev, c_save, N * sizeof(uint64_t));
    }

    return CAGOULE_OK;
}