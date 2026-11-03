/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xdc2c0, 23 bytes. 0xb3fb0(dword 0x15b754, 0, dword 0x164600). */
void clear_buffer_dc2c0(void)
{
    guest_fn_b3fb0(EF_U32(0x0015b754u), 0, EF_U32(0x00164600u));
}

/* 0xdc314, 32 bytes. Unless both dwords 0x15bdd4 and 0x15bdd8 equal 0x100,
 * run 0xdc3d5 and 0xce354. */
void update_view_scale_dc314(void)
{
    if (EF_U32(0x0015bdd4u) != 0x100u || EF_U32(0x0015bdd8u) != 0x100u) {
        guest_fn_dc3d5();
        guest_fn_ce354();
    }
}

/* 0xdc860, 47 bytes. Halve the dwords at EAX and EDX until both fit in a signed 16-bit range. */
void halve_pair_to_fit16(ef_addr32 first, ef_addr32 second)
{
    int32_t a;
    int32_t b;
    do {
        EF_S32(first) >>= 1;
        b = EF_S32(second) >> 1;
        EF_S32(second) = b;
        a = EF_S32(first);
    } while (a < -0x8000 || a > 0x7fff || b < -0x8000 || b > 0x7fff);
}

/* 0xddbc8, 14 bytes. Signed 'a'..'z' to uppercase. */
int32_t to_upper_ascii(int32_t c)
{
    if (c >= 0x61 && c <= 0x7a)
        c -= 0x20;
    return c;
}

/* 0xe7835, 14 bytes. Signed 'A'..'Z' to lowercase. */
int32_t to_lower_ascii(int32_t c)
{
    if (c >= 0x41 && c <= 0x5a)
        c += 0x20;
    return c;
}

/* 0xdec00, 21 bytes. Byte at dword 0x15b754 + EDX + dword 0x163200[EBX] = AL. */
void plot_byte(uint8_t value, uint32_t x, uint32_t y)
{
    EF_U8(EF_U32(0x0015b754u) + x + EF_U32(0x00163200u + y * 4u)) = value;
}

/* 0xdec18, 51 bytes. As 0xdec00 but only inside the signed clip rectangle
 * 0x15bdc0..c4 (x) and 0x15bdc8..cc (y). */
void plot_byte_clipped(uint8_t value, int32_t x, int32_t y)
{
    if (x < EF_S32(0x0015bdc0u) || x > EF_S32(0x0015bdc4u))
        return;
    if (y < EF_S32(0x0015bdc8u) || y > EF_S32(0x0015bdccu))
        return;
    EF_U8(EF_U32(0x0015b754u) + (uint32_t)x + EF_U32(0x00163200u + (uint32_t)y * 4u)) = value;
}

/* 0xdec90, 30 bytes. 0xe2b60 of the sum of squares of three signed words. */
uint32_t length_squared_dec90(int16_t x, int16_t y, int16_t z)
{
    int32_t sum = (int32_t)x * x + (int32_t)y * y + (int32_t)z * z;
    return integer_sqrt((uint32_t)sum);
}

/* 0xdecb0, 19 bytes. Low word of 0xe2b60(EAX^2 + EDX^2). */
uint32_t length_2d_decb0(int32_t x, int32_t y)
{
    return integer_sqrt((uint32_t)(x * x + y * y)) & 0xffffu;
}

/* 0xdecc4, 26 bytes. Low word of 0xe2b60(EAX^2 + EDX^2 + EBX^2). */
uint32_t length_3d_decc4(int32_t x, int32_t y, int32_t z)
{
    return integer_sqrt((uint32_t)(y * y + x * x + z * z)) & 0xffffu;
}

/* 0xe2b60, 38 bytes. Bitwise unsigned integer square root. */
uint32_t integer_sqrt(uint32_t value)
{
    uint32_t bit = 0x40000000u;
    uint32_t root = 0;
    do {
        uint32_t trial = bit + root;
        root >>= 1;
        if (!(trial > value)) {
            value -= trial;
            root |= bit;
        }
        bit >>= 2;
    } while (bit != 0);
    return root;
}

