/*
 * k_pke_decrypt.h
 *
 * Interfaz publica de k_pke_decrypt.c — K-PKE.Decrypt (FIPS 203
 * Algorithm 15) para ML-KEM-512.
 */

#ifndef K_PKE_DECRYPT_H
#define K_PKE_DECRYPT_H

#include <stdint.h>

void k_pke_decrypt(const uint8_t dk_pke[768], const uint8_t c[768], uint8_t m[32]);

#endif /* K_PKE_DECRYPT_H */
