# Core RISC-V con Aceleración Vectorial para Kyber

Core RV32I de 5 etapas con instrucciones vectoriales integradas al pipeline para
acelerar Kyber/ML-KEM (NTT, reducción modular, constant-time). Continuación del
TFG (RVV-lite sobre PicoRV32 vía PCPI).

Ver `docs/cronograma_core_riscv_kyber.docx` para el plan de fases completo y las
decisiones arquitectónicas (Apéndice A).

## Estructura del repositorio

```
rtl/            RTL sintetizable (SystemVerilog/Verilog)
  core/         Datapath escalar RV32I (register file, ALU, control, hazard unit)
  vector/       Unidad vectorial (banco de registros v0-v3, reducción modular, NTT)
tb/             Testbenches de Verilator (C++/SystemVerilog)
sim/            Scripts de build/simulación (Makefiles), salidas de waveform (.vcd)
docs/           Documentación de referencia y generada (specs de ISA, diagramas, green card)
sw/             Firmware de prueba
  crt/          Startup code / linker scripts para bare-metal RV32I
  tests/        Programas .s / .c de prueba (riscv-tests dirigidos, Fibonacci, etc.)
```

## Entorno (Fase 0)

Herramientas instaladas y versión fijada — ver `docs/entorno.md` para el detalle
completo y los pasos de verificación.

| Herramienta | Versión |
|---|---|
| Verilator | 5.020 (Debian 5.020-1) |
| riscv64-unknown-elf-gcc | 13.2.0 |
| binutils (as/ld/objcopy) | 2.42 |
| GTKWave | 3.3.116 |

## Build rápido

```bash
cd sim
make hello_verilator   # corre el testbench trivial de Fase 0
make hello_riscv       # compila y verifica el .c mínimo de Fase 0
```
