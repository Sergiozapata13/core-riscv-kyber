#!/usr/bin/env python3
"""
bin_to_hex.py

Convierte un binario RISC-V crudo (.bin) a un archivo .hex de palabras de
32 bits, una por linea, compatible con $readmemh en rtl/core/imem.sv.

Por que no usar 'objcopy -O verilog' directamente: ese formato empaqueta
BYTES individuales (dos digitos hex por linea/entrada), no palabras de 32
bits, y no coincide con lo que $readmemh espera cuando el array destino es
de tipo `logic [31:0] mem []` (como en imem.sv). Este script hace la
conversion correcta: agrupa de a 4 bytes en little-endian (el orden nativo
de RISC-V) y escribe cada palabra como un valor hex de 8 digitos.

Uso:
    riscv64-unknown-elf-objcopy -O binary firmware.elf firmware.bin
    python3 bin_to_hex.py firmware.bin firmware.hex
"""

import sys


def bin_to_hex(bin_path: str, hex_path: str) -> None:
    with open(bin_path, "rb") as f:
        data = f.read()

    # Padding a multiplo de 4 bytes si el binario no cae justo en una
    # palabra completa (puede pasar segun como el linker alinee .bss/.data).
    while len(data) % 4 != 0:
        data += b"\x00"

    lines = ["@00000000"]
    for i in range(0, len(data), 4):
        word = data[i : i + 4]
        # Little-endian: el primer byte es el menos significativo.
        val = word[0] | (word[1] << 8) | (word[2] << 16) | (word[3] << 24)
        lines.append(f"{val:08x}")

    with open(hex_path, "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Uso: {sys.argv[0]} <entrada.bin> <salida.hex>", file=sys.stderr)
        sys.exit(1)
    bin_to_hex(sys.argv[1], sys.argv[2])
