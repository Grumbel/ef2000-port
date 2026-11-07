/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Functions in 0x103244..0x10dfe0; stack-argument functions are cdecl unless noted. */

/* 0x103244, 52 bytes. Trace wrapper for 0x1141a0. */
void trace_call_103244(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001942b4u, a, b);
    guest_fn_1141a0(a, b);
}

/* 0x1033c8, 52 bytes. Trace wrapper for 0x1142cc. */
void trace_call_1033c8(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00194350u, a, b);
    guest_fn_1142cc(a, b);
}

/* 0x103504, 44 bytes. Trace wrapper for 0x113c24. */
void trace_call_103504(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001943e0u, a);
    guest_fn_113c24(a);
}

/* 0x1037e0, 52 bytes. Trace wrapper for 0x11458c. */
void trace_call_1037e0(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00194524u, a, b);
    guest_fn_11458c(a, b);
}

/* 0x103b5c, 52 bytes. Trace wrapper for 0x114878. */
void trace_call_103b5c(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001946c8u, a, b);
    guest_fn_114878(a, b);
}

/* 0x103b90, 58 bytes. Trace wrapper for 0x114994. */
void trace_call_103b90(uint32_t a, uint32_t b, uint32_t c)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001946e8u, a, b, c);
    guest_fn_114994(a, b, c);
}

/* 0x103d48, 42 bytes. If dword 0x17d2cc is set, run 0x1048b4(0x101a90, 0x103d48) and clear it. */
void shutdown_103d48(void)
{
    if (EF_U32(0x0017d2ccu) != 0) {
        guest_fn_1048b4(0x00101a90u, 0x00103d48u);
        EF_U32(0x0017d2ccu) = 0;
    }
}

/* 0x1041e4, 25 bytes. verified: diff+capture+replays. Exchange dword 0x4dd7cc[index]; return old value. */
uint32_t EF_CDECL exchange_slot_1041e4(uint32_t index, uint32_t value)
{
    uint32_t old = EF_U32(0x004dd7ccu + index * 4u);
    EF_U32(0x004dd7ccu + index * 4u) = value;
    return old;
}

/* 0x104694, 45 bytes. Call 0x102488 on each non-null dword 0x4dd670[0..15], then 0x1023b0 and 0x1110c5. */
void shutdown_104694(void)
{
    int32_t offset;
    for (offset = 0x3c; offset != -4; offset -= 4) {
        uint32_t value = EF_U32(0x004dd670u + (uint32_t)offset);
        if (value != 0)
            trace_call_102488(value);
    }
    guest_fn_1023b0();
    guest_fn_1110c5();
}

/* 0x1047a4, 32 bytes. Allocate with 0xc3f70(size) and, if successful, run 0x104908(block, size). */
uint32_t allocate_and_init_1047a4(uint32_t size)
{
    uint32_t block = guest_fn_c3f70(size);
    if (block != 0)
        stack_range_104908(block, size);
    return block;
}

/* 0x1047c4, 32 bytes. For a non-null block run 0x104920(block, size) then free it with 0xc3f80. */
void free_block_1047c4(uint32_t block, uint32_t size)
{
    if (block == 0)
        return;
    stack_range_104920(block, size);
    guest_fn_c3f80(block);
}

/* 0x104908, 21 bytes. 0x104840(base, base + length). */
void stack_range_104908(uint32_t base, uint32_t length)
{
    guest_fn_104840(base, base + length);
}

/* 0x104920, 21 bytes. 0x1048b4(base, base + length). */
void stack_range_104920(uint32_t base, uint32_t length)
{
    guest_fn_1048b4(base, base + length);
}

/* 0x105334, 40 bytes. If dword +0x54 is set, send message 0x402 through 0x101f88 and clear it. */
void stop_object_105334(ef_addr32 object)
{
    if (EF_U32(object + 0x54u) == 0)
        return;
    guest_fn_101f88(EF_U32(object), 0x402u, 0, 0);
    EF_U32(object + 0x54u) = 0;
}

