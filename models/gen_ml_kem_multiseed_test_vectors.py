#!/usr/bin/env python3
"""
gen_ml_kem_multiseed_test_vectors.py

Genera sw/tests/native/test_ml_kem_multiseed_vectors.h — vectores de
prueba para ml_kem.c con VARIAS semillas distintas (no solo la semilla
fija ya usada en test_ml_kem_native.c) — Fase 5, cierre del riesgo de
que la correctitud del firmware C dependiera, por coincidencia, de los
valores particulares de la unica semilla usada hasta ahora en todo el
pipeline C (nativo y en el core real).

N_SEEDS semillas (d, z, m) generadas con random.seed() distinto cada
una, cada una ejercitando el ciclo completo keygen->encaps->decaps
(incluyendo un caso de rechazo implicito por semilla, corrompiendo 1
bit del ciphertext).

Uso:
    python3 gen_ml_kem_multiseed_test_vectors.py > ../sw/tests/native/test_ml_kem_multiseed_vectors.h
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import ml_kem_keygen, ml_kem_encaps, ml_kem_decaps

N_SEEDS = 5
BASE_SEED = 20260819  # fecha de esta sesion, arbitraria pero fija (reproducible)


def emit(name, values, n):
    print(f"static const uint8_t {name}[{n}] = {{")
    print("    " + ", ".join(str(b) for b in values))
    print("};")


def main():
    print("// test_ml_kem_multiseed_vectors.h")
    print("//")
    print("// Vectores de prueba con MULTIPLES semillas distintas para ml_kem.c,")
    print("// generados desde kyber_ref.py (ya validado contra kyber-py y contra")
    print("// los vectores oficiales de NIST ACVP) — no transcritos a mano.")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#include <stdint.h>")
    print()
    print(f"#define N_SEEDS {N_SEEDS}")
    print()

    for seed_idx in range(N_SEEDS):
        random.seed(BASE_SEED + seed_idx * 1000003)  # primo grande, separa bien las secuencias
        d = bytes(random.randint(0, 255) for _ in range(32))
        z = bytes(random.randint(0, 255) for _ in range(32))
        m = bytes(random.randint(0, 255) for _ in range(32))

        ek, dk = ml_kem_keygen(d, z)
        K, c = ml_kem_encaps(ek, m)
        K_decap = ml_kem_decaps(dk, c)
        assert K_decap == K

        c_corrupted = bytearray(c)
        c_corrupted[seed_idx % len(c_corrupted)] ^= 0x01
        c_corrupted = bytes(c_corrupted)
        K_rejected = ml_kem_decaps(dk, c_corrupted)
        assert K_rejected != K

        print(f"// ---- Semilla {seed_idx} ----")
        emit(f"seed{seed_idx}_d", d, 32)
        emit(f"seed{seed_idx}_z", z, 32)
        emit(f"seed{seed_idx}_m", m, 32)
        emit(f"seed{seed_idx}_ek_expected", ek, 800)
        emit(f"seed{seed_idx}_dk_expected", dk, 1632)
        emit(f"seed{seed_idx}_K_expected", K, 32)
        emit(f"seed{seed_idx}_c_expected", c, 768)
        emit(f"seed{seed_idx}_c_corrupted", c_corrupted, 768)
        emit(f"seed{seed_idx}_K_rejected_expected", K_rejected, 32)
        print()


if __name__ == "__main__":
    main()
