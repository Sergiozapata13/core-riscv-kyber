#!/usr/bin/env python3
"""
gen_pack_generic_test_vectors.py

Genera sw/tests/native/test_pack_generic_vectors.h — vectores de prueba
para byte_encode_generic/byte_decode_generic, con d=1,4,10,12 (todos
los valores usados por el protocolo ML-KEM-512), derivados de
kyber_ref.byte_encode() (ya validado contra kyber-py).

Uso:
    python3 gen_pack_generic_test_vectors.py > ../sw/tests/native/test_pack_generic_vectors.h
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import byte_encode, Q

SEED = 10101
D_VALUES = [1, 4, 10, 12]


def main():
    random.seed(SEED)

    print("// test_pack_generic_vectors.h")
    print("//")
    print("// Vectores de prueba para byte_encode_generic/byte_decode_generic,")
    print("// derivados de kyber_ref.byte_encode() — no transcritos a mano.")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#include <stdint.h>")
    print()

    for d in D_VALUES:
        max_val = (1 << d) - 1 if d < 12 else Q - 1
        coeffs = [random.randint(0, max_val) for _ in range(256)]
        encoded = byte_encode(coeffs, d)
        n_bytes = len(encoded)
        print(f"static const uint16_t test_pack_gen_coeffs_d{d}[256] = {{")
        print("    " + ", ".join(str(c) for c in coeffs))
        print("};")
        print(f"static const uint8_t test_pack_gen_encoded_d{d}[{n_bytes}] = {{")
        print("    " + ", ".join(str(b) for b in encoded))
        print("};")
        print()


if __name__ == "__main__":
    main()
