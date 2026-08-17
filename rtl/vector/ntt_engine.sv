// ntt_engine.sv
//
// Motor NTT/INTT — Fase 4 (vntt/vintt), FSM secuencial.
//
// Orquesta butterfly_ct (modo NTT) o butterfly_gs (modo INTT) a traves de
// los 7 niveles x 128 mariposas (896 butterflies totales) que especifica
// isa_vectorial_kyber.docx seccion 6.5, generando en cada ciclo las
// direcciones de lectura/escritura al vreg_file y el indice de twiddle
// factor, replicando EXACTAMENTE el patron de bucles de kyber_ref.ntt()/
// intt() (ver models/kyber_ref.py) — mismo generador de direcciones,
// solo traducido de bucles de Python a contadores de hardware.
//
// Decision de diseño (ver discusion, Fase 4): 1 butterfly por ciclo,
// 896 ciclos de latencia por NTT/INTT completo. Se evaluo paralelizar
// multiples butterflies por nivel, pero eso requiere un banco de
// registros multi-puerto con esquema de acceso libre de conflictos —
// un problema de diseño no trivial en si mismo (varios papers de la
// literatura dedican secciones completas solo a "conflict-free memory
// mapping" para NTT paralelo). Se descarto para esta fase del proyecto
// porque el costo de verificacion adicional (evitar conflictos de
// memoria entre butterflies concurrentes) es alto en relacion al
// beneficio de reducir la latencia de una unidad que ya es funcional y
// correcta. Queda documentado como extension futura si el cronograma lo
// permite tras cerrar la Fase 4 — mismo criterio que ya se aplico a la
// cola de instrucciones vectoriales (Apendice A.4) y a la reduccion
// modular especializada (isa_vectorial_kyber.docx seccion 5.3).
//
// Patron de direccionamiento (identico a kyber_ref.ntt(), traducido):
//   length: 128, 64, 32, 16, 8, 4, 2   (7 niveles, potencias de 2 decrecientes)
//   para cada length: start recorre 0, 2*length, 4*length, ... < 256
//     para cada start: j recorre start .. start+length-1
//       butterfly(r[j], r[j+length], zeta[k]); k++
//
// Interfaz: vreg_src (registro de entrada), vreg_dst (registro de
// salida — puede ser el mismo que vreg_src, operacion in-place, como
// hace kyber_ref.py). mode selecciona CT (vntt) o GS (vintt).
//
// NOTA sobre el factor de escalado de INTT: kyber_ref.intt() aplica una
// multiplicacion final por el inverso de 128 (ver models/kyber_ref.py,
// comentario "Factor de escalado") DESPUES de los 7 niveles de
// mariposas. Esta FSM cubre solo los 896 butterflies — el escalado final
// se implementa como un estado adicional (SCALE) que aplica esa
// multiplicacion constante a los 256 coeficientes antes de señalizar
// done, solo en modo INTT.

