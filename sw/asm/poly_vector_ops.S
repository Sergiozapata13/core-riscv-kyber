# poly_vector_ops.S
#
# Version ACELERADA (vectorial) de poly_ntt/poly_intt/
# poly_pointwise_mul/poly_add/poly_sub — Fase 5, medicion de speedup.
#
# Implementa EXACTAMENTE las mismas firmas que sw/lib/poly_ntt.c (mismo
# nombre de funcion, misma convencion de llamada C estandar: argumentos
# en a0/a1/a2 segun la ABI de RISC-V), usando las 8 instrucciones
# vectoriales de la Fase 4 (vload/vstore/vntt/vintt/vpmul/vadd/vsub) en
# vez de las rutinas escalares en C — asi k_pke_keygen.c/encrypt.c/
# decrypt.c NO necesitan ningun cambio: alcanza con linkear este
# archivo en vez de poly_ntt.c para obtener el firmware acelerado.
#
# Sincronizacion: el hardware (scoreboard + stall automatico, ver
# Fase 4, rtl/vector/vector_control.sv) resuelve las dependencias entre
# instrucciones vectoriales consecutivas sin intervencion de software
# — no hace falta ningun polling ni sincronizacion manual entre
# vload/vntt/vstore.
#
# Direccion de memoria: vload/vstore toman la direccion DIRECTA de un
# registro escalar (sin offset, ver isa_vectorial_kyber.docx seccion
# 2.3) — los punteros a0/a1/a2 que llegan desde C (direccion de un
# array int16_t[256] en el stack del llamador) se usan tal cual.

# ---------------------------------------------------------------------
# NOTA DE DISEÑO CRITICA — sincronizacion memoria-escalar tras vstore:
#
# El scoreboard de la Fase 4 (rtl/vector/vector_scoreboard.sv) protege
# unicamente los REGISTROS vectoriales (v0-v3) — NO protege la MEMORIA
# que vload/vstore leen/escriben. El desacople (Apendice A.1) funciona
# exactamente como fue diseñado: una vez que una instruccion vectorial
# DESPACHA (no cuando TERMINA), el pipeline escalar sigue avanzando de
# inmediato. Esto significa que, sin ninguna precaucion adicional, un
# 'ret' inmediatamente despues de un 'vstore' permite que el LLAMADOR
# lea la memoria de destino CON INSTRUCCIONES ESCALARES (lw/lh) MIENTRAS
# el vstore SIGUE ESCRIBIENDO en segundo plano (hasta 256 ciclos) — una
# race condition real, confirmada empiricamente con trazas de senales
# internas del pipeline (ver docs/entorno.md, Fase 5, para el
# diagnostico completo).
#
# Solucion: cada funcion que termina con un 'vstore' hacia memoria que
# el LLAMADOR va a leer con instrucciones escalares agrega un 'vload'
# de sincronizacion (hacia un registro vectorial sin uso posterior)
# INMEDIATAMENTE DESPUES del vstore, ANTES del 'ret'. Como la unidad
# vectorial es un recurso unico (misma decision A.4), ese vload NO
# puede despachar hasta que el vstore anterior TERMINE por completo —
# y como el pipeline escalar hace stall mientras una instruccion
# vectorial no logra despachar, el 'ret' (y por lo tanto cualquier
# lectura escalar posterior del llamador) queda bloqueado hasta que la
# escritura este 100% completa. No requiere tocar el RTL ya cerrado de
# la Fase 4 — reusa el mecanismo de recurso unico ya existente.
# ---------------------------------------------------------------------

.include "vector_macros.S"

.section .text

# ---------------------------------------------------------------------
# void poly_ntt(int16_t r[256])   -- in-place, a0 = &r[0]
# ---------------------------------------------------------------------
.global poly_ntt
poly_ntt:
    vload  v0, a0
    vntt   v1, v0
    vstore v1, a0
    vload  v3, a0    # sincronizacion: forzar espera a que el vstore anterior termine
    ret

# ---------------------------------------------------------------------
# void poly_intt(int16_t r[256])  -- in-place, a0 = &r[0]
# ---------------------------------------------------------------------
.global poly_intt
poly_intt:
    vload  v0, a0
    vintt  v1, v0
    vstore v1, a0
    vload  v3, a0    # sincronizacion
    ret

# ---------------------------------------------------------------------
# void poly_pointwise_mul(const int16_t a[256], const int16_t b[256],
#                          int16_t out[256])
# a0 = &a[0], a1 = &b[0], a2 = &out[0]
# ---------------------------------------------------------------------
.global poly_pointwise_mul
poly_pointwise_mul:
    vload  v0, a0
    vload  v1, a1
    vpmul  v2, v0, v1
    vstore v2, a2
    vload  v3, a2    # sincronizacion
    ret

# ---------------------------------------------------------------------
# void poly_add(const int16_t a[256], const int16_t b[256],
#               int16_t out[256])
# a0 = &a[0], a1 = &b[0], a2 = &out[0]
# ---------------------------------------------------------------------
.global poly_add
poly_add:
    vload  v0, a0
    vload  v1, a1
    vadd   v2, v0, v1
    vstore v2, a2
    vload  v3, a2    # sincronizacion
    ret

# ---------------------------------------------------------------------
# void poly_sub(const int16_t a[256], const int16_t b[256],
#               int16_t out[256])
# a0 = &a[0], a1 = &b[0], a2 = &out[0]
# ---------------------------------------------------------------------
.global poly_sub
poly_sub:
    vload  v0, a0
    vload  v1, a1
    vsub   v2, v0, v1
    vstore v2, a2
    vload  v3, a2    # sincronizacion
    ret
