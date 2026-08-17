// tb_base_case_mul.cpp
//
// Testbench de la multiplicacion de base case (Fase 4, dentro de vpmul).
// Los 605 casos vienen de models/gen_base_case_mul_testvectors.py,
// generados llamando directamente a kyber_ref._base_case_multiply() —
// el mismo oraculo ya usado dentro de poly_pointwise_mul() y validado en
// la Fase 3.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vbase_case_mul.h"
#include "verilated.h"

static Vbase_case_mul* top;
static int errors = 0;
static int n_checks = 0;

static void check(int a0, int a1, int b0, int b1, int zeta, int expected_c0, int expected_c1) {
    n_checks++;
    top->a0   = a0;
    top->a1   = a1;
    top->b0   = b0;
    top->b1   = b1;
    top->zeta = zeta;
    top->eval();

    bool ok = ((int)top->c0 == expected_c0) && ((int)top->c1 == expected_c1);
    if (!ok) {
        std::printf("FAIL: base_case_mul(a0=%d,a1=%d,b0=%d,b1=%d,zeta=%d) = (c0=%u,c1=%u), esperado (c0=%d,c1=%d)\n",
                     a0, a1, b0, b1, zeta, top->c0, top->c1, expected_c0, expected_c1);
        errors++;
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vbase_case_mul;

#include "base_case_mul_testvectors.inc"

    delete top;

    if (errors == 0) {
        std::printf("PASS: base_case_mul — %d/%d casos correctos (vs modelo de referencia).\n", n_checks, n_checks);
        return 0;
    } else {
        std::printf("FAIL: %d/%d error(es) detectado(s).\n", errors, n_checks);
        return 1;
    }
}
