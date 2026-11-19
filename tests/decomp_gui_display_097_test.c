/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_gui_cursor_097.h"
#include "ef_gui_script_095.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
static uint32_t calls[128], call_args[128];
static unsigned call_count, pointer_count, hit_count, move_count;
static uint32_t pointer_status[8], hover_result;
static uint32_t mouse_pressed, screen_move;
static struct { uint32_t eax, edx, ebx, ecx, stack[4]; } actual_params;
static unsigned params_count;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return ram + address;
}

static void record_call(uint32_t target, uint32_t eax)
{
    assert(call_count < sizeof(calls) / sizeof(calls[0]));
    calls[call_count] = target;
    call_args[call_count] = eax;
    ++call_count;
}

void set_param_block_dd9e4(uint32_t eax, uint32_t edx, uint32_t ebx,
                           uint32_t ecx, uint32_t s0, uint32_t s1,
                           uint32_t s2, uint32_t s3)
{
    actual_params.eax = eax;
    actual_params.edx = edx;
    actual_params.ebx = ebx;
    actual_params.ecx = ecx;
    actual_params.stack[0] = s0;
    actual_params.stack[1] = s1;
    actual_params.stack[2] = s2;
    actual_params.stack[3] = s3;
    ++params_count;
}

#define VOID_CALL(name, target) void name(void) { record_call(target, 0u); }
VOID_CALL(apply_request_f41c4, 0xf41c4u)
VOID_CALL(present_status_frame_f34cc, 0xf34ccu)
VOID_CALL(update_remote_view_f2ac0, 0xf2ac0u)
VOID_CALL(step_animation_f36a4, 0xf36a4u)
VOID_CALL(process_command_dispatch_loop, 0xf3b78u)
VOID_CALL(refresh_f34a0, 0xf34a0u)
VOID_CALL(poll_mouse_ddae0, 0xddae0u)
VOID_CALL(wait_vsync_dc2e0, 0xdc2e0u)
VOID_CALL(run_shutdown_pair_dd8d0, 0xdd8d0u)
VOID_CALL(call_e9a24_c3834, 0xc3834u)
VOID_CALL(leave_scroll_mode_c39ac, 0xc39acu)
VOID_CALL(call_e9a24_c386c, 0xc386cu)
VOID_CALL(release_pending_handle_eae10, 0xeae10u)
VOID_CALL(toggle_page_dc250, 0xdc250u)
VOID_CALL(restore_saved_block_dd870, 0xdd870u)
#undef VOID_CALL

uint8_t get_byte_3efa0(void)
{
    record_call(0x3efa0u, 0u);
    return (uint8_t)(screen_move != 0u);
}

void fade_palette_out_de648(ef_addr32 palette)
{
    record_call(0xde648u, palette);
}

void draw_record_text_f3aa8(uint32_t index)
{
    record_call(0xf3aa8u, index);
}

void gui_display_pointer_097(uint32_t *buttons, uint32_t *x, uint32_t *y)
{
    assert(pointer_count < sizeof(pointer_status) / sizeof(pointer_status[0]));
    *buttons = pointer_status[pointer_count++];
    *x = 123u;
    *y = 67u;
}

uint32_t gui_display_hit_test_097(int32_t x, int32_t y)
{
    assert(x == (int16_t)EF_U16(0x425fc8u));
    assert(y == (int16_t)EF_U16(0x425fc6u));
    ++hit_count;
    return hover_result;
}

void gui_display_cursor_097(uint32_t *x, uint32_t *y, uint32_t *button)
{
    ++move_count;
    *x = 80u;
    *y = 90u;
    *button = mouse_pressed;
    EF_U16(0x425fc8u) = 80u;
    EF_U16(0x425fc6u) = 90u;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(calls, 0, sizeof(calls));
    memset(call_args, 0, sizeof(call_args));
    memset(pointer_status, 0, sizeof(pointer_status));
    memset(&actual_params, 0, sizeof(actual_params));
    call_count = pointer_count = hit_count = move_count = params_count = 0u;
    hover_result = mouse_pressed = screen_move = 0u;
    EF_U32(0x161e1cu) = 1u << 16; /* second 20-byte parameter record */
    EF_U16(0x425fc8u) = 101u;
    EF_U16(0x425fc6u) = 55u;
}

