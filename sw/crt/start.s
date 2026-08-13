# start.s
#
# Startup code minimo para firmware bare-metal RV32I (Fase 0).
# Inicializa el stack pointer, limpia .bss, y salta a main().
# Si main() retorna, entra en un loop infinito (no hay SO al que volver).

    .section .text.start
    .global _start

_start:
    # Stack pointer al tope de la RAM (definido por el linker script)
    la      sp, __stack_top

    # Limpiar .bss: __bss_start hasta __bss_end, palabra por palabra
    la      t0, __bss_start
    la      t1, __bss_end
bss_clear_loop:
    bge     t0, t1, bss_clear_done
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       bss_clear_loop
bss_clear_done:

    call    main

halt:
    # main() no debe retornar en un sistema bare-metal sin SO;
    # si lo hace, quedarse en un loop infinito de forma explicita.
    j       halt
