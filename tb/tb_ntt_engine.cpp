// tb_ntt_engine.cpp
//
// Testbench del motor NTT/INTT completo (Fase 4).
// Carga un polinomio real de 256 coeficientes en v0 (via el puerto de
// acceso externo de ntt_engine_top), dispara una NTT completa (vntt),
// espera 'done', verifica los 256 coeficientes contra kyber_ref.ntt().
// Despues corre una INTT sobre el resultado (vintt) y verifica el
// round-trip completo contra el polinomio original.
//
// Este es el primer testbench que ejercita el FLUJO COMPLETO de 896
// ciclos de butterfly (7 niveles) — no solo un butterfly aislado — asi
// que tambien sirve como confirmacion de que el patron de
// direccionamiento generado por la FSM (contadores length/start_idx/j/k)
// replica correctamente el patron de bucles de kyber_ref.ntt()/intt().

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vntt_engine_top.h"
#include "verilated.h"

#include "ntt_engine_testvectors.inc"

static Vntt_engine_top* top;
static int errors = 0;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

static void write_vreg(int vreg, int coef, uint16_t data) {
    top->ext_addr_vreg = vreg;
    top->ext_addr_coef = coef;
    top->ext_wdata = data;
    top->ext_we = 1;
    tick();
    top->ext_we = 0;
}

static uint16_t read_vreg(int vreg, int coef) {
    top->ext_addr_vreg = vreg;
    top->ext_addr_coef = coef;
    top->ext_we = 0;
    top->eval();
    return top->ext_rdata;
}

// Corre el motor hasta que 'done' se afirme, con un limite de ciclos de
// seguridad para no colgar el testbench si algo sale mal.
static bool run_until_done(int max_cycles) {
    for (int i = 0; i < max_cycles; i++) {
        tick();
        if (top->done) return true;
    }
    return false;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vntt_engine_top;

    top->rst_n = 0;
    top->clk = 0;
    top->start = 0;
    top->ext_we = 0;
    tick();
    tick();
    top->rst_n = 1;

    // ---- Cargar el polinomio de prueba en v0 ----
    for (int i = 0; i < 256; i++) {
        write_vreg(0, i, test_poly_in[i]);
    }

    // Confirmar que la carga fue correcta antes de seguir (para no
    // confundir un bug de carga con un bug del motor NTT).
    bool load_ok = true;
    for (int i = 0; i < 256; i++) {
        if (read_vreg(0, i) != test_poly_in[i]) {
            load_ok = false;
            std::printf("FAIL [carga_inicial]: v0[%d] = %u, esperado %u\n", i, read_vreg(0, i), test_poly_in[i]);
            errors++;
        }
    }
    if (load_ok) std::printf("OK   [carga_inicial]: 256/256 coeficientes cargados correctamente\n");

    // ---- Disparar vntt: v1 = NTT(v0) ----
    top->mode_intt = 0;
    top->vreg_src = 0;
    top->vreg_dst = 1;
    top->start = 1;
    tick();
    top->start = 0;

    bool ntt_done = run_until_done(1300);  // 256 (copy) + 896 (butterflies) + margen
    if (!ntt_done) {
        std::printf("FAIL [vntt_termina]: no se afirmo 'done' tras 1000 ciclos\n");
        errors++;
    } else {
        std::printf("OK   [vntt_termina]: 'done' afirmado correctamente\n");
    }

    // ---- Verificar NTT(v0) contra el modelo de referencia ----
    bool ntt_ok = true;
    for (int i = 0; i < 256; i++) {
        uint16_t got = read_vreg(1, i);
        if (got != test_ntt_expected[i]) {
            ntt_ok = false;
            std::printf("FAIL [vntt_coef_%d]: got=%u esperado=%u\n", i, got, test_ntt_expected[i]);
            errors++;
        }
    }
    if (ntt_ok) std::printf("OK   [vntt_resultado]: 256/256 coeficientes coinciden con kyber_ref.ntt()\n");

    // ---- Disparar vintt: v2 = INTT(v1) ----
    top->mode_intt = 1;
    top->vreg_src = 1;
    top->vreg_dst = 2;
    top->start = 1;
    tick();
    top->start = 0;

    bool intt_done = run_until_done(1700);  // 256 (copy) + 896 (butterflies) + 256 (scale) + margen
    if (!intt_done) {
        std::printf("FAIL [vintt_termina]: no se afirmo 'done' tras 1200 ciclos\n");
        errors++;
    } else {
        std::printf("OK   [vintt_termina]: 'done' afirmado correctamente\n");
    }

    // ---- Verificar INTT(NTT(v0)) == v0 (round-trip) ----
    bool roundtrip_ok = true;
    for (int i = 0; i < 256; i++) {
        uint16_t got = read_vreg(2, i);
        if (got != test_intt_expected[i]) {
            roundtrip_ok = false;
            std::printf("FAIL [vintt_roundtrip_coef_%d]: got=%u esperado=%u\n", i, got, test_intt_expected[i]);
            errors++;
        }
    }
    if (roundtrip_ok) std::printf("OK   [vintt_roundtrip]: 256/256 coeficientes coinciden — INTT(NTT(x))==x\n");

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: ntt_engine — NTT y round-trip completos coinciden con el modelo de referencia.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
