/* link.ld
 *
 * Linker script minimo para firmware bare-metal sobre el core RV32I.
 * Memoria unificada simple (sin distincion Harvard a nivel de linker;
 * la separacion instr/datos, si aplica, se maneja en la memoria simulada
 * del testbench, no aqui).
 */

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : {
        KEEP(*(.text.start))   /* _start entra primero, siempre en la direccion 0 */
        *(.text*)
    } > RAM

    .rodata : {
        *(.rodata*)
    } > RAM

    .data : {
        *(.data*)
    } > RAM

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    . = ALIGN(16);
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
