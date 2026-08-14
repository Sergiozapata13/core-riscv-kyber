// tb_hazard_detection_unit.cpp
//
// Testbench de la hazard detection unit (Fase 2).
// Casos cubiertos:
//   1. Sin load en EX (mem_read=0): nunca hay hazard, sin importar rd/rs1/rs2.
//   2. Load en EX, pero rd no coincide con rs1 ni rs2: sin hazard.
//   3. Load en EX, rd coincide con rs1: hazard detectado.
//   4. Load en EX, rd coincide con rs2: hazard detectado.
//   5. Load en EX, rd coincide con AMBOS rs1 y rs2: hazard detectado (una vez).
//   6. rd=x0: nunca genera hazard, aunque "coincida" con rs1/rs2=0.
//   7. Las tres señales de salida (pc_stall, if_id_stall, id_ex_flush) se
//      activan juntas — nunca una sin las otras dos.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vhazard_detection_unit.h"
#include "verilated.h"

static Vhazard_detection_unit* top;
static int errors = 0;

static void check(const char* label, int mem_read, uint8_t rd, uint8_t rs1, uint8_t rs2,
                   int expected_hazard) {
    top->id_ex_mem_read = mem_read;
    top->id_ex_rd       = rd;
    top->id_rs1          = rs1;
    top->id_rs2          = rs2;
    top->eval();

    bool ok = ((int)top->pc_stall == expected_hazard)
            && ((int)top->if_id_stall == expected_hazard)
            && ((int)top->id_ex_flush == expected_hazard);

    if (!ok) {
        std::printf("FAIL [%s]: pc_stall=%d if_id_stall=%d id_ex_flush=%d (todos deberian ser %d)\n",
                     label, top->pc_stall, top->if_id_stall, top->id_ex_flush, expected_hazard);
        errors++;
    } else {
        std::printf("OK   [%s]: hazard=%d\n", label, top->pc_stall);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vhazard_detection_unit;

    // ---- Caso 1: sin load en EX -> nunca hay hazard ----
    check("caso1_sin_load_rd_coincide_rs1", /*mem_read=*/0, /*rd=*/5, /*rs1=*/5, /*rs2=*/6, /*hazard=*/0);
    check("caso1b_sin_load_rd_coincide_rs2", /*mem_read=*/0, /*rd=*/6, /*rs1=*/5, /*rs2=*/6, /*hazard=*/0);

    // ---- Caso 2: load en EX pero rd no coincide con nada ----
    check("caso2_load_sin_coincidencia", /*mem_read=*/1, /*rd=*/9, /*rs1=*/5, /*rs2=*/6, /*hazard=*/0);

    // ---- Caso 3: load en EX, rd coincide con rs1 ----
    check("caso3_load_coincide_rs1", /*mem_read=*/1, /*rd=*/5, /*rs1=*/5, /*rs2=*/6, /*hazard=*/1);

    // ---- Caso 4: load en EX, rd coincide con rs2 ----
    check("caso4_load_coincide_rs2", /*mem_read=*/1, /*rd=*/6, /*rs1=*/5, /*rs2=*/6, /*hazard=*/1);

    // ---- Caso 5: load en EX, rd coincide con AMBOS rs1 y rs2 ----
    check("caso5_load_coincide_ambos", /*mem_read=*/1, /*rd=*/7, /*rs1=*/7, /*rs2=*/7, /*hazard=*/1);

    // ---- Caso 6: rd=x0 nunca genera hazard ----
    check("caso6_rd_x0_no_hazard", /*mem_read=*/1, /*rd=*/0, /*rs1=*/0, /*rs2=*/6, /*hazard=*/0);

    // ---- Caso 7: instruccion tipo I sin rs2 real (rs2=x0 por convencion del
    //      decoder) no debe generar falso positivo si rd tambien es distinto de 0
    //      pero no coincide con rs1 real ----
    check("caso7_rs2_x0_no_hazard_si_rd_no_es_0_ni_coincide", /*mem_read=*/1, /*rd=*/3, /*rs1=*/5, /*rs2=*/0, /*hazard=*/0);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: hazard_detection_unit — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
