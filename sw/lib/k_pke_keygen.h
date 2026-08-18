/*
 * k_pke_keygen.h
 *
 * Interfaz publica de k_pke_keygen.c — K-PKE.KeyGen (FIPS 203
 * Algorithm 13) para ML-KEM-512.
 */

#ifndef K_PKE_KEYGEN_H
#define K_PKE_KEYGEN_H

#include <stdint.h>

void k_pke_keygen(const uint8_t d[32], uint8_t ek_pke[800], uint8_t dk_pke[768]);

#endif /* K_PKE_KEYGEN_H */
