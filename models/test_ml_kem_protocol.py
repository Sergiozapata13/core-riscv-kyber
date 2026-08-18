"""
test_ml_kem_protocol.py

Verificacion del protocolo completo ML-KEM-512 (keygen/encaps/decaps)
implementado en kyber_ref.py contra kyber-py — Fase 5, criterio de
cierre antes de portar a firmware C.

A diferencia de test_kyber_ref.py (que valida primitivas aisladas:
NTT, Barrett, etc.) y test_vector_isa.py (que valida las 8
instrucciones de la ISA vectorial), este script valida el PROTOCOLO
COMPLETO end-to-end: que keygen/encaps/decaps propios coincidan byte a
byte con kyber-py, que interoperen correctamente (mi decaps con un
ciphertext de kyber-py), y que el mecanismo de rechazo implicito
(ciphertext corrompido) produzca la misma clave "basura" que kyber-py.

Uso:
    python3 test_ml_kem_protocol.py
"""

import random
import sys

import kyber_ref
from kyber_py.ml_kem import ML_KEM_512

RNG_SEED = 8088
N_TRIALS = 10

errors = 0


def check(label, condition):
    global errors
    if condition:
        print(f"OK   [{label}]")
    else:
        print(f"FAIL [{label}]")
        errors += 1


def main():
    random.seed(RNG_SEED)

    print("=== KeyGen: coincide byte a byte con kyber-py ===")
    keys = []
    for trial in range(N_TRIALS):
        d = bytes(random.randint(0, 255) for _ in range(32))
        z = bytes(random.randint(0, 255) for _ in range(32))

        ek_mine, dk_mine = kyber_ref.ml_kem_keygen(d, z)
        ek_theirs, dk_theirs = ML_KEM_512._keygen_internal(d, z)

        check(f"keygen_trial_{trial}_ek", ek_mine == ek_theirs)
        check(f"keygen_trial_{trial}_dk", dk_mine == dk_theirs)
        check(f"keygen_trial_{trial}_ek_size_800B", len(ek_mine) == 800)
        check(f"keygen_trial_{trial}_dk_size_1632B", len(dk_mine) == 1632)

        keys.append((ek_mine, dk_mine))

    print("\n=== Encaps: coincide byte a byte con kyber-py ===")
    ciphertexts = []
    for trial in range(N_TRIALS):
        ek, dk = keys[trial]
        m = bytes(random.randint(0, 255) for _ in range(32))

        K_mine, c_mine = kyber_ref.ml_kem_encaps(ek, m)
        K_theirs, c_theirs = ML_KEM_512._encaps_internal(ek, m)

        check(f"encaps_trial_{trial}_K", K_mine == K_theirs)
        check(f"encaps_trial_{trial}_c", c_mine == c_theirs)
        check(f"encaps_trial_{trial}_c_size_768B", len(c_mine) == 768)

        ciphertexts.append((K_mine, c_mine, c_theirs))

    print("\n=== Decaps: recupera el secreto compartido correctamente ===")
    for trial in range(N_TRIALS):
        ek, dk = keys[trial]
        K_expected, c_mine, c_theirs = ciphertexts[trial]

        K_decap_own = kyber_ref.ml_kem_decaps(dk, c_mine)
        check(f"decaps_trial_{trial}_propio_ciphertext", K_decap_own == K_expected)

        K_decap_cross = kyber_ref.ml_kem_decaps(dk, c_theirs)
        check(f"decaps_trial_{trial}_interop_ciphertext_kyberpy", K_decap_cross == K_expected)

    print("\n=== Rechazo implicito: ciphertext corrompido produce la misma 'basura' que kyber-py ===")
    for trial in range(3):
        ek, dk = keys[trial]
        K_original, c_original, _ = ciphertexts[trial]

        corrupted = bytearray(c_original)
        corrupted[trial % len(corrupted)] ^= 0x01
        corrupted = bytes(corrupted)

        K_mine_rejected = kyber_ref.ml_kem_decaps(dk, corrupted)
        K_theirs_rejected = ML_KEM_512._decaps_internal(dk, corrupted)

        check(f"rechazo_trial_{trial}_no_coincide_con_original", K_mine_rejected != K_original)
        check(f"rechazo_trial_{trial}_coincide_con_kyberpy", K_mine_rejected == K_theirs_rejected)

    print()
    if errors == 0:
        print("PASS: protocolo ML-KEM-512 completo (keygen/encaps/decaps) coincide con kyber-py,")
        print("      incluyendo interoperabilidad cruzada y rechazo implícito.")
        return 0
    else:
        print(f"FAIL: {errors} discrepancia(s) detectada(s).")
        return 1


if __name__ == "__main__":
    sys.exit(main())
