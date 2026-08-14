#!/usr/bin/env python3
"""
gen_butterfly_testvectors.py

Genera casos de test para butterfly_ct.sv y butterfly_gs.sv (Fase 4),
implementando directamente las formulas de isa_vectorial_kyber.docx
secciones 6.2/6.3 en Python (usando el mismo barrett_reduce ya validado
en kyber_ref.py) como oraculo.

Uso:
    python3 gen_butterfly_testvectors.py ct  > ../tb/butterfly_ct_testvectors.inc
    python3 gen_butterfly_testvectors.py gs  > ../tb/butterfly_gs_testvectors.inc
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import barrett_reduce, Q, ZETAS

RNG_SEED = 99
N_RANDOM = 300


def ct_butterfly(a, b, zeta):
    """Cooley-Tukey — isa_vectorial_kyber.docx seccion 6.2."""
    t = barrett_reduce(zeta * b)
    a_out = barrett_reduce(a + t)
    b_out = barrett_reduce(a - t)
    return a_out, b_out


def gs_butterfly(a, b, zeta_inv):
    """Gentleman-Sande — isa_vectorial_kyber.docx seccion 6.3."""
    a_out = barrett_reduce(a + b)
    diff = barrett_reduce(a - b)
    b_out = barrett_reduce(zeta_inv * diff)
    return a_out, b_out


def gen_ct_cases():
    cases = []
    # Bordes explicitos
    cases += [(0, 0, 0), (0, 0, 1), (Q - 1, Q - 1, Q - 1), (0, Q - 1, 1), (Q - 1, 0, 1)]
    # Aleatorios, usando twiddles reales de la tabla ZETAS (los que
    # realmente se usarian en el flujo real)
    for _ in range(N_RANDOM):
        a = random.randint(0, Q - 1)
        b = random.randint(0, Q - 1)
        zeta = random.choice(ZETAS)
        cases.append((a, b, zeta))
    return cases


def gen_gs_cases():
    cases = []
    cases += [(0, 0, 0), (0, 0, 1), (Q - 1, Q - 1, Q - 1), (0, Q - 1, 1), (Q - 1, 0, 1)]
    zeta_invs = [pow(z, Q - 2, Q) for z in ZETAS]
    for _ in range(N_RANDOM):
        a = random.randint(0, Q - 1)
        b = random.randint(0, Q - 1)
        zeta_inv = random.choice(zeta_invs)
        cases.append((a, b, zeta_inv))
    return cases


def main():
    random.seed(RNG_SEED)
    mode = sys.argv[1] if len(sys.argv) > 1 else "ct"

    if mode == "ct":
        cases = gen_ct_cases()
        print(f"// Generado por models/gen_butterfly_testvectors.py ct — {len(cases)} casos")
        print("// derivados de kyber_ref.barrett_reduce() aplicando la formula CT (seccion 6.2).")
        print("// NO EDITAR A MANO.")
        for a, b, zeta in cases:
            a_out, b_out = ct_butterfly(a, b, zeta)
            print(f"    check({a}, {b}, {zeta}, {a_out}, {b_out});")
    elif mode == "gs":
        cases = gen_gs_cases()
        print(f"// Generado por models/gen_butterfly_testvectors.py gs — {len(cases)} casos")
        print("// derivados de kyber_ref.barrett_reduce() aplicando la formula GS (seccion 6.3).")
        print("// NO EDITAR A MANO.")
        for a, b, zeta_inv in cases:
            a_out, b_out = gs_butterfly(a, b, zeta_inv)
            print(f"    check({a}, {b}, {zeta_inv}, {a_out}, {b_out});")
    else:
        print("Uso: gen_butterfly_testvectors.py [ct|gs]", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