static unsigned count(uint32_t target)
{
    unsigned i, result = 0u;
    for (i = 0u; i < call_count; ++i)
        if (calls[i] == target) ++result;
    return result;
}

static void test_early_exit_and_parameter_packing(void)
{
    static const uint32_t sequence[] = {
        0xf41c4u, 0xf34ccu, 0xde648u, 0xdc250u, 0xdc250u, 0xdd870u
    };
    unsigned i;
    reset();
    EF_U8(0x161e22u) = 1u;
    EF_U8(0x161e20u) = 1u;
    {
        uint32_t base = 0x161d98u + 20u;
        EF_U32(base) = 0xabcdef01u;
        EF_U32(base + 4u) = 0x98765432u;
        EF_U16(base + 8u) = 0xfffeu;
        EF_U16(base + 10u) = 10u;
        EF_U16(base + 12u) = 0xffffu;
        EF_U16(base + 14u) = 20u;
        EF_U16(base + 16u) = 0x8000u;
        EF_U16(base + 18u) = 0x7fffu;
    }
    gui_options_display_f3f84();
    assert(params_count == 1u);
    assert(actual_params.eax == 0xabcdef01u);
    assert(actual_params.edx == 0x98765432u);
    assert(actual_params.ebx == 0xfffffffeu);
    assert(actual_params.ecx == 10u);
    assert(actual_params.stack[0] == 0xffffffffu);
    assert(actual_params.stack[1] == 20u);
    assert(actual_params.stack[2] == 0xffff8000u);
    assert(actual_params.stack[3] == 0x7fffu);
    assert(pointer_count == 0u);
    assert(call_count == sizeof(sequence) / sizeof(sequence[0]));
    for (i = 0u; i < call_count; ++i)
        assert(calls[i] == sequence[i]);
    assert(call_args[2] == 0x425c8cu);
}

static void test_click_dispatch_and_release(void)
{
    unsigned i;
    reset();
    pointer_status[0] = 1u;
    pointer_status[1] = 0u;
    hover_result = 4u;
    EF_U16(0x161e18u) = 1u;
    gui_options_display_f3f84();
    assert(pointer_count == 2u);
    assert(hit_count == 1u); /* Selection freezes hover queries. */
    assert(count(0xf36a4u) == 2u);
    assert(count(0xf3aa8u) == 1u);
    assert(count(0xf3b78u) == 1u);
    assert(count(0xf2ac0u) == 2u);
    assert(count(0x3efa0u) == 2u);
    assert(count(0xde648u) == 1u);
    assert(EF_U16(0x1616eeu) == 101u);
    assert(EF_U16(0x1616f0u) == 55u);
    for (i = 0u; i < call_count; ++i)
        if (calls[i] == 0xf3aa8u) assert(call_args[i] == 3u);

    reset();
    pointer_status[0] = 1u;
    pointer_status[1] = 0u;
    hover_result = 0xffffu;
    gui_options_display_f3f84();
    assert(count(0xf3aa8u) == 1u);
    for (i = 0u; i < call_count; ++i)
        if (calls[i] == 0xf3aa8u) assert(call_args[i] == 0xfffffffeu);
}

static void test_nonfade_cursor_and_refresh_paths(void)
{
    reset();
    EF_U8(0x161e20u) = 1u;
    EF_U8(0x4859dcu) = 1u; /* nofade */
    EF_U8(0x4859deu) = 1u; /* refreshed background */
    gui_options_display_f3f84();
    assert(count(0xc3834u) == 1u);
    assert(count(0xc39acu) == 0u);
    assert(count(0xc386cu) == 1u);
    assert(count(0xeae10u) == 1u);

    reset();
    /* A pointer-free display still polls once, then the test adapter flips
     * the exit flag on an event-driven callback below. */
    EF_U8(0x4859dcu) = 1u;
    screen_move = 1u;
    pointer_status[0] = 1u;
    hover_result = 1u;
    gui_options_display_f3f84();
    assert(move_count >= 1u);
    assert(count(0xc39acu) == 1u);
    assert(count(0xeae10u) == 0u);
    assert(EF_U16(0x1616eeu) == 80u);
    assert(EF_U16(0x1616f0u) == 90u);
}

int main(void)
{
    test_early_exit_and_parameter_packing();
    test_click_dispatch_and_release();
    test_nonfade_cursor_and_refresh_paths();
    return 0;
}
