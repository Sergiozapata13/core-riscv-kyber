// control.sv
//
// Unidad de control RV32I — Fase 1 (core escalar monociclo).
//
// Combinacional pura: decodifica opcode/funct3/funct7 y genera las senales
// de control que gobiernan el resto del datapath (ver diagrama de bloques
// de la Fase 1). No conoce nada de pipeline — eso se agrega en la Fase 2
// como registros de segmentacion que propagan estas mismas senales.
//
// Opcodes RV32I cubiertos (ver green card, "OPCODES IN NUMERICAL ORDER"):
//   0110011 R-type   (add, sub, and, or, xor, sll, srl, sra, slt, sltu)
//   0010011 I-type   (addi, andi, ori, xori, slli, srli, srai, slti, sltiu)
//   0000011 I-type   (lb, lh, lw, lbu, lhu — loads)
//   0100011 S-type   (sb, sh, sw — stores)
//   1100011 SB-type  (beq, bne, blt, bge, bltu, bgeu — branches)
//   1101111 UJ-type  (jal)
//   1100111 I-type   (jalr)
//   0110111 U-type   (lui)
//   0010111 U-type   (auipc)

package control_pkg;

    // Selector de formato de inmediato — usado por el generador de inmediato
    // (modulo separado, no incluido en este archivo) para saber que campos
    // de la instruccion ensamblar y como hacer sign-extend.
    typedef enum logic [2:0] {
        IMM_I = 3'b000,
        IMM_S = 3'b001,
        IMM_B = 3'b010,
        IMM_U = 3'b011,
        IMM_J = 3'b100,
        IMM_X = 3'b111   // no aplica (ej. instrucciones R-type)
    } imm_sel_t;

endpackage

module control
    import alu_pkg::*;
    import control_pkg::*;
