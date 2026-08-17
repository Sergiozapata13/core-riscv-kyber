// vload_vstore_engine.sv
//
// Motor de vload/vstore — Fase 4, FSM secuencial.
//
// Mueve un polinomio completo (256 coeficientes de 16 bits) entre la
// memoria de datos escalar (dmem.sv, Fase 1) y un registro vectorial,
// segun la variante MEMORIA de isa_vectorial_kyber.docx seccion 2.3:
// rs1 (registro escalar) trae la direccion base; rd[1:0] selecciona el
// registro vectorial destino (vload) u origen (vstore).
//
// Acceso a dmem en HALFWORDS consecutivos (funct3=001 para sh / 101
// para lhu — ver green card): cada coeficiente Kyber es de 16 bits, sin
// signo (vive en [0,q), q=3329), asi que se usa lhu para la carga
// (zero-extend) y sh para el guardado. Direcciones consecutivas de a 2
// bytes: coeficiente i vive en memoria en addr_base + 2*i.
//
// 256 ciclos por operacion completa (1 coeficiente/ciclo) — mismo
// criterio de "simple antes que rapido" que el resto de los motores de
// esta fase.

module vload_vstore_engine (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         start,
    input  logic         is_store,     // 0 = vload, 1 = vstore
    input  logic [31:0]  addr_base,    // direccion base (desde rs1 escalar)
    input  logic  [1:0]  vreg,         // registro vectorial destino/origen

    output logic         busy,
    output logic         done,

    // Interfaz hacia dmem (instanciada fuera de este modulo, en la
    // integracion final con el pipeline escalar — Fase 4)
    output logic [15:0]  dmem_addr,
    output logic [31:0]  dmem_wdata,
    output logic         dmem_mem_read,
    output logic         dmem_mem_write,
    output logic  [2:0]  dmem_funct3,
    // verilator lint_off UNUSEDSIGNAL
    // Solo los 16 bits bajos de dmem_rdata importan: dmem devuelve una
    // palabra de 32 bits, pero el acceso es lhu (halfword sin signo,
    // ya zero-extendido dentro de dmem.sv) — los bits altos son 0.
    input  logic [31:0]  dmem_rdata,
    // verilator lint_on UNUSEDSIGNAL

    // Interfaz hacia vreg_file
    output logic  [1:0]  vreg_raddr1_vreg,
    output logic  [7:0]  vreg_raddr1_coef,
    input  logic [15:0]  vreg_rdata1,

    output logic         vreg_we1,
    output logic  [1:0]  vreg_waddr1_vreg,
    output logic  [7:0]  vreg_waddr1_coef,
    output logic [15:0]  vreg_wdata1
);

    localparam logic [2:0] FUNCT3_LHU = 3'b101;  // ver green card: lhu, unsigned halfword
    localparam logic [2:0] FUNCT3_SH  = 3'b001;  // sh, halfword store

    typedef enum logic [1:0] {
        IDLE,
        RUN,
        DONE_PULSE
    } state_t;

    state_t state;

    logic [7:0]  idx;              // 0-255
    logic        is_store_latched;
    logic [1:0]  vreg_latched;
    logic [31:0] addr_base_latched;

    // verilator lint_off UNUSEDSIGNAL
    // Solo los 16 bits bajos de coef_addr importan: dmem.sv tiene un
    // espacio de direcciones de 16 bits (ADDR_WIDTH=16, 64KB) — los
    // bits altos de la suma addr_base+2*idx se descartan al truncar a
    // dmem_addr[15:0] mas abajo.
    logic [31:0] coef_addr;
    // verilator lint_on UNUSEDSIGNAL
    assign coef_addr = addr_base_latched + {23'd0, idx, 1'b0};  // addr_base + 2*idx

    always_comb begin
        vreg_raddr1_vreg = vreg_latched;
        vreg_raddr1_coef = idx;

        vreg_waddr1_vreg = vreg_latched;
        vreg_waddr1_coef = idx;

        dmem_addr   = coef_addr[15:0];
        dmem_funct3 = is_store_latched ? FUNCT3_SH : FUNCT3_LHU;

        vreg_we1       = 1'b0;
        vreg_wdata1    = 16'd0;
        dmem_mem_read  = 1'b0;
        dmem_mem_write = 1'b0;
        dmem_wdata     = 32'd0;

        if (state == RUN) begin
            if (is_store_latched) begin
                // vstore: leer del vreg_file (async), escribir a dmem (sync)
                dmem_mem_write = 1'b1;
                dmem_wdata     = {16'd0, vreg_rdata1};
            end else begin
                // vload: leer de dmem (async), escribir al vreg_file (sync)
                dmem_mem_read = 1'b1;
                vreg_we1      = 1'b1;
                vreg_wdata1   = dmem_rdata[15:0];
            end
        end
    end

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
                        is_store_latched  <= is_store;
                        vreg_latched      <= vreg;
                        addr_base_latched <= addr_base;
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
