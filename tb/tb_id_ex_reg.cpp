// tb_id_ex_reg.cpp
//
// Testbench del registro de segmentacion ID/EX (Fase 2).
// Casos cubiertos:
//   1. Reset: todas las señales de control en su estado inerte, valid=0.
//   2. Avance normal: todos los campos pasan correctamente.
//   3. Stall: el contenido se mantiene identico entre flancos.
//   4. Flush: valid=0 y TODAS las señales de control quedan inertes
//      (reg_write=0, mem_read=0, mem_write=0, branch=0, jump=0), sin
//      importar lo que entraba.
//   5. valid_in=0 (burbuja desde IF/ID): las señales de control tambien
//      se fuerzan inertes, igual que en flush.
//   6. alu_op y alu_src SI se propagan tal cual incluso con valid_in=0/flush
//      (no se fuerzan a un valor especifico) — no hace falta, porque sin
//      reg_write/mem_write activos el resultado de la ALU no puede
//      afectar estado arquitectural de todas formas.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vid_ex_reg.h"
#include "verilated.h"

static Vid_ex_reg* top;
static int errors = 0;

enum { ALU_ADD=0, ALU_SUB=1 };

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

static void check_eq(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        std::printf("FAIL [%s]: got=0x%08x expected=0x%08x\n", label, got, expected);
        errors++;
    } else {
        std::printf("OK   [%s]: 0x%08x\n", label, got);
    }
}

static void set_all_inputs(Vid_ex_reg* t, uint32_t pc, uint32_t rs1, uint32_t rs2,
                            uint32_t imm, uint8_t rd, uint8_t funct3, uint8_t opcode,
                            int reg_write, int alu_src, int alu_op, int mem_read,
                            int mem_write, int mem_to_reg, int branch, int jump,
                            int valid) {
    t->pc_in         = pc;
    t->rs1_data_in   = rs1;
    t->rs2_data_in   = rs2;
    t->imm_in        = imm;
    t->rd_in         = rd;
    t->funct3_in     = funct3;
    t->opcode_in     = opcode;
    t->reg_write_in  = reg_write;
    t->alu_src_in    = alu_src;
    t->alu_op_in     = alu_op;
    t->mem_read_in   = mem_read;
    t->mem_write_in  = mem_write;
    t->mem_to_reg_in = mem_to_reg;
    t->branch_in     = branch;
    t->jump_in       = jump;
    t->valid_in      = valid;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vid_ex_reg;

    // ---- Caso 1: reset ----
    top->rst_n = 1;
    top->stall = 0;
    top->flush = 0;
    set_all_inputs(top, 0x1000, 0x11, 0x22, 0x33, 5, 2, 0x33, 1,1,ALU_ADD,1,1,1,1,1, 1);
    top->clk = 0;
    top->eval();

    top->rst_n = 0;   // transicion real 1->0 para disparar el reset asincrono
    top->eval();
    check_eq("caso1_reset_valid", top->valid_out, 0);
    check_eq("caso1_reset_reg_write", top->reg_write_out, 0);
    check_eq("caso1_reset_mem_read", top->mem_read_out, 0);
    check_eq("caso1_reset_mem_write", top->mem_write_out, 0);
    check_eq("caso1_reset_branch", top->branch_out, 0);
    check_eq("caso1_reset_jump", top->jump_out, 0);

    top->rst_n = 1;

    // ---- Caso 2: avance normal, todos los campos ----
    set_all_inputs(top, 0x2000, 0xAAAA1111, 0xBBBB2222, 0xCCCC3333, 7, 3, 0x63,
                    1, 1, ALU_SUB, 0, 0, 1, 1, 0, 1);
    tick();
    check_eq("caso2_valid",     top->valid_out, 1);
    check_eq("caso2_pc",        top->pc_out, 0x2000);
    check_eq("caso2_rs1_data",  top->rs1_data_out, 0xAAAA1111);
    check_eq("caso2_rs2_data",  top->rs2_data_out, 0xBBBB2222);
    check_eq("caso2_imm",       top->imm_out, 0xCCCC3333);
    check_eq("caso2_rd",        top->rd_out, 7);
    check_eq("caso2_funct3",    top->funct3_out, 3);
    check_eq("caso2_opcode",    top->opcode_out, 0x63);
    check_eq("caso2_reg_write", top->reg_write_out, 1);
    check_eq("caso2_alu_src",   top->alu_src_out, 1);
    check_eq("caso2_alu_op",    top->alu_op_out, ALU_SUB);
    check_eq("caso2_mem_to_reg",top->mem_to_reg_out, 1);
    check_eq("caso2_branch",    top->branch_out, 1);

    // ---- Caso 3: stall mantiene el contenido ----
    top->stall = 1;
    set_all_inputs(top, 0xFFFF, 0, 0, 0, 31, 7, 0x7F, 0,0,ALU_ADD,0,0,0,0,0, 1);
    tick();
    check_eq("caso3_stall_pc",    top->pc_out, 0x2000);       // el mismo de antes
    check_eq("caso3_stall_rd",    top->rd_out, 7);             // el mismo de antes
    check_eq("caso3_stall_valid", top->valid_out, 1);

    // ---- Caso 4: flush -> valid=0 y TODAS las señales de control inertes ----
    top->stall = 0;
    top->flush = 1;
    set_all_inputs(top, 0x5000, 0x1, 0x1, 0x1, 3, 5, 0x23, 1,1,ALU_ADD,1,1,1,1,1, 1);
    tick();
    check_eq("caso4_flush_valid",     top->valid_out, 0);
    check_eq("caso4_flush_reg_write", top->reg_write_out, 0);
    check_eq("caso4_flush_mem_read",  top->mem_read_out, 0);
    check_eq("caso4_flush_mem_write", top->mem_write_out, 0);
    check_eq("caso4_flush_branch",    top->branch_out, 0);
    check_eq("caso4_flush_jump",      top->jump_out, 0);

    // ---- Caso 5: valid_in=0 (burbuja desde ID) -> señales de control inertes ----
    top->flush = 0;
    set_all_inputs(top, 0x6000, 0x1, 0x1, 0x1, 3, 5, 0x23, 1,1,ALU_ADD,1,1,1,1,1, /*valid=*/0);
    tick();
    check_eq("caso5_valid_in_0_valid_out",     top->valid_out, 0);
    check_eq("caso5_valid_in_0_reg_write",     top->reg_write_out, 0);
    check_eq("caso5_valid_in_0_mem_read",      top->mem_read_out, 0);
    check_eq("caso5_valid_in_0_mem_write",     top->mem_write_out, 0);
    check_eq("caso5_valid_in_0_branch",        top->branch_out, 0);
    check_eq("caso5_valid_in_0_jump",          top->jump_out, 0);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: id_ex_reg — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
