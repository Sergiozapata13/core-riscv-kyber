# ISA Vectorial Custom — Tabla de Referencia (estilo "Green Card")

**Core RISC-V con Aceleración Vectorial para Kyber/ML-KEM — Fase 6**

Fuente de verdad: `isa_vectorial_kyber.docx` (Fase 3, especificación
congelada antes de escribir RTL). Este documento es un resumen de
referencia rápida — ante cualquier discrepancia, el `.docx` manda.

---

## 1. Formato de instrucción

Formato R puro de RISC-V, opcode `custom-0` (uno de los 4 rangos que
RISC-V reserva explícitamente para extensiones no estándar). **Sin
campo de inmediato** — a diferencia de muchas extensiones custom, este
diseño no necesita offsets embebidos en la instrucción.

```
 31         25 24     20 19     15 14  12 11      7 6      0
| funct7      |  rs2     |  rs1    |funct3|   rd    | opcode |
|   7 bits     |  5 bits  |  5 bits | 3 bits| 5 bits  | 7 bits |
```

`opcode = 0001011` (`0x0B`) para las 8 instrucciones, sin excepción.
`funct7` queda reservado en cero para las 8 — espacio para una novena
generación de instrucciones si el proyecto se extiende.

### Dos variantes de uso de los campos, según `funct3`

| Variante | Instrucciones | `rs1` | `rs2` | `rd` |
|---|---|---|---|---|
| **MEMORIA** | `vload`, `vstore` | Registro escalar **completo** (`x0`-`x31`) con la dirección de memoria | No usado (debe ser 0) | `rd[1:0]` = registro vectorial destino (`vload`) u origen (`vstore`) |
| **CÓMPUTO** | `vntt`, `vintt`, `vbarrett`, `vpmul`, `vadd`, `vsub` | `rs1[1:0]` = vreg de entrada A | `rs2[1:0]` = vreg de entrada B (solo `vpmul`/`vadd`/`vsub`) | `rd[1:0]` = vreg destino |

En ambas variantes, cualquier instrucción con un solo operando vectorial
(`vload`, `vntt`, `vintt`, `vbarrett`) **ignora** `rs2` — debe codificarse
en 0 por convención, pero el hardware no lo consulta.

**Sin offset en `vload`/`vstore`**: la dirección es el valor *directo*
de `rs1`. Si el firmware necesita un desplazamiento, debe calcular la
dirección efectiva con una instrucción escalar antes (`addi t0, base,
offset` → `vload v1, t0`).

---

## 2. Banco de registros vectoriales

| Registro | Ancho | Contenido |
|---|---|---|
| `v0`–`v3` | 128 × 32 bits (4096 bits) cada uno | Un polinomio Kyber completo: 256 coeficientes de 16 bits |

Solo **4 registros** — suficiente para mantener simultáneamente
polinomio A, polinomio B, resultado, y un temporal (la cadena típica
`NTT(a) × NTT(b) → INTT` de Kyber). Seleccionados con 2 bits (`rd[1:0]`,
`rs1[1:0]`, `rs2[1:0]`) — los bits altos de esos campos quedan
reservados/ignorados en la variante CÓMPUTO.

---

## 3. Conjunto de instrucciones (por `funct3`)

| `funct3` | Mnemonic | Variante | Ciclos¹ | Semántica |
|---|---|---|---:|---|
| `000` | `vload vd, rs1` | MEMORIA | 257 | Carga un polinomio de 256 coeficientes desde memoria (dirección en `rs1`) al registro vectorial `vd` |
| `001` | `vstore vs, rs1` | MEMORIA | 257 | Guarda el registro vectorial `vs` a memoria (dirección en `rs1`) |
| `010` | `vntt vd, vs1` | CÓMPUTO | 1153 | NTT forward (Cooley-Tukey, 7 niveles) — entrada en orden natural, salida en orden bit-reversed |
| `011` | `vintt vd, vs1` | CÓMPUTO | 1409 | NTT inverso (Gentleman-Sande, 7 niveles + escalado ×`INV128`) — entrada bit-reversed, salida en orden natural |
| `100` | `vpmul vd, vs1, vs2` | CÓMPUTO | 257 | Multiplicación punto a punto de dos polinomios en dominio NTT (64 productos de grado 1) |
| `101` | `vbarrett vd, vs1` | CÓMPUTO | 257 | Reducción modular Barrett de los 256 coeficientes crudos de `vs1` |
| `110` | `vadd vd, vs1, vs2` | CÓMPUTO | 257 | Suma de polinomios, coeficiente a coeficiente, mod `q` |
| `111` | `vsub vd, vs1, vs2` | CÓMPUTO | 257 | Resta de polinomios, coeficiente a coeficiente, mod `q` |

