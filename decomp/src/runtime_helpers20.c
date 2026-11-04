/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Table allocators, record constructors and string duplication, 0x27544..0x8c0c8. */

/* Call the original allocator stored at 0x12dd9c. */
static uint32_t call_allocator_12dd9c(uint32_t bytes, uint32_t flags)
{
    return ((uint32_t (*)(uint32_t, uint32_t))EF_U32(0x0012dd9cu))(bytes, flags);
}

/* 0x27544, 96 bytes. Draw a null-terminated list of 6-byte text entries, centered at x=160. */
void draw_centered_entries_27544(ef_addr32 list, int16_t y, int32_t step)
{
    int16_t i = 0;
    int32_t row = y;

    for (;;) {
        ef_addr32 entry = list + (uint32_t)((int32_t)i * 6);
        ef_addr32 text = EF_U32(entry);
        int32_t width;
        int32_t half;

        if (text == 0)
            break;
        width = (int16_t)((uint32_t (*)(ef_addr32))EF_U32(0x0019b430u))(text);
        half = (width - (width >> 31)) >> 1;
        ((void (*)(uint8_t, int16_t, int16_t, ef_addr32))EF_U32(0x0019b434u))(
            EF_U8(entry + 4u), (int16_t)(0xa0 - half), (int16_t)row, text);
        i++;
        row += step;
    }
}

/* 0x2aef8, 38 bytes. Allocate and zero the unit table for count+1 records of 0xca bytes. */
void alloc_unit_table_2aef8(int16_t count)
{
    int32_t total = (int32_t)count + 1;
    uint32_t bytes = (uint32_t)total * 0xcau;

    EF_U32(0x0019bb54u) = (uint32_t)total;
    EF_U32(0x0019bb58u) = call_allocator_12dd9c(bytes, 0);
    guest_fn_b3fb0(EF_U32(0x0019bb58u), 0, bytes);
}

/* 0x34d50, 68 bytes. Show the 60-step progress sequence, waiting five clock ticks per step. */
void run_progress_delay_34d50(void)
{
    int32_t step;

    EF_U16(0x001357b2u) = 7;
    for (step = 0; step < 0x3c; step++) {
        guest_fn_a3eac_args(0x00125921u, 0x14u);
        EF_U32(0x0019f338u) = ((uint32_t (*)(void))EF_U32(0x0015b620u))();
        while (((uint32_t (*)(void))EF_U32(0x0015b620u))() - EF_U32(0x0019f338u) < 5u)
            ;
    }
}

/* 0x35748, 90 bytes. Arm the timer record unless one is already active. */
void arm_timer_record_35748(uint8_t kind, uint16_t duration, uint8_t flag)
{
    if (EF_U8(0x0019f5a9u) != 0)
        ui_report_missing_record(0x00182fbcu);
    EF_U8(0x0019f5a2u) = kind;
    EF_U16(0x0019f586u) = duration;
    EF_U32(0x0019f340u) = 0;
    EF_U8(0x0019f5a1u) = 2;
    EF_U8(0x0019f5a0u) = flag;
    EF_U8(0x0019f5a9u) = 1;
    EF_U16(0x0019f58au) = 0;
    EF_U8(0x0019f5a5u) = 0;
}

/* 0x372d4, 53 bytes. Allocate and zero a table of count+1 records of 0x45 bytes. */
void alloc_record_table_372d4(int16_t count)
{
    int32_t total = (int32_t)count + 1;
    uint32_t bytes = (uint32_t)total * 0x45u;

    EF_U32(0x001a14f4u) = (uint32_t)total;
    EF_U32(0x001a14f8u) = call_allocator_12dd9c(bytes, 0);
    guest_fn_b3fb0(EF_U32(0x001a14f8u), 0, bytes);
    EF_U16(0x001a1506u) = 0;
}

/* 0x38584, 71 bytes. Take the next free slot index of the 2000-slot table. */
uint32_t take_free_slot_38584(void)
{
    uint32_t index = 0;

    if (((int32_t)EF_U32(0x001a1502u) >> 16) < 0x7d0) {
        index = EF_U16(0x001a1504u);
        EF_U32(0x0019f5b0u + (uint32_t)((int32_t)(int16_t)index * 4)) = 0;
        EF_U16(0x001a1504u) = (uint16_t)(index + 1u);
        return index;
    }
    ui_report_missing_record(0x00183434u);
    return index;
}

