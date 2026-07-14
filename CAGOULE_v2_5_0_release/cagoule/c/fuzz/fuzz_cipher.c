/**
 * fuzz_cipher.c — libFuzzer harness for CAGOULE decrypt
 *                 CAGOULE v2.5.4
 *
 * Build:
 *   clang -O1 -g -fsanitize=fuzzer,address,undefined \
 *         -Iinclude fuzz/fuzz_cipher.c -L. -lcagoule -Wl,-rpath,. -o fuzz_cipher
 *
 * Run:
 *   ./fuzz_cipher -max_len=65536 -runs=1000000
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "cagoule_cipher.h"
#include "cagoule_matrix.h"
#include "cagoule_sbox.h"

/* ── Fixed test parameters ────────────────────────────────────────── */
static CagouleMatrix* g_mat = NULL;
static CagouleSBox64  g_sbox;
static uint64_t       g_rk[64];
static const uint64_t P = 10441487724840939323ULL;

static void setup(void) {
    uint64_t nodes[16];
    for (int i = 0; i < 16; i++)
        nodes[i] = (uint64_t)((i + 1) * 7 + 3) % P;
    for (int j = 0; j < (int)(sizeof(nodes)/sizeof(nodes[0])); j++) {
        for (int k = 0; k < j; k++) {
            while (nodes[j] == nodes[k])
                nodes[j] = (nodes[j] + 1) % P;
        }
    }
    g_mat = cagoule_matrix_build(nodes, 16, P);
    cagoule_sbox_init(&g_sbox, P, 2147483693ULL, 3221225473ULL);
    for (int i = 0; i < 64; i++)
        g_rk[i] = (uint64_t)(i * 1234567891011ULL % P) + 1;
}

/* ── libFuzzer entry point ────────────────────────────────────────── */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static int initialized = 0;
    if (!initialized) { setup(); initialized = 1; }

    /* Minimum: 1 block = 16 uint64_t = 128 bytes */
    if (size < 128) return 0;

    /* Use first byte to select test mode */
    uint8_t mode = data[0];
    const uint8_t *ct = data + 1;
    size_t ct_len = size - 1;

    /* Round down to block boundary (16 * 8 = 128 bytes per block) */
    size_t n_blocks = ct_len / 128;
    if (n_blocks == 0) return 0;
    if (n_blocks > 1024) n_blocks = 1024;  /* Cap at 16KB */

    size_t pt_size = n_blocks * 16;
    uint8_t *out = (uint8_t *)malloc(pt_size);
    if (!out) return 0;

    /* Mode 0: Basic decrypt with no Z-offset */
    /* Mode 1: Decrypt with Z-offset (use first 16 uint64 of input if available) */
    /* Mode 2: Decrypt with all-zero Z-offset */
    /* Mode 3: Decrypt with max-value Z-offset */

    const uint64_t *zo = NULL;
    size_t nzo = 0;

    if (mode & 1) {
        /* Use Z-offset from input if we have enough bytes */
        size_t zo_needed = 1 + n_blocks * 128 + 128;  /* mode + ct + z_offset */
        if (size >= zo_needed) {
            zo = (const uint64_t *)(data + 1 + n_blocks * 128);
            nzo = 16;
        }
        /* else: not enough bytes, zo stays NULL — no Z-offset applied */
    }

    /* Call decrypt — we only care about crashes, not return value */
    cagoule_cbc_decrypt(ct, n_blocks, out, pt_size,
                         g_mat, &g_sbox, g_rk, 64, P, zo, nzo);

    /* Also test encrypt with same input (for roundtrip fuzzing) */
    if (mode & 2) {
        size_t enc_ct_size = n_blocks * 16 * 8;  /* p_bytes = 8 for P≈2^64 */
        uint8_t *enc_out = (uint8_t *)malloc(enc_ct_size);
        if (enc_out) {
            /* Use the decrypted output as plaintext (may be invalid PKCS7) */
            cagoule_cbc_encrypt(out, n_blocks, enc_out, enc_ct_size,
                                g_mat, &g_sbox, g_rk, 64, P, zo, nzo);
            free(enc_out);
        }
    }

    free(out);
    return 0;
}
