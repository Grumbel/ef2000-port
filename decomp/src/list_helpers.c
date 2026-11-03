/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Linked-list, record-free and classification helpers, 0xaa1e4..0xbec84. */

/* Append node to the doubly linked list whose head word is at head_addr (links at +0/+4). */
static void append_list_node(ef_addr32 head_addr, ef_addr32 node)
{
    ef_addr32 head = EF_U32(head_addr);

    if (head == 0) {
        EF_U32(node) = head;
        EF_U32(head_addr) = node;
        EF_U32(node + 4u) = head;
    } else {
        ef_addr32 tail = head;

        if (EF_U32(head) != 0) {
            do
                tail = EF_U32(tail);
            while (EF_U32(tail) != 0);
        }
        EF_U32(tail) = node;
        EF_U32(node) = 0;
        EF_U32(node + 4u) = tail;
    }
}

/* 0xaa1e4, 66 bytes. */
void append_node_aa1e4(ef_addr32 node)
{
    EF_U16(0x004194c4u) = (uint16_t)(EF_U16(0x004194c4u) + 1u);
    append_list_node(0x004191fcu, node);
}

/* 0xaa228, 69 bytes. Append a child (links at +1/+5) to the list of the current owner. */
void append_child_aa228(ef_addr32 node)
{
    ef_addr32 owner = EF_U32(0x004191f8u);
    ef_addr32 first;

    EF_U16(owner + 0xcu) = (uint16_t)(EF_U16(owner + 0xcu) + 1u);
    first = EF_U32(owner + 8u);
    if (first == 0) {
        EF_U32(owner + 8u) = node;
        EF_U32(node + 1u) = first;
        EF_U32(node + 5u) = first;
    } else {
        ef_addr32 tail = first;

        if (EF_U32(first + 1u) != 0) {
            do
                tail = EF_U32(tail + 1u);
            while (EF_U32(tail + 1u) != 0);
        }
        EF_U32(tail + 1u) = node;
        EF_U32(node + 1u) = 0;
        EF_U32(node + 5u) = tail;
    }
}

/* 0xaa270, 66 bytes. */
void append_node_aa270(ef_addr32 node)
{
    EF_U16(0x004194c2u) = (uint16_t)(EF_U16(0x004194c2u) + 1u);
    append_list_node(0x00419200u, node);
}

/* 0xaa2b4, 68 bytes. Resolve the (parent index, child index) pair of node to the child pointer. */
void resolve_child_aa2b4(ef_addr32 node)
{
    ef_addr32 p = EF_U32(0x00419200u);
    uint32_t i;

    for (i = 0; i < EF_U8(node + 0xbu); i++)
        p = EF_U32(p);
    p = EF_U32(p + 8u);
    for (i = 0; i < EF_U8(node + 0xcu); i++)
        p = EF_U32(p + 1u);
    EF_U32(node + 0xdu) = p;
}

/* 0xb0404, 68 bytes. Advance the 16-bit counters at 0x13799c..0x1379a2. */
void advance_counters_b0404(void)
{
    int32_t a = EF_S16(0x001379a0u);
    int32_t b;
    int32_t next = a < 0x10 ? a + 8 : 0;

    EF_U16(0x001379a0u) = (uint16_t)next;
    b = EF_S16(0x0013799eu);
    if ((uint16_t)next == 0)
        b++;
    EF_U16(0x0013799eu) = (uint16_t)b;
    EF_U16(0x001379a2u) = 0;
}

/* 0xb16c0, 64 bytes. Free a record with 10-byte entries (count at +5, array at +6). */
void free_entry_array_b16c0(ef_addr32 record)
{
    int16_t i;

    for (i = 0; i < (int16_t)EF_U8(record + 5u); i++) {
        ef_addr32 sub = EF_U32(EF_U32(record + 6u) + (uint32_t)(int32_t)i * 10u + 4u);

        if (sub != 0)
            guest_fn_cc768(sub);
    }
    guest_fn_cc768(EF_U32(record + 6u));
    guest_fn_cc768(record);
}

/* 0xb1700, 64 bytes. As above with count at +4 and array at +5. */
void free_entry_array_b1700(ef_addr32 record)
{
    int16_t i;

    for (i = 0; i < (int16_t)EF_U8(record + 4u); i++) {
        ef_addr32 sub = EF_U32(EF_U32(record + 5u) + (uint32_t)(int32_t)i * 10u + 4u);

        if (sub != 0)
            guest_fn_cc768(sub);
    }
    guest_fn_cc768(EF_U32(record + 5u));
    guest_fn_cc768(record);
}