/* 0x3c410, 34 bytes. Allocate the 0x9202-byte work area. */
void alloc_work_area_3c410(void)
{
    EF_U32(0x001a2e78u) = guest_fn_e2a94(0x9202u);
    if (EF_U32(0x001a2e78u) == 0)
        ui_report_missing_record(0x00183638u);
}

/* 0x3c6c0, 97 bytes. Copy a string into a fresh buffer and register it. */
void register_string_3c6c0(ef_addr32 text)
{
    ef_addr32 copy;

    EF_U32(0x001a2e70u) = ((uint32_t (*)(ef_addr32))EF_U32(0x00132ab4u))(text);
    if (EF_U32(0x001a2e70u) == 0)
        guest_report_parser_enum_error(0x00183664u, text, 0);
    copy = guest_fn_e2a94(EF_U32(0x001a2e70u));
    if (copy == 0)
        ui_report_missing_record(0x0018367cu);
    ((void (*)(ef_addr32, ef_addr32))EF_U32(0x00132ab8u))(text, copy);
    EF_U32(0x001a2e6cu) = copy;
    guest_fn_3c5b8();
    guest_fn_e2af4(copy);
}

/* 0x3c730, 56 bytes. Find a named entry in the scene or abort. */
ef_addr32 find_entry_or_abort_3c730(ef_addr32 name, int16_t page)
{
    ef_addr32 group = find_scene_named_group(name, (int32_t)page);

    if (group != 0) {
        ef_addr32 entry = find_scene_named_entry(page, group);

        if (entry != 0)
            return entry;
    }
    guest_report_parser_enum_error(0x0018369cu, name, EF_U32(0x001a2e80u));
    return 0;
}

/* 0x3ccec, 47 bytes. Duplicate a string into freshly allocated memory. */
ef_addr32 duplicate_string_3ccec(ef_addr32 text)
{
    uint32_t length = ((uint32_t (*)(ef_addr32))EF_U32(0x00132ab4u))(text);
    ef_addr32 copy;

    if (length == 0)
        return 0;
    copy = guest_fn_e2a94(length + 1u);
    ((void (*)(ef_addr32, ef_addr32))EF_U32(0x00132ab8u))(text, copy);
    EF_U8(copy + length) = 0;
    return copy;
}

/* 0x76b10, 32 bytes. Allocate a table of count 25-byte records. */
void alloc_table_25_76b10(uint8_t count)
{
    EF_U32(0x001f8918u) = call_allocator_12dd9c((uint32_t)count * 25u, count);
}

/* 0x7a3e8, 64 bytes. Allocate (once) and clear the 0xc000-byte buffer. */
void clear_buffer_7a3e8(void)
{
    if (EF_U32(0x0012afc4u) == 0) {
        EF_U32(0x0012afc4u) = guest_fn_c3f70(0xc000u);
        if (EF_U32(0x0012afc4u) == 0)
            ui_report_missing_record(0x00187b78u);
    }
    guest_fn_b3fb0(EF_U32(0x0012afc4u), 0, 0xc000u);
}

/* 0x82000, 36 bytes. Build a 17-byte record from the arguments. */
ef_addr32 make_record_17_82000(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint8_t e)
{
    ef_addr32 block = call_allocator_12dd9c(0x11u, b);

    EF_U32(block + 0xdu) = a;
    EF_U32(block) = b;
    EF_U32(block + 4u) = c;
    EF_U32(block + 8u) = d;
    EF_U8(block + 0xcu) = e;
    return block;
}

/* 0x82024, 37 bytes. Build a 16-byte record from the arguments. */
ef_addr32 make_record_16_82024(uint32_t a, uint32_t b, uint32_t unused_bx, uint32_t unused_cx,
                               uint32_t c, uint32_t d)
{
    ef_addr32 block;

    (void)unused_bx;
    (void)unused_cx;
    block = call_allocator_12dd9c(0x10u, b);
    EF_U32(block + 4u) = a;
    EF_U32(block) = b;
    EF_U32(block + 8u) = c;
    EF_U32(block + 0xcu) = d;
    return block;
}

/* 0x8c0c8, 37 bytes. Store a value in the 0x898-entry table, aborting on a bad index. */
void set_table_value_8c0c8(int16_t index, uint32_t value)
{
    if (index >= 0x898)
        ui_report_missing_record(0x0018a8a0u);
    EF_U32(0x0020f550u + (uint32_t)((int32_t)index * 4)) = value;
}
