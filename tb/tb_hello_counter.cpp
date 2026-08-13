// tb_hello_counter.cpp
//
// Testbench trivial de validación de entorno (Fase 0).
// Corre hello_counter unos cuantos ciclos, verifica que el contador avanza
// como se espera, y vuelca una waveform .vcd para confirmar que el flujo
// Verilator -> GTKWave/Surfer funciona de punta a punta.

#include <memory>
#include <cstdio>

#include "Vhello_counter.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    auto top = std::make_unique<Vhello_counter>();

    Verilated::traceEverOn(true);
    auto tfp = std::make_unique<VerilatedVcdC>();
    top->trace(tfp.get(), 99);
    tfp->open("hello_counter.vcd");

    vluint64_t sim_time = 0;
    const vluint64_t HALF_PERIOD = 5;  // 10 time units por ciclo de reloj
    const int RESET_CYCLES = 2;
    const int N_CYCLES = 20;

    // Avanza el reloj medio período (un flanco), evalúa y vuelca la waveform.
    auto tick = [&]() {
        top->clk = !top->clk;
        top->eval();
        tfp->dump(sim_time);
        sim_time += HALF_PERIOD;
    };

    top->rst_n = 0;
    top->clk   = 0;
    top->eval();
    tfp->dump(sim_time);
    sim_time += HALF_PERIOD;

    // Mantener el reset asertado durante RESET_CYCLES ciclos completos.
    for (int i = 0; i < RESET_CYCLES * 2; i++) tick();

    // Liberar el reset. El registro ya cargó '0' en cada flanco bajo reset.
    top->rst_n = 1;

    int errors = 0;
    // El primer flanco de subida tras liberar rst_n ya evalúa count<=count+1
    // sobre el count actual (0), así que el primer valor observado es 1.
    uint8_t expected_count = 1;

    for (int cycle = 0; cycle < N_CYCLES; cycle++) {
        tick();  // flanco de bajada (o el que corresponda) — no muestreamos aquí
        tick();  // flanco de subida — el registro ya actualizó count

        if (top->count != expected_count) {
            std::printf("ERROR en ciclo %d: count=%u expected=%u\n",
                        cycle, top->count, expected_count);
            errors++;
        }
        expected_count++;
    }

    tfp->close();

    if (errors == 0) {
        std::printf("PASS: hello_counter conto %d ciclos correctamente. VCD escrito en sim/hello_counter.vcd\n",
                    N_CYCLES);
        return 0;
    } else {
        std::printf("FAIL: %d errores detectados.\n", errors);
        return 1;
    }
}
