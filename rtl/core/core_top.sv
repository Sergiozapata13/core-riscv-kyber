// core_top.sv
//
// Datapath monociclo RV32I completo — Fase 1 (criterio de cierre).
//
// Instancia y conecta todos los modulos ya verificados por separado
// (regfile, alu, control, imm_gen, branch_unit, imem, dmem) segun el
// diagrama de bloques de la Fase 1: IF -> ID -> EX -> MEM -> WB, todo en
// un solo ciclo de reloj, sin segmentacion (eso llega en la Fase 2).
//
// Puntos de diseno resueltos aca (no cubiertos por los modulos
// individuales, porque son especificos de como se cablea el datapath):
//
//   1. Operando A de la ALU: normalmente rs1, pero AUIPC necesita PC
//      (para calcular PC + imm_U) y LUI necesita 0 (para que el
//      resultado sea simplemente el inmediato). Se resuelve con un mux
//      de 3 vias en base al opcode.
//
//   2. Dato de write-back: normalmente el resultado de la ALU o el dato
//      de memoria (eso ya lo cubre mem_to_reg de control.sv), pero
//      JAL/JALR escriben PC+4 en rd (direccion de retorno), no un
//      resultado de la ALU. Se resuelve con un mux adicional.
//
//   3. Calculo del proximo PC: PC+4 por defecto; PC+imm si un branch se
//      toma o si es JAL; (rs1+imm) con bit 0 forzado a 0 si es JALR
//      (ver green card, nota sobre jalr).

module core_top
    import alu_pkg::*;
    import control_pkg::*;
