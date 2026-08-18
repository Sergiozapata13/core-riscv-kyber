/*
 * test_keccak_firmware.c
 *
 * Firmware de prueba para correr keccak.c sobre el core RV32I real
 * (Fase 5) — calcula sha3_256("abc") y guarda el resultado de 32 bytes
 * en una direccion fija de memoria, para que el testbench de Verilator
 * lo verifique por inspeccion directa (mismo patron que hello.c usaba
 * para el patron de status en la Fase 0).
 *
 * El valor esperado es el mismo ya validado nativamente contra hashlib
 * de Python en test_keccak_native.c — si este firmware corriendo en el
 * core real produce el mismo resultado, confirma que el flujo completo
 * de compilacion (gcc -march=rv32i -ffreestanding, sin libc) mas la
 * ejecucion real en el pipeline (memoria, ALU, branches, etc.) no
 * introduce ninguna discrepancia respecto al modelo C validado.
 */

#include "keccak.h"

#define RESULT_ADDR ((volatile uint8_t *)0x1000)
#define DONE_ADDR   ((volatile uint32_t *)0x1100)
#define DONE_MAGIC  0xC0FFEE00u

int main(void) {
    static const uint8_t msg[] = {'a', 'b', 'c'};
    uint8_t out[32];

    sha3_256(msg, 3, out);

    for (int i = 0; i < 32; i++) {
        RESULT_ADDR[i] = out[i];
    }

    *DONE_ADDR = DONE_MAGIC;

    return 0;
}
