#!/usr/bin/env python3
"""
gen_barrett_engine_testvectors.py

Genera tb/barrett_engine_testvectors.inc — un polinomio de 256
coeficientes CRUDOS (fuera de [0,q), como resultarían de una
multiplicación sin reducir — el caso de uso real de vbarrett según
isa_vectorial_kyber.docx) y su reducción esperada, derivados de
kyber_ref.barrett_reduce_poly() (oráculo validado en la Fase 3).

Uso:
    python3 gen_barrett_engine_testvectors.py > ../tb/barrett_engine_testvectors.inc
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import barrett_reduce_poly, Q

RNG_SEED = 999


def emit_array(name, values):
    print(f"static const uint16_t {name}[256] = {{")
    print("    " + ", ".join(str(v) for v in values))
    print("};")


def main():
    random.seed(RNG_SEED)
    # Coeficientes crudos acotados a 16 bits (0-65535) — el ancho fisico
    # del vreg_file. Este rango cae dentro del Caso A de barrett_reduce
    # (verificado exhaustivamente en [0,(q-1)^2] ~ [0, 11M]), asi que
    # sigue siendo representativo del uso real (reducir un valor crudo
    # tras una operacion previa sin reducir), acotado a lo que el banco
    # vectorial puede fisicamente almacenar.
    raw = [random.randint(0, 65535) for _ in range(256)]
    reduced = barrett_reduce_poly(raw)

    print("// Generado por models/gen_barrett_engine_testvectors.py")
    print("// derivado de kyber_ref.barrett_reduce_poly(), oráculo validado en Fase 3.")
    print("// Coeficientes crudos en [0,65535] (acotado al ancho fisico de vreg_file,")
    print("// 16 bits) — dentro del rango verificado exhaustivamente para barrett_reduce.")
    print("// NO EDITAR A MANO.")
    print()
    emit_array("test_barrett_raw", raw)
    print()
    emit_array("test_barrett_expected", reduced)


if __name__ == "__main__":
    main()
