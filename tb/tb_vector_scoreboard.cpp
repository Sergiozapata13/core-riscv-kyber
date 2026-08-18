// tb_vector_scoreboard.cpp
//
// Testbench del scoreboard vectorial (Fase 4).
// Casos cubiertos:
//   1. Reset: todos los bits libres.
//   2. Set marca el bit correspondiente ocupado; los demas no se afectan.
//   3. Clear libera el bit correspondiente; los demas no se afectan.
//   4. Independencia: set/clear en registros distintos no interfieren.
//   5. Colision: set y clear apuntando al MISMO bit el mismo ciclo ->
//      set gana (el bit queda ocupado, no libre).
//   6. Sin set/clear, el estado se mantiene entre ciclos.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vvector_scoreboard.h"
#include "verilated.h"

static Vvector_scoreboard* top;
static int errors = 0;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

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
    top = new Vvector_scoreboard;

    // ---- Caso 1: reset ----
    top->rst_n = 1;
    top->set_pulse = 0;
    top->clear = 0;
    top->clk = 0;
    top->eval();
    top->rst_n = 0;   // transicion real 1->0 para disparar el reset asincrono
    top->eval();
    check_eq("caso1_reset_todos_libres", top->busy, 0b0000);

    top->rst_n = 1;

    // ---- Caso 2: set marca el bit correspondiente ----
    top->set_pulse = 1; top->set_vreg = 2;  // v2
    top->clear = 0;
    tick();
    top->set_pulse = 0;
    check_eq("caso2_set_v2", top->busy, 0b0100);

    // ---- Caso 3: clear libera el bit correspondiente ----
    top->clear = 1; top->clear_vreg = 2;
    tick();
    top->clear = 0;
    check_eq("caso3_clear_v2", top->busy, 0b0000);

    // ---- Caso 4: independencia entre registros ----
    top->set_pulse = 1; top->set_vreg = 0;  // v0
    tick();
    top->set_pulse = 1; top->set_vreg = 3;  // v3 (v0 sigue ocupado)
    tick();
    top->set_pulse = 0;
    check_eq("caso4_v0_y_v3_ocupados", top->busy, 0b1001);

    top->clear = 1; top->clear_vreg = 0;  // liberar solo v0
    tick();
    top->clear = 0;
    check_eq("caso4_solo_v3_queda_ocupado", top->busy, 0b1000);

    // Limpiar v3 para el siguiente caso
    top->clear = 1; top->clear_vreg = 3;
    tick();
    top->clear = 0;
    check_eq("caso4_limpieza", top->busy, 0b0000);

    // ---- Caso 5: colision set+clear en el MISMO bit, mismo ciclo -> set gana ----
    // Primero ocupar v1
    top->set_pulse = 1; top->set_vreg = 1;
    tick();
    top->set_pulse = 0;
    check_eq("caso5_v1_ocupado_antes", top->busy, 0b0010);

    // Ahora, en el mismo ciclo: clear v1 (libera) Y set v1 (re-ocupa) simultaneamente
    top->clear = 1; top->clear_vreg = 1;
    top->set_pulse   = 1; top->set_vreg   = 1;
    tick();
    top->clear = 0; top->set_pulse = 0;
    check_eq("caso5_colision_set_gana", top->busy, 0b0010);  // deberia seguir ocupado

    // ---- Caso 6: sin set/clear, el estado se mantiene ----
    tick();
    tick();
    check_eq("caso6_estado_estable", top->busy, 0b0010);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: vector_scoreboard — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
