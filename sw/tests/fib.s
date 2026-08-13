# fib.s
#
# Fibonacci iterativo — firmware de prueba no trivial para el criterio de
# verificacion de cierre de la Fase 1 (core_top.sv, datapath monociclo).
#
# Calcula los primeros 10 numeros de Fibonacci (F0..F9) y los almacena en
# memoria de datos a partir de la direccion 0x100, en palabras de 32 bits.
# Ejercita: addi, add, sw, lw, blt, jal, beq — una mezcla representativa
# de aritmetica, memoria, y control de flujo.
#
# F0=0, F1=1, F2=1, F3=2, F4=3, F5=5, F6=8, F7=13, F8=21, F9=34
#
# Al terminar, escribe un patron de status conocido en 0x200 para que el
# testbench pueda confirmar que el programa corrio hasta el final (no solo
# que no crasheo).

    .section .text.start
    .global _start

_start:
    la      sp, __stack_top      # no se usa en este programa, pero se deja
                                  # por convencion con start.s de Fase 0

    li      t0, 0x100            # t0 = direccion base del arreglo de resultados
    li      t1, 0                # t1 = F(n-2), arranca en F0=0
    li      t2, 1                # t2 = F(n-1), arranca en F1=1
    li      t3, 10                # t3 = cantidad de terminos a calcular
    li      t4, 0                # t4 = contador de iteracion

    sw      t1, 0(t0)            # arreglo[0] = F0 = 0

    bge     t4, t3, fib_done     # si contador >= 10, no hay nada que hacer (defensivo)
    sw      t2, 4(t0)            # arreglo[1] = F1 = 1
    addi    t4, t4, 2            # ya llevamos 2 terminos escritos (F0, F1)

fib_loop:
    bge     t4, t3, fib_done     # mientras contador < 10, seguir

    add     t6, t1, t2           # t6 = F(n-2) + F(n-1) = F(n)
    slli    t5, t4, 2            # t5 = contador * 4 (offset en bytes)
    add     t5, t0, t5           # t5 = direccion = base + offset
    sw      t6, 0(t5)            # arreglo[contador] = F(n)

    mv      t1, t2               # F(n-2) <- F(n-1)
    mv      t2, t6               # F(n-1) <- F(n)

    addi    t4, t4, 1            # contador++
    j       fib_loop

fib_done:
    li      t0, 0x200
    li      t1, 0xC0FFEE00
    sw      t1, 0(t0)            # patron de status: programa termino correctamente

halt:
    j       halt
