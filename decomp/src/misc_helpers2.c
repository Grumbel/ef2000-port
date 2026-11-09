/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xeba4c, 21 bytes. Zero when EDX is zero, else dword +8 of 0xeb520's result (or 0). */
uint32_t lookup_field_eba4c(uint32_t key, uint32_t index)
{
    ef_addr32 entry;
    if (index == 0)
        return 0;
    entry = guest_fn_eb520(key, index);
    if (entry == 0)
        return 0;
    return EF_U32(entry + 8u);
}

/* 0xebcd8, 36 bytes. Release the object's pointer at +0x154 for flag 0x80000, else defer to 0xa7990. */
void release_or_forward_ebcd8(uint32_t flags, ef_addr32 object)
{
    if (object == 0)
        return;
    if (flags & 0x80000u)
        release_pointer_eb4c0(object + 0x154u);
    else
        guest_fn_a7990(flags);
}

/* 0xebcfc, 52 bytes. Advance the cursor at +0x158 by EAX and return the distance from +0x154. */
uint32_t advance_cursor_ebcfc(uint32_t amount, uint32_t flags, ef_addr32 object)
{
    uint32_t cursor;
    if (object == 0)
        return 0;
    if (flags & 0x80000u) {
        cursor = EF_U32(object + 0x158u) + amount;
        EF_U32(object + 0x158u) = cursor;
        return cursor - EF_U32(object + 0x154u);
    }
    /* 0xa7ac8 reads flags at [ESP+4] and amount at [ESP+8]. */
    return guest_fn_a7ac8(flags, amount);
}

/* 0xebe6d, 12 bytes. Store EAX through the pointer returned by 0xebd30. */
void store_through_ebd30(uint32_t value)
{
    ef_addr32 slot = guest_fn_ebd30(value);
    EF_U32(slot) = value;
}

/* 0xebe90, 12 bytes. As 0xebe6d with 0xebd36. */
void store_through_ebd36(uint32_t value)
{
    ef_addr32 slot = guest_fn_ebd36(value);
    EF_U32(slot) = value;
}

/* 0xec0ec, 51 bytes. Compare 6 bytes at dword 0x1368bc[AX]+0x15 with those at dword 0x135874[DX*32]+0x16. */
uint8_t compare_names_ec0ec(uint16_t first, uint16_t second)
{
    ef_addr32 a = EF_U32(0x001368bcu + (uint32_t)first * 4u) + 0x15u;
    ef_addr32 b = EF_U32(0x00135874u + ((uint32_t)second << 5)) + 0x16u;
    return guest_memcmp(a, b, 6) == 0;
}

/* 0xece90, 30 bytes. Clear 0x10 bytes at 0x13577c and byte 0x484665. */
void reset_table_ece90(void)
{
    guest_fn_b3fb0(0x0013577cu, 0, 0x10);
    EF_U8(0x00484665u) = 0;
}

/* 0xed115, 30 bytes. memcmp of EBX bytes at EAX and EDX: 0, -1 or 1 (unsigned). */
int32_t guest_memcmp(ef_addr32 first, ef_addr32 second, uint32_t count)
{
    while (count != 0) {
        uint8_t a = EF_U8(first);
        uint8_t b = EF_U8(second);
        if (a != b)
            return a < b ? -1 : 1;
        ++first;
        ++second;
        --count;
    }
    return 0;
}

/* 0xedb15, 24 bytes. Uppercase the string at EAX in place. */
void uppercase_string_edb15(ef_addr32 text)
{
    while (EF_U8(text) != 0) {
        EF_U8(text) = (uint8_t)to_upper_ascii(EF_U8(text));
        ++text;
    }
}

/* 0xf17d8, 51 bytes. Call 0xa4cb4 on up to five words starting at +0x59. */
void process_words_f17d8(ef_addr32 record)
{
    int32_t i = 0;
    while (EF_U16(record + 0x59u) != 0 && i < 5) {
        guest_fn_a4cb4(EF_U16(record + 0x59u));
        record += 2u;
        ++i;
    }
}

/* 0xf180c, 51 bytes. As 0xf17d8 with words at +0x63 and 0xa4b80. */
void process_words_f180c(ef_addr32 record)
{
    int32_t i = 0;
    while (EF_U16(record + 0x63u) != 0 && i < 5) {
        guest_fn_a4b80(EF_U16(record + 0x63u));
        record += 2u;
        ++i;
    }
}

