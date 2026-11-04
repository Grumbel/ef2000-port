/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Target checks, queue shifts, packet and list helpers, 0x2c17c..0x90880. */

/* 0x2c17c, 165 bytes. True when an owner has a linked unit whose own link is on another team. */
uint8_t has_enemy_link_2c17c(int16_t owner)
{
    int16_t i = 0;

    for (;;) {
        ef_addr32 base = EF_U32(0x0019bb58u);
        ef_addr32 record = base + (uint32_t)((int32_t)owner * 0xca);
        ef_addr32 unit;
        ef_addr32 target;

        if (!((int32_t)i < (int32_t)EF_U8(record + 0x4fu)))
            return 0;
        unit = guest_fn_7489c((int32_t)(int16_t)EF_U16(record + (uint32_t)((int32_t)i * 2) + 0x50u));
        if (unit != 0) {
            target = guest_fn_7489c((int32_t)(int16_t)EF_U16(unit + 0x146u));
            if (target != 0) {
                ef_addr32 unit_owner = base + (uint32_t)((int32_t)(int16_t)EF_U16(unit + 0x28u) * 0xca);
                ef_addr32 target_owner = base + (uint32_t)((int32_t)(int16_t)EF_U16(target + 0x28u) * 0xca);

                if (EF_U16(unit_owner + 0x15u) != EF_U16(target_owner + 0x15u) &&
                    EF_U16(record + 0xb9u) == EF_U16(target + 0x28u))
                    return 1;
            }
        }
        i++;
    }
}

/* 0x4e6bc, 169 bytes. Drop the oldest queued entry by shifting the 24-entry queue down. */
void shift_queue_4e6bc(void)
{
    uint32_t i;
    uint8_t code;

    if (EF_U8(0x001a5674u) == 0x11)
        return;
    i = EF_U8(0x001a559fu);
    code = EF_U8(0x001a5672u + i);
    if (code == 8)
        EF_U8(0x001a5708u) = 0;
    code = EF_U8(0x001a5672u + EF_U8(0x001a559fu));
    if (code == 5)
        EF_U8(0x001a5709u) = 0;
    for (; (int16_t)i < 0x18; i++) {
        EF_U16(0x001a55a2u + i * 2u) = EF_U16(0x001a55a4u + i * 2u);
        EF_U16(0x001a55d6u + i * 2u) = EF_U16(0x001a55d8u + i * 2u);
        EF_U8(0x001a5672u + i) = EF_U8(0x001a5673u + i);
        EF_U32(0x001a560au + i * 4u) = EF_U32(0x001a560eu + i * 4u);
    }
    EF_U8(0x001a568au) = 0x11;
}

/* 0x56a4c, 166 bytes. Send packet 0x12 with the local owner's name and counters. */
void send_owner_summary_56a4c(void)
{
    uint8_t packet[0x20];
    uint32_t owner = EF_U8(0x001a666bu);
    uint32_t i;
    uint8_t flag;

    if ((int32_t)owner >= 8)
        return;
    for (i = 0; i < sizeof(packet); i++)
        packet[i] = 0;
    packet[0] = 0x12;
    packet[1] = (uint8_t)owner;
    packet[2] = 0;
    packet[3] = (uint8_t)EF_U16(0x001f71ceu);
    packet[4] = (uint8_t)(EF_U16(0x001f71ceu) >> 8);
    for (i = 0; i < 0x11; i++) {
        uint8_t c = EF_U8(0x001a62f0u + owner * 17u + i);

        packet[5 + i] = c;
        if (c == 0)
            break;
    }
    owner = EF_U8(0x001a666bu);
    packet[0x16] = (uint8_t)EF_U16(0x001a6388u + owner * 2u);
    packet[0x17] = (uint8_t)(EF_U16(0x001a6388u + owner * 2u) >> 8);
    packet[0x18] = (uint8_t)EF_U16(0x001a6378u + owner * 2u);
    packet[0x19] = (uint8_t)(EF_U16(0x001a6378u + owner * 2u) >> 8);
    packet[0x1a] = (uint8_t)EF_U16(0x001a6398u + owner * 2u);
    packet[0x1b] = (uint8_t)(EF_U16(0x001a6398u + owner * 2u) >> 8);
    packet[0x1c] = EF_U8(0x00122c5cu);
    packet[0x1e] = EF_U8(0x00126518u);
    flag = packet[0x1c];
    EF_U8(0x001a63a8u + owner) = (uint8_t)(flag * 0xf4u);
    emit_command_packet(packet, 0x1f);
}

