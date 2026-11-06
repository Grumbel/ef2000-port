/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xf60, 32 bytes. Zero returns 0; otherwise a 15-bit guest random value
 * (low word, via 0x9c960) modulo the signed divisor. */
int32_t random_modulo(int32_t divisor)
{
    if (divisor == 0)
        return 0;
    return (int32_t)(uint16_t)guest_random_word() % divisor;
}

/* 0x3cd4, 27 bytes. Signed maximum of three registers (EAX, EDX, EBX). */
int32_t max3_signed(int32_t a, int32_t b, int32_t c)
{
    int32_t m = a > b ? a : b;
    return m > c ? m : c;
}

/* 0x6bf4, 40 bytes. Clear three words and two bytes of command state. */
void clear_command_state_6bf4(void)
{
    EF_U16(0x00197244u) = 0;
    EF_U16(0x00197246u) = 0;
    EF_U16(0x00197248u) = 0;
    EF_U8(0x00197254u) = 0;
    EF_U8(0x00121e4fu) = 0;
}

/* 0x73d4, 23 bytes. EDX points at a record; AL = !(double at +0x71 > double
 * constant 0x1803c4). Unordered compares also yield 1 (JBE after SAHF). */
uint8_t record_value_not_above_limit(ef_addr32 record)
{
    double value = *(const double *)ef_guest_memory_ptr(record + 0x71u);
    double limit = *(const double *)ef_guest_memory_ptr(0x001803c4u);
    return (uint8_t)!(value > limit);
}

/* 0x1064c, 16 bytes. Sample the clock, then latch it as the previous tick. */
void latch_tick_counter(void)
{
    guest_tick_sample();
    EF_U32(0x00197c10u) = EF_U32(0x00197c14u);
}

/* 0x10688, 32 bytes. Sample the clock, return elapsed ticks, latch previous. */
uint32_t elapsed_ticks(void)
{
    uint32_t now;
    uint32_t previous;
    guest_tick_sample();
    now = EF_U32(0x00197c14u);
    previous = EF_U32(0x00197c10u);
    EF_U32(0x00197c10u) = now;
    return now - previous;
}

/* 0x11d20, 35 bytes. Slot flag byte (29-byte rows at 0x121fb4) = DL, refresh. */
void set_runtime_slot_flag(int16_t index, uint8_t value)
{
    EF_U8(0x00121fb4u + (uint32_t)((int32_t)index * 29)) = value;
    guest_refresh_runtime_slot(index);
}

/* 0x12140, 29 bytes. Refresh all 54 runtime slots in order. */
void refresh_all_runtime_slots(void)
{
    int16_t i = 0;
    while (i < 54) {
        guest_refresh_runtime_slot(i);
        ++i;
    }
}

/* 0x14d3c, 26 bytes. Byte at 0x1225d4 + 19*signed index. */
uint8_t read_event_slot_byte(int16_t index)
{
    return EF_U8(0x001225d4u + (uint32_t)((int32_t)index * 19));
}

/* 0x151a0, 36 bytes. Byte 0x12289e = 1, 0x12289f = 0, 0x1228a0 = 0 and the
 * constant byte 0x181730 copied to 0x197c50. */
void reset_dialog_latches(void)
{
    EF_U8(0x0012289eu) = 1;
    EF_U8(0x0012289fu) = 0;
    EF_U8(0x001228a0u) = 0;
    EF_U8(0x00197c50u) = EF_U8(0x00181730u);
}

/* 0x17388, 25 bytes. Look up an object by signed index through 0x8ffe8;
 * if found set bits 0 and 1 of its byte +0x30 and tail-call 0x3739c with the
 * signed high word of dword +0x36. Not found returns 0. */
uint32_t flag_object_and_forward(int16_t index)
{
    ef_addr32 object = guest_lookup_object_8ffe8((int32_t)index);
    if (object == 0)
        return 0;
    EF_U8(object + 0x30u) |= 3;
    return guest_forward_high_word(EF_S32(object + 0x36u) >> 16);
}

