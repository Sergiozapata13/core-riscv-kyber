// tb_vector_unit.cpp
//
// Testbench de la unidad vectorial completa (Fase 4, integracion final).
// Ejercita las 8 instrucciones sobre UNA SOLA instancia de vector_unit
// (con su vreg_file y twiddle_rom compartidos), en la secuencia real de
// uso de Kyber: vload -> vntt -> vpmul -> vintt -> vstore, mas vadd/vsub
// y vbarrett de forma independiente. Cada resultado se verifica contra
// kyber_ref.py.
//
// Esta es la prueba mas exigente hasta ahora: confirma no solo que cada
// motor funciona aislado (ya verificado en tb_*_engine.cpp), sino que el
// MULTIPLEXADO de recursos compartidos (vreg_file, twiddle_rom, dmem)
// entre los 5 motores no introduce interferencia cuando se encadenan
// operaciones distintas una tras otra.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vvector_unit.h"
#include "verilated.h"

#include "vector_unit_testvectors.inc"

// Códigos de funct3 (ver isa_vectorial_kyber.docx seccion 3)
enum {
    OP_VLOAD = 0, OP_VSTORE = 1, OP_VNTT = 2, OP_VINTT = 3,
    OP_VPMUL = 4, OP_VBARRETT = 5, OP_VADD = 6, OP_VSUB = 7
};

static Vvector_unit* top;
static int errors = 0;

// dmem propia del testbench, para poder verificar vload/vstore end-to-end
// (vector_unit expone la interfaz dmem hacia afuera, no la posee).
static uint32_t test_dmem[16384];  // 64KB / 4 bytes

static void serve_dmem_read() {
    // Replica el comportamiento de dmem.sv real para lhu (funct3=101):
    // selecciona el halfword bajo o alto segun addr[1], zero-extendido a
    // 32 bits — NO basta con exponer la palabra completa como dmem_rdata,
    // el motor (vload_vstore_engine.sv) espera dmem_rdata[15:0] ya con el
    // halfword correcto seleccionado (asi es como se comporta dmem.sv
    // real, verificado en la Fase 1 y en tb_vload_vstore_engine.cpp).
    uint16_t addr = top->dmem_addr;
    uint16_t word_idx = addr >> 2;
    uint32_t word = test_dmem[word_idx];
    uint16_t hword = (addr & 0x2) ? (uint16_t)((word >> 16) & 0xFFFF)
                                   : (uint16_t)(word & 0xFFFF);
    top->dmem_rdata = hword;  // zero-extendido a 32 bits (lhu)
}

static void tick() {
    top->clk = 0;
    serve_dmem_read();
    top->eval();
    // Servir dmem: si el motor pide escritura, resolverla contra la
    // memoria del testbench (comportamiento equivalente a dmem.sv, solo
    // el camino de escritura halfword que usa vstore).
    if (top->dmem_mem_write) {
        uint16_t addr = top->dmem_addr;
        uint16_t word_idx = addr >> 2;
        uint8_t byte_off = addr & 0x3;
        uint32_t word = test_dmem[word_idx];
        uint16_t hword = (uint16_t)(top->dmem_wdata & 0xFFFF);
        if (byte_off == 0) word = (word & 0xFFFF0000) | hword;
        else                word = (word & 0x0000FFFF) | ((uint32_t)hword << 16);
        test_dmem[word_idx] = word;
    }
    top->clk = 1;
    serve_dmem_read();
    top->eval();
}

static void load_poly_into_dmem(uint16_t addr_base, const uint16_t* poly) {
    for (int i = 0; i < 256; i += 2) {
        uint32_t word = (uint32_t)poly[i] | ((uint32_t)poly[i + 1] << 16);
        test_dmem[(addr_base + i * 2) >> 2] = word;
    }
}

static bool run_until_done(int max_cycles) {
    for (int i = 0; i < max_cycles; i++) {
        tick();
        if (top->done) return true;
    }
    return false;
}

static bool do_vload(uint16_t addr, int vreg_dst) {
    top->funct3 = OP_VLOAD;
    top->vreg_rd = vreg_dst;
    top->scalar_addr = addr;
    top->start = 1;
    tick();
    top->start = 0;
    return run_until_done(400);
}

static bool do_vstore(uint16_t addr, int vreg_src) {
    top->funct3 = OP_VSTORE;
    top->vreg_rs1 = vreg_src;
    top->scalar_addr = addr;
    top->start = 1;
    tick();
    top->start = 0;
    return run_until_done(400);
}

static bool do_ntt(int mode_intt, int vreg_src, int vreg_dst) {
    top->funct3 = mode_intt ? OP_VINTT : OP_VNTT;
    top->vreg_rs1 = vreg_src;
    top->vreg_rd = vreg_dst;
    top->start = 1;
    tick();
    top->start = 0;
    return run_until_done(1700);
}

static bool do_vpmul(int vreg_a, int vreg_b, int vreg_dst) {
    top->funct3 = OP_VPMUL;
    top->vreg_rs1 = vreg_a;
    top->vreg_rs2 = vreg_b;
    top->vreg_rd = vreg_dst;
    top->start = 1;
    tick();
    top->start = 0;
    return run_until_done(400);
}

static bool do_addsub(int is_sub, int vreg_a, int vreg_b, int vreg_dst) {
    top->funct3 = is_sub ? OP_VSUB : OP_VADD;
    top->vreg_rs1 = vreg_a;
    top->vreg_rs2 = vreg_b;
    top->vreg_rd = vreg_dst;
    top->start = 1;
    tick();
    top->start = 0;
    return run_until_done(400);
}

