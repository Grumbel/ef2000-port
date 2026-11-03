/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <string.h>

static uint8_t memory[0x240000];
uint16_t g_tactical_cross_radius;
uint8_t g_tactical_display_mode, g_tactical_marker_code;
uint32_t g_ui_composite_arg_d, g_ui_composite_arg_b, g_ui_composite_arg_c;

static unsigned emits, postprocesses, lines, template_lookups, composites;
static unsigned reports, unlink_calls, install_calls, fragment_calls, length_calls, compare_calls;
static uint8_t last_packet[6], after_packet[6];
static ef_addr32 last_table, last_record, last_link, last_report;
static uint32_t last_composite_d, last_composite_b, last_composite_c;
static int32_t last_template_index;
static ef_addr32 template_result;
static char callback_sequence[4];
static struct { int32_t y1, x1, y2, x2; uint32_t color; } last_line[2];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(memory));
    return memory + address;
}

static void put16(ef_addr32 address, uint16_t value)
{
    memory[address] = (uint8_t)value;
    memory[address + 1u] = (uint8_t)(value >> 8);
}

static void put32(ef_addr32 address, uint32_t value)
{
    put16(address, (uint16_t)value);
    put16(address + 2u, (uint16_t)(value >> 16));
}

static uint32_t get32(ef_addr32 address)
{
    return memory[address] | ((uint32_t)memory[address + 1u] << 8) |
           ((uint32_t)memory[address + 2u] << 16) |
           ((uint32_t)memory[address + 3u] << 24);
}

void emit_command_packet(const uint8_t *bytes, uint32_t size)
{
    assert(size == 6);
    memcpy(last_packet, bytes, size);
    ++emits;
}

void guest_packet_after_send(const uint8_t *packet)
{
    assert(emits > postprocesses);
    memcpy(after_packet, packet, 6);
    ++postprocesses;
}

void draw_tactical_line(int32_t y1, int32_t x1, int32_t y2,
                        int32_t x2, uint32_t marker)
{
    assert(lines < 2);
    last_line[lines].y1 = y1;
    last_line[lines].x1 = x1;
    last_line[lines].y2 = y2;
    last_line[lines].x2 = x2;
    last_line[lines].color = marker;
    ++lines;
}

ef_addr32 lookup_ui_record_template(ef_addr32 table, int32_t index)
{
    ++template_lookups;
    last_table = table;
    last_template_index = index;
    return template_result;
}

void guest_ui_composite_sink(ef_addr32 base, uint32_t d,
                             uint32_t b, uint32_t c)
{
    ++composites;
    last_record = base;
    last_composite_d = d;
    last_composite_b = b;
    last_composite_c = c;
}

void ui_report_missing_record(ef_addr32 message)
{
    ++reports;
    last_report = message;
}

void guest_object_unlink_if_current(ef_addr32 object, ef_addr32 link)
{
    callback_sequence[unlink_calls + install_calls] = 'U';
    ++unlink_calls;
    last_record = object;
    last_link = link;
}

void guest_object_install_link(ef_addr32 object, ef_addr32 link)
{
    callback_sequence[unlink_calls + install_calls] = 'I';
    ++install_calls;
    assert(last_record == object && last_link == link);
}

ef_addr32 next_ui_fragment(ef_addr32 string)
{
    ++fragment_calls;
    while (memory[string] && memory[string] != '|') { ++string; }
    return string + (memory[string] == '|');
}

uint32_t guest_string_length(ef_addr32 string)
{
    ++length_calls;
    return (uint32_t)strlen((const char *)(memory + string));
}

int32_t guest_prefix_compare(ef_addr32 a, ef_addr32 b, uint32_t length)
{
    ++compare_calls;
    return memcmp(memory + a, memory + b, length);
}

static void test_packet(void)
{
    static const uint8_t expected[] = {0x2c, 0x34, 0x56, 0x78, 0x9a, 0xbc};
    send_packet_2c(0x34, 0x56, 0x78, 0x9a, 0xbc);
    assert(emits == 1 && postprocesses == 1);
    assert(memcmp(last_packet, expected, 6) == 0);
    assert(memcmp(after_packet, expected, 6) == 0);
}

