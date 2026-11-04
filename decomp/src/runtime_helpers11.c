/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Block release and list search helpers, 0xac9a0..0xb55f8. */

/* 0xac9a0, 58 bytes. Redraw the rectangle of a widget and remember it as the active one. */
void redraw_widget_ac9a0(ef_addr32 widget)
{
    uint16_t left = EF_U16(widget + 0x41u);
    uint16_t top = EF_U16(widget + 0x43u);
    int16_t right = (int16_t)(left + EF_U16(widget + 0x45u));
    int16_t bottom = (int16_t)(top + EF_U16(widget + 0x47u));

    guest_fn_ac980((int16_t)left, (int16_t)top, right, bottom);
    EF_U32(0x0041ec18u) = widget;
}

/* 0xb0554, 69 bytes. True when the list holds a node with the same two keys and the hash of b. */
uint32_t list_has_match_b0554(ef_addr32 a, ef_addr32 b, ef_addr32 list)
{
    uint32_t hash = guest_fn_ca9fc(EF_U32(b + 0x11u), EF_U32(b + 0x15u));
    ef_addr32 node = EF_U32(list);

    while (node != 0) {
        if (EF_U32(node + 9u) == EF_U32(a + 0x11u) && EF_U32(node + 0xdu) == EF_U32(a + 0x15u)
            && (uint16_t)hash == EF_U16(node + 0x1cu))
            return 1;
        node = EF_U32(node + 4u);
    }
    return 0;
}

/* 0xb1840, 76 bytes. Free the four blocks of a record, then the record. */
void free_record_b1840(ef_addr32 record)
{
    if (EF_U32(record + 0xcu) != 0)
        guest_fn_cc768(EF_U32(record + 0xcu));
    if (EF_U32(record + 0x10u) != 0)
        guest_fn_cc768(EF_U32(record + 0x10u));
    if (EF_U32(record + 0x14u) != 0)
        guest_fn_cc768(EF_U32(record + 0x14u));
    if (EF_U32(record + 0x18u) != 0)
        guest_fn_cc768(EF_U32(record + 0x18u));
    guest_fn_cc768(record);
}

/* 0xb19a0, 66 bytes. Free a record with an owned block, a nested block and its own storage. */
void free_nested_record_b19a0(ef_addr32 record)
{
    ef_addr32 nested;

    if (record == 0)
        return;
    if (EF_U32(record + 0xfu) != 0)
        guest_fn_cc768(EF_U32(record + 0xfu));
    nested = EF_U32(record + 0x4du);
    if (nested != 0) {
        if (EF_U32(nested) != 0)
            guest_fn_cc768(EF_U32(nested));
        guest_fn_cc768(EF_U32(record + 0x4du));
    }
    guest_fn_cc768(record);
}

/* 0xb4e74, 79 bytes. Fill the 9-byte entry idx of the table at +0x4a of an object. */
void set_table_entry_b4e74(int16_t index, uint8_t byte_b, uint8_t byte_a, uint16_t word_b, uint32_t stack_word_a, ef_addr32 stack_object)
{
    uint32_t entry = (uint32_t)((int32_t)index * 9);

    EF_U8(EF_U32(stack_object + 0x4au) + entry + 3u) = byte_a;
    EF_U8(EF_U32(stack_object + 0x4au) + entry + 4u) = byte_b;
    EF_U16(EF_U32(stack_object + 0x4au) + entry + 5u) = (uint16_t)stack_word_a;
    EF_U16(EF_U32(stack_object + 0x4au) + entry + 7u) = word_b;
}

/* 0xb55f8, 69 bytes. Find the list node matching two key pairs and a kind byte. */
ef_addr32 find_node_b55f8(ef_addr32 list, ef_addr32 a, ef_addr32 b, uint8_t kind)
{
    ef_addr32 node = EF_U32(list);

    while (node != 0) {
        if (EF_U32(node + 9u) == EF_U32(a + 0x11u) && EF_U32(node + 0xdu) == EF_U32(a + 0x15u)
            && EF_U16(node + 0x15u) == EF_U16(b + 0x11u) && EF_U16(node + 0x17u) == EF_U16(b + 0x15u)
            && EF_U8(node + 0x1bu) == kind)
            return node;
        node = EF_U32(node + 4u);
    }
    return 0;
}
