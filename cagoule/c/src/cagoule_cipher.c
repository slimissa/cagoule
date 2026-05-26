/**
 * cagoule_cipher.c — Pipeline CBC CAGOULE v2.5.4
 *
 * v2.5.4 — Z-Domain Shifting inline (no malloc) :
 *   z_offset[16] ∈ Z/pZ transmis depuis Python.
 *   Appliqué comme whitening additif sur les OCTETS du plaintext :
 *     encrypt : byte[j] = (byte[j] + zo[j] % 256) % 256  AVANT chiffrement
 *     decrypt : byte[j] = (byte[j] - zo[j] % 256 + 256) % 256  APRÈS déchiffrement
 *   L'opération est en Z/256Z (byte domain), pas en Z/pZ.
 *   Les zo[j] sont des uint64 dérivés de k_master via HKDF — zo[j]%256 est
 *   indistinguable d'un octet aléatoire pour un attaquant sans k_master.
 *
 *   Implémentation :
 * v2.5.4: Z-shift applied inline in _load_plain / _cbc_unsub — no malloc, no copy.
 *     Pré-calcul : zo_byte[16] = {zo[i]%256, ...} → 1 tableau de 16 uint8.
 *     Cost : 1 modulo par octet, ~6-8 ms/MB en C (vs 82 ms en Python).
 *
 * v2.4.0 — Pipeline multi-blocs SIMD, pipeline4, prefetch.
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
static inline size_t _pb(uint64_t p) { return cagoule_p_bytes(p); }

static inline void _u64_to_be(uint64_t v, uint8_t* b, size_t pb) {
    for (size_t i = pb; i-- > 0;) { b[i] = (uint8_t)(v & 0xFF); v >>= 8; }
}
static inline uint64_t _be_to_u64(const uint8_t* b, size_t pb) {
    uint64_t v = 0;
    for (size_t i = 0; i < pb; i++) v = (v << 8) | b[i];
    return v;
}

static int _avx2_ok(void) {
#if defined(__AVX2__)
    return __builtin_cpu_supports("avx2");
#else
    return 0;
#endif
}

/* ══════════════════════════════════════════════════════════════════════
 * Z-Domain Shifting — niveau octet
 *
 * zo_byte[16] = {zo[0]%256, ..., zo[15]%256}
 * Appliqué comme whitening additif sur le plaintext (avant chiffrement)
 * et soustrait après déchiffrement.
 * ══════════════════════════════════════════════════════════════════════ */

/* Pré-calculer les 16 octets de z_offset */
static inline void _precompute_zo_byte(const uint64_t* zo, size_t nzo,
                                         uint8_t zo_byte[N]) {
    if (!zo || nzo < (size_t)N) { memset(zo_byte, 0, N); return; }
    for (int i = 0; i < N; i++)
        zo_byte[i] = (uint8_t)(zo[i] % 256);
}

/* v2.5.4: _apply_zshift and _undo_zshift removed — Z-shift now applied inline in _load_plain/_cbc_unsub */

#if defined(__AVX2__)
/* ── Sérialisation AVX2 ─────────────────────────────────────────────── */

static inline __m256i _bswap64x4(__m256i v) {
    const __m256i mask = _mm256_set_epi8(
        8,9,10,11,12,13,14,15, 0,1,2,3,4,5,6,7,
        8,9,10,11,12,13,14,15, 0,1,2,3,4,5,6,7);
    return _mm256_shuffle_epi8(v, mask);
}

