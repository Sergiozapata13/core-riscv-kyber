// vector_control.sv
//
// Unidad de control vectorial — Fase 4 (integracion final).
//
// Decodifica las instrucciones vectoriales custom (opcode 0001011,
// custom-0) y despacha a vector_unit.sv. Vive en EX, no en ID —
// decision de diseño (ver discusion, Fase 4): id_ex_reg YA propaga
// opcode/funct3/rs1/rs2/rd completos hasta EX (los usa el mux de
// operando de la ALU para AUIPC/LUI/JALR), asi que no hace falta
// decodificar de nuevo en ID ni agregar campos nuevos al registro de
// segmentacion — toda la informacion ya esta disponible en EX.
//
// Ademas, control.sv YA trata el opcode custom-0 como "no reconocido":
// todas las señales de control escalares (reg_write, mem_read,
// mem_write) quedan en 0 por defecto para esa instruccion — el pipeline
// escalar ya se comporta de forma segura/inerte ante una instruccion
// vectorial sin necesitar ningun cambio a control.sv ni a los registros
// de segmentacion.
//
// Encoding (ver isa_vectorial_kyber.docx seccion 2):
//   Variante MEMORIA (vload/vstore, funct3=000/001):
//     rs1[4:0] = registro escalar con la direccion (puntero completo)
//     rd[1:0]  = registro vectorial destino (vload) u origen (vstore)
//   Variante COMPUTO (vntt/vintt/vpmul/vbarrett/vadd/vsub, funct3=010-111):
//     rs1[1:0] = registro vectorial de entrada A
//     rs2[1:0] = registro vectorial de entrada B (solo vpmul/vadd/vsub)
//     rd[1:0]  = registro vectorial destino
//
// La direccion escalar para vload/vstore NO viene de la instruccion —
// viene del VALOR de rs1, que el pipeline ya lee y forwardea (ver
// core_top_pipelined.sv, señal ex_rs1_fwd) exactamente igual que
// cualquier lw/sw — se reusa ese mismo camino de datos, sin puerto de
// lectura nuevo al regfile escalar.

module vector_control (
    input  logic         idex_valid,
    input  logic  [6:0]  idex_opcode,
    input  logic  [2:0]  idex_funct3,
    // verilator lint_off UNUSEDSIGNAL
    // Solo los 2 bits bajos de rs1/rs2/rd son selectores de registro
    // vectorial (4 posiciones: v0-v3) en la variante COMPUTO del
    // encoding — los bits altos quedan reservados por
    // isa_vectorial_kyber.docx seccion 2.4 y no se consultan aca. Para
    // rs1 en la variante MEMORIA (vload/vstore), el valor completo de
    // 5 bits SI importa, pero solo para identificar el registro
    // escalar en la etapa de decodificacion original (control.sv/
    // id_ex_reg) — este modulo no necesita el numero de registro
    // escalar en si, solo su VALOR ya resuelto (ex_rs1_fwd).
    input  logic  [4:0]  idex_rs1,
    input  logic  [4:0]  idex_rs2,
    input  logic  [4:0]  idex_rd,
    // verilator lint_on UNUSEDSIGNAL
    input  logic [31:0]  ex_rs1_fwd,     // rs1_data ya forwardeado (direccion escalar)

    input  logic         vector_unit_busy,  // para no reafirmar 'start' mientras la unidad esta ocupada

    output logic         is_vector_instr,   // 1 si la instruccion en EX es vectorial (opcode custom-0)
    output logic         vec_start,
    output logic  [2:0]  vec_funct3,
    output logic  [1:0]  vec_vreg_rs1,
    output logic  [1:0]  vec_vreg_rs2,
    output logic  [1:0]  vec_vreg_rd,
    output logic [31:0]  vec_scalar_addr
);

    localparam logic [6:0] OPCODE_CUSTOM0 = 7'b0001011;

    assign is_vector_instr = idex_valid && (idex_opcode == OPCODE_CUSTOM0);

    // vec_start es un pulso de 1 ciclo: solo se afirma mientras la
    // instruccion vectorial esta en EX Y la unidad no esta ya ocupada
    // (evita reafirmar start en cada ciclo si, por algun motivo, la
    // instruccion permaneciera varios ciclos en EX — en el diseño
    // actual del pipeline esto no deberia pasar sin un stall explicito,
    // pero la señal queda protegida igual, mismo criterio defensivo
    // que "una sola correccion" en barrett_reduce: mejor una guarda
    // barata que un supuesto no verificado).
    assign vec_start = is_vector_instr && !vector_unit_busy;

    assign vec_funct3 = idex_funct3;

    // Variante MEMORIA vs COMPUTO: en ambas, rd[1:0] es siempre el
    // registro vectorial destino — no hace falta distinguir variantes
    // para ese campo. rs1/rs2 vectoriales solo aplican en la variante
    // computo; en la variante memoria, rs1 completo es la direccion
    // escalar (manejada por separado via vec_scalar_addr) y rs2 no se
    // usa — vector_unit.sv ya ignora vreg_rs2 para vload/vstore (ver
    // el mux de vector_unit.sv, caso default).
    assign vec_vreg_rs1 = idex_rs1[1:0];
    assign vec_vreg_rs2 = idex_rs2[1:0];
    assign vec_vreg_rd  = idex_rd[1:0];

    assign vec_scalar_addr = ex_rs1_fwd;

endmodule
