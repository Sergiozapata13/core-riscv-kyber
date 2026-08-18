/*
 * poly_ntt.c
 *
 * NTT/INTT escalar, multiplicacion punto a punto, suma/resta de
 * polinomios — Fase 5 (firmware de referencia, sin aceleracion
 * vectorial). Replica EXACTAMENTE el algoritmo ya validado en
 * models/kyber_ref.py (a su vez validado contra kyber-py) — incluye
 * las correcciones de los 3 bugs reales encontrados al construir el
 * RTL vectorial equivalente en la Fase 4:
 *   1. NTT/INTT operan in-place, no hace falta un truco especial en C
 *      (a diferencia del RTL, que necesito un estado COPY explicito).
 *   2. Gentleman-Sande (INTT) usa el twiddle factor DIRECTO (mismo que
 *      Cooley-Tukey), no su inverso, y el orden de la resta es (b-a),
 *      no (a-b) — ver rtl/vector/butterfly_gs.sv para el detalle
 *      completo de esta correccion.
 *   3. El factor de escalado final de INTT es INV128=3303 (inverso
 *      modular de 128 mod q), no 3212 como se penso originalmente.
 *
 * Acepta la dependencia de libgcc (__mulsi3, y potencialmente
 * __muldi3/__divdi3 para la aritmetica de 64 bits de Barrett) — mismo
 * criterio ya aplicado en pack.c: multiplicacion genuina, sin tabla de
 * lookup razonable, dependencia estandar para RV32I sin extension M.
 */

#include <stdint.h>
#include "poly_ntt_constants.h"

#define KYBER_Q 3329
#define KYBER_N 256

#define BARRETT_SHIFT 26
#define BARRETT_R_HALF (1 << (BARRETT_SHIFT - 1))  /* 2^25 */
#define BARRETT_MULTIPLIER 20159

/* Inverso modular de 128 mod q = 3303 (ver nota de cabecera, correccion
 * del bug de la Fase 4 donde este valor estaba mal calculado como 3212
 * pese a decir "verificado" en el comentario original). */
#define INV128 3303

/*
 * barrett_reduce_c: misma formula que rtl/vector/barrett_reduce.sv y
 * kyber_ref.barrett_reduce() — mult+shift+resta, mas UNA correccion
 * aritmetica bidireccional (sin operador '%', sin bifurcacion de
 * control — mismo principio constant-time que el RTL, aunque aca la
 * prioridad es portabilidad/simplicidad, no timing real de software).
 *
 * Rango de entrada verificado (igual que el RTL): 'a' proviene de
 * zeta*b (producto de dos valores <q, cabe en 24 bits) o de sumas/
 * restas de valores ya reducidos (cabe en 13 bits) — el remainder
 * crudo siempre cae en (-q,q), una sola correccion basta.
 */
static int16_t barrett_reduce_c(int32_t a) {
    int64_t t = (int64_t)a * BARRETT_MULTIPLIER + BARRETT_R_HALF;
    int32_t quotient = (int32_t)(t >> BARRETT_SHIFT);
    int32_t remainder = a - quotient * KYBER_Q;

    if (remainder < 0) remainder += KYBER_Q;
    if (remainder >= KYBER_Q) remainder -= KYBER_Q;
    return (int16_t)remainder;
}

/*
 * ntt: Cooley-Tukey, in-place, 7 niveles (NTT incompleto de Kyber).
 * Identico al bucle de kyber_ref.ntt().
 */
void poly_ntt(int16_t r[KYBER_N]) {
    int k = 1;
    int length = 128;
    while (length >= 2) {
        int start = 0;
        while (start < KYBER_N) {
            int16_t zeta = POLY_NTT_ZETAS[k];
            k++;
            for (int j = start; j < start + length; j++) {
                int16_t t = barrett_reduce_c((int32_t)zeta * r[j + length]);
                r[j + length] = barrett_reduce_c((int32_t)r[j] - t);
                r[j] = barrett_reduce_c((int32_t)r[j] + t);
            }
            start += 2 * length;
        }
        length /= 2;
    }
}

/*
 * intt: Gentleman-Sande, in-place, 7 niveles + escalado final por
 * INV128. Identico al bucle de kyber_ref.intt() — incluyendo la
 * correccion de signo (b-a) y twiddle directo (no inverso).
 */
void poly_intt(int16_t r[KYBER_N]) {
    int k = 127;
    int length = 2;
    while (length <= 128) {
        int start = 0;
        while (start < KYBER_N) {
            int16_t zeta = POLY_NTT_ZETAS[k];
            k--;
            for (int j = start; j < start + length; j++) {
                int16_t t = r[j];
                r[j] = barrett_reduce_c((int32_t)t + r[j + length]);
                /* CORRECCION Fase 4: (b - a), no (a - b) */
                r[j + length] = barrett_reduce_c((int32_t)r[j + length] - t);
                /* CORRECCION Fase 4: zeta directo, no zeta_inv */
                r[j + length] = barrett_reduce_c((int32_t)zeta * r[j + length]);
            }
            start += 2 * length;
        }
        length *= 2;
    }

    for (int i = 0; i < KYBER_N; i++) {
        r[i] = barrett_reduce_c((int32_t)r[i] * INV128);
    }
}

/*
 * base_case_multiply: multiplicacion de polinomios de grado 1,
 * identico a kyber_ref._base_case_multiply().
 */
static void base_case_multiply(int16_t a0, int16_t a1, int16_t b0, int16_t b1,
                                int16_t zeta, int16_t *c0, int16_t *c1) {
    int32_t a1b1 = barrett_reduce_c((int32_t)a1 * b1);
    *c0 = barrett_reduce_c((int32_t)a0 * b0 + (int32_t)zeta * a1b1);
    *c1 = barrett_reduce_c((int32_t)a0 * b1 + (int32_t)a1 * b0);
}

/*
 * poly_pointwise_mul: multiplicacion punto a punto en dominio NTT,
 * identico a kyber_ref.poly_pointwise_mul() (64 grupos de 4
 * coeficientes, zeta y Q-zeta alternados).
 */
void poly_pointwise_mul(const int16_t a[KYBER_N], const int16_t b[KYBER_N], int16_t out[KYBER_N]) {
    for (int i = 0; i < 64; i++) {
        int16_t zeta = POLY_NTT_ZETAS[64 + i];

        base_case_multiply(a[4 * i + 0], a[4 * i + 1], b[4 * i + 0], b[4 * i + 1],
                            zeta, &out[4 * i + 0], &out[4 * i + 1]);

        int16_t neg_zeta = (int16_t)(KYBER_Q - zeta);
        base_case_multiply(a[4 * i + 2], a[4 * i + 3], b[4 * i + 2], b[4 * i + 3],
                            neg_zeta, &out[4 * i + 2], &out[4 * i + 3]);
    }
}

void poly_add(const int16_t a[KYBER_N], const int16_t b[KYBER_N], int16_t out[KYBER_N]) {
    for (int i = 0; i < KYBER_N; i++) {
        out[i] = barrett_reduce_c((int32_t)a[i] + b[i]);
    }
}

void poly_sub(const int16_t a[KYBER_N], const int16_t b[KYBER_N], int16_t out[KYBER_N]) {
    for (int i = 0; i < KYBER_N; i++) {
        out[i] = barrett_reduce_c((int32_t)a[i] - b[i]);
    }
}
