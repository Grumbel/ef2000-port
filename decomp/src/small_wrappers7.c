/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small table and list helpers. */

/* 0xaaec, 61 bytes. Pick 3, 6 or 9 from a random number modulo 10. */
uint32_t random_step_aaec(void)
{
    switch ((half_word_9c960() & 0xffffu) % 10u) {
    case 0: case 1: case 5: return 3u;
    case 2: case 3: case 8: case 9: return 6u;
    default: return 9u;
    }
}

/* 0x9c1e0, 63 bytes. Sum 16 times the value of 0xeb2a8 and the +4 field of every node on the list headed at 0x132a70 (node pointers go through 0x9c1bb). */
uint32_t sum_list_9c1e0(void)
{
    uint32_t total = guest_fn_eb2a8_x() << 4;
    uint32_t seg = EF_U32(0x00132a70u);
    while (seg != 0u) {
        uint32_t node = real_mode_to_linear_9c1bb(0u, 0u, 0u, 0u, seg);
        total += EF_U32(node + 4u);
        node = real_mode_to_linear_9c1bb(0u, 0u, 0u, 0u, seg);
        seg = EF_U32(node);
    }
    return total;
}

/* 0xcdfe8, 68 bytes. Query service 0x33 sub-function 3 through 0x8b8 and return the low three bits of the status, half the x value and the y value. */
void query_pointer_cdfe8(ef_addr32 x_out, ef_addr32 y_out, ef_addr32 buttons_out)
{
    EF_U32(0x004201ccu) = 3u;
    guest_fn_8b8(0x33u, 0x004201b0u);
    EF_U16(buttons_out) = (uint16_t)(EF_U8(0x004201c0u) & 7u);
    EF_U16(x_out) = (uint16_t)((int32_t)EF_S32(0x004201c8u) >> 1);
    EF_U16(y_out) = EF_U16(0x004201c4u);
}

/* 0xa0060, 65 bytes. Reset the time scale at 0x132b20 and, if 0x137a00 is set, run 0xa0174 on every entry of the 0x4e-byte chain starting at 0x132b26, then 0xa00e8. */
void run_entry_chain_a0060(void)
{
    int32_t idx;
    EF_U32(0x00132b20u) = 0x3e000000u;
    if (EF_U8(0x00137a00u) == 0u)
        return;
    idx = (int32_t)EF_S16(0x00132b26u);
    while (idx != -1) {
        guest_fn_a0174_x(0x00416e40u + (uint32_t)(idx * 0x4e));
        idx = (int32_t)EF_S16(0x00416e44u + (uint32_t)(idx * 0x4e));
    }
    guest_fn_a00e8_x();
}

/* 0xa9fc, 69 bytes. Pick a delay (0x28, 0x46, 0x78 or 0xc8) from a random number modulo 4. */
uint32_t random_delay_a9fc(void)
{
    switch ((half_word_9c960() & 0xffffu) % 4u) {
    case 0: return 0x28u;
    case 1: return 0x46u;
    case 3: return 0xc8u;
    default: return 0x78u;
    }
}

/* 0xcbcc5, 74 bytes. Convert an unsigned number to text in the given base (digits 0-9, a-z) at dst, returning dst. */
ef_addr32 utoa_cbcc5(uint32_t value, ef_addr32 dst, uint32_t base)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    uint8_t tmp[0x28];
    uint32_t n = 1u;
    uint32_t out = dst;
    uint8_t c;
    tmp[0] = 0u;
    do {
        uint32_t q = value / base;
        tmp[n++] = (uint8_t)digits[value % base];
        value = q;
    } while (value != 0u);
    do {
        n--;
        c = tmp[n];
        EF_U8(out++) = c;
    } while (c != 0u);
    return dst;
}
