/*
 * k_pke_keygen.c
 *
 * K-PKE.KeyGen — FIPS 203 Algorithm 13, ML-KEM-512 (k=2, eta1=3) —
 * Fase 5, firmware de referencia (sin aceleracion vectorial).
 *
 * Orquesta keccak.c (G=SHA3-512), mlkem_common.c (matriz A, vectores de
 * ruido — compartido con k_pke_encrypt.c), poly_ntt.c (NTT,
 * multiplicacion, suma), y pack.c (ByteEncode) — replica EXACTAMENTE
 * la orquestacion ya validada en models/kyber_ref.k_pke_keygen()
 * (96/96 casos contra kyber-py en test_ml_kem_protocol.py).
 *
 * Detalles no obvios, confirmados contra el codigo fuente real de
 * kyber-py antes de implementar (ver models/kyber_ref.py para las
 * notas completas):
 *   - rho,sigma = G(d || k) — el byte k=2 es separacion de dominio,
 *     NO se omite.
 *   - A[i][j] = SampleNTT(XOF(rho, j, i)) — el orden es (j,i), no (i,j)
 *     (implementado en mlkem_common.c).
 *   - t_hat[i] = sum_j( A[i][j] * s_hat[j] ) + e_hat[i] — indice NORMAL
 *     (no transpuesto; la transposicion solo aplica en encrypt).
 */

#include <stdint.h>
#include "keccak.h"
#include "poly_ntt.h"
#include "pack.h"
#include "mlkem_common.h"

#define KYBER_K 2
#define KYBER_ETA1 3
#define KYBER_N 256

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

    int16_t A[MLKEM_K_MAX][MLKEM_K_MAX][KYBER_N];
    generate_matrix(rho, KYBER_K, A);

    int N = 0;
    int16_t s[MLKEM_K_MAX][KYBER_N];
    int16_t e[MLKEM_K_MAX][KYBER_N];
    generate_error_vector(sigma, KYBER_ETA1, KYBER_K, &N, s);
    generate_error_vector(sigma, KYBER_ETA1, KYBER_K, &N, e);

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