/* 0x106634, 16 bytes. Dword +4 = 1. */
#if defined(__WATCOMC__)
static void set_state_1_106634_original_sequence(void);
#pragma aux set_state_1_106634_original_sequence = \
    0x8b 0x44 0x24 0x04 0x85 0xc0 0x74 0x07 \
    0xc7 0x40 0x04 0x01 0x00 0x00 0x00 \
    __modify [eax]
#endif
void EF_CDECL set_state_1_106634(ef_addr32 object)
{
#if defined(__WATCOMC__)
    (void)object;
    set_state_1_106634_original_sequence();
#else
    if (object != 0)
        EF_U32(object + 4u) = 1;
#endif
}

/* 0x106708, 13 bytes. Dword +4, or 0 for null. */
uint32_t EF_CDECL get_state_106708(ef_addr32 object)
{
    if (object == 0)
        return 0;
#if defined(__WATCOMC__)
    return *(uint32_t *)(uintptr_t)(object + 4u);
#else
    return EF_U32(object + 4u);
#endif
}

/* 0x106718, 37 bytes. Reset +0xc and +0x14, set +8 and +0x10. */
void set_range_106718(ef_addr32 object, uint32_t first, uint32_t second)
{
    if (object == 0)
        return;
    EF_U32(object + 0xcu) = 0;
    EF_U32(object + 0x14u) = 0;
    EF_U32(object + 8u) = first;
    EF_U32(object + 0x10u) = second;
}

/* 0x106740, 42 bytes. Update +0x34 / +0x38 and notify through 0x10535c when they change. */
void set_pair_106740(ef_addr32 object, uint32_t first, uint32_t second)
{
    if (object == 0)
        return;
    if (first == EF_U32(object + 0x34u) && second == EF_U32(object + 0x38u))
        return;
    EF_U32(object + 0x34u) = first;
    EF_U32(object + 0x38u) = second;
    guest_fn_10535c(object);
}

/* 0x10677c, 16 bytes. Dword +0x3c = value. */
#if defined(__WATCOMC__)
static void set_field_10677c_original_sequence(void);
#pragma aux set_field_10677c_original_sequence = \
    0x8b 0x44 0x24 0x04 0x85 0xc0 0x74 0x07 \
    0x8b 0x54 0x24 0x08 0x89 0x50 0x3c \
    __modify [eax edx]
#endif
void EF_CDECL set_field_10677c(ef_addr32 object, uint32_t value)
{
#if defined(__WATCOMC__)
    (void)object;
    (void)value;
    set_field_10677c_original_sequence();
#else
    if (object != 0)
        EF_U32(object + 0x3cu) = value;
#endif
}

/* 0x10679c, 30 bytes. Update +0x40 and notify when it changes. */
void set_field_10679c(ef_addr32 object, uint32_t value)
{
    if (object == 0 || value == EF_U32(object + 0x40u))
        return;
    EF_U32(object + 0x40u) = value;
    guest_fn_10535c(object);
}

/* 0x1067cc, 30 bytes. As 0x10679c for +0x44. */
void set_field_1067cc(ef_addr32 object, uint32_t value)
{
    if (object == 0 || value == EF_U32(object + 0x44u))
        return;
    EF_U32(object + 0x44u) = value;
    guest_fn_10535c(object);
}

/* 0x10682c, 16 bytes. Dword +0x30 = value. */
#if defined(__WATCOMC__)
static void set_field_10682c_original_sequence(void);
#pragma aux set_field_10682c_original_sequence = \
    0x8b 0x44 0x24 0x04 0x85 0xc0 0x74 0x07 \
    0x8b 0x54 0x24 0x08 0x89 0x50 0x30 \
    __modify [eax edx]
#endif
void EF_CDECL set_field_10682c(ef_addr32 object, uint32_t value)
{
#if defined(__WATCOMC__)
    (void)object;
    (void)value;
    set_field_10682c_original_sequence();
#else
    if (object != 0)
        EF_U32(object + 0x30u) = value;
#endif
}

