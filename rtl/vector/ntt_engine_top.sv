// ntt_engine_top.sv
//
// Wrapper que conecta ntt_engine con vreg_file y twiddle_rom — Fase 4.
//
// ntt_engine, vreg_file y twiddle_rom se disenaron como modulos
// independientes (cada uno verificado por separado). Este wrapper los
// une para poder correr una NTT/INTT real de punta a punta en el
// testbench, y sirve tambien de referencia de como se conectan dentro
// de la unidad vectorial completa (integracion final de la Fase 4).
//
// Expone acceso directo a los puertos del vreg_file para que el
// testbench pueda cargar un polinomio de entrada antes de start=1 y leer
// el resultado despues de done=1 — en la integracion final, esos mismos
// puertos los va a usar la logica de vload/vstore.

module ntt_engine_top (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         start,
    input  logic         mode_intt,
    input  logic  [1:0]  vreg_src,
    input  logic  [1:0]  vreg_dst,

    output logic         busy,
    output logic         done,

    // Acceso externo al banco vectorial (para cargar/leer polinomios de
    // prueba desde el testbench) — puerto compartido con el de lectura 1
    // del motor cuando esta libre (busy=0), y puerto de escritura 1
    // compartido de la misma forma.
    input  logic  [1:0]  ext_addr_vreg,
    input  logic  [7:0]  ext_addr_coef,
    input  logic         ext_we,
    input  logic [15:0]  ext_wdata,
    output logic [15:0]  ext_rdata
);

    logic  [1:0] eng_raddr1_vreg, eng_raddr2_vreg, eng_waddr1_vreg, eng_waddr2_vreg;
    logic  [7:0] eng_raddr1_coef, eng_raddr2_coef, eng_waddr1_coef, eng_waddr2_coef;
    logic [15:0] eng_rdata1, eng_rdata2;
    logic        eng_we1, eng_we2;
    logic [15:0] eng_wdata1, eng_wdata2;

    logic  [6:0] twiddle_k;
    logic [11:0] twiddle_zeta, twiddle_zeta_inv;

    ntt_engine u_ntt_engine (
        .clk               (clk),
        .rst_n             (rst_n),
        .start             (start),
        .mode_intt         (mode_intt),
        .vreg_src          (vreg_src),
        .vreg_dst          (vreg_dst),
        .busy              (busy),
        .done              (done),
        .vreg_raddr1_vreg  (eng_raddr1_vreg),
        .vreg_raddr1_coef  (eng_raddr1_coef),
        .vreg_rdata1       (eng_rdata1),
        .vreg_raddr2_vreg  (eng_raddr2_vreg),
        .vreg_raddr2_coef  (eng_raddr2_coef),
        .vreg_rdata2       (eng_rdata2),
        .vreg_we1          (eng_we1),
        .vreg_waddr1_vreg  (eng_waddr1_vreg),
        .vreg_waddr1_coef  (eng_waddr1_coef),
        .vreg_wdata1       (eng_wdata1),
        .vreg_we2          (eng_we2),
        .vreg_waddr2_vreg  (eng_waddr2_vreg),
        .vreg_waddr2_coef  (eng_waddr2_coef),
        .vreg_wdata2       (eng_wdata2),
        .twiddle_k         (twiddle_k),
        .twiddle_zeta      (twiddle_zeta),
        .twiddle_zeta_inv  (twiddle_zeta_inv)
    );

    twiddle_rom u_twiddle_rom (
        .k         (twiddle_k),
        .zeta      (twiddle_zeta),
        .zeta_inv  (twiddle_zeta_inv)
    );

    // Acceso externo comparte el puerto de lectura/escritura 1 del motor
    // cuando este no esta ocupado (busy=0) — vreg_file solo tiene 2
    // puertos de cada tipo, y el motor usa ambos durante RUN/SCALE.
    logic [1:0] mux_raddr1_vreg;
    logic [7:0] mux_raddr1_coef;

    assign mux_raddr1_vreg = busy ? eng_raddr1_vreg : ext_addr_vreg;
    assign mux_raddr1_coef = busy ? eng_raddr1_coef : ext_addr_coef;
    assign ext_rdata = eng_rdata1;

    vreg_file u_vreg_file (
        .clk          (clk),
        .raddr1_vreg  (mux_raddr1_vreg),
        .raddr1_coef  (mux_raddr1_coef),
        .rdata1       (eng_rdata1),
        .raddr2_vreg  (eng_raddr2_vreg),
        .raddr2_coef  (eng_raddr2_coef),
        .rdata2       (eng_rdata2),
        .we1          (busy ? eng_we1 : ext_we),
        .waddr1_vreg  (busy ? eng_waddr1_vreg : ext_addr_vreg),
        .waddr1_coef  (busy ? eng_waddr1_coef : ext_addr_coef),
        .wdata1       (busy ? eng_wdata1 : ext_wdata),
        .we2          (eng_we2),
        .waddr2_vreg  (eng_waddr2_vreg),
        .waddr2_coef  (eng_waddr2_coef),
        .wdata2       (eng_wdata2)
    );

endmodule
