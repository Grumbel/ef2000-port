/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x200000];
static unsigned draw_calls;
static unsigned callback_calls;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return &ram[address];
}

void run_handler_stack_c425a(void)
{
    ++draw_calls;
}

void renderer_frame_callback_c4167(ef_addr32 callback)
{
    assert(callback == 0x12340u);
    assert(draw_calls == callback_calls + 1u);
    ++callback_calls;
}

static void set_pair(uint32_t index, int16_t value, uint16_t tag)
{
    EF_U16(0x146e54u + 2u * index) = (uint16_t)value;
    EF_U16(0x147e54u + 2u * index) = tag;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    set_pair(0, -32768, 0xdddd);
    EF_U32(0xfc8a8u) = 0x12340u;
    EF_U8(0xc41c4u) = 0x1bu;
    draw_calls = callback_calls = 0u;
}

static void test_sort(void)
{
    static const int16_t values[] = { 65, 12, 5, 13, 12, -8, 100, 10,
                                      20, 22, 21, 18, 17, 1, 50, -10, 8 };
    uint16_t seen[17] = { 0 };
    uint32_t i;
    reset();
    for (i = 0; i < 17u; ++i)
        set_pair(i + 1, values[i], (uint16_t)(i + 1));
    EF_U16(0x146e50u) = 17;
    renderer_partial_quicksort_c3fd4(1, 17);
    renderer_insertion_sort_c409c();
    for (i = 1; i <= 17u; ++i) {
        uint16_t tag = EF_U16(0x147e54u + i * 2u);
        assert(tag >= 1u && tag <= 17u);
        assert(!seen[tag - 1u]++);
        assert(EF_S16(0x146e54u + 2u * i) == values[tag - 1u]);
        if (i > 1)
            assert(EF_S16(0x146e54u + 2u * (i - 1u)) <= EF_S16(0x146e54u + 2u * i));
    }
    assert(EF_U16(0x147e54u) == 0xdddd);
}

static void test_finish_and_enqueue(void)
{
    reset();
    /* The byte at c41c4 is the original JGE displacement, not a Boolean. */
    renderer_enqueue_c41b0(300);
    renderer_enqueue_c41b0(100);
    renderer_enqueue_c41b0(200);
    assert(EF_U16(0x146e50u) == 3);
    assert(EF_U16(0x146e52u) == 3);
    assert(EF_U16(0x146e56u) == 300);
    assert(EF_U16(0x146e58u) == 100);
    assert(EF_U16(0x146e5au) == 200);
    renderer_finish_queue_c4167();
    assert(draw_calls == 1u && callback_calls == 1u);
    assert(EF_U8(0xc41c4u) == 0x6eu);
    assert(EF_S16(0x146e56u) == 100);
    assert(EF_S16(0x146e58u) == 200);
    assert(EF_S16(0x146e5au) == 300);
    renderer_finish_queue_c4167();
    assert(draw_calls == 2u && callback_calls == 2u);
}

static void test_both_overflow_paths(void)
{
    uint32_t i;
    reset();
    for (i = 1; i <= 0x7ffu; ++i)
        set_pair(i, (int16_t)(i + 100u), (uint16_t)(i - 1u));
    EF_U16(0x146e50u) = 0x7ffu;
    EF_U16(0x146e52u) = 0x7ffu;
    renderer_enqueue_c41b0(150u);
    assert(EF_U16(0x146e50u) == 0x800u);
    assert(EF_U8(0xc41c4u) == 0x6eu);
    for (i = 2; i <= 0x800u; ++i)
        assert(EF_S16(0x146e54u + 2u * (i - 1u)) <=
               EF_S16(0x146e54u + 2u * i));
    /* Already sorted flag selects the alternate c4233 overflow branch. */
    renderer_enqueue_c41b0(175u);
    assert(EF_U16(0x146e50u) == 0x800u);
    assert(EF_U8(0xc41c4u) == 0x6eu);
    for (i = 2; i <= 0x800u; ++i)
        assert(EF_S16(0x146e54u + 2u * (i - 1u)) <=
               EF_S16(0x146e54u + 2u * i));
}

static void test_endian(void)
{
    ef_addr32 cursor;
    reset();
    cursor = 0x200u;
    EF_U16(cursor) = 0x1234;
    assert(renderer_swap_word_c5384(&cursor) == 0x3412);
    assert(cursor == 0x202u && EF_U16(0x200u) == 0x3412u);
    EF_U16(cursor) = 0xaabb;
    assert(renderer_write_special_word_c5369(&cursor) == 0xbfu);
    assert(EF_U16(0x202u) == 0xbfu && cursor == 0x204u);
    EF_U16(0x15af90u) = 1960;
    EF_U16(cursor) = 0x0200u; /* decoded: 2; (2+2000-1960)*48 */
    assert(renderer_index_biased_c53b4(&cursor) == 2016u);
    assert(EF_U16(0x204u) == 2016u && cursor == 0x206u);
    EF_U16(cursor) = 0x0300u;
    assert(renderer_index_absolute_c53cf(&cursor) == 144u);
    assert(cursor == 0x208u);
    EF_U16(cursor) = 0xffffu;
    assert(renderer_index_absolute_c53cf(&cursor) == (uint16_t)(0xffffu * 48u));
}

int main(void)
{
    test_sort();
    test_finish_and_enqueue();
    test_both_overflow_paths();
    test_endian();
    return 0;
}
