/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <string.h>

uint8_t g_world_grid_marker;
ef_addr32 g_world_grid_base;

static uint8_t mem[0x1b0000];
static struct { uint8_t bytes[8]; uint32_t size; } seen[16];
static unsigned count;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(mem));
    return mem + address;
}

void emit_command_packet(const uint8_t *bytes, uint32_t size)
{
    assert(count < 16 && size <= 8);
    seen[count].size = size;
    memcpy(seen[count].bytes, bytes, size);
    ++count;
}

static void expect(unsigned slot, const uint8_t *bytes, uint32_t size)
{
    assert(seen[slot].size == size);
    assert(memcmp(seen[slot].bytes, bytes, size) == 0);
}

int main(void)
{
    const uint8_t p1d[1] = {0x1d}, p1f[1] = {0x1f};
    const uint8_t p26[2] = {0x26, 0x9c}, p1e[2] = {0x1e, 0x9c};
    const uint8_t p24[4] = {0x24, 0x01, 0x80, 0xe5};
    const uint8_t p1b[2] = {0x1b, 0x9c};

    g_world_grid_marker = 0x9c;
    send_command_1d();
    send_marker_packet_26();
    send_marker_packet_1e();
    send_command_1f();
    send_word_byte_packet_24((int16_t)0x8001, 0xe5);
    send_marker_packet_1b();
    expect(0, p1d, 1);
    expect(1, p26, 2);
    expect(2, p1e, 2);
    expect(3, p1f, 1);
    expect(4, p24, 4);
    expect(5, p1b, 2);

    /* commit_marker_selection: copies byte +1, packet 1b, latch. */
    mem[0x1000] = 0xaa;
    mem[0x1001] = 0x5d;
    commit_marker_selection(0x1000);
    assert(mem[0x12583b] == 0x5d && mem[0x1a5d1c] == 1);
    assert(count == 7);
    expect(6, p1b, 2);

    /* apply_marker_to_grid_row: matching marker uses a signed word index. */
    g_world_grid_base = 0x180000;
    mem[0x2000] = 0;
    mem[0x2001] = 0xff;
    mem[0x2002] = 0xff; /* index -1 */
    mem[0x2003] = 0x9c;
    apply_marker_to_grid_row(0x2000);
    assert(mem[0x180000 - 0xca] == 0x9c);
    mem[0x2001] = 3;
    mem[0x2002] = 0;
    apply_marker_to_grid_row(0x2000);
    assert(mem[0x180000 + 3 * 0xca] == 0x9c);
    /* non-matching marker leaves the row untouched */
    mem[0x2003] = 0x9d;
    mem[0x2001] = 5;
    apply_marker_to_grid_row(0x2000);
    assert(mem[0x180000 + 5 * 0xca] == 0);
    return 0;
}
