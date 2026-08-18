#!/usr/bin/env python3
"""
gen_ml_kem_test_vectors.py

Genera sw/tests/native/test_ml_kem_vectors.h — vectores de prueba para
ml_kem.c (keygen/encaps/decaps completos), derivados de kyber_ref.py
(ya validado contra kyber-py, 96/96 casos en test_ml_kem_protocol.py).
Incluye el caso de rechazo implicito (ciphertext corrompido).

Uso:
    python3 gen_ml_kem_test_vectors.py > ../sw/tests/native/test_ml_kem_vectors.h
"""

import sys

sys.path.insert(0, ".")
from kyber_ref import ml_kem_keygen, ml_kem_encaps, ml_kem_decaps

SEED_D = bytes((i * 3 + 5) % 256 for i in range(32))
SEED_Z = bytes((i * 13 + 17) % 256 for i in range(32))
SEED_M = bytes((i * 7 + 11) % 256 for i in range(32))


def emit(name, values, n):
    print(f"static const uint8_t {name}[{n}] = {{")
    print("    " + ", ".join(str(b) for b in values))
    print("};")
    print()


def main():
    ek, dk = ml_kem_keygen(SEED_D, SEED_Z)
    assert len(ek) == 800 and len(dk) == 1632

    K, c = ml_kem_encaps(ek, SEED_M)
    assert len(K) == 32 and len(c) == 768

    K_decap = ml_kem_decaps(dk, c)
    assert K_decap == K

    c_corrupted = bytearray(c)
    c_corrupted[0] ^= 0x01
    c_corrupted = bytes(c_corrupted)
    K_rejected = ml_kem_decaps(dk, c_corrupted)
    assert K_rejected != K

    print("// test_ml_kem_vectors.h")
    print("//")
    print("// Vectores de prueba para ml_kem.c, generados desde kyber_ref.py")
    print("// (ya validado contra kyber-py, 96/96 casos) — no transcritos a mano.")
    print("// Incluye el caso de rechazo implicito (ciphertext corrompido).")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#include <stdint.h>")
    print()
    emit("test_d", SEED_D, 32)
    emit("test_z", SEED_Z, 32)
    emit("test_m", SEED_M, 32)
    emit("test_ek_expected", ek, 800)
    emit("test_dk_expected", dk, 1632)
    emit("test_K_expected", K, 32)
    emit("test_c_expected", c, 768)
    emit("test_c_corrupted", c_corrupted, 768)
    emit("test_K_rejected_expected", K_rejected, 32)


if __name__ == "__main__":
    main()
