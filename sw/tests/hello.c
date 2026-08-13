/* hello.c
 *
 * Firmware minimo de validacion de toolchain (Fase 0).
 * No depende de ninguna libreria estandar (compilado con -nostdlib):
 * unicamente escribe un patron reconocible a una direccion fija de memoria,
 * para que un testbench futuro (Fase 1) pueda verificar por inspeccion de
 * memoria que el programa realmente corrio.
 */

#define STATUS_ADDR ((volatile unsigned int *)0x00001000)
#define STATUS_OK   0xCAFEF00D

int main(void) {
    *STATUS_ADDR = STATUS_OK;

    /* Suma trivial para confirmar que el compilador genera codigo real
     * de ALU (no solo un store), util como humo de que -march=rv32i
     * esta generando instrucciones base validas. */
    volatile int a = 21;
    volatile int b = 21;
    volatile int c = a + b;

    (void)c;

    return 0;
}
