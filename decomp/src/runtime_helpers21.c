/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Table lookups, I/O callback wrappers and checked allocation, 0x8fdf0..0xa46cc. */

/* Call the original allocator stored at 0x12dd9c. */
static uint32_t call_allocator_12dd9c(uint32_t bytes, uint32_t flags)
{
    return ((uint32_t (*)(uint32_t, uint32_t))EF_U32(0x0012dd9cu))(bytes, flags);
}

/* 0x8fdf0, 64 bytes. Find the entry with the given id in the 50-entry table or abort. */
ef_addr32 find_entry_by_id_8fdf0(int16_t id)
{
    int16_t i;

    for (i = 0; i < 0x32; i++) {
        ef_addr32 entry = EF_U32(0x00211a00u + (uint32_t)((int32_t)i * 4));

        if (entry != 0 && (uint32_t)EF_U16(entry + 0x20u) == (uint32_t)(int32_t)id)
            return entry;
    }
    ui_report_missing_record(0x0018aa70u);
    return 0;
}

/* 0x8fee0, 64 bytes. Reset the 0x5dc0-byte buffer state and allocate the buffer. */
void init_buffer_8fee0(void)
{
    EF_U32(0x00211ad4u) = 0;
    EF_U16(0x001a1504u) = 0;
    EF_U16(0x00211ae2u) = 0;
    EF_U16(0x00211ae0u) = 0;
    EF_U16(0x00211adeu) = 1;
    EF_U32(0x00211ad0u) = call_allocator_12dd9c(0x5dc0u, 1);
}

/* 0x8ff20, 40 bytes. Allocate and zero a table of count 68-byte records. */
ef_addr32 alloc_zero_68_8ff20(uint8_t count)
{
    uint32_t bytes = (uint32_t)count * 68u;
    ef_addr32 block = call_allocator_12dd9c(bytes, 0);

    guest_fn_b3fb0(block, 0, bytes);
    return block;
}

/* 0x980e8, 76 bytes. Reset a 13-byte slot when the sound path is on; abort on a bad slot. */
void reset_sound_slot_980e8(uint8_t slot)
{
    if (EF_U8(0x00420a72u) != 0) {
        if (slot < 4)
            guest_fn_d5ec0(EF_U32(0x0012dd98u) + (uint32_t)slot * 13u);
        if (slot > 4)
            guest_fatal_arg(0x0018ad0cu, slot);
    }
}

/* 0x985b0, 66 bytes. Store a range and start the transfer; abort when the range is negative. */
void start_transfer_985b0(uint8_t mode, uint32_t target, uint32_t start, uint32_t end)
{
    int32_t length = (int32_t)(end - start);

    EF_U32(0x0012dda0u) = target;
    EF_U32(0x0040c120u) = start;
    EF_U32(0x0040c118u) = (uint32_t)length;
    if (length < 0)
        ui_report_missing_record(0x0018ad5cu);
    EF_U32(0x0040c11cu) = 0;
    guest_fn_98658(mode);
}

/* 0x9d060, 37 bytes. Allocate a block or abort with the requested size. */
ef_addr32 alloc_or_abort_9d060(uint32_t size)
{
    ef_addr32 block = guest_fn_c3f70(size);

    if (block == 0)
        guest_fatal_arg(0x0018b1b8u, size);
    return block;
}

/* 0xa00a4, 67 bytes. Count a record down; true while it is still live. */
uint32_t countdown_record_a00a4(ef_addr32 record)
{
    uint8_t now[12];
    uint16_t left = (uint16_t)(EF_U16(record + 6u) - 1u);

    EF_U16(record + 6u) = left;
    if (left == 0)
        return 1;
    ((void (*)(ef_addr32, ef_addr32))EF_U32(0x00132af0u))((ef_addr32)(uintptr_t)now, 0x00417164u);
    if (*(int32_t *)(now + 4) < EF_S32(record + 0x10u))
        return 1;
    return 0;
}

/* 0xa1afc, 41 bytes. Read from the stream and abort if the count is short. */
void read_checked_a1afc(uint32_t buffer, uint32_t count)
{
    if (guest_fn_a79d4_ret(EF_U32(0x00132b18u), count, buffer) != count)
        ui_report_missing_record(0x0018b348u);
}

/* 0xa1b28, 41 bytes. Write to the stream and abort if the count is short. */
void write_checked_a1b28(uint32_t buffer, uint32_t count)
{
    if (guest_fn_a7a1c_ret(EF_U32(0x00132b18u), count, buffer) != count)
        ui_report_missing_record(0x0018b35cu);
}

/* 0xa2900, 44 bytes. Check a count against the limit at 0x1357ac, aborting if above. */
uint8_t check_count_a2900(uint16_t count)
{
    if (count > EF_U16(0x001357acu))
        guest_report_parser_enum_error(0x0018cbb8u, count, EF_U16(0x001357acu));
    return 1;
}

