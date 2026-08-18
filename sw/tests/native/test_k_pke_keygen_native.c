/*
 * test_k_pke_keygen_native.c
 *
 * Validacion NATIVA de k_pke_keygen.c contra vectores generados desde
 * kyber_ref.k_pke_keygen() (Fase 5) — primer criterio de cierre de la
 * funcion, verificado funcion por funcion antes de seguir con
 * encrypt/decrypt.
 */

#include <stdio.h>
#include "../../lib/k_pke_keygen.h"
#include "test_k_pke_keygen_vectors.h"

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
    uint8_t ek[800];
    uint8_t dk[768];

    k_pke_keygen(test_d, ek, dk);

    check("ek_pke", ek, test_ek_expected, 800);
    check("dk_pke", dk, test_dk_expected, 768);

    printf("\n");
    if (errors == 0) {
        printf("PASS: k_pke_keygen.c coincide con kyber_ref.k_pke_keygen().\n");
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s).\n", errors);
        return 1;
    }
}
