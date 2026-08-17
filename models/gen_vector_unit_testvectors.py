#!/usr/bin/env python3
"""
gen_vector_unit_testvectors.py

Genera tb/vector_unit_testvectors.inc — un conjunto de datos que ejercita
las 8 instrucciones de la ISA vectorial (vload/vstore implicitos via el
testbench, vntt, vintt, vpmul, vadd, vsub, vbarrett) sobre vector_unit.sv,
derivado de kyber_ref.py — el mismo oraculo validado en la Fase 3.

Uso:
    python3 gen_vector_unit_testvectors.py > ../tb/vector_unit_testvectors.inc
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import ntt, intt, poly_pointwise_mul, poly_add, poly_sub, barrett_reduce_poly, Q

RNG_SEED = 2026


def emit(name, values):
    print(f"static const uint16_t {name}[256] = {{")
    print("    " + ", ".join(str(v) for v in values))
    print("};")
    print()


def main():
    random.seed(RNG_SEED)

    a = [random.randint(0, Q - 1) for _ in range(256)]
    b = [random.randint(0, Q - 1) for _ in range(256)]

    a_ntt = ntt(a)
    b_ntt = ntt(b)
    pmul_result = poly_pointwise_mul(a_ntt, b_ntt)
    intt_result = intt(pmul_result)
    add_result = poly_add(a, b)
    sub_result = poly_sub(a, b)
    raw = [random.randint(0, 65535) for _ in range(256)]
    barrett_result = barrett_reduce_poly(raw)

    print("// Generado por models/gen_vector_unit_testvectors.py")
    print("// Ejercita las 8 instrucciones de la ISA vectorial en secuencia,")
    print("// derivado de kyber_ref.py (oráculo validado en Fase 3).")
    print("// NO EDITAR A MANO.")
    print()
    emit("tv_poly_a", a)
    emit("tv_poly_b", b)
    emit("tv_ntt_a_expected", a_ntt)
    emit("tv_ntt_b_expected", b_ntt)
    emit("tv_vpmul_expected", pmul_result)
    emit("tv_vintt_expected", intt_result)
    emit("tv_vadd_expected", add_result)
    emit("tv_vsub_expected", sub_result)
    emit("tv_barrett_raw", raw)
    emit("tv_barrett_expected", barrett_result)


if __name__ == "__main__":
    main()
