// vpmul_engine.sv
//
// Motor de multiplicacion punto a punto — Fase 4 (vpmul), FSM secuencial.
//
// Orquesta base_case_mul a traves de los 64 grupos de 4 coeficientes
// (256 coeficientes = 64 grupos x 4) que especifica
// kyber_ref.poly_pointwise_mul() (ya validado en la Fase 3), leyendo de
// dos registros vectoriales de entrada (vreg_a, vreg_b) y escribiendo en
// uno de salida (vreg_dst).
//
// Restriccion de puertos: vreg_file solo tiene 2 puertos de lectura,
// pero cada base_case_mul necesita 4 operandos de entrada (a0,a1 del
// registro A; b0,b1 del registro B). Se resuelve leyendo en 2 sub-ciclos
// por base case (READ_A: leer a0,a1; READ_B_RUN: leer b0,b1 y computar+
// escribir en el mismo ciclo, ya que para ese momento a0/a1 ya estan
// registrados) — mismo criterio de "simple y verificable antes que
// rapido" ya aplicado en ntt_engine.sv (1 butterfly/ciclo en vez de
// paralelizar). 2 base cases por grupo x 2 sub-ciclos x 64 grupos = 256
// ciclos totales, similar en orden de magnitud a una NTT completa.
//
// Patron identico a kyber_ref.poly_pointwise_mul():
//   para i en 0..63:
//     zeta = ZETAS[64+i]
//     base_case(a[4i+0],a[4i+1], b[4i+0],b[4i+1], zeta)      -> r[4i+0],r[4i+1]
//     base_case(a[4i+2],a[4i+3], b[4i+2],b[4i+3], Q-zeta)    -> r[4i+2],r[4i+3]
//
// El indice de twiddle que se pasa a twiddle_rom (fuera de este modulo)
// es 64+i — el offset +64 se suma en la conexion externa (ver
// vpmul_engine_top.sv), no dentro de este modulo, para mantener el
// generador de direcciones de twiddle simple.

module vpmul_engine (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         start,
    input  logic  [1:0]  vreg_a,
    input  logic  [1:0]  vreg_b,
    input  logic  [1:0]  vreg_dst,

    output logic         busy,
    output logic         done,

    output logic  [1:0]  vreg_raddr1_vreg,
    output logic  [7:0]  vreg_raddr1_coef,
    // verilator lint_off UNUSEDSIGNAL
    // Bits altos no usados: coeficientes de Kyber caben en 12 bits
    // (q=3329<4096), el bus fisico del vreg_file es de 16 bits.
    input  logic [15:0]  vreg_rdata1,
    // verilator lint_on UNUSEDSIGNAL

    output logic  [1:0]  vreg_raddr2_vreg,
    output logic  [7:0]  vreg_raddr2_coef,
    // verilator lint_off UNUSEDSIGNAL
    input  logic [15:0]  vreg_rdata2,
    // verilator lint_on UNUSEDSIGNAL

    output logic         vreg_we1,
    output logic  [1:0]  vreg_waddr1_vreg,
    output logic  [7:0]  vreg_waddr1_coef,
    output logic [15:0]  vreg_wdata1,

    output logic         vreg_we2,
    output logic  [1:0]  vreg_waddr2_vreg,
    output logic  [7:0]  vreg_waddr2_coef,
    output logic [15:0]  vreg_wdata2,

    output logic  [5:0]  twiddle_i,      // 0-63; sumar 64 externamente para indexar ZETAS
    input  logic [11:0]  twiddle_zeta
);

    localparam logic [11:0] Q = 12'd3329;

    typedef enum logic [1:0] {
        IDLE,
        READ_A,
        READ_B_RUN,
        DONE_PULSE
    } state_t;

    state_t state;

    logic [5:0] i;
    logic       sub_pair;

    logic [1:0] vreg_a_latched, vreg_b_latched, vreg_dst_latched;
    logic [11:0] a0_reg, a1_reg;

    logic [7:0] idx0, idx1;
    assign idx0 = {i, sub_pair, 1'b0};
    assign idx1 = {i, sub_pair, 1'b1};

    logic [11:0] zeta_effective;
    assign zeta_effective = sub_pair ? (Q - twiddle_zeta) : twiddle_zeta;

    assign twiddle_i = i;

    always_comb begin
        vreg_raddr1_vreg = vreg_a_latched;
        vreg_raddr1_coef = idx0;
        vreg_raddr2_vreg = vreg_a_latched;
        vreg_raddr2_coef = idx1;

        vreg_waddr1_vreg = vreg_dst_latched;
        vreg_waddr1_coef = idx0;
        vreg_waddr2_vreg = vreg_dst_latched;
        vreg_waddr2_coef = idx1;

        vreg_we1 = 1'b0;
        vreg_we2 = 1'b0;
        vreg_wdata1 = 16'd0;
        vreg_wdata2 = 16'd0;

        if (state == READ_B_RUN) begin
            vreg_raddr1_vreg = vreg_b_latched;
            vreg_raddr1_coef = idx0;
            vreg_raddr2_vreg = vreg_b_latched;
            vreg_raddr2_coef = idx1;

            vreg_we1 = 1'b1;
            vreg_we2 = 1'b1;
            vreg_wdata1 = {4'd0, bcm_c0};
            vreg_wdata2 = {4'd0, bcm_c1};
        end
    end

    logic [11:0] bcm_c0, bcm_c1;

    base_case_mul u_base_case_mul (
        .a0    (a0_reg),
        .a1    (a1_reg),
        .b0    (vreg_rdata1[11:0]),
        .b1    (vreg_rdata2[11:0]),
        .zeta  (zeta_effective),
        .c0    (bcm_c0),
        .c1    (bcm_c1)
    );

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= IDLE;
            busy  <= 1'b0;
            done  <= 1'b0;
        end else begin
            done <= 1'b0;

            case (state)
                IDLE: begin
                    busy <= 1'b0;
                    if (start) begin
                        vreg_a_latched   <= vreg_a;
                        vreg_b_latched   <= vreg_b;
                        vreg_dst_latched <= vreg_dst;
                        busy  <= 1'b1;
                        state <= READ_A;
                        i        <= 6'd0;
                        sub_pair <= 1'b0;
                    end
                end

                READ_A: begin
                    a0_reg <= vreg_rdata1[11:0];
                    a1_reg <= vreg_rdata2[11:0];
                    state  <= READ_B_RUN;
                end

                READ_B_RUN: begin
                    if (!sub_pair) begin
                        sub_pair <= 1'b1;
                        state    <= READ_A;
                    end else begin
                        sub_pair <= 1'b0;
                        if (i == 6'd63) begin
                            state <= DONE_PULSE;
                        end else begin
                            i     <= i + 1;
                            state <= READ_A;
                        end
                    end
                end

                DONE_PULSE: begin
                    done  <= 1'b1;
                    busy  <= 1'b0;
                    state <= IDLE;
                end

                default: state <= IDLE;
            endcase
        end
    end

endmodule