static inline void _store_blk(const uint64_t bl[N], uint8_t* dst) {
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

static inline void _load_blk(const uint8_t* src, uint64_t bl[N]) {
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

/* v2.5.4: Z-shift applied inline — no malloc needed */
static inline void _load_plain(const uint8_t* src, uint64_t bl[N],
                                const uint8_t zo_byte[N]) {
    /* Load 16 bytes, apply Z-shift if zo_byte is provided, zero-extend to uint64 */
    uint8_t tmp[16];
    if (zo_byte) {
        for (int j = 0; j < 16; j++)
            tmp[j] = (uint8_t)(src[j] + zo_byte[j]);  /* mod 256 */
    } else {
        memcpy(tmp, src, 16);
    }
    __m128i raw = _mm_loadu_si128((const __m128i*)tmp);
    _mm256_storeu_si256((__m256i*)&bl[0],
        _mm256_cvtepu8_epi64(_mm_srli_si128(raw,  0)));
    _mm256_storeu_si256((__m256i*)&bl[4],
        _mm256_cvtepu8_epi64(_mm_srli_si128(raw,  4)));
    _mm256_storeu_si256((__m256i*)&bl[8],
        _mm256_cvtepu8_epi64(_mm_srli_si128(raw,  8)));
    _mm256_storeu_si256((__m256i*)&bl[12],
        _mm256_cvtepu8_epi64(_mm_srli_si128(raw, 12)));
}

static inline void _rk_add(uint64_t bl[N], uint64_t rk, uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    __m256i rv = _mm256_set1_epi64x((int64_t)rk);
    for (int j = 0; j < N; j += 4) {
        __m256i b = _mm256_loadu_si256((const __m256i*)(bl+j));
        _mm256_storeu_si256((__m256i*)(bl+j), addmod64x4(b, rv, pv));
    }
}

static inline void _rk_sub(uint64_t bl[N], uint64_t rk, uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    __m256i rv = _mm256_set1_epi64x((int64_t)rk);
    for (int j = 0; j < N; j += 4) {
        __m256i b = _mm256_loadu_si256((const __m256i*)(bl+j));
        _mm256_storeu_si256((__m256i*)(bl+j), submod64x4(b, rv, pv));
    }
}

static inline void _cbc_add(uint64_t bl[N], const uint64_t prev[N], uint64_t p) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    for (int j = 0; j < N; j += 4) {
        __m256i b = _mm256_loadu_si256((const __m256i*)(bl+j));
        __m256i v = _mm256_loadu_si256((const __m256i*)(prev+j));
        _mm256_storeu_si256((__m256i*)(bl+j), addmod64x4(b, v, pv));
    }
}

/* v2.5.4: inverse Z-shift applied inline */
static inline int _cbc_unsub(const uint64_t m[N], const uint64_t prev[N],
                               uint8_t* dst, uint64_t p,
                               const uint8_t zo_byte[N]) {
    __m256i pv = _mm256_set1_epi64x((int64_t)p);
    uint64_t tmp[N];
    for (int j = 0; j < N; j += 4) {
        __m256i a = _mm256_loadu_si256((const __m256i*)(m+j));
        __m256i v = _mm256_loadu_si256((const __m256i*)(prev+j));
        _mm256_storeu_si256((__m256i*)(tmp+j), submod64x4(a, v, pv));
    }
    for (int j = 0; j < N; j++) {
        if (tmp[j] > 255) return 0;
        dst[j] = (uint8_t)(zo_byte ? ((tmp[j] - zo_byte[j]) & 0xFF) : tmp[j]);
    }
    return 1;
}

/* ── Encrypt mono-bloc ───────────────────────────────────────────────── */
static int _enc_mono(const uint8_t* padded, size_t nb, uint8_t* out, size_t os,
                      const CagouleMatrix* mat, const CagouleSBox64* sb,
                      const uint64_t* rk, size_t nk, uint64_t p,
                      const uint8_t zo_byte[N])
{
    size_t pb = _pb(p);
    if (os < nb*N*pb) return CAGOULE_ERR_SIZE;
    uint64_t buf[2][N]; memset(buf,0,sizeof(buf));
    uint64_t *prev=buf[0], *blk=buf[1], tmp[N];
    for (size_t bi=0; bi<nb; bi++) {
        _load_plain(padded+bi*N, blk, zo_byte);
        _cbc_add(blk, prev, p);
        cagoule_matrix_mul_avx2(mat, blk, tmp);
        _sbox_block_forward_hot_avx2(sb, tmp, blk, N);
        _rk_add(blk, rk[bi%nk], p);
        _store_blk(blk, out+bi*N*pb);
        uint64_t* sw=prev; prev=blk; blk=sw;
    }
    _mm256_zeroupper(); return CAGOULE_OK;
}

/* ── Encrypt pipeline4 ──────────────────────────────────────────────── */
static int _enc_p4(const uint8_t* padded, size_t nb, uint8_t* out, size_t os,
                    const CagouleMatrix* mat, const CagouleSBox64* sb,
                    const uint64_t* rk, size_t nk, uint64_t p,
                    const uint8_t zo_byte[N])
{
    size_t pb = _pb(p);
    if (os < nb*N*pb) return CAGOULE_ERR_SIZE;
    uint64_t prev[N], blk[N], tmp[N]; memset(prev,0,N*8);
    size_t bi=0;
    for (; bi+4<=nb; bi+=4) {
        if (bi+8<=nb) {
            __builtin_prefetch(padded+(bi+4)*N,0,1);
            __builtin_prefetch(padded+(bi+5)*N,0,1);
            __builtin_prefetch(padded+(bi+6)*N,0,1);
            __builtin_prefetch(padded+(bi+7)*N,0,1);
        }
#define EB(I) _load_plain(padded+(bi+(I))*N,blk,zo_byte); \
    _cbc_add(blk,prev,p); cagoule_matrix_mul_avx2(mat,blk,tmp); \
    _sbox_block_forward_hot_avx2(sb,tmp,blk,N); \
    _rk_add(blk,rk[(bi+(I))%nk],p); \
    _store_blk(blk,out+(bi+(I))*N*pb); memcpy(prev,blk,N*8);
        EB(0) EB(1) EB(2) EB(3)
#undef EB
    }
    for (; bi<nb; bi++) {
        _load_plain(padded+bi*N,blk,zo_byte); _cbc_add(blk,prev,p);
        cagoule_matrix_mul_avx2(mat,blk,tmp);
        _sbox_block_forward_hot_avx2(sb,tmp,blk,N);
        _rk_add(blk,rk[bi%nk],p); _store_blk(blk,out+bi*N*pb);
        memcpy(prev,blk,N*8);
    }
    _mm256_zeroupper(); return CAGOULE_OK;
}

/* ── Decrypt mono-bloc ───────────────────────────────────────────────── */
static int _dec_mono(const uint8_t* cb, size_t nb, uint8_t* out, size_t os,
                      const CagouleMatrix* mat, const CagouleSBox64* sb,
                      const uint64_t* rk, size_t nk, uint64_t p,
                      const uint8_t zo_byte[N])
{
    size_t pb = _pb(p);
    if (os < nb*N) return CAGOULE_ERR_SIZE;
    uint64_t prev[N]; memset(prev,0,sizeof(prev));
    uint64_t cblk[N], tmp[N], cs[N];
    for (size_t bi=0; bi<nb; bi++) {
        _load_blk(cb+bi*N*pb, cblk); memcpy(cs,cblk,N*8);
        _rk_sub(cblk, rk[bi%nk], p);
        _sbox_block_inverse_hot_avx2(sb, cblk, tmp, N);
        cagoule_matrix_mul_inv_avx2(mat, tmp, cblk);
        if (!_cbc_unsub(cblk, prev, out+bi*N, p, zo_byte)) {
            _mm256_zeroupper(); return CAGOULE_ERR_CORRUPT;
        }
        memcpy(prev,cs,N*8);
    }
    _mm256_zeroupper(); return CAGOULE_OK;
}

/* ── Decrypt pipeline4 ──────────────────────────────────────────────── */
static int _dec_p4(const uint8_t* cb, size_t nb, uint8_t* out, size_t os,
                    const CagouleMatrix* mat, const CagouleSBox64* sb,
                    const uint64_t* rk, size_t nk, uint64_t p,
                    const uint8_t zo_byte[N])
{
    size_t pb = _pb(p);
    if (os < nb*N) return CAGOULE_ERR_SIZE;
    uint64_t cblk[4][N], tmp[4][N], saved[5][N], prev[N];
    memset(prev,0,N*8);
    size_t bi=0;
    for (; bi+4<=nb; bi+=4) {
        if (bi+8<=nb) __builtin_prefetch(cb+(bi+8)*N*pb,0,1);
        _load_blk(cb+(bi+0)*N*pb,cblk[0]); _load_blk(cb+(bi+1)*N*pb,cblk[1]);
        _load_blk(cb+(bi+2)*N*pb,cblk[2]); _load_blk(cb+(bi+3)*N*pb,cblk[3]);
        memcpy(saved[0],prev,N*8); memcpy(saved[1],cblk[0],N*8);
        memcpy(saved[2],cblk[1],N*8); memcpy(saved[3],cblk[2],N*8);
        memcpy(saved[4],cblk[3],N*8);
        _rk_sub(cblk[0],rk[(bi+0)%nk],p); _rk_sub(cblk[1],rk[(bi+1)%nk],p);
        _rk_sub(cblk[2],rk[(bi+2)%nk],p); _rk_sub(cblk[3],rk[(bi+3)%nk],p);
        _sbox_block_inverse_hot_avx2(sb,cblk[0],tmp[0],N);
        _sbox_block_inverse_hot_avx2(sb,cblk[1],tmp[1],N);
        _sbox_block_inverse_hot_avx2(sb,cblk[2],tmp[2],N);
        _sbox_block_inverse_hot_avx2(sb,cblk[3],tmp[3],N);
        cagoule_matrix_mul_inv_avx2(mat,tmp[0],cblk[0]);
        cagoule_matrix_mul_inv_avx2(mat,tmp[1],cblk[1]);
        cagoule_matrix_mul_inv_avx2(mat,tmp[2],cblk[2]);
        cagoule_matrix_mul_inv_avx2(mat,tmp[3],cblk[3]);
        if (!_cbc_unsub(cblk[0],saved[0],out+(bi+0)*N,p,zo_byte)) goto corrupt;
        if (!_cbc_unsub(cblk[1],saved[1],out+(bi+1)*N,p,zo_byte)) goto corrupt;
        if (!_cbc_unsub(cblk[2],saved[2],out+(bi+2)*N,p,zo_byte)) goto corrupt;
        if (!_cbc_unsub(cblk[3],saved[3],out+(bi+3)*N,p,zo_byte)) goto corrupt;
        memcpy(prev,saved[4],N*8);
    }
    for (; bi<nb; bi++) {
        _load_blk(cb+bi*N*pb,cblk[0]); memcpy(saved[0],prev,N*8);
        memcpy(prev,cblk[0],N*8);
        _rk_sub(cblk[0],rk[bi%nk],p);
        _sbox_block_inverse_hot_avx2(sb,cblk[0],tmp[0],N);
        cagoule_matrix_mul_inv_avx2(mat,tmp[0],cblk[0]);
        if (!_cbc_unsub(cblk[0],saved[0],out+bi*N,p,zo_byte)) goto corrupt;
    }
    _mm256_zeroupper(); return CAGOULE_OK;
corrupt:
    _mm256_zeroupper(); return CAGOULE_ERR_CORRUPT;
}

#endif /* __AVX2__ */

/* ══════════════════════════════════════════════════════════════════════
 * API publique — cagoule_cbc_encrypt v2.5.0
 *
 * z_offset appliqué sur les OCTETS du plaintext AVANT chiffrement.
 * Opération en Z/256Z (zo[i]%256) — domaine naturel des octets.
 * ══════════════════════════════════════════════════════════════════════ */
int cagoule_cbc_encrypt(
    const uint8_t* padded, size_t n_blocks,
    uint8_t* out, size_t out_size,
    const CagouleMatrix* mat, const CagouleSBox64* sbox,
    const uint64_t* rk, size_t nk, uint64_t p,
    const uint64_t* zo, size_t nzo)
{
    if (!padded||!out||!mat||!sbox||!rk) return CAGOULE_ERR_NULL;
    size_t pb = _pb(p);
    if (out_size < n_blocks*N*pb)        return CAGOULE_ERR_SIZE;

    /* Pré-calculer zo_byte[16] et appliquer sur le buffer padded (copie locale) */
    uint8_t zo_byte[N] = {0};
    int use_zo = (zo && nzo >= (size_t)N);
    if (use_zo) _precompute_zo_byte(zo, nzo, zo_byte);

    /* v2.5.4: Z-shift applied inline in _load_plain — no malloc needed */

#if defined(__AVX2__)
    if (_avx2_ok() && sbox->use_feistel) {
        const uint8_t* zo_ptr = use_zo ? zo_byte : NULL;
        return (n_blocks >= CAGOULE_PIPELINE4_THRESHOLD)
            ? _enc_p4(padded, n_blocks, out, out_size, mat, sbox, rk, nk, p, zo_ptr)
            : _enc_mono(padded, n_blocks, out, out_size, mat, sbox, rk, nk, p, zo_ptr);
    }
#endif

    /* Fallback scalaire */
    uint64_t buf[2][N]; memset(buf,0,sizeof(buf));
    uint64_t *prev=buf[0], *blk=buf[1], tmp[N];
    for (size_t bi=0; bi<n_blocks; bi++) {
        const uint8_t* src = padded + bi*N;
        for (int j=0; j<N; j++)
            blk[j] = (uint64_t)((src[j] + zo_byte[j]) & 0xFF);
        for (int j=0; j<N; j++) blk[j] = addmod64(blk[j], prev[j], p);
        _matmul16_scalar(mat->fwd, blk, tmp, p);
        cagoule_sbox_block_forward(sbox, tmp, blk, N);
        uint64_t k = rk[bi%nk];
        for (int j=0; j<N; j++) blk[j] = addmod64(blk[j], k, p);
        uint8_t* dst = out + bi*N*pb;
        for (int j=0; j<N; j++) _u64_to_be(blk[j], dst+j*pb, pb);
        uint64_t* sw=prev; prev=blk; blk=sw;
    }
    return CAGOULE_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 * API publique — cagoule_cbc_decrypt v2.5.0
 *
 * z_offset annulé sur les OCTETS du plaintext APRÈS déchiffrement.
 * ══════════════════════════════════════════════════════════════════════ */
int cagoule_cbc_decrypt(
    const uint8_t* cb, size_t n_blocks,
    uint8_t* out, size_t out_size,
    const CagouleMatrix* mat, const CagouleSBox64* sbox,
    const uint64_t* rk, size_t nk, uint64_t p,
    const uint64_t* zo, size_t nzo)
{
    if (!cb||!out||!mat||!sbox||!rk) return CAGOULE_ERR_NULL;
    if (out_size < n_blocks*N)       return CAGOULE_ERR_SIZE;

    uint8_t zo_byte[N] = {0};
    int use_zo = (zo && nzo >= (size_t)N);
    if (use_zo) _precompute_zo_byte(zo, nzo, zo_byte);

#if defined(__AVX2__)
    if (_avx2_ok() && sbox->use_feistel) {
        const uint8_t* zo_ptr = use_zo ? zo_byte : NULL;
        return (n_blocks >= CAGOULE_PIPELINE4_THRESHOLD)
            ? _dec_p4(cb, n_blocks, out, out_size, mat, sbox, rk, nk, p, zo_ptr)
            : _dec_mono(cb, n_blocks, out, out_size, mat, sbox, rk, nk, p, zo_ptr);
    }
#endif

    /* Fallback scalaire */
    size_t pbytes = _pb(p);
    uint64_t prev[N]; memset(prev,0,N*8);
    uint64_t cblk[N], tmp[N], cs[N];
    for (size_t bi=0; bi<n_blocks; bi++) {
        const uint8_t* src = cb + bi*N*pbytes;
        for (int j=0; j<N; j++) cblk[j] = _be_to_u64(src+j*pbytes, pbytes);
        memcpy(cs,cblk,N*8);
        uint64_t k = rk[bi%nk];
        for (int j=0; j<N; j++) tmp[j] = submod64(cblk[j], k, p);
        cagoule_sbox_block_inverse(sbox, tmp, cblk, N);
        _matmul16_scalar(mat->inv, cblk, tmp, p);
        uint8_t* dst = out + bi*N;
        for (int j=0; j<N; j++) {
            uint64_t b = submod64(tmp[j], prev[j], p);
            if (b > 255) return CAGOULE_ERR_CORRUPT;
            dst[j] = (uint8_t)((b - zo_byte[j]) & 0xFF);
        }
        memcpy(prev,cs,N*8);
    }
    return CAGOULE_OK;
}
