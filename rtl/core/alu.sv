// alu.sv
//
// ALU RV32I — Fase 1 (core escalar monociclo).
//
// Combinacional pura: un solo ciclo, sin estado. Recibe dos operandos y un
// codigo de operacion (alu_op) ya decodificado por la unidad de control a
// partir de opcode/funct3/funct7 (la traduccion opcode->alu_op se hace en
// la unidad de control, no aca — la ALU no conoce el formato de instruccion).
//
// Operaciones cubiertas (RV32I base, ver green card):
//   ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU
//
// SLT/SLTU tambien se usan internamente para resolver los branches
// (BLT/BGE/BLTU/BGEU se arman comparando con SLT/SLTU en la unidad de
// control o en una unidad de branch separada — la ALU solo expone el
// resultado de la comparacion cruda).

package alu_pkg;
    typedef enum logic [3:0] {
        ALU_ADD  = 4'b0000,
        ALU_SUB  = 4'b0001,
        ALU_AND  = 4'b0010,
        ALU_OR   = 4'b0011,
        ALU_XOR  = 4'b0100,
        ALU_SLL  = 4'b0101,
        ALU_SRL  = 4'b0110,
        ALU_SRA  = 4'b0111,
        ALU_SLT  = 4'b1000,
        ALU_SLTU = 4'b1001
    } alu_op_t;
endpackage

module alu
    import alu_pkg::*;
(
    input  logic [31:0]  a,
    input  logic [31:0]  b,
    input  alu_op_t      alu_op,

    output logic [31:0]  result,
    output logic         zero      // result == 0 — usado para branches (beq/bne)
);

    // Los shifts en RV32I usan solo los 5 bits menos significativos del
    // operando de shift (shamt), ya que un shift de 32 bits solo tiene
    // sentido en el rango 0-31.
    logic [4:0] shamt;
    assign shamt = b[4:0];

    always_comb begin
        case (alu_op)
            ALU_ADD:  result = a + b;
            ALU_SUB:  result = a - b;
            ALU_AND:  result = a & b;
            ALU_OR:   result = a | b;
            ALU_XOR:  result = a ^ b;
            ALU_SLL:  result = a << shamt;
            ALU_SRL:  result = a >> shamt;
            ALU_SRA:  result = $signed(a) >>> shamt;
            ALU_SLT:  result = ($signed(a) < $signed(b)) ? 32'd1 : 32'd0;
            ALU_SLTU: result = (a < b) ? 32'd1 : 32'd0;
            default:  result = 32'd0;
        endcase
    end

    assign zero = (result == 32'd0);

endmodule
