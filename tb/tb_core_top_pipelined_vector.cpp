// tb_core_top_pipelined_vector.cpp
//
// Testbench de integracion final (Fase 4): corre mixed_scalar_vector.hex
// sobre core_top_pipelined.sv COMPLETO (con vector_control + vector_unit
// + vector_scoreboard integrados), y verifica los dos comportamientos
// centrales de la integracion que tb_core_top_pipelined.cpp (solo
// escalar, fib.s) no puede ejercitar:
//
//   1. DESACOPLE (Apendice A.1): mientras 'vntt v1,v0' (~1152 ciclos)
//      esta en curso, el pipeline escalar sigue avanzando instrucciones
//      independientes (5x 'addi x2,x2,1' + 'sw x2,0x100(x0)') — el
//      canario debe aparecer en memoria en un numero de ciclos MUCHO
//      menor a los ~1152 que tarda vntt, no despues.
//
//   2. SERIALIZACION (Apendice A.4, recurso unico): la segunda
//      instruccion vectorial ('vbarrett v1,v1') no debe poder DESPACHAR
//      hasta que vntt haya hecho 'done' — verificado monitoreando
//      vector_unit_busy directamente (debe mostrar exactamente 2
//      flancos de subida, sin huecos prolongados entre ambos, y una
//      duracion total consistente con las dos operaciones corriendo en
//      serie). Nota: el valor final (x3=99) aparece en memoria poco
//      DESPUES de que vbarrett logra despachar, no despues de que
//      TERMINA — el desacople (A.1) aplica a toda instruccion
//      vectorial, asi que el pipeline escalar sigue avanzando en cuanto
//      vbarrett es aceptado por la unidad, sin esperar sus 256 ciclos
//      internos.
//
// Generado por sw/tests/gen_mixed_scalar_vector.py — ver ese script para
// el detalle del programa y la verificacion de los encodings contra el
// toolchain real.

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vcore_top_pipelined.h"
#include "Vcore_top_pipelined___024root.h"
#include "verilated.h"

static Vcore_top_pipelined* top;
static int errors = 0;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

