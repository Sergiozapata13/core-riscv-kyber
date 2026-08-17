#!/usr/bin/env python3
"""
gen_ntt_engine_testvectors.py

Genera tb/ntt_engine_testvectors.inc — un polinomio de 256 coeficientes,
su NTT esperado, y el INTT(NTT(...)) esperado (round-trip), derivados de
kyber_ref.ntt()/intt() (el oraculo validado en la Fase 3).

Uso:
    python3 gen_ntt_engine_testvectors.py > ../tb/ntt_engine_testvectors.inc
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import ntt, intt, Q

RNG_SEED = 555


def emit_array(name, values):
    print(f"static const uint16_t {name}[256] = {{")
    print("    " + ", ".join(str(v) for v in values))
    print("};")


def main():
    random.seed(RNG_SEED)
    poly = [random.randint(0, Q - 1) for _ in range(256)]
    ntt_result = ntt(poly)
    intt_of_ntt = intt(ntt_result)

    assert intt_of_ntt == poly, "round-trip debe coincidir exactamente (ya verificado en Fase 3)"

    print("// Generado por models/gen_ntt_engine_testvectors.py")
    print("// derivado de kyber_ref.ntt()/intt(), oráculo validado en Fase 3.")
    print("// NO EDITAR A MANO.")
    print()
    emit_array("test_poly_in", poly)
    print()
    emit_array("test_ntt_expected", ntt_result)
    print()
    emit_array("test_intt_expected", intt_of_ntt)


if __name__ == "__main__":
    main()
