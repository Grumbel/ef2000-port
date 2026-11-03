/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small string, record-table and widget-list helpers, 0x9c3cf..0xa983c. */

/* 0x9c3cf, 60 bytes. Push a handler on the 32-entry stack at 0x40d030; -1 if full. */
int32_t push_handler_9c3cf(uint32_t handler)
{
    int32_t count = EF_S32(0x0040d0b0u);

    EF_U32(0x00160788u) = 0x9c398u;
    if (count >= 0x20)
        return -1;
    EF_U32(0x0040d030u + (uint32_t)count * 4u) = handler;
    EF_U32(0x0040d0b0u) = (uint32_t)count + 1u;
    return 0;
}

/* 0x9d749, 31 bytes. strcpy. */
void copy_string_9d749(ef_addr32 destination, ef_addr32 source)
{
    uint8_t c;

    do {
        c = EF_U8(source++);
        EF_U8(destination++) = c;
    } while (c != 0);
}

/* 0x9d7ac, 39 bytes. */
void set_axis_9d7ac(uint32_t a, uint32_t b)
{
    EF_U16(0x00417224u) = (uint16_t)b;
    EF_U16(0x00417228u) = (uint16_t)a;
    EF_U16(0x00417222u) = 0x4000;
    EF_U16(0x00417228u) = (uint16_t)(0u - a);
}

/* 0x9d80c, 37 bytes. */
void set_axis_9d80c(uint32_t a, uint32_t b)
{
    EF_U16(0x00417224u) = (uint16_t)b;
    EF_U16(0x00417228u) = (uint16_t)a;
    EF_U16(0x00417222u) = 0xc000;
    EF_U16(0x00417224u) = (uint16_t)(0u - b);
}

/* 0x9d938, 41 bytes. */
void reset_parser_state_9d938(void)
{
    EF_U8(0x00132b12u) = 0;
    EF_U32(0x00132b08u) = 0;
    EF_U32(0x00132b0cu) = 0;
    EF_U16(0x00132b10u) = 1;
}

/* 0x9d964, 42 bytes. Parse the decimal digits at text. */
uint32_t parse_decimal_9d964(ef_addr32 text)
{
    uint32_t previous = 0x30;
    uint32_t value = 0;
    uint32_t current;

    do {
        value = value * 10u + previous - 0x30u;
        current = EF_U8(text++);
        previous = current;
    } while ((int32_t)current >= 0x30 && (int32_t)current <= 0x39);
    return value;
}

/* 0x9da50, 47 bytes. Skip to the end of the current line and past the line break. */
ef_addr32 skip_line_9da50(ef_addr32 text)
{
    if (EF_U8(text) != 0) {
        for (;;) {
            uint32_t c = EF_U8(text);
            uint8_t next;

            if (c == 0xd || c == 0xa)
                break;
            next = EF_U8(text + 1u);
            text++;
            if (next == 0)
                break;
        }
    }
    while (EF_U8(text) == 0xd || EF_U8(text) == 0xa)
        text++;
    return text;
}

/* 0xa0da8, 54 bytes. Address of 16-bit slot index in the current page record. */
ef_addr32 page_slot_a0da8(int16_t index)
{
    uint32_t page = EF_U8(EF_U32(0x0041717cu) + 1u);

    return EF_U32(0x00417174u) + 0xbu + page * 29u + (uint32_t)(int32_t)index * 2u;
}

/* 0xa0de0, 68 bytes. Select page record and return the address of slot index. */
ef_addr32 select_page_slot_a0de0(int16_t page_id, int16_t index)
{
    ef_addr32 record = guest_fn_b3f58(page_id);
    ef_addr32 type = EF_U32(0x0040d200u + (uint32_t)EF_U16(record) * 4u);
    uint32_t page = EF_U8(type + 1u);

    EF_U32(0x00417174u) = record;
    EF_U32(0x0041717cu) = type;
    return record + 0xbu + page * 29u + (uint32_t)(int32_t)index * 2u;
}

/* 0xa0f4c, 56 bytes. Clear slot index of the current page. */
void clear_page_slot_a0f4c(int16_t index)
{
    uint32_t page = EF_U8(EF_U32(0x0041717cu) + 1u);

    EF_U16(EF_U32(0x00417174u) + 0xbu + page * 29u + (uint32_t)(int32_t)index * 2u) = 0;
}

/* 0xa14c8, 55 bytes. Set the high flag of slot index of the current page. */
void flag_page_slot_a14c8(int16_t index)
{
    uint32_t page = EF_U8(EF_U32(0x0041717cu) + 1u);

    EF_U8(EF_U32(0x00417174u) + 0xbu + page * 29u + (uint32_t)(int32_t)index * 2u + 1u) |= 0x80u;
}

/* 0xa17e8, 58 bytes. Value of the type byte (bits 2..7) if bit 1 is set, else -1. */
int32_t record_type_value_a17e8(int16_t id)
{
    ef_addr32 record = guest_fn_b3f58(id);
    ef_addr32 type = EF_U32(0x0040d200u + (uint32_t)EF_U16(record) * 4u);

    if ((EF_U8(type + 4u) & 2u) == 0)
        return -1;
    return (int32_t)((EF_U8(type + 4u) & 0xfcu) >> 2);
}

