/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"

#include <assert.h>
#include <string.h>

static uint8_t mem[0x230000];
static unsigned invoked, refreshed, queried;
static ef_addr32 last_callback;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(mem));
    return mem + address;
}

static void put16(ef_addr32 a, uint16_t n)
{
    mem[a] = (uint8_t)n;
    mem[a + 1u] = (uint8_t)(n >> 8);
}

static void put32(ef_addr32 a, uint32_t n)
{
    put16(a, (uint16_t)n);
    put16(a + 2u, (uint16_t)(n >> 16));
}

static uint16_t get16(ef_addr32 a)
{
    return (uint16_t)(mem[a] | (uint16_t)(mem[a + 1u] << 8));
}

void guest_invoke_runtime_slot(ef_addr32 callback)
{
    ++invoked;
    last_callback = callback;
    assert(mem[0x122882] == 1);
    assert(get16(0x122880) == (invoked == 1 ? 3 : 53));
}

void guest_refresh_runtime_slot(int16_t index)
{
    assert(index == (int16_t)refreshed);
    ++refreshed;
}

void guest_query_runtime_pair(uint32_t *first, uint32_t *second)
{
    assert(get16(0x121ed0) == 0);
    *first = 1234;
    *second = 5678;
    ++queried;
}

int main(void)
{
    memset(mem, 0, sizeof(mem));
    put32(0x120cc6, 0x20000);
    assert(encode_runtime_segment_offset(0x2012f) == 0x0012000fu);
    assert(encode_runtime_segment_offset(0x1ffff) == 0xffff000fu);

    for (unsigned i = 0; i < 54; ++i) { mem[0x121fb4 + i * 29] = 1; }
    put32(0x121fb4 + 3 * 29 + 25, 0x1c0000);
    put32(0x121fb4 + 53 * 29 + 25, 0x1c1000);
    reinitialize_runtime_slots();
    assert(invoked == 2 && refreshed == 54 && last_callback == 0x1c1000);
    assert(mem[0x122882] == 0 && get16(0x12288a) == 0);
    assert(mem[0x121fb4 + 0 * 29] == 0);
    assert(mem[0x121fb4 + 3 * 29] == 1);
    assert(mem[0x121fb4 + 53 * 29] == 1);

    put32(0x121ec6 + 2 * 2, 0x00030000);
    mem[0x121fb4 + 5 * 29] = 0xa7;
    assert(read_runtime_slot_flag(2, 2) == 0xa7);
    assert(read_runtime_slot_flag(1, 2) == 0);

    initialize_runtime_probe_latch();
    assert(queried == 1 && get16(0x121ed0) == 0x96);
    return 0;
}