/* 0x10683c, 62 bytes. */
void stop_selected_10683c(ef_addr32 object)
{
    ef_addr32 entry;
    if (object == 0 || EF_U32(object + 4u) == 1)
        return;
    entry = object + (EF_U32(object + 0x28u) << 2);
    if (EF_U32(entry + 0x10u) == 0 || EF_U32(entry + 8u) == 0)
        return;
    EF_U32(entry + 0x18u) = 0;
    EF_U32(object + 4u) = 4;
    guest_fn_1052f0(EF_U32(object));
}

/* 0x10687c, 22 bytes. State 4 becomes 8. */
void EF_CDECL pause_object_10687c(ef_addr32 object)
{
    if (object != 0 && EF_U32(object + 4u) == 4)
        EF_U32(object + 4u) = 8;
}

/* 0x106894, 33 bytes. State 8 becomes 4 and 0x1052f0 runs. */
void resume_object_106894(ef_addr32 object)
{
    if (object != 0 && EF_U32(object + 4u) == 8) {
        uint32_t handle = EF_U32(object);
        EF_U32(object + 4u) = 4;
        guest_fn_1052f0(handle);
    }
}

/* 0x106938, 28 bytes. cdecl: exchange +0x850; null object returns 0. */
uint32_t EF_CDECL exchange_callback_106938(ef_addr32 object, uint32_t value)
{
    uint32_t old;
    if (object == 0)
        return 0;
    old = EF_U32(object + 0x850u);
    EF_U32(object + 0x850u) = value;
    return old;
}

/* 0x106988, 63 bytes. Store dword +0x68 and notify each of the +0x60 entries (0x894 bytes each) whose state is not 1. */
void set_and_notify_106988(ef_addr32 object, uint32_t value)
{
    int32_t i;
    EF_U32(object + 0x68u) = value;
    for (i = 0; i < EF_S32(object + 0x60u); ++i) {
        ef_addr32 entry = EF_U32(object + 0x5cu) + (uint32_t)i * 0x894u;
        if (EF_U32(entry + 4u) != 1)
            guest_fn_10535c(entry);
    }
}

/* 0x106be8, 18 bytes. value * 100. */
int32_t times_100(int32_t value)
{
    return (int32_t)((uint32_t)value * 100u);
}

/* 0x106bfc, 32 bytes. modulo-2^32 product, signed divide by 10. */
int32_t times_100_div_10(int32_t value)
{
    int32_t product = (int32_t)((uint32_t)value * 100u);
    return product / 10;
}

/* 0x106c3c, 22 bytes. 0xf8248() - EAX >= EDX (signed). */
uint8_t elapsed_at_least(int32_t start, int32_t duration)
{
    return (int32_t)(guest_fn_f8248() - (uint32_t)start) >= duration;
}

/* 0x10703c, 33 bytes. Release the object stored at *slot. */
void release_object_10703c(ef_addr32 slot)
{
    uint32_t object = EF_U32(slot);
    if (object != 0) {
        guest_fn_118290(object);
        guest_fn_fa9d2(EF_U32(slot));
        EF_U32(slot) = 0;
    }
}

/* 0x107060, 15 bytes. 0x118428(*EAX, 0x53). */
void send_command_107060(ef_addr32 slot)
{
    guest_fn_118428(EF_U32(slot), 0x53u);
}

/* 0x1089d4, 67 bytes. Build a 256-bit set at EDX from the bytes of the string at EAX up to ']'. */
ef_addr32 build_char_set(ef_addr32 text, ef_addr32 set)
{
    uint32_t c;
    guest_fn_b3fb0(set, 0, 0x20);
    c = EF_U8(text);
    ++text;
    if (c == 0)
        return text;
    for (;;) {
        do {
            EF_U8(set + (c >> 3)) |= EF_U8(0x00195c58u + (c & 7u));
            c = EF_U8(text);
            if (c == 0)
                return text;
            ++text;
        } while (c != 0x5du);
        c = EF_U8(text);
        ++text;
        if (c == 0)
            return text;
    }
}

/* 0x108fd3, 39 bytes. Hex digit value of a character, or 0x10. */
int32_t hex_digit_value(int32_t c)
{
    if (c >= 0x30 && c <= 0x39)
        return c - 0x30;
    c = to_lower_ascii(c);
    if (c >= 0x61 && c <= 0x66)
        return c - 0x57;
    return 0x10;
}

