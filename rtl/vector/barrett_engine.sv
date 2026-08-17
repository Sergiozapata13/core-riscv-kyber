// barrett_engine.sv
//
// Motor de reduccion modular Barrett — Fase 4 (vbarrett), FSM secuencial.
//
// El mas simple de todos los motores: recorre los 256 coeficientes de un
// registro de entrada (vreg_src), aplicando barrett_reduce directamente
// (ya verificado con 412 casos en la Fase 4), y escribe el resultado en
// vreg_dst. Solo necesita 1 puerto de lectura y 1 de escritura — no hay
// segundo operando como en vadd/vsub/vpmul.
//
// Implementa exactamente kyber_ref.barrett_reduce_poly() (ya validado
// en la Fase 3): [barrett_reduce(c) for c in poly].

module barrett_engine (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         start,
    input  logic  [1:0]  vreg_src,
    input  logic  [1:0]  vreg_dst,

    output logic         busy,
    output logic         done,

    output logic  [1:0]  vreg_raddr1_vreg,
    output logic  [7:0]  vreg_raddr1_coef,
    input  logic [15:0]  vreg_rdata1,

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

    logic [7:0] idx;
    logic [1:0] vreg_src_latched, vreg_dst_latched;

    always_comb begin
        vreg_raddr1_vreg = vreg_src_latched;
        vreg_raddr1_coef = idx;

        vreg_waddr1_vreg = vreg_dst_latched;
        vreg_waddr1_coef = idx;

        vreg_we1    = (state == RUN);
        vreg_wdata1 = {4'd0, barrett_result};
    end

    logic [11:0] barrett_result;

    barrett_reduce u_barrett_reduce (
        .a      ({{16{1'b0}}, vreg_rdata1}),
        .result (barrett_result)
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
                        vreg_src_latched <= vreg_src;
                        vreg_dst_latched <= vreg_dst;
                        busy  <= 1'b1;
                        state <= RUN;
                        idx   <= 8'd0;
                    end
                end

                RUN: begin
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
