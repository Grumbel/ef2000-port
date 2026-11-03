/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <stdint.h>

static uint8_t guest[0x10000];
ef_addr32 g_buffer_start;
ef_addr32 g_buffer_cursor;
int32_t g_plane_selected_mode;
ef_plane_source g_plane_source_table[3];
ef_addr32 g_active_plane_high;
ef_addr32 g_active_plane_low;
ef_addr32 g_active_plane_aux;
ef_addr32 g_indexed_object_table[256];
int16_t g_indexed_object_count;
uint8_t g_scene_refresh_pending;

static unsigned capacity_warnings;
static ef_addr32 last_message;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(guest));
    return guest + address;
}

void report_buffer_capacity_exceeded(ef_addr32 message)
{
    ++capacity_warnings;
    last_message = message;
}

int main(void)
{
    g_buffer_start = 0x1000;
    g_buffer_cursor = 0x1000 + 0x4000;
    check_buffer_capacity();
    assert(capacity_warnings == 0);
    buffer_write_word((int16_t)0x8001);
    assert(g_buffer_cursor == 0x5002 && capacity_warnings == 1);
    assert(last_message == 0x187b8c);
    assert(*(uint16_t *)(guest + 0x5000) == 0x8001);

    g_buffer_start = 0x200;
    g_buffer_cursor = 0x200;
    buffer_write_word(-17);
    assert(g_buffer_cursor == 0x202);
    g_buffer_cursor = 0x200;
    assert(buffer_read_word() == -17);
    assert(g_buffer_cursor == 0x202 && capacity_warnings == 1);

    g_plane_selected_mode = -1;
    g_plane_source_table[0].high_plane = 0x2000;
    g_plane_source_table[0].low_plane = 0x4000;
    g_plane_source_table[0].auxiliary = 0x6000;
    g_plane_source_table[1].high_plane = 0x2500;
    g_plane_source_table[1].low_plane = 0x4500;
    g_plane_source_table[1].auxiliary = 0x6500;
    assert(select_plane_source(0) == 0x2000);
    assert(g_active_plane_high == 0x2000 && g_active_plane_low == 0x4000);
    assert(g_active_plane_aux == 0x6000 && g_plane_selected_mode == 0);
    assert(select_plane_source(0) == 0);
    assert(select_plane_source(1) == 0x2500);
    assert(select_plane_source(1) == 1);

    guest[0x2500 + 2 * 400 + (399 - 5)] = 0xab;
    guest[0x4500 + 2 * 400 + (399 - 5)] = 0xcd;
    assert(read_plane_cell(1, 2, 5) == 0xabcd);
    guest[0x2000 + 399] = 0x12;
    guest[0x4000 + 399] = 0x34;
    assert(read_plane_cell(0, 0, 0) == 0x1234);

    ef_scene_node *node = (ef_scene_node *)(guest + 0x700);
    node->index_packed = 2 << 16;
    g_indexed_object_count = 4;
    g_indexed_object_table[2] = 0x700;
    g_indexed_object_table[3] = 0x880;
    assert(indexed_object_is_canonical(0) == 0);
    assert(indexed_object_is_canonical(0x700) == 1);
    assert(indexed_object_direct_slot(2) == 0x700);
    assert(indexed_object_direct_slot(3) == 0x880);
    g_indexed_object_table[2] = 0x900;
    assert(indexed_object_is_canonical(0x700) == 0);

    assert(g_scene_refresh_pending == 0);
    request_scene_refresh();
    assert(g_scene_refresh_pending == 1);
    return 0;
}
