// dmem.sv
//
// Memoria de datos — Fase 1 (core escalar monociclo), extendida en Fase 4
// con un segundo puerto para vector_unit.
//
// Modelo behavioral para simulacion (Verilator). Byte-direccionable, con
// soporte para los 5 tamanos de acceso de RV32I (ver green card):
//   lb/sb   - byte,      con signo en la carga
//   lh/sh   - halfword,  con signo en la carga
//   lw/sw   - word
//   lbu/lhu - byte/halfword, SIN signo en la carga (zero-extend)
//
// El campo funct3 de la instruccion original codifica exactamente el
// tamano + signo (ver green card, notas 3/4/5 de RV32I): se pasa funct3
// directo a este modulo en vez de re-decodificar el tamano en otro lado.
//
// Escritura sincrona (en el flanco de reloj, como corresponde a memoria
// real), lectura asincrona combinacional (igual que imem e igual que el
// register file, por consistencia con el resto del datapath monociclo).
//
// Internamente el array se modela como palabras de 32 bits (no bytes
// individuales) por simplicidad del modelo behavioral; el acceso a
// sub-palabra se resuelve con muxes de byte/halfword sobre esa palabra.
//
// SEGUNDO PUERTO (Fase 4): vload/vstore necesitan acceder a esta misma
// memoria mientras el pipeline escalar sigue corriendo instrucciones
// independientes (decision A.1) — con un solo puerto, un lw/sw escalar
// y un vload/vstore vectorial podrian competir por el mismo acceso en el
// mismo ciclo. Se opto por agregar un SEGUNDO PUERTO independiente (en
// vez de arbitrar un unico puerto con stalls adicionales) — mismo
// patron ya usado en vreg_file (2 puertos de lectura + 2 de escritura,
// para que un butterfly no tenga que serializar sus propios accesos).
// El PUERTO 1 (addr/wdata/mem_read/mem_write/funct3/rdata) preserva EXACTAMENTE
// la misma interfaz y comportamiento que tenia en la Fase 1 — el pipeline
// escalar no necesita ningun cambio. El PUERTO 2 (addr2/wdata2/...) es
// nuevo, exclusivo de vector_unit.
//
// Si ambos puertos escriben a la MISMA palabra en el MISMO ciclo, el
// puerto 2 (vectorial) gana — misma convencion que vreg_file (we1/we2,
// puerto 2 gana en colision). Caso de esquina que no deberia ocurrir en
// el uso real (el firmware de Kyber no deberia hacer un sw escalar y un
// vstore vectorial a la misma direccion en el mismo ciclo), pero se deja
// un comportamiento determinista por si acaso.

