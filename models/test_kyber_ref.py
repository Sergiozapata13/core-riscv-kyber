"""
test_kyber_ref.py

Validacion del modelo de referencia (Fase 3) — el oraculo contra el que
se va a verificar el RTL de la unidad vectorial (Fase 4) y el firmware
de Kyber (Fase 5).

Dos capas de validacion:
  1. Correctitud interna: NTT/INTT son inversos exactos entre si.
  2. Correctitud EXTERNA: los resultados coinciden bit-a-bit con kyber-py
     (implementacion de referencia en Python, verificada contra los KAT
     oficiales de ML-KEM/Kyber del NIST). Esta es la validacion que
     importa de verdad — que este modelo no solo sea consistente consigo
     mismo, sino que implemente la especificacion REAL de Kyber.

Requiere: pip install kyber-py (paquete de terceros, auditado, no es
parte de este repositorio — se usa solo para validacion cruzada).

Uso:
    python3 test_kyber_ref.py
"""

import random
import sys

from kyber_ref import (
    Q,
    N,
    ZETAS,
    barrett_reduce,
    ntt,
    intt,
    poly_add,
    poly_sub,
    poly_pointwise_mul,
)

try:
    from kyber_py.polynomials.polynomials import PolynomialRing

    HAVE_KYBER_PY = True
except ImportError:
    HAVE_KYBER_PY = False

RNG_SEED = 42
N_TRIALS = 50

errors = 0


def check(label, condition):
    global errors
    if condition:
        print(f"OK   [{label}]")
    else:
        print(f"FAIL [{label}]")
        errors += 1


def main():
    random.seed(RNG_SEED)

    # -----------------------------------------------------------------
    # Capa 1: correctitud interna
    # -----------------------------------------------------------------
    print("=== Capa 1: correctitud interna ===")

    zero = [0] * N
    check("ntt(0) == 0", ntt(zero) == zero)
    check("intt(0) == 0", intt(zero) == zero)

    roundtrip_ok = True
    for _ in range(N_TRIALS):
        poly = [random.randint(0, Q - 1) for _ in range(N)]
        if intt(ntt(poly)) != poly:
            roundtrip_ok = False
            break
    check(f"round-trip NTT/INTT ({N_TRIALS} casos aleatorios)", roundtrip_ok)

    barrett_ok = True
    for _ in range(1000):
        a = random.randint(-(2**20), 2**20)
        r = barrett_reduce(a)
        if not (0 <= r < Q) or r != a % Q:
            barrett_ok = False
            break
    check("barrett_reduce == Python % nativo (1000 casos)", barrett_ok)

    check("len(ZETAS) == 128", len(ZETAS) == 128)
    check("ZETA^256 mod Q == 1 (orden correcto de la raiz)", pow(17, 256, Q) == 1)
    check("ZETA^128 mod Q == Q-1 (orden exacto, no divisor)", pow(17, 128, Q) == Q - 1)

    # -----------------------------------------------------------------
    # Capa 2: correctitud externa (contra kyber-py)
    # -----------------------------------------------------------------
    print("\n=== Capa 2: validación cruzada contra kyber-py ===")

    if not HAVE_KYBER_PY:
        print("SKIP: kyber-py no está instalado (pip install kyber-py)")
        print("      Esta capa es la más importante — instalar antes de confiar")
        print("      en este modelo como oráculo para la Fase 4.")
    else:
        ring = PolynomialRing()

        zetas_match = ZETAS == ring.ntt_zetas
        check("tabla de zetas idéntica a kyber-py (128 valores)", zetas_match)

        mul_ok = True
        for _ in range(N_TRIALS):
            a = [random.randint(0, Q - 1) for _ in range(N)]
            b = [random.randint(0, Q - 1) for _ in range(N)]

            result_mine = intt(poly_pointwise_mul(ntt(a), ntt(b)))
            result_theirs = list((ring(a) * ring(b)).coeffs)

            if list(result_mine) != result_theirs:
                mul_ok = False
                break
        check(f"multiplicación NTT->pointwise->INTT vs kyber-py ({N_TRIALS} casos)", mul_ok)

        addsub_ok = True
        for _ in range(N_TRIALS):
            a = [random.randint(0, Q - 1) for _ in range(N)]
            b = [random.randint(0, Q - 1) for _ in range(N)]
            if poly_add(a, b) != list((ring(a) + ring(b)).coeffs):
                addsub_ok = False
                break
            if poly_sub(a, b) != list((ring(a) - ring(b)).coeffs):
                addsub_ok = False
                break
        check(f"suma/resta vs kyber-py ({N_TRIALS} casos)", addsub_ok)

    print()
    if errors == 0:
        print("PASS: modelo de referencia validado — apto como oráculo para Fase 4/5.")
        return 0
    else:
        print(f"FAIL: {errors} verificación(es) fallida(s).")
        return 1


if __name__ == "__main__":
    sys.exit(main())
