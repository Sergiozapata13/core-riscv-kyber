// tb_forwarding_unit.cpp
//
// Testbench de la forwarding unit (Fase 2).
// Casos cubiertos:
//   1. Sin dependencia: ambos operandos toman FWD_NONE.
//   2. Forward desde EX/MEM en rs1.
//   3. Forward desde EX/MEM en rs2.
//   4. Forward desde MEM/WB en rs1 (cuando EX/MEM no aplica).
//   5. PRIORIDAD: EX/MEM y MEM/WB compiten por el mismo rs1 -> EX/MEM gana.
//   6. x0 nunca se forwardea, ni desde EX/MEM ni desde MEM/WB, aunque
//      reg_write este activo.
//   7. reg_write=0 en la fuente: no debe forwardear aunque rd coincida.
//   8. rs1 y rs2 con dependencias distintas simultaneas (una de cada fuente).

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vforwarding_unit.h"
#include "verilated.h"

static Vforwarding_unit* top;
static int errors = 0;

enum { FWD_NONE = 0, FWD_EX_MEM = 1, FWD_MEM_WB = 2 };

static void check(const char* label, uint8_t rs1, uint8_t rs2,
                   uint8_t ex_mem_rd, int ex_mem_rw,
                   uint8_t mem_wb_rd, int mem_wb_rw,
                   int expected_fwd_a, int expected_fwd_b) {
    top->id_ex_rs1       = rs1;
    top->id_ex_rs2       = rs2;
    top->ex_mem_rd       = ex_mem_rd;
    top->ex_mem_reg_write = ex_mem_rw;
    top->mem_wb_rd       = mem_wb_rd;
    top->mem_wb_reg_write = mem_wb_rw;
    top->eval();

    bool ok = ((int)top->fwd_a == expected_fwd_a) && ((int)top->fwd_b == expected_fwd_b);
    if (!ok) {
        std::printf("FAIL [%s]: fwd_a=%d (esperado %d) fwd_b=%d (esperado %d)\n",
                     label, top->fwd_a, expected_fwd_a, top->fwd_b, expected_fwd_b);
        errors++;
    } else {
        std::printf("OK   [%s]: fwd_a=%d fwd_b=%d\n", label, top->fwd_a, top->fwd_b);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vforwarding_unit;

    // ---- Caso 1: sin dependencia ----
    check("caso1_sin_dependencia",
          /*rs1=*/5, /*rs2=*/6,
          /*ex_mem_rd=*/10, /*ex_mem_rw=*/1,
          /*mem_wb_rd=*/11, /*mem_wb_rw=*/1,
          FWD_NONE, FWD_NONE);

    // ---- Caso 2: forward desde EX/MEM en rs1 ----
    check("caso2_fwd_ex_mem_rs1",
          /*rs1=*/7, /*rs2=*/6,
          /*ex_mem_rd=*/7, /*ex_mem_rw=*/1,
          /*mem_wb_rd=*/11, /*mem_wb_rw=*/1,
          FWD_EX_MEM, FWD_NONE);

    // ---- Caso 3: forward desde EX/MEM en rs2 ----
    check("caso3_fwd_ex_mem_rs2",
          /*rs1=*/5, /*rs2=*/8,
          /*ex_mem_rd=*/8, /*ex_mem_rw=*/1,
          /*mem_wb_rd=*/11, /*mem_wb_rw=*/1,
          FWD_NONE, FWD_EX_MEM);

    // ---- Caso 4: forward desde MEM/WB en rs1 (EX/MEM no aplica) ----
    check("caso4_fwd_mem_wb_rs1",
          /*rs1=*/9, /*rs2=*/6,
          /*ex_mem_rd=*/10, /*ex_mem_rw=*/1,
          /*mem_wb_rd=*/9, /*mem_wb_rw=*/1,
          FWD_MEM_WB, FWD_NONE);

    // ---- Caso 5: PRIORIDAD — ambas fuentes escriben al mismo rs1 ----
    check("caso5_prioridad_ex_mem_gana",
          /*rs1=*/4, /*rs2=*/6,
          /*ex_mem_rd=*/4, /*ex_mem_rw=*/1,
          /*mem_wb_rd=*/4, /*mem_wb_rw=*/1,
          FWD_EX_MEM, FWD_NONE);

    // ---- Caso 6a: x0 nunca se forwardea desde EX/MEM ----
    check("caso6a_x0_no_forward_ex_mem",
          /*rs1=*/0, /*rs2=*/6,
          /*ex_mem_rd=*/0, /*ex_mem_rw=*/1,
          /*mem_wb_rd=*/11, /*mem_wb_rw=*/1,
          FWD_NONE, FWD_NONE);

    // ---- Caso 6b: x0 nunca se forwardea desde MEM/WB ----
    check("caso6b_x0_no_forward_mem_wb",
          /*rs1=*/0, /*rs2=*/6,
          /*ex_mem_rd=*/10, /*ex_mem_rw=*/1,
          /*mem_wb_rd=*/0, /*mem_wb_rw=*/1,
          FWD_NONE, FWD_NONE);

    // ---- Caso 7: reg_write=0 en la fuente -> no forwardea aunque rd coincida ----
    check("caso7_reg_write_0_no_forward",
          /*rs1=*/7, /*rs2=*/6,
          /*ex_mem_rd=*/7, /*ex_mem_rw=*/0,   // rd coincide pero no escribe
          /*mem_wb_rd=*/11, /*mem_wb_rw=*/1,
          FWD_NONE, FWD_NONE);

    // ---- Caso 8: rs1 y rs2 con dependencias distintas simultaneas ----
    check("caso8_rs1_ex_mem_rs2_mem_wb",
          /*rs1=*/3, /*rs2=*/4,
          /*ex_mem_rd=*/3, /*ex_mem_rw=*/1,
          /*mem_wb_rd=*/4, /*mem_wb_rw=*/1,
          FWD_EX_MEM, FWD_MEM_WB);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: forwarding_unit — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
