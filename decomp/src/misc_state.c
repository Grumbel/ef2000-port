/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0x85400, 31 bytes. Grid row (signed AX) byte 0x8e = byte 0x8f - 1. */
void decrement_grid_row_link(int16_t cell)
{
    ef_addr32 row = g_world_grid_base + (ef_addr32)((int32_t)cell * 0xca);
    EF_U8(row + 0x8e) = (uint8_t)(EF_U8(row + 0x8f) - 1u);
}

/* 0x8828c, 34 bytes. Clear three bytes 0x12d7aa..ac and word 0x20f196. */
void clear_input_state_block(void)
{
    EF_U8(0x0012d7aau) = 0;
    EF_U8(0x0012d7abu) = 0;
    EF_U8(0x0012d7acu) = 0;
    EF_U16(0x0020f196u) = 0;
}

/* 0x89140, 33 bytes. Record dword +0x26 plus a double constant, truncated
 * toward zero through the 0x9d72c rounding helper, stored back; then 0x89924. */
void adjust_record_scalar(ef_addr32 record)
{
    double value = (double)EF_S32(record + 0x26u) + *(const double *)ef_guest_memory_ptr(0x0018a4e0u);
    EF_S32(record + 0x26u) = (int32_t)value;
    guest_after_scalar_adjust(record);
}

/* 0x8ba78, 11 bytes. */
void clear_dword_12db5c(void)
{
    EF_U32(0x0012db5cu) = 0;
}

/* 0x8bdd4, 11 bytes. */
void copy_dword_20f420_to_132ad4(void)
{
    EF_U32(0x00132ad4u) = EF_U32(0x0020f420u);
}

/* 0x8f5d8, 26 bytes. If guest check 0x8c09c(record, value) is nonzero and
 * value equals the dword at +0x1c, clear that dword. */
void clear_matching_link_a(ef_addr32 record, uint32_t value)
{
    if (guest_link_check_a(record, value) != 0 && value == EF_U32(record + 0x1cu))
        EF_U32(record + 0x1cu) = 0;
}

/* 0x90924, 26 bytes. As 0x8f5d8, but with the 0x90050 check. */
void clear_matching_link_b(ef_addr32 record, uint32_t value)
{
    if (guest_link_check_b(record, value) != 0 && value == EF_U32(record + 0x1cu))
        EF_U32(record + 0x1cu) = 0;
}

/* 0x91238, 33 bytes. Byte 0x12db9e = 0x35, word 0x21449c = 1, 0xd0b68(0x12dbec). */
void begin_mode_35(void)
{
    EF_U8(0x0012db9eu) = 0x35;
    EF_U16(0x0021449cu) = 1;
    guest_runtime_acquire(0x0012dbecu);
}

/* 0x919b8, 25 bytes. If mode byte 0x12db9e is 0x36 call 0xdc334(1, 1). */
void end_mode_36(void)
{
    if (EF_U8(0x0012db9eu) == 0x36)
        guest_mode_36_hook(1, 1);
}

/* 0x91c00, 30 bytes. Sum of squares of three signed words (AX, DX, BX)
 * passed in EAX to 0xe2b60. */
uint32_t length_squared_word_triple(int16_t x, int16_t y, int16_t z)
{
    int32_t sum = (int32_t)x * x + (int32_t)y * y + (int32_t)z * z;
    return guest_square_root_of(sum);
}

/* 0x91c34, 17 bytes. AL = word 0x2144ae != word 0x2144ac. */
uint8_t scene_words_differ(void)
{
    return (uint8_t)(EF_U16(0x002144aeu) != EF_U16(0x002144acu));
}

/* 0x94948, 11 bytes. Forward record and its dword +0x30 to 0x9457c. */
uint32_t forward_record_field_30(ef_addr32 record)
{
    return guest_record_field_30(record, EF_U32(record + 0x30u));
}

/* 0x94ce8, 31 bytes. Clear three words 0x12dba2/a4/a6 then call 0x94a74. */
void reset_scene_words_and_update(void)
{
    EF_U16(0x0012dba6u) = 0;
    EF_U16(0x0012dba4u) = 0;
    EF_U16(0x0012dba2u) = 0;
    guest_scene_update();
}

/* 0x94ffc, 28 bytes. Signed high word of dword 0x21449e selects a filtered
 * object; if found store it at 0x214458 and call 0x94d08. All paths fall
 * through into 0x95018. */
void select_scene_object(void)
{
    ef_addr32 object = lookup_filtered_indexed_object((int16_t)(EF_S32(0x0021449eu) >> 16));
    if (object != 0) {
        EF_U32(0x00214458u) = object;
        guest_scene_apply();
    }
    request_scene_refresh();
}

/* 0x9783c, 16 bytes. Word +6 = 0, word +8 = EDX - 1, tail call 0x975f0. */
void reset_record_range(ef_addr32 record, uint32_t count)
{
    EF_U16(record + 6u) = 0;
    EF_U16(record + 8u) = (uint16_t)(count - 1u);
    guest_record_tail(record);
}

/* 0x97dc0, 11 bytes. Call 0x10640, return dword 0x197c14. */
uint32_t sample_tick_counter(void)
{
    guest_tick_sample();
    return EF_U32(0x00197c14u);
}

/* 0x97f70, 21 bytes. Result of 0x9c858, or 0x18acb0 when it returns 0. */
uint32_t pointer_or_default(void)
{
    uint32_t value = guest_current_pointer();
    if (value == 0)
        value = 0x0018acb0u;
    return value;
}

/* 0x98000, 19 bytes. If byte 0x420a72 is set tail-call 0xd5f0c with dword
 * 0x12dd98; otherwise return (EAX left as the caller's). */
void forward_if_enabled(void)
{
    if (EF_U8(0x00420a72u) != 0)
        guest_forward_value(EF_U32(0x0012dd98u));
}

/* 0x98558, 27 bytes. Install allocator entry 0x9850c at 0x12dd9c and
 * pool top 0x1f4000 at 0x408580. */
void install_allocator_hooks(void)
{
    EF_U32(0x0012dd9cu) = 0x0009850cu;
    EF_U32(0x00408580u) = 0x001f4000u;
}