#(
    parameter int ADDR_WIDTH = 16,
    parameter string INIT_FILE = ""
) (
    input  logic clk,
    input  logic rst_n
);

    localparam logic [6:0] OP_AUIPC = 7'b0010111;
    localparam logic [6:0] OP_LUI   = 7'b0110111;
    localparam logic [6:0] OP_JALR  = 7'b1100111;

    // -------------------------------------------------------------------
    // IF — Program Counter + memoria de instrucciones
    // -------------------------------------------------------------------
    logic [31:0] pc;
    logic [31:0] pc_next;
    logic [31:0] pc_plus4;
    logic [31:0] instr;

    assign pc_plus4 = pc + 32'd4;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) pc <= 32'd0;
        else        pc <= pc_next;
    end

    imem #(
        .ADDR_WIDTH(ADDR_WIDTH),
        .INIT_FILE (INIT_FILE)
    ) u_imem (
        .addr  (pc[ADDR_WIDTH-1:0]),
        .instr (instr)
    );

    // -------------------------------------------------------------------
    // ID — Decodificacion de campos + unidad de control + regfile + imm_gen
    // -------------------------------------------------------------------
    logic [6:0] opcode;
    logic [4:0] rd, rs1, rs2;
    logic [2:0] funct3;
    logic [6:0] funct7;

    assign opcode = instr[6:0];
    assign rd     = instr[11:7];
    assign funct3 = instr[14:12];
    assign rs1    = instr[19:15];
    assign rs2    = instr[24:20];
    assign funct7 = instr[31:25];

    // verilator lint_off UNUSEDSIGNAL
    // c_illegal queda reservado para manejo de excepciones/trap en una
    // fase futura; en el datapath monociclo de la Fase 1 no se conecta
    // a ninguna logica todavia.
    logic         c_reg_write, c_alu_src, c_mem_read, c_mem_write;
    logic         c_mem_to_reg, c_branch, c_jump, c_illegal;
    // verilator lint_on UNUSEDSIGNAL
    alu_op_t      c_alu_op;
    imm_sel_t     c_imm_sel;

    control u_control (
        .opcode     (opcode),
        .funct3     (funct3),
        .funct7     (funct7),
        .reg_write  (c_reg_write),
        .alu_src    (c_alu_src),
        .alu_op     (c_alu_op),
        .mem_read   (c_mem_read),
        .mem_write  (c_mem_write),
        .mem_to_reg (c_mem_to_reg),
        .branch     (c_branch),
        .jump       (c_jump),
        .imm_sel    (c_imm_sel),
        .illegal    (c_illegal)
    );

    logic [31:0] rs1_data, rs2_data;
    logic [31:0] wb_data;   // definido mas abajo (WB), el regfile lo escribe

    regfile #(
        .ENABLE_BYPASS(1'b0)   // monociclo: sin bypass, ver justificacion en regfile.sv
    ) u_regfile (
        .clk    (clk),
        .we     (c_reg_write),
        .waddr  (rd),
        .wdata  (wb_data),
        .raddr1 (rs1),
        .raddr2 (rs2),
        .rdata1 (rs1_data),
        .rdata2 (rs2_data)
    );

    logic [31:0] imm;

    imm_gen u_imm_gen (
        .instr   (instr),
        .imm_sel (c_imm_sel),
        .imm     (imm)
    );

    // -------------------------------------------------------------------
    // EX — ALU + unidad de branch
    // -------------------------------------------------------------------
    // Operando A: PC para AUIPC, 0 para LUI, rs1 para el resto.
    logic [31:0] alu_opA;
    always_comb begin
        case (opcode)
            OP_AUIPC: alu_opA = pc;
            OP_LUI:   alu_opA = 32'd0;
            default:  alu_opA = rs1_data;
        endcase
    end

    // Operando B: rs2 o inmediato, segun ya decide control.alu_src.
    logic [31:0] alu_opB;
    assign alu_opB = c_alu_src ? imm : rs2_data;

    logic [31:0] alu_result;
    logic         alu_zero;

    alu u_alu (
        .a      (alu_opA),
        .b      (alu_opB),
        .alu_op (c_alu_op),
        .result (alu_result),
        .zero   (alu_zero)
    );

    logic branch_taken;

    branch_unit u_branch_unit (
        .branch       (c_branch),
        .funct3       (funct3),
        .alu_zero     (alu_zero),
        .alu_result0  (alu_result[0]),
        .branch_taken (branch_taken)
    );

    // -------------------------------------------------------------------
    // MEM — memoria de datos
    // -------------------------------------------------------------------
    logic [31:0] mem_rdata;

    logic [31:0] dmem_rdata2_unused;

    dmem #(
        .ADDR_WIDTH(ADDR_WIDTH)
    ) u_dmem (
        .clk        (clk),
        .addr       (alu_result[ADDR_WIDTH-1:0]),
        .wdata      (rs2_data),
        .mem_read   (c_mem_read),
        .mem_write  (c_mem_write),
        .funct3     (funct3),
        .rdata      (mem_rdata),
        // Puerto 2 (Fase 4, vector_unit): inerte aca — core_top.sv es el
        // datapath monociclo de la Fase 1, sin integracion vectorial.
        .addr2      ({ADDR_WIDTH{1'b0}}),
        .wdata2     (32'd0),
        .mem_read2  (1'b0),
        .mem_write2 (1'b0),
        .funct3_2   (3'd0),
        .rdata2     (dmem_rdata2_unused)
    );

    // -------------------------------------------------------------------
    // WB — mux de write-back: ALU, memoria, o PC+4 (para JAL/JALR)
    // -------------------------------------------------------------------
    always_comb begin
        if (c_jump)        wb_data = pc_plus4;   // direccion de retorno
        else if (c_mem_to_reg) wb_data = mem_rdata;
        else               wb_data = alu_result;
    end

    // -------------------------------------------------------------------
    // Proximo PC: secuencial, branch tomado, JAL, o JALR
    // -------------------------------------------------------------------
    logic [31:0] branch_target;
    logic [31:0] jalr_target;

    assign branch_target = pc + imm;                          // beq/bne/...  y jal
    assign jalr_target   = (rs1_data + imm) & ~32'd1;          // jalr: bit 0 forzado a 0

    always_comb begin
        if (opcode == OP_JALR)          pc_next = jalr_target;
        else if (c_jump)                pc_next = branch_target;   // jal
        else if (c_branch && branch_taken) pc_next = branch_target;
        else                             pc_next = pc_plus4;
    end

endmodule
