/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* 0x9cde8, 154 bytes. Read count bytes into dst: with flag bits 16/17 from the archive context at 0x40d1f0/0x40d1f4 (0xebc50), with bit 18 from the in-memory block at 0x40d1ec/0x40d1e4 (padding with 0xff), otherwise from the handle in flags. */
uint32_t read_bytes_9cde8(uint32_t dst, uint32_t count, uint32_t flags)
{
    if (flags & 0x30000u) {
        uint32_t rest = flags & 0xfffcffffu;
        uint32_t ctx = (flags & 0x10000u) ? EF_U32(0x0040d1f0u) : EF_U32(0x0040d1f4u);
        return read_archive_ebc50(dst, count, rest, ctx);
    }
    if (flags & 0x40000u) {
        int32_t avail = (int32_t)(EF_U32(0x0040d1e4u) - EF_U32(0x0040d1ecu));
        if ((int32_t)count > avail) {
            fill_bytes_b3fb0(dst + (uint32_t)avail, 0xffu, count - (uint32_t)avail);
            count = (uint32_t)avail;
        }
        guest_fn_dc832(dst, EF_U32(0x0040d1ecu), count);
        EF_U32(0x0040d1ecu) = EF_U32(0x0040d1ecu) + count;
        return count;
    }
    return guest_fn_a79d4_ret(flags, count, dst);
}
