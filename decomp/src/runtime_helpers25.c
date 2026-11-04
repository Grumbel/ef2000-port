/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Waits, arena allocator and resource file loading, 0xdead0..0xeaaf4. */

/* 0xdead0, 72 bytes. Find a key in the 1-based id table (stack argument) or abort. */
uint32_t find_table_entry_dead0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_key)
{
    int16_t i = 1;
    int16_t count = (int16_t)EF_U16(0x0015eea0u);

    (void)u0; (void)u1; (void)u2; (void)u3;
    if (i < count) {
        do {
            if ((int16_t)stack_key == (int16_t)EF_U16(0x00428090u + (uint32_t)((int32_t)i * 2)))
                return EF_U32(EF_U32(0x00428860u) + (uint32_t)((int32_t)i * 4));
            i++;
        } while (i < count);
    }
    ui_report_missing_record(0x001905a4u);
    return 0;
}

/* 0xe1130, 43 bytes. Wait for the given ticks or until the abort bit 0x80 is set. */
void wait_ticks_or_key_e1130(uint32_t ticks)
{
    uint32_t end = (ticks & 0xffffu) + ((uint32_t (*)(void))EF_U32(0x0015b620u))();

    do {
        if ((EF_U8(0x0015eed4u) & 0x80u) != 0)
            break;
    } while (((uint32_t (*)(void))EF_U32(0x0015b620u))() < end);
}

/* 0xe115c, 53 bytes. Wait for the given ticks or until the input poll reports a press. */
void wait_ticks_or_press_e115c(uint32_t ticks)
{
    uint32_t end = (ticks & 0xffffu) + ((uint32_t (*)(void))EF_U32(0x0015b620u))();

    do {
        if ((guest_fn_e0b10(0x001905f8u) & 0xffu) != 0)
            break;
    } while (((uint32_t (*)(void))EF_U32(0x0015b620u))() < end);
    guest_fn_e1110(3);
}

/* 0xe29a0, 72 bytes. Allocate the arena on first use, register its release hook, then reset it. */
void init_arena_e29a0(void)
{
    if (EF_U32(0x0015f75cu) == 0) {
        EF_U32(0x0015f760u) = EF_U32(0x001264d4u);
        EF_U32(0x0015f75cu) = guest_fn_c3f70(EF_U32(0x001264d4u));
        if (EF_U32(0x0015f75cu) == 0)
            guest_fatal_arg(0x00190880u, EF_U32(0x0015f760u));
        guest_fn_9c3cf(0xe2a50u);
    }
    guest_fn_e2a1c();
}

/* 0xe29e8, 50 bytes. Take bytes from the arena without a header; abort when it runs out. */
uint32_t arena_take_e29e8(uint32_t bytes)
{
    int32_t left = (int32_t)(EF_U32(0x0015f758u) - bytes);

    EF_U32(0x0015f758u) = (uint32_t)left;
    if (left >= 0)
        return EF_U32(0x0015f754u) + (uint32_t)left;
    guest_report_parser_enum_error(0x001908a4u, bytes + (uint32_t)left, bytes);
    return 0;
}

/* 0xe2a94, 94 bytes. Allocate a block with a 4-byte size header from the arena. */
uint32_t arena_alloc_e2a94(uint32_t bytes)
{
    uint32_t total = bytes + 4u;
    int32_t left = (int32_t)(EF_U32(0x0015f758u) - total);
    uint32_t block;

    EF_U32(0x0015f758u) = (uint32_t)left;
    if (left < 0)
        guest_fatal_arg(0x001908bcu, total);
    block = EF_U32(0x0015f754u);
    EF_U32(block) = total;
    EF_U32(0x0015f754u) = block + total;
    EF_U32(0x0015f764u) = EF_U32(0x0015f764u) + total;
    EF_U32(0x0015f768u) = EF_U32(0x0015f768u) + total;
    return block + 4u;
}

/* 0xe2af4, 95 bytes. Free a block of the arena; the arena resets when the last block is freed. */
void arena_free_e2af4(uint32_t block)
{
    uint32_t total;
    uint32_t live;

    if (EF_U32(0x0015f764u) == 0)
        ui_report_missing_record(0x001908ccu);
    total = EF_U32(block - 4u);
    live = EF_U32(0x0015f764u) - total;
    EF_U32(0x0015f764u) = live;
    if (live == 0) {
        uint32_t used = EF_U32(0x0015f768u);

        EF_U32(0x0015f768u) = 0;
        EF_U32(0x0015f758u) = EF_U32(0x0015f758u) + used;
        EF_U32(0x0015f754u) = EF_U32(0x0015f754u) - used;
    }
}

