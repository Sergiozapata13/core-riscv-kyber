// tb_addsub_engine.cpp
//
// Testbench del motor vadd/vsub completo (Fase 4).
// Carga dos polinomios en v0/v1, corre vadd (resultado en v2) y vsub
// (resultado en v3), verifica ambos contra kyber_ref.poly_add()/poly_sub().

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vaddsub_engine_top.h"
#include "verilated.h"

#include "addsub_engine_testvectors.inc"

static Vaddsub_engine_top* top;
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

static bool run_and_check(int vreg_dst, int is_sub, const uint16_t* expected, const char* label) {
    top->is_sub = is_sub;
    top->vreg_a = 0;
    top->vreg_b = 1;
    top->vreg_dst = vreg_dst;
    top->start = 1;
    tick();
    top->start = 0;

    if (!run_until_done(400)) {
        std::printf("FAIL [%s_termina]: no se afirmo 'done' tras 400 ciclos\n", label);
        errors++;
        return false;
    }
    std::printf("OK   [%s_termina]: 'done' afirmado correctamente\n", label);

    bool ok = true;
    for (int i = 0; i < 256; i++) {
        uint16_t got = read_vreg(vreg_dst, i);
        if (got != expected[i]) {
            ok = false;
            std::printf("FAIL [%s_coef_%d]: got=%u esperado=%u\n", label, i, got, expected[i]);
            errors++;
        }
    }
    if (ok) std::printf("OK   [%s_resultado]: 256/256 coeficientes correctos\n", label);
    return ok;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vaddsub_engine_top;

    top->rst_n = 0;
    top->clk = 0;
    top->start = 0;
    top->ext_we = 0;
    tick();
    tick();
    top->rst_n = 1;

    for (int i = 0; i < 256; i++) {
        write_vreg(0, i, test_addsub_a[i]);
        write_vreg(1, i, test_addsub_b[i]);
    }

    run_and_check(2, /*is_sub=*/0, test_add_expected, "vadd");
    run_and_check(3, /*is_sub=*/1, test_sub_expected, "vsub");

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: addsub_engine — vadd y vsub coinciden con el modelo de referencia.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
