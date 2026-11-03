/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xa7b98, 21 bytes. Dword 0x4191c8 += EAX. */
void add_to_counter_4191c8(uint32_t delta)
{
    EF_U32(0x004191c8u) = EF_U32(0x004191c8u) + delta;
}

/* 0xa7bb0, 11 bytes. Dword 0x4191c4 = 0xb3f50(). */
void latch_b3f50(void)
{
    EF_U32(0x004191c4u) = guest_fn_b3f50();
}

/* 0xa7bbc, 21 bytes. Dword 0x4191c4 += EAX. */
void add_to_counter_4191c4(uint32_t delta)
{
    EF_U32(0x004191c4u) = EF_U32(0x004191c4u) + delta;
}

/* 0xa7bd4, 19 bytes. Byte 0x1371fc = 1, byte 0x1371ee = 0. */
void set_latches_1371fc(void)
{
    EF_U8(0x001371fcu) = 1;
    EF_U8(0x001371eeu) = 0;
}

/* 0xa7df4, 41 bytes. Copy 100 dwords from 0x418e38 to 0x418fc8. */
void save_table_418e38(void)
{
    int16_t i;
    for (i = 0; i < 100; ++i)
        EF_U32(0x00418fc8u + (uint32_t)i * 4u) = EF_U32(0x00418e38u + (uint32_t)i * 4u);
}

/* 0xa7e20, 41 bytes. Copy 100 dwords from 0x418fc8 back to 0x418e38. */
void restore_table_418e38(void)
{
    int16_t i;
    for (i = 0; i < 100; ++i)
        EF_U32(0x00418e38u + (uint32_t)i * 4u) = EF_U32(0x00418fc8u + (uint32_t)i * 4u);
}

/* 0xa86e4, 50 bytes. When byte 0x41f833 is 0x39 and signed word 0x425fa6 is
 * below signed word 0x4194ae, copy the latter over it and clear the byte. */
void clamp_word_425fa6(void)
{
    if (EF_U8(0x0041f833u) == 0x39) {
        int16_t limit = EF_S16(0x004194aeu);
        if (EF_S16(0x00425fa6u) < limit) {
            EF_S16(0x00425fa6u) = limit;
            EF_U8(0x0041f833u) = 0;
        }
    }
}

/* 0xa944c, 28 bytes. Decrement word 0x4194ac; dword 0x4191dc = table
 * 0x418e38[signed high word of dword 0x4194aa]. */
void pop_table_entry_4191dc(void)
{
    EF_U16(0x004194acu) = (uint16_t)(EF_U16(0x004194acu) - 1u);
    EF_U32(0x004191dcu) = EF_U32(0x00418e38u + (uint32_t)((EF_S32(0x004194aau) >> 16) * 4));
}

/* 0xa9468, 40 bytes. Random 16-bit value times signed SI, divided by 65536
 * rounding toward zero (the SBB sequence). */
int32_t random_scaled(int16_t scale)
{
    int32_t product = (int32_t)((uint32_t)guest_read_selector_word() * (uint32_t)(int32_t)scale);
    return product / 65536;
}

/* 0xa98d0, 48 bytes. Latch several words and dwords after 0xa7bb0. */
void save_state_a98d0(void)
{
    latch_b3f50();
    EF_U16(0x004194beu) = EF_U16(0x004194c2u);
    EF_U32(0x004191f4u) = EF_U32(0x00419200u);
    EF_U32(0x004191e0u) = EF_U32(0x004191f8u);
    EF_U32(0x004191e8u) = EF_U32(0x004191dcu);
}

/* 0xa9900, 43 bytes. Reverse of 0xa98d0 (without the 0xa7bb0 call). */
void restore_state_a9900(void)
{
    EF_U16(0x004194c2u) = EF_U16(0x004194beu);
    EF_U32(0x00419200u) = EF_U32(0x004191f4u);
    EF_U32(0x004191f8u) = EF_U32(0x004191e0u);
    EF_U32(0x004191dcu) = EF_U32(0x004191e8u);
}

/* 0xa9990, 47 bytes. Load the live words from a second saved set, then
 * tail-call 0xb3f50. */
uint32_t restore_state_a9990(void)
{
    EF_U16(0x004194c2u) = EF_U16(0x004194bcu);
    EF_U32(0x00419200u) = EF_U32(0x004191d8u);
    EF_U32(0x004191f8u) = EF_U32(0x004191e4u);
    EF_U32(0x004191dcu) = EF_U32(0x004191f0u);
    return guest_fn_b3f50();
}

