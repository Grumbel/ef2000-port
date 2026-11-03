/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0x41ea0, 38 bytes. AL is 0, 1, 5, 6 or 7. */
uint32_t code_is_0_1_5_6_7(uint8_t code)
{
    return code == 0 || code == 1 || code == 5 || code == 6 || code == 7;
}

/* 0x41f00, 34 bytes. AL is 2, 3, 0x18 or 0x19. */
uint32_t code_is_2_3_18_19(uint8_t code)
{
    return code == 3 || code == 0x18 || code == 2 || code == 0x19;
}

/* 0x47ee4, 22 bytes. Byte 0x126654 = 0, dword 0x126688 = 0x12665c. */
void reset_pointer_126688(void)
{
    EF_U8(0x00126654u) = 0;
    EF_U32(0x00126688u) = 0x0012665cu;
}

/* 0x47f9c, 29 bytes. Run 0x46070; clear byte 0x126655 when the signed
 * high word of dword 0x12668e equals 0xff. */
void sync_scene_marker(void)
{
    guest_fn_46070();
    if ((EF_S32(0x0012668eu) >> 16) == 0xff)
        EF_U8(0x00126655u) = 0;
}

/* 0x50d9c, 41 bytes. Pass each nonzero byte of the string at 0x1a5968
 * through 0xe7835 in place until the terminating zero. */
void transform_string_1a5968(void)
{
    uint16_t i = 0;
    for (;;) {
        int16_t index = (int16_t)i;
        ef_addr32 address = 0x001a5968u + (uint32_t)(int32_t)index;
        if (EF_U8(address) == 0)
            break;
        ++i;
        EF_U8(address) = (uint8_t)guest_fn_e7835(EF_U8(address));
    }
}

/* 0x55084, 43 bytes. If the filtered object at signed word +1 exists and
 * byte +3 equals the grid marker, set the object's word +0x18 to the marker. */
void mark_filtered_object(ef_addr32 record)
{
    ef_addr32 object = lookup_filtered_indexed_object(EF_S16(record + 1u));
    if (object != 0 && g_world_grid_marker == EF_U8(record + 3u))
        EF_U16(object + 0x18u) = g_world_grid_marker;
}

/* 0x5511c, 38 bytes. Marshal record fields to 0x46454 (callee pops the two
 * stack dwords): EAX signed word +1, EDX signed high word of dword +1,
 * EBX byte +5, ECX dword +6, then dwords +0xa and +0xe on the stack. */
void forward_record_to_46454(ef_addr32 record)
{
    guest_fn_46454(EF_S16(record + 1u), EF_S32(record + 1u) >> 16, EF_U8(record + 5u),
                   EF_U32(record + 6u), EF_U32(record + 0xau), EF_U32(record + 0xeu));
}

/* 0x55884, 43 bytes. When byte 0x1f9012 is zero set byte 0 of the 23-byte
 * activity record chosen by record byte +1; always call 0x544c0. */
void set_activity_record_flag(ef_addr32 record)
{
    if (EF_U8(0x001f9012u) == 0)
        EF_U8(0x001a5c32u + (uint32_t)EF_U8(record + 1u) * 23u) = 1;
    guest_refresh_activity_records();
}

/* 0x56928, 44 bytes. Copy dwords +1/+5/+9 to a local triple and pass it with
 * the signed high word of dword +0xd to 0x1d5f4. */
void dispatch_event_triple(ef_addr32 record)
{
    uint32_t triple[3];
    triple[0] = EF_U32(record + 1u);
    triple[1] = EF_U32(record + 5u);
    triple[2] = EF_U32(record + 9u);
    guest_dispatch_event_triple(triple, EF_S32(record + 0xdu) >> 16);
}

/* 0x56c90, 42 bytes. Packet 4e plus marker as a zero-extended word, then 0xa3f30. */
void send_marker_word_packet_4e(void)
{
    uint8_t bytes[3];
    bytes[0] = 0x4e;
    bytes[1] = g_world_grid_marker;
    bytes[2] = 0;
    emit_command_packet(bytes, sizeof(bytes));
    guest_fn_a3f30();
}

