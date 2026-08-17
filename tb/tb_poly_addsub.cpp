// tb_poly_addsub.cpp
//
// Testbench de la unidad de suma/resta (Fase 4, vadd/vsub).
// Los 612 casos (306 vadd + 306 vsub) vienen de
// models/gen_poly_addsub_testvectors.py, generados llamando directamente
// a kyber_ref.barrett_reduce() — el mismo oráculo validado en la Fase 3.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vpoly_addsub.h"
#include "verilated.h"

static Vpoly_addsub* top;
static int errors = 0;
static int n_checks = 0;

static void check(int a, int b, int is_sub, int expected) {
    n_checks++;
    top->a      = a;
    top->b      = b;
    top->is_sub = is_sub;
    top->eval();

    if ((int)top->result != expected) {
        std::printf("FAIL: poly_addsub(a=%d, b=%d, is_sub=%d) = %u, esperado %d\n",
                     a, b, is_sub, top->result, expected);
        errors++;
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vpoly_addsub;

#include "poly_addsub_testvectors.inc"

    delete top;

    if (errors == 0) {
        std::printf("PASS: poly_addsub — %d/%d casos correctos (vs modelo de referencia).\n", n_checks, n_checks);
        return 0;
    } else {
        std::printf("FAIL: %d/%d error(es) detectado(s).\n", errors, n_checks);
        return 1;
    }
}
