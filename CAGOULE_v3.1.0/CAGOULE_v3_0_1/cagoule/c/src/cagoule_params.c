/**
 * cagoule_params.c — Dérivation complète des paramètres CAGOULE
 * CAGOULE v3.1.0 Feature 2 — voir cagoule_params.h pour le pipeline documenté.
 */
#include "cagoule_params.h"
#include "cagoule_kdf.h"
#include "cagoule_omega.h"
#include "cagoule_mu_table.h"
#include "cagoule_math.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_NODE_ATTEMPTS 10000

static void zeroize(void *p, size_t n) {
    volatile uint8_t *vp = (volatile uint8_t *)p;
    while (n--) *vp++ = 0;
}

static int derive_nodes(const uint8_t k_master[CAGOULE_K_MASTER_LEN],
                         uint64_t alpha0, uint64_t p,
                         uint64_t nodes_out[CAGOULE_N])
{
    nodes_out[0] = alpha0 % p;
    /* "seen" : recherche linéaire sur <=16 éléments, pas besoin de hash set */
    for (int i = 1; i < CAGOULE_N; i++) {
        char info[32];
        int info_len = snprintf(info, sizeof(info), "CAGOULE_NODE_%d", i);

        uint64_t raw;
        int ret = cagoule_kdf_hkdf_u64(k_master, CAGOULE_K_MASTER_LEN,
                                        (const uint8_t *)info, (size_t)info_len,
                                        8, &raw);
        if (ret != CAGOULE_KDF_OK) return CAGOULE_PARAMS_ERR_KDF;
        raw %= p;

        int attempts = 0;
        int collision;
        do {
            collision = 0;
            for (int j = 0; j < i; j++) {
                if (nodes_out[j] == raw) { collision = 1; break; }
            }
            if (collision) {
                raw = (raw + 1) % p;
                attempts++;
            }
        } while (collision && attempts < MAX_NODE_ATTEMPTS);

        if (collision) return CAGOULE_PARAMS_ERR_COLLISION;
        nodes_out[i] = raw;
    }
    return CAGOULE_PARAMS_OK;
}

