// tb_vector_control.cpp
//
// Testbench de la unidad de control vectorial (Fase 4).
// Verifica que cada una de las 8 instrucciones se decodifica
// correctamente: funct3, selectores vectoriales (rs1/rs2/rd[1:0]), y
// que la direccion escalar (vload/vstore) se toma de ex_rs1_fwd tal
// cual, sin modificacion.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vvector_control.h"
#include "verilated.h"

static Vvector_control* top;
static int errors = 0;

static void check_eq(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        std::printf("FAIL [%s]: got=0x%x expected=0x%x\n", label, got, expected);
        errors++;
    } else {
        std::printf("OK   [%s]: 0x%x\n", label, got);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vvector_control;

    const uint8_t OPCODE_CUSTOM0 = 0b0001011;
    const uint8_t OPCODE_ADD     = 0b0110011;  // instruccion escalar normal, para confirmar is_vector_instr=0

    top->vector_unit_busy = 0;

    // ---- Caso 1: instruccion NO vectorial -> is_vector_instr=0, vec_start=0 ----
    top->idex_valid = 1;
    top->idex_opcode = OPCODE_ADD;
    top->idex_funct3 = 0;
    top->idex_rs1 = 5;
    top->idex_rs2 = 6;
    top->idex_rd = 7;
    top->ex_rs1_fwd = 0x1000;
    top->eval();
    check_eq("caso1_no_vector_is_vector_instr", top->is_vector_instr, 0);
    check_eq("caso1_no_vector_vec_start", top->vec_start, 0);

    // ---- Caso 2: valid=0 -> is_vector_instr=0 aunque el opcode sea custom-0 ----
    top->idex_valid = 0;
    top->idex_opcode = OPCODE_CUSTOM0;
    top->idex_funct3 = 0b010;  // vntt
    top->eval();
    check_eq("caso2_burbuja_no_dispara", top->is_vector_instr, 0);
    check_eq("caso2_burbuja_vec_start_0", top->vec_start, 0);

    // ---- Caso 3: variante MEMORIA — vload (funct3=000) ----
    // rs1 = registro escalar completo (direccion), rd[1:0] = vreg destino
    top->idex_valid  = 1;
    top->idex_opcode = OPCODE_CUSTOM0;
    top->idex_funct3 = 0b000;  // vload
    top->idex_rs1    = 0b10101;  // registro escalar x21 (valor completo, no truncado)
    top->idex_rs2    = 0b00000;  // sin uso en variante memoria
    top->idex_rd     = 0b00010;  // rd[1:0]=10 -> v2, rd[4:2] reservado/ignorado
    top->ex_rs1_fwd  = 0xDEADBEEF;  // direccion escalar ya forwardeada
    top->eval();
    check_eq("caso3_vload_is_vector_instr", top->is_vector_instr, 1);
    check_eq("caso3_vload_vec_start",       top->vec_start, 1);
    check_eq("caso3_vload_funct3",          top->vec_funct3, 0b000);
    check_eq("caso3_vload_vreg_rd",         top->vec_vreg_rd, 0b10);
    check_eq("caso3_vload_scalar_addr",     top->vec_scalar_addr, 0xDEADBEEF);

    // ---- Caso 4: variante MEMORIA — vstore (funct3=001) ----
    top->idex_funct3 = 0b001;
    top->idex_rd     = 0b00011;  // rd[1:0]=11 -> v3 (origen del store)
    top->ex_rs1_fwd  = 0x12345678;
    top->eval();
    check_eq("caso4_vstore_funct3",      top->vec_funct3, 0b001);
    check_eq("caso4_vstore_vreg_rd",     top->vec_vreg_rd, 0b11);
    check_eq("caso4_vstore_scalar_addr", top->vec_scalar_addr, 0x12345678);

    // ---- Caso 5: variante COMPUTO — vntt (funct3=010), un solo operando ----
    top->idex_funct3 = 0b010;
    top->idex_rs1    = 0b00001;  // rs1[1:0]=01 -> v1 (entrada)
    top->idex_rd     = 0b00010;  // rd[1:0]=10 -> v2 (salida)
    top->eval();
    check_eq("caso5_vntt_funct3",    top->vec_funct3, 0b010);
    check_eq("caso5_vntt_vreg_rs1",  top->vec_vreg_rs1, 0b01);
    check_eq("caso5_vntt_vreg_rd",   top->vec_vreg_rd, 0b10);

    // ---- Caso 6: variante COMPUTO — vpmul (funct3=100), dos operandos ----
    top->idex_funct3 = 0b100;
    top->idex_rs1    = 0b00010;  // v2
    top->idex_rs2    = 0b00011;  // v3
    top->idex_rd     = 0b00000;  // v0
    top->eval();
    check_eq("caso6_vpmul_funct3",   top->vec_funct3, 0b100);
    check_eq("caso6_vpmul_vreg_rs1", top->vec_vreg_rs1, 0b10);
    check_eq("caso6_vpmul_vreg_rs2", top->vec_vreg_rs2, 0b11);
    check_eq("caso6_vpmul_vreg_rd",  top->vec_vreg_rd, 0b00);

    // ---- Caso 7: vector_unit_busy=1 -> vec_start se bloquea, is_vector_instr sigue 1 ----
    top->vector_unit_busy = 1;
    top->eval();
    check_eq("caso7_busy_is_vector_instr", top->is_vector_instr, 1);
    check_eq("caso7_busy_vec_start_bloqueado", top->vec_start, 0);
    top->vector_unit_busy = 0;

    // ---- Caso 8: vbarrett, vadd, vsub — confirmar funct3 pasa directo ----
    const uint8_t funct3_values[] = {0b101, 0b110, 0b111};  // vbarrett, vadd, vsub
    const char* names[] = {"vbarrett", "vadd", "vsub"};
    for (int i = 0; i < 3; i++) {
        top->idex_funct3 = funct3_values[i];
        top->eval();
        char label[32];
        std::snprintf(label, sizeof(label), "caso8_%s_funct3", names[i]);
        check_eq(label, top->vec_funct3, funct3_values[i]);
    }

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: vector_control — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
