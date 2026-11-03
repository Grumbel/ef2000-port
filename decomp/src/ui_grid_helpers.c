/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

static uint16_t read_word(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_dword(const uint8_t *p)
{
    return (uint32_t)read_word(p) | ((uint32_t)read_word(p + 2) << 16);
}

static void write_word(uint32_t address, uint16_t value)
{
    uint8_t *p = (uint8_t *)ef_guest_memory_ptr(address);
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

/* 0x40774, 0x36 bytes. The three words are signed. The Watcom arithmetic
 * computes a*3600+b*60+c, wrapping at 32 bits rather than saturating. */
int32_t clock_words_to_seconds(ef_addr32 clock)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(clock);
    uint32_t a = (uint32_t)(int32_t)(int16_t)read_word(p);
    uint32_t b = (uint32_t)(int32_t)(int16_t)read_word(p + 2);
    uint32_t c = (uint32_t)(int32_t)(int16_t)read_word(p + 4);
    return (int32_t)(a * 3600u + b * 60u + c);
}

/* 0x50a1c, 0x36 bytes. Search index is AL and wraps at 256. The original
 * would run forever with a limit over 255 and no match: keep that behavior. */
uint8_t tactical_lookup_contains(uint8_t code)
{
    uint8_t index = 0;
    while ((int16_t)index < g_tactical_lookup_count) {
        if (g_tactical_lookup_codes[index] == code) { return 1; }
        ++index;
    }
    return 0;
}

/* 0x85144, 0x25 bytes. Zero AL iff all three signed words are -1. */
uint8_t clock_triplet_is_present(ef_addr32 clock)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(clock);
    return read_word(p) != 0xffffu || read_word(p + 2) != 0xffffu ||
           read_word(p + 4) != 0xffffu;
}

/* 0x85358, 0x26 bytes. Signed AX row, 0xca-byte guest records. */
void decrement_grid_activity(int16_t row)
{
    ef_world_grid_cell *cell =
        (ef_world_grid_cell *)ef_guest_memory_ptr(g_world_grid_base + (int32_t)row * 0xca);
    if (cell->activity_countdown != 0) { --cell->activity_countdown; }
}

/* 0x86920, 0x47 bytes. Five indices all become one; four independent
 * counters are zeroed. Original EAX ends at 5. */
uint32_t reset_ui_panel_selection(void)
{
    unsigned i;
    g_ui_panel_zero_a[0] = g_ui_panel_zero_a[1] = 0;
    g_ui_panel_zero_b[0] = g_ui_panel_zero_b[1] = 0;
    for (i = 0; i < 5; ++i) { g_ui_panel_defaults[i] = 1; }
    return 5;
}

/* 0x87f9c, 0x47 bytes. This is a circular list with the sentinel held in
 * a separate global; the header's +0x34 field points to the first node. */
ef_addr32 find_ui_linked_tag(uint8_t tag, int16_t value)
{
    const uint8_t *head = (const uint8_t *)ef_guest_memory_ptr(g_ui_linked_list_head);
    ef_addr32 node = read_dword(head + 0x34);
    while (node != g_ui_linked_list_sentinel) {
        const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(node);
        if (p[0x50] == tag && (int16_t)read_word(p + 0x51) == value) {
            return node;
        }
        node = read_dword(p + 0x34);
    }
    return 0;
}

/* 0x8d0e0, 0x39 bytes. Visit every linked element, including duplicates;
 * EAX returns the sentinel after the final +0x34 traversal. */
ef_addr32 mark_ui_links_by_slot(int16_t slot)
{
    const uint8_t *head = (const uint8_t *)ef_guest_memory_ptr(g_ui_linked_list_head);
    ef_addr32 node = read_dword(head + 0x34);
    while (node != g_ui_linked_list_sentinel) {
        uint8_t *p = (uint8_t *)ef_guest_memory_ptr(node);
        ef_addr32 next = read_dword(p + 0x34);
        if ((int16_t)read_word(p + 0x56) == slot) { p[0x42] = 1; }
        node = next;
    }
    return node;
}

/* 0x809bc, 0x59 bytes. C remainder is truncated toward zero, matching IDIV
 * for negative AX. Clears strip entries 1..16, not 0..15. The fixed addresses
 * may be indexed below their nominal start when the remainder is negative. */
int32_t clear_panel_strip(int16_t slot)
{
    int32_t band = (int32_t)slot % 16;
    int32_t i;
    for (i = 1; i <= 16; ++i) {
        int32_t index = band * 16 + i;
        uint32_t address = (uint32_t)(0x001f90a0 + index * 0x38);
        *(uint8_t *)ef_guest_memory_ptr(address) = 0;
    }
    write_word((uint32_t)(0x001fc8a6 + band * 12), 0);
    write_word((uint32_t)(0x001fc8a2 + band * 12), 0xffffu);
    return band * 3;
}
