/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_gui_cursor_097.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

enum { OUT_X = 0x22000, OUT_Y = 0x22002, OUT_BUTTON = 0x22004,
       SCREEN_X = 0x425fc8, SCREEN_Y = 0x425fc6,
       X_TIMER = 0x161e24, Y_TIMER = 0x161e28 };

static uint8_t ram[0x500000];
static uint32_t tick_now, poll_count;
static int32_t poll_x, poll_y;
static uint8_t poll_button;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return ram + address;
}

uint32_t guest_read_runtime_tick(void)
{
    return tick_now;
}

void gui_cursor_poll_input_097(void)
{
    ++poll_count;
    EF_U32(0x420dc4u) = (uint32_t)poll_x;
    EF_U32(0x420dc8u) = (uint32_t)poll_y;
    EF_U8(0x15ba2cu) = poll_button;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    tick_now = 100u;
    EF_U32(0x197c10u) = 98u;
    EF_U16(SCREEN_X) = 100u;
    EF_U16(SCREEN_Y) = 100u;
    EF_U32(X_TIMER) = 30u;
    EF_U32(Y_TIMER) = 30u;
    poll_count = 0;
    poll_x = poll_y = 0;
    poll_button = 0;
    EF_U16(OUT_X) = 0xfafau;
    EF_U16(OUT_Y) = 0xfbfb;
    EF_U16(OUT_BUTTON) = 0xfdfd;
}

static void step(void)
{
    gui_cursor_step_f3d54(OUT_X, OUT_Y, OUT_BUTTON);
    assert(poll_count == 1u);
    assert(EF_U32(0x197c10u) == tick_now);
    assert(EF_U32(0x197c14u) == tick_now);
    assert(EF_U16(OUT_X) == EF_U16(SCREEN_X));
    assert(EF_U16(OUT_Y) == EF_U16(SCREEN_Y));
}

static void test_step_thresholds(void)
{
    static const struct { int32_t motion; int16_t increment; } cases[] = {
        {0, 0}, {7, 0}, {80, 0}, {88, 1}, {160, 1},
        {168, 2}, {280, 2}, {288, 5},
        {-7, 0}, {-80, 0}, {-88, -1}, {-160, -1},
        {-168, -2}, {-280, -2}, {-288, -5}
    };
    unsigned i;
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        reset();
        poll_x = cases[i].motion;
        poll_y = -cases[i].motion;
        step();
        /* No small motion changes the coordinate until its timer expires. */
        if (cases[i].motion != 0 && cases[i].motion / 8 != 0 &&
            (cases[i].motion / 8 <= 10 && cases[i].motion / 8 >= -10)) {
            assert(EF_U16(SCREEN_X) == 100u);
            assert(EF_U32(X_TIMER) == 28u);
        } else {
            assert(EF_U16(SCREEN_X) == (uint16_t)(100 + cases[i].increment));
            assert(EF_U32(X_TIMER) == 30u);
        }
        if (poll_y / 8 != 0 && poll_y / 8 <= 10 && poll_y / 8 >= -10)
            assert(EF_U32(Y_TIMER) == 28u);
        else
            assert(EF_U32(Y_TIMER) == 30u);
    }
}

static void test_repeat_timer_and_backward_tick(void)
{
    reset();
    poll_x = 8;
    EF_U32(X_TIMER) = 5u;
    step();
    assert(EF_U16(SCREEN_X) == 100u);
    assert(EF_U32(X_TIMER) == 3u);

    poll_count = 0u;
    tick_now = 104u;
    step();
    assert(EF_U16(SCREEN_X) == 101u);
    assert(EF_U32(X_TIMER) == 30u);

    reset();
    poll_x = -8;
    EF_U32(X_TIMER) = 1u;
    tick_now = 30u; /* Guest time moved backwards: elapsed is clamped to 2. */
    step();
    assert(EF_U16(SCREEN_X) == 99u);
    assert(EF_U32(X_TIMER) == 30u);
}

static void test_clamping_and_button_state(void)
{
    reset();
    EF_U16(SCREEN_X) = 317u;
    EF_U16(SCREEN_Y) = 2u;
    poll_x = 288;
    poll_y = -288;
    poll_button = 0x80;
    step();
    assert(EF_U16(SCREEN_X) == 319u);
    assert(EF_U16(SCREEN_Y) == 0u);
    assert(EF_U16(OUT_BUTTON) == 1u);

    reset();
    EF_U16(SCREEN_X) = 1u;
    EF_U16(SCREEN_Y) = 197u;
    poll_x = -288;
    poll_y = 288;
    step();
    assert(EF_U16(SCREEN_X) == 0u);
    assert(EF_U16(SCREEN_Y) == 199u);
    assert(EF_U16(OUT_BUTTON) == 0u);
}

int main(void)
{
    test_step_thresholds();
    test_repeat_timer_and_backward_tick();
    test_clamping_and_button_state();
    return 0;
}
