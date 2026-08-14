"""
test_vector_isa.py

Verificacion de cierre de la Fase 3: cada una de las 8 instrucciones
custom, ejecutada sobre el simulador funcional (vector_isa_sim.py),
comparada contra el modelo de referencia (kyber_ref.py) — instruccion
por instruccion, y tambien en secuencias representativas del flujo real
de Kyber (NTT(a) x NTT(b) -> INTT, la cadena que mas se repite en
keygen/encapsulation/decapsulation).

Este es el ultimo criterio de verificacion de la Fase 3 segun el
cronograma: "Simulacion 'a mano' o en script de cada instruccion custom
nueva contra el modelo de referencia, antes de escribir RTL."
"""

import random
import sys

from kyber_ref import Q, N, ntt, intt, barrett_reduce, poly_add, poly_sub, poly_pointwise_mul
from vector_isa_sim import VectorUnit

RNG_SEED = 7
N_TRIALS = 20

errors = 0


def check(label, condition):
    global errors
    if condition:
        print(f"OK   [{label}]")
    else:
        print(f"FAIL [{label}]")
        errors += 1


def random_poly():
    return [random.randint(0, Q - 1) for _ in range(N)]


def main():
    random.seed(RNG_SEED)

    print("=== Instrucciones de memoria (vload / vstore) ===")
    for trial in range(N_TRIALS):
        vu = VectorUnit()
        poly = random_poly()
        addr = 0x100 + trial * 4
        vu.mem[addr] = poly

        vu.vload(vreg_dst=0, addr=addr)
        check(f"vload trial {trial}: v0 == memoria[0x{addr:x}]", vu.vregs[0] == poly)

        vu.vstore(vreg_src=0, addr=addr + 0x1000)
        check(f"vstore trial {trial}: memoria[0x{addr+0x1000:x}] == v0", vu.mem[addr + 0x1000] == poly)

    print("\n=== vntt (funct3=010) vs modelo de referencia ===")
    for trial in range(N_TRIALS):
        vu = VectorUnit()
        poly = random_poly()
        vu.vregs[0] = poly
        vu.vntt(vreg_dst=1, vreg_src=0)
        expected = ntt(poly)
        check(f"vntt trial {trial}", vu.vregs[1] == expected)

    print("\n=== vintt (funct3=011) vs modelo de referencia ===")
    for trial in range(N_TRIALS):
        vu = VectorUnit()
        # Partir de un polinomio ya en dominio NTT (salida de ntt()), como
        # seria en el flujo real: vintt siempre opera sobre algo que vntt
        # (u otra operacion en dominio NTT) produjo antes.
        poly_ntt_domain = ntt(random_poly())
        vu.vregs[0] = poly_ntt_domain
        vu.vintt(vreg_dst=1, vreg_src=0)
        expected = intt(poly_ntt_domain)
        check(f"vintt trial {trial}", vu.vregs[1] == expected)

    print("\n=== vpmul (funct3=100) vs modelo de referencia ===")
    for trial in range(N_TRIALS):
        vu = VectorUnit()
        a_ntt = ntt(random_poly())
        b_ntt = ntt(random_poly())
        vu.vregs[0] = a_ntt
        vu.vregs[1] = b_ntt
        vu.vpmul(vreg_dst=2, vreg_a=0, vreg_b=1)
        expected = poly_pointwise_mul(a_ntt, b_ntt)
        check(f"vpmul trial {trial}", vu.vregs[2] == expected)

    print("\n=== vbarrett (funct3=101) vs modelo de referencia ===")
    for trial in range(N_TRIALS):
        vu = VectorUnit()
        # Coeficientes fuera de [0,q) — el caso real de uso, tras una
        # multiplicacion cruda sin reducir todavia.
        raw = [random.randint(0, 4 * Q) for _ in range(N)]
        vu.vregs[0] = raw
        vu.vbarrett(vreg_dst=1, vreg_src=0)
        expected = [barrett_reduce(c) for c in raw]
        check(f"vbarrett trial {trial}", vu.vregs[1] == expected)

    print("\n=== vadd / vsub (funct3=110/111) vs modelo de referencia ===")
    for trial in range(N_TRIALS):
        vu = VectorUnit()
        a = random_poly()
        b = random_poly()
        vu.vregs[0] = a
        vu.vregs[1] = b

        vu.vadd(vreg_dst=2, vreg_a=0, vreg_b=1)
        check(f"vadd trial {trial}", vu.vregs[2] == poly_add(a, b))

        vu.vsub(vreg_dst=3, vreg_a=0, vreg_b=1)
        check(f"vsub trial {trial}", vu.vregs[3] == poly_sub(a, b))

    # -------------------------------------------------------------
    # Secuencia representativa del flujo real de Kyber:
    # NTT(a) x NTT(b) -> INTT  (multiplicacion de polinomios completa,
    # la cadena que mas se repite en keygen/encaps/decaps — ver Apendice
    # A.2 del cronograma, "cadena tipica NTT(a) x NTT(b) -> INTT")
    # -------------------------------------------------------------
    print("\n=== Secuencia completa: NTT(a) x NTT(b) -> INTT ===")
    for trial in range(N_TRIALS):
        vu = VectorUnit()
        a = random_poly()
        b = random_poly()
        addr_a, addr_b = 0x200, 0x300

        vu.mem[addr_a] = a
        vu.mem[addr_b] = b

        vu.vload(vreg_dst=0, addr=addr_a)      # v0 = a
        vu.vload(vreg_dst=1, addr=addr_b)      # v1 = b
        vu.vntt(vreg_dst=0, vreg_src=0)         # v0 = NTT(a)
        vu.vntt(vreg_dst=1, vreg_src=1)         # v1 = NTT(b)
        vu.vpmul(vreg_dst=2, vreg_a=0, vreg_b=1)  # v2 = NTT(a) . NTT(b)
        vu.vintt(vreg_dst=3, vreg_src=2)        # v3 = INTT(v2) = a * b

        expected = intt(poly_pointwise_mul(ntt(a), ntt(b)))
        check(f"secuencia completa trial {trial}: a*b via ISA vectorial", vu.vregs[3] == expected)

    print()
    if errors == 0:
        print("PASS: las 8 instrucciones + secuencia representativa coinciden con el modelo de referencia.")
        print("      Fase 3 lista para cerrar — semántica congelada antes de escribir RTL (Fase 4).")
        return 0
    else:
        print(f"FAIL: {errors} verificación(es) fallida(s).")
        return 1


if __name__ == "__main__":
    sys.exit(main())
