/*
 * test_keccak_native.c
 *
 * Validacion NATIVA (compilada para el sandbox, no RV32I) de keccak.c
 * contra vectores de prueba generados con hashlib de Python (Fase 5).
 * Iteracion rapida sobre bugs de algoritmo antes de cross-compilar a
 * RV32I y correr en el core real — mismo patron de "validar el modelo
 * computacional en un entorno rapido antes del hardware" que se uso con
 * kyber_ref.py en las Fases 3/4.
 */

#include <stdio.h>
#include <string.h>
#include "../../lib/keccak.h"
#include "test_keccak_vectors.h"

static int errors = 0;

static void check_bytes(const char *label, const uint8_t *got, const uint8_t *expected, size_t len) {
    if (memcmp(got, expected, len) == 0) {
        printf("OK   [%s]\n", label);
    } else {
        printf("FAIL [%s]: ", label);
        printf("got=");
        for (size_t i = 0; i < len; i++) printf("%02x", got[i]);
        printf(" expected=");
        for (size_t i = 0; i < len; i++) printf("%02x", expected[i]);
        printf("\n");
        errors++;
    }
}

int main(void) {
    uint8_t out[64];

    /* --- SHA3-256 --- */
    sha3_256(msg_empty, msg_empty_len, out);
    check_bytes("sha3_256(empty)", out, expected_sha3_256_empty, 32);

    sha3_256(msg_abc, msg_abc_len, out);
    check_bytes("sha3_256(abc)", out, expected_sha3_256_abc, 32);

    sha3_256(msg_quickfox, msg_quickfox_len, out);
    check_bytes("sha3_256(quickfox)", out, expected_sha3_256_quickfox, 32);

    /* --- SHAKE128 (32 bytes de salida) --- */
    shake128(msg_empty, msg_empty_len, out, 32);
    check_bytes("shake128(empty,32)", out, expected_shake128_empty, 32);

    shake128(msg_abc, msg_abc_len, out, 32);
    check_bytes("shake128(abc,32)", out, expected_shake128_abc, 32);

    shake128(msg_quickfox, msg_quickfox_len, out, 32);
    check_bytes("shake128(quickfox,32)", out, expected_shake128_quickfox, 32);

    /* --- SHAKE256 (32 bytes de salida) --- */
    shake256(msg_empty, msg_empty_len, out, 32);
    check_bytes("shake256(empty,32)", out, expected_shake256_empty, 32);

    shake256(msg_abc, msg_abc_len, out, 32);
    check_bytes("shake256(abc,32)", out, expected_shake256_abc, 32);

    shake256(msg_quickfox, msg_quickfox_len, out, 32);
    check_bytes("shake256(quickfox,32)", out, expected_shake256_quickfox, 32);

    printf("\n");
    if (errors == 0) {
        printf("PASS: keccak.c coincide con hashlib de Python en todos los casos.\n");
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s).\n", errors);
        return 1;
    }
}
