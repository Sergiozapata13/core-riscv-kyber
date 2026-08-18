/*
 * keccak.c
 *
 * Implementacion de Keccak-f[1600] y las construcciones esponja
 * SHA3-256, SHAKE128, SHAKE256 — Fase 5 (firmware Kyber end-to-end).
 *
 * Escrito en C portable (sin dependencias de libc mas alla de stdint.h),
 * para compilar tanto nativo (validacion rapida contra KATs de NIST)
 * como cross-compilado a RV32I bare-metal (sin extension M: no usa
 * multiplicacion/division en ningun punto, solo XOR/AND/NOT/rotaciones
 * — Keccak es "ARX-free", ideal para un core RV32I base).
 *
 * El estado se representa como 25 palabras de 64 bits (5x5), tal como
 * especifica FIPS 202. En un RV32I de 32 bits, cada uint64_t se maneja
 * como un par de words de 32 bits internamente por el compilador — mas
 * lento que en un target nativo de 64 bits, pero funcionalmente
 * correcto y no requiere ninguna instruccion fuera de RV32I base.
 */

#include <stdint.h>
#include <stddef.h>
#include "keccak_constants.h"

/*
 * Entorno bare-metal sin libc (-nostdlib): se evita CUALQUIER dependencia
 * implicita de libgcc/libc que el compilador podria insertar por su
 * cuenta al reconocer ciertos patrones de codigo:
 *   - El operador '%' sobre valores que no son potencia de 2 (ej. '% 5')
 *     genera una llamada a __modsi3 (rutina de software de libgcc) — se
 *     reemplaza por tablas de lookup precalculadas, ademas de ser mas
 *     barato en un core sin multiplicador/divisor de hardware.
 *   - La inicializacion '= {0}' y los bucles de copia de bytes pueden
 *     ser reconocidos por el compilador y reemplazados por llamadas a
 *     memset/memcpy — se reemplazan por funciones propias, triviales.
 */

static void mem_zero(uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++) p[i] = 0;
}

static void mem_copy(uint8_t *dst, const uint8_t *src, size_t n) {
    for (size_t i = 0; i < n; i++) dst[i] = src[i];
}

/* Tablas de lookup para evitar el operador '%' (ver nota arriba). */
static const int MOD5_PLUS1[5] = {1, 2, 3, 4, 0};   /* (x+1)%5 */
static const int MOD5_PLUS4[5] = {4, 0, 1, 2, 3};   /* (x+4)%5, equivalente a (x-1) mod 5 */
static const int MOD5_PLUS2[5] = {2, 3, 4, 0, 1};   /* (x+2)%5 */
/* PI_LANE[x][y] = (2*x + 3*y) % 5 */
static const int PI_LANE[5][5] = {
    {0, 3, 1, 4, 2},
    {2, 0, 3, 1, 4},
    {4, 2, 0, 3, 1},
    {1, 4, 2, 0, 3},
    {3, 1, 4, 2, 0},
};

static uint64_t rotl64(uint64_t x, unsigned int n) {
    n &= 63;
    if (n == 0) return x;
    return (x << n) | (x >> (64 - n));
}

/*
 * Permutacion Keccak-f[1600]: 24 rondas de theta, rho, pi, chi, iota.
 * Estado indexado state[x + 5*y], x,y en [0,5) — misma convencion que
 * gen_keccak_constants.py usa para los offsets rho.
 */
static void keccak_f1600(uint64_t state[25]) {
    for (int round = 0; round < 24; round++) {
        /* --- theta --- */
        uint64_t C[5], D[5];
        for (int x = 0; x < 5; x++) {
            C[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
        }
        for (int x = 0; x < 5; x++) {
            D[x] = C[MOD5_PLUS4[x]] ^ rotl64(C[MOD5_PLUS1[x]], 1);
        }
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                state[x + 5 * y] ^= D[x];
            }
        }

        /* --- rho + pi combinados: B[y][2x+3y mod 5] = rotl(state[x][y], rho[x][y]) --- */
        uint64_t B[25];
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                int new_x = y;
                int new_y = PI_LANE[x][y];
                B[new_x + 5 * new_y] = rotl64(state[x + 5 * y], KECCAK_RHO[x][y]);
            }
        }

        /* --- chi --- */
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                state[x + 5 * y] = B[x + 5 * y] ^
                    ((~B[MOD5_PLUS1[x] + 5 * y]) & B[MOD5_PLUS2[x] + 5 * y]);
            }
        }

        /* --- iota --- */
        state[0] ^= KECCAK_RC[round];
    }
}

/*
 * Construccion esponja generica: absorbe 'inlen' bytes de 'in' con la
 * tasa 'rate' (en bytes) y el byte de dominio 'domain' (0x06 para
 * SHA3, 0x1F para SHAKE — ver FIPS 202), y exprime 'outlen' bytes a
 * 'out'.
 */
static void keccak_sponge(const uint8_t *in, size_t inlen,
                           uint8_t domain, size_t rate,
                           uint8_t *out, size_t outlen) {
    uint8_t state_bytes[200];  /* 1600 bits = 200 bytes */
    mem_zero(state_bytes, 200);

    /* --- Fase de absorcion --- */
    size_t offset = 0;
    while (inlen - offset >= rate) {
        for (size_t i = 0; i < rate; i++) {
            state_bytes[i] ^= in[offset + i];
        }
        keccak_f1600((uint64_t *)state_bytes);
        offset += rate;
    }

    /* Ultimo bloque parcial + padding (multi-rate padding, FIPS 202
     * seccion 5.1: domain byte en la primera posicion libre, 0x80 en
     * el ultimo byte del bloque — si ambos caen en la misma posicion,
     * se OR-ean, ver el caso remaining==rate-1 mas abajo). */
    size_t remaining = inlen - offset;
    uint8_t last_block[200];
    mem_zero(last_block, 200);
    mem_copy(last_block, in + offset, remaining);
    last_block[remaining] ^= domain;
    last_block[rate - 1] ^= 0x80;

    for (size_t i = 0; i < rate; i++) {
        state_bytes[i] ^= last_block[i];
    }
    keccak_f1600((uint64_t *)state_bytes);

    /* --- Fase de exprimido (squeeze) --- */
    size_t produced = 0;
    while (produced < outlen) {
        size_t chunk = (outlen - produced < rate) ? (outlen - produced) : rate;
        mem_copy(out + produced, state_bytes, chunk);
        produced += chunk;
        if (produced < outlen) {
            keccak_f1600((uint64_t *)state_bytes);
        }
    }
}

/* SHA3-256: tasa = 1600-2*256 = 1088 bits = 136 bytes, dominio 0x06, salida fija 32 bytes. */
void sha3_256(const uint8_t *in, size_t inlen, uint8_t out[32]) {
    keccak_sponge(in, inlen, 0x06, 136, out, 32);
}

/* SHA3-512: tasa = 1600-2*512 = 576 bits = 72 bytes, dominio 0x06, salida fija 64 bytes. */
void sha3_512(const uint8_t *in, size_t inlen, uint8_t out[64]) {
    keccak_sponge(in, inlen, 0x06, 72, out, 64);
}

/* SHAKE128: tasa = 1600-2*128 = 1344 bits = 168 bytes, dominio 0x1F, salida variable. */
void shake128(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen) {
    keccak_sponge(in, inlen, 0x1F, 168, out, outlen);
}

/* SHAKE256: tasa = 1600-2*256 = 1088 bits = 136 bytes, dominio 0x1F, salida variable. */
void shake256(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen) {
    keccak_sponge(in, inlen, 0x1F, 136, out, outlen);
}
