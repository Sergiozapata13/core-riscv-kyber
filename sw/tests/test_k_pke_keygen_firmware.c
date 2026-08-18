/*
 * test_k_pke_keygen_firmware.c
 *
 * Firmware de prueba para correr k_pke_keygen.c sobre el core RV32I
 * real (Fase 5) — usa la misma semilla 'd' que test_k_pke_keygen_native.c
 * y guarda ek_pke/dk_pke en memoria para verificacion desde el
 * testbench de Verilator contra los mismos valores ya validados
 * nativamente (que a su vez coinciden con kyber_ref.k_pke_keygen()).
 */

#include "k_pke_keygen.h"

#define EK_ADDR   ((volatile uint8_t *)0x1000)
#define DK_ADDR   ((volatile uint8_t *)0x1400)
#define DONE_ADDR ((volatile uint32_t *)0x1800)
#define DONE_MAGIC 0xC0FFEE00u

int main(void) {
    uint8_t d[32];
    for (int i = 0; i < 32; i++) {
        d[i] = (uint8_t)(i * 3 + 5);
    }

    uint8_t ek[800];
    uint8_t dk[768];

    k_pke_keygen(d, ek, dk);

    for (int i = 0; i < 800; i++) {
        EK_ADDR[i] = ek[i];
    }
    for (int i = 0; i < 768; i++) {
        DK_ADDR[i] = dk[i];
    }

    *DONE_ADDR = DONE_MAGIC;
    return 0;
}
