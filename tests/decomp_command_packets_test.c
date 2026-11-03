/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

uint8_t g_world_grid_marker;

typedef struct observed_packet {
    uint8_t bytes[8];
    uint32_t size;
} observed_packet;

static observed_packet observed[12];
static unsigned count;

void emit_command_packet(const uint8_t *bytes, uint32_t size)
{
    assert(count < 12);
    assert(size <= sizeof(observed[0].bytes));
    observed[count].size = size;
    memcpy(observed[count].bytes, bytes, size);
    ++count;
}

static void assert_packet(unsigned slot, const uint8_t *expected, uint32_t size)
{
    assert(observed[slot].size == size);
    assert(memcmp(observed[slot].bytes, expected, size) == 0);
}

int main(void)
{
    const uint8_t first[4] = {0x32, 0x34, 0x12, 0xe7};
    const uint8_t neg[4] = {0x32, 0xff, 0xff, 0x00};
    const uint8_t c39[3] = {0x39, 0xfe, 0xff};
    const uint8_t c3a[3] = {0x3a, 0x00, 0x80};
    const uint8_t c4a[3] = {0x4a, 0x02, 0x01};
    const uint8_t c4b[3] = {0x4b, 0x02, 0x01};
    const uint8_t c4c[3] = {0x4c, 0x02, 0x01};
    const uint8_t grid5[5] = {0x0c, 0xff, 0xfe, 0xa9, 0xd4};
    const uint8_t grid4[4] = {0x16, 0x20, 0x10, 0x7e};

    send_object_state_packet(0x1234, 0xe7);
    send_object_state_packet(-1, 0);
    send_object_code_39(-2);
    send_object_code_3a((int16_t)0x8000);
    send_object_code_4a(0x102);
    send_object_code_4b(0x102);
    send_object_code_4c(0x102);
    g_world_grid_marker = 0xa9;
    send_grid_marker_packet(-257, 0xd4);
    g_world_grid_marker = 0x7e;
    send_grid_marker_short_packet(0x1020);

    assert(count == 9);
    assert_packet(0, first, sizeof(first));
    assert_packet(1, neg, sizeof(neg));
    assert_packet(2, c39, sizeof(c39));
    assert_packet(3, c3a, sizeof(c3a));
    assert_packet(4, c4a, sizeof(c4a));
    assert_packet(5, c4b, sizeof(c4b));
    assert_packet(6, c4c, sizeof(c4c));
    assert_packet(7, grid5, sizeof(grid5));
    assert_packet(8, grid4, sizeof(grid4));
    return 0;
}
