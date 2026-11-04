/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Script text slots, mode setup and driver channel wrappers, 0xcd2a8..0xdc280. */

/* 0xcd2a8, 64 bytes. Store a half-word offset in the current table entry. */
void store_offset_cd2a8(uint32_t pointer)
{
    int32_t offset;

    if (EF_U16(EF_U32(0x0015b628u)) == 0)
        ui_report_missing_record(0x0018f570u);
    offset = ((int32_t)(pointer - EF_U32(0x0041f704u))) >> 1;
    EF_U16(EF_U32(0x0041f700u) + (uint32_t)(((int32_t)EF_U32(EF_U32(0x0015b628u) + 2u) >> 16) * 2) + 2u) =
        (uint16_t)offset;
}

/* 0xcd640, 58 bytes. Allocate the 0xc000-byte text block of a slot when it has none. */
void ensure_text_block_cd640(int16_t slot)
{
    if (EF_U32(0x0041f840u + (uint32_t)((int32_t)slot * 4)) == 0) {
        uint32_t block = guest_fn_c3f70(0xc000u);

        if (block == 0)
            ui_report_missing_record(0x0018f590u);
        EF_U32(0x0041f840u + (uint32_t)((int32_t)slot * 4)) = block;
    }
}

/* 0xcd67c, 79 bytes. Copy a text into the block of a slot; abort when empty or too long. */
void copy_text_to_slot_cd67c(int16_t slot, ef_addr32 text)
{
    int32_t length = (int32_t)((uint32_t (*)(ef_addr32))EF_U32(0x00132ab4u))(text);

    if (length == 0)
        guest_fatal_arg(0x0018f5a8u, text);
    if (length > 0xc000)
        guest_fatal_arg(0x0018f5c4u, text);
    ((void (*)(ef_addr32, ef_addr32))EF_U32(0x00132ab8u))(
        text, EF_U32(0x0041f840u + (uint32_t)((int32_t)slot * 4)));
}

/* 0xcd724, 47 bytes. Store a text in a fixed slot (0..99). */
void set_text_slot_cd724(int16_t slot, ef_addr32 text)
{
    if (slot < 0 || slot >= 0x64)
        ui_report_missing_record(0x0018f5dcu);
    ensure_text_block_cd640(slot);
    copy_text_to_slot_cd67c(slot, text);
}

/* 0xcd754, 73 bytes. Store a text in the next dynamic slot and return its id. */
uint32_t add_text_slot_cd754(ef_addr32 text)
{
    uint32_t next = EF_U32(0x0015b730u) + 1u;
    uint32_t id = (uint32_t)EF_U16(0x0015b730u) + 0x64u;

    EF_U32(0x0015b730u) = next;
    if ((int32_t)next >= 0x1f4)
        ui_report_missing_record(0x0018f5f0u);
    ensure_text_block_cd640((int16_t)id);
    copy_text_to_slot_cd67c((int16_t)id, text);
    return id;
}

/* 0xcd7f0, 42 bytes. Return the text block of a slot (0..599). */
uint32_t get_text_slot_cd7f0(int16_t slot)
{
    if (slot < 0 || slot >= 0x258)
        ui_report_missing_record(0x0018f604u);
    return EF_U32(0x0041f840u + (uint32_t)((int32_t)slot * 4));
}

/* 0xd16d0, 41 bytes. Write to the log file and abort if the count is short. */
void write_log_d16d0(uint32_t buffer, uint32_t count)
{
    if (guest_fn_a7a1c_ret(EF_U32(0x00420944u), count, buffer) != count)
        ui_report_missing_record(0x0018f6a4u);
}

/* 0xd1774, 72 bytes. Open the log file and write its header. */
void open_log_d1774(ef_addr32 name)
{
    EF_U32(0x00420944u) = guest_fn_a7948(name);
    if (EF_U32(0x00420944u) == 0xffffffffu)
        guest_fatal_arg(0x0018f6c0u, name);
    guest_fn_d16fc(0x0018f6d4u);
    guest_fn_d170c(0x10370u);
    guest_fn_d16fc(0x0018f6dcu);
}

/* 0xd1da8, 61 bytes. Pick the 640x400 video mode if either mode is available. */
void set_mode_640x400_d1da8(uint32_t first, uint32_t second)
{
    guest_fn_fc534();
    if ((guest_fn_fc600(first) & 0xffu) == 0 && (guest_fn_fc600(second) & 0xffu) == 0)
        ui_report_missing_record(0x0018f73cu);
    guest_fn_d1d40(0x280u, 0x190u);
}

/* 0xd1de8, 82 bytes. Set up the 640x480 video mode; other modes abort but continue. */
void set_video_mode_d1de8(uint32_t mode, uint32_t mode_id, uint32_t height, uint32_t width)
{
    if (mode == 5) {
        mode_id = 0x101;
        width = 0x280;
        height = 0x1e0;
    } else {
        ui_report_missing_record(0x0018f75cu);
    }
    guest_fn_fc534();
    if ((guest_fn_fc600(mode_id) & 0xffu) == 0)
        ui_report_missing_record(0x0018f77cu);
    guest_fn_d1d40(width, height);
}

