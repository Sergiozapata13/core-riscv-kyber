#!/usr/bin/env python3
"""
gen_sample_ntt_test_vectors.py

Genera sw/tests/native/test_sample_ntt_vectors.h — vectores de prueba
para sample_ntt(), usando SHAKE128 real (hashlib) como fuente de bytes
(el caso de uso genuino: rho||i||j -> SHAKE128 -> sample_ntt), derivados
de kyber_ref.sample_ntt() (ya validado contra kyber-py, 50/50 casos en
la Fase 5).

Uso:
    python3 gen_sample_ntt_test_vectors.py > ../sw/tests/native/test_sample_ntt_vectors.h
"""

import hashlib
import random
import sys

sys.path.insert(0, ".")
from kyber_ref import sample_ntt

SEED = 4044
XOF_BUFFER_BYTES = 840


def emit_array(name, values, ctype):
    print(f"static const {ctype} {name}[{len(values)}] = {{")
    print("    " + ", ".join(str(v) for v in values))
    print("};")
    print()


def main():
    random.seed(SEED)
    seed = bytes(random.randint(0, 255) for _ in range(34))  # rho(32) + i + j
    xof_bytes = hashlib.shake_128(seed).digest(XOF_BUFFER_BYTES)
    result = sample_ntt(xof_bytes)

    print("// test_sample_ntt_vectors.h")
    print("//")
    print("// Vectores de prueba para sample_ntt.c, generados con SHAKE128 real")
    print("// (hashlib) como fuente de bytes, derivados de kyber_ref.sample_ntt()")
    print("// (ya validado contra kyber-py, 50/50 casos) — no transcritos a mano.")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#include <stdint.h>")
    print()
    emit_array("test_sample_ntt_input", list(xof_bytes), "uint8_t")
    emit_array("test_sample_ntt_expected", result, "int16_t")


if __name__ == "__main__":
    main()
