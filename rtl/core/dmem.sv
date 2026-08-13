// dmem.sv
//
// Memoria de datos — Fase 1 (core escalar monociclo).
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

module dmem #(
    parameter int ADDR_WIDTH = 16    // 2^16 bytes = 64 KB, igual que link.ld
) (
    input  logic                    clk,
    input  logic [ADDR_WIDTH-1:0]   addr,
    input  logic [31:0]             wdata,
    input  logic                    mem_read,
    input  logic                    mem_write,
    input  logic [2:0]              funct3,   // tamano + signo del acceso

    output logic [31:0]             rdata
);

    localparam int NUM_WORDS = (1 << ADDR_WIDTH) / 4;

    logic [31:0] mem [NUM_WORDS];

    initial begin
        for (int i = 0; i < NUM_WORDS; i++) begin
            mem[i] = 32'd0;
        end
    end

    // Indice de palabra y offset de byte dentro de esa palabra.
    logic [ADDR_WIDTH-3:0] word_idx;
    logic [1:0]            byte_off;
    assign word_idx = addr[ADDR_WIDTH-1:2];
    assign byte_off = addr[1:0];

    // -------------------------------------------------------------------
    // Escritura sincrona: solo actualiza los bytes/halfword/word
    // correspondientes segun funct3, dejando el resto de la palabra
    // intacta (necesario para sb/sh, que no deben pisar bytes vecinos).
    // -------------------------------------------------------------------
    always_ff @(posedge clk) begin
        if (mem_write) begin
            case (funct3[1:0])
                2'b00: begin // sb
                    case (byte_off)
                        2'b00: mem[word_idx][7:0]   <= wdata[7:0];
                        2'b01: mem[word_idx][15:8]  <= wdata[7:0];
                        2'b10: mem[word_idx][23:16] <= wdata[7:0];
                        2'b11: mem[word_idx][31:24] <= wdata[7:0];
                    endcase
                end
                2'b01: begin // sh — se asume alineado a 2 bytes (byte_off[0]==0)
                    if (byte_off[1] == 1'b0)
                        mem[word_idx][15:0]  <= wdata[15:0];
                    else
                        mem[word_idx][31:16] <= wdata[15:0];
                end
                2'b10: begin // sw — se asume alineado a 4 bytes
                    mem[word_idx] <= wdata;
                end
                default: ; // funct3[1:0]=11 no existe para stores
            endcase
        end
    end

    // -------------------------------------------------------------------
    // Lectura asincrona: extrae el byte/halfword/word segun funct3, con
    // sign-extend o zero-extend segun corresponda (funct3[2]: 0=signed,
    // 1=unsigned — ver green card, ej. lb funct3=000 vs lbu funct3=100).
    // -------------------------------------------------------------------
    logic [31:0] raw_word;
    assign raw_word = mem[word_idx];

    logic [7:0]  sel_byte;
    logic [15:0] sel_half;

    always_comb begin
        case (byte_off)
            2'b00: sel_byte = raw_word[7:0];
            2'b01: sel_byte = raw_word[15:8];
            2'b10: sel_byte = raw_word[23:16];
            2'b11: sel_byte = raw_word[31:24];
        endcase

        sel_half = (byte_off[1] == 1'b0) ? raw_word[15:0] : raw_word[31:16];
    end

    always_comb begin
        if (!mem_read) begin
            rdata = 32'd0;
        end else begin
            case (funct3)
                3'b000:  rdata = {{24{sel_byte[7]}}, sel_byte};   // lb  (signed byte)
                3'b001:  rdata = {{16{sel_half[15]}}, sel_half};  // lh  (signed half)
                3'b010:  rdata = raw_word;                        // lw
                3'b100:  rdata = {24'd0, sel_byte};               // lbu (unsigned byte)
                3'b101:  rdata = {16'd0, sel_half};                // lhu (unsigned half)
                default: rdata = 32'd0;
            endcase
        end
    end

endmodule
