/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xcbef4, 12 bytes. AL if it is 1..4, else 0. */
uint8_t code_if_one_to_four(uint8_t code)
{
    if (code < 1 || code > 4)
        return 0;
    return code;
}

/* 0xcbf00, 30 bytes. Release dword 0x1379c8 through 0xcc768 when set, then clear it. */
void release_pointer_1379c8(void)
{
    uint32_t pointer = EF_U32(0x001379c8u);
    if (pointer != 0)
        guest_fn_cc768(pointer);
    EF_U32(0x001379c8u) = 0;
}

/* 0xcc164, 36 bytes. In list EBX node number AL, set byte [dword +0xc + DL]
 * to 0x9c and byte [dword +0x10 + DL] to 0x7f. */
void mark_list_entry_9c(uint8_t node_index, uint8_t slot, ef_addr32 head)
{
    ef_addr32 node = list_nth(head, node_index);
    EF_U8(EF_U32(node + 0xcu) + slot) = 0x9c;
    EF_U8(EF_U32(node + 0x10u) + slot) = 0x7f;
}

/* 0xcc188, 36 bytes. As 0xcc164 with the value 0x64. */
void mark_list_entry_64(uint8_t node_index, uint8_t slot, ef_addr32 head)
{
    ef_addr32 node = list_nth(head, node_index);
    EF_U8(EF_U32(node + 0xcu) + slot) = 0x64;
    EF_U8(EF_U32(node + 0x10u) + slot) = 0x7f;
}

/* 0xcc26c, 25 bytes. Node in the list at *EDX whose byte +0xb equals AL; spliced, gameplay hit pending. */
ef_addr32 find_node_by_byte_b(uint8_t key, ef_addr32 head)
{
#if defined(__WATCOMC__)
    volatile uint8_t *node = (volatile uint8_t *)(uintptr_t)
        *(volatile uint32_t *)(uintptr_t)head;
    while (node != 0) {
        if (node[0xbu] == key)
            return (ef_addr32)(uintptr_t)node;
        node = (volatile uint8_t *)(uintptr_t)
            *(volatile uint32_t *)(uintptr_t)(node + 4u);
    }
    return 0;
#else
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        if (EF_U8(node + 0xbu) == key)
            return node;
        node = EF_U32(node + 4u);
    }
    return 0;
#endif
}

/* 0xcc5fc, 44 bytes. AL is 0x10..0x15. */
uint32_t code_is_10_to_15(uint8_t code)
{
    return code >= 0x10 && code <= 0x15;
}

/* 0xcc628, 44 bytes. AL is 0x18, 0x19, 0x1a, 0x1b, 0x1d or 0x1e. */
uint32_t code_is_18_to_1b_1d_1e(uint8_t code)
{
    return code == 0x18 || code == 0x19 || code == 0x1a || code == 0x1b || code == 0x1e || code == 0x1d;
}

/* 0xcc654, 39 bytes. AL is 0x18 or 0x1c..0x1f. */
uint32_t code_is_18_or_1c_to_1f(uint8_t code)
{
    return code == 0x18 || code == 0x1c || code == 0x1d || code == 0x1e || code == 0x1f;
}

/* 0xcc67c, 24 bytes. AL is 0x20 or 0x21. */
uint32_t code_is_20_or_21(uint8_t code)
{
    return code == 0x20 || code == 0x21;
}

/* 0xcc694, 24 bytes. AL is 0x23 or 0x24. */
uint32_t code_is_23_or_24(uint8_t code)
{
    return code == 0x23 || code == 0x24;
}

/* 0xcc6ac, 17 bytes. AL is 0x22. */
uint32_t code_is_22(uint8_t code)
{
    return code == 0x22;
}

/* 0xcc7e8, 52 bytes. DL in {0x16, 0x17, 0x0e, 0x11, 0x13} and AL is 2. */
uint32_t pair_matches_cc7e8(uint8_t value, uint8_t kind)
{
    if (kind == 0x16 || kind == 0x17 || kind == 0x0e || kind == 0x11 || kind == 0x13)
        return value == 2;
    return 0;
}

/* 0xcc81c, 34 bytes. DL is 8 or 0x1a and AL is 2. */
uint32_t pair_matches_cc81c(uint8_t value, uint8_t kind)
{
    if (kind == 8 || kind == 0x1a)
        return value == 2;
    return 0;
}

