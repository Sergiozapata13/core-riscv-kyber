#!/usr/bin/env python3
"""
gen_twiddle_rom.py

Genera rtl/vector/twiddle_rom.sv — la ROM de twiddle factors (zetas y sus
inversos modulares) usada por la unidad NTT/INTT (Fase 4).

Los valores vienen directamente de kyber_ref.ZETAS (el mismo modelo de
referencia validado en dos capas durante la Fase 3, incluyendo contra
kyber-py) — no se transcriben a mano en ningun punto, evitando el riesgo
de un error de transcripcion en la pieza que alimenta CADA butterfly de
la NTT.

Uso:
    python3 gen_twiddle_rom.py > ../rtl/vector/twiddle_rom.sv
"""

import sys

sys.path.insert(0, ".")
from kyber_ref import ZETAS, Q


def main():
    zeta_invs = [pow(z, Q - 2, Q) for z in ZETAS]

    print("// twiddle_rom.sv")
    print("//")
    print("// ROM de twiddle factors — Fase 4 (Kyber, generada por")
    print("// models/gen_twiddle_rom.py, NO EDITAR A MANO).")
    print("//")
    print("// Contiene los 128 valores de zeta (potencias del root of unity")
    print("// primitivo, zeta=17, en el orden bit-reversed que usa Cooley-Tukey)")
    print("// y sus 128 inversos modulares (para Gentleman-Sande). Ambas tablas")
    print("// vienen DIRECTAMENTE de kyber_ref.ZETAS — el mismo modelo de")
    print("// referencia validado en la Fase 3 (incluyendo verificacion cruzada")
    print("// exacta, valor a valor, contra kyber-py) — no se transcriben a mano")
    print("// para evitar el riesgo de un error de transcripcion en la pieza que")
    print("// alimenta cada uno de los 896 butterflies de una NTT completa.")
    print("//")
    print("// Los valores son PUBLICOS y fijos por el estandar (ver")
    print("// isa_vectorial_kyber.docx seccion 6.4) — no hay riesgo de")
    print("// constant-time asociado a su acceso, la posicion en la tabla")
    print("// depende solo del nivel/indice de la mariposa (ambos publicos).")
    print()
    print("module twiddle_rom (")
    print("    input  logic [6:0]  k,          // indice 0-127")
    print("    output logic [11:0] zeta,       // ZETAS[k]")
    print("    output logic [11:0] zeta_inv    // inverso modular de ZETAS[k]")
    print(");")
    print()
    print("    logic [11:0] zeta_table [128];")
    print("    logic [11:0] zeta_inv_table [128];")
    print()
    print("    initial begin")
    for i, z in enumerate(ZETAS):
        print(f"        zeta_table[{i}] = 12'd{z};")
    print()
    for i, zi in enumerate(zeta_invs):
        print(f"        zeta_inv_table[{i}] = 12'd{zi};")
    print("    end")
    print()
    print("    assign zeta     = zeta_table[k];")
    print("    assign zeta_inv = zeta_inv_table[k];")
    print()
    print("endmodule")


if __name__ == "__main__":
    main()
