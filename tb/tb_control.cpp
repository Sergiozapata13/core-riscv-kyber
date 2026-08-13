// tb_control.cpp
//
// Testbench de la unidad de control (Fase 1).
// Combinacional pura: se fijan opcode/funct3/funct7 y se verifica el vector
// completo de senales de control esperado para instrucciones reales
// tomadas de la green card.
//
// alu_op_t (debe coincidir con alu_pkg en alu.sv):
//   ADD=0 SUB=1 AND=2 OR=3 XOR=4 SLL=5 SRL=6 SRA=7 SLT=8 SLTU=9
//
// imm_sel_t (debe coincidir con control_pkg en control.sv):
//   IMM_I=0 IMM_S=1 IMM_B=2 IMM_U=3 IMM_J=4 IMM_X=7

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vcontrol.h"
#include "verilated.h"

static Vcontrol* top;
static int errors = 0;

enum { ALU_ADD=0, ALU_SUB=1, ALU_AND=2, ALU_OR=3, ALU_XOR=4,
       ALU_SLL=5, ALU_SRL=6, ALU_SRA=7, ALU_SLT=8, ALU_SLTU=9 };
enum { IMM_I=0, IMM_S=1, IMM_B=2, IMM_U=3, IMM_J=4, IMM_X=7 };

struct Expected {
    int reg_write, alu_src, alu_op, mem_read, mem_write,
        mem_to_reg, branch, jump, imm_sel, illegal;
};