/* 0xa39ec, 36 bytes. Abort unless the stored name matches the expected one. */
void check_name_a39ec(void)
{
    if (guest_fn_a4380(EF_U32(0x00135770u), 0x0018ca80u) != 0)
        ui_report_missing_record(0x0018cd20u);
}

/* 0xa3bdc, 31 bytes. Query the current slot through the callback and cache its table entry. */
void cache_current_slot_a3bdc(void)
{
    uint8_t slot = (uint8_t)((uint32_t (*)(void))EF_U32(0x00136bb4u))();

    EF_U8(0x0013578eu) = slot;
    EF_U32(0x00135790u) = EF_U32(0x00135710u + (uint32_t)slot * 4u);
}

/* 0xa3da0, 99 bytes. Shut the device down and clear its state flags. */
void shutdown_device_a3da0(void)
{
    if (EF_U8(0x0013578cu) != 0 || EF_U8(0x0013578du) != 0) {
        guest_fn_a42f4();
        if ((uint8_t)((uint32_t (*)(void))EF_U32(0x00136ba4u))() == 0)
            ui_report_missing_record(0x0018cf08u);
    }
    if (EF_U8(0x0013578cu) != 0) {
        guest_fn_a3fe0();
        guest_fn_a3fe0();
        clear_all_entries_a2e6c();
    }
    EF_U16(0x001357b0u) = 0;
    EF_U8(0x00135798u) = 0;
    EF_U8(0x0013578cu) = 0;
}

/* 0xa3e04, 46 bytes. Close the device through its callback when it is armed but not running. */
void close_device_a3e04(void)
{
    if (EF_U8(0x0013578du) != 0 && EF_U8(0x0013578cu) == 0) {
        ((void (*)(void))EF_U32(0x00136bb8u))();
        EF_U8(0x00135798u) = 0;
        EF_U8(0x0013578cu) = 0;
        EF_U8(0x0013578du) = 0;
    }
}

/* 0xa3e50, 91 bytes. Issue a request through the callback and account for it. */
void issue_request_a3e50(uint32_t request, uint16_t size, uint8_t flag)
{
    if (EF_U8(0x0013581eu) == 0 && check_count_a2900(size) != 0) {
        EF_U32(0x00135820u) = EF_U32(0x00135820u) + 1u;
        EF_U32(0x00135830u) = EF_U32(0x00135830u) + 1u;
        EF_U32(0x00135834u) = EF_U32(0x00135834u) + size;
        ((void (*)(uint32_t, uint32_t, uint32_t))EF_U32(0x00136bbcu))(request, size, flag);
    }
}

/* 0xa3eac, 96 bytes. Issue a request with a zero flag if the device is ready. */
void issue_request_ready_a3eac(uint32_t request, uint16_t size)
{
    if (EF_U8(0x0013581eu) == 0 && guest_fn_a292c() != 0 && check_count_a2900(size) != 0) {
        EF_U32(0x00135820u) = EF_U32(0x00135820u) + 1u;
        EF_U32(0x00135830u) = EF_U32(0x00135830u) + 1u;
        EF_U32(0x00135834u) = EF_U32(0x00135834u) + size;
        ((void (*)(uint32_t, uint32_t, uint32_t))EF_U32(0x00136bbcu))(request, size, 0);
    }
}

/* 0xa3fe0, 78 bytes. Poll the device and accumulate its two status bytes. */
void poll_device_a3fe0(void)
{
    ef_addr32 first;
    ef_addr32 second;
    uint32_t total;

    ((void (*)(void))EF_U32(0x00136bc0u))();
    first = EF_U32(0x00136b88u);
    second = EF_U32(0x00136b78u);
    EF_U32(0x0013582cu) = (uint32_t)EF_U8(first) + (uint32_t)EF_U8(second);
    total = EF_U32(0x00135824u) + EF_U8(first);
    EF_U32(0x00135824u) = total;
    EF_U32(0x00135824u) = total + EF_U8(second);
}

/* 0xa42d0, 34 bytes. Start the device through its callback if it is not yet started. */
void start_device_a42d0(void)
{
    if (EF_U8(0x00136bc8u) == 0) {
        ((void (*)(uint32_t, uint8_t))EF_U32(0x00136ba8u))(0, 1);
        EF_U8(0x00136bc8u) = 1;
        EF_U8(0x0013583du) = 0;
    }
}

/* 0xa42f4, 35 bytes. Stop the device through its callback when running. */
void stop_device_a42f4(void)
{
    if (EF_U8(0x0013578cu) != 0 && EF_U8(0x00136bc8u) != 0) {
        ((void (*)(void))EF_U32(0x00136bacu))();
        EF_U8(0x00136bc8u) = 0;
    }
}

/* 0xa46cc, 35 bytes. Look up a named object or abort. */
ef_addr32 find_named_or_abort_a46cc(ef_addr32 name)
{
    ef_addr32 object = guest_fn_a4678(name);

    if (object == 0)
        guest_fatal_arg(0x0018cf9cu, name);
    return object;
}