static bool do_barrett(int vreg_src, int vreg_dst) {
    top->funct3 = OP_VBARRETT;
    top->vreg_rs1 = vreg_src;
    top->vreg_rd = vreg_dst;
    top->start = 1;
    tick();
    top->start = 0;
    return run_until_done(400);
}

static uint16_t read_dmem_halfword(uint16_t addr) {
    uint16_t word_idx = addr >> 2;
    uint8_t byte_off = addr & 0x3;
    uint32_t word = test_dmem[word_idx];
    return (byte_off == 0) ? (uint16_t)(word & 0xFFFF) : (uint16_t)((word >> 16) & 0xFFFF);
}

static bool check_poly_in_dmem(const char* label, uint16_t addr_base, const uint16_t* expected) {
    bool ok = true;
    for (int i = 0; i < 256; i++) {
        uint16_t got = read_dmem_halfword(addr_base + i * 2);
        if (got != expected[i]) {
            ok = false;
            std::printf("FAIL [%s_coef_%d]: got=%u esperado=%u\n", label, i, got, expected[i]);
            errors++;
        }
    }
    if (ok) std::printf("OK   [%s]: 256/256 coeficientes correctos\n", label);
    return ok;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vvector_unit;

    top->rst_n = 0;
    top->clk = 0;
    top->start = 0;
    tick();
    tick();
    top->rst_n = 1;

    const uint16_t ADDR_A = 0x000, ADDR_B = 0x200;
    const uint16_t ADDR_ADD_OUT = 0x400, ADDR_SUB_OUT = 0x600;
    const uint16_t ADDR_VINTT_OUT = 0x800, ADDR_BARRETT_RAW = 0xA00, ADDR_BARRETT_OUT = 0xC00;

    load_poly_into_dmem(ADDR_A, tv_poly_a);
    load_poly_into_dmem(ADDR_B, tv_poly_b);
    load_poly_into_dmem(ADDR_BARRETT_RAW, tv_barrett_raw);

    // ---- vload: v0 <= a, v1 <= b ----
    if (!do_vload(ADDR_A, 0)) { std::printf("FAIL [vload_a_termina]\n"); errors++; }
    else std::printf("OK   [vload_a_termina]\n");
    if (!do_vload(ADDR_B, 1)) { std::printf("FAIL [vload_b_termina]\n"); errors++; }
    else std::printf("OK   [vload_b_termina]\n");

    // ---- vntt: v2 <= NTT(v0), v3 <= NTT(v1) ----
    if (!do_ntt(0, 0, 2)) { std::printf("FAIL [vntt_a_termina]\n"); errors++; }
    else std::printf("OK   [vntt_a_termina]\n");
    if (!do_ntt(0, 1, 3)) { std::printf("FAIL [vntt_b_termina]\n"); errors++; }
    else std::printf("OK   [vntt_b_termina]\n");

    // Verificar NTT(a) via vstore + lectura de memoria
    do_vstore(0xE00, 2);
    check_poly_in_dmem("vntt_a", 0xE00, tv_ntt_a_expected);

    // ---- vpmul: v0 <= NTT(a) . NTT(b) ----
    if (!do_vpmul(2, 3, 0)) { std::printf("FAIL [vpmul_termina]\n"); errors++; }
    else std::printf("OK   [vpmul_termina]\n");
    do_vstore(0x1000, 0);
    check_poly_in_dmem("vpmul", 0x1000, tv_vpmul_expected);

    // ---- vintt: v1 <= INTT(vpmul_result) ----
    if (!do_ntt(1, 0, 1)) { std::printf("FAIL [vintt_termina]\n"); errors++; }
    else std::printf("OK   [vintt_termina]\n");
    do_vstore(ADDR_VINTT_OUT, 1);
    check_poly_in_dmem("vintt", ADDR_VINTT_OUT, tv_vintt_expected);

    // ---- vadd / vsub: recargar a y b limpios (v1 fue sobreescrito por vintt) ----
    if (!do_vload(ADDR_A, 0)) { errors++; }
    if (!do_vload(ADDR_B, 1)) { errors++; }

    if (!do_addsub(0, 0, 1, 2)) { std::printf("FAIL [vadd_termina]\n"); errors++; }
    else std::printf("OK   [vadd_termina]\n");
    do_vstore(ADDR_ADD_OUT, 2);
    check_poly_in_dmem("vadd", ADDR_ADD_OUT, tv_vadd_expected);

    if (!do_addsub(1, 0, 1, 3)) { std::printf("FAIL [vsub_termina]\n"); errors++; }
    else std::printf("OK   [vsub_termina]\n");
    do_vstore(ADDR_SUB_OUT, 3);
    check_poly_in_dmem("vsub", ADDR_SUB_OUT, tv_vsub_expected);

    // ---- vbarrett ----
    if (!do_vload(ADDR_BARRETT_RAW, 0)) { errors++; }
    if (!do_barrett(0, 1)) { std::printf("FAIL [vbarrett_termina]\n"); errors++; }
    else std::printf("OK   [vbarrett_termina]\n");
    do_vstore(ADDR_BARRETT_OUT, 1);
    check_poly_in_dmem("vbarrett", ADDR_BARRETT_OUT, tv_barrett_expected);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: vector_unit — las 8 instrucciones encadenadas coinciden con el modelo de referencia.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
