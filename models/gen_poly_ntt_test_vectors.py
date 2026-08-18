#!/usr/bin/env python3
"""
gen_poly_ntt_test_vectors.py

Genera sw/tests/native/test_poly_ntt_vectors.h — vectores de prueba para
poly_ntt.c (NTT, INTT, pointwise-mul, add, sub), derivados de
kyber_ref.py (ya validado contra kyber-py en la Fase 3/4).

Uso:
    python3 gen_poly_ntt_test_vectors.py > ../sw/tests/native/test_poly_ntt_vectors.h
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import ntt, intt, poly_pointwise_mul, poly_add, poly_sub, Q

SEED = 9099


def emit(name, vals):
    print(f"static const int16_t {name}[256] = {{")
    print("    " + ", ".join(str(v) for v in vals))
    print("};")
    print()


def main():
    random.seed(SEED)
    a = [random.randint(0, Q - 1) for _ in range(256)]
    b = [random.randint(0, Q - 1) for _ in range(256)]

    a_ntt = ntt(a)
    b_ntt = ntt(b)
    roundtrip = intt(a_ntt)
    pmul = poly_pointwise_mul(a_ntt, b_ntt)
    padd = poly_add(a, b)
    psub = poly_sub(a, b)

    print("// test_poly_ntt_vectors.h")
    print("//")
    print("// Vectores de prueba para poly_ntt.c, generados desde kyber_ref.py")
    print("// (ya validado contra kyber-py) — no transcritos a mano.")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#include <stdint.h>")
    print()
    emit("test_a", a)
    emit("test_b", b)
    emit("test_a_ntt_expected", a_ntt)
    emit("test_roundtrip_expected", roundtrip)
    emit("test_pmul_expected", pmul)
    emit("test_padd_expected", padd)
    emit("test_psub_expected", psub)


if __name__ == "__main__":
    main()
