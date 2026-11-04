/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Engine node and table helpers, 0xcbc54..0xcd81c. */

/* 0xcbc54, 71 bytes. Look up each node's tile byte in a 40-wide grid from its position. */
void lookup_tiles_cbc54(ef_addr32 grid, ef_addr32 list)
{
    ef_addr32 node = EF_U32(list);

    while (node != 0) {
        int32_t row = (0x190 - (int32_t)EF_U32(node + 0x1au)) / 10;
        int32_t column = (int32_t)EF_U32(node + 0x16u) / 10;

        EF_U8(node + 9u) = EF_U8(grid + (uint32_t)(int32_t)(int16_t)(uint16_t)(column + row * 0x28));
        node = EF_U32(node + 4u);
    }
}

/* 0xcc468, 74 bytes. True when the first node of this kind has a negative flag byte. */
uint32_t node_has_negative_flag_cc468(uint8_t kind, ef_addr32 list)
{
    ef_addr32 node = EF_U32(list);

    while (node != 0) {
        if (EF_U8(node + 0xbu) == kind) {
            int16_t i;

            for (i = 0; i < (int16_t)EF_U16(node + 9u); i++) {
                if ((int8_t)EF_U8(EF_U32(node + 0xcu) + (uint32_t)(int32_t)i) < 0)
                    return 1;
            }
            return 0;
        }
        node = EF_U32(node + 4u);
    }
    return 0;
}

/* 0xcc840, 83 bytes. Count the 21-byte entries that are free (0xff) or reserved (0xfe) for other kinds. */
uint32_t count_open_entries_cc840(ef_addr32 object)
{
    uint32_t count = 0;
    int16_t i;

    for (i = 0; i < (int16_t)EF_U16(object + 0x2au); i++) {
        uint32_t code = EF_U8(EF_U32(object + 0x2cu) + (uint32_t)((int32_t)i * 0x15));

        if (code == 0xff) {
            count++;
        } else if (code == 0xfe) {
            uint32_t kind = EF_U8(object + 0xbu);

            if (kind != 0x49 && kind != 0x4f)
                count++;
        }
    }
    return count;
}

/* 0xcd7a0, 77 bytes. Free the blocks in table slots 101..n and reset the count. */
void free_table_blocks_cd7a0(void)
{
    int32_t index = (int32_t)EF_U32(0x0015b730u) + 0x64;

    if (index > 0x64) {
        for (;;) {
            uint32_t slot = (uint32_t)(index * 4);
            ef_addr32 block = EF_U32(slot + 0x0041f83cu);

            slot -= 4u;
            index--;
            if (block != 0) {
                guest_fn_cc768(block);
                EF_U32(slot + 0x0041f840u) = 0;
            }
            if (!((int32_t)slot > 0x190))
                break;
        }
    }
    EF_U32(0x0015b730u) = 0;
}

/* 0xcd81c, 15 bytes. Forward the stack word to 0xcd7f0. */
uint32_t forward_word_cd81c(int16_t stack_value)
{
    return guest_fn_cd7f0((int32_t)stack_value);
}
