/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Time zone rule parsing and chunk lookup. */

/* 0x10eef0, 242 bytes. Parse a TZ rule ("Jn", "n" or "Mm.w.d", optionally "/h:m:s") into the record at out; returns the text position after the rule. Fields: +0 seconds, +4 minutes, +8 hours (default 2), +0xc week, +0x10 month-1, +0x18 weekday, +0x1c day of year, +0x20 kind (1 = J, -1 = plain day, 0 = M). */
ef_addr32 parse_tz_rule_10eef0(ef_addr32 text, ef_addr32 out)
{
    uint32_t tmp;
    uint32_t hms[3];
    int32_t kind = -1;
    ef_addr32 p = text;

    if (EF_U8(p) == 0x4a) {
        kind = 1;
        p += 1u;
    }
    if (EF_U8(p) == 0x4d) {
        p++;
        kind = 0;
    }
    EF_S32(out + 0x20u) = kind;
    p = parse_decimal(p, (uint32_t)(uintptr_t)&tmp);
    if (kind != 0) {
        EF_U32(out + 0x1cu) = tmp;
    } else {
        EF_U32(out + 0x10u) = tmp - 1u;
        if (EF_U8(p) == 0x2e) {
            p = parse_decimal(p + 1u, (uint32_t)(uintptr_t)&tmp);
            EF_U32(out + 0xcu) = tmp;
            if (EF_U8(p) == 0x2e) {
                p = parse_decimal(p + 1u, (uint32_t)(uintptr_t)&tmp);
                EF_U32(out + 0x18u) = tmp;
            }
        }
        EF_U32(out + 0x1cu) = 0;
    }
    hms[0] = 0;
    hms[1] = 0;
    hms[2] = 2;
    if (EF_U8(p) == 0x2f) {
        p = parse_decimal(p + 1u, (uint32_t)(uintptr_t)&hms[2]);
        if (EF_U8(p) == 0x3a) {
            p = parse_decimal(p + 1u, (uint32_t)(uintptr_t)&hms[1]);
            if (EF_U8(p) == 0x3a)
                p = parse_decimal(p + 1u, (uint32_t)(uintptr_t)&hms[0]);
        }
    }
    EF_U32(out) = hms[0];
    EF_U32(out + 4u) = hms[1];
    EF_U32(out + 8u) = hms[2];
    return p;
}

/* 0x111f84, 196 bytes. Walk a chunk stream (4-byte tags, big-endian sizes): skip chunks until one with tag 0x1953b8 or 0x1953c0 whose sub-tag at +8 is 0x1953c8. A 0x1953b8 chunk is returned when n == 0; a 0x1953c0 container is searched for its n-th sub-chunk with the 0x1953c8 sub-tag. Returns 0 when absent (cdecl: data, n). */
ef_addr32 find_chunk_111f84(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 data, int32_t n)
{
    ef_addr32 p = data;
    uint32_t size = 0;
    ef_addr32 end;
    (void)u0; (void)u1; (void)u2; (void)u3;

    for (;;) {
        p += size;
        if (compare_strings_e5c03(p, 0x001953b8u, 4u) != 0 && compare_strings_e5c03(p, 0x001953c0u, 4u) != 0)
            return 0;
        size = byte_swap_111f50(EF_U32(p + 4u)) + 8u;
        if (compare_strings_e5c03(p + 8u, 0x001953c8u, 4u) == 0)
            break;
    }
    if (compare_strings_e5c03(p, 0x001953b8u, 4u) == 0)
        return n != 0 ? 0 : p;
    end = p + size;
    p += 0xcu;
    while (p < end) {
        if (compare_strings_e5c03(p + 8u, 0x001953c8u, 4u) == 0) {
            n--;
            if (n == -1)
                return p;
        }
        p += byte_swap_111f50(EF_U32(p + 4u)) + 8u;
    }
    return 0;
}

/* 0x10ec62, 277 bytes. Break a day count and seconds-of-day (minus a zone offset) into a broken-down time record: +0 sec, +4 min, +8 hour, +0xc day of month, +0x10 month, +0x14 year (from 1900), +0x18 weekday, +0x1c day of year; returns the record. */
ef_addr32 break_down_time_10ec62(uint32_t days, uint32_t secs, int32_t offset, ef_addr32 tm)
{
    uint32_t rem;
    uint32_t year;
    uint32_t yday;
    uint32_t year_len;
    uint32_t month;
    ef_addr32 table;

    if (secs < 0xa8c0u && offset > 0) {
        secs = secs + 0x15180u - (uint32_t)offset;
        days = days + secs / 0x15180u - 1u;
    } else {
        secs = secs - (uint32_t)offset;
        days = days + secs / 0x15180u;
    }
    rem = secs % 0x15180u;
    EF_U32(tm + 8u) = rem / 0xe10u;
    rem %= 0xe10u;
    EF_U32(tm + 4u) = rem / 0x3cu;
    EF_U32(tm) = rem % 0x3cu;

    year = days / 0x16eu;
    yday = days - year * 0x16du;
    if (year > 0)
        yday -= (year - 1u) >> 2;
    year_len = is_leap_year(year + 0x76cu) + 0x16du;
    /* ORIGINAL BUG: the year length is not recomputed as the year advances. */
    while (yday >= year_len) {
        year++;
        yday -= year_len;
    }
    EF_U32(tm + 0x1cu) = yday;
    EF_U32(tm + 0x14u) = year;
    table = 0x00195cb0u;
    if (is_leap_year(year + 0x76cu) != 0)
        table = 0x00195ccau;
    month = yday / 0x1fu;
    if (yday >= EF_U16(table + month * 2u + 2u))
        month++;
    EF_U32(tm + 0x10u) = month;
    EF_U32(tm + 0xcu) = yday - (uint32_t)EF_S16(table + month * 2u) + 1u;
    EF_U32(tm + 0x18u) = (days + 1u) % 7u;
    return tm;
}
