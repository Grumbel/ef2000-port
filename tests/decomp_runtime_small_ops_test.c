/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

static uint8_t mem[0x230000];
static uint32_t tick_next;
static unsigned tick_reads;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    assert(addr < sizeof(mem));
    return mem + addr;
}

uint32_t guest_read_runtime_tick(void)
{
    ++tick_reads;
    return tick_next;
}

static uint16_t get16(ef_addr32 addr)
{
    return (uint16_t)(mem[addr] | ((uint16_t)mem[addr + 1] << 8));
}

static uint32_t get32(ef_addr32 addr)
{
    return (uint32_t)get16(addr) | ((uint32_t)get16(addr + 2) << 16);
}

static void put16(ef_addr32 addr, uint16_t value)
{
    mem[addr] = (uint8_t)value;
    mem[addr + 1] = (uint8_t)(value >> 8);
}

static void put32(ef_addr32 addr, uint32_t value)
{
    put16(addr, (uint16_t)value);
    put16(addr + 2, (uint16_t)(value >> 16));
}

static void reset(void)
{
    memset(mem, 0, sizeof(mem));
    tick_reads = 0;
}

static void test_input_copy(void)
{
    reset();
    put16(0x197696, 0x8123);
    put16(0x19bb5e, 0xfffc);
    mem[0x121e8c] = 0x7d;
    copy_runtime_input_state(0x1d0000, 0x1d0002, 0x1d0004);
    assert(get16(0x1d0000) == 0x8123);
    assert(get16(0x1d0002) == 0xfffc);
    assert(mem[0x1d0004] == 0x7d);
}

static void test_ticks(void)
{
    reset();
    tick_next = 0x12345678;
    assert(sample_runtime_tick() == tick_next);
    assert(tick_reads == 1 && get32(0x197c14) == tick_next);

    put32(0x197c10, 0x12345676);
    assert(advance_runtime_tick() == 2);
    assert(tick_reads == 2 && get32(0x197c10) == tick_next);
    tick_next += 17;
    assert(advance_runtime_tick() == 17);
    tick_next -= 25;
    assert(advance_runtime_tick() == 2);
    put32(0x197c10, UINT_MAX);
    tick_next = 3;
    assert(advance_runtime_tick() == 4);
}

static void test_position_pack(void)
{
    reset();
    mem[0x121ec6] = 0xaa;
    pack_runtime_position((int16_t)-7, 3072, -1, 0xe3);
    assert(get16(0x197c28) == (uint16_t)-7);
    assert(get16(0x197c2a) == 1);
    assert(get16(0x197c2c) == (uint16_t)-1);
    assert(get16(0x197c24) == 0);
    assert(get16(0x197c26) == 1023);
    assert(mem[0x121ec6] == 0 && mem[0x197c2e] == 0xe3);

    pack_runtime_position(55, INT_MIN, INT_MAX, 0);
    assert(get16(0x197c2a) == (uint16_t)(INT_MIN / 2048));
    assert(get16(0x197c2c) == (uint16_t)(INT_MAX / 2048));
    assert(get16(0x197c24) == (uint16_t)-1024);
    assert(get16(0x197c26) == (uint16_t)1023);
}

static void test_pair_transform(void)
{
    for (uint16_t mode = 0; mode <= 5; ++mode) {
        reset();
        put16(0x1d0000, 7);
        put16(0x1d0002, 11);
        transform_runtime_word_pair(mode, 0x1d0000, 0x1d0002);
        uint16_t result_a = get16(0x1d0000), result_b = get16(0x1d0002);
        switch (mode) {
        case 1: assert(result_a == (uint16_t)-11 && result_b == 7); break;
        case 2: assert(result_a == (uint16_t)-7 && result_b == (uint16_t)-11); break;
        case 3: assert(result_a == 11 && result_b == (uint16_t)-7); break;
        default: assert(result_a == 7 && result_b == 11); break;
        }
    }
}

int main(void)
{
    test_input_copy();
    test_ticks();
    test_position_pack();
    test_pair_transform();
    return 0;
}