¹ Ciclos medidos directamente (`start`→`done`) sobre `vector_unit.sv`,
instancia aislada — ver `tb/tb_constant_time.cpp` (`vntt`/`vintt`/
`vpmul`/`vadd`/`vsub`/`vbarrett`) y medición equivalente para
`vload`/`vstore`. **Constant-time confirmado**: cada instrucción tarda
el mismo número de ciclos sin importar el valor de los datos de entrada
(ceros, valor máximo válido, o datos aleatorios realistas de Kyber) —
ver sección de constant-time en `docs/entorno.md`.

Las 8 combinaciones de `funct3` (3 bits) están asignadas — cubren el
conjunto completo de operaciones que ML-KEM-512 necesita en keygen,
encapsulation, y decapsulation.

---

## 4. Parámetros de Kyber/ML-KEM-512 relevantes al hardware

| Constante | Valor | Uso |
|---|---|---|
| `q` | 3329 | Módulo primo |
| `n` | 256 | Coeficientes por polinomio (= ancho de un `vreg`) |
| `BARRETT_SHIFT` | 26 | Reducción Barrett |
| `BARRETT_MULTIPLIER` | 20159 | `= ⌈2²⁶/q⌋` |
| `INV128` | 3303 | Inverso modular de 128 mod `q`, escalado final de `vintt` |
| `ζ` (root of unity) | 17 | Base de la tabla de 128 twiddle factors (ROM) |

---

## 5. Ejemplo de código (macros de `sw/asm/vector_macros.S`)

Secuencia típica: cargar dos polinomios, llevarlos a dominio NTT,
multiplicar, volver a dominio normal, guardar:

```asm
    vload  v0, a0        # v0 <= polinomio A (direccion en a0)
    vload  v1, a1        # v1 <= polinomio B (direccion en a1)
    vntt   v0, v0         # v0 <= NTT(A)
    vntt   v1, v1         # v1 <= NTT(B)
    vpmul  v2, v0, v1      # v2 <= NTT(A) . NTT(B)
    vintt  v2, v2          # v2 <= INTT(v2)  (dominio normal)
    vstore v2, a2         # memoria[a2] <= v2
```

Cada instrucción, incluidas las de cómputo, retorna el control al
pipeline escalar apenas **despacha** (no cuando termina) — el pipeline
escalar sigue avanzando con instrucciones independientes mientras la
unidad vectorial procesa en segundo plano (Apéndice A.1, diseño
desacoplado). Una nueva instrucción vectorial que dependa del resultado
de la anterior se stallea automáticamente vía hardware (scoreboard +
recurso único) hasta que esté disponible — **pero esa protección cubre
únicamente los registros vectoriales, no la memoria**: código escalar
que lee inmediatamente la memoria de destino de un `vstore` debe
sincronizarse explícitamente (ver hallazgo #3 en `docs/entorno.md`,
resuelto en `sw/asm/poly_vector_ops.S` con un `vload` de sincronización).

---

## 6. Ejemplos de encoding (para referencia rápida al desensamblar)

Instrucción y su encoding hexadecimal exacto — misma metodología de
verificación que `sw/asm/test_vector_macros.py` (11/11 casos
verificados bit a bit contra un codificador Python independiente):

| Instrucción | Encoding (hex) |
|---|---|
| `vload v1, a0` (`a0`=`x10`) | `0x0005008b` |
| `vstore v2, a1` (`a1`=`x11`) | `0x0005910b` |
| `vntt v1, v0` | `0x0000208b` |
| `vintt v3, v2` | `0x0001318b` |
| `vpmul v0, v1, v2` | `0x0020c00b` |
| `vbarrett v1, v2` | `0x0001508b` |
| `vadd v2, v0, v1` | `0x0010610b` |
| `vsub v3, v2, v0` | `0x0001718b` |

Nota los bits bajos constantes: **`...0b`** en todas — el opcode
`0001011` ocupa siempre los 7 bits menos significativos.
