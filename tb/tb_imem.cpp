// tb_imem.cpp
//
// Testbench de la memoria de instrucciones (Fase 1).
// Carga un .hex de prueba (tb/hex/imem_test.hex) y verifica que cada
// palabra se lee correctamente en su direccion de byte correspondiente
// (word 0 -> addr 0x0, word 1 -> addr 0x4, etc.), incluyendo que
// addr[1:0] se ignora (fetch siempre alineado a palabra).

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vimem.h"
#include "verilated.h"

static Vimem* top;
static int errors = 0;

static void check(const char* label, uint32_t addr, uint32_t expected) {
    top->addr = addr;
    top->eval();

    if (top->instr != expected) {
        std::printf("FAIL [%s]: addr=0x%08x instr=0x%08x esperado=0x%08x\n",
                     label, addr, top->instr, expected);
        errors++;
    } else {
        std::printf("OK   [%s]: addr=0x%08x -> instr=0x%08x\n", label, addr, top->instr);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vimem;

    // Contenido esperado segun tb/hex/imem_test.hex:
    //   word0=deadbeef word1=cafef00d word2=12345678
    //   word3=00000000 word4=000000ff word5=80000001
    check("word0_addr0x0", 0x00, 0xdeadbeef);
    check("word1_addr0x4", 0x04, 0xcafef00d);
    check("word2_addr0x8", 0x08, 0x12345678);
    check("word3_addr0xC", 0x0C, 0x00000000);
    check("word4_addr0x10", 0x10, 0x000000ff);
    check("word5_addr0x14", 0x14, 0x80000001);

    // addr[1:0] debe ignorarse: cualquier direccion dentro de la misma
    // palabra de 4 bytes debe devolver el mismo valor.
    check("word0_addr_misaligned_1", 0x01, 0xdeadbeef);
    check("word0_addr_misaligned_2", 0x02, 0xdeadbeef);
    check("word0_addr_misaligned_3", 0x03, 0xdeadbeef);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: imem — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
