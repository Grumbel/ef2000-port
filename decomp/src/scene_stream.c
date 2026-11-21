/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

static uint16_t byteswap16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

#define SCENE_BAD_FORMAT   0x00158e92u /* fatal message for flag bits 0..1 */
#define SCENE_INDEX_HOOK   0x0015af8cu /* index converter used by the stream interpreters */
#define SCENE_END          0x7fffu
#define SCENE_RECORD       0x000c4e3bu /* record in ESI, scene read from [EBP+8] */

/* 0xc4db4, 135 bytes. Byte-swap the big-endian scene header in place: the
 * flag word at +0x20 (bits 0..1 set is fatal; bit 2 selects the 0xc53b4
 * index converter instead of 0xc53cf), an empty name at +0x30 is filled in
 * by 0xe3cac, then the words at +0x10 and +0 and each command word from +2
 * on, which 0xc4e3b (record in ESI) normalizes, until the end marker; its
 * handlers read the scene back from this function's frame at [EBP+8]. One
 * stack argument, callee pops. The fatal exit does not return (the
 * original does not even pop its argument). */
void renderer_normalize_scene_c4db4(ef_addr32 scene)
{
    ef_addr32 cursor;
    uint16_t flags = byteswap16(EF_U16(scene + 0x20u));

    EF_U16(scene + 0x20u) = flags;
    if ((flags & 3u) != 0u) {
        guest_callptr1_x(EF_U32(0x00132a74u), SCENE_BAD_FORMAT);
        return;
    }
    EF_U32(SCENE_INDEX_HOOK) = (flags & 4u) ? 0xc53b4u : 0xc53cfu;
    if ((EF_U32(scene + 0x30u) | EF_U32(scene + 0x34u)) == 0u)
        guest_fn_e3cac(scene + 0x30u);
    EF_U16(scene + 0x10u) = byteswap16(EF_U16(scene + 0x10u));
    EF_U16(scene) = byteswap16(EF_U16(scene));
    cursor = scene;
    do {
        cursor += 2u;
        EF_U16(cursor) = byteswap16(EF_U16(cursor));
        guest_callptr_esi_frame_x(SCENE_RECORD, cursor, scene);
    } while (EF_U16(cursor) != SCENE_END);
}
