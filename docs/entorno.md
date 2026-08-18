# Fase 0 — Entorno de simulación

## Herramientas instaladas (versión fijada)

| Herramienta | Versión | Fuente |
|---|---|---|
| Verilator | 5.020 (Debian 5.020-1) | `apt install verilator` |
| riscv64-unknown-elf-gcc | 13.2.0-11ubuntu1+12 | `apt install gcc-riscv64-unknown-elf` |
| binutils (as/ld/objcopy) | 2.42-1ubuntu1+6 | `apt install binutils-riscv64-unknown-elf` |
| picolibc-riscv64-unknown-elf | 1.8.6-2 | `apt install picolibc-riscv64-unknown-elf` |
| GTKWave | 3.3.116-1build2 | `apt install gtkwave` |

Instalación reproducible:

```bash
apt-get update
apt-get install -y verilator gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf \
    picolibc-riscv64-unknown-elf gtkwave
```

### Nota sobre el toolchain: riscv64 en vez de riscv32

El paquete Ubuntu disponible es `gcc-riscv64-unknown-elf`, no un paquete
`riscv32-*` separado. Esto no es un problema: el toolchain soporta multilib y
genera código RV32I puro con las flags:

```
-march=rv32i -mabi=ilp32
```

Verificado en el `objdump` del firmware de prueba (`hello_riscv.elf`): el
código generado usa únicamente instrucciones base RV32I (`auipc`, `lui`, `sw`,
`lw`, `add`, `addi`, `bge`, `jal`, `jalr`, `ret`) — sin `mul`/`div` (extensión M),
sin atómicos (extensión A), sin punto flotante. Confirma que el binario es
RV32I estricto, coherente con el alcance del proyecto.

## Estructura de repositorio (Fase 0)

```
rtl/core/       Datapath escalar (Fase 1-2)
rtl/vector/     Unidad vectorial (Fase 4)
tb/             Testbenches de Verilator
sim/            Makefile + artefactos de simulación (.vcd, .elf, .hex)
docs/           Documentación (este archivo, cronograma, specs de ISA futuras)
sw/crt/         Startup code (start.s) y linker script (link.ld)
sw/tests/       Firmware de prueba
```

## Verificación — Criterio 1: Verilator + waveform

**Módulo:** `rtl/core/hello_counter.sv` — contador de 8 bits trivial, sin
relación con el core real; existe únicamente para validar la cadena de
herramientas antes de escribir el datapath.

**Testbench:** `tb/tb_hello_counter.cpp` — corre 20 ciclos de reloj tras un
reset de 2 ciclos, verifica que `count` incrementa exactamente como se espera
en cada flanco de subida, y vuelca una waveform.

**Comando:**
```bash
cd sim && make hello_verilator
```

**Resultado:**
```
PASS: hello_counter conto 20 ciclos correctamente. VCD escrito en sim/hello_counter.vcd
```

**Waveform:** `sim/hello_counter.vcd`, formato VCD estándar, verificado
manualmente como texto (cabecera `$var`, transiciones de `clk`/`rst_n`/`count`
presentes y coherentes) — legible en GTKWave o Surfer.

### Detalle de comportamiento verificado (útil para Fase 1/2)

El registro sigue la semántica esperada de un flip-flop síncrono con reset
asíncrono activo en bajo: mientras `rst_n=0`, `count` se mantiene en 0 en cada
flanco. En el **primer** flanco de subida posterior a liberar `rst_n`, el
valor observado ya es `count=1` (no `count=0`) — porque ese flanco evalúa
`count <= count + 1` sobre el valor previo (0). Este detalle de temporización
(cuándo exactamente un valor se vuelve observable tras liberar el reset) es
el mismo tipo de razonamiento que se va a necesitar para el register file y
los registros de segmentación del pipeline en la Fase 1/2 — vale la pena
tenerlo interiorizado desde ahora.

## Verificación — Criterio 2: toolchain RISC-V .c → .elf → .hex/.bin

**Firmware:** `sw/tests/hello.c` — programa mínimo sin libc (`-nostdlib`), que
escribe un patrón reconocible (`0xCAFEF00D`) a una dirección fija de memoria
y hace una suma trivial, para ejercitar tanto acceso a memoria como la ALU.

