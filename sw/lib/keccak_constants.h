// keccak_constants.h
//
// Constantes de Keccak-f[1600] (24 rondas), generadas programaticamente
// por models/gen_keccak_constants.py siguiendo el algoritmo LFSR de FIPS
// 202 seccion 3.2.5 (round constants) y la formula estandar de offsets de
// rotacion (rho) — NO transcritas a mano, y verificadas contra la tabla
// publicada de referencia (keccak.team) antes de confiar en ellas. Mismo
// criterio de "nunca confiar en una constante sin verificar" que ya costo
// un bug real en la Fase 4 (INV128 mal calculado).
//
// NO EDITAR A MANO.

#ifndef KECCAK_CONSTANTS_H
#define KECCAK_CONSTANTS_H

#include <stdint.h>

static const uint64_t KECCAK_RC[24] = {
    0x0000000000000001ULL,
    0x0000000000008082ULL,
    0x800000000000808aULL,
    0x8000000080008000ULL,
    0x000000000000808bULL,
    0x0000000080000001ULL,
    0x8000000080008081ULL,
    0x8000000000008009ULL,
    0x000000000000008aULL,
    0x0000000000000088ULL,
    0x0000000080008009ULL,
    0x000000008000000aULL,
    0x000000008000808bULL,
    0x800000000000008bULL,
    0x8000000000008089ULL,
    0x8000000000008003ULL,
    0x8000000000008002ULL,
    0x8000000000000080ULL,
    0x000000000000800aULL,
    0x800000008000000aULL,
    0x8000000080008081ULL,
    0x8000000000008080ULL,
    0x0000000080000001ULL,
    0x8000000080008008ULL,
};

// Offsets de rotacion, indexados [x][y] (0-4 cada uno)
static const unsigned int KECCAK_RHO[5][5] = {
    {0, 36, 3, 41, 18},
    {1, 44, 10, 45, 2},
    {62, 6, 43, 15, 61},
    {28, 55, 25, 21, 56},
    {27, 20, 39, 8, 14},
};

#endif // KECCAK_CONSTANTS_H
