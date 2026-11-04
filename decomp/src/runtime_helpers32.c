/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Named key table, widget drawing and scene group walk, 0xa6c20..0xaa350.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0xa6c20, 203 bytes. Draw a widget's text when its rectangle lies inside the clip box. */
void draw_widget_text_a6c20(ef_addr32 widget)
{
    char text[0x84];
    int32_t x = (EF_S32(0x0041ec00u) >> 16) + (EF_S32(widget + 0x2bu) >> 16);
    int32_t limit = (EF_S32(0x0041ec0cu) >> 16) + (EF_S32(0x0041ec08u) >> 16);
    int32_t y;
    int32_t ylimit;

    if (x >= limit || x + EF_U16(widget + 0x31u) < (EF_S32(0x0041ec08u) >> 16))
        return;
    y = (EF_S32(0x0041ebfeu) >> 16) + (EF_S32(widget + 0x29u) >> 16);
    ylimit = (EF_S32(0x0041ec0au) >> 16) + (EF_S32(0x0041ec06u) >> 16);
    if (y >= ylimit || y + EF_U16(widget + 0x2fu) < (EF_S32(0x0041ec06u) >> 16))
        return;
    copy_string_9d749((ef_addr32)(uintptr_t)text, EF_U32(widget + 0x27u));
    guest_fn_e26bc(0, (int16_t)(EF_U16(0x0041ec00u) + EF_U16(widget + 0x2bu)),
                   (int16_t)(EF_U16(0x0041ec02u) + EF_U16(widget + 0x2du)),
                   (ef_addr32)(uintptr_t)text, (uint32_t)(EF_U8(widget + 0x8bu) >> 4));
}

/* 0xa713c, 199 bytes. Draw a widget's frame rectangle when it is visible. */
void draw_widget_frame_a713c(ef_addr32 widget)
{
    ef_addr32 owner;
    int32_t x;
    int32_t y;
    int32_t w;
    uint32_t color;

    if ((EF_U8(widget + 0x3au) & 0x40) == 0 || (EF_U8(widget + 0x3au) & 1) == 0)
        return;
    if ((EF_S32(widget + 0x3fu) >> 16) + (EF_S32(widget + 0x43u) >> 16) > EF_S32(0x0015b744u))
        return;
    owner = find_widget_checked_a78bc(widget, 0x0018d1ecu);
    x = (int16_t)(EF_U16(widget + 0x41u) + EF_U16(owner + 4u));
    w = (int16_t)((uint16_t)(EF_U16(widget + 0x41u) + EF_U16(widget + 0x45u)) - (uint16_t)x);
    y = (int16_t)(EF_U16(widget + 0x43u) + 1);
    if (EF_U8(owner + 0x1du) & 1) {
        color = EF_U32(0x0041ec1cu);
    } else {
        color = EF_U32(0x0041ec24u);
        w = (int16_t)(w - 1);
    }
    guest_fn_c92e4(x, y, w, EF_S32(owner + 4u) >> 16, EF_S32(0x0015b742u) >> 16, x, y, color);
}

/* 0xa7510, 119 bytes. Store a name in a record, allocating its buffer on first use. */
void set_record_name_a7510(ef_addr32 record, ef_addr32 name)
{
    if (name == 0)
        name = 0x0018d1f4u;
    if (EF_U32(record + 0x27u) == 0) {
        EF_U16(record + 0x3du) = (uint16_t)guest_string_length(name);
        EF_U32(record + 0x27u) = alloc_zeroed_ab840((uint32_t)EF_U16(record + 0x3du) + 1u);
        if (EF_U32(record + 0x27u) == 0)
            guest_fatal_arg(0x0018d1f8u, name);
    } else if (guest_string_length(name) > (uint32_t)EF_U16(record + 0x3du)) {
        guest_fatal_arg3(0x0018d234u, record + 0x3fu, name, EF_U32(record + 0x27u));
    }
    copy_string_9d749(EF_U32(record + 0x27u), name);
}