/* 0xf227f, 48 bytes. Position from 0x10f4df(dword +0x10) combined with dword +4. */
int32_t file_position_f227f(ef_addr32 file)
{
    int32_t position = (int32_t)guest_fn_10f4df(EF_U32(file + 0x10u));
    int32_t base;
    if (position == -1)
        return position;
    base = EF_S32(file + 4u);
    if (base == 0)
        return position;
    if (EF_U8(file + 0xdu) & 0x10u)
        return base + position;
    return position - base;
}

/* 0xcd4d4, 125 bytes. Initialize keyboard input, its two request buffers and
 * timer; install the restore-page callback on the first initialization. */
void initialize_input_services_cd4d4(void)
{
    uint16_t code_selector = (uint16_t)guest_seg_x(1u);
    uint16_t data_selector = (uint16_t)guest_seg_x(3u);

    if (EF_U8(0x0015b72eu) != 1u) {
        set_keyboard_leds_cd414(0u);
        init_keys_cd430();
        (void)store_selector_fe610();
        EF_U32(0x0041f828u) = start_request_628(0x000fe61fu,
                                                   code_selector, 0xd5);
        EF_U32(0x0041f82cu) = start_request_628(0x0041f833u,
                                                   data_selector, 0x118);
        EF_U32(0x0041f824u) = start_timer_734(9u, 0u, 0x000fe61fu,
                                               code_selector);
        if (EF_U8(0x0015b72eu) == 0u)
            guest_fn_9c3cf(0x000cd554u);
        EF_U8(0x0015b72eu) = 1u;
    }
#if defined(EF_DECOMP_NATIVE)
    {
        extern void ef_decomp_x32_after_input(void);
        ef_decomp_x32_after_input();
    }
#endif
}

/* 0xf23ac, 49 bytes. For each byte of the string at ECX call 0xf2320(x, y, byte & 0x3f), x += 8. */
void draw_text_f23ac(uint8_t color, int32_t x, int32_t y, ef_addr32 text)
{
    EF_U8(0x00484680u) = color;
    while (EF_U8(text) != 0) {
        uint32_t c = EF_U8(text) & 0x3fu;
        ++text;
        guest_fn_f2320(x, y, c);
        x += 8;
    }
}

/* 0xf2800, 25 bytes. 0xf2819(), then return byte 0x161156. */
uint32_t result_f2800(void)
{
    guest_fn_f2819();
    return EF_U8(0x00161156u);
}

/* 0xf2d54, 31 bytes. The string at 0x48592c ends with ':'. */
uint8_t path_ends_with_colon(void)
{
    int16_t length = (int16_t)guest_fn_b1b8b(0x0048592cu);
    return EF_U8(0x0048592bu + (uint32_t)(int32_t)length) == 0x3au;
}

/* 0xf2dcc, 46 bytes. Truncate the string at the last '.'. */
void strip_extension(ef_addr32 text)
{
    int16_t i = (int16_t)guest_fn_b1b8b(text);
    while (i > 0) {
        ef_addr32 p = text + (uint32_t)(int32_t)i;
        if (EF_U8(p) == 0x2eu) {
            EF_U8(p) = 0;
            return;
        }
        --i;
    }
}

/* 0xf2e54, 30 bytes. 0xf2cd0(), 0x9d749(0x485908, 0x48592c), byte 0x4859de = 1. */
void copy_path_f2e54(void)
{
    guest_fn_f2cd0();
    guest_fn_9d749(0x00485908u, 0x0048592cu);
    EF_U8(0x004859deu) = 1;
}

/* 0xf3480, 32 bytes. */
void refresh_f3480(void)
{
    guest_fn_dc250();
    guest_fn_c386c();
    guest_fn_ddae0();
    guest_fn_dd8dc();
    guest_fn_dc250();
}

/* 0xf34a0, 42 bytes. */
void refresh_f34a0(void)
{
    guest_fn_c3974();
    guest_fn_eab44();
    guest_fn_c39ac();
    guest_fn_c386c();
    guest_fn_ddae0();
    guest_fn_dd8dc();
    guest_fn_dc250();
}

/* 0xf35d8, 22 bytes. Word 0x161e1c = 0x9d4cd(0x48592c) after 0xf2cd0(). */
void load_path_value_f35d8(void)
{
    guest_fn_f2cd0();
    EF_U16(0x00161e1cu) = (uint16_t)guest_fn_9d4cd(0x0048592cu);
}

