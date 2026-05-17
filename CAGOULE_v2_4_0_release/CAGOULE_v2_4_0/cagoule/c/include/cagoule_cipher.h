/**
 * cagoule_cipher.h — Pipeline CBC-like CAGOULE v2.4.0
 *
 * v2.4.0 — Pipeline multi-blocs SIMD :
 *   Dispatch automatique vers _pipeline4 si AVX2 + n_blocks >= THRESHOLD.
 *   Résultats bit-à-bit identiques au chemin mono-bloc.
 *   Aucun breaking change sur le format CGL1.
 */

#ifndef CAGOULE_CIPHER_H
#define CAGOULE_CIPHER_H

#include <stdint.h>
#include <stddef.h>
#include "cagoule_math.h"
#include "cagoule_matrix.h"
#include "cagoule_sbox.h"

/* Codes d'erreur */
#define CAGOULE_OK           0
#define CAGOULE_ERR_NULL    -1
#define CAGOULE_ERR_SIZE    -2
#define CAGOULE_ERR_CORRUPT -3

/* Seuil pour activer le pipeline4 (amortit fill+flush sur 8+ blocs) */
#define CAGOULE_PIPELINE4_THRESHOLD 8

/* Nombre d'octets pour un élément de Z/pZ */
static inline size_t cagoule_p_bytes(uint64_t p) {
    return (p > 0xFFFFFFFF) ? 8 : 4;
}

/**
 * cagoule_cbc_encrypt — v2.4.0
 *   Dispatch pipeline4 (n_blocks >= 8, AVX2) ou mono-bloc.
 *
 * @param padded      Plaintext PKCS7-paddé (n_blocks * 16 bytes)
 * @param n_blocks    Nombre de blocs
 * @param out         Sortie (n_blocks * 16 * p_bytes)
 * @param out_size    Taille buffer out
 * @param mat         Matrice Vandermonde forward
 * @param sbox        S-box Feistel
 * @param round_keys  Clés de ronde (mod p)
 * @param num_keys    Nombre de clés
 * @param p           Premier de travail
 * @return            CAGOULE_OK ou code erreur négatif
 */
int cagoule_cbc_encrypt(
    const uint8_t*       padded,
    size_t               n_blocks,
    uint8_t*             out,
    size_t               out_size,
    const CagouleMatrix* mat,
    const CagouleSBox64* sbox,
    const uint64_t*      round_keys,
    size_t               num_keys,
    uint64_t             p
);

/**
 * cagoule_cbc_decrypt — v2.4.0
 *   Dispatch pipeline4 (n_blocks >= 8, AVX2, CBC parallèle) ou mono-bloc.
 *
 * @param cipher_bytes  Ciphertext (n_blocks * 16 * p_bytes)
 * @param n_blocks      Nombre de blocs
 * @param out           Plaintext PKCS7-paddé (n_blocks * 16)
 * @param out_size      Taille buffer out
 * @param mat           Matrice Vandermonde inverse
 * @param sbox          S-box Feistel
 * @param round_keys    Clés de ronde
 * @param num_keys      Nombre de clés
 * @param p             Premier de travail
 * @return              CAGOULE_OK ou code erreur négatif
 */
int cagoule_cbc_decrypt(
    const uint8_t*       cipher_bytes,
    size_t               n_blocks,
    uint8_t*             out,
    size_t               out_size,
    const CagouleMatrix* mat,
    const CagouleSBox64* sbox,
    const uint64_t*      round_keys,
    size_t               num_keys,
    uint64_t             p
);

#endif /* CAGOULE_CIPHER_H */
