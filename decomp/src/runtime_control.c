/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

/* 0x3cd64, 0x06 bytes, six caller functions. Full EAX store. */
void store_runtime_control_word(uint32_t value)
{
    g_runtime_control_word = value;
}

/* 0x3dcc0, 0x0a bytes, two caller functions. */
void set_runtime_phase_five(void)
{
    g_runtime_phase_word = 5;
}

/* 0x58d24, 0x1d bytes, two caller functions. Both limits are words. */
void initialize_mode_limits(void)
{
    g_mode_limit_first = 8;
    g_mode_limit_second = 10;
}

/* 0x5e658, 0x0c bytes, three caller functions. */
void reset_unit_counter(void)
{
    g_unit_counter = 0;
}

/* 0x7a428, 0x0b bytes, two caller functions. EAX is the copied value. */
uint32_t copy_active_buffer_pointer(void)
{
    g_buffer_pointer_copy = g_buffer_pointer_source;
    return g_buffer_pointer_source;
}

/* 0x7eea0, 0x19 bytes, two caller functions. With input disabled, the
 * original starts DL at zero and disregards AL. It returns cleared-high
 * EAX, and also stores that byte to a separate output latch. */
uint8_t set_control_mode_a(uint8_t value)
{
    uint8_t selected = (g_mode_input_enabled_a != 0) ? value : 0;
    g_mode_output_a = selected;
    return selected;
}

/* 0x810d0 and 0x810f4, each 0x19 bytes, two independent callers each.
 * Both depend on the same mode gate but store to different guest bytes. */
uint8_t set_control_mode_b(uint8_t value)
{
    uint8_t selected = (g_mode_input_enabled_b != 0) ? value : 0;
    g_mode_output_b = selected;
    return selected;
}

uint8_t set_control_mode_c(uint8_t value)
{
    uint8_t selected = (g_mode_input_enabled_b != 0) ? value : 0;
    g_mode_output_c = selected;
    return selected;
}
