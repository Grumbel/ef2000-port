/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0x815f4, 22 bytes. When byte 0x12b104 is set tail-call 0xd72b4(AL), else 0. */
uint32_t forward_if_flag_12b104(uint8_t value)
{
    if (EF_U8(0x0012b104u) != 0)
        return guest_fn_d72b4(value);
    return 0;
}

/* 0x83734, 35 bytes. Call 0xe99b8, then arithmetic-shift three dwords of the
 * vector at EAX right by 16 in place. EBX, ECX and EDX are clobbered. */
void scale_vector_down_16(ef_addr32 vector)
{
    guest_fn_e99b8(vector);
    EF_S32(vector) >>= 16;
    EF_S32(vector + 4u) >>= 16;
    EF_S32(vector + 8u) >>= 16;
}

/* 0x84394, 25 bytes. Pass dwords 0/4/8 of the vector to 0x83708(0x1fcc3e, ...). */
void forward_vector_to_83708(ef_addr32 vector)
{
    guest_fn_83708(0x001fcc3eu, EF_U32(vector), EF_U32(vector + 4u), EF_U32(vector + 8u));
}

/* 0x844e4, 47 bytes. Word 0x12b4a6 = 1; 0xdc832(0x1fcc2c, 0x1fcbc0, 0xc);
 * byte 0x1fcc43 = 0; returns AL = 0. */
uint8_t reset_pending_mode_844e4(void)
{
    EF_U16(0x0012b4a6u) = 1;
    guest_fn_dc832(0x001fcc2cu, 0x001fcbc0u, 0x0c);
    EF_U8(0x001fcc43u) = 0;
    return 0;
}

/* 0x88548, 42 bytes. Dword 0x20f14c = 0x7fffffff, 0x20f154 = 0, and 0x20f150
 * = filtered lookup of the signed high word of dword 0x1f71c8. */
void reset_target_search(void)
{
    EF_U32(0x0020f14cu) = 0x7fffffffu;
    EF_U32(0x0020f154u) = 0;
    EF_U32(0x0020f150u) = lookup_filtered_indexed_object((int16_t)(EF_S32(0x001f71c8u) >> 16));
}

/* 0x88d08, 37 bytes. Copy six dwords from EDX to EAX. */
void copy_24_bytes(ef_addr32 destination, ef_addr32 source)
{
    uint32_t i;
    for (i = 0; i < 6; ++i)
        EF_U32(destination + i * 4u) = EF_U32(source + i * 4u);
}

/* 0x89960, 48 bytes. Clear byte 0x12d7b3; if byte 0x125847 is set call
 * 0x9e508(high word of dword +0x2c, 4); then 0x8a318(record). */
void finish_record_89960(ef_addr32 record)
{
    EF_U8(0x0012d7b3u) = 0;
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S32(record + 0x2cu) >> 16, 4);
    guest_fn_8a318(record);
}

/* 0x8b530, 38 bytes. Copy 0x8e dwords from the filtered object selected by
 * the signed high word of 0x1f71c8 to 0x2117b0. */
void copy_selected_object(void)
{
    ef_addr32 object = lookup_filtered_indexed_object((int16_t)(EF_S32(0x001f71c8u) >> 16));
    if (object != 0) {
        uint32_t i;
        for (i = 0; i < 0x8e; ++i)
            EF_U32(0x002117b0u + i * 4u) = EF_U32(object + i * 4u);
    }
}

/* 0x8cd48, 46 bytes. Word 0x12db60 = 1; 0x8c0f0(0) returns a signed word in
 * AX; unless it is -1, store it at word 0x1f71d0 and at +0x16 of the object
 * returned by 0x8c0bc(sign-extended AX). */
void select_object_8cd48(void)
{
    int16_t value;
    EF_U16(0x0012db60u) = 1;
    value = (int16_t)guest_fn_8c0f0(0);
    if ((int32_t)value != -1) {
        ef_addr32 object = guest_fn_8c0bc((int32_t)value);
        EF_U16(0x001f71d0u) = (uint16_t)value;
        EF_U16(object + 0x16u) = (uint16_t)value;
    }
}

