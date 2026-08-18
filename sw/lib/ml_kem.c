/*
 * ml_kem.c
 *
 * ML-KEM.KeyGen / ML-KEM.Encaps / ML-KEM.Decaps — FIPS 203 Algoritmos
 * 16/19, 17/20, 18/21, ML-KEM-512 (k=2) — Fase 5.
 *
 * Wrappers delgados sobre K-PKE (keygen/encrypt/decrypt, ya validados)
 * que agregan el manejo de z/H(ek)/rechazo implicito. Replica
 * exactamente models/kyber_ref.ml_kem_keygen()/ml_kem_encaps()/
 * ml_kem_decaps() (96/96 casos contra kyber-py en
 * test_ml_kem_protocol.py, incluyendo interoperabilidad cruzada y
 * rechazo implicito).
 */

#include <stdint.h>
#include "keccak.h"
#include "k_pke_keygen.h"
#include "k_pke_encrypt.h"
#include "k_pke_decrypt.h"

#define KYBER_K 2

/*
 * ml_kem_keygen: dk = dk_pke(768) || ek(800) || H(ek)(32) || z(32) = 1632B.
 */
void ml_kem_keygen(const uint8_t d[32], const uint8_t z[32],
                    uint8_t ek[800], uint8_t dk[1632]) {
    uint8_t dk_pke[768];
    k_pke_keygen(d, ek, dk_pke);

    uint8_t h_ek[32];
    sha3_256(ek, 800, h_ek);

    int idx = 0;
    for (int i = 0; i < 768; i++) dk[idx++] = dk_pke[i];
    for (int i = 0; i < 800; i++) dk[idx++] = ek[i];
    for (int i = 0; i < 32; i++) dk[idx++] = h_ek[i];
    for (int i = 0; i < 32; i++) dk[idx++] = z[i];
}

/*
 * ml_kem_encaps: (K,r) = G(m || H(ek)); c = K-PKE.Encrypt(ek, m, r).
 */
void ml_kem_encaps(const uint8_t ek[800], const uint8_t m[32],
                    uint8_t K[32], uint8_t c[768]) {
    uint8_t h_ek[32];
    sha3_256(ek, 800, h_ek);

    uint8_t g_input[64];
    for (int i = 0; i < 32; i++) g_input[i] = m[i];
    for (int i = 0; i < 32; i++) g_input[32 + i] = h_ek[i];

    uint8_t g_out[64];
    sha3_512(g_input, 64, g_out);
    for (int i = 0; i < 32; i++) K[i] = g_out[i];
    const uint8_t *r = g_out + 32;

    k_pke_encrypt(ek, m, r, c);
}

/*
 * ml_kem_decaps: recupera K desde dk(1632) y c(768), con rechazo
 * implicito (si el ciphertext re-encriptado no coincide, devuelve
 * K_bar=J(z||c) en vez del K real).
 *
 * NOTA (misma que kyber_ref.py): la comparacion c==c_prime deberia ser
 * constant-time en una implementacion de produccion — aca, igual que
 * en el modelo de referencia Python, se usa comparacion simple por
 * claridad (este firmware es una pieza de portafolio/demostracion de
 * arquitectura, no una implementacion endurecida contra side-channels
 * de software; el analisis constant-time del proyecto se centro en
 * las 8 instrucciones vectoriales de la Fase 3/4, ver
 * isa_vectorial_kyber.docx seccion 4).
 */
void ml_kem_decaps(const uint8_t dk[1632], const uint8_t c[768], uint8_t K[32]) {
    const uint8_t *dk_pke = dk;
    const uint8_t *ek_pke = dk + 384 * KYBER_K;
    const uint8_t *h = dk + 768 * KYBER_K + 32;
    const uint8_t *z = dk + 768 * KYBER_K + 64;

    uint8_t m_prime[32];
    k_pke_decrypt(dk_pke, c, m_prime);

    uint8_t g_input[64];
    for (int i = 0; i < 32; i++) g_input[i] = m_prime[i];
    for (int i = 0; i < 32; i++) g_input[32 + i] = h[i];
    uint8_t g_out[64];
    sha3_512(g_input, 64, g_out);
    const uint8_t *K_prime = g_out;
    const uint8_t *r_prime = g_out + 32;

    uint8_t j_input[800]; /* z(32) + c(768) = 800 bytes exactos */
    for (int i = 0; i < 32; i++) j_input[i] = z[i];
    for (int i = 0; i < 768; i++) j_input[32 + i] = c[i];
    uint8_t K_bar[32];
    shake256(j_input, 800, K_bar, 32);

    uint8_t c_prime[768];
    k_pke_encrypt(ek_pke, m_prime, r_prime, c_prime);

    int equal = 1;
    for (int i = 0; i < 768; i++) {
        if (c[i] != c_prime[i]) equal = 0;
    }

    const uint8_t *selected = equal ? K_prime : K_bar;
    for (int i = 0; i < 32; i++) K[i] = selected[i];
}