/* 0x1091e0, 28 bytes. verified: diff+capture+replays. Remove CRs in place. */
ef_addr32 strip_cr_inplace(ef_addr32 text)
{
    ef_addr32 dst = text;
    uint8_t c;
    for (;;) {
        c = EF_U8(text);
        if (c == 0x0d) {
            ++text;
            continue;
        }
        ++dst;
        ++text;
        EF_U8(dst - 1u) = c;
        if (c == 0)
            return text;
    }
}

/* 0x1091fc, 46 bytes. Copy one line from EDX to EAX, consuming the newline; returns the source after it. */
ef_addr32 copy_line(ef_addr32 destination, ef_addr32 source)
{
    while (EF_U8(source) != 0 && EF_U8(source) != 0x0a) {
        EF_U8(destination) = EF_U8(source);
        ++source;
        ++destination;
    }
    if (EF_U8(source) == 0x0a)
        ++source;
    EF_U8(destination) = 0;
    return source;
}

/* 0x109284, 41 bytes. Advance *pointer over characters whose class-table entry (0x195af0[c+1]) has bit 2. */
void skip_class_chars(ef_addr32 pointer)
{
    ef_addr32 p = EF_U32(pointer);
    while (EF_U8(0x00195af0u + (uint8_t)(EF_U8(p) + 1u)) & 2u)
        ++p;
    EF_U32(pointer) = p;
}

/* 0x10d1b5, 37 bytes. strncpy with NUL padding; returns the destination. */
ef_addr32 copy_padded(ef_addr32 destination, ef_addr32 source, uint32_t count)
{
    ef_addr32 start = destination;
    while (count != 0) {
        uint8_t c = EF_U8(source);
        if (c == 0)
            break;
        ++source;
        --count;
        EF_U8(destination) = c;
        ++destination;
    }
    while (count != 0) {
        --count;
        EF_U8(destination) = 0;
        ++destination;
    }
    return start;
}

/* 0x10d1da, 26 bytes. verified: diff+capture+replays. Last byte match includes the terminator. */
ef_addr32 find_last_char(ef_addr32 text, uint8_t ch)
{
    ef_addr32 found = 0;
    uint8_t c;
    do {
        if (ch == EF_U8(text))
            found = text;
        c = EF_U8(text);
        ++text;
    } while (c != 0);
    return found;
}

/* 0x10d427, 63 bytes. Unlink a node from the doubly linked list with heads at dwords 0x164650 and 0x16464c. */
void unlink_node_10d427(ef_addr32 node)
{
    ef_addr32 previous;
    ef_addr32 next;
    if (node == EF_U32(0x00164650u))
        EF_U32(0x00164650u) = EF_U32(node + 8u);
    if (node == EF_U32(0x0016464cu))
        EF_U32(0x0016464cu) = EF_U32(node + 8u);
    previous = EF_U32(node + 4u);
    next = EF_U32(node + 8u);
    if (previous != 0)
        EF_U32(previous + 8u) = next;
    if (next != 0)
        EF_U32(next + 4u) = previous;
}

/* 0x10ddfc, 47 bytes. Count list entries (list at dword 0x485f9f) whose flags in +0xc match the mask; flush those with bit 0x10 in +0xd via 0xfa8a0. */
int32_t count_matching_streams(uint32_t mask)
{
    int32_t count = 0;
    ef_addr32 node = EF_U32(0x00485f9cu);
    while (node != 0) {
        ef_addr32 stream = EF_U32(node + 4u);
        if (EF_U32(stream + 0xcu) & mask) {
            ++count;
            if (EF_U8(stream + 0xdu) & 0x10u)
                guest_fn_fa8a0(stream);
        }
        node = EF_U32(node);
    }
    return count;
}

/* 0x10dfe0, 12 bytes. Dword +0x46c of the object pointed to by dword 0x120cc6. */
uint32_t field_10dfe0(void)
{
#if defined(__WATCOMC__)
    return *(uint32_t *)(uintptr_t)(g_runtime_segment_base + 0x46cu);
#else
    return EF_U32(EF_U32(0x00120cc6u) + 0x46cu);
#endif
}
