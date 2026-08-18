/*
 * test_poly_ntt_native.c
 *
 * Validacion NATIVA de poly_ntt.c contra vectores generados desde
 * kyber_ref.py (Fase 5).
 */

#include <stdio.h>
#include "../../lib/poly_ntt.h"
#include "test_poly_ntt_vectors.h"

static int errors = 0;

static void check(const char *label, const int16_t *got, const int16_t *expected, int n) {
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

int main(void) {
    int16_t a_ntt[256];
    for (int i = 0; i < 256; i++) a_ntt[i] = test_a[i];
    poly_ntt(a_ntt);
    check("ntt", a_ntt, test_a_ntt_expected, 256);

    int16_t roundtrip[256];
    for (int i = 0; i < 256; i++) roundtrip[i] = a_ntt[i];
    poly_intt(roundtrip);
    check("roundtrip", roundtrip, test_roundtrip_expected, 256);

    int16_t b_ntt[256];
    for (int i = 0; i < 256; i++) b_ntt[i] = test_b[i];
    poly_ntt(b_ntt);

    int16_t pmul[256];
    poly_pointwise_mul(a_ntt, b_ntt, pmul);
    check("pointwise_mul", pmul, test_pmul_expected, 256);

    int16_t padd[256];
    poly_add(test_a, test_b, padd);
    check("add", padd, test_padd_expected, 256);

    int16_t psub[256];
    poly_sub(test_a, test_b, psub);
    check("sub", psub, test_psub_expected, 256);

    printf("\n");
    if (errors == 0) {
        printf("PASS: poly_ntt.c coincide con kyber_ref.py en todos los casos.\n");
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s).\n", errors);
        return 1;
    }
}