/* 0xe2fdb, 36 bytes. Linear congruential generator on the seed at the
 * pointer returned by 0xe2fd5: seed = seed*0x41c64e6d + 0x3039, result is
 * bits 16..30. Returns 0 when the seed pointer is null. */
uint32_t random_lcg(void)
{
    ef_addr32 seed = guest_fn_e2fd5();
    uint32_t next;
    if (seed == 0)
        return 0;
    next = EF_U32(seed) * 0x41c64e6du + 0x3039u;
    EF_U32(seed) = next;
    return (next >> 16) & 0x7fffu;
}

/* 0xdf2e4, 15 bytes. Byte 0x15f07d = (byte + 1) & 7. */
void advance_counter_15f07d(void)
{
    EF_U8(0x0015f07du) = (uint8_t)((EF_U8(0x0015f07du) + 1u) & 7u);
}

/* 0xe10e0, 48 bytes. Byte 0x15f685 = 0, fill 0x80 bytes at 0x15f5fd with 0xff,
 * byte 0x15f684 = 0, byte 0x15f685 = 1. */
void reset_table_15f5fd(void)
{
    EF_U8(0x0015f685u) = 0;
    guest_fn_b3fb0(0x0015f5fdu, 0xff, 0x80);
    EF_U8(0x0015f684u) = 0;
    EF_U8(0x0015f685u) = 1;
}

/* 0xe1540, 38 bytes. Next index after AL (mod 5) whose byte at 0x15f708+index is nonzero. */
uint8_t next_enabled_index(uint8_t index)
{
    do {
        index = (uint8_t)((uint32_t)(uint8_t)(index + 1u) % 5u);
    } while (EF_U8(0x0015f708u + index) == 0);
    return index;
}

/* 0xe17a0, 22 bytes. Clear 0x5000 bytes at 0x4198d0. */
void clear_block_e17a0(void)
{
    guest_fn_b3fb0(0x004198d0u, 0, 0x5000);
}

/* 0xe192c, 25 bytes. Byte at 0x1378d8 + 10*AL. */
uint8_t read_table_e192c(uint8_t index)
{
    return EF_U8(0x001378d8u + (uint32_t)index * 10u);
}

/* 0xe2988, 24 bytes. Store EAX in dword 0x15f748 and, if positive, wait for
 * an interrupt handler to bring it to zero or below. */
void wait_for_counter_15f748(int32_t count)
{
    EF_S32(0x0015f748u) = count;
    if (EF_S32(0x0015f748u) > 0) {
        while (*(volatile int32_t *)ef_guest_memory_ptr(0x0015f748u) > 0) {
        }
    }
}

/* 0xe2a1c, 50 bytes. Clear dwords 0x15f764/68, dword 0x15f758 = dword
 * 0x15f760, dword 0x15f754 = dword 0x15f75c, then clear that buffer. */
void reset_buffer_e2a1c(void)
{
    uint32_t pointer;
    EF_U32(0x0015f764u) = 0;
    EF_U32(0x0015f768u) = 0;
    EF_U32(0x0015f758u) = EF_U32(0x0015f760u);
    pointer = EF_U32(0x0015f75cu);
    EF_U32(0x0015f754u) = pointer;
    guest_fn_b3fb0(pointer, 0, EF_U32(0x0015f758u));
}

/* 0xe4304, 14 bytes. Byte 0x15f930 equals 1. */
uint8_t flag_is_one_15f930(void)
{
    return EF_U8(0x0015f930u) == 1;
}

/* 0xe599c, 46 bytes. Copy words +0x41..+0x47 to 0x41ec08..0e. */
void copy_record_words_e599c(ef_addr32 record)
{
    EF_U16(0x0041ec08u) = EF_U16(record + 0x41u);
    EF_U16(0x0041ec0au) = EF_U16(record + 0x43u);
    EF_U16(0x0041ec0cu) = EF_U16(record + 0x45u);
    EF_U16(0x0041ec0eu) = EF_U16(record + 0x47u);
}

