/*
 * test_ml_kem_firmware.c
 *
 * Firmware de prueba para correr ml_kem.c (protocolo completo) sobre
 * el core RV32I real (Fase 5) — ejecuta el ciclo end-to-end completo:
 * keygen -> encaps -> decaps (normal) -> decaps (rechazo implicito,
 * ciphertext corrompido), guardando cada resultado en memoria para
 * verificacion desde el testbench de Verilator.
 *
 * Firmware mas pesado del proyecto: encadena 2 K-PKE.KeyGen-equivalent
 * (keygen + la re-encriptacion interna de decaps) y 2 K-PKE.Encrypt
 * (encaps + la re-encriptacion de verificacion en decaps), compilado
 * con -O0 (mismo bug de codegen del compilador ya documentado).
 */

#include "ml_kem.h"

#define K_ADDR           ((volatile uint8_t *)0x1000)
#define K_REJECTED_ADDR  ((volatile uint8_t *)0x1020)
#define DONE_ADDR        ((volatile uint32_t *)0x1E00)
#define DONE_MAGIC 0xC0FFEE00u

int main(void) {
    uint8_t d[32], z[32], m[32];
    for (int i = 0; i < 32; i++) {
        d[i] = (uint8_t)((i * 3 + 5) % 256);
        z[i] = (uint8_t)((i * 13 + 17) % 256);
        m[i] = (uint8_t)((i * 7 + 11) % 256);
    }

    uint8_t ek[800];
    uint8_t dk[1632];
    ml_kem_keygen(d, z, ek, dk);

    uint8_t K[32];
    uint8_t c[768];
    ml_kem_encaps(ek, m, K, c);

    uint8_t K_decap[32];
    ml_kem_decaps(dk, c, K_decap);

    for (int i = 0; i < 32; i++) {
        K_ADDR[i] = K_decap[i];
    }

    /* Rechazo implicito: corromper 1 bit del ciphertext */
    uint8_t c_corrupted[768];
    for (int i = 0; i < 768; i++) c_corrupted[i] = c[i];
    c_corrupted[0] ^= 0x01;

    uint8_t K_rejected[32];
    ml_kem_decaps(dk, c_corrupted, K_rejected);

    for (int i = 0; i < 32; i++) {
        K_REJECTED_ADDR[i] = K_rejected[i];
    }

    *DONE_ADDR = DONE_MAGIC;
    return 0;
}
