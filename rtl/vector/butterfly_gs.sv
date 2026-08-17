// butterfly_gs.sv
//
// Mariposa Gentleman-Sande — Fase 4 (Kyber, vintt).
//
// Implementa:
//   a' = (a + b)      (mod q)
//   b' = zeta*(b - a)  (mod q)
//
// CORRECCIONES IMPORTANTES (post-verificacion contra el motor NTT
// completo, Fase 4) respecto a lo que documentaba originalmente
// isa_vectorial_kyber.docx seccion 6.3:
//
//   1. Twiddle factor: se usa el MISMO ZETAS[k] que Cooley-Tukey, no su
//      inverso modular. La convencion academica estandar de FFT/NTT usa
//      el twiddle inverso en GS, pero la implementacion de referencia
//      REAL de Kyber (pq-crystals/kyber) no invierte nada.
//
//   2. Signo de la resta: es (b - a), NO (a - b). El orden importa
//      porque la resta es asimetrica bajo reduccion modular con signo.
//
// Ambas correcciones se confirmaron debuggeando el motor NTT completo
// (ntt_engine.sv): con la formula original (zeta inverso, resta a-b) el
// round-trip INTT(NTT(x)) daba resultados incorrectos en todos los
// coeficientes; con esta formula corregida coincide exactamente con
// kyber_ref.intt() (ya validado contra kyber-py en la Fase 3).
//
// El documento de especificacion (Fase 3) debe corregirse para
// reflejar esta realidad antes de considerarse fuente de verdad final.
//
// A diferencia de butterfly_ct.sv, aca la multiplicacion por el twiddle
// factor ocurre DESPUES de la resta, no antes — es la diferencia
// estructural entre CT y GS que motivo mantenerlos como modulos
// separados (ver discusion de diseño, Fase 4).

module butterfly_gs (
    input  logic [11:0]  a,
    input  logic [11:0]  b,
    input  logic [11:0]  zeta,

    output logic [11:0]  a_out,   // (a + b) mod q
    output logic [11:0]  b_out    // zeta*(b - a) mod q
);

    // a' = a + b (mod q). Suma en [0, 2q-2], dentro del rango verificado
    // de barrett_reduce (Caso A).
    logic [12:0] sum_raw;
    assign sum_raw = {1'b0, a} + {1'b0, b};

    barrett_reduce u_barrett_sum (
        .a      ({{19{1'b0}}, sum_raw}),
        .result (a_out)
    );

    // diff = b - a (mod q, con signo antes de reducir). ORDEN INVERTIDO
    // respecto a butterfly_ct (que usa a-b implicitamente via a+t/a-t) —
    // ver correccion 2 en el comentario de cabecera. Rango [-(q-1),q-1],
    // Caso B de barrett_reduce.
    logic signed [12:0] diff_raw;
    logic [11:0]         diff_reduced;

    assign diff_raw = $signed({1'b0, b}) - $signed({1'b0, a});

    barrett_reduce u_barrett_diff (
        .a      ({{19{diff_raw[12]}}, diff_raw}),
        .result (diff_reduced)
    );

    // b' = zeta * diff_reduced (mod q). Ambos factores en [0,q),
    // producto en [0,(q-1)^2] — Caso A de barrett_reduce.
    logic [23:0] prod_raw;
    assign prod_raw = zeta * diff_reduced;

    barrett_reduce u_barrett_prod (
        .a      ({{8{1'b0}}, prod_raw}),
        .result (b_out)
    );

endmodule