/* 0xcd24c, 10 bytes. Word at +2 of the structure at dword 0x15b628. */
uint16_t cursor_word_at_2(void)
{
    return EF_U16(EF_U32(0x0015b628u) + 2u);
}

/* 0xcd258, 24 bytes. Word table 0x41f700 indexed by the signed high word of
 * the cursor's dword +2. */
uint16_t cursor_table_word(void)
{
    int32_t index = EF_S32(EF_U32(0x0015b628u) + 2u) >> 16;
    return EF_U16(EF_U32(0x0041f700u) + (uint32_t)(index * 2));
}

/* 0xcd270, 42 bytes. Dword table 0x41f700 selects a signed high word; the
 * result is twice that plus dword 0x41f704. */
int32_t cursor_table_address(void)
{
    int32_t index = EF_S32(EF_U32(0x0015b628u) + 2u) >> 16;
    int32_t entry = EF_S32(EF_U32(0x0041f700u) + (uint32_t)(index * 2));
    return (entry >> 16) * 2 + EF_S32(0x0041f704u);
}

/* 0xcd390, 34 bytes. Advance dword 0x15b628 by twice its signed word at +0;
 * return whether the word at the new position is nonzero. */
uint32_t advance_cursor(void)
{
    uint32_t cursor = EF_U32(0x0015b628u);
    cursor += (uint32_t)((int32_t)EF_S16(cursor) * 2);
    EF_U32(0x0015b628u) = cursor;
    return EF_U16(cursor) != 0;
}

/* 0xcd6fc, 38 bytes. When byte 0x15b734 is set clear it and run 0x9c3cf(0xcd6cc);
 * dword 0x15b730 = 0. */
void reset_handler_cd6fc(void)
{
    if (EF_U8(0x0015b734u) != 0) {
        EF_U8(0x0015b734u) = 0;
        guest_fn_9c3cf(0x000cd6ccu);
    }
    EF_U32(0x0015b730u) = 0;
}

/* 0xcdf48, 24 bytes. Dwords 0x17cf9c..a8 = EAX, EDX, EBX, ECX. */
void store_four_dwords_17cf9c(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    EF_U32(0x0017cf9cu) = a;
    EF_U32(0x0017cfa0u) = b;
    EF_U32(0x0017cfa4u) = c;
    EF_U32(0x0017cfa8u) = d;
}

/* 0xcdc38, 44 bytes. Run 0xc37ec, then release dword 0x15b758 as 0xc3a48 does. */
void release_buffer_cdc38(void)
{
    uint32_t buffer;
    guest_fn_c37ec();
    buffer = EF_U32(0x0015b758u);
    if (buffer != 0 && EF_U8(0x00146e4au) == 0) {
        guest_fn_c3f80(buffer);
        EF_U32(0x0015b758u) = 0;
    }
}

/* 0xce824, 11 bytes. Append EDX to the counted array at EAX (count dword at +0). */
void counted_array_append(ef_addr32 array, uint32_t value)
{
    uint32_t count = EF_U32(array);
    EF_U32(array + count * 4u + 4u) = value;
    EF_U32(array) = EF_U32(array) + 1u;
}

/* 0xcff64, 26 bytes. 0xb3830(signed word +0, high words of +2 and +6), as signed 16. */
int32_t forward_triple_cff64(ef_addr32 record)
{
    return (int32_t)(int16_t)guest_fn_b3830((int32_t)EF_S16(record), EF_S32(record + 2u) >> 16,
                                            EF_S32(record + 6u) >> 16);
}

/* 0xd13dc, 14 bytes. 0xb3b1c(dword +8, dword +0x20). */
uint32_t forward_pair_d13dc(ef_addr32 record)
{
    return guest_fn_b3b1c(EF_U32(record + 8u), EF_U32(record + 0x20u));
}

/* 0xd13ac, 46 bytes. -0xb3b1c(-dword +0x14, low word of 0xe2b60(dword+8 squared + dword+0x20 squared)). */
int32_t angle_d13ac(ef_addr32 record)
{
    int32_t sum = EF_S32(record + 8u) * EF_S32(record + 8u) + EF_S32(record + 0x20u) * EF_S32(record + 0x20u);
    uint32_t length = guest_square_root_of(sum) & 0xffffu;
    return -(int32_t)guest_fn_b3b1c((uint32_t)(-EF_S32(record + 0x14u)), length);
}

