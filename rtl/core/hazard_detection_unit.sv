// hazard_detection_unit.sv
//
// Unidad de deteccion de hazards — Fase 2 (pipeline de 5 etapas).
//
// Resuelve el UNICO caso que la forwarding unit no puede cubrir: el
// load-use hazard. Ejemplo:
//
//   lw   x1, 0(x2)     // x1 recien existe al FINAL de MEM
//   add  x3, x1, x4    // necesita x1 en su propio EX, un ciclo antes
//                      // de que el dato de 'lw' este disponible en
//                      // ningun punto forwardeable (EX/MEM todavia no
//                      // lo tiene: 'lw' recien esta en EX ese ciclo).
//
// No hay forwarding posible hacia atras en el tiempo — la unica salida
// es perder un ciclo: mantener 'add' un ciclo mas en ID (stall) para
// que 'lw' avance a MEM, y en ese momento si el forwarding EX/MEM->EX
// puede alcanzar el dato.
//
// Condicion de deteccion (evaluada en ID, comparando contra lo que hay
// actualmente en id_ex_reg — es decir, la instruccion que esta a punto
// de entrar a EX):
//
//   id_ex_reg.mem_read == 1                    (la instruccion en EX es un load)
//   AND (id_ex_reg.rd == id_rs1 OR == id_rs2)  (ID necesita ese registro)
//   AND id_ex_reg.rd != 0                       (x0 nunca genera hazard real)
//
// Cuando se detecta:
//   - pc_stall=1     : el PC no avanza, se re-fetchea la misma instruccion.
//   - if_id_stall=1  : if_id_reg mantiene su contenido (ID se re-decodifica).
//   - id_ex_flush=1  : se inserta una burbuja en id_ex_reg — la
//                       instruccion que iba a entrar a EX con datos
//                       incorrectos (rs1/rs2 leidos ANTES de que 'lw'
//                       complete) no debe avanzar; en su lugar entra un
//                       ciclo de burbuja mientras 'lw' sigue su curso.
//
// Notese que esta unidad NO necesita conocer nada de EX/MEM ni MEM/WB:
// el load-use hazard es, por definicion, siempre contra la instruccion
// que esta UNA posicion adelante (en id_ex_reg) — si la dependencia
// fuera con una instruccion mas atras en el pipeline, el forwarding
// normal (EX/MEM->EX o MEM/WB->EX) ya la resuelve sin stall.

module hazard_detection_unit (
    input  logic         id_ex_mem_read,
    input  logic [4:0]   id_ex_rd,

    input  logic [4:0]   id_rs1,
    input  logic [4:0]   id_rs2,

    output logic         pc_stall,
    output logic         if_id_stall,
    output logic         id_ex_flush
);

    logic hazard;

    always_comb begin
        hazard = id_ex_mem_read
               && (id_ex_rd != 5'd0)
               && ((id_ex_rd == id_rs1) || (id_ex_rd == id_rs2));

        pc_stall    = hazard;
        if_id_stall = hazard;
        id_ex_flush = hazard;
    end

endmodule
