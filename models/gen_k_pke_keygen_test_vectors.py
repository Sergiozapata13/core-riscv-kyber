#!/usr/bin/env python3
"""
gen_k_pke_keygen_test_vectors.py

Genera sw/tests/native/test_k_pke_keygen_vectors.h — vectores de prueba
para k_pke_keygen.c, derivados de kyber_ref.k_pke_keygen() (ya validado
contra kyber-py, 96/96 casos en test_ml_kem_protocol.py).

Uso:
    python3 gen_k_pke_keygen_test_vectors.py > ../sw/tests/native/test_k_pke_keygen_vectors.h
"""

import sys

sys.path.insert(0, ".")
from kyber_ref import k_pke_keygen

SEED_D = bytes(i * 3 + 5 for i in range(32))


def emit(name, values, n):
    print(f"static const uint8_t {name}[{n}] = {{")
    print("    " + ", ".join(str(b) for b in values))
    print("};")
    print()


def main():
    ek, dk = k_pke_keygen(SEED_D)
    assert len(ek) == 800 and len(dk) == 768

    print("// test_k_pke_keygen_vectors.h")
    print("//")
    print("// Vectores de prueba para k_pke_keygen.c, generados desde")
    print("// kyber_ref.k_pke_keygen() (ya validado contra kyber-py,")
    print("// 96/96 casos en test_ml_kem_protocol.py) — no transcritos a mano.")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#include <stdint.h>")
    print()
    emit("test_d", SEED_D, 32)
    emit("test_ek_expected", ek, 800)
    emit("test_dk_expected", dk, 768)


if __name__ == "__main__":
    main()
