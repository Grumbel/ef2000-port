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

/* 0x9e3f8, 155 bytes. Make a page current: record it at 0x41722a/0x417174/0x41717c and, when its table entry has flag 1, clear the eight words (+2..+0x10) and eight bytes (+0x14..) of each of its 0x1d-byte sub-records before notifying action 0. */
void select_page_9e3f8(uint32_t page)
{
    ef_addr32 obj, entry;
    uint32_t i, j;
    EF_U16(0x0041722au) = (uint16_t)page;
    obj = (uint32_t)guest_fn_b3f58((int32_t)(int16_t)page);
    entry = EF_U32(0x0040d200u + (uint32_t)EF_U16(obj) * 4u);
    EF_U32(0x00417174u) = obj;
    if (EF_U8(entry + 5u) & 1u) {
        for (i = 0; i < EF_U8(entry + 1u); i++) {
            uint32_t base = obj + 0xbu + i * 0x1du;
            for (j = 0; j < 8u; j++) {
                EF_U8(base + 0x14u + j) = 0u;
                EF_U16(base + 2u + j * 2u) = 0u;
            }
        }
    }
    EF_U32(0x0041717cu) = entry;
    notify_tactical_action((int32_t)(int16_t)page, 0u);
}

/* 0x110790, 156 bytes. Load run-length palette blocks into the 768-byte table at 0x4dd9b0: a count byte, then per block a start delta and an entry count (0 means 256) followed by 3 bytes per entry. Calls the hook at 0x17d61c afterwards if set. */
void load_palette_blocks_110790(ef_addr32 data)
{
    uint32_t blocks = EF_U8(data);
    uint32_t p = data + 2u;
    uint32_t b;
    uint8_t cur = 0u;
    for (b = 0; (int16_t)b < (int16_t)blocks; b++) {
        int16_t n = (int16_t)EF_U8(p + 1u);
        int16_t k;
        cur = (uint8_t)(cur + EF_U8(p));
        p += 2u;
        if (n == 0)
            n = 0x100;
        for (k = 0; (int32_t)k < (int32_t)n * 3; k++)
            EF_U8(0x004dd9b0u + (uint32_t)cur * 3u + (uint32_t)(int32_t)k) = EF_U8(p++);
    }
    if (EF_U32(0x0017d61cu) != 0u)
        guest_callptr_eax_x(EF_U32(0x0017d61cu), 0x004dd9b0u);
}

/* 0xc9238, 171 bytes. Store a clip rectangle (a, b, c-1, d-1) at 0x15b60e..0x15b61c, build the two 64-entry shade tables at 0x41f3f0 on first use, then select table s0 and run 0xc4b95 on the rectangle block. */
void set_clip_and_shade_c9238(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t shade)
{
    uint32_t t, k;
    EF_U16(0x0015b614u) = (uint16_t)b;
    EF_U16(0x0015b610u) = (uint16_t)b;
    EF_U16(0x0015b61au) = (uint16_t)a;
    EF_U16(0x0015b60eu) = (uint16_t)a;
    EF_U16(0x0015b61cu) = (uint16_t)(d - 1u);
    EF_U16(0x0015b618u) = (uint16_t)(d - 1u);
    EF_U16(0x0015b616u) = (uint16_t)(c - 1u);
    EF_U16(0x0015b612u) = (uint16_t)(c - 1u);
    if (EF_U8(0x0015b61eu) == 0u) {
        EF_U8(0x0015b61eu) = 1u;
        for (t = 0; t < 2u; t++)
            for (k = 0; k < 0x40u; k++)
                fill_bytes_b3fb0(0x0041f3f0u + t * 0x100u + k * 4u, (uint8_t)k, 4u);
    }
    EF_U32(0x00160000u) = 0u;
    EF_U32(0x00159080u) = 0x0041f3f0u + (shade & 0xffu) * 4u;
    guest_fn_c4b95_x(0x0015b5fcu);
}

/* 0x74ed8, 219 bytes. Randomly raise the twelve 0..100 counters (+0x18e) of the object found for key by up to n, redrawing its entries first when the counter is zero and the object is not flagged 4 or 8. */
void raise_random_counters_74ed8(uint32_t key, uint32_t n)
{
    ef_addr32 obj = guest_fn_7489c((int32_t)(int16_t)key);
    uint32_t j;
    if (obj == 0u || (int16_t)n <= 0)
        return;
    for (j = 0; j < 12u; j++) {
        uint32_t rem2, add, v;
        if ((half_word_9c960() & 0xffffu) % 2u == 0u)
            continue;
        if (!(EF_U8(obj + 0x24u) & 4u) && !(EF_U8(obj + 0x24u) & 8u) && EF_U8(obj + j + 0x18eu) == 0u)
            draw_object_entries_46168((uint32_t)(int32_t)(int16_t)key, (uint32_t)(int32_t)EF_S16(0x00128524u + j * 2u), 0u, 0u, 0u);
        rem2 = (half_word_9c960() & 0xffffu);
        add = rem2 % (uint32_t)(int32_t)(int16_t)n;
        v = (uint8_t)(EF_U8(obj + j + 0x18eu) + add);
        EF_U8(obj + j + 0x18eu) = (uint8_t)v;
        if (v > 100u)
            EF_U8(obj + j + 0x18eu) = 100u;
    }
}

/* 0x22f28, 241 bytes. Draw the HUD indicator for slot idx (10-byte records at 0x127a96): if its text slot exists and its enable byte (0x1a81d8, offset 5 for slots above 2) is set, set up the scaled table and plot the indicator at the position scaled by 1/4 and 3/8 from the record's words. */
void draw_indicator_22f28(uint32_t index)
{
    int32_t idx = (int16_t)index;
    uint32_t rec = (uint32_t)(idx * 10);
    int32_t px, py;
    if (guest_fn_cd7f0((int32_t)(EF_U32(rec + 0x127a9du) >> 24)) == 0u)
        return;
    if (EF_U8(0x001a81d8u + (uint32_t)(idx - (idx > 2 ? 5 : 0))) == 0u)
        return;
    set_scaled_table_38750(0xedu, 0x1eu, 0xfu, 6u, 0xdu, 1u, 1u);
    guest_fn_387e0_x(EF_U16(0x00127d3cu), EF_U16(0x00127d40u));
    px = (int32_t)EF_S16(rec + 0x127a9eu) / 4 + (int32_t)EF_S16(rec + 0x127a9au);
    py = (int32_t)EF_S16(rec + 0x127a9cu) * 3 / 8 + (int32_t)EF_S16(rec + 0x127a98u);
    guest_fn_38be8_x(EF_U32(rec + 0x127a9du) >> 24, (int32_t)(int16_t)py, (int32_t)(int16_t)px);
}
