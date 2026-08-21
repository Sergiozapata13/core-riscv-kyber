// tb_waveform_extract.cpp
//
// Extrae, ciclo a ciclo, las señales relevantes para la waveform
// anotada del desacople (A.1) y la serialización (A.4) — Fase 6.
// Corre el mismo firmware que tb_core_top_pipelined_vector.cpp
// (mixed_scalar_vector.hex) pero en vez de solo pasar/fallar, vuelca
// el estado exacto a stdout en formato CSV, para dibujar la waveform
// con datos reales (no estimados) en docs/diagrams/waveform_desacople.svg.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vcore_top_pipelined.h"
#include "Vcore_top_pipelined___024root.h"
#include "verilated.h"

static Vcore_top_pipelined* top;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

static uint32_t read_dmem_word(uint32_t byte_addr) {
    return top->rootp->core_top_pipelined__DOT__u_dmem__DOT__mem[byte_addr / 4];
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vcore_top_pipelined;

    top->rst_n = 0;
    top->clk = 0;
    tick();
    tick();
    top->rst_n = 1;

    std::printf("ciclo,pc,is_vector_instr,gated_start,vector_busy,x2,dmem_0x100,dmem_0x104\n");

    for (int i = 0; i < 1450; i++) {
        tick();
        uint32_t pc = top->rootp->core_top_pipelined__DOT__pc;
        bool is_vec = top->rootp->core_top_pipelined__DOT__is_vector_instr;
        bool gstart = top->rootp->core_top_pipelined__DOT__u_vector_unit__DOT__gated_start;
        bool busy = top->rootp->core_top_pipelined__DOT__u_vector_unit__DOT__any_busy;
        uint32_t x2 = top->rootp->core_top_pipelined__DOT__u_regfile__DOT__regs[2];
        uint32_t d100 = read_dmem_word(0x100);
        uint32_t d104 = read_dmem_word(0x104);

        std::printf("%d,%u,%d,%d,%d,%u,%u,%u\n", i, pc, is_vec, gstart, busy, x2, d100, d104);
    }

    delete top;
    return 0;
}
