/*
 * test_ml_kem_native.c
 *
 * Validacion NATIVA de ml_kem.c contra vectores generados desde
 * kyber_ref.py (Fase 5) — criterio de cierre del protocolo completo
 * antes de correr en el core real. Cubre keygen, encaps, decaps
 * normal, y decaps con rechazo implicito (ciphertext corrompido).
 */

#include <stdio.h>
#include "../../lib/ml_kem.h"
#include "test_ml_kem_vectors.h"

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
    uint8_t dk[1632];
    ml_kem_keygen(test_d, test_z, ek, dk);
    check("keygen_ek", ek, test_ek_expected, 800);
    check("keygen_dk", dk, test_dk_expected, 1632);

    uint8_t K[32];
    uint8_t c[768];
    ml_kem_encaps(ek, test_m, K, c);
    check("encaps_K", K, test_K_expected, 32);
    check("encaps_c", c, test_c_expected, 768);

    uint8_t K_decap[32];
    ml_kem_decaps(dk, c, K_decap);
    check("decaps_normal", K_decap, test_K_expected, 32);

    uint8_t K_rejected[32];
    ml_kem_decaps(dk, test_c_corrupted, K_rejected);
    check("decaps_rechazo_implicito", K_rejected, test_K_rejected_expected, 32);

    printf("\n");
    if (errors == 0) {
        printf("PASS: ml_kem.c coincide con kyber_ref.py en todos los casos, incluyendo rechazo implicito.\n");
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s).\n", errors);
        return 1;
    }
}
