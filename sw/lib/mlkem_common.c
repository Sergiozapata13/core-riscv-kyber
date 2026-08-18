/*
 * mlkem_common.c
 *
 * Funciones auxiliares compartidas entre K-PKE.KeyGen y K-PKE.Encrypt
 * (Fase 5) — extraidas para no duplicar codigo entre ambos (riesgo de
 * que diverjan con el tiempo si se mantienen copias separadas).
 *
 * generate_matrix: A[i][j] = SampleNTT(XOF(rho, j, i)) — el orden es
 * (j,i), no (i,j), confirmado contra el codigo fuente real de kyber-py
 * (ver models/kyber_ref.py para la nota completa).
 *
 * generate_error_vector: genera k polinomios CBD(eta) desde
 * PRF(sigma, N), PRF(sigma, N+1), ...
 */

#include <stdint.h>
#include "keccak.h"
#include "cbd.h"
#include "sample_ntt.h"
#include "mlkem_common.h"

void generate_matrix(const uint8_t rho[32], int k, int16_t A[MLKEM_K_MAX][MLKEM_K_MAX][MLKEM_N]) {
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            uint8_t seed[34];
            for (int b = 0; b < 32; b++) seed[b] = rho[b];
            seed[32] = (uint8_t)j;  /* primer indice del XOF: j */
            seed[33] = (uint8_t)i;  /* segundo indice del XOF: i */

            uint8_t xof_bytes[XOF_BUFFER_BYTES];
            shake128(seed, 34, xof_bytes, XOF_BUFFER_BYTES);

            int ok = sample_ntt(xof_bytes, A[i][j]);
            (void)ok; /* en un firmware de produccion, tratar ok==0 como error fatal */
        }
    }
}

void generate_error_vector(const uint8_t sigma[32], unsigned int eta, int k, int *N,
                            int16_t out[MLKEM_K_MAX][MLKEM_N]) {
    for (int i = 0; i < k; i++) {
        uint8_t b_byte = (uint8_t)(*N);
        uint8_t prf_input[33];
        for (int b = 0; b < 32; b++) prf_input[b] = sigma[b];
        prf_input[32] = b_byte;

        uint8_t prf_out[64 * MLKEM_ETA_MAX];
        shake256(prf_input, 33, prf_out, 64 * eta);

        cbd_sample(prf_out, eta, out[i]);
        (*N)++;
    }
}