/* 0xe3170, 45 bytes. Read from the resource handle and abort when the count is short. */
void read_resource_e3170(uint32_t buffer, uint16_t count)
{
    if (guest_fn_a79d4_ret(EF_U32(0x0015f8fcu), count, buffer) != count)
        ui_report_missing_record(0x001908e0u);
}

/* 0xe3c20, 63 bytes. Take the next 36-byte record of the 1000-record pool. */
ef_addr32 next_pool_record_e3c20(void)
{
    uint32_t index = EF_U32(0x0015f938u);
    ef_addr32 record = 0x00429230u + index * 36u;

    EF_U32(0x0015f938u) = index + 1u;
    if ((int32_t)(index + 1u) >= 0x3e8)
        ui_report_missing_record(0x00190a20u);
    return record;
}

/* 0xe3fbc, 61 bytes. Translate a value and append the non-zero result to the 32-entry list. */
void append_translated_e3fbc(uint32_t value)
{
    uint32_t input = value;
    uint32_t result = guest_fn_1092b0((ef_addr32)(uintptr_t)&input);

    if (result != 0) {
        uint32_t count = EF_U32(0x0015f93cu) + 1u;

        EF_U8(0x00431ecfu + count) = (uint8_t)result;
        EF_U32(0x0015f93cu) = count;
        if ((int32_t)count >= 0x20)
            ui_report_missing_record(0x00190ab8u);
    }
}

/* 0xe9574, 70 bytes. Release the buffer at 0x1600a0 once and mark it released. */
void release_buffer_e9574(void)
{
    if (EF_U8(0x001600a4u) == 0)
        ui_report_missing_record(0x0019139cu);
    if (EF_U8(0x001600a4u) == 2)
        ui_report_missing_record(0x001913d4u);
    arena_free_e2af4(EF_U32(0x001600a0u));
    EF_U8(0x001600a4u) = 2;
}

/* 0xea100, 46 bytes. Allocate and zero a block, aborting on failure. */
uint32_t alloc_zeroed_checked_ea100(uint32_t size)
{
    uint32_t block = guest_fn_c3f70(size);

    if (block == 0)
        guest_fatal_arg(0x001914dcu, size);
    guest_fn_b3fb0(block, 0, size);
    return block;
}

/* 0xea1a0, 70 bytes. Read a header into an object and take its chunk size when tagged 'RA'. */
void read_chunk_header_ea1a0(ef_addr32 object, uint32_t source)
{
    int16_t tag;

    guest_fn_9cc28(source, EF_U32(object + 8u));
    tag = (int16_t)EF_U16(EF_U32(object + 8u));
    if (tag == 0x524a)
        guest_fatal_arg(0x00191500u, source);
    if (tag == 0x4152)
        EF_U32(object + 4u) = EF_U32(EF_U32(object + 8u) + 4u);
}

/* 0xea420, 70 bytes. Decode the current chunk either with the 'RA' decoder or a plain copy. */
void decode_chunk_ea420(uint32_t destination)
{
    ef_addr32 table = EF_U32(0x004644d0u);
    ef_addr32 data = EF_U32(table + 8u);

    if ((int16_t)EF_U16(data) == 0x4152) {
        if ((guest_fn_10da50(data, destination) & 0xffu) == 0)
            ui_report_missing_record(0x0019152cu);
        return;
    }
    guest_fn_dc832(destination, data, EF_U32(table + 4u));
}

/* 0xea470, 42 bytes. Install the exit handler and restart; abort when called with a mode other than 1. */
void install_exit_handler_ea470(uint32_t mode)
{
    if (mode != 1)
        ui_report_missing_record(0x00191538u);
    guest_fn_10dc7f(2, 0x0010dc40u);
    guest_fn_81c();
}

/* 0xeaab0, 65 bytes. Open the data file and read its 0x306-byte header. */
void open_data_file_eaab0(void)
{
    EF_U32(0x00160644u) = guest_fn_9ccb8(0x004644f0u);
    if (EF_U32(0x00160644u) == 0xffffffffu)
        guest_report_parser_enum_error(0x00191550u, 0x004644f0u, 0);
    guest_fn_9cde8(0x00484160u, 0x306u, EF_U32(0x00160644u));
}

/* 0xeaaf4, 79 bytes. Read the data file's size-prefixed block into 0x474368. */
void read_data_block_eaaf4(void)
{
    uint32_t size = 0x20000u;

    guest_fn_9cde8((ef_addr32)(uintptr_t)&size, 4u, EF_U32(0x00160644u));
    if (size > 0xfde8u)
        ui_report_missing_record(0x00191568u);
    guest_fn_9cde8(0x00474368u, size, EF_U32(0x00160644u));
}