**Startup/linker:** `sw/crt/start.s` (setup de stack pointer, limpieza de
`.bss`, salto a `main`) + `sw/crt/link.ld` (memoria única de 64K en
`0x00000000`).

**Comando:**
```bash
cd sim && make hello_riscv
```

**Resultado:**
```
=== hello_riscv.elf generado correctamente ===
   text    data     bss     dec     hex filename
    104       0       0     104      68 hello_riscv.elf
```

Verificado con `readelf -h`: `ELF32`, `Machine: RISC-V`. Verificado con
`objdump -d`: disassembly limpio, solo instrucciones RV32I base (ver nota
arriba). Extraído correctamente a `hello_riscv.hex` (formato Verilog, listo
para `$readmemh` en una memoria simulada) y `hello_riscv.bin` (binario crudo).

## Estado de Fase 0

Ambos criterios de verificación de la Fase 0 pasan de forma reproducible.
Entorno listo para arrancar la Fase 1 (core escalar RV32I monociclo).

## Fase 1 — Core escalar RV32I monociclo (cierre)

### Módulos implementados

| Módulo | Archivo | Tests |
|---|---|---|
| Register file | `rtl/core/regfile.sv` | 7 casos |
| ALU | `rtl/core/alu.sv` | 20 casos |
| Unidad de control | `rtl/core/control.sv` | 28 casos |
| Generador de inmediato | `rtl/core/imm_gen.sv` | 21 casos |
| Unidad de branch | `rtl/core/branch_unit.sv` | 14 casos |
| Memoria de instrucciones | `rtl/core/imem.sv` | 9 casos |
| Memoria de datos | `rtl/core/dmem.sv` | 13 casos |
| **Datapath completo** | `rtl/core/core_top.sv` | Fibonacci end-to-end |

### Decisión de diseño: bypass del register file es parametrizable

El `regfile` tiene un parámetro `ENABLE_BYPASS` (default `1`). El bypass
write-then-read (necesario para que una lectura y escritura al mismo
registro en el mismo ciclo de reloj devuelva el valor nuevo) es correcto y
necesario en un **pipeline** (Fase 2), donde WB de una instrucción y ID de
otra ocurren en el mismo ciclo físico. En el **datapath monociclo**
(`core_top.sv`), en cambio, ese mismo bypass crea un **ciclo combinacional
real** (`alu_result → dmem → regfile write → regfile read (bypass) →
alu_opA → alu → alu_result`), porque toda una instrucción completa su
IF-ID-EX-MEM-WB dentro de un único ciclo. Verilator lo marca como
`UNOPTFLAT` — no es un falso positivo, es un lazo genuino en ese cableado.

**Solución:** `core_top.sv` instancia `regfile` con `ENABLE_BYPASS(1'b0)`.
En un monociclo la escritura ocurre en el flanco, después de que la
lectura del mismo ciclo ya fue consumida — no hace falta bypass. Este
mismo parámetro se reutiliza en la Fase 2 con `ENABLE_BYPASS(1'b1)` para
el pipeline real. Documentado en detalle en `regfile.sv`.

### Firmware de prueba: Fibonacci iterativo

`sw/tests/fib.s` — ensamblador puro RV32I (sin C), calcula los primeros
10 términos de Fibonacci (0,1,1,2,3,5,8,13,21,34) y los almacena en
memoria de datos desde `0x100`, más un patrón de status (`0xC0FFEE00`) en
`0x200` para confirmar terminación. Ejercita `addi`, `add`, `sw`, `lw`,
`bge`, `j`, `slli` — aritmética, memoria, y control de flujo juntos.

Flujo de compilación (documentado porque `objcopy -O verilog` no genera
el formato que `$readmemh` espera para un array de palabras de 32 bits):
```bash
riscv64-unknown-elf-as -march=rv32i -mabi=ilp32 sw/tests/fib.s -o fib.o
riscv64-unknown-elf-ld -m elf32lriscv -T sw/crt/link.ld fib.o -o fib.elf
riscv64-unknown-elf-objcopy -O binary fib.elf fib.bin
python3 sw/bin_to_hex.py fib.bin sw/tests/fib.hex
```

