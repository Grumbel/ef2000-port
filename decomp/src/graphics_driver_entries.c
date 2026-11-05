/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

static uint16_t byteswap16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

/* c4db4, 135 bytes: endian-normalize a BE scene-stream header, select one
 * of two index-converter callbacks and iterate command records. The command
 * parser c4e3b remains a guest dependency; it has its own threaded opcode
 * table and cannot be represented by just this header routine. */
void renderer_normalize_scene_c4db4(ef_addr32 scene)
{
    ef_addr32 cursor;
    uint16_t flags = byteswap16(EF_U16(scene + 0x20u));
    EF_U16(scene + 0x20u) = flags;
    if ((flags & 3u) != 0u) {
        renderer_scene_error_c4db4(0x158e92u);
        return;
    }
    EF_U32(0x15af8cu) = (flags & 4u) ? 0xc53b4u : 0xc53cfu;
    if ((EF_U32(scene + 0x30u) | EF_U32(scene + 0x34u)) == 0u)
        renderer_scene_zero_c4db4(scene + 0x30u);
    EF_U16(scene + 0x10u) = byteswap16(EF_U16(scene + 0x10u));
    EF_U16(scene) = byteswap16(EF_U16(scene));
    cursor = scene + 2u;
    for (;;) {
        uint16_t command = byteswap16(EF_U16(cursor));
        EF_U16(cursor) = command;
        renderer_scene_record_c4db4(cursor);
        if (command == 0x7fffu)
            break;
        cursor += 2u;
    }
}

/* c8ccc, 58 bytes: five callee-cleaned stack words in REVERSE parameter
 * order into packed viewport data. The native check returns carry to skip
 * the final renderer flush. */
void renderer_viewport_five_c8ccc(uint16_t a, uint16_t b, uint16_t c,
                                 uint16_t d, uint16_t e)
{
    EF_U16(0x4208e4u) = e;
    EF_U16(0x4208e6u) = d;
    EF_U16(0x4208e8u) = c;
    EF_U16(0x4208eau) = b;
    EF_U16(0x4208ecu) = a;
    if (!renderer_viewport_check_c8ccc(0x4208e6u))
        renderer_viewport_flush_c8ccc();
}

/* e7a34, 71 bytes: six words with a different ordering (the source really
 * reads +0x18 before +0x1c). ESI points two words into the packed block. */
void renderer_viewport_six_e7a34(uint16_t a, uint16_t b, uint16_t c,
                                uint16_t d, uint16_t e, uint16_t f)
{
    EF_U16(0x159088u) = e;
    EF_U16(0x15908au) = f;
    EF_U16(0x15908cu) = d;
    EF_U16(0x15908eu) = c;
    EF_U16(0x159090u) = b;
    EF_U16(0x159092u) = a;
    if (!renderer_viewport_check_e7a34(0x15908cu))
        renderer_viewport_flush_e7a34(0x15908cu);
}
