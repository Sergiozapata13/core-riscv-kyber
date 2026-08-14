// tb_butterfly_ct.cpp
//
// Testbench de la mariposa Cooley-Tukey (Fase 4, vntt).
// Los 305 casos vienen de models/gen_butterfly_testvectors.py ct,
// generados aplicando la formula de isa_vectorial_kyber.docx seccion 6.2
// sobre el mismo barrett_reduce (kyber_ref.py) validado en la Fase 3,
// incluyendo twiddle factors reales tomados de la tabla ZETAS.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vbutterfly_ct.h"
#include "verilated.h"

static Vbutterfly_ct* top;
static int errors = 0;
static int n_checks = 0;

static void check(int a, int b, int zeta, int expected_a_out, int expected_b_out) {
    n_checks++;
    top->a    = a;
    top->b    = b;
    top->zeta = zeta;
    top->eval();

    bool ok = ((int)top->a_out == expected_a_out) && ((int)top->b_out == expected_b_out);
    if (!ok) {
        std::printf("FAIL: butterfly_ct(a=%d, b=%d, zeta=%d) = (a_out=%u, b_out=%u), esperado (a_out=%d, b_out=%d)\n",
                     a, b, zeta, top->a_out, top->b_out, expected_a_out, expected_b_out);
        errors++;
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vbutterfly_ct;

#include "butterfly_ct_testvectors.inc"

    delete top;

    if (errors == 0) {
        std::printf("PASS: butterfly_ct — %d/%d casos correctos (vs modelo de referencia).\n", n_checks, n_checks);
        return 0;
    } else {
        std::printf("FAIL: %d/%d error(es) detectado(s).\n", errors, n_checks);
        return 1;
    }
}
