/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

/* 0x3cd64, 0x06 bytes, verified: differential+replay. Full EAX store. */
#if defined(__WATCOMC__)
static void store_runtime_control_word_direct(void);
#pragma aux store_runtime_control_word_direct = \
    0xa3 0xbc 0x35 0x1a 0x00 __modify []
#endif
void store_runtime_control_word(uint32_t value)
{
#if defined(__WATCOMC__)
    (void)value;
    store_runtime_control_word_direct();
#else
    g_runtime_control_word = value;
#endif
}

/* 0x3dcc0, 0x0a bytes, verified: differential+replay. */
#if defined(__WATCOMC__)
static void write_runtime_phase_five(void);
#pragma aux write_runtime_phase_five = \
    0x66 0xc7 0x05 0xec 0x35 0x1a 0x00 0x05 0x00 __modify []
#endif
void set_runtime_phase_five(void)
{
#if defined(__WATCOMC__)
    write_runtime_phase_five();
#else
    g_runtime_phase_word = 5;
#endif
}

/* 0x58d24, 0x1d bytes, verified: differential+replay. Both limits are words. */
#if defined(__WATCOMC__)
#pragma aux mode_limit_timing_nop = 0x90 __modify []
static void mode_limit_timing_nop(void);
#pragma aux write_first_mode_limit = \
    0x66 0xc7 0x05 0xe2 0x6c 0x1a 0x00 0x08 0x00 __modify []
static void write_first_mode_limit(void);
#pragma aux write_second_mode_limit = \
    0x66 0xc7 0x05 0xe8 0x6c 0x1a 0x00 0x0a 0x00 __modify []
static void write_second_mode_limit(void);
#endif
void initialize_mode_limits(void)
{
#if defined(__WATCOMC__)
    mode_limit_timing_nop();
    mode_limit_timing_nop();
    mode_limit_timing_nop();
    mode_limit_timing_nop();
    write_first_mode_limit();
    write_second_mode_limit();
    mode_limit_timing_nop();
    mode_limit_timing_nop();
#else
    g_mode_limit_first = 8;
    g_mode_limit_second = 10;
#endif
}

/* 0x5e658, 0x0c bytes, verified: captured call+replay. Four call sites. */
#if defined(__WATCOMC__)
static void store_unit_counter_zero(void);
#pragma aux store_unit_counter_zero = \
    0x66 0xc7 0x05 0x4e 0x81 0x12 0x00 0x00 0x00 __modify []
#endif
void reset_unit_counter(void)
{
#if defined(__WATCOMC__)
    store_unit_counter_zero();
#else
    g_unit_counter = 0;
#endif
}

/* 0x7a428, 0x0b bytes, verified: differential+replay. EAX is the copied value. */
#if defined(__WATCOMC__)
static uint32_t copy_active_buffer_pointer_direct(void);
#pragma aux copy_active_buffer_pointer_direct = \
    0xa1 0xc4 0xaf 0x12 0x00 0xa3 0x9c 0x8e 0x1f 0x00 \
    __value [eax] __modify [eax]
#endif
uint32_t copy_active_buffer_pointer(void)
{
#if defined(__WATCOMC__)
    return copy_active_buffer_pointer_direct();
#else
    g_buffer_pointer_copy = g_buffer_pointer_source;
    return g_buffer_pointer_source;
#endif
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