`sw/bin_to_hex.py` es un script propio: agrupa el binario en palabras de
32 bits little-endian y escribe cada una como hex de 8 dígitos, una por
línea — el formato exacto que `$readmemh` necesita para `logic [31:0]
mem []`, a diferencia del formato byte-por-byte de `objcopy -O verilog`.

**Comando:**
```bash
cd sim && make core_top
```

**Resultado:** los 10 valores de Fibonacci correctos, patrón de status
verificado, y el PC confirmado estabilizado en la dirección del loop
`halt` (`0x60`) — el programa corrió completo, no crasheó ni quedó
corriendo fuera de control.

### Estado de Fase 1

Los criterios de verificación de la Fase 1 (suite dirigida por módulo +
programa no trivial end-to-end) pasan de forma reproducible. Entorno listo
para arrancar la Fase 2 (segmentación a pipeline de 5 etapas).

## Fase 2 — Segmentación a pipeline de 5 etapas (cierre)

### Módulos implementados

| Módulo | Archivo | Tests |
|---|---|---|
| Registro IF/ID | `rtl/core/if_id_reg.sv` | 14 casos |
| Registro ID/EX | `rtl/core/id_ex_reg.sv` | 34 casos |
| Registro EX/MEM | `rtl/core/ex_mem_reg.sv` | 21 casos |
| Registro MEM/WB | `rtl/core/mem_wb_reg.sv` | 15 casos |
| Forwarding unit | `rtl/core/forwarding_unit.sv` | 9 casos |
| Hazard detection unit | `rtl/core/hazard_detection_unit.sv` | 8 casos |
| **Datapath pipelineado completo** | `rtl/core/core_top_pipelined.sv` | Fibonacci — resultados idénticos a Fase 1 |

### Política de burbuja: valid explícito + NOP/inerte como red de seguridad

Cada registro de segmentación combina dos mecanismos: un bit `valid`
explícito (que la lógica de control/forwarding consulta) y, además, fuerza
las señales que importan a un estado inerte cuando `valid=0` (NOP real en
`if_id_reg`; `reg_write`/`mem_read`/`mem_write`/`branch`/`jump`
forzados a 0 en los registros posteriores, que no llevan una `instr` cruda
para reinyectar). Doble seguridad: si algo en el datapath no consultara
`valid` correctamente, la burbuja de todas formas no puede afectar estado
arquitectural. El mismo criterio se aplicó a `rs1`/`rs2` en `id_ex_reg`
(forzados a `x0` en burbuja), para que la forwarding unit nunca dispare un
forward espurio contra una burbuja.

### Prioridad flush > stall

En todos los registros de segmentación, si `flush` y `stall` están
activos el mismo ciclo, `flush` tiene prioridad (invalidar es más
"fuerte" que mantener). Verificado explícitamente en `if_id_reg`.

### Forwarding: prioridad EX/MEM sobre MEM/WB

Cuando ambas fuentes de forwarding (EX/MEM y MEM/WB) escriben al mismo
registro que la instrucción en EX necesita, EX/MEM gana — es el resultado
más reciente. Caso clásico: `add x1,.. / add x1,.. / usa x1`. Verificado
explícitamente en `tb_forwarding_unit.cpp` (caso 5). `x0` nunca se
forwardea desde ninguna fuente, aunque `reg_write` esté activo.

### Load-use hazard: el único caso que el forwarding no resuelve

```
lw   x1, 0(x2)     // x1 recién existe al FINAL de MEM
add  x3, x1, x4    // necesita x1 en su propio EX, un ciclo antes
```

No hay forwarding posible hacia atrás en el tiempo. `hazard_detection_unit`
detecta la condición (`id_ex_reg.mem_read && id_ex_reg.rd` coincide con
`rs1`/`rs2` de la instrucción en ID) y genera un stall de 1 ciclo:
`pc_stall`, `if_id_stall`, `id_ex_flush` — todo lo cual re-decodifica la
instrucción dependiente un ciclo más tarde, cuando el forwarding EX/MEM→EX
ya puede alcanzar el dato.

### Política de flush ante branch/jump: 2 ciclos, resueltos en EX

El branch/jump se resuelve en EX (`branch_unit` + cálculo de target), no
en ID. Para ese momento el pipeline ya fetcheó (IF) y decodificó (ID) dos
instrucciones de más asumiendo flujo secuencial. Cuando `branch_taken` o
`jump` resultan verdaderos en EX: `if_id_reg` e `id_ex_reg` reciben
`flush` (dos ciclos de burbuja), y `pc_next` toma el target calculado en
vez de `pc_plus4`.

