// ex_mem_reg.sv
//
// Registro de segmentacion EX/MEM — Fase 2 (pipeline de 5 etapas).
//
// Retiene, al final de EX, lo que MEM (y WB, via el siguiente registro)
// necesitan. A partir de aca varios campos de ID/EX ya cumplieron su
// funcion y se descartan:
//   - rs1_data, imm, opcode, alu_src: ya se usaron dentro de EX para
//     calcular alu_result; no hacen falta mas alla de esta etapa.
//   - pc completo: ya no hace falta el PC crudo — lo unico que WB
//     necesita de esa familia es la direccion de retorno (pc+4) para
//     JAL/JALR, que se calcula UNA vez en EX (pc_plus4_in) y se propaga
//     ya resuelta, en vez de repetir la suma en cada etapa.
//   - branch: el branch ya se resolvio en EX (branch_unit + calculo de
//     target); MEM/WB no necesitan saber que la instruccion fue un
//     branch.
//
// Lo que SI sobrevive:
//   - alu_result: es la direccion de memoria para loads/stores, o el
//     resultado a escribir en WB para operaciones que no van a memoria.
//   - rs2_data: el dato a escribir en un store (dmem.wdata).
//   - rd, funct3: rd para el destino de WB; funct3 porque dmem todavia
//     lo necesita en MEM para decidir tamano/signo del acceso.
//   - reg_write, mem_read, mem_write, mem_to_reg, jump: control que
//     sigue siendo relevante en MEM y WB.
//   - pc_plus4: direccion de retorno para JAL/JALR, ya calculada en EX.
//
// Politica de burbuja: identica a id_ex_reg — valid explicito, y
// reg_write/mem_read/mem_write forzados a inerte en flush o cuando
// valid_in=0 (no hay 'instr' que reinyectar como NOP en este registro,
// igual que en ID/EX).

module ex_mem_reg (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         stall,
    input  logic         flush,

    // ---- Entradas (salida de EX) ----
    input  logic         valid_in,
    input  logic [31:0]  alu_result_in,
    input  logic [31:0]  rs2_data_in,
    input  logic [31:0]  pc_plus4_in,
    input  logic [4:0]   rd_in,
    input  logic [2:0]   funct3_in,

    input  logic         reg_write_in,
    input  logic         mem_read_in,
    input  logic         mem_write_in,
    input  logic         mem_to_reg_in,
    input  logic         jump_in,

    // ---- Salidas (entrada de MEM) ----
    output logic         valid_out,
    output logic [31:0]  alu_result_out,
    output logic [31:0]  rs2_data_out,
    output logic [31:0]  pc_plus4_out,
    output logic [4:0]   rd_out,
    output logic [2:0]   funct3_out,

    output logic         reg_write_out,
    output logic         mem_read_out,
    output logic         mem_write_out,
    output logic         mem_to_reg_out,
    output logic         jump_out
);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            valid_out      <= 1'b0;
            reg_write_out  <= 1'b0;
            mem_read_out   <= 1'b0;
            mem_write_out  <= 1'b0;
            mem_to_reg_out <= 1'b0;
            jump_out       <= 1'b0;
            alu_result_out <= 32'd0;
            rs2_data_out   <= 32'd0;
            pc_plus4_out   <= 32'd0;
            rd_out         <= 5'd0;
            funct3_out     <= 3'd0;
        end else if (flush) begin
            valid_out      <= 1'b0;
            reg_write_out  <= 1'b0;
            mem_read_out   <= 1'b0;
            mem_write_out  <= 1'b0;
            mem_to_reg_out <= 1'b0;
            jump_out       <= 1'b0;
        end else if (stall) begin
            // Mantener el contenido actual: no se actualiza ningun campo.
        end else begin
            valid_out      <= valid_in;
            alu_result_out <= alu_result_in;
            rs2_data_out   <= rs2_data_in;
            pc_plus4_out   <= pc_plus4_in;
            rd_out         <= rd_in;
            funct3_out     <= funct3_in;

            reg_write_out  <= valid_in ? reg_write_in  : 1'b0;
            mem_read_out   <= valid_in ? mem_read_in   : 1'b0;
            mem_write_out  <= valid_in ? mem_write_in  : 1'b0;
            mem_to_reg_out <= mem_to_reg_in;
            jump_out       <= valid_in ? jump_in       : 1'b0;
        end
    end

endmodule
