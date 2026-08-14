// tb_butterfly_gs.cpp
//
// Testbench de la mariposa Gentleman-Sande (Fase 4, vintt).
// Los 305 casos vienen de models/gen_butterfly_testvectors.py gs,
// generados aplicando la formula de isa_vectorial_kyber.docx seccion 6.3,
// usando los inversos modulares reales de los twiddle factors de ZETAS.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vbutterfly_gs.h"
#include "verilated.h"

static Vbutterfly_gs* top;
static int errors = 0;
static int n_checks = 0;

static void check(int a, int b, int zeta_inv, int expected_a_out, int expected_b_out) {
    n_checks++;
    top->a        = a;
    top->b        = b;
    top->zeta_inv = zeta_inv;
    top->eval();

    bool ok = ((int)top->a_out == expected_a_out) && ((int)top->b_out == expected_b_out);
    if (!ok) {
        std::printf("FAIL: butterfly_gs(a=%d, b=%d, zeta_inv=%d) = (a_out=%u, b_out=%u), esperado (a_out=%d, b_out=%d)\n",
                     a, b, zeta_inv, top->a_out, top->b_out, expected_a_out, expected_b_out);
        errors++;
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vbutterfly_gs;

#include "butterfly_gs_testvectors.inc"

    delete top;

    if (errors == 0) {
        std::printf("PASS: butterfly_gs — %d/%d casos correctos (vs modelo de referencia).\n", n_checks, n_checks);
        return 0;
    } else {
        std::printf("FAIL: %d/%d error(es) detectado(s).\n", errors, n_checks);
        return 1;
    }
}
