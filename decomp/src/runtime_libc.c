/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Runtime-library and timer helpers in 0x10e438..0x111569. */

/* 0x10e438, 18 bytes. 0x11d258(0x195168, 1). */
void call_11d258_default(void)
{
    guest_fn_11d258(0x00195168u, 1);
}

/* 0x10e46f, 73 bytes. Convert EAX to text in radix EBX at EDX using the digit table at 0x10e44a;
 * returns the destination start. */
ef_addr32 unsigned_to_text(uint32_t value, ef_addr32 destination, uint32_t radix)
{
    uint8_t digits[0x28];
    uint32_t count = 0;
    uint8_t c;
    ef_addr32 out = destination;
    do {
        digits[count++] = EF_U8(0x0010e44au + value % radix);
        value /= radix;
    } while (value != 0);
    do {
        c = digits[--count];
        EF_U8(out) = c;
        ++out;
    } while (c != 0);
    return destination;
}

/* 0x10e4b8, 27 bytes. Signed conversion: radix 10 negatives get a leading '-'. */
ef_addr32 signed_to_text(int32_t value, ef_addr32 destination, uint32_t radix)
{
    ef_addr32 out = destination;
    if (radix == 10 && value < 0) {
        value = -value;
        EF_U8(out) = 0x2d;
        ++out;
    }
    unsigned_to_text((uint32_t)value, out, radix);
    return destination;
}

/* 0x10e837, 59 bytes. Gregorian leap year. */
uint32_t is_leap_year(int32_t year)
{
    if ((year & 3) != 0)
        return 0;
    if (year % 100 != 0)
        return 1;
    return year % 400 == 0;
}

/* 0x10e957, 40 bytes. 0x10e872(a, c) > 0x10e872(b, c). */
uint32_t compare_dates_10e957(uint32_t first, uint32_t second, uint32_t third)
{
    int32_t a = (int32_t)guest_fn_10e872(first, third);
    int32_t b = (int32_t)guest_fn_10e872(second, third);
    return a > b;
}

/* 0x10ec32, 48 bytes. Three-dword signed lexicographic less-than (most significant dword at +8). */
uint32_t triple_less_than(ef_addr32 first, ef_addr32 second)
{
    if (EF_S32(first + 8u) != EF_S32(second + 8u))
        return EF_S32(first + 8u) < EF_S32(second + 8u);
    if (EF_S32(first + 4u) != EF_S32(second + 4u))
        return EF_S32(first + 4u) < EF_S32(second + 4u);
    return EF_S32(first) < EF_S32(second);
}

/* 0x10edb4, 38 bytes. Parse decimal digits at EAX into *EDX; returns the pointer after them. */
ef_addr32 parse_decimal(ef_addr32 text, ef_addr32 result)
{
    uint32_t value = 0;
    while (EF_U8(text) >= 0x30 && EF_U8(text) <= 0x39) {
        value = value * 10u + EF_U8(text) - 0x30u;
        ++text;
    }
    EF_U32(result) = value;
    return text;
}

/* 0x10f190, 47 bytes. Read one byte from a counted buffer stream; -1 at the end. */
int32_t stream_getc(ef_addr32 stream)
{
    if (guest_fn_10f1bf(stream) == 0)
        return -1;
    EF_U32(stream + 4u) = EF_U32(stream + 4u) - 1u;
    EF_U32(stream) = EF_U32(stream) + 1u;
    return EF_U8(EF_U32(stream) - 1u);
}

/* 0x10f4df, 17 bytes. 0xfa95f(EAX, 0, 1). */
uint32_t call_fa95f_from_start(uint32_t handle)
{
    return guest_fn_fa95f(handle, 0, 1);
}

/* 0x1100b1, 33 bytes. Elapsed value of 0x11004b relative to dword 0x4dd984, wrapping at 0x83d600. */
uint32_t elapsed_since_start(void)
{
    uint32_t now = guest_fn_11004b();
    uint32_t base = EF_U32(0x004dd984u);
    if (now < base)
        now += 0x83d600u;
    return now - base;
}

/* 0x110123, 13 bytes. 0x1100d2(EAX, 0x4dd988). */
uint32_t call_1100d2_default(uint32_t value)
{
    return guest_fn_1100d2(value, 0x004dd988u);
}

/* 0x1102a3, 21 bytes. Store value | 0x4000 in dword table 0x17d614[index]. */
void store_flagged_value(uint32_t index, uint32_t value)
{
    EF_U32(EF_U32(0x0017d614u) + index * 4u) = value | 0x4000u;
}