**Consecuencia observable, documentada porque no es intuitiva**: en el
loop `halt: j halt` del firmware de prueba, el PC del pipeline **nunca
converge a un único valor fijo** como sí hacía en el monociclo (Fase 1)
— en cambio, cicla establemente entre `0x60/0x64/0x68`, porque cada
iteración del salto incondicional alcanza a fetchear especulativamente
las 2 instrucciones siguientes antes de que el flush las invalide. Esto
es el comportamiento **correcto** del pipeline, no un bug — el testbench
de cierre (`tb_core_top_pipelined.cpp`) lo detecta explícitamente como
criterio de "programa terminado" (PC confinado a ese rango de 3 palabras),
en vez de asumir un valor único como en el criterio de la Fase 1.

### Decisión de diseño: bypass del regfile ahora SÍ habilitado

A diferencia de `core_top.sv` (Fase 1, monociclo, `ENABLE_BYPASS(1'b0)`),
`core_top_pipelined.sv` instancia el regfile con `ENABLE_BYPASS(1'b1)`.
En pipeline, WB de una instrucción e ID de OTRA instrucción distinta
ocurren en el mismo ciclo físico — el bypass ahí es necesario y correcto
(sin él, ID leería un valor stale para una dependencia RAW de 1 ciclo),
no el ciclo combinacional espurio que se daba en monociclo.

### Verificación de no regresión: mismo firmware, mismos resultados

`tb_core_top_pipelined.cpp` corre el **mismo** `fib.hex` usado para
verificar el monociclo en la Fase 1, y compara contra los mismos 10
valores esperados y el mismo patrón de status — la comparación más
directa posible de que la segmentación no alteró el comportamiento
arquitectural del programa.

**Comando:**
```bash
cd sim && make core_top_pipelined
```

### Estado de Fase 2

Los criterios de verificación de la Fase 2 (cada componente aislado +
programa completo con resultados idénticos al monociclo) pasan de forma
reproducible. Entorno listo para arrancar la Fase 3 (diseño de la ISA
vectorial custom).

## Fase 4 — Integración de la unidad vectorial al pipeline (cierre)

La especificación de la ISA vectorial (8 instrucciones, encoding, análisis
constant-time) y el modelo de referencia en Python quedaron documentados
en `isa_vectorial_kyber.docx` (Fase 3). Esta sección cubre la
implementación RTL y su integración con el pipeline de la Fase 2.

### Módulos implementados

| Módulo | Rol | Tests |
|---|---|---|
| `barrett_reduce.sv` | Reducción modular base, usada por todo lo demás | 412 casos |
| `butterfly_ct.sv` / `butterfly_gs.sv` | Mariposas Cooley-Tukey / Gentleman-Sande | 305 + 305 casos |
| `base_case_mul.sv` | Multiplicación de grado 1, dominio NTT | 605 casos |
| `poly_addsub.sv` | Suma/resta de coeficientes | 612 casos |
| `vreg_file.sv` | Banco de 4 registros vectoriales, 256 coef de 16b c/u | 15 casos |
| `twiddle_rom.sv` | Tabla de 128 twiddle factors, generada desde el modelo de referencia | — |
| `ntt_engine.sv` (+ `_top.sv`) | Motor FSM de `vntt`/`vintt`, 7 niveles × 128 mariposas | round-trip 256/256 coef |
| `vpmul_engine.sv` (+ `_top.sv`) | Motor de `vpmul` | 256/256 coef |
| `addsub_engine.sv` (+ `_top.sv`) | Motor de `vadd`/`vsub` | 256/256 coef ×2 |
| `barrett_engine.sv` (+ `_top.sv`) | Motor de `vbarrett` | 256/256 coef |
| `vload_vstore_engine.sv` (+ `_top.sv`) | Motor de `vload`/`vstore` | round-trip 256/256 coef |
| `vector_unit.sv` | Integra los 5 motores, banco/ROM compartidos, mux por `funct3` | 8 instrucciones encadenadas |
| `vector_control.sv` | Decodificador (vive en EX, no ID — ver más abajo) | 24 casos |
| `vector_scoreboard.sv` | Scoreboard de 4 bits (Apéndice A.4) | 9 casos |
| `dmem.sv` (extendida) | +1 puerto independiente para `vector_unit` | 17 casos |

