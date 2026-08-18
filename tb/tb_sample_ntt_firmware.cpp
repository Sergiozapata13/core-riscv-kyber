// tb_sample_ntt_firmware.cpp
//
// Testbench de Verilator (Fase 5): corre test_sample_ntt_firmware.hex
// sobre core_top_pipelined.sv (combinando keccak.c + sample_ntt.c en
// un solo firmware) y verifica el polinomio resultante contra el valor
// ya validado nativamente contra kyber_ref.sample_ntt() (via SHAKE128
// real).

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vcore_top_pipelined.h"
#include "Vcore_top_pipelined___024root.h"
#include "verilated.h"

static const uint16_t expected_sample_ntt[256] = {
    2156, 3053, 2054, 225, 2283, 3227, 1529, 1998, 2003, 193, 458, 2570, 1443, 1737, 3007, 2781, 1725, 426, 2626, 627, 2209, 2568, 2784, 2260, 2007, 141, 1223, 883, 1775, 3142, 1422, 2571, 1790, 3217, 64, 3282, 2066, 550, 3059, 2951, 471, 3195, 1916, 2912, 1749, 2256, 822, 2539, 872, 3311, 3081, 2140, 2284, 998, 2676, 2331, 2849, 2103, 156, 597, 953, 187, 1988, 3079, 443, 2914, 2625, 1975, 2020, 1739, 3029, 60, 545, 1712, 2176, 1670, 3249, 720, 806, 2901, 1257, 123, 356, 516, 2700, 2093, 2856, 2073, 2192, 916, 2619, 886, 2510, 1859, 1149, 1995, 1194, 665, 2893, 2535, 2814, 382, 3121, 729, 642, 1728, 1727, 2451, 1492, 1619, 1732, 1836, 306, 530, 1379, 2853, 2574, 1032, 1237, 1200, 2996, 176, 2515, 3010, 1428, 2584, 1432, 1237, 788, 1529, 3234, 1834, 2646, 159, 3222, 1242, 3272, 2882, 779, 717, 2078, 698, 1758, 130, 3079, 277, 68, 2291, 1510, 2479, 56, 2733, 988, 2300, 2588, 1270, 385, 187, 2969, 2197, 1166, 526, 431, 2981, 2875, 2817, 3174, 1031, 1659, 1002, 3009, 2910, 283, 2471, 1255, 2064, 37, 3155, 1520, 488, 2880, 1007, 3181, 2670, 1445, 3066, 1276, 2194, 2463, 3129, 2062, 2392, 215, 3211, 2599, 2455, 1305, 2615, 2993, 2888, 2818, 1939, 3050, 1313, 806, 669, 2679, 1709, 1867, 3320, 3299, 1351, 1896, 2046, 2840, 583, 205, 722, 68, 3271, 714, 1823, 2665, 1415, 3117, 635, 2914, 1823, 2137, 1318, 2770, 2398, 1066, 875, 2790, 794, 394, 2565, 139, 984, 393, 1415, 2888, 1894, 434, 2368, 1278, 1295, 1229, 982, 570, 448, 1550, 2658, 3120, 1314
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
    const uint32_t FAIL_MAGIC = 0xBAADF00Du;
    const uint32_t RESULT_ADDR = 0x1000;

    const int MAX_CYCLES = 500000;  // shake128(840 bytes) necesita ~6 permutaciones Keccak completas
    bool done = false;
    bool failed = false;

    for (int i = 0; i < MAX_CYCLES; i++) {
        tick();
        uint32_t status = read_dmem_word(DONE_ADDR);
        if (status == DONE_MAGIC) {
            done = true;
            std::printf("Firmware termino en el ciclo %d\n", i);
            break;
        }
        if (status == FAIL_MAGIC) {
            failed = true;
            std::printf("Firmware reporto FALLO (buffer XOF agotado) en el ciclo %d\n", i);
            break;
        }
    }

    if (failed) {
        std::printf("FAIL [sample_ntt_buffer]: el firmware reporto que el buffer de 840 bytes no alcanzo\n");
        errors++;
    } else if (!done) {
        std::printf("FAIL [firmware_termina]: no se alcanzo el patron de status tras %d ciclos\n", MAX_CYCLES);
        errors++;
    } else {
        std::printf("OK   [firmware_termina]\n");
    }

    if (done) {
        bool poly_ok = true;
        for (int i = 0; i < 256; i++) {
            uint16_t got = read_dmem_halfword(RESULT_ADDR + i * 2);
            if (got != expected_sample_ntt[i]) {
                poly_ok = false;
                std::printf("FAIL [coef_%d]: got=%u esperado=%u\n", i, got, expected_sample_ntt[i]);
                errors++;
            }
        }
        if (poly_ok) std::printf("OK   [sample_ntt]: 256/256 coeficientes coinciden con la validacion nativa\n");
    }

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: keccak.c + sample_ntt.c corriendo en el core real producen el mismo resultado que la validacion nativa.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
