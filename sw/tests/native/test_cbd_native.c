/*
 * test_cbd_native.c
 *
 * Validacion NATIVA de cbd.c contra vectores generados desde
 * kyber_ref.cbd() (Fase 5) — mismo patron que test_keccak_native.c.
 */

#include <stdio.h>
#include "../../lib/cbd.h"
#include "test_cbd_vectors.h"

static int errors = 0;

static void check_poly(const char *label, const int16_t *got, const int16_t *expected, int n) {
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (got[i] != expected[i]) {
            ok = 0;
            printf("FAIL [%s_coef_%d]: got=%d esperado=%d\n", label, i, got[i], expected[i]);
            errors++;
        }
    }
    if (ok) printf("OK   [%s]: %d/%d coeficientes correctos\n", label, n, n);
}

int main(void) {
    int16_t out[256];

    cbd_sample(test_cbd_input_eta3, 3, out);
    check_poly("cbd_eta3", out, test_cbd_expected_eta3, 256);

    cbd_sample(test_cbd_input_eta2, 2, out);
    check_poly("cbd_eta2", out, test_cbd_expected_eta2, 256);

    printf("\n");
    if (errors == 0) {
        printf("PASS: cbd.c coincide con kyber_ref.cbd() en todos los casos.\n");
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s).\n", errors);
        return 1;
    }
}