### Decisión de diseño: `vector_control` vive en EX, no en ID

`id_ex_reg` ya propaga `opcode`/`funct3`/`rs1`/`rs2`/`rd` completos hasta
EX (los usa el mux de operando de la ALU para AUIPC/LUI/JALR), y
`control.sv` ya trata el opcode custom-0 como "no reconocido" — todas las
señales de control escalares quedan en 0 por defecto para una
instrucción vectorial, sin necesitar ningún cambio. Por lo tanto no hace
falta decodificar de nuevo en ID ni agregar campos nuevos al registro de
segmentación: `vector_control` es un decodificador puramente
combinacional en EX, que además necesita `ex_rs1_fwd` (la dirección
escalar para `vload`/`vstore`, ya forwardeada por el pipeline existente)
— ese dato solo está disponible en EX, otra razón para no decodificar en
ID.

### Decisión de diseño: `dmem` extendida a 2 puertos, no arbitraje

`vload`/`vstore` acceden a la misma `dmem` que usa la etapa MEM del
pipeline escalar, pero pueden tardar hasta 256 ciclos — y mientras tanto
el pipeline escalar sigue corriendo sus propias instrucciones de memoria
(decisión A.1). Con un solo puerto esto es un conflicto estructural real.
Se evaluaron dos opciones: arbitrar un único puerto con stalls
adicionales, o extender `dmem` a 2 puertos independientes (mismo patrón
ya usado en `vreg_file`). Se eligió la segunda por evitar la complejidad
de arbitraje que el proyecto viene evitando deliberadamente en otros
puntos (ej. NTT secuencial en vez de paralela). El puerto 1 preserva
exactamente la interfaz y comportamiento de la Fase 1 (los 13 casos
originales siguen pasando sin modificación); el puerto 2 es nuevo,
exclusivo de `vector_unit`. En colisión de escritura a la misma palabra,
el puerto 2 gana (misma convención que `vreg_file`).

### Decisión de diseño: el scoreboard es redundante, se construye igual

Dado que `vector_unit` es un recurso único no pipelineado (Apéndice A.1),
`vector_control` ya bloquea `vec_start` mientras `vector_unit_busy=1`,
sin importar qué registros toque la nueva instrucción — como máximo un
bit del scoreboard de 4 bits puede estar activo a la vez. Se demostró que,
bajo este diseño, el chequeo fino por registro que hace el scoreboard es
funcionalmente redundante con el chequeo grueso "unidad ocupada" para
efectos de stall del pipeline: no existe ningún escenario donde uno
permita avanzar algo que el otro bloquearía. Se construyó el scoreboard
igual, tal como especifica el Apéndice A.4, por tres razones: fidelidad
al diseño ya cerrado, ser el gancho natural para una futura cola de
instrucciones (que sí rompería la redundancia), y dar visibilidad útil en
waveforms para la Fase 6. El stall real del pipeline usa la condición más
simple y suficiente: `is_vector_instr_en_EX && vector_unit_busy`.

### Mecanismo de stall: `id_ex_reg.stall` + `ex_mem_reg.flush`

A diferencia del hazard de load-use (que descarta y re-decodifica la
instrucción dependiente), una instrucción vectorial que no puede
despachar debe permanecer **la misma** en EX hasta que `vector_unit` se
libere. Se usa el puerto `stall` de `id_ex_reg` (antes fijo en `1'b0`)
para congelar EX con la instrucción vectorial, y el puerto `flush` de
`ex_mem_reg` para insertar una burbuja hacia MEM/WB en cada ciclo de
espera — así la instrucción no se "completa" una vez por cada ciclo de
stall, solo cuando finalmente despacha.

### Bugs reales encontrados y corregidos

- **NTT no operaba in-place**: la FSM leía siempre del registro fuente en
  vez de encadenar los 7 niveles sobre el destino — a partir del segundo
  nivel, los resultados del nivel anterior estaban en el registro
  incorrecto. Corregido agregando un estado `COPY` inicial (copia
  fuente→destino, 256 ciclos) antes de operar in-place, igual que el
  buffer único del modelo de referencia.
