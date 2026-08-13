// imem.sv
//
// Memoria de instrucciones — Fase 1 (core escalar monociclo).
//
// Modelo behavioral para simulacion (Verilator), NO sintetizable como
// memoria real de silicio tal cual — es un array behavioral que se carga
// via $readmemh desde un archivo .hex generado por el toolchain RISC-V
// (ver sw/crt/link.ld y el flujo objcopy -O verilog de la Fase 0).
//
// Lectura combinacional (asincrona): en un datapath monociclo, la
// instruccion debe estar disponible en el mismo ciclo en que PC la
// direcciona, sin esperar un flanco de reloj — coherente con la decision
// de register file (lectura asincrona) de este mismo proyecto.
//
// Direccionamiento por PALABRA, no por byte: aunque RV32I es byte-
// direccionable en general (para loads/stores en dmem.sv), las
// instrucciones siempre estan alineadas a 4 bytes, asi que addr[1:0] se
// descarta y se usa addr[N-1:2] como indice de palabra — evita tener que
// modelar un mux de bytes que nunca se necesita para fetch de instruccion.

module imem #(
    parameter int ADDR_WIDTH = 16,          // 2^16 bytes = 64 KB, igual que link.ld
    parameter string INIT_FILE = ""          // ruta al .hex; "" = memoria en 0
) (
    // verilator lint_off UNUSEDSIGNAL
    // addr[1:0] se ignora deliberadamente: el fetch de instruccion siempre
    // esta alineado a palabra de 4 bytes, no hace falta un mux de byte aqui.
    input  logic [ADDR_WIDTH-1:0]  addr,     // direccion de byte (PC)
    // verilator lint_on UNUSEDSIGNAL
    output logic [31:0]            instr
);

    localparam int NUM_WORDS = (1 << ADDR_WIDTH) / 4;

    logic [31:0] mem [NUM_WORDS];

    initial begin
        if (INIT_FILE != "") begin
            $readmemh(INIT_FILE, mem);
        end
    end

    // addr[1:0] se ignora: fetch siempre alineado a palabra de 4 bytes.
    assign instr = mem[addr[ADDR_WIDTH-1:2]];

endmodule
