/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <string.h>

uint8_t g_reference_clock[6];
uint32_t g_tactical_accumulators[3];
uint16_t g_tactical_status_words[2];
uint8_t g_tactical_status_byte;
uint32_t g_tactical_runtime_count;
uint8_t g_tactical_slots[16];
uint8_t g_tactical_reset_mode;
uint8_t g_tactical_low_flag;
uint8_t g_tactical_status_entries[25];
uint8_t g_tactical_status_changed;
uint8_t g_external_grid_count;
uint8_t g_external_grid_marked_count;
ef_grid_external_probe g_grid_external_probes[256];
int16_t g_tactical_min_x, g_tactical_max_x, g_tactical_min_y, g_tactical_max_y;
ef_addr32 g_active_coordinate_source;
static uint8_t memory[4096];

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    assert(addr < sizeof(memory));
    return memory + addr;
}

static void test_clock(void)
{
    /* Each component is a signed little-endian word. */
    static const uint8_t ref[6] = {0xff,0xff, 0x02,0x00, 0x03,0x00};
    memcpy(g_reference_clock, ref, 6);
    memcpy(memory + 0x100, ref, 6);
    assert(reference_clock_reaches(0x100) == 1);
    memory[0x100] = 0xfe;
    assert(reference_clock_reaches(0x100) == 1);
    memory[0x100] = 0x00; memory[0x101] = 0x00;
    assert(reference_clock_reaches(0x100) == 0);
    memory[0x100] = 0xff; memory[0x101] = 0xff;
    memory[0x102] = 0x03;
    assert(reference_clock_reaches(0x100) == 0);
    memory[0x102] = 0x02; memory[0x104] = 0x04;
    assert(reference_clock_reaches(0x100) == 0);
}

static void test_tactical_reset(void)
{
    unsigned i;
    for (i = 0; i < 256; ++i) {
        assert(command_code_is_group_1b((uint8_t)i) ==
               (i >= 0x1b && i <= 0x27));
    }
    for (i = 0; i < 3; ++i) { g_tactical_accumulators[i] = 0xffffffffu; }
    g_tactical_status_words[0] = 0xffff;
    g_tactical_status_words[1] = 0xffff;
    g_tactical_status_byte = 0xcc;
    g_tactical_runtime_count = 123;
    clear_tactical_accumulators();
    for (i = 0; i < 3; ++i) { assert(g_tactical_accumulators[i] == 0); }
    assert(g_tactical_status_words[0] == 0 && g_tactical_status_words[1] == 0);
    assert(g_tactical_status_byte == 0 && g_tactical_runtime_count == 0);

    memset(g_tactical_slots, 0x11, sizeof(g_tactical_slots));
    g_tactical_reset_mode = 1;
    g_tactical_low_flag = 9;
    reset_tactical_slots();
    for (i = 0; i < 16; ++i) { assert(g_tactical_slots[i] == 0xff); }
    assert(g_tactical_low_flag == 9);
    g_tactical_reset_mode = 0;
    reset_tactical_slots();
    assert(g_tactical_low_flag == 0);

    memset(g_tactical_status_entries, 8, sizeof(g_tactical_status_entries));
    g_tactical_status_changed = 0x7f;
    advance_first_tactical_status();
    assert(g_tactical_status_entries[0] == 8);
    assert(g_tactical_status_entries[1] == 0x12);
    assert(g_tactical_status_entries[2] == 8 && g_tactical_status_changed == 0);
    memset(g_tactical_status_entries, 0, sizeof(g_tactical_status_entries));
    g_tactical_status_changed = 0xa5;
    advance_first_tactical_status();
    assert(g_tactical_status_changed == 0xa5);
}

static void test_external_and_bounds(void)
{
    g_external_grid_count = 0;
    g_external_grid_marked_count = 0x77;
    count_external_grid_markers();
    assert(g_external_grid_marked_count == 0);
    g_external_grid_count = 3;
    g_grid_external_probes[0].unknown_04 = 1;
    g_grid_external_probes[1].unknown_04 = 0;
    g_grid_external_probes[2].unknown_04 = 200;
    count_external_grid_markers();
    assert(g_external_grid_marked_count == 2);
    g_tactical_min_x = -5; g_tactical_max_x = 5;
    g_tactical_min_y = -10; g_tactical_max_y = 7;
    assert(position_within_tactical_bounds(-5, 7));
    assert(position_within_tactical_bounds(5, -10));
    assert(!position_within_tactical_bounds(-6, 0));
    assert(!position_within_tactical_bounds(0, 8));
}

static void test_coordinate_projection(void)
{
    uint32_t output[2];
    g_active_coordinate_source = 0;
    memset(memory + 0x200, 0xa5, 8);
    sample_active_coordinates(0x200);
    memcpy(output, memory + 0x200, 8);
    assert(output[0] == 0 && output[1] == 0);

    g_active_coordinate_source = 0x100;
    { uint32_t first = 3, second = 0xffffffffu;
      memcpy(memory + 0x108, &first, 4);
      memcpy(memory + 0x10c, &second, 4); }
    sample_active_coordinates(0x200);
    memcpy(output, memory + 0x200, 8);
    assert(output[0] == 3 * 2048u + 1024u);
    assert(output[1] == (0xffffffffu << 11) + 1024u);
}

int main(void)
{
    test_clock();
    test_tactical_reset();
    test_external_and_bounds();
    test_coordinate_projection();
    return 0;
}