/* 0x56ed0, 42 bytes. As 0x56c90 with opcode 0x33. */
void send_marker_word_packet_33(void)
{
    uint8_t bytes[3];
    bytes[0] = 0x33;
    bytes[1] = g_world_grid_marker;
    bytes[2] = 0;
    emit_command_packet(bytes, sizeof(bytes));
    guest_fn_a3f30();
}

/* 0x56d68, 42 bytes. When byte 0x1357c8 is set, clear bit (record byte +1)
 * of word 0x1a662e. The shift count is masked to five bits as by SHL. */
void clear_flag_bit_1a662e(ef_addr32 record)
{
    if (EF_U8(0x001357c8u) != 0) {
        uint32_t bit = 1u << (EF_U8(record + 1u) & 31u);
        EF_U16(0x001a662eu) = (uint16_t)(EF_U16(0x001a662eu) & ~bit);
    }
}

/* 0x56fa8, 35 bytes. Copy words +1, +3, +5 to 0x12583c, 0x12583e, 0x125840. */
void copy_record_words_12583c(ef_addr32 record)
{
    EF_U16(0x0012583cu) = EF_U16(record + 1u);
    EF_U16(0x0012583eu) = EF_U16(record + 3u);
    EF_U16(0x00125840u) = EF_U16(record + 5u);
}

/* 0x56fe8, 17 bytes. If the grid marker is zero set byte 0x1264e1 to 1. */
void flag_marker_zero(void)
{
    if (g_world_grid_marker == 0)
        EF_U8(0x001264e1u) = 1;
}

/* 0x57670, 18 bytes. Filtered lookup of signed word +1; if found, tail-call
 * 0x741f0 with the object, else return 0. */
uint32_t forward_filtered_object(ef_addr32 record)
{
    ef_addr32 object = lookup_filtered_indexed_object(EF_S16(record + 1u));
    if (object == 0)
        return 0;
    return guest_fn_741f0(object);
}

/* 0x5f8a8, 44 bytes. Signed high word of dword 0x1f71c8 selects a filtered
 * object; if found call 0x9e508(high word of its dword +0x1e, 0x24). Clear
 * byte 0x1a6d18 on all paths. */
void notify_selected_object(void)
{
    ef_addr32 object = lookup_filtered_indexed_object((int16_t)(EF_S32(0x001f71c8u) >> 16));
    if (object != 0)
        guest_fn_9e508(EF_S32(object + 0x1eu) >> 16, 0x24);
    EF_U8(0x001a6d18u) = 0;
}

/* 0x65498, 14 bytes. Dword +0x16 of the record, or dword 0x1a7c54 when null. */
uint32_t record_field_16_or_default(ef_addr32 record)
{
    if (record == 0)
        return EF_U32(0x001a7c54u);
    return EF_U32(record + 0x16u);
}

/* 0x70418, 31 bytes. Pass (byte +0x18c of the object at 0x1f71bc, plus 2,
 * low byte) and 3 to 0x6e390 and return its EAX. */
uint32_t forward_object_level(void)
{
    uint8_t level = (uint8_t)(EF_U8(EF_U32(0x001f71bcu) + 0x18cu) + 2u);
    return guest_fn_6e390(level, 3);
}

/* 0x757b0, 10 bytes. Zero becomes 1. */
uint32_t nonzero_or_one(uint32_t value)
{
    return value == 0 ? 1u : value;
}

/* 0x77950, 41 bytes. 0xa1a40 gives a base; for a nonzero, non-0xffff word
 * in DX return 0xa1a90(sign-extended DX, base), else 0. */
uint32_t lookup_by_word_checked(uint16_t word)
{
    uint32_t base = guest_fn_a1a40();
    if (base != 0 && word != 0 && word != 0xffffu)
        return guest_fn_a1a90((int32_t)(int16_t)word, base);
    return 0;
}

