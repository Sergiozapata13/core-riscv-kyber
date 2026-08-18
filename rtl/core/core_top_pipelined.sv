// core_top_pipelined.sv
//
// Datapath pipelineado de 5 etapas — Fase 2 (criterio de cierre).
//
// Convierte el datapath monociclo de la Fase 1 (core_top.sv) en un
// pipeline IF-ID-EX-MEM-WB real: cinco instrucciones distintas pueden
// estar en vuelo simultaneamente, una por etapa. Reusa TODAS las
// unidades funcionales ya verificadas de la Fase 1 (regfile, alu,
// control, imm_gen, branch_unit, imem, dmem) sin modificarlas — lo
// nuevo de esta fase son los 4 registros de segmentacion mas
// forwarding_unit y hazard_detection_unit.
//
// Diferencia clave respecto a core_top.sv: aca el regfile SI usa
// ENABLE_BYPASS=1 (ver regfile.sv). En el monociclo ese bypass creaba
// un ciclo combinacional real (una instruccion completa IF-WB en un
// solo ciclo). En pipeline, WB de una instruccion y ID de OTRA
// instruccion ocurren en el mismo ciclo fisico — el bypass ahi es
// necesario y correcto, no un lazo.
//
// Politica de flush ante branch/jump (ver discusion de diseno, Fase 2):
// el branch se resuelve en EX (branch_unit + calculo de target), y para
// ese momento el pipeline ya fetcheo (IF) y decodifico (ID) dos
// instrucciones de mas asumiendo flujo secuencial. Cuando
// branch_taken||jump resulta verdadero en EX:
//   - if_id_reg recibe flush (la instruccion fetcheada especulativamente
//     en la ranura de IF/ID se invalida)
//   - id_ex_reg recibe flush (la instruccion que ya estaba siendo
//     decodificada tambien se invalida)
//   - pc_next toma el target del branch/jump en vez de pc_plus4
//
// El load-use hazard (hazard_detection_unit) y el flush de branch nunca
// compiten por las mismas señales en el mismo ciclo: el hazard se
// detecta en ID (mirando id_ex_reg) y actua sobre if_id_reg/id_ex_reg
// como STALL/flush de burbuja; el branch se resuelve en EX y actua
// sobre if_id_reg/id_ex_reg como FLUSH de instrucciones incorrectas.
// Son etapas distintas del pipeline actuando en momentos distintos del
// flujo de una instruccion — no hay superposicion posible en este diseño.