/* 0xa1824, 52 bytes. Copy the header fields of record id into dest. */
void copy_record_header_a1824(ef_addr32 destination, int16_t id)
{
    ef_addr32 record = guest_fn_b3f58(id);

    EF_U16(destination) = (uint16_t)(((uint8_t)(EF_U8(record + 0xau) * 2u)) >> 6);
    EF_U16(destination + 2u) = EF_U16(record + 2u);
    EF_U16(destination + 4u) = EF_U16(record + 4u);
    EF_U16(destination + 6u) = EF_U16(record + 6u);
}

/* 0xa6998, 53 bytes. Refresh every child of a widget that is not of kind 9. */
void refresh_children_a6998(ef_addr32 widget)
{
    ef_addr32 child;

    for (child = EF_U32(widget + 0x91u); child != 0; child = EF_U32(child + 0x21u)) {
        if ((EF_U8(child + 0x8cu) & 0x1fu) != 9u)
            guest_fn_a66cc(child, 1);
        guest_fn_a6808(child);
    }
}

/* 0xa7104, 54 bytes. Redraw the kind 1..3 children of a widget, stopping at the first kind 0. */
void redraw_children_a7104(ef_addr32 widget)
{
    ef_addr32 child;

    for (child = EF_U32(widget + 0x91u); child != 0; child = EF_U32(child + 0x21u)) {
        uint8_t kind = EF_U8(child + 0x8cu) & 0x1fu;

        if (kind == 0)
            return;
        if (kind == 1 || kind == 2 || kind == 3)
            guest_fn_a6d00(child);
    }
}

/* 0xa7260, 57 bytes. Redraw all children of a widget (both original branches are identical). */
void redraw_all_children_a7260(ef_addr32 widget)
{
    ef_addr32 child;

    for (child = EF_U32(widget + 0x91u); child != 0; child = EF_U32(child + 0x21u))
        guest_fn_a6d00(child);
}

/* 0xa729c, 60 bytes. Call a7380 on enabled siblings of the same group as other. */
void notify_group_a729c(ef_addr32 widget, ef_addr32 other)
{
    ef_addr32 child;

    for (child = EF_U32(widget + 0x91u); child != 0; child = EF_U32(child + 0x21u)) {
        if ((EF_U8(child + 0x85u) & 0x1fu) == (EF_U8(other + 0x85u) & 0x1fu) &&
            child != other && (EF_U8(child + 0x1du) & 1u) != 0)
            guest_fn_a7380(child);
    }
}

/* 0xa72d8, 61 bytes. Enable a widget, notifying its group. */
void enable_widget_a72d8(ef_addr32 widget)
{
    if (widget == 0 || (EF_U8(widget + 0x1du) & 1u) != 0)
        return;
    if ((EF_U8(widget + 0x85u) & 0x1fu) != 0)
        notify_group_a729c(EF_U32(0x0041edfcu + (uint32_t)EF_U8(widget + 0x50u) * 4u), widget);
    EF_U8(widget + 0x1du) |= 1u;
    guest_fn_a6d00(widget);
}

/* 0xa7e4c, 60 bytes. Copy table 0x418708 to 0x418690 for unlocked entries. */
void save_table_a7e4c(void)
{
    int16_t count = EF_S16(0x004194c4u);
    int16_t i;

    for (i = 0; i < count; i++) {
        if (EF_U8(0x0041948eu + (uint32_t)(int32_t)i) == 0)
            EF_U32(0x00418690u + (uint32_t)(int32_t)i * 4u) =
                EF_U32(0x00418708u + (uint32_t)(int32_t)i * 4u);
    }
    EF_U16(0x004194c4u) = (uint16_t)count;
}

/* 0xa7e88, 60 bytes. Copy table 0x418690 back to 0x418708 for unlocked entries. */
void restore_table_a7e88(void)
{
    int16_t count = EF_S16(0x004194c4u);
    int16_t i;

    for (i = 0; i < count; i++) {
        if (EF_U8(0x0041948eu + (uint32_t)(int32_t)i) == 0)
            EF_U32(0x00418708u + (uint32_t)(int32_t)i * 4u) =
                EF_U32(0x00418690u + (uint32_t)(int32_t)i * 4u);
    }
    EF_U16(0x004194c4u) = (uint16_t)count;
}

/* 0xa983c, 60 bytes. Apply a set/add/subtract operation to a guest value. */
void apply_operation_a983c(ef_addr32 op)
{
    ef_addr32 target = guest_fn_a7ec4(EF_U32(op) + 9u);

    switch (EF_U8(op + 4u)) {
    case 0:
        EF_U32(target) = EF_U32(op + 5u);
        break;
    case 1:
        EF_U32(target) += EF_U32(op + 5u);
        break;
    case 2:
        EF_U32(target) -= EF_U32(op + 5u);
        break;
    default:
        break;
    }
}
