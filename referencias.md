# Referencias — Aceleración de Kyber/ML-KEM sobre RISC-V

Papers relevantes para el diseño de la extensión ISA vectorial custom (Fase 3) y la unidad de reducción modular/NTT (Fase 4). Priorizados por relevancia directa a "instrucciones custom RISC-V para NTT/Kyber".

## Más directamente relevantes (ISA extensions para NTT/Kyber)

1. **Miteloudi et al.** — "PQ.V.ALU.E: Post-quantum RISC-V Custom ALU Extensions on Dilithium and Kyber" — CARDIS 2023.
   Extensiones custom de ALU RISC-V evaluadas específicamente sobre Dilithium y Kyber. Referencia directa para el diseño de tu formato de instrucción tipo R.

2. **Fritzmann et al.** — "RISQ-V: Tightly Coupled RISC-V Accelerators for Post-Quantum Cryptography" — IACR TCHES 2020.
   Acopla aceleradores PQC directamente al pipeline (no como coprocesador externo vía bus) — es el patrón arquitectónico más cercano a tu decisión de integración.

3. **Gewehr, Luza, Moraes** — "Hardware Acceleration of Crystals-Kyber in Low-Complexity Embedded Systems With RISC-V Instruction Set Extensions" — IEEE Access, 2024.
   https://doi.org/10.1109/access.2024.3416812
   Kyber completo acelerado con extensiones ISA RISC-V en sistemas embebidos de bajo costo — buen punto de comparación de recursos/complejidad.

4. **"A RISC-V Post Quantum Cryptography Instruction Set Extension for Number Theoretic Transform to speed-up CRYSTALS Algorithms"**
   Extensión de instrucciones específica para NTT sobre Kyber, implementada en FPGA Artix-7. Cubre muestreo polinomial, NTT y multiplicación punto a punto — mapea muy de cerca a tu Fase 3/4.

## Arquitectura vectorial (RVV) aplicada a NTT

5. **Rodrigues, T. B. et al.** — "Accelerating NTT with RISC-V Vector Extension for Fully Homomorphic Encryption" — IACR TCHES, 2025(4), 711-736.
   https://tches.iacr.org/index.php/TCHES/article/view/12426
   Usa la extensión vectorial estándar de RISC-V (RVV) para acelerar NTT — relevante para decidir si tu ISA custom debería inspirarse en la semántica de RVV en vez de partir de cero.

6. **"Enhancing RISC-V Vector Extension for Efficient Application of Post-Quantum Cryptography"**
   Introduce "Barrett multiplication" (combinación de Montgomery + Barrett) para multiplicación modular eficiente vectorizada — técnica concreta aplicable a tu unidad de reducción modular.

## Co-diseño HW/SW y resultados de referencia

7. **"A scalable SIMD RISC-V based processor with customized vector extensions for CRYSTALS-Kyber"** — DAC 2022 (ACM/IEEE Design Automation Conference).
   https://dl.acm.org/doi/10.1145/3489517.3530552
   Propone 12 extensiones vectoriales para multiplicación Kyber y 4 para operaciones de cuerpo finito. Reporta speedups de 141.7x (NTT), 168.7x (INTT) y 245.5x (CWM) vs. baseline — buena referencia de qué speedup es razonable esperar y cómo reportarlo (conecta con la métrica de tu Fase 5).

8. **"RISC-V-based Acceleration Strategies for Post-Quantum Cryptography"** — RISC-V Summit Europe 2025.
   https://riscv-europe.org/summit/2025/media/proceedings/2025-05-14-RISC-V-Summit-Europe-P3.1.02-SARNO-abstract.pdf
   Survey/panorama de estrategias de aceleración RISC-V para PQC — útil como mapa general del estado del arte para la introducción de tu documentación final (Fase 6).

9. **"Accelerating Post-Quantum Cryptography: A High-Efficiency NTT for ML-KEM on RISC-V"** — Electronics, 2026, 15, 100.
   https://www.researchgate.net/publication/399080727_Accelerating_Post-Quantum_Cryptography_A_High-Efficiency_NTT_for_ML-KEM_on_RISC-V
   SoC fabricado con acelerador NTT/INTT de doble butterfly unit, integrado vía interfaz RoCC (Rocket Custom Coprocessor) a un core RISC-V de 64 bits — primer chip físico reportado con este enfoque. Buen contraste con tu decisión de integración directa al pipeline en vez de RoCC.

## Notas

- Casi todos coinciden en que la **NTT/INTT y la reducción modular** son los dos bloques que más se benefician de aceleración — confirma que tu enfoque (Fase 3/4 centradas en esos dos bloques) apunta a lo correcto.
- Varios papers usan **RoCC** (interfaz de coprocesador de Rocket Chip) como mecanismo de integración — es la alternativa "intermedia" entre coprocesador externo (tu TFG) e instrucciones totalmente integradas al pipeline (tu decisión actual). Vale la pena tenerlo como punto de comparación al documentar por qué elegiste integración directa.
- El paper de Barrett multiplication (ítem 6) tiene una técnica concreta y citable para tu unidad de reducción modular — vale la pena leerlo con detalle antes de la Fase 3.

## Resultado de este proyecto, para contraste directo con el ítem 7

Este proyecto mide el speedup **end-to-end del protocolo ML-KEM-512
completo** (keygen+encaps+decaps, incluyendo Keccak/SHA3/SHAKE, que
domina el tiempo total): **~1.53×** — sustancialmente menor que los
141-245× que el ítem 7 reporta para la **NTT/INTT/CWM aisladas**. Ambos
números son correctos y no son comparables directamente: este proyecto
no aceleró Keccak (fuera del alcance de las 8 instrucciones
vectoriales, Fase 3), que consume la mayor parte de los ciclos del
protocolo completo. Ver `docs/entorno.md`, sección de speedup, para el
desglose completo.
