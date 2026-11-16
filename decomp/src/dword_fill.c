/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define FILL_WORD(a) (*(volatile uint32_t *)ef_guest_memory_ptr(a))
#define FILL_BYTE(a) (*(volatile uint8_t *)ef_guest_memory_ptr(a))

/* Watcom discards a void-cast volatile byte expression; spell the original
 * read instruction explicitly. Pack the secondary result without U8 helpers,
 * and restore the original EDX pattern before returning. */
static void touch_fill_ahead(ef_addr32 address, uint32_t value);
static uint64_t fill_result(ef_addr32 end, uint32_t residual, uint32_t value);
#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux touch_fill_ahead = "cmp byte ptr [eax],dl" \
    parm [eax] [edx] modify []
#pragma aux fill_result = "" parm [eax] [ecx] [edx] value [eax ecx] modify []
#else
static void touch_fill_ahead(ef_addr32 address, uint32_t value)
{
    volatile uint8_t byte = FILL_BYTE(address);
    (void)byte;
    (void)value;
}
static uint64_t fill_result(ef_addr32 end, uint32_t residual, uint32_t value)
{
    (void)value;
    return ((uint64_t)residual << 32) | (uint64_t)end;
}
#endif

/* Four stores retain their original order, including overlapping regions. */
static void fill_four(ef_addr32 dst, uint32_t value)
{
    FILL_WORD(dst) = value;
    FILL_WORD(dst + 4u) = value;
    FILL_WORD(dst + 8u) = value;
    FILL_WORD(dst + 12u) = value;
}

/* 0xdeb87,108 bytes. EAX destination, EDX pattern, ECX dword count.
 * Return low half in EAX (end), high half in ECX (residual). Plain RET.
 * The original's third tail store leaves ECX equal to one. */
uint64_t fill_dwords_deb87(ef_addr32 dst, uint32_t value, uint32_t count)
{
    uint32_t groups;
    uint32_t tail;

    if (count == 0u)
        return fill_result(dst, 0u, value);
    while ((dst & 31u) != 0u && count != 0u) {
        FILL_WORD(dst) = value;
        dst += 4u;
        count--;
    }
    groups = count >> 2;
    while (groups > 1u) {
        fill_four(dst, value);
        groups--;
        if (groups == 1u) {
            dst += 16u;
            break;
        }
        /* Preserve the original CMP byte read, including its possible fault. */
        touch_fill_ahead(dst + 32u, value);
        fill_four(dst + 16u, value);
        groups--;
        dst += 32u;
    }
    if (groups != 0u) {
        fill_four(dst, value);
        dst += 16u;
    }
    tail = count & 3u;
    if (tail != 0u) {
        FILL_WORD(dst) = value;
        dst += 4u;
        tail--;
        if (tail != 0u) {
            FILL_WORD(dst) = value;
            dst += 4u;
            tail--;
            if (tail != 0u) {
                FILL_WORD(dst) = value;
                dst += 4u;
            }
        }
    }
    return fill_result(dst, tail, value);
}
