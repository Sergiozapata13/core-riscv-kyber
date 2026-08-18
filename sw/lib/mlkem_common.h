/*
 * mlkem_common.h
 *
 * Interfaz publica de mlkem_common.c — generate_matrix y
 * generate_error_vector, compartidas entre K-PKE.KeyGen y
 * K-PKE.Encrypt.
 *
 * MLKEM_K_MAX/MLKEM_ETA_MAX: cotas superiores usadas para dimensionar
 * arrays estaticos (k=2..4 segun el nivel de seguridad, eta=2..3) —
 * este proyecto usa ML-KEM-512 (k=2, eta1=3, eta2=2) exclusivamente,
 * pero las cotas se dejan un poco holgadas (k<=4, eta<=3) por si
 * se extiende a ML-KEM-768/1024 en el futuro (documentado como
 * posible trabajo futuro, no implementado).
 */

#ifndef MLKEM_COMMON_H
#define MLKEM_COMMON_H

#include <stdint.h>

#define MLKEM_N 256
#define MLKEM_K_MAX 4
#define MLKEM_ETA_MAX 3

void generate_matrix(const uint8_t rho[32], int k, int16_t A[MLKEM_K_MAX][MLKEM_K_MAX][MLKEM_N]);
void generate_error_vector(const uint8_t sigma[32], unsigned int eta, int k, int *N,
                            int16_t out[MLKEM_K_MAX][MLKEM_N]);

#endif /* MLKEM_COMMON_H */
