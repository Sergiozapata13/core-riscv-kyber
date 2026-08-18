// tb_cbd_firmware.cpp
//
// Testbench de Verilator (Fase 5): corre test_cbd_firmware.hex sobre
// core_top_pipelined.sv y verifica que el polinomio CBD (eta=3)
// resultante coincide con el valor ya validado nativamente contra
// kyber_ref.cbd() (que a su vez ya se valido contra kyber-py).

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vcore_top_pipelined.h"
#include "Vcore_top_pipelined___024root.h"
#include "verilated.h"

static const uint16_t expected_cbd_eta3[256] = {
    2, 3327, 3328, 1, 3327, 3327, 0, 0, 0, 3328, 0, 1, 1, 1, 1, 0, 2, 3327, 1, 3328, 3328, 1, 2, 0, 1, 0, 3326, 1, 1, 0, 0, 3328, 0, 0, 0, 0, 0, 1, 3328, 0, 1, 0, 3328, 3326, 3328, 3, 2, 3328, 0, 0, 3328, 2, 0, 3328, 0, 3328, 3328, 0, 0, 1, 3328, 0, 1, 1, 1, 3328, 1, 3327, 3327, 3328, 1, 0, 3328, 0, 3326, 0, 0, 1, 0, 3327, 2, 3327, 3328, 0, 3327, 1, 3328, 3328, 0, 1, 3328, 1, 1, 1, 1, 3328, 3328, 1, 3328, 1, 3328, 3328, 0, 3, 1, 3327, 3328, 0, 3327, 1, 1, 0, 2, 3328, 1, 2, 0, 0, 1, 3328, 3327, 1, 3326, 0, 1, 1, 0, 2, 1, 3328, 3328, 3328, 3326, 3328, 0, 3328, 1, 1, 3328, 0, 0, 2, 1, 3327, 1, 3328, 1, 1, 0, 0, 0, 2, 0, 3328, 3328, 1, 0, 3328, 2, 0, 1, 0, 1, 1, 3328, 0, 1, 0, 0, 3328, 1, 3328, 0, 2, 0, 1, 1, 3328, 3328, 0, 3328, 0, 0, 3327, 0, 2, 3328, 0, 0, 2, 1, 3328, 0, 0, 1, 0, 3328, 3327, 3328, 2, 0, 3328, 3328, 0, 3328, 0, 2, 3328, 3, 3327, 0, 1, 3328, 1, 1, 3328, 3328, 0, 1, 1, 2, 0, 3328, 1, 0, 0, 3328, 3328, 3327, 0, 0, 1, 2, 0, 3327, 3328, 3328, 3, 1, 3327, 1, 0, 1, 0, 1, 3328, 3328, 1, 3328, 0, 3328, 0, 0, 0, 2, 2
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

static uint16_t read_dmem_halfword(uint32_t byte_addr) {
    uint32_t word = read_dmem_word(byte_addr & ~0x3u);
    return (byte_addr & 0x2u) ? (uint16_t)(word >> 16) : (uint16_t)(word & 0xFFFF);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vcore_top_pipelined;

    top->rst_n = 0;
    top->clk = 0;
    tick();
    tick();
    top->rst_n = 1;

    const uint32_t DONE_ADDR = 0x1800;
    const uint32_t DONE_MAGIC = 0xC0FFEE00u;
    const uint32_t RESULT_ADDR = 0x1000;

    const int MAX_CYCLES = 50000;
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

    bool poly_ok = true;
    for (int i = 0; i < 256; i++) {
        uint16_t got = read_dmem_halfword(RESULT_ADDR + i * 2);
        if (got != expected_cbd_eta3[i]) {
            poly_ok = false;
            std::printf("FAIL [cbd_coef_%d]: got=%u esperado=%u\n", i, got, expected_cbd_eta3[i]);
            errors++;
        }
    }
    if (poly_ok) {
        std::printf("OK   [cbd_eta3]: 256/256 coeficientes coinciden con la validacion nativa\n");
    }

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: cbd.c corriendo en el core real produce el mismo resultado que la validacion nativa.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