- **Fórmula de Gentleman-Sande incorrecta**: la sección 6.3 original de
  `isa_vectorial_kyber.docx` especificaba el twiddle factor inverso y
  resta `(a−b)`, siguiendo la convención académica estándar de FFT/NTT.
  La implementación real de Kyber usa el twiddle **directo** (mismo que
  Cooley-Tukey) y resta `(b−a)`. Ambos errores se confirmaron debuggeando
  el motor NTT completo contra el modelo de referencia, y se corrigieron
  en el RTL y en el documento de especificación.
- **Constante `INV128` mal calculada**: `3212` en el comentario original
  (nunca verificada realmente contra Python pese a decir "verificado"),
  valor correcto `3303` — confirmado `128×3303 mod 3329 = 1`.
- **Overflow de bits en `poly_addsub`**: `sum_raw` en 13 bits con signo
  (máximo 4095) no alcanzaba para la suma máxima real `2×(q−1)=6656`,
  causando resultados incorrectos en ~18% de los casos de prueba
  (justamente los que tenían `a+b≥q`). Corregido a 14 bits.
- **Truncamiento a 12 bits en `vbarrett`**: la entrada al `barrett_reduce`
  interno truncaba a `[11:0]`, perdiendo bits altos — pasó desapercibido
  en los demás motores porque todos operan sobre coeficientes ya
  reducidos a `[0,q)` (caben en 12 bits), pero `vbarrett` existe
  justamente para reducir valores crudos más grandes que `q`.

### Precondición de diseño de `barrett_reduce`: rango de entrada acotado

`barrett_reduce` implementa una sola corrección bidireccional (no una
cadena de correcciones), verificada exhaustivamente correcta para el
rango real de uso del proyecto (productos y diferencias de coeficientes
ya reducidos a `[0,q)`, remainder crudo siempre en `(-q,q)`, confirmado
sobre 33M+ valores). No garantiza corrección para el rango completo de 32
bits con signo (los extremos `±2^31` caen fuera del rango soportado) —
documentado explícitamente como precondición de uso, no una limitación
descubierta tarde.

### Verificación de integración: firmware real, no señales simuladas a mano

El criterio de cierre más exigente de la fase: un firmware con
instrucciones RV32I estándar y vectoriales custom mezcladas (codificado a
mano, con las instrucciones escalares verificadas bit a bit contra el
toolchain real `riscv64-unknown-elf-as` antes de confiar en el
codificador propio para las instrucciones custom), corriendo por el
pipeline completo:

```
addi x2, x0, 0
vntt v1, v0            # ~1152 ciclos
addi x2, x2, 1  (x5)   # canario — debe avanzar RAPIDO, sin esperar a vntt
sw   x2, 0x100(x0)
vbarrett v1, v1        # debe ESPERAR a que vntt termine (recurso único)
addi x3, x0, 99
sw   x3, 0x104(x0)
```

Resultados: el canario aparece en memoria en el ciclo 11 (confirmando
desacople, Apéndice A.1); la unidad vectorial registra exactamente 2
despachos con un hueco máximo de 1 ciclo entre operaciones y 1410 ciclos
totales ocupada (confirmando serialización sin solapamiento, Apéndice
A.4). Nota de diseño encontrada durante este test: el criterio inicial de
"serialización" asumía que había que esperar a que **ambas** operaciones
vectoriales terminaran completamente — incorrecto, porque el desacople
aplica a *toda* instrucción vectorial, no solo a la primera. El criterio
correcto (y el que finalmente se verificó) es que la segunda instrucción
vectorial no pueda **despachar** hasta que la primera **termine**, lo
cual es una propiedad más débil y es exactamente lo que Apéndice A.4
especifica.

**Comandos:**
```bash
cd sim
make core_top_pipelined         # regresión: mismo fib.s, resultados idénticos a Fase 1/2
make core_top_pipelined_vector  # integración: firmware mixto escalar+vectorial
```

### Estado de Fase 4

Los 33 módulos del proyecto (15 escalares + 18 vectoriales/integración)
pasan de forma reproducible, incluyendo la verificación de integración
con firmware real. Entorno listo para arrancar la Fase 5 (Kyber
end-to-end como firmware bare-metal, con métrica de speedup vectorial vs.
escalar).