module core_top_pipelined
    import alu_pkg::*;
    import control_pkg::*;
    import forwarding_pkg::*;
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

    // =====================================================================
    // Señales de control global del pipeline (stall/flush)
    // =====================================================================
    logic pc_stall, if_id_stall, id_ex_flush_hazard;
    logic hazard_pc_stall, hazard_if_id_stall;
    logic vector_stall;
    logic branch_or_jump_taken;
    logic if_id_flush, id_ex_flush;

    // =====================================================================
    // IF — Program Counter + memoria de instrucciones
    // =====================================================================
    logic [31:0] pc, pc_next, pc_plus4, if_instr;

    assign pc_plus4 = pc + 32'd4;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n)          pc <= 32'd0;
        else if (pc_stall)   pc <= pc;           // load-use hazard: no avanzar
        else                 pc <= pc_next;
    end

    imem #(
        .ADDR_WIDTH(ADDR_WIDTH),
        .INIT_FILE (INIT_FILE)
    ) u_imem (
        .addr  (pc[ADDR_WIDTH-1:0]),
        .instr (if_instr)
    );

    // =====================================================================
    // IF/ID
    // =====================================================================
    assign if_id_flush = branch_or_jump_taken;

    logic         if_id_valid;
    logic [31:0]  if_id_instr, if_id_pc;

    if_id_reg u_if_id_reg (
        .clk       (clk),
        .rst_n     (rst_n),
        .stall     (if_id_stall),
        .flush     (if_id_flush),
        .valid_in  (1'b1),          // IF siempre produce algo valido tras reset
        .instr_in  (if_instr),
        .pc_in     (pc),
        .valid_out (if_id_valid),
        .instr_out (if_id_instr),
        .pc_out    (if_id_pc)
    );

    // =====================================================================
    // ID — decodificacion + regfile + imm_gen + hazard detection
    // =====================================================================
    logic [6:0] id_opcode;
    logic [4:0] id_rd, id_rs1, id_rs2;
    logic [2:0] id_funct3;
    logic [6:0] id_funct7;

    assign id_opcode = if_id_instr[6:0];
    assign id_rd     = if_id_instr[11:7];
    assign id_funct3 = if_id_instr[14:12];
    assign id_rs1    = if_id_instr[19:15];
    assign id_rs2    = if_id_instr[24:20];
    assign id_funct7 = if_id_instr[31:25];

    logic         id_reg_write, id_alu_src, id_mem_read, id_mem_write;
    logic         id_mem_to_reg, id_branch, id_jump;
    // verilator lint_off UNUSEDSIGNAL
    // id_illegal queda reservado para manejo de excepciones/trap en una
    // fase futura, igual que en core_top.sv (Fase 1).
    logic         id_illegal;
    // verilator lint_on UNUSEDSIGNAL
    alu_op_t      id_alu_op;
    imm_sel_t     id_imm_sel;

    control u_control (
        .opcode     (id_opcode),
        .funct3     (id_funct3),
        .funct7     (id_funct7),
        .reg_write  (id_reg_write),
        .alu_src    (id_alu_src),
        .alu_op     (id_alu_op),
        .mem_read   (id_mem_read),
        .mem_write  (id_mem_write),
        .mem_to_reg (id_mem_to_reg),
        .branch     (id_branch),
        .jump       (id_jump),
        .imm_sel    (id_imm_sel),
        .illegal    (id_illegal)
    );

    logic [31:0] id_rs1_data, id_rs2_data;
    logic [31:0] wb_data;   // definido mas abajo (WB); el regfile lo escribe

    // rd de la instruccion actualmente en WB, para el puerto de escritura.
    logic [4:0]  wb_rd;
    logic        wb_reg_write;

    regfile #(
        .ENABLE_BYPASS(1'b1)   // pipeline: bypass necesario (ver core_top.sv Fase 1)
    ) u_regfile (
        .clk    (clk),
        .we     (wb_reg_write),
        .waddr  (wb_rd),
        .wdata  (wb_data),
        .raddr1 (id_rs1),
        .raddr2 (id_rs2),
        .rdata1 (id_rs1_data),
        .rdata2 (id_rs2_data)
    );

    logic [31:0] id_imm;

    imm_gen u_imm_gen (
        .instr   (if_id_instr),
        .imm_sel (id_imm_sel),
        .imm     (id_imm)
    );

    hazard_detection_unit u_hazard_detection_unit (
        .id_ex_mem_read (idex_mem_read),
        .id_ex_rd       (idex_rd),
        .id_rs1         (id_rs1),
        .id_rs2         (id_rs2),
        .pc_stall       (hazard_pc_stall),
        .if_id_stall    (hazard_if_id_stall),
        .id_ex_flush    (id_ex_flush_hazard)
    );

    // =====================================================================
    // ID/EX
    // =====================================================================
    assign id_ex_flush = branch_or_jump_taken || id_ex_flush_hazard;

    logic         idex_valid;
    logic [31:0]  idex_pc, idex_rs1_data, idex_rs2_data, idex_imm;
    logic [4:0]   idex_rd, idex_rs1, idex_rs2;
    logic [2:0]   idex_funct3;
    logic [6:0]   idex_opcode;
    logic         idex_reg_write, idex_alu_src, idex_mem_read, idex_mem_write;
    logic         idex_mem_to_reg, idex_branch, idex_jump;
    alu_op_t      idex_alu_op;

    id_ex_reg u_id_ex_reg (
        .clk            (clk),
        .rst_n          (rst_n),
        .stall          (vector_stall),  // Fase 4: mantiene la instruccion vectorial en EX mientras no logra despachar
        .flush          (id_ex_flush),
        .valid_in       (if_id_valid),
        .pc_in          (if_id_pc),
        .rs1_data_in    (id_rs1_data),
        .rs2_data_in    (id_rs2_data),
        .imm_in         (id_imm),
        .rd_in          (id_rd),
        .rs1_in         (id_rs1),
        .rs2_in         (id_rs2),
        .funct3_in      (id_funct3),
        .opcode_in      (id_opcode),
        .reg_write_in   (id_reg_write),
        .alu_src_in     (id_alu_src),
        .alu_op_in      (id_alu_op),
        .mem_read_in    (id_mem_read),
        .mem_write_in   (id_mem_write),
        .mem_to_reg_in  (id_mem_to_reg),
        .branch_in      (id_branch),
        .jump_in        (id_jump),
        .valid_out      (idex_valid),
        .pc_out         (idex_pc),
        .rs1_data_out   (idex_rs1_data),
        .rs2_data_out   (idex_rs2_data),
        .imm_out        (idex_imm),
        .rd_out         (idex_rd),
        .rs1_out        (idex_rs1),
        .rs2_out        (idex_rs2),
        .funct3_out     (idex_funct3),
        .opcode_out     (idex_opcode),
        .reg_write_out  (idex_reg_write),
        .alu_src_out    (idex_alu_src),
        .alu_op_out     (idex_alu_op),
        .mem_read_out   (idex_mem_read),
        .mem_write_out  (idex_mem_write),
        .mem_to_reg_out (idex_mem_to_reg),
        .branch_out     (idex_branch),
        .jump_out       (idex_jump)
    );

    // =====================================================================
    // EX — forwarding + ALU + branch unit
    // =====================================================================
    fwd_sel_t fwd_a, fwd_b;

    forwarding_unit u_forwarding_unit (
        .id_ex_rs1        (idex_rs1),
        .id_ex_rs2        (idex_rs2),
        .ex_mem_rd        (exmem_rd),
        .ex_mem_reg_write (exmem_reg_write),
        .mem_wb_rd        (memwb_rd),
        .mem_wb_reg_write (memwb_reg_write),
        .fwd_a            (fwd_a),
        .fwd_b            (fwd_b)
    );

    // rs1_data/rs2_data ya forwardeados, antes de aplicar el mux de
    // operando A (PC para AUIPC, 0 para LUI) o el de operando B (alu_src).
    logic [31:0] ex_rs1_fwd, ex_rs2_fwd;

    always_comb begin
        case (fwd_a)
            FWD_EX_MEM: ex_rs1_fwd = exmem_alu_result;
            FWD_MEM_WB: ex_rs1_fwd = wb_data;   // mismo valor que WB va a escribir
            default:    ex_rs1_fwd = idex_rs1_data;
        endcase

        case (fwd_b)
            FWD_EX_MEM: ex_rs2_fwd = exmem_alu_result;
            FWD_MEM_WB: ex_rs2_fwd = wb_data;
            default:    ex_rs2_fwd = idex_rs2_data;
        endcase
    end

    // Operando A: PC para AUIPC, 0 para LUI, rs1 (forwardeado) para el resto.
    logic [31:0] alu_opA;
    always_comb begin
        case (idex_opcode)
            OP_AUIPC: alu_opA = idex_pc;
            OP_LUI:   alu_opA = 32'd0;
            default:  alu_opA = ex_rs1_fwd;
        endcase
    end

    // Operando B: rs2 (forwardeado) o inmediato, segun alu_src.
    logic [31:0] alu_opB;
    assign alu_opB = idex_alu_src ? idex_imm : ex_rs2_fwd;

    logic [31:0] ex_alu_result;
    logic         ex_alu_zero;

    alu u_alu (
        .a      (alu_opA),
        .b      (alu_opB),
        .alu_op (idex_alu_op),
        .result (ex_alu_result),
        .zero   (ex_alu_zero)
    );

    logic ex_branch_taken;

    branch_unit u_branch_unit (
        .branch       (idex_branch),
        .funct3       (idex_funct3),
        .alu_zero     (ex_alu_zero),
        .alu_result0  (ex_alu_result[0]),
        .branch_taken (ex_branch_taken)
    );

    assign branch_or_jump_taken = idex_valid && (ex_branch_taken || idex_jump);

    // Proximo PC: secuencial, branch tomado, JAL, o JALR — resuelto en EX.
    logic [31:0] ex_branch_target, ex_jalr_target, ex_pc_plus4;

    assign ex_pc_plus4      = idex_pc + 32'd4;
    assign ex_branch_target = idex_pc + idex_imm;                    // beq/bne/... y jal
    assign ex_jalr_target   = (ex_rs1_fwd + idex_imm) & ~32'd1;       // jalr: bit 0 forzado a 0

    always_comb begin
        if (idex_opcode == OP_JALR)     pc_next = ex_jalr_target;
        else if (idex_jump)             pc_next = ex_branch_target;   // jal
        else if (ex_branch_taken)       pc_next = ex_branch_target;
        else                             pc_next = pc_plus4;
    end

    // =====================================================================
    // EX — unidad vectorial (Fase 4, integracion final)
    // =====================================================================
    // Decodifica y despacha instrucciones vectoriales (opcode custom-0)
    // desde EX, reusando idex_opcode/funct3/rs1/rs2/rd y ex_rs1_fwd que
    // el pipeline escalar ya calcula — ver discusion de diseño, Fase 4:
    // vector_control vive en EX porque toda la informacion que necesita
    // (instruccion decodificada + direccion escalar ya forwardeada) ya
    // esta disponible ahi, sin necesitar decodificar de nuevo en ID ni
    // agregar campos a id_ex_reg.
    logic         is_vector_instr, vec_start;
    logic  [2:0]  vec_funct3;
    logic  [1:0]  vec_vreg_rs1, vec_vreg_rs2, vec_vreg_rd;
    logic [31:0]  vec_scalar_addr;
    logic         vector_unit_busy, vector_done;

    vector_control u_vector_control (
        .idex_valid        (idex_valid),
        .idex_opcode       (idex_opcode),
        .idex_funct3       (idex_funct3),
        .idex_rs1          (idex_rs1),
        .idex_rs2          (idex_rs2),
        .idex_rd           (idex_rd),
        .ex_rs1_fwd        (ex_rs1_fwd),
        .vector_unit_busy  (vector_unit_busy),
        .is_vector_instr   (is_vector_instr),
        .vec_start         (vec_start),
        .vec_funct3        (vec_funct3),
        .vec_vreg_rs1      (vec_vreg_rs1),
        .vec_vreg_rs2      (vec_vreg_rs2),
        .vec_vreg_rd       (vec_vreg_rd),
        .vec_scalar_addr   (vec_scalar_addr)
    );

    // Stall estructural: mientras haya una instruccion vectorial en EX
    // que todavia no logro despachar (vector_unit ocupada con una
    // operacion anterior), el pipeline entero se congela — ver
    // discusion de diseño, Fase 4. En el ciclo en que vec_start
    // efectivamente dispara, vector_unit_busy todavia refleja el valor
    // REGISTRADO del ciclo anterior (0), asi que vector_stall es 0 ese
    // mismo ciclo — la instruccion recien despachada avanza normalmente
    // hacia MEM/WB (donde no tiene efecto escalar, ya que control.sv
    // trata custom-0 como opcode inerte), y el pipeline no queda
    // congelado de mas.
    assign vector_stall = is_vector_instr && vector_unit_busy;

    // Señales de stall combinadas: hazard de load-use (Fase 2) O stall
    // estructural vectorial (Fase 4) — ver declaracion de pc_stall/
    // if_id_stall al inicio del modulo, usadas por PC e if_id_reg.
    assign pc_stall    = hazard_pc_stall    || vector_stall;
    assign if_id_stall = hazard_if_id_stall || vector_stall;

    // Interfaz hacia dmem (puerto 2, dedicado a vector_unit — ver
    // dmem.sv, extendido en la Fase 4 justamente para esto).
    logic [ADDR_WIDTH-1:0] vec_dmem_addr;
    logic [31:0]            vec_dmem_wdata;
    logic                   vec_dmem_mem_read, vec_dmem_mem_write;
    logic [2:0]             vec_dmem_funct3;
    logic [31:0]            vec_dmem_rdata;

    vector_unit u_vector_unit (
        .clk             (clk),
        .rst_n           (rst_n),
        .start           (vec_start),
        .funct3          (vec_funct3),
        .vreg_rs1        (vec_vreg_rs1),
        .vreg_rs2        (vec_vreg_rs2),
        .vreg_rd         (vec_vreg_rd),
        .scalar_addr     (vec_scalar_addr),
        .busy            (vector_unit_busy),
        .done            (vector_done),
        .dmem_addr       (vec_dmem_addr),
        .dmem_wdata      (vec_dmem_wdata),
        .dmem_mem_read   (vec_dmem_mem_read),
        .dmem_mem_write  (vec_dmem_mem_write),
        .dmem_funct3     (vec_dmem_funct3),
        .dmem_rdata      (vec_dmem_rdata)
    );

    // Scoreboard vectorial (Apendice A.4): marca ocupado el registro
    // destino al despachar (solo para operaciones que ESCRIBEN un
    // registro vectorial — vstore no escribe ninguno, su campo rd es el
    // ORIGEN, ver isa_vectorial_kyber.docx seccion 2.3), y lo libera
    // cuando vector_unit señaliza 'done'.
    //
    // 'dispatched_vreg_rd' latchea el registro destino EN EL MOMENTO del
    // despacho, para saber cual liberar cuando 'done' llegue potencialmente
    // cientos de ciclos despues — para ese entonces, vec_vreg_rd ya
    // refleja una instruccion distinta (la que este en EX en ese momento),
    // asi que no se puede usar directamente para el clear.
    localparam logic [2:0] VEC_FUNCT3_VSTORE = 3'b001;

    logic [1:0] dispatched_vreg_rd;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            dispatched_vreg_rd <= 2'd0;
        end else if (vec_start) begin
            dispatched_vreg_rd <= vec_vreg_rd;
        end
    end

    logic scoreboard_set;
    // verilator lint_off UNUSEDSIGNAL
    // vector_scoreboard_busy no tiene consumidor interno: dado el
    // gateo de recurso unico (vector_stall ya cubre la condicion de
    // stall real, ver nota de diseño en vector_scoreboard.sv), esta
    // señal existe para dar visibilidad en waveforms/documentacion
    // (Fase 6) de que registro esta ocupado y por que, no para
    // gobernar logica adicional del datapath.
    logic [3:0] vector_scoreboard_busy;
    // verilator lint_on UNUSEDSIGNAL

    assign scoreboard_set = vec_start && (vec_funct3 != VEC_FUNCT3_VSTORE);

    vector_scoreboard u_vector_scoreboard (
        .clk         (clk),
        .rst_n       (rst_n),
        .set_pulse   (scoreboard_set),
        .set_vreg    (vec_vreg_rd),
        .clear       (vector_done),
        .clear_vreg  (dispatched_vreg_rd),
        .busy        (vector_scoreboard_busy)
    );

    // =====================================================================
    // EX/MEM
    // =====================================================================
    logic         exmem_valid;
    logic [31:0]  exmem_alu_result, exmem_rs2_data, exmem_pc_plus4;
    logic [4:0]   exmem_rd;
    logic [2:0]   exmem_funct3;
    logic         exmem_reg_write, exmem_mem_read, exmem_mem_write;
    logic         exmem_mem_to_reg, exmem_jump;

    ex_mem_reg u_ex_mem_reg (
        .clk            (clk),
        .rst_n          (rst_n),
        .stall          (1'b0),
        .flush          (vector_stall),   // Fase 4: burbuja mientras la instruccion vectorial en EX no logra despachar
        .valid_in       (idex_valid),
        .alu_result_in  (ex_alu_result),
        .rs2_data_in    (ex_rs2_fwd),
        .pc_plus4_in    (ex_pc_plus4),
        .rd_in          (idex_rd),
        .funct3_in      (idex_funct3),
        .reg_write_in   (idex_reg_write),
        .mem_read_in    (idex_mem_read),
        .mem_write_in   (idex_mem_write),
        .mem_to_reg_in  (idex_mem_to_reg),
        .jump_in        (idex_jump),
        .valid_out      (exmem_valid),
        .alu_result_out (exmem_alu_result),
        .rs2_data_out   (exmem_rs2_data),
        .pc_plus4_out   (exmem_pc_plus4),
        .rd_out         (exmem_rd),
        .funct3_out     (exmem_funct3),
        .reg_write_out  (exmem_reg_write),
        .mem_read_out   (exmem_mem_read),
        .mem_write_out  (exmem_mem_write),
        .mem_to_reg_out (exmem_mem_to_reg),
        .jump_out       (exmem_jump)
    );

    // =====================================================================
    // MEM — memoria de datos
    // =====================================================================
    logic [31:0] mem_rdata;

    dmem #(
        .ADDR_WIDTH(ADDR_WIDTH)
    ) u_dmem (
        .clk        (clk),
        .addr       (exmem_alu_result[ADDR_WIDTH-1:0]),
        .wdata      (exmem_rs2_data),
        .mem_read   (exmem_mem_read),
        .mem_write  (exmem_mem_write),
        .funct3     (exmem_funct3),
        .rdata      (mem_rdata),
        // Puerto 2 (Fase 4): dedicado a vector_unit (vload/vstore),
        // independiente del puerto 1 que usa la etapa MEM del pipeline
        // escalar — ver dmem.sv para la justificacion de por que se
        // agrego un segundo puerto en vez de arbitrar uno solo.
        .addr2      (vec_dmem_addr),
        .wdata2     (vec_dmem_wdata),
        .mem_read2  (vec_dmem_mem_read),
        .mem_write2 (vec_dmem_mem_write),
        .funct3_2   (vec_dmem_funct3),
        .rdata2     (vec_dmem_rdata)
    );

    // =====================================================================
    // MEM/WB
    // =====================================================================
    // verilator lint_off UNUSEDSIGNAL
    // memwb_valid no se consulta directamente: wb_reg_write ya viene
    // forzado a 0 en burbuja por mem_wb_reg.sv, asi que gobierna el mux
    // de WB sin necesidad de chequear valid por separado.
    logic         memwb_valid;
    // verilator lint_on UNUSEDSIGNAL
    logic [31:0]  memwb_mem_rdata, memwb_alu_result, memwb_pc_plus4;
    logic [4:0]   memwb_rd;
    logic         memwb_reg_write, memwb_mem_to_reg, memwb_jump;

    mem_wb_reg u_mem_wb_reg (
        .clk            (clk),
        .rst_n          (rst_n),
        .stall          (1'b0),
        .flush          (1'b0),
        .valid_in       (exmem_valid),
        .mem_rdata_in   (mem_rdata),
        .alu_result_in  (exmem_alu_result),
        .pc_plus4_in    (exmem_pc_plus4),
        .rd_in          (exmem_rd),
        .reg_write_in   (exmem_reg_write),
        .mem_to_reg_in  (exmem_mem_to_reg),
        .jump_in        (exmem_jump),
        .valid_out      (memwb_valid),
        .mem_rdata_out  (memwb_mem_rdata),
        .alu_result_out (memwb_alu_result),
        .pc_plus4_out   (memwb_pc_plus4),
        .rd_out         (memwb_rd),
        .reg_write_out  (memwb_reg_write),
        .mem_to_reg_out (memwb_mem_to_reg),
        .jump_out       (memwb_jump)
    );

    // =====================================================================
    // WB — mux de write-back: ALU, memoria, o PC+4 (para JAL/JALR)
    // =====================================================================
    always_comb begin
        if (memwb_jump)             wb_data = memwb_pc_plus4;
        else if (memwb_mem_to_reg)  wb_data = memwb_mem_rdata;
        else                         wb_data = memwb_alu_result;
    end

    assign wb_rd        = memwb_rd;
    assign wb_reg_write = memwb_reg_write;

endmodule
