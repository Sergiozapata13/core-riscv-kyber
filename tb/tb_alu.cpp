// tb_alu.cpp
//
// Testbench de la ALU (Fase 1).
// Combinacional pura: no hay reloj, solo se fijan entradas y se llama eval().

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Valu.h"
#include "verilated.h"

static Valu* top;
static int errors = 0;

// Codigos de alu_op_t — deben coincidir exactamente con alu_pkg en alu.sv
enum {
    ALU_ADD  = 0,
    ALU_SUB  = 1,
    ALU_AND  = 2,
    ALU_OR   = 3,
    ALU_XOR  = 4,
    ALU_SLL  = 5,
    ALU_SRL  = 6,
    ALU_SRA  = 7,
    ALU_SLT  = 8,
    ALU_SLTU = 9
};

static void check(const char* label, uint32_t a, uint32_t b, int op,
                   uint32_t expected_result, int expected_zero) {
    top->a      = a;
    top->b      = b;
    top->alu_op = op;
    top->eval();

    bool ok = (top->result == expected_result) && (top->zero == (unsigned)expected_zero);
    if (!ok) {
        std::printf("FAIL [%s]: a=0x%08x b=0x%08x -> result=0x%08x (esperado 0x%08x) zero=%d (esperado %d)\n",
                    label, a, b, top->result, expected_result, top->zero, expected_zero);
        errors++;
    } else {
        std::printf("OK   [%s]: result=0x%08x zero=%d\n", label, top->result, top->zero);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Valu;

    // ---- ADD ----
    check("add_basico",        10, 15, ALU_ADD, 25, 0);
    check("add_overflow_wrap", 0xFFFFFFFF, 1, ALU_ADD, 0x00000000, 1); // wraparound -> 0

    // ---- SUB ----
    check("sub_basico",        20, 5, ALU_SUB, 15, 0);
    check("sub_resultado_cero", 7, 7, ALU_SUB, 0, 1);
    check("sub_negativo_wrap",  0, 1, ALU_SUB, 0xFFFFFFFF, 0); // 0-1 = -1 en complemento a 2

    // ---- AND / OR / XOR ----
    check("and_basico", 0xFF00FF00, 0x0F0F0F0F, ALU_AND, 0x0F000F00, 0);
    check("or_basico",  0xFF00FF00, 0x0F0F0F0F, ALU_OR,  0xFF0FFF0F, 0);
    check("xor_basico", 0xFF00FF00, 0x0F0F0F0F, ALU_XOR, 0xF00FF00F, 0);
    check("xor_iguales_da_cero", 0x12345678, 0x12345678, ALU_XOR, 0, 1);

    // ---- SLL / SRL / SRA ----
    check("sll_shamt_4",      0x00000001, 4,  ALU_SLL, 0x00000010, 0);
    check("sll_shamt_31",     0x00000001, 31, ALU_SLL, 0x80000000, 0);
    check("srl_shamt_4",      0x80000000, 4,  ALU_SRL, 0x08000000, 0); // logico: entra 0 por la izquierda
    check("sra_shamt_4_neg",  0x80000000, 4,  ALU_SRA, 0xF8000000, 0); // aritmetico: se extiende el signo
    check("sra_shamt_4_pos",  0x40000000, 4,  ALU_SRA, 0x04000000, 0);
    check("sll_shamt_usa_solo_5bits", 0x00000001, 32+3, ALU_SLL, 0x00000008, 0); // b[4:0]=3, ignora bits altos

    // ---- SLT (con signo) ----
    check("slt_a_menor_positivos",  3, 5, ALU_SLT, 1, 0);
    check("slt_a_mayor_positivos",  5, 3, ALU_SLT, 0, 1);
    check("slt_negativo_vs_positivo", 0xFFFFFFFF /* -1 */, 1, ALU_SLT, 1, 0); // -1 < 1
    check("slt_iguales",            9, 9, ALU_SLT, 0, 1);

    // ---- SLTU (sin signo) ----
    check("sltu_basico",            3, 5, ALU_SLTU, 1, 0);
    check("sltu_0xFFFFFFFF_vs_1",   0xFFFFFFFF, 1, ALU_SLTU, 0, 1); // sin signo: 0xFFFFFFFF es enorme, no es menor que 1

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: alu — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
