/*
 * test_k_pke_decrypt_firmware.c
 *
 * Firmware de prueba para correr k_pke_decrypt.c sobre el core RV32I
 * real (Fase 5) — usa dk_pke/ciphertext ya calculados y hardcodeados
 * (test_dk_c_hardcoded.h, mismos valores ya validados en
 * k_pke_keygen/k_pke_encrypt) y guarda el mensaje recuperado en
 * memoria para verificacion desde el testbench de Verilator.
 */

#include "k_pke_decrypt.h"
#include "test_dk_c_hardcoded.h"

#define M_ADDR    ((volatile uint8_t *)0x1000)
#define DONE_ADDR ((volatile uint32_t *)0x1E00)
#define DONE_MAGIC 0xC0FFEE00u

int main(void) {
    uint8_t m[32];

    k_pke_decrypt(test_dk_hardcoded, test_c_hardcoded, m);

    for (int i = 0; i < 32; i++) {
        M_ADDR[i] = m[i];
    }

    *DONE_ADDR = DONE_MAGIC;
    return 0;
}
