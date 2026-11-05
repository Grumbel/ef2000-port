/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

static uint16_t swap16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

/* Four short code entries, including a shared return branch at c5369, in the renderer's threaded
 * stream interpreter omitted from the Ghidra function inventory. Original
 * implicit ABI: EBX in/out byte cursor; AX contains decoded word on exit.
 * C represents the updated cursor explicitly. */

/* c5369, 9 bytes, shared special branch of c5372: write fixed 0x00bf. */
uint16_t renderer_write_special_word_c5369(ef_addr32 *cursor)
{
    EF_U16(*cursor) = 0x00bfu;
    *cursor += 2u;
    return 0x00bfu;
}

/* c5384, 12 bytes, generic BE16 to guest LE16 conversion. */
uint16_t renderer_swap_word_c5384(ef_addr32 *cursor)
{
    uint16_t value = swap16(EF_U16(*cursor));
    EF_U16(*cursor) = value;
    *cursor += 2u;
    return value;
}

/* c53b4, 27 bytes: relative index biased by 2000 minus origin, multiplied
 * by the 48-byte vertex-record stride (wrap to 16 bits). */
uint16_t renderer_index_biased_c53b4(ef_addr32 *cursor)
{
    uint16_t value = swap16(EF_U16(*cursor));
    value = (uint16_t)((uint32_t)(uint16_t)(value + 2000u - EF_U16(0x15af90u)) * 48u);
    EF_U16(*cursor) = value;
    *cursor += 2u;
    return value;
}

/* c53cf, 16 bytes: absolute index, 48-byte vertex-record stride. */
uint16_t renderer_index_absolute_c53cf(ef_addr32 *cursor)
{
    uint16_t value = swap16(EF_U16(*cursor));
    value = (uint16_t)((uint32_t)value * 48u);
    EF_U16(*cursor) = value;
    *cursor += 2u;
    return value;
}