/* 0xf35f0, 32 bytes. */
void refresh_f35f0(void)
{
    guest_fn_c39ac();
    guest_fn_c386c();
    guest_fn_dc250();
    guest_fn_c3990();
    guest_fn_dd8dc();
}

/* 0xf3b1c, 33 bytes. */
void load_path_f3b1c(void)
{
    guest_fn_f2cd0();
    guest_fn_9ca57(0x0048592cu, 0x00191918u);
    guest_fn_f3a1c(0x0048592cu);
}

/* 0xf3b40, 22 bytes. As 0xf35d8, storing word 0x161e1e. */
void load_path_value_f3b40(void)
{
    guest_fn_f2cd0();
    EF_U16(0x00161e1eu) = (uint16_t)guest_fn_9d4cd(0x0048592cu);
}

/* 0xf3b58, 32 bytes. */
void load_path_pointer_f3b58(void)
{
    ef_addr32 slot = guest_fn_a7ec4(0x0048592cu);
    guest_fn_f2cd0();
    EF_U32(slot) = guest_fn_cc76d(0x0048592cu);
}

/* 0xf4178, 47 bytes. */
void reset_flags_f4178(void)
{
    EF_U8(0x004859dbu) = 0;
    EF_U8(0x004859dcu) = 0;
    EF_U8(0x004859deu) = 0;
    EF_U8(0x004859ddu) = 0;
    EF_U8(0x00161e21u) = 0xffu;
    guest_fn_f2b7c();
    guest_fn_f3b78();
}

/* 0xf9ed2, 13 bytes. */
void call_f9dcb(uint32_t value)
{
    guest_fn_f9dcb(value, 0x00485f80u);
}

/* 0xf9d98, 51 bytes. Store the two decimal digits of EAX at EBX+EDX. */
void store_two_digits(int32_t value, ef_addr32 base, int32_t offset)
{
    int32_t tens = value / 10;
    int32_t ones = value % 10;
    EF_U8(base + (uint32_t)offset) = (uint8_t)(tens + 0x30);
    EF_U8(base + (uint32_t)offset + 1u) = (uint8_t)(ones + 0x30);
}

/* 0xfa7d2, 49 bytes. */
void update_flags_fa7d2(ef_addr32 file)
{
    uint8_t flags;
    if (EF_U8(file + 0xdu) & 0x20u)
        return;
    if (guest_fn_1101be(EF_U32(file + 0x10u)) == 0)
        return;
    flags = (uint8_t)(EF_U8(file + 0xdu) | 0x20u);
    EF_U8(file + 0xdu) = flags;
    if ((flags & 7u) == 0)
        EF_U8(file + 0xdu) = (uint8_t)(flags | 2u);
}

/* 0xfba54, 24 bytes. 0xfba6c(dword +0x16, dword +0x1a) with EBX = EAX. */
void call_fba6c(uint32_t first, ef_addr32 record)
{
    (void)first;
    guest_fn_fba6c(EF_U32(record + 0x16u), EF_U32(record + 0x1au));
}

/* 0xfddd2, 27 bytes. Map 0xfddb3 to 0xfddb7 and 0xfddc5 to 0xfddcc, else keep. */
uint32_t map_address_fddd2(uint32_t address)
{
    if (address == 0xfddb3u)
        return 0xfddb7u;
    if (address == 0xfddc5u)
        return 0xfddccu;
    return address;
}

/* 0xfef26, 27 bytes. Fill 192 dwords at 0x17cfc0 with -1. */
void fill_table_fef26(void)
{
    uint32_t i;
    for (i = 0; i != 0x300u; i += 4u)
        EF_U32(0x0017cfc0u + i) = 0xffffffffu;
}

/* 0xfc9f8, 37 bytes. */
void update_fc9f8(int32_t value)
{
    if (value == EF_S32(0x00485fa4u))
        return;
    EF_S32(0x00485fa4u) = value;
    guest_fn_c24(EF_S32(0x00485fa6u) >> 16, EF_U32(0x00485fa0u) + (uint32_t)value);
}

/* Debug-trace wrappers: when dword 0x17d2c8 is 1, log through 0xbc400, then forward the cdecl arguments. */

