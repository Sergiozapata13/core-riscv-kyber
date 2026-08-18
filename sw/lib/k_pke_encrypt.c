/*
 * k_pke_encrypt.c
 *
 * K-PKE.Encrypt — FIPS 203 Algorithm 14, ML-KEM-512 (k=2, eta1=3,
 * eta2=2, du=10, dv=4) — Fase 5, firmware de referencia (sin
 * aceleracion vectorial).
 *
 * Replica EXACTAMENTE la orquestacion ya validada en
 * models/kyber_ref.k_pke_encrypt() (96/96 casos contra kyber-py en
 * test_ml_kem_protocol.py, incluyendo interoperabilidad cruzada).
 *
 * Detalle no obvio, confirmado contra el codigo fuente real de
 * kyber-py antes de implementar (ver models/kyber_ref.py):
 *   - u[i] = intt( sum_j( A[j][i] * y_hat[j] ) ) + e1[i] — el indice es
 *     A[j][i], NO A[i][j] — la "transposicion" de A en encrypt no
 *     regenera nada, solo invierte el orden de acceso (A^T[i][j] =
 *     A[j][i], ya que A[i][j] fue generada UNA vez con
 *     A[i][j]=SampleNTT(XOF(rho,j,i)) en generate_matrix()).
 *   - v = intt( sum_i( t_hat[i] * y_hat[i] ) ) + e2 + mu — producto
 *     interno (mismo indice en ambos vectores), no matriz-vector.
 *
 * Generacion de e2: kyber_ref usa un contador PRF compartido N que
 * avanza secuencialmente (y: N=0,1 -> e1: N=2,3 -> e2: N=4), asi que
 * e2 es un UNICO polinomio generado con PRF(r, N=4). Se reusa
 * generate_error_vector() con k=1 para no duplicar el acceso a
 * shake256, tratandolo como un "vector de un solo elemento".
 */

#include <stdint.h>
#include "pack.h"
#include "poly_ntt.h"
#include "mlkem_common.h"

#define KYBER_K 2
#define KYBER_ETA1 3
#define KYBER_ETA2 2
#define KYBER_DU 10
#define KYBER_DV 4
#define KYBER_N 256

/*
 * k_pke_encrypt: cifra el mensaje de 32 bytes 'm' usando la clave
 * publica 'ek_pke' (800 bytes) y la aleatoriedad 'r' (32 bytes),
 * produciendo el ciphertext de 768 bytes (c1=320B + c2=128B para
 * ML-KEM-512).
 */
void k_pke_encrypt(const uint8_t ek_pke[800], const uint8_t m[32], const uint8_t r[32],
                    uint8_t c[768]) {
    const uint8_t *t_hat_bytes = ek_pke;
    const uint8_t *rho = ek_pke + 384 * KYBER_K;

    int16_t t_hat[KYBER_K][KYBER_N];
    for (int i = 0; i < KYBER_K; i++) {
        byte_decode_d12(t_hat_bytes + 384 * i, t_hat[i]);
    }

    int16_t A[MLKEM_K_MAX][MLKEM_K_MAX][KYBER_N];
    generate_matrix(rho, KYBER_K, A);

    int N = 0;
    int16_t y[MLKEM_K_MAX][KYBER_N];
    int16_t e1[MLKEM_K_MAX][KYBER_N];
    generate_error_vector(r, KYBER_ETA1, KYBER_K, &N, y);
    generate_error_vector(r, KYBER_ETA2, KYBER_K, &N, e1);

    int16_t e2_wrap[MLKEM_K_MAX][KYBER_N];
    generate_error_vector(r, KYBER_ETA2, 1, &N, e2_wrap);
    int16_t *e2 = e2_wrap[0];

    int16_t y_hat[KYBER_K][KYBER_N];
    for (int i = 0; i < KYBER_K; i++) {
        for (int c_i = 0; c_i < KYBER_N; c_i++) y_hat[i][c_i] = y[i][c_i];
        poly_ntt(y_hat[i]);
    }

    /* u[i] = intt( sum_j( A[j][i] * y_hat[j] ) ) + e1[i]  -- indice INVERTIDO (A^T) */
    int16_t u[KYBER_K][KYBER_N];
    for (int i = 0; i < KYBER_K; i++) {
        int16_t acc[KYBER_N];
        for (int c_i = 0; c_i < KYBER_N; c_i++) acc[c_i] = 0;
        for (int j = 0; j < KYBER_K; j++) {
            int16_t prod[KYBER_N];
            poly_pointwise_mul(A[j][i], y_hat[j], prod); /* A[j][i], no A[i][j] */
            int16_t new_acc[KYBER_N];
            poly_add(acc, prod, new_acc);
            for (int c_i = 0; c_i < KYBER_N; c_i++) acc[c_i] = new_acc[c_i];
        }
        poly_intt(acc);
        poly_add(acc, e1[i], u[i]);
    }

    /* mu = decompress_1( decode_1(m) ) */
    int16_t m_decoded[KYBER_N];
    byte_decode_generic(m, 1, m_decoded);
    uint16_t m_decoded_u16[KYBER_N];
    for (int c_i = 0; c_i < KYBER_N; c_i++) m_decoded_u16[c_i] = (uint16_t)m_decoded[c_i];
    int16_t mu[KYBER_N];
    poly_decompress(m_decoded_u16, 1, mu);

    /* v = intt( sum_i( t_hat[i] * y_hat[i] ) ) + e2 + mu  -- producto interno */
    int16_t acc_v[KYBER_N];
    for (int c_i = 0; c_i < KYBER_N; c_i++) acc_v[c_i] = 0;
    for (int i = 0; i < KYBER_K; i++) {
        int16_t prod[KYBER_N];
        poly_pointwise_mul(t_hat[i], y_hat[i], prod);
        int16_t new_acc[KYBER_N];
        poly_add(acc_v, prod, new_acc);
        for (int c_i = 0; c_i < KYBER_N; c_i++) acc_v[c_i] = new_acc[c_i];
    }
    poly_intt(acc_v);
    int16_t v_tmp[KYBER_N];
    poly_add(acc_v, e2, v_tmp);
    int16_t v[KYBER_N];
    poly_add(v_tmp, mu, v);

    /* c1 = ByteEncode_du(Compress_du(u[0])) || ByteEncode_du(Compress_du(u[1])) */
    const int du_bytes_per_poly = 32 * KYBER_DU;
    for (int i = 0; i < KYBER_K; i++) {
        uint16_t compressed[KYBER_N];
        poly_compress(u[i], KYBER_DU, compressed);
        byte_encode_generic(compressed, KYBER_DU, c + du_bytes_per_poly * i);
    }

    /* c2 = ByteEncode_dv(Compress_dv(v)) */
    uint16_t v_compressed[KYBER_N];
    poly_compress(v, KYBER_DV, v_compressed);
    byte_encode_generic(v_compressed, KYBER_DV, c + du_bytes_per_poly * KYBER_K);
}
