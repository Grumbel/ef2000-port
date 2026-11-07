/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Request wrappers, lookup tables and small predicates. */

/* 0xa37c4, 31 bytes. Send a one-byte request with code 8. */
void send_byte_request_8_a37c4(uint8_t value)
{
    uint8_t buf = value;

    issue_request_a3e50((uint32_t)(uintptr_t)&buf, 1, 8);
}

/* 0xa37e4, 31 bytes. Send a one-byte request with code 9. */
void send_byte_request_9_a37e4(uint8_t value)
{
    uint8_t buf = value;

    issue_request_a3e50((uint32_t)(uintptr_t)&buf, 1, 9);
}

/* 0xa3804, 31 bytes. Send a one-byte request with code 10. */
void send_byte_request_10_a3804(uint8_t value)
{
    uint8_t buf = value;

    issue_request_a3e50((uint32_t)(uintptr_t)&buf, 1, 10);
}

/* 0xa3f0c, 33 bytes. Send a zero byte request with code 5. */
void send_zero_request_5_a3f0c(void)
{
    uint8_t buf = 0;

    issue_request_a3e50((uint32_t)(uintptr_t)&buf, 1, 5);
}

/* 0xbbae4, 26 bytes. Map a code 0..26 to 9 or 10 (0 when out of range). */
uint8_t map_code_9_10_bbae4(uint8_t code)
{
    static const uint8_t table[27] = { 10, 10, 9, 9, 9, 10, 10, 10, 9, 9, 9, 10, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 9, 9 };

    return code > 0x1a ? 0 : table[code];
}

/* 0xbbb34, 26 bytes. Map a code 0..12 to 5 or 6 (0 when out of range). */
uint8_t map_code_5_6_bbb34(uint8_t code)
{
    return code > 0xc ? 0 : (code >= 11 ? 6 : 5);
}

/* 0xbbbe4, 23 bytes. Map a code 0..36 to a flag. */
uint8_t map_code_flag_bbbe4(uint8_t code)
{
    static const uint8_t table[37] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0 };

    return code > 0x24 ? 0 : table[code];
}

/* 0xbbd8c, 37 bytes. Map a code 1..100 to a class 0..8 (0 when out of range). */
uint8_t map_code_class_bbd8c(uint8_t code)
{
    static const uint8_t table[100] = {
        7, 7, 7, 2, 0, 2, 3, 4, 2, 7, 0, 8, 4, 2, 2, 2, 2, 2, 0, 2,
        2, 2, 2, 2, 3, 2, 2, 2, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 2,
        2, 2, 0, 2, 2, 2, 2, 2, 0, 4, 2, 2, 2, 8, 8, 2, 0, 7, 7, 7,
        7, 7, 7, 7, 3, 3, 3, 0, 0, 0, 0, 3, 0, 2, 2, 2, 3, 2, 0, 2,
        2, 2, 2, 2, 2, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
    };
    uint8_t idx = (uint8_t)(code - 1);

    return idx > 0x63 ? 0 : table[idx];
}

/* 0x940e0, 8 bytes. Whether the scene record pointer is unset. */
uint8_t scene_record_unset_940e0(void)
{
    return EF_U32(0x00214458u) == 0;
}

/* 0x9c1bb, 37 bytes. Convert a real-mode segment:offset dword to a linear address. */
uint32_t real_mode_to_linear_9c1bb(uint32_t seg_off)
{
    return (seg_off >> 16) * 16u + EF_U32(0x00120cc6u) + (seg_off & 0xffffu);
}

/* 0x5ca60, 38 bytes. Read the 9th stream value into a local and apply it when valid. */
void apply_stream_value_5ca60(void)
{
    uint32_t local;
    int32_t r = guest_fn_86f7c_x(9, (uint32_t)(uintptr_t)&local);

    if (r >= 0)
        guest_fn_5c9d4_x((uint32_t)(uintptr_t)&local, (uint32_t)r);
}
