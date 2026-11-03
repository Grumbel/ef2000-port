/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <string.h>

uint8_t g_world_grid_marker;
uint8_t g_mode_input_enabled_a;
uint16_t g_packet_2b_word;
uint8_t g_packet_2b_byte;
uint8_t g_ui_status_flag_a, g_ui_status_flag_b, g_ui_status_flag_c;
float g_ui_status_scalar;
uint8_t g_tactical_display_mode;
uint8_t g_tactical_indicator_source, g_tactical_indicator_latch;

static uint8_t last_packet[32];
static uint32_t last_packet_length;
static unsigned packet_count, format_pair_count, format_float_count;
static unsigned draw_count, callback_count;
static ef_addr32 last_destination, last_format, last_item;
static uint32_t last_first, last_second, last_mode;
static double last_scalar;

void emit_command_packet(const uint8_t *bytes, uint32_t size)
{
    assert(size <= sizeof(last_packet));
    memcpy(last_packet, bytes, size);
    last_packet_length = size;
    ++packet_count;
}

void format_ui_status_pair(ef_addr32 destination, ef_addr32 format,
                           uint32_t first, uint32_t second)
{
    last_destination = destination;
    last_format = format;
    last_first = first;
    last_second = second;
    ++format_pair_count;
}

void format_ui_status_float(ef_addr32 destination, ef_addr32 format, double value)
{
    last_destination = destination;
    last_format = format;
    last_scalar = value;
    ++format_float_count;
}

void draw_tactical_mode_item(uint32_t mode, ef_addr32 item)
{
    last_mode = mode;
    last_item = item;
    ++draw_count;
}

void read_tactical_indicator_callback(void)
{
    ++callback_count;
    g_tactical_indicator_source = 0x57;
}

static void packet_equals(const uint8_t *expected, unsigned length)
{
    assert(last_packet_length == length);
    assert(memcmp(last_packet, expected, length) == 0);
}

static void test_packets(void)
{
    static const uint8_t a[] = {
        0x15, 0x34, 0x12, 0xfe, 0xff, 0x67, 0xef, 0xcd, 0xab, 0x89,
        0x04, 0x03, 0x02, 0x01, 0x44, 0x33, 0x22, 0x11
    };
    static const uint8_t b[] = {0x41, 0x98, 0xba, 0x57, 0x13, 0x24, 0xce, 0xfa, 0x91};
    static const uint8_t c[] = {0x2b, 0x34, 0x12, 0x68, 0x24, 0xef, 0xcd, 0x25, 0x79, 0x86};

    send_packet_15(0x1234, -2, 0x67, 0x89abcdefu, 0x01020304u, 0x11223344u);
    packet_equals(a, sizeof(a));
    g_world_grid_marker = 0x91;
    send_packet_41((int16_t)0xba98, 0x1357, 0x24, 0xface);
    packet_equals(b, sizeof(b));
    g_packet_2b_word = 0x7925;
    g_packet_2b_byte = 0x86;
    send_packet_2b(0x1234, (int16_t)0xcdef, 0x2468);
    packet_equals(c, sizeof(c));
    assert(packet_count == 3);
}

static void test_status_formatters(void)
{
    g_mode_input_enabled_a = 1;
    g_ui_status_flag_a = 7;
    write_ui_status_flags_a(0x210000);
    assert(last_destination == 0x210000 && last_format == 0x188b58);
    assert(last_first == 1 && last_second == 7);
    g_ui_status_flag_c = 2;
    g_ui_status_flag_b = 9;
    write_ui_status_flags_b(0x210080);
    assert(last_destination == 0x210080 && last_format == 0x188b74);
    assert(last_first == 2 && last_second == 9 && format_pair_count == 2);
    g_ui_status_scalar = 1.25f;
    write_ui_status_float(0x210100);
    assert(last_destination == 0x210100 && last_format == 0x188bfc);
    assert(last_scalar == 1.25 && format_float_count == 1);
}

static void test_tactical_helpers(void)
{
    g_tactical_display_mode = 1;
    draw_tactical_item_mode(0x34567);
    assert(last_mode == 0 && last_item == 0x34567);
    g_tactical_display_mode = 0;
    draw_tactical_item_mode(0x45678);
    assert(last_mode == 1 && last_item == 0x45678 && draw_count == 2);

    g_tactical_indicator_source = 0x12;
    g_tactical_indicator_latch = 0;
    refresh_tactical_indicator();
    assert(callback_count == 1 && g_tactical_indicator_latch == 0x57);
}

int main(void)
{
    test_packets();
    test_status_formatters();
    test_tactical_helpers();
    return 0;
}
