// tb_mem_wb_reg.cpp
//
// Testbench del registro de segmentacion MEM/WB (Fase 2).
// El mas simple de los cuatro: solo reg_write necesita forzarse a
// inerte en flush/valid_in=0 (mem_to_reg y jump son inofensivas por si
// solas si reg_write=0, como se explica en mem_wb_reg.sv).

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vmem_wb_reg.h"
#include "verilated.h"

static Vmem_wb_reg* top;
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

static void set_all_inputs(Vmem_wb_reg* t, uint32_t mem_rdata, uint32_t alu_result,
                            uint32_t pc4, uint8_t rd, int reg_write, int mem_to_reg,
                            int jump, int valid) {
    t->mem_rdata_in  = mem_rdata;
    t->alu_result_in = alu_result;
    t->pc_plus4_in   = pc4;
    t->rd_in         = rd;
    t->reg_write_in  = reg_write;
    t->mem_to_reg_in = mem_to_reg;
    t->jump_in       = jump;
    t->valid_in      = valid;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vmem_wb_reg;

    // ---- Caso 1: reset ----
    top->rst_n = 1;
    top->stall = 0;
    top->flush = 0;
    set_all_inputs(top, 0x1000, 0x2000, 0x3000, 5, 1, 1, 1, 1);
    top->clk = 0;
    top->eval();

    top->rst_n = 0;
    top->eval();
    check_eq("caso1_reset_valid", top->valid_out, 0);
    check_eq("caso1_reset_reg_write", top->reg_write_out, 0);

    top->rst_n = 1;

    // ---- Caso 2: avance normal ----
    set_all_inputs(top, 0xAAAA0001, 0xBBBB0002, 0xCCCC0003, 12, 1, 1, 0, 1);
    tick();
    check_eq("caso2_valid",       top->valid_out, 1);
    check_eq("caso2_mem_rdata",   top->mem_rdata_out, 0xAAAA0001);
    check_eq("caso2_alu_result",  top->alu_result_out, 0xBBBB0002);
    check_eq("caso2_pc_plus4",    top->pc_plus4_out, 0xCCCC0003);
    check_eq("caso2_rd",          top->rd_out, 12);
    check_eq("caso2_reg_write",   top->reg_write_out, 1);
    check_eq("caso2_mem_to_reg",  top->mem_to_reg_out, 1);

    // ---- Caso 3: stall mantiene el contenido ----
    top->stall = 1;
    set_all_inputs(top, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 31, 0, 0, 0, 1);
    tick();
    check_eq("caso3_stall_mem_rdata", top->mem_rdata_out, 0xAAAA0001);  // el mismo de antes
    check_eq("caso3_stall_rd",        top->rd_out, 12);

    // ---- Caso 4: flush -> valid=0 y reg_write=0 ----
    top->stall = 0;
    top->flush = 1;
    set_all_inputs(top, 0x5000, 0x6000, 0x7000, 3, 1, 1, 1, 1);
    tick();
    check_eq("caso4_flush_valid",     top->valid_out, 0);
    check_eq("caso4_flush_reg_write", top->reg_write_out, 0);

    // ---- Caso 5: valid_in=0 -> reg_write forzado a 0 ----
    top->flush = 0;
    set_all_inputs(top, 0x8000, 0x9000, 0xA000, 4, 1, 1, 1, /*valid=*/0);
    tick();
    check_eq("caso5_valid_in_0_valid_out", top->valid_out, 0);
    check_eq("caso5_valid_in_0_reg_write", top->reg_write_out, 0);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: mem_wb_reg — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
