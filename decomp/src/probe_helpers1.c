/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Hardware probe helpers. */

/* 0xd9b30, 112 bytes. Probe base values 0x6000 + n*0x100 (n = 0..16) with the 0x8b8 service 0x33 and return the first value whose reply echoes 0x7fXX, or 0. */
uint32_t probe_service_d9b30(void)
{
    uint32_t ebx = 0x60u, ecx = 0x6000u;
    do {
        uint32_t esi = ecx | 0x7fu;
        uint32_t edi = ebx;
        EF_U32(0x00420ed8u) = 0u;
        EF_U32(0x00420ed4u) = 0u;
        EF_U32(0x00420ed0u) = 0u;
        EF_U32(0x00420edcu) = esi;
        guest_fn_8b8(0x33u, 0x00420ec0u);
        if ((EF_U32(0x00420edcu) & 0xffffu) == ((edi | 0x7f00u) & 0xffffu))
            return esi & 0xff00u;
        ebx++;
        ecx += 0x100u;
    } while ((int32_t)ebx <= 0x70);
    return 0u;
}

/* 0xe9ac0, 117 bytes. Run the probe selected by the state word at 0x464472 (1, 2 or 3), print a message when it reports failure, and return the state. */
uint32_t run_probe_e9ac0(void)
{
    uint32_t state = EF_U16(0x00464472u);
    switch (state) {
    case 1:
        if ((uint16_t)probe_service_d9b30() == 0u)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x001913fcu);
        break;
    case 2:
        if ((uint16_t)open_stream_formats_d9c30() == 0u)
            guest_callptr2_x(EF_U32(0x00132a74u), 0x00191428u, (uint32_t)EF_S16(0x00464470u));
        break;
    case 3:
        if ((uint16_t)return_zero_10d9e0() == 0u)
            guest_callptr2_x(EF_U32(0x00132a74u), 0x0019145cu, (uint32_t)EF_S16(0x00464470u));
        break;
    default:
        break;
    }
    return EF_U16(0x00464472u);
}

/* 0x1e214, 117 bytes. For an open entry, rotate the query point (x, y, z) by the entry's orientation (0..3) and walk the script cursor until one of its boxes contains the point (1) or the entries run out (0). */
uint32_t point_in_entry_boxes_1e214(uint32_t entry, int32_t x, int32_t y, int32_t z)
{
    int32_t px, pz;
    int16_t page = (int16_t)entry;
    if (open_entry_cd150(page) == 0u)
        return 0u;
    switch (record_flag_bits56(page)) {
    case 0: px = x; pz = z; break;
    case 1: px = -z; pz = x; break;
    case 2: px = -x; pz = -z; break;
    case 3: px = z; pz = -x; break;
    default: px = x; pz = z; break;
    }
    do {
        if (point_in_box_1e1c0(script_cursor_data_cd29c(), px, y, pz) != 0u)
            return 1u;
    } while (advance_cursor() != 0u);
    return 0u;
}
