/*
 * test_pack_generic_native.c
 *
 * Validacion NATIVA de byte_encode_generic/byte_decode_generic (pack.c)
 * contra vectores generados desde kyber_ref.byte_encode(), para
 * d=1,4,10,12 — Fase 5.
 */

#include <stdio.h>
#include "../../lib/pack.h"
#include "test_pack_generic_vectors.h"

static int errors = 0;

static void check_encode(const char *label, const uint8_t *got, const uint8_t *expected, int n) {
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (got[i] != expected[i]) {
            ok = 0;
            errors++;
        }
    }
    printf("%s [%s]: %d/%d bytes\n", ok ? "OK  " : "FAIL", label, n, n);
}

static void check_roundtrip(const char *label, const int16_t *got, const uint16_t *expected, int n) {
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (got[i] != (int16_t)expected[i]) {
            ok = 0;
            errors++;
        }
    }
    printf("%s [%s]: %d/%d coeficientes\n", ok ? "OK  " : "FAIL", label, n, n);
}

int main(void) {
    uint8_t buf12[384], buf10[320], buf4[128], buf1[32];
    int16_t decoded[256];

    byte_encode_generic(test_pack_gen_coeffs_d1, 1, buf1);
    check_encode("encode_d1", buf1, test_pack_gen_encoded_d1, 32);
    byte_decode_generic(test_pack_gen_encoded_d1, 1, decoded);
    check_roundtrip("decode_d1", decoded, test_pack_gen_coeffs_d1, 256);

    byte_encode_generic(test_pack_gen_coeffs_d4, 4, buf4);
    check_encode("encode_d4", buf4, test_pack_gen_encoded_d4, 128);
    byte_decode_generic(test_pack_gen_encoded_d4, 4, decoded);
    check_roundtrip("decode_d4", decoded, test_pack_gen_coeffs_d4, 256);

    byte_encode_generic(test_pack_gen_coeffs_d10, 10, buf10);
    check_encode("encode_d10", buf10, test_pack_gen_encoded_d10, 320);
    byte_decode_generic(test_pack_gen_encoded_d10, 10, decoded);
    check_roundtrip("decode_d10", decoded, test_pack_gen_coeffs_d10, 256);

    byte_encode_generic(test_pack_gen_coeffs_d12, 12, buf12);
    check_encode("encode_d12", buf12, test_pack_gen_encoded_d12, 384);
    byte_decode_generic(test_pack_gen_encoded_d12, 12, decoded);
    check_roundtrip("decode_d12", decoded, test_pack_gen_coeffs_d12, 256);

    printf("\n");
    if (errors == 0) {
        printf("PASS: byte_encode_generic/byte_decode_generic correctos para d=1,4,10,12.\n");
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s).\n", errors);
        return 1;
    }
}
