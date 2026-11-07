/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Tiny wrappers, tail jumps and leaf helpers (each under 25 bytes). */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0xe764, 15 bytes. Format a string through the allocator log helper. */
void log_alloc_e764(uint32_t a)
{
    guest_fn_cc6c0_pair(a, 0x00180bfcu);
}

/* 0x35218, 8 bytes. Constant 0x44. */
uint32_t constant_44_35218(void)
{
    return 0x44;
}

/* 0x39300, 12 bytes. Store the result of 0x9ddfc as a word. */
void store_word_39300(void)
{
    EF_U16(0x001a17b2u) = (uint16_t)guest_fn_9ddfc_w();
}

/* 0x52eb0, 13 bytes. Scale a float to double. */
double scale_float_52eb0(float value)
{
    return (double)value * EF_F64(0x00185b24u);
}

/* 0x56c7c, 20 bytes. Run 0x56c90 with mode 1 and set the flag word. */
void run_mode_one_56c7c(uint32_t a)
{
    guest_fn_56c90_m(a, 1);
    EF_U16(0x00126518u) = 1;
}

/* 0x7a290, 8 bytes. Low byte of 0x7a194's result. */
uint8_t low_byte_7a290(void)
{
    return (uint8_t)guest_fn_7a194_r();
}

/* 0x7a38c, 24 bytes. Register the 0x7a2b0 handler and remember 0x7a338. */
void register_handler_7a38c(void)
{
    guest_fn_e7fa4_h(0x007a2b0u, 0x007a338u);
    EF_U32(0x00132af0u) = 0x007a338u;
}

/* 0x9d778, 4 bytes. Return zero. */
uint8_t return_zero_9d778(void)
{
    return 0;
}

/* 0x9d77c, 1 byte. No-op. */
void nop_9d77c(void)
{
}

/* 0x9dddc, 1 byte. No-op. */
void nop_9dddc(void)
{
}

/* 0xa4104, 8 bytes. Poll the device. */
void poll_device_a4104(void)
{
    guest_fn_a3fe0_p();
}

/* 0xa75b4, 23 bytes. Look up a widget and, when found, apply a value to it. */
ef_addr32 apply_to_widget_a75b4(ef_addr32 root, ef_addr32 name, uint32_t value)
{
    ef_addr32 widget = find_widget_checked_a78bc(root, name);

    if (widget != 0)
        guest_fn_a7588_v(widget, value);
    return widget;
}

/* 0xa7ab4, 18 bytes. Seek a handle to its start. */
uint32_t seek_start_a7ab4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a, uint32_t stack_b)
{
    (void)u0;
    (void)u1;
    (void)u2;
    (void)u3;
    return guest_fn_a7a64_r(stack_b, 0, stack_a);
}

/* 0xa7ac8, 21 bytes. Seek a handle relative to the current position. */
uint32_t seek_current_a7ac8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a, uint32_t stack_b)
{
    (void)u0;
    (void)u1;
    (void)u2;
    (void)u3;
    return guest_fn_a7a64_r(stack_b, 1, stack_a);
}

/* 0xa7ae0, 21 bytes. Seek a handle to its end (returns the size). */
uint32_t seek_end_a7ae0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a, uint32_t stack_b)
{
    (void)u0;
    (void)u1;
    (void)u2;
    (void)u3;
    return guest_fn_a7a64_r(stack_b, 2, stack_a);
}

/* 0xabaf0, 13 bytes. Build the default path into a buffer. */
void build_default_path_abaf0(ef_addr32 buffer)
{
    build_path_9c9c0(buffer, 0x0018da80u);
}

/* 0xb3fb0, 24 bytes. Fill bytes of a block with one value. */
void fill_bytes_b3fb0(ef_addr32 dst, uint8_t value, uint32_t count)
{
#if defined(__WATCOMC__)
    /* The original entry preserves EAX around 0xdeb50. */
    __asm { push eax }
#endif
    guest_fn_deb50_f(dst, (uint32_t)value * 0x01010101u, count);
#if defined(__WATCOMC__)
    __asm { pop eax }
#endif
}

