/*
 * poly_ntt.h
 *
 * Interfaz publica de poly_ntt.c — NTT/INTT escalar, multiplicacion
 * punto a punto, suma/resta de polinomios (firmware de referencia sin
 * aceleracion vectorial).
 */

#ifndef POLY_NTT_H
#define POLY_NTT_H

#include <stdint.h>

void poly_ntt(int16_t r[256]);
void poly_intt(int16_t r[256]);
void poly_pointwise_mul(const int16_t a[256], const int16_t b[256], int16_t out[256]);
void poly_add(const int16_t a[256], const int16_t b[256], int16_t out[256]);
void poly_sub(const int16_t a[256], const int16_t b[256], int16_t out[256]);

#endif /* POLY_NTT_H */