/* 0xb17d4, 61 bytes. Free a record with 0x7c-byte objects (count at +0x12, array at +0x13). */
void free_object_array_b17d4(ef_addr32 record)
{
    int16_t i;
    ef_addr32 array;

    for (i = 0; i < (int16_t)EF_U8(record + 0x12u); i++)
        free_object_b1740(EF_U32(record + 0x13u) + (uint32_t)(int32_t)i * 0x7cu);
    array = EF_U32(record + 0x13u);
    if (array != 0)
        guest_fn_cc768(array);
    guest_fn_cc768(record);
}

/* 0xb3d74, 57 bytes. */
void update_ring_b3d74(void)
{
    uint32_t count = EF_U32(0x00146e2cu);
    uint32_t size = EF_U32(0x00146e10u);

    EF_U32(0x00146e20u) = EF_U32(0x00146e14u);
    count += size;
    EF_U16(0x00146e24u) = 0;
    if ((int32_t)(size + count) > 0xfffa)
        count = 0x101;
    EF_U32(0x00146e2cu) = count;
}

/* 0xba40c, 56 bytes. Fill an 8-byte table depending on mode (3 selects the flat variant). */
void fill_level_table_ba40c(ef_addr32 table, uint8_t mode)
{
    if (mode == 3) {
        EF_U8(table) = 0x10;
        EF_U8(table + 1u) = 0x10;
        EF_U8(table + 2u) = 0x10;
        EF_U8(table + 3u) = 0x10;
    } else {
        EF_U8(table) = 4;
        EF_U8(table + 1u) = 4;
        EF_U8(table + 2u) = 3;
        EF_U8(table + 3u) = 3;
    }
    EF_U8(table + 4u) = 3;
    EF_U8(table + 5u) = 3;
    EF_U8(table + 6u) = 2;
    EF_U8(table + 7u) = 2;
}

/* 0xbc594, 59 bytes. Unlink node from a list whose head slot is *head_a (edx) / *head_b (ebx). */
void unlink_node_bc594(ef_addr32 node, ef_addr32 head_a, ef_addr32 head_b)
{
    ef_addr32 next = EF_U32(node);
    ef_addr32 prev;

    if (next != 0) {
        EF_U32(next + 4u) = EF_U32(node + 4u);
    } else {
        next = EF_U32(node + 4u);
        EF_U32(head_a) = next;
        if (next != 0)
            EF_U32(next) = 0;
    }
    prev = EF_U32(node + 4u);
    if (prev != 0) {
        EF_U32(prev) = EF_U32(node);
        return;
    }
    next = EF_U32(node);
    EF_U32(head_b) = next;
    if (next != 0)
        EF_U32(next + 4u) = prev;
}

/* 0xbcbc0, 54 bytes. Is the kind one of 8, 0xe, 0xf, 0x11, 0x13, 0x16, 0x17, 0x1a? */
uint32_t is_kind_bcbc0(uint32_t kind)
{
    kind &= 0xffu;
    return kind == 0x16 || kind == 0x17 || kind == 0xe || kind == 0xf ||
           kind == 0x11 || kind == 8 || kind == 0x13 || kind == 0x1a;
}

/* 0xbd344, 61 bytes. Is there a kind-0x10 entry in the list matching the id at ref+9? */
uint32_t find_kind10_bd344(ef_addr32 ref, ef_addr32 list)
{
    ef_addr32 p;

    for (p = EF_U32(list); p != 0; p = EF_U32(p + 4u)) {
        if (EF_U8(p + 0xau) == 0x10 && EF_U16(p + 0xeu) == EF_U16(ref + 9u) &&
            EF_U16(EF_U32(p + 0x13u) + 0x1eu) == 0)
            return 1;
    }
    return 0;
}

/* 0xbec84, 59 bytes. */
uint32_t is_active_bec84(ef_addr32 object)
{
    uint32_t id;

    if (EF_U8(object + 0x14u) >= 4u)
        return 0;
    if ((int32_t)EF_U8(object + 5u) <= 0x32)
        return 0;
    if (EF_U8(object + 6u) == 0)
        return 0;
    id = EF_U8(object);
    return id != 0xffu && id != 0xfeu;
}
