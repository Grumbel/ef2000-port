/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Engine list and table helpers, 0xca1c8..0xcb688. */

/* 0xca1c8, 53 bytes. Store position words and optionally the two link pointers in a record. */
void set_record_links_ca1c8(ef_addr32 record, uint32_t link, uint16_t word_a, uint16_t word_b,
                            uint32_t stack_word_c, uint8_t stack_set_first, uint8_t stack_set_second)
{
    if (record == 0)
        return;
    if (stack_set_first != 0)
        EF_U32(record + 0x8du) = link;
    if (stack_set_second != 0)
        EF_U32(record + 0x91u) = link;
    EF_U16(record + 0x37u) = word_a;
    EF_U16(record + 0x39u) = word_b;
    EF_U16(record + 0x3bu) = (uint16_t)stack_word_c;
}

/* 0xca838, 58 bytes. Unlink and free every kind-2 node of the list. */
void free_kind2_nodes_ca838(ef_addr32 head, ef_addr32 other)
{
    ef_addr32 node = EF_U32(head);

    while (node != 0) {
        ef_addr32 next = EF_U32(node + 4u);

        if (EF_U8(node + 0xbu) == 2) {
            unlink_node_bc594(node, head, other);
            free_nested_record_b19a0(node);
        }
        node = next;
    }
}

/* 0xcabb0, 56 bytes. Translate 400 bytes through the lookup routine at 0xcbef4. */
void translate_bytes_cabb0(ef_addr32 destination, ef_addr32 source)
{
    int16_t i;

    for (i = 0; i < 0x190; i++)
        EF_U8(destination + (uint32_t)(int32_t)i) = (uint8_t)guest_fn_cbef4(EF_U8(source + (uint32_t)(int32_t)i));
}

/* 0xcabe8, 98 bytes. Copy fields from the first node of list A into every matching node of list B. */
void copy_matching_fields_cabe8(ef_addr32 list_a, ef_addr32 list_b)
{
    ef_addr32 source = EF_U32(list_a);
    ef_addr32 target = EF_U32(list_b);

    while (source != 0) {
        /* The original never reloads the target list, so only the first source node can match. */
        while (target != 0) {
            if (EF_U32(source + 0x11u) == EF_U32(target + 0x16u) && EF_U32(source + 0x15u) == EF_U32(target + 0x1au)) {
                EF_U16(target + 0x32u) = EF_U16(source + 0x23u);
                if ((int16_t)EF_U16(target + 0x32u) < 0x3e8)
                    EF_U16(target + 0x32u) = (uint16_t)(EF_U16(target + 0x32u) + 0x3e8u);
                EF_U16(target + 0x34u) = EF_U16(source + 0x25u);
            }
            target = EF_U32(target + 4u);
        }
        source = EF_U32(source + 4u);
    }
}

/* 0xcb688, 90 bytes. Count down the per-slot timers of every node and reload expired ones. */
void tick_slot_timers_cb688(ef_addr32 head)
{
    ef_addr32 node = EF_U32(head);

    while (node != 0) {
        int16_t i;

        for (i = 0; i < (int16_t)EF_U16(node + 9u); i++) {
            ef_addr32 timer = EF_U32(node + 0x10u) + (uint32_t)(int32_t)i;

            if (EF_U8(timer) != 0)
                EF_U8(timer) = (uint8_t)(EF_U8(timer) - 1);
            if (EF_U8(EF_U32(node + 0x10u) + (uint32_t)(int32_t)i) == 0)
                EF_U8(EF_U32(node + 0xcu) + (uint32_t)(int32_t)i) = EF_U8(EF_U32(node + 0x14u) + (uint32_t)(int32_t)i);
        }
        node = EF_U32(node + 4u);
    }
}
