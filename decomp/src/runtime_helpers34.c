/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xb3b90, 114 bytes. Return a resource's payload length. JR includes its
 * sixteen-byte wrapper; RA stores the payload length at header offset four.
 * Other files use their actual length. The original ignores a short read. */
uint32_t read_file_payload_size_b3b90(ef_addr32 name)
{
    int32_t handle = open_slot_a7900(0, 0, 0, 0, name);
    uint32_t size = 0;

    if (handle == -1)
        return 0;
    EF_U16(0x0041f360u) = 0;
    (void)read_handle_a79d4(0, 0, 0, 0, (uint32_t)handle, 8u, 0x0041f360u);
    if (EF_U16(0x0041f360u) == 0x524au)
        size = EF_U32(0x0041f364u) + 16u;
    else if (EF_U16(0x0041f360u) == 0x4152u)
        size = EF_U32(0x0041f364u);
    else
        size = seek_end_a7ae0(0, 0, 0, 0, (uint32_t)handle, 0);
    close_handle_a7990(0, 0, 0, 0, (uint32_t)handle);
    return size;
}

/* asctime-style time formatting. */

/* 0xf9d98, 51 bytes. Write a value (0..99) as two ASCII digits at buffer[pos]. */
void put_two_digits_f9d98(int32_t value, uint32_t pos, ef_addr32 buffer)
{
    EF_U8(buffer + pos) = (uint8_t)(value / 10 + 0x30);
    EF_U8(buffer + pos + 1u) = (uint8_t)(value % 10 + 0x30);
}

/* 0xf9dcb, 233 bytes. asctime: format a broken-down time record into "Www Mmm dd hh:mm:ss yyyy\n"; returns the buffer. */
ef_addr32 format_time_f9dcb(ef_addr32 tm, ef_addr32 out)
{
    uint32_t wday = EF_U32(tm + 0x18u);
    uint32_t mon = EF_U32(tm + 0x10u);
    int32_t year = EF_S32(tm + 0x14u);

    EF_U8(out) = EF_U8(0x00195c40u + wday);
    EF_U8(out + 1u) = EF_U8(0x00195c47u + wday);
    EF_U8(out + 2u) = EF_U8(0x00195c4eu + wday);
    EF_U8(out + 3u) = 0x20;
    EF_U8(out + 4u) = EF_U8(0x00195c1cu + mon);
    EF_U8(out + 5u) = EF_U8(0x00195c28u + mon);
    EF_U8(out + 6u) = EF_U8(0x00195c34u + mon);
    EF_U8(out + 7u) = 0x20;
    put_two_digits_f9d98(EF_S32(tm + 0xcu), 8, out);
    if (EF_U8(out + 8u) == 0x30)
        EF_U8(out + 8u) = 0x20;
    EF_U8(out + 0xau) = 0x20;
    put_two_digits_f9d98(EF_S32(tm + 8u), 0xb, out);
    EF_U8(out + 0xdu) = 0x3a;
    put_two_digits_f9d98(EF_S32(tm + 4u), 0xe, out);
    EF_U8(out + 0x10u) = 0x3a;
    put_two_digits_f9d98(EF_S32(tm), 0x11, out);
    EF_U8(out + 0x13u) = 0x20;
    put_two_digits_f9d98(year / 100 + 0x13, 0x14, out);
    put_two_digits_f9d98(year % 100, 0x16, out);
    EF_U8(out + 0x18u) = 0xa;
    EF_U8(out + 0x19u) = 0;
    return out;
}