/* 0xd13ec, 39 bytes. -0xb3b1c(dword +0xc, (dword+0 * dword+0x20 - dword+8 * dword+0x18) >> 15). */
int32_t angle_d13ec(ef_addr32 record)
{
    int32_t cross = EF_S32(record) * EF_S32(record + 0x20u) - EF_S32(record + 8u) * EF_S32(record + 0x18u);
    return -(int32_t)guest_fn_b3b1c(EF_U32(record + 0xcu), (uint32_t)(cross >> 15));
}

/* 0xd17bc, 19 bytes. 0xa7990(dword 0x420944), callee pops 4. */
void call_a7990_420944(void)
{
    guest_fn_a7990(EF_U32(0x00420944u));
}

/* 0xd1f5c, 41 bytes. Copy dwords 0x15b88c..98 to 0x15b87c..88. */
void copy_four_dwords_15b87c(void)
{
    EF_U32(0x0015b87cu) = EF_U32(0x0015b88cu);
    EF_U32(0x0015b880u) = EF_U32(0x0015b890u);
    EF_U32(0x0015b884u) = EF_U32(0x0015b894u);
    EF_U32(0x0015b888u) = EF_U32(0x0015b898u);
}

/* 0xd2110, 30 bytes. 0xfc8cc(dword [p], p+4) for p = dword +0x4c; set bit 0 of byte +0x2c. */
void forward_record_d2110(ef_addr32 record)
{
    ef_addr32 p = EF_U32(record + 0x4cu);
    guest_fn_fc8cc(EF_U32(p), p + 4u);
    EF_U8(record + 0x2cu) = (uint8_t)(EF_U8(record + 0x2cu) | 1u);
}

/* 0xd5fa8, 31 bytes. When byte 0x15b968 is set call cdecl 0x103778(dword 0x420a64, EAX, EDX). */
void call_103778_if_enabled(uint32_t first, uint32_t second)
{
    if (EF_U8(0x0015b968u) != 0)
        guest_fn_103778(EF_U32(0x00420a64u), first, second);
}

/* 0xd6410, 26 bytes. Sum of the first three bytes. */
uint32_t sum_three_bytes(ef_addr32 bytes)
{
    return (uint32_t)EF_U8(bytes) + EF_U8(bytes + 1u) + EF_U8(bytes + 2u);
}

/* 0xd6d5c, 26 bytes. When bit 1 of byte 0x15bac0 is set store dword 0x420da0 at EAX; return whether it was. */
uint8_t store_handle_if_enabled(ef_addr32 destination)
{
    if (EF_U8(0x0015bac0u) & 2u) {
        EF_U32(destination) = EF_U32(0x00420da0u);
        return 1;
    }
    return 0;
}

/* 0xd6f90, 22 bytes. For each pointer in the array at EAX whose string is nonempty run 0xd6fa8. */
void process_string_list(ef_addr32 list)
{
    while (EF_U8(EF_U32(list)) != 0) {
        guest_fn_d6fa8(EF_U32(list));
        list += 4u;
    }
}

/* 0xd7594, 50 bytes. With byte 0x15bac4 set, 0x1028f4(dword 0x420dd0[AL]) equals 2. */
uint8_t device_state_is_2(uint8_t index)
{
    if (EF_U8(0x0015bac4u) != 0 && guest_fn_1028f4(EF_U32(0x00420dd0u + (uint32_t)index * 4u)) == 2u)
        return 1;
    return 0;
}

/* 0xd75fc, 49 bytes. As 0xd7594, comparing with 4. */
uint8_t device_state_is_4(uint8_t index)
{
    if (EF_U8(0x0015bac4u) != 0)
        return (uint8_t)(guest_fn_1028f4(EF_U32(0x00420dd0u + (uint32_t)index * 4u)) == 4u);
    return 0;
}

/* 0xd7724, 11 bytes. Word +0x11 += DX; return the signed high word of dword +0xf. */
int32_t add_to_word_11(ef_addr32 record, uint16_t delta)
{
    EF_U16(record + 0x11u) = (uint16_t)(EF_U16(record + 0x11u) + delta);
    return EF_S32(record + 0xfu) >> 16;
}