static uint32_t read_dmem_word(uint32_t byte_addr) {
    // dmem.sv modela la memoria como palabras de 32 bits — acceso
    // jerarquico directo al array interno (mismo patron que
    // tb_core_top_pipelined.cpp usa para leer fib_expected).
    return top->rootp->core_top_pipelined__DOT__u_dmem__DOT__mem[byte_addr / 4];
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vcore_top_pipelined;

    top->rst_n = 0;
    top->clk = 0;
    tick();
    tick();
    top->rst_n = 1;

    const uint32_t ADDR_CANARY = 0x100;
    const uint32_t ADDR_FINAL  = 0x104;

    int cycle_canary_appears = -1;
    int cycle_final_appears  = -1;

    // Monitoreo directo de vector_unit_busy: mas preciso que inferir la
    // serializacion del timing de memoria (ver mas abajo por que).
    int busy_total_cycles = 0;
    int busy_rising_edges = 0;   // cuantas veces paso de 0->1 (deberia ser 2: vntt, luego vbarrett)
    bool busy_prev = false;
    int max_gap_between_busy = 0;   // mayor hueco consecutivo de busy=0 DESPUES del primer dispatch
    bool seen_first_busy = false;
    int current_gap = 0;

    const int MAX_CYCLES = 3000;  // ~1152 (vntt) + ~256 (vbarrett) + margen generoso

    for (int i = 0; i < MAX_CYCLES; i++) {
        tick();

        if (cycle_canary_appears < 0 && read_dmem_word(ADDR_CANARY) == 5) {
            cycle_canary_appears = i;
        }
        if (cycle_final_appears < 0 && read_dmem_word(ADDR_FINAL) == 99) {
            cycle_final_appears = i;
        }

        bool busy_now = top->rootp->core_top_pipelined__DOT__u_vector_unit__DOT__any_busy;
        if (busy_now) {
            busy_total_cycles++;
            if (!busy_prev) busy_rising_edges++;
            seen_first_busy = true;
            if (current_gap > max_gap_between_busy) max_gap_between_busy = current_gap;
            current_gap = 0;
        } else if (seen_first_busy) {
            current_gap++;
        }
        busy_prev = busy_now;
    }

    std::printf("Canario (x2=5) aparecio en memoria en el ciclo: %d\n", cycle_canary_appears);
    std::printf("Valor final (x3=99) aparecio en memoria en el ciclo: %d\n", cycle_final_appears);
    std::printf("vector_unit ocupada un total de %d ciclos, con %d flancos de subida (dispatches), hueco maximo entre operaciones: %d ciclos\n",
                 busy_total_cycles, busy_rising_edges, max_gap_between_busy);

    // ---- Verificacion 1: desacople — el canario debe aparecer RAPIDO ----
    // Con el pipeline de 5 etapas, 7 instrucciones escalares (incluyendo
    // el sw final) deberian completarse en el orden de ~15-20 ciclos,
    // MUY por debajo de los ~1152 ciclos que tarda vntt. Se usa un limite
    // generoso (100 ciclos) para no acoplar el test a un conteo exacto
    // de ciclos del pipeline, solo confirmar el ORDEN DE MAGNITUD
    // esperado (decenas, no miles).
    if (cycle_canary_appears < 0) {
        std::printf("FAIL [desacople_canario_aparece]: el canario nunca aparecio en memoria\n");
        errors++;
    } else if (cycle_canary_appears > 100) {
        std::printf("FAIL [desacople_canario_rapido]: aparecio en el ciclo %d, esperado < 100 (desacople A.1 no esta funcionando)\n",
                     cycle_canary_appears);
        errors++;
    } else {
        std::printf("OK   [desacople_canario_rapido]: aparecio en el ciclo %d (<< ~1152 ciclos de vntt)\n", cycle_canary_appears);
    }

    // ---- Verificacion 2: serializacion (Apendice A.4) — verificada
    // DIRECTAMENTE sobre vector_unit_busy, no inferida del timing de
    // memoria.
    //
    // NOTA IMPORTANTE (correccion tras la primera version de este test):
    // el criterio original asumia que 'x3=99' debia aparecer en memoria
    // solo DESPUES de que AMBAS operaciones vectoriales TERMINARAN por
    // completo (~1152+256 ciclos) — pero eso ignora que el desacople
    // (A.1) aplica a TODA instruccion vectorial, no solo a la primera:
    // 'addi x3,x0,99' es una instruccion ESCALAR independiente que
    // avanza por el pipeline en cuanto 'vbarrett' logra DESPACHAR (no
    // cuando vbarrett TERMINA). El criterio correcto de "serializacion"
    // es que vbarrett no haya podido despachar hasta que vntt hiciera
    // done — verificado aca contando flancos de subida de busy (deben
    // ser exactamente 2, uno por cada operacion) y confirmando que NO
    // hay un hueco prolongado de busy=0 entre ambas (lo cual indicaria
    // que el pipeline avanzo de largo sin que vbarrett llegara a
    // despachar, un bug real de instruccion perdida).
    if (busy_rising_edges != 2) {
        std::printf("FAIL [serializacion_dos_dispatches]: %d flancos de subida, esperado exactamente 2 (vntt, vbarrett)\n",
                     busy_rising_edges);
        errors++;
    } else {
        std::printf("OK   [serializacion_dos_dispatches]: 2 operaciones vectoriales efectivamente despacharon\n");
    }

    if (max_gap_between_busy > 5) {
        std::printf("FAIL [serializacion_sin_hueco_largo]: hueco maximo de %d ciclos entre operaciones (esperado <=5, un hueco grande sugiere que vbarrett tardo en despachar por otra razon que no sea la espera legitima a vntt)\n",
                     max_gap_between_busy);
        errors++;
    } else {
        std::printf("OK   [serializacion_sin_hueco_largo]: hueco maximo de %d ciclos entre vntt y vbarrett (transicion inmediata, como se espera)\n",
                     max_gap_between_busy);
    }

    if (busy_total_cycles < 1300) {
        std::printf("FAIL [serializacion_duracion_total]: unidad ocupada %d ciclos en total, esperado >= ~1300 (suma de vntt+vbarrett — si es menor, alguna operacion no corrio completa)\n",
                     busy_total_cycles);
        errors++;
    } else {
        std::printf("OK   [serializacion_duracion_total]: unidad ocupada %d ciclos en total (consistente con vntt+vbarrett en serie, sin solapamiento)\n",
                     busy_total_cycles);
    }

    // x3=99 deberia aparecer POCO DESPUES de que la unidad vectorial
    // termine su segunda operacion (unos pocos ciclos de latencia de
    // pipeline, no cientos) — y en cualquier caso, mucho despues del
    // canario (ciclo ~11), confirmando que si hubo una espera real.
    if (cycle_final_appears < 0) {
        std::printf("FAIL [serializacion_final_aparece]: el valor final nunca aparecio en memoria (posible deadlock)\n");
        errors++;
    } else if (cycle_final_appears < 1100) {
        std::printf("FAIL [serializacion_no_prematura]: aparecio en el ciclo %d, esperado >= ~1100 (vbarrett parece haber despachado antes de que vntt terminara)\n",
                     cycle_final_appears);
        errors++;
    } else {
        std::printf("OK   [serializacion_no_prematura]: aparecio en el ciclo %d (despues de que vntt termino y vbarrett pudo despachar)\n",
                     cycle_final_appears);
    }

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: integracion final — desacople (A.1) y serializacion (A.4) confirmados con firmware mixto real.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