/* 0x8fe30, 39 bytes. Fill two blocks through 0xb3fb0(dest, 0, count). */
void clear_blocks_8fe30(void)
{
    guest_fn_b3fb0(0x00211ac8u, 0, 4);
    guest_fn_b3fb0(0x00211a00u, 0, 0xc8);
}

/* 0x94194, 39 bytes. Clear words 0x12dba0/0x12dba6; run 0x91df4, 0x9202c,
 * 0x93a8c, then request a scene refresh. */
void reset_scene_update_94194(void)
{
    EF_U16(0x0012dba0u) = 0;
    EF_U16(0x0012dba6u) = 0;
    guest_fn_91df4();
    guest_fn_9202c();
    guest_fn_93a8c();
    request_scene_refresh();
}

/* 0x941bc, 39 bytes. Identical body to 0x94194 at a second entry point. */
void reset_scene_update_941bc(void)
{
    EF_U16(0x0012dba0u) = 0;
    EF_U16(0x0012dba6u) = 0;
    guest_fn_91df4();
    guest_fn_9202c();
    guest_fn_93a8c();
    request_scene_refresh();
}

/* 0x978e8, 44 bytes. If word +0xa is set: when the signed high word of
 * dword +6 is at most 2, run 0x979c8 and 0x97914; always run 0x97950. */
void update_record_978e8(ef_addr32 record)
{
    if (EF_U16(record + 0xau) != 0) {
        if ((EF_S32(record + 6u) >> 16) <= 2) {
            guest_fn_979c8(record);
            guest_fn_97914(record);
        }
        guest_fn_97950(record);
    }
}

/* 0x97d90, 48 bytes. 0xe30e0 at (0, x+1, y+1) and (6, x, y), with ECX = EBX. */
void draw_pair_97d90(int16_t x, int16_t y, uint32_t extra)
{
    guest_fn_e30e0(0, (int32_t)x + 1, (int32_t)y + 1, extra);
    guest_fn_e30e0(6, (int32_t)x, (int32_t)y, extra);
}

/* 0x984b4, 39 bytes. Call 0xde4b0 when byte 0x420a72 is 2; dword 0x12dcfc =
 * 0xffffffff and byte 0x12dd00 = 1. */
void reset_sound_state_984b4(void)
{
    if (EF_U8(0x00420a72u) == 2)
        guest_fn_de4b0();
    EF_U32(0x0012dcfcu) = 0xffffffffu;
    EF_U8(0x0012dd00u) = 1;
}

/* 0x982d4, 43 bytes. When byte 0x420a70 is 2, 0x420a72 is set and 0x426147
 * is clear, tail-call 0xde3b4 with the signed high word of dword 0x12dcf6. */
void forward_sound_state_982d4(void)
{
    if (EF_U8(0x00420a70u) == 2 && EF_U8(0x00420a72u) != 0 && EF_U8(0x00426147u) == 0)
        guest_fn_de3b4(EF_S32(0x0012dcf6u) >> 16);
}

/* 0xa0e98, 16 bytes. Bit 4 of byte +0xa of the record from 0xb3f58. */
uint8_t record_flag_bit4(int16_t index)
{
    ef_addr32 record = guest_fn_b3f58((int32_t)index);
    return (uint8_t)((EF_U8(record + 0xau) >> 4) & 1u);
}

/* 0xa0ea8, 19 bytes. Words +2, +4, +6 of the 0xb3f58 record = DX, BX, CX. */
void set_record_words(int16_t index, uint16_t first, uint16_t second, uint16_t third)
{
    ef_addr32 record = guest_fn_b3f58((int32_t)index);
    EF_U16(record + 2u) = first;
    EF_U16(record + 4u) = second;
    EF_U16(record + 6u) = third;
}