/* 0xd6fa8, 83 bytes. Allocate the text buffer of an object and fill it from the default text. */
void alloc_object_text_d6fa8(ef_addr32 object)
{
    uint32_t length = EF_U32(object + 0x18u);
    uint32_t text = 0;

    if (length == 0) {
        text = guest_fn_d7070();
        length = ((uint32_t (*)(uint32_t))EF_U32(0x00132ab4u))(text);
        if (length == 0)
            return;
    }
    EF_U32(object + 0x1cu) = guest_fn_c3f70(length);
    if (EF_U32(object + 0x1cu) == 0)
        ui_report_missing_record(0x0018f96cu);
    if (EF_U32(object + 0x18u) == 0) {
        EF_U32(object + 0x18u) = length;
        ((void (*)(uint32_t, uint32_t))EF_U32(0x00132ab8u))(text, EF_U32(object + 0x1cu));
    }
}

/* 0xd73b8, 55 bytes. Run channel call 0x102858 on the driver channel (0..13). */
void channel_call_102858_d73b8(uint8_t channel)
{
    if (EF_U8(0x0015bac4u) != 0) {
        if (channel >= 0xe)
            ui_report_missing_record(0x0018f9c0u);
        guest_fn_102858(EF_U32(0x00420dd0u + (uint32_t)channel * 4u));
    }
}

/* 0xd7458, 55 bytes. Run channel call 0x102824 on the driver channel (0..13). */
void channel_call_102824_d7458(uint8_t channel)
{
    if (EF_U8(0x0015bac4u) != 0) {
        if (channel >= 0xe)
            ui_report_missing_record(0x0018f9f0u);
        guest_fn_102824(EF_U32(0x00420dd0u + (uint32_t)channel * 4u));
    }
}

/* 0xd7490, 61 bytes. Run channel call 0x1027a0 on the driver channel (0..13). */
void channel_call_1027a0_d7490(uint8_t channel)
{
    if (EF_U8(0x0015bac4u) != 0) {
        if (channel >= 0xe)
            guest_fatal_arg(0x0018fa18u, channel);
        guest_fn_1027a0(EF_U32(0x00420dd0u + (uint32_t)channel * 4u));
    }
}

/* 0xd74d0, 96 bytes. Start the driver channel and queue its follow-up request. */
void channel_start_d74d0(uint8_t channel)
{
    if (EF_U8(0x0015bac4u) != 0) {
        ef_addr32 object;
        uint32_t follow_up;

        if (channel >= 0xe)
            guest_fatal_arg(0x0018fa34u, channel);
        object = EF_U32(0x00420dd0u + (uint32_t)channel * 4u);
        guest_fn_1027f8(object);
        follow_up = EF_U32(object + 8u);
        if (follow_up != 0)
            guest_fn_104920(follow_up, EF_U32(object + 0x10u) - 1u);
        EF_U32(0x0015bad4u) = EF_U32(0x0015bad4u) + 1u;
    }
}

/* 0xd7530, 61 bytes. Run channel call 0x1027cc on the driver channel (0..13). */
void channel_call_1027cc_d7530(uint8_t channel)
{
    if (EF_U8(0x0015bac4u) != 0) {
        if (channel >= 0xe)
            guest_fatal_arg(0x0018fa50u, channel);
        guest_fn_1027cc(EF_U32(0x00420dd0u + (uint32_t)channel * 4u));
    }
}

/* 0xda190, 36 bytes. Select the table entry matching a name, store the key and run its handler. */
void run_named_handler_da190(ef_addr32 table, ef_addr32 name)
{
    EF_U32(table + 0x1c0u) = name;
    while (guest_fn_a4380(EF_U32(table), name) != 0)
        table += 8u;
    ((void (*)(void))EF_U32(table + 4u))();
}

/* 0xdc250, 46 bytes. Toggle the page flag between 0 and 1 around the page callbacks. */
void toggle_page_dc250(void)
{
    guest_fn_eb35c();
    ((void (*)(uint32_t))EF_U32(0x0015b74cu))(EF_U8(0x0015bdd1u));
    EF_U8(0x0015bdd1u) = (uint8_t)((EF_U8(0x0015bdd1u) ^ 1u) & 1u);
    ((void (*)(uint32_t))EF_U32(0x0015b750u))(EF_U8(0x0015bdd1u));
}

/* 0xdc280, 60 bytes. Cycle the page flag through 0..2 around the page callbacks. */
void cycle_page_dc280(void)
{
    guest_fn_eb35c();
    ((void (*)(uint32_t))EF_U32(0x0015b74cu))(EF_U8(0x0015bdd1u));
    EF_U8(0x0015bdd1u) = (uint8_t)(EF_U8(0x0015bdd1u) + 1u);
    if (EF_U8(0x0015bdd1u) > 2)
        EF_U8(0x0015bdd1u) = 0;
    ((void (*)(uint32_t))EF_U32(0x0015b750u))(EF_U8(0x0015bdd1u));
}