static void test_cross(void)
{
    g_tactical_cross_radius = 3;
    g_tactical_display_mode = 1;
    g_tactical_marker_code = 0xe1;
    draw_tactical_cross(12, -9);
    assert(lines == 2);
    assert(last_line[0].y1 == -9 && last_line[0].x1 == 22);
    assert(last_line[0].y2 == -9 && last_line[0].x2 == 2);
    assert(last_line[1].y1 == 1 && last_line[1].x1 == 12);
    assert(last_line[1].y2 == -19 && last_line[1].x2 == 12);
    assert(last_line[0].color == 0xe1 && last_line[1].color == 0xe1);
}

static void test_template(void)
{
    const ef_addr32 record = 0x20000;
    template_result = 0x21000;
    put16(template_result, 0xabc0);
    put16(template_result + 2, 0xfffc);
    put16(template_result + 4, 0x7fff);
    put16(template_result + 6, 0x8000);
    update_ui_record_from_template(-3, record);
    assert(template_lookups == 1 && last_table == 0x188874 && last_template_index == -3);
    assert(memory[record + 0x0e] == 0xc0 && memory[record + 0x0f] == 0xab);
    assert(get32(record + 0x16) == 0xfffffffcu);
    assert(get32(record + 0x1a) == 0x7fffu);
    assert(get32(record + 0x1e) == 0xffff8000u);
    template_result = 0;
    update_ui_record_from_template(4, record);
    assert(template_lookups == 2 && last_template_index == 4);
    assert(memory[record + 0x0e] == 0xcc);
    assert(get32(record + 0x16) == 0 && get32(record + 0x1a) == 0);
    assert(get32(record + 0x1e) == 0);
}

static void test_composite_and_table(void)
{
    g_ui_composite_arg_d = 0x12345678u;
    g_ui_composite_arg_b = 0xfedcba98u;
    g_ui_composite_arg_c = 0x431;
    update_ui_composite();
    assert(composites == 1 && last_record == 0x1fcc3e);
    assert(last_composite_d == 0x12345678u);
    assert(last_composite_b == 0xfedcba98u && last_composite_c == 0x431);

    put32(0x12d7b4, 0x30000);
    put32(0x12d7b8, 0x31000);
    put32(0x12d7bc, 0);
    put32(0x20000 + 0x3c, 0x31000);
    assert(ui_record_current_is_active(0x20000) == 1 && reports == 0);
    put32(0x20000 + 0x3c, 0x32000);
    assert(ui_record_current_is_active(0x20000) == 0);
    assert(reports == 1 && last_report == 0x18a3b8);
}

static void test_link(void)
{
    update_object_with_link(0x50000, 0x60000);
    assert(unlink_calls == 1 && install_calls == 1);
    assert(callback_sequence[0] == 'U' && callback_sequence[1] == 'I');
}

static void test_ui_fragments(void)
{
    ef_addr32 string = 0x40000;
    strcpy((char *)(memory + string), "prefix|suffix");
    compact_ui_fragment(string);
    assert(strcmp((const char *)(memory + string), "suffix") == 0);
    assert(fragment_calls == 1);
    compact_ui_fragment(0);
    assert(fragment_calls == 1);
    memory[string] = 0;
    compact_ui_fragment(string);
    assert(fragment_calls == 1);

    strcpy((char *)(memory + string), "alpha|bravo|charlie|");
    strcpy((char *)(memory + 0x41000), "bravo");
    assert(find_matching_ui_fragment(string, 0x41000) == string + 6);
    assert(length_calls == 1 && compare_calls == 2);
    strcpy((char *)(memory + 0x41000), "missing");
    assert(find_matching_ui_fragment(string, 0x41000) == 0);
    assert(length_calls == 2 && compare_calls == 5);
}

int main(void)
{
    test_packet();
    test_cross();
    test_template();
    test_composite_and_table();
    test_link();
    test_ui_fragments();
    return 0;
}