## Fase 5 — Kyber end-to-end en el core (en curso)

### Alcance decidido

Kyber completo y fiel al estándar (ML-KEM, FIPS 203, nivel de seguridad
ML-KEM-512/k=2), no un subconjunto simplificado — incluye SHA3/SHAKE
real, CBD, empaquetado/compresión. La derivación de aleatoriedad
(semillas, matriz `A`) usa un generador determinista propio en vez del
DRBG oficial de NIST (evita implementar AES-256-CTR-DRBG, que no aporta
a la arquitectura de aceleración vectorial que es el foco del proyecto),
documentado explícitamente como simplificación consciente.

### Macros de ensamblador (Apéndice A.3)

`sw/asm/vector_macros.S` — las 8 instrucciones vectoriales con mnemonics
legibles (`vntt v1, v0`), verificadas bit a bit contra un codificador
Python independiente (11 casos) y confirmadas funcionando tanto con
`riscv64-unknown-elf-as` directo como con `gcc -x assembler-with-cpp`
(necesario para firmware mixto C/asm). Encoding es formato R puro **sin
campo de inmediato** — `vload`/`vstore` toman la dirección directa del
valor de un registro escalar; el firmware debe precalcular la dirección
efectiva con `addi` si necesita un offset.

### Keccak-f[1600] / SHA3-256 / SHAKE128/256

`sw/lib/keccak.c` — implementado en C portable, sin dependencias de
libc. Las 24 constantes de ronda y los offsets de rotación se generan
programáticamente (`models/gen_keccak_constants.py`, algoritmo LFSR de
FIPS 202) y se verifican contra la tabla publicada de referencia con un
`assert` en el propio script generador — mismo criterio de "nunca
transcribir una constante sin verificar" que costó el bug de `INV128` en
la Fase 4.

**Compatibilidad con RV32I sin extensión M**: el código original usaba
el operador `%` sobre valores no potencia de 2, generando llamadas a
`__modsi3` de `libgcc` — reemplazado por tablas de lookup precalculadas
(`MOD5_PLUS1`, `MOD5_PLUS4`, `MOD5_PLUS2`, `PI_LANE`), eliminando esa
dependencia por completo. También se reemplazaron `={0}` y los bucles de
copia (que el compilador reconoce como patrones de `memset`/`memcpy` y
sustituye por llamadas a biblioteca inexistente en este entorno
`-nostdlib`) por funciones propias triviales (`mem_zero`, `mem_copy`).
El binario final compilado con `-march=rv32i -ffreestanding` no contiene
ninguna instrucción `mul`/`div`/`rem` ni símbolo externo sin resolver.

**Validación en tres capas**:
1. Algoritmo correcto — nativo (compilado para el sandbox) contra
   `hashlib` de Python: 9/9 casos (SHA3-256, SHAKE128, SHAKE256, cada
   uno sobre 3 mensajes de longitud distinta).
2. Toolchain compatible — RV32I puro, confirmado sin instrucciones M ni
   dependencias de libc tras resolver los dos problemas de arriba.
3. Ejecución real en el core — firmware completo (`crt/start.s` +
   `keccak.c` + programa de prueba) corriendo sobre `core_top_pipelined`
   vía Verilator, resultado verificado byte a byte.

### Bug real encontrado: `dmem` nunca se inicializaba con `.rodata`

La capa 3 de validación de Keccak falló inicialmente con un resultado
casi todo en cero, pese a que las capas 1 y 2 ya habían pasado. El
diagnóstico se hizo de forma incremental y sistemática (mismo patrón
usado en toda la Fase 4): se aisló la variable sospechosa escribiendo
firmwares cada vez más simples hasta encontrar el punto exacto de
divergencia —

1. Un patrón fijo de bytes escrito con literales directos: correcto.
2. Una variable local de 32 bits en el stack: correcta.
3. Una variable local de **64 bits** en el stack: **todo cero**.

