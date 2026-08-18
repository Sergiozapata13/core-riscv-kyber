/*
 * test_cbd_firmware.c
 *
 * Firmware de prueba para correr cbd.c sobre el core RV32I real
 * (Fase 5) — genera un polinomio CBD (eta=3) desde un patron de bytes
 * fijo y guarda los 256 coeficientes (como halfwords de 16 bits) en
 * memoria, para verificacion desde el testbench de Verilator contra
 * el mismo resultado ya validado nativamente.
 */

#include "cbd.h"

#define RESULT_ADDR ((volatile uint16_t *)0x1000)
#define DONE_ADDR   ((volatile uint32_t *)0x1800)
#define DONE_MAGIC  0xC0FFEE00u

int main(void) {
    /* Patron de entrada fijo y simple (no aleatorio real — no hace
     * falta para esta prueba de integracion, solo necesitamos un
     * patron determinista para comparar contra Python). */
    uint8_t input[192];
    for (int i = 0; i < 192; i++) {
        input[i] = (uint8_t)(i * 7 + 3);
    }

    int16_t out[256];
    cbd_sample(input, 3, out);

    for (int i = 0; i < 256; i++) {
        RESULT_ADDR[i] = (uint16_t)out[i];
    }

    *DONE_ADDR = DONE_MAGIC;
    return 0;
}
