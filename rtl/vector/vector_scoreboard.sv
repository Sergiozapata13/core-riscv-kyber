// vector_scoreboard.sv
//
// Scoreboard vectorial de 4 bits — Fase 4 (decision A.4 del Apendice A).
//
// Un bit por registro vectorial (v0-v3). Se marca ocupado cuando se
// despacha una instruccion vectorial con ese registro como destino
// (pulso 'set_pulse' + 'set_vreg'), y se libera cuando la unidad vectorial
// señaliza 'done' para esa operacion (pulso 'clear' + 'clear_vreg').
//
// NOTA DE DISEÑO — redundancia consciente con el gateo de recurso unico:
// dado que vector_unit es un recurso unico no pipelineado (Apendice A.1),
// vector_control.sv YA bloquea 'vec_start' mientras vector_unit_busy=1,
// sin importar que registros toque la nueva instruccion — es decir, como
// maximo UNA operacion vectorial puede estar "en vuelo" en cualquier
// momento, y por lo tanto como maximo UN bit de este scoreboard puede
// estar activo a la vez. Esto significa que, en este diseño especifico,
// el chequeo POR REGISTRO que hace este scoreboard es funcionalmente
// redundante con el chequeo "unidad ocupada" para efectos de STALL del
// pipeline (ver core_top_pipelined.sv, señal vector_stall) — no existe
// ningun escenario, bajo un recurso unico, donde el chequeo fino por
// registro permita avanzar algo que el chequeo grueso bloquearia, ni
// viceversa.
//
// Se construye igual, exactamente como especifica el Apendice A.4, por
// tres razones: (1) fidelidad al diseño ya cerrado y revisado, (2) es el
// gancho natural si en el futuro se agrega una cola de instrucciones
// vectoriales (alternativa ya evaluada y descartada en A.4, documentada
// como extension futura) — en ese escenario SI dejaria de ser redundante,
// ya que podria haber mas de una operacion en vuelo, y (3) da visibilidad
// util en las waveforms para la documentacion de la Fase 6 (que registro
// esta ocupado y por que).
//
// Politica de colision: si 'set_pulse' y 'clear' apuntan al MISMO bit en el
// MISMO ciclo (puede pasar: una operacion termina con 'done' el mismo
// ciclo en que busy cae a 0 y una NUEVA operacion sobre el mismo
// registro logra despachar de inmediato), 'set_pulse' tiene prioridad — el
// registro queda "recien reservado por la nueva operacion", no libre.

module vector_scoreboard (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         set_pulse,
    input  logic  [1:0]  set_vreg,

    input  logic         clear,
    input  logic  [1:0]  clear_vreg,

    output logic  [3:0]  busy   // busy[i] = 1 si vi esta ocupado
);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            busy <= 4'b0000;
        end else begin
            // 'clear' se aplica primero (en orden textual dentro del
            // bloque); 'set_pulse' se aplica despues — en simulacion, la
            // ULTIMA asignacion no-bloqueante a un mismo bit en el mismo
            // ciclo es la que efectivamente queda, asi que este orden
            // garantiza que 'set_pulse' gana si ambos apuntan al mismo bit.
            if (clear) begin
                busy[clear_vreg] <= 1'b0;
            end
            if (set_pulse) begin
                busy[set_vreg] <= 1'b1;
            end
        end
    end

endmodule
