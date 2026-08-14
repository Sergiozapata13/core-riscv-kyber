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
