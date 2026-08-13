// tb_imm_gen.cpp
//
// Testbench del generador de inmediato (Fase 1).
// Combinacional pura: se construye una instruccion cruda de 32 bits con
// los campos de inmediato deseados en las posiciones correctas del
// encoding, y se verifica que el modulo ensamble y sign-extienda
// correctamente para cada formato.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vimm_gen.h"
#include "verilated.h"

static Vimm_gen* top;
static int errors = 0;

enum { IMM_I=0, IMM_S=1, IMM_B=2, IMM_U=3, IMM_J=4, IMM_X=7 };

static void check(const char* label, uint32_t instr, int imm_sel, int32_t expected) {
    top->instr   = instr;
    top->imm_sel = imm_sel;
    top->eval();

    int32_t got = (int32_t)top->imm;
    if (got != expected) {
        std::printf("FAIL [%s]: instr=0x%08x imm_sel=%d -> imm=%d (0x%08x) esperado=%d (0x%08x)\n",
                     label, instr, imm_sel, got, (uint32_t)got, expected, (uint32_t)expected);
        errors++;
    } else {
        std::printf("OK   [%s]: imm=%d (0x%08x)\n", label, got, (uint32_t)got);
    }
}

// ---- Helpers para construir instrucciones con el inmediato deseado ----
// Cada helper coloca el inmediato dado en los bits de instr[] que le
// corresponden segun el formato, dejando el resto de la instruccion en 0
// (el resto de campos — rd, rs1, rs2, funct3, opcode — son irrelevantes
// para este modulo, que solo mira los bits de inmediato).

static uint32_t make_itype(int32_t imm12) {
    // imm[11:0] = instr[31:20]
    return ((uint32_t)imm12 & 0xFFF) << 20;
}

static uint32_t make_stype(int32_t imm12) {
    uint32_t u = (uint32_t)imm12 & 0xFFF;
    uint32_t imm11_5 = (u >> 5) & 0x7F;
    uint32_t imm4_0  = u & 0x1F;
    return (imm11_5 << 25) | (imm4_0 << 7);
}

static uint32_t make_btype(int32_t imm13) {
    // imm[12|10:5|4:1|11], siempre con imm[0]=0 (branches saltan a direcciones pares)
    uint32_t u = (uint32_t)imm13;
    uint32_t b12   = (u >> 12) & 0x1;
    uint32_t b11   = (u >> 11) & 0x1;
    uint32_t b10_5 = (u >> 5)  & 0x3F;
    uint32_t b4_1  = (u >> 1)  & 0xF;
    return (b12 << 31) | (b10_5 << 25) | (b4_1 << 8) | (b11 << 7);
}

static uint32_t make_utype(uint32_t imm20) {
    // imm[31:12] = instr[31:12]; el modulo pone los 12 bits bajos en 0
    return (imm20 & 0xFFFFF) << 12;
}

static uint32_t make_jtype(int32_t imm21) {
    // imm[20|10:1|11|19:12], siempre con imm[0]=0
    uint32_t u = (uint32_t)imm21;
    uint32_t b20    = (u >> 20) & 0x1;
    uint32_t b19_12 = (u >> 12) & 0xFF;
    uint32_t b11    = (u >> 11) & 0x1;
    uint32_t b10_1  = (u >> 1)  & 0x3FF;
    return (b20 << 31) | (b10_1 << 21) | (b11 << 20) | (b19_12 << 12);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vimm_gen;

    // ---- IMM_I ----
    check("i_positivo_pequeno", make_itype(5),   IMM_I, 5);
    check("i_cero",             make_itype(0),   IMM_I, 0);
    check("i_negativo_-1",      make_itype(-1),  IMM_I, -1);
    check("i_max_positivo_2047",make_itype(2047),IMM_I, 2047);
    check("i_min_negativo_-2048",make_itype(-2048),IMM_I, -2048);

    // ---- IMM_S ----
    check("s_positivo",  make_stype(100),  IMM_S, 100);
    check("s_negativo",  make_stype(-100), IMM_S, -100);
    check("s_max_2047",  make_stype(2047), IMM_S, 2047);
    check("s_min_-2048", make_stype(-2048),IMM_S, -2048);

    // ---- IMM_B (siempre par: bit 0 forzado a 0 por el propio encoding) ----
    check("b_positivo_par",   make_btype(100),   IMM_B, 100);
    check("b_negativo_par",   make_btype(-100),  IMM_B, -100);
    check("b_max_4094",       make_btype(4094),  IMM_B, 4094);
    check("b_min_-4096",      make_btype(-4096), IMM_B, -4096);

    // ---- IMM_U ----
    check("u_basico",     make_utype(0x12345), IMM_U, (int32_t)0x12345000);
    check("u_todo_unos",  make_utype(0xFFFFF), IMM_U, (int32_t)0xFFFFF000);
    check("u_cero",       make_utype(0x00000), IMM_U, 0);

    // ---- IMM_J (siempre par: bit 0 forzado a 0 por el propio encoding) ----
    check("j_positivo_par", make_jtype(2000),   IMM_J, 2000);
    check("j_negativo_par", make_jtype(-2000),  IMM_J, -2000);
    check("j_max_aprox",    make_jtype(1048574),IMM_J, 1048574);
    check("j_min_-1048576", make_jtype(-1048576),IMM_J, -1048576);

    // ---- IMM_X (R-type: no consume inmediato, debe dar 0) ----
    check("x_no_aplica", 0xFFFFFFFF, IMM_X, 0);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: imm_gen — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
