/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xb55c8, 47 bytes. Walk the list at *head (next at +4) for a node whose
 * dwords +9/+0xd equal the record's +0x11/+0x15, word +0x1c equals BX and
 * byte +0x1b equals CL. Returns the node or 0. */
ef_addr32 find_node_by_key(ef_addr32 head, ef_addr32 record, uint16_t word, uint8_t byte)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        if (EF_U32(node + 9u) == EF_U32(record + 0x11u) &&
            EF_U32(node + 0xdu) == EF_U32(record + 0x15u) &&
            EF_U16(node + 0x1cu) == word && EF_U8(node + 0x1bu) == byte)
            return node;
        node = EF_U32(node + 4u);
    }
    return 0;
}

/* 0xba3bc, 40 bytes. Fill bytes 0..9 with 4. */
void fill_ten_bytes_04(ef_addr32 destination)
{
    uint32_t i;
    for (i = 0; i < 10; ++i)
        EF_U8(destination + i) = 4;
}

/* 0xba3e4, 40 bytes. Fill bytes 0..9 with 0x0d. */
void fill_ten_bytes_0d(ef_addr32 destination)
{
    uint32_t i;
    for (i = 0; i < 10; ++i)
        EF_U8(destination + i) = 0x0d;
}

/* 0xbae34, 23 bytes. Bytes 0 and 1 = 0x12 when DL is 1, else 0x11. */
void fill_pair_by_flag(ef_addr32 destination, uint8_t flag)
{
    uint8_t value = flag == 1 ? 0x12 : 0x11;
    EF_U8(destination) = value;
    EF_U8(destination + 1u) = value;
}

/* 0xbae4c, 16 bytes. Bytes 0..3 = 2, 2, 3, 3. */
void fill_four_bytes_2233(ef_addr32 destination)
{
    EF_U8(destination) = 2;
    EF_U8(destination + 1u) = 2;
    EF_U8(destination + 2u) = 3;
    EF_U8(destination + 3u) = 3;
}

/* 0xbc34f, 27 bytes. When 0xfa6d3(0) returns nonzero, run 0xbc287(EAX). */
void run_if_available_bc34f(ef_addr32 argument)
{
    if (guest_fn_fa6d3(0) != 0)
        guest_fn_bc287(argument);
}

/* 0xbc36a, 10 bytes. Run 0xbc34f with EBX preserved as zero. */
void run_if_available_bc36a(ef_addr32 argument)
{
    __asm {
        push ebx
        xor ebx, ebx
    }
    run_if_available_bc34f(argument);
    __asm {
        pop ebx
    }
}

/* 0xbc465, 12 bytes. Digit character for a value: +0x30, then +0x27 past '9'. */
int32_t digit_character(int32_t value)
{
    int32_t c = value + 0x30;
    if (c > 0x39)
        c += 0x27;
    return c;
}

/* 0xbc570, 36 bytes. Insert EAX at the head of the doubly linked list whose
 * head pointer is EBX and tail pointer EDX (previous at +0, next at +4). */
void list_insert_head(ef_addr32 node, ef_addr32 head, ef_addr32 tail)
{
    ef_addr32 first = EF_U32(head);
    if (first == 0) {
        EF_U32(head) = node;
        EF_U32(tail) = node;
        EF_U32(node + 4u) = first;
        EF_U32(node) = first;
    } else {
        EF_U32(first + 4u) = node;
        EF_U32(node + 4u) = 0;
        EF_U32(node) = EF_U32(head);
        EF_U32(head) = node;
    }
}

/* 0xbc5d0, 21 bytes. Count nodes of the list at *EAX (next at +4). */
uint32_t list_count(ef_addr32 head)
{
    uint32_t n = 0;
    ef_addr32 node = EF_U32(head);
    if (node != 0) {
        do {
            node = EF_U32(node + 4u);
            ++n;
        } while (node != 0);
    }
    return n;
}

/* 0xbc5e8, 27 bytes. Node number EDX of the list at *EAX, or 0. */
ef_addr32 list_nth(ef_addr32 head, uint32_t index)
{
    ef_addr32 node = EF_U32(head);
    uint32_t i = 0;
    while (node != 0) {
        if (i == index)
            return node;
        node = EF_U32(node + 4u);
        ++i;
    }
    return 0;
}

/* 0xbcb08, 28 bytes. Byte +5 at least 0x32 (signed) and byte +6 nonzero. */
uint32_t record_is_strong(ef_addr32 record)
{
    return EF_U8(record + 5u) >= 0x32 && EF_U8(record + 6u) != 0;
}

/* 0xbcb24, 43 bytes. Byte +6 at least 0x55 and byte +0 not 0xff or 0xfe. */
uint32_t record_is_elite(ef_addr32 record)
{
    uint8_t kind;
    if (EF_U8(record + 6u) < 0x55)
        return 0;
    kind = EF_U8(record);
    return kind != 0xff && kind != 0xfe;
}

