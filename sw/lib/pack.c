/*
 * pack.c
 *
 * Empaquetado (ByteEncode/ByteDecode, FIPS 203 Algoritmos 3/4) y
 * compresion con perdida (Compress/Decompress, seccion 4.2.1) — Fase 5.
 *
 * A diferencia de keccak.c y cbd.c, este modulo SI necesita
 * multiplicacion y division genuinas (compress_coeff divide por
 * q=3329, que no es potencia de 2 y no admite una tabla de lookup
 * razonable como se hizo con el operador '%' en Keccak/CBD). Se acepta
 * conscientemente el link contra libgcc (-lgcc, que provee __mulsi3/
 * __udivsi3/__umodsi3 en software para un target sin extension M) —
 * es una dependencia estandar y razonable para firmware RV32I con
 * aritmetica genuina, distinta de las dependencias EVITABLES que se
 * eliminaron en keccak.c/cbd.c (memset/memcpy, modulo por constantes
 * pequeñas con tabla de lookup trivial).
 */

#include <stdint.h>

#define KYBER_Q 3329
#define KYBER_N 256

/*
 * byte_encode_d12: empaqueta 256 coeficientes (cada uno < q, cabe en 12
 * bits) en 384 bytes (32*12), little-endian, sin compresion ni perdida.
 * Usado para serializar t (parte publica) y s (clave secreta).
 *
 * Empaquetado bit a bit con una "ventana de bits" acumulada, mismo
 * patron que cbd_sample — evita depender de aritmetica de precision
 * arbitraria (el entero de 256*12=3072 bits que usa la implementacion
 * de referencia en Python no es representable directamente en C). No
 * hace falta inicializar out[] antes del bucle: cada uno de los 384
 * bytes se escribe exactamente una vez (256*12 bits = 3072 = 384*8,
 * multiplo exacto), asi que el bucle cubre out[] por completo.
 */
void byte_encode_d12(const int16_t coeffs[KYBER_N], uint8_t out[384]) {
    unsigned int acc = 0;
    unsigned int acc_bits = 0;
    int out_idx = 0;

    for (int i = 0; i < KYBER_N; i++) {
        acc |= ((unsigned int)coeffs[i] << acc_bits);
        acc_bits += 12;
        while (acc_bits >= 8) {
            out[out_idx] = (uint8_t)(acc & 0xFF);
            out_idx++;
            acc >>= 8;
            acc_bits -= 8;
        }
    }
}

/*
 * byte_decode_d12: inverso de byte_encode_d12. Reduce cada coeficiente
 * mod q (aunque en el caso normal de uso, un valor ya codificado con
 * byte_encode_d12 nunca excede q-1, esta reduccion es defensiva, igual
 * que la referencia FIPS 203).
 */
void byte_decode_d12(const uint8_t in[384], int16_t out[KYBER_N]) {
    unsigned int acc = 0;
    unsigned int acc_bits = 0;
    int in_idx = 0;

    for (int i = 0; i < KYBER_N; i++) {
        while (acc_bits < 12) {
            acc |= ((unsigned int)in[in_idx] << acc_bits);
            in_idx++;
            acc_bits += 8;
        }
        unsigned int coef = acc & 0xFFFu;
        acc >>= 12;
        acc_bits -= 12;

        /* Reduccion defensiva mod q: coef siempre esta en [0,4095], y
         * q=3329, asi que a lo sumo UNA resta de q basta (sin
         * necesitar el operador '%'). */
        if (coef >= KYBER_Q) coef -= KYBER_Q;
        out[i] = (int16_t)coef;
    }
}

/*
 * compress_coeff: Compress_d(x) = round((2^d/q)*x) mod 2^d.
 * Usa multiplicacion/division genuinas (ver nota de cabecera sobre
 * libgcc). d tipico: 10 (para u, ML-KEM-512) o 4 (para v).
 */
static uint16_t compress_coeff(int16_t x, unsigned int d) {
    uint32_t t = 1u << d;
    uint32_t y = ((uint32_t)x * t + (KYBER_Q / 2)) / KYBER_Q;
    /* y mod t: t es potencia de 2, asi que un AND basta (no hace falta
     * el operador '%' aca, aunque la division de arriba SI es genuina). */
    return (uint16_t)(y & (t - 1u));
}

/*
 * decompress_coeff: Decompress_d(x) = round((q/2^d)*x).
 */
static int16_t decompress_coeff(uint16_t x, unsigned int d) {
    uint32_t half = 1u << (d - 1);
    uint32_t y = ((uint32_t)KYBER_Q * (uint32_t)x + half) >> d;
    return (int16_t)y;
}

void poly_compress(const int16_t coeffs[KYBER_N], unsigned int d, uint16_t out[KYBER_N]) {
    for (int i = 0; i < KYBER_N; i++) {
        out[i] = compress_coeff(coeffs[i], d);
    }
}

void poly_decompress(const uint16_t coeffs[KYBER_N], unsigned int d, int16_t out[KYBER_N]) {
    for (int i = 0; i < KYBER_N; i++) {
        out[i] = decompress_coeff(coeffs[i], d);
    }
}
