// tb_pack_firmware.cpp
//
// Testbench de Verilator (Fase 5): corre test_pack_firmware.hex sobre
// core_top_pipelined.sv y verifica byte_encode_d12 y
// compress/decompress (d=10) contra los valores ya validados
// nativamente contra kyber_ref.py (que a su vez ya se valido contra
// kyber-py).

#include <memory>
#include <cstdio>
#include <cstdint>

#include "Vcore_top_pipelined.h"
#include "Vcore_top_pipelined___024root.h"
#include "verilated.h"

static const uint8_t expected_encoded[384] = {
    7, 64, 1, 33, 224, 2, 59, 128, 4, 85, 32, 6, 111, 192, 7, 137, 96, 9, 163, 0, 11, 189, 160, 12, 215, 64, 14, 241, 224, 15, 11, 129, 17, 37, 33, 19, 63, 193, 20, 89, 97, 22, 115, 1, 24, 141, 161, 25, 167, 65, 27, 193, 225, 28, 219, 129, 30, 245, 33, 32, 15, 194, 33, 41, 98, 35, 67, 2, 37, 93, 162, 38, 119, 66, 40, 145, 226, 41, 171, 130, 43, 197, 34, 45, 223, 194, 46, 249, 98, 48, 19, 3, 50, 45, 163, 51, 71, 67, 53, 97, 227, 54, 123, 131, 56, 149, 35, 58, 175, 195, 59, 201, 99, 61, 227, 3, 63, 253, 163, 64, 23, 68, 66, 49, 228, 67, 75, 132, 69, 101, 36, 71, 127, 196, 72, 153, 100, 74, 179, 4, 76, 205, 164, 77, 231, 68, 79, 1, 229, 80, 27, 133, 82, 53, 37, 84, 79, 197, 85, 105, 101, 87, 131, 5, 89, 157, 165, 90, 183, 69, 92, 209, 229, 93, 235, 133, 95, 5, 38, 97, 31, 198, 98, 57, 102, 100, 83, 6, 102, 109, 166, 103, 135, 70, 105, 161, 230, 106, 187, 134, 108, 213, 38, 110, 239, 198, 111, 9, 103, 113, 35, 7, 115, 61, 167, 116, 87, 71, 118, 113, 231, 119, 139, 135, 121, 165, 39, 123, 191, 199, 124, 217, 103, 126, 243, 7, 128, 13, 168, 129, 39, 72, 131, 65, 232, 132, 91, 136, 134, 117, 40, 136, 143, 200, 137, 169, 104, 139, 195, 8, 141, 221, 168, 142, 247, 72, 144, 17, 233, 145, 43, 137, 147, 69, 41, 149, 95, 201, 150, 121, 105, 152, 147, 9, 154, 173, 169, 155, 199, 73, 157, 225, 233, 158, 251, 137, 160, 21, 42, 162, 47, 202, 163, 73, 106, 165, 99, 10, 167, 125, 170, 168, 151, 74, 170, 177, 234, 171, 203, 138, 173, 229, 42, 175, 255, 202, 176, 25, 107, 178, 51, 11, 180, 77, 171, 181, 103, 75, 183, 129, 235, 184, 155, 139, 186, 181, 43, 188, 207, 203, 189, 233, 107, 191, 3, 12, 193, 29, 172, 194, 55, 76, 196, 81, 236, 197, 107, 140, 199, 133, 44, 201, 159, 204, 202, 185, 108, 204, 211, 12, 206, 237, 172, 207
};

static const uint16_t expected_compressed[256] = {
    2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50, 54, 58, 62, 66, 70, 74, 78, 82, 86, 90, 94, 98, 102, 106, 110, 114, 118, 122, 126, 130, 134, 138, 142, 146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190, 194, 198, 202, 206, 210, 214, 218, 222, 226, 230, 234, 238, 242, 246, 250, 254, 258, 262, 266, 270, 274, 278, 282, 286, 290, 294, 298, 302, 306, 310, 314, 318, 322, 326, 330, 334, 338, 342, 346, 350, 354, 358, 362, 366, 370, 374, 378, 382, 386, 390, 394, 398, 402, 406, 410, 414, 418, 422, 426, 430, 434, 438, 442, 446, 450, 454, 458, 462, 466, 470, 474, 478, 482, 486, 490, 494, 498, 502, 506, 510, 514, 518, 522, 526, 530, 534, 538, 542, 546, 550, 554, 558, 562, 566, 570, 574, 578, 582, 586, 590, 594, 598, 602, 606, 610, 614, 618, 622, 626, 630, 634, 638, 642, 646, 650, 654, 658, 662, 666, 670, 674, 678, 682, 686, 690, 694, 698, 702, 706, 710, 714, 718, 722, 726, 730, 734, 738, 742, 746, 750, 754, 758, 762, 766, 770, 774, 778, 782, 786, 790, 794, 798, 802, 806, 810, 814, 818, 822, 826, 830, 834, 838, 842, 846, 850, 854, 858, 862, 866, 870, 874, 878, 882, 886, 890, 894, 898, 902, 906, 910, 914, 918, 922, 926, 930, 934, 938, 942, 946, 950, 954, 958, 962, 966, 970, 974, 978, 982, 986, 990, 994, 998, 1002, 1006, 1010, 1014, 1018, 1022
};

