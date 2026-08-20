/*
 * test_ml_kem_multiseed_native.c
 *
 * Validacion NATIVA de ml_kem.c con MULTIPLES semillas distintas — Fase
 * 5, cierra el riesgo de que la correctitud del firmware C dependiera,
 * por coincidencia, de los valores particulares de la unica semilla
 * fija usada hasta ahora en todo el pipeline C (test_ml_kem_native.c,
 * y todos los firmwares corridos en el core real).
 */

#include <stdio.h>
#include "../../lib/ml_kem.h"
#include "test_ml_kem_multiseed_vectors.h"

static int errors = 0;

static int check(const char *label, const uint8_t *got, const uint8_t *expected, int n) {
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
        printf("  OK   [%s]: %d/%d bytes correctos\n", label, n, n);
    } else {
        printf("  FAIL [%s]: primer byte incorrecto en indice %d (got=%u esperado=%u)\n",
               label, first_fail, got[first_fail], expected[first_fail]);
    }
    return ok;
}

static void run_seed(int idx, const uint8_t *d, const uint8_t *z, const uint8_t *m,
                      const uint8_t *ek_expected, const uint8_t *dk_expected,
                      const uint8_t *K_expected, const uint8_t *c_expected,
                      const uint8_t *c_corrupted, const uint8_t *K_rejected_expected) {
    printf("--- Semilla %d ---\n", idx);

    uint8_t ek[800];
    uint8_t dk[1632];
    ml_kem_keygen(d, z, ek, dk);
    check("keygen_ek", ek, ek_expected, 800);
    check("keygen_dk", dk, dk_expected, 1632);

    uint8_t K[32];
    uint8_t c[768];
    ml_kem_encaps(ek, m, K, c);
    check("encaps_K", K, K_expected, 32);
    check("encaps_c", c, c_expected, 768);

    uint8_t K_decap[32];
    ml_kem_decaps(dk, c, K_decap);
    check("decaps_normal", K_decap, K_expected, 32);

    uint8_t K_rejected[32];
    ml_kem_decaps(dk, c_corrupted, K_rejected);
    check("decaps_rechazo_implicito", K_rejected, K_rejected_expected, 32);
}

int main(void) {
    run_seed(0, seed0_d, seed0_z, seed0_m, seed0_ek_expected, seed0_dk_expected,
              seed0_K_expected, seed0_c_expected, seed0_c_corrupted, seed0_K_rejected_expected);
    run_seed(1, seed1_d, seed1_z, seed1_m, seed1_ek_expected, seed1_dk_expected,
              seed1_K_expected, seed1_c_expected, seed1_c_corrupted, seed1_K_rejected_expected);
    run_seed(2, seed2_d, seed2_z, seed2_m, seed2_ek_expected, seed2_dk_expected,
              seed2_K_expected, seed2_c_expected, seed2_c_corrupted, seed2_K_rejected_expected);
    run_seed(3, seed3_d, seed3_z, seed3_m, seed3_ek_expected, seed3_dk_expected,
              seed3_K_expected, seed3_c_expected, seed3_c_corrupted, seed3_K_rejected_expected);
    run_seed(4, seed4_d, seed4_z, seed4_m, seed4_ek_expected, seed4_dk_expected,
              seed4_K_expected, seed4_c_expected, seed4_c_corrupted, seed4_K_rejected_expected);

    printf("\n");
    if (errors == 0) {
        printf("PASS: ml_kem.c coincide con kyber_ref.py en %d semillas distintas (%d verificaciones cada una).\n",
               N_SEEDS, 7);
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s) en total.\n", errors);
        return 1;
    }
}
