// tb_regfile.cpp
//
// Testbench del register file (Fase 1).
// Casos cubiertos:
//   1. Escritura seguida de lectura en un ciclo posterior (caso basico).
//   2. x0 siempre lee 0, incluso si se intenta escribir con we=1, waddr=0.
//   3. Write-then-read: escribir y leer el MISMO registro en el MISMO ciclo
//      debe devolver el valor nuevo (bypass), no el viejo.
//   4. we=0 no debe modificar ningun registro.
//   5. Lectura simultanea de dos registros distintos por los dos puertos.

#include <memory>
#include <cstdio>

#include "Vregfile.h"
#include "verilated.h"

static Vregfile* top;
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

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vregfile;

    // -----------------------------------------------------------------
    // Caso 1: escritura basica + lectura en un ciclo posterior
    // -----------------------------------------------------------------
    top->we    = 1;
    top->waddr = 5;
    top->wdata = 0xDEADBEEF;
    tick();  // se escribe regs[5] = 0xDEADBEEF en este flanco

    top->we    = 0;
    top->raddr1 = 5;
    top->raddr2 = 0;
    top->eval(); // lectura asincrona, sin nuevo flanco de clk
    check_eq("caso1_write_then_read_ciclo_posterior", top->rdata1, 0xDEADBEEF);

    // -----------------------------------------------------------------
    // Caso 2: x0 hardwireado a 0, incluso si se intenta escribir
    // -----------------------------------------------------------------
    top->we    = 1;
    top->waddr = 0;
    top->wdata = 0xFFFFFFFF;
    tick();  // intento de escritura a x0

    top->we     = 0;
    top->raddr1 = 0;
    top->eval();
    check_eq("caso2_x0_hardwired_tras_intento_de_escritura", top->rdata1, 0x00000000);

    // -----------------------------------------------------------------
    // Caso 3: write-then-read en el MISMO ciclo (bypass)
    // -----------------------------------------------------------------
    // Primero dejamos un valor viejo conocido en regs[7]
    top->we    = 1;
    top->waddr = 7;
    top->wdata = 0x11111111;
    tick();

    // Ahora, en el mismo "ciclo" de escritura, leemos raddr1=7 mientras
    // we=1 y waddr=7 escriben un valor NUEVO — antes del flanco de reloj
    // que efectivamente lo graba, el bypass combinacional debe reflejar
    // ya el valor nuevo.
    top->we     = 1;
    top->waddr  = 7;
    top->wdata  = 0x22222222;
    top->raddr1 = 7;
    top->eval();  // sin tick: mismo ciclo, solo re-evaluamos combinacional
    check_eq("caso3_write_then_read_bypass_mismo_ciclo", top->rdata1, 0x22222222);

    // Confirmamos que efectivamente se grabo tras el flanco
    tick();
    top->we     = 0;
    top->raddr1 = 7;
    top->eval();
    check_eq("caso3b_valor_grabado_tras_flanco", top->rdata1, 0x22222222);

    // -----------------------------------------------------------------
    // Caso 4: we=0 no debe modificar nada
    // -----------------------------------------------------------------
    top->we    = 0;
    top->waddr = 10;
    top->wdata = 0xBAADF00D;
    tick();  // we=0: no deberia escribirse nada en regs[10]

    top->raddr1 = 10;
    top->eval();
    check_eq("caso4_we0_no_escribe", top->rdata1, 0x00000000);

    // -----------------------------------------------------------------
    // Caso 5: lectura simultanea de dos registros distintos
    // -----------------------------------------------------------------
    top->we    = 1;
    top->waddr = 3;
    top->wdata = 0xAAAA0003;
    tick();
    top->we    = 1;
    top->waddr = 4;
    top->wdata = 0xAAAA0004;
    tick();

    top->we     = 0;
    top->raddr1 = 3;
    top->raddr2 = 4;
    top->eval();
    check_eq("caso5_puerto1", top->rdata1, 0xAAAA0003);
    check_eq("caso5_puerto2", top->rdata2, 0xAAAA0004);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: regfile — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
