// tb_vpmul_engine.cpp
//
// Testbench del motor vpmul completo (Fase 4).
// Carga dos polinomios (en dominio NTT, como en el uso real) en v0 y v1,
// dispara vpmul, verifica los 256 coeficientes contra
// kyber_ref.poly_pointwise_mul().

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vvpmul_engine_top.h"
#include "verilated.h"

#include "vpmul_engine_testvectors.inc"

static Vvpmul_engine_top* top;
static int errors = 0;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

static void write_vreg(int vreg, int coef, uint16_t data) {
    top->ext_addr_vreg = vreg;
    top->ext_addr_coef = coef;
    top->ext_wdata = data;
    top->ext_we = 1;
    tick();
    top->ext_we = 0;
}

static uint16_t read_vreg(int vreg, int coef) {
    top->ext_addr_vreg = vreg;
    top->ext_addr_coef = coef;
    top->ext_we = 0;
    top->eval();
    return top->ext_rdata;
}

static bool run_until_done(int max_cycles) {
    for (int i = 0; i < max_cycles; i++) {
        tick();
        if (top->done) return true;
    }
    return false;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vvpmul_engine_top;

    top->rst_n = 0;
    top->clk = 0;
    top->start = 0;
    top->ext_we = 0;
    tick();
    tick();
    top->rst_n = 1;

    for (int i = 0; i < 256; i++) {
        write_vreg(0, i, test_vpmul_a[i]);
        write_vreg(1, i, test_vpmul_b[i]);
    }

    top->vreg_a = 0;
    top->vreg_b = 1;
    top->vreg_dst = 2;
    top->start = 1;
    tick();
    top->start = 0;

    bool finished = run_until_done(400);  // 256 ciclos esperados + margen
    if (!finished) {
        std::printf("FAIL [vpmul_termina]: no se afirmo 'done' tras 400 ciclos\n");
        errors++;
    } else {
        std::printf("OK   [vpmul_termina]: 'done' afirmado correctamente\n");
    }

    bool result_ok = true;
    for (int i = 0; i < 256; i++) {
        uint16_t got = read_vreg(2, i);
        if (got != test_vpmul_expected[i]) {
            result_ok = false;
            std::printf("FAIL [vpmul_coef_%d]: got=%u esperado=%u\n", i, got, test_vpmul_expected[i]);
            errors++;
        }
    }
    if (result_ok) std::printf("OK   [vpmul_resultado]: 256/256 coeficientes coinciden con kyber_ref.poly_pointwise_mul()\n");

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: vpmul_engine — resultado coincide con el modelo de referencia.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
