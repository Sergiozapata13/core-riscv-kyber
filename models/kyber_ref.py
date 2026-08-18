"""
kyber_ref.py

Modelo de referencia en Python — Fase 3 (Core RISC-V + Kyber).

Implementa exactamente lo que especifica isa_vectorial_kyber.docx:
  - Reduccion modular Barrett (seccion 5)
  - NTT forward via Cooley-Tukey, INTT via Gentleman-Sande (seccion 6)
  - Multiplicacion punto a punto en dominio NTT (para vpmul)
  - Suma/resta de polinomios (para vadd/vsub)

Este modulo es el ORACULO contra el que se valida:
  - El RTL de la unidad vectorial (Fase 4), instruccion por instruccion.
  - El firmware de Kyber end-to-end (Fase 5).

Cualquier discrepancia entre el RTL y este modelo es, por definicion, un
bug del RTL — este modulo es la fuente de verdad de la semantica.
"""

from gen_zetas import gen_zetas

Q = 3329
N = 256

ZETAS = gen_zetas()  # 128 twiddle factors, en orden bit-reversed (ver gen_zetas.py)

# Barrett reduction — constantes, ver isa_vectorial_kyber.docx seccion 5.1
BARRETT_SHIFT = 26
BARRETT_R = 1 << BARRETT_SHIFT          # 2^26 = 67108864
BARRETT_MULTIPLIER = (BARRETT_R + Q // 2) // Q  # ceil(2^26 / q) = 20159


def barrett_reduce(a: int) -> int:
    """
    Reduccion modular Barrett — ver isa_vectorial_kyber.docx seccion 5.2.
    Entrada: entero con signo (tipicamente resultado de multiplicar dos
    coeficientes de 16 bits, hasta 32 bits).
    Salida: a mod q, en el rango [0, q).

    Nota: la formula documentada (t = a*M + R/2; quotient = t>>shift;
    remainder = a - quotient*q) puede dar un remainder en {-q, ..., q-1}
    en vez de exactamente [0, q) por el redondeo — se normaliza al final
    con una correccion aritmetica (no condicional de control, ver
    seccion 4.2), consistente con el requisito constant-time.
    """
    t = a * BARRETT_MULTIPLIER + (BARRETT_R // 2)
    quotient = t >> BARRETT_SHIFT
    remainder = a - quotient * Q

    # Normalizacion aritmetica a [0, q) — no es un branch de control real,
    # es la misma "seleccion aritmetica" de la seccion 4.2 (en software
    # puro Python no hay una nocion de timing de branch que proteger, pero
    # la FORMA de esta normalizacion es la que el RTL de la Fase 4 debe
    # implementar como mux/mascara, no como control-flow condicionado por
    # el dato).
    remainder = remainder % Q
    return remainder


def barrett_reduce_poly(poly: list[int]) -> list[int]:
    return [barrett_reduce(c) for c in poly]


def ntt(poly: list[int]) -> list[int]:
    """
    NTT forward, Cooley-Tukey — ver isa_vectorial_kyber.docx seccion 6.2.
    Entrada: polinomio de 256 coeficientes en orden NATURAL.
    Salida: 256 coeficientes en orden BIT-REVERSED (dominio NTT interno).
    7 niveles (NTT incompleto — no 8), 128 mariposas por nivel.
    """
    r = list(poly)
    k = 1
    length = 128
    while length >= 2:
        start = 0
        while start < N:
            zeta = ZETAS[k]
            k += 1
            for j in range(start, start + length):
                t = barrett_reduce(zeta * r[j + length])
                r[j + length] = barrett_reduce(r[j] - t)
                r[j] = barrett_reduce(r[j] + t)
            start += 2 * length
        length //= 2
    return r


def intt(poly: list[int]) -> list[int]:
    """
    INTT, Gentleman-Sande — ver isa_vectorial_kyber.docx seccion 6.3.
    Entrada: 256 coeficientes en orden BIT-REVERSED (dominio NTT).
    Salida: 256 coeficientes en orden NATURAL.
    Incluye la multiplicacion final por el inverso de 128 (n/2, dado el
    NTT incompleto) mod q, requerida para deshacer el escalado del NTT.
    """
    r = list(poly)
    k = 127
    length = 2
    while length <= 128:
        start = 0
        while start < N:
            zeta = ZETAS[k]
            k -= 1
            for j in range(start, start + length):
                t = r[j]
                r[j] = barrett_reduce(t + r[j + length])
                r[j + length] = barrett_reduce(r[j + length] - t)
                r[j + length] = barrett_reduce(zeta * r[j + length])
            start += 2 * length
        length *= 2

    # Factor de escalado: el NTT incompleto de Kyber requiere multiplicar
    # por el inverso modular de 128 al final del INTT (ver referencia
    # oficial pq-crystals/kyber, funcion invntt_tomont + reduce).
    f = pow(128, Q - 2, Q)  # inverso modular de 128 mod q (Fermat, q primo)
    r = [barrett_reduce(c * f) for c in r]
    return r


def poly_add(a: list[int], b: list[int]) -> list[int]:
    """vadd — ver isa_vectorial_kyber.docx, tabla de operaciones funct3=110."""
    return [barrett_reduce(x + y) for x, y in zip(a, b)]


def poly_sub(a: list[int], b: list[int]) -> list[int]:
    """vsub — funct3=111."""
    return [barrett_reduce(x - y) for x, y in zip(a, b)]


def _base_case_multiply(a0, a1, b0, b1, zeta):
    """
    Multiplicacion de dos polinomios de grado 1 (a0 + a1*X) * (b0 + b1*X)
    mod (X^2 - zeta), usada para cada uno de los 128 pares del dominio NTT
    (ver isa_vectorial_kyber.docx seccion 6, nota sobre NTT incompleto:
    el resultado del NTT son 128 pares, no 256 escalares independientes).
    """
    c0 = barrett_reduce(a0 * b0 + barrett_reduce(a1 * b1) * zeta)
    c1 = barrett_reduce(a0 * b1 + a1 * b0)
    return c0, c1


def poly_pointwise_mul(a: list[int], b: list[int]) -> list[int]:
    """
    vpmul — multiplicacion punto a punto en dominio NTT (funct3=100).
    Opera sobre 128 pares (no 256 escalares), consistente con el NTT
    incompleto de Kyber (ver seccion 6 del documento de especificacion).
    """
    r = [0] * N
    for i in range(64):
        # Cada "doble par" (indices 4i..4i+3) usa zeta y -zeta como en la
        # referencia oficial de Kyber (basemul con signos alternados).
        zeta = ZETAS[64 + i]

        c0, c1 = _base_case_multiply(a[4 * i + 0], a[4 * i + 1], b[4 * i + 0], b[4 * i + 1], zeta)
        r[4 * i + 0], r[4 * i + 1] = c0, c1

        c0, c1 = _base_case_multiply(a[4 * i + 2], a[4 * i + 3], b[4 * i + 2], b[4 * i + 3], Q - zeta)
        r[4 * i + 2], r[4 * i + 3] = c0, c1
    return r


def _bit_count(x: int) -> int:
    return bin(x).count("1")


def cbd(input_bytes: bytes, eta: int) -> list[int]:
    """
    Muestreo de la distribucion binomial centrada (CBD) — Fase 5.

    Algoritmo identico a kyber_py.polynomials.PolynomialRing.cbd()
    (verificado por comparacion directa de codigo fuente, no solo por
    resultado — ver models/test_kyber_ref.py), que a su vez implementa
    el Algorithm 7 (SamplePolyCBD) de FIPS 203 / Algorithm 2 de la
    especificacion Kyber round 3.

    Entrada: 64*eta bytes de un stream uniformemente aleatorio (en la
    practica, la salida de PRF/SHAKE256 con la semilla de ruido).
    Para cada uno de los 256 coeficientes, se toman 2*eta bits: los
    primeros eta bits cuentan como 'a' (suma de bits en 1), los
    siguientes eta bits cuentan como 'b'; el coeficiente es (a-b) mod q.

    eta=3 para ML-KEM-512 (ruido de generacion de clave, eta1);
    eta=2 para el ruido de encapsulacion (eta2).
    """
    assert 64 * eta == len(input_bytes), f"se esperaban {64*eta} bytes, se recibieron {len(input_bytes)}"
    coefficients = [0] * N
    b_int = int.from_bytes(input_bytes, "little")
    mask = (1 << eta) - 1
    mask2 = (1 << (2 * eta)) - 1
    for i in range(N):
        x = b_int & mask2
        a = _bit_count(x & mask)
        b = _bit_count((x >> eta) & mask)
        b_int >>= 2 * eta
        coefficients[i] = (a - b) % Q
    return coefficients


def byte_encode(coeffs: list[int], d: int) -> bytes:
    """
    ByteEncode (Algorithm 4 de FIPS 203, "Encode" en kyber-py) — Fase 5.

    Empaqueta 256 coeficientes de d bits cada uno (sin compresion, sin
    perdida) en un stream de 32*d bytes, little-endian. d=12 para
    codificar coeficientes completos mod q (q=3329 < 2^12).

    Algoritmo identico a kyber_py.Polynomial.encode() (verificado por
    comparacion directa de codigo fuente).
    """
    t = 0
    for i in range(255):
        t |= coeffs[256 - i - 1]
        t <<= d
    t |= coeffs[0]
    return t.to_bytes(32 * d, "little")


def byte_decode(input_bytes: bytes, d: int) -> list[int]:
    """
    ByteDecode (Algorithm 3 de FIPS 203) — Fase 5.

    Inverso de byte_encode. Si d=12, reduce mod q (3329); para otros
    valores de d (compresion), reduce mod 2^d — ver seccion 4.2.1 de
    FIPS 203, donde ByteDecode siempre aplica una reduccion final.

    Algoritmo identico a kyber_py.PolynomialRing.decode() (verificado
    por comparacion directa de codigo fuente).
    """
    assert 256 * d == len(input_bytes) * 8, \
        f"se esperaban {256*d//8} bytes para d={d}, se recibieron {len(input_bytes)}"
    m = Q if d == 12 else (1 << d)
    coeffs = [0] * N
    b_int = int.from_bytes(input_bytes, "little")
    mask = (1 << d) - 1
    for i in range(N):
        coeffs[i] = (b_int & mask) % m
        b_int >>= d
    return coeffs


def compress_coeff(x: int, d: int) -> int:
    """
    Compress_d(x) = round((2^d/q)*x) mod 2^d — FIPS 203 seccion 4.2.1.
    Compresion CON PERDIDA de un solo coeficiente. Algoritmo identico a
    kyber_py.Polynomial._compress_ele().
    """
    t = 1 << d
    y = (t * x + Q // 2) // Q
    return y % t


def decompress_coeff(x: int, d: int) -> int:
    """
    Decompress_d(x) = round((q/2^d)*x) — FIPS 203 seccion 4.2.1.
    Inverso aproximado de compress_coeff (con perdida, x' != x original
    pero cercano en magnitud). Algoritmo identico a
    kyber_py.Polynomial._decompress_ele().
    """
    t = 1 << (d - 1)
    return (Q * x + t) >> d


def poly_compress(coeffs: list[int], d: int) -> list[int]:
    return [compress_coeff(c, d) for c in coeffs]


def poly_decompress(coeffs: list[int], d: int) -> list[int]:
    return [decompress_coeff(c, d) for c in coeffs]