/* 0x173a4, 30 bytes. As 0x85400 but with a full 32-bit row index. */
void decrement_grid_row_link_wide(int32_t cell)
{
    ef_addr32 row = g_world_grid_base + (ef_addr32)(cell * 0xca);
    EF_U8(row + 0x8e) = (uint8_t)(EF_U8(row + 0x8f) - 1u);
}

/* 0x27538, 12 bytes. Dword 0x19b430 = EDX, dword 0x19b434 = EAX. */
void store_dword_pair_19b430(uint32_t first, uint32_t second)
{
    EF_U32(0x0019b430u) = second;
    EF_U32(0x0019b434u) = first;
}

/* 0x28a70, 25 bytes. Filtered lookup of signed index; true when the object
 * exists and its dword +0x1c is nonzero. */
uint8_t filtered_object_has_link(int16_t index)
{
    ef_addr32 object = lookup_filtered_indexed_object(index);
    if (object == 0)
        return 0;
    return (uint8_t)(EF_U32(object + 0x1cu) != 0);
}

/* 0x28a8c, 25 bytes. As 0x28a70 through the 0x8ffe8 lookup. */
uint8_t object_has_link(int16_t index)
{
    ef_addr32 object = guest_lookup_object_8ffe8((int32_t)index);
    if (object == 0)
        return 0;
    return (uint8_t)(EF_U32(object + 0x1cu) != 0);
}

/* 0x2a4a0, 40 bytes. Signed rectangle test: EAX <= limit_x, EBX >= limit_x,
 * EDX <= limit_y, ECX >= limit_y with limits at 0x1a5bec and 0x1a5be8. */
uint8_t point_range_contains_limits(int32_t low_x, int32_t low_y, int32_t high_x, int32_t high_y)
{
    int32_t limit_x = EF_S32(0x001a5becu);
    int32_t limit_y;
    if (low_x > limit_x)
        return 0;
    if (high_x < limit_x)
        return 0;
    limit_y = EF_S32(0x001a5be8u);
    if (low_y > limit_y)
        return 0;
    if (high_y < limit_y)
        return 0;
    return 1;
}

/* 0x2a794, 23 bytes. Clear three dwords at 0x19bb38..40. */
void clear_dwords_19bb38(void)
{
    EF_U32(0x0019bb40u) = 0;
    EF_U32(0x0019bb3cu) = 0;
    EF_U32(0x0019bb38u) = 0;
}

/* The seven stubs below load a constant into EAX and tail-jump to 0x32c04. */
/* 0x32b3c, 10 bytes. */
uint32_t call_32c04_182ad8(void) { return guest_fn_32c04(0x00182ad8u); }
/* 0x32b48, 10 bytes. */
uint32_t call_32c04_182adc(void) { return guest_fn_32c04(0x00182adcu); }
/* 0x32b54, 10 bytes. */
uint32_t call_32c04_182ae0(void) { return guest_fn_32c04(0x00182ae0u); }
/* 0x32b60, 10 bytes. */
uint32_t call_32c04_182ae4(void) { return guest_fn_32c04(0x00182ae4u); }
/* 0x32b6c, 10 bytes. */
uint32_t call_32c04_182ae8(void) { return guest_fn_32c04(0x00182ae8u); }
/* 0x32b9c, 10 bytes. */
uint32_t call_32c04_182af8(void) { return guest_fn_32c04(0x00182af8u); }
/* 0x32ba8, 10 bytes. */
uint32_t call_32c04_182afc(void) { return guest_fn_32c04(0x00182afcu); }

/* 0x33750, 14 bytes. Tail-call 0xa41c8 when byte 0x1357c8 is nonzero. */
void forward_if_flag_1357c8(void)
{
    if (EF_U8(0x001357c8u) != 0)
        guest_fn_a41c8();
}

