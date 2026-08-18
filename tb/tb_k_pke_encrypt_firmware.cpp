// tb_k_pke_encrypt_firmware.cpp
//
// Testbench de Verilator (Fase 5): corre test_k_pke_encrypt_firmware.hex
// sobre core_top_pipelined.sv (K-PKE.Encrypt completo, sin aceleracion
// vectorial, compilado con -O0 por el bug de codegen ya documentado en
// docs/entorno.md) y verifica el ciphertext contra el valor ya validado
// nativamente contra kyber_ref.k_pke_encrypt().

#include <memory>
#include <cstdio>
#include <cstdint>
#include <ctime>

#include "Vcore_top_pipelined.h"
#include "Vcore_top_pipelined___024root.h"
#include "verilated.h"

static const uint8_t expected_c[768] = {
    212, 108, 227, 251, 70, 101, 76, 29, 221, 44, 127, 205, 135, 219, 2, 92, 42, 96, 161, 66, 204, 144, 35, 214, 133, 147, 68, 124, 142, 156, 178, 217, 168, 32, 152, 81, 59, 19, 108, 158, 46, 210, 48, 12, 110, 195, 157, 223, 87, 223, 33, 177, 203, 199, 96, 136, 184, 66, 71, 52, 164, 250, 86, 166, 48, 17, 157, 137, 176, 142, 123, 169, 160, 157, 44, 29, 226, 130, 139, 158, 92, 107, 126, 43, 249, 121, 167, 18, 163, 252, 142, 60, 229, 171, 222, 231, 148, 53, 180, 72, 211, 120, 127, 213, 41, 102, 4, 151, 196, 150, 222, 215, 173, 2, 95, 100, 234, 140, 155, 243, 36, 179, 95, 38, 183, 57, 133, 196, 187, 17, 124, 228, 64, 170, 149, 254, 248, 201, 179, 10, 153, 217, 83, 228, 204, 21, 128, 50, 186, 243, 130, 73, 228, 10, 218, 2, 43, 242, 66, 159, 140, 210, 245, 247, 213, 1, 31, 108, 145, 80, 147, 126, 205, 53, 41, 60, 5, 47, 118, 5, 132, 106, 44, 17, 255, 45, 212, 108, 64, 201, 151, 210, 4, 183, 45, 216, 106, 134, 81, 186, 119, 35, 92, 25, 134, 5, 159, 232, 97, 157, 98, 149, 250, 145, 82, 164, 145, 185, 61, 172, 110, 221, 75, 134, 74, 27, 142, 106, 32, 110, 32, 153, 122, 150, 42, 215, 224, 134, 147, 76, 149, 156, 136, 64, 150, 47, 47, 197, 124, 236, 248, 100, 117, 78, 68, 229, 26, 15, 31, 90, 234, 149, 80, 120, 92, 82, 143, 255, 105, 91, 225, 245, 132, 131, 176, 244, 180, 6, 91, 130, 189, 222, 105, 238, 90, 197, 156, 158, 110, 156, 216, 95, 5, 124, 43, 54, 152, 208, 209, 81, 220, 87, 46, 116, 246, 46, 143, 150, 247, 244, 206, 8, 74, 2, 132, 63, 182, 82, 53, 61, 242, 53, 118, 99, 17, 197, 193, 6, 219, 242, 140, 52, 196, 108, 26, 180, 67, 124, 94, 109, 176, 230, 111, 228, 10, 27, 134, 214, 145, 95, 94, 143, 51, 113, 32, 52, 22, 213, 92, 77, 159, 97, 248, 13, 135, 144, 7, 199, 34, 191, 99, 119, 199, 61, 210, 160, 236, 92, 108, 133, 192, 29, 3, 154, 132, 22, 239, 237, 62, 75, 234, 157, 180, 124, 36, 85, 224, 75, 135, 12, 231, 73, 138, 48, 235, 103, 171, 211, 243, 22, 88, 116, 105, 211, 246, 184, 203, 73, 39, 11, 82, 172, 57, 97, 44, 187, 202, 50, 105, 220, 48, 248, 139, 179, 173, 196, 121, 13, 221, 156, 38, 240, 207, 97, 151, 77, 30, 189, 204, 75, 5, 174, 201, 130, 54, 194, 47, 203, 35, 100, 207, 40, 27, 212, 77, 155, 8, 248, 201, 98, 187, 102, 98, 58, 236, 235, 114, 71, 129, 53, 9, 95, 153, 12, 255, 37, 13, 254, 1, 119, 67, 200, 60, 126, 81, 175, 165, 132, 179, 228, 211, 253, 63, 131, 150, 128, 114, 3, 175, 13, 76, 59, 138, 140, 113, 142, 197, 15, 114, 242, 174, 171, 44, 42, 174, 53, 27, 199, 38, 122, 32, 144, 27, 51, 173, 157, 76, 86, 232, 36, 20, 177, 222, 90, 54, 15, 215, 146, 250, 220, 0, 65, 45, 46, 104, 139, 102, 229, 181, 242, 233, 178, 56, 145, 47, 152, 182, 32, 93, 27, 147, 62, 58, 169, 231, 25, 58, 80, 169, 118, 144, 33, 173, 159, 255, 20, 14, 122, 17, 228, 202, 127, 120, 1, 250, 205, 90, 251, 57, 126, 189, 115, 11, 93, 9, 12, 20, 235, 194, 217, 92, 157, 159, 8, 169, 83, 24, 87, 24, 19, 118, 204, 5, 137, 21, 190, 252, 126, 51, 136, 215, 157, 221, 74, 202, 101, 138, 211, 89, 92, 149, 158, 215, 176, 15, 229, 18, 70, 78, 233, 15, 68, 253, 181, 64, 125, 233, 189, 160, 157, 171, 98, 217, 222, 59, 66, 57, 40, 38, 158, 128, 85, 124, 209, 118, 23, 132, 195, 156, 238, 19, 100, 136, 183, 145, 55, 212, 48, 140, 193, 218, 179, 39, 114, 115, 210, 179, 46, 55, 183, 216, 162, 128, 157, 198, 213, 177, 38, 12, 59, 121, 121, 163, 77, 182, 104, 234, 246, 102, 105, 49, 72, 249, 245, 6, 244, 17, 174, 206, 127, 227, 248, 58, 40, 33, 212, 9, 127, 100, 191, 7, 112, 153, 3, 101, 110, 155, 6, 192, 59, 111, 51, 181, 114, 158, 134, 227, 201, 36, 74, 198, 141, 177, 83, 42, 175, 12, 65
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

    const uint32_t C_ADDR = 0x1000;
    const uint32_t DONE_ADDR = 0x1E00;
    const uint32_t DONE_MAGIC = 0xC0FFEE00u;

    const long MAX_CYCLES = 20000000;
    bool done = false;

    time_t start_time = time(nullptr);

    for (long i = 0; i < MAX_CYCLES; i++) {
        tick();
        if (read_dmem_word(DONE_ADDR) == DONE_MAGIC) {
            done = true;
            std::printf("Firmware termino en el ciclo %ld\n", i);
            break;
        }
        if (i % 2000000 == 0 && i > 0) {
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
        bool c_ok = true;
        for (int i = 0; i < 768; i++) {
            uint8_t got = read_dmem_byte(C_ADDR + i);
            if (got != expected_c[i]) {
                c_ok = false;
                std::printf("FAIL [c_%d]: got=%u esperado=%u\n", i, got, expected_c[i]);
                errors++;
            }
        }
        if (c_ok) std::printf("OK   [ciphertext]: 768/768 bytes correctos\n");
    }

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: k_pke_encrypt.c corriendo en el core real produce el mismo resultado que la validacion nativa.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
