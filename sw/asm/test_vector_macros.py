#!/usr/bin/env python3
"""
test_vector_macros.py

Verifica que sw/asm/vector_macros.S produce exactamente el encoding
especificado en isa_vectorial_kyber.docx — ensambla un firmware de
prueba que ejercita las 8 instrucciones con distintas combinaciones de
registros escalares/vectoriales, usando el toolchain real
(riscv64-unknown-elf-as), y compara cada palabra generada contra un
codificador Python independiente (misma logica que ya se usó para
verificar vector_control.sv en la Fase 4).

Uso:
    python3 test_vector_macros.py
"""

import subprocess
import sys
import tempfile
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASM_DIR = os.path.join(SCRIPT_DIR, "..", "asm")


def r_type(funct7, rs2, rs1, funct3, rd, opcode):
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode


OPCODE_CUSTOM0 = 0b0001011
FUNCT3 = {
    "vload": 0b000, "vstore": 0b001, "vntt": 0b010, "vintt": 0b011,
    "vpmul": 0b100, "vbarrett": 0b101, "vadd": 0b110, "vsub": 0b111,
}

# Cada caso: (mnemonic_asm, funct7, rs2, rs1, funct3_key, rd)
# rs1/rs2/rd tal como quedarian codificados (numeros crudos, no vX/xN)
CASES = [
    ("vload v0, x5",        0, 0, 5, "vload", 0),
    ("vstore v1, x6",       0, 0, 6, "vstore", 1),
    ("vload v3, x31",       0, 0, 31, "vload", 3),
    ("vntt v2, v0",         0, 0, 0, "vntt", 2),
    ("vintt v3, v1",        0, 0, 1, "vintt", 3),
    ("vbarrett v1, v2",     0, 0, 2, "vbarrett", 1),
    ("vpmul v0, v1, v2",    0, 2, 1, "vpmul", 0),
    ("vpmul v3, v0, v3",    0, 3, 0, "vpmul", 3),
    ("vadd v2, v0, v1",     0, 1, 0, "vadd", 2),
    ("vsub v3, v2, v0",     0, 0, 2, "vsub", 3),
    ("vadd v0, v3, v3",     0, 3, 3, "vadd", 0),
]


def main():
    asm_lines = ['.include "vector_macros.S"', "", ".text"]
    expected = []
    for mnemonic, funct7, rs2, rs1, f3_key, rd in CASES:
        asm_lines.append(mnemonic)
        expected.append(r_type(funct7, rs2, rs1, FUNCT3[f3_key], rd, OPCODE_CUSTOM0))

    with tempfile.TemporaryDirectory() as tmpdir:
        asm_path = os.path.join(tmpdir, "test.s")
        obj_path = os.path.join(tmpdir, "test.o")

        with open(asm_path, "w") as f:
            f.write("\n".join(asm_lines) + "\n")

        result = subprocess.run(
            [
                "riscv64-unknown-elf-as", "-march=rv32i", "-mabi=ilp32",
                "-I", ASM_DIR, "-o", obj_path, asm_path,
            ],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            print("FAIL: el ensamblador reportó errores:")
            print(result.stderr)
            sys.exit(1)

        objdump = subprocess.run(
            ["riscv64-unknown-elf-objdump", "-d", obj_path],
            capture_output=True, text=True,
        )

    # Extraer las palabras generadas, en orden, de la salida de objdump.
    got_words = []
    for line in objdump.stdout.splitlines():
        line = line.strip()
        if "\t.word\t0x" in line or ("\t" in line and "0x" in line and ":" in line):
            parts = line.split("\t")
            for p in parts:
                p = p.strip()
                if len(p) == 8 and all(c in "0123456789abcdef" for c in p):
                    got_words.append(int(p, 16))
                    break

    errors = 0
    if len(got_words) != len(expected):
        print(f"FAIL: se esperaban {len(expected)} palabras, se encontraron {len(got_words)}")
        errors += 1

    for i, (case, exp) in enumerate(zip(CASES, expected)):
        mnemonic = case[0]
        got = got_words[i] if i < len(got_words) else None
        if got == exp:
            print(f"OK   [{mnemonic}]: 0x{exp:08x}")
        else:
            got_str = f"0x{got:08x}" if got is not None else "N/A"
            print(f"FAIL [{mnemonic}]: esperado=0x{exp:08x} generado={got_str}")
            errors += 1

    print()
    if errors == 0:
        print(f"PASS: vector_macros.S — {len(CASES)}/{len(CASES)} instrucciones coinciden con el encoding esperado.")
        return 0
    else:
        print(f"FAIL: {errors} discrepancia(s) detectada(s).")
        return 1


if __name__ == "__main__":
    sys.exit(main())
