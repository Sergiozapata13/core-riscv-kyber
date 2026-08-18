/*
 * sample_ntt.h
 *
 * Interfaz publica de sample_ntt.c — SampleNTT (muestreo por rechazo)
 * para generar la matriz A de Kyber/ML-KEM.
 */

#ifndef SAMPLE_NTT_H
#define SAMPLE_NTT_H

#include <stdint.h>

#define XOF_BUFFER_BYTES 840

int sample_ntt(const uint8_t input_bytes[XOF_BUFFER_BYTES], int16_t out[256]);

#endif /* SAMPLE_NTT_H */
