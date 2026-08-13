// if_id_reg.sv
//
// Registro de segmentacion IF/ID — Fase 2 (pipeline de 5 etapas).
//
// Retiene, al final de IF, lo que ID necesita en el ciclo siguiente:
// la instruccion fetcheada y su PC (para calcular targets de branch/jump
// y direcciones de retorno de JAL/JALR mas adelante en el pipeline).
//
// Politica de burbuja (ver discusion de diseno, Fase 2): cada burbuja se
// representa con DOS senales en conjunto, no una sola:
//   - valid=0: senal explicita que la logica de control/forwarding debe
//     consultar antes de actuar sobre el contenido de esta etapa.
//   - instr=NOP (addi x0,x0,0 = 0x00000013): red de seguridad adicional,
//     para que aunque algo en el datapath no chequee 'valid' 
//     correctamente, la instruccion en si sea inerte (no escribe
//     registros, no accede memoria, no hace branch).
//
// Señales de control:
//   stall: mantiene el contenido actual (no avanza) — usado para el
//          load-use hazard (Fase 2, hazard detection unit).
//   flush: invalida el contenido (valid<=0, instr<=NOP) — usado cuando
//          un branch/jump se resuelve como tomado y la instruccion ya
//          fetcheada en esta ranura resulta ser la incorrecta.
//   Si ambas estan activas el mismo ciclo, flush tiene prioridad sobre
//   stall (invalidar es mas "fuerte" que mantener).

module if_id_reg (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         stall,
    input  logic         flush,

    input  logic         valid_in,
    input  logic [31:0]  instr_in,
    input  logic [31:0]  pc_in,

    output logic         valid_out,
    output logic [31:0]  instr_out,
    output logic [31:0]  pc_out
);

    localparam logic [31:0] NOP = 32'h00000013;  // addi x0, x0, 0

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            valid_out <= 1'b0;
            instr_out <= NOP;
            pc_out    <= 32'd0;
        end else if (flush) begin
            valid_out <= 1'b0;
            instr_out <= NOP;
            // pc_out se deja sin especificar en flush: no importa, ya que
            // valid_out=0 hace que ID nunca deba usar este pc para nada
            // que afecte estado arquitectural.
        end else if (stall) begin
            // Mantener el contenido actual: no se actualiza ningun campo.
        end else begin
            valid_out <= valid_in;
            instr_out <= valid_in ? instr_in : NOP;
            pc_out    <= pc_in;
        end
    end

endmodule
