// tb_constant_time.cpp
//
// Verificacion de constant-time a nivel de ciclos — Fase 5, cierre del
// pendiente explicito de la Fase 4 ("Verificacion de constant-time: el
// numero de ciclos de cada instruccion custom es independiente del
// valor de los datos de entrada (no solo de su tamano)").
//
// Reusa el mismo patron de instanciacion directa que tb_vector_unit.cpp
// (una sola instancia de vector_unit.sv, sirviendo su propia dmem de
// testbench). Para cada instruccion de computo (vntt, vintt, vpmul,
// vadd, vsub, vbarrett), mide el numero EXACTO de ciclos entre
// start=1 y done=1 con TRES conjuntos de datos deliberadamente muy
// distintos:
//   - ZERO:   los 256 coeficientes en 0
//   - MAX:    los 256 coeficientes en el valor mas alto valido (q-1
//             para operandos en dominio [0,q); 0xFFFF crudo para el
//             input de vbarrett, que opera sobre datos "sin reducir")
//   - RANDOM: los mismos vectores de prueba ya usados en
//             tb_vector_unit.cpp (datos realistas de Kyber)
//
// Si el hardware respeta el diseño constant-time (isa_vectorial_kyber.
// docx seccion 4.2: "el hardware siempre ejecuta la misma cantidad de
// trabajo... y solo el resultado final cambia segun el dato — no el
// numero de ciclos ni el camino tomado"), los tres conteos de ciclos
// deben ser IDENTICOS para cada instruccion. No se verifica
// correctitud del resultado aca (ya establecida en tb_vector_unit.cpp)
// — el unico interes de este testbench es el CONTEO DE CICLOS.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vvector_unit.h"
#include "verilated.h"

#include "vector_unit_testvectors.inc"

enum {
    OP_VLOAD = 0, OP_VSTORE = 1, OP_VNTT = 2, OP_VINTT = 3,
    OP_VPMUL = 4, OP_VBARRETT = 5, OP_VADD = 6, OP_VSUB = 7
};

static Vvector_unit* top;
static int errors = 0;

static uint32_t test_dmem[16384];

static void serve_dmem_read() {
    uint16_t addr = top->dmem_addr;
    uint16_t word_idx = addr >> 2;
    uint32_t word = test_dmem[word_idx];
    uint16_t hword = (addr & 0x2) ? (uint16_t)((word >> 16) & 0xFFFF)
                                   : (uint16_t)(word & 0xFFFF);
    top->dmem_rdata = hword;
}