/* 0xe5870, 25 bytes. 0xe599c(record), then 0xe52a4(byte +0x10, 1). */
void apply_record_e5870(ef_addr32 record)
{
    copy_record_words_e599c(record);
    guest_fn_e52a4(EF_U8(record + 0x10u), 1);
}

/* 0xe588c, 25 bytes. As 0xe5870 through 0xe5960. */
void apply_record_e588c(ef_addr32 record)
{
    guest_fn_e5960(record);
    guest_fn_e52a4(EF_U8(record + 0x10u), 1);
}

/* 0xe6800, 23 bytes. 0xe67a0(EAX, signed DX, signed BX+DX, signed CX); returns EAX. */
ef_addr32 call_e67a0(ef_addr32 object, int16_t x, int16_t y, int16_t z)
{
    int32_t sum = (int32_t)(int16_t)((int32_t)y + (int32_t)x);
    guest_fn_e67a0(object, (int32_t)x, sum, (int32_t)z);
    return object;
}

/* 0xe6818, 48 bytes. For each node of the list at *EAX call 0xe6800 on its
 * dword +0x2c with (0, word +0x2a minus 1, 1). */
void adjust_list_e6818(ef_addr32 head)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        int16_t amount = (int16_t)((int32_t)EF_U16(node + 0x2au) - 1);
        EF_U32(node + 0x2cu) = call_e67a0(EF_U32(node + 0x2cu), 0, amount, 1);
        node = EF_U32(node + 4u);
    }
}

/* 0xe7fa4, 19 bytes. Dword 0x464360 = EAX, dword 0x15b268 = 0xe7f60. */
void install_handler_e7fa4(uint32_t value)
{
    EF_U32(0x00464360u) = value;
    EF_U32(0x0015b268u) = 0x000e7f60u;
}

/* 0xe8348, 32 bytes. 0xe81b0 on (A,B), (B,C), (C,A) for EAX, EDX, EBX. */
void rotate_calls_e8348(uint32_t a, uint32_t b, uint32_t c)
{
    guest_fn_e81b0(a, b);
    guest_fn_e81b0(b, c);
    guest_fn_e81b0(c, a);
}

/* 0xe85e4, 12 bytes. Byte 0x46437e = AL, byte 0x46437d = DL. */
void store_bytes_46437d(uint8_t first, uint8_t second)
{
    EF_U8(0x0046437eu) = first;
    EF_U8(0x0046437du) = second;
}

/* 0xe8733, 24 bytes. Allocate EAX*EDX bytes through 0xfc795 and clear them. */
ef_addr32 allocate_cleared(uint32_t count, uint32_t size)
{
    uint32_t total = count * size;
    ef_addr32 block = guest_fn_fc795(total);
    if (block != 0)
        guest_fn_b3fb0(block, 0, total);
    return block;
}

/* 0xe874b, 26 bytes. strchr: first occurrence of byte EDX in the string at EAX
 * (including the terminator), or 0. */
ef_addr32 find_char(ef_addr32 text, uint32_t ch)
{
    ef_addr32 p = text;
    for (;;) {
        uint8_t c = EF_U8(p);
        if ((uint32_t)c == ch)
            return p;
        ++p;
        if (c == 0)
            return 0;
    }
}

/* 0xea130, 35 bytes. Remove carriage returns in place; returns the number removed. */
uint32_t strip_carriage_returns(ef_addr32 text)
{
    ef_addr32 src = text;
    ef_addr32 dst = text;
    uint8_t c;
    do {
        c = EF_U8(src);
        ++src;
        if (c != 0x0d) {
            ++dst;
            EF_U8(dst - 1u) = c;
        }
    } while (c != 0);
    return src - dst;
}

