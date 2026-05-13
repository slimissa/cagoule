/**
 * cagoule_cipher.c — Pipeline CBC CAGOULE v2.3.0
 *
 * Nouveautés v2.3.0 :
 *   - cagoule_sbox_block_forward_avx2 / _inverse_avx2 intégrés
 *     → S-box : ~20 ms/MB → ~5 ms/MB
 *   - Round key add/sub via addmod64x4 / submod64x4 (4 lanes)
 *   - XOR CBC via addmod64x4 / submod64x4
 *   - Dispatch S-box hoisted une fois par message
 *   - Cible end-to-end : ~8-12 MB/s
 *
 * Héritage v2.2.0 conservé :
 *   - Dispatch matrice AVX2 hoisted
 *   - Bulk serialization AVX2
 *   - Ring buffer prev/block (pointer swap)
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

/* Prototypes forward (cagoule_matrix_avx2.c) */
#if defined(__AVX2__)
extern void cagoule_matrix_mul_avx2(const CagouleMatrix*,
                                     const uint64_t[CAGOULE_N],
                                     uint64_t[CAGOULE_N]);
extern void cagoule_matrix_mul_inv_avx2(const CagouleMatrix*,
                                         const uint64_t[CAGOULE_N],
                                         uint64_t[CAGOULE_N]);
/* cagoule_sbox_block_forward_avx2 / _inverse_avx2 déclarées dans cagoule_sbox.h */
#endif

extern void _matmul16_scalar(const uint64_t[CAGOULE_N][CAGOULE_N],
                              const uint64_t[CAGOULE_N],
                              uint64_t[CAGOULE_N], uint64_t);

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

/* ── AVX2 dispatch — détection runtime ────────────────────────────────
 *
 * __builtin_cpu_supports("avx2") est sûr : sur glibc il lit un flag CPUID
 * mis en cache à l'init du processus (pas d'appel CPUID à chaque message).
 *
 * La condition p >= 2^63 est TOUJOURS vraie pour les primes CAGOULE
 * (params.py force p_seed |= (1<<63) avant nextprime). On la retire du
 * dispatch chaud pour éviter une comparaison inutile par message.
 *
 * sbox->use_feistel = 1 est garanti pour p >= CAGOULE_SBOX_LARGE_PRIME_THRESHOLD
 * (= 2^32) et tous les primes CAGOULE sont bien au-dessus de ce seuil.
 * On ne vérifie que __AVX2__ (compile-time) et cpu_supports (runtime).
 */
static int _avx2_runtime_supported(void) {
#if defined(__AVX2__)
    /* Résultat mis en cache par glibc dans __cpu_features_init() */
    return __builtin_cpu_supports("avx2");
#else
    return 0;
#endif
}
/*
 * CORRECTIF ENDIANNESS (BUG v2.2.0) :
 *   Le chemin scalaire sérialise chaque uint64_t en big-endian (MSB first)
 *   via _u64_to_be(). Le chemin AVX2 doit produire le même résultat octet
 *   par octet pour garantir la propriété "bit-à-bit identiques".
 *
 *   Sur x86-64 (little-endian natif), _mm256_storeu_si256 écrirait les
 *   octets en little-endian. On corrige avec un byte-swap via shuffle_epi8.
 *
 *   Masque de byte-swap pour chaque lane 64-bit dans un registre __m256i :
 *     Lane i (octets 8i..8i+7) : prendre l'octet 8i+7 en premier (MSB).
 *     Mask indexé par _mm256_set_epi8(e31..e0) → e_k = mask[k].
 */
#if defined(__AVX2__)
/* Byte-swap chaque lane uint64 d'un registre __m256i (big-endian ↔ little-endian) */
static inline __m256i _bswap64x4(__m256i v) {
    /* shuffle_epi8 : pour chaque byte de sortie, mask[i] indique l'index source
     * dans la même lane 128-bit. Deux lanes identiques (VPSHUFB opère par 128b).
     * Layout: byte 0←src7, byte 1←src6, ..., byte 7←src0 (bswap du premier u64)
     *         byte 8←src15, ..., byte 15←src8  (bswap du second u64) */
    const __m256i bswap = _mm256_set_epi8(
        /* lane haute (bl[2], bl[3]) */
        8, 9,10,11,12,13,14,15,   /* bl[3] : e31..e24 → bytes 31..24 */
        0, 1, 2, 3, 4, 5, 6, 7,  /* bl[2] : e23..e16 → bytes 23..16 */
        /* lane basse (bl[0], bl[1]) */
        8, 9,10,11,12,13,14,15,   /* bl[1] : e15..e8  → bytes 15..8  */
        0, 1, 2, 3, 4, 5, 6, 7   /* bl[0] : e7..e0   → bytes 7..0   */
    );
    return _mm256_shuffle_epi8(v, bswap);
}