/* 0xa9bbc, 17 bytes. When byte 0x1371ff is zero clear byte 0x1371ee. */
void clear_latch_if_idle(void)
{
    if (EF_U8(0x001371ffu) == 0)
        EF_U8(0x001371eeu) = 0;
}

/* 0xa9bd0, 25 bytes. Byte 0x137200 = 1 when 0x1371ff is set, else byte 0x1371f8 = 1. */
void request_by_state(void)
{
    if (EF_U8(0x001371ffu) != 0)
        EF_U8(0x00137200u) = 1;
    else
        EF_U8(0x001371f8u) = 1;
}

/* 0xa9bec, 16 bytes. Clear byte 0x1371ee, then call 0xf41dc. */
void clear_latch_and_notify(void)
{
    EF_U8(0x001371eeu) = 0;
    guest_fn_f41dc();
}

/* 0xaa198, 39 bytes. Follow the singly linked list from dword 0x4191fc as
 * many steps as the dword at +1 says (at least none) and store the node there. */
void resolve_link_at_1(ef_addr32 record)
{
    ef_addr32 node = EF_U32(0x004191fcu);
    int32_t steps = EF_S32(record + 1u);
    if (steps > 0) {
        int32_t i = 0;
        do {
            ++i;
            node = EF_U32(node);
        } while (i < EF_S32(record + 1u));
    }
    EF_U32(record + 1u) = node;
}

/* 0xaa1c0, 36 bytes. As 0xaa198 for the dword at +0. */
void resolve_link_at_0(ef_addr32 record)
{
    ef_addr32 node = EF_U32(0x004191fcu);
    int32_t steps = EF_S32(record);
    if (steps > 0) {
        int32_t i = 0;
        do {
            ++i;
            node = EF_U32(node);
        } while (i < EF_S32(record));
    }
    EF_U32(record) = node;
}

/* 0xaa4b8, 26 bytes. Step AX (signed word) times along the chain starting at
 * dword +8 of EDX (next pointer at +1); returns the node reached. */
ef_addr32 walk_chain_from_8(int16_t steps, ef_addr32 record)
{
    ef_addr32 node = EF_U32(record + 8u);
    if (steps > 0) {
        int16_t i = 0;
        do {
            ++i;
            node = EF_U32(node + 1u);
        } while (i < steps);
    }
    return node;
}

/* 0xab0b0, 42 bytes. Two 0xa7a1c(EAX, ...) stack calls with record fields. */
void emit_record_pair_ab0b0(ef_addr32 target, ef_addr32 record)
{
    guest_fn_a7a1c(target, 2, record + 0xau);
    guest_fn_a7a1c(target, (uint32_t)(EF_S32(record + 6u) >> 16), EF_U32(record + 0xeu));
}

/* 0xab260, 42 bytes. As 0xab0b0 through 0xa79d4. */
void emit_record_pair_ab260(ef_addr32 target, ef_addr32 record)
{
    guest_fn_a79d4(target, 2, record + 0xau);
    guest_fn_a79d4(target, (uint32_t)(EF_S32(record + 6u) >> 16), EF_U32(record + 0xeu));
}

/* 0xab8b0, 17 bytes. Dword 0x41ec14 = 0xc382c(). */
void latch_c382c(void)
{
    EF_U32(0x0041ec14u) = guest_fn_c382c();
}

/* 0xab8c4, 28 bytes. Dword 0x137270 = dword 0x13726c; 0xb3fb0(that, 0, dword 0x41ecd4). */
void clear_pool_ab8c4(void)
{
    uint32_t base = EF_U32(0x0013726cu);
    uint32_t size = EF_U32(0x0041ecd4u);
    EF_U32(0x00137270u) = base;
    guest_fn_b3fb0(base, 0, size);
}

/* 0xab8e0, 38 bytes. Allocate EAX bytes via 0xe29e8; record base, cursor,
 * size and end; then clear the pool. */
void init_pool_ab8e0(uint32_t size)
{
    uint32_t base = guest_fn_e29e8(size);
    EF_U32(0x0013726cu) = base;
    EF_U32(0x00137270u) = base;
    EF_U32(0x0041ecd4u) = size;
    EF_U32(0x00137274u) = base + size;
    clear_pool_ab8c4();
}

/* 0xabe24, 31 bytes. Clear 0x150 bytes at 0x41eab0 and word 0x41eeca. */
void clear_block_abe24(void)
{
    guest_fn_b3fb0(0x0041eab0u, 0, 0x150);
    EF_U16(0x0041eecau) = 0;
}