/* 0xa1858, 17 bytes. Bits 5..6 of byte +0xa of the 0xb3f58 record. */
uint8_t record_flag_bits56(int16_t index)
{
    ef_addr32 record = guest_fn_b3f58((int32_t)index);
    return (uint8_t)((EF_U8(record + 0xau) >> 5) & 3u);
}

/* 0xa1a10, 47 bytes. Resolve the record's word +0 through the pointer table
 * at 0x40d200 and return that entry's +0xa plus 6 times its byte +9. */
ef_addr32 record_entry_address(int16_t index)
{
    ef_addr32 record = guest_fn_b3f58((int32_t)index);
    ef_addr32 entry = EF_U32(0x0040d200u + (uint32_t)EF_U16(record) * 4u);
    return entry + 0xau + (uint32_t)EF_U8(entry + 9u) * 6u;
}

/* 0xa2e4c, 29 bytes. 0xb3fb0(dword 0x1368bc[AX], 0, 0x21). */
void clear_entry_a2e4c(uint16_t index)
{
    guest_fn_b3fb0(EF_U32(0x001368bcu + (uint32_t)index * 4u), 0, 0x21);
}

/* 0xa2e6c, 33 bytes. Clear entries 0..7. */
void clear_all_entries_a2e6c(void)
{
    uint16_t i = 0;
    while (i < 8) {
        clear_entry_a2e4c(i);
        ++i;
    }
}

/* 0xa2e90, 32 bytes. 0xe9a24(0x21, 0x135840, dword 0x1368bc[AX]); callee pops 12. */
void format_entry_a2e90(uint16_t index)
{
    guest_fn_e9a24(0x21, 0x00135840u, EF_U32(0x001368bcu + (uint32_t)index * 4u));
}

/* 0xa3108, 33 bytes. When byte 0x13579a is zero tail-call 0xec0ec with the
 * two low words, else return 0 (AL). */
uint32_t forward_if_clear_a3108(uint16_t first, uint16_t second)
{
    if (EF_U8(0x0013579au) == 0)
        return guest_fn_ec0ec(first, second);
    return 0;
}

/* 0xa38c8, 48 bytes. Clear words at 0x1357d8+2i and 0x13580a+2i and dwords
 * at 0x1357e8+4i for i = 1..8. */
void clear_tables_a38c8(void)
{
    uint32_t i;
    for (i = 1; i <= 8; ++i) {
        EF_U16(0x001357d8u + i * 2u) = 0;
        EF_U16(0x0013580au + i * 2u) = 0;
        EF_U32(0x001357e8u + i * 4u) = 0;
    }
}

/* 0xa3a10, 24 bytes. Nonempty string whose 0xb1b8b result is at most 0x10. */
uint8_t string_short_enough(ef_addr32 text)
{
    if (EF_U8(text) == 0)
        return 0;
    return (uint8_t)(guest_fn_b1b8b(text) <= 0x10u);
}

/* 0xa3b48, 25 bytes. 0xdeb87(0x136dd8, 0xffffffff, 8). */
void fill_block_a3b48(void)
{
    guest_fn_deb87(0x00136dd8u, 0xffffffffu, 8);
}

/* 0xa3bfc, 27 bytes. Pointer table 0x1368bc[i] = 0x136dfc + 0x21*i, i = 0..7. */
void init_entry_table_a3bfc(void)
{
    uint32_t i;
    for (i = 0; i < 8; ++i)
        EF_U32(0x001368bcu + i * 4u) = 0x00136dfcu + i * 0x21u;
}

/* 0xa4318, 23 bytes. Clear dwords 0x135820, 0x135824 and 0x135828. */
void clear_dwords_135820(void)
{
    EF_U32(0x00135824u) = 0;
    EF_U32(0x00135820u) = 0;
    EF_U32(0x00135828u) = 0;
}

