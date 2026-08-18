// tb_keccak_firmware.cpp
//
// Testbench de Verilator (Fase 5): corre test_keccak_firmware.hex sobre
// core_top_pipelined.sv (el pipeline real, sin instrucciones
// vectoriales activas en este firmware) y verifica que el hash
// resultante en memoria coincide con sha3_256("abc") ya validado
// nativamente en test_keccak_native.c.
//
// Cierra el ciclo de validacion en 3 capas para keccak.c:
//   1. Algoritmo correcto (nativo vs. hashlib de Python) — ya hecho.
//   2. Toolchain compatible (RV32I puro, sin instrucciones M, sin
//      dependencias de libc) — confirmado al lograr el link completo.
//   3. Ejecucion real en el core (memoria, pipeline, ALU, branches) —
//      este testbench.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vcore_top_pipelined.h"
#include "Vcore_top_pipelined___024root.h"
#include "verilated.h"

static Vcore_top_pipelined* top;
static int errors = 0;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

static uint32_t read_dmem_word(uint32_t byte_addr) {
    return top->rootp->core_top_pipelined__DOT__u_dmem__DOT__mem[byte_addr / 4];
}

static uint8_t read_dmem_byte(uint32_t byte_addr) {
    uint32_t word = read_dmem_word(byte_addr & ~0x3u);
    unsigned shift = (byte_addr & 0x3u) * 8;
    return (uint8_t)((word >> shift) & 0xFF);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vcore_top_pipelined;

    top->rst_n = 0;
    top->clk = 0;
    tick();
    tick();
    top->rst_n = 1;

    const uint32_t DONE_ADDR = 0x1100;
    const uint32_t DONE_MAGIC = 0xC0FFEE00u;
    const uint32_t RESULT_ADDR = 0x1000;

    const int MAX_CYCLES = 200000;  // Keccak en software escalar puede tomar varios miles de ciclos
    bool done = false;

    for (int i = 0; i < MAX_CYCLES; i++) {
        tick();
        if (read_dmem_word(DONE_ADDR) == DONE_MAGIC) {
            done = true;
            std::printf("Firmware termino en el ciclo %d\n", i);
            break;
        }
    }

    if (!done) {
        std::printf("FAIL [firmware_termina]: no se alcanzo el patron de status tras %d ciclos\n", MAX_CYCLES);
        errors++;
    } else {
        std::printf("OK   [firmware_termina]\n");
    }

    static const uint8_t expected_sha3_256_abc[32] = {
        0x3a, 0x98, 0x5d, 0xa7, 0x4f, 0xe2, 0x25, 0xb2, 0x04, 0x5c, 0x17, 0x2d,
        0x6b, 0xd3, 0x90, 0xbd, 0x85, 0x5f, 0x08, 0x6e, 0x3e, 0x9d, 0x52, 0x5b,
        0x46, 0xbf, 0xe2, 0x45, 0x11, 0x43, 0x15, 0x32
    };

    bool hash_ok = true;
    for (int i = 0; i < 32; i++) {
        uint8_t got = read_dmem_byte(RESULT_ADDR + i);
        if (got != expected_sha3_256_abc[i]) {
            hash_ok = false;
            std::printf("FAIL [hash_byte_%d]: got=0x%02x esperado=0x%02x\n", i, got, expected_sha3_256_abc[i]);
            errors++;
        }
    }
    if (hash_ok) {
        std::printf("OK   [sha3_256_abc]: 32/32 bytes coinciden con el valor validado nativamente\n");
    }

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: keccak.c corriendo en el core real produce el mismo resultado que la validacion nativa.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
