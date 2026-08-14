"""
vector_isa_sim.py

Simulacion "a mano" de cada instruccion custom contra el modelo de
referencia — criterio de verificacion de cierre de la Fase 3.

Modela el banco de registros vectorial (4 registros v0-v3, ver Apendice A.2
del cronograma) y una memoria de datos simple, y ejecuta cada una de las 8
instrucciones definidas en isa_vectorial_kyber.docx exactamente segun su
semantica documentada. El resultado de cada instruccion se compara contra
kyber_ref.py — si alguna vez difieren, es una senal de que la especificacion
de ISA (el documento) y el modelo de referencia (el oraculo matematico) se
desviaron entre si, y hay que resolver la discrepancia ANTES de tocar RTL
en la Fase 4.

Este simulador es deliberadamente simple (arrays de Python, no ciclos de
reloj) — su unico proposito es fijar la semantica funcional de cada
instruccion, no modelar temporizacion (eso es trabajo de la Fase 4).
"""

from kyber_ref import (
    Q,
    N,
    ntt,
    intt,
    barrett_reduce_poly,
    poly_add,
    poly_sub,
    poly_pointwise_mul,
)


class VectorUnit:
    """
    Modelo funcional del banco de registros vectorial (4 registros,
    v0-v3) y una memoria de datos simple, ejecutando las 8 instrucciones
    de la ISA vectorial exactamente segun su semantica documentada.
    """

    def __init__(self):
        self.vregs = [[0] * N for _ in range(4)]  # v0..v3, cada uno 256 coef
        self.mem = {}  # direccion (int) -> polinomio (list[int])

    # -------------------------------------------------------------
    # Variante MEMORIA
    # -------------------------------------------------------------
    def vload(self, vreg_dst: int, addr: int):
        """funct3=000. Carga un polinomio desde memoria al registro vectorial."""
        assert addr in self.mem, f"vload: direccion 0x{addr:x} sin inicializar"
        self.vregs[vreg_dst] = list(self.mem[addr])

    def vstore(self, vreg_src: int, addr: int):
        """funct3=001. Guarda el registro vectorial a memoria."""
        self.mem[addr] = list(self.vregs[vreg_src])

    # -------------------------------------------------------------
    # Variante CÓMPUTO
    # -------------------------------------------------------------
    def vntt(self, vreg_dst: int, vreg_src: int):
        """funct3=010. NTT forward (Cooley-Tukey)."""
        self.vregs[vreg_dst] = ntt(self.vregs[vreg_src])

    def vintt(self, vreg_dst: int, vreg_src: int):
        """funct3=011. NTT inverso (Gentleman-Sande)."""
        self.vregs[vreg_dst] = intt(self.vregs[vreg_src])

    def vpmul(self, vreg_dst: int, vreg_a: int, vreg_b: int):
        """funct3=100. Multiplicacion punto a punto en dominio NTT."""
        self.vregs[vreg_dst] = poly_pointwise_mul(self.vregs[vreg_a], self.vregs[vreg_b])

    def vbarrett(self, vreg_dst: int, vreg_src: int):
        """funct3=101. Reduccion modular Barrett de todos los coeficientes."""
        self.vregs[vreg_dst] = barrett_reduce_poly(self.vregs[vreg_src])

    def vadd(self, vreg_dst: int, vreg_a: int, vreg_b: int):
        """funct3=110. Suma de polinomios, coeficiente a coeficiente, mod q."""
        self.vregs[vreg_dst] = poly_add(self.vregs[vreg_a], self.vregs[vreg_b])

    def vsub(self, vreg_dst: int, vreg_a: int, vreg_b: int):
        """funct3=111. Resta de polinomios, coeficiente a coeficiente, mod q."""
        self.vregs[vreg_dst] = poly_sub(self.vregs[vreg_a], self.vregs[vreg_b])