/* 0xa4421, 31 bytes. Uppercase ASCII a..z in place; returns the pointer. */
ef_addr32 uppercase_string(ef_addr32 text)
{
    ef_addr32 p = text;
    while (EF_U8(p) != 0) {
        uint8_t c = EF_U8(p);
        if ((uint8_t)(c - 0x61u) <= 0x19u)
            EF_U8(p) = (uint8_t)(c - 0x20u);
        ++p;
    }
    return text;
}

/* 0xa4b5c, 36 bytes. When byte +0x11 is clear run 0xa4a94 and 0xa4a20 on
 * byte +0x10, then 0xa6038. */
void refresh_widget_a4b5c(ef_addr32 widget)
{
    if (EF_U8(widget + 0x11u) == 0) {
        guest_fn_a4a94(EF_U8(widget + 0x10u));
        guest_fn_a4a20(EF_U8(widget + 0x10u));
        guest_fn_a6038();
    }
}

/* 0xa4c84, 45 bytes. Call 0xa4b80(i) for i = 1..word 0x41eed8. */
void refresh_all_widgets_a4c84(void)
{
    uint32_t i = 1;
    while (i <= EF_U16(0x0041eed8u)) {
        guest_fn_a4b80(i);
        ++i;
    }
}

/* 0xa4e98, 40 bytes. Clear 41 dwords at 0x41edfc. */
void clear_widget_table_a4e98(void)
{
    uint32_t i;
    for (i = 0; i <= 0x28; ++i)
        EF_U32(0x0041edfcu + i * 4u) = 0;
}

/* 0xa4ec0, 43 bytes. Initialise margin words from flag bit 0 of byte +0x3a. */
void init_widget_margins(ef_addr32 widget)
{
    uint32_t base = (EF_U8(widget + 0x3au) & 1u) ? 0x12u : 0u;
    EF_U16(widget + 0x5fu) = 4;
    EF_U16(widget + 0x63u) = 2;
    EF_U16(widget + 0x61u) = 4;
    EF_U16(widget + 0x5du) = (uint16_t)(base + 2u);
}

/* 0xa51dc, 48 bytes. For widgets with flag 1, look up the child with 0xa78bc
 * (name 0x18cff8) and set its word +4 to word +0x45 (minus 0x44 with flag 0x40). */
void update_widget_child_a51dc(ef_addr32 widget)
{
    if (EF_U8(widget + 0x3au) & 1u) {
        ef_addr32 child = guest_select_dialog_widget(widget, 0x0018cff8u);
        uint16_t value = EF_U16(widget + 0x45u);
        if (EF_U8(widget + 0x3au) & 0x40u)
            value = (uint16_t)(value - 0x44u);
        EF_U16(child + 4u) = value;
    }
}

/* 0xa54f0, 22 bytes. 0xa5484(widget, high word of +0x3f, high word of +0x41). */
void forward_widget_position(ef_addr32 widget)
{
    guest_fn_a5484(widget, EF_S32(widget + 0x3fu) >> 16, EF_S32(widget + 0x41u) >> 16);
}

/* 0xa56f8, 17 bytes. Clear words +0x69 and +0x6b, tail-call 0xf13ac. */
void clear_widget_words_a56f8(ef_addr32 widget)
{
    EF_U16(widget + 0x69u) = 0;
    EF_U16(widget + 0x6bu) = 0;
    guest_fn_f13ac(widget);
}

/* 0xa5980, 44 bytes. 0xa5764(dword 0x41edfc[i]) for i = 1..word 0x41eed8. */
void update_all_widgets_a5980(void)
{
    uint16_t i = 1;
    while (i <= EF_U16(0x0041eed8u)) {
        guest_fn_a5764(EF_U32(0x0041edfcu + (uint32_t)i * 4u));
        ++i;
    }
}

/* 0xa59ac, 41 bytes. Without flag 2 on byte +0x3a, find the child via 0xa788c
 * (name 0x18cff8) and clear its words +0xe, +0x10 and +0x12. */
