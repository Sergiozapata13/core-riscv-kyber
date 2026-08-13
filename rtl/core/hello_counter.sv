// hello_counter.sv
//
// Módulo trivial de validación de entorno (Fase 0).
// No forma parte del core: es únicamente un "hello world" para confirmar
// que Verilator compila, simula, y produce una waveform legible antes de
// escribir una sola línea del datapath real.

module hello_counter #(
    parameter int WIDTH = 8
) (
    input  logic             clk,
    input  logic             rst_n,
    output logic [WIDTH-1:0] count
);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            count <= '0;
        end else begin
            count <= count + 1'b1;
        end
    end

endmodule
