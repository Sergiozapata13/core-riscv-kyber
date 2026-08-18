#!/usr/bin/env python3
"""
gen_cbd_test_vectors.py

Genera sw/tests/native/test_cbd_vectors.h — vectores de entrada aleatoria
y su polinomio CBD esperado, para eta=2 y eta=3, derivados de
kyber_ref.cbd() (ya validado contra kyber-py, 40/40 casos en la Fase 5).

Uso:
    python3 gen_cbd_test_vectors.py > ../sw/tests/native/test_cbd_vectors.h
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import cbd, Q

SEED_ETA3 = 2027
SEED_ETA2 = 2028


def emit_array(name, values, is_bytes=False):
    ctype = "uint8_t" if is_bytes else "int16_t"
    print(f"static const {ctype} {name}[{len(values)}] = {{")
    print("    " + ", ".join(str(v) for v in values))
    print("};")
    print()


def main():
    random.seed(SEED_ETA3)
    input_eta3 = bytes(random.randint(0, 255) for _ in range(64 * 3))
    output_eta3 = cbd(input_eta3, 3)

    random.seed(SEED_ETA2)
    input_eta2 = bytes(random.randint(0, 255) for _ in range(64 * 2))
    output_eta2 = cbd(input_eta2, 2)

    assert all(0 <= c < Q for c in output_eta3)
    assert all(0 <= c < Q for c in output_eta2)

    print("// test_cbd_vectors.h")
    print("//")
    print("// Vectores de prueba para cbd.c, generados desde kyber_ref.cbd()")
    print("// (ya validado contra kyber-py, 40/40 casos) — no transcritos a mano.")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#include <stdint.h>")
    print()
    emit_array("test_cbd_input_eta3", list(input_eta3), is_bytes=True)
    emit_array("test_cbd_expected_eta3", output_eta3)
    emit_array("test_cbd_input_eta2", list(input_eta2), is_bytes=True)
    emit_array("test_cbd_expected_eta2", output_eta2)


if __name__ == "__main__":
    main()
