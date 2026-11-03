/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

#include <string.h>

static uint16_t read16(ef_addr32 address)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(address);
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read32(ef_addr32 address)
{
    return (uint32_t)read16(address) | ((uint32_t)read16(address + 2u) << 16);
}

static void write16(ef_addr32 address, uint16_t value)
{
    uint8_t *p = (uint8_t *)ef_guest_memory_ptr(address);
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void write32(ef_addr32 address, uint32_t value)
{
    write16(address, (uint16_t)value);
    write16(address + 2u, (uint16_t)(value >> 16));
}

/* 0x59420, 99 bytes. The game doubles low words only in display mode 1;
 * addition and sign extension wrap at 16 bits for both coordinates. The
 * caller's original EBX is the fifth stack argument of 0x48bb0. */
void draw_tactical_coordinates(int16_t x, int16_t y, uint32_t aux)
{
    uint32_t mode = g_tactical_display_mode == 1 ? 0u : 1u;
    unsigned factor = mode == 0 ? 2u : 1u;
    int32_t mx = (int16_t)((uint16_t)((uint32_t)(uint16_t)x * factor +
                                      (uint16_t)g_tactical_coordinate_offset_x));
    int32_t my = (int16_t)((uint16_t)((uint32_t)(uint16_t)y * factor +
                                      (uint16_t)g_tactical_coordinate_offset_y));
    draw_tactical_coordinate_marker(g_tactical_marker_code, mode, mx, my, aux);
}

/* 0x594a4, 41 bytes. In mode 1 the guest first sign-extends the low
 * 16 bits of 0x48c34's return and then arithmetic-shifts by one. */
int32_t draw_tactical_item_metric(ef_addr32 item)
{
    if (g_tactical_display_mode == 1) {
        return (int16_t)(uint16_t)draw_tactical_mode_item(0, item) >> 1;
    }
    return draw_tactical_mode_item(1, item);
}

/* 0x66ac8, 38 bytes. The input dword overlaps the output word at +2.
 * Preserve the low half while rewriting the high half, saturating negative
 * returned AX values to the guest 0xffff sentinel. */
void update_position_bin(void)
{
    int32_t coordinate = (int32_t)g_position_bin_packed >> 16;
    int16_t index = (int16_t)bin_coordinate_to_slot(coordinate);
    uint16_t stored = index < 0 ? 0xffffu : (uint16_t)index;
    g_position_bin_packed = (g_position_bin_packed & 0xffffu) |
                            ((uint32_t)stored << 16);
}

/* 0x741f0, 53 bytes. Ignore objects when the global event gate is zero.
 * An empty +0x12 link selects event 0x26; otherwise event 0x12. */
void report_tactical_object_action(ef_addr32 object)
{
    if (g_tactical_action_enabled) {
        uint32_t code = read32(object + 0x12) == 0 ? 0x26u : 0x12u;
        int32_t index = (int32_t)read32(object + 0x1e) >> 16;
        notify_tactical_action(index, code);
    }
}

/* 0x7a3b0, 54 bytes. This builds one shared description string and returns
 * its address, forwarding the input suffix after a constant prefix. */
ef_addr32 build_status_description(ef_addr32 suffix)
{
    guest_string_copy(0x001f8ec4, 0x001f8d60);
    guest_string_append(0x001f8ec4, 0x00187b74);
    guest_string_append(0x001f8ec4, suffix);
    return 0x001f8ec4;
}

/* 0x87970, 81 bytes. The selected index is signed high word of +0x14.
 * Its 6-byte record entry stores a dword child and a word state. */
void refresh_record_selection(ef_addr32 record)
{
    int32_t index = (int32_t)read32(record + 0x14) >> 16;
    ef_addr32 slots = read32(record + 0x12);
    ef_addr32 entry = slots + (uint32_t)(index * 6);
    ef_addr32 child = read32(entry + 2);
    write32(record + 0x3c, child);
    write16(record + 0x40, read16(entry));
    if (child != 0 && original_record_selection_check(record) == 1) {
        write16(record + 0x16, (uint16_t)(read16(record + 0x16) + 1u));
    }
}

/* 0x879c4, 89 bytes. Move the first cursor node after the anchor in a
 * doubly linked ring. The missing-cursor path calls the original allocator. */
void rotate_ui_record_ring(void)
{
    ef_addr32 current, anchor, after_anchor;
    if (g_ui_record_cursor == 0) {
        initialize_ui_record_ring(g_ui_record_head);
    }
    current = g_ui_record_cursor;
    g_ui_record_cursor = read32(current + 0x34);
    anchor = g_ui_record_anchor;
    if (g_ui_record_end == read32(anchor + 0x34)) {
        g_ui_record_head = current;
    }
    after_anchor = read32(anchor + 0x34);
    write32(current + 0x34, after_anchor);
    write32(after_anchor + 0x38, current);
    write32(current + 0x38, anchor);
    write32(anchor + 0x34, current);
    write16(current + 0x51, read16(current + 0x54));
}

/* 0x8f5f4, 41 bytes. Guest 0xb3fb0 clears exactly 0x2260 bytes here. */
void reset_ui_record_pool(void)
{
    g_ui_record_pool_count = 0;
    memset(ef_guest_memory_ptr(0x0020f550), 0, 0x2260);
    g_ui_record_pool_pointer = 0;
}
