/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Tiny accessors, flag setters and thin wrappers, 0x9c024..0xea864. */

/* 0x9c024, 23 bytes. Real-mode segment:offset style conversion of a dword (cdecl). */
uint32_t far_to_linear_9c024(uint32_t value)
{
    return ((value & 0xffff0000u) >> 12) + (value & 0xffffu);
}

/* 0x9c1a1, 16 bytes. High word of a dword (cdecl). */
uint32_t high_word_9c1a1(uint32_t value)
{
    return value >> 16;
}

/* 0x9c1b1, 10 bytes. Identity (cdecl). */
uint32_t identity_9c1b1(uint32_t value)
{
    return value;
}

/* 0x9c960, 19 bytes. */
uint32_t half_word_9c960(void)
{
    return (guest_fn_a45b3() & 0xffffu) >> 1;
}

/* 0x9d780, 20 bytes. Copy the first and third dword of a record and clear the second. */
void copy_head_9d780(ef_addr32 destination, ef_addr32 source)
{
    uint32_t first = EF_U32(source);

    EF_U32(destination + 4u) = 0;
    EF_U32(destination) = first;
    EF_U32(destination + 8u) = EF_U32(source + 8u);
}

/* 0x9d794, 23 bytes. */
void set_axis_9d794(uint32_t a, uint32_t b)
{
    EF_U16(0x00417228u) = (uint16_t)b;
    EF_U16(0x00417224u) = (uint16_t)a;
    EF_U16(0x00417222u) = 0;
}

/* 0x9dde0, 27 bytes. Store the converted table entry index into the 16-bit slot at destination. */
uint16_t store_table_entry_9dde0(ef_addr32 destination, uint32_t index)
{
    uint16_t value = (uint16_t)guest_fn_de864(EF_U32(EF_U32(0x00132b08u) + (index & 0xffffu) * 4u));

    EF_U16(destination) = value;
    return value;
}

/* 0xa41c8, 8 bytes. */
void set_flag_a41c8(void)
{
    EF_U8(0x00136dd0u) = 1;
}

/* 0xa41d0, 9 bytes. */
void clear_flag_a41d0(void)
{
    EF_U8(0x00136dd0u) = 0;
}

/* 0xa455b, 25 bytes. Run edd49 with the byte at 0x132a60 cleared, then restore the dword. */
void call_edd49_a455b(void)
{
    uint32_t saved = EF_U32(0x00132a60u);

    EF_U8(0x00132a60u) = 0;
    guest_fn_edd49();
    EF_U32(0x00132a60u) = saved;
}

/* 0xa4d90, 29 bytes. */
ef_addr32 apply_a4cb4_a4d90(ef_addr32 key)
{
    ef_addr32 object = guest_fn_a46cc(key);

    if (object != 0)
        guest_fn_a4cb4(EF_U8(object + 0x10u));
    return object;
}

/* 0xa4db0, 29 bytes. */
ef_addr32 apply_a4b80_a4db0(ef_addr32 key)
{
    ef_addr32 object = guest_fn_a46cc(key);

    if (object != 0)
        guest_fn_a4b80(EF_U8(object + 0x10u));
    return object;
}

/* 0xa6018, 29 bytes. */
ef_addr32 apply_a5ef8_a6018(ef_addr32 key)
{
    ef_addr32 object = guest_fn_a46cc(key);

    if (object != 0)
        guest_fn_a5ef8(EF_U8(object + 0x10u));
    return object;
}

/* 0xa7318, 15 bytes. */
ef_addr32 enable_found_widget_a7318(ef_addr32 key)
{
    ef_addr32 widget = guest_fn_a78bc(key);

    enable_widget_a72d8(widget);
    return widget;
}

/* 0xa75fc, 8 bytes. */
uint32_t first_link_a75fc(ef_addr32 object)
{
    if (object == 0)
        return object;
    return EF_U32(object + 0x27u);
}

/* 0xa7b70, 16 bytes. */
ef_addr32 default_path_a7b70(void)
{
    build_default_path_9c980(0x00419560u);
    return 0x00419560u;
}

/* 0xb3d68, 9 bytes. */
uint32_t read_counter_b3d68(void)
{
    return EF_U16(0x00146e24u);
}

/* 0xbaf24, 8 bytes. */
void clear_pair_baf24(ef_addr32 pair)
{
    EF_U8(pair) = 0;
    EF_U8(pair + 1u) = 0;
}

/* 0xbc450, 21 bytes. */
uint32_t release_by_key_bc450(ef_addr32 key, uint32_t extra)
{
    uint32_t result = guest_fn_bc4da(key, extra);

    release_list_node_fa77b(key);
    return result;
}

/* 0xc8e38, 9 bytes. */
void clear_byte_c8e38(void)
{
    EF_U8(0x0041f5f0u) = 0;
}

/* 0xc8fdc, 9 bytes. */
void clear_byte_c8fdc(void)
{
    EF_U8(0x0041f6f2u) = 0;
}

/* 0xc90c0, 9 bytes. */
void clear_byte_c90c0(void)
{
    EF_U8(0x0041ef06u) = 0;
}

/* 0xcbd0f, 27 bytes. Signed decimal prefix, then the digit conversion. */
ef_addr32 signed_to_digits_cbd0f(int32_t value, ef_addr32 buffer, uint32_t base)
{
    ef_addr32 start = buffer;

    if (base == 10 && value < 0) {
        value = -value;
        EF_U8(buffer) = 0x2d;
        buffer++;
    }
    guest_fn_cbcc5(value, buffer, base);
    return start;
}

/* 0xcd29c, 9 bytes. */
ef_addr32 script_cursor_data_cd29c(void)
{
    return EF_U32(0x0015b628u) + 0xeu;
}

/* 0xd16fc, 13 bytes. */
void call_d16d0_d16fc(uint32_t value)
{
    guest_fn_d16d0(value, 4);
}

/* 0xd6f88, 8 bytes. */
uint32_t read_byte_d6f88(void)
{
    return EF_U8(0x0015bac4u);
}

/* 0xd7070, 20 bytes. */
ef_addr32 build_path_d7070(ef_addr32 second)
{
    build_path_d7040(0x00420e11u, second);
    return 0x00420e11u;
}

/* 0xd7738, 9 bytes. */
uint32_t read_field_d7738(ef_addr32 record)
{
    return EF_U8(record + 0xeu);
}

/* 0xd7748, 9 bytes. */
uint32_t read_field_d7748(ef_addr32 record)
{
    return EF_U8(record + 0x17u);
}

/* 0xe7cd0, 28 bytes. Append a fixed string and then second to first. */
void append_two_e7cd0(ef_addr32 first, ef_addr32 second)
{
    guest_fn_9ca57(first, 0x00190c4cu);
    guest_fn_9ca57(first, second);
}

/* 0xea414, 9 bytes. */
uint32_t read_link_ea414(void)
{
    return EF_U32(EF_U32(0x004644d0u) + 4u);
}

/* 0xea864, 9 bytes. Store three dwords. */
void store_triple_ea864(ef_addr32 destination, uint32_t a, uint32_t b, uint32_t c)
{
    EF_U32(destination) = a;
    EF_U32(destination + 4u) = b;
    EF_U32(destination + 8u) = c;
}

/* 0xf13ac, 27 bytes. */
void refresh_object_f13ac(ef_addr32 object)
{
    guest_fn_a5764(object);
    guest_fn_a66a0(object);
    guest_fn_a5ef8(EF_U8(object + 0x10u));
}
