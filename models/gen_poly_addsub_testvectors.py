#!/usr/bin/env python3
"""
gen_poly_addsub_testvectors.py

Genera casos de test para poly_addsub.sv (Fase 4, vadd/vsub), usando
kyber_ref.barrett_reduce() directamente para calcular a+b y a-b mod q —
mismo oráculo validado en la Fase 3, sin reimplementar la fórmula.

Uso:
    python3 gen_poly_addsub_testvectors.py > ../tb/poly_addsub_testvectors.inc
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import barrett_reduce, Q

RNG_SEED = 555
N_RANDOM = 300


def main():
    random.seed(RNG_SEED)

    cases = []
    cases += [(0, 0), (0, 1), (1, 0), (Q - 1, Q - 1), (Q - 1, 1), (1, Q - 1)]
    for _ in range(N_RANDOM):
        a = random.randint(0, Q - 1)
        b = random.randint(0, Q - 1)
        cases.append((a, b))

    print(f"// Generado por models/gen_poly_addsub_testvectors.py — {len(cases)} casos")
    print("// derivados de kyber_ref.barrett_reduce(), oráculo validado en Fase 3.")
    print("// NO EDITAR A MANO.")
    for a, b in cases:
        add_expected = barrett_reduce(a + b)
        sub_expected = barrett_reduce(a - b)
        print(f"    check({a}, {b}, /*is_sub=*/0, {add_expected});  // vadd")
        print(f"    check({a}, {b}, /*is_sub=*/1, {sub_expected});  // vsub")


if __name__ == "__main__":
    main()
