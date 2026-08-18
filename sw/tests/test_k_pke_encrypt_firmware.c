/*
 * test_k_pke_encrypt_firmware.c
 *
 * Firmware de prueba para correr k_pke_encrypt.c sobre el core RV32I
 * real (Fase 5) — usa el mismo ek/m/r que test_k_pke_encrypt_native.c
 * y guarda el ciphertext en memoria para verificacion desde el
 * testbench de Verilator.
 *
 * Usa un ek_pke ya calculado y hardcodeado (test_ek_hardcoded.h, mismo
 * valor ya validado en k_pke_keygen) en vez de recalcularlo con
 * k_pke_keygen dentro de este firmware — evita duplicar el costo de
 * ciclos de keygen (~9M ciclos) en un test que solo quiere ejercitar
 * encrypt.
 */

#include "k_pke_encrypt.h"
#include "test_ek_hardcoded.h"

#define C_ADDR    ((volatile uint8_t *)0x1000)
#define DONE_ADDR ((volatile uint32_t *)0x1E00)
#define DONE_MAGIC 0xC0FFEE00u

int main(void) {
    uint8_t m[32], r[32];
    for (int i = 0; i < 32; i++) {
        m[i] = (uint8_t)((i * 7 + 11) % 256);
        r[i] = (uint8_t)((i * 11 + 3) % 256);
    }

    uint8_t c[768];
    k_pke_encrypt(test_ek_hardcoded, m, r, c);

    for (int i = 0; i < 768; i++) {
        C_ADDR[i] = c[i];
    }

    *DONE_ADDR = DONE_MAGIC;
    return 0;
}
