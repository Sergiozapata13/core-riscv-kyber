// vreg_file.sv
//
// Banco de registros vectorial — Fase 4 (Kyber, decision A.2 del Apendice A).
//
// 4 registros vectoriales (v0-v3), cada uno con 256 coeficientes de 16
// bits (un polinomio Kyber completo por registro). Fisicamente equivale
// a los "128 palabras de 32 bits" que especifica el Apendice A.2 — es el
// MISMO tamano total (4 x 256 x 16b = 4 x 128 x 32b), solo que el
// direccionamiento es por coeficiente individual, no por palabra
// empaquetada.
//
// Por que coeficiente individual y no palabra de 32 bits: el patron de
// que coeficientes se emparejan en cada butterfly cambia en cada uno de
// los 7 niveles de la NTT (ver isa_vectorial_kyber.docx seccion 6.5) —
// en la gran mayoria de los niveles, los dos coeficientes de un butterfly
// NO caen en la misma palabra de 32 bits si se empaquetaran fijo. Acceso
// por coeficiente individual evita necesitar logica de desempaquetado/
// realineacion en el caso comun.
//
// Puertos: 2 de lectura + 2 de escritura, direccionados por coeficiente
// (indice 0-255 dentro del registro seleccionado). Suficientes para que
// un butterfly (que lee (a,b) y escribe (a',b') en un solo ciclo) no
// necesite serializar sus propios accesos internos — coherente con que
// la unidad vectorial es un recurso unico no pipelineado (Apendice A.1):
// no hace falta soportar multiples operaciones EN VUELO simultaneamente,
// pero una operacion individual si necesita sus propios 2+2 accesos.
//
// Lectura asincrona (igual criterio que el regfile escalar, Fase 1):
// dentro del mismo ciclo en que la unidad vectorial decodifica una
// instruccion, ya necesita los datos disponibles para operar.
// Escritura sincrona, en el flanco de reloj.

module vreg_file (
    input  logic         clk,

    // Puerto de lectura 1
    input  logic  [1:0]  raddr1_vreg,   // v0-v3
    input  logic  [7:0]  raddr1_coef,   // 0-255
    output logic [15:0]  rdata1,

    // Puerto de lectura 2
    input  logic  [1:0]  raddr2_vreg,
    input  logic  [7:0]  raddr2_coef,
    output logic [15:0]  rdata2,

    // Puerto de escritura 1
    input  logic         we1,
    input  logic  [1:0]  waddr1_vreg,
    input  logic  [7:0]  waddr1_coef,
    input  logic [15:0]  wdata1,

    // Puerto de escritura 2
    input  logic         we2,
    input  logic  [1:0]  waddr2_vreg,
    input  logic  [7:0]  waddr2_coef,
    input  logic [15:0]  wdata2
);

    // 4 registros x 256 coeficientes de 16 bits.
    logic [15:0] vregs [4][256];

    // ------------------------------------------------------------------
    // Escritura sincrona. Si ambos puertos de escritura apuntan a la
    // misma direccion (vreg+coef) en el mismo ciclo, el puerto 2 gana
    // (ultimo assign en el bloque secuencial) — caso de esquina que no
    // deberia ocurrir en el uso real (los butterflies escriben a
    // direcciones (a,b) siempre distintas por construccion del patron
    // de la NTT), pero se deja un comportamiento determinista por si
    // acaso, en vez de dejarlo como 'x' o dependiente de orden de
    // simulacion.
    // ------------------------------------------------------------------
    always_ff @(posedge clk) begin
        if (we1) begin
            vregs[waddr1_vreg][waddr1_coef] <= wdata1;
        end
        if (we2) begin
            vregs[waddr2_vreg][waddr2_coef] <= wdata2;
        end
    end

    // ------------------------------------------------------------------
    // Lectura asincrona, sin bypass write-then-read: a diferencia del
    // regfile escalar (Fase 1/2), aca no hay flujo de pipeline donde WB
    // de una instruccion coincide con ID de otra en el mismo ciclo — la
    // unidad vectorial es un recurso unico no pipelineado (A.1), asi que
    // dentro de una misma operacion no hay lectura y escritura del MISMO
    // dato en el mismo ciclo que requiera bypass. Si llegara a hacer
    // falta en la integracion final (Fase 4, unidad completa), se
    // agregaria explicitamente con la misma justificacion que en
    // regfile.sv, no por adelantado sin un consumidor real.
    // ------------------------------------------------------------------
    assign rdata1 = vregs[raddr1_vreg][raddr1_coef];
    assign rdata2 = vregs[raddr2_vreg][raddr2_coef];

endmodule