static void check(const char* label, uint8_t opcode, uint8_t funct3, uint8_t funct7,
                   Expected exp) {
    top->opcode = opcode;
    top->funct3 = funct3;
    top->funct7 = funct7;
    top->eval();

    bool ok = true;
    ok &= (top->reg_write  == (unsigned)exp.reg_write);
    ok &= (top->alu_src    == (unsigned)exp.alu_src);
    ok &= (top->alu_op     == (unsigned)exp.alu_op);
    ok &= (top->mem_read   == (unsigned)exp.mem_read);
    ok &= (top->mem_write  == (unsigned)exp.mem_write);
    ok &= (top->mem_to_reg == (unsigned)exp.mem_to_reg);
    ok &= (top->branch     == (unsigned)exp.branch);
    ok &= (top->jump       == (unsigned)exp.jump);
    ok &= (top->imm_sel    == (unsigned)exp.imm_sel);
    ok &= (top->illegal    == (unsigned)exp.illegal);

    if (!ok) {
        std::printf("FAIL [%s]:\n", label);
        std::printf("  got:      reg_write=%d alu_src=%d alu_op=%d mem_read=%d mem_write=%d mem_to_reg=%d branch=%d jump=%d imm_sel=%d illegal=%d\n",
                     top->reg_write, top->alu_src, top->alu_op, top->mem_read, top->mem_write,
                     top->mem_to_reg, top->branch, top->jump, top->imm_sel, top->illegal);
        std::printf("  expected: reg_write=%d alu_src=%d alu_op=%d mem_read=%d mem_write=%d mem_to_reg=%d branch=%d jump=%d imm_sel=%d illegal=%d\n",
                     exp.reg_write, exp.alu_src, exp.alu_op, exp.mem_read, exp.mem_write,
                     exp.mem_to_reg, exp.branch, exp.jump, exp.imm_sel, exp.illegal);
        errors++;
    } else {
        std::printf("OK   [%s]\n", label);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vcontrol;

    // ---- R-type (opcode 0110011) ----
    // add: funct3=000 funct7=0000000
    check("add", 0b0110011, 0b000, 0b0000000,
          {1,0,ALU_ADD, 0,0,0, 0,0, IMM_X, 0});
    // sub: funct3=000 funct7=0100000 (funct7[5]=1)
    check("sub", 0b0110011, 0b000, 0b0100000,
          {1,0,ALU_SUB, 0,0,0, 0,0, IMM_X, 0});
    // sll: funct3=001
    check("sll", 0b0110011, 0b001, 0b0000000,
          {1,0,ALU_SLL, 0,0,0, 0,0, IMM_X, 0});
    // slt: funct3=010
    check("slt", 0b0110011, 0b010, 0b0000000,
          {1,0,ALU_SLT, 0,0,0, 0,0, IMM_X, 0});
    // sltu: funct3=011
    check("sltu", 0b0110011, 0b011, 0b0000000,
          {1,0,ALU_SLTU, 0,0,0, 0,0, IMM_X, 0});
    // xor: funct3=100
    check("xor", 0b0110011, 0b100, 0b0000000,
          {1,0,ALU_XOR, 0,0,0, 0,0, IMM_X, 0});
    // srl: funct3=101 funct7=0000000
    check("srl", 0b0110011, 0b101, 0b0000000,
          {1,0,ALU_SRL, 0,0,0, 0,0, IMM_X, 0});
    // sra: funct3=101 funct7=0100000
    check("sra", 0b0110011, 0b101, 0b0100000,
          {1,0,ALU_SRA, 0,0,0, 0,0, IMM_X, 0});
    // or: funct3=110
    check("or", 0b0110011, 0b110, 0b0000000,
          {1,0,ALU_OR, 0,0,0, 0,0, IMM_X, 0});
    // and: funct3=111
    check("and", 0b0110011, 0b111, 0b0000000,
          {1,0,ALU_AND, 0,0,0, 0,0, IMM_X, 0});

    // ---- I-type aritmetico (opcode 0010011) ----
    // addi: funct3=000 (nunca SUB, aunque funct7[5]=1 por casualidad)
    check("addi", 0b0010011, 0b000, 0b0000000,
          {1,1,ALU_ADD, 0,0,0, 0,0, IMM_I, 0});
    // slli: funct3=001
    check("slli", 0b0010011, 0b001, 0b0000000,
          {1,1,ALU_SLL, 0,0,0, 0,0, IMM_I, 0});
    // srli: funct3=101 funct7[5]=0
    check("srli", 0b0010011, 0b101, 0b0000000,
          {1,1,ALU_SRL, 0,0,0, 0,0, IMM_I, 0});
    // srai: funct3=101 funct7[5]=1
    check("srai", 0b0010011, 0b101, 0b0100000,
          {1,1,ALU_SRA, 0,0,0, 0,0, IMM_I, 0});
    // andi: funct3=111
    check("andi", 0b0010011, 0b111, 0b0000000,
          {1,1,ALU_AND, 0,0,0, 0,0, IMM_I, 0});

    // ---- Loads (opcode 0000011) ----
    // lw: funct3=010 (el testbench no distingue lb/lh/lw a nivel de control,
    // el tamano del acceso lo maneja la memoria de datos con funct3 directo)
    check("lw", 0b0000011, 0b010, 0b0000000,
          {1,1,ALU_ADD, 1,0,1, 0,0, IMM_I, 0});

    // ---- Stores (opcode 0100011) ----
    // sw: funct3=010
    check("sw", 0b0100011, 0b010, 0b0000000,
          {0,1,ALU_ADD, 0,1,0, 0,0, IMM_S, 0});

    // ---- Branches (opcode 1100011) ----
    check("beq", 0b1100011, 0b000, 0b0000000,
          {0,0,ALU_SUB, 0,0,0, 1,0, IMM_B, 0});
    check("bne", 0b1100011, 0b001, 0b0000000,
          {0,0,ALU_SUB, 0,0,0, 1,0, IMM_B, 0});
    check("blt", 0b1100011, 0b100, 0b0000000,
          {0,0,ALU_SLT, 0,0,0, 1,0, IMM_B, 0});
    check("bge", 0b1100011, 0b101, 0b0000000,
          {0,0,ALU_SLT, 0,0,0, 1,0, IMM_B, 0});
    check("bltu", 0b1100011, 0b110, 0b0000000,
          {0,0,ALU_SLTU, 0,0,0, 1,0, IMM_B, 0});
    check("bgeu", 0b1100011, 0b111, 0b0000000,
          {0,0,ALU_SLTU, 0,0,0, 1,0, IMM_B, 0});

    // ---- Jumps ----
    // jal: opcode 1101111
    check("jal", 0b1101111, 0b000, 0b0000000,
          {1,0,ALU_ADD, 0,0,0, 0,1, IMM_J, 0});
    // jalr: opcode 1100111
    check("jalr", 0b1100111, 0b000, 0b0000000,
          {1,1,ALU_ADD, 0,0,0, 0,1, IMM_I, 0});

    // ---- U-type ----
    check("lui", 0b0110111, 0b000, 0b0000000,
          {1,1,ALU_ADD, 0,0,0, 0,0, IMM_U, 0});
    check("auipc", 0b0010111, 0b000, 0b0000000,
          {1,1,ALU_ADD, 0,0,0, 0,0, IMM_U, 0});

    // ---- Opcode invalido ----
    check("opcode_invalido", 0b1111111, 0b000, 0b0000000,
          {0,0,ALU_ADD, 0,0,0, 0,0, IMM_X, 1});

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: control — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
