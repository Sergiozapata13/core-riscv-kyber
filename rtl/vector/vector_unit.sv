// vector_unit.sv
//
// Unidad vectorial completa — Fase 4 (integracion final).
//
// Integra los 5 motores ya verificados por separado (ntt_engine,
// vpmul_engine, addsub_engine, barrett_engine, vload_vstore_engine)
// compartiendo UN SOLO vreg_file y UN SOLO twiddle_rom — a diferencia de
// los *_engine_top.sv de testing, que cada uno instanciaba su propia
// copia de estos recursos para poder probarse aislado.
//
// Selector de operacion: funct3, tal como lo define
// isa_vectorial_kyber.docx seccion 3 — no hace falta traducir nada entre
// el decodificador (vector_control.sv, siguiente paso) y esta unidad:
//   000 vload   001 vstore   010 vntt   011 vintt
//   100 vpmul   101 vbarrett 110 vadd   111 vsub
//
// Coherente con la decision A.1/A.4 del Apendice A: la unidad vectorial
// es un RECURSO UNICO no pipelineado — solo un motor puede estar activo
// a la vez, asi que el mux de vreg_file/twiddle_rom/dmem se resuelve
// simplemente seleccionando las señales del motor activo (latcheado al
// disparar 'start'), sin necesidad de arbitraje complejo.

