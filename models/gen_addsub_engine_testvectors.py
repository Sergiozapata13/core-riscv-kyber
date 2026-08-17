#!/usr/bin/env python3
"""
gen_addsub_engine_testvectors.py

Genera tb/addsub_engine_testvectors.inc — dos polinomios de 256
coeficientes y su suma/resta esperada, derivados de
kyber_ref.poly_add()/poly_sub() (oráculo validado en la Fase 3).

Uso:
    python3 gen_addsub_engine_testvectors.py > ../tb/addsub_engine_testvectors.inc
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import poly_add, poly_sub, Q

RNG_SEED = 888


def emit_array(name, values):
    print(f"static const uint16_t {name}[256] = {{")
    print("    " + ", ".join(str(v) for v in values))
    print("};")


def main():
    random.seed(RNG_SEED)
    a = [random.randint(0, Q - 1) for _ in range(256)]
    b = [random.randint(0, Q - 1) for _ in range(256)]
    add_result = poly_add(a, b)
    sub_result = poly_sub(a, b)

    print("// Generado por models/gen_addsub_engine_testvectors.py")
    print("// derivado de kyber_ref.poly_add()/poly_sub(), oráculo validado en Fase 3.")
    print("// NO EDITAR A MANO.")
    print()
    emit_array("test_addsub_a", a)
    print()
    emit_array("test_addsub_b", b)
    print()
    emit_array("test_add_expected", add_result)
    print()
    emit_array("test_sub_expected", sub_result)


if __name__ == "__main__":
    main()
