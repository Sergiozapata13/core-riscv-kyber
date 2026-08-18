/*
 * test_pack_native.c
 *
 * Validacion NATIVA de pack.c contra vectores generados desde
 * kyber_ref.py (Fase 5) — mismo patron que test_keccak_native.c y
 * test_cbd_native.c.
 */

#include <stdio.h>
#include "../../lib/pack.h"
#include "test_pack_vectors.h"

static int errors = 0;

static void check_i16(const char *label, const int16_t *got, const int16_t *expected, int n) {
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (got[i] != expected[i]) {
            ok = 0;
            printf("FAIL [%s_%d]: got=%d esperado=%d\n", label, i, got[i], expected[i]);
            errors++;
        }
    }
    if (ok) printf("OK   [%s]: %d/%d correctos\n", label, n, n);
}

static void check_u8(const char *label, const uint8_t *got, const uint8_t *expected, int n) {
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (got[i] != expected[i]) {
            ok = 0;
            printf("FAIL [%s_%d]: got=%u esperado=%u\n", label, i, got[i], expected[i]);
            errors++;
        }
    }
    if (ok) printf("OK   [%s]: %d/%d correctos\n", label, n, n);
}

static void check_u16(const char *label, const uint16_t *got, const uint16_t *expected, int n) {
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (got[i] != expected[i]) {
            ok = 0;
            printf("FAIL [%s_%d]: got=%u esperado=%u\n", label, i, got[i], expected[i]);
            errors++;
        }
    }
    if (ok) printf("OK   [%s]: %d/%d correctos\n", label, n, n);
}

int main(void) {
    /* --- byte_encode_d12 / byte_decode_d12 --- */
    uint8_t encoded[384];
    byte_encode_d12(test_pack_coeffs, encoded);
    check_u8("byte_encode_d12", encoded, test_pack_encoded_d12, 384);

    int16_t decoded[256];
    byte_decode_d12(test_pack_encoded_d12, decoded);
    check_i16("byte_decode_d12", decoded, test_pack_coeffs, 256);

    /* --- compress/decompress, d=10 --- */
    uint16_t compressed10[256];
    poly_compress(test_pack_coeffs, 10, compressed10);
    check_u16("compress_d10", compressed10, test_pack_compressed_d10, 256);

    int16_t decompressed10[256];
    poly_decompress(compressed10, 10, decompressed10);
    check_i16("decompress_d10", decompressed10, test_pack_decompressed_d10, 256);

    /* --- compress/decompress, d=4 --- */
    uint16_t compressed4[256];
    poly_compress(test_pack_coeffs, 4, compressed4);
    check_u16("compress_d4", compressed4, test_pack_compressed_d4, 256);

    int16_t decompressed4[256];
    poly_decompress(compressed4, 4, decompressed4);
    check_i16("decompress_d4", decompressed4, test_pack_decompressed_d4, 256);

    printf("\n");
    if (errors == 0) {
        printf("PASS: pack.c coincide con el modelo de referencia en todos los casos.\n");
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s).\n", errors);
        return 1;
    }
}
