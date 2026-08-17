// barrett_engine_top.sv
//
// Wrapper que conecta barrett_engine con vreg_file — Fase 4.
// Mismo patron que addsub_engine_top.sv, pero con un solo puerto de
// lectura/escritura del motor (vbarrett solo tiene un operando).

module barrett_engine_top (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         start,
    input  logic  [1:0]  vreg_src,
    input  logic  [1:0]  vreg_dst,

    output logic         busy,
    output logic         done,

    input  logic  [1:0]  ext_addr_vreg,
    input  logic  [7:0]  ext_addr_coef,
    input  logic         ext_we,
    input  logic [15:0]  ext_wdata,
    output logic [15:0]  ext_rdata
);

    logic  [1:0] eng_raddr1_vreg, eng_waddr1_vreg;
    logic  [7:0] eng_raddr1_coef, eng_waddr1_coef;
    logic [15:0] eng_rdata1;
    logic        eng_we1;
    logic [15:0] eng_wdata1;

    barrett_engine u_barrett_engine (
        .clk               (clk),
        .rst_n             (rst_n),
        .start             (start),
        .vreg_src          (vreg_src),
        .vreg_dst          (vreg_dst),
        .busy              (busy),
        .done              (done),
        .vreg_raddr1_vreg  (eng_raddr1_vreg),
        .vreg_raddr1_coef  (eng_raddr1_coef),
        .vreg_rdata1       (eng_rdata1),
        .vreg_we1          (eng_we1),
        .vreg_waddr1_vreg  (eng_waddr1_vreg),
        .vreg_waddr1_coef  (eng_waddr1_coef),
        .vreg_wdata1       (eng_wdata1)
    );

    logic [1:0] mux_raddr1_vreg;
    logic [7:0] mux_raddr1_coef;

    assign mux_raddr1_vreg = busy ? eng_raddr1_vreg : ext_addr_vreg;
    assign mux_raddr1_coef = busy ? eng_raddr1_coef : ext_addr_coef;
    assign ext_rdata = eng_rdata1;

    logic [15:0] rdata2_unused;

    vreg_file u_vreg_file (
        .clk          (clk),
        .raddr1_vreg  (mux_raddr1_vreg),
        .raddr1_coef  (mux_raddr1_coef),
        .rdata1       (eng_rdata1),
        .raddr2_vreg  (2'd0),
        .raddr2_coef  (8'd0),
        .rdata2       (rdata2_unused),
        .we1          (busy ? eng_we1 : ext_we),
        .waddr1_vreg  (busy ? eng_waddr1_vreg : ext_addr_vreg),
        .waddr1_coef  (busy ? eng_waddr1_coef : ext_addr_coef),
        .wdata1       (busy ? eng_wdata1 : ext_wdata),
        .we2          (1'b0),
        .waddr2_vreg  (2'd0),
        .waddr2_coef  (8'd0),
        .wdata2       (16'd0)
    );

endmodule
