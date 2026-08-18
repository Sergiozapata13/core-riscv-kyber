#!/usr/bin/env python3
"""
gen_keccak_constants.py

Genera sw/lib/keccak_constants.h — las 24 constantes de ronda (RC) y los
25 offsets de rotacion (rho) de Keccak-f[1600], usados por SHA3/SHAKE
(Fase 5).

Las constantes se GENERAN programaticamente (no se transcriben a mano):
  - RC: algoritmo LFSR de FIPS 202, seccion 3.2.5, Algorithm 5 — indexado
    bit a bit exactamente como especifica el pseudocodigo oficial.
  - rho: formula estandar sobre las coordenadas (x,y) del estado.

Ambas tablas se verifican contra la tabla publicada de referencia
(keccak.team) antes de emitir el header — mismo criterio de "nunca
confiar en una constante sin verificar" que ya costo un bug real en la
Fase 4 (la constante INV128 estaba mal calculada pese a decir
"verificado" en el comentario).

Uso:
    python3 gen_keccak_constants.py > ../sw/lib/keccak_constants.h
"""

import sys


def rc_bit(t):
    """FIPS 202 Algorithm 5, indexado explicito bit a bit (R[0]=MSB)."""
    if t % 255 == 0:
        return 1
    R = [1, 0, 0, 0, 0, 0, 0, 0]  # "10000000"
    for _ in range(1, (t % 255) + 1):
        R = [0] + R  # prepend 0 -> 9 elementos
        R[0] ^= R[8]
        R[4] ^= R[8]
        R[5] ^= R[8]
        R[6] ^= R[8]
        R = R[0:8]  # truncar a 8
    return R[0]


def gen_round_constants():
    RC = []
    for ir in range(24):
        rc = 0
        for j in range(7):
            t = j + 7 * ir
            bit = rc_bit(t)
            rc |= bit << ((1 << j) - 1)
        RC.append(rc)
    return RC


def gen_rho_offsets():
    """Offsets de rotacion rho, indexados [x][y], algoritmo estandar."""
    offsets = [[0] * 5 for _ in range(5)]
    x, y = 1, 0
    for t in range(24):
        offsets[x][y] = ((t + 1) * (t + 2) // 2) % 64
        x, y = y, (2 * x + 3 * y) % 5
    return offsets


# Tabla de referencia publicada (keccak.team), para verificacion cruzada.
KNOWN_RC = [
    0x0000000000000001, 0x0000000000008082, 0x800000000000808A, 0x8000000080008000,
    0x000000000000808B, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
    0x000000000000008A, 0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
    0x000000008000808B, 0x800000000000008B, 0x8000000000008089, 0x8000000000008003,
    0x8000000000008002, 0x8000000000000080, 0x000000000000800A, 0x800000008000000A,
    0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
]
# Indexada [y][x] en la fuente de referencia habitual — se compara
# transpuesta contra offsets[x][y].
KNOWN_RHO_YX = [
    [0, 1, 62, 28, 27],
    [36, 44, 6, 55, 20],
    [3, 10, 43, 25, 39],
    [41, 45, 15, 21, 8],
    [18, 2, 61, 56, 14],
]


def main():
    RC = gen_round_constants()
    rho = gen_rho_offsets()

    assert RC == KNOWN_RC, "Round constants NO coinciden con la tabla publicada"
    assert all(
        rho[x][y] == KNOWN_RHO_YX[y][x] for x in range(5) for y in range(5)
    ), "Rho offsets NO coinciden con la tabla publicada"

    print("// keccak_constants.h")
    print("//")
    print("// Constantes de Keccak-f[1600] (24 rondas), generadas programaticamente")
    print("// por models/gen_keccak_constants.py siguiendo el algoritmo LFSR de FIPS")
    print("// 202 seccion 3.2.5 (round constants) y la formula estandar de offsets de")
    print("// rotacion (rho) — NO transcritas a mano, y verificadas contra la tabla")
    print("// publicada de referencia (keccak.team) antes de confiar en ellas. Mismo")
    print('// criterio de "nunca confiar en una constante sin verificar" que ya costo')
    print("// un bug real en la Fase 4 (INV128 mal calculado).")
    print("//")
    print("// NO EDITAR A MANO.")
    print()
    print("#ifndef KECCAK_CONSTANTS_H")
    print("#define KECCAK_CONSTANTS_H")
    print()
    print("#include <stdint.h>")
    print()
    print("static const uint64_t KECCAK_RC[24] = {")
    for rc in RC:
        print(f"    0x{rc:016x}ULL,")
    print("};")
    print()
    print("// Offsets de rotacion, indexados [x][y] (0-4 cada uno)")
    print("static const unsigned int KECCAK_RHO[5][5] = {")
    for x in range(5):
        row = ", ".join(str(rho[x][y]) for y in range(5))
        print(f"    {{{row}}},")
    print("};")
    print()
    print("#endif // KECCAK_CONSTANTS_H")


if __name__ == "__main__":
    main()
