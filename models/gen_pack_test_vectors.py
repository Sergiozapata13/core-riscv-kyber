#!/usr/bin/env python3
"""
gen_pack_test_vectors.py

Genera sw/tests/native/test_pack_vectors.h — vectores de prueba para
byte_encode/byte_decode (d=12) y compress/decompress (d=10, d=4),
derivados de kyber_ref.py (ya validado contra kyber-py, 120/120 casos
en la Fase 5).

Uso:
    python3 gen_pack_test_vectors.py > ../sw/tests/native/test_pack_vectors.h
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import byte_encode, poly_compress, poly_decompress, Q

SEED = 3033


def emit_array(name, values, ctype):
    print(f"static const {ctype} {name}[{len(values)}] = {{")
    print("    " + ", ".join(str(v) for v in values))
    print("};")
    print()


def main():
    random.seed(SEED)

    coeffs = [random.randint(0, Q - 1) for _ in range(256)]
    encoded = byte_encode(coeffs, 12)

    compressed_d10 = poly_compress(coeffs, 10)
    decompressed_d10 = poly_decompress(compressed_d10, 10)

    compressed_d4 = poly_compress(coeffs, 4)
    decompressed_d4 = poly_decompress(compressed_d4, 4)

    print("// test_pack_vectors.h")
    print("//")
    print("// Vectores de prueba para pack.c, generados desde kyber_ref.py")
    print("// (ya validado contra kyber-py, 120/120 casos) — no transcritos a mano.")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#include <stdint.h>")
    print()
    emit_array("test_pack_coeffs", coeffs, "int16_t")
    emit_array("test_pack_encoded_d12", list(encoded), "uint8_t")
    emit_array("test_pack_compressed_d10", compressed_d10, "uint16_t")
    emit_array("test_pack_decompressed_d10", decompressed_d10, "int16_t")
    emit_array("test_pack_compressed_d4", compressed_d4, "uint16_t")
    emit_array("test_pack_decompressed_d4", decompressed_d4, "int16_t")


if __name__ == "__main__":
    main()