static uint8_t map_code(uint8_t code, uint8_t at_31, uint8_t at_49, uint8_t special)
{
    if (code < 0x31)
        return code == 0x0b ? special : 0;
    if (code == 0x31)
        return at_31;
    if (code < 0x49)
        return 0;
    if (code == 0x49)
        return at_49;
    return code == 0x56 ? special : 0;
}

/* 0xbcc04, 38 bytes. Map code: 0x31 -> 0x37, 0x49 -> 0x4b, 0x0b/0x56 -> 0x0c, else 0. */
uint8_t map_code_bcc04(uint8_t code)
{
    return map_code(code, 0x37, 0x4b, 0x0c);
}

/* 0xbcd78, 38 bytes. Map code: 0x31 -> 0x27, 0x49 -> 0x12, 0x0b/0x56 -> 0x1b, else 0. */
uint8_t map_code_bcd78(uint8_t code)
{
    return map_code(code, 0x27, 0x12, 0x1b);
}

/* 0xbce98, 38 bytes. Map code: 0x31 -> 2, 0x49 -> 0x3c, 0x0b/0x56 -> 0x0c, else 0. */
uint8_t map_code_bce98(uint8_t code)
{
    return map_code(code, 0x02, 0x3c, 0x0c);
}

/* 0xbcf18, 13 bytes. Byte +0xd is zero. */
uint32_t record_flag_clear_d(ef_addr32 record)
{
    return EF_U8(record + 0xdu) == 0;
}

/* 0xbcf28, 28 bytes. Dwords +0x11/+0x15 equal the other record's +0x3b/+0x3f. */
uint32_t records_share_position(ef_addr32 first, ef_addr32 second)
{
    return EF_U32(first + 0x11u) == EF_U32(second + 0x3bu) &&
           EF_U32(first + 0x15u) == EF_U32(second + 0x3fu);
}

/* 0xbd4e0, 39 bytes. AL is 2, 3, 4, 9 or 0x0c. */
uint32_t code_is_2_3_4_9_0c(uint8_t code)
{
    return code == 2 || code == 3 || code == 4 || code == 0x0c || code == 9;
}

/* 0xbd728, 20 bytes. DL (as a signed word) is below the signed word +0x23. */
uint32_t value_below_word_23(ef_addr32 record, uint8_t value)
{
    return (int16_t)value < EF_S16(record + 0x23u);
}

/* 0xbdbc8, 47 bytes. Count the 10-byte entries (pointer +6, count byte +5)
 * whose first word is nonzero. */
uint32_t count_nonzero_entries(ef_addr32 record)
{
    uint32_t count = 0;
    int16_t i = 0;
    while (i < (int16_t)EF_U8(record + 5u)) {
        if (EF_U16(EF_U32(record + 6u) + (uint32_t)((int32_t)i * 10)) != 0)
            ++count;
        ++i;
    }
    return count;
}

/* 0xbecf8, 36 bytes. 1 when 0xbc7ec(record) is zero and 0xbecc0(record) is nonzero. */
uint32_t record_pending_becf8(ef_addr32 record)
{
    if ((guest_fn_bc7ec(record) & 0xffffu) == 0 && (guest_fn_becc0(record) & 0xffffu) != 0)
        return 1;
    return 0;
}

/* 0xbef90, 40 bytes. Find the node in the list at *EBX with dword +0x16 ==
 * signed AX and dword +0x1a == signed DX. */
ef_addr32 find_node_by_pair(int16_t first, int16_t second, ef_addr32 head)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        if ((int32_t)first == EF_S32(node + 0x16u) && (int32_t)second == EF_S32(node + 0x1au))
            return node;
        node = EF_U32(node + 4u);
    }
    return 0;
}

/* 0xbfa28, 48 bytes. Byte +0x22 of the records at dwords +0x33 and +0x37
 * differ and the first is nonzero. */
uint32_t linked_records_differ(ef_addr32 record)
{
    uint8_t first = EF_U8(EF_U32(record + 0x33u) + 0x22u);
    if (first == 0)
        return 0;
    return first != EF_U8(EF_U32(record + 0x37u) + 0x22u);
}

/* 0xbfcd8, 36 bytes. Node in the list at *EDX with dwords +0x16/+0x1a equal
 * to the record's +0x11/+0x15. */
ef_addr32 find_node_by_record_a(ef_addr32 record, ef_addr32 head)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        if (EF_U32(node + 0x16u) == EF_U32(record + 0x11u) &&
            EF_U32(node + 0x1au) == EF_U32(record + 0x15u))
            return node;
        node = EF_U32(node + 4u);
    }
    return 0;
}

/* 0xbfcfc, 34 bytes. Node in the list at *EAX with dwords +0x11/+0x15 equal
 * to the record's +0x16/+0x1a. */
ef_addr32 find_node_by_record_b(ef_addr32 head, ef_addr32 record)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        if (EF_U32(node + 0x11u) == EF_U32(record + 0x16u) &&
            EF_U32(node + 0x15u) == EF_U32(record + 0x1au))
            return node;
        node = EF_U32(node + 4u);
    }
    return 0;
}