/* 0x80d20, 30 bytes. As 0x77950 without the zero and 0xffff exclusions. */
uint32_t lookup_by_word(int16_t word)
{
    uint32_t base = guest_fn_a1a40();
    if (base == 0)
        return 0;
    return guest_fn_a1a90((int32_t)word, base);
}

/* The five functions below index a dword table with the 16-bit selector word
 * divided by a constant. */
/* 0x77b20, 36 bytes. */
uint32_t random_table_77b20(void) { return EF_U32(0x0012a934u + (uint32_t)(guest_read_selector_word() / 0x666u) * 4u); }
/* 0x77b44, 36 bytes. */
uint32_t random_table_77b44(void) { return EF_U32(0x0012a9d4u + (uint32_t)(guest_read_selector_word() / 0x333u) * 4u); }
/* 0x77b68, 36 bytes. */
uint32_t random_table_77b68(void) { return EF_U32(0x0012ab14u + (uint32_t)(guest_read_selector_word() / 0x1c3u) * 4u); }
/* 0x77b8c, 36 bytes. */
uint32_t random_table_77b8c(void) { return EF_U32(0x0012ad58u + (uint32_t)(guest_read_selector_word() / 0x1745u) * 4u); }
/* 0x77bb0, 36 bytes. */
uint32_t random_table_77bb0(void) { return EF_U32(0x0012ad84u + (uint32_t)(guest_read_selector_word() / 0x3fffu) * 4u); }

/* 0x77c18, 15 bytes. Copy dword 0x126694 to 0x12adf8, then tail into 0x47f9c. */
void copy_and_sync_scene_marker(void)
{
    EF_U32(0x0012adf8u) = EF_U32(0x00126694u);
    sync_scene_marker();
}

/* 0x7a298, 10 bytes. Tail-call 0x79ff0 with EAX = 0x12af60. */
uint32_t call_79ff0_12af60(void)
{
    return guest_fn_79ff0(0x0012af60u);
}

/* 0x7a454, 44 bytes. Call 0x7a9bc if dword 0x12afd0 is set; clear
 * 0x1f8ea0 and 0x12afd0 and set 0x12afc0 to 0xffffffff. */
void reset_sound_state_7a454(void)
{
    if (EF_U32(0x0012afd0u) != 0)
        guest_fn_7a9bc();
    EF_U32(0x001f8ea0u) = 0;
    EF_U32(0x0012afd0u) = 0;
    EF_U32(0x0012afc0u) = 0xffffffffu;
}

/* 0x7a510, 10 bytes. Tail-call 0xe2af4 with dword 0x1f8e90. */
uint32_t forward_value_1f8e90(void)
{
    return guest_fn_e2af4(EF_U32(0x001f8e90u));
}

/* 0x7ccd0, 10 bytes. Tail-call 0xb1a10 with EAX = 0x1379bc. */
uint32_t call_b1a10_1379bc(void)
{
    return guest_fn_b1a10(0x001379bcu);
}

/* 0x7e89c, 33 bytes. AL = 1 when the signed high word of dword 0x19bb5c is
 * not 0xff and 0x2c358 on it returns a nonzero low byte. */
uint8_t scene_slot_valid(void)
{
    int32_t slot = EF_S32(0x0019bb5cu) >> 16;
    if (slot != 0xff && (guest_fn_2c358(slot) & 0xffu) != 0)
        return 1;
    return 0;
}

/* 0x7f6ac, 43 bytes. True when the signed high word of dword +0x16 is at
 * most 7 and word +0x18 differs from the grid marker. */
uint32_t record_mismatches_marker(ef_addr32 record)
{
    if ((EF_S32(record + 0x16u) >> 16) > 7)
        return 0;
    if ((uint16_t)g_world_grid_marker == EF_U16(record + 0x18u))
        return 0;
    return 1;
}

/* 0x803e8, 21 bytes. When byte 0x15bac6 is set, poll 0xd7594(0) until it
 * returns a nonzero low byte. */
void wait_for_event_803e8(void)
{
    if (EF_U8(0x0015bac6u) != 0) {
        do {
        } while ((guest_fn_d7594(0) & 0xffu) == 0);
    }
}