module vector_unit (
    input  logic         clk,
    input  logic         rst_n,

    // ---- Interfaz de despacho (desde vector_control.sv) ----
    input  logic         start,
    input  logic  [2:0]  funct3,       // selector de operacion, ver arriba
    input  logic  [1:0]  vreg_rs1,     // registro vectorial de entrada A
    input  logic  [1:0]  vreg_rs2,     // registro vectorial de entrada B (vpmul/vadd/vsub)
    input  logic  [1:0]  vreg_rd,      // registro vectorial destino
    input  logic [31:0]  scalar_addr,  // direccion de memoria (vload/vstore, desde rs1 escalar)

    output logic         busy,
    output logic         done,

    // ---- Interfaz hacia dmem (memoria escalar compartida con el pipeline) ----
    output logic [15:0]  dmem_addr,
    output logic [31:0]  dmem_wdata,
    output logic         dmem_mem_read,
    output logic         dmem_mem_write,
    output logic  [2:0]  dmem_funct3,
    input  logic [31:0]  dmem_rdata
);

    // Códigos de funct3 (ver isa_vectorial_kyber.docx seccion 3)
    localparam logic [2:0] OP_VLOAD   = 3'b000;
    localparam logic [2:0] OP_VSTORE  = 3'b001;
    localparam logic [2:0] OP_VNTT    = 3'b010;
    localparam logic [2:0] OP_VINTT   = 3'b011;
    localparam logic [2:0] OP_VPMUL   = 3'b100;
    localparam logic [2:0] OP_VBARRETT= 3'b101;
    localparam logic [2:0] OP_VADD    = 3'b110;
    localparam logic [2:0] OP_VSUB    = 3'b111;

    // Se latchea que operacion esta activa al disparar start, para que
    // el mux de recursos compartidos sea estable durante toda la
    // duracion de la operacion (no solo el ciclo de start).
    logic [2:0] active_op;
    logic       any_busy;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            active_op <= OP_VLOAD;
        end else if (start && !any_busy) begin
            active_op <= funct3;
        end
    end

    // ------------------------------------------------------------------
    // Señales de start individuales: solo la que corresponde a funct3
    // recibe el pulso. Se bloquea un nuevo start si ya hay una operacion
    // en curso (any_busy) — coherente con "recurso unico no pipelineado".
    // ------------------------------------------------------------------
    logic start_ntt, start_vpmul, start_addsub, start_barrett, start_loadstore;
    logic gated_start;
    assign gated_start = start && !any_busy;

    assign start_ntt      = gated_start && (funct3 == OP_VNTT || funct3 == OP_VINTT);
    assign start_vpmul    = gated_start && (funct3 == OP_VPMUL);
    assign start_addsub   = gated_start && (funct3 == OP_VADD || funct3 == OP_VSUB);
    assign start_barrett  = gated_start && (funct3 == OP_VBARRETT);
    assign start_loadstore= gated_start && (funct3 == OP_VLOAD || funct3 == OP_VSTORE);

    // ------------------------------------------------------------------
    // Instancias de los 5 motores — cada uno con sus puertos de vreg_file
    // propios, que se muxean mas abajo hacia el vreg_file unico.
    // ------------------------------------------------------------------
    logic [15:0] vreg_rdata1_shared, vreg_rdata2_shared;
    logic [11:0] twiddle_zeta_shared, twiddle_zeta_inv_shared;

    logic ntt_busy, ntt_done;
    logic  [1:0] ntt_raddr1_vreg, ntt_raddr2_vreg, ntt_waddr1_vreg, ntt_waddr2_vreg;
    logic  [7:0] ntt_raddr1_coef, ntt_raddr2_coef, ntt_waddr1_coef, ntt_waddr2_coef;
    logic        ntt_we1, ntt_we2;
    logic [15:0] ntt_wdata1, ntt_wdata2;
    logic  [6:0] ntt_twiddle_k;

    ntt_engine u_ntt_engine (
        .clk               (clk),
        .rst_n             (rst_n),
        .start             (start_ntt),
        .mode_intt         (funct3 == OP_VINTT),
        .vreg_src          (vreg_rs1),
        .vreg_dst          (vreg_rd),
        .busy              (ntt_busy),
        .done              (ntt_done),
        .vreg_raddr1_vreg  (ntt_raddr1_vreg),
        .vreg_raddr1_coef  (ntt_raddr1_coef),
        .vreg_rdata1       (vreg_rdata1_shared),
        .vreg_raddr2_vreg  (ntt_raddr2_vreg),
        .vreg_raddr2_coef  (ntt_raddr2_coef),
        .vreg_rdata2       (vreg_rdata2_shared),
        .vreg_we1          (ntt_we1),
        .vreg_waddr1_vreg  (ntt_waddr1_vreg),
        .vreg_waddr1_coef  (ntt_waddr1_coef),
        .vreg_wdata1       (ntt_wdata1),
        .vreg_we2          (ntt_we2),
        .vreg_waddr2_vreg  (ntt_waddr2_vreg),
        .vreg_waddr2_coef  (ntt_waddr2_coef),
        .vreg_wdata2       (ntt_wdata2),
        .twiddle_k         (ntt_twiddle_k),
        .twiddle_zeta      (twiddle_zeta_shared),
        .twiddle_zeta_inv  (twiddle_zeta_inv_shared)
    );

    logic vpmul_busy, vpmul_done;
    logic  [1:0] vpmul_raddr1_vreg, vpmul_raddr2_vreg, vpmul_waddr1_vreg, vpmul_waddr2_vreg;
    logic  [7:0] vpmul_raddr1_coef, vpmul_raddr2_coef, vpmul_waddr1_coef, vpmul_waddr2_coef;
    logic        vpmul_we1, vpmul_we2;
    logic [15:0] vpmul_wdata1, vpmul_wdata2;
    logic  [5:0] vpmul_twiddle_i;
    logic  [6:0] vpmul_twiddle_k;

    vpmul_engine u_vpmul_engine (
        .clk               (clk),
        .rst_n             (rst_n),
        .start             (start_vpmul),
        .vreg_a            (vreg_rs1),
        .vreg_b            (vreg_rs2),
        .vreg_dst          (vreg_rd),
        .busy              (vpmul_busy),
        .done              (vpmul_done),
        .vreg_raddr1_vreg  (vpmul_raddr1_vreg),
        .vreg_raddr1_coef  (vpmul_raddr1_coef),
        .vreg_rdata1       (vreg_rdata1_shared),
        .vreg_raddr2_vreg  (vpmul_raddr2_vreg),
        .vreg_raddr2_coef  (vpmul_raddr2_coef),
        .vreg_rdata2       (vreg_rdata2_shared),
        .vreg_we1          (vpmul_we1),
        .vreg_waddr1_vreg  (vpmul_waddr1_vreg),
        .vreg_waddr1_coef  (vpmul_waddr1_coef),
        .vreg_wdata1       (vpmul_wdata1),
        .vreg_we2          (vpmul_we2),
        .vreg_waddr2_vreg  (vpmul_waddr2_vreg),
        .vreg_waddr2_coef  (vpmul_waddr2_coef),
        .vreg_wdata2       (vpmul_wdata2),
        .twiddle_i         (vpmul_twiddle_i),
        .twiddle_zeta      (twiddle_zeta_shared)
    );
    // vpmul_engine espera twiddle indexado por 'i' (0-63), con offset +64
    // aplicado externamente (ver vpmul_engine_top.sv original) — se
    // replica ese mismo offset aca.
    assign vpmul_twiddle_k = {1'b0, vpmul_twiddle_i} + 7'd64;

    logic addsub_busy, addsub_done;
    logic  [1:0] addsub_raddr1_vreg, addsub_raddr2_vreg, addsub_waddr1_vreg;
    logic  [7:0] addsub_raddr1_coef, addsub_raddr2_coef, addsub_waddr1_coef;
    logic        addsub_we1;
    logic [15:0] addsub_wdata1;

    addsub_engine u_addsub_engine (
        .clk               (clk),
        .rst_n             (rst_n),
        .start             (start_addsub),
        .is_sub            (funct3 == OP_VSUB),
        .vreg_a            (vreg_rs1),
        .vreg_b            (vreg_rs2),
        .vreg_dst          (vreg_rd),
        .busy              (addsub_busy),
        .done              (addsub_done),
        .vreg_raddr1_vreg  (addsub_raddr1_vreg),
        .vreg_raddr1_coef  (addsub_raddr1_coef),
        .vreg_rdata1       (vreg_rdata1_shared),
        .vreg_raddr2_vreg  (addsub_raddr2_vreg),
        .vreg_raddr2_coef  (addsub_raddr2_coef),
        .vreg_rdata2       (vreg_rdata2_shared),
        .vreg_we1          (addsub_we1),
        .vreg_waddr1_vreg  (addsub_waddr1_vreg),
        .vreg_waddr1_coef  (addsub_waddr1_coef),
        .vreg_wdata1       (addsub_wdata1)
    );

    logic barrett_busy, barrett_done;
    logic  [1:0] barrett_raddr1_vreg, barrett_waddr1_vreg;
    logic  [7:0] barrett_raddr1_coef, barrett_waddr1_coef;
    logic        barrett_we1;
    logic [15:0] barrett_wdata1;

    barrett_engine u_barrett_engine (
        .clk               (clk),
        .rst_n             (rst_n),
        .start             (start_barrett),
        .vreg_src          (vreg_rs1),
        .vreg_dst          (vreg_rd),
        .busy              (barrett_busy),
        .done              (barrett_done),
        .vreg_raddr1_vreg  (barrett_raddr1_vreg),
        .vreg_raddr1_coef  (barrett_raddr1_coef),
        .vreg_rdata1       (vreg_rdata1_shared),
        .vreg_we1          (barrett_we1),
        .vreg_waddr1_vreg  (barrett_waddr1_vreg),
        .vreg_waddr1_coef  (barrett_waddr1_coef),
        .vreg_wdata1       (barrett_wdata1)
    );

    logic loadstore_busy, loadstore_done;
    logic  [1:0] loadstore_raddr1_vreg, loadstore_waddr1_vreg;
    logic  [7:0] loadstore_raddr1_coef, loadstore_waddr1_coef;
    logic        loadstore_we1;
    logic [15:0] loadstore_wdata1;
    logic [15:0] ls_dmem_addr;
    logic [31:0] ls_dmem_wdata;
    logic        ls_dmem_mem_read, ls_dmem_mem_write;
    logic  [2:0] ls_dmem_funct3;

    vload_vstore_engine u_vload_vstore_engine (
        .clk               (clk),
        .rst_n             (rst_n),
        .start             (start_loadstore),
        .is_store          (funct3 == OP_VSTORE),
        .addr_base         (scalar_addr),
        .vreg              (vreg_rd),
        .busy              (loadstore_busy),
        .done              (loadstore_done),
        .dmem_addr         (ls_dmem_addr),
        .dmem_wdata        (ls_dmem_wdata),
        .dmem_mem_read     (ls_dmem_mem_read),
        .dmem_mem_write    (ls_dmem_mem_write),
        .dmem_funct3       (ls_dmem_funct3),
        .dmem_rdata        (dmem_rdata),
        .vreg_raddr1_vreg  (loadstore_raddr1_vreg),
        .vreg_raddr1_coef  (loadstore_raddr1_coef),
        .vreg_rdata1       (vreg_rdata1_shared),
        .vreg_we1          (loadstore_we1),
        .vreg_waddr1_vreg  (loadstore_waddr1_vreg),
        .vreg_waddr1_coef  (loadstore_waddr1_coef),
        .vreg_wdata1       (loadstore_wdata1)
    );

    // ------------------------------------------------------------------
    // busy/done agregados: exactamente uno de los 5 motores puede estar
    // activo a la vez (recurso unico), asi que un OR simple es correcto
    // — no hace falta arbitraje.
    // ------------------------------------------------------------------
    assign any_busy = ntt_busy | vpmul_busy | addsub_busy | barrett_busy | loadstore_busy;
    assign busy = any_busy;
    assign done = ntt_done | vpmul_done | addsub_done | barrett_done | loadstore_done;

    // ------------------------------------------------------------------
    // Mux hacia vreg_file: segun active_op, se conectan las señales del
    // motor correspondiente. Se usa active_op (latcheado), no funct3
    // directo, para que el mux siga apuntando al motor correcto incluso
    // en el ciclo en que funct3 ya cambio (ej. una nueva instruccion
    // decodificada en ID mientras la anterior sigue en vuelo).
    // ------------------------------------------------------------------
    logic  [1:0] mux_raddr1_vreg, mux_raddr2_vreg, mux_waddr1_vreg, mux_waddr2_vreg;
    logic  [7:0] mux_raddr1_coef, mux_raddr2_coef, mux_waddr1_coef, mux_waddr2_coef;
    logic        mux_we1, mux_we2;
    logic [15:0] mux_wdata1, mux_wdata2;

    always_comb begin
        case (active_op)
            OP_VNTT, OP_VINTT: begin
                mux_raddr1_vreg = ntt_raddr1_vreg; mux_raddr1_coef = ntt_raddr1_coef;
                mux_raddr2_vreg = ntt_raddr2_vreg; mux_raddr2_coef = ntt_raddr2_coef;
                mux_waddr1_vreg = ntt_waddr1_vreg; mux_waddr1_coef = ntt_waddr1_coef;
                mux_waddr2_vreg = ntt_waddr2_vreg; mux_waddr2_coef = ntt_waddr2_coef;
                mux_we1 = ntt_we1; mux_we2 = ntt_we2;
                mux_wdata1 = ntt_wdata1; mux_wdata2 = ntt_wdata2;
            end
            OP_VPMUL: begin
                mux_raddr1_vreg = vpmul_raddr1_vreg; mux_raddr1_coef = vpmul_raddr1_coef;
                mux_raddr2_vreg = vpmul_raddr2_vreg; mux_raddr2_coef = vpmul_raddr2_coef;
                mux_waddr1_vreg = vpmul_waddr1_vreg; mux_waddr1_coef = vpmul_waddr1_coef;
                mux_waddr2_vreg = vpmul_waddr2_vreg; mux_waddr2_coef = vpmul_waddr2_coef;
                mux_we1 = vpmul_we1; mux_we2 = vpmul_we2;
                mux_wdata1 = vpmul_wdata1; mux_wdata2 = vpmul_wdata2;
            end
            OP_VADD, OP_VSUB: begin
                mux_raddr1_vreg = addsub_raddr1_vreg; mux_raddr1_coef = addsub_raddr1_coef;
                mux_raddr2_vreg = addsub_raddr2_vreg; mux_raddr2_coef = addsub_raddr2_coef;
                mux_waddr1_vreg = addsub_waddr1_vreg; mux_waddr1_coef = addsub_waddr1_coef;
                mux_waddr2_vreg = 2'd0; mux_waddr2_coef = 8'd0;
                mux_we1 = addsub_we1; mux_we2 = 1'b0;
                mux_wdata1 = addsub_wdata1; mux_wdata2 = 16'd0;
            end
            OP_VBARRETT: begin
                mux_raddr1_vreg = barrett_raddr1_vreg; mux_raddr1_coef = barrett_raddr1_coef;
                mux_raddr2_vreg = 2'd0; mux_raddr2_coef = 8'd0;
                mux_waddr1_vreg = barrett_waddr1_vreg; mux_waddr1_coef = barrett_waddr1_coef;
                mux_waddr2_vreg = 2'd0; mux_waddr2_coef = 8'd0;
                mux_we1 = barrett_we1; mux_we2 = 1'b0;
                mux_wdata1 = barrett_wdata1; mux_wdata2 = 16'd0;
            end
            default: begin // OP_VLOAD, OP_VSTORE
                mux_raddr1_vreg = loadstore_raddr1_vreg; mux_raddr1_coef = loadstore_raddr1_coef;
                mux_raddr2_vreg = 2'd0; mux_raddr2_coef = 8'd0;
                mux_waddr1_vreg = loadstore_waddr1_vreg; mux_waddr1_coef = loadstore_waddr1_coef;
                mux_waddr2_vreg = 2'd0; mux_waddr2_coef = 8'd0;
                mux_we1 = loadstore_we1; mux_we2 = 1'b0;
                mux_wdata1 = loadstore_wdata1; mux_wdata2 = 16'd0;
            end
        endcase
    end

    vreg_file u_vreg_file (
        .clk          (clk),
        .raddr1_vreg  (mux_raddr1_vreg),
        .raddr1_coef  (mux_raddr1_coef),
        .rdata1       (vreg_rdata1_shared),
        .raddr2_vreg  (mux_raddr2_vreg),
        .raddr2_coef  (mux_raddr2_coef),
        .rdata2       (vreg_rdata2_shared),
        .we1          (mux_we1),
        .waddr1_vreg  (mux_waddr1_vreg),
        .waddr1_coef  (mux_waddr1_coef),
        .wdata1       (mux_wdata1),
        .we2          (mux_we2),
        .waddr2_vreg  (mux_waddr2_vreg),
        .waddr2_coef  (mux_waddr2_coef),
        .wdata2       (mux_wdata2)
    );

    // ------------------------------------------------------------------
    // Mux hacia twiddle_rom: solo ntt_engine y vpmul_engine lo usan.
    // ------------------------------------------------------------------
    logic [6:0] mux_twiddle_k;
    assign mux_twiddle_k = (active_op == OP_VPMUL) ? vpmul_twiddle_k : ntt_twiddle_k;

    twiddle_rom u_twiddle_rom (
        .k         (mux_twiddle_k),
        .zeta      (twiddle_zeta_shared),
        .zeta_inv  (twiddle_zeta_inv_shared)
    );

    // ------------------------------------------------------------------
    // dmem: solo vload_vstore_engine lo usa; el resto de motores no
    // tocan memoria en absoluto.
    // ------------------------------------------------------------------
    assign dmem_addr       = ls_dmem_addr;
    assign dmem_wdata      = ls_dmem_wdata;
    assign dmem_mem_read   = ls_dmem_mem_read;
    assign dmem_mem_write  = ls_dmem_mem_write;
    assign dmem_funct3     = ls_dmem_funct3;

endmodule
