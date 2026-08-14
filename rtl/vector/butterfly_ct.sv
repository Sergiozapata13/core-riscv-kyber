// butterfly_ct.sv
//
// Mariposa Cooley-Tukey — Fase 4 (Kyber, vntt).
//
// Implementa exactamente isa_vectorial_kyber.docx, seccion 6.2:
//   a' = a + zeta*b  (mod q)
//   b' = a - zeta*b  (mod q)
//
// Combinacional pura (sin registro interno): un ciclo de latencia hasta
// que la unidad vectorial (Fase 4, integracion completa) decida como
// pipelinear la cadena de 7 niveles. La multiplicacion zeta*b ocurre
// PRIMERO, antes de la suma/resta — esto es lo que distingue CT de GS
// (ver butterfly_gs.sv, donde el orden es al reves), y es la razon por
// la que se decidio mantener ambos modos como modulos separados en vez
// de un unico modulo parametrizable (ver discusion de diseño, Fase 4):
// no es la misma operacion con un mux al final, el multiplicador vive en
// un punto distinto del datapath en cada caso.
//
// a, b, zeta llegan ya reducidos a [0,q) (asi viven los coeficientes en
// el banco vectorial, ver Apendice A.2). El producto zeta*b puede llegar
// a (q-1)^2 ~ 2^23, dentro del rango de entrada verificado para
// barrett_reduce (Caso A). a +/- t tambien esta dentro de rango (Caso B
// de barrett_reduce, zeta*(a-b) tiene la misma forma que a-t aqui salvo
// que el signo del termino que se reduce es distinto — se verifica de
// nuevo explicitamente para este modulo en el testbench, no se asume).

module butterfly_ct (
    input  logic [11:0]  a,
    input  logic [11:0]  b,
    input  logic [11:0]  zeta,

    output logic [11:0]  a_out,   // a + zeta*b (mod q)
    output logic [11:0]  b_out    // a - zeta*b (mod q)
);

    // t = zeta * b, reducido mod q. El producto crudo cabe en 24 bits
    // sin signo (12b * 12b), se extiende a 32 bits con signo para
    // barrett_reduce (entrada siempre no-negativa aqui, ya que zeta y b
    // son ambos no-negativos).
    logic [23:0] t_raw;
    logic [11:0] t;

    assign t_raw = zeta * b;

    barrett_reduce u_barrett_t (
        .a      ({{8{1'b0}}, t_raw}),
        .result (t)
    );

    // a' = a + t (mod q). a y t estan ambos en [0,q), la suma cabe en
    // [0, 2q-2] -- dentro del rango de entrada verificado de
    // barrett_reduce (mucho menor a (q-1)^2, Caso A).
    logic [12:0] sum_raw;
    assign sum_raw = {1'b0, a} + {1'b0, t};

    barrett_reduce u_barrett_sum (
        .a      ({{19{1'b0}}, sum_raw}),
        .result (a_out)
    );

    // b' = a - t (mod q). a-t puede ser negativo (a,t en [0,q)), el
    // rango [-(q-1), q-1] esta dentro del Caso B de barrett_reduce
    // (verificado con signo).
    logic signed [12:0] diff_raw;
    assign diff_raw = $signed({1'b0, a}) - $signed({1'b0, t});

    barrett_reduce u_barrett_diff (
        .a      ({{19{diff_raw[12]}}, diff_raw}),
        .result (b_out)
    );

endmodule
