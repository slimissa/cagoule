/**
 * cagoule_api.h — Wrapper C unifié — CAGOULE v3.1.0 Feature 2
 *
 * Élimine la surcharge ctypes (~28% mesuré v3.0.0/roadmap §3.1) en
 * exposant le pipeline complet (KDF → matrix/sbox → CTR → AEAD) comme
 * une poignée de fonctions C, buffer de sortie possédé par l'appelant.
 *
 * ════════════════════════════════════════════════════════════════════
 * ⚠️  CONSTAT DE SÉCURITÉ — RÉUTILISATION DE HANDLE (lire avant usage)
 * ════════════════════════════════════════════════════════════════════
 * En vérifiant le pipeline existant avant d'écrire ce module, j'ai trouvé
 * que encrypt_bulk_ctr(params=...) (Python, v3.0.0, DÉJÀ LIVRÉ) dérive
 * l'IV CTR UNIQUEMENT depuis k_master — donc IDENTIQUE pour tout message
 * partageant le même `params`. Vérifié empiriquement : deux messages de
 * même taille chiffrés avec le même `params` partagé produisent un
 * keystream algébrique CTR bit-à-bit identique (two-time-pad classique).
 *
 * Impact v0x02 (ChaCha20-Poly1305, défaut) : NON exploitable de
 * l'extérieur — le nonce ChaCha20 est frais à chaque message et
 * ré-randomise entièrement le ct_alg interne avant publication. C'est
 * une utilisation standard et sûre de ChaCha20-Poly1305 (clé fixe, nonce
 * frais par message).
 *
 * Impact v0x03 (Poly1305 seul, expérimental, roadmap §2) : CRITIQUE.
 * ct_alg est publié SANS ré-encryption — la réutilisation de keystream
 * entre deux messages partageant un handle expose XOR(pt1, pt2) à tout
 * observateur du ciphertext. Combiner v0x03 avec un handle bulk partagé
 * (exactement le scénario visé par §3.3/§6 pour la cible >150 MB/s)
 * casserait la confidentialité dès le deuxième message.
 *
 * FIX appliqué ICI (cagoule_api.c uniquement, pas de modification du
 * code Python v3.0.0 déjà audité/fermé) :
 *   - cagoule_encrypt_v3()/decrypt_v3() (mono-message, sans handle) :
 *     IV = HKDF(k_master, "CAGOULE_CTR_V30", 8) — FORMULE INCHANGÉE,
 *     compatible bit-à-bit avec encrypt_ctr()/decrypt_ctr() Python.
 *     Sûr tel quel : k_master est unique par appel (salt frais).
 *   - cagoule_encrypt_with_handle()/decrypt_with_handle() (bulk,
 *     k_master potentiellement partagé entre appels) :
 *     IV = HKDF(k_master, "CAGOULE_CTR_V31_BULK" || msg_salt, 8) —
 *     label HKDF distinct ET salt du message mélangé. Conséquence
 *     volontaire : un ciphertext produit par *_with_handle() n'est PAS
 *     déchiffrable par decrypt_ctr() Python ni par cagoule_decrypt_v3()
 *     — uniquement par decrypt_with_handle(). C'est intentionnel : une
 *     divergence de format explicite et documentée vaut mieux qu'une
 *     compatibilité silencieuse qui dissimulerait la faille ci-dessus.
 *
 * Recommandation pour LASS : le même correctif (mélanger msg_salt dans
 * l'IV) devrait être rétroporté dans cipher_ctr.py::_derive_ctr_iv()
 * pour le chemin encrypt_bulk_ctr(params=...) — c'est une faille réelle
 * dans du code déjà livré, pas seulement un risque théorique pour v3.1.0.
 * Hors scope de ce sprint (ne pas modifier le cycle v3.0.0 fermé sans
 * validation), mais à traiter avant tout usage bulk en production.
 * ════════════════════════════════════════════════════════════════════
 *
 * Format wire — identique à cipher_ctr.py / cipher_ctr_raw.py (Feature 1) :
 *   0x02 : MAGIC(4) VERSION(1) SALT(32) NONCE(12) CT(n) TAG(16) — overhead 65
 *   0x03 : MAGIC(4) VERSION(1) SALT(32) CT(n) TAG(16)            — overhead 53
 *
 * Garantie d'ordre (roadmap §3.1) : le MAC est TOUJOURS vérifié dans un
 * buffer interne avant tout déchiffrement CTR vers le buffer de sortie
 * de l'appelant — aucun plaintext non authentifié n'est jamais écrit
 * dans `out`.
 */
#ifndef CAGOULE_API_H
#define CAGOULE_API_H

#include <stdint.h>
#include <stddef.h>
#include "cagoule_params.h"

