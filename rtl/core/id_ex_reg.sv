// id_ex_reg.sv
//
// Registro de segmentacion ID/EX — Fase 2 (pipeline de 5 etapas).
//
// Retiene, al final de ID, todo lo que EX (y las etapas posteriores, via
// los registros siguientes) van a necesitar: datos leidos del regfile,
// el inmediato ya ensamblado, el destino de escritura, y todas las
// senales de control que control.sv genero para esta instruccion.
//
// rs1/rs2 (los NUMEROS de registro, no sus valores) se agregan en este
// punto del proyecto porque ahora SI hay un consumidor real: la
// forwarding unit (Fase 2) los necesita para comparar contra rd de
// ex_mem_reg/mem_wb_reg y decidir de donde forwardear cada operando de
// la ALU. Antes de que la forwarding unit existiera, se dejaron fuera
// deliberadamente (mismo criterio que para no anticipar un campo de
// excepcion/illegal sin consumidor) — este es el mismo criterio aplicado
// en sentido inverso: agregar el campo apenas aparece quien lo consume.
//
// opcode se propaga completo (en vez de descomponerlo en señales mas
// especificas) porque EX todavia necesita distinguir AUIPC/LUI/JALR para
// los muxes de operando de la ALU y de proximo PC — igual que en
// core_top.sv (Fase 1).
//
// funct3 se propaga mas alla de EX (seguira hacia EX/MEM) porque tanto
// branch_unit (dentro de EX) como dmem (en MEM) lo necesitan.
//
// Politica de burbuja: identica a if_id_reg — valid explicito + NOP
// como red de seguridad. Como esta reg no lleva 'instr' cruda (ID ya la
// descompuso), el "NOP" aca se expresa forzando las señales de control a
// su estado inerte (reg_write=0, mem_write=0, branch=0, jump=0) cuando
// valid=0, no reinyectando el encoding 0x13 — no hay instr en este
// registro para reinyectar.

module id_ex_reg
    import alu_pkg::*;
(
    input  logic         clk,
    input  logic         rst_n,

    input  logic         stall,
    input  logic         flush,

    // ---- Entradas (salida de ID) ----
    input  logic         valid_in,
    input  logic [31:0]  pc_in,
    input  logic [31:0]  rs1_data_in,
    input  logic [31:0]  rs2_data_in,
    input  logic [31:0]  imm_in,
    input  logic [4:0]   rd_in,
    input  logic [4:0]   rs1_in,
    input  logic [4:0]   rs2_in,
    input  logic [2:0]   funct3_in,
    input  logic [6:0]   opcode_in,

    input  logic         reg_write_in,
    input  logic         alu_src_in,
    input  alu_op_t      alu_op_in,
    input  logic         mem_read_in,
    input  logic         mem_write_in,
    input  logic         mem_to_reg_in,
    input  logic         branch_in,
    input  logic         jump_in,

    // ---- Salidas (entrada de EX) ----
    output logic         valid_out,
    output logic [31:0]  pc_out,
    output logic [31:0]  rs1_data_out,
    output logic [31:0]  rs2_data_out,
    output logic [31:0]  imm_out,
    output logic [4:0]   rd_out,
    output logic [4:0]   rs1_out,
    output logic [4:0]   rs2_out,
    output logic [2:0]   funct3_out,
    output logic [6:0]   opcode_out,

    output logic         reg_write_out,
    output logic         alu_src_out,
    output alu_op_t      alu_op_out,
    output logic         mem_read_out,
    output logic         mem_write_out,
    output logic         mem_to_reg_out,
    output logic         branch_out,
    output logic         jump_out
);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            valid_out      <= 1'b0;
            reg_write_out  <= 1'b0;
            mem_read_out   <= 1'b0;
            mem_write_out  <= 1'b0;
            mem_to_reg_out <= 1'b0;
            branch_out     <= 1'b0;
            jump_out       <= 1'b0;
            alu_src_out    <= 1'b0;
            alu_op_out     <= ALU_ADD;
            pc_out         <= 32'd0;
            rs1_data_out   <= 32'd0;
            rs2_data_out   <= 32'd0;
            imm_out        <= 32'd0;
            rd_out         <= 5'd0;
            rs1_out        <= 5'd0;
            rs2_out        <= 5'd0;
            funct3_out     <= 3'd0;
            opcode_out     <= 7'd0;
        end else if (flush) begin
            // Burbuja: invalidar + forzar todas las senales de control a
            // su estado inerte (nada de esto debe escribir estado
            // arquitectural). Los campos de dato (pc, rs1_data, etc.) no
            // se fuerzan — son irrelevantes cuando valid_out=0, y forzarlos
            // solo agregaria logica sin beneficio real.
            valid_out      <= 1'b0;
            reg_write_out  <= 1'b0;
            mem_read_out   <= 1'b0;
            mem_write_out  <= 1'b0;
            mem_to_reg_out <= 1'b0;
            branch_out     <= 1'b0;
            jump_out       <= 1'b0;
        end else if (stall) begin
            // Mantener el contenido actual: no se actualiza ningun campo.
        end else begin
            valid_out      <= valid_in;
            pc_out         <= pc_in;
            rs1_data_out   <= rs1_data_in;
            rs2_data_out   <= rs2_data_in;
            imm_out        <= imm_in;
            rd_out         <= rd_in;
            // rs1/rs2 se fuerzan a x0 en burbuja (valid_in=0): x0 nunca
            // es destino de escritura real, asi que si la forwarding
            // unit compara contra estos valores sin chequear 'valid'
            // primero, la comparacion nunca dispara un forward espurio
            // — misma logica de doble seguridad que el NOP en if_id_reg.
            rs1_out        <= valid_in ? rs1_in : 5'd0;
            rs2_out        <= valid_in ? rs2_in : 5'd0;
            funct3_out     <= funct3_in;
            opcode_out     <= opcode_in;

            // Si la instruccion entrante no es valida (burbuja desde
            // IF/ID), las senales de control tambien se fuerzan a su
            // estado inerte, igual que en el caso de flush.
            reg_write_out  <= valid_in ? reg_write_in  : 1'b0;
            alu_src_out    <= alu_src_in;
            alu_op_out     <= alu_op_in;
            mem_read_out   <= valid_in ? mem_read_in   : 1'b0;
            mem_write_out  <= valid_in ? mem_write_in  : 1'b0;
            mem_to_reg_out <= mem_to_reg_in;
            branch_out     <= valid_in ? branch_in     : 1'b0;
            jump_out       <= valid_in ? jump_in       : 1'b0;
        end
    end

endmodule
