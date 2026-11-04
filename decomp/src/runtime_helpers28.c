/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Setup file readers, session reset and record pool helpers, 0x105a4..0x39e0c.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0x105a4, 141 bytes. Read four numbers from a parser file into the given words. */
uint32_t read_four_words_105a4(ef_addr32 first, ef_addr32 second, ef_addr32 third, ef_addr32 fourth,
                               ef_addr32 stack_name)
{
    if (((uint32_t (*)(ef_addr32))EF_U32(0x00132ab4u))(stack_name) == 0)
        guest_fatal_arg(0x00181594u, stack_name);
    EF_U32(0x00197ab0u) = guest_fn_9ccb8(stack_name);
    if (EF_U32(0x00197ab0u) == 0xffffffffu)
        guest_fatal_arg(0x001815c0u, stack_name);
    initialize_parser_records();
    guest_fn_e810();
    guest_fn_e810();
    guest_fn_e89c();
    EF_U16(first) = (uint16_t)guest_fn_e95c();
    EF_U16(second) = (uint16_t)guest_fn_e95c();
    EF_U16(third) = (uint16_t)guest_fn_e95c();
    EF_U16(fourth) = (uint16_t)guest_fn_e95c();
    guest_fn_9ce84(EF_U32(0x00197ab0u));
    return 1;
}

/* 0x1e3c4, 176 bytes. Re-roll the strength of every entry whose spot is occupied, per list node. */
void reroll_list_entries_1e3c4(void)
{
    ef_addr32 node = EF_U32(0x00137988u);

    while (node != 0) {
        int16_t cell = (int16_t)read_plane_cell(0, (int32_t)EF_U32(node + 0x16u), (int32_t)EF_U32(node + 0x1au));
        ef_addr32 entry;
        int32_t i;

        if (guest_fn_cd150(cell) == 0)
            guest_report_parser_enum_error(0x00181a44u, EF_U32(node + 0x16u), EF_U32(node + 0x1au));
        entry = EF_U32(node + 0x2cu);
        for (i = 0; i < ((int32_t)EF_U32(node + 0x28u) >> 16); i++) {
            if (guest_fn_1e214(cell, (int32_t)EF_U32(entry + 5u) >> 16,
                               (int32_t)EF_U32(entry + 9u) >> 16, (int32_t)EF_U32(entry + 7u) >> 16) != 0)
                EF_U8(entry + 5u) = (uint8_t)(100 - (int16_t)guest_fn_cd258());
            entry += 0x15u;
        }
        node = EF_U32(node + 4u);
    }
}

/* 0x285b8, 132 bytes. Write the save file header block for the current game file. */
void write_save_header_285b8(void)
{
    uint8_t name[0x98];
    uint32_t handle;

    if (EF_U16(0x0015ee9au) == 0)
        format_ui_status_pair((ef_addr32)(uintptr_t)name, 0x0018236cu, guest_fn_9c858(0x001822d8u), 0x00125208u);
    else
        format_ui_status_value((ef_addr32)(uintptr_t)name, 0x00182374u, 0x00125208u);
    handle = guest_fn_a7948((ef_addr32)(uintptr_t)name);
    if (handle == 0xffffffffu)
        ui_report_missing_record(0x00182378u);
    guest_fn_ab324(handle, 0);
    guest_fn_a7990(handle);
}

/* 0x2863c, 130 bytes. Read the save file header block of the current game file. */
void read_save_header_2863c(void)
{
    uint8_t name[0x98];
    uint32_t handle;

    if (EF_U16(0x0015ee9au) == 0)
        format_ui_status_pair((ef_addr32)(uintptr_t)name, 0x0018236cu, guest_fn_9c858(0x001822d8u), 0x00125208u);
    else
        format_ui_status_value((ef_addr32)(uintptr_t)name, 0x00182374u, 0x00125208u);
    handle = guest_fn_a7900((ef_addr32)(uintptr_t)name);
    if (handle == 0xffffffffu)
        ui_report_missing_record(0x0018239cu);
    guest_fn_ab47c(handle);
    guest_fn_a7990(handle);
}

/* 0x34ca4, 170 bytes. Start a new session: clear the flags and open the device if needed. */
void start_session_34ca4(void)
{
    uint8_t running = EF_U8(0x0013578cu);

    EF_U8(0x0019f59du) = 0;
    EF_U8(0x0019f59cu) = 0;
    EF_U8(0x0019f5aeu) = 0;
    EF_U8(0x0019f59bu) = 0xff;
    EF_U8(0x0019f59au) = 0xff;
    if (running == 0 && EF_U8(0x001357bau) == 0 && (guest_fn_a29cc(0) & 0xffu) != 0) {
        uint32_t mode = EF_U8(0x0013578eu);

        if (mode == 9) {
            ui_report_missing_record(0x00182ee4u);
        } else if (mode == 8 || mode == 0xc) {
            guest_fn_a29f8();
            guest_fn_a2bdc();
            EF_U8(0x001357bau) = 1;
        } else {
            ui_report_missing_record(0x00182f08u);
        }
    }
    guest_fn_33630();
    EF_U8(0x0019f5aeu) = 0;
    guest_fn_a41d0();
    EF_U8(0x0019f5a4u) = 0;
    EF_U32(0x0019f334u) = ((uint32_t (*)(void))EF_U32(0x0015b620u))();
}