/* 0xabf24, 45 bytes. When non-null store EAX into eight dwords spaced 0x12 apart from 0x1621be. */
void set_eight_pointers(uint32_t value)
{
    if (value != 0) {
        uint32_t i;
        for (i = 0; i < 8; ++i)
            EF_U32(0x001621beu + i * 0x12u) = value;
    }
}

/* 0xac980, 32 bytes. Words 0x137950..56 = AX, DX, BX, CX; tail-call 0xac7f0. */
void set_view_words_and_update(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    EF_U16(0x00137950u) = a;
    EF_U16(0x00137952u) = b;
    EF_U16(0x00137954u) = c;
    EF_U16(0x00137956u) = d;
    guest_fn_ac7f0();
}

/* 0xac9e8, 42 bytes. 0xac980(0, 0, high word 0x15b742, high word 0x15b746); dword 0x41ec18 = 0. */
void reset_view_ac9e8(void)
{
    set_view_words_and_update(0, 0, (uint16_t)(EF_S32(0x0015b742u) >> 16),
                              (uint16_t)(EF_S32(0x0015b746u) >> 16));
    EF_U32(0x0041ec18u) = 0;
}

/* 0xaca88, 12 bytes. Word 0x41eeda = 0. */
void clear_word_41eeda(void)
{
    EF_U16(0x0041eedau) = 0;
}

/* 0xaca94, 41 bytes. Word 0x41eeda = 1; when word 0x41eedc is zero, clear
 * word 0x41eede and run 0xaca14. */
void set_word_41eeda(void)
{
    uint16_t pending = EF_U16(0x0041eedcu);
    EF_U16(0x0041eedau) = 1;
    if (pending == 0) {
        EF_U16(0x0041eedeu) = 0;
        guest_fn_aca14();
    }
}

/* 0xb03e8, 26 bytes. Clear three words 0x13799e, 0x1379a0 and 0x1379a2. */
void clear_words_13799e(void)
{
    EF_U16(0x0013799eu) = 0;
    EF_U16(0x001379a0u) = 0;
    EF_U16(0x001379a2u) = 0;
}

/* 0xb1814, 41 bytes. Free each node of the list at *head via 0xb17d4, then clear *head and *tail. */
void free_list_b1814(ef_addr32 head, ef_addr32 tail)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        ef_addr32 next = EF_U32(node + 4u);
        guest_fn_b17d4(node);
        node = next;
    }
    EF_U32(head) = 0;
    EF_U32(tail) = 0;
}

/* 0xb188c, 41 bytes. As 0xb1814 through 0xb1840. */
void free_list_b188c(ef_addr32 head, ef_addr32 tail)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        ef_addr32 next = EF_U32(node + 4u);
        guest_fn_b1840(node);
        node = next;
    }
    EF_U32(head) = 0;
    EF_U32(tail) = 0;
}

/* 0xb1974, 41 bytes. As 0xb1814 through 0xb1918. */
void free_list_b1974(ef_addr32 head, ef_addr32 tail)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        ef_addr32 next = EF_U32(node + 4u);
        guest_fn_b1918(node);
        node = next;
    }
    EF_U32(head) = 0;
    EF_U32(tail) = 0;
}

/* 0xb19e4, 41 bytes. As 0xb1814 through 0xb19a0. */
void free_list_b19e4(ef_addr32 head, ef_addr32 tail)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        ef_addr32 next = EF_U32(node + 4u);
        guest_fn_b19a0(node);
        node = next;
    }
    EF_U32(head) = 0;
    EF_U32(tail) = 0;
}

/* 0xb1ae4, 41 bytes. As 0xb1814 through 0xb1a68. */
void free_list_b1ae4(ef_addr32 head, ef_addr32 tail)
{
    ef_addr32 node = EF_U32(head);
    while (node != 0) {
        ef_addr32 next = EF_U32(node + 4u);
        guest_fn_b1a68(node);
        node = next;
    }
    EF_U32(head) = 0;
    EF_U32(tail) = 0;
}

/* 0xb1a10, 42 bytes. Release *EAX via 0xcc768 when set, clear it and dwords 0x1379a8/0x1379b0. */
void release_pointer_b1a10(ef_addr32 slot)
{
    if (EF_U32(slot) != 0)
        guest_fn_cc768(EF_U32(slot));
    EF_U32(slot) = 0;
    EF_U32(0x001379a8u) = 0;
    EF_U32(0x001379b0u) = 0;
}

