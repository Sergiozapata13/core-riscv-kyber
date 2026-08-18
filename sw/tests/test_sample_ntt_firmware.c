/*
 * test_sample_ntt_firmware.c
 *
 * Firmware de prueba para correr sample_ntt.c sobre el core RV32I real
 * (Fase 5) — usa shake128() (ya validado en el core) para generar el
 * buffer de entrada desde una semilla fija, y sample_ntt() para
 * derivar el polinomio, guardando el resultado en memoria.
 *
 * Este firmware ademas es la primera vez que se combinan DOS de las
 * piezas de software de la Fase 5 en un solo programa (keccak.c +
 * sample_ntt.c) — un primer paso hacia el firmware completo de keygen.
 */

#include "keccak.h"
#include "sample_ntt.h"

#define RESULT_ADDR ((volatile uint16_t *)0x1000)
#define DONE_ADDR   ((volatile uint32_t *)0x1800)
#define DONE_MAGIC  0xC0FFEE00u
#define FAIL_MAGIC  0xBAADF00Du

int main(void) {
    uint8_t seed[34];
    for (int i = 0; i < 34; i++) {
        seed[i] = (uint8_t)(i * 5 + 11);
    }

    uint8_t xof_bytes[XOF_BUFFER_BYTES];
    shake128(seed, 34, xof_bytes, XOF_BUFFER_BYTES);

    int16_t out[256];
    int ok = sample_ntt(xof_bytes, out);

    if (!ok) {
        *DONE_ADDR = FAIL_MAGIC;
        return 1;
    }

    for (int i = 0; i < 256; i++) {
        RESULT_ADDR[i] = (uint16_t)out[i];
    }

    *DONE_ADDR = DONE_MAGIC;
    return 0;
}