#define CAGOULE_API_OK            0
#define CAGOULE_API_ERR_NULL     -1
#define CAGOULE_API_ERR_SIZE     -2
#define CAGOULE_API_ERR_AUTH     -3   /* Échec vérification MAC */
#define CAGOULE_API_ERR_FORMAT   -4   /* Magic/version invalide */
#define CAGOULE_API_ERR_KDF      -5
#define CAGOULE_API_ERR_CRYPTO   -6   /* Échec OpenSSL EVP interne */
#define CAGOULE_API_ERR_ALLOC    -7

#define CAGOULE_API_VERSION_AEAD 0x02   /* ChaCha20-Poly1305 — défaut */
#define CAGOULE_API_VERSION_RAW  0x03   /* Poly1305 seul — expérimental */

#define CAGOULE_API_OVERHEAD_AEAD 65    /* MAGIC+VERSION+SALT+NONCE+TAG */
#define CAGOULE_API_OVERHEAD_RAW  53    /* MAGIC+VERSION+SALT+TAG (corrigé, roadmap §6.1) */

/* ── Handle de clé pré-dérivée (roadmap §3.3 — amortissement bulk) ──── */
typedef struct CagouleKeyHandle CagouleKeyHandle;

/**
 * Dérive une clé complète (Argon2id + matrix + sbox + round_keys +
 * z_offset + k_stream + poly_key) et retourne un handle opaque.
 * Coût : ~Argon2id(64MiB, t=3) — à amortir sur N messages via les
 * fonctions *_with_handle().
 *
 * @return handle alloué, ou NULL en cas d'échec (KDF ou paramètres invalides)
 */
CagouleKeyHandle* cagoule_derive_key(const uint8_t* password, size_t pwd_len,
                                      const uint8_t* salt, size_t salt_len);

/** Libère le handle — zéroïse k_master/k_stream/poly_key/round_keys/z_offset. */
void cagoule_key_handle_free(CagouleKeyHandle* handle);

/* ── Tailles de buffer requises (à appeler avant d'allouer `out`) ───── */
static inline size_t cagoule_api_encrypt_out_len(size_t pt_len) { return pt_len + CAGOULE_API_OVERHEAD_AEAD; }
static inline size_t cagoule_api_encrypt_raw_out_len(size_t pt_len) { return pt_len + CAGOULE_API_OVERHEAD_RAW; }
static inline size_t cagoule_api_decrypt_out_len(size_t ct_len) {
    return (ct_len >= CAGOULE_API_OVERHEAD_AEAD) ? (ct_len - CAGOULE_API_OVERHEAD_AEAD) : 0;
}
static inline size_t cagoule_api_decrypt_raw_out_len(size_t ct_len) {
    return (ct_len >= CAGOULE_API_OVERHEAD_RAW) ? (ct_len - CAGOULE_API_OVERHEAD_RAW) : 0;
}

/* ── Chemin bulk (handle partagé) — VERSION 0x02 (défaut, ChaCha20) ── */

/**
 * @param out      Buffer appelant, taille >= cagoule_api_encrypt_out_len(pt_len)
 * @param out_len  IN: capacité de out. OUT: octets effectivement écrits.
 */
int cagoule_encrypt_with_handle(CagouleKeyHandle* handle,
                                 const uint8_t* pt, size_t pt_len,
                                 uint8_t* out, size_t* out_len);

int cagoule_decrypt_with_handle(CagouleKeyHandle* handle,
                                 const uint8_t* ct, size_t ct_len,
                                 uint8_t* out, size_t* out_len);

/* ── Chemin bulk — VERSION 0x03 (expérimental, Poly1305 seul) ───────
 * Gate double, comme cipher_ctr_raw.py (Feature 1) : nécessite
 * allow_experimental=1 ET la variable d'environnement
 * CAGOULE_EXPERIMENTAL_NO_AEAD=1 au moment de l'appel (vérifié à
 * l'exécution, pas seulement à la compilation — voir cagoule_api.c). */
int cagoule_encrypt_with_handle_raw(CagouleKeyHandle* handle, int allow_experimental,
                                     const uint8_t* pt, size_t pt_len,
                                     uint8_t* out, size_t* out_len);

int cagoule_decrypt_with_handle_raw(CagouleKeyHandle* handle, int allow_experimental,
                                     const uint8_t* ct, size_t ct_len,
                                     uint8_t* out, size_t* out_len);

/* ── Chemin mono-message (derive + crypt + free en un appel) ─────────
 * VERSION 0x02 — compatible bit-à-bit avec encrypt_ctr()/decrypt_ctr()
 * Python (même formule IV, pas de salt bulk-safe nécessaire ici car
 * k_master est toujours unique par appel). */
int cagoule_encrypt_v3(const uint8_t* password, size_t pwd_len,
                        const uint8_t* pt, size_t pt_len,
                        uint8_t* out, size_t* out_len);

int cagoule_decrypt_v3(const uint8_t* password, size_t pwd_len,
                        const uint8_t* ct, size_t ct_len,
                        uint8_t* out, size_t* out_len);

#endif /* CAGOULE_API_H */
