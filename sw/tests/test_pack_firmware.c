/*
 * test_pack_firmware.c
 *
 * Firmware de prueba para correr pack.c sobre el core RV32I real
 * (Fase 5) — codifica un polinomio fijo con byte_encode_d12, y
 * comprime/descomprime con d=10, guardando resultados en memoria para
 * verificacion desde el testbench de Verilator.
 *
 * NOTA: este firmware usa '%' directamente (para generar el patron de
 * entrada), a diferencia de keccak.c/cbd.c — es seguro aca porque
 * pack.c YA requiere -lgcc para su propia division genuina
 * (compress_coeff), asi que __modsi3 se resuelve del mismo link, sin
 * agregar una dependencia nueva.
 */

#include "pack.h"

#define ENCODED_ADDR      ((volatile uint8_t *)0x1000)
#define COMPRESSED_ADDR   ((volatile uint16_t *)0x1200)
#define DECOMPRESSED_ADDR ((volatile uint16_t *)0x1800)
#define DONE_ADDR         ((volatile uint32_t *)0x1E00)
#define DONE_MAGIC        0xC0FFEE00u

int main(void) {
    int16_t coeffs[256];
    for (int i = 0; i < 256; i++) {
        coeffs[i] = (int16_t)((i * 13 + 7) % 3329);
    }

    uint8_t encoded[384];
    byte_encode_d12(coeffs, encoded);
    for (int i = 0; i < 384; i++) {
        ENCODED_ADDR[i] = encoded[i];
    }

    uint16_t compressed[256];
    poly_compress(coeffs, 10, compressed);
    for (int i = 0; i < 256; i++) {
        COMPRESSED_ADDR[i] = compressed[i];
    }

    int16_t decompressed[256];
    poly_decompress(compressed, 10, decompressed);
    for (int i = 0; i < 256; i++) {
        DECOMPRESSED_ADDR[i] = (uint16_t)decompressed[i];
    }

    *DONE_ADDR = DONE_MAGIC;
    return 0;
}