/* 0x34ea4, 139 bytes. Clear the session flags and check the device state in mode 3. */
void clear_session_34ea4(void)
{
    EF_U8(0x0019f5aeu) = 0;
    EF_U8(0x00125902u) = 0;
    EF_U8(0x00125935u) = 0;
    EF_U8(0x00127da8u) = 0;
    EF_U8(0x00125839u) = 0;
    EF_U8(0x00127da9u) = 0;
    EF_U8(0x001a666bu) = 0;
    EF_U8(0x0019f59bu) = 0xff;
    EF_U32(0x0012656cu) = 0;
    if (EF_U8(0x0013579au) == 3) {
        if (EF_U8(0x00125936u) != 0) {
            if ((guest_fn_e1780() & 0xffu) == 0 && (guest_fn_e1744(EF_U32(0x00428cf0u)) & 0xffu) == 0)
                ui_report_missing_record(0x00182d98u);
        }
        if (EF_U8(0x001357c8u) != 0)
            guest_fn_a4d90(0x00182cf4u);
    }
}

/* 0x34e18, 140 bytes. Wait for the device to stop, then shut it down and clear the session. */
void end_session_34e18(void)
{
    for (;;) {
        EF_U8(0x0019f5aeu) = 0;
        guest_fn_a41c8();
        if (EF_U8(0x0013578cu) != 0 && EF_U8(0x001357c8u) != 0) {
            EF_U32(0x0019f334u) = ((uint32_t (*)(void))EF_U32(0x0015b620u))();
            while (((uint32_t (*)(void))EF_U32(0x0015b620u))() - EF_U32(0x0019f334u) < 0x64u)
                guest_fn_a4030();
        }
        if (EF_U8(0x0013578cu) != 0) {
            if (EF_U8(0x00135798u) == 0
                || (EF_U8(0x001357c8u) != 0 && ((int32_t)EF_U32(0x001357aeu) >> 16) < 2)) {
                guest_fn_a3da0();
                clear_session_34ea4();
            }
        }
        if (EF_U8(0x0013578du) != 0)
            break;
    }
    guest_fn_a3e04();
    EF_U8(0x0019f59bu) = 0xff;
    clear_session_34ea4();
}

/* 0x36054, 172 bytes. Open the named stream for playback and arm the stream state. */
void open_stream_36054(ef_addr32 name, uint8_t volume)
{
    uint32_t handle;
    uint32_t block = 0;

    if (EF_U8(0x0019f5aau) != 0)
        ui_report_missing_record(0x00183124u);
    guest_fn_35fbc();
    EF_U32(0x0019f34cu) = guest_fn_e2a94(((uint32_t)((int32_t)EF_U32(0x001258feu) >> 16)) << 3);
    handle = guest_fn_a7900(name);
    EF_U32(0x0019f360u) = handle;
    if (handle != 0xffffffffu) {
        block = guest_fn_a7b3c(name);
        EF_U32(0x0019f350u) = block;
    }
    if (handle == 0xffffffffu || block == 0)
        guest_report_parser_enum_error(0x00183148u, name, 0);
    EF_U8(0x0019f5a3u) = 1;
    EF_U32(0x0019f348u) = EF_U32(0x0019f350u);
    EF_U8(0x0019f5aau) = 2;
    EF_U16(0x0019f594u) = 0;
    EF_U8(0x0019f5afu) = 1;
    EF_U8(0x0019f5acu) = volume;
    EF_U32(0x0019f358u) = 0;
}

/* 0x3730c, 141 bytes. Take the next 69-byte record of the table, linking it to the previous one. */
uint32_t take_table_record_3730c(void)
{
    int32_t index = (int32_t)EF_U32(0x001a1504u) >> 16;
    ef_addr32 table;
    ef_addr32 record;
    uint16_t counter;

    if (index >= (int32_t)EF_U32(0x001a14f4u))
        return 0;
    if (EF_U32(0x001a14f8u) == 0)
        ui_report_missing_record(0x00183334u);
    index = (int32_t)EF_U32(0x001a1504u) >> 16;
    table = EF_U32(0x001a14f8u);
    record = table + (uint32_t)(index * 69);
    EF_U32(record + 0x40u) = 0;
    if ((int16_t)EF_U16(0x001a1506u) > 0)
        EF_U32(table + (uint32_t)((index - 1) * 69) + 0x40u) = record;
    counter = EF_U16(0x001a1506u);
    EF_U16(0x001a1506u) = (uint16_t)(counter + 1u);
    return counter;
}

/* 0x39e0c, 208 bytes. Claim a free slot in the 4-byte slot table for a unit and mark its owner. */
void claim_unit_slot_39e0c(int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);
    ef_addr32 slots;
    uint8_t count;
    uint8_t i;

    if (unit == 0)
        return;
    slots = EF_U32(0x001a1860u);
    if (EF_U8(unit + 0xe9u) == 0x22)
        EF_U8(((int32_t)EF_U32(EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca) + 0x13u) >> 16)
              + 0x001fcc66u) = 1;
    count = EF_U8(0x001a1864u);
    for (i = 0; i < count; i++) {
        ef_addr32 slot = slots + (uint32_t)i * 4u;

        /* ORIGINAL BUG: the free test reads the high word of the first dword, i.e. +2. */
        if (((int32_t)EF_U32(slot) >> 16) == 0xff && EF_U8(unit + 0x187u) == 0xff) {
            EF_U16(slot) = 0;
            EF_U16(slot + 2u) = (uint16_t)unit_id;
            EF_U8(unit + 0x187u) = i;
        }
        count = EF_U8(0x001a1864u);
    }
    EF_U32(0x001a1860u) = slots;
    if (EF_U8(unit + 0x187u) == 0xff)
        ui_report_missing_record(0x00183540u);
}
