#!/usr/bin/env python3
"""
gen_mixed_scalar_vector.py

Genera sw/tests/mixed_scalar_vector.hex — firmware mixto (instrucciones
escalares RV32I + instrucciones vectoriales custom) para el testbench de
integracion final de la Fase 4 (tb_core_top_pipelined_vector.cpp).

Las instrucciones escalares (addi/sw/jal) se verificaron bit a bit contra
el toolchain real (riscv64-unknown-elf-as) antes de escribir este script
— coinciden exactamente. Las instrucciones vectoriales (vntt, vbarrett)
no las conoce el toolchain estandar (son custom, ver isa_vectorial_kyber.
docx seccion 2), asi que se codifican a mano siguiendo el formato R
documentado, y se verificaron decodificando bit a bit el resultado antes
de confiar en el archivo generado.

Programa (ver tb_core_top_pipelined_vector.cpp para el detalle de que
verifica cada parte):
  0x00  addi x1, x0, 5        (no usado, solo relleno antes del vector)
  0x04  addi x2, x0, 0        (canario, arranca en 0)
  0x08  vntt v1, v0           (operacion larga, ~1152 ciclos: COPY+7 niveles)
  0x0c-0x1c  addi x2, x2, 1  x5 (canario += 5, debe ejecutar RAPIDO,
                                 mucho antes de que vntt termine —
                                 prueba el desacople A.1)
  0x20  sw x2, 0x100(x0)      (guarda el canario temprano)
  0x24  vbarrett v1, v1       (segunda instruccion vectorial — debe
                                ESPERAR a que vntt termine, recurso
                                unico, Apendice A.4)
  0x28  addi x3, x0, 99
  0x2c  sw x3, 0x104(x0)      (solo debe aparecer en memoria DESPUES de
                                que ambas operaciones vectoriales
                                terminen — prueba la serializacion)
  0x30  halt: j halt

Uso:
    python3 gen_mixed_scalar_vector.py
"""

def r_type(funct7, rs2, rs1, funct3, rd, opcode):
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode

def i_type(imm, rs1, funct3, rd, opcode):
    imm = imm & 0xFFF
    return (imm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode

def s_type(imm, rs2, rs1, funct3, opcode):
    imm = imm & 0xFFF
    imm_11_5 = (imm >> 5) & 0x7F
    imm_4_0  = imm & 0x1F
    return (imm_11_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (imm_4_0 << 7) | opcode

def j_type(imm, rd, opcode):
    # jal encoding: imm[20|10:1|11|19:12]
    imm = imm & 0x1FFFFF
    bit20 = (imm >> 20) & 0x1
    bits10_1 = (imm >> 1) & 0x3FF
    bit11 = (imm >> 11) & 0x1
    bits19_12 = (imm >> 12) & 0xFF
    packed = (bit20 << 19) | (bits19_12 << 11) | (bit11 << 10) | (bits10_1)
    return (bit20 << 31) | (bits19_12 << 12) | (bit11 << 20) | (bits10_1 << 21) | (rd << 7) | opcode

OPCODE_ADDI = 0b0010011
OPCODE_SW   = 0b0100011
OPCODE_JAL  = 0b1101111
OPCODE_CUSTOM0 = 0b0001011

def addi(rd, rs1, imm):
    return i_type(imm, rs1, 0b000, rd, OPCODE_ADDI)

def sw(rs2, imm, rs1):
    return s_type(imm, rs2, rs1, 0b010, OPCODE_SW)

def vec_compute(funct3, vrs1, vrs2, vrd):
    # variante computo: rs1[1:0]=vrs1, rs2[1:0]=vrs2, rd[1:0]=vrd, funct7=0
    return r_type(0, vrs2, vrs1, funct3, vrd, OPCODE_CUSTOM0)

FUNCT3_VNTT = 0b010
FUNCT3_VBARRETT = 0b101

instrs = []
instrs.append(addi(1, 0, 5))          # 0x00: addi x1, x0, 5 (no usado, solo para tener algo antes)
instrs.append(addi(2, 0, 0))          # 0x04: addi x2, x0, 0   (canario)
instrs.append(vec_compute(FUNCT3_VNTT, 0, 0, 1))   # 0x08: vntt v1, v0  (v0=v1=0, ~1152 ciclos)
instrs.append(addi(2, 2, 1))          # 0x0c: addi x2, x2, 1
instrs.append(addi(2, 2, 1))          # 0x10: addi x2, x2, 1
instrs.append(addi(2, 2, 1))          # 0x14: addi x2, x2, 1
instrs.append(addi(2, 2, 1))          # 0x18: addi x2, x2, 1
instrs.append(addi(2, 2, 1))          # 0x1c: addi x2, x2, 1   (x2 = 5)
instrs.append(sw(2, 0x100, 0))        # 0x20: sw x2, 0x100(x0)  -- canario a memoria, deberia pasar rapido
instrs.append(vec_compute(FUNCT3_VBARRETT, 1, 0, 1))  # 0x24: vbarrett v1, v1  (debe esperar a que vntt termine)
instrs.append(addi(3, 0, 99))         # 0x28: addi x3, x0, 99
instrs.append(sw(3, 0x104, 0))        # 0x2c: sw x3, 0x104(x0) -- solo despues de que ambos vectoriales terminen
# halt: j halt (offset 0, salto a si mismo)
halt_addr = len(instrs) * 4
instrs.append(j_type(0, 0, OPCODE_JAL))           # 0x30: j halt (jal x0, 0 -- relativo a si mismo)

with open("mixed_scalar_vector.hex", "w") as f:
    f.write("@00000000\n")
    for instr in instrs:
        f.write(f"{instr:08x}\n")

print(f"Generadas {len(instrs)} instrucciones, halt en 0x{halt_addr:02x}")
for i, instr in enumerate(instrs):
    print(f"0x{i*4:02x}: {instr:08x}")