/* Sérialise 16 uint64_t en big-endian (compatible avec le chemin scalaire) */
static inline void _store_block_avx2(const uint64_t bl[N], uint8_t* dst) {
    __m256i r;
    r = _mm256_set_epi64x((int64_t)bl[3],(int64_t)bl[2],(int64_t)bl[1],(int64_t)bl[0]);
    _mm256_storeu_si256((__m256i*)(dst+ 0), _bswap64x4(r));
    r = _mm256_set_epi64x((int64_t)bl[7],(int64_t)bl[6],(int64_t)bl[5],(int64_t)bl[4]);
    _mm256_storeu_si256((__m256i*)(dst+32), _bswap64x4(r));
    r = _mm256_set_epi64x((int64_t)bl[11],(int64_t)bl[10],(int64_t)bl[9],(int64_t)bl[8]);
    _mm256_storeu_si256((__m256i*)(dst+64), _bswap64x4(r));
    r = _mm256_set_epi64x((int64_t)bl[15],(int64_t)bl[14],(int64_t)bl[13],(int64_t)bl[12]);
    _mm256_storeu_si256((__m256i*)(dst+96), _bswap64x4(r));
}

/* Désérialise 16 uint64_t depuis des octets big-endian (compatible scalaire) */
static inline void _load_block_avx2(const uint8_t* src, uint64_t bl[N]) {
    __m256i r;
    r = _mm256_loadu_si256((const __m256i*)(src+ 0));
    _mm256_storeu_si256((__m256i*)&bl[0],  _bswap64x4(r));
    r = _mm256_loadu_si256((const __m256i*)(src+32));
    _mm256_storeu_si256((__m256i*)&bl[4],  _bswap64x4(r));
    r = _mm256_loadu_si256((const __m256i*)(src+64));
    _mm256_storeu_si256((__m256i*)&bl[8],  _bswap64x4(r));
    r = _mm256_loadu_si256((const __m256i*)(src+96));
    _mm256_storeu_si256((__m256i*)&bl[12], _bswap64x4(r));
}

/* ── v2.3.0 : round-key add/sub vectorisés ─────────────────────────── */
static inline void _rk_add_avx2(uint64_t bl[N], uint64_t rk, uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    __m256i rv = _mm256_set1_epi64x((int64_t)rk);
    for (int j = 0; j < N; j += 4) {
        __m256i b = _mm256_loadu_si256((const __m256i*)(bl+j));
        _mm256_storeu_si256((__m256i*)(bl+j), addmod64x4(b, rv, pv));
    }
}
static inline void _rk_sub_avx2(uint64_t bl[N], uint64_t rk, uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    __m256i rv = _mm256_set1_epi64x((int64_t)rk);
    for (int j = 0; j < N; j += 4) {
        __m256i b = _mm256_loadu_si256((const __m256i*)(bl+j));
        _mm256_storeu_si256((__m256i*)(bl+j), submod64x4(b, rv, pv));
    }
}
/* ── v2.3.0 : CBC XOR via addmod64x4 ────────────────────────────── */
static inline void _cbc_xor_avx2(uint64_t bl[N],
                                   const uint64_t prev[N], uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    for (int j = 0; j < N; j += 4) {
        __m256i b = _mm256_loadu_si256((const __m256i*)(bl+j));
        __m256i v = _mm256_loadu_si256((const __m256i*)(prev+j));
        _mm256_storeu_si256((__m256i*)(bl+j), addmod64x4(b, v, pv));
    }
}
/* ── v2.3.0 : CBC XOR inverse + écriture plaintext ──────────────── */
static inline int _cbc_unsub_avx2(const uint64_t mat_out[N],
                                    const uint64_t prev[N],
                                    uint8_t* dst, uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    uint64_t tmp[N];
    for (int j = 0; j < N; j += 4) {
        __m256i a = _mm256_loadu_si256((const __m256i*)(mat_out+j));
        __m256i v = _mm256_loadu_si256((const __m256i*)(prev+j));
        _mm256_storeu_si256((__m256i*)(tmp+j), submod64x4(a, v, pv));
    }
    for (int j = 0; j < N; j++) {
        if (tmp[j] > 255) return 0;
        dst[j] = (uint8_t)tmp[j];
    }
    return 1;
}
#endif /* __AVX2__ */

/* ══════════════════════════════════════════════════════════════════════
 *  cagoule_cbc_encrypt — v2.3.0
 * ══════════════════════════════════════════════════════════════════════ */
