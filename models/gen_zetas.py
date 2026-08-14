"""
gen_zetas.py

Genera la tabla de twiddle factors (zetas) para el NTT de Kyber, siguiendo
el algoritmo estandar de la especificacion (ver kyber-specification-round3.pdf,
seccion 1.1, y la implementacion de referencia pq-crystals/kyber).

zeta = 17 es la raiz primitiva 256-esima de la unidad mod q=3329 usada por
Kyber. La tabla se genera en el orden bit-reversed que consume el algoritmo
Cooley-Tukey (zetas[i] = zeta^(brv(i)) mod q, para i=1..127).

Se calcula programaticamente (no se copia de una tabla de terceros) para
poder verificar independientemente que la raiz primitiva es correcta antes
de confiarle nada al modelo de referencia.
"""

Q = 3329
ZETA = 17


def brv(x, bits):
    """Bit-reversal de x en un campo de 'bits' bits."""
    result = 0
    for i in range(bits):
        if x & (1 << i):
            result |= 1 << (bits - 1 - i)
    return result


def gen_zetas():
    # zetas[i] = ZETA^(brv_7(i)) mod q, para i = 0..127
    # brv_7: bit-reversal en 7 bits (ya que hay 128 = 2^7 posiciones)
    zetas = []
    for i in range(128):
        exp = brv(i, 7)
        zetas.append(pow(ZETA, exp, Q))
    return zetas


if __name__ == "__main__":
    zetas = gen_zetas()
    print(f"ZETA={ZETA}, Q={Q}")
    print(f"Verificacion: ZETA^256 mod Q = {pow(ZETA, 256, Q)} (debe ser 1)")
    print(f"Verificacion: ZETA^128 mod Q = {pow(ZETA, 128, Q)} (debe ser Q-1 = {Q-1})")
    print(f"len(zetas) = {len(zetas)}")
    print("Primeros 10 zetas:", zetas[:10])
