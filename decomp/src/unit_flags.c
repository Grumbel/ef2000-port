/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Unit flag toggles and menu helpers, 0x6c3d0..0x8dc94. */

/* Copy bit `from` of the word at record+offset into bit `to`, as the original does with shifts. */
static void copy_flag_bit(ef_addr32 record, uint32_t offset, uint32_t from, uint32_t to)
{
    uint16_t word = EF_U16(record + offset);
    uint16_t bit = (uint16_t)((word >> from) & 1u);
    word = (uint16_t)(word & ~(1u << to));
    word = (uint16_t)(word | (bit << to));
    EF_U16(record + offset) = word;
}

/* 0x6c3d0, 99 bytes. */
void notify_player_unit_6c3d0(void)
{
    ef_addr32 player = EF_U32(0x001f71bcu);
    uint32_t type = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(player + 0x26u) >> 16) * 0xcau + 0x1au);
    uint8_t kind;
    if (type != 2)
        return;
    kind = EF_U8(player + 0x16cu);
    if (kind == 1 || kind == 3) {
        player = EF_U32(0x001f71bcu);
        type = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(player + 0x26u) >> 16) * 0xcau + 0x1au);
        guest_fn_6a1d0(EF_S32(0x001f71c6u) >> 16, type);
    }
}

/* 0x6c8e0, 73 bytes. */
void refresh_player_6c8e0(void)
{
    uint8_t i;
    for (i = 0; i < 8; ++i)
        guest_fn_73500(EF_S32(0x001f71c6u) >> 16);
    if (EF_U8(EF_U32(0x001f71bcu) + 0x141u) != 0)
        guest_fn_73c38(EF_S32(0x001f71c6u) >> 16);
    guest_fn_73d1c(EF_S32(0x001f71c6u) >> 16);
}

/* 0x7104c, 87 bytes. Advance the state byte +0x16e of the player record. */
void advance_state_7104c(void)
{
    ef_addr32 player = EF_U32(0x001f71bcu);
    uint8_t state = EF_U8(player + 0x16eu);
    if (state == 1) {
        uint32_t value = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(player + 0x26u) >> 16) * 0xcau + 0x5fu);
        guest_fn_7bc0(EF_S32(0x001f71c6u) >> 16, value);
        EF_U8(EF_U32(0x001f71bcu) + 0x16eu) = 2;
    } else if (state == 2 || state == 3) {
        guest_fn_710a4();
    }
}

/* 0x7baa4, 91 bytes. */
void adjust_zoom_7baa4(void)
{
    int16_t value = (int16_t)EF_U16(0x0012afd6u);
    if (EF_U8(0x0015b7e4u) < 2 && EF_U32(0x0012813cu) == 0) {
        uint16_t reference = EF_U16(0x0012afd4u);
        if (reference > 8 && value > 6)
            --value;
        if (reference < 6 && value < 0x0e)
            ++value;
    } else {
        value = (int16_t)EF_U16(0x0012afd8u);
    }
    EF_U16(0x0012afd6u) = (uint16_t)value;
}

/* 0x7fb9c, 77 bytes. Character code for an object. */
uint8_t object_glyph_7fb9c(ef_addr32 object)
{
    uint8_t type;
    if (object == 0 || EF_U8(object + 0x42u) != 0)
        return 0x47;
    type = EF_U8(object + 0x18u);
    if (EF_U8(object + 0x58u) != 0 || type == 5)
        return 0x26;
    if (type == 8)
        return EF_U8(0x0012db9eu) == 0x10 ? 0x26 : 0x43;
    return 0x47;
}

/* 0x80120, 105 bytes. */
void reset_menu_80120(void)
{
    EF_U8(0x001f9097u) = 0x0e;
    EF_U8(0x001f9096u) = 0;
    EF_U8(0x001f909bu) = 0;
    EF_U8(0x001f9094u) = 0;
    EF_U8(0x001f909au) = 0;
    EF_U8(0x001f9099u) = 0;
    EF_U8(0x0012b088u) = 0;
    EF_U32(0x0012b080u) = 0x47;
    EF_U8(0x0012b089u) = 1;
    EF_U32(0x0012b084u) = 0;
    EF_U8(0x001f9095u) = 0;
    if (guest_fn_d6f88() != 0)
        guest_fn_7fe8c();
}

/* 0x81308, 74 bytes. Prepare the 0x28-byte block at 0x1fc980; returns its descriptor or 0. */
uint32_t prepare_block_81308(void)
{
    if (EF_U8(0x0012b104u) == 0 || (uint8_t)guest_fn_d75fc(0xb) != 0)
        return 0;
    EF_U32(0x0012b118u) = 0x32000;
    guest_fn_b3fb0(0x001fc980u, 0, 0x28);
    EF_U32(0x001fc9f8u) = 0;
    return 0x00188994u;
}

/* 0x8160c, 111 bytes. */
void trigger_sound_8160c(uint16_t index)
{
    ef_addr32 player = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    if (player == 0 || (EF_U8(player + 0x24u) & 0x10u) || index == 0xffffu)
        return;
    if (EF_U8(0x0021454au) != 0 || EF_U8(0x0012b104u) == 0 || EF_U8(0x0012b105u) == 0)
        return;
    if ((uint8_t)guest_fn_d75fc(0xc) != 0)
        return;
    guest_fn_d7158(0xc, 1, EF_U32(EF_U32(0x001fca00u) + (uint32_t)(int32_t)(int16_t)index * 4u), 0x7f);
}

