/*
 * cbd.c
 *
 * Muestreo de la distribucion binomial centrada (CBD) — Fase 5.
 *
 * Implementa exactamente el algoritmo ya validado en
 * models/kyber_ref.cbd() (verificado contra kyber-py, 40/40 casos) —
 * Algorithm 7 (SamplePolyCBD) de FIPS 203.
 *
 * Igual que keccak.c, escrito para ser completamente autocontenido en
 * un entorno bare-metal -nostdlib: sin operador '%' sobre valores que
 * no son potencia de 2 (evita __modsi3 de libgcc), sin depender de
 * memset/memcpy.
 */

#include <stdint.h>
#include <stddef.h>

#define KYBER_Q 3329
#define KYBER_N 256

/* Cuenta bits en 1 de un valor de hasta 8 bits (eta maximo es 3, asi
 * que el valor enmascarado nunca excede 2*eta=6 bits) — conteo simple
 * bit a bit, sin necesitar ninguna instruccion mas alla de shifts/AND. */
static unsigned int bit_count(unsigned int x) {
    unsigned int count = 0;
    while (x != 0) {
        count += (x & 1u);
        x >>= 1;
    }
    return count;
}

/*
 * cbd_sample: genera un polinomio de 256 coeficientes desde
 * 'input_bytes' (debe tener exactamente 64*eta bytes), siguiendo la
 * misma logica bit a bit que kyber_ref.cbd():
 *   - Los bytes se interpretan como un entero grande en little-endian
 *     (bit 0 del primer byte es el bit menos significativo del entero).
 *   - Para cada coeficiente, se extraen 2*eta bits: los primeros eta
 *     bits forman 'a' (via bit_count), los siguientes eta forman 'b'.
 *   - coeficiente = (a - b) mod q.
 *
 * eta=3 para el ruido de generacion de clave (eta1, ML-KEM-512);
 * eta=2 para el ruido de encapsulacion (eta2).
 *
 * Implementacion SIN construir el entero de 64*eta*8 bits completo en
 * memoria (evitaria depender de aritmetica de precision arbitraria,
 * que este entorno bare-metal no tiene) — en cambio, se recorre
 * byte a byte y bit a bit, manteniendo solo una pequeña ventana de
 * bits pendientes en un acumulador de 32 bits, suficiente para 2*eta
 * (maximo 6) bits a la vez.
 */
void cbd_sample(const uint8_t *input_bytes, unsigned int eta, int16_t out[KYBER_N]) {
    unsigned int mask = (1u << eta) - 1u;

    size_t byte_idx = 0;
    unsigned int bit_idx = 0;       /* bit actual dentro del byte, 0-7 */
    unsigned int acc = 0;           /* bits ya leidos, pendientes de consumir */
    unsigned int acc_bits = 0;      /* cuantos bits validos hay en acc */

    for (int i = 0; i < KYBER_N; i++) {
        /* Asegurar que acc tenga al menos 2*eta bits disponibles. */
        while (acc_bits < 2u * eta) {
            unsigned int byte_val = input_bytes[byte_idx];
            unsigned int bit_val = (byte_val >> bit_idx) & 1u;
            acc |= (bit_val << acc_bits);
            acc_bits++;

            bit_idx++;
            if (bit_idx == 8u) {
                bit_idx = 0;
                byte_idx++;
            }
        }

        unsigned int a_bits = acc & mask;
        unsigned int b_bits = (acc >> eta) & mask;
        acc >>= (2u * eta);
        acc_bits -= (2u * eta);

        unsigned int a = bit_count(a_bits);
        unsigned int b = bit_count(b_bits);

        /* (a - b) mod q, sin operador '%': a,b estan acotados a [0,eta]
         * (eta<=3), asi que (a-b) esta en [-3,3] — una sola correccion
         * aritmetica (sumar q si es negativo) basta, sin necesitar el
         * operador de modulo. */
        int diff = (int)a - (int)b;
        if (diff < 0) diff += KYBER_Q;
        out[i] = (int16_t)diff;
    }
}
