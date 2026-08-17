// poly_addsub.sv
//
// Suma/resta de coeficientes — Fase 4 (vadd/vsub).
//
// Implementa exactamente kyber_ref.poly_add()/poly_sub() (ya validados
// en la Fase 3): a+b mod q, o a-b mod q, segun is_sub.
//
// A diferencia de butterfly_ct.sv/butterfly_gs.sv (que se mantuvieron
// como modulos separados porque el multiplicador aparece en un punto
// distinto del datapath en cada caso), aca SI es la misma operacion con
// un solo bit de diferencia: sumar o restar el mismo par de operandos,
// sin ningun reordenamiento de operaciones. Un solo modulo parametrizado
// por is_sub es la opcion mas simple sin sacrificar claridad — no hay
// riesgo de que el selector "mezcle" comportamiento de un modo con el
// otro, porque la unica diferencia es el signo de un operando antes de
// sumar.

module poly_addsub (
    input  logic [11:0]  a,
    input  logic [11:0]  b,
    input  logic          is_sub,   // 0 = vadd (a+b), 1 = vsub (a-b)

    output logic [11:0]  result
);

    // a +/- b (mod q, con signo antes de reducir). Rango:
    //   suma:  [0, 2q-2] = [0, 6656]  -> Caso A de barrett_reduce
    //   resta: [-(q-1), q-1]          -> Caso B de barrett_reduce
    // 6656 excede el rango de un signed[12:0] (max 4095) — se usa 14
    // bits con signo (rango +-8191) para cubrir la suma maxima sin
    // overflow. Bug real encontrado en la primera compilacion de este
    // modulo: con 13 bits, sum_raw se truncaba silenciosamente para
    // a+b > 4095, dando resultados incorrectos en ~18% de los casos de
    // prueba (los que tenian a+b >= q, precisamente el caso que importa
    // verificar).
    logic signed [13:0] sum_raw, diff_raw;
    logic signed [13:0] op_raw;

    assign sum_raw  = $signed({2'b0, a}) + $signed({2'b0, b});
    assign diff_raw = $signed({2'b0, a}) - $signed({2'b0, b});
    assign op_raw   = is_sub ? diff_raw : sum_raw;

    barrett_reduce u_barrett (
        .a      ({{18{op_raw[13]}}, op_raw}),
        .result (result)
    );

endmodule
