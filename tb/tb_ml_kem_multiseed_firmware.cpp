// tb_ml_kem_multiseed_firmware.cpp
//
// Testbench de Verilator (Fase 5, cierre de validacion multi-semilla):
// corre test_ml_kem_multiseed_firmware.hex sobre core_top_pipelined.sv
// — el protocolo ML-KEM-512 completo (version ACELERADA, vectorial)
// con 2 semillas adicionales, distintas de la unica semilla fija
// usada hasta ahora en todos los firmwares que corrieron en el core
// real. Confirma que la correctitud observada previamente no dependia,
// por coincidencia, de esa semilla en particular.

#include <memory>
#include <cstdio>
#include <cstdint>
#include <ctime>

#include "Vcore_top_pipelined.h"
#include "Vcore_top_pipelined___024root.h"
#include "verilated.h"

static const uint8_t expected_K_seed1[32] = {
    47, 183, 225, 32, 206, 229, 71, 77, 117, 107, 73, 20, 194, 51, 231, 167, 113, 148, 170, 180, 25, 52, 240, 4, 89, 208, 154, 175, 56, 39, 253, 95
};
static const uint8_t expected_K_rej_seed1[32] = {
    246, 77, 205, 224, 36, 76, 200, 184, 229, 112, 66, 143, 160, 124, 100, 70, 79, 218, 219, 18, 253, 157, 142, 190, 208, 227, 53, 21, 58, 124, 58, 34
};
static const uint8_t expected_K_seed2[32] = {
    0, 73, 159, 163, 208, 226, 195, 60, 193, 169, 173, 42, 225, 115, 179, 121, 198, 226, 143, 116, 89, 139, 193, 254, 26, 35, 91, 127, 141, 94, 27, 241
};
static const uint8_t expected_K_rej_seed2[32] = {
    224, 151, 4, 81, 228, 108, 126, 34, 108, 147, 118, 91, 251, 59, 187, 204, 222, 143, 59, 31, 12, 210, 221, 126, 106, 95, 15, 224, 140, 51, 2, 124
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

static bool check_32(const char* label, uint32_t addr, const uint8_t* expected) {
    bool ok = true;
    for (int i = 0; i < 32; i++) {
        uint8_t got = read_dmem_byte(addr + i);
        if (got != expected[i]) {
            ok = false;
            std::printf("FAIL [%s_%d]: got=%u esperado=%u\n", label, i, got, expected[i]);
            errors++;
        }
    }
    if (ok) std::printf("OK   [%s]: 32/32 bytes correctos\n", label);
    return ok;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vcore_top_pipelined;

    top->rst_n = 0;
    top->clk = 0;
    tick();
    tick();
    top->rst_n = 1;

    const uint32_t K_SEED1_ADDR = 0x1000, K_REJ_SEED1_ADDR = 0x1020;
    const uint32_t K_SEED2_ADDR = 0x1040, K_REJ_SEED2_ADDR = 0x1060;
    const uint32_t DONE_ADDR = 0x1E00;
    const uint32_t DONE_MAGIC = 0xC0FFEE00u;

    const long MAX_CYCLES = 70000000;
    bool done = false;

    time_t start_time = time(nullptr);

    for (long i = 0; i < MAX_CYCLES; i++) {
        tick();
        if (read_dmem_word(DONE_ADDR) == DONE_MAGIC) {
            done = true;
            std::printf("Firmware termino en el ciclo %ld\n", i);
            break;
        }
        if (i % 5000000 == 0 && i > 0) {
            std::printf("  ... %ld ciclos simulados (%.0f segundos reales)\n", i, difftime(time(nullptr), start_time));
        }
    }

    if (!done) {
        std::printf("FAIL [firmware_termina]: no se alcanzo el patron de status tras %ld ciclos\n", MAX_CYCLES);
        errors++;
    } else {
        std::printf("OK   [firmware_termina]\n");
    }

    if (done) {
        check_32("K_seed1", K_SEED1_ADDR, expected_K_seed1);
        check_32("K_rej_seed1", K_REJ_SEED1_ADDR, expected_K_rej_seed1);
        check_32("K_seed2", K_SEED2_ADDR, expected_K_seed2);
        check_32("K_rej_seed2", K_REJ_SEED2_ADDR, expected_K_rej_seed2);
    }

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: ML-KEM-512 acelerado, corriendo en el core real con 2 semillas ADICIONALES,\n");
        std::printf("      confirma que la correctitud no dependia de la semilla unica usada antes.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