/* 0x79128, 169 bytes. Per-frame HUD pass: latch the screen rectangle and run the overlay updates. */
void update_hud_79128(void)
{
    latch_values_ce328(0, EF_U32(0x0015b744u) - 1u, 0, EF_U32(0x0015b748u) - 1u);
    guest_fn_cdf48(0, EF_U32(0x0015b744u) - 1u, 0, EF_U32(0x0015b748u) - 1u);
    if (EF_U8(0x00126654u) != 0)
        guest_fn_78848();
    if ((int16_t)EF_U16(0x001f8d52u) > 0)
        guest_fn_77c34();
    else if (EF_U8(0x00126655u) != 0)
        guest_fn_77c90();
    if (EF_U8(0x001f8d58u) != 0)
        guest_fn_78934();
    guest_fn_78a0c();
    if (EF_U16(0x001f8d56u) != 0)
        guest_fn_78ac0();
    if (EF_U8(0x00197256u) != 0) {
        if (EF_U16(0x00126516u) == 0)
            guest_fn_78b4c();
        guest_fn_78f3c();
    }
}

/* 0x80a18, 162 bytes. Reset the 256 name entries, register them in the page table and clear 16 slots. */
void init_name_entries_80a18(void)
{
    int16_t i;

    EF_U16(0x001fc96eu) = (uint16_t)guest_fn_a1a40_key(0x0018885cu);
    for (i = 0; i < 0x100; i++) {
        uint32_t offset = (uint32_t)((int32_t)i * 0x38);
        uint32_t pair;

        EF_U8(offset + 0x001f90a0u) = 0;
        EF_U16(offset + 0x001f90a2u) = (uint16_t)guest_lookup_value(0x00188868u);
        pair = find_page_pair_a1a90((int16_t)EF_U16(offset + 0x001f90a2u),
                                    (int32_t)(int16_t)EF_U16(0x001fc96eu));
        EF_U16(pair) = (uint16_t)i;
    }
    for (i = 0; i < 0x10; i++) {
        EF_U16((uint32_t)((int32_t)i * 12) + 0x001fc8a4u) = (uint16_t)(i * 0x10);
        EF_U16((uint32_t)((int32_t)i * 12) + 0x001fc8a2u) = 0xffffu;
    }
    EF_U16(0x001fc96cu) = 0xffffu;
    EF_U32(0x00158f70u) = 0x0008078cu;
}

/* 0x878c8, 165 bytes. Remove an object from the active list and put it on the free list. */
void release_object_878c8(ef_addr32 object)
{
    ef_addr32 head;
    ef_addr32 next;
    uint8_t flag;

    if (((int32_t)EF_U32(object + 0x45u) >> 16) != 0x1234 &&
        ((int32_t)EF_U32(object + 0x47u) >> 16) != 0x1234)
        guest_fn_a1770();
    update_object_with_link(EF_U32(object + 0x30u), object);
    if (object == EF_U32(0x0020f15cu))
        EF_U32(0x0020f15cu) = 0;
    if (EF_U8(0x00125847u) != 0 && EF_U16(object + 0x2eu) != 0)
        guest_fn_9e508((int32_t)EF_U32(object + 0x2cu) >> 16, 0);
    head = EF_U32(0x0020f158u);
    if (object == head)
        EF_U32(0x0020f158u) = EF_U32(head + 0x38u);
    next = EF_U32(object + 0x34u);
    EF_U32(EF_U32(object + 0x38u) + 0x34u) = next;
    EF_U32(next + 0x38u) = EF_U32(object + 0x38u);
    flag = EF_U8(object + 0x58u);
    next = EF_U32(0x0020f168u);
    EF_U32(0x0020f168u) = object;
    EF_U32(object + 0x34u) = next;
    if (flag != 0)
        clear_panel_strip((int16_t)((int32_t)EF_U32(object + 0x66u) >> 16));
}

/* 0x90880, 161 bytes. Keep the nearest candidate in an object's +0x1c link (Chebyshev-like distance). */
void update_nearest_link_90880(ef_addr32 object, ef_addr32 candidate)
{
    int32_t dx;
    int32_t dz;
    int32_t distance;
    ef_addr32 current;

    if (scene_node_is_registered(object) == 0)
        return;
    if (candidate == EF_U32(object + 0x1cu))
        return;
    dx = (int32_t)abs_cf94a((int32_t)(EF_U32(candidate) - EF_U32(object)));
    dz = (int32_t)abs_cf94a((int32_t)(EF_U32(candidate + 8u) - EF_U32(object + 8u)));
    if (dx > dz)
        distance = (int32_t)abs_cf94a((int32_t)(EF_U32(candidate) - EF_U32(object)));
    else
        distance = (int32_t)abs_cf94a((int32_t)(EF_U32(candidate + 8u) - EF_U32(object + 8u)));
    if (distance > 0x30d400)
        return;
    current = EF_U32(object + 0x1cu);
    if (current != 0) {
        int32_t cx = (int32_t)abs_cf94a((int32_t)(EF_U32(current) - EF_U32(object)));
        int32_t cz = (int32_t)abs_cf94a((int32_t)(EF_U32(current + 8u) - EF_U32(object + 8u)));
        int32_t current_distance;

        if (cx > cz)
            current_distance = (int32_t)abs_cf94a((int32_t)(EF_U32(current) - EF_U32(object)));
        else
            current_distance = (int32_t)abs_cf94a((int32_t)(EF_U32(current + 8u) - EF_U32(object + 8u)));
        if (current_distance < distance)
            return;
    }
    EF_U32(object + 0x1cu) = candidate;
}
