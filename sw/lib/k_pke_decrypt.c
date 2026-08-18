/*
 * k_pke_decrypt.c
 *
 * K-PKE.Decrypt — FIPS 203 Algorithm 15, ML-KEM-512 (k=2, du=10,
 * dv=4) — Fase 5, firmware de referencia (sin aceleracion vectorial).
 *
 * Mas simple que keygen/encrypt: no genera matriz A ni ruido nuevo
 * (solo decodifica el ciphertext, NTT, producto interno, resta, y
 * compresion final). Replica exactamente
 * models/kyber_ref.k_pke_decrypt() (ya validado contra kyber-py,
 * 96/96 casos en test_ml_kem_protocol.py, incluyendo
 * interoperabilidad cruzada).
 */

#include <stdint.h>
#include "pack.h"
#include "poly_ntt.h"

#define KYBER_K 2
#define KYBER_DU 10
#define KYBER_DV 4
#define KYBER_N 256

/*
 * k_pke_decrypt: recupera el mensaje de 32 bytes desde el ciphertext
 * 'c' (768 bytes) usando la clave secreta 'dk_pke' (768 bytes).
 */
void k_pke_decrypt(const uint8_t dk_pke[768], const uint8_t c[768], uint8_t m[32]) {
    const int du_bytes_per_poly = 32 * KYBER_DU;
    const uint8_t *c1 = c;
    const uint8_t *c2 = c + du_bytes_per_poly * KYBER_K;

    int16_t u[KYBER_K][KYBER_N];
    for (int i = 0; i < KYBER_K; i++) {
        int16_t u_decoded[KYBER_N];
        byte_decode_generic(c1 + du_bytes_per_poly * i, KYBER_DU, u_decoded);
        uint16_t u_decoded_u16[KYBER_N];
        for (int c_i = 0; c_i < KYBER_N; c_i++) u_decoded_u16[c_i] = (uint16_t)u_decoded[c_i];
        poly_decompress(u_decoded_u16, KYBER_DU, u[i]);
    }

    int16_t v_decoded[KYBER_N];
    byte_decode_generic(c2, KYBER_DV, v_decoded);
    uint16_t v_decoded_u16[KYBER_N];
    for (int c_i = 0; c_i < KYBER_N; c_i++) v_decoded_u16[c_i] = (uint16_t)v_decoded[c_i];
    int16_t v[KYBER_N];
    poly_decompress(v_decoded_u16, KYBER_DV, v);

    int16_t s_hat[KYBER_K][KYBER_N];
    for (int i = 0; i < KYBER_K; i++) {
        byte_decode_d12(dk_pke + 384 * i, s_hat[i]);
    }

    int16_t u_hat[KYBER_K][KYBER_N];
    for (int i = 0; i < KYBER_K; i++) {
        for (int c_i = 0; c_i < KYBER_N; c_i++) u_hat[i][c_i] = u[i][c_i];
        poly_ntt(u_hat[i]);
    }

    /* w = v - intt( sum_i( s_hat[i] * u_hat[i] ) ) */
    int16_t acc[KYBER_N];
    for (int c_i = 0; c_i < KYBER_N; c_i++) acc[c_i] = 0;
    for (int i = 0; i < KYBER_K; i++) {
        int16_t prod[KYBER_N];
        poly_pointwise_mul(s_hat[i], u_hat[i], prod);
        int16_t new_acc[KYBER_N];
        poly_add(acc, prod, new_acc);
        for (int c_i = 0; c_i < KYBER_N; c_i++) acc[c_i] = new_acc[c_i];
    }
    poly_intt(acc);
    int16_t w[KYBER_N];
    poly_sub(v, acc, w);

    /* m = ByteEncode_1(Compress_1(w)) */
    uint16_t w_compressed[KYBER_N];
    poly_compress(w, 1, w_compressed);
    byte_encode_generic(w_compressed, 1, m);
}
