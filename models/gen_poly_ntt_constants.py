#!/usr/bin/env python3
"""
gen_poly_ntt_constants.py

Genera sw/lib/poly_ntt_constants.h — la tabla de 128 twiddle factors
(ZETAS) para la NTT escalar en C, reusando DIRECTAMENTE kyber_ref.ZETAS
(la misma tabla ya generada por gen_zetas.py y validada exhaustivamente
contra kyber-py en la Fase 3, y ya usada por rtl/vector/twiddle_rom.sv
en la Fase 4) — no se transcribe ni se regenera de forma independiente,
para garantizar que la version escalar C y la version vectorial RTL
usan exactamente la misma fuente de verdad.

Uso:
    python3 gen_poly_ntt_constants.py > ../sw/lib/poly_ntt_constants.h
"""

import sys

sys.path.insert(0, ".")
from kyber_ref import ZETAS, Q


def main():
    assert len(ZETAS) == 128
    assert all(0 <= z < Q for z in ZETAS)

    print("// poly_ntt_constants.h")
    print("//")
    print("// Tabla de 128 twiddle factors para la NTT escalar en C — Fase 5.")
    print("// Identica a kyber_ref.ZETAS (Fase 3) y a rtl/vector/twiddle_rom.sv")
    print("// (Fase 4): las tres se derivan de la MISMA fuente")
    print("// (models/gen_zetas.py), nunca transcritas independientemente, para")
    print("// garantizar que la version escalar (este archivo) y la version")
    print("// vectorial (RTL) usan exactamente los mismos valores.")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#ifndef POLY_NTT_CONSTANTS_H")
    print("#define POLY_NTT_CONSTANTS_H")
    print()
    print("#include <stdint.h>")
    print()
    print("static const int16_t POLY_NTT_ZETAS[128] = {")
    print("    " + ", ".join(str(z) for z in ZETAS))
    print("};")
    print()
    print("#endif // POLY_NTT_CONSTANTS_H")


if __name__ == "__main__":
    main()
