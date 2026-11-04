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

/* 0x8557c, 73 bytes. Return 0 when the owner has no weight of the class implied by its type byte (+0x1b: filter 1 for types 0-2, 9, 13; filter 2 for 6, 7, 10, 11), else 1. */
uint32_t owner_has_weight_8557c(uint32_t owner)
{
    int16_t o = (int16_t)owner;
    uint32_t type = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)o * 0xca) + 0x1bu);
    uint8_t filter;
    switch (type) {
    case 0: case 1: case 2: case 9: case 13: filter = 1u; break;
    case 6: case 7: case 10: case 11: filter = 2u; break;
    default: return 1u;
    }
    if ((uint16_t)sum_owner_weights_2af78(o, filter) == 0u)
        return 0u;
    return 1u;
}

/* 0xd5ec0, 75 bytes. Tear down a loaded block record: trace-call 0x1031a0 with the handle at 0x420a64, release its range (+9 pointer, +5 length - 1), reset the MIDI channels and forget it if it is the current block at 0x420a68. */
void release_block_d5ec0(ef_addr32 block)
{
    if (EF_U8(0x0015b968u) == 0u)
        return;
    trace_call_1031a0(EF_U32(0x00420a64u));
    stack_range_104920(EF_U32(block + 9u), EF_U32(block + 5u) - 1u);
    midi_reset_channels_d6024();
    if (block == EF_U32(0x00420a68u))
        EF_U32(0x00420a68u) = 0u;
}

/* 0x84678, 79 bytes. Advance the page counter, run the transition selected by bits 3..5 of the word at 0x1fcbe2 (1..4), then call the handler indexed by the word at 0x12b4a6 in the table at 0x12b4a8. */
void run_state_handler_84678(void)
{
    advance_page_counter_83b50();
    switch ((EF_U16(0x001fcbe2u) >> 3) & 7u) {
    case 1: reset_pending_mode_844e4(); break;
    case 2: initialize_ui_component(); break;
    case 3: set_pending_mode_two(); break;
    case 4: advance_state_8461c(); break;
    default: break;
    }
    guest_callptr0_x(EF_U32(0x0012b4a8u + (uint32_t)EF_U16(0x0012b4a6u) * 4u));
}

/* 0x912b8, 79 bytes. While the pending-message count at 0x2144b2 is positive, shrink it by the limit at 0x127d92 and draw the message at row 5 near the right edge. */
void draw_pending_message_912b8(ef_addr32 text)
{
    int32_t count = EF_S16(0x002144b2u);
    int32_t width;
    if (count <= 0)
        return;
    EF_U16(0x002144b2u) = (uint16_t)(count - (int32_t)EF_U16(0x00127d92u));
    EF_U8(0x00127b1au) = 1u;
    width = (int16_t)guest_fn_48d5c_x(text);
    guest_fn_48c90(0xfu, (uint32_t)(int32_t)(int16_t)(0x136 - width), 5u, text);
}

/* 0x106cb0, 78 bytes. Read count bytes from a ring record into buf, waiting up to timeout; returns 0 when done or 3 on timeout. */
uint32_t read_ring_bytes_106cb0(ef_addr32 rec, ef_addr32 buf, uint32_t count, uint32_t timeout)
{
    uint32_t start = tail_jump_106be0(rec);
    uint32_t n = 0u;
    for (;;) {
        if (entry_empty_118520(EF_U32(rec)) == 0u) {
            EF_U8(buf++) = (uint8_t)ring_pop_1184c4(EF_U32(rec));
            n++;
            if (n == count)
                return 0u;
        } else if (elapsed_at_least((int32_t)start, (int32_t)timeout) != 0u) {
            return 3u;
        }
    }
}
