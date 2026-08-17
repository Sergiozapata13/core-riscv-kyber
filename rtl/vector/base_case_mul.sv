// base_case_mul.sv
//
// Multiplicacion de base case (polinomios de grado 1) — Fase 4 (vpmul).
//
// Implementa exactamente kyber_ref._base_case_multiply() (ya validado en
// la Fase 3 dentro de test_vector_isa.py, caso vpmul):
//
//   c0 = a0*b0 + zeta*(a1*b1)   (mod q)
//   c1 = a0*b1 + a1*b0          (mod q)
//
// Es la multiplicacion de (a0 + a1*X) * (b0 + b1*X) mod (X^2 - zeta),
// usada para cada uno de los 128 pares del dominio NTT — necesaria
// porque el NTT incompleto de Kyber (7 niveles, no 8) deja el resultado
// como 128 "slots" de polinomios de grado 1, no 256 escalares
// independientes (ver isa_vectorial_kyber.docx seccion 6).
//
// Dado el bug de "constante nunca verificada" que aparecio en
// ntt_engine.sv (INV128), aca CADA operando/constante pasa por
// barrett_reduce explicitamente, sin atajos — mismo patron ya usado en
// butterfly_ct/gs.sv.

module base_case_mul (
    input  logic [11:0]  a0,
    input  logic [11:0]  a1,
    input  logic [11:0]  b0,
    input  logic [11:0]  b1,
    input  logic [11:0]  zeta,

    output logic [11:0]  c0,   // a0*b0 + zeta*(a1*b1)  mod q
    output logic [11:0]  c1    // a0*b1 + a1*b0          mod q
);

    // ---- c0 = a0*b0 + zeta*(a1*b1) mod q ----
    logic [23:0] a1b1_raw;
    logic [11:0] a1b1_reduced;

    assign a1b1_raw = a1 * b1;

    barrett_reduce u_barrett_a1b1 (
        .a      ({{8{1'b0}}, a1b1_raw}),
        .result (a1b1_reduced)
    );

    logic [23:0] zeta_a1b1_raw;
    logic [11:0] zeta_a1b1_reduced;

    assign zeta_a1b1_raw = zeta * a1b1_reduced;

    barrett_reduce u_barrett_zeta_a1b1 (
        .a      ({{8{1'b0}}, zeta_a1b1_raw}),
        .result (zeta_a1b1_reduced)
    );

    logic [23:0] a0b0_raw;
    logic [11:0] a0b0_reduced;

    assign a0b0_raw = a0 * b0;

    barrett_reduce u_barrett_a0b0 (
        .a      ({{8{1'b0}}, a0b0_raw}),
        .result (a0b0_reduced)
    );

    // a0b0_reduced + zeta_a1b1_reduced: ambos en [0,q), suma en
    // [0,2q-2] — Caso A de barrett_reduce (mismo patron que las sumas
    // de butterfly_ct/gs).
    logic [12:0] c0_sum_raw;
    assign c0_sum_raw = {1'b0, a0b0_reduced} + {1'b0, zeta_a1b1_reduced};

    barrett_reduce u_barrett_c0 (
        .a      ({{19{1'b0}}, c0_sum_raw}),
        .result (c0)
    );

    // ---- c1 = a0*b1 + a1*b0 mod q ----
    logic [23:0] a0b1_raw;
    logic [11:0] a0b1_reduced;

    assign a0b1_raw = a0 * b1;

    barrett_reduce u_barrett_a0b1 (
        .a      ({{8{1'b0}}, a0b1_raw}),
        .result (a0b1_reduced)
    );

    logic [23:0] a1b0_raw;
    logic [11:0] a1b0_reduced;

    assign a1b0_raw = a1 * b0;

    barrett_reduce u_barrett_a1b0 (
        .a      ({{8{1'b0}}, a1b0_raw}),
        .result (a1b0_reduced)
    );

    logic [12:0] c1_sum_raw;
    assign c1_sum_raw = {1'b0, a0b1_reduced} + {1'b0, a1b0_reduced};

    barrett_reduce u_barrett_c1 (
        .a      ({{19{1'b0}}, c1_sum_raw}),
        .result (c1)
    );

endmodule
