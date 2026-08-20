"""
test_nist_acvp.py

Verificacion del protocolo ML-KEM-512 completo (kyber_ref.py) contra los
vectores de prueba OFICIALES de NIST ACVP — Fase 5, cierre de la
recomendacion original del cronograma ("comparacion contra los test
vectors oficiales de referencia de Kyber/ML-KEM (NIST) en al menos un
subconjunto").

A diferencia de test_ml_kem_protocol.py (que valida contra kyber-py,
una implementacion de referencia de terceros pero no oficial), este
script valida contra los vectores publicados por NIST mismo en el
repositorio del servidor ACVP (Automated Cryptographic Validation
Protocol) — la misma fuente que usan los laboratorios de validacion
FIPS 140-3 para certificar implementaciones.

Fuente de los vectores (descargados y congelados en models/nist_vectors/,
rama 'master' del repositorio al momento de la descarga — Agosto 2026):
    https://github.com/usnistgov/ACVP-Server/blob/master/gen-val/json-files/ML-KEM-keyGen-FIPS203/internalProjection.json
    https://github.com/usnistgov/ACVP-Server/blob/master/gen-val/json-files/ML-KEM-encapDecap-FIPS203/internalProjection.json

Solo se usan los testGroups con parameterSet == "ML-KEM-512" (el unico
nivel de seguridad que implementa este proyecto) — se ignoran
ML-KEM-768/1024.

Los casos de decaps con reason == "modified ciphertext" son
particularmente valiosos: son vectores de rechazo implicito generados
por NIST mismo (no por este proyecto), asi que pasar estos casos
confirma que el mecanismo J(z||c) coincide con la especificacion
oficial, no solo con la interpretacion de kyber-py.

Uso:
    python3 test_nist_acvp.py
"""

import json
import sys

import kyber_ref

KEYGEN_JSON = "nist_vectors/ML-KEM-keyGen-internalProjection.json"
ENCAPDECAP_JSON = "nist_vectors/ML-KEM-encapDecap-internalProjection.json"

errors = 0


def check(label, condition):
    global errors
    if condition:
        print(f"OK   [{label}]")
    else:
        print(f"FAIL [{label}]")
        errors += 1


def load_testgroup(path, predicate):
    with open(path) as f:
        data = json.load(f)
    matches = [tg for tg in data["testGroups"] if predicate(tg)]
    assert len(matches) == 1, f"esperaba exactamente 1 testGroup, encontre {len(matches)}"
    return matches[0]


def test_keygen():
    print("=== NIST ACVP: ML-KEM keyGen (ML-KEM-512) ===")
    tg = load_testgroup(KEYGEN_JSON, lambda tg: tg["parameterSet"] == "ML-KEM-512")
    for t in tg["tests"]:
        d = bytes.fromhex(t["d"])
        z = bytes.fromhex(t["z"])
        ek_expected = bytes.fromhex(t["ek"])
        dk_expected = bytes.fromhex(t["dk"])

        ek, dk = kyber_ref.ml_kem_keygen(d, z)

        check(f"keygen_tc{t['tcId']}_ek", ek == ek_expected)
        check(f"keygen_tc{t['tcId']}_dk", dk == dk_expected)


def test_encaps():
    print("\n=== NIST ACVP: ML-KEM encaps (ML-KEM-512) ===")
    tg = load_testgroup(
        ENCAPDECAP_JSON,
        lambda tg: tg["parameterSet"] == "ML-KEM-512" and tg["function"] == "encapsulation",
    )
    for t in tg["tests"]:
        ek = bytes.fromhex(t["ek"])
        m = bytes.fromhex(t["m"])
        c_expected = bytes.fromhex(t["c"])
        k_expected = bytes.fromhex(t["k"])

        K, c = kyber_ref.ml_kem_encaps(ek, m)

        check(f"encaps_tc{t['tcId']}_K", K == k_expected)
        check(f"encaps_tc{t['tcId']}_c", c == c_expected)


def test_decaps():
    print("\n=== NIST ACVP: ML-KEM decaps (ML-KEM-512), incluye rechazo implicito ===")
    tg = load_testgroup(
        ENCAPDECAP_JSON,
        lambda tg: tg["parameterSet"] == "ML-KEM-512" and tg["function"] == "decapsulation",
    )
    reasons_seen = set()
    for t in tg["tests"]:
        dk = bytes.fromhex(t["dk"])
        c = bytes.fromhex(t["c"])
        k_expected = bytes.fromhex(t["k"])
        reason = t.get("reason", "valid decapsulation")
        reasons_seen.add(reason)

        K = kyber_ref.ml_kem_decaps(dk, c)

        check(f"decaps_tc{t['tcId']}_K ({reason})", K == k_expected)

    print(f"\n(razones cubiertas en estos casos: {sorted(reasons_seen)})")


def main():
    test_keygen()
    test_encaps()
    test_decaps()

    print()
    if errors == 0:
        print("PASS: kyber_ref.py (ML-KEM-512) coincide con los vectores OFICIALES de NIST ACVP")
        print("      en keyGen, encaps, y decaps (incluyendo rechazo implicito generado por NIST).")
        return 0
    else:
        print(f"FAIL: {errors} discrepancia(s) detectada(s).")
        return 1


if __name__ == "__main__":
    sys.exit(main())
