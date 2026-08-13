// imm_gen.sv
//
// Generador de inmediato RV32I — Fase 1 (core escalar monociclo).
//
// Combinacional pura: toma la instruccion cruda de 32 bits y el selector
// de formato (imm_sel, producido por la unidad de control) y ensambla el
// inmediato de 32 bits, sign-extendido segun corresponda.
//
// El bit de signo de la instruccion (instr[31]) es siempre el MSB del
// inmediato en los 5 formatos — es una propiedad deliberada del encoding
// de RISC-V, para que el sign-extend sea identico sin importar el formato
// (ver green card, nota "The immediate field is sign-extended in RISC-V").
//
// Formatos cubiertos (ver green card, "CORE INSTRUCTION FORMATS"):
//   I: imm[11:0]  = instr[31:20]
//   S: imm[11:5]  = instr[31:25], imm[4:0]  = instr[11:7]
//   B: imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7], imm[0]=0
//   U: imm[31:12] = instr[31:12], imm[11:0]=0
//   J: imm[20|10:1|11|19:12] = instr[31|30:21|20|19:12], imm[0]=0

module imm_gen
    import control_pkg::*;
(
    // verilator lint_off UNUSEDSIGNAL
    // Solo se consultan los bits donde viven los campos de inmediato
    // (instr[31:7]); el opcode (instr[6:0]) ya fue decodificado por la
    // unidad de control y no aporta informacion aca.
    input  logic [31:0]  instr,
    // verilator lint_on UNUSEDSIGNAL
    input  imm_sel_t      imm_sel,
    output logic [31:0]  imm
);

    always_comb begin
        case (imm_sel)

            IMM_I: begin
                imm = {{20{instr[31]}}, instr[31:20]};
            end

            IMM_S: begin
                imm = {{20{instr[31]}}, instr[31:25], instr[11:7]};
            end

            IMM_B: begin
                imm = {{19{instr[31]}}, instr[31], instr[7], instr[30:25], instr[11:8], 1'b0};
            end

            IMM_U: begin
                imm = {instr[31:12], 12'b0};
            end

            IMM_J: begin
                imm = {{11{instr[31]}}, instr[31], instr[19:12], instr[20], instr[30:21], 1'b0};
            end

            default: begin
                // IMM_X u otro valor no usado: instrucciones que no
                // consumen inmediato (R-type). Se fuerza 0 por claridad,
                // aunque en la practica alu_src=0 hace que este valor
                // nunca llegue a la ALU.
                imm = 32'd0;
            end

        endcase
    end

endmodule
