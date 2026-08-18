/*
 * keccak.h
 *
 * Interfaz publica de keccak.c — SHA3-256/512, SHAKE128/256.
 */

#ifndef KECCAK_H
#define KECCAK_H

#include <stdint.h>
#include <stddef.h>

void sha3_256(const uint8_t *in, size_t inlen, uint8_t out[32]);
void sha3_512(const uint8_t *in, size_t inlen, uint8_t out[64]);
void shake128(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen);
void shake256(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen);

#endif /* KECCAK_H */