module ntt_engine (
    input  logic         clk,
    input  logic         rst_n,

    input  logic         start,       // pulso: comenzar una NTT/INTT
    input  logic         mode_intt,   // 0=NTT (Cooley-Tukey), 1=INTT (Gentleman-Sande)
    input  logic  [1:0]  vreg_src,
    input  logic  [1:0]  vreg_dst,

    output logic         busy,
    output logic         done,        // pulso de 1 ciclo al terminar

    // Interfaz hacia vreg_file (instanciado FUERA de este modulo, en la
    // unidad vectorial completa — este motor solo genera direcciones/
    // datos, no posee el banco de registros).
    output logic  [1:0]  vreg_raddr1_vreg,
    output logic  [7:0]  vreg_raddr1_coef,
    // verilator lint_off UNUSEDSIGNAL
    // vreg_rdata1/2 son buses de 16 bits (ancho fisico del vreg_file),
    // pero los coeficientes de Kyber caben en 12 bits (q=3329<4096) — los
    // 4 bits altos siempre son 0 por construccion (vreg_file solo
    // almacena valores ya reducidos a [0,q)).
    input  logic [15:0]  vreg_rdata1,
    // verilator lint_on UNUSEDSIGNAL

    output logic  [1:0]  vreg_raddr2_vreg,
    output logic  [7:0]  vreg_raddr2_coef,
    // verilator lint_off UNUSEDSIGNAL
    input  logic [15:0]  vreg_rdata2,
    // verilator lint_on UNUSEDSIGNAL

    output logic         vreg_we1,
    output logic  [1:0]  vreg_waddr1_vreg,
    output logic  [7:0]  vreg_waddr1_coef,
    output logic [15:0]  vreg_wdata1,

    output logic         vreg_we2,
    output logic  [1:0]  vreg_waddr2_vreg,
    output logic  [7:0]  vreg_waddr2_coef,
    output logic [15:0]  vreg_wdata2,

    // Interfaz hacia twiddle_rom (instanciado fuera, mismo criterio)
    output logic  [6:0]  twiddle_k,
    input  logic [11:0]  twiddle_zeta,
    // verilator lint_off UNUSEDSIGNAL
    // twiddle_zeta_inv queda deliberadamente sin conectar: la
    // implementacion real de Kyber (confirmada via kyber_ref.py, ya
    // validado contra kyber-py) usa el twiddle DIRECTO en ambos CT y GS,
    // no su inverso — ver correccion documentada en butterfly_gs.sv. Se
    // mantiene este puerto en la interfaz (en vez de eliminarlo) porque
    // twiddle_rom sigue generando la tabla de inversos, que puede servir
    // como referencia/documentacion aunque el datapath activo no la use.
    input  logic [11:0]  twiddle_zeta_inv
    // verilator lint_on UNUSEDSIGNAL
);

    // Inverso modular de 128 mod q=3329 = 3303 (verificado: pow(128,
    // Q-2, Q) == 3303 en Python, y 128*3303 mod 3329 == 1). CORRECCION:
    // el valor original en este comentario (3212) estaba mal calculado
    // y nunca se verifico contra Python antes de escribirlo — se
    // detecto al debuggear el motor NTT completo contra el modelo de
    // referencia, cuando el escalado final de INTT daba resultados
    // incorrectos pese a que los 7 niveles de butterfly ya coincidian
    // exactamente. Constante publica, sin riesgo de constant-time.
    localparam logic [11:0] INV128 = 12'd3303;

    typedef enum logic [2:0] {
        IDLE,
        COPY,      // copiar vreg_src -> vreg_dst antes de operar (in-place desde aca)
        RUN,       // ejecutando un butterfly por ciclo, sobre vreg_dst (in-place)
        SCALE,     // solo INTT: multiplicar los 256 coef por INV128
        DONE_PULSE
    } state_t;

    state_t state;

    // Contadores del patron de bucles (identico a kyber_ref.ntt()/intt())
    logic [7:0] length;      // 128, 64, ..., 2 (o 2..128 en INTT)
    logic [8:0] start_idx;   // 0, 2*length, ... (9 bits: hasta 256+)
    logic [8:0] j;           // start_idx .. start_idx+length-1
    logic [6:0] k;           // indice de twiddle, 0-127

    logic [7:0] scale_idx;   // contador del estado SCALE, 0-255
    logic [7:0] copy_idx;    // contador del estado COPY, 0-255

    // NOTA DE DISEÑO (correccion tras verificacion contra el modelo de
    // referencia): kyber_ref.ntt()/intt() operan IN-PLACE sobre un unico
    // buffer — cada butterfly lee y escribe el MISMO array, asi que el
    // resultado de un nivel es visible como entrada del siguiente nivel
    // automaticamente. Si vreg_src != vreg_dst, la primera version de
    // esta FSM leia siempre de vreg_src y escribia siempre a vreg_dst,
    // lo cual es CORRECTO solo para el primer nivel — a partir del
    // segundo nivel, los resultados del nivel anterior estan en
    // vreg_dst, no en vreg_src, y la FSM seguia leyendo el vreg_src
    // original (datos viejos). Se corrige con un estado COPY inicial
    // (copia vreg_src -> vreg_dst, 256 ciclos) seguido de operacion
    // in-place sobre vreg_dst durante los 7 niveles — replicando
    // exactamente la semantica de un unico buffer del modelo de
    // referencia.

    logic mode_intt_latched;
    logic [1:0] vreg_src_latched, vreg_dst_latched;
    // verilator lint_off UNUSEDSIGNAL
    // j_plus_length[8] nunca se usa: length maximo es 128 y j maximo es
    // 255, la suma cabe siempre en 8 bits utiles (se trunca a [7:0] al
    // usarse como indice de coeficiente, ver vreg_raddr2_coef).
    logic [8:0] j_plus_length;
    // verilator lint_on UNUSEDSIGNAL

    assign j_plus_length = j + length;

    // ------------------------------------------------------------------
    // Direcciones combinacionales hacia vreg_file y twiddle_rom, en
    // funcion del estado actual y los contadores.
    // ------------------------------------------------------------------
    always_comb begin
        // Durante RUN/SCALE, tanto lectura como escritura son sobre
        // vreg_dst_latched — operacion IN-PLACE, igual que el modelo de
        // referencia (ver nota de diseño arriba). vreg_src_latched solo
        // se usa durante COPY.
        vreg_raddr1_vreg = vreg_dst_latched;
        vreg_raddr1_coef = j[7:0];
        vreg_raddr2_vreg = vreg_dst_latched;
        vreg_raddr2_coef = j_plus_length[7:0];

        vreg_waddr1_vreg = vreg_dst_latched;
        vreg_waddr1_coef = j[7:0];
        vreg_waddr2_vreg = vreg_dst_latched;
        vreg_waddr2_coef = j_plus_length[7:0];

        twiddle_k = k;

        vreg_we1 = 1'b0;
        vreg_we2 = 1'b0;
        vreg_wdata1 = 16'd0;
        vreg_wdata2 = 16'd0;

        if (state == COPY) begin
            vreg_raddr1_vreg = vreg_src_latched;
            vreg_raddr1_coef = copy_idx;
            vreg_waddr1_vreg = vreg_dst_latched;
            vreg_waddr1_coef = copy_idx;
            vreg_we1 = 1'b1;
            vreg_wdata1 = vreg_rdata1;
        end else if (state == RUN) begin
            vreg_we1 = 1'b1;
            vreg_we2 = 1'b1;
            // Los datos de escritura se calculan combinacionalmente via
            // las instancias de butterfly_ct/gs (ver butterfly_a_out/
            // butterfly_b_out mas abajo).
            vreg_wdata1 = butterfly_a_out;
            vreg_wdata2 = butterfly_b_out;
        end else if (state == SCALE) begin
            vreg_we1 = 1'b1;
            vreg_raddr1_vreg = vreg_dst_latched;
            vreg_raddr1_coef = scale_idx;
            vreg_waddr1_vreg = vreg_dst_latched;
            vreg_waddr1_coef = scale_idx;
            vreg_wdata1 = scale_result;
        end
    end

    // ------------------------------------------------------------------
    // Butterfly combinacional: ambos modos instanciados en paralelo, se
    // selecciona el resultado segun mode_intt_latched. No hay riesgo de
    // que ambos escriban a la vez — vreg_we1/we2 son señales unicas
    // gobernadas por el estado, no por el modo.
    // ------------------------------------------------------------------
    logic [15:0] butterfly_a_out, butterfly_b_out;
    logic [11:0] ct_a_out, ct_b_out;
    logic [11:0] gs_a_out, gs_b_out;

    butterfly_ct u_butterfly_ct (
        .a      (vreg_rdata1[11:0]),
        .b      (vreg_rdata2[11:0]),
        .zeta   (twiddle_zeta),
        .a_out  (ct_a_out),
        .b_out  (ct_b_out)
    );

    // NOTA: butterfly_gs usa el mismo twiddle_zeta que butterfly_ct, NO
    // su inverso (ver correccion documentada en butterfly_gs.sv — la
    // implementacion real de Kyber no invierte el twiddle en GS, a
    // diferencia de la convencion academica estandar de FFT/NTT).
    butterfly_gs u_butterfly_gs (
        .a      (vreg_rdata1[11:0]),
        .b      (vreg_rdata2[11:0]),
        .zeta   (twiddle_zeta),
        .a_out  (gs_a_out),
        .b_out  (gs_b_out)
    );

    assign butterfly_a_out = mode_intt_latched ? {4'd0, gs_a_out} : {4'd0, ct_a_out};
    assign butterfly_b_out = mode_intt_latched ? {4'd0, gs_b_out} : {4'd0, ct_b_out};

    // Escalado final de INTT: multiplicar por INV128 mod q, vía
    // barrett_reduce (mismo patron que el resto de operaciones mod q).
    logic [23:0] scale_prod_raw;
    logic [11:0] scale_result_narrow;
    logic [15:0] scale_result;

    assign scale_prod_raw = vreg_rdata1[11:0] * INV128;

    barrett_reduce u_barrett_scale (
        .a      ({{8{1'b0}}, scale_prod_raw}),
        .result (scale_result_narrow)
    );

    assign scale_result = {4'd0, scale_result_narrow};

    // ------------------------------------------------------------------
    // FSM principal: avanza los contadores j/start_idx/length/k
    // replicando exactamente kyber_ref.ntt()/intt().
    //
    // NOTA sobre orden de k en INTT: kyber_ref.intt() recorre k desde
    // 127 hacia abajo (k=127, decrementando), a diferencia de ntt() que
    // va de 1 hacia arriba. Se replica ese mismo orden aca.
    // ------------------------------------------------------------------
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= IDLE;
            busy  <= 1'b0;
            done  <= 1'b0;
        end else begin
            done <= 1'b0;  // pulso de 1 ciclo, se resetea salvo que DONE_PULSE lo reafirme

            case (state)
                IDLE: begin
                    busy <= 1'b0;
                    if (start) begin
                        mode_intt_latched <= mode_intt;
                        vreg_src_latched  <= vreg_src;
                        vreg_dst_latched  <= vreg_dst;
                        busy  <= 1'b1;
                        state <= COPY;
                        copy_idx <= 8'd0;

                        if (mode_intt) begin
                            length <= 8'd2;
                            k      <= 7'd127;
                        end else begin
                            length <= 8'd128;
                            k      <= 7'd1;
                        end
                        start_idx <= 9'd0;
                        j         <= 9'd0;
                    end
                end

                COPY: begin
                    if (copy_idx == 8'd255) begin
                        state <= RUN;
                    end else begin
                        copy_idx <= copy_idx + 1;
                    end
                end

                RUN: begin
                    // Avanzar j dentro del bloque [start_idx, start_idx+length)
                    if (j + 1 < start_idx + length) begin
                        j <= j + 1;
                    end else begin
                        // Bloque de mariposas completo para este start_idx:
                        // avanzar el twiddle index y pasar al siguiente start_idx.
                        if (mode_intt_latched) k <= k - 1;
                        else                    k <= k + 1;

                        if (start_idx + 2 * length < 9'd256) begin
                            start_idx <= start_idx + 2 * length;
                            j         <= start_idx + 2 * length;
                        end else begin
                            // Nivel completo: avanzar length y reiniciar start_idx/j.
                            start_idx <= 9'd0;
                            j         <= 9'd0;

                            if (mode_intt_latched) begin
                                if (length == 8'd128) begin
                                    // Los 7 niveles de INTT terminaron.
                                    state     <= SCALE;
                                    scale_idx <= 8'd0;
                                end else begin
                                    length <= length << 1;  // 2,4,8,...,128
                                end
                            end else begin
                                if (length == 8'd2) begin
                                    // Los 7 niveles de NTT terminaron.
                                    state <= DONE_PULSE;
                                end else begin
                                    length <= length >> 1;  // 128,64,...,2
                                end
                            end
                        end
                    end
                end

                SCALE: begin
                    if (scale_idx == 8'd255) begin
                        state <= DONE_PULSE;
                    end else begin
                        scale_idx <= scale_idx + 1;
                    end
                end

                DONE_PULSE: begin
                    done  <= 1'b1;
                    busy  <= 1'b0;
                    state <= IDLE;
                end

                default: state <= IDLE;
            endcase
        end
    end

endmodule