module dmem #(
    parameter int ADDR_WIDTH = 16    // 2^16 bytes = 64 KB, igual que link.ld
) (
    input  logic                    clk,

    // ---- Puerto 1: pipeline escalar (Fase 1, interfaz sin cambios) ----
    input  logic [ADDR_WIDTH-1:0]   addr,
    input  logic [31:0]             wdata,
    input  logic                    mem_read,
    input  logic                    mem_write,
    input  logic [2:0]              funct3,
    output logic [31:0]             rdata,

    // ---- Puerto 2: vector_unit (Fase 4, vload/vstore) ----
    input  logic [ADDR_WIDTH-1:0]   addr2,
    input  logic [31:0]             wdata2,
    input  logic                    mem_read2,
    input  logic                    mem_write2,
    input  logic [2:0]              funct3_2,
    output logic [31:0]             rdata2
);

    localparam int NUM_WORDS = (1 << ADDR_WIDTH) / 4;

    logic [31:0] mem [NUM_WORDS];

    initial begin
        for (int i = 0; i < NUM_WORDS; i++) begin
            mem[i] = 32'd0;
        end
    end

    // ------------------------------------------------------------------
    // Indices de palabra y offset de byte, por puerto.
    // ------------------------------------------------------------------
    logic [ADDR_WIDTH-3:0] word_idx1, word_idx2;
    logic [1:0]            byte_off1, byte_off2;
    assign word_idx1 = addr[ADDR_WIDTH-1:2];
    assign byte_off1 = addr[1:0];
    assign word_idx2 = addr2[ADDR_WIDTH-1:2];
    assign byte_off2 = addr2[1:0];

    // ------------------------------------------------------------------
    // Escritura sincrona, ambos puertos. Si ambos escriben la MISMA
    // palabra en el mismo ciclo, el puerto 2 gana (ultimo bloque
    // ejecutado en orden textual dentro del always_ff — mismo criterio
    // que vreg_file we1/we2).
    // ------------------------------------------------------------------
    always_ff @(posedge clk) begin
        if (mem_write) begin
            case (funct3[1:0])
                2'b00: begin // sb
                    case (byte_off1)
                        2'b00: mem[word_idx1][7:0]   <= wdata[7:0];
                        2'b01: mem[word_idx1][15:8]  <= wdata[7:0];
                        2'b10: mem[word_idx1][23:16] <= wdata[7:0];
                        2'b11: mem[word_idx1][31:24] <= wdata[7:0];
                    endcase
                end
                2'b01: begin // sh
                    if (byte_off1[1] == 1'b0)
                        mem[word_idx1][15:0]  <= wdata[15:0];
                    else
                        mem[word_idx1][31:16] <= wdata[15:0];
                end
                2'b10: begin // sw
                    mem[word_idx1] <= wdata;
                end
                default: ;
            endcase
        end

        if (mem_write2) begin
            case (funct3_2[1:0])
                2'b00: begin // sb
                    case (byte_off2)
                        2'b00: mem[word_idx2][7:0]   <= wdata2[7:0];
                        2'b01: mem[word_idx2][15:8]  <= wdata2[7:0];
                        2'b10: mem[word_idx2][23:16] <= wdata2[7:0];
                        2'b11: mem[word_idx2][31:24] <= wdata2[7:0];
                    endcase
                end
                2'b01: begin // sh
                    if (byte_off2[1] == 1'b0)
                        mem[word_idx2][15:0]  <= wdata2[15:0];
                    else
                        mem[word_idx2][31:16] <= wdata2[15:0];
                end
                2'b10: begin // sw
                    mem[word_idx2] <= wdata2;
                end
                default: ;
            endcase
        end
    end

    // ------------------------------------------------------------------
    // Lectura asincrona, ambos puertos — logica identica, duplicada por
    // puerto (sin generate, por legibilidad: son solo 2 instancias).
    // ------------------------------------------------------------------
    logic [31:0] raw_word1, raw_word2;
    assign raw_word1 = mem[word_idx1];
    assign raw_word2 = mem[word_idx2];

    logic [7:0]  sel_byte1, sel_byte2;
    logic [15:0] sel_half1, sel_half2;

    always_comb begin
        case (byte_off1)
            2'b00: sel_byte1 = raw_word1[7:0];
            2'b01: sel_byte1 = raw_word1[15:8];
            2'b10: sel_byte1 = raw_word1[23:16];
            2'b11: sel_byte1 = raw_word1[31:24];
        endcase
        sel_half1 = (byte_off1[1] == 1'b0) ? raw_word1[15:0] : raw_word1[31:16];

        case (byte_off2)
            2'b00: sel_byte2 = raw_word2[7:0];
            2'b01: sel_byte2 = raw_word2[15:8];
            2'b10: sel_byte2 = raw_word2[23:16];
            2'b11: sel_byte2 = raw_word2[31:24];
        endcase
        sel_half2 = (byte_off2[1] == 1'b0) ? raw_word2[15:0] : raw_word2[31:16];
    end

    always_comb begin
        if (!mem_read) begin
            rdata = 32'd0;
        end else begin
            case (funct3)
                3'b000:  rdata = {{24{sel_byte1[7]}}, sel_byte1};
                3'b001:  rdata = {{16{sel_half1[15]}}, sel_half1};
                3'b010:  rdata = raw_word1;
                3'b100:  rdata = {24'd0, sel_byte1};
                3'b101:  rdata = {16'd0, sel_half1};
                default: rdata = 32'd0;
            endcase
        end
    end

    always_comb begin
        if (!mem_read2) begin
            rdata2 = 32'd0;
        end else begin
            case (funct3_2)
                3'b000:  rdata2 = {{24{sel_byte2[7]}}, sel_byte2};
                3'b001:  rdata2 = {{16{sel_half2[15]}}, sel_half2};
                3'b010:  rdata2 = raw_word2;
                3'b100:  rdata2 = {24'd0, sel_byte2};
                3'b101:  rdata2 = {16'd0, sel_half2};
                default: rdata2 = 32'd0;
            endcase
        end
    end

endmodule