/* 0x11024e, 85 bytes. Look up dword table 0x17d614[index], marking the first six slots as seen. */
uint32_t lookup_slot_11024e(int32_t index)
{
    uint32_t table;
    if ((uint32_t)index >= EF_U32(0x0017d5c0u))
        return 0;
    if (index <= 5) {
        table = EF_U32(0x0017d614u);
        if (!(EF_U8(table + (uint32_t)index * 4u + 1u) & 0x40u)) {
            EF_U8(table + (uint32_t)index * 4u + 1u) |= 0x40u;
            if (guest_fn_1101be((uint32_t)index) != 0) {
                table = EF_U32(0x0017d614u);
                EF_U8(table + (uint32_t)index * 4u + 1u) |= 0x20u;
            }
        }
    }
    return EF_U32(EF_U32(0x0017d614u) + (uint32_t)index * 4u);
}

/* 0x110dcc, 13 bytes. */
void increment_17d7ba(void)
{
    EF_U32(0x0017d7bau) = EF_U32(0x0017d7bau) + 1u;
}

/* 0x110dd9, 13 bytes. */
void decrement_17d7ba(void)
{
    EF_U32(0x0017d7bau) = EF_U32(0x0017d7bau) - 1u;
}

/* 0x110f3c, 51 bytes. 0x110f0c(value < 0xd68d ? value * 10000 / 0x20bc : 0). */
uint32_t scale_110f3c(uint32_t value)
{
    uint32_t scaled = 0;
    if (value < 0xd68du)
        scaled = (uint32_t)(((uint64_t)value * 0x2710u) / 0x20bcu);
    return guest_fn_110f0c(scaled);
}

/* 0x111359, 12 bytes. */
uint32_t value_17d7b6(void)
{
    return EF_U32(0x0017d7b6u);
}

/* 0x111569, 12 bytes. */
uint32_t value_17d7ae(void)
{
    return EF_U32(0x0017d7aeu);
}

/* 0x1113ac, 40 bytes. Exchange dword 0x17d764[index] under the lock 0x101e70/0x101e78. */
uint32_t EF_CDECL exchange_slot_1113ac(uint32_t index, uint32_t value)
{
    uint32_t result = guest_fn_101e70();
    if (index != 0xffffffffu) {
        result = EF_U32(0x0017d764u + index);
        EF_U32(0x0017d764u + index) = value;
    }
    guest_fn_101e78();
    return result;
}

/* 0x1113fb, 36 bytes. Call trace_call_102384 for the slots 0x38 down to 0. */
void for_each_slot_1113fb(void)
{
    int32_t slot;
    guest_fn_101e70();
    for (slot = 0x38; slot >= 0; slot -= 4)
        trace_call_102384((uint32_t)slot);
    guest_fn_101e78();
}

/* 0x11144f, 36 bytes. As 0x1113fb for 0x1022dc. */
void for_each_slot_11144f(void)
{
    int32_t slot;
    guest_fn_101e70();
    for (slot = 0x38; slot >= 0; slot -= 4)
        trace_call_1022dc((uint32_t)slot);
    guest_fn_101e78();
}

/* 0x1114a3, 36 bytes. As 0x1113fb for 0x102330. */
void for_each_slot_1114a3(void)
{
    int32_t slot;
    guest_fn_101e70();
    for (slot = 0x38; slot >= 0; slot -= 4)
        trace_call_102330((uint32_t)slot);
    guest_fn_101e78();
}

/* 0x1114f7, 48 bytes. 0x1021f4(a, 1000000 / b) under the lock. */
void EF_CDECL set_rate_1114f7(uint32_t slot, uint32_t divisor)
{
    guest_fn_101e70();
    trace_call_1021f4(slot, 1000000u / divisor);
    guest_fn_101e78();
}

/* 0x111527, 66 bytes. 0x1021f4(a, b == 0 ? 0xd68d : 10000 * b / 0x2e9c) under the lock. */
void EF_CDECL set_period_111527(uint32_t slot, uint32_t value)
{
    uint32_t scaled;
    guest_fn_101e70();
    if (value == 0)
        scaled = 0xd68du;
    else
        scaled = (uint32_t)(((uint64_t)0x2710u * value) / 0x2e9cu);
    trace_call_1021f4(slot, scaled);
    guest_fn_101e78();
}
