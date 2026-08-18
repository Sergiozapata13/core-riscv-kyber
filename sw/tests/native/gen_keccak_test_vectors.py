#!/usr/bin/env python3
"""
gen_keccak_test_vectors.py

Genera test_keccak_vectors.h — vectores de prueba para keccak.c, usando
hashlib de Python (implementacion estandar de referencia de SHA3/SHAKE,
no una reimplementacion propia) como oraculo. No se transcriben valores
de hash a mano en ningun punto.

Uso:
    python3 gen_keccak_test_vectors.py > test_keccak_vectors.h
"""

import hashlib

TESTS = [b"", b"abc", b"The quick brown fox jumps over the lazy dog"]
NAMES = ["empty", "abc", "quickfox"]


def main():
    print("// test_keccak_vectors.h")
    print("//")
    print("// Vectores de prueba para keccak.c, generados con hashlib de Python")
    print("// (implementacion estandar de referencia de SHA3/SHAKE) — no")
    print("// transcritos a mano.")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#ifndef TEST_KECCAK_VECTORS_H")
    print("#define TEST_KECCAK_VECTORS_H")
    print()
    print("#include <stdint.h>")
    print("#include <stddef.h>")
    print()

    for msg, name in zip(TESTS, NAMES):
        arr = ", ".join(str(b) for b in msg) if msg else "0"
        print(f"static const uint8_t msg_{name}[] = {{{arr}}};")
        print(f"static const size_t msg_{name}_len = {len(msg)};")
        print()

    for msg, name in zip(TESTS, NAMES):
        h = hashlib.sha3_256(msg).digest()
        bytes_str = ", ".join(f"0x{b:02x}" for b in h)
        print(f"static const uint8_t expected_sha3_256_{name}[32] = {{{bytes_str}}};")
    print()

    for msg, name in zip(TESTS, NAMES):
        h = hashlib.shake_128(msg).digest(32)
        bytes_str = ", ".join(f"0x{b:02x}" for b in h)
        print(f"static const uint8_t expected_shake128_{name}[32] = {{{bytes_str}}};")
    print()

    for msg, name in zip(TESTS, NAMES):
        h = hashlib.shake_256(msg).digest(32)
        bytes_str = ", ".join(f"0x{b:02x}" for b in h)
        print(f"static const uint8_t expected_shake256_{name}[32] = {{{bytes_str}}};")
    print()

    print("#endif // TEST_KECCAK_VECTORS_H")


if __name__ == "__main__":
    main()
