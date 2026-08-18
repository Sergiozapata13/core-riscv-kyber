#!/usr/bin/env python3
"""
gen_k_pke_decrypt_test_vectors.py

Genera sw/tests/native/test_k_pke_decrypt_vectors.h — vectores de
prueba para k_pke_decrypt.c, usando el mismo dk (de keygen) y c (de
encrypt) que los tests anteriores, confirmando el ciclo completo
encrypt->decrypt recupera el mensaje original 'm'.

Uso:
    python3 gen_k_pke_decrypt_test_vectors.py > ../sw/tests/native/test_k_pke_decrypt_vectors.h
"""

import sys

sys.path.insert(0, ".")
from kyber_ref import k_pke_keygen, k_pke_encrypt

SEED_D = bytes((i * 3 + 5) % 256 for i in range(32))
SEED_M = bytes((i * 7 + 11) % 256 for i in range(32))
SEED_R = bytes((i * 11 + 3) % 256 for i in range(32))


def emit(name, values, n):
    print(f"static const uint8_t {name}[{n}] = {{")
    print("    " + ", ".join(str(b) for b in values))
    print("};")
    print()


def main():
    ek, dk = k_pke_keygen(SEED_D)
    c = k_pke_encrypt(ek, SEED_M, SEED_R)
    assert len(dk) == 768 and len(c) == 768

    print("// test_k_pke_decrypt_vectors.h")
    print("//")
    print("// Vectores de prueba para k_pke_decrypt.c — reusa dk (de keygen) y")
    print("// c (de encrypt), confirmando que el ciclo completo")
    print("// encrypt->decrypt recupera el mensaje original 'm'.")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#include <stdint.h>")
    print()
    emit("test_dk", dk, 768)
    emit("test_c", c, 768)
    emit("test_m_expected", SEED_M, 32)


if __name__ == "__main__":
    main()
