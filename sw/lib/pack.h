/*
 * pack.h
 *
 * Interfaz publica de pack.c — ByteEncode/ByteDecode (d=12) y
 * Compress/Decompress (d generico) para polinomios de Kyber/ML-KEM.
 */

#ifndef PACK_H
#define PACK_H

#include <stdint.h>

void byte_encode_d12(const int16_t coeffs[256], uint8_t out[384]);
void byte_decode_d12(const uint8_t in[384], int16_t out[256]);

void poly_compress(const int16_t coeffs[256], unsigned int d, uint16_t out[256]);
void poly_decompress(const uint16_t coeffs[256], unsigned int d, int16_t out[256]);

#endif /* PACK_H */
