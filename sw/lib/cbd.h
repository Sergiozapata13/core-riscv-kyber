/*
 * cbd.h
 *
 * Interfaz publica de cbd.c — muestreo de la distribucion binomial
 * centrada (CBD) para el ruido de Kyber/ML-KEM.
 */

#ifndef CBD_H
#define CBD_H

#include <stdint.h>

void cbd_sample(const uint8_t *input_bytes, unsigned int eta, int16_t out[256]);

#endif /* CBD_H */