static void tick() {
    top->clk = 0;
    serve_dmem_read();
    top->eval();
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

static void load_constant_into_dmem(uint16_t addr_base, uint16_t value) {
    uint32_t word = (uint32_t)value | ((uint32_t)value << 16);
    for (int i = 0; i < 256; i += 2) {
        test_dmem[(addr_base + i * 2) >> 2] = word;
    }
}

// Corre hasta done, devuelve el numero de ciclos consumidos (o -1 si
// nunca termino dentro del limite).
static int run_and_count(int max_cycles) {
    for (int i = 0; i < max_cycles; i++) {
        tick();
        if (top->done) return i + 1;
    }
    return -1;
}

static int do_vload(uint16_t addr, int vreg_dst) {
    top->funct3 = OP_VLOAD;
    top->vreg_rd = vreg_dst;
    top->scalar_addr = addr;
    top->start = 1;
    tick();
    top->start = 0;
    return run_and_count(400);
}

static int do_ntt(int mode_intt, int vreg_src, int vreg_dst) {
    top->funct3 = mode_intt ? OP_VINTT : OP_VNTT;
    top->vreg_rs1 = vreg_src;
    top->vreg_rd = vreg_dst;
    top->start = 1;
    tick();
    top->start = 0;
    return run_and_count(1700);
}

static int do_vpmul(int vreg_a, int vreg_b, int vreg_dst) {
    top->funct3 = OP_VPMUL;
    top->vreg_rs1 = vreg_a;
    top->vreg_rs2 = vreg_b;
    top->vreg_rd = vreg_dst;
    top->start = 1;
    tick();
    top->start = 0;
    return run_and_count(400);
}

static int do_addsub(int is_sub, int vreg_a, int vreg_b, int vreg_dst) {
    top->funct3 = is_sub ? OP_VSUB : OP_VADD;
    top->vreg_rs1 = vreg_a;
    top->vreg_rs2 = vreg_b;
    top->vreg_rd = vreg_dst;
    top->start = 1;
    tick();
    top->start = 0;
    return run_and_count(400);
}

static int do_barrett(int vreg_src, int vreg_dst) {
    top->funct3 = OP_VBARRETT;
    top->vreg_rs1 = vreg_src;
    top->vreg_rd = vreg_dst;
    top->start = 1;
    tick();
    top->start = 0;
    return run_and_count(400);
}

// Verifica que los 3 conteos de ciclos (zero, max, random) coincidan
// exactamente. Reporta los 3 valores siempre, para que un eventual
// FAIL muestre de inmediato la magnitud de la discrepancia.
static void check_constant(const char* label, int c_zero, int c_max, int c_random) {
    bool ok = (c_zero == c_max) && (c_max == c_random);
    if (ok) {
        std::printf("OK   [%s]: %d ciclos en los 3 casos (zero/max/random)\n", label, c_zero);
    } else {
        std::printf("FAIL [%s]: ciclos NO constantes — zero=%d max=%d random=%d\n",
                    label, c_zero, c_max, c_random);
        errors++;
    }
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

    const uint16_t ADDR_ZERO = 0x000, ADDR_MAX = 0x200, ADDR_RANDOM_A = 0x400, ADDR_RANDOM_B = 0x600;
    const uint16_t Q_MINUS_1 = 3328;  // valor mas alto valido en dominio [0,q)

    load_constant_into_dmem(ADDR_ZERO, 0);
    load_constant_into_dmem(ADDR_MAX, Q_MINUS_1);
    load_poly_into_dmem(ADDR_RANDOM_A, tv_poly_a);
    load_poly_into_dmem(ADDR_RANDOM_B, tv_poly_b);

    // Patron uniforme para evitar cualquier aliasing fuente/destino:
    // v0 (y v1 para operaciones de 2 operandos) siempre es la FUENTE,
    // recargada antes de cada corrida; v3 siempre es el DESTINO, nunca
    // coincide con una fuente.

    // ---- vntt (1 operando: v0 -> v3) ----
    do_vload(ADDR_ZERO, 0);
    int ntt_zero = do_ntt(0, 0, 3);
    do_vload(ADDR_MAX, 0);
    int ntt_max = do_ntt(0, 0, 3);
    do_vload(ADDR_RANDOM_A, 0);
    int ntt_random = do_ntt(0, 0, 3);
    check_constant("vntt", ntt_zero, ntt_max, ntt_random);

    // ---- vintt (1 operando: v0 -> v3) ----
    do_vload(ADDR_ZERO, 0);
    int intt_zero = do_ntt(1, 0, 3);
    do_vload(ADDR_MAX, 0);
    int intt_max = do_ntt(1, 0, 3);
    do_vload(ADDR_RANDOM_A, 0);
    int intt_random = do_ntt(1, 0, 3);
    check_constant("vintt", intt_zero, intt_max, intt_random);

    // ---- vpmul (2 operandos: v0,v1 -> v3) ----
    do_vload(ADDR_ZERO, 0); do_vload(ADDR_ZERO, 1);
    int pmul_zero = do_vpmul(0, 1, 3);
    do_vload(ADDR_MAX, 0); do_vload(ADDR_MAX, 1);
    int pmul_max = do_vpmul(0, 1, 3);
    do_vload(ADDR_RANDOM_A, 0); do_vload(ADDR_RANDOM_B, 1);
    int pmul_random = do_vpmul(0, 1, 3);
    check_constant("vpmul", pmul_zero, pmul_max, pmul_random);

    // ---- vadd (2 operandos: v0,v1 -> v3) ----
    do_vload(ADDR_ZERO, 0); do_vload(ADDR_ZERO, 1);
    int add_zero = do_addsub(0, 0, 1, 3);
    do_vload(ADDR_MAX, 0); do_vload(ADDR_MAX, 1);
    int add_max = do_addsub(0, 0, 1, 3);
    do_vload(ADDR_RANDOM_A, 0); do_vload(ADDR_RANDOM_B, 1);
    int add_random = do_addsub(0, 0, 1, 3);
    check_constant("vadd", add_zero, add_max, add_random);

    // ---- vsub (2 operandos: v0,v1 -> v3) ----
    do_vload(ADDR_ZERO, 0); do_vload(ADDR_ZERO, 1);
    int sub_zero = do_addsub(1, 0, 1, 3);
    do_vload(ADDR_MAX, 0); do_vload(ADDR_MAX, 1);
    int sub_max = do_addsub(1, 0, 1, 3);
    do_vload(ADDR_RANDOM_A, 0); do_vload(ADDR_RANDOM_B, 1);
    int sub_random = do_addsub(1, 0, 1, 3);
    check_constant("vsub", sub_zero, sub_max, sub_random);

    // ---- vbarrett (1 operando, input "crudo": v0 -> v3) ----
    // Ademas del caso zero/random ya cubiertos, se agrega 0xFFFF (el
    // valor mas alto posible en el ancho del vreg) como caso "max",
    // ya que vbarrett opera sobre datos sin reducir, no necesariamente
    // en [0,q) como las demas instrucciones.
    load_constant_into_dmem(ADDR_MAX, 0xFFFF);
    do_vload(ADDR_ZERO, 0);
    int barrett_zero = do_barrett(0, 3);
    do_vload(ADDR_MAX, 0);
    int barrett_max = do_barrett(0, 3);
    do_vload(ADDR_RANDOM_A, 0);
    int barrett_random = do_barrett(0, 3);
    check_constant("vbarrett", barrett_zero, barrett_max, barrett_random);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: las 6 instrucciones de computo tardan un numero de ciclos\n");
        std::printf("      IDENTICO sin importar el valor de los datos de entrada —\n");
        std::printf("      confirma constant-time a nivel de ciclos (Fase 4, seccion 4.2).\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d instruccion(es) con conteo de ciclos NO constante.\n", errors);
        return 1;
    }
}