(
    input  logic [6:0]  opcode,
    input  logic [2:0]  funct3,
    // verilator lint_off UNUSEDSIGNAL
    // Solo funct7[5] se consulta (distingue add/sub y srl/sra); el resto
    // de bits de funct7 no aporta informacion en RV32I base.
    input  logic [6:0]  funct7,
    // verilator lint_on UNUSEDSIGNAL

    output logic         reg_write,   // habilita escritura al register file (WB)
    output logic         alu_src,     // 0: ALU usa rs2 | 1: ALU usa inmediato
    output alu_op_t      alu_op,      // operacion que ejecuta la ALU
    output logic         mem_read,    // habilita lectura de memoria de datos (loads)
    output logic         mem_write,   // habilita escritura a memoria de datos (stores)
    output logic         mem_to_reg,  // 0: WB toma resultado de ALU | 1: WB toma dato de memoria
    output logic         branch,      // instruccion es un branch condicional (SB-type)
    output logic         jump,        // instruccion es un salto incondicional (jal/jalr)
    output imm_sel_t     imm_sel,     // formato de inmediato a ensamblar
    output logic         illegal      // opcode no reconocido (util para debug/trap futuro)
);

    // Opcodes RV32I base (ver green card)
    localparam logic [6:0] OP_RTYPE  = 7'b0110011;
    localparam logic [6:0] OP_ITYPE  = 7'b0010011;
    localparam logic [6:0] OP_LOAD   = 7'b0000011;
    localparam logic [6:0] OP_STORE  = 7'b0100011;
    localparam logic [6:0] OP_BRANCH = 7'b1100011;
    localparam logic [6:0] OP_JAL    = 7'b1101111;
    localparam logic [6:0] OP_JALR   = 7'b1100111;
    localparam logic [6:0] OP_LUI    = 7'b0110111;
    localparam logic [6:0] OP_AUIPC  = 7'b0010111;

    // ------------------------------------------------------------------
    // Traduccion funct3/funct7 -> alu_op_t para R-type e I-type aritmetico.
    // Ambos comparten la misma tabla funct3 (con la excepcion del bit 30
    // de la instruccion — equivalente a funct7[5] — que distingue
    // add/sub y srl/sra). Se factoriza en una funcion para no duplicar
    // el case entre R-type e I-type.
    // ------------------------------------------------------------------
    function automatic alu_op_t decode_alu_op(
        input logic [2:0] f3,
        input logic       f7_bit5,   // funct7[5]: distingue add/sub y srl/sra
        input logic       is_rtype   // I-type nunca usa SUB (no existe subi)
    );
        case (f3)
            3'b000:  decode_alu_op = (is_rtype && f7_bit5) ? ALU_SUB : ALU_ADD;
            3'b001:  decode_alu_op = ALU_SLL;
            3'b010:  decode_alu_op = ALU_SLT;
            3'b011:  decode_alu_op = ALU_SLTU;
            3'b100:  decode_alu_op = ALU_XOR;
            3'b101:  decode_alu_op = f7_bit5 ? ALU_SRA : ALU_SRL;
            3'b110:  decode_alu_op = ALU_OR;
            3'b111:  decode_alu_op = ALU_AND;
            default: decode_alu_op = ALU_ADD;
        endcase
    endfunction

    always_comb begin
        // Defaults seguros: instruccion invalida no debe escribir estado.
        reg_write  = 1'b0;
        alu_src    = 1'b0;
        alu_op     = ALU_ADD;
        mem_read   = 1'b0;
        mem_write  = 1'b0;
        mem_to_reg = 1'b0;
        branch     = 1'b0;
        jump       = 1'b0;
        imm_sel    = IMM_X;
        illegal    = 1'b0;

        case (opcode)

            OP_RTYPE: begin
                reg_write = 1'b1;
                alu_src   = 1'b0;               // operando B viene de rs2
                alu_op    = decode_alu_op(funct3, funct7[5], 1'b1);
                imm_sel   = IMM_X;               // no hay inmediato
            end

            OP_ITYPE: begin
                reg_write = 1'b1;
                alu_src   = 1'b1;               // operando B viene del inmediato
                // srli/srai codifican el bit distintivo en funct7[5] igual
                // que R-type; el resto de I-type aritmetico lo ignora
                // (decode_alu_op ya fuerza ADD para funct3=000 sin importar
                // f7_bit5 al pasar is_rtype=0).
                alu_op    = decode_alu_op(funct3, funct7[5], 1'b0);
                imm_sel   = IMM_I;
            end

            OP_LOAD: begin
                reg_write  = 1'b1;
                alu_src    = 1'b1;               // ALU calcula rs1 + offset
                alu_op     = ALU_ADD;
                mem_read   = 1'b1;
                mem_to_reg = 1'b1;               // WB toma el dato leido de memoria
                imm_sel    = IMM_I;
            end

            OP_STORE: begin
                alu_src   = 1'b1;                // ALU calcula rs1 + offset
                alu_op    = ALU_ADD;
                mem_write = 1'b1;
                imm_sel   = IMM_S;
                // reg_write permanece en 0: un store no escribe el regfile
            end

            OP_BRANCH: begin
                alu_src = 1'b0;                  // ALU compara rs1 vs rs2
                branch  = 1'b1;
                imm_sel = IMM_B;
                // La ALU se usa para la comparacion; el codigo exacto
                // (beq/bne/blt/...) se resuelve combinando funct3 con el
                // resultado de la ALU en la unidad de branch (ver EX,
                // modulo separado) — aca solo se fija SUB para beq/bne
                // (compara igualdad via zero flag) y SLT/SLTU para el resto.
                case (funct3)
                    3'b000, 3'b001: alu_op = ALU_SUB;   // beq, bne -> usan 'zero'
                    3'b100, 3'b101: alu_op = ALU_SLT;   // blt, bge
                    3'b110, 3'b111: alu_op = ALU_SLTU;  // bltu, bgeu
                    default:        alu_op = ALU_SUB;
                endcase
            end

            OP_JAL: begin
                reg_write = 1'b1;                // rd <= PC+4
                jump      = 1'b1;
                imm_sel   = IMM_J;
            end

            OP_JALR: begin
                reg_write = 1'b1;                // rd <= PC+4
                alu_src   = 1'b1;                // ALU calcula rs1 + offset (target)
                alu_op    = ALU_ADD;
                jump      = 1'b1;
                imm_sel   = IMM_I;
            end

            OP_LUI: begin
                reg_write = 1'b1;                // rd <= imm_U (ALU pasa el inmediato)
                alu_src   = 1'b1;
                alu_op    = ALU_ADD;             // 0 + imm_U (rs1 forzado a 0 fuera de esta unidad, o sumado con a=0)
                imm_sel   = IMM_U;
            end

            OP_AUIPC: begin
                reg_write = 1'b1;                // rd <= PC + imm_U
                alu_src   = 1'b1;
                alu_op    = ALU_ADD;             // PC + imm_U (el operando A=PC se selecciona fuera de esta unidad)
                imm_sel   = IMM_U;
            end

            default: begin
                illegal = 1'b1;                  // opcode no reconocido
            end

        endcase
    end

endmodule