/* 0xa7ec4, 158 bytes. Look up a key name; unknown names return a fallback slot. */
ef_addr32 lookup_key_a7ec4(ef_addr32 name)
{
    int16_t i = 0;

    guest_fn_eb478(name);
    copy_string_9d749(0x00419218u + (uint32_t)(EF_S32(0x004194c2u) >> 16) * 21u, name);
    while (guest_fn_a4380(name, 0x00419218u + (uint32_t)(int32_t)i * 21u) != 0)
        i++;
    if (i >= (int16_t)EF_U16(0x004194c4u)) {
        if (EF_U8(0x001371f9u) == 0)
            return 0x001371e8u;
        guest_fatal_arg(0x0018d584u, name);
    }
    if (EF_U8(0x0041948eu + (uint32_t)(int32_t)i) != 0)
        guest_fatal_arg(0x0018d5acu, name);
    return 0x00418708u + (uint32_t)(int32_t)i * 4u;
}

/* 0xa7f64, 173 bytes. Look up a key name that must exist and be of kind 1. */
ef_addr32 find_key_checked_a7f64(ef_addr32 name)
{
    char message[0x50];
    int16_t i = 0;

    guest_fn_eb478(name);
    while (guest_fn_a4380(name, 0x00419218u + (uint32_t)(int32_t)i * 21u) != 0
           && i < (int16_t)EF_U16(0x004194c4u))
        i++;
    if (i >= (int16_t)EF_U16(0x004194c4u)) {
        if (EF_U8(0x001371f9u) == 0)
            return 0x001371d8u;
        format_ui_status_value((ef_addr32)(uintptr_t)message, 0x0018d5dcu, name);
        ui_report_missing_record((ef_addr32)(uintptr_t)message);
    }
    if (EF_U8(0x0041948eu + (uint32_t)(int32_t)i) != 1) {
        format_ui_status_value((ef_addr32)(uintptr_t)message, 0x0018d604u, name);
        ui_report_missing_record((ef_addr32)(uintptr_t)message);
    }
    return 0x00418708u + (uint32_t)(int32_t)i * 4u;
}

/* 0xa9bfc, 209 bytes. Reset the key table state and its buffer pointers. */
void reset_key_tables_a9bfc(void)
{
    EF_U8(0x00137205u) = 0;
    EF_U8(0x001371fbu) = 0;
    EF_U32(0x00419214u) = 0;
    EF_U16(0x004194acu) = 0;
    EF_U8(0x001371edu) = 0;
    EF_U32(0x00417254u) = 0x0041952fu;
    EF_U32(0x00417258u) = 0x00419502u;
    EF_U32(0x0041725cu) = 0x00419511u;
    EF_U32(0x00417260u) = 0x004194e4u;
    EF_U16(0x004194c4u) = 0;
    EF_U32(0x00417250u) = 0x00419520u;
    EF_U32(0x00417264u) = 0x004194f3u;
    EF_U32(0x00417270u) = 0x0041953eu;
    EF_U32(0x00417274u) = 0x0041954du;
    EF_U32(0x00417268u) = 0x004194c6u;
    EF_U32(0x0041726cu) = 0x004194d5u;
    latch_b3f48();
    EF_U8(0x001371f9u) = return_true_3ce2c();
    EF_U8(0x001371eeu) = 0;
    EF_U8(0x001371ffu) = 0;
    EF_U8(0x00137200u) = 0;
    EF_U8(0x00137201u) = 0;
}

/* 0xaa350, 140 bytes. Walk every scene group and resolve its child nodes. */
void resolve_scene_groups_aa350(void)
{
    ef_addr32 group = EF_U32(0x00419200u);
    int16_t g;
    int16_t n;

    for (g = 0; g < (int16_t)EF_U16(0x004194c2u); g++) {
        ef_addr32 node = EF_U32(group + 8u);
        for (n = 0; n < (int16_t)EF_U16(group + 0xcu); n++) {
            uint8_t type = EF_U8(node);
            if (type == 3) {
                if (guest_fn_aa2f8(EF_U32(node + 9u)) != 0)
                    guest_report_parser_enum_error(0x0018d888u, (uint32_t)(int32_t)g, (uint32_t)(int32_t)n);
                resolve_child_aa2b4(EF_U32(node + 9u));
                guest_fn_aa198(EF_U32(node + 9u));
            } else if (type == 0xa) {
                guest_fn_aa1c0(EF_U32(node + 9u));
            }
            node = EF_U32(node + 1u);
        }
        group = EF_U32(group);
    }
}
