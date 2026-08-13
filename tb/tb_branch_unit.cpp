// tb_branch_unit.cpp
//
// Testbench de la unidad de branch (Fase 1).
// Combinacional pura: para cada uno de los 6 branches, se prueban ambos
// casos (taken y not-taken) fijando directamente las banderas de la ALU
// que branch_unit consume (alu_zero, alu_result0) — no se instancia una
// ALU real aca, ya que branch_unit no depende de su implementacion interna,
// solo del contrato de esas dos senales.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vbranch_unit.h"
#include "verilated.h"

static Vbranch_unit* top;
static int errors = 0;

static void check(const char* label, int branch, int funct3, int alu_zero, int alu_result0,
                   int expected_taken) {
    top->branch      = branch;
    top->funct3      = funct3;
    top->alu_zero    = alu_zero;
    top->alu_result0 = alu_result0;
    top->eval();

    if ((int)top->branch_taken != expected_taken) {
        std::printf("FAIL [%s]: branch_taken=%d esperado=%d\n", label, top->branch_taken, expected_taken);
        errors++;
    } else {
        std::printf("OK   [%s]: branch_taken=%d\n", label, top->branch_taken);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vbranch_unit;

    // ---- beq (funct3=000): taken cuando zero=1 ----
    check("beq_taken_iguales",     1, 0b000, /*zero=*/1, /*r0=*/0, 1);
    check("beq_not_taken_distintos", 1, 0b000, /*zero=*/0, /*r0=*/0, 0);

    // ---- bne (funct3=001): taken cuando zero=0 ----
    check("bne_taken_distintos", 1, 0b001, /*zero=*/0, /*r0=*/0, 1);
    check("bne_not_taken_iguales", 1, 0b001, /*zero=*/1, /*r0=*/0, 0);

    // ---- blt (funct3=100): taken cuando alu_result0=1 (rs1<rs2, con signo) ----
    check("blt_taken",     1, 0b100, /*zero=*/0, /*r0=*/1, 1);
    check("blt_not_taken", 1, 0b100, /*zero=*/0, /*r0=*/0, 0);

    // ---- bge (funct3=101): taken cuando alu_result0=0 (rs1>=rs2, con signo) ----
    check("bge_taken",     1, 0b101, /*zero=*/0, /*r0=*/0, 1);
    check("bge_not_taken", 1, 0b101, /*zero=*/0, /*r0=*/1, 0);

    // ---- bltu (funct3=110): taken cuando alu_result0=1 (rs1<rs2, sin signo) ----
    check("bltu_taken",     1, 0b110, /*zero=*/0, /*r0=*/1, 1);
    check("bltu_not_taken", 1, 0b110, /*zero=*/0, /*r0=*/0, 0);

    // ---- bgeu (funct3=111): taken cuando alu_result0=0 (rs1>=rs2, sin signo) ----
    check("bgeu_taken",     1, 0b111, /*zero=*/0, /*r0=*/0, 1);
    check("bgeu_not_taken", 1, 0b111, /*zero=*/0, /*r0=*/1, 0);

    // ---- branch=0: nunca debe tomarse, sin importar el resto de senales ----
    check("no_es_branch_zero_1", 0, 0b000, /*zero=*/1, /*r0=*/0, 0);
    check("no_es_branch_r0_1",   0, 0b100, /*zero=*/0, /*r0=*/1, 0);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: branch_unit — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
