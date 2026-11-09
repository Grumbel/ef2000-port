/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* X-32 requests use packed 10-byte slots, timers packed 14-byte slots.
 * Alternate entries put their dword fields on 2-byte boundaries. The DOS
 * CPU permits this; native C must not dereference a misaligned uint32_t*. */
static uint32_t read_packed_dword(ef_addr32 address)
{
    return (uint32_t)EF_U8(address)
         | ((uint32_t)EF_U8(address + 1u) << 8)
         | ((uint32_t)EF_U8(address + 2u) << 16)
         | ((uint32_t)EF_U8(address + 3u) << 24);
}

static void write_packed_dword(ef_addr32 address, uint32_t value)
{
    EF_U8(address) = (uint8_t)value;
    EF_U8(address + 1u) = (uint8_t)(value >> 8);
    EF_U8(address + 2u) = (uint8_t)(value >> 16);
    EF_U8(address + 3u) = (uint8_t)(value >> 24);
}

/* 0x628, 144 bytes. Take a free 10-byte request slot, fill it and start the request. */
uint32_t start_request_628(uint32_t first, uint32_t second, int32_t third)
{
    uint32_t free_index;
    uint32_t slot;

#if defined(EF_DECOMP_NATIVE)
    /* Host-only boundary: the normal main probe never ran the X-32
     * pre-main initializer. Do not underflow its uninitialized pool. */
    extern void ef_decomp_x32_request_guard(uint32_t, uint32_t, int32_t);
    ef_decomp_x32_request_guard(first, second, third);
#endif
    EF_U32(0x00196144u) = EF_U32(0x00196144u) - 1u;
    if ((int32_t)EF_U32(0x00196144u) < 0)
        ui_report_missing_record(0x0017f098u);
    if (third <= 0)
        guest_fatal_arg(0x0017f0acu, (uint32_t)third);
    free_index = EF_U8(0x00195fb0u + EF_U32(0x00196144u));
    slot = free_index * 5u;
    EF_U16(0x00196026u + slot * 2u) = (uint16_t)second;
    write_packed_dword(0x00196022u + slot * 2u, first);
    write_packed_dword(0x00196028u + slot * 2u, (uint32_t)third);
    if (guest_fn_9c220(first, second & 0xffffu, (uint32_t)third) != 0)
        guest_fatal_arg(0x0017f0ccu, (uint32_t)third);
    return free_index;
}

/* 0x6b8, 124 bytes. Finish a request slot and return it to the free list. */
void finish_request_6b8(uint32_t index)
{
    uint32_t slot = index * 5u;
    uint16_t second = EF_U16(0x00196026u + slot * 2u);
    uint32_t first = read_packed_dword(0x00196022u + slot * 2u);
    uint32_t third = read_packed_dword(0x00196028u + slot * 2u);
    uint32_t next;

    if (third == 0)
        return;
    if (guest_fn_9c23d(first, second, third) != 0)
        ui_report_missing_record(0x0017f0e8u);
    write_packed_dword(0x00196028u + slot * 2u, 0u);
    next = EF_U32(0x00196144u) + 1u;
    EF_U8(0x00195fafu + next) = (uint8_t)index;
    EF_U32(0x00196144u) = next;
}

/* 0x734, 120 bytes. Take a free 14-byte timer slot, fill it from the source and start it. */
uint32_t start_timer_734(uint32_t source, uint32_t unused_dx, uint32_t flags,
                         uint32_t selector)
{
    uint32_t free_index;
    uint32_t slot;
    uint64_t result;

    (void)unused_dx;
    EF_U32(0x00196140u) = EF_U32(0x00196140u) - 1u;
    if ((int32_t)EF_U32(0x00196140u) < 0)
        ui_report_missing_record(0x0017f0fcu);
    free_index = EF_U8(0x00196134u + EF_U32(0x00196140u));
    slot = free_index * 7u;
    write_packed_dword(0x001960c4u + slot * 2u, source);
    write_packed_dword(0x001960c8u + slot * 2u, guest_fn_324(source));
    result = guest_fn_348(source);
    EF_U16(0x001960d0u + slot * 2u) = (uint16_t)(result >> 32);
    write_packed_dword(0x001960ccu + slot * 2u, (uint32_t)result);
    guest_fn_484(source, (uint32_t)(result >> 32), flags, selector);
    return free_index;
}
