// regfile.sv
//
// Register file RV32I — Fase 1 (core escalar monociclo).
//
// - 32 registros de 32 bits (x0-x31).
// - Lectura asincrona (combinacional) de 2 puertos: necesaria porque en ID
//   se leen rs1/rs2 en el mismo ciclo en que se decodifica la instruccion,
//   sin gastar un ciclo de reloj adicional solo para leer.
// - Escritura sincrona de 1 puerto, en el flanco de subida (WB).
// - x0 esta hardwireado a 0: nunca se actualiza, sin importar we/waddr.
// - Write-then-read en el mismo ciclo (solo si ENABLE_BYPASS=1): si en el
//   mismo ciclo se escribe en waddr y se lee ese mismo registro por
//   raddr1/raddr2, el dato leido debe ser el NUEVO valor (el que se esta
//   escribiendo), no el viejo. Se logra con un bypass combinacional en la
//   lectura, comparando waddr con raddr1/raddr2 cuando we esta activo.
//
//   IMPORTANTE — por que este bypass es parametrizable y no siempre activo:
//   en un datapath MONOCICLO (Fase 1), cada instruccion completa todo su
//   IF-ID-EX-MEM-WB dentro de un unico ciclo de reloj; la escritura a
//   waddr ocurre en el FLANCO, es decir, despues de que la lectura de
//   raddr1/raddr2 ya fue consumida por el resto del datapath en ese mismo
//   ciclo. Si el bypass combinacional esta activo ahi, crea un ciclo
//   combinacional real (wdata depende de rs1_data/rs2_data via la ALU y
//   la memoria, que a su vez dependen de rdata1/rdata2 con bypass activo)
//   — Verilator lo marca como UNOPTFLAT, y con razon: no es un falso
//   positivo, es un lazo combinacional genuino en ese cableado.
//
//   En un pipeline (Fase 2), en cambio, el bypass SI es necesario y
//   correcto: WB de una instruccion y ID de OTRA instruccion ocurren en
//   el mismo ciclo de reloj fisico, y sin el bypass, ID leeria un valor
//   stale (el de antes de la escritura) para una dependencia RAW de 1
//   ciclo — exactamente el caso que forwarding/bypass existe para resolver.
//
//   Por eso: ENABLE_BYPASS=0 para instanciar en core_top (monociclo);
//   ENABLE_BYPASS=1 (default) para la instancia que se usara en la Fase 2.

module regfile #(
    parameter bit ENABLE_BYPASS = 1'b1   // ver comentario arriba: 0 para monociclo, 1 para pipeline
) (
    input  logic        clk,
    input  logic         we,
    input  logic  [4:0]  waddr,
    input  logic [31:0]  wdata,

    input  logic  [4:0]  raddr1,
    input  logic  [4:0]  raddr2,
    output logic [31:0]  rdata1,
    output logic [31:0]  rdata2
);

    // 32 registros de 32 bits. regs[0] existe fisicamente pero nunca se lee
    // ni se escribe con un valor distinto de 0 (ver logica de lectura/escritura
    // abajo) — mas simple que excluirlo del array.
    logic [31:0] regs [32];

    // -------------------------------------------------------------------
    // Escritura sincrona
    // -------------------------------------------------------------------
    // x0 nunca se escribe: si waddr==0, el write enable se ignora para el
    // banco de registros (regs[0] se mantiene en 0 de todas formas gracias
    // al bypass de lectura, pero no depender de eso por claridad).
    always_ff @(posedge clk) begin
        if (we && waddr != 5'd0) begin
            regs[waddr] <= wdata;
        end
    end

    // -------------------------------------------------------------------
    // Lectura asincrona con bypass write-then-read y x0 hardwireado
    // -------------------------------------------------------------------
    always_comb begin
        // Puerto 1
        if (raddr1 == 5'd0) begin
            rdata1 = 32'd0;
        end else if (ENABLE_BYPASS && we && (waddr == raddr1)) begin
            rdata1 = wdata;             // bypass: mismo registro escrito este ciclo
        end else begin
            rdata1 = regs[raddr1];
        end

        // Puerto 2
        if (raddr2 == 5'd0) begin
            rdata2 = 32'd0;
        end else if (ENABLE_BYPASS && we && (waddr == raddr2)) begin
            rdata2 = wdata;             // bypass: mismo registro escrito este ciclo
        end else begin
            rdata2 = regs[raddr2];
        end
    end

endmodule
