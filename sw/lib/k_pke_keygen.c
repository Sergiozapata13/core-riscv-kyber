/*
 * k_pke_keygen.c
 *
 * K-PKE.KeyGen — FIPS 203 Algorithm 13, ML-KEM-512 (k=2, eta1=3) —
 * Fase 5, firmware de referencia (sin aceleracion vectorial).
 *
 * Orquesta keccak.c (G=SHA3-512, PRF=SHAKE256, XOF=SHAKE128),
 * cbd.c (muestreo de ruido), sample_ntt.c (muestreo de la matriz A),
 * poly_ntt.c (NTT, multiplicacion, suma), y pack.c (ByteEncode) —
 * replica EXACTAMENTE la orquestacion ya validada en
 * models/kyber_ref.k_pke_keygen() (96/96 casos contra kyber-py en
 * test_ml_kem_protocol.py).
 *
 * Detalles no obvios, confirmados contra el codigo fuente real de
 * kyber-py antes de implementar (ver models/kyber_ref.py para las
 * notas completas):
 *   - rho,sigma = G(d || k) — el byte k=2 es separacion de dominio,
 *     NO se omite.
 *   - A[i][j] = SampleNTT(XOF(rho, j, i)) — el orden es (j,i), no (i,j).
 *   - t_hat[i] = sum_j( A[i][j] * s_hat[j] ) + e_hat[i] — indice NORMAL
 *     (no transpuesto; la transposicion solo aplica en encrypt).
 */

#include <stdint.h>
#include "keccak.h"
#include "cbd.h"
#include "sample_ntt.h"
#include "poly_ntt.h"
#include "pack.h"

#define KYBER_K 2
#define KYBER_ETA1 3
#define KYBER_N 256

/*
 * Genera la matriz A completa: A[i][j], i,j en [0,k).
 * XOF(rho, j, i) -- ojo, el orden de los indices dentro del XOF es
 * (j,i), no (i,j) (ver nota de cabecera).
 */
static void generate_matrix(const uint8_t rho[32], int16_t A[KYBER_K][KYBER_K][KYBER_N]) {
    for (int i = 0; i < KYBER_K; i++) {
        for (int j = 0; j < KYBER_K; j++) {
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

/*
 * Genera k polinomios de ruido CBD(eta) desde PRF(sigma, N),
 * PRF(sigma, N+1), ... — igual que kyber_ref._generate_error_vector().
 */
static void generate_error_vector(const uint8_t sigma[32], unsigned int eta, int *N,
                                   int16_t out[KYBER_K][KYBER_N]) {
    for (int i = 0; i < KYBER_K; i++) {
        uint8_t b_byte = (uint8_t)(*N);
        uint8_t prf_input[33];
        for (int b = 0; b < 32; b++) prf_input[b] = sigma[b];
        prf_input[32] = b_byte;

        uint8_t prf_out[64 * KYBER_ETA1]; /* dimensionado para el eta mas grande usado (eta1=3) */
        shake256(prf_input, 33, prf_out, 64 * eta);

        cbd_sample(prf_out, eta, out[i]);
        (*N)++;
    }
}

/*
 * k_pke_keygen: produce ek_pke (384*k + 32 bytes = 800B para k=2) y
 * dk_pke (384*k bytes = 768B para k=2).
 */
void k_pke_keygen(const uint8_t d[32], uint8_t ek_pke[800], uint8_t dk_pke[768]) {
    uint8_t d_with_k[33];
    for (int i = 0; i < 32; i++) d_with_k[i] = d[i];
    d_with_k[32] = (uint8_t)KYBER_K;

    uint8_t g_out[64];
    sha3_512(d_with_k, 33, g_out);
    const uint8_t *rho = g_out;
    const uint8_t *sigma = g_out + 32;

    int16_t A[KYBER_K][KYBER_K][KYBER_N];
    generate_matrix(rho, A);

    int N = 0;
    int16_t s[KYBER_K][KYBER_N];
    int16_t e[KYBER_K][KYBER_N];
    generate_error_vector(sigma, KYBER_ETA1, &N, s);
    generate_error_vector(sigma, KYBER_ETA1, &N, e);

    int16_t s_hat[KYBER_K][KYBER_N];
    int16_t e_hat[KYBER_K][KYBER_N];
    for (int i = 0; i < KYBER_K; i++) {
        for (int c = 0; c < KYBER_N; c++) s_hat[i][c] = s[i][c];
        poly_ntt(s_hat[i]);
        for (int c = 0; c < KYBER_N; c++) e_hat[i][c] = e[i][c];
        poly_ntt(e_hat[i]);
    }

    /* t_hat[i] = sum_j( A[i][j] * s_hat[j] ) + e_hat[i] */
    int16_t t_hat[KYBER_K][KYBER_N];
    for (int i = 0; i < KYBER_K; i++) {
        int16_t acc[KYBER_N];
        for (int c = 0; c < KYBER_N; c++) acc[c] = 0;
        for (int j = 0; j < KYBER_K; j++) {
            int16_t prod[KYBER_N];
            poly_pointwise_mul(A[i][j], s_hat[j], prod);
            int16_t new_acc[KYBER_N];
            poly_add(acc, prod, new_acc);
            for (int c = 0; c < KYBER_N; c++) acc[c] = new_acc[c];
        }
        poly_add(acc, e_hat[i], t_hat[i]);
    }

    /* ek_pke = ByteEncode12(t_hat[0]) || ByteEncode12(t_hat[1]) || rho */
    for (int i = 0; i < KYBER_K; i++) {
        byte_encode_d12(t_hat[i], ek_pke + 384 * i);
    }
    for (int b = 0; b < 32; b++) ek_pke[384 * KYBER_K + b] = rho[b];

    /* dk_pke = ByteEncode12(s_hat[0]) || ByteEncode12(s_hat[1]) */
    for (int i = 0; i < KYBER_K; i++) {
        byte_encode_d12(s_hat[i], dk_pke + 384 * i);
    }
}