int cagoule_params_derive(const uint8_t *password, size_t pwd_len,
                           const uint8_t *salt, size_t salt_len,
                           CagouleDerivedParams *out)
{
    if (!password || !salt || !out) return CAGOULE_PARAMS_ERR_NULL;
    if (salt_len != CAGOULE_SALT_LEN) return CAGOULE_PARAMS_ERR_PARAM;

    memset(out, 0, sizeof(*out));
    memcpy(out->salt, salt, CAGOULE_SALT_LEN);

    /* 1. k_master = Argon2id(password, salt) */
    if (cagoule_kdf_argon2id(password, pwd_len, salt, salt_len, out->k_master)
        != CAGOULE_KDF_OK)
        return CAGOULE_PARAMS_ERR_KDF;

    /* 2. n_zeta = (HKDF_u64("CAGOULE_N", 2) % 65533) + 4 */
    {
        uint64_t n_raw;
        if (cagoule_kdf_hkdf_u64(out->k_master, CAGOULE_K_MASTER_LEN,
                                  (const uint8_t *)"CAGOULE_N", 9, 2, &n_raw)
            != CAGOULE_KDF_OK) { cagoule_params_free(out); return CAGOULE_PARAMS_ERR_KDF; }
        out->n_zeta = (int)((n_raw % 65533ULL) + 4ULL);
    }

    /* 3. prime_idx = HKDF_u64("CAGOULE_PRIME_SEL_V25", 1) % 8 */
    uint64_t prime_idx;
    {
        if (cagoule_kdf_hkdf_u64(out->k_master, CAGOULE_K_MASTER_LEN,
                                  (const uint8_t *)"CAGOULE_PRIME_SEL_V25", 21,
                                  1, &prime_idx)
            != CAGOULE_KDF_OK) { cagoule_params_free(out); return CAGOULE_PARAMS_ERR_KDF; }
        prime_idx %= CAGOULE_MERSENNE_POOL_SIZE;
    }
    out->p          = CAGOULE_MERSENNE_P[prime_idx];
    out->k_mersenne = CAGOULE_MERSENNE_K[prime_idx];

    /* 4. alpha0 — LUT (voir cagoule_mu_table.h) */
    uint64_t alpha0 = CAGOULE_MU_ALPHA0[prime_idx];

    /* 5. delta -> rk0/rk1 */
    uint64_t delta;
    if (cagoule_kdf_hkdf_u64(out->k_master, CAGOULE_K_MASTER_LEN,
                              (const uint8_t *)"CAGOULE_DELTA", 13, 8, &delta)
        != CAGOULE_KDF_OK) { cagoule_params_free(out); return CAGOULE_PARAMS_ERR_KDF; }
    delta %= out->p;
    uint64_t rk0 = (delta % (CAGOULE_P32_PRIME - 1)) + 1;
    uint64_t rk1 = ((delta >> 32) % (CAGOULE_P32_PRIME - 1)) + 1;
    cagoule_sbox_init(&out->sbox, out->p, rk0, rk1);

    /* 6. nodes[16] */
    uint64_t nodes[CAGOULE_N];
    int ret = derive_nodes(out->k_master, alpha0, out->p, nodes);
    if (ret != CAGOULE_PARAMS_OK) { cagoule_params_free(out); return ret; }

    /* 7. matrix */
    out->matrix = cagoule_matrix_build(nodes, CAGOULE_N, out->p);
    if (!out->matrix) { cagoule_params_free(out); return CAGOULE_PARAMS_ERR_ALLOC; }

    /* 9. k_stream (AEAD 0x02) */
    if (cagoule_kdf_hkdf(out->k_master, CAGOULE_K_MASTER_LEN,
                          (const uint8_t *)"CAGOULE_ENC", 11,
                          out->k_stream, CAGOULE_K_STREAM_LEN)
        != CAGOULE_KDF_OK) { cagoule_params_free(out); return CAGOULE_PARAMS_ERR_KDF; }

    /* 10. round_keys[64] via cagoule_omega.c (déjà porté C, audité v3.0.0) */
    int oret = cagoule_omega_generate_round_keys(
        out->n_zeta, salt, salt_len, out->p,
        CAGOULE_NUM_ROUND_KEYS, out->round_keys);
    if (oret != CAGOULE_OMEGA_OK) { cagoule_params_free(out); return CAGOULE_PARAMS_ERR_OMEGA; }

    /* 11. z_offset[16] */
    {
        uint8_t z_raw[128];
        if (cagoule_kdf_hkdf(out->k_master, CAGOULE_K_MASTER_LEN,
                              (const uint8_t *)"CAGOULE_Z_SHIFT_V25", 19,
                              z_raw, sizeof(z_raw))
            != CAGOULE_KDF_OK) { cagoule_params_free(out); return CAGOULE_PARAMS_ERR_KDF; }
        for (int i = 0; i < CAGOULE_Z_OFFSET_N; i++) {
            uint64_t v = 0;
            for (int b = 0; b < 8; b++) v = (v << 8) | z_raw[i*8 + b];
            out->z_offset[i] = v % out->p;
        }
        zeroize(z_raw, sizeof(z_raw));
    }

    /* 12. poly_key (MAC 0x03 expérimental — roadmap v3.1.0 §2.2) */
    if (cagoule_kdf_hkdf(out->k_master, CAGOULE_K_MASTER_LEN,
                          (const uint8_t *)"CAGOULE_POLY_V31", 16,
                          out->poly_key, CAGOULE_POLY_KEY_LEN)
        != CAGOULE_KDF_OK) { cagoule_params_free(out); return CAGOULE_PARAMS_ERR_KDF; }

    return CAGOULE_PARAMS_OK;
}

void cagoule_params_free(CagouleDerivedParams *params)
{
    if (!params) return;
    if (params->matrix) {
        cagoule_matrix_free(params->matrix);
        params->matrix = NULL;
    }
    zeroize(params->k_master, sizeof(params->k_master));
    zeroize(params->k_stream, sizeof(params->k_stream));
    zeroize(params->poly_key, sizeof(params->poly_key));
    zeroize(params->round_keys, sizeof(params->round_keys));
    zeroize(params->z_offset, sizeof(params->z_offset));
    zeroize(&params->sbox, sizeof(params->sbox));
}