static const uint16_t expected_decompressed[256] = {
    7, 20, 33, 46, 59, 72, 85, 98, 111, 124, 137, 150, 163, 176, 189, 202, 215, 228, 241, 254, 267, 280, 293, 306, 319, 332, 345, 358, 371, 384, 397, 410, 423, 436, 449, 462, 475, 488, 501, 514, 527, 540, 553, 566, 579, 592, 605, 618, 631, 644, 657, 670, 683, 696, 709, 722, 735, 748, 761, 774, 787, 800, 813, 826, 839, 852, 865, 878, 891, 904, 917, 930, 943, 956, 969, 982, 995, 1008, 1021, 1034, 1047, 1060, 1073, 1086, 1099, 1112, 1125, 1138, 1151, 1164, 1177, 1190, 1203, 1216, 1229, 1242, 1255, 1268, 1281, 1294, 1307, 1320, 1333, 1346, 1359, 1372, 1385, 1398, 1411, 1424, 1437, 1450, 1463, 1476, 1489, 1502, 1515, 1528, 1541, 1554, 1567, 1580, 1593, 1606, 1619, 1632, 1645, 1658, 1671, 1684, 1697, 1710, 1723, 1736, 1749, 1762, 1775, 1788, 1801, 1814, 1827, 1840, 1853, 1866, 1879, 1892, 1905, 1918, 1931, 1944, 1957, 1970, 1983, 1996, 2009, 2022, 2035, 2048, 2061, 2074, 2087, 2100, 2113, 2126, 2139, 2152, 2165, 2178, 2191, 2204, 2217, 2230, 2243, 2256, 2269, 2282, 2295, 2308, 2321, 2334, 2347, 2360, 2373, 2386, 2399, 2412, 2425, 2438, 2451, 2464, 2477, 2490, 2503, 2516, 2529, 2542, 2555, 2568, 2581, 2594, 2607, 2620, 2633, 2646, 2659, 2672, 2685, 2698, 2711, 2724, 2737, 2750, 2763, 2776, 2789, 2802, 2815, 2828, 2841, 2854, 2867, 2880, 2893, 2906, 2919, 2932, 2945, 2958, 2971, 2984, 2997, 3010, 3023, 3036, 3049, 3062, 3075, 3088, 3101, 3114, 3127, 3140, 3153, 3166, 3179, 3192, 3205, 3218, 3231, 3244, 3257, 3270, 3283, 3296, 3309, 3322
};

static Vcore_top_pipelined* top;
static int errors = 0;

static void tick() {
    top->clk = 0;
    top->eval();
    top->clk = 1;
    top->eval();
}

static uint32_t read_dmem_word(uint32_t byte_addr) {
    return top->rootp->core_top_pipelined__DOT__u_dmem__DOT__mem[byte_addr / 4];
}

static uint8_t read_dmem_byte(uint32_t byte_addr) {
    uint32_t word = read_dmem_word(byte_addr & ~0x3u);
    unsigned shift = (byte_addr & 0x3u) * 8;
    return (uint8_t)((word >> shift) & 0xFF);
}

static uint16_t read_dmem_halfword(uint32_t byte_addr) {
    uint32_t word = read_dmem_word(byte_addr & ~0x3u);
    return (byte_addr & 0x2u) ? (uint16_t)(word >> 16) : (uint16_t)(word & 0xFFFF);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    top = new Vcore_top_pipelined;

    top->rst_n = 0;
    top->clk = 0;
    tick();
    tick();
    top->rst_n = 1;

    const uint32_t ENCODED_ADDR = 0x1000;
    const uint32_t COMPRESSED_ADDR = 0x1200;
    const uint32_t DECOMPRESSED_ADDR = 0x1800;
    const uint32_t DONE_ADDR = 0x1E00;
    const uint32_t DONE_MAGIC = 0xC0FFEE00u;

    const int MAX_CYCLES = 100000;
    bool done = false;

    for (int i = 0; i < MAX_CYCLES; i++) {
        tick();
        if (read_dmem_word(DONE_ADDR) == DONE_MAGIC) {
            done = true;
            std::printf("Firmware termino en el ciclo %d\n", i);
            break;
        }
    }

    if (!done) {
        std::printf("FAIL [firmware_termina]: no se alcanzo el patron de status tras %d ciclos\n", MAX_CYCLES);
        errors++;
    } else {
        std::printf("OK   [firmware_termina]\n");
    }

    bool encoded_ok = true;
    for (int i = 0; i < 384; i++) {
        uint8_t got = read_dmem_byte(ENCODED_ADDR + i);
        if (got != expected_encoded[i]) {
            encoded_ok = false;
            std::printf("FAIL [encoded_%d]: got=%u esperado=%u\n", i, got, expected_encoded[i]);
            errors++;
        }
    }
    if (encoded_ok) std::printf("OK   [byte_encode_d12]: 384/384 bytes correctos\n");

    bool compressed_ok = true;
    for (int i = 0; i < 256; i++) {
        uint16_t got = read_dmem_halfword(COMPRESSED_ADDR + i * 2);
        if (got != expected_compressed[i]) {
            compressed_ok = false;
            std::printf("FAIL [compressed_%d]: got=%u esperado=%u\n", i, got, expected_compressed[i]);
            errors++;
        }
    }
    if (compressed_ok) std::printf("OK   [compress_d10]: 256/256 coeficientes correctos\n");

    bool decompressed_ok = true;
    for (int i = 0; i < 256; i++) {
        uint16_t got = read_dmem_halfword(DECOMPRESSED_ADDR + i * 2);
        if (got != expected_decompressed[i]) {
            decompressed_ok = false;
            std::printf("FAIL [decompressed_%d]: got=%u esperado=%u\n", i, got, expected_decompressed[i]);
            errors++;
        }
    }
    if (decompressed_ok) std::printf("OK   [decompress_d10]: 256/256 coeficientes correctos\n");

    delete top;

    if (errors == 0) {
        std::printf("\nPASS: pack.c corriendo en el core real produce el mismo resultado que la validacion nativa.\n");
        return 0;
    } else {
        std::printf("\nFAIL: %d error(es) detectado(s).\n", errors);
        return 1;
    }
}