/* 0xb3fd0, 9 bytes. Square of a double. */
double square_double_b3fd0(double x)
{
    return x * x;
}

/* 0xb3fdc, 13 bytes. Cube of a double. */
double cube_double_b3fdc(double x)
{
    return x * x * x;
}

/* 0xc5390, 12 bytes. Byte-swap a word in place; returns the advanced pointer (EBX). */
ef_addr32 swap_word_c5390(uint32_t u0, uint32_t u1, ef_addr32 ptr)
{
    (void)u0;
    (void)u1;
    uint16_t v = EF_U16(ptr);

    EF_U16(ptr) = (uint16_t)((v << 8) | (v >> 8));
    return ptr + 2u;
}

/* 0xc539c, 12 bytes. Byte-swap a word in place; returns the advanced pointer (EBX). */
ef_addr32 swap_word_c539c(uint32_t u0, uint32_t u1, ef_addr32 ptr)
{
    (void)u0;
    (void)u1;
    uint16_t v = EF_U16(ptr);

    EF_U16(ptr) = (uint16_t)((v << 8) | (v >> 8));
    return ptr + 2u;
}

/* 0xc53a8, 12 bytes. Byte-swap a word in place; returns the advanced pointer (EBX). */
ef_addr32 swap_word_c53a8(uint32_t u0, uint32_t u1, ef_addr32 ptr)
{
    (void)u0;
    (void)u1;
    uint16_t v = EF_U16(ptr);

    EF_U16(ptr) = (uint16_t)((v << 8) | (v >> 8));
    return ptr + 2u;
}

/* 0xc53df, 15 bytes. Byte-swap a dword in place; returns the advanced pointer (EBX). */
ef_addr32 swap_dword_c53df(uint32_t u0, uint32_t u1, ef_addr32 ptr)
{
    (void)u0;
    (void)u1;
    uint32_t v = EF_U32(ptr);

    EF_U32(ptr) = (v << 24) | ((v & 0xff00u) << 8) | ((v >> 8) & 0xff00u) | (v >> 24);
    return ptr + 4u;
}

/* 0xd175c, 24 bytes. Log a single value through the 0xd16d0 helper. */
void log_value_d175c(uint32_t value)
{
    guest_fn_d16d0((uint32_t)(uintptr_t)&value, 1);
}

/* 0xdcb74, 9 bytes. Stack bytes in use relative to the recorded stack base. */
uint32_t stack_used_dcb74(void)
{
    uint32_t marker;

    return (uint32_t)(uintptr_t)&marker - EF_U32(0x00132a40u);
}

/* 0xdd8d0, 12 bytes. Run the two shutdown helpers in order. */
void run_shutdown_pair_dd8d0(void)
{
    guest_fn_dd870_s();
    guest_fn_ddae0_s();
}

/* 0xe3022, 21 bytes. Format into a buffer through 0xed133 and terminate the result. */
uint32_t format_to_buffer_e3022(ef_addr32 buffer, ef_addr32 format, ef_addr32 args)
{
    uint32_t length = guest_fn_ed133_f(buffer, format, args, 0x000e300fu);

    EF_U8(buffer + length) = 0;
    return length;
}

/* 0xe3ffc, 20 bytes. Copy a name to the scratch string and run 0x107310. */
void set_scratch_name_e3ffc(ef_addr32 name)
{
    copy_string_9d749(0x0017d368u, name);
    guest_fn_107310_s();
}

/* 0xea4a7, 16 bytes. Forward a non-zero value to 0xfa8a0, else clear via 0x10ddf7. */
uint32_t forward_or_clear_ea4a7(uint32_t value)
{
    if (value != 0)
        return guest_tail_fa8a0(value);
    guest_fn_10ddf7_s();
    return 0;
}

/* 0xebe79, 7 bytes. Report error code 14. */
void report_error_14_ebe79(void)
{
    guest_fn_ebe6d(0xe);
}

/* 0xf9ecd, 5 bytes. Call 0x110123. */
void call_110123_f9ecd(void)
{
    guest_fn_110123_s();
}

/* 0x109070, 19 bytes. Reset via 0xe6120 then continue with the saved byte. */
void reset_with_saved_byte_109070(void)
{
    guest_fn_e6120_s(0);
    guest_fn_e6144_s(EF_U8(0x0041f833u));
}