/* 0x3918c, 22 bytes. Dword 0x1a1780 = 0x53ecc, byte 0x1a363a = 0. */
void install_callback_53ecc(void)
{
    EF_U32(0x001a1780u) = 0x00053eccu;
    EF_U8(0x001a363au) = 0;
}

/* 0x3bd28, 34 bytes. Grid row byte +0x1a in 0x0a..0x0c returns 0, else 1. */
uint8_t grid_row_state_allowed(int16_t cell)
{
    uint8_t state = EF_U8(g_world_grid_base + (ef_addr32)((int32_t)cell * 0xca) + 0x1au);
    if (state >= 0x0a && state <= 0x0c)
        return 0;
    return 1;
}

/* 0x3c434, 10 bytes. Tail-call 0xe2af4 with dword 0x1a2e78. */
uint32_t forward_value_1a2e78(void)
{
    return guest_fn_e2af4(EF_U32(0x001a2e78u));
}

/* 0x3c768, 37 bytes. Copy byte +0 and word +2 of the table entry returned
 * by 0x3c730(0x1836c4, signed index) into the record at dword 0x1264c8,
 * at +0x0b and +0x2a. */
void copy_table_entry_to_record(int16_t index)
{
    ef_addr32 entry = guest_table_entry_3c730(0x001836c4u, (int32_t)index);
    ef_addr32 target = EF_U32(0x001264c8u);
    EF_U8(target + 0x0bu) = EF_U8(entry);
    EF_U16(target + 0x2au) = EF_U16(entry + 2u);
}

/* 0x3cd24, 26 bytes. When 0x3ccec returns a nonzero object run 0x3cc94 and
 * 0xe2af4(object); returns that result, else 0. */
uint32_t activate_object_3cc94(void)
{
    uint32_t object = guest_fn_3ccec();
    if (object == 0)
        return 0;
    guest_fn_3cc94();
    return guest_fn_e2af4(object);
}

/* 0x3cd40, 31 bytes. As 0x3cd24 with 0x3cb2c, returning dword 0x1a2e98. */
uint32_t activate_object_3cb2c(void)
{
    uint32_t object = guest_fn_3ccec();
    if (object == 0)
        return 0;
    guest_fn_3cb2c();
    guest_fn_e2af4(object);
    return EF_U32(0x001a2e98u);
}

/* 0x3ce04, 37 bytes. Fill the dword table at 0x1a3150..0x1a3554 (EDX steps
 * 4..0x408) with (0x2fdb result & 0x1ff) - 0x100. */
void fill_random_offsets(void)
{
    uint32_t offset = 0;
    do {
        int32_t value = (int32_t)(guest_fn_e2fdb() & 0x1ffu);
        offset += 4;
        EF_S32(0x001a314cu + offset) = value - 0x100;
    } while (offset != 0x408);
}

/* 0x3dcb0, 15 bytes. Run 0x394e0 and 0x8846c, then tail-call 0x888cc. */
void run_reset_sequence_3dcb0(void)
{
    guest_fn_394e0();
    guest_fn_8846c();
    guest_fn_888cc();
}

/* 0x410d0, 36 bytes. Run 0x53270, clear byte 0x125847, 0x8bd58(0x1a58b4,
 * 0x1a5a1c) and 0x49228. */
void reset_world_state_410d0(void)
{
    guest_fn_53270();
    EF_U8(0x00125847u) = 0;
    guest_fn_8bd58(0x001a58b4u, 0x001a5a1cu);
    guest_fn_49228();
}

/* 0x41df4, 24 bytes. AL is 0x0b or 0x0c. */
uint32_t code_is_0b_or_0c(uint8_t code)
{
    return code == 0x0b || code == 0x0c;
}

/* 0x41e0c, 24 bytes. AL is 0x26 or 0x27. */
uint32_t code_is_26_or_27(uint8_t code)
{
    return code == 0x26 || code == 0x27;
}

/* 0x41e84, 28 bytes. AL is 0, 0x16 or 2. */
uint32_t code_is_0_16_or_2(uint8_t code)
{
    return code == 0 || code == 0x16 || code == 2;
}
