/*
 * test_k_pke_encrypt_native.c
 *
 * Validacion NATIVA de k_pke_encrypt.c contra vectores generados desde
 * kyber_ref.k_pke_encrypt() (Fase 5) — funcion por funcion, antes de
 * seguir con decrypt.
 */

#include <stdio.h>
#include "../../lib/k_pke_encrypt.h"
#include "test_k_pke_encrypt_vectors.h"

static int errors = 0;

static void check(const char *label, const uint8_t *got, const uint8_t *expected, int n) {
    int ok = 1;
    int first_fail = -1;
    for (int i = 0; i < n; i++) {
        if (got[i] != expected[i]) {
            ok = 0;
            if (first_fail < 0) first_fail = i;
            errors++;
        }
    }
    if (ok) {
        printf("OK   [%s]: %d/%d bytes correctos\n", label, n, n);
    } else {
        printf("FAIL [%s]: primer byte incorrecto en indice %d (got=%u esperado=%u)\n",
               label, first_fail, got[first_fail], expected[first_fail]);
    }
}

int main(void) {
    uint8_t c[768];

    k_pke_encrypt(test_ek, test_m, test_r, c);

    check("ciphertext", c, test_c_expected, 768);

    printf("\n");
    if (errors == 0) {
        printf("PASS: k_pke_encrypt.c coincide con kyber_ref.k_pke_encrypt().\n");
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s).\n", errors);
        return 1;
    }
}
