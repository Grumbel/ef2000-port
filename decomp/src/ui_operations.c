/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

static uint16_t load16(ef_addr32 address)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(address);
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t load32(ef_addr32 address)
{
    return (uint32_t)load16(address) | ((uint32_t)load16(address + 2) << 16);
}

static void store16(ef_addr32 address, uint16_t value)
{
    uint8_t *p = (uint8_t *)ef_guest_memory_ptr(address);
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void store32(ef_addr32 address, uint32_t value)
{
    store16(address, (uint16_t)value);
    store16(address + 2, (uint16_t)(value >> 16));
}

/* 0x577ac, 57 bytes. Original EAX/EDX/EBX/ECX low bytes, then low
 * byte of one caller stack dword, form the 6-byte command. The callback
 * at 0x57820 receives the same local buffer AFTER it was emitted. */
void send_packet_2c(uint8_t a, uint8_t d, uint8_t b, uint8_t c,
                    uint8_t stack_code)
{
    uint8_t packet[6] = {0x2c, a, d, b, c, stack_code};
    emit_command_packet(packet, sizeof(packet));
    guest_packet_after_send(packet);
}

/* 0x5dbc8, 87 bytes. Radius is (word + 2) shifted by the display
 * mode. Only its signed low 16 bits are consumed. Draw horizontal and
 * vertical segments in the original cdecl argument order. */
void draw_tactical_cross(int16_t x, int16_t y)
{
    uint32_t widened = (uint32_t)g_tactical_cross_radius + 2u;
    int32_t radius = (int16_t)(uint16_t)(widened << (g_tactical_display_mode & 31u));
    draw_tactical_line(y, (int32_t)x + radius, y, (int32_t)x - radius,
                       g_tactical_marker_code);
    draw_tactical_line((int32_t)y + radius, x, (int32_t)y - radius, x,
                       g_tactical_marker_code);
}

/* 0x80d40, 72 bytes. 0x80d20 uses the signed AX index and a fixed
 * table pointer. Four 16-bit source words become one raw word and three
 * sign-extended dwords; a missing entry sets sentinel 0xcc. */
void update_ui_record_from_template(int16_t index, ef_addr32 record)
{
    ef_addr32 found = lookup_ui_record_template(0x00188874, index);
    if (found != 0) {
        store16(record + 0x0e, load16(found));
        store32(record + 0x16, (uint32_t)(int32_t)(int16_t)load16(found + 2));
        store32(record + 0x1a, (uint32_t)(int32_t)(int16_t)load16(found + 4));
        store32(record + 0x1e, (uint32_t)(int32_t)(int16_t)load16(found + 6));
    } else {
        store16(record + 0x0e, 0xcc);
        store32(record + 0x16, 0);
        store32(record + 0x1a, 0);
        store32(record + 0x1e, 0);
    }
}

/* 0x84370, 35 bytes. The three global dwords are forwarded as
 * EDX/EBX/ECX; EAX names one fixed composite state record. */
void update_ui_composite(void)
{
    guest_ui_composite_sink(0x001fcc3e, g_ui_composite_arg_d,
                            g_ui_composite_arg_b, g_ui_composite_arg_c);
}

/* 0x866e0, 64 bytes. Scan the original null-terminated array of
 * 32-bit object pointers for object+0x3c. A missing slot invokes the
 * original indirect reporter with fixed message 0x18a3b8. */
uint32_t ui_record_current_is_active(ef_addr32 record)
{
    uint32_t index = 0;
    for (;;) {
        ef_addr32 ptr = load32(0x0012d7b4 + index * 4u);
        if (ptr == 0) {
            ui_report_missing_record(0x0018a3b8);
            return 0;
        }
        if (ptr == load32(record + 0x3c)) { return 1; }
        ++index;
    }
}

/* 0x8be54, 23 bytes. Both guest calls receive the same original
 * EAX/EDX pair, in unlink-then-install order. */
void update_object_with_link(ef_addr32 object, ef_addr32 link)
{
    guest_object_unlink_if_current(object, link);
    guest_object_install_link(object, link);
}

/* 0x82dd0, 45 bytes. Operate in-place only for nonempty text. The
 * guest delimiter helper may return a pointer within the same buffer. */
void compact_ui_fragment(ef_addr32 string)
{
    ef_addr32 fragment;
    if (string == 0 || *(uint8_t *)ef_guest_memory_ptr(string) == 0) { return; }
    fragment = next_ui_fragment(string);
    while (*(uint8_t *)ef_guest_memory_ptr(fragment) != 0) {
        uint8_t value = *(uint8_t *)ef_guest_memory_ptr(fragment++);
        *(uint8_t *)ef_guest_memory_ptr(string++) = value;
    }
    *(uint8_t *)ef_guest_memory_ptr(string) = 0;
}

/* 0x82e00, 61 bytes. Test each token against the second argument for
 * its full length. A zero compare returns the current token pointer;
 * otherwise advance through 0x82da4 until the empty-string sentinel. */
ef_addr32 find_matching_ui_fragment(ef_addr32 list, ef_addr32 needle)
{
    ef_addr32 current = list;
    uint32_t length = guest_string_length(needle);
    for (;;) {
        if (guest_prefix_compare(current, needle, length) == 0) {
            return current;
        }
        current = next_ui_fragment(current);
        if (*(uint8_t *)ef_guest_memory_ptr(current) == 0) { return 0; }
    }
}
