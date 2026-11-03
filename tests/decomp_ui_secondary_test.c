/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <string.h>

static uint8_t memory[0x230000];
uint32_t g_ui_reference_x, g_ui_reference_y;
uint16_t g_ui_composite_offset_x, g_ui_composite_offset_y;
uint16_t g_ui_component_enabled;
uint8_t g_ui_component_latch, g_tactical_action_enabled;
uint8_t g_ui_active_record_scan_latch;

static int32_t metric_result, last_delta_x, last_delta_y;
static unsigned metric_calls, prepare_calls, copy_calls, event_calls, recalc_calls;
static unsigned candidate_reads, candidate_converts, candidate_checks;
static unsigned unlink_prepares, unlink_applies, nav_calls;
static ef_addr32 candidate_source, converted_candidate, last_owner, last_candidate;
static ef_addr32 last_nav_object, last_nav_predicate;
static uint8_t candidate_valid;
static char unlink_sequence[4];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(memory));
    return memory + address;
}

static void put16(ef_addr32 addr, uint16_t value)
{
    memory[addr] = (uint8_t)value;
    memory[addr + 1u] = (uint8_t)(value >> 8);
}

static void put32(ef_addr32 addr, uint32_t value)
{
    put16(addr, (uint16_t)value);
    put16(addr + 2u, (uint16_t)(value >> 16));
}

static uint16_t get16(ef_addr32 addr)
{
    return (uint16_t)(memory[addr] | ((uint16_t)memory[addr + 1u] << 8));
}

int32_t guest_ui_distance_metric(int32_t dx, int32_t dy)
{
    last_delta_x = dx;
    last_delta_y = dy;
    ++metric_calls;
    return metric_result;
}

void guest_prepare_ui_vector(ef_addr32 base, uint32_t d, uint32_t b, uint32_t c)
{
    assert(base == 0x1fcc3e && d == 11 && b == 22 && c == 33);
    put16(base, 123);
    put16(base + 2, 456);
    g_ui_composite_offset_x = 11;
    g_ui_composite_offset_y = 17;
    ++prepare_calls;
}

void guest_copy_ui_component(ef_addr32 dest, ef_addr32 src, uint32_t length)
{
    assert(g_ui_component_enabled == 1);
    assert(dest == 0x1fcc2c && src == 0x1fcbb0 && length == 12);
    memcpy(memory + dest, memory + src, length);
    ++copy_calls;
}

void notify_tactical_action(int32_t index, uint32_t action)
{
    assert(index == -9 && action == 4);
    ++event_calls;
}

void guest_recalculate_object(ef_addr32 object)
{
    assert(object == 0x20000);
    assert(memory[object + 0x5a] == 1);
    assert(g_ui_active_record_scan_latch == 0);
    memory[object + 0x5a] = 0xee;
    ++recalc_calls;
}

ef_addr32 guest_read_candidate_record(void)
{
    ++candidate_reads;
    return candidate_source;
}

ef_addr32 guest_convert_candidate_record(ef_addr32 source)
{
    assert(source == candidate_source);
    ++candidate_converts;
    return converted_candidate;
}

uint8_t guest_validate_candidate_record(ef_addr32 owner, ef_addr32 candidate)
{
    last_owner = owner;
    last_candidate = candidate;
    ++candidate_checks;
    return candidate_valid;
}

void guest_object_prepare_unlink(ef_addr32 object, ef_addr32 link)
{
    assert(object == 0x12300 && link == 0x12400);
    unlink_sequence[unlink_prepares + unlink_applies] = 'P';
    ++unlink_prepares;
}

void guest_object_apply_unlink(ef_addr32 object, ef_addr32 link)
{
    assert(object == 0x12300 && link == 0x12400);
    unlink_sequence[unlink_prepares + unlink_applies] = 'A';
    ++unlink_applies;
}

void guest_prepare_navigation_component(ef_addr32 buffer, ef_addr32 predicate,
                                       ef_addr32 object)
{
    assert(buffer == 0x2143e4);
    last_nav_object = object;
    last_nav_predicate = predicate;
    put16(buffer, 0x0123);
    put16(buffer + 2u, 0x1234);
    ++nav_calls;
}

