// butterfly_gs.sv
//
// Mariposa Gentleman-Sande — Fase 4 (Kyber, vintt).
//
// Implementa exactamente isa_vectorial_kyber.docx, seccion 6.3:
//   a' = (a + b)          (mod q)
//   b' = zeta_inv*(a - b)  (mod q)
//
// A diferencia de butterfly_ct.sv, aca la multiplicacion por el twiddle
// factor ocurre DESPUES de la resta, no antes — es la diferencia
// estructural entre CT y GS que motivo mantenerlos como modulos
// separados (ver discusion de diseño, Fase 4).
//
// El parametro de entrada se llama zeta_inv (no zeta) para dejar
// explicito en la interfaz que quien invoca este modulo debe pasar el
// inverso modular del twiddle factor correspondiente, no el twiddle
// factor crudo — es responsabilidad del firmware/tabla de constantes
// (ROM de twiddles, ver seccion 6.4 del documento de ISA) proveer el
// valor correcto; este modulo no invierte nada, solo multiplica.

module butterfly_gs (
    input  logic [11:0]  a,
    input  logic [11:0]  b,
    input  logic [11:0]  zeta_inv,

    output logic [11:0]  a_out,   // (a + b) mod q
    output logic [11:0]  b_out    // zeta_inv*(a - b) mod q
);

    // a' = a + b (mod q). Suma en [0, 2q-2], dentro del rango verificado
    // de barrett_reduce (Caso A).
    logic [12:0] sum_raw;
    assign sum_raw = {1'b0, a} + {1'b0, b};

    barrett_reduce u_barrett_sum (
        .a      ({{19{1'b0}}, sum_raw}),
        .result (a_out)
    );

    // diff = a - b (mod q, con signo antes de reducir). Rango
    // [-(q-1), q-1], Caso B de barrett_reduce.
    logic signed [12:0] diff_raw;
    logic [11:0]         diff_reduced;

    assign diff_raw = $signed({1'b0, a}) - $signed({1'b0, b});

    barrett_reduce u_barrett_diff (
        .a      ({{19{diff_raw[12]}}, diff_raw}),
        .result (diff_reduced)
    );

    // b' = zeta_inv * diff_reduced (mod q). Ambos factores en [0,q),
    // producto en [0,(q-1)^2] — Caso A de barrett_reduce.
    logic [23:0] prod_raw;
    assign prod_raw = zeta_inv * diff_reduced;

    barrett_reduce u_barrett_prod (
        .a      ({{8{1'b0}}, prod_raw}),
        .result (b_out)
    );

endmodule
