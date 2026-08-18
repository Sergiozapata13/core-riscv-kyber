/*
 * ml_kem.h
 *
 * Interfaz publica de ml_kem.c — ML-KEM.KeyGen/Encaps/Decaps completos
 * (FIPS 203), ML-KEM-512.
 */

#ifndef ML_KEM_H
#define ML_KEM_H

#include <stdint.h>

void ml_kem_keygen(const uint8_t d[32], const uint8_t z[32],
                    uint8_t ek[800], uint8_t dk[1632]);

void ml_kem_encaps(const uint8_t ek[800], const uint8_t m[32],
                    uint8_t K[32], uint8_t c[768]);

void ml_kem_decaps(const uint8_t dk[1632], const uint8_t c[768], uint8_t K[32]);

#endif /* ML_KEM_H */