/* 0xb1a3c, 42 bytes. As 0xb1a10 with dwords 0x1379ac and 0x1379b4. */
void release_pointer_b1a3c(ef_addr32 slot)
{
    if (EF_U32(slot) != 0)
        guest_fn_cc768(EF_U32(slot));
    EF_U32(slot) = 0;
    EF_U32(0x001379acu) = 0;
    EF_U32(0x001379b4u) = 0;
}

/* 0xb2e64, 50 bytes. Count the 9-byte entries (pointer +0x4a, count word
 * +0x48) whose byte +3 is nonzero. */
uint32_t count_active_entries(ef_addr32 record)
{
    uint32_t count = 0;
    int16_t i = 0;
    if (EF_S16(record + 0x48u) > 0) {
        ef_addr32 entries = EF_U32(record + 0x4au);
        do {
            if (EF_U8(entries + (uint32_t)((int32_t)i * 9) + 3u) != 0)
                ++count;
            ++i;
        } while (i < EF_S16(record + 0x48u));
    }
    return count;
}

/* 0xb33ac, 22 bytes. Dword 0x41f354 = 0xa4574(0); tail-call 0xf9ecd(0x41f354). */
uint32_t latch_and_forward_b33ac(void)
{
    EF_U32(0x0041f354u) = guest_fn_a4574(0);
    return guest_fn_f9ecd(0x0041f354u);
}

/* 0xb4e48, 43 bytes. First index >= AX with a zero byte +3 in the 9-byte
 * entry table of EDX, or -1. */
int32_t find_free_entry(int16_t start, ef_addr32 record)
{
    int16_t i = start;
    if (i < EF_S16(record + 0x48u)) {
        ef_addr32 entries = EF_U32(record + 0x4au);
        do {
            if (EF_U8(entries + (uint32_t)((int32_t)i * 9) + 3u) == 0)
                return (int32_t)i;
            ++i;
        } while (i < EF_S16(record + 0x48u));
    }
    return -1;
}

/* 0xb549c, 49 bytes. q = AX / 8 truncated; 60 when q <= 0 else 60 / q. */
int32_t divisor_for_rate(int16_t rate)
{
    int16_t q = (int16_t)((int32_t)rate / 8);
    if (q <= 0)
        return 0x3c;
    return 0x3c / (int32_t)q;
}

/* 0xb5580, 34 bytes. Random 16-bit value modulo the signed high word of dword +0x1c. */
int32_t random_modulo_high_word(ef_addr32 record)
{
    int32_t modulus = EF_S32(record + 0x1cu) >> 16;
    return (int32_t)guest_read_selector_word() % modulus;
}

/* 0xb43f4, 47 bytes. Cross product EDX x EBX into EAX (three doubles each). */
void cross_product_doubles(ef_addr32 out, ef_addr32 a, ef_addr32 b)
{
    const double *va = (const double *)ef_guest_memory_ptr(a);
    const double *vb = (const double *)ef_guest_memory_ptr(b);
    double *vo = (double *)ef_guest_memory_ptr(out);
    vo[0] = va[1] * vb[2] - va[2] * vb[1];
    vo[1] = va[2] * vb[0] - va[0] * vb[2];
    vo[2] = va[0] * vb[1] - va[1] * vb[0];
}

/* 0xb44d0, 25 bytes. out = a + b for three doubles (EDX + EBX into EAX). */
void add_vectors_doubles(ef_addr32 out, ef_addr32 a, ef_addr32 b)
{
    const double *va = (const double *)ef_guest_memory_ptr(a);
    const double *vb = (const double *)ef_guest_memory_ptr(b);
    double *vo = (double *)ef_guest_memory_ptr(out);
    vo[0] = va[0] + vb[0];
    vo[1] = va[1] + vb[1];
    vo[2] = va[2] + vb[2];
}

/* 0xb44ec, 25 bytes. out = a - b for three doubles (EDX - EBX into EAX). */
void sub_vectors_doubles(ef_addr32 out, ef_addr32 a, ef_addr32 b)
{
    const double *va = (const double *)ef_guest_memory_ptr(a);
    const double *vb = (const double *)ef_guest_memory_ptr(b);
    double *vo = (double *)ef_guest_memory_ptr(out);
    vo[0] = va[0] - vb[0];
    vo[1] = va[1] - vb[1];
    vo[2] = va[2] - vb[2];
}
