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


def sample_ntt(input_bytes: bytes) -> list[int]:
    """
    SampleNTT / Parse (Algorithm 1 de FIPS 203, "Algorithm 6" de la
    especificacion Kyber round 3) — Fase 5.

    Muestreo por rechazo (rejection sampling): consume 3 bytes por
    iteracion, produce hasta 2 candidatos de 12 bits cada uno, rechaza
    los que caen en [q, 4096) — sesgo cero garantizado, a diferencia de
    tomar directamente 12 bits mod q (que introduciria sesgo, ya que
    4096 no es multiplo de 3329).

    Algoritmo identico a kyber_py.PolynomialRing.ntt_sample() (verificado
    por comparacion directa de codigo fuente). 'input_bytes' debe venir
    de un XOF (SHAKE128) con suficientes bytes — en la practica, un
    buffer generoso (ver sw/lib/sample_ntt.c para el tamaño exacto
    usado en la implementacion C) alcanza con probabilidad
    overwhelmingly alta, dado que P(rechazo por candidato) ~= 18.7%.
    """
    i, j = 0, 0
    coefficients = [0] * N
    while j < N:
        d1 = input_bytes[i] + 256 * (input_bytes[i + 1] % 16)
        d2 = (input_bytes[i + 1] // 16) + 16 * input_bytes[i + 2]

        if d1 < Q:
            coefficients[j] = d1
            j += 1

        if d2 < Q and j < N:
            coefficients[j] = d2
            j += 1

        i += 3

        if i + 3 > len(input_bytes):
            raise ValueError(
                f"sample_ntt: se agotaron los {len(input_bytes)} bytes de entrada "
                f"antes de completar los 256 coeficientes (j={j}) — aumentar el buffer"
            )
    return coefficients


# =====================================================================
# ML-KEM-512 (FIPS 203) — protocolo completo — Fase 5
#
# Orquesta las primitivas ya validadas (ntt, intt, poly_pointwise_mul,
# poly_add, poly_sub, cbd, byte_encode, byte_decode, poly_compress,
# poly_decompress, sample_ntt, barrett_reduce) siguiendo exactamente
# el algoritmo de kyber_py.ml_kem.ml_kem.ML_KEM — codigo fuente
# revisado directamente (no de memoria) para capturar los detalles
# exactos: orden de indices en la matriz A (A[i][j] usa xof(rho,j,i),
# NO xof(rho,i,j)), la transposicion en encrypt (A^T[i][j] = A[j][i],
# sin regenerar nada — solo se invierte el indice de acceso), y el
# byte de separacion de dominio d+bytes([k]) en keygen.
#
# Parametros ML-KEM-512: k=2, eta1=3, eta2=2, du=10, dv=4.
# =====================================================================

import hashlib

MLKEM_K = 2
MLKEM_ETA1 = 3
MLKEM_ETA2 = 2
MLKEM_DU = 10
MLKEM_DV = 4


def _G(s: bytes) -> tuple[bytes, bytes]:
    """G = SHA3-512, dividido en dos mitades de 32 bytes (FIPS 203 4.5)."""
    h = hashlib.sha3_512(s).digest()
    return h[:32], h[32:]


def _H(s: bytes) -> bytes:
    """H = SHA3-256 (FIPS 203 4.4)."""
    return hashlib.sha3_256(s).digest()


def _J(s: bytes) -> bytes:
    """J = SHAKE256, 32 bytes de salida (FIPS 203 4.4)."""
    return hashlib.shake_256(s).digest(32)


def _prf(eta: int, s: bytes, b: bytes) -> bytes:
    """PRF = SHAKE256(s||b, 64*eta) (FIPS 203 4.3). s: 32 bytes, b: 1 byte."""
    assert len(s) == 32 and len(b) == 1
    return hashlib.shake_256(s + b).digest(64 * eta)


def _xof(rho: bytes, i: bytes, j: bytes) -> bytes:
    """XOF = SHAKE128(rho||i||j, 840) (FIPS 203 4.9). rho: 32 bytes, i,j: 1 byte c/u."""
    assert len(rho) == 32 and len(i) == 1 and len(j) == 1
    return hashlib.shake_128(rho + i + j).digest(840)


def _generate_matrix(rho: bytes, k: int = MLKEM_K) -> list[list[list[int]]]:
    """
    A[i][j] = sample_ntt(xof(rho, j, i)) — OJO: el orden es (j,i), no
    (i,j), confirmado contra el codigo fuente de kyber-py
    (_generate_matrix_from_seed). Se genera UNA sola vez; la
    "transposicion" en encrypt no regenera nada, solo invierte el
    indice de acceso al usarla (ver k_pke_encrypt).
    """
    A = [[None] * k for _ in range(k)]
    for i in range(k):
        for j in range(k):
            xof_bytes = _xof(rho, bytes([j]), bytes([i]))
            A[i][j] = sample_ntt(xof_bytes)
    return A


def _generate_error_vector(sigma: bytes, eta: int, N: int, k: int = MLKEM_K):
    """Genera k polinomios CBD(eta) desde PRF(sigma, N), PRF(sigma,N+1), ..."""
    elements = []
    for _ in range(k):
        prf_out = _prf(eta, sigma, bytes([N]))
        elements.append(cbd(prf_out, eta))
        N += 1
    return elements, N


def k_pke_keygen(d: bytes, k: int = MLKEM_K) -> tuple[bytes, bytes]:
    """K-PKE.KeyGen — FIPS 203 Algorithm 13."""
    rho, sigma = _G(d + bytes([k]))  # separacion de dominio por k

    A = _generate_matrix(rho, k)

    N = 0
    s, N = _generate_error_vector(sigma, MLKEM_ETA1, N, k)
    e, N = _generate_error_vector(sigma, MLKEM_ETA1, N, k)

    s_hat = [ntt(s[i]) for i in range(k)]
    e_hat = [ntt(e[i]) for i in range(k)]

    # t_hat[i] = sum_j( A[i][j] * s_hat[j] ) + e_hat[i]   (indice normal, NO transpuesto)
    t_hat = []
    for i in range(k):
        acc = [0] * N_COEFFS
        for j in range(k):
            prod = poly_pointwise_mul(A[i][j], s_hat[j])
            acc = poly_add(acc, prod)
        t_hat.append(poly_add(acc, e_hat[i]))

    ek_pke = b"".join(byte_encode(t_hat[i], 12) for i in range(k)) + rho
    dk_pke = b"".join(byte_encode(s_hat[i], 12) for i in range(k))
    return ek_pke, dk_pke


def k_pke_encrypt(ek_pke: bytes, m: bytes, r: bytes, k: int = MLKEM_K) -> bytes:
    """K-PKE.Encrypt — FIPS 203 Algorithm 14."""
    t_hat_bytes = ek_pke[: 384 * k]
    rho = ek_pke[384 * k :]
    t_hat = [byte_decode(t_hat_bytes[384 * i : 384 * (i + 1)], 12) for i in range(k)]

    A = _generate_matrix(rho, k)

    N = 0
    y, N = _generate_error_vector(r, MLKEM_ETA1, N, k)
    e1, N = _generate_error_vector(r, MLKEM_ETA2, N, k)
    prf_out = _prf(MLKEM_ETA2, r, bytes([N]))
    e2 = cbd(prf_out, MLKEM_ETA2)

    y_hat = [ntt(y[i]) for i in range(k)]

    # u[i] = intt( sum_j( A[j][i] * y_hat[j] ) ) + e1[i]   (indice INVERTIDO: A^T)
    u = []
    for i in range(k):
        acc = [0] * N_COEFFS
        for j in range(k):
            prod = poly_pointwise_mul(A[j][i], y_hat[j])  # A[j][i], no A[i][j]
            acc = poly_add(acc, prod)
        u.append(poly_add(intt(acc), e1[i]))

    mu = poly_decompress(byte_decode(m, 1), 1)

    # v = intt( sum_i( t_hat[i] * y_hat[i] ) ) + e2 + mu   (producto interno)
    acc_v = [0] * N_COEFFS
    for i in range(k):
        prod = poly_pointwise_mul(t_hat[i], y_hat[i])
        acc_v = poly_add(acc_v, prod)
    v = poly_add(poly_add(intt(acc_v), e2), mu)

    c1 = b"".join(byte_encode(poly_compress(u[i], MLKEM_DU), MLKEM_DU) for i in range(k))
    c2 = byte_encode(poly_compress(v, MLKEM_DV), MLKEM_DV)
    return c1 + c2


def k_pke_decrypt(dk_pke: bytes, c: bytes, k: int = MLKEM_K) -> bytes:
    """K-PKE.Decrypt — FIPS 203 Algorithm 15."""
    n = k * MLKEM_DU * 32
    c1, c2 = c[:n], c[n:]

    du_bytes_per_poly = 32 * MLKEM_DU
    u = [
        poly_decompress(byte_decode(c1[du_bytes_per_poly * i : du_bytes_per_poly * (i + 1)], MLKEM_DU), MLKEM_DU)
        for i in range(k)
    ]
    v = poly_decompress(byte_decode(c2, MLKEM_DV), MLKEM_DV)
    s_hat = [byte_decode(dk_pke[384 * i : 384 * (i + 1)], 12) for i in range(k)]

    u_hat = [ntt(u[i]) for i in range(k)]

    # w = v - intt( sum_i( s_hat[i] * u_hat[i] ) )
    acc = [0] * N_COEFFS
    for i in range(k):
        prod = poly_pointwise_mul(s_hat[i], u_hat[i])
        acc = poly_add(acc, prod)
    w = poly_sub(v, intt(acc))

    m = byte_encode(poly_compress(w, 1), 1)
    return m


def ml_kem_keygen(d: bytes, z: bytes, k: int = MLKEM_K) -> tuple[bytes, bytes]:
    """ML-KEM.KeyGen — FIPS 203 Algorithm 16/19."""
    ek_pke, dk_pke = k_pke_keygen(d, k)
    ek = ek_pke
    dk = dk_pke + ek + _H(ek) + z
    return ek, dk


def ml_kem_encaps(ek: bytes, m: bytes, k: int = MLKEM_K) -> tuple[bytes, bytes]:
    """ML-KEM.Encaps — FIPS 203 Algorithm 17/20."""
    K_shared, r = _G(m + _H(ek))
    c = k_pke_encrypt(ek, m, r, k)
    return K_shared, c


def ml_kem_decaps(dk: bytes, c: bytes, k: int = MLKEM_K) -> bytes:
    """ML-KEM.Decaps — FIPS 203 Algorithm 18/21."""
    dk_pke = dk[: 384 * k]
    ek_pke = dk[384 * k : 768 * k + 32]
    h = dk[768 * k + 32 : 768 * k + 64]
    z = dk[768 * k + 64 :]

    m_prime = k_pke_decrypt(dk_pke, c, k)
    K_prime, r_prime = _G(m_prime + h)
    K_bar = _J(z + c)
    c_prime = k_pke_encrypt(ek_pke, m_prime, r_prime, k)

    # Rechazo implicito: si el ciphertext re-encriptado no coincide,
    # devolver K_bar (basura pseudoaleatoria) en vez de K_prime. En
    # produccion esto DEBE ser constant-time; aca (modelo de
    # referencia en Python, no el firmware final) se usa un simple if
    # por claridad — el firmware C debera implementarlo sin
    # bifurcacion si se busca constant-time real en la Fase 5.
    if c == c_prime:
        return K_prime
    else:
        return K_bar


N_COEFFS = N  # alias local para las funciones de arriba (mismo valor que N=256)
