#!/usr/bin/env python3
"""
gen_base_case_mul_testvectors.py

Genera casos de test para base_case_mul.sv (Fase 4), llamando
DIRECTAMENTE a kyber_ref._base_case_multiply() — la misma función ya
usada dentro de poly_pointwise_mul() y validada en la Fase 3 (test_
vector_isa.py, caso vpmul, incluida la secuencia completa NTT(a) x
NTT(b) -> INTT). No se reimplementa la fórmula en este script, para
evitar el mismo tipo de error humano (signo/constante mal transcrita)
que se encontró al debuggear ntt_engine.sv (Fase 4).

Uso:
    python3 gen_base_case_mul_testvectors.py > ../tb/base_case_mul_testvectors.inc
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import _base_case_multiply, Q, ZETAS

RNG_SEED = 321
N_RANDOM = 300


def main():
    random.seed(RNG_SEED)

    cases = []
    # Bordes explicitos
    cases += [
        (0, 0, 0, 0, 0),
        (0, 0, 0, 0, 1),
        (Q - 1, Q - 1, Q - 1, Q - 1, Q - 1),
        (1, 0, 1, 0, 5),
        (0, 1, 0, 1, 5),
    ]
    # Aleatorios, usando zetas reales de la mitad superior de la tabla
    # (que es la que realmente usa poly_pointwise_mul, indices 64-127)
    upper_zetas = ZETAS[64:128]
    for _ in range(N_RANDOM):
        a0 = random.randint(0, Q - 1)
        a1 = random.randint(0, Q - 1)
        b0 = random.randint(0, Q - 1)
        b1 = random.randint(0, Q - 1)
        zeta = random.choice(upper_zetas)
        cases.append((a0, a1, b0, b1, zeta))
    # También con Q - zeta (el "zeta negado" que usa el segundo par de
    # cada grupo de 4 en poly_pointwise_mul)
    for _ in range(N_RANDOM):
        a0 = random.randint(0, Q - 1)
        a1 = random.randint(0, Q - 1)
        b0 = random.randint(0, Q - 1)
        b1 = random.randint(0, Q - 1)
        zeta = Q - random.choice(upper_zetas)
        cases.append((a0, a1, b0, b1, zeta))

    print(f"// Generado por models/gen_base_case_mul_testvectors.py — {len(cases)} casos")
    print("// derivados de kyber_ref._base_case_multiply(), oráculo validado en Fase 3.")
    print("// NO EDITAR A MANO.")
    for a0, a1, b0, b1, zeta in cases:
        c0, c1 = _base_case_multiply(a0, a1, b0, b1, zeta)
        print(f"    check({a0}, {a1}, {b0}, {b1}, {zeta}, {c0}, {c1});")


if __name__ == "__main__":
    main()
