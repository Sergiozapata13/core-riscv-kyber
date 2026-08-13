// mem_wb_reg.sv
//
// Registro de segmentacion MEM/WB — Fase 2 (pipeline de 5 etapas).
//
// El mas simple de los cuatro: retiene, al final de MEM, exactamente lo
// que el mux de write-back necesita para decidir que valor escribir en
// el register file. A partir de aca varios campos de EX/MEM ya
// cumplieron su funcion y se descartan:
//   - rs2_data: ya se uso como wdata de dmem dentro de MEM; WB no lo
//     necesita (WB nunca escribe a memoria, solo al regfile).
//   - funct3: ya se uso dentro de MEM para decidir tamano/signo del
//     acceso a dmem; no aporta nada mas alla de esa etapa.
//   - mem_read, mem_write: ya cumplieron su funcion en MEM (habilitar
//     la lectura/escritura de dmem); WB no las necesita.
//
// Lo que SI sobrevive, porque el mux de WB (identico en espiritu al de
// core_top.sv en la Fase 1) lo necesita:
//   - mem_rdata: dato leido de memoria (si la instruccion fue un load).
//   - alu_result: resultado de la ALU (si la instruccion no fue load ni
//     jump) — se mantiene el mismo nombre que en los registros previos
//     por consistencia, aunque en WB representa "el resultado no
//     proveniente de memoria ni de PC+4".
//   - pc_plus4: direccion de retorno, para JAL/JALR.
//   - rd: registro destino.
//   - reg_write, mem_to_reg, jump: las tres señales que el mux de WB
//     consulta, en el mismo orden de prioridad que core_top.sv (Fase 1):
//     jump > mem_to_reg > alu_result.
//
// Politica de burbuja: identica al resto — valid explicito, reg_write
// forzado a inerte en flush o valid_in=0. mem_to_reg y jump no
// necesitan forzarse (si reg_write=0, el mux de WB nunca llega a
// escribir nada al regfile sin importar que seleccionen).

module mem_wb_reg (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         stall,
    input  logic         flush,

    // ---- Entradas (salida de MEM) ----
    input  logic         valid_in,
    input  logic [31:0]  mem_rdata_in,
    input  logic [31:0]  alu_result_in,
    input  logic [31:0]  pc_plus4_in,
    input  logic [4:0]   rd_in,

    input  logic         reg_write_in,
    input  logic         mem_to_reg_in,
    input  logic         jump_in,

    // ---- Salidas (entrada de WB) ----
    output logic         valid_out,
    output logic [31:0]  mem_rdata_out,
    output logic [31:0]  alu_result_out,
    output logic [31:0]  pc_plus4_out,
    output logic [4:0]   rd_out,

    output logic         reg_write_out,
    output logic         mem_to_reg_out,
    output logic         jump_out
);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            valid_out      <= 1'b0;
            reg_write_out  <= 1'b0;
            mem_to_reg_out <= 1'b0;
            jump_out       <= 1'b0;
            mem_rdata_out  <= 32'd0;
            alu_result_out <= 32'd0;
            pc_plus4_out   <= 32'd0;
            rd_out         <= 5'd0;
        end else if (flush) begin
            valid_out      <= 1'b0;
            reg_write_out  <= 1'b0;
        end else if (stall) begin
            // Mantener el contenido actual: no se actualiza ningun campo.
        end else begin
            valid_out      <= valid_in;
            mem_rdata_out  <= mem_rdata_in;
            alu_result_out <= alu_result_in;
            pc_plus4_out   <= pc_plus4_in;
            rd_out         <= rd_in;

            reg_write_out  <= valid_in ? reg_write_in : 1'b0;
            mem_to_reg_out <= mem_to_reg_in;
            jump_out       <= jump_in;
        end
    end

endmodule
