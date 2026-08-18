/*
 * test_k_pke_decrypt_native.c
 *
 * Validacion NATIVA de k_pke_decrypt.c contra vectores generados desde
 * kyber_ref.py (Fase 5) — confirma que el ciclo completo
 * encrypt->decrypt recupera el mensaje original.
 */

#include <stdio.h>
#include "../../lib/k_pke_decrypt.h"
#include "test_k_pke_decrypt_vectors.h"

static int errors = 0;

static void check(const char *label, const uint8_t *got, const uint8_t *expected, int n) {
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (got[i] != expected[i]) {
            ok = 0;
            printf("FAIL [%s_%d]: got=%u esperado=%u\n", label, i, got[i], expected[i]);
            errors++;
        }
    }
    if (ok) printf("OK   [%s]: %d/%d bytes correctos\n", label, n, n);
}

int main(void) {
    uint8_t m[32];

    k_pke_decrypt(test_dk, test_c, m);

    check("mensaje_recuperado", m, test_m_expected, 32);

    printf("\n");
    if (errors == 0) {
        printf("PASS: k_pke_decrypt.c coincide con kyber_ref.k_pke_decrypt() — ciclo encrypt->decrypt correcto.\n");
        return 0;
    } else {
        printf("FAIL: %d discrepancia(s) detectada(s).\n", errors);
        return 1;
    }
}
