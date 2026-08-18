// tb_dmem.cpp
//
// Testbench de la memoria de datos (Fase 1).
// Casos cubiertos:
//   1. sw + lw basico (word completo)
//   2. sb en cada uno de los 4 offsets de byte dentro de una palabra,
//      confirmando que los bytes vecinos NO se modifican.
//   3. lb / lbu: mismo byte almacenado, distinta extension (signed/unsigned)
//   4. sh + lh / lhu: halfword en offset 0 y offset 2, con signo y sin signo
//   5. mem_read=0 debe devolver 0 (no una lectura real)
//   6. mem_write=0 no debe modificar memoria

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vdmem.h"
#include "verilated.h"

static Vdmem* top;
static int errors = 0;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

static void do_write(uint32_t addr, uint32_t wdata, int funct3_write_code) {
    top->addr      = addr;
    top->wdata     = wdata;
    top->mem_write = 1;
    top->mem_read  = 0;
    top->funct3    = funct3_write_code; // solo funct3[1:0] importa para store
    tick();
    top->mem_write = 0;
}

static uint32_t do_read(uint32_t addr, int funct3_read_code) {
    top->addr     = addr;
    top->mem_read = 1;
    top->funct3   = funct3_read_code;
    top->eval();
    uint32_t r = top->rdata;
    top->mem_read = 0;
    return r;
}

static void check(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        std::printf("FAIL [%s]: got=0x%08x esperado=0x%08x\n", label, got, expected);
        errors++;
    } else {
        std::printf("OK   [%s]: 0x%08x\n", label, got);
    }
}

// Codigos funct3 (deben coincidir con la logica de dmem.sv)
enum { F3_LB=0b000, F3_LH=0b001, F3_LW=0b010, F3_LBU=0b100, F3_LHU=0b101,
       F3_SB=0b00,  F3_SH=0b01,  F3_SW=0b10 };

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vdmem;
    top->clk = 0;
    // Puerto 2 inerte por defecto — no debe interferir con los casos del
    // puerto 1 (Fase 1) mientras no se lo ejercite explicitamente.
    top->addr2 = 0;
    top->wdata2 = 0;
    top->mem_read2 = 0;
    top->mem_write2 = 0;
    top->funct3_2 = 0;

    // ---- Caso 1: sw + lw basico ----
    do_write(0x00, 0xDEADBEEF, F3_SW);
    check("caso1_sw_lw", do_read(0x00, F3_LW), 0xDEADBEEF);

    // ---- Caso 2: sb en cada offset, sin pisar bytes vecinos ----
    // Partimos de una palabra conocida en 0x04 y sobreescribimos un byte a la vez.
    do_write(0x04, 0x11223344, F3_SW);
    do_write(0x04, 0xAA, F3_SB);         // offset 0 -> byte bajo
    check("caso2_sb_offset0", do_read(0x04, F3_LW), 0x112233AA);

    do_write(0x08, 0x11223344, F3_SW);
    do_write(0x09, 0xBB, F3_SB);         // offset 1
    check("caso2_sb_offset1", do_read(0x08, F3_LW), 0x1122BB44);

    do_write(0x0C, 0x11223344, F3_SW);
    do_write(0x0E, 0xCC, F3_SB);         // offset 2
    check("caso2_sb_offset2", do_read(0x0C, F3_LW), 0x11CC3344);

    do_write(0x10, 0x11223344, F3_SW);
    do_write(0x13, 0xDD, F3_SB);         // offset 3 -> byte alto
    check("caso2_sb_offset3", do_read(0x10, F3_LW), 0xDD223344);

    // ---- Caso 3: lb (signed) vs lbu (unsigned) sobre el mismo byte 0x80 ----
    do_write(0x14, 0x00000080, F3_SW);   // byte bajo = 0x80 (bit de signo=1)
    check("caso3_lb_signo_negativo", do_read(0x14, F3_LB), 0xFFFFFF80);
    check("caso3_lbu_sin_signo",     do_read(0x14, F3_LBU), 0x00000080);

    // ---- Caso 4: sh/lh/lhu en offset 0 y offset 2 ----
    do_write(0x18, 0xFFFF0000, F3_SW);
    do_write(0x18, 0x8000, F3_SH);       // offset 0: halfword bajo = 0x8000
    check("caso4_lh_offset0_signed",   do_read(0x18, F3_LH), 0xFFFF8000);
    check("caso4_lhu_offset0_unsigned",do_read(0x18, F3_LHU), 0x00008000);

    do_write(0x1C, 0x00000000, F3_SW);
    do_write(0x1E, 0x8000, F3_SH);       // offset 2: halfword alto
    check("caso4_lh_offset2_signed",   do_read(0x1E, F3_LH), 0xFFFF8000);
    check("caso4_lhu_offset2_unsigned",do_read(0x1E, F3_LHU), 0x00008000);
    check("caso4_verificar_palabra_completa", do_read(0x1C, F3_LW), 0x80000000);

    // ---- Caso 5: mem_read=0 debe devolver 0 ----
    do_write(0x20, 0xCAFECAFE, F3_SW);
    top->addr     = 0x20;
    top->mem_read = 0;
    top->eval();
    check("caso5_mem_read0_da_cero", top->rdata, 0x00000000);

    // ---- Caso 6: mem_write=0 no debe modificar memoria ----
    do_write(0x24, 0x12345678, F3_SW);
    uint32_t before = do_read(0x24, F3_LW);
    top->addr      = 0x24;
    top->wdata     = 0xFFFFFFFF;
    top->mem_write = 0;
    tick();
    check("caso6_mem_write0_no_modifica", do_read(0x24, F3_LW), before);

    // ---- Caso 7: puerto 2 escribe/lee independientemente del puerto 1 ----
    // (Fase 4: vector_unit necesita su propio acceso, sin interferir con
    // el puerto que usa el pipeline escalar)
    top->addr2      = 0x40;
    top->wdata2     = 0xCAFEBABE;
    top->mem_write2 = 1;
    top->mem_read2  = 0;
    top->funct3_2   = F3_SW;
    tick();
    top->mem_write2 = 0;

    top->addr2     = 0x40;
    top->mem_read2 = 1;
    top->funct3_2  = F3_LW;
    top->eval();
    check("caso7_puerto2_sw_lw", top->rdata2, 0xCAFEBABE);
    top->mem_read2 = 0;

    // Confirmar que el puerto 1 no vio nada de esto (direcciones distintas,
    // pero tambien confirmamos que mem_read/mem_write del puerto 1 en 0
    // durante la escritura del puerto 2 no producen efecto cruzado)
    check("caso7_puerto1_no_afectado", do_read(0x24, F3_LW), before);

    // ---- Caso 8: escritura simultanea de ambos puertos a la MISMA
    //      palabra, mismo ciclo -> puerto 2 gana (misma convencion que
    //      vreg_file we1/we2) ----
    top->addr      = 0x44; top->wdata  = 0x11111111;
    top->mem_write = 1;    top->funct3 = F3_SW;
    top->addr2     = 0x44; top->wdata2 = 0x22222222;
    top->mem_write2 = 1;   top->funct3_2 = F3_SW;
    tick();
    top->mem_write = 0; top->mem_write2 = 0;
    check("caso8_colision_puerto2_gana", do_read(0x44, F3_LW), 0x22222222);

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: dmem — todos los casos correctos.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
