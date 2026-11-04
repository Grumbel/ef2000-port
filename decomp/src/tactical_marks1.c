/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Tactical map markers. */

/* 0x5d48c, 148 bytes. Draw a diamond of four lines around (x, y); the half-size is the word at 0x1a6ce8 shifted by the zoom byte 0x12b369 and the colour is 0x128148. */
void draw_diamond_5d48c(uint32_t x, uint32_t y)
{
    int32_t s = (int16_t)(uint16_t)((uint32_t)EF_U16(0x001a6ce8u) << EF_U8(0x0012b369u));
    int32_t cx = (int16_t)x, cy = (int16_t)y;
    uint32_t colour = EF_U8(0x00128148u);
    draw_tactical_line(cy - s, cx, cy, cx - s, colour);
    draw_tactical_line(cy, cx + s, cy - s, cx, colour);
    draw_tactical_line(cy + s, cx, cy, cx + s, colour);
    draw_tactical_line(cy, cx - s, cy + s, cx, colour);
}

/* 0x8873c, 148 bytes. Walk the object ring at 0x20f160: for each object of the current side whose link equals link, whose class byte matches kind and for which a random percentage is below chance, re-link it to new_link. */
void relink_random_objects_8873c(int32_t chance, ef_addr32 link, uint32_t kind, ef_addr32 new_link)
{
    uint32_t node = EF_U32(EF_U32(0x0020f160u) + 0x34u);
    while (node != EF_U32(0x0020f164u)) {
        if (EF_U8(node + 0x50u) == EF_U8(0x001a666bu) &&
            link == EF_U32(node + 0x30u) &&
            (uint8_t)kind == EF_U8(0x0012bc97u + EF_U8(node + 0x18u) * 0x91u)) {
            int16_t r = (int16_t)(half_word_9c960() % 100u);
            if (r < (int16_t)chance && new_link != 0u) {
                update_object_with_link(EF_U32(node + 0x30u), node);
                EF_U8(node + 0x59u) = 1u;
                EF_U16(node + 0x56u) = 0xffu;
                EF_U32(node + 0x30u) = new_link;
            }
        }
        node = EF_U32(node + 0x34u);
    }
}

/* 0xf3610, 148 bytes. Draw the segments of path record index (42-byte records at 0x484be0, count in the first word): one 0xe7a34 call per segment but the last, with the colour byte selected from 0x161e10. */
void draw_path_segments_f3610(uint32_t index)
{
    uint32_t rec = (uint32_t)((int32_t)(int16_t)index * 0x2a);
    int16_t i;
    guest_fn_c39ac();
    call_e9a24_c386c();
    for (i = 0; (int32_t)i < (int32_t)EF_S16(rec + 0x484be0u) - 1; i++) {
        uint32_t p = rec + (uint32_t)(int32_t)i * 4u;
        guest_fn_e7a34_x((uint32_t)(int32_t)EF_S16(p + 0x484be8u),
                         (uint32_t)(int32_t)EF_S16(p + 0x484be6u),
                         (uint32_t)(int32_t)EF_S16(p + 0x484be4u),
                         (uint32_t)(int32_t)EF_S16(p + 0x484be2u),
                         EF_U8(0x00161e10u + (uint32_t)(int32_t)EF_S16(0x00161e16u)),
                         (uint32_t)(int32_t)EF_S16(0x00161e1cu));
    }
    guest_fn_dc250();
    guest_fn_c3990();
    guest_fn_dd8dc();
}

/* 0x85f78, 148 bytes. Starting after the current slot (byte +0x1d7) of the player's unit, scan its 20 three-byte slots (+0x198) cyclically for one of class key whose count word (+0x19c) is non-zero and whose type byte is not 0x21/0x22. Returns the slot, 0x13 (after storing key at 0x12bd25) or 0x14 for none. */
uint32_t next_matching_slot_85f78(uint32_t key)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)EF_S16(0x001f71cau));
    int16_t slot;
    key &= 0xffu;
    if ((EF_U32(unit + 0x1d4u) >> 24) == 0x14u)
        return 0x14u;
    slot = (int8_t)EF_U8(unit + 0x1d7u);
    for (;;) {
        uint32_t base, type;
        slot++;
        if ((int32_t)slot >= 0x14)
            slot = 0;
        if (slot == 0x13) {
            EF_U8(0x0012bd25u) = (uint8_t)key;
            return (uint32_t)(uint16_t)slot;
        }
        if (slot == (int16_t)(int8_t)EF_U8(unit + 0x1d7u))
            return 0x14u;
        base = unit + (uint32_t)(int32_t)slot * 3u;
        type = EF_U32(base + 0x198u) >> 24;
        if (key != EF_U8(0x0012bc94u + type * 0x91u))
            continue;
        if (EF_U16(base + 0x19cu) == 0u)
            continue;
        if (type == 0x21u || type == 0x22u)
            continue;
        return (uint32_t)(uint16_t)slot;
    }
}
