// tb_vload_vstore_engine.cpp
//
// Testbench del motor vload/vstore completo (Fase 4).
// Flujo:
//   1. Escribir un polinomio de prueba en dmem (256 halfwords, direcciones
//      consecutivas de a 2 bytes, desde addr_base=0x100).
//   2. vload: mover ese polinomio de dmem a v0. Verificar v0 == poly.
//   3. vstore: mover v0 de vuelta a otra direccion de memoria (0x300).
//      Verificar que esa nueva zona de memoria coincide con el poly
//      original — round-trip completo memoria->vector->memoria.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vvload_vstore_engine_top.h"
#include "verilated.h"

#include "vload_vstore_testvectors.inc"

static Vvload_vstore_engine_top* top;
static int errors = 0;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

// Escribe el polinomio completo directamente como words de 32 bits
// (2 coeficientes empaquetados por word, little-endian) — coherente con
// como el motor los lee despues como halfwords individuales en
// direcciones consecutivas de a 2 bytes.
static void write_dmem_poly(uint16_t addr_base, const uint16_t* poly, int n) {
    for (int i = 0; i < n; i += 2) {
        uint32_t word = (uint32_t)poly[i] | ((uint32_t)poly[i + 1] << 16);
        top->ext_dmem_addr  = addr_base + i * 2;
        top->ext_dmem_wdata = word;
        top->ext_dmem_we    = 1;
        tick();
        top->ext_dmem_we    = 0;
    }
}

static uint16_t read_dmem_halfword(uint16_t addr) {
    // Lectura de word alineada, extraer la mitad baja o alta segun offset.
    uint16_t word_addr = addr & ~0x3;
    top->ext_dmem_addr = word_addr;
    top->ext_dmem_we   = 0;
    top->eval();
    uint32_t word = top->ext_dmem_rdata;
    if ((addr & 0x2) == 0) return (uint16_t)(word & 0xFFFF);
    else                    return (uint16_t)((word >> 16) & 0xFFFF);
}

static uint16_t read_vreg(int vreg, int coef) {
    top->ext_vreg_addr_vreg = vreg;
    top->ext_vreg_addr_coef = coef;
    top->ext_vreg_we        = 0;
    top->eval();
    return top->ext_vreg_rdata;
}

static bool run_until_done(int max_cycles) {
    for (int i = 0; i < max_cycles; i++) {
        tick();
        if (top->done) return true;
    }
    return false;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vvload_vstore_engine_top;

    top->rst_n = 0;
    top->clk = 0;
    top->start = 0;
    top->ext_dmem_we = 0;
    top->ext_vreg_we = 0;
    tick();
    tick();
    top->rst_n = 1;

    const uint16_t ADDR_LOAD  = 0x100;
    const uint16_t ADDR_STORE = 0x300;

    // ---- Preparar memoria: escribir el polinomio de prueba ----
    write_dmem_poly(ADDR_LOAD, test_vload_poly, 256);

    // ---- vload: v0 <= mem[ADDR_LOAD..] ----
    top->is_store  = 0;
    top->addr_base = ADDR_LOAD;
    top->vreg      = 0;
    top->start     = 1;
    tick();
    top->start = 0;

    bool load_done = run_until_done(400);  // 256 ciclos + margen
    if (!load_done) {
        std::printf("FAIL [vload_termina]: no se afirmo 'done' tras 400 ciclos\n");
        errors++;
    } else {
        std::printf("OK   [vload_termina]: 'done' afirmado correctamente\n");
    }

    bool load_ok = true;
    for (int i = 0; i < 256; i++) {
        uint16_t got = read_vreg(0, i);
        if (got != test_vload_poly[i]) {
            load_ok = false;
            std::printf("FAIL [vload_coef_%d]: got=%u esperado=%u\n", i, got, test_vload_poly[i]);
            errors++;
        }
    }
    if (load_ok) std::printf("OK   [vload_resultado]: 256/256 coeficientes coinciden con la memoria de origen\n");

    // ---- vstore: mem[ADDR_STORE..] <= v0 ----
    top->is_store  = 1;
    top->addr_base = ADDR_STORE;
    top->vreg      = 0;
    top->start     = 1;
    tick();
    top->start = 0;

    bool store_done = run_until_done(400);
    if (!store_done) {
        std::printf("FAIL [vstore_termina]: no se afirmo 'done' tras 400 ciclos\n");
        errors++;
    } else {
        std::printf("OK   [vstore_termina]: 'done' afirmado correctamente\n");
    }

    bool store_ok = true;
    for (int i = 0; i < 256; i++) {
        uint16_t got = read_dmem_halfword(ADDR_STORE + i * 2);
        if (got != test_vload_poly[i]) {
            store_ok = false;
            std::printf("FAIL [vstore_coef_%d]: got=%u esperado=%u\n", i, got, test_vload_poly[i]);
            errors++;
        }
    }
    if (store_ok) std::printf("OK   [vstore_resultado]: 256/256 coeficientes coinciden — round-trip memoria->vector->memoria correcto\n");

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: vload_vstore_engine — round-trip completo correcto.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
