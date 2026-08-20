/*
 * test_ml_kem_multiseed_firmware.c
 *
 * Firmware de prueba (Fase 5, cierre de validacion multi-semilla): corre
 * el ciclo completo ML-KEM-512 (keygen -> encaps -> decaps normal ->
 * decaps con rechazo implicito) con DOS semillas adicionales, distintas
 * de la unica semilla fija usada hasta ahora en todos los firmwares que
 * corrieron en el core real — cierra el riesgo de que la correctitud
 * observada dependiera, por coincidencia, de esa semilla en particular.
 */

#include "ml_kem.h"
#include "test_multiseed_inputs.h"

#define K_SEED1_ADDR          ((volatile uint8_t *)0x1000)
#define K_REJ_SEED1_ADDR      ((volatile uint8_t *)0x1020)
#define K_SEED2_ADDR          ((volatile uint8_t *)0x1040)
#define K_REJ_SEED2_ADDR      ((volatile uint8_t *)0x1060)
#define DONE_ADDR             ((volatile uint32_t *)0x1E00)
#define DONE_MAGIC 0xC0FFEE00u

static void run_one_seed(int seed_idx, const uint8_t *d, const uint8_t *z, const uint8_t *m,
                          volatile uint8_t *K_out, volatile uint8_t *K_rej_out) {
    uint8_t ek[800];
    uint8_t dk[1632];
    ml_kem_keygen(d, z, ek, dk);

    uint8_t K[32];
    uint8_t c[768];
    ml_kem_encaps(ek, m, K, c);

    uint8_t K_decap[32];
    ml_kem_decaps(dk, c, K_decap);
    for (int i = 0; i < 32; i++) K_out[i] = K_decap[i];

    uint8_t c_corrupted[768];
    for (int i = 0; i < 768; i++) c_corrupted[i] = c[i];
    /* Mismo byte que corrompio el generador Python: seed_idx % len(c) */
    c_corrupted[seed_idx % 768] ^= 0x01;

    uint8_t K_rejected[32];
    ml_kem_decaps(dk, c_corrupted, K_rejected);
    for (int i = 0; i < 32; i++) K_rej_out[i] = K_rejected[i];
}

int main(void) {
    run_one_seed(1, seed1_d, seed1_z, seed1_m, K_SEED1_ADDR, K_REJ_SEED1_ADDR);
    run_one_seed(2, seed2_d, seed2_z, seed2_m, K_SEED2_ADDR, K_REJ_SEED2_ADDR);

    *DONE_ADDR = DONE_MAGIC;
    return 0;
}
