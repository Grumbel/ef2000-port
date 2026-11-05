/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
static unsigned calls;
static unsigned vertices_calls;
static unsigned update_calls;
static uint32_t source_seen;
static uint32_t count_seen;
static uint32_t draw_seen;
static uint32_t shade_seen;
static uint32_t depth_seen;
static uint32_t update_value_seen;
static uint16_t indices_seen[4];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return ram + address;
}

uint32_t renderer_stream_saved_esp(void)
{
    ++calls;
    return 0x3ffdd00u;
}

void load_vertices_c48dc(ef_addr32 src, uint32_t count)
{
    ++vertices_calls;
    source_seen = src;
    count_seen = count;
}

void update_fc9f8(int32_t value)
{
    ++update_calls;
    update_value_seen = (uint32_t)value;
}

static void observe_dispatch(uint32_t list, uint32_t count, uint32_t target)
{
    unsigned i;
    assert(list == 0x159190u);
    assert(count == count_seen && count <= 4u);
    for (i = 0; i < count; ++i)
        indices_seen[i] = EF_U16(list + i * 2u);
    assert(EF_U32(0x159064u) == 0x3ffdd00u);
    draw_seen = target;
    shade_seen = EF_U32(0x15907cu);
    depth_seen = EF_U32(0x159084u);
}

void build_render_object_d2584(ef_addr32 list, uint32_t count)
{
    observe_dispatch(list, count, 0xd2584u);
}

void build_textured_object_d2634(ef_addr32 list, uint32_t count)
{
    observe_dispatch(list, count, 0xd2634u);
}

void build_render_list_d2704(uint32_t list, uint32_t count)
{
    observe_dispatch(list, count, 0xd2704u);
}

void build_render_list_d2c84(uint32_t list, uint32_t count)
{
    observe_dispatch(list, count, 0xd2c84u);
}

void build_render_list_d2d7c(uint32_t list, uint32_t count)
{
    observe_dispatch(list, count, 0xd2d7cu);
}

void build_render_list_d2500(uint32_t list, uint32_t count)
{
    observe_dispatch(list, count, 0xd2500u);
}

static void reset(void)
{
    memset(ram, 0xa5, sizeof(ram));
    calls = vertices_calls = update_calls = 0;
    source_seen = count_seen = draw_seen = shade_seen = depth_seen = 0;
    update_value_seen = 0;
    EF_U32(0x164604u) = 0x120000u;
    EF_U32(0x159080u) = 0x75ab1234u;
    EF_U32(0x158f0cu) = 9;
}

static void check_common(uint32_t expected_target, uint32_t source,
                         uint32_t count, uint32_t depth, unsigned updates)
{
    assert(calls == 1 && vertices_calls == 1 && update_calls == updates);
    assert(source_seen == source && count_seen == count);
    assert(draw_seen == expected_target);
    assert(depth_seen == depth);
    assert(EF_U32(0x15918cu) == 0xffffffffu || expected_target == 0xd2500u);
    assert(EF_U8(0x158ed8u) == 0);
    assert(indices_seen[0] == 0 && indices_seen[1] == 0x30u);
    if (count > 2)
        assert(indices_seen[2] == 0x60u);
    if (updates)
        assert(update_value_seen == 0x75ab1234u);
}

static void test_colours(void)
{
    reset();
    EF_U16(0x100u) = 3;
    EF_U8(0x102u) = 0x11;
    EF_U8(0x103u) = 0x80;
    EF_U8(0x104u) = 0xff;
    render_colour_stream_c49c4(0x100u);
    check_common(0xd2584u, 0x105u, 3, 0x120000u, 0);
    assert(EF_U32(0x15977cu) == 0x1100u);
    assert(EF_U32(0x159780u) == 0x8000u);
    assert(EF_U32(0x159784u) == 0xff00u);
    assert(shade_seen == 4u && EF_U32(0x15907cu) == 0u);
}

static void test_textured(void)
{
    static const uint32_t target[] = {0xd2634u, 0xd2704u, 0xd2c84u, 0xd2d7cu};
    unsigned i;
    for (i = 0; i < 4; ++i) {
        reset();
        EF_U16(0x100u) = 2;
        EF_U16(0x102u) = 0x0102u;
        EF_U16(0x104u) = 0x1234u;
        EF_U16(0x106u) = 0xfffeu;
        EF_U16(0x108u) = 0x0030u;
        if (i == 0)
            render_texture_stream_c4a50(0x100u);
        else if (i == 1)
            render_texture_stream_c4aeb(0x100u);
        else if (i == 2)
            render_texture_stream_c4b95(0x100u);
        else
            render_texture_stream_c4c30(0x100u);
        check_common(target[i], 0x10au, 2, 0x120000u + (i == 1 ? 0x8000u : 0), 1);
        assert(EF_U16(0x15997cu) == 0x0400u);
        assert(EF_U16(0x15997eu) == 0x6800u);
        assert(EF_U16(0x159980u) == 0xfc00u);
        assert(EF_U16(0x159982u) == 0x6000u);
        assert(EF_U32(0x159984u) == EF_U32(0x15997cu));
        assert(shade_seen == 0u);
    }
}

static void test_indexed(void)
{
    reset();
    EF_U16(0x100u) = 0x1234u;
    EF_U16(0x102u) = 0x3au;
    EF_U16(0x104u) = 2;
    render_index_stream_c4ccb(0x100u);
    check_common(0xd2500u, 0x106u, 2, 0x120000u, 0);
    assert(EF_U32(0x15918cu) == 0x1234u);
    assert(EF_U8(0x159138u) == 0x3au);

    reset();
    EF_U16(0x100u) = 0x12f4u;
    EF_U16(0x102u) = 3;
    render_index_stream_c4d43(0x100u);
    check_common(0xd2500u, 0x104u, 3, 0x120000u, 0);
    assert(EF_U8(0x159138u) == 0xf4u);
    assert(EF_U32(0x15918cu) == 0xffffffffu);
}

int main(void)
{
    test_colours();
    test_textured();
    test_indexed();
    return 0;
}
