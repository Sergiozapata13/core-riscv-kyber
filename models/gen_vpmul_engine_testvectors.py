#!/usr/bin/env python3
"""
gen_vpmul_engine_testvectors.py

Genera tb/vpmul_engine_testvectors.inc — dos polinomios de 256
coeficientes (representando datos ya en dominio NTT) y su
poly_pointwise_mul esperado, derivados de kyber_ref.poly_pointwise_mul()
(oráculo validado en la Fase 3, incluida la secuencia NTT(a)xNTT(b)->INTT
en test_vector_isa.py).

Uso:
    python3 gen_vpmul_engine_testvectors.py > ../tb/vpmul_engine_testvectors.inc
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import poly_pointwise_mul, ntt, Q

RNG_SEED = 777


def emit_array(name, values):
    print(f"static const uint16_t {name}[256] = {{")
    print("    " + ", ".join(str(v) for v in values))
    print("};")


def main():
    random.seed(RNG_SEED)
    # Los operandos reales de vpmul siempre vienen de un vntt previo
    # (dominio NTT) — se generan asi para que el test sea representativo
    # del flujo real, no polinomios aleatorios sin relacion con el uso.
    a = ntt([random.randint(0, Q - 1) for _ in range(256)])
    b = ntt([random.randint(0, Q - 1) for _ in range(256)])
    result = poly_pointwise_mul(a, b)

    print("// Generado por models/gen_vpmul_engine_testvectors.py")
    print("// derivado de kyber_ref.poly_pointwise_mul(), oráculo validado en Fase 3.")
    print("// a y b son salidas de ntt() (dominio NTT), como en el uso real de vpmul.")
    print("// NO EDITAR A MANO.")
    print()
    emit_array("test_vpmul_a", a)
    print()
    emit_array("test_vpmul_b", b)
    print()
    emit_array("test_vpmul_expected", result)


if __name__ == "__main__":
    main()
