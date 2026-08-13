// tb_ex_mem_reg.cpp
//
// Testbench del registro de segmentacion EX/MEM (Fase 2).
// Mismo patron de casos que id_ex_reg.cpp, adaptado al contenido de
// este registro (sin branch/alu_src/alu_op/opcode/imm, que ya no
// sobreviven mas alla de EX).

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vex_mem_reg.h"
#include "verilated.h"

static Vex_mem_reg* top;
static int errors = 0;

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

static void set_all_inputs(Vex_mem_reg* t, uint32_t alu_result, uint32_t rs2, uint32_t pc4,
                            uint8_t rd, uint8_t funct3, int reg_write, int mem_read,
                            int mem_write, int mem_to_reg, int jump, int valid) {
    t->alu_result_in = alu_result;
    t->rs2_data_in   = rs2;
    t->pc_plus4_in   = pc4;
    t->rd_in         = rd;
    t->funct3_in     = funct3;
    t->reg_write_in  = reg_write;
    t->mem_read_in   = mem_read;
    t->mem_write_in  = mem_write;
    t->mem_to_reg_in = mem_to_reg;
    t->jump_in       = jump;
    t->valid_in      = valid;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vex_mem_reg;

    // ---- Caso 1: reset ----
    top->rst_n = 1;
    top->stall = 0;
    top->flush = 0;
    set_all_inputs(top, 0x1000, 0x2000, 0x3000, 5, 2, 1,1,1,1,1, 1);
    top->clk = 0;
    top->eval();

    top->rst_n = 0;
    top->eval();
    check_eq("caso1_reset_valid", top->valid_out, 0);
    check_eq("caso1_reset_reg_write", top->reg_write_out, 0);
    check_eq("caso1_reset_mem_read", top->mem_read_out, 0);
    check_eq("caso1_reset_mem_write", top->mem_write_out, 0);
    check_eq("caso1_reset_jump", top->jump_out, 0);

    top->rst_n = 1;

    // ---- Caso 2: avance normal ----
    set_all_inputs(top, 0xAAAA0001, 0xBBBB0002, 0xCCCC0003, 9, 5, 1, 0, 1, 0, 0, 1);
    tick();
    check_eq("caso2_valid",      top->valid_out, 1);
    check_eq("caso2_alu_result", top->alu_result_out, 0xAAAA0001);
    check_eq("caso2_rs2_data",   top->rs2_data_out, 0xBBBB0002);
    check_eq("caso2_pc_plus4",   top->pc_plus4_out, 0xCCCC0003);
    check_eq("caso2_rd",         top->rd_out, 9);
    check_eq("caso2_funct3",     top->funct3_out, 5);
    check_eq("caso2_reg_write",  top->reg_write_out, 1);
    check_eq("caso2_mem_write",  top->mem_write_out, 1);

    // ---- Caso 3: stall mantiene el contenido ----
    top->stall = 1;
    set_all_inputs(top, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 31, 7, 0,0,0,0,0, 1);
    tick();
    check_eq("caso3_stall_alu_result", top->alu_result_out, 0xAAAA0001);  // el mismo de antes
    check_eq("caso3_stall_rd",         top->rd_out, 9);

    // ---- Caso 4: flush -> valid=0 y control inerte ----
    top->stall = 0;
    top->flush = 1;
    set_all_inputs(top, 0x5000, 0x1, 0x6000, 3, 5, 1,1,1,1,1, 1);
    tick();
    check_eq("caso4_flush_valid",     top->valid_out, 0);
    check_eq("caso4_flush_reg_write", top->reg_write_out, 0);
    check_eq("caso4_flush_mem_read",  top->mem_read_out, 0);
    check_eq("caso4_flush_mem_write", top->mem_write_out, 0);
    check_eq("caso4_flush_jump",      top->jump_out, 0);

    // ---- Caso 5: valid_in=0 -> control inerte ----
    top->flush = 0;
    set_all_inputs(top, 0x7000, 0x1, 0x8000, 4, 2, 1,1,1,1,1, /*valid=*/0);
    tick();
    check_eq("caso5_valid_in_0_valid_out", top->valid_out, 0);
    check_eq("caso5_valid_in_0_reg_write", top->reg_write_out, 0);
    check_eq("caso5_valid_in_0_mem_write", top->mem_write_out, 0);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: ex_mem_reg — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
