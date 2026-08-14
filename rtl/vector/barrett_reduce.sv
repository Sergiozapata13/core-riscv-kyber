// barrett_reduce.sv
//
// Unidad de reduccion modular Barrett — Fase 4 (Kyber, mod q=3329).
//
// Implementa exactamente el algoritmo de isa_vectorial_kyber.docx, seccion
// 5.2, mas la normalizacion constant-time de la seccion 4.2:
//
//   t         = a * BARRETT_MULTIPLIER + (BARRETT_R >> 1)
//   quotient  = t >> BARRETT_SHIFT
//   remainder = a - quotient * q
//   result    = normalizar(remainder) a [0, q)   <- via seleccion aritmetica
//
// Rango de entrada: 'a' es un entero CON SIGNO. El rango de remainder tras
// la formula Barrett fue verificado EXHAUSTIVAMENTE (no por muestreo) con
// el modelo de referencia en Python (models/) para los dos casos de uso
// reales del proyecto:
//   - Caso A: producto de dos coeficientes ya reducidos, a en [0,(q-1)^2]
//     -> remainder crudo en (-q, q)
//   - Caso B: zeta*(a-b) con a,b en [0,q) -> remainder crudo en (-q, q)
// En ambos casos (33M+ valores verificados), el remainder crudo NUNCA cae
// fuera de (-q, q) — por lo tanto UNA SOLA correccion bidireccional basta
// (nunca hacen falta dos restas/sumas de q en cadena). Ver
// models/gen_barrett_testvectors.py para el script de verificacion.
//
// PRECONDICION DE USO (importante): este modulo NO garantiza un resultado
// correcto para valores de 'a' fuera de los rangos A/B de arriba. Se
// intento verificar contra los extremos absolutos de 32 bits con signo
// (a = +-2^31) y el remainder crudo cae hasta +-4.3q — bien fuera de lo
// que una sola correccion puede arreglar. Esto es una decision de diseño
// deliberada, no un descuido: cubrir el rango completo de 32 bits
// requeriria una cadena de hasta ~5 correcciones (o un enfoque distinto,
// como un modulo real en vez de correccion iterativa), lo cual es
// desperdicio de hardware para un caso que NUNCA ocurre en el uso real
// de este proyecto (los operandos de vntt/vintt/vpmul/vbarrett/vadd/vsub
// siempre son coeficientes de Kyber ya reducidos a [0,q), o productos/
// diferencias de los mismos — nunca enteros de 32 bits arbitrarios).
// Si en el futuro este modulo se reutiliza en un contexto con rango de
// entrada mas amplio, esta precondicion debe revisarse primero.
//
// Constant-time (obligatorio, ver seccion 4.2 del documento de ISA): la
// normalizacion NO usa un if/branch de control. Se calculan dos mascaras
// de comparador puro (neg_mask, ge_q_mask) y dos sumas/restas enmascaradas
// que SIEMPRE se ejecutan, sin importar el valor de 'a' — solo el
// resultado final cambia segun el dato, no el camino tomado ni el numero
// de ciclos.

module barrett_reduce (
    input  logic signed [31:0]  a,
    output logic         [11:0] result   // [0, q) cabe en 12 bits (q=3329 < 4096)
);

    localparam int Q = 3329;
    localparam int BARRETT_SHIFT      = 26;
    localparam int BARRETT_MULTIPLIER = 20159;   // = ceil(2^26 / q)
    localparam logic signed [63:0] BARRETT_R_HALF = 64'sd33554432; // 2^26 >> 1

    // ------------------------------------------------------------------
    // Formula Barrett: mult + shift + resta. Timing fijo por construccion
    // (ninguna de estas tres operaciones tiene camino de datos variable).
    // Se usa 64 bits internamente para evitar overflow: a (32b con signo)
    // * multiplier (15b) puede llegar a ~47 bits.
    // ------------------------------------------------------------------
    logic signed [63:0] t;
    logic signed [63:0] quotient;
    // verilator lint_off UNUSEDSIGNAL
    // Los bits altos [63:13] de remainder_wide nunca se usan de forma
    // intencional: el rango real del remainder crudo de Barrett para el
    // dominio de entrada de este proyecto fue verificado exhaustivamente
    // en (-q, q) (ver comentario de cabecera), asi que cabe siempre en 13
    // bits con signo — no hace falta propagar mas bits.
    logic signed [63:0] remainder_wide;
    // verilator lint_on UNUSEDSIGNAL

    assign t         = ($signed({{32{a[31]}}, a}) * BARRETT_MULTIPLIER) + BARRETT_R_HALF;
    assign quotient  = t >>> BARRETT_SHIFT;   // shift aritmetico: preserva signo
    assign remainder_wide = $signed({{32{a[31]}}, a}) - (quotient * Q);

    // remainder_wide esta garantizado en (-q, q) para el rango de entrada
    // real del proyecto (ver comentario de cabecera) — cabe holgadamente
    // en 13 bits con signo.
    logic signed [12:0] remainder;
    assign remainder = remainder_wide[12:0];

    // ------------------------------------------------------------------
    // Normalizacion constant-time: dos mascaras de comparador puro,
    // ambas siempre calculadas, ambas correcciones siempre "ejecutadas"
    // (aunque una de las dos sea un no-op porque su mascara es 0).
    // ------------------------------------------------------------------
    logic                neg_mask;    // 1 si remainder < 0
    logic                ge_q_mask;   // 1 si remainder >= q (evaluado DESPUES de la correccion de signo, ver nota abajo)
    logic signed [13:0]  after_neg_fix;
    // verilator lint_off UNUSEDSIGNAL
    // after_ge_fix[12] (bit de signo tras la segunda correccion) nunca
    // se usa: para el rango de entrada verificado, after_ge_fix siempre
    // queda en [0, q), asi que su bit de signo es siempre 0 por
    // construccion matematica, no hace falta consultarlo.
    logic signed [13:0]  after_ge_fix;
    // verilator lint_on UNUSEDSIGNAL

    assign neg_mask      = remainder[12];              // bit de signo: 1 si negativo
    assign after_neg_fix = {{1{remainder[12]}}, remainder} + (neg_mask ? 14'sd3329 : 14'sd0);

    // NOTA sobre "seleccion aritmetica, no branch de control": neg_mask y
    // ge_q_mask son señales de DATOS (resultado de una comparacion pura),
    // no señales de control de flujo — no hay ningun camino del datapath
    // que se salte o tome menos ciclos segun su valor. Ambas correcciones
    // (after_neg_fix, after_ge_fix) se calculan siempre, en paralelo,
    // exactamente como lo haria el hardware para cualquier valor de 'a'.
    // Esto es lo que exige la seccion 4.2 del documento de ISA: el
    // resultado cambia segun el dato, el TRABAJO REALIZADO no.
    assign ge_q_mask     = (after_neg_fix >= 14'sd3329) ? 1'b1 : 1'b0;
    assign after_ge_fix  = after_neg_fix - (ge_q_mask ? 14'sd3329 : 14'sd0);

    assign result = after_ge_fix[11:0];

endmodule