/* 0xea154, 38 bytes. Advance to the next newline, replace it with NUL and
 * return the pointer after it (or the end of string). */
ef_addr32 split_line(ef_addr32 text)
{
    ef_addr32 p = text;
    while (EF_U8(p) != 0 && EF_U8(p) != 0x0a)
        ++p;
    if (EF_U8(p) == 0x0a) {
        EF_U8(p) = 0;
        ++p;
    }
    return p;
}

/* 0xea1e8, 33 bytes. Signed compare of the dwords at EAX and EDX: -1, 0 or 1. */
int32_t compare_dwords(ef_addr32 first, ef_addr32 second)
{
    int32_t a = EF_S32(first);
    int32_t b = EF_S32(second);
    if (a == b)
        return 0;
    return a < b ? -1 : 1;
}

/* 0xea240, 48 bytes. If dword +0xc is set, release dwords +0x14 and +0x10
 * through 0xc3f80 and clear +0xc, +0x10 and +0x14. */
void release_record_ea240(ef_addr32 record)
{
    if (EF_U32(record + 0xcu) != 0) {
        guest_fn_c3f80(EF_U32(record + 0x14u));
        guest_fn_c3f80(EF_U32(record + 0x10u));
        EF_U32(record + 0xcu) = 0;
        EF_U32(record + 0x10u) = 0;
        EF_U32(record + 0x14u) = 0;
    }
}

/* 0xea84c, 21 bytes. Clear three dwords. */
void clear_vector3(ef_addr32 vector)
{
    EF_U32(vector) = 0;
    EF_U32(vector + 4u) = 0;
    EF_U32(vector + 8u) = 0;
}

/* 0xea870, 19 bytes. Copy three dwords from EDX to EAX. */
void copy_vector3(ef_addr32 destination, ef_addr32 source)
{
    EF_U32(destination) = EF_U32(source);
    EF_U32(destination + 4u) = EF_U32(source + 4u);
    EF_U32(destination + 8u) = EF_U32(source + 8u);
}

/* 0xea958, 33 bytes. Add three dwords of EDX to those at EAX. */
void add_vector3(ef_addr32 destination, ef_addr32 source)
{
    EF_U32(destination) += EF_U32(source);
    EF_U32(destination + 4u) += EF_U32(source + 4u);
    EF_U32(destination + 8u) += EF_U32(source + 8u);
}

/* 0xea9a0, 22 bytes. Dwords at EAX = sign-extended DX, BX, CX. */
void set_vector3_from_words(ef_addr32 vector, int16_t x, int16_t y, int16_t z)
{
    EF_S32(vector) = x;
    EF_S32(vector + 4u) = y;
    EF_S32(vector + 8u) = z;
}

/* 0xea9b8, 52 bytes. Fixed-point (16.16) dot product of two dword triples. */
int32_t dot_product_fixed(ef_addr32 first, ef_addr32 second)
{
    int32_t sum = 0;
    uint32_t i;
    for (i = 0; i < 3; ++i)
        sum += (int32_t)(((int64_t)EF_S32(first + i * 4u) * EF_S32(second + i * 4u)) / 65536);
    return sum;
}

/* 0xeb478, 31 bytes. Lowercase ASCII 'A'..'Z' in place; returns the pointer. */
ef_addr32 lowercase_string(ef_addr32 text)
{
    ef_addr32 p = text;
    while (EF_U8(p) != 0) {
        uint8_t c = EF_U8(p);
        if ((uint8_t)(c - 0x41u) <= 0x19u)
            EF_U8(p) = (uint8_t)(c + 0x20u);
        ++p;
    }
    return text;
}

/* 0xeb4c0, 26 bytes. Release the pointer stored at EAX through 0xc3f80 and clear it. */
void release_pointer_eb4c0(ef_addr32 slot)
{
    uint32_t pointer = EF_U32(slot);
    if (pointer != 0) {
        guest_fn_c3f80(pointer);
        EF_U32(slot) = 0;
    }
}