/* 0x109440, 10 bytes. Run 0x10944c then 0x118b60. */
void run_pair_109440(void)
{
    guest_fn_10944c_s();
    guest_fn_118b60_s();
}

/* 0x10ddf7, 5 bytes. Return -1. */
uint32_t return_minus_one_10ddf7(void)
{
    return 0xffffffffu;
}

/* 0x10e4d3, 10 bytes. Run 0x10e4dd with the second argument also in EBX. */
void run_with_ebx_10e4d3(uint32_t a, uint32_t b)
{
    guest_fn_10e4dd_s(a, b, b);
}

/* 0x1105c3, 3 bytes. Return zero. */
uint32_t return_zero_1105c3(void)
{
    return 0;
}

/* 0x1105d0, 3 bytes. Stub that discards three stack arguments. */
void discard_three_1105d0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a, uint32_t stack_b,
                          uint32_t stack_c)
{
    (void)u0;
    (void)u1;
    (void)u2;
    (void)u3;
    (void)stack_a;
    (void)stack_b;
    (void)stack_c;
}

/* 0x1105d4, 3 bytes. Stub that discards two stack arguments. */
void discard_two_1105d4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a, uint32_t stack_b)
{
    (void)u0;
    (void)u1;
    (void)u2;
    (void)u3;
    (void)stack_a;
    (void)stack_b;
}

/* 0x1115f0, 10 bytes. Upper three bytes of a dword. */
uint32_t dword_shr8_1115f0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 stack_ptr)
{
    (void)u0;
    (void)u1;
    (void)u2;
    (void)u3;
    return EF_U32(stack_ptr) >> 8;
}

/* 0x1141a0, 16 bytes. Store a value at +0x2c of a record when the record exists. */
void set_record_field_1141a0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 stack_record,
                             uint32_t stack_value)
{
    (void)u0;
    (void)u1;
    (void)u2;
    (void)u3;
    if (stack_record != 0)
        EF_U32(stack_record + 0x2cu) = stack_value;
}

/* 0x11af88, 17 bytes. Absolute width of a bounding box. */
double box_width_11af88(ef_addr32 box)
{
    return abs_double_b427c(EF_F64(box + 0x20u) - EF_F64(box));
}

/* 0x11af9c, 21 bytes. Scaled sum of two extents. */
double box_extent_sum_11af9c(ef_addr32 box)
{
    return (EF_F64(box + 0x18u) - EF_F64(box + 0x10u) + (EF_F64(box + 0x38u) - EF_F64(box + 0x30u)))
           * EF_F64(0x0019580cu);
}

/* 0x11afe4, 15 bytes. Ratio of two extents. */
double box_extent_ratio_11afe4(ef_addr32 box)
{
    return (EF_F64(box + 0x38u) - EF_F64(box + 0x30u)) / (EF_F64(box + 0x18u) - EF_F64(box + 0x10u));
}

/* 0x11b3c0, 7 bytes. Constant double at 0x195864. */
double constant_double_11b3c0(void)
{
    return EF_F64(0x00195864u);
}

/* 0x11b694, 19 bytes. Scaled distance below the reference level. */
double level_below_11b694(ef_addr32 box, double value)
{
    return (EF_F64(0x00433070u) - EF_F64(box + 0x40u)) * value / EF_F64(box + 0x10u);
}

/* 0x11b6a8, 19 bytes. Scaled distance above the reference level. */
double level_above_11b6a8(ef_addr32 box, double value)
{
    return (EF_F64(box + 0x40u) - EF_F64(0x00433070u)) * value / EF_F64(box + 0x10u);
}

/* 0x11f2d3, 2 bytes. Disable interrupts (no-op on the host). */
void disable_interrupts_11f2d3(void)
{
}

/* 0x11f2d5, 2 bytes. Enable interrupts (no-op on the host). */
void enable_interrupts_11f2d5(void)
{
}

