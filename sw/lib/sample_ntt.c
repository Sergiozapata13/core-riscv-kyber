/*
 * sample_ntt.c
 *
 * SampleNTT / Parse (Algorithm 1 de FIPS 203) — Fase 5.
 *
 * Muestreo por rechazo: consume bytes de un XOF (SHAKE128) de a 3 en 3,
 * produce hasta 2 candidatos de 12 bits por iteracion, rechaza los que
 * caen en [q, 4096). Algoritmo identico al ya validado en
 * models/kyber_ref.sample_ntt() (50/50 casos contra kyber-py).
 *
 * Sin operador '%' generico: '% 16' se reemplaza por AND con 0xF (16 es
 * potencia de 2), y '/ 16' por shift — ningun modulo/division genuina
 * en este modulo, a diferencia de pack.c.
 *
 * XOF_BUFFER_BYTES: tamaño del buffer de bytes aleatorios que el
 * llamador debe proveer (ya generado con SHAKE128 antes de llamar a
 * esta funcion). 840 bytes = 5 bloques de SHAKE128 (168 bytes cada
 * uno) — verificado en la Fase 5 (models/kyber_ref.py) que alcanza
 * con probabilidad muy alta (P(rechazo por candidato) ~= 18.7%, y 840
 * bytes cubre holgadamente el caso esperado de ~472 bytes mas margen
 * para la cola de la distribucion). Si algun dia se agotara el buffer,
 * sample_ntt() falla de forma explicita (retorna 0) en vez de producir
 * silenciosamente un polinomio incompleto/incorrecto.
 */

#include <stdint.h>

#define KYBER_Q 3329
#define KYBER_N 256

#define XOF_BUFFER_BYTES 840

/*
 * Retorna 1 si logro completar los 256 coeficientes, 0 si se agoto el
 * buffer de entrada antes de tiempo (deberia ser extremadamente raro,
 * ver nota de cabecera — el llamador debe tratar un retorno de 0 como
 * un error fatal, no seguir adelante con datos parciales).
 */
int sample_ntt(const uint8_t input_bytes[XOF_BUFFER_BYTES], int16_t out[KYBER_N]) {
    int i = 0;
    int j = 0;

    while (j < KYBER_N) {
        if (i + 3 > XOF_BUFFER_BYTES) {
            return 0; /* buffer agotado, fallo explicito */
        }

        unsigned int b0 = input_bytes[i];
        unsigned int b1 = input_bytes[i + 1];
        unsigned int b2 = input_bytes[i + 2];

        unsigned int d1 = b0 + 256u * (b1 & 0xFu);       /* b1 % 16 == b1 & 0xF */
        unsigned int d2 = (b1 >> 4) + 16u * b2;           /* b1 / 16 == b1 >> 4 */

        if (d1 < KYBER_Q) {
            out[j] = (int16_t)d1;
            j++;
        }

        if (d2 < KYBER_Q && j < KYBER_N) {
            out[j] = (int16_t)d2;
            j++;
        }

        i += 3;
    }

    return 1;
}