void clear_child_words_a59ac(ef_addr32 widget)
{
    if ((EF_U8(widget + 0x3au) & 2u) == 0) {
        ef_addr32 child = guest_fn_a788c(widget, 0x0018cff8u);
        if (child != 0) {
            EF_U16(child + 0xeu) = 0;
            EF_U16(child + 0x10u) = 0;
            EF_U16(child + 0x12u) = 0;
        }
    }
}

/* 0xa66a0, 44 bytes. Walk the child list (head +0x91, next +0x21); for
 * children with any of the low five bits of byte +0x8c call 0xa6438(parent, child). */
void walk_children_a66a0(ef_addr32 parent)
{
    ef_addr32 child = EF_U32(parent + 0x91u);
    while (child != 0) {
        if (EF_U8(child + 0x8cu) & 0x1fu)
            guest_fn_a6438(parent, child);
        child = EF_U32(child + 0x21u);
    }
}

/* 0xa6a64, 44 bytes. As 0xa66a0 for children without those bits, via 0xa69d0. */
void walk_children_a6a64(ef_addr32 parent)
{
    ef_addr32 child = EF_U32(parent + 0x91u);
    while (child != 0) {
        if ((EF_U8(child + 0x8cu) & 0x1fu) == 0)
            guest_fn_a69d0(parent, child);
        child = EF_U32(child + 0x21u);
    }
}

/* 0xa7380, 28 bytes. When non-null with bit 0 of byte +0x1d, clear that bit
 * and call 0xa6d00(widget). */
void clear_widget_dirty(ef_addr32 widget)
{
    if (widget != 0 && (EF_U8(widget + 0x1du) & 1u)) {
        EF_U8(widget + 0x1du) = (uint8_t)(EF_U8(widget + 0x1du) & 0xfeu);
        guest_fn_a6d00(widget);
    }
}

/* 0xa739c, 48 bytes. Set bit 6 of byte +0x1d from DL bit 0; unless byte
 * 0x41f2e5 is set call 0xa5ef8(byte +0x50). */
void set_widget_bit6(ef_addr32 widget, uint8_t value)
{
    if (widget != 0) {
        uint8_t flags = (uint8_t)((EF_U8(widget + 0x1du) & 0xbfu) | ((value & 1u) << 6));
        EF_U8(widget + 0x1du) = flags;
        if (EF_U8(0x0041f2e5u) == 0)
            guest_fn_a5ef8(EF_U8(widget + 0x50u));
    }
}

/* 0xa7408, 44 bytes. For each child without the low five bits of +0x8c call
 * 0xa739c(child, DL). */
void set_children_bit6(ef_addr32 parent, uint8_t value)
{
    ef_addr32 child = EF_U32(parent + 0x91u);
    while (child != 0) {
        if ((EF_U8(child + 0x8cu) & 0x1fu) == 0)
            set_widget_bit6(child, value);
        child = EF_U32(child + 0x21u);
    }
}

/* 0xa7434, 37 bytes. Clear the dirty bit on every child with bit 0 of +0x1d. */
void clear_children_dirty(ef_addr32 parent)
{
    if (parent != 0) {
        ef_addr32 child = EF_U32(parent + 0x91u);
        while (child != 0) {
            if (EF_U8(child + 0x1du) & 1u)
                clear_widget_dirty(child);
            child = EF_U32(child + 0x21u);
        }
    }
}

/* 0xa7b80, 10 bytes. Tail-call 0x9c858 with EAX = 0x18d57c. */
uint32_t call_9c858_18d57c(void)
{
    return guest_fn_9c858_arg(0x0018d57cu);
}

/* 0xa7b8c, 11 bytes. Dword 0x4191c8 = result of 0xb3f48. */
void latch_b3f48(void)
{
    EF_U32(0x004191c8u) = guest_fn_b3f48();
}
