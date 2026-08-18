// tb_k_pke_decrypt_firmware.cpp
//
// Testbench de Verilator (Fase 5): corre test_k_pke_decrypt_firmware.hex
// sobre core_top_pipelined.sv (K-PKE.Decrypt completo, sin aceleracion
// vectorial, -O0) y verifica que el mensaje recuperado coincide con el
// original — confirmando el ciclo completo encrypt->decrypt en el
// core real.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vcore_top_pipelined.h"
#include "Vcore_top_pipelined___024root.h"
#include "verilated.h"

static const uint8_t expected_m[32] = {
    11, 18, 25, 32, 39, 46, 53, 60, 67, 74, 81, 88, 95, 102, 109, 116, 123, 130, 137, 144, 151, 158, 165, 172, 179, 186, 193, 200, 207, 214, 221, 228
};

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

    const uint32_t M_ADDR = 0x1000;
    const uint32_t DONE_ADDR = 0x1E00;
    const uint32_t DONE_MAGIC = 0xC0FFEE00u;

    const long MAX_CYCLES = 10000000;
    bool done = false;

    for (long i = 0; i < MAX_CYCLES; i++) {
        tick();
        if (read_dmem_word(DONE_ADDR) == DONE_MAGIC) {
            done = true;
            std::printf("Firmware termino en el ciclo %ld\n", i);
            break;
        }
    }

    if (!done) {
        std::printf("FAIL [firmware_termina]: no se alcanzo el patron de status tras %ld ciclos\n", MAX_CYCLES);
        errors++;
    } else {
        std::printf("OK   [firmware_termina]\n");
    }

    if (done) {
        bool m_ok = true;
        for (int i = 0; i < 32; i++) {
            uint8_t got = read_dmem_byte(M_ADDR + i);
            if (got != expected_m[i]) {
                m_ok = false;
                std::printf("FAIL [m_%d]: got=%u esperado=%u\n", i, got, expected_m[i]);
                errors++;
            }
        }
        if (m_ok) std::printf("OK   [mensaje_recuperado]: 32/32 bytes correctos\n");
    }

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: k_pke_decrypt.c corriendo en el core real recupera el mensaje original.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
