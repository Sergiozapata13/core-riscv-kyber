/*
 * test_sample_ntt_native.c
 *
 * Validacion NATIVA de sample_ntt.c contra vectores generados con
 * SHAKE128 real (hashlib) como fuente, derivados de
 * kyber_ref.sample_ntt() (Fase 5).
 */

#include <stdio.h>
#include "../../lib/sample_ntt.h"
#include "test_sample_ntt_vectors.h"

int main(void) {
    int16_t out[256];
    int errors = 0;

    int ok = sample_ntt(test_sample_ntt_input, out);
    if (!ok) {
        printf("FAIL [sample_ntt_buffer_agotado]: el buffer de 840 bytes no alcanzo\n");
        return 1;
    }

    int all_ok = 1;
    for (int i = 0; i < 256; i++) {
        if (out[i] != test_sample_ntt_expected[i]) {
            all_ok = 0;
            printf("FAIL [coef_%d]: got=%d esperado=%d\n", i, out[i], test_sample_ntt_expected[i]);
            errors++;
        }
    }
    if (all_ok) printf("OK   [sample_ntt]: 256/256 coeficientes correctos\n");

    printf("\n");
    if (errors == 0) {
        printf("PASS: sample_ntt.c coincide con kyber_ref.sample_ntt() (via SHAKE128 real).\n");
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s).\n", errors);
        return 1;
    }
}