int cagoule_cbc_encrypt(
    const uint8_t*       padded,
    size_t               n_blocks,
    uint8_t*             out,
    size_t               out_size,
    const CagouleMatrix* mat,
    const CagouleSBox64*       sbox,
    const uint64_t*      round_keys,
    size_t               num_keys,
    uint64_t             p)
{
    size_t pb = _p_bytes(p);
    if (!padded||!out||!mat||!sbox||!round_keys) return CAGOULE_ERR_NULL;
    if (out_size < n_blocks * N * pb) return CAGOULE_ERR_SIZE;

    int use_avx2 = _avx2_runtime_supported() && sbox->use_feistel;

    uint64_t buf[2][N]; memset(buf, 0, sizeof(buf));
    uint64_t *prev=buf[0], *block=buf[1], tmp[N];

    for (size_t bi = 0; bi < n_blocks; bi++) {
        const uint8_t* src = padded + bi * N;

#if defined(__AVX2__)
        if (use_avx2) {
            /* Chargement AVX2 : 16 octets → 16 uint64_t zero-étendus.
             * Utilise 4 × _mm_cvtepu8_epi16 + _mm256_cvtepu16_epi64 pour
             * zero-étendre chaque octet en uint64 sans boucle scalaire.
             * Gain : élimine 16 affectations scalaires par bloc. */
            __m128i raw = _mm_loadu_si128((const __m128i*)src);
            /* Étendre bytes 0-3 en 4 × uint64 */
            _mm256_storeu_si256((__m256i*)&block[0],
                _mm256_cvtepu8_epi64(_mm_srli_si128(raw, 0)));
            _mm256_storeu_si256((__m256i*)&block[4],
                _mm256_cvtepu8_epi64(_mm_srli_si128(raw, 4)));
            _mm256_storeu_si256((__m256i*)&block[8],
                _mm256_cvtepu8_epi64(_mm_srli_si128(raw, 8)));
            _mm256_storeu_si256((__m256i*)&block[12],
                _mm256_cvtepu8_epi64(_mm_srli_si128(raw, 12)));
            _cbc_xor_avx2(block, prev, p);
            cagoule_matrix_mul_avx2(mat, block, tmp);
            _sbox_block_forward_hot_avx2(sbox, tmp, block, N);
            _rk_add_avx2(block, round_keys[bi % num_keys], p);
            _store_block_avx2(block, out + bi * N * pb);
        } else {
#endif
            for (int j = 0; j < N; j++) block[j] = (uint64_t)src[j];
            for (int j=0;j<N;j++) block[j]=addmod64(block[j],prev[j],p);
            _matmul16_scalar(mat->fwd, block, tmp, p);
            cagoule_sbox_block_forward(sbox, tmp, block, N);
            uint64_t rk=round_keys[bi%num_keys];
            for (int j=0;j<N;j++) block[j]=addmod64(block[j],rk,p);
            uint8_t* dst=out+bi*N*pb;
            for (int j=0;j<N;j++) _u64_to_be(block[j],dst+j*pb,pb);
#if defined(__AVX2__)
        }
#endif
        uint64_t* sw=prev; prev=block; block=sw;
    }
#if defined(__AVX2__)
    if (use_avx2) _mm256_zeroupper();
#endif
    return CAGOULE_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 *  cagoule_cbc_decrypt — v2.3.0
 * ══════════════════════════════════════════════════════════════════════ */
int cagoule_cbc_decrypt(
    const uint8_t*       cipher_bytes,
    size_t               n_blocks,
    uint8_t*             out,
    size_t               out_size,
    const CagouleMatrix* mat,
    const CagouleSBox64*       sbox,
    const uint64_t*      round_keys,
    size_t               num_keys,
    uint64_t             p)
{
    size_t pb = _p_bytes(p);
    if (!cipher_bytes||!out||!mat||!sbox||!round_keys) return CAGOULE_ERR_NULL;
    if (out_size < n_blocks * N) return CAGOULE_ERR_SIZE;

    int use_avx2 = _avx2_runtime_supported() && sbox->use_feistel;

    uint64_t buf[2][N]; memset(buf, 0, sizeof(buf));
    uint64_t *prev=buf[0], *cipher_block=buf[1], tmp[N];

    for (size_t bi = 0; bi < n_blocks; bi++) {
        const uint8_t* src = cipher_bytes + bi * N * pb;

#if defined(__AVX2__)
        if (use_avx2) {
            _load_block_avx2(src, cipher_block);
            uint64_t c_save[N]; memcpy(c_save, cipher_block, N*8);
            _rk_sub_avx2(cipher_block, round_keys[bi%num_keys], p);         /* RK sub  */
            _sbox_block_inverse_hot_avx2(sbox, cipher_block, tmp, N);       /* S-box⁻¹ */
            cagoule_matrix_mul_inv_avx2(mat, tmp, cipher_block);             /* Mat⁻¹   */
            if (!_cbc_unsub_avx2(cipher_block, prev, out+bi*N, p))
                return CAGOULE_ERR_CORRUPT;
            memcpy(prev, c_save, N*8);
        } else {
#endif
            for (int j=0;j<N;j++) cipher_block[j]=_be_to_u64(src+j*pb,pb);
            uint64_t c_save[N]; memcpy(c_save, cipher_block, N*8);
            uint64_t rk=round_keys[bi%num_keys];
            for (int j=0;j<N;j++) tmp[j]=submod64(cipher_block[j],rk,p);
            cagoule_sbox_block_inverse(sbox, tmp, cipher_block, N);
            _matmul16_scalar(mat->inv, cipher_block, tmp, p);
            uint8_t* dst=out+bi*N;
            for (int j=0;j<N;j++) {
                uint64_t b=submod64(tmp[j],prev[j],p);
                if (b>255) return CAGOULE_ERR_CORRUPT;
                dst[j]=(uint8_t)b;
            }
            memcpy(prev, c_save, N*8);
#if defined(__AVX2__)
        }
#endif
    }
#if defined(__AVX2__)
    if (use_avx2) _mm256_zeroupper();
#endif
    return CAGOULE_OK;
}