El desensamblado del caso 3 reveló la causa: el compilador, al no poder
construir un literal de 64 bits inline con `lui`/`addi` (rango
insuficiente), lo coloca en una sección `.rodata`/`.srodata.cst8` en una
dirección fija del binario, y lo carga con `lw` — una instrucción de
**datos**, que pasa por `dmem`, no por `imem`. Este core tiene
arquitectura Harvard (memorias separadas), pero el linker script asume
memoria unificada (`.text`, `.rodata`, `.data` en el mismo espacio de
direcciones del binario) — un desajuste ya anotado como riesgo explícito
en el comentario original de `sw/crt/link.ld` desde la Fase 0, pero que
nunca se había materializado porque todo el firmware anterior (`fib.s`,
`hello.c`) usaba solo constantes pequeñas que el compilador podía
construir inline, sin tocar `.rodata`.

Confirmado con evidencia directa: `readelf` mostró la constante de 64
bits del caso de prueba en `.srodata.cst8`, dirección `0x88` — la misma
dirección que el desensamblado usaba en `lw a0, 136(zero)`. En el
firmware real de Keccak, las 24 constantes `KECCAK_RC` (192 bytes) caen
en `.rodata` de la misma forma. Solo `imem.sv` tenía un parámetro
`INIT_FILE` que carga el binario compilado — `dmem.sv` arrancaba siempre
en ceros. Verificación matemática de por qué el síntoma era "todo cero"
y no un valor parcialmente incorrecto: Keccak con las 24 constantes de
ronda en cero, aplicado sobre un estado inicial en cero, permanece en
cero durante las 24 rondas (theta/rho/pi/chi de un estado nulo siguen
siendo nulos, e iota XOR-ea con una constante que también es cero).

**Corrección**: se agregó el mismo parámetro `INIT_FILE` a `dmem.sv` que
ya tenía `imem.sv`, cargando el mismo archivo `.hex` en ambas memorias.
Comportamiento por defecto preservado explícitamente (`INIT_FILE=""`
dejando el array sin inicialización, igual que antes) — los 17 casos
existentes de `dmem` y el firmware `fib.s` de `core_top_pipelined` se
re-verificaron sin cambios tras el fix, confirmando que no hay
regresión.

**Comandos:**
```bash
cd sim
make dmem                 # 17/17, comportamiento por defecto preservado
make core_top_pipelined   # fib.s, sin regresion
make keccak_firmware      # firmware real de SHA3-256("abc") sobre el core
```

### Estado de Fase 5 (parcial)

Prerrequisitos resueltos: macros de ensamblador verificadas, Keccak/SHA3/
SHAKE validado en tres capas (incluyendo ejecución real en el core), y
un bug de arquitectura de memoria del testbench (no del diseño RTL en
sí) encontrado y corregido, con la corrección aplicada de forma
retrocompatible. Pendiente: CBD (muestreo de ruido), empaquetado/
compresión de polinomios, y el firmware completo de keygen/encapsulation/
decapsulation.

### CBD (Centered Binomial Distribution)

`sw/lib/cbd.c` — muestreo del ruido de Kyber (`η=3` para el ruido de
generación de clave en ML-KEM-512, `η=2` para el ruido de
encapsulación). Algoritmo idéntico a `kyber_py.PolynomialRing.cbd()`
(FIPS 203 Algorithm 7), agregado primero al modelo de referencia
(`models/kyber_ref.py`) y verificado contra `kyber-py` por comparación
directa de código fuente además de resultado — 40/40 casos (ambos
valores de `η`, 20 pruebas aleatorias cada uno).

Implementación en C, aplicando desde el principio la misma disciplina de
autocontención que costó un ciclo de debug completo en Keccak: sin
operador `%` (la corrección `(a-b) mod q` se resuelve con una sola suma
condicional, ya que `a-b` está acotado a `[-η,η]`), sin `memset`/
`memcpy`. Validado en las mismas tres capas que Keccak:

1. Nativo contra el modelo de referencia: 512/512 coeficientes correctos
   (256 para cada `η`).
2. Toolchain RV32I: compila limpio con `-march=rv32i -ffreestanding`,
   sin instrucciones `mul`/`div`.
3. Ejecución real en el core: firmware completo corriendo sobre
   `core_top_pipelined`, 256/256 coeficientes correctos — **sin
   necesitar ningún debug adicional**, gracias al fix de `dmem` ya
   aplicado y al cuidado de escribir el C de forma autocontenida desde
   el principio.

**Comandos:**
```bash
cd sim && make cbd_firmware
```