/* 0x101f44, 44 bytes. Trace wrapper for 0x1111ca. */
void trace_call_101f44(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193990u, a);
    guest_fn_1111ca(a);
}

/* 0x101ff4, 44 bytes. Trace wrapper for 0x1112ed. */
void trace_call_101ff4(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001939d4u, a);
    guest_fn_1112ed(a);
}

/* 0x1022dc, 44 bytes. Trace wrapper for 0x11141f. */
void EF_CDECL trace_call_1022dc(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193b24u, a);
    guest_fn_11141f(a);
}

/* 0x102330, 44 bytes. Trace wrapper for 0x111473. */
void EF_CDECL trace_call_102330(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193b54u, a);
    guest_fn_111473(a);
}

/* 0x102384, 44 bytes. Trace wrapper for 0x1113d4. */
void EF_CDECL trace_call_102384(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193b80u, a);
    guest_fn_1113d4(a);
}

/* 0x102488, 44 bytes. Trace wrapper for 0x104464. */
void trace_call_102488(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193bfcu, a);
    guest_fn_104464(a);
}

/* 0x102644, 44 bytes. Trace wrapper for 0x106634. */
void trace_call_102644(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193cc8u, a);
    guest_fn_106634(a);
}

/* 0x102670, 44 bytes. Trace wrapper for 0x106644. */
void trace_call_102670(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193cecu, a);
    guest_fn_106644(a);
}

/* 0x102774, 44 bytes. Trace wrapper for 0x10683c. */
void trace_call_102774(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193d74u, a);
    guest_fn_10683c(a);
}

/* 0x1027a0, 44 bytes. Trace wrapper for 0x10687c. */
void EF_CDECL trace_call_1027a0(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193d8cu, a);
    guest_fn_10687c(a);
}

/* 0x1027cc, 44 bytes. Trace wrapper for 0x106894. */
void trace_call_1027cc(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193da4u, a);
    guest_fn_106894(a);
}

/* 0x1027f8, 44 bytes. Trace wrapper for 0x1068b8. */
void trace_call_1027f8(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193dc0u, a);
    guest_fn_1068b8(a);
}

/* 0x10311c, 44 bytes. Trace wrapper for 0x11402c. */
void trace_call_10311c(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001941fcu, a);
    guest_fn_11402c(a);
}

/* 0x103148, 44 bytes. Trace wrapper for 0x114058. */
void trace_call_103148(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00194218u, a);
    guest_fn_114058(a);
}

/* 0x1031a0, 44 bytes. Trace wrapper for 0x114170. */
void trace_call_1031a0(uint32_t a)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00194250u, a);
    guest_fn_114170(a);
}

/* 0x101ed4, 52 bytes. Trace wrapper for 0x111142. */
void trace_call_101ed4(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193950u, a, b);
    guest_fn_111142(a, b);
}

/* 0x1021f4, 52 bytes. Trace wrapper for 0x1114c7. */
void EF_CDECL trace_call_1021f4(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193aa8u, a, b);
    guest_fn_1114c7(a, b);
}

/* 0x102228, 52 bytes. Trace wrapper for 0x1114f7. */
void EF_CDECL trace_call_102228(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193ac8u, a, b);
    guest_fn_1114f7(a, b);
}

/* 0x10225c, 52 bytes. Trace wrapper for 0x111527. */
void EF_CDECL trace_call_10225c(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193ae8u, a, b);
    guest_fn_111527(a, b);
}

/* 0x102824, 52 bytes. Trace wrapper for 0x10677c. */
void EF_CDECL trace_call_102824(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193dd8u, a, b);
    guest_fn_10677c(a, b);
}

/* 0x102858, 52 bytes. Trace wrapper for 0x10679c. */
void trace_call_102858(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193e00u, a, b);
    guest_fn_10679c(a, b);
}

/* 0x10288c, 52 bytes. Trace wrapper for 0x1067cc. */
void trace_call_10288c(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193e20u, a, b);
    guest_fn_1067cc(a, b);
}

/* 0x1028c0, 52 bytes. Trace wrapper for 0x10682c. */
void EF_CDECL trace_call_1028c0(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193e40u, a, b);
    guest_fn_10682c(a, b);
}

/* 0x102a98, 52 bytes. Trace wrapper for 0x106988. */
void trace_call_102a98(uint32_t a, uint32_t b)
{
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193ef4u, a, b);
    guest_fn_106988(a, b);
}
