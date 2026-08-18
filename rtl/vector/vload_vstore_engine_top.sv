// vload_vstore_engine_top.sv
//
// Wrapper que conecta vload_vstore_engine con dmem (memoria escalar,
// Fase 1) y vreg_file (banco vectorial, Fase 4) — para testbench y
// referencia de integracion final.
//
// A diferencia de los otros *_engine_top.sv, aca dmem YA es la memoria
// escalar real (no un modelo de prueba nuevo) — es la misma dmem.sv que
// usa el pipeline. El acceso externo (ext_*) permite al testbench
// escribir/leer memoria directamente para preparar/verificar datos.

module vload_vstore_engine_top (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         start,
    input  logic         is_store,
    input  logic [31:0]  addr_base,
    input  logic  [1:0]  vreg,

    output logic         busy,
    output logic         done,

    // Acceso externo a dmem (para cargar/verificar memoria desde el testbench)
    input  logic [15:0]  ext_dmem_addr,
    input  logic [31:0]  ext_dmem_wdata,
    input  logic         ext_dmem_we,
    output logic [31:0]  ext_dmem_rdata,

    // Acceso externo al vreg_file (para verificar el resultado de vload)
    input  logic  [1:0]  ext_vreg_addr_vreg,
    input  logic  [7:0]  ext_vreg_addr_coef,
    input  logic         ext_vreg_we,
    input  logic [15:0]  ext_vreg_wdata,
    output logic [15:0]  ext_vreg_rdata
);

    logic [15:0] eng_dmem_addr;
    logic [31:0] eng_dmem_wdata;
    logic        eng_dmem_mem_read, eng_dmem_mem_write;
    logic  [2:0] eng_dmem_funct3;
    logic [31:0] dmem_rdata_shared;

    logic  [1:0] eng_vreg_raddr1_vreg, eng_vreg_waddr1_vreg;
    logic  [7:0] eng_vreg_raddr1_coef, eng_vreg_waddr1_coef;
    logic [15:0] eng_vreg_rdata1;
    logic        eng_vreg_we1;
    logic [15:0] eng_vreg_wdata1;

    vload_vstore_engine u_vload_vstore_engine (
        .clk               (clk),
        .rst_n             (rst_n),
        .start             (start),
        .is_store          (is_store),
        .addr_base         (addr_base),
        .vreg              (vreg),
        .busy              (busy),
        .done              (done),
        .dmem_addr         (eng_dmem_addr),
        .dmem_wdata        (eng_dmem_wdata),
        .dmem_mem_read     (eng_dmem_mem_read),
        .dmem_mem_write    (eng_dmem_mem_write),
        .dmem_funct3       (eng_dmem_funct3),
        .dmem_rdata        (dmem_rdata_shared),
        .vreg_raddr1_vreg  (eng_vreg_raddr1_vreg),
        .vreg_raddr1_coef  (eng_vreg_raddr1_coef),
        .vreg_rdata1       (eng_vreg_rdata1),
        .vreg_we1          (eng_vreg_we1),
        .vreg_waddr1_vreg  (eng_vreg_waddr1_vreg),
        .vreg_waddr1_coef  (eng_vreg_waddr1_coef),
        .vreg_wdata1       (eng_vreg_wdata1)
    );

    // ---- Mux de acceso a dmem: motor cuando busy, externo cuando no ----
    logic [15:0] mux_dmem_addr;
    logic [31:0] mux_dmem_wdata;
    logic        mux_dmem_we, mux_dmem_re;
    logic  [2:0] mux_dmem_funct3;

    localparam logic [2:0] FUNCT3_LW = 3'b010;

    assign mux_dmem_addr   = busy ? eng_dmem_addr   : ext_dmem_addr;
    assign mux_dmem_wdata  = busy ? eng_dmem_wdata  : ext_dmem_wdata;
    assign mux_dmem_we     = busy ? eng_dmem_mem_write : ext_dmem_we;
    assign mux_dmem_re     = busy ? eng_dmem_mem_read  : 1'b1;  // lectura externa siempre habilitada (behavioral, sin costo)
    assign mux_dmem_funct3 = busy ? eng_dmem_funct3 : FUNCT3_LW;
    assign ext_dmem_rdata  = dmem_rdata_shared;

    logic [31:0] dmem_port2_rdata_unused;

    dmem #(
        .ADDR_WIDTH(16)
    ) u_dmem (
        .clk        (clk),
        .addr       (mux_dmem_addr),
        .wdata      (mux_dmem_wdata),
        .mem_read   (mux_dmem_re),
        .mem_write  (mux_dmem_we),
        .funct3     (mux_dmem_funct3),
        .rdata      (dmem_rdata_shared),
        // Puerto 2: sin uso en este wrapper de testing (sigue usando el
        // mux interno del puerto 1, ya verificado en la Fase 4) — el
        // puerto 2 real de dmem se aprovecha en la integracion final
        // (core_top_pipelined.sv), no en este wrapper aislado.
        .addr2      (16'd0),
        .wdata2     (32'd0),
        .mem_read2  (1'b0),
        .mem_write2 (1'b0),
        .funct3_2   (3'd0),
        .rdata2     (dmem_port2_rdata_unused)
    );

    // ---- Mux de acceso a vreg_file: motor cuando busy, externo cuando no ----
    logic [1:0] mux_vreg_addr_vreg;
    logic [7:0] mux_vreg_addr_coef;
    logic [15:0] rdata2_unused;

    assign mux_vreg_addr_vreg = busy ? eng_vreg_raddr1_vreg : ext_vreg_addr_vreg;
    assign mux_vreg_addr_coef = busy ? eng_vreg_raddr1_coef : ext_vreg_addr_coef;
    assign ext_vreg_rdata     = eng_vreg_rdata1;

    vreg_file u_vreg_file (
        .clk          (clk),
        .raddr1_vreg  (mux_vreg_addr_vreg),
        .raddr1_coef  (mux_vreg_addr_coef),
        .rdata1       (eng_vreg_rdata1),
        .raddr2_vreg  (2'd0),
        .raddr2_coef  (8'd0),
        .rdata2       (rdata2_unused),
        .we1          (busy ? eng_vreg_we1 : ext_vreg_we),
        .waddr1_vreg  (busy ? eng_vreg_waddr1_vreg : ext_vreg_addr_vreg),
        .waddr1_coef  (busy ? eng_vreg_waddr1_coef : ext_vreg_addr_coef),
        .wdata1       (busy ? eng_vreg_wdata1 : ext_vreg_wdata),
        .we2          (1'b0),
        .waddr2_vreg  (2'd0),
        .waddr2_coef  (8'd0),
        .wdata2       (16'd0)
    );

endmodule
