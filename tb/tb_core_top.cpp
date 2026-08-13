// tb_core_top.cpp
//
// Testbench del datapath monociclo completo (Fase 1, criterio de cierre).
//
// Corre el firmware fib.hex (Fibonacci iterativo, 10 terminos) sobre
// core_top y verifica:
//   1. Los 10 valores de Fibonacci correctos en memoria de datos
//      (direcciones 0x100 a 0x124).
//   2. El patron de status 0xC0FFEE00 en 0x200, que confirma que el
//      programa llego hasta el final (no solo que no crasheo a mitad
//      de camino).
//   3. Que el programa efectivamente termina en el loop 'halt' (PC deja
//      de avanzar), en vez de correr fuera de control.
//
// Como el testbench no tiene visibilidad directa de la memoria de datos
// interna de dmem (es un array privado del modulo), se lee via la
// jerarquia de senales expuesta por Verilator (rootp->...).

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vcore_top.h"
#include "Vcore_top___024root.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

static Vcore_top* top;
static VerilatedVcdC* tfp = nullptr;
static vluint64_t sim_time = 0;
static int errors = 0;

static void tick() {
    top->clk = 0;
    top->eval();
    if (tfp) tfp->dump(sim_time++);
    top->clk = 1;
    top->eval();
    if (tfp) tfp->dump(sim_time++);
}

static void check_eq(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        std::printf("FAIL [%s]: got=0x%08x expected=0x%08x\n", label, got, expected);
        errors++;
    } else {
        std::printf("OK   [%s]: 0x%08x\n", label, got);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vcore_top;

    Verilated::traceEverOn(true);
    tfp = new VerilatedVcdC;
    top->trace(tfp, 99);
    tfp->open("core_top.vcd");

    // Reset
    top->rst_n = 0;
    top->clk   = 0;
    tick();
    tick();
    top->rst_n = 1;

    // Correr suficientes ciclos para que el programa termine y quede
    // atrapado en el loop 'halt' (direccion 0x60, ver disassembly de
    // fib.s). El programa tiene ~25 instrucciones estaticas mas un loop
    // de 8 iteraciones (~8*7=56 instrucciones dinamicas) — 500 ciclos es
    // sobrado margen.
    const int MAX_CYCLES = 500;
    uint32_t pc_prev = 0xFFFFFFFF;
    int settled_count = 0;
    bool reached_halt = false;

    for (int i = 0; i < MAX_CYCLES; i++) {
        tick();
        uint32_t pc_now = top->rootp->core_top__DOT__pc;

        if (pc_now == pc_prev) {
            settled_count++;
            if (settled_count > 3) {
                reached_halt = true;
                break;
            }
        } else {
            settled_count = 0;
        }
        pc_prev = pc_now;
    }

    if (!reached_halt) {
        std::printf("FAIL [programa_termina]: no se estabilizo en 'halt' tras %d ciclos (PC final=0x%08x)\n",
                     MAX_CYCLES, pc_prev);
        errors++;
    } else {
        std::printf("OK   [programa_termina]: PC se estabilizo en 0x%08x (halt)\n", pc_prev);
        check_eq("pc_final_es_halt_0x60", pc_prev, 0x60);
    }

    // Fibonacci esperado: F0..F9 = 0,1,1,2,3,5,8,13,21,34
    const uint32_t fib_expected[10] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};

    for (int i = 0; i < 10; i++) {
        uint32_t word_idx = (0x100 / 4) + i;  // dmem interno indexado por palabra
        uint32_t got = top->rootp->core_top__DOT__u_dmem__DOT__mem[word_idx];
        char label[32];
        std::snprintf(label, sizeof(label), "fib_%d", i);
        check_eq(label, got, fib_expected[i]);
    }

    // Patron de status en 0x200
    uint32_t status = top->rootp->core_top__DOT__u_dmem__DOT__mem[0x200 / 4];
    check_eq("status_pattern_0x200", status, 0xC0FFEE00);

    tfp->close();
    delete top;

    if (errors == 0) {
        std::printf("\nPASS: core_top (fib.s) — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
