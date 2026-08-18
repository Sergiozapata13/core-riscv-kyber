/*
 * k_pke_encrypt.h
 *
 * Interfaz publica de k_pke_encrypt.c — K-PKE.Encrypt (FIPS 203
 * Algorithm 14) para ML-KEM-512.
 */

#ifndef K_PKE_ENCRYPT_H
#define K_PKE_ENCRYPT_H

#include <stdint.h>

void k_pke_encrypt(const uint8_t ek_pke[800], const uint8_t m[32], const uint8_t r[32],
                    uint8_t c[768]);

#endif /* K_PKE_ENCRYPT_H */
