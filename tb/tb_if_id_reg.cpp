// tb_if_id_reg.cpp
//
// Testbench del registro de segmentacion IF/ID (Fase 2).
// Casos cubiertos:
//   1. Reset asincrono: valid=0, instr=NOP.
//   2. Avance normal: valid_in/instr_in/pc_in pasan a la salida tras un flanco.
//   3. Stall: el contenido se mantiene identico entre flancos.
//   4. Flush: valid_out pasa a 0 e instr_out a NOP, sin importar la entrada.
//   5. Flush + stall simultaneos: flush tiene prioridad.
//   6. valid_in=0 (burbuja entrante desde IF) se propaga como NOP tambien.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vif_id_reg.h"
#include "verilated.h"

static Vif_id_reg* top;
static int errors = 0;

static const uint32_t NOP = 0x00000013;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

static void check_eq(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        std::printf("FAIL [%s]: got=0x%08x expected=0x%08x\n", label, got, expected);
        errors++;
    } else {
        std::printf("OK   [%s]: 0x%08x\n", label, got);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vif_id_reg;

    // ---- Caso 1: reset asincrono ----
    // Se fuerza una transicion real de rst_n (1->0) para que el bloque
    // always_ff con reset asincrono efectivamente dispare: en la primera
    // evaluacion del simulador no hay una transicion de posedge/negedge
    // "real" que Verilator pueda detectar, asi que se inicializa primero
    // en rst_n=1 y luego se baja.
    top->rst_n = 1;
    top->stall = 0;
    top->flush = 0;
    top->valid_in = 1;
    top->instr_in = 0xDEADBEEF;
    top->pc_in    = 0x1000;
    top->clk = 0;
    top->eval();

    top->rst_n = 0;   // transicion real 1->0: dispara el reset asincrono
    top->eval();
    check_eq("caso1_reset_valid", top->valid_out, 0);
    check_eq("caso1_reset_instr_es_nop", top->instr_out, NOP);

    top->rst_n = 1;

    // ---- Caso 2: avance normal ----
    top->stall = 0;
    top->flush = 0;
    top->valid_in = 1;
    top->instr_in = 0x12345678;
    top->pc_in    = 0x2000;
    tick();
    check_eq("caso2_valid_out", top->valid_out, 1);
    check_eq("caso2_instr_out", top->instr_out, 0x12345678);
    check_eq("caso2_pc_out",    top->pc_out,    0x2000);

    // ---- Caso 3: stall mantiene el contenido ----
    top->stall = 1;
    top->valid_in = 1;
    top->instr_in = 0xFFFFFFFF;  // valor distinto, no deberia colarse
    top->pc_in    = 0x9999;
    tick();
    check_eq("caso3_stall_mantiene_valid", top->valid_out, 1);
    check_eq("caso3_stall_mantiene_instr", top->instr_out, 0x12345678);  // el mismo de antes
    check_eq("caso3_stall_mantiene_pc",    top->pc_out,    0x2000);       // el mismo de antes

    // ---- Caso 4: flush invalida ----
    top->stall = 0;
    top->flush = 1;
    top->valid_in = 1;
    top->instr_in = 0xABCDEF01;  // no deberia importar, flush gana
    tick();
    check_eq("caso4_flush_valid", top->valid_out, 0);
    check_eq("caso4_flush_instr_es_nop", top->instr_out, NOP);

    // Restaurar un estado conocido para el siguiente caso
    top->flush = 0;
    top->stall = 0;
    top->valid_in = 1;
    top->instr_in = 0x55555555;
    top->pc_in    = 0x3000;
    tick();
    check_eq("post_caso4_restaurado", top->instr_out, 0x55555555);

    // ---- Caso 5: flush + stall simultaneos -> flush gana ----
    top->stall = 1;
    top->flush = 1;
    top->valid_in = 1;
    top->instr_in = 0x77777777;
    tick();
    check_eq("caso5_flush_gana_sobre_stall_valid", top->valid_out, 0);
    check_eq("caso5_flush_gana_sobre_stall_instr", top->instr_out, NOP);

    // ---- Caso 6: valid_in=0 desde IF (burbuja entrante) se propaga como NOP ----
    top->stall = 0;
    top->flush = 0;
    top->valid_in = 0;
    top->instr_in = 0xCAFEBABE;  // IF podria mandar basura si valid_in=0, no deberia importar
    top->pc_in    = 0x4000;
    tick();
    check_eq("caso6_valid_in_0_propaga_invalid", top->valid_out, 0);
    check_eq("caso6_valid_in_0_propaga_nop",     top->instr_out, NOP);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: if_id_reg — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
