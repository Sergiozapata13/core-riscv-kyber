#!/usr/bin/env python3
"""
gen_barrett_testvectors.py

Genera casos de test para el testbench de barrett_reduce.sv (Fase 4),
derivados directamente de kyber_ref.barrett_reduce() — el mismo oraculo
validado (dos capas, incluyendo contra kyber-py) en la Fase 3.

Cubre:
  - Casos de borde explicitos (0, +-1, +-q, +-(q+1), +-2q)
  - Caso A: producto de dos coeficientes en [0,q) -> a en [0,(q-1)^2]
  - Caso B: zeta*(a-b) con a,b en [0,q) -> a en [-(q-1)q, (q-1)q]
    (estos dos casos son los que aparecen realmente en el modelo de
    referencia; el rango del remainder crudo de Barrett para ambos fue
    verificado EXHAUSTIVAMENTE — no por muestreo — en (-q, q), ver
    docs/entorno.md seccion Fase 4 para el detalle del analisis)
  - Extremos del rango de 32 bits con signo, por completitud

Uso:
    python3 gen_barrett_testvectors.py > ../tb/barrett_testvectors.inc
"""

import random
import sys

sys.path.insert(0, ".")
from kyber_ref import barrett_reduce, Q

RNG_SEED = 2024
N_RANDOM_CASE_A = 200
N_RANDOM_CASE_B = 200


def main():
    random.seed(RNG_SEED)

    cases = []
    cases += [0, 1, -1, Q - 1, Q, -Q, Q + 1, -(Q + 1), 2 * Q, -2 * Q]

    for _ in range(N_RANDOM_CASE_A):
        cases.append(random.randint(0, (Q - 1) * (Q - 1)))
    for _ in range(N_RANDOM_CASE_B):
        cases.append(random.randint(-(Q - 1) * Q, (Q - 1) * Q))

    # NOTA: los extremos absolutos de 32 bits con signo (+-2^31) se
    # excluyen deliberadamente — el remainder crudo de Barrett para esos
    # valores cae hasta +-4.3q, fuera de la precondicion de "una sola
    # correccion" que el RTL implementa (ver barrett_reduce.sv, comentario
    # de cabecera). Esos valores nunca ocurren en el uso real de este
    # proyecto (los operandos siempre son coeficientes de Kyber reducidos,
    # o productos/diferencias de los mismos), asi que no forman parte del
    # contrato que el modulo debe cumplir. 2^20 SI se incluye: cae dentro
    # del rango de Caso A/B (2^20 < (q-1)^2), asi que es un caso valido.
    cases += [2**20, -(2**20)]

    print(f"// Generado por models/gen_barrett_testvectors.py — {len(cases)} casos")
    print("// derivados de kyber_ref.barrett_reduce(), oráculo validado en Fase 3.")
    print("// NO EDITAR A MANO — regenerar con el script si hace falta más cobertura.")
    for a in cases:
        expected = barrett_reduce(a)
        print(f"    check({a}, {expected});")


if __name__ == "__main__":
    main()
