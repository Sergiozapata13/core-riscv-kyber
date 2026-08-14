// tb_vreg_file.cpp
//
// Testbench del banco de registros vectorial (Fase 4).
// Casos cubiertos:
//   1. Escritura + lectura basica en cada uno de los 4 registros (v0-v3).
//   2. Aislamiento: escribir en v0 no afecta v1/v2/v3.
//   3. Los 256 coeficientes de un registro son direccionables
//      independientemente (no hay empaquetado que los acople).
//   4. Los dos puertos de lectura simultaneos leen datos distintos
//      correctamente (de registros distintos, o del mismo).
//   5. Los dos puertos de escritura simultaneos escriben a direcciones
//      distintas correctamente en el mismo ciclo.
//   6. Colision de escritura (ambos puertos, misma direccion, mismo
//      ciclo): comportamiento determinista (puerto 2 gana).
//   7. we1=0/we2=0 no escribe nada.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vvreg_file.h"
#include "verilated.h"

static Vvreg_file* top;
static int errors = 0;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

static void check_eq(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        std::printf("FAIL [%s]: got=0x%04x expected=0x%04x\n", label, got, expected);
        errors++;
    } else {
        std::printf("OK   [%s]: 0x%04x\n", label, got);
    }
}

static void write_port1(uint8_t vreg, uint8_t coef, uint16_t data) {
    top->we1 = 1;
    top->waddr1_vreg = vreg;
    top->waddr1_coef = coef;
    top->wdata1 = data;
    top->we2 = 0;
    tick();
    top->we1 = 0;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vvreg_file;
    top->clk = 0;
    top->we1 = 0;
    top->we2 = 0;

    // ---- Caso 1: escritura + lectura basica en cada registro ----
    for (int v = 0; v < 4; v++) {
        uint16_t val = 0x1000 + v;
        write_port1(v, 5, val);
        top->raddr1_vreg = v;
        top->raddr1_coef = 5;
        top->eval();
        char label[32];
        std::snprintf(label, sizeof(label), "caso1_v%d_coef5", v);
        check_eq(label, top->rdata1, val);
    }

    // ---- Caso 2: aislamiento entre registros ----
    write_port1(0, 10, 0xAAAA);
    write_port1(1, 10, 0xBBBB);
    top->raddr1_vreg = 0; top->raddr1_coef = 10; top->eval();
    check_eq("caso2_v0_no_afectado_por_v1", top->rdata1, 0xAAAA);
    top->raddr1_vreg = 1; top->raddr1_coef = 10; top->eval();
    check_eq("caso2_v1_correcto", top->rdata1, 0xBBBB);

    // ---- Caso 3: 256 coeficientes direccionables independientemente ----
    write_port1(2, 0, 0x0001);
    write_port1(2, 1, 0x0002);
    write_port1(2, 255, 0x00FF);
    top->raddr1_vreg = 2; top->raddr1_coef = 0;   top->eval();
    check_eq("caso3_coef0", top->rdata1, 0x0001);
    top->raddr1_vreg = 2; top->raddr1_coef = 1;   top->eval();
    check_eq("caso3_coef1", top->rdata1, 0x0002);
    top->raddr1_vreg = 2; top->raddr1_coef = 255; top->eval();
    check_eq("caso3_coef255", top->rdata1, 0x00FF);

    // ---- Caso 4: dos puertos de lectura simultaneos ----
    top->raddr1_vreg = 0; top->raddr1_coef = 10;   // v0[10] = 0xAAAA
    top->raddr2_vreg = 1; top->raddr2_coef = 10;   // v1[10] = 0xBBBB
    top->eval();
    check_eq("caso4_puerto1", top->rdata1, 0xAAAA);
    check_eq("caso4_puerto2", top->rdata2, 0xBBBB);

    // ---- Caso 5: dos puertos de escritura simultaneos, direcciones distintas ----
    top->we1 = 1; top->waddr1_vreg = 3; top->waddr1_coef = 20; top->wdata1 = 0x1111;
    top->we2 = 1; top->waddr2_vreg = 3; top->waddr2_coef = 21; top->wdata2 = 0x2222;
    tick();
    top->we1 = 0; top->we2 = 0;
    top->raddr1_vreg = 3; top->raddr1_coef = 20; top->eval();
    check_eq("caso5_puerto1_escritura", top->rdata1, 0x1111);
    top->raddr1_vreg = 3; top->raddr1_coef = 21; top->eval();
    check_eq("caso5_puerto2_escritura", top->rdata1, 0x2222);

    // ---- Caso 6: colision de escritura, misma direccion, mismo ciclo ----
    top->we1 = 1; top->waddr1_vreg = 3; top->waddr1_coef = 30; top->wdata1 = 0xAAAA;
    top->we2 = 1; top->waddr2_vreg = 3; top->waddr2_coef = 30; top->wdata2 = 0xBBBB;
    tick();
    top->we1 = 0; top->we2 = 0;
    top->raddr1_vreg = 3; top->raddr1_coef = 30; top->eval();
    check_eq("caso6_colision_puerto2_gana", top->rdata1, 0xBBBB);

    // ---- Caso 7: we1=0/we2=0 no escribe nada ----
    write_port1(0, 50, 0x5555);
    top->we1 = 0; top->we2 = 0;
    top->waddr1_vreg = 0; top->waddr1_coef = 50; top->wdata1 = 0xFFFF;
    top->waddr2_vreg = 0; top->waddr2_coef = 50; top->wdata2 = 0xFFFF;
    tick();
    top->raddr1_vreg = 0; top->raddr1_coef = 50; top->eval();
    check_eq("caso7_we0_no_escribe", top->rdata1, 0x5555);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: vreg_file — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
