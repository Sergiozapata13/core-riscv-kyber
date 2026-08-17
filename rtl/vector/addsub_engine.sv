// addsub_engine.sv
//
// Motor de suma/resta — Fase 4 (vadd/vsub), FSM secuencial.
//
// El mas simple de los motores de la unidad vectorial: recorre los 256
// coeficientes de dos registros de entrada (vreg_a, vreg_b), aplicando
// poly_addsub coeficiente a coeficiente, sin ningun patron especial de
// emparejamiento (a diferencia de NTT o vpmul). Como cada operacion solo
// necesita 1 coeficiente de A y 1 de B, y vreg_file tiene 2 puertos de
// lectura, se puede leer ambos operandos en el MISMO ciclo (sin
// necesitar el truco de 2 sub-ciclos que usa vpmul_engine.sv) — 256
// ciclos totales para un polinomio completo.

module addsub_engine (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         start,
    input  logic         is_sub,     // 0 = vadd, 1 = vsub
    input  logic  [1:0]  vreg_a,
    input  logic  [1:0]  vreg_b,
    input  logic  [1:0]  vreg_dst,

    output logic         busy,
    output logic         done,

    output logic  [1:0]  vreg_raddr1_vreg,
    output logic  [7:0]  vreg_raddr1_coef,
    // verilator lint_off UNUSEDSIGNAL
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
    output logic [15:0]  vreg_wdata1
);

    typedef enum logic [1:0] {
        IDLE,
        RUN,
        DONE_PULSE
    } state_t;

    state_t state;

    logic [7:0] idx;   // 0-255
    logic       is_sub_latched;
    logic [1:0] vreg_a_latched, vreg_b_latched, vreg_dst_latched;

    always_comb begin
        vreg_raddr1_vreg = vreg_a_latched;
        vreg_raddr1_coef = idx;
        vreg_raddr2_vreg = vreg_b_latched;
        vreg_raddr2_coef = idx;

        vreg_waddr1_vreg = vreg_dst_latched;
        vreg_waddr1_coef = idx;

        vreg_we1 = (state == RUN);
        vreg_wdata1 = {4'd0, addsub_result};
    end

    logic [11:0] addsub_result;

    poly_addsub u_poly_addsub (
        .a       (vreg_rdata1[11:0]),
        .b       (vreg_rdata2[11:0]),
        .is_sub  (is_sub_latched),
        .result  (addsub_result)
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
                        is_sub_latched   <= is_sub;
                        vreg_a_latched   <= vreg_a;
                        vreg_b_latched   <= vreg_b;
                        vreg_dst_latched <= vreg_dst;
                        busy  <= 1'b1;
                        state <= RUN;
                        idx   <= 8'd0;
                    end
                end

                RUN: begin
                    // El resultado de este ciclo ya se escribio
                    // combinacionalmente (vreg_we1, ver always_comb).
                    if (idx == 8'd255) begin
                        state <= DONE_PULSE;
                    end else begin
                        idx <= idx + 1;
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