static void test_advance_fragment(void)
{
    const ef_addr32 p = 0x20000;
    strcpy((char *)(memory + p), "first\r\n\nsecond\nthird");
    assert(next_ui_fragment(p) == p + 8);
    assert(next_ui_fragment(p + 8) == p + 15);
    assert(next_ui_fragment(p + 15) == p + 20);
    assert(next_ui_fragment(p + 20) == p + 20);
    memory[p] = 0;
    assert(next_ui_fragment(p) == p);
}

static void test_metric(void)
{
    g_ui_reference_x = 50;
    g_ui_reference_y = 200;
    metric_result = 0x3b4c00;
    assert(ui_point_near_reference(55, 190) == 1);
    assert(last_delta_x == 5 && last_delta_y == -10);
    metric_result = 0x3b4c01;
    assert(ui_point_near_reference(55, 190) == 0 && metric_calls == 2);
}

static void test_composite_and_init(void)
{
    g_ui_composite_offset_x = 5;
    g_ui_composite_offset_y = 7;
    compose_ui_relative_vector(0x1fcc3e, 11, 22, 33);
    assert(prepare_calls == 1);
    assert(get16(0x1fcc3e) == 112 && get16(0x1fcc40) == 439);
    for (unsigned i = 0; i < 12; ++i) { memory[0x1fcbb0 + i] = (uint8_t)(i + 1); }
    g_ui_component_enabled = 0;
    g_ui_component_latch = 9;
    assert(initialize_ui_component() == 0);
    assert(copy_calls == 1 && g_ui_component_enabled == 1 && g_ui_component_latch == 0);
    assert(memcmp(memory + 0x1fcc2c, memory + 0x1fcbb0, 12) == 0);
}

static void test_object_gate(void)
{
    const ef_addr32 object = 0x20000;
    memory[object + 0x5a] = 0x43;
    put32(object + 0x2c, 0xfff70000u);
    g_ui_active_record_scan_latch = 1;
    g_tactical_action_enabled = 0;
    recalculate_object_temporarily(object);
    assert(event_calls == 0 && recalc_calls == 1);
    assert(memory[object + 0x5a] == 0x43 && g_ui_active_record_scan_latch == 0);
    g_tactical_action_enabled = 1;
    recalculate_object_temporarily(object);
    assert(event_calls == 1 && recalc_calls == 2);
    assert(memory[object + 0x5a] == 0x43);
}

static void test_candidate_and_unlink(void)
{
    candidate_source = 0;
    assert(select_validated_ui_record(0x12300) == 0);
    assert(candidate_reads == 1 && candidate_converts == 0 && candidate_checks == 0);
    candidate_source = 0x33000;
    converted_candidate = 0x34000;
    candidate_valid = 1;
    assert(select_validated_ui_record(0x12300) == 0x34000);
    assert(candidate_reads == 2 && candidate_converts == 1 && candidate_checks == 1);
    assert(last_owner == 0x12300 && last_candidate == 0x34000);
    candidate_valid = 0;
    assert(select_validated_ui_record(0x12300) == 0);
    assert(candidate_checks == 2);
    update_object_remove_link(0x12300, 0x12400);
    assert(unlink_prepares == 1 && unlink_applies == 1);
    assert(unlink_sequence[0] == 'P' && unlink_sequence[1] == 'A');
}

static void test_navigation_component(void)
{
    prepare_navigation_component(0, 0x12000);
    prepare_navigation_component(0x12300, 0);
    assert(nav_calls == 0);
    prepare_navigation_component(0x12300, 0x12400);
    assert(nav_calls == 1 && last_nav_object == 0x12300);
    assert(last_nav_predicate == 0x12400);
    assert(get16(0x2143e4) == 0xfadcu && get16(0x2143e6) == 0xedcbu);
}

int main(void)
{
    test_advance_fragment();
    test_metric();
    test_composite_and_init();
    test_object_gate();
    test_candidate_and_unlink();
    test_navigation_component();
    return 0;
}
