// tb_ml_kem_firmware.cpp
//
// Testbench de Verilator (Fase 5): corre test_ml_kem_firmware.hex
// sobre core_top_pipelined.sv — el PROTOCOLO ML-KEM-512 COMPLETO
// (keygen + encaps + decaps normal + decaps con rechazo implicito)
// ejecutando end-to-end en el core real, verificado contra los
// mismos valores ya validados nativamente contra kyber_ref.py.
//
// Firmware mas pesado del proyecto: se usa un limite de ciclos muy
// generoso y se reporta progreso periodicamente.

#include <memory>
#include <cstdio>
#include <cstdint>
#include <ctime>

#include "Vcore_top_pipelined.h"
#include "Vcore_top_pipelined___024root.h"
#include "verilated.h"

static const uint8_t expected_K[32] = {
    228, 255, 168, 0, 246, 207, 89, 224, 151, 101, 230, 36, 45, 174, 58, 249, 52, 44, 92, 123, 223, 96, 8, 143, 61, 171, 231, 1, 135, 231, 236, 123
};

static const uint8_t expected_K_rejected[32] = {
    2, 248, 162, 95, 97, 110, 2, 8, 153, 178, 1, 17, 167, 60, 216, 83, 200, 67, 201, 77, 28, 6, 14, 247, 11, 206, 72, 162, 181, 241, 108, 177
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

    const uint32_t K_ADDR = 0x1000;
    const uint32_t K_REJECTED_ADDR = 0x1020;
    const uint32_t DONE_ADDR = 0x1E00;
    const uint32_t DONE_MAGIC = 0xC0FFEE00u;

    const long MAX_CYCLES = 60000000;
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
        bool k_ok = true;
        for (int i = 0; i < 32; i++) {
            uint8_t got = read_dmem_byte(K_ADDR + i);
            if (got != expected_K[i]) {
                k_ok = false;
                std::printf("FAIL [K_%d]: got=%u esperado=%u\n", i, got, expected_K[i]);
                errors++;
            }
        }
        if (k_ok) std::printf("OK   [K_decaps_normal]: 32/32 bytes correctos\n");

        bool k_rej_ok = true;
        for (int i = 0; i < 32; i++) {
            uint8_t got = read_dmem_byte(K_REJECTED_ADDR + i);
            if (got != expected_K_rejected[i]) {
                k_rej_ok = false;
                std::printf("FAIL [K_rejected_%d]: got=%u esperado=%u\n", i, got, expected_K_rejected[i]);
                errors++;
            }
        }
        if (k_rej_ok) std::printf("OK   [K_decaps_rechazo_implicito]: 32/32 bytes correctos\n");
    }

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: ML-KEM-512 completo (keygen+encaps+decaps, incluyendo rechazo implicito) corriendo en el core real.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
