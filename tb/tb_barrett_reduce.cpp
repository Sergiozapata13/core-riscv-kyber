// tb_barrett_reduce.cpp
//
// Testbench de la unidad de reduccion modular Barrett (Fase 4).
// Los 414 casos vienen de models/gen_barrett_testvectors.py, generados
// directamente contra kyber_ref.barrett_reduce() (el oraculo validado en
// dos capas durante la Fase 3, incluyendo verificacion cruzada contra
// kyber-py). Esto asegura que el RTL implementa exactamente la misma
// semantica que el modelo de referencia, no una reinterpretacion.
//
// Cubre casos de borde explicitos, y los dos rangos de entrada reales
// del proyecto (producto de coeficientes, y zeta*(a-b) del butterfly),
// cuyo rango de remainder crudo fue verificado exhaustivamente en (-q,q)
// antes de fijar el diseño de "una sola correccion" en el RTL.

#include <memory>
#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "Vbarrett_reduce.h"
#include "verilated.h"

static Vbarrett_reduce* top;
static int errors = 0;
static int n_checks = 0;

static void check(int64_t a, int64_t expected) {
    n_checks++;
    top->a = (int32_t)a;
    top->eval();

    if ((int64_t)top->result != expected) {
        std::printf("FAIL: barrett_reduce(%lld) = %u, esperado %lld\n",
                     (long long)a, top->result, (long long)expected);
        errors++;
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vbarrett_reduce;

#include "barrett_testvectors.inc"

    delete top;

    if (errors == 0) {
        std::printf("PASS: barrett_reduce — %d/%d casos correctos (vs modelo de referencia Python).\n",
                     n_checks, n_checks);
        return 0;
    } else {
        std::printf("FAIL: %d/%d error(es) detectado(s).\n", errors, n_checks);
        return 1;
    }
}
