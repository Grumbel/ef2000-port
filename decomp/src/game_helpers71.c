/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Scan code weights and saved-table readback. */

/* 0xcbd30, 451 bytes. Weight for a scan code (0 when unmapped); the original is a binary decision tree. */
uint8_t scan_code_weight_cbd30(uint8_t code)
{
    static const uint8_t ranges[][3] = {
        {0x1, 0x1, 95},
        {0x2, 0x4, 100},
        {0x6, 0x6, 100},
        {0x7, 0x7, 85},
        {0x8, 0x9, 90},
        {0xa, 0xa, 100},
        {0xc, 0xd, 95},
        {0xe, 0xe, 50},
        {0xf, 0xf, 40},
        {0x10, 0x10, 50},
        {0x11, 0x11, 100},
        {0x12, 0x12, 50},
        {0x14, 0x18, 100},
        {0x19, 0x1b, 95},
        {0x1c, 0x1c, 50},
        {0x1e, 0x1e, 40},
        {0x1f, 0x1f, 95},
        {0x20, 0x20, 40},
        {0x21, 0x21, 95},
        {0x22, 0x22, 50},
        {0x23, 0x25, 40},
        {0x27, 0x27, 95},
        {0x28, 0x28, 60},
        {0x29, 0x2a, 100},
        {0x2c, 0x2d, 40},
        {0x2e, 0x2e, 100},
        {0x2f, 0x2f, 85},
        {0x30, 0x30, 50},
        {0x32, 0x32, 90},
        {0x33, 0x33, 95},
        {0x34, 0x34, 90},
        {0x35, 0x35, 100},
        {0x36, 0x36, 95},
        {0x37, 0x37, 100},
        {0x3a, 0x41, 95},
        {0x42, 0x42, 85},
        {0x43, 0x43, 95},
        {0x4a, 0x4a, 40},
        {0x4b, 0x4d, 100},
        {0x4e, 0x4e, 60},
        {0x50, 0x51, 60},
        {0x53, 0x53, 50},
        {0x5a, 0x5b, 60},
        {0x5c, 0x64, 85},
        {0x65, 0x66, 100},
        {0x68, 0x68, 100},
    };
    uint32_t i;

    for (i = 0; i < sizeof(ranges) / sizeof(ranges[0]); i++) {
        if (code >= ranges[i][0] && code <= ranges[i][1])
            return ranges[i][2];
    }
    return 0;
}

/* 0xe34e8, 467 bytes. Read back the saved table blocks through the indirect readers; returns whether a record id is set. */
uint8_t read_saved_tables_e34e8(void)
{
    typedef void (*reader_t)(uint32_t addr, uint32_t size);
    typedef void (*reader_ex_t)(uint32_t flag, uint32_t addr, uint32_t size);
    reader_t rd = (reader_t)EF_U32(0x0042922cu);
    reader_ex_t rx = (reader_ex_t)EF_U32(0x00429220u);

    rd(0x0015f904u, 8);
    if (EF_U16(0x0015f904u) != 0)
        EF_U16(0x0015f904u) = 0;
    rd(0x001590a8u, 2);
    rd(0x001590acu, 2);
    rd(0x001590b0u, 2);
    rd(0x00146e50u, 2);
    rd(0x00146e56u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    rd(0x00147e56u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    rd(0x00148e58u, EF_U16(0x00146e50u));
    rx(0, 0x00149658u, EF_U16(0x00146e50u));
    rx(1, 0x00149e58u, EF_U16(0x00146e50u));
    rd(0x0014ae58u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    rx(0, 0x0014be58u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    rx(0, 0x0014ce58u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    rx(0, 0x0014de58u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    rx(0, 0x0014ee58u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    rd(0x00150e58u, (EF_U16(0x00146e50u) * 4u) & 0xffffu);
    rd(0x00152e58u, (EF_U16(0x00146e50u) * 4u) & 0xffffu);
    rd(0x00154e58u, (EF_U16(0x00146e50u) * 4u) & 0xffffu);
    EF_U16(0x0015f900u) = (uint16_t)(EF_U16(0x0015f900u) + 1);
    return EF_U32(0x0015f8f8u) != 0xffffffffu;
}