/* 0xc1b8c, 26 bytes. Node in the list at *EDX whose word +0x13 equals AX. */
ef_addr32 find_node_by_word_c1b8c(uint16_t key, ef_addr32 head)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        if (EF_U16(node + 0x13u) == key)
            return node;
        node = EF_U32(node + 4u);
    }
    return 0;
}

/* 0xcb5f8, 26 bytes. Identical body to 0xc1b8c. */
ef_addr32 find_node_by_word_cb5f8(uint16_t key, ef_addr32 head)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        if (EF_U16(node + 0x13u) == key)
            return node;
        node = EF_U32(node + 4u);
    }
    return 0;
}

/* 0xc1ba8, 30 bytes. Node number AX of the list at *EDX (16-bit compare), or 0. */
ef_addr32 list_nth_word(uint16_t index, ef_addr32 head)
{
    ef_addr32 node = EF_U32(head);
    uint16_t i = 0;
    while (node != 0) {
        if (i == index)
            return node;
        node = EF_U32(node + 4u);
        ++i;
    }
    return 0;
}

/* 0xc3834, 27 bytes. 0xe9a24(dword 0x164600, dword 0x15b754, dword 0x146e40). */
void call_e9a24_c3834(void)
{
    guest_fn_e9a24(EF_U32(0x00164600u), EF_U32(0x0015b754u), EF_U32(0x00146e40u));
}

/* 0xc3850, 27 bytes. 0xe9a24(dword 0x164600, dword 0x15b754, dword 0x146e44). */
void call_e9a24_c3850(void)
{
    guest_fn_e9a24(EF_U32(0x00164600u), EF_U32(0x0015b754u), EF_U32(0x00146e44u));
}

/* 0xc386c, 27 bytes. 0xe9a24(dword 0x164600, dword 0x146e40, dword 0x15b754). */
void call_e9a24_c386c(void)
{
    guest_fn_e9a24(EF_U32(0x00164600u), EF_U32(0x00146e40u), EF_U32(0x0015b754u));
}

/* 0xc3888, 27 bytes. 0xe9a24(dword 0x164600, dword 0x146e44, dword 0x15b754). */
void call_e9a24_c3888(void)
{
    guest_fn_e9a24(EF_U32(0x00164600u), EF_U32(0x00146e44u), EF_U32(0x0015b754u));
}

/* 0xc38a4, 18 bytes. Tail-call 0xb3fb0(dword 0x146e40, 0, dword 0x164600). */
void clear_buffer_c38a4(void)
{
    guest_fn_b3fb0(EF_U32(0x00146e40u), 0, EF_U32(0x00164600u));
}

/* 0xc3974, 11 bytes. Dword 0x15b754 = dword 0x146e40. */
void select_buffer_146e40(void)
{
    EF_U32(0x0015b754u) = EF_U32(0x00146e40u);
}

/* 0xc3980, 11 bytes. Dword 0x15b754 = dword 0x146e44. */
void select_buffer_146e44(void)
{
    EF_U32(0x0015b754u) = EF_U32(0x00146e44u);
}

/* 0xc3990, 11 bytes. Dword 0x15b754 = dword 0x15b758. */
void select_buffer_15b758(void)
{
    EF_U32(0x0015b754u) = EF_U32(0x0015b758u);
}

/* 0xc399c, 16 bytes. Dwords 0x15b754 and 0x146e4c = dword 0x15b758. */
void select_buffer_15b758_both(void)
{
    uint32_t value = EF_U32(0x0015b758u);
    EF_U32(0x0015b754u) = value;
    EF_U32(0x00146e4cu) = value;
}

/* 0xc3a28, 29 bytes. When dword 0x146e40 is set, replace it with dword
 * 0x15b758 and store the old value at 0x15b758. */
void swap_buffers_c3a28(void)
{
    uint32_t old = EF_U32(0x00146e40u);
    if (old != 0) {
        EF_U32(0x00146e40u) = EF_U32(0x0015b758u);
        EF_U32(0x0015b758u) = old;
    }
}

/* 0xc3a48, 41 bytes. When dword 0x15b758 is set and byte 0x146e4a clear,
 * release it through 0xc3f80 and clear it. */
void release_buffer_c3a48(void)
{
    uint32_t buffer = EF_U32(0x0015b758u);
    if (buffer != 0 && EF_U8(0x00146e4au) == 0) {
        guest_fn_c3f80(buffer);
        EF_U32(0x0015b758u) = 0;
    }
}

/* 0xc3f50, 21 bytes. Byte at dword 0x15b754 + EAX + dword 0x163200[EDX]. */
uint8_t read_buffer_byte(uint32_t offset, uint32_t row)
{
    return EF_U8(EF_U32(0x0015b754u) + offset + EF_U32(0x00163200u + row * 4u));
}

/* 0xc4389, 10 bytes. Run 0xe3c20. */
void run_e3c20(void)
{
    guest_fn_e3c20();
}

/* 0xca9fc, 43 bytes. EAX / 40 + 10 * (EDX / 40), signed. */
int32_t grid_index_ca9fc(int32_t x, int32_t y)
{
    return x / 40 + 10 * (y / 40);
}
