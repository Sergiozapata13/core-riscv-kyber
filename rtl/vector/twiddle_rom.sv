// twiddle_rom.sv
//
// ROM de twiddle factors — Fase 4 (Kyber, generada por
// models/gen_twiddle_rom.py, NO EDITAR A MANO).
//
// Contiene los 128 valores de zeta (potencias del root of unity
// primitivo, zeta=17, en el orden bit-reversed que usa Cooley-Tukey)
// y sus 128 inversos modulares (para Gentleman-Sande). Ambas tablas
// vienen DIRECTAMENTE de kyber_ref.ZETAS — el mismo modelo de
// referencia validado en la Fase 3 (incluyendo verificacion cruzada
// exacta, valor a valor, contra kyber-py) — no se transcriben a mano
// para evitar el riesgo de un error de transcripcion en la pieza que
// alimenta cada uno de los 896 butterflies de una NTT completa.
//
// Los valores son PUBLICOS y fijos por el estandar (ver
// isa_vectorial_kyber.docx seccion 6.4) — no hay riesgo de
// constant-time asociado a su acceso, la posicion en la tabla
// depende solo del nivel/indice de la mariposa (ambos publicos).

module twiddle_rom (
    input  logic [6:0]  k,          // indice 0-127
    output logic [11:0] zeta,       // ZETAS[k]
    output logic [11:0] zeta_inv    // inverso modular de ZETAS[k]
);

    logic [11:0] zeta_table [128];
    logic [11:0] zeta_inv_table [128];

    initial begin
        zeta_table[0] = 12'd1;
        zeta_table[1] = 12'd1729;
        zeta_table[2] = 12'd2580;
        zeta_table[3] = 12'd3289;
        zeta_table[4] = 12'd2642;
        zeta_table[5] = 12'd630;
        zeta_table[6] = 12'd1897;
        zeta_table[7] = 12'd848;
        zeta_table[8] = 12'd1062;
        zeta_table[9] = 12'd1919;
        zeta_table[10] = 12'd193;
        zeta_table[11] = 12'd797;
        zeta_table[12] = 12'd2786;
        zeta_table[13] = 12'd3260;
        zeta_table[14] = 12'd569;
        zeta_table[15] = 12'd1746;
        zeta_table[16] = 12'd296;
        zeta_table[17] = 12'd2447;
        zeta_table[18] = 12'd1339;
        zeta_table[19] = 12'd1476;
        zeta_table[20] = 12'd3046;
        zeta_table[21] = 12'd56;
        zeta_table[22] = 12'd2240;
        zeta_table[23] = 12'd1333;
        zeta_table[24] = 12'd1426;
        zeta_table[25] = 12'd2094;
        zeta_table[26] = 12'd535;
        zeta_table[27] = 12'd2882;
        zeta_table[28] = 12'd2393;
        zeta_table[29] = 12'd2879;
        zeta_table[30] = 12'd1974;
        zeta_table[31] = 12'd821;
        zeta_table[32] = 12'd289;
        zeta_table[33] = 12'd331;
        zeta_table[34] = 12'd3253;
        zeta_table[35] = 12'd1756;
        zeta_table[36] = 12'd1197;
        zeta_table[37] = 12'd2304;
        zeta_table[38] = 12'd2277;
        zeta_table[39] = 12'd2055;
        zeta_table[40] = 12'd650;
        zeta_table[41] = 12'd1977;
        zeta_table[42] = 12'd2513;
        zeta_table[43] = 12'd632;
        zeta_table[44] = 12'd2865;
        zeta_table[45] = 12'd33;
        zeta_table[46] = 12'd1320;
        zeta_table[47] = 12'd1915;
        zeta_table[48] = 12'd2319;
        zeta_table[49] = 12'd1435;
        zeta_table[50] = 12'd807;
        zeta_table[51] = 12'd452;
        zeta_table[52] = 12'd1438;
        zeta_table[53] = 12'd2868;
        zeta_table[54] = 12'd1534;
        zeta_table[55] = 12'd2402;
        zeta_table[56] = 12'd2647;
        zeta_table[57] = 12'd2617;
        zeta_table[58] = 12'd1481;
        zeta_table[59] = 12'd648;
        zeta_table[60] = 12'd2474;
        zeta_table[61] = 12'd3110;
        zeta_table[62] = 12'd1227;
        zeta_table[63] = 12'd910;
        zeta_table[64] = 12'd17;
        zeta_table[65] = 12'd2761;
        zeta_table[66] = 12'd583;
        zeta_table[67] = 12'd2649;
        zeta_table[68] = 12'd1637;
        zeta_table[69] = 12'd723;
        zeta_table[70] = 12'd2288;
        zeta_table[71] = 12'd1100;
        zeta_table[72] = 12'd1409;
        zeta_table[73] = 12'd2662;
        zeta_table[74] = 12'd3281;
        zeta_table[75] = 12'd233;
        zeta_table[76] = 12'd756;
        zeta_table[77] = 12'd2156;
        zeta_table[78] = 12'd3015;
        zeta_table[79] = 12'd3050;
        zeta_table[80] = 12'd1703;
        zeta_table[81] = 12'd1651;
        zeta_table[82] = 12'd2789;
        zeta_table[83] = 12'd1789;
        zeta_table[84] = 12'd1847;
        zeta_table[85] = 12'd952;
        zeta_table[86] = 12'd1461;
        zeta_table[87] = 12'd2687;
        zeta_table[88] = 12'd939;
        zeta_table[89] = 12'd2308;
        zeta_table[90] = 12'd2437;
        zeta_table[91] = 12'd2388;
        zeta_table[92] = 12'd733;
        zeta_table[93] = 12'd2337;
        zeta_table[94] = 12'd268;
        zeta_table[95] = 12'd641;
        zeta_table[96] = 12'd1584;
        zeta_table[97] = 12'd2298;
        zeta_table[98] = 12'd2037;
        zeta_table[99] = 12'd3220;
        zeta_table[100] = 12'd375;
        zeta_table[101] = 12'd2549;
        zeta_table[102] = 12'd2090;
        zeta_table[103] = 12'd1645;
        zeta_table[104] = 12'd1063;
        zeta_table[105] = 12'd319;
        zeta_table[106] = 12'd2773;
        zeta_table[107] = 12'd757;
        zeta_table[108] = 12'd2099;
        zeta_table[109] = 12'd561;
        zeta_table[110] = 12'd2466;
        zeta_table[111] = 12'd2594;
        zeta_table[112] = 12'd2804;
        zeta_table[113] = 12'd1092;
        zeta_table[114] = 12'd403;
        zeta_table[115] = 12'd1026;
        zeta_table[116] = 12'd1143;
        zeta_table[117] = 12'd2150;
        zeta_table[118] = 12'd2775;
        zeta_table[119] = 12'd886;
        zeta_table[120] = 12'd1722;
        zeta_table[121] = 12'd1212;
        zeta_table[122] = 12'd1874;
        zeta_table[123] = 12'd1029;
        zeta_table[124] = 12'd2110;
        zeta_table[125] = 12'd2935;
        zeta_table[126] = 12'd885;
        zeta_table[127] = 12'd2154;

        zeta_inv_table[0] = 12'd1;
        zeta_inv_table[1] = 12'd1600;
        zeta_inv_table[2] = 12'd40;
        zeta_inv_table[3] = 12'd749;
        zeta_inv_table[4] = 12'd2481;
        zeta_inv_table[5] = 12'd1432;
        zeta_inv_table[6] = 12'd2699;
        zeta_inv_table[7] = 12'd687;
        zeta_inv_table[8] = 12'd1583;
        zeta_inv_table[9] = 12'd2760;
        zeta_inv_table[10] = 12'd69;
        zeta_inv_table[11] = 12'd543;
        zeta_inv_table[12] = 12'd2532;
        zeta_inv_table[13] = 12'd3136;
        zeta_inv_table[14] = 12'd1410;
        zeta_inv_table[15] = 12'd2267;
        zeta_inv_table[16] = 12'd2508;
        zeta_inv_table[17] = 12'd1355;
        zeta_inv_table[18] = 12'd450;
        zeta_inv_table[19] = 12'd936;
        zeta_inv_table[20] = 12'd447;
        zeta_inv_table[21] = 12'd2794;
        zeta_inv_table[22] = 12'd1235;
        zeta_inv_table[23] = 12'd1903;
        zeta_inv_table[24] = 12'd1996;
        zeta_inv_table[25] = 12'd1089;
        zeta_inv_table[26] = 12'd3273;
        zeta_inv_table[27] = 12'd283;
        zeta_inv_table[28] = 12'd1853;
        zeta_inv_table[29] = 12'd1990;
        zeta_inv_table[30] = 12'd882;
        zeta_inv_table[31] = 12'd3033;
        zeta_inv_table[32] = 12'd2419;
        zeta_inv_table[33] = 12'd2102;
        zeta_inv_table[34] = 12'd219;
        zeta_inv_table[35] = 12'd855;
        zeta_inv_table[36] = 12'd2681;
        zeta_inv_table[37] = 12'd1848;
        zeta_inv_table[38] = 12'd712;
        zeta_inv_table[39] = 12'd682;
        zeta_inv_table[40] = 12'd927;
        zeta_inv_table[41] = 12'd1795;
        zeta_inv_table[42] = 12'd461;
        zeta_inv_table[43] = 12'd1891;
        zeta_inv_table[44] = 12'd2877;
        zeta_inv_table[45] = 12'd2522;
        zeta_inv_table[46] = 12'd1894;
        zeta_inv_table[47] = 12'd1010;
        zeta_inv_table[48] = 12'd1414;
        zeta_inv_table[49] = 12'd2009;
        zeta_inv_table[50] = 12'd3296;
        zeta_inv_table[51] = 12'd464;
        zeta_inv_table[52] = 12'd2697;
        zeta_inv_table[53] = 12'd816;
        zeta_inv_table[54] = 12'd1352;
        zeta_inv_table[55] = 12'd2679;
        zeta_inv_table[56] = 12'd1274;
        zeta_inv_table[57] = 12'd1052;
        zeta_inv_table[58] = 12'd1025;
        zeta_inv_table[59] = 12'd2132;
        zeta_inv_table[60] = 12'd1573;
        zeta_inv_table[61] = 12'd76;
        zeta_inv_table[62] = 12'd2998;
        zeta_inv_table[63] = 12'd3040;
        zeta_inv_table[64] = 12'd1175;
        zeta_inv_table[65] = 12'd2444;
        zeta_inv_table[66] = 12'd394;
        zeta_inv_table[67] = 12'd1219;
        zeta_inv_table[68] = 12'd2300;
        zeta_inv_table[69] = 12'd1455;
        zeta_inv_table[70] = 12'd2117;
        zeta_inv_table[71] = 12'd1607;
        zeta_inv_table[72] = 12'd2443;
        zeta_inv_table[73] = 12'd554;
        zeta_inv_table[74] = 12'd1179;
        zeta_inv_table[75] = 12'd2186;
        zeta_inv_table[76] = 12'd2303;
        zeta_inv_table[77] = 12'd2926;
        zeta_inv_table[78] = 12'd2237;
        zeta_inv_table[79] = 12'd525;
        zeta_inv_table[80] = 12'd735;
        zeta_inv_table[81] = 12'd863;
        zeta_inv_table[82] = 12'd2768;
        zeta_inv_table[83] = 12'd1230;
        zeta_inv_table[84] = 12'd2572;
        zeta_inv_table[85] = 12'd556;
        zeta_inv_table[86] = 12'd3010;
        zeta_inv_table[87] = 12'd2266;
        zeta_inv_table[88] = 12'd1684;
        zeta_inv_table[89] = 12'd1239;
        zeta_inv_table[90] = 12'd780;
        zeta_inv_table[91] = 12'd2954;
        zeta_inv_table[92] = 12'd109;
        zeta_inv_table[93] = 12'd1292;
        zeta_inv_table[94] = 12'd1031;
        zeta_inv_table[95] = 12'd1745;
        zeta_inv_table[96] = 12'd2688;
        zeta_inv_table[97] = 12'd3061;
        zeta_inv_table[98] = 12'd992;
        zeta_inv_table[99] = 12'd2596;
        zeta_inv_table[100] = 12'd941;
        zeta_inv_table[101] = 12'd892;
        zeta_inv_table[102] = 12'd1021;
        zeta_inv_table[103] = 12'd2390;
        zeta_inv_table[104] = 12'd642;
        zeta_inv_table[105] = 12'd1868;
        zeta_inv_table[106] = 12'd2377;
        zeta_inv_table[107] = 12'd1482;
        zeta_inv_table[108] = 12'd1540;
        zeta_inv_table[109] = 12'd540;
        zeta_inv_table[110] = 12'd1678;
        zeta_inv_table[111] = 12'd1626;
        zeta_inv_table[112] = 12'd279;
        zeta_inv_table[113] = 12'd314;
        zeta_inv_table[114] = 12'd1173;
        zeta_inv_table[115] = 12'd2573;
        zeta_inv_table[116] = 12'd3096;
        zeta_inv_table[117] = 12'd48;
        zeta_inv_table[118] = 12'd667;
        zeta_inv_table[119] = 12'd1920;
        zeta_inv_table[120] = 12'd2229;
        zeta_inv_table[121] = 12'd1041;
        zeta_inv_table[122] = 12'd2606;
        zeta_inv_table[123] = 12'd1692;
        zeta_inv_table[124] = 12'd680;
        zeta_inv_table[125] = 12'd2746;
        zeta_inv_table[126] = 12'd568;
        zeta_inv_table[127] = 12'd3312;
    end

    assign zeta     = zeta_table[k];
    assign zeta_inv = zeta_inv_table[k];

endmodule