/* 0x16010e, 20 bytes. Signed word at +0x10 of the record found by id. */
int32_t record_word_16010e(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_id)
{
    (void)u0;
    (void)u1;
    (void)u2;
    (void)u3;
    ef_addr32 record = guest_fn_b3f58_id(stack_id);

    return EF_S16(record + 0x10u);
}

/* 0x39180, tail jump. Forward to 0x7a290. */
uint32_t tail_jump_39180(uint32_t a)
{
    return guest_tail_7a290(a);
}

/* 0x56a44, tail jump. Forward to 0x8fbc8. */
uint32_t tail_jump_56a44(uint32_t a)
{
    return guest_tail_8fbc8(a + 1u);
}

/* 0x58094, tail jump. Forward to 0xa3f30. */
uint32_t tail_jump_58094(uint32_t a)
{
    return guest_tail_a3f30(a);
}

/* 0x9d088, tail jump. Forward to 0xc3f80. */
uint32_t tail_jump_9d088(uint32_t a)
{
    return guest_tail_c3f80(a);
}

/* 0xcc768, tail jump. Forward to 0xc3f80. */
uint32_t tail_jump_cc768(uint32_t a)
{
    return guest_tail_c3f80(a);
}

/* 0xab954, tail jump. Forward to 0xc9fc4. */
uint32_t tail_jump_ab954(uint32_t a)
{
    return guest_tail_c9fc4(a);
}

/* 0xac7c0, tail jump. Forward to 0xabec4. */
uint32_t tail_jump_ac7c0(uint32_t a)
{
    return guest_tail_abec4(a);
}

/* 0xc3f70, tail jump. Forward to 0xfc795. */
uint32_t tail_jump_c3f70(uint32_t a)
{
    return guest_tail_fc795(a);
}

/* 0xc3f78, tail jump. Forward to 0xfc354. */
uint32_t tail_jump_c3f78(uint32_t a)
{
    return guest_tail_fc354(a);
}

/* 0xc3f80, tail jump. Forward to 0xfa9d2. */
uint32_t tail_jump_c3f80(uint32_t a)
{
    return guest_tail_fa9d2(a);
}

/* 0xcbc9b, tail jump. Forward to 0xfe541. */
uint32_t tail_jump_cbc9b(uint32_t a)
{
    return guest_tail_fe541(a);
}

/* 0x11c0a4, tail jump. Forward to 0xc3f70. */
uint32_t tail_jump_11c0a4(uint32_t a)
{
    return guest_tail_c3f70((a & 0xffu) << 6);
}

/* 0xe5bfe, tail jump. Forward to 0x109e6c. */
uint32_t tail_jump_e5bfe(uint32_t a)
{
    return guest_tail_109e6c(a);
}

/* 0x101e70, tail jump. Forward to 0x110dcc. */
uint32_t tail_jump_101e70(uint32_t a)
{
    return guest_tail_110dcc(a);
}

/* 0x101e78, tail jump. Forward to 0x110dd9. */
uint32_t tail_jump_101e78(uint32_t a)
{
    return guest_tail_110dd9(a);
}

/* 0x101f70, tail jump. Forward to 0x1111e1. */
uint32_t tail_jump_101f70(uint32_t a)
{
    return guest_tail_1111e1(a);
}

/* 0x102020, tail jump. Forward to 0x111359. */
uint32_t tail_jump_102020(uint32_t a)
{
    return guest_tail_111359(a);
}

/* 0x106be0, tail jump. Forward to 0xf8248. */
uint32_t tail_jump_106be0(uint32_t a)
{
    return guest_tail_f8248(a);
}

/* 0x115984, tail jump. Forward to 0xfa9a0. */
uint32_t tail_jump_115984(uint32_t a)
{
    return guest_tail_fa9a0(a);
}

/* 0x11d24e, tail jump. Forward to 0x120485. */
uint32_t tail_jump_11d24e(uint32_t a)
{
    return guest_tail_120485(a);
}

/* 0xc9ec4, tail jump. Forward to 0xdc2c0. */
void tail_jump_c9ec4(void)
{
    guest_tail_dc2c0();
}

/* 0x9c23d, tail jump. Forward to 0x9c224. */
uint32_t tail_jump_9c23d(void)
{
    return guest_tail_9c224(0x601u);
}
