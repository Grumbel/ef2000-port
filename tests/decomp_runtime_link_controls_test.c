/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t guest[0x10000];
ef_addr32 g_link_pointer_table[32];
uint32_t g_runtime_control_word;
int16_t g_runtime_phase_word;
int16_t g_mode_limit_first;
int16_t g_mode_limit_second;
int16_t g_unit_counter;
uint32_t g_buffer_pointer_source;
uint32_t g_buffer_pointer_copy;
uint8_t g_mode_input_enabled_a;
uint8_t g_mode_input_enabled_b;
uint8_t g_mode_output_a;
uint8_t g_mode_output_b;
uint8_t g_mode_output_c;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(guest));
    return guest + address;
}

static void test_link_resolution(void)
{
    g_link_pointer_table[3] = 0x1110;
    g_link_pointer_table[5] = 0x2220;
    assert(resolve_link_pointer(3, 0) == 0x1110);
    assert(resolve_link_pointer(5, 0) == 0x2220);

    ef_addr32 alternate = 0x3456;
    memcpy(guest + 0x700 + 0x15, &alternate, sizeof(alternate));
    assert(resolve_link_pointer(3, 0x700) == 0x3456);
    alternate = 0;
    memcpy(guest + 0x700 + 0x15, &alternate, sizeof(alternate));
    assert(resolve_link_pointer(3, 0x700) == 0x1110);

    ef_link_reference reference;
    memset(&reference, 0xaa, sizeof(reference));
    reference.index = 5;
    reference.current = 0;
    memcpy(guest + 0x500, &reference, sizeof(reference));
    assert(refresh_link_pointer(0x500) == 0x2220);
    memcpy(&reference, guest + 0x500, sizeof(reference));
    assert(reference.current == 0x2220 && reference.index == 5);
    for (unsigned i = 0; i < 0x16; ++i) {
        assert(guest[0x500 + i] == 0xaa);
    }

    reference.current = 0x700;
    alternate = 0x9999;
    memcpy(guest + 0x700 + 0x15, &alternate, sizeof(alternate));
    memcpy(guest + 0x500, &reference, sizeof(reference));
    assert(refresh_link_pointer(0x500) == 0x9999);
    memcpy(&reference, guest + 0x500, sizeof(reference));
    assert(reference.current == 0x9999);
}

static void test_control_words(void)
{
    store_runtime_control_word(0xfedcba98u);
    assert(g_runtime_control_word == 0xfedcba98u);
    g_runtime_phase_word = -1;
    set_runtime_phase_five();
    assert(g_runtime_phase_word == 5);

    initialize_mode_limits();
    assert(g_mode_limit_first == 8 && g_mode_limit_second == 10);
    g_unit_counter = -123;
    reset_unit_counter();
    assert(g_unit_counter == 0);

    g_buffer_pointer_source = 0xfefedcba;
    g_buffer_pointer_copy = 1;
    assert(copy_active_buffer_pointer() == 0xfefedcba);
    assert(g_buffer_pointer_copy == 0xfefedcba);

    g_mode_input_enabled_a = 0;
    g_mode_input_enabled_b = 0;
    g_mode_output_a = 31;
    g_mode_output_b = 32;
    g_mode_output_c = 33;
    assert(set_control_mode_a(255) == 0);
    assert(set_control_mode_b(123) == 0);
    assert(set_control_mode_c(19) == 0);
    assert(g_mode_output_a == 0 && g_mode_output_b == 0 && g_mode_output_c == 0);

    g_mode_input_enabled_a = 1;
    g_mode_input_enabled_b = 2;
    assert(set_control_mode_a(0xd3) == 0xd3);
    assert(set_control_mode_b(0xff) == 0xff);
    assert(set_control_mode_c(0x81) == 0x81);
    assert(g_mode_output_a == 0xd3 && g_mode_output_b == 0xff && g_mode_output_c == 0x81);
    g_mode_input_enabled_b = 0;
    assert(set_control_mode_b(0xe4) == 0);
    assert(g_mode_output_c == 0x81); /* independent output latches */
}

int main(void)
{
    test_link_resolution();
    test_control_words();
    return 0;
}
