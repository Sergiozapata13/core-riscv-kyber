#  vector_macros.S
# 
#  Macros de GNU as para las 8 instrucciones vectoriales custom —
#  Apendice A.3 del cronograma: "las instrucciones custom se codifican
#  mediante macros de GNU as (.macro) que envuelven el encoding .word
#  correspondiente, permitiendo mnemonics legibles (ej. vntt v1, v0) en
#  el codigo fuente en vez de valores .word crudos."
# 
#  Encoding exacto: ver isa_vectorial_kyber.docx secciones 2 y 3.
#  Formato R puro (opcode custom-0 = 0001011), SIN campo de inmediato:
# 
#    31        25 24    20 19    15 14  12 11     7 6      0
#    | funct7    |  rs2    |  rs1   |funct3|   rd   | opcode |
# 
#  Variante MEMORIA (vload/vstore): rs1[4:0] = registro escalar completo
#  con la DIRECCION DE MEMORIA DIRECTA (sin offset — este ISA no tiene
#  campo de inmediato; si el firmware necesita un offset, debe calcular
#  la direccion efectiva con una instruccion escalar ANTES de emitir
#  vload/vstore, ej. 'addi t0, base, offset' seguido de 'vload v1, t0').
#  rd[1:0] = registro vectorial destino (vload) u origen (vstore).
# 
#  Variante COMPUTO (vntt/vintt/vpmul/vbarrett/vadd/vsub): rs1[1:0] y
#  rs2[1:0] son los registros vectoriales de entrada, rd[1:0] el destino.
# 
#  Convencion de nombres: v0-v3 se definen como simbolos numericos (0-3)
#  para poder escribir 'vntt v1, v0' en vez de valores crudos — el
#  ensamblador sustituye v0..v3 por su valor numerico en tiempo de
#  ensamblado (no colisiona con los nombres ABI estandar de RISC-V,
#  que no usan el prefijo 'v').

.ifndef VECTOR_MACROS_S
.equ VECTOR_MACROS_S, 1

.equ v0, 0
.equ v1, 1
.equ v2, 2
.equ v3, 3

# x0-x31: GNU as reconoce "x5" como nombre de registro DENTRO de
# instrucciones reales, pero no como simbolo numerico evaluable en una
# expresion aritmetica (como la que usan estas macros dentro de .word)
# — se definen explicitamente para poder escribir 'vload v0, x5' de
# forma natural, igual que en cualquier lw/sw estandar.
.equ x0,   0
.equ x1,   1
.equ x2,   2
.equ x3,   3
.equ x4,   4
.equ x5,   5
.equ x6,   6
.equ x7,   7
.equ x8,   8
.equ x9,   9
.equ x10, 10
.equ x11, 11
.equ x12, 12
.equ x13, 13
.equ x14, 14
.equ x15, 15
.equ x16, 16
.equ x17, 17
.equ x18, 18
.equ x19, 19
.equ x20, 20
.equ x21, 21
.equ x22, 22
.equ x23, 23
.equ x24, 24
.equ x25, 25
.equ x26, 26
.equ x27, 27
.equ x28, 28
.equ x29, 29
.equ x30, 30
.equ x31, 31

# Alias ABI estandar de RISC-V (ver green card, tabla de "REGISTER
# NAME, USE, CALLING CONVENTION") — necesarios para poder escribir
# 'vload v0, a0' de forma natural en codigo ensamblador que sigue la
# convencion de llamada C estandar (ej. sw/asm/poly_vector_ops.S),
# igual que x0-x31 ya se definieron arriba para permitir su uso dentro
# de las expresiones aritmeticas de estas macros.
.equ zero, 0
.equ ra,   1
.equ sp,   2
.equ gp,   3
.equ tp,   4
.equ t0,   5
.equ t1,   6
.equ t2,   7
.equ s0,   8
.equ fp,   8
.equ s1,   9
.equ a0,  10
.equ a1,  11
.equ a2,  12
.equ a3,  13
.equ a4,  14
.equ a5,  15
.equ a6,  16
.equ a7,  17
.equ s2,  18
.equ s3,  19
.equ s4,  20
.equ s5,  21
.equ s6,  22
.equ s7,  23
.equ s8,  24
.equ s9,  25
.equ s10, 26
.equ s11, 27
.equ t3,  28
.equ t4,  29
.equ t5,  30
.equ t6,  31

.equ OPCODE_CUSTOM0, 0b0001011

.equ FUNCT3_VLOAD,    0b000
.equ FUNCT3_VSTORE,   0b001
.equ FUNCT3_VNTT,     0b010
.equ FUNCT3_VINTT,    0b011
.equ FUNCT3_VPMUL,    0b100
.equ FUNCT3_VBARRETT, 0b101
.equ FUNCT3_VADD,     0b110
.equ FUNCT3_VSUB,     0b111

#  ---------------------------------------------------------------------
#  Variante MEMORIA: vload vd, rs1   /   vstore vs, rs1
#  rs1 = registro escalar (x0-x31) con la direccion directa.
#  rd[1:0] = registro vectorial (v0-v3); rd[4:2], rs2 y funct7 en 0.
#  ---------------------------------------------------------------------
.macro vload vd, rs1
    .word (0 << 25) | (0 << 20) | ((\rs1) << 15) | (FUNCT3_VLOAD << 12) | ((\vd) << 7) | OPCODE_CUSTOM0
.endm

.macro vstore vs, rs1
    .word (0 << 25) | (0 << 20) | ((\rs1) << 15) | (FUNCT3_VSTORE << 12) | ((\vs) << 7) | OPCODE_CUSTOM0
.endm

#  ---------------------------------------------------------------------
#  Variante COMPUTO — un operando: vntt/vintt/vbarrett vd, vs
#  rs1[1:0] = vreg entrada; rs2 y funct7 en 0.
#  ---------------------------------------------------------------------
.macro vntt vd, vs1
    .word (0 << 25) | (0 << 20) | ((\vs1) << 15) | (FUNCT3_VNTT << 12) | ((\vd) << 7) | OPCODE_CUSTOM0
.endm

.macro vintt vd, vs1
    .word (0 << 25) | (0 << 20) | ((\vs1) << 15) | (FUNCT3_VINTT << 12) | ((\vd) << 7) | OPCODE_CUSTOM0
.endm

.macro vbarrett vd, vs1
    .word (0 << 25) | (0 << 20) | ((\vs1) << 15) | (FUNCT3_VBARRETT << 12) | ((\vd) << 7) | OPCODE_CUSTOM0
.endm

#  ---------------------------------------------------------------------
#  Variante COMPUTO — dos operandos: vpmul/vadd/vsub vd, vs1, vs2
#  rs1[1:0] = vreg entrada A; rs2[1:0] = vreg entrada B; funct7 en 0.
#  ---------------------------------------------------------------------
.macro vpmul vd, vs1, vs2
    .word (0 << 25) | ((\vs2) << 20) | ((\vs1) << 15) | (FUNCT3_VPMUL << 12) | ((\vd) << 7) | OPCODE_CUSTOM0
.endm

.macro vadd vd, vs1, vs2
    .word (0 << 25) | ((\vs2) << 20) | ((\vs1) << 15) | (FUNCT3_VADD << 12) | ((\vd) << 7) | OPCODE_CUSTOM0
.endm

.macro vsub vd, vs1, vs2
    .word (0 << 25) | ((\vs2) << 20) | ((\vs1) << 15) | (FUNCT3_VSUB << 12) | ((\vd) << 7) | OPCODE_CUSTOM0
.endm

.endif  #  VECTOR_MACROS_S
