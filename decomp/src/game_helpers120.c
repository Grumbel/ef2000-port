/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small record and palette helpers. */

/* 0x114170, 47 bytes. Activate a record: trace it, set state 2 and run its field 0x28 callback (cdecl, record on the stack). */
void activate_record_114170(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    ef_addr32 callback;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (rec != 0 && EF_U32(rec + 4u) != 1u) {
        trace_call_103148(rec);
        callback = EF_U32(rec + 0x28u);
        EF_U32(rec + 4u) = 2;
        if (callback != 0)
            guest_callptr1_x(callback, rec);
    }
}

/* 0x1115fc, 54 bytes. Tear down a record: run its field 0x874 callback, trace-call 0x102644 if field 0x88c is positive, then set it to -1 (cdecl, record on the stack). */
void teardown_record_1115fc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(rec + 0x874u) != 0)
        guest_callptr1_x(EF_U32(rec + 0x874u), rec);
    if (EF_S32(rec + 0x88cu) > 0)
        trace_call_102644(rec);
    EF_S32(rec + 0x88cu) = -1;
}

/* 0xd660c, 46 bytes. Remap the 256-entry palette at the stack argument and compare the result with the 0x300 bytes at 0x420a90 (cdecl; returns the memcmp result). */
int32_t compare_remapped_palette_d660c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 palette)
{
    uint8_t buf[0x300];
    (void)u0; (void)u1; (void)u2; (void)u3;
    remap_palette_d65a8((uint32_t)(uintptr_t)buf, palette);
    return guest_memcmp((uint32_t)(uintptr_t)buf, 0x00420a90u, 0x300u);
}

/* 0xd6df0, 49 bytes. Register the stack ranges 0xd7108 (0x48 bytes) and 0x1048b4 (0x400 bytes). */
void register_stack_ranges_d6df0(void)
{
    stack_range_104908(0x000d7108u, 0x000d7150u - 0x000d7108u);
    stack_range_104908(0x001048b4u, 0x400u);
}

/* 0xdc8d8, 67 bytes. Angle of the vector (x, y) after halving it until both components fit in 16 bits. */
int32_t angle_of_fitted_pair_dc8d8(int32_t x, int32_t y)
{
    int32_t pair[2];
    pair[0] = x;
    pair[1] = y;
    if (x < -0x8000 || x > 0x7fff || y < -0x8000 || y > 0x7fff)
        halve_pair_to_fit16((uint32_t)(uintptr_t)&pair[0], (uint32_t)(uintptr_t)&pair[1]);
    return angle_of_b3b1c(pair[0], pair[1]);
}
