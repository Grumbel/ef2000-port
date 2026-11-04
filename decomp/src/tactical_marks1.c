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
