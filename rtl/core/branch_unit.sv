// branch_unit.sv
//
// Unidad de branch RV32I — Fase 1 (core escalar monociclo).
//
// Combinacional pura. La unidad de control ya fijo el alu_op correcto para
// cada branch (ver control.sv):
//   beq/bne  -> ALU_SUB, se usa la bandera 'zero' de la ALU
//   blt/bge  -> ALU_SLT,  se usa alu_result[0] (1 si rs1 < rs2, con signo)
//   bltu/bgeu-> ALU_SLTU, se usa alu_result[0] (1 si rs1 < rs2, sin signo)
//
// Esta unidad solo necesita 'zero' y el bit 0 del resultado de la ALU (que,
// para SLT/SLTU, es directamente 0 o 1) mas funct3, para decidir si el
// branch efectivamente se toma. No vuelve a comparar rs1/rs2 — reusa el
// trabajo que la ALU ya hizo, evitando duplicar comparadores en hardware.
//
// Tabla de decision (ver green card, columna "USES" de pseudo-instructions
// y la seccion SB de opcodes):
//   funct3 000 (beq):  taken = zero
//   funct3 001 (bne):  taken = ~zero
//   funct3 100 (blt):  taken = alu_result[0]      (rs1 < rs2, con signo)
//   funct3 101 (bge):  taken = ~alu_result[0]      (rs1 >= rs2, con signo)
//   funct3 110 (bltu): taken = alu_result[0]      (rs1 < rs2, sin signo)
//   funct3 111 (bgeu): taken = ~alu_result[0]      (rs1 >= rs2, sin signo)

module branch_unit (
    input  logic        branch,       // de la unidad de control: instruccion es SB-type
    input  logic [2:0]  funct3,
    input  logic         alu_zero,     // bandera zero de la ALU (para beq/bne)
    input  logic         alu_result0,  // bit 0 del resultado de la ALU (para blt/bge/bltu/bgeu)

    output logic         branch_taken
);

    always_comb begin
        if (!branch) begin
            branch_taken = 1'b0;
        end else begin
            case (funct3)
                3'b000:  branch_taken = alu_zero;        // beq
                3'b001:  branch_taken = ~alu_zero;       // bne
                3'b100:  branch_taken = alu_result0;     // blt
                3'b101:  branch_taken = ~alu_result0;    // bge
                3'b110:  branch_taken = alu_result0;     // bltu
                3'b111:  branch_taken = ~alu_result0;    // bgeu
                default: branch_taken = 1'b0;            // funct3 no valido para branch
            endcase
        end
    end

endmodule