/* 0x895bc, 77 bytes. */
void select_object_895bc(ef_addr32 object)
{
    EF_U8(0x0012d7b3u) = 1;
    EF_U8(0x0020f22fu) = 1;
    if (EF_U8(object + 0x5au) == 0) {
        guest_fn_87970(object);
        return;
    }
    EF_U8(object + 0x58u) = 1;
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S32(object + 0x2cu) >> 16, 3);
    guest_fn_8a318(object);
}

/* 0x8960c, 79 bytes. */
void select_object_8960c(ef_addr32 object)
{
    EF_U8(0x0020f22fu) = 2;
    EF_U8(0x0012d7b3u) = 0;
    if (EF_U8(object + 0x5au) == 0) {
        guest_fn_87970(object);
        return;
    }
    EF_U8(object + 0x58u) = 0;
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S32(object + 0x2cu) >> 16, 4);
    guest_fn_8a318(object);
}

/* 0x898d4, 80 bytes. */
void force_select_898d4(ef_addr32 object)
{
    uint8_t saved = EF_U8(object + 0x5au);
    uint8_t type;
    EF_U8(object + 0x58u) = 1;
    EF_U8(0x0012d7b3u) = 1;
    EF_U8(object + 0x5au) = 1;
    type = EF_U8(object + 0x18u);
    if (type == 5 || type == 8)
        EF_U8(object + 0x58u) = 0;
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S32(object + 0x2cu) >> 16, 3);
    guest_fn_8a318(object);
    EF_U8(object + 0x5au) = saved;
}

/* 0x8b9e8, 71 bytes. Find the list node (list dword 0x12db5c, next at +0x1d) whose coordinates match; returns its word +0x1b or -1. */
uint32_t find_cell_8b9e8(int16_t x, int16_t z)
{
    uint32_t gx = ((uint32_t)(int32_t)x << 19) + 0x40000u;
    uint32_t gz = ((uint32_t)(int32_t)z << 19) + 0x40000u;
    ef_addr32 node = EF_U32(0x0012db5cu);
    while (node != 0) {
        if (gx == EF_U32(node + 0xfu) && gz == EF_U32(node + 0x17u))
            return EF_U16(node + 0x1bu);
        node = EF_U32(node + 0x1du);
    }
    return 0xffffffffu;
}

/* 0x8daa0, 126 bytes. Copy flag bit 2 of word +0x26 into bit 3 and announce it. */
void toggle_flag_8daa0(int16_t unit)
{
    ef_addr32 record = guest_fn_7489c((int32_t)unit);
    if (record == 0)
        return;
    copy_flag_bit(record, 0x26u, 2, 3);
    if (EF_U8(0x001285a1u + (uint32_t)EF_U8(record + 0xe9u) * 0xe5u) != 1 || EF_U8(0x00125847u) == 0)
        return;
    guest_fn_9e508(EF_S32(record + 0x1eu) >> 16, ((EF_U8(record + 0x26u) & 8u) == 0) + 7u);
}

/* 0x8db20, 95 bytes. Copy flag bit 6 of word +0x24 into bit 7. */
void toggle_flag_8db20(int16_t unit)
{
    ef_addr32 record = guest_fn_7489c((int32_t)unit);
    if (record == 0)
        return;
    copy_flag_bit(record, 0x24u, 6, 7);
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S32(record + 0x1eu) >> 16, ((EF_U8(record + 0x24u) & 0x80u) == 0) + 3u);
}

/* 0x8db80, 95 bytes. Copy flag bit 10 of word +0x24 into bit 11. */
void toggle_flag_8db80(int16_t unit)
{
    ef_addr32 record = guest_fn_7489c((int32_t)unit);
    if (record == 0)
        return;
    copy_flag_bit(record, 0x24u, 10, 11);
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S32(record + 0x1eu) >> 16, ((EF_U8(record + 0x25u) & 8u) == 0) + 0x0du);
}

/* 0x8dbe0, 95 bytes. Copy flag bit 8 of word +0x24 into bit 9. */
void toggle_flag_8dbe0(int16_t unit)
{
    ef_addr32 record = guest_fn_7489c((int32_t)unit);
    if (record == 0)
        return;
    copy_flag_bit(record, 0x24u, 8, 9);
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S32(record + 0x1eu) >> 16, ((EF_U8(record + 0x25u) & 2u) != 0) + 0x14u);
}

/* 0x8dc40, 82 bytes. Copy flag bit 0 of word +0x24 into bit 5. */
void toggle_flag_8dc40(int16_t unit)
{
    ef_addr32 record = guest_fn_7489c((int32_t)unit);
    if (record == 0)
        return;
    copy_flag_bit(record, 0x24u, 0, 5);
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S32(record + 0x1eu) >> 16, ((EF_U8(record + 0x24u) & 0x20u) == 0) + 9u);
}

/* 0x8dc94, 95 bytes. Copy flag bit 12 of word +0x24 into bit 14. */
void toggle_flag_8dc94(int16_t unit)
{
    ef_addr32 record = guest_fn_7489c((int32_t)unit);
    if (record == 0)
        return;
    copy_flag_bit(record, 0x24u, 12, 14);
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S32(record + 0x1eu) >> 16, ((EF_U8(record + 0x25u) & 0x40u) == 0) + 5u);
}
