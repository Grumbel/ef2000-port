/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <stdarg.h>
#include <string.h>
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Miscellaneous drafted helpers. */

/* 0x28744, 119 bytes. Sibling of 0x287bc: build the same file name, open it with 0xa7948, run 0xab324 with mode 2 and close the handle. */
void open_named_file_28744(void)
{
    uint8_t name[0x98];
    int32_t handle;
    ef_addr32 buf = (uint32_t)(uintptr_t)name;
    if (EF_U16(0x0015ee9au) == 0u)
        guest_sprintf_a4342(buf, 0x001823c0u, guest_fn_9c858(0x001822d8u), 0x00125208u);
    else
        guest_sprintf_a4342(buf, 0x001823c8u, 0x00125208u);
    handle = open_slot_a7948(0u, 0u, 0u, 0u, buf);
    if (handle != -1) {
        guest_fn_ab324((uint32_t)handle, 2u);
        close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
    }
}

/* 0x36100, 124 bytes. (ret 8) Store a request block (0x19f34c.., flag 0x19f5aa) and run 0x35de0; reports errors through the 0x132a74 handler if a request is already pending or dx is above 0x1f6. */
void queue_request_36100(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1)
{
    int32_t v = (int32_t)(int16_t)edx;
    if (EF_U8(0x0019f5aau) != 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00183178u);
    if ((uint32_t)v > 0x1f6u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018319cu);
    EF_U16(0x001357b2u) = (uint16_t)ecx;
    EF_U32(0x0019f34cu) = eax;
    EF_U8(0x0019f5afu) = 2u;
    EF_U32(0x0019f350u) = (uint32_t)v;
    EF_U8(0x0019f5a6u) = (uint8_t)ebx;
    EF_U8(0x0019f5acu) = (uint8_t)s0;
    EF_U8(0x0019f5aau) = 1u;
    EF_U8(0x0019f5adu) = (uint8_t)s1;
    guest_fn_35de0_x(0u, 0u);
}

/* 0xa8660, 129 bytes. (ret 4) Dispatch on the mode byte at [[0x4191d0]+4]: 0 passes the arguments straight on to 0xa8578, 1 mirrors x against 0x13f using a table at 0x418780, 2 offsets x from a table at 0x4187d0. */
void draw_mode_dispatch_a8660(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t index)
{
    uint8_t mode = EF_U8(EF_U32(0x004191d0u) + 4u);
    uint32_t col = ecx & 0xffu;
    if (mode == 0u) {
        draw_text_by_font_a8578((uint32_t)(int32_t)(int16_t)edx, (uint32_t)(int32_t)(int16_t)ebx, col, eax);
    } else if (mode == 1u) {
        uint32_t entry = (index & 0xffff0000u) | EF_U16(0x00418780u + (uint32_t)(int32_t)(int16_t)index * 2u);
        uint32_t x = edx + (0x13fu - entry);
        draw_text_by_font_a8578((uint32_t)(int32_t)(int16_t)x, (uint32_t)(int32_t)(int16_t)ebx, col, eax);
    } else if (mode == 2u) {
        uint16_t x = (uint16_t)(edx + EF_U16(0x004187d0u + (uint32_t)(int32_t)(int16_t)index * 2u));
        draw_text_by_font_a8578((uint32_t)(int32_t)(int16_t)x, (uint32_t)(int32_t)(int16_t)ebx, col, eax);
    }
}

/* 0xc1e3c, 136 bytes. Decode a PackBits-style run-length stream at the pointer 0x41f3a0 into dst: rows (byte 0x146e34) of width 0x146e38; control byte >= 0 copies n+1 literals, negative repeats the next byte -n+1 times (-128 is a no-op). */
void decode_rle_rows_c1e3c(uint32_t dst)
{
    int32_t row;
    for (row = 0; (int32_t)EF_U8(0x00146e34u) > row; row++) {
        int32_t done = 0;
        do {
            uint32_t p = EF_U32(0x0041f3a0u);
            int32_t n = (int32_t)(int8_t)EF_U8(p);
            p++;
            EF_U32(0x0041f3a0u) = p;
            if (n >= 0) {
                n++;
                guest_fn_e9a24((uint32_t)n, p, dst);
                EF_U32(0x0041f3a0u) = EF_U32(0x0041f3a0u) + (uint32_t)n;
                done += n;
                dst += (uint32_t)n;
            } else {
                n = -n;
                if (n < 0x80) {
                    n++;
                    p++;
                    EF_U32(0x0041f3a0u) = p;
                    fill_bytes_b3fb0(dst, EF_U8(p - 1u), (uint32_t)n);
                    done += n;
                    dst += (uint32_t)n;
                }
            }
        } while (done < (int32_t)EF_U32(0x00146e38u));
    }
}

/* 0x6b344, 137 bytes. For unit id, look up its object; if the table entry (0xca bytes at 0x19bb58) of the object's owner still points back at the unit, enable the owner's grid link and slot, otherwise call 0x46254 with the current owner's word at +0x50. */
void unit_owner_update_6b344(uint32_t unit)
{
    int32_t id = (int32_t)(int16_t)unit;
    ef_addr32 obj = lookup_filtered_indexed_object((int16_t)id);
    uint32_t tbl;
    int16_t idx;
    if (obj == 0u)
        return;
    idx = EF_S16(obj + 0x28u);
    tbl = EF_U32(0x0019bb58u);
    if ((uint32_t)EF_U16(tbl + (uint32_t)((int32_t)idx * 0xca) + 0x50u) == (uint32_t)id) {
        set_grid_link_state(idx, 2);
        enable_grid_slot_state(idx);
        enable_grid_slot_activation(idx);
        return;
    }
    {
        int16_t idx2 = EF_S16(EF_U32(0x001f71bcu) + 0x28u);
        int32_t w = (int32_t)EF_S16(tbl + (uint32_t)((int32_t)idx2 * 0xca) + 0x50u);
        guest_fn_46254((uint32_t)id, w, 0x1bu, 0u, 0u, 0u);
    }
}

/* 0xbc4da, 137 bytes. Close a stream record: flush when flagged, release its handle, optionally run 0xfa9a0, free the buffer and remove a temporary file. Returns the accumulated status (-1 for an unused record). */
uint32_t close_stream_record_bc4da(uint32_t rec, uint32_t extra)
{
    uint8_t tmp[0x10];
    uint32_t status = 0u;
    if (EF_U32(rec + 0xcu) == 0u)
        return 0xffffffffu;
    if (EF_U8(rec + 0xdu) & 0x10u)
        status = (uint32_t)stream_flush_fa8a0(rec);
    {
        int32_t pos = file_position_f227f(rec);
        if (pos != -1)
            guest_fn_fa95f(EF_U32(rec + 0x10u), (uint32_t)pos, 0u);
    }
    if (extra != 0u)
        status |= guest_tail_fa9a0(EF_U32(rec + 0x10u));
    if (EF_U8(rec + 0xcu) & 0x8u) {
        guest_fn_fa9d2(EF_U32(rec + 0x8u));
        EF_U32(rec + 0x8u) = 0u;
    }
    if (EF_U8(rec + 0xdu) & 0x8u) {
        make_temp_name_bc471((uint32_t)(uintptr_t)tmp, EF_U8(rec + 0x19u));
        guest_fn_cbc9b_w((uint32_t)(uintptr_t)tmp);
    }
    return status;
}

/* 0x5ca88, 146 bytes. Convert a fixed-point position (words at +0,+4,+8, 8 fractional bits) to be camera relative (origin at 0x21444c/214448/214450); if any component is 0x4000 or more away, scale all three down by 16; add the origin back and hand the truncated 16-bit triple plus esi to 0x15c0c4. */
void project_position_5ca88(uint32_t pos, uint32_t arg)
{
    int32_t x = (EF_S32(pos) >> 8) - EF_S32(0x0021444cu);
    int32_t y = (EF_S32(pos + 4u) >> 8) - EF_S32(0x00214448u);
    int32_t z = (EF_S32(pos + 8u) >> 8) - EF_S32(0x00214450u);
    if (abs_a28f2(x) >= 0x4000 || abs_a28f2(y) >= 0x4000 || abs_a28f2(z) >= 0x4000) {
        x >>= 4;
        y >>= 4;
        z >>= 4;
    }
    x += EF_S32(0x0021444cu);
    y += EF_S32(0x00214448u);
    z += EF_S32(0x00214450u);
    guest_fn_15c0c4_x((uint32_t)(int32_t)(int16_t)z, (uint32_t)(int32_t)(int16_t)y, (uint32_t)(int32_t)(int16_t)x, arg);
}

/* 0x9cc28, 143 bytes. Load the resource with key into buffer: try the two archive contexts at 0x40d1f0/0x40d1f4 (enabled by bits 0 and 1 of 0x132ab0), then fall back to opening the file through the archive lookup. Returns 1 on success. */
uint8_t load_resource_9cc28(uint32_t key, uint32_t buffer)
{
    uint32_t ctx = EF_U32(0x0040d1f0u);
    uint32_t count;
    int32_t handle;
    if (ctx != 0u && (EF_U8(0x00132ab0u) & 1u) && read_entry_block_ebb10(key, buffer, ctx) != 0u)
        return 1u;
    ctx = EF_U32(0x0040d1f4u);
    if (ctx != 0u && (EF_U8(0x00132ab0u) & 2u) && read_entry_block_ebb10(key, buffer, ctx) != 0u)
        return 1u;
    count = lookup_in_archives_9cb70(key);
    if (count == 0u)
        return 0u;
    handle = open_slot_a7900(0u, 0u, 0u, 0u, key);
    if (handle == -1)
        return 0u;
    read_handle_a79d4(0u, 0u, 0u, 0u, (uint32_t)handle, count, buffer);
    close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
    return 1u;
}

/* 0xa9694, 143 bytes. Evaluate up to three variable conditions of a script record (index, operator, value triples at +0, +0xa, +0x14); the first that holds triggers 0xa94e4 on the matching target (+0x1e, +0x22, +0x26) and stops the scan. */
void run_script_conditions_a9694(uint32_t rec)
{
    uint8_t fired = 0u;
    if (compare_variable_a9640(EF_U32(rec), (uint32_t)EF_S16(rec + 4u), EF_S32(rec + 6u)) & 0xffu) {
        guest_fn_a94e4_x(EF_U32(rec + 0x1eu), 1u);
        fired = 1u;
    }
    if (fired == 0u && EF_U32(rec + 0x22u) != 0u) {
        if (compare_variable_a9640(EF_U32(rec + 0xau), (uint32_t)EF_S16(rec + 0xeu), EF_S32(rec + 0x10u)) & 0xffu) {
            /* EDX at the call is the variable value left by 0xa9640. */
            guest_fn_a94e4_x(EF_U32(rec + 0x22u), EF_U32(0x00418708u + EF_U32(rec + 0xau) * 4u));
            fired++;
        }
    }
    if (fired == 0u && EF_U32(rec + 0x26u) != 0u) {
        if (compare_variable_a9640(EF_U32(rec + 0x14u), (uint32_t)EF_S16(rec + 0x18u), EF_S32(rec + 0x1au)) & 0xffu)
            guest_fn_a94e4_x(EF_U32(rec + 0x26u), EF_U32(0x00418708u + EF_U32(rec + 0x14u) * 4u));
    }
}

/* 0x3237c, 150 bytes. Update two running minima (0x19e5c5 with its companion word 0x19e5a5, and 0x19e5b9) from the distances computed by 0x7b16c between pairs of fixed-point values, offset by 0x19e55b. */
void update_distance_minima_3237c(void)
{
    int32_t d = -(int32_t)guest_fn_7b16c_x(0u, (uint32_t)(EF_S32(0x0019e5c1u) >> 8), (uint32_t)(EF_S32(0x0019e5c9u) >> 8));
    int32_t v = (int32_t)((uint32_t)d << 8) + EF_S32(0x0019e55bu);
    if (v < EF_S32(0x0019e5c5u)) {
        int32_t idx = EF_S32(0x0019e547u) >> 16;
        EF_S32(0x0019e5c5u) = v;
        EF_U16(0x0019e5a5u) = EF_U16((uint32_t)(idx * 161) + 0x0019cc33u);
    }
    d = -(int32_t)guest_fn_7b16c_x(0u, (uint32_t)(EF_S32(0x0019e5b5u) >> 8), (uint32_t)(EF_S32(0x0019e5bdu) >> 8));
    v = (int32_t)((uint32_t)d << 8) + EF_S32(0x0019e55bu);
    if (v < EF_S32(0x0019e5b9u))
        EF_S32(0x0019e5b9u) = v;
}

/* 0xc8d30, 183 bytes. (ret 8) Offset a 16.16 vector (eax, edx, ebx) by the table-rotated displacement of length s1: two angles (-ecx and -s0, 16-bit, 8192-entry sin/cos tables at 0x141e08/0x142e08) give the x/z/y deltas; the results are stored as 32-bit values at 0xc4756/0xc475c/0xc4762 and their high words at 0x159094/0x159098/0x15909c. */
void rotate_offset_c8d30(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1)
{
    uint32_t a = ((0u - ecx) & 0xffffu) >> 3;
    uint32_t b = ((0u - s0) & 0xffffu) >> 3;
    uint32_t len = s1 + s1;
    uint32_t dx = (uint32_t)((int32_t)EF_S16(0x00141e08u + a * 2u) * (int32_t)len);
    int32_t t4 = (int32_t)EF_S16(0x00142e08u + a * 2u);
    int32_t cb = (int32_t)EF_S16(0x00141e08u + b * 2u);
    int32_t t0 = (int32_t)EF_S16(0x00142e08u + b * 2u);
    int32_t k = (int32_t)((uint32_t)(t4 * (int32_t)len)) >> 15;
    eax -= (uint32_t)(cb * k);
    edx -= dx;
    ebx -= (uint32_t)(k * t0);
    EF_U32(0x000c475cu) = edx;
    EF_U16(0x00159098u) = (uint16_t)((int32_t)edx >> 16);
    EF_U32(0x000c4756u) = eax;
    EF_U32(0x000c4762u) = ebx;
    EF_U16(0x00159094u) = (uint16_t)((int32_t)eax >> 16);
    EF_U16(0x0015909cu) = (uint16_t)((int32_t)ebx >> 16);
}

/* 0xc8d10, 31 bytes. (ret 8) Wrapper for 0xc8d30 that first shifts eax, edx and ebx left by 16. */
void rotate_offset_shift_c8d10(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1)
{
    rotate_offset_c8d30(eax << 16, edx << 16, ebx << 16, ecx, s0, s1);
}

/* 0xef950, 42 bytes. Combine two 10-byte records (dword, dword, word) at eax and edx with the core routine 0xef9aa and store the 10-byte result at the record in ebx. */
void combine_records_ef950(uint32_t a, uint32_t b, uint32_t out)
{
    guest_fn_ef9aa_x(EF_U32(a), EF_U32(a + 4u), EF_U32(b), EF_U32(b + 4u),
                     ((uint32_t)EF_U16(b + 8u) << 16) | EF_U16(a + 8u), out);
}

/* 0xefbaf, 42 bytes. Same wrapper as 0xef950 around the core routine 0xefbd9. */
void combine_records_efbaf(uint32_t a, uint32_t b, uint32_t out)
{
    guest_fn_efbd9_x(EF_U32(a), EF_U32(a + 4u), EF_U32(b), EF_U32(b + 4u),
                     ((uint32_t)EF_U16(b + 8u) << 16) | EF_U16(a + 8u), out);
}

/* 0xefd84, 42 bytes. Same wrapper as 0xef950 around the core routine 0xefdae. */
void combine_records_efd84(uint32_t a, uint32_t b, uint32_t out)
{
    guest_fn_efdae_x(EF_U32(a), EF_U32(a + 4u), EF_U32(b), EF_U32(b + 4u),
                     ((uint32_t)EF_U16(b + 8u) << 16) | EF_U16(a + 8u), out);
}

/* 0xef97a, 48 bytes. (ret 12) Variant of 0xef950 whose second record comes by value on the stack (dword, dword, word). */
void combine_record_value_ef97a(uint32_t a, uint32_t edx, uint32_t out, uint32_t ecx, uint32_t y0, uint32_t y1, uint32_t y2)
{
    (void)edx;
    (void)ecx;
    guest_fn_ef9aa_x(EF_U32(a), EF_U32(a + 4u), y0, y1, ((y2 & 0xffffu) << 16) | EF_U16(a + 8u), out);
}

/* 0x106d00, 52 bytes. Optionally trace the string through printf (when 0x17d364 is set), then feed each byte to the handler 0x118428 of the object whose first word is at eax. */
void feed_string_106d00(uint32_t obj, uint32_t str)
{
    if (EF_U32(0x0017d364u) != 0u)
        stdout_printf_cd12e(0x00194a60u, str);
    while (EF_U8(str) != 0u) {
        guest_fn_118428(EF_U32(obj), EF_U8(str));
        str++;
    }
}

/* 0x8fdb8, 56 bytes. Map the record type (low 7 bits of the first byte, 14..17) of the record at eax to 0x4a, 0x3a, 0x2e or 0x22; anything else (or null) gives 0. */
uint32_t record_type_size_8fdb8(uint32_t rec)
{
    if (rec == 0u)
        return 0u;
    switch ((uint32_t)(EF_U8(rec) & 0x7fu)) {
    case 0xeu: return 0x4au;
    case 0xfu: return 0x3au;
    case 0x10u: return 0x2eu;
    case 0x11u: return 0x22u;
    default: return 0u;
    }
}

/* 0x1087cd, 53 bytes. Pull characters through the reader at [obj] until one is not in the class-2 skip set (table 0x195af0, indexed by char+1), counting skipped ones; unless bit 1 of obj+0x10 is set, then hand the character to [obj+4]. Returns the skip count. */
uint32_t skip_and_dispatch_1087cd(uint32_t obj)
{
    uint32_t skipped = 0u;
    uint32_t ch;
    for (;;) {
        ch = guest_callptr_eax_ret_x(EF_U32(obj), obj);
        if ((EF_U8(0x00195af0u + ((ch + 1u) & 0xffu)) & 2u) == 0u)
            break;
        skipped++;
    }
    if ((EF_U8(obj + 0x10u) & 2u) == 0u)
        guest_callptr_eaxedx_x(EF_U32(obj + 4u), ch, obj);
    return skipped;
}

/* 0xd6ffc, 65 bytes. Walk a list of record pointers until one whose first byte is zero; for each, release the range (+0x1c, +0x18) when both are set and clear +0x1c. */
void release_record_ranges_d6ffc(uint32_t list)
{
    for (;;) {
        uint32_t rec = EF_U32(list);
        uint32_t base;
        uint32_t length;
        if (EF_U8(rec) == 0u)
            return;
        base = EF_U32(rec + 0x1cu);
        length = EF_U32(rec + 0x18u);
        if (base != 0u && length != 0u) {
            guest_fn_104920(base, length);
            guest_fn_c3f80(EF_U32(EF_U32(list) + 0x1cu));
        }
        rec = EF_U32(list);
        list += 4u;
        EF_U32(rec + 0x1cu) = 0u;
    }
}

/* 0x9cee8, 68 bytes. Seek: when flags carry bit 16 or 17 use the archive context at 0x40d1f0 (bit 16) or 0x40d1f4 and advance its cursor by amount with the remaining flags; otherwise fall back to the plain 0xa7ac8 seek. */
uint32_t seek_context_9cee8(uint32_t amount, uint32_t flags)
{
    if ((flags & 0x30000u) != 0u) {
        uint32_t rest = flags & 0xfffcffffu;
        uint32_t ctx = (flags & 0x10000u) ? EF_U32(0x0040d1f0u) : EF_U32(0x0040d1f4u);
        return advance_cursor_ebcfc(amount, rest, ctx);
    }
    return seek_current_a7ac8(0u, 0u, 0u, 0u, flags, amount);
}

/* 0x97914, 59 bytes. Unless 0x122c5c is set, copy the vector at obj+0x30 to a temporary, scale it by the float bits at 0x132b20 and add it to the vector at obj+0x24. */
void add_scaled_vector_97914(uint32_t obj)
{
    uint8_t tmp[12];
    ef_addr32 t = (uint32_t)(uintptr_t)tmp;
    if (EF_U8(0x00122c5cu) != 0u)
        return;
    copy_vector3(t, obj + 0x30u);
    guest_fn_ea8cc_x(t, 0u, 0u, 0u, EF_U32(0x00132b20u));
    add_vector3(obj + 0x24u, t);
}

/* 0x8ba98, 60 bytes. Walk a list of length-prefixed records (word length at +0, zero ends); type byte at +2 of 0x11 links an effect via 0x8ba30 and 0x12 latches the mode word via 0x8b5f0. */
void walk_effect_records_8ba98(uint32_t rec)
{
    for (;;) {
        int16_t len = (int16_t)EF_U16(rec);
        uint8_t type;
        if (len == 0)
            return;
        type = (uint8_t)(EF_U8(rec + 2u) - 0xeu);
        rec += 2u;
        if (type == 3u)
            link_effect_8ba30(rec);
        else if (type == 4u)
            latch_mode_word_8b5f0(rec);
        rec += (uint32_t)(int32_t)len;
    }
}

/* 0x8bad4, 68 bytes. Build the default path with suffix 0x18a86c, append the two names, look up the UI record for it and process its effect list. */
void load_effect_list_8bad4(uint32_t first, uint32_t second)
{
    uint8_t buf[0x78];
    ef_addr32 b = (uint32_t)(uintptr_t)buf;
    ef_addr32 rec;
    guest_fn_9c980_sfx(b, 0x0018a86cu);
    guest_fn_9ca57(b, first);
    guest_fn_9ca57(b, second);
    rec = select_validated_ui_record(b);
    if (rec != 0u)
        walk_effect_records_8ba98(rec);
}

/* 0xa7dac, 70 bytes. Map the mode byte at [[0x4191d0]+0x10] (0..9) to a code through a jump table; out of range gives 0. */
uint32_t mode_code_a7dac(void)
{
    static const uint8_t codes[10] = { 0xd, 9, 6, 0x10, 0xc, 7, 7, 0x12, 9, 7 };
    uint8_t mode = EF_U8(EF_U32(0x004191d0u) + 0x10u);
    return mode <= 9u ? codes[mode] : 0u;
}

/* 0x82b6c, 80 bytes. sprintf(buf, format 0x188bd8, seven state bytes and the byte at 0x15b7e6). */
void format_state_bytes_82b6c(uint32_t buf)
{
    guest_sprintf_a4342(buf, 0x00188bd8u,
                        (uint32_t)EF_U8(0x0012854bu), (uint32_t)EF_U8(0x001a363eu),
                        (uint32_t)EF_U8(0x001a3636u), (uint32_t)EF_U8(0x001a3640u),
                        (uint32_t)EF_U8(0x001a3639u), (uint32_t)EF_U8(0x001a3641u),
                        (uint32_t)EF_U8(0x0015b7e6u));
}

/* 0x80374, 80 bytes. Create two entries with 0xd7764 (the first with the caller's EAX and parameters 1, 1, 0x5622, 0x7f, 1, 1; the second for the string 0x18878c), store them at 0x1f9020/0x1f9024, set 0x12b0f4 and process the list at 0x1f9020. */
void init_sound_entries_80374(uint32_t eax)
{
    EF_U8(0x0012b0f4u) = 1u;
    EF_U32(0x001f9020u) = guest_fn_d7764_7(eax, 1u, 1u, 0x5622u, 0x7fu, 1u, 1u);
    EF_U32(0x001f9024u) = guest_fn_d7764_7(0x0018878cu, 0u, 0u, 0u, 0u, 0u, 0u);
    guest_fn_d6f90(0x001f9020u, 0u, 0u);
}

/* 0x362c4, 86 bytes. Look up the widget named 0x1830f8 and enable it with a label chosen by the state byte 0x19f5aa (2..7); other states clear the children dirty flags instead. */
void enable_state_widget_362c4(void)
{
    static const uint32_t labels[6] = { 0x1831c0u, 0x1831d0u, 0x1831c8u, 0x1831c0u, 0x1831c8u, 0x1831d0u };
    ef_addr32 widget = find_named_or_abort_a46cc(0x001830f8u);
    uint8_t idx;
    if (widget == 0u)
        return;
    idx = (uint8_t)(EF_U8(0x0019f5aau) - 2u);
    if (idx > 5u) {
        clear_children_dirty(widget);
        return;
    }
    guest_fn_a7318_2_x(widget, labels[idx]);
}

/* 0x101688, 85 bytes. Run a short hardware delay loop: n pulses of the two callbacks at 0x486da4/0x486da0, one for the low bit and two per remaining half step. */
void pulse_delay_101688(int32_t n)
{
    if (n & 1) {
        guest_callptr0_x(EF_U32(0x00486da4u));
        guest_callptr0_x(EF_U32(0x00486da0u));
    }
    n >>= 1;
    if (n != 0) {
        do {
            guest_callptr0_x(EF_U32(0x00486da4u));
            guest_callptr0_x(EF_U32(0x00486da0u));
            guest_callptr0_x(EF_U32(0x00486da4u));
            n--;
            guest_callptr0_x(EF_U32(0x00486da0u));
        } while (n > 0);
    }
}

/* 0x7ff0c, 84 bytes. (ret 12) Create an entry through 0xd7764 with the caller's EAX/EDX/EBX/ECX and three stack arguments and append it to the table at 0x1f9078 (count at 0x12b090, capacity 0x12b0b4); reports an error through 0x132a74 when full. */
void append_entry_7ff0c(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1, uint32_t s2)
{
    uint32_t entry;
    uint32_t count;
    if (EF_U32(0x0012b090u) == EF_U32(0x0012b0b4u))
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00188790u);
    entry = guest_fn_d7764_7(eax, edx, ebx, ecx, s0, s1, s2);
    count = EF_U32(0x0012b090u);
    EF_U32(EF_U32(0x001f9078u) + count * 4u) = entry;
    EF_U32(0x0012b090u) = count + 1u;
}

/* 0xab060, 80 bytes. Find the node at 0x4191d4 in the group lists and write its two indices (words from a zeroed pair) plus one byte at 0x41955f to the handle in eax. */
void write_node_position_ab060(uint32_t handle)
{
    uint32_t pos[2];
    ef_addr32 p = (uint32_t)(uintptr_t)pos;
    pos[0] = 0u;
    pos[1] = 0u;
    find_node_position_aafe8(p, p + 4u, EF_U32(0x004191d4u));
    write_handle_a7a1c(0u, 0u, 0u, 0u, handle, 2u, p);
    write_handle_a7a1c(0u, 0u, 0u, 0u, handle, 2u, p + 4u);
    write_handle_a7a1c(0u, 0u, 0u, 0u, handle, 1u, 0x0041955fu);
}

/* 0x35220, 92 bytes. Build a 3-byte request (the key byte plus a word: 0x19f592 - 1 for key 0x48, else 0xffff), mirror the word to 0x19f58c, set 0x1357b2 to 3, send it with 0xa3eac (size 0x13) and set 0x19f599. */
void send_key_request_35220(uint32_t key)
{
    uint8_t req[0x14];
    uint16_t word;
    EF_U16(0x001357b2u) = 3u;
    if ((key & 0xffu) == 0x48u)
        word = (uint16_t)(EF_U16(0x0019f592u) - 1u);
    else
        word = 0xffffu;
    EF_U16(0x0019f58cu) = word;
    req[0] = (uint8_t)key;
    req[1] = (uint8_t)word;
    req[2] = (uint8_t)(word >> 8);
    issue_request_ready_a3eac((uint32_t)(uintptr_t)req, 0x13u);
    EF_U8(0x0019f599u) = 1u;
}

/* 0x7ca78, 92 bytes. Walk the widget chain (next at +0x21) starting at the widget found under 0x187f38/0x187dfc; the first n entries (n = 0x12b054 >> 16) are shown with the 0x4c-byte record at 0x1f8f74 + 0x4c * i, the rest get state 1. */
void update_widget_chain_7ca78(void)
{
    uint32_t node = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00187f38u), 0x00187dfcu);
    uint32_t i = 0u;
    uint32_t off = 0u;
    while (node != 0u) {
        if ((int32_t)i < (EF_S32(0x0012b054u) >> 16))
            guest_fn_a7588_v(node, EF_U32(0x001f8f74u) + off);
        else
            guest_fn_a739c(node, 1u);
        off += 0x4cu;
        node = EF_U32(node + 0x21u);
        i++;
    }
}

/* 0xc9ecc, 92 bytes. Set the view origin to the screen size (0x15b744 x 0x15b748) minus one with the half-size corner, and latch the full rectangle twice. */
void init_view_corners_c9ecc(void)
{
    int32_t w = EF_S32(0x0015b744u);
    int32_t h = EF_S32(0x0015b748u);
    int32_t hx = w / 2 - 1;
    int32_t hy = h / 2 - 1;
    int32_t x = EF_S32(0x0015b744u) - 1;
    int32_t y = EF_S32(0x0015b748u) - 1;
    set_view_origin_dc21c((uint32_t)y, (uint32_t)x, (uint32_t)hy, (uint32_t)hx);
    guest_fn_ce328_w(0, x, 0, y);
    guest_fn_cdf48(0u, (uint32_t)x, 0u, (uint32_t)y);
}

/* 0x118428, 98 bytes. Queue byte ch into the ring buffer of an open device object (buffer at +4, head word at +2, tail word at +4 of the record at [obj], size word at +6): returns 1 on success or when the object is closed, 0 when full. */
uint32_t queue_byte_118428(uint32_t obj, uint32_t ch)
{
    uint32_t q;
    int16_t head;
    if (EF_U32(obj + 0x1cu) == 0u)
        return 1u;
    q = EF_U32(obj);
    if (EF_U16(q + 4u) == EF_U16(q + 6u))
        return 0u;
    disable_interrupts_11f2d3();
    head = (int16_t)EF_U16(q + 2u);
    EF_U16(q + 2u) = (uint16_t)(EF_U16(q + 2u) + 1u);
    EF_U8(EF_U32(obj + 4u) + (uint32_t)(int32_t)head) = (uint8_t)ch;
    if (EF_U16(q + 2u) == EF_U16(q + 6u))
        EF_U16(q + 2u) = 0u;
    EF_U16(q + 4u) = (uint16_t)(EF_U16(q + 4u) + 1u);
    guest_fn_117b18_x(obj);
    enable_interrupts_11f2d5();
    return 1u;
}

/* 0x11d322, 99 bytes. Lay out a decimal digit string as "intdigits.fracdigits" at out: copy up to intdigits digits from src, pad with zeros, add the point, pad leading fractional zeros when intdigits is negative, then copy and zero-pad frac digits. Returns the end pointer. */
uint32_t layout_digits_11d322(uint32_t out, uint32_t src, int32_t intdigits, int32_t frac)
{
    int32_t n = intdigits;
    if (frac == 0 && intdigits <= 0) {
        EF_U8(out++) = '0';
        EF_U8(out++) = '.';
        return out;
    }
    while (n > 0) {
        uint8_t c = EF_U8(src);
        if (c == 0u)
            break;
        src++;
        n--;
        EF_U8(out++) = c;
    }
    while (n > 0) {
        n--;
        EF_U8(out++) = '0';
    }
    EF_U8(out++) = '.';
    while (frac > 0 && n != 0) {
        n++;
        EF_U8(out++) = '0';
        frac--;
    }
    while (frac > 0) {
        uint8_t c = EF_U8(src);
        if (c == 0u)
            break;
        src++;
        EF_U8(out++) = c;
        frac--;
    }
    while (frac != 0) {
        frac--;
        EF_U8(out++) = '0';
    }
    return out;
}

/* 0x11d385, 107 bytes. Right-align the len-byte text at buf inside width bytes: move it to the end, then fix a leading "." or sign+"." by inserting a zero, and fill the freed leading bytes with spaces. */
void right_align_number_11d385(uint32_t buf, int32_t len, int32_t width)
{
    uint8_t first;
    uint8_t second;
    if (len == width)
        return;
    second = EF_U8(buf + 1u);
    {
        uint32_t src = buf + (uint32_t)len;
        uint32_t dst = buf + (uint32_t)width;
        do {
            src--;
            dst--;
            EF_U8(dst) = EF_U8(src);
            width--;
        } while (src != buf);
    }
    first = EF_U8(buf);
    if (first == '.') {
        width--;
        EF_U8(buf + (uint32_t)width) = '0';
    } else if ((first == '+' || first == '-') && second == '.') {
        EF_U8(buf + (uint32_t)width) = '0';
        width--;
        EF_U8(buf + (uint32_t)width) = EF_U8(buf);
    }
    fill_bytes_b3fb0(buf, 0x20u, (uint32_t)width);
}

/* 0x11451c, 110 bytes. Advance a (column, row) cursor of a text object by 0x4dd804 steps: each step adds +0x68 to the accumulator at +0x6c and carries into the column (+0x5c) and row (+0x60) against the limits at +0x70 and +0x64. Stores row and column through the two pointers. */
void advance_text_cursor_11451c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t obj, uint32_t out_col, uint32_t out_row)
{
    int32_t col;
    int32_t row;
    int32_t acc;
    int32_t i;
    uint32_t rec;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (obj == 0u)
        return;
    rec = EF_U32(obj);
    EF_S32(rec + 0x14u) = EF_S32(rec + 0x14u) + 1;
    col = EF_S32(obj + 0x5cu);
    row = EF_S32(obj + 0x60u);
    acc = EF_S32(obj + 0x6cu);
    if (EF_S32(0x004dd804u) > 0) {
        i = 0;
        do {
            acc += EF_S32(obj + 0x68u);
            if (acc >= EF_S32(obj + 0x70u)) {
                col++;
                acc -= EF_S32(obj + 0x70u);
                if (col >= EF_S32(obj + 0x64u)) {
                    row++;
                    col = 0;
                }
            }
            i++;
        } while (i < EF_S32(0x004dd804u));
    }
    if (row < 0)
        row = 0;
    if (out_row != 0u)
        EF_S32(out_row) = row;
    if (out_col != 0u)
        EF_S32(out_col) = col;
    rec = EF_U32(obj);
    EF_S32(rec + 0x14u) = EF_S32(rec + 0x14u) - 1;
}

/* 0xa6038, 111 bytes. Redraw: when a backdrop (0x41eeac) is active draw it over the screen rectangle, otherwise run 0xc9ec4; then re-apply each entry of the word list at 0x41eda8 (count 0x41eed8) through 0xa5ef8. */
void redraw_entries_a6038(void)
{
    uint16_t i;
    uint16_t count;
    if (EF_U32(0x0041eeacu) != 0u && EF_U8(0x0041ef06u) != 0u)
        put_screen_rows_eb050(0u, 0u, 0u, 0u, 0u, 0u, EF_U16(0x0015b744u), EF_U16(0x0015b748u), EF_U32(0x0041eeacu));
    else
        tail_jump_c9ec4();
    count = EF_U16(0x0041eed8u);
    if (count != 0u) {
        i = 0u;
        do {
            guest_fn_a5ef8((uint32_t)EF_U16(0x0041eda8u + (uint32_t)i * 2u));
            i++;
        } while (i < EF_U16(0x0041eed8u));
    }
}

/* 0x97950, 118 bytes. Integrate a record's motion unless 0x122c5c is set: add the vector at +0x18 (scaled by the step 0x132b20) to the position at +0x0c, then fold the vector scaled by +0x48 and offset by +0x3c back into +0x18 (also scaled by the step). */
void integrate_motion_97950(uint32_t obj)
{
    uint8_t tmp[12];
    ef_addr32 t = (uint32_t)(uintptr_t)tmp;
    if (EF_U8(0x00122c5cu) != 0u)
        return;
    copy_vector3(t, obj + 0x18u);
    guest_fn_ea8cc_x(t, 0u, 0u, 0u, EF_U32(0x00132b20u));
    add_vector3(obj + 0xcu, t);
    copy_vector3(t, obj + 0x18u);
    guest_fn_ea8cc_x(t, 0u, 0u, 0u, EF_U32(obj + 0x48u));
    add_vector3(t, obj + 0x3cu);
    guest_fn_ea8cc_x(t, 0u, 0u, 0u, EF_U32(0x00132b20u));
    add_vector3(obj + 0x18u, t);
}

/* 0xa374c, 118 bytes. When at least 8 bytes are pending (0x1357da), send a 13-byte request built from 0x1357ae, 7 bytes of the template at [0x13587c + 32 * code + 1], the pending count and the word at 0x1357ec, then clear both pending values. */
void flush_pending_a374c(uint32_t code)
{
    uint8_t req[0x10];
    if ((uint32_t)EF_U16(0x001357dau) < 8u)
        return;
    {
        uint32_t src = EF_U32(0x0013587cu + ((code & 0xffu) << 5));
        uint16_t w1 = EF_U16(src + 1u);
        uint32_t d3 = EF_U32(src + 3u);
        uint16_t pend = EF_U16(0x001357dau);
        uint32_t d9 = EF_U32(0x001357ecu);
        req[0] = EF_U8(0x001357aeu);
        req[1] = (uint8_t)w1;
        req[2] = (uint8_t)(w1 >> 8);
        req[3] = (uint8_t)d3;
        req[4] = (uint8_t)(d3 >> 8);
        req[5] = (uint8_t)(d3 >> 16);
        req[6] = (uint8_t)(d3 >> 24);
        req[7] = (uint8_t)pend;
        req[8] = (uint8_t)(pend >> 8);
        req[9] = (uint8_t)d9;
        req[10] = (uint8_t)(d9 >> 8);
        req[11] = (uint8_t)(d9 >> 16);
        req[12] = (uint8_t)(d9 >> 24);
    }
    issue_request_a3e50((uint32_t)(uintptr_t)req, 0xdu, 7u);
    EF_U16(0x001357dau) = 0u;
    EF_U32(0x001357ecu) = 0u;
}

/* 0xd7764, 118 bytes. (ret 12) Allocate and clear a 0x25-byte entry, copy the name from EAX into it and fill in the bytes/words at +0x0d, +0x0e, +0x0f, +0x11, +0x13 and +0x17 from EDX, EBX, ECX and the three stack arguments. Reports an error through 0x132a74 when allocation fails. */
uint32_t create_entry_d7764(uint32_t name, uint32_t b13, uint32_t b14, uint32_t w15, uint32_t w17, uint32_t d19, uint32_t b23)
{
    uint32_t rec = guest_fn_fc795(0x25u);
    if (rec == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018fa70u);
    fill_bytes_b3fb0(rec, 0u, 0x25u);
    copy_string_9d749(rec, name);
    EF_U16(rec + 0xfu) = (uint16_t)w15;
    EF_U8(rec + 0xdu) = (uint8_t)b13;
    EF_U8(rec + 0xeu) = (uint8_t)b14;
    EF_U16(rec + 0x11u) = (uint16_t)w17;
    EF_U32(rec + 0x13u) = d19;
    EF_U8(rec + 0x17u) = (uint8_t)b23;
    return rec;
}

/* 0xdd2f4, 122 bytes. Measure the pixel width of a zero-terminated string: spaces and tabs count 3, characters above 0x7f use the byte table at 0x15ea20 (+1), others index through 0x15e1bb/0x15e31d (+1). 0xa5 is measured as 'N'. */
uint32_t text_width_dd2f4(uint32_t str)
{
    uint32_t width = 0u;
    int16_t i = 0;
    for (;; i++) {
        uint8_t c = EF_U8(str + (uint32_t)(int32_t)i);
        if (c == 0u)
            return width;
        if (c == 0xa5u)
            c = 0x4eu;
        if (c == 0x20u || c == 9u) {
            width += 3u;
            continue;
        }
        if (c > 0x7fu) {
            width += EF_U8(0x0015ea20u + (uint32_t)(uint8_t)(c - 0x80u)) + 1u;
        } else {
            int32_t k = (int32_t)(int8_t)EF_U8(0x0015e1bbu + (uint32_t)(c & 0x3fu) + 3u);
            width += EF_U8(0x0015e31du + (uint32_t)k) + 1u;
        }
    }
}

/* 0x5cb1c, 126 bytes. Variant of 0x5ca88 taking the position in EDX/EBX/ECX (fixed point already shifted) and the extra argument in EAX: make it camera relative, scale down by 16 until all components are below 0x4000, add the origin back and call 0x15c0c4. */
void project_position_5cb1c(uint32_t arg, int32_t x, int32_t y, int32_t z)
{
    x -= EF_S32(0x0021444cu);
    y -= EF_S32(0x00214448u);
    z -= EF_S32(0x00214450u);
    while (abs_a28f2(x) >= 0x4000 || abs_a28f2(y) >= 0x4000 || abs_a28f2(z) >= 0x4000) {
        x >>= 4;
        y >>= 4;
        z >>= 4;
    }
    x += EF_S32(0x0021444cu);
    y += EF_S32(0x00214448u);
    z += EF_S32(0x00214450u);
    guest_fn_15c0c4_x((uint32_t)(int32_t)(int16_t)z, (uint32_t)(int32_t)(int16_t)y, (uint32_t)(int32_t)(int16_t)x, arg);
}

/* 0x66b74, 126 bytes. Return 1 when the metric distance (0x23760) from the 8.8 fixed-point position at pos to the reference point is below the radius stored in the table at 0x12bca3 (145-byte rows) for the current unit's selected entry. */
uint8_t within_unit_radius_66b74(uint32_t pos)
{
    uint32_t unit = lookup_filtered_indexed_object((int16_t)(EF_S32(0x001f71c8u) >> 16));
    uint8_t vec[12];
    ef_addr32 v = (uint32_t)(uintptr_t)vec;
    int32_t sel;
    int32_t slot;
    int32_t radius;
    if (unit == 0u)
        return 0u;
    sel = EF_S32(unit + 0x1d4u) >> 24;
    slot = EF_S32(unit + (uint32_t)(sel * 3) + 0x198u) >> 24;
    radius = EF_S32(0x0012bca3u + (uint32_t)(slot * 145));
    EF_S32(v) = EF_S32(pos) >> 8;
    EF_S32(v + 4u) = EF_S32(pos + 4u) >> 8;
    EF_S32(v + 8u) = EF_S32(pos + 8u) >> 8;
    return guest_fn_23760_x(v) < radius ? 1u : 0u;
}

/* 0x40624, 125 bytes. Latch the full screen rectangle twice, optionally format "ratio" text (50 / 0x127d92) and print it at row 12, column 10, y 30. */
void draw_ratio_text_40624(void)
{
    uint8_t buf[0x64];
    uint16_t d;
    int32_t w = EF_S32(0x0015b744u) - 1;
    int32_t h = EF_S32(0x0015b748u) - 1;
    guest_fn_ce328_w(0, w, 0, h);
    w = EF_S32(0x0015b744u) - 1;
    h = EF_S32(0x0015b748u) - 1;
    guest_fn_cdf48(0u, (uint32_t)w, 0u, (uint32_t)h);
    d = EF_U16(0x00127d92u);
    if (d != 0u)
        guest_sprintf_a4342((uint32_t)(uintptr_t)buf, 0x001842d4u, (uint32_t)d, (uint32_t)(50 / (int32_t)d));
    /* ORIGINAL BUG: prints the uninitialised buffer when 0x127d92 is zero. */
    guest_fn_48c90(12u, 10u, 0x1eu, (uint32_t)(uintptr_t)buf);
}

/* 0x286c0, 131 bytes. Sibling of 0x28744: build the file name (two formats chosen by 0x15ee9a), open it with 0xa7948, write the 2-byte value 0x1736 and close the handle. */
void write_marker_file_286c0(void)
{
    uint8_t name[0x98];
    uint32_t value = 0x1736u;
    int32_t handle;
    ef_addr32 buf = (uint32_t)(uintptr_t)name;
    if (EF_U16(0x0015ee9au) == 0u)
        guest_sprintf_a4342(buf, 0x00182344u, guest_fn_9c858(0x001822d8u));
    else
        guest_sprintf_a4342(buf, 0x00182350u);
    handle = open_slot_a7948(0u, 0u, 0u, 0u, buf);
    if (handle != -1) {
        write_handle_a7a1c(0u, 0u, 0u, 0u, (uint32_t)handle, 2u, (uint32_t)(uintptr_t)&value);
        close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
    }
}

/* 0xa499c, 130 bytes. Variant of 0xa4a94 that appends the entry (0xa479c) and rewrites every entry owned by id via 0xa48ac, then refreshes the entities unless flag 2 is set. */
void append_entry_and_children_a499c(uint32_t id)
{
    uint32_t i;
    uint32_t rec;
    id &= 0xffffu;
    if (id == 0u || id > EF_U16(0x0041eed8u))
        return;
    rec = EF_U32(0x0041edfcu + id * 4u);
    if (rec == 0u || (EF_U8(rec + 0x3bu) & 8u) == 0u)
        return;
    append_id_a479c((uint16_t)id);
    for (i = 1u; (int32_t)i <= (int32_t)EF_U16(0x0041eed8u); i++) {
        if (EF_U8(EF_U32(0x0041edfcu + i * 4u) + 0x11u) == id)
            guest_fn_a48ac_x(i & 0xffffu);
    }
    if ((EF_U8(rec + 0x3bu) & 2u) == 0u)
        update_entities_a5a68();
}

/* 0xf4bdc, 131 bytes. Reset the command parser state (0x485dfc = 3, 0x485f70/71 = 0), read tokens and run handlers until 0x485e08 clears, report errors for the two missing flags and set 0x485f75. */
void run_command_loop_f4bdc(void)
{
    EF_U16(0x00485dfcu) = 3u;
    EF_U8(0x00485f70u) = 0u;
    EF_U8(0x00485f71u) = 0u;
    guest_fn_f46cc();
    if (EF_U8(0x00485e08u) != 0u) {
        do {
            run_matching_handler_f478c();
            guest_fn_f46cc();
        } while (EF_U8(0x00485e08u) != 0u);
    }
    if (EF_U8(0x00485f70u) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00191f44u, EF_U32(0x00161ea8u));
    if (EF_U8(0x00485f71u) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00191f74u, EF_U32(0x00161ea8u));
    EF_U8(0x00485f75u) = 1u;
}

/* 0x36340, 132 bytes. Enable the widget named 0x182fb0 with a label picked from the state byte 0x19f5a9 (2..10): 0x1831dc/0x1831e8 (by 0x19f598), 0x1831f4, or clear the children dirty flags. */
void enable_state_widget_36340(void)
{
    ef_addr32 widget = find_named_or_abort_a46cc(0x00182fb0u);
    uint8_t idx;
    if (widget == 0u)
        return;
    idx = (uint8_t)(EF_U8(0x0019f5a9u) - 2u);
    switch (idx <= 8u ? idx : 3u) {
    case 0: case 2: case 4: case 6:
        guest_fn_a7318_2_x(widget, EF_U8(0x0019f598u) != 0u ? 0x001831dcu : 0x001831e8u);
        return;
    case 1: case 5:
        guest_fn_a7318_2_x(widget, 0x001831f4u);
        return;
    case 8:
        if (EF_U8(0x0019f599u) != 0u) {
            guest_fn_a7318_2_x(widget, 0x001831f4u);
            return;
        }
        clear_children_dirty(widget);
        return;
    default:
        clear_children_dirty(widget);
        return;
    }
}

/* 0x48df8, 132 bytes. Clamp the counter 0x127b1c at zero; while it is non-zero draw the two status strings (0x1a524c) at (17, 17) with modes 0 and 6 selected by the toggle 0x12b369, then subtract the step 0x127d92. */
void draw_status_counter_48df8(void)
{
    uint8_t mode;
    if ((int16_t)EF_U16(0x00127b1cu) < 0)
        EF_U16(0x00127b1cu) = 0u;
    if (EF_U16(0x00127b1cu) == 0u)
        return;
    mode = (uint8_t)(1u - EF_U8(0x0012b369u));
    draw_formatted_text_mode_48bb0(0u, mode, 0x11, 0x11, EF_U32(0x001a524cu));
    mode = (uint8_t)(1u - EF_U8(0x0012b369u));
    draw_formatted_text_mode_48bb0(6u, mode, 0x11, 0x11, EF_U32(0x001a524cu));
    EF_U16(0x00127b1cu) = (uint16_t)(EF_U16(0x00127b1cu) - EF_U16(0x00127d92u));
}

/* 0x97860, 135 bytes. Step the animation record: bump its frame word (+6), restart it through 0x975f0 when the frame reaches the limit table entry 0x12dcb8[kind], then act on the kind word (+8): 1 restarts with kind 3 when the object faces the camera, 2 refreshes and applies 0x9775c, 3 and 4 apply 0x9775c. Returns the result of the final call or 0. */
uint32_t step_animation_97860(uint32_t obj)
{
    int16_t frame = (int16_t)EF_U16(obj + 6u);
    int32_t limit = EF_S32(0x0012dcb8u + (uint32_t)((int32_t)(int16_t)EF_U16(obj + 8u)) * 4u);
    uint32_t result = 0u;
    EF_U16(obj + 6u) = (uint16_t)(frame + 1);
    if ((int32_t)frame >= limit)
        guest_fn_975f0_x(obj);
    switch ((uint16_t)(EF_U16(obj + 8u) - 1u)) {
    case 0:
        if (object_faces_camera_97534(obj) != 0u)
            result = guest_fn_9783c_ret_x(obj, 3u);
        break;
    case 1:
        if (object_faces_camera_97534(obj) != 0u)
            guest_fn_975f0_x(obj);
        result = guest_fn_9775c_x(obj);
        break;
    case 2:
    case 3:
        result = guest_fn_9775c_x(obj);
        break;
    default:
        break;
    }
    return result;
}

/* 0x1046c4, 135 bytes. When 0x17d2d4 is set, run 0x1048b4 over the range (0x103d80, 0x1046c4), release the six runtime buffers (0x4dd670, 0x4dd7cc, 0x4dd6b0, 0x4dd7b0, 0x17d2d0, 0x4dd7c8) and clear the flag. */
void release_runtime_buffers_1046c4(void)
{
    if (EF_U32(0x0017d2d4u) == 0u)
        return;
    guest_fn_1048b4(0x00103d80u, 0x001046c4u);
    guest_fn_104920(0x004dd670u, 0x40u);
    guest_fn_104920(0x004dd7ccu, 0x4cu);
    guest_fn_104920(0x004dd6b0u, 0x100u);
    guest_fn_104920(0x004dd7b0u, 0x18u);
    guest_fn_104920(0x0017d2d0u, 4u);
    guest_fn_104920(0x004dd7c8u, 4u);
    EF_U32(0x0017d2d4u) = 0u;
}

/* Draw a glyph bitmap: rows of mask bytes starting at tbl, one pixel per set bit (starting at bit first, going down), written as colour to the row start addresses of the screen buffer. */
static void draw_glyph_rows(uint32_t colour, uint32_t x, uint32_t y, uint32_t tbl, uint32_t rows, uint32_t width, uint32_t first_bit)
{
    uint32_t stride = EF_U32(0x001631f4u);
    uint32_t p = EF_U32(0x0015b754u) + EF_U32(0x00163200u + y * 4u) + x;
    uint32_t r;
    uint32_t b;
    for (r = 0u; r < rows; r++) {
        uint8_t mask = EF_U8(tbl++);
        for (b = 0u; b < width; b++) {
            if (mask & (first_bit >> b))
                EF_U8(p + b) = (uint8_t)colour;
        }
        p += stride;
    }
    EF_U32(0x001631f4u) = stride;
}

/* 0xeaf80, 136 bytes. Draw a 6x5 glyph (index ecx, 5 mask bytes at 0x160648) in colour al at (x = edx, y row = ebx). */
void draw_glyph_6x5_eaf80(uint32_t colour, uint32_t x, uint32_t y, uint32_t glyph)
{
    draw_glyph_rows(colour, x, y, 0x00160648u + glyph * 5u, 5u, 6u, 0x20u);
}

/* 0xf2960, 138 bytes. Draw a 6x6 glyph (index ecx, 6 mask bytes at 0x16116c) in colour al at (x = edx, y row = ebx). */
void draw_glyph_6x6_f2960(uint32_t colour, uint32_t x, uint32_t y, uint32_t glyph)
{
    draw_glyph_rows(colour, x, y, 0x0016116cu + glyph * 6u, 6u, 6u, 0x80u);
}

/* 0xc38cc, 137 bytes. Copy a planar (4 interleaved planes) image into the screen buffer at 0x146e40: for each plane, every row, every fourth pixel starting at the plane number. */
void unpack_planes_c38cc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t src)
{
    int32_t w = EF_S32(0x0015b744u);
    int32_t plane;
    (void)u0; (void)u1; (void)u2; (void)u3;
    for (plane = 0; plane < 4; plane++) {
        int32_t row;
        if (EF_S32(0x0015b748u) <= 0)
            continue;
        row = 0;
        do {
            int32_t x = plane;
            if (w > x) {
                do {
                    EF_U8(EF_U32(0x00146e40u) + EF_U32(0x00163200u + (uint32_t)row * 4u) + (uint32_t)x) = EF_U8(src++);
                    x += 4;
                } while (x < w);
            }
            row++;
        } while (row < EF_S32(0x0015b748u));
    }
    EF_S32(0x0015b744u) = w;
}

/* 0x767f4, 136 bytes. Reset the 256-entry colour tables: fill 0x1f71e0 with (0x3f, 0, 0x3f), zero the first entry at 0x464060, copy the 256 triples at 0x464060 to 0x1f74e0, set 0x1f88fa to 0x32, 0x425b9c to 0x425b90 - 0x425b94, then expand the table. */
void reset_colour_tables_767f4(void)
{
    int16_t i;
    EF_U8(0x001f88fau) = 0x32u;
    EF_S32(0x00425b9cu) = EF_S32(0x00425b90u) - EF_S32(0x00425b94u);
    EF_U8(0x00464060u) = 0u;
    EF_U8(0x00464061u) = 0u;
    EF_U8(0x00464062u) = 0u;
    for (i = 0; i < 0x100; i++) {
        uint32_t o = (uint32_t)i * 3u;
        EF_U8(0x001f71e0u + o) = 0x3fu;
        EF_U8(0x001f71e1u + o) = 0u;
        EF_U8(0x001f71e2u + o) = 0x3fu;
    }
    for (i = 0; i < 0x100; i++) {
        uint32_t o = (uint32_t)i * 3u;
        EF_U16(0x001f74e0u + o) = EF_U16(0x00464060u + o);
        EF_U8(0x001f74e2u + o) = EF_U8(0x00464062u + o);
    }
    expand_table_7632c();
}

/* 0x385cc, 140 bytes. (ret 8) Append a new 0x19-byte node to the list for slot idx (heads at 0x19f5b0, next at +0x15, previous at +0x11) and fill it from the arguments (+0 ebx, +4 ecx, +0xc byte edx, +8 and +0xd the two stack arguments). Reports an error through 0x132a74 when idx is not below 0x1a1504. */
void append_list_node_385cc(uint32_t idx, uint32_t byte_val, uint32_t val0, uint32_t val4, uint32_t s0, uint32_t s1)
{
    uint32_t node;
    uint32_t slot;
    uint32_t head;
    if ((int16_t)idx >= (int16_t)EF_U16(0x001a1504u)) {
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00183464u);
        return;
    }
    node = guest_callptr_eax_ret_x(EF_U32(0x0012dd9cu), 0x19u);
    slot = 0x0019f5b0u + (uint32_t)((int32_t)(int16_t)idx) * 4u;
    head = EF_U32(slot);
    if (head != 0u) {
        uint32_t p = head;
        while (EF_U32(p + 0x15u) != 0u)
            p = EF_U32(p + 0x15u);
        EF_U32(p + 0x15u) = node;
        EF_U32(node + 0x11u) = p;
    } else {
        EF_U32(slot) = node;
        EF_U32(node + 0x11u) = head;
    }
    EF_U32(node + 0x15u) = 0u;
    EF_U32(node) = val0;
    EF_U32(node + 4u) = val4;
    EF_U8(node + 0xcu) = (uint8_t)byte_val;
    EF_U32(node + 8u) = s0;
    EF_U32(node + 0xdu) = s1;
}

/* 0xd8610, 140 bytes. Rescale the pair of words at 0x15baf2/0x15baf6: save the old values to 0x15bafe/0x15bafa, then recompute them from the ratio of the words at 0x420e7a, 0x420eaa and 0x420eac (and 0x420e78) with the extent 0x15bdcc. */
void rescale_window_d8610(void)
{
    int32_t a = (int32_t)EF_S16(0x00420e7au);
    int32_t b = (int32_t)EF_S16(0x00420eaau);
    int32_t c = (int32_t)EF_S16(0x00420eacu);
    int32_t d = (int32_t)EF_S16(0x00420e78u);
    int32_t t;
    int32_t margin;
    int32_t ext;
    int32_t y;
    EF_U16(0x0015bafeu) = EF_U16(0x0015baf2u);
    EF_U16(0x0015bafau) = EF_U16(0x0015baf6u);
    t = d + (a * b) / c;
    ext = (EF_S32(0x0015bdccu) + 1 - a) * b;
    margin = (c * 5) >> 15;
    EF_U16(0x0015baf2u) = (uint16_t)(t - margin);
    y = d - ext / c - margin;
    EF_U16(0x0015baf6u) = (uint16_t)y;
}

/* 0xa1500, 141 bytes. Step the timer word entry idx of the current page (entries are 2 bytes wide at 0x417174 + 0xb + 29 * [[0x41717c]+1]); the entry must be a type 0x0b word. Counts down byte +7 while byte +4 is below byte +2 and returns 1 when it reaches zero or below; otherwise resets bytes +4/+5 when the limit is exceeded. Returns 0 for locked entries. */
uint8_t step_timer_entry_a1500(uint32_t idx)
{
    uint32_t kind = EF_U8(EF_U32(0x0041717cu) + 1u);
    uint32_t p = EF_U32(0x00417174u) + 0xbu + kind * 29u + (uint32_t)((int32_t)(int16_t)idx) * 2u;
    int8_t cur;
    int8_t lim;
    if ((EF_U16(p) & 0x7c00u) != 0x2c00u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018b318u);
    if (EF_U8(p + 1u) & 0x80u)
        return 0u;
    if (EF_U8(0x00132ac3u) != 0u)
        return 0u;
    cur = (int8_t)EF_U8(p + 4u);
    lim = (int8_t)EF_U8(p + 2u);
    if (cur < lim) {
        EF_U8(p + 7u) = (uint8_t)(EF_U8(p + 7u) - 1u);
        return (int8_t)EF_U8(p + 7u) < 1 ? 1u : 0u;
    }
    if (lim > (int8_t)EF_U8(p + 5u))
        return 0u;
    EF_U8(p + 4u) = 0u;
    EF_U8(p + 5u) = 0u;
    return 0u;
}

/* 0xb5640, 142 bytes. (ret 4) Fill the sub-record at [obj+0x20] from the object in edx: find a free slot run (0xb58a8 with start bx and count s0), store the run at +0xe, compute the extent with 0xb54d0, then set the byte +0x18 (cl), clear words +0x16 and +0xc and copy +0x11/+0x15/+0x1d/+0x1f of the object into +0, +4, +8, +0xa. */
void init_slot_record_b5640(uint32_t obj, uint32_t src, uint32_t ecx, uint32_t ebx, uint32_t need)
{
    uint32_t rec;
    uint8_t flag = (uint8_t)ecx;
    EF_U16(EF_U32(obj + 0x20u) + 0xeu) = (uint16_t)find_free_slot_run_b58a8(src, (int16_t)ebx, (int16_t)need);
    rec = EF_U32(obj + 0x20u);
    guest_fn_b54d0_x(rec + 0x10u, rec + 0x12u, rec + 0x14u, (uint32_t)(int32_t)(int16_t)EF_U16(rec + 0xeu), (uint32_t)(int32_t)(int16_t)EF_U16(src + 0x48u));
    EF_U8(EF_U32(obj + 0x20u) + 0x18u) = flag;
    EF_U16(EF_U32(obj + 0x20u) + 0x16u) = 0u;
    EF_U16(EF_U32(obj + 0x20u) + 0xcu) = 0u;
    EF_U32(EF_U32(obj + 0x20u)) = EF_U32(src + 0x11u);
    EF_U32(EF_U32(obj + 0x20u) + 4u) = EF_U32(src + 0x15u);
    EF_U16(EF_U32(obj + 0x20u) + 8u) = EF_U16(src + 0x1du);
    EF_U16(EF_U32(obj + 0x20u) + 0xau) = EF_U16(src + 0x1fu);
}

/* 0xf2a30, 142 bytes. Apply the palette at 0x425c8c, decrement the word 0x1616ee and send the parameter block built from 0x485920/0x48591c and the words at 0x48594a..0x485954 to 0xdd9e4; when 0x161e18 and 0x161e16 are clear and 0x1616ed is set, set 0x161e18 to 0xffff. */
void send_param_block_f2a30(void)
{
    guest_fn_d663c(0x00425c8cu);
    EF_U16(0x001616eeu) = (uint16_t)(EF_U16(0x001616eeu) - 1u);
    set_param_block_dd9e4(EF_U32(0x00485920u), EF_U32(0x0048591cu),
                          (uint32_t)(int32_t)EF_S16(0x0048594eu), (uint32_t)(int32_t)EF_S16(0x0048594au),
                          (uint32_t)(int32_t)EF_S16(0x00485954u), (uint32_t)(int32_t)EF_S16(0x00485952u),
                          (uint32_t)(int32_t)EF_S16(0x00485950u), (uint32_t)(int32_t)EF_S16(0x0048594cu));
    if (EF_U16(0x00161e18u) == 0u && EF_U16(0x00161e16u) == 0u && EF_U8(0x001616edu) != 0u)
        EF_U16(0x00161e18u) = 0xffffu;
}

/* Draw a zero-terminated string with the glyph pointer table glyph_tbl and the advance table width_tbl: spaces and tabs advance by gap, other characters (low 6 bits index the map at 0x15e1be, -1 for none) are drawn with 0xcf7c4 and advance by width + extra. */
static void draw_text_glyphs(uint32_t colour, uint32_t x, uint32_t y, uint32_t str, uint32_t glyph_tbl, uint32_t width_tbl, uint32_t gap, uint32_t extra, int remap_a5)
{
    int16_t i;
    for (i = 0;; i++) {
        uint8_t c = EF_U8(str + (uint32_t)(int32_t)i);
        int32_t idx;
        if (c == 0u)
            return;
        if (remap_a5 && c == 0xa5u)
            c = 0x4eu;
        if (c == 0x20u || c == 9u) {
            x += gap;
            continue;
        }
        idx = (int32_t)(int8_t)EF_U8(0x0015e1beu + (uint32_t)(c & 0x3fu));
        if (idx == -1)
            continue;
        guest_fn_cf7c4_x(EF_U32(glyph_tbl + (uint32_t)idx * 4u), (uint32_t)(int32_t)(int16_t)y, (uint32_t)(int32_t)(int16_t)x, colour & 0xffu);
        x += EF_U8(width_tbl + (uint32_t)idx) + extra;
    }
}

/* 0xdd4c4, 150 bytes. Draw a string with the large font (glyphs 0x15e61c, widths 0x15e27e, space 0x11, spacing 2). */
void draw_text_large_dd4c4(uint32_t colour, uint32_t x, uint32_t y, uint32_t str)
{
    draw_text_glyphs(colour, x, y, str, 0x0015e61cu, 0x0015e27eu, 0x11u, 2u, 1);
}

/* 0xdd5bc, 148 bytes. Draw a string with the medium font (glyphs 0x15e6ac, widths 0x15e2a2, space 5, spacing 1). */
void draw_text_medium_dd5bc(uint32_t colour, uint32_t x, uint32_t y, uint32_t str)
{
    draw_text_glyphs(colour, x, y, str, 0x0015e6acu, 0x0015e2a2u, 5u, 1u, 1);
}

/* 0xdd6a0, 134 bytes. Draw a string with the small font (glyphs 0x15e73c, widths 0x15e2c6, space 5, spacing 1; 0xa5 is not remapped). */
void draw_text_small_dd6a0(uint32_t colour, uint32_t x, uint32_t y, uint32_t str)
{
    draw_text_glyphs(colour, x, y, str, 0x0015e73cu, 0x0015e2c6u, 5u, 1u, 0);
}

/* 0xde7d0, 147 bytes. Look up the hash of the name in the binary tree whose keys are at 0x426150 and child word pairs at 0x4270f0 (left/right by the sign of the key difference); return the node index when found, otherwise insert a new node numbered by the word at 0x15eea0 under the last visited node and return -1. The new node's key is not stored here. */
uint32_t tree_find_or_insert_de7d0(uint32_t text)
{
    uint32_t h = name_hash_ebfd0(text);
    uint32_t cur = 0u;
    uint32_t prev;
    int32_t diff = 1;
    uint32_t idx;
    EF_U32(0x00426150u) = h;
    do {
        prev = cur;
        cur = EF_U16(0x004270f0u + cur * 4u + (diff >= 0 ? 2u : 0u));
        diff = (int32_t)(h - EF_U32(0x00426150u + cur * 4u));
    } while (diff != 0);
    if (cur != 0u)
        return cur;
    idx = (uint32_t)(int32_t)EF_S16(0x0015eea0u);
    EF_U32(EF_U32(0x00428860u) + idx * 4u) = (uint32_t)diff;
    EF_U16(0x004270f0u + idx * 4u) = 0u;
    EF_U16(0x004270f2u + idx * 4u) = 0u;
    diff = (int32_t)(h - EF_U32(0x00426150u + prev * 4u));
    EF_U16(0x004270f0u + prev * 4u + (diff >= 0 ? 2u : 0u)) = (uint16_t)idx;
    return 0xffffffffu;
}

/* 0x9dbf0, 149 bytes. Same lookup/insert as 0xde7d0 over the tables at 0x412fc0 (keys), 0x40f140 (signed child words) and 0x411080 (payload, node 0 receives the argument), new node numbers from the word at 0x132b10. */
uint32_t tree_find_or_insert_9dbf0(uint32_t text)
{
    uint32_t h;
    uint32_t cur = 0u;
    uint32_t prev;
    int32_t diff = 1;
    uint32_t idx;
    EF_U32(0x00411080u) = text;
    h = name_hash_ebfd0(text);
    EF_U32(0x00412fc0u) = h;
    do {
        prev = cur;
        cur = (uint32_t)(int32_t)EF_S16(0x0040f140u + cur * 4u + (diff >= 0 ? 2u : 0u));
        diff = (int32_t)(EF_U32(0x00412fc0u) - EF_U32(0x00412fc0u + cur * 4u));
    } while (diff != 0);
    if (cur != 0u)
        return cur;
    idx = (uint32_t)(int32_t)EF_S16(0x00132b10u);
    EF_U32(0x00411080u + idx * 4u) = (uint32_t)diff;
    EF_U16(0x0040f140u + idx * 4u) = 0u;
    EF_U16(0x0040f142u + idx * 4u) = 0u;
    diff = (int32_t)(EF_U32(0x00412fc0u) - EF_U32(0x00412fc0u + prev * 4u));
    EF_U16(0x0040f140u + prev * 4u + (diff >= 0 ? 2u : 0u)) = (uint16_t)idx;
    return 0xffffffffu;
}

/* 0xc1c24, 151 bytes. Scan the chunk stream that starts 0xc bytes into the buffer at 0x41f3bc (cursor 0x41f3a0, end 0x41f3c0) for the chunk with the given id: each chunk has an 8-byte header (id, big-endian size, rounded up to even). Returns the chunk size and leaves the cursor at its data; reports an error and returns 0 when not found. */
uint32_t find_chunk_c1c24(uint32_t id)
{
    uint32_t hdr[2];
    uint32_t p = EF_U32(0x0041f3bcu) + 0xcu;
    EF_U32(0x0041f3a0u) = p;
    if (p < EF_U32(0x0041f3c0u)) {
        do {
            uint32_t size;
            guest_fn_e9a24(8u, EF_U32(0x0041f3a0u), (uint32_t)(uintptr_t)hdr);
            EF_U32(0x0041f3a0u) = EF_U32(0x0041f3a0u) + 8u;
            size = byte_swap32_c1bd0(hdr[1]);
            size += size & 1u;
            hdr[1] = size;
            if (id == hdr[0])
                return size;
            p = EF_U32(0x0041f3a0u) + size;
            EF_U32(0x0041f3a0u) = p;
        } while (p < EF_U32(0x0041f3c0u));
    }
    guest_callptr1_x(EF_U32(0x00132a74u), 0x0018eda8u);
    return 0u;
}

/* 0xc9f28, 153 bytes. Set the view to the rectangle at 0x41ec08 (x, y, width, height words): origin at the centre, latch the rectangle twice and refresh the view scale when bit 0 of obj+0x3c is set. */
void set_view_rect_c9f28(uint32_t obj)
{
    int32_t x0 = EF_S16(0x0041ec08u);
    int32_t y0 = EF_S16(0x0041ec0au);
    int32_t x1 = EF_S16(0x0041ec0cu) + x0 - 1;
    int32_t y1 = EF_S16(0x0041ec0eu) + y0 - 1;
    set_view_origin_dc21c((uint32_t)y1, (uint32_t)x1, (uint32_t)((y1 + y0) / 2 - 1), (uint32_t)((x1 + x0) / 2 - 1));
    guest_fn_ce328_w(x0, x1, y0, y1);
    guest_fn_cdf48((uint32_t)x0, (uint32_t)x1, (uint32_t)y0, (uint32_t)y1);
    if (EF_U8(obj + 0x3cu) & 1u)
        update_view_scale_dc314();
}

/* 0x78934, 153 bytes. Unless 0x127da8 is set: count the timer 0x1f8d58 down by the step 0x127d92 (clearing it above 0xdc), format the hour-like value (0x12583c + 0x12b054 wrapped into 0..23) and the word at 0x12583e with the format 0x12f4a1, print it at (15, 10, 110) and set 0x127b1a. */
void draw_clock_text_78934(void)
{
    uint8_t buf[0x20];
    int16_t v;
    if (EF_U8(0x00127da8u) != 0u)
        return;
    EF_U8(0x001f8d58u) = (uint8_t)(EF_U8(0x001f8d58u) - EF_U8(0x00127d92u));
    if (EF_U8(0x001f8d58u) > 0xdcu)
        EF_U8(0x001f8d58u) = 0u;
    v = (int16_t)(EF_U16(0x0012583cu) + EF_U16(0x0012b054u));
    while (v > 0x17)
        v = (int16_t)(v - 0x18);
    while (v < 0)
        v = (int16_t)(v + 0x18);
    guest_sprintf_a4342((uint32_t)(uintptr_t)buf, 0x0012f4a1u, (uint32_t)(int32_t)v, (uint32_t)(int32_t)EF_S16(0x0012583eu));
    EF_U8(0x00127b1au) = 1u;
    guest_fn_48c90(15u, 10u, 0x6eu, (uint32_t)(uintptr_t)buf);
}

/* 0xebc50, 133 bytes. Read count bytes into dst from the archive context ctx (position +0x158, end +0x15c): the part beyond the end is filled with 0xff; with bit 19 of flags the data is copied from memory (0xdc832), otherwise read from the handle in flags (0xa79d4). Returns the number of bytes read (0 without a context). */
uint32_t read_archive_ebc50(uint32_t dst, uint32_t count, uint32_t flags, uint32_t ctx)
{
    uint32_t remaining;
    if (ctx == 0u)
        return 0u;
    remaining = EF_U32(ctx + 0x15cu) - EF_U32(ctx + 0x158u);
    if ((int32_t)count > (int32_t)remaining) {
        fill_bytes_b3fb0(dst + remaining, 0xffu, count - remaining);
        count = remaining;
    }
    if ((flags >> 16) & 8u) {
        guest_fn_dc832(dst, EF_U32(ctx + 0x158u), count);
        EF_U32(ctx + 0x158u) = EF_U32(ctx + 0x158u) + count;
        return count;
    }
    EF_U32(ctx + 0x158u) = EF_U32(ctx + 0x158u) + count;
    return read_handle_a79d4(0u, 0u, 0u, 0u, flags, count, dst);
}

/* 0x9cde8, 154 bytes. Read count bytes into dst: with flag bits 16/17 from the archive context at 0x40d1f0/0x40d1f4 (0xebc50), with bit 18 from the in-memory block at 0x40d1ec/0x40d1e4 (padding with 0xff), otherwise from the handle in flags. */
uint32_t read_bytes_9cde8(uint32_t dst, uint32_t count, uint32_t flags)
{
    if (flags & 0x30000u) {
        uint32_t rest = flags & 0xfffcffffu;
        uint32_t ctx = (flags & 0x10000u) ? EF_U32(0x0040d1f0u) : EF_U32(0x0040d1f4u);
        return read_archive_ebc50(dst, count, rest, ctx);
    }
    if (flags & 0x40000u) {
        int32_t avail = EF_S32(0x0040d1e4u) - EF_S32(0x0040d1ecu);
        if ((int32_t)count > avail) {
            fill_bytes_b3fb0(dst + (uint32_t)avail, 0xffu, count - (uint32_t)avail);
            count = (uint32_t)avail;
        }
        guest_fn_dc832(dst, EF_U32(0x0040d1ecu), count);
        EF_U32(0x0040d1ecu) = EF_U32(0x0040d1ecu) + count;
        return count;
    }
    return read_handle_a79d4(0u, 0u, 0u, 0u, flags, count, dst);
}

/* 0x78a0c, 155 bytes. For each of the 8 slots (0x7a bytes at 0x1f8930, countdown word at +0x78): count it down by the frame step 0x127d92 (clamped to the remaining value), uppercase its name text, mark 0x127b1a and print the name at (15, 10, 0x55 + 10 * slot); a non-positive counter is reset to 0. */
void update_slot_texts_78a0c(void)
{
    uint8_t slot;
    for (slot = 0u; slot < 8u; slot++) {
        uint32_t rec = (uint32_t)slot * 0x7au;
        if ((int16_t)EF_U16(0x001f89a8u + rec) > 0) {
            int32_t c = (int32_t)(int16_t)EF_U16(0x001f89a8u + rec);
            int32_t step = (int32_t)EF_U16(0x00127d92u);
            uint32_t text = 0x001f8930u + rec;
            if (step >= c)
                step = c;
            EF_U16(0x001f89a8u + rec) = (uint16_t)(c - step);
            uppercase_string(text);
            EF_U8(0x00127b1au) = 1u;
            guest_fn_48c90(15u, 10u, (uint32_t)(int32_t)(int16_t)(0x55u + (uint32_t)slot * 10u), text);
        } else {
            EF_U16(0x001f89a8u + rec) = 0u;
        }
    }
}

/* 0x34b2c, 158 bytes. Dispatch on the state byte 0x13578e (3..19): rebuild the child dialog for the matching label, update the buttons (states 4 and 13), clear 0x15f07f and run 0xa2bdc (state 5); any other state reports an error through 0x132a74. */
void dispatch_dialog_state_34b2c(void)
{
    uint8_t idx = (uint8_t)(EF_U8(0x0013578eu) - 3u);
    uint32_t label = 0u;
    if (idx > 16u) {
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00182ed4u, EF_U32(0x00135790u));
        return;
    }
    switch (idx) {
    case 0: label = 0x00182e80u; break;
    case 1:
    case 10:
        update_buttons_33e78();
        return;
    case 2:
        EF_U8(0x0015f07fu) = 0u;
        guest_fn_a2bdc();
        return;
    case 3: label = 0x00182e8cu; break;
    case 4: label = 0x00182e9cu; break;
    case 12: label = 0x00182ea8u; break;
    case 13: label = 0x00182ebcu; break;
    case 15: label = 0x00182ec8u; break;
    case 16: label = 0x00182eb0u; break;
    default:
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00182ed4u, EF_U32(0x00135790u));
        return;
    }
    rebuild_child_dialog_34a98(label);
}

/* 0xbd7cc, 157 bytes. Try to shrink the counter byte at p+3 by one: the minimum depends on the type byte p+9 (types 1, 2, 10 need more than 2, type 12 more than 3, type 3 never, others more than 1). Returns 1 when it was decremented; EDX is returned unchanged (with DH cleared) in the early case of a high level and an idle type. */
uint32_t shrink_counter_bd7cc(uint32_t p, uint32_t edx)
{
    uint8_t type;
    uint8_t min;
    if (EF_U8(0x0012583au) > 4u) {
        uint8_t t9 = EF_U8(p + 9u);
        if ((t9 == 0u || t9 == 2u) && (edx & 0xffu) == 0u)
            return edx & 0xffff00ffu;
    }
    type = (uint8_t)(EF_U8(p + 9u) - 1u);
    if (type > 0xbu)
        min = 1u;
    else if (type == 0u || type == 1u || type == 9u)
        min = 2u;
    else if (type == 11u)
        min = 3u;
    else if (type == 2u)
        return 0u;
    else
        min = 1u;
    if (EF_U8(p + 3u) > min) {
        EF_U8(p + 3u) = (uint8_t)(EF_U8(p + 3u) - 1u);
        return 1u;
    }
    return 0u;
}

/* 0xe5e90, 159 bytes. Draw a 7x9 glyph in the colour byte 0x433ec0 at (x = eax, y row = edx): code bit 7 selects the table at 0x15feb0 (index code & 0x1f) otherwise 0x15fc70 (index code & 0x3f), 9 mask bytes per glyph. */
void draw_glyph_7x9_e5e90(uint32_t x, uint32_t y, uint32_t code)
{
    uint8_t colour = EF_U8(0x00433ec0u);
    uint32_t tbl;
    if (code & 0x80u)
        tbl = 0x0015feb0u + (code & 0x1fu) * 9u;
    else
        tbl = 0x0015fc70u + (code & 0x3fu) * 9u;
    draw_glyph_rows(colour, x, y, tbl, 9u, 7u, 0x80u);
    EF_U8(0x00433ec0u) = colour;
}

/* 0x6f6b8, 160 bytes. For the unit's 20 three-byte slot records (+0x198: type word area, +0x19b: kind byte) find the slot of rank-2 weapon type with the lowest class (1, 2, 3 by kind byte - 6, 0xff for others) and return its kind byte; 0 when no slot qualifies or the unit is unknown. */
uint8_t best_slot_kind_6f6b8(uint32_t unit)
{
    static const uint8_t cls[23] = { 1, 1, 1, 3, 1, 1, 2, 2, 1, 3, 0xff, 0xff, 0xff, 0xff, 3, 1, 1, 2, 1, 1, 1, 3, 1 };
    ef_addr32 obj = lookup_filtered_indexed_object((int16_t)unit);
    uint8_t best = 0xffu;
    uint8_t result = 0u;
    int16_t i;
    if (obj == 0u)
        return 0u;
    for (i = 0; i < 20; i++) {
        uint32_t rec = obj + (uint32_t)((int32_t)i * 3);
        int32_t type;
        uint8_t kind;
        uint8_t c;
        if (EF_U16(rec + 0x19cu) == 0u)
            continue;
        type = (int32_t)(int8_t)EF_U8(rec + 0x19bu);
        if (EF_U8(0x0012bc94u + (uint32_t)(type * 0x91)) != 2u)
            continue;
        kind = EF_U8(rec + 0x19bu);
        c = (uint8_t)(kind - 6u);
        c = c > 0x16u ? 0xffu : cls[c];
        if (c < best) {
            best = c;
            result = EF_U8(rec + 0x19bu);
        }
    }
    return result;
}

/* 0xe3868, 160 bytes. Load the string table from the resource stream: the entry count word (0x15f902), a 2 * count byte header into the buffer at [0x429224], then a size dword, an arena block of that size and the packed zero-terminated strings; entry i (1..count-1) of the pointer table at [0x429228] points at the i-th string and entry 0 at the constant 0x190944. */
void load_string_table_e3868(void)
{
    uint32_t size = 0u;
    uint32_t block;
    int16_t i;
    read_resource_e3170(0x0015f902u, 2u);
    read_resource_e3170(EF_U32(0x00429224u), (uint16_t)(EF_U16(0x0015f902u) * 2u));
    read_resource_e3170((uint32_t)(uintptr_t)&size, 4u);
    block = guest_fn_e29e8(size);
    read_resource_e3170(block, (uint16_t)size);
    for (i = 1; i < (int16_t)EF_U16(0x0015f902u); i++) {
        EF_U32(EF_U32(0x00429228u) + (uint32_t)(int32_t)i * 4u) = block;
        if (EF_U8(block) != 0u) {
            uint8_t c;
            do {
                c = EF_U8(block + 1u);
                block++;
            } while (c != 0u);
        }
        block++;
    }
    EF_U32(EF_U32(0x00429228u)) = 0x00190944u;
}

/* 0xfdec0, 160 bytes. (ret 8) Bresenham circle quadrant walk: x = esi decreasing, y = edi increasing until it reaches the end row ebp, err = ebx the decision variable; the callback fn (first stack argument) is called with EAX = ctx (second stack argument) and EDX = x at the points of the walk. */
void circle_walk_down_fdec0(int32_t x, int32_t y, int32_t err, int32_t y_end, uint32_t fn, uint32_t ctx)
{
    while (y < y_end) {
        if (err == 0) {
            y++;
            x--;
            err = (y - x) * 2;
            guest_callptr_eaxedx_x(fn, ctx, (uint32_t)x);
            err += 2;
        } else if (err < 0) {
            if ((err + x) * 2 - 1 > 0) {
                x--;
                guest_callptr_eaxedx_x(fn, ctx, (uint32_t)x);
                y++;
                err += (y - x) * 2 + 2;
            } else {
                guest_callptr_eaxedx_x(fn, ctx, (uint32_t)x);
                y++;
                err += y * 2 + 1;
            }
        } else {
            if ((err - y) * 2 - 1 > 0) {
                x--;
                err += 1 - x * 2;
            } else {
                x--;
                guest_callptr_eaxedx_x(fn, ctx, (uint32_t)x);
                y++;
                err += (y - x) * 2 + 2;
            }
        }
    }
}

/* 0xfde20, 160 bytes. (ret 8) Mirror image of 0xfdec0: x = esi increasing, y = edi decreasing until it reaches the end row ebp. */
void circle_walk_up_fde20(int32_t x, int32_t y, int32_t err, int32_t y_end, uint32_t fn, uint32_t ctx)
{
    while (y > y_end) {
        if (err == 0) {
            uint32_t cx = (uint32_t)x;
            x++;
            y--;
            err = (x - y) * 2;
            guest_callptr_eaxedx_x(fn, ctx, cx);
            err += 2;
        } else if (err < 0) {
            if ((err + y) * 2 - 1 > 0) {
                guest_callptr_eaxedx_x(fn, ctx, (uint32_t)x);
                x++;
                y--;
                err += (x - y) * 2 + 2;
            } else {
                x++;
                err += x * 2 + 1;
            }
        } else {
            if ((err - x) * 2 - 1 > 0) {
                guest_callptr_eaxedx_x(fn, ctx, (uint32_t)x);
                y--;
                err += 1 - y * 2;
            } else {
                guest_callptr_eaxedx_x(fn, ctx, (uint32_t)x);
                x++;
                y--;
                err += (x - y) * 2 + 2;
            }
        }
    }
}

/* 0xe40e4, 161 bytes. Allocate (once) the pattern buffer of ([0x15f93c] << 8) + 0x200 bytes, align it to 256 bytes (0x15b958) and fill consecutive runs of it with the values 1, 2, 3, ... whose lengths are the bytes of the list at 0x431ed0 (terminated by a zero). */
void init_pattern_buffer_e40e4(void)
{
    uint32_t p;
    uint32_t i;
    if (EF_U32(0x0015f93cu) == 0u) {
        EF_U8(0x00431ed0u) = 0xffu;
        EF_U32(0x0015f93cu) = 1u;
    }
    if (EF_U32(0x0015f940u) == 0u) {
        uint32_t block = guest_fn_c3f70((EF_U32(0x0015f93cu) << 8) + 0x200u);
        EF_U32(0x0015f940u) = block;
        if (block == 0u)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x00190af0u);
        EF_U32(0x0015b958u) = (EF_U32(0x0015f940u) + 0xffu) & 0xffffff00u;
    }
    p = EF_U32(0x0015b958u);
    if (EF_U8(0x00431ed0u) == 0u)
        return;
    i = 0u;
    for (;;) {
        uint8_t next;
        fill_bytes_b3fb0(p, i + 1u, EF_U8(0x00431ed0u + i));
        p += EF_U8(0x00431ed0u + i);
        next = EF_U8(0x00431ed1u + i);
        i++;
        if (next == 0u)
            break;
    }
}

/* 0xd23d8, 162 bytes. Process a render object: count it (0x4209c8), prepare it with 0xd2130 unless 0x158ed8 is set, and when the object is active (flag 8 of +0x2c clear) and its mesh header has at least 3 entries, refresh it (0xd2110 unless flag 1, 0xd2320) and, if the word at +0x1e exceeds the word at +0x1a and the triangle is not clockwise, call the object's handler at +0x20. */
void process_render_object_d23d8(uint32_t obj)
{
    uint32_t mesh;
    EF_U32(0x004209c8u) = EF_U32(0x004209c8u) + 1u;
    if (EF_U8(0x00158ed8u) == 0u)
        guest_fn_d2130_x(obj);
    if (EF_U8(obj + 0x2cu) & 8u)
        return;
    if (EF_U16(obj + 0x2cu) & 0x1f0u)
        obj = apply_pending_events_ce944(obj);
    mesh = EF_U32(obj + 0x4cu);
    if (EF_S32(mesh) < 3)
        return;
    EF_U32(0x004209c4u) = EF_U32(0x004209c4u) + 1u;
    if ((EF_U8(obj + 0x2cu) & 1u) == 0u)
        forward_record_d2110(obj);
    guest_fn_d2320_x(obj);
    if ((int16_t)EF_U16(obj + 0x1eu) > (int16_t)EF_U16(obj + 0x1au)) {
        uint32_t tri = (EF_U32(obj + 0xcu) - 1u) * 4u + mesh + 4u;
        if (triangle_is_clockwise_d22d0(tri) == 0u) {
            EF_U32(0x004209c0u) = EF_U32(0x004209c0u) + 1u;
            guest_callptr_eax_x(EF_U32(obj + 0x20u), obj);
        }
    }
}

/* 0xb9e9c, 164 bytes. Map a (class, selector) pair to a code: the class byte comes from the record at [obj+0x51] through 0xba0e4 (4 when absent), the selector 0..5 picks one of two 7-entry tables (selectors 0, 2, 5 use the first); selector above 5 gives 4, class out of range 0. */
uint8_t class_selector_code_b9e9c(uint32_t obj, uint32_t sel)
{
    static const uint8_t tbl_a[7] = { 0x13, 9, 0x11, 2, 0xa, 8, 0x14 };
    static const uint8_t tbl_b[7] = { 0x17, 0xc, 0x1a, 5, 0xf, 7, 0x18 };
    uint8_t code = 4u;
    uint8_t v;
    uint32_t cls = EF_U32(obj + 0x51u);
    if (cls != 0u)
        code = map_code_ba0e4(EF_U8(cls));
    sel &= 0xffu;
    if (sel > 5u)
        return 4u;
    v = (uint8_t)(code - 1u);
    if (v > 6u)
        return 0u;
    return (sel == 0u || sel == 2u || sel == 5u) ? tbl_a[v] : tbl_b[v];
}

/* 0xe3444, 163 bytes. Finish the open resource file 0x15f8f8 (when not -1): read two file positions around the call to 0xe36bc, append them (4 bytes each, through the writer at [0x42922c]), rewrite the 2-byte header at 0x15f900, close the file and mark it closed. */
void finish_resource_file_e3444(void)
{
    uint32_t pos_a;
    uint32_t pos_b;
    uint32_t h = EF_U32(0x0015f8f8u);
    if (h == 0xffffffffu)
        return;
    pos_a = seek_end_a7ae0(0u, 0u, 0u, 0u, h, 0u);
    write_string_table_e36bc();
    pos_b = seek_end_a7ae0(0u, 0u, 0u, 0u, EF_U32(0x0015f8f8u), 0u);
    seek_start_a7ab4(0u, 0u, 0u, 0u, EF_U32(0x0015f8f8u), 2u);
    guest_callptr_eaxedx_x(EF_U32(0x0042922cu), (uint32_t)(uintptr_t)&pos_b, 4u);
    guest_callptr_eaxedx_x(EF_U32(0x0042922cu), (uint32_t)(uintptr_t)&pos_a, 4u);
    seek_start_a7ab4(0u, 0u, 0u, 0u, EF_U32(0x0015f8f8u), 0u);
    guest_callptr_eaxedx_x(EF_U32(0x0042922cu), 0x0015f900u, 2u);
    close_handle_a7990(0u, 0u, 0u, 0u, EF_U32(0x0015f8f8u));
    EF_U32(0x0015f8f8u) = 0xffffffffu;
}

/* 0x98ac0, 166 bytes. Restore the screen strip saved at 0x40cd72/0x40cd74: when a saved position exists, put the 0x23x0x1e strip at 0x12dda4 back there; otherwise capture two strips (AL selects the left or right panel position) from the screen into 0x40c124 and redraw them from 0x408590. Marks the position as unset afterwards. */
void restore_strip_98ac0(uint32_t a)
{
    if (EF_U16(0x0040cd74u) != 0xffffu) {
        put_screen_rows_eb050(0u, 0u, 0u, 0u, EF_U16(0x0040cd74u), EF_U16(0x0040cd72u), 0x23u, 0x1eu, EF_U32(0x0012dda4u));
    } else if ((a & 0xffu) == 0u) {
        get_screen_rows_eb1f0(0u, 0u, 0u, 0u, 0xfu, 0xa0u, 0x23u, 0x1eu, 0x0040c124u);
        get_screen_rows_eb1f0(0u, 0u, 0u, 0u, 0xfu, 0xa0u, 0xfdu, 0x1eu, 0x00408590u);
    } else {
        get_screen_rows_eb1f0(0u, 0u, 0u, 0u, 0x41u, 0x168u, 0x23u, 0x1eu, 0x0040c124u);
        get_screen_rows_eb1f0(0u, 0u, 0u, 0u, 0x41u, 0x168u, 0x1fcu, 0x1eu, 0x00408590u);
    }
    EF_U16(0x0040cd74u) = 0xffffu;
}

/* 0x11d27b, 167 bytes. Format a signed value as a sign character plus zero padded decimal digits (at most 3 digits are produced; no terminator is written). OUT in EAX, VALUE in EDX, WIDTH in EBX (0 means 2, or 3 for values of 100 and up), buffer size in ECX. Nothing is written when WIDTH+1 exceeds the buffer size or the digits do not fit. Returns WIDTH+1. */
uint32_t format_signed_11d27b(uint32_t out, int32_t value, int32_t width, int32_t room)
{
    uint8_t sign = '+';
    int32_t digits;
    int32_t div;
    int32_t ret;
    if (value < 0) {
        sign = '-';
        value = -value;
    }
    if (value >= 100) {
        div = 100;
        digits = 3;
    } else if (value >= 10) {
        div = 10;
        digits = 2;
    } else {
        digits = 1;
        div = 1;
    }
    if (width == 0) {
        width = 2;
        if (digits == 3)
            width = digits;
    }
    ret = width + 1;
    if (ret > room)
        return (uint32_t)ret;
    if (digits > width)
        return (uint32_t)(digits + 1);
    EF_U8(out) = sign;
    out++;
    while (width > digits) {
        width--;
        EF_U8(out) = '0';
        out++;
    }
    do {
        EF_U8(out) = (uint8_t)(value / div + 0x30);
        value = value % div;
        div = div / 10;
        out++;
    } while (div != 0);
    return (uint32_t)ret;
}

/* 0xcd2e8, 166 bytes. Add DELTA to the 16-bit progress value of the current entry (clamped to 100, stored in the table at 0x41f700). When the value crosses a 25-step boundary, run the text for the new step on the page at 0x41f708. Returns 1 when the value reached 100 (or the entry at 0x15b628 is empty). */
uint32_t add_progress_cd2e8(uint32_t delta)
{
    uint32_t p = EF_U32(0x0015b628u);
    int32_t idx;
    int32_t v;
    int32_t before;
    int32_t after;
    uint32_t t;
    if (EF_U16(p) == 0u)
        return 1u;
    idx = (int16_t)EF_U16(p + 4u);
    v = (int16_t)EF_U16(EF_U32(0x0041f700u) + (uint32_t)idx * 2u);
    before = v / 25;
    v += (int16_t)delta;
    if (v > 100)
        v = 100;
    after = v / 25;
    p = EF_U32(0x0015b628u);
    idx = (int16_t)EF_U16(p + 4u);
    EF_U16(EF_U32(0x0041f700u) + (uint32_t)idx * 2u) = (uint16_t)v;
    if (before != after) {
        t = p + (uint32_t)(after * 2 + 4);
        t += (uint32_t)((int32_t)(int16_t)EF_U16(t) * 2);
        run_text_on_page_9e494((uint32_t)(int16_t)EF_U16(0x0041f708u), t);
    }
    return v == 100;
}

/* 0x103e5c, 166 bytes. Parse a number from the string STR in base BASE (digit characters are looked up case insensitively in the table at 0x1947e8; '-' flips the sign and characters flagged 2 in the table at 0x195af0 are skipped). Parsing stops at the first character that is not a digit. When ENDP is not null, stores the address of the stopping character there. (cdecl) */
int32_t parse_number_103e5c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t str, int32_t base, uint32_t endp)
{
    int32_t result = 0;
    int32_t sign = 1;
    uint32_t i = 0u;
    (void)u0; (void)u1; (void)u2; (void)u3;
    while (i < guest_fn_b1b8b(str)) {
        uint32_t c = EF_U8(str + i);
        int32_t d;
        if (c == 0x2du) {
            sign = -sign;
        } else if ((EF_U8(0x00195af0u + ((c + 1u) & 0xffu)) & 2u) == 0u) {
            d = 0;
            if (base > 0) {
                do {
                    if ((uint32_t)to_upper_ascii((int32_t)c) == EF_U8(0x001947e8u + (uint32_t)d)) {
                        result = result * base + d;
                        break;
                    }
                    d++;
                } while (d < base);
            }
            if (d == base)
                break;
        }
        i++;
    }
    if (endp != 0u)
        EF_U32(endp) = str + i;
    return result * sign;
}

/* 0x85bc8, 168 bytes. Classify the map cell at POS (two 16-bit coordinates): look up the cell byte in the grid at 0x134d8a (rows of 40, 10 units per cell). When the mode byte at 0x125846 is 3 and 0x1a666b is clear, returns 0 if the cell is flagged negative, 1 if flagged positive, otherwise 2 (group lists at 0x137980). Otherwise maps the cell byte through a jump table: 0 and 5 give 1, 1 gives 0, 2..4 give 2, above 5 gives 4. */
uint8_t classify_cell_85bc8(ef_addr32 pos)
{
    int32_t row = (400 - (int16_t)EF_U16(pos)) / 10;
    int32_t col = (int16_t)EF_U16(pos + 2u) / 10;
    uint8_t cell = EF_U8(0x00134d8au + (uint32_t)(row * 40 + col));
    if (EF_U8(0x00125846u) == 3u && EF_U8(0x001a666bu) == 0u) {
        if ((uint16_t)group_flag_negative_cc1ac(0u, cell, 0x00137980u) != 0u)
            return 0u;
        if ((uint16_t)group_flag_positive_cc22c(0u, cell, 0x00137980u) != 0u)
            return 1u;
        return 2u;
    }
    switch (cell) {
    case 0:
    case 5:
        return 1u;
    case 1:
        return 0u;
    case 2:
    case 3:
    case 4:
        return 2u;
    default:
        return 4u;
    }
}

/* 0xddbe0, 168 bytes. One-time mouse/cursor setup: on the first call allocate the cursor buffers (7, 0x1a, 7 and 0x800 bytes) into 0x4260b0..0x4260a8, report an error through 0x132a74 when one of the first two allocations fails, probe the mouse driver (0xddc88) and, when present, finish the setup (0xddce8). Registers the handler 0xde374. Returns the byte at 0x15ee99. */
uint8_t init_mouse_ddbe0(void)
{
    if (EF_U8(0x0015ee9cu) == 0u) {
        EF_U8(0x00426147u) = 0u;
        EF_U8(0x0015ee9cu) = 1u;
        EF_U32(0x004260b0u) = alloc_aligned_510(7u);
        EF_U32(0x00426098u) = alloc_aligned_510(0x1au);
        EF_U32(0x0042609cu) = EF_U32(0x00426098u);
        EF_U32(0x004260a4u) = EF_U32(0x004260b0u);
        EF_U32(0x004260a0u) = alloc_aligned_510(7u);
        EF_U32(0x004260a8u) = alloc_aligned_510(0x800u);
        if (EF_U32(0x004260b0u) == 0u || EF_U32(0x00426098u) == 0u)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x0019053cu);
        if (query_mouse_ddc88() != 0u)
            guest_fn_ddce8_x();
        guest_fn_9c3cf(0x000de374u);
    }
    return EF_U8(0x0015ee99u);
}

/* 0x101a90, 169 bytes. One-time runtime init guarded by the flag at 0x17d2cc: register the exit handler pair (0x101a90, 0x103d48) and mark eight 4-byte ranges (0x4dd620..0x4dd640) through 0x104908. */
void init_once_101a90(void)
{
    if (EF_U32(0x0017d2ccu) == 0u) {
        guest_fn_104840(0x00101a90u, 0x00103d48u);
        stack_range_104908(0x004dd624u, 4u);
        stack_range_104908(0x004dd628u, 4u);
        stack_range_104908(0x004dd620u, 4u);
        stack_range_104908(0x004dd62cu, 4u);
        stack_range_104908(0x004dd63cu, 4u);
        stack_range_104908(0x004dd638u, 4u);
        stack_range_104908(0x004dd640u, 4u);
        stack_range_104908(0x004dd630u, 4u);
        EF_U32(0x0017d2ccu) = 1u;
    }
}

/* 0xfa6d3, 168 bytes. Allocate a stream slot (26 bytes): reuse a node from the free list at 0x4644e0 (keeping its 0x4000 flag), else take an unused slot of the table 0x160424..0x16062c (flags & 3 clear) with a fresh 8-byte list node, else allocate node and slot together (flag 0x4003). The slot is cleared, flagged 3 and linked into the list at 0x485f9c. Returns the slot, or 0 after reporting error 5. */
ef_addr32 alloc_stream_slot_fa6d3(void)
{
    uint32_t node = EF_U32(0x004644e0u);
    uint32_t slot;
    uint32_t flags;
    if (node != 0u) {
        slot = EF_U32(node + 4u);
        flags = (EF_U32(slot + 0xcu) & 0x4003u) | 3u;
        EF_U32(0x004644e0u) = EF_U32(node);
    } else {
        slot = 0x00160424u;
        for (;;) {
            if (slot >= 0x0016062cu) {
                node = guest_fn_fc795(0x22u);
                flags = 0x4003u;
                slot = node;
                if (node == 0u) {
                    guest_fn_ebe6d(5u);
                    return 0u;
                }
                slot = node + 8u;
                break;
            }
            if ((EF_U8(slot + 0xcu) & 3u) == 0u) {
                node = guest_fn_fc795(8u);
                if (node == 0u) {
                    guest_fn_ebe6d(5u);
                    return 0u;
                }
                flags = 3u;
                break;
            }
            slot += 0x1au;
        }
    }
    fill_bytes_b3fb0(slot, 0u, 0x1au);
    EF_U32(slot + 0xcu) = flags;
    EF_U32(node + 4u) = slot;
    EF_U32(node) = EF_U32(0x00485f9cu);
    EF_U32(0x00485f9cu) = node;
    return slot;
}

/* 0xcd957, 79 bytes. Copy a SRC block of ROWS rows of WIDTH bytes to the frame buffer at (X, Y), skipping zero (transparent) source bytes; SKIP is added to the source pointer after each row (ret 24). */
void blit_transparent_cd957(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t skip, uint32_t rows, uint32_t width, int32_t y, int32_t x, ef_addr32 src)
{
    uint32_t pitch = EF_U32(0x001631f4u);
    uint32_t dst = EF_U32(0x00163200u + (uint32_t)y * 4u) + EF_U32(0x0015b754u) + (uint32_t)x;
    (void)u0; (void)u1; (void)u2; (void)u3;
    rows &= 0xffffu;
    width &= 0xffffu;
    do {
        uint32_t n = width;
        uint32_t d = dst;
        do {
            uint8_t c = EF_U8(src);
            if (c != 0u)
                EF_U8(d) = c;
            src++;
            d++;
            n--;
        } while (n != 0u);
        dst += pitch;
        src += skip;
        rows--;
    } while (rows != 0u);
}

/* 0x98b68, 104 bytes. Save the 0x23x0x1e screen strip at (X, Y) (negative values clamp to 0) into 0x12dda4, remember the position at 0x40cd74/0x40cd72 and draw the 0x40c958 sprite over it. */
void save_and_draw_strip_98b68(uint32_t x, uint32_t y)
{
    if ((int16_t)x < 0)
        x = 0u;
    if ((int16_t)y < 0)
        y = 0u;
    get_screen_rows_eb1f0(0u, 0u, 0u, 0u, (uint16_t)x, (uint16_t)y, 0x23u, 0x1eu, EF_U32(0x0012dda4u));
    EF_U16(0x0040cd74u) = (uint16_t)x;
    EF_U16(0x0040cd72u) = (uint16_t)y;
    blit_transparent_cd957(0u, 0u, 0u, 0u, 0u, 0x1eu, 0x23u, (uint16_t)y, (uint16_t)x, 0x0040c958u);
}

/* 0xd1bec, 104 bytes. Finish the log file at handle 0x420944: write the sizes into the header (offsets 4 and 0x334, size 0x330 rounded up to even) and seek to the end. */
void finish_log_d1bec(void)
{
    uint32_t v;
    if (EF_U8(0x00420940u) & 1u)
        log_value_d175c(0u);
    seek_start_a7ab4(0u, 0u, 0u, 0u, EF_U32(0x00420944u), 4u);
    v = EF_U32(0x00420940u);
    write_log_dword_swapped_d170c(v + 0x330u + (v & 1u));
    seek_start_a7ab4(0u, 0u, 0u, 0u, EF_U32(0x00420944u), 0x334u);
    write_log_dword_swapped_d170c(EF_U32(0x00420940u));
    seek_end_a7ae0(0u, 0u, 0u, 0u, EF_U32(0x00420944u), 0u);
}

/* 0x5e1b0, 106 bytes. Draw the format string FMT at cell (A, B) (scaled by the shift at 0x12b369, offset by 0x1a6cfc/0x1a6cfa) and continue with 0x5e21c at the column after the text. */
void draw_cell_text_5e1b0(int32_t a, int32_t b, ef_addr32 fmt, int32_t c)
{
    uint8_t s = EF_U8(0x0012b369u);
    uint32_t w;
    int32_t px = (int16_t)(((uint32_t)a << s) + EF_U16(0x001a6cfcu));
    int32_t py = (int16_t)(((uint32_t)b << s) + EF_U16(0x001a6cfau));
    draw_formatted_text_mode_48bb0(EF_U8(0x00128148u), (uint8_t)(1u - EF_U8(0x0012b369u)), px, py, fmt);
    w = formatted_width_48d5c(fmt);
    a += (int32_t)w + 1;
    guest_fn_5e21c_x((int16_t)a, (int16_t)b, (uint32_t)c, 0u);
}

/* 0x3bcc0, 101 bytes. Optionally (when the last stack argument is 1) draw through 0xc8ccc with the first and second words of PTR added to B and A (doubled in video mode 3), then draw a box of colour C at (A, B) (ret 4). */
void draw_marked_box_3bcc0(int32_t a, int32_t b, ef_addr32 ptr, int32_t c, uint32_t mode)
{
    if ((mode & 0xffu) == 1u) {
        if (EF_U8(0x0012635fu) == 3u)
            guest_fn_c8ccc_x((uint32_t)((int32_t)(int16_t)EF_U16(ptr + 2u) * 2 + b), (uint32_t)((int32_t)(int16_t)EF_U16(ptr) * 2 + a), (uint32_t)b, (uint32_t)a, (uint32_t)c);
        else
            guest_fn_c8ccc_x((uint32_t)((int32_t)(int16_t)EF_U16(ptr + 2u) + b), (uint32_t)((int32_t)(int16_t)EF_U16(ptr) + a), (uint32_t)b, (uint32_t)a, (uint32_t)c);
    }
    draw_box_3bc90((int16_t)c, a, b);
}

/* 0x10e28a, 107 bytes. Convert the positive double at P in place to a 64-bit integer: restore the hidden mantissa bit, shift left for large exponents or right (round to nearest, ties to even) for small ones. The sign bit is not handled (it is treated as part of the exponent). */
void round_double_to_int64_10e28a(ef_addr32 p)
{
    uint32_t lo = EF_U32(p);
    uint32_t hi = EF_U32(p + 4u);
    uint32_t e = hi & 0xfff00000u;
    uint32_t ex = e >> 20;
    uint32_t guard = 0u;
    uint32_t sticky = 0u;
    hi = (hi ^ e) ^ 0x100000u;
    if (ex > 0x433u) {
        uint32_t n = ex - 0x433u;
        while (n != 0u) {
            hi = (hi << 1) | (lo >> 31);
            lo <<= 1;
            n--;
        }
    } else if (ex < 0x433u) {
        uint32_t n = 0x433u - ex;
        while (n != 0u) {
            uint32_t cb = hi & 1u;
            uint32_t cc = lo & 1u;
            uint32_t ca = guard & 1u;
            hi >>= 1;
            lo = (lo >> 1) | (cb << 31);
            guard = (guard >> 1) | (cc << 31);
            sticky = sticky * 2u + ca;
            n--;
        }
        if (guard >= 0x80000000u) {
            if (guard != 0x80000000u || sticky != 0u || (lo & 1u) != 0u) {
                lo += 1u;
                if (lo == 0u)
                    hi += 1u;
            }
        }
    }
    EF_U32(p) = lo;
    EF_U32(p + 4u) = hi;
}

/* 0x7cc5c, 114 bytes. Show the main widget set: release the parser buffers, activate the widget stored at "0x187f74" when the mode byte at 0x125839 is 1, run 0x7bee8 on 0x1f8f74, position the "0x187f38" widget at row 0x18 + 20 * [0x12b056] and refresh the widget chain. */
void show_main_widgets_7cc5c(void)
{
    ef_addr32 w;
    EF_U8(0x00137a00u) = 1u;
    guest_fn_e710();
    if (EF_U8(0x00125839u) == 1u) {
        w = find_named_or_abort_a46cc(0x00187f74u);
        activate_widget_a4cb4((uint16_t)EF_U8(w + 0x10u));
    }
    EF_U8(0x00125847u) = 1u;
    guest_fn_7bee8_x(0x001f8f74u);
    w = find_named_or_abort_a46cc(0x00187f38u);
    guest_fn_a520c(w, (int16_t)EF_U16(w + 0x45u), (uint32_t)(int16_t)(EF_U16(0x0012b056u) * 0x14u + 0x18u), 0u);
    update_widget_chain_7ca78();
}

/* 0xd1c54, 70 bytes. Start a log: remember [0x15b744] / 8 at 0x15b7cc, open the log file NAME, emit the display list, log the palette SRC (mode MODE), run 0xd1afc, finish the log and close the file. */
void write_log_d1c54(ef_addr32 name, ef_addr32 src, int32_t mode)
{
    EF_U32(0x0015b7ccu) = (uint32_t)((int32_t)EF_U32(0x0015b744u) / 8);
    open_log_d1774(name);
    emit_display_list_d17d0();
    log_palette_d1894(src, (int16_t)mode);
    guest_fn_d1afc_x();
    finish_log_d1bec();
    call_a7990_420944();
}

/* 0x110130, 60 bytes. Wrapper for 0x10e4dd on the double (S0, S1): control word (S2 & 0x1f) with 0x1080 when S2 is not 8 and the value is +/-0, otherwise 0x1040 (ret 12). */
void classify_double_110130(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, uint32_t s1, uint32_t s2)
{
    uint32_t v[2];
    uint32_t w = s2 & 0x1fu;
    (void)u0; (void)u1; (void)u2; (void)u3;
    v[0] = s0;
    v[1] = s1;
    if (w != 8u && (s1 & 0x7fffffffu) == 0u && s0 == 0u)
        w |= 0x1080u;
    else
        w |= 0x1040u;
    run_with_ebx_10e4d3(w, (uint32_t)(uintptr_t)v);
}

/* 0xf017a, 69 bytes. Convert the single precision float BITS to an 80-bit extended value at OUT (mantissa at +0/+4, sign and exponent at +8). ORIGINAL BUG: zero and denormal inputs store the raw float bits in the mantissa high word and a zero exponent, and a NaN whose only fraction bit is the top one becomes an infinity. */
void float_to_extended_f017a(uint32_t bits, ef_addr32 out)
{
    uint32_t e = (bits >> 23) & 0xffu;
    uint32_t lo = 0u;
    uint32_t hi = bits;
    uint32_t ex = 0u;
    if (e != 0u) {
        hi = bits << 8;
        if (e == 0xffu) {
            ex = 0x7fffu;
            hi &= 0x7fffffffu;
            if (hi != 0u)
                hi |= 0x40000000u;
        } else {
            ex = e + 0x3f80u;
        }
        ex = ((bits >> 31) << 15) | (ex & 0x7fffu);
        hi |= 0x80000000u;
    }
    EF_U32(out) = lo;
    EF_U32(out + 4u) = hi;
    EF_U16(out + 8u) = (uint16_t)ex;
}

/* 0xebdcc, 75 bytes. Convert the decimal digit string TEXT (digit characters only, low nibble used) to a double stored at OUT: the digits are accumulated into a 64-bit integer which is normalised with the exponent word 0x43300000 (0xebe17) and rounded half up on the first dropped bit. */
void decimal_string_to_double_ebdcc(ef_addr32 text, ef_addr32 out)
{
    uint32_t lo = 0u;
    uint32_t hi = 0u;
    uint32_t expo = 0x43300000u;
    uint32_t c;
    while ((c = EF_U8(text)) != 0u) {
        uint32_t olo = lo;
        uint32_t ohi = hi;
        uint64_t v = ((uint64_t)ohi << 32) | olo;
        v = v * 10u + (c & 0xfu);
        lo = (uint32_t)v;
        hi = (uint32_t)(v >> 32);
        text++;
    }
    if ((lo | hi) != 0u) {
        uint32_t guard = 0u;
        if ((hi & 0xfff00000u) == 0u) {
            do {
                expo -= 0x100000u;
                hi = (hi << 1) | (lo >> 31);
                lo <<= 1;
            } while ((hi & 0xfff00000u) == 0u);
        } else if ((hi & 0xffe00000u) != 0u) {
            uint32_t sticky = 0u;
            do {
                expo += 0x100000u;
                sticky = (guard << 1) | 0u;
                guard = lo & 1u;
                lo = (lo >> 1) | (hi << 31);
                hi >>= 1;
            } while ((hi & 0xffe00000u) != 0u);
            (void)sticky;
            lo += guard;
            if (lo == 0u && guard != 0u)
                hi += 1u;
        }
        hi = (hi & 0xfffffu) | expo;
    }
    EF_U32(out + 4u) = hi;
    EF_U32(out) = lo;
}

/* 0x9be0d, 40 bytes. Store the current segment registers into the 12-byte record at OUT (ES, CS, SS, DS, FS, GS as words, like segread). */
void segread_9be0d(ef_addr32 out)
{
    EF_U16(out + 2u) = (uint16_t)guest_seg_x(1u);
    EF_U16(out + 6u) = (uint16_t)guest_seg_x(3u);
    EF_U16(out) = (uint16_t)guest_seg_x(0u);
    EF_U16(out + 4u) = (uint16_t)guest_seg_x(2u);
    EF_U16(out + 8u) = (uint16_t)guest_seg_x(4u);
    EF_U16(out + 0xau) = (uint16_t)guest_seg_x(5u);
}

/* 0x9c47a, 56 bytes. The software interrupt dispatcher behind int386x (registers ESI = interrupt number, EDI = input register block, EDX = output register block, EBX = segment register block): load the registers and the ES/DS selectors from the input blocks, run "int N" through the stub table at 0x9c558 (0x9c4b2), then store EAX..ESI/EDI and the carry flag (as 0/-1) into the output block at +0..+0x18 and the returned ES/DS back into the segment block. */
void int386x_core_9c47a(uint32_t intno, uint32_t in, uint32_t out, uint32_t sregs)
{
    guest_dos_int_x(intno, in, out, sregs);
}

/* 0x9c461, 25 bytes. int386x: perform software interrupt INTNO with the register blocks IN/OUT and the segment block SREGS; returns the EAX the interrupt produced. */
uint32_t int386x_9c461(uint32_t intno, uint32_t in, uint32_t out, uint32_t sregs)
{
    int386x_core_9c47a(intno, in, out, sregs);
    return EF_U32(out);
}

/* 0x9c446, 27 bytes. int386: int386x with a freshly read segment block. */
uint32_t int386_9c446(uint32_t intno, uint32_t in, uint32_t out)
{
    uint8_t sregs[12];
    segread_9be0d((uint32_t)(uintptr_t)sregs);
    return int386x_9c461(intno, in, out, (uint32_t)(uintptr_t)sregs);
}

/* 0xd6694, 46 bytes. Run the step 0xd66c2; when it succeeds (non-zero) run it five more times and return -1, otherwise return 0. */
uint32_t run_steps_d6694(void)
{
    if (guest_fn_d66c2_x() == 0u)
        return 0u;
    guest_fn_d66c2_x();
    guest_fn_d66c2_x();
    guest_fn_d66c2_x();
    guest_fn_d66c2_x();
    guest_fn_d66c2_x();
    return 0xffffffffu;
}

/* 0x107004, 56 bytes. Try 0x106f10 with the object A and the current slot index [0x464470] - 1: on success (0) store the index through OUT and return 0, otherwise release the object and return 2. */
uint8_t try_open_slot_107004(ef_addr32 a, ef_addr32 out)
{
    uint8_t r = (uint8_t)guest_fn_106f10_x(a, (uint8_t)(EF_U8(0x00464470u) - 1u));
    if (r == 0u) {
        EF_U8(out) = (uint8_t)(EF_U8(0x00464470u) - 1u);
        return r;
    }
    release_object_10703c(a);
    return 2u;
}

/* 0xea20c, 51 bytes. Hash the name TEXT (0xebfd0), place the hash in a 16-byte block (a callback address 0xea1e8 sits just below it on the stack) and run 0x10d9e3 on it with the object's fields at +0xc and +0x10. */
void hash_and_run_ea20c(ef_addr32 obj, ef_addr32 text)
{
    uint32_t block[4];
    block[0] = name_hash_ebfd0(text);
    guest_fn_10d9e3_x((uint32_t)(uintptr_t)block, EF_U32(obj + 0x10u), EF_U32(obj + 0xcu), 0x10u);
}

/* 0x113b78, 169 bytes. Classify the record pair CTX (cdecl): returns 3 when the name of the first record (+0xba, only when its size at +8 is at least 0x112) matches neither string at 0x1954fc nor 0x195510, otherwise looks up the name referenced by the second record's far pointer (segment base table 0x120cc6): 1 when it equals the string at 0x19552c and the size at +0xc is at most 0x5000, 2 when it equals the string at 0x195530, else 0. */
uint32_t classify_record_113b78(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 ctx)
{
    uint32_t rec = EF_U32(EF_U32(ctx) + 0x10u);
    uint32_t far_ptr;
    uint32_t name;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(rec + 8u) >= 0x112u) {
        name = rec + 0xbau;
        if (guest_fn_9d524(name, 0x001954fcu) == 0u)
            return 3u;
        if (guest_fn_9d524(name, 0x00195510u) == 0u)
            return 3u;
    }
    far_ptr = EF_U32(EF_U32(ctx + 4u) + 4u);
    name = (far_ptr >> 16) * 16u + (far_ptr & 0xffffu) + EF_U32(0x00120cc6u);
    if (name != 0u && EF_U8(name) != 0u) {
        if (guest_fn_9d524(name, 0x0019552cu) == 0u) {
            if (EF_U32(EF_U32(ctx) + 0xcu) <= 0x5000u)
                return 1u;
        } else if (guest_fn_9d524(name, 0x00195530u) == 0u) {
            return 2u;
        }
    }
    return 0u;
}

/* 0xf819c, 171 bytes. Load the text file NAME from the default path: build the path in a 144-byte buffer, append NAME and the suffix at 0x19322c, load it with the flags byte at 0x41f2df, report through 0x132a74 when the load returned non-zero (the case-2 message at 0x19326c is unreachable because the result is reduced to 0/1), reset the flag byte 0x41f2e2, rebuild both tables at 0x161fb8/0x1621b4, release the buffer and re-initialise the entities. */
void reload_text_file_f819c(ef_addr32 name)
{
    uint8_t buf[0x90];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    uint32_t r;
    build_default_path_abaf0(b);
    guest_fn_9ca57(b, name);
    guest_fn_9ca57(b, 0x0019322cu);
    r = (load_text_file_e94ec(b, EF_U8(0x0041f2dfu)) & 0xffu) != 0u;
    if (r == 1u) {
        guest_callptr3_x(EF_U32(0x00132a74u), 0x00193234u, name, b);
    } else if (r == 2u) {
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0019326cu, name);
    }
    EF_U8(0x0041f2e2u) = 0u;
    guest_fn_e93dc(0x00161fb8u, 0u);
    guest_fn_e93dc(0x001621b4u, 1u);
    release_buffer_e9574();
    init_entities_a59d8();
}

/* 0x9f664, 170 bytes. For the 6-byte point entry INDEX of the current page (table at 0x41717c + 0xa) copy its Y word to 0x417226, let the hook at 0x132af4 transform the point, add the offset 0x417194..98 and compute the bearing (out 0x417150) from the position 0x159094..9c to the offset point. */
void update_bearing_9f664(int32_t index)
{
    uint32_t e = EF_U32(0x0041717cu) + 0xau + (uint32_t)(index * 6);
    uint16_t from[3];
    uint16_t to[3];
    EF_U16(0x00417226u) = EF_U16(e + 2u);
    guest_callptr2r_x(EF_U32(0x00132af4u), (int16_t)EF_U16(e), (int16_t)EF_U16(e + 4u));
    to[0] = (uint16_t)(EF_U16(0x00417224u) + EF_U16(0x00417194u));
    to[1] = (uint16_t)(EF_U16(0x00417226u) + EF_U16(0x00417196u));
    to[2] = (uint16_t)(EF_U16(0x00417228u) + EF_U16(0x00417198u));
    from[0] = EF_U16(0x00159094u);
    from[1] = EF_U16(0x00159098u);
    from[2] = EF_U16(0x0015909cu);
    bearing_between_positions_dcaa4(0x00417150u, (uint32_t)(uintptr_t)from, (uint32_t)(uintptr_t)to);
}

/* 0xb54d0, 173 bytes. Compute the clock time that lies RATE steps (scaled by the divisor for the rate S0, ret 4) after the stored time hours at 0x1379a0 and minutes at 0x1379a2: stores hours (wrapped once at 24) at HOURS, the remaining minutes (signed remainder) at MINUTES and 0 at SECONDS. */
void add_time_offset_b54d0(ef_addr32 hours, ef_addr32 minutes, ef_addr32 seconds, int32_t rate, uint32_t rate_code)
{
    int16_t step = (int16_t)divisor_for_rate((int16_t)rate_code);
    int32_t base = (int16_t)EF_U16(0x001379a2u) + (int32_t)(int16_t)EF_U16(0x001379a0u) * 60;
    int32_t total = (int32_t)(int16_t)rate * step + base;
    EF_U16(hours) = (uint16_t)(total / 60);
    if ((int16_t)EF_U16(hours) >= 0x18)
        EF_U16(hours) = (uint16_t)(EF_U16(hours) - 0x18u);
    total = (int32_t)(int16_t)EF_U16(0x001379a0u) * 60 + (int16_t)EF_U16(0x001379a2u) + step * (int32_t)(int16_t)rate;
    EF_U16(minutes) = (uint16_t)(total % 60);
    EF_U16(seconds) = 0u;
}

/* 0x598e8, 175 bytes. Per-mode refresh of the current object (selected by the signed word at 0x1f71ca; nothing happens when it does not exist): mode byte 0x1282c5 - 2 selects the sequence of update steps 0x5a2d0, 0x59fa4, 0x5e658, 0x5a220, 0x5a468, 0x5e87c, 0x5ca60 or 0x5c518, and 0x59998 always runs last (except for a missing object). */
void refresh_current_object_598e8(void)
{
    uint32_t v;
    if (lookup_filtered_indexed_object((int16_t)EF_U16(0x001f71cau)) == 0u)
        return;
    switch ((uint8_t)(EF_U8(0x001282c5u) - 2u)) {
    case 0:
        guest_fn_5a2d0_x();
        reset_unit_counter();
        guest_fn_5a220_x();
        break;
    case 1:
        guest_fn_5a468_x();
        v = EF_U32(0x0019721cu);
        if ((int32_t)v <= 0x41 && (int32_t)v >= 0)
            guest_fn_5e87c_x((int16_t)EF_U16(0x0019721cu));
        break;
    case 2:
    case 3:
        guest_fn_5a2d0_x();
        break;
    case 4:
        guest_fn_5a2d0_x();
        guest_fn_5c518_x();
        break;
    case 5:
        guest_fn_5a2d0_x();
        guest_fn_59fa4_x();
        break;
    case 7:
        guest_fn_5a2d0_x();
        apply_stream_value_5ca60();
        break;
    default:
        break;
    }
    guest_fn_59998_x();
}

/* 0xa0174, 177 bytes. Advance the two vector pairs of the object OBJ: copy the vector at +0x3c to 0x132b28, then add the vector at +0x30 scaled by [0x132b20] to +0x24, the vector at +0x18 scaled by [0x132b20] to +0xc, and update +0x18 by (+0x18 scaled by [0x132b34]) + the vector at 0x132b28, scaled by [0x132b20] (all scaling truncates toward zero). */
void advance_object_vectors_a0174(ef_addr32 obj)
{
    uint32_t tmp[3];
    uint32_t t = (uint32_t)(uintptr_t)tmp;
    EF_U32(0x00132b28u) = EF_U32(obj + 0x3cu);
    EF_U32(0x00132b2cu) = EF_U32(obj + 0x40u);
    EF_U32(0x00132b30u) = EF_U32(obj + 0x44u);
    copy_vector3(t, obj + 0x30u);
    scale_vector3_ea8cc(t, t, 0u, 0u, EF_U32(0x00132b20u));
    add_vector3(obj + 0x24u, t);
    copy_vector3(t, obj + 0x18u);
    scale_vector3_ea8cc(t, t, 0u, 0u, EF_U32(0x00132b20u));
    add_vector3(obj + 0xcu, t);
    copy_vector3(t, obj + 0x18u);
    scale_vector3_ea8cc(t, 0x00132b28u, 0u, 0u, EF_U32(0x00132b34u));
    add_vector3(t, 0x00132b28u);
    scale_vector3_ea8cc(t, t, 0u, 0u, EF_U32(0x00132b20u));
    add_vector3(obj + 0x18u, t);
}

/* 0x1a3cc, 176 bytes. Step the demo script at 0x122c80: when the expected key code 0x41f833 matches the current script byte, clear it and advance the script position (word 0x122c7e); at the script end toggle the mode word 0x198438 and push the matching event (0x18188c or 0x18189c), otherwise a mismatch rewinds to the start. Only runs while 0x41f7c8 and 0x41f833 are non-zero. */
void step_demo_script_1a3cc(void)
{
    if (EF_U8(0x0041f7c8u) == 0u || EF_U8(0x0041f833u) == 0u)
        return;
    if (EF_U8(0x0041f833u) == EF_U8(0x00122c80u + (uint32_t)(int32_t)(int16_t)EF_U16(0x00122c7eu))) {
        EF_U8(0x0041f833u) = 0u;
        EF_U16(0x00122c7eu) = (uint16_t)(EF_U16(0x00122c7eu) + 1u);
        if (EF_U8(0x00122c80u + (uint32_t)(int32_t)(int16_t)EF_U16(0x00122c7eu)) != 0u)
            return;
        if ((int16_t)EF_U16(0x00198438u) != 1) {
            EF_U16(0x00122c7eu) = 0u;
            EF_U16(0x00198438u) = 1u;
            push_event_1f878(0x0018188cu);
        } else {
            EF_U16(0x00198438u) = 0u;
            EF_U16(0x00122c7eu) = 0u;
            push_event_1f878(0x0018189cu);
        }
    } else {
        EF_U16(0x00122c7eu) = 0u;
    }
}

/* 0xaa3dc, 176 bytes. Walk the linked list at 0x4191fc for as many entries as the signed word at 0x4194c4 says: copy each entry's name (+9) into the 21-byte slots at 0x419218, record its flag byte (+8) at 0x41948e+index, and for flagged entries allocate a 21-byte block (stored at 0x418708 + 4 * index) preset with the 5 bytes from 0x18d8e4, for unflagged ones store 0. Reports errors through 0x132a74 when the list ends early or an allocation fails. */
void load_list_names_aa3dc(void)
{
    uint32_t slot = 0x00419218u;
    uint32_t node = EF_U32(0x004191fcu);
    uint32_t idx = 0u;
    while ((int32_t)(int16_t)EF_U16(0x004194c4u) > (int32_t)idx) {
        uint32_t cell = 0x00418708u + idx * 4u;
        if (node == 0u)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x0018d8a8u);
        copy_string_9d749(slot, node + 9u);
        if (EF_U8(node + 8u) == 0u) {
            EF_U32(cell) = 0u;
        } else {
            EF_U32(cell) = guest_fn_c3f70(0x15u);
            if (EF_U32(cell) == 0u)
                guest_callptr1_x(EF_U32(0x00132a74u), 0x0018d8c0u);
            EF_U32(EF_U32(cell)) = EF_U32(0x0018d8e4u);
            EF_U8(EF_U32(cell) + 4u) = EF_U8(0x0018d8e8u);
        }
        idx++;
        slot += 0x15u;
        EF_U8(0x0041948du + idx) = EF_U8(node + 8u);
        node = EF_U32(node);
    }
}

/* 0xd3844, 178 bytes. Count how many of the four tiles around the object's cell (x, y)..(x+1, y+1), read with 0xc3f50, equal the value [0x159144] + 0xe0, add the count to 0x420a54 and the new total to 0x4209cc. */
void count_matching_tiles_d3844(ef_addr32 obj)
{
    uint32_t rec = EF_U32(EF_U32(obj + 0x4cu) + 4u);
    int32_t x = (int16_t)EF_U16(rec + 0x1au);
    int32_t y = (int16_t)EF_U16(rec + 0x1eu);
    uint32_t want = EF_U32(0x00159144u) + 0xe0u;
    uint32_t n = 0u;
    uint32_t total;
    n += (guest_fn_c3f50(x, y) & 0xffu) == want;
    n += (guest_fn_c3f50(x + 1, y) & 0xffu) == want;
    n += (guest_fn_c3f50(x + 1, y + 1) & 0xffu) == want;
    n += (guest_fn_c3f50(x, y + 1) & 0xffu) == want;
    total = EF_U32(0x00420a54u) + n;
    EF_U32(0x00420a54u) = total;
    EF_U32(0x004209ccu) += total;
}

/* 0x2a7f8, 178 bytes. When the larger of the two coordinate distances between (0x125250, 0x125254) and (0x1a5bec, 0x1a5be8) is at most 0x7fff, run 0x9e0c4 for every node of the list at 0x19bb3c (next pointer at +0x12) with the node's four words (the first three scaled by 65536), add the step [0x127d92] to the counter word 0x125258 and recycle the oldest grid node when it exceeds 1000. */
void update_grid_nodes_2a7f8(void)
{
    uint32_t dx = abs_cf94a((int32_t)(EF_U32(0x00125250u) - EF_U32(0x001a5becu)));
    uint32_t dy = abs_cf94a((int32_t)(EF_U32(0x00125254u) - EF_U32(0x001a5be8u)));
    uint32_t d = ((int32_t)dx > (int32_t)dy) ? abs_cf94a((int32_t)(EF_U32(0x00125250u) - EF_U32(0x001a5becu))) : abs_cf94a((int32_t)(EF_U32(0x00125254u) - EF_U32(0x001a5be8u)));
    uint32_t p;
    if ((int32_t)d > 0x7fff)
        return;
    for (p = EF_U32(0x0019bb3cu); p != 0u; p = EF_U32(p + 0x12u)) {
        guest_fn_9e0c4((int16_t)EF_U16(p + 6u), (uint32_t)(int32_t)(int16_t)EF_U16(p) << 16, (uint32_t)(int32_t)(int16_t)EF_U16(p + 2u) << 16, (uint32_t)(int32_t)(int16_t)EF_U16(p + 4u) << 16);
    }
    EF_U16(0x00125258u) = (uint16_t)(EF_U16(0x00125258u) + EF_U16(0x00127d92u));
    if ((int16_t)EF_U16(0x00125258u) > 0x3e8) {
        recycle_oldest_grid_node();
        EF_U16(0x00125258u) = 0u;
    }
}

/* 0xd5e0c, 179 bytes. Register the block REC (size at +5, address at +9) with the trace layer: touch each of its pages, mark the range (0x104908), start tracing the handle 0x420a64 on it (error through 0x132a74 when that fails), and remember REC at 0x420a68. Returns 1 for a null REC, 0 otherwise (and when tracing is disabled via 0x15b968). */
uint32_t register_trace_block_d5e0c(ef_addr32 rec)
{
    uint32_t off;
    if (EF_U8(0x0015b968u) == 0u)
        return 0u;
    if (rec == 0u)
        return 1u;
    for (off = 0u; (int32_t)off < (int32_t)EF_U32(rec + 5u); off += 0x1000u)
        (void)EF_U8(EF_U32(rec + 9u) + off);
    stack_range_104908(EF_U32(rec + 9u), EF_U32(rec + 5u));
    if ((int32_t)trace_call_1030bc(0u, 0u, 0u, 0u, EF_U32(0x00420a64u), EF_U32(rec + 9u), 0u) <= 0)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018f880u);
    trace_call_103244(EF_U32(0x00420a64u), 0u);
    trace_call_10311c(EF_U32(0x00420a64u));
    EF_U32(0x00420a68u) = rec;
    return 0u;
}

/* 0xcee88, 180 bytes. Add the direction (from the sine/cosine tables at 0x141e08/0x142e08, indexed by the negated angles A and B divided by 8) scaled by K (fixed point, 15 bits) to the three 16-bit components at VEC. */
void add_direction_offset_cee88(ef_addr32 vec, uint32_t a, uint32_t b, int32_t k)
{
    uint32_t i1 = ((0u - a) & 0xffffu) >> 3;
    uint32_t i2 = ((0u - b) & 0xffffu) >> 3;
    int32_t ta = (int16_t)EF_U16(0x00141e08u + i1 * 2u);
    int32_t tb = (int16_t)EF_U16(0x00142e08u + i1 * 2u);
    int32_t tc = (int16_t)EF_U16(0x00141e08u + i2 * 2u);
    int32_t td = (int16_t)EF_U16(0x00142e08u + i2 * 2u);
    int32_t k16 = (int16_t)k;
    int32_t dx = (int16_t)(-((tc * tb) >> 15));
    int32_t dy = (int16_t)(-ta);
    int32_t dz = (int16_t)((tb * td) >> 15);
    EF_U16(vec) = (uint16_t)((int16_t)EF_U16(vec) + ((dx * k16) >> 15));
    EF_U16(vec + 2u) = (uint16_t)((int16_t)EF_U16(vec + 2u) + ((dy * k16) >> 15));
    EF_U16(vec + 4u) = (uint16_t)((int16_t)EF_U16(vec + 4u) + ((dz * k16) >> 15));
}

/* 0xc94f4, 182 bytes. Clip the rectangle (X0, Y0)-(X1, Y1) to the screen size 0x15b744 x 0x15b748 (ret 4, Y1 on the stack); a rectangle entirely outside is ignored, otherwise store the clipped corners at 0x4208e6..0x4208ec and the colour byte at 0x4208e4 and run 0xcf78c. */
void set_clip_rect_c94f4(uint32_t colour, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    int32_t w = (int32_t)EF_U32(0x0015b744u);
    int32_t h = (int32_t)EF_U32(0x0015b748u);
    int16_t ax0 = (int16_t)x0;
    int16_t ay0 = (int16_t)y0;
    int16_t ax1 = (int16_t)x1;
    int16_t ay1 = (int16_t)y1;
    if (ax0 < 0 && ax1 < 0)
        return;
    if (ay0 < 0 && ay1 < 0)
        return;
    if (ax0 >= w && ax1 >= w)
        return;
    if (ay0 >= h && ay1 >= h)
        return;
    if (ax0 < 0)
        ax0 = 0;
    if (ay0 < 0)
        ay0 = 0;
    if (ax1 >= w)
        ax1 = (int16_t)(EF_U16(0x0015b744u) - 1u);
    if (ay1 >= h)
        ay1 = (int16_t)(EF_U16(0x0015b748u) - 1u);
    EF_U16(0x004208e6u) = (uint16_t)ax0;
    EF_U16(0x004208e8u) = (uint16_t)ay0;
    EF_U16(0x004208eau) = (uint16_t)ax1;
    EF_U16(0x004208ecu) = (uint16_t)ay1;
    EF_U16(0x004208e4u) = (uint16_t)(colour & 0xffu);
    forward_record_cf78c();
}

/* 0xd2320, 182 bytes. Prepare the shape list of OBJ (list at +0x4c: count, then entry pointers): shift each entry's three coordinates (+0x24, +0x28, +0x2c) by 0x8000, append a copy of the pointer table after the end, and record in the object the index of the entry with the smallest key (+0x1c) at +8, the index of the entry with the largest key (ties broken by +0x18; 0 maps to the count) at +0xc, and the smallest and largest keys at +0x18 and +0x1c. */
void prepare_shape_list_d2320(ef_addr32 obj)
{
    uint32_t list = EF_U32(obj + 0x4cu);
    int32_t count = (int32_t)EF_U32(list);
    int32_t min_key = 0x7fffffff;
    int32_t max_key = (int32_t)0x80000000u;
    int32_t max_lo = (int32_t)0x80000000u;
    int32_t min_idx = 0;
    int32_t max_idx = 0;
    int32_t i;
    for (i = 0; i < count; i++) {
        uint32_t e = EF_U32(list + (uint32_t)(i + 1) * 4u);
        int32_t key;
        EF_U32(e + 0x24u) += 0x8000u;
        EF_U32(e + 0x28u) += 0x8000u;
        EF_U32(e + 0x2cu) += 0x8000u;
        key = (int32_t)EF_U32(e + 0x1cu);
        if (key < min_key) {
            min_key = key;
            min_idx = i;
        }
        if (key >= max_key) {
            if (key > max_key || (int32_t)EF_U32(e + 0x18u) >= max_lo) {
                max_idx = i;
                max_lo = (int32_t)EF_U32(e + 0x18u);
            }
            max_key = key;
        }
        EF_U32(list + (uint32_t)(count + i + 1) * 4u) = e;
    }
    EF_U32(obj + 8u) = (uint32_t)min_idx;
    EF_U32(obj + 0xcu) = (max_idx != 0) ? (uint32_t)max_idx : (uint32_t)count;
    EF_U32(obj + 0x18u) = (uint32_t)min_key;
    EF_U32(obj + 0x1cu) = (uint32_t)max_key;
}

/* 0xe58a8, 183 bytes. Clip the rectangle at 0x41ec08 (x, y, width, height words) to the screen: a negative origin moves into the size, a rectangle reaching past the screen width 0x15b744 / height 0x15b748 is shortened. */
void clip_rect_to_screen_e58a8(void)
{
    int16_t x = (int16_t)EF_U16(0x0041ec08u);
    int16_t y;
    if (x < 0) {
        EF_U16(0x0041ec0cu) = (uint16_t)(EF_U16(0x0041ec0cu) + (uint16_t)x);
        EF_U16(0x0041ec08u) = 0u;
    }
    if ((int32_t)(int16_t)EF_U16(0x0041ec0cu) + (int32_t)(int16_t)EF_U16(0x0041ec08u) >= (int32_t)EF_U32(0x0015b744u))
        EF_U16(0x0041ec0cu) = (uint16_t)(EF_U16(0x0015b744u) - EF_U16(0x0041ec08u));
    y = (int16_t)EF_U16(0x0041ec0au);
    if (y < 0) {
        EF_U16(0x0041ec0eu) = (uint16_t)(EF_U16(0x0041ec0eu) + (uint16_t)y);
        EF_U16(0x0041ec0au) = 0u;
    }
    if ((int32_t)(int16_t)EF_U16(0x0041ec0eu) + (int32_t)(int16_t)EF_U16(0x0041ec0au) >= (int32_t)EF_U32(0x0015b748u))
        EF_U16(0x0041ec0eu) = (uint16_t)(EF_U16(0x0015b748u) - EF_U16(0x0041ec0au));
}

/* 0x4f718, 183 bytes. Update a row of four button widgets of the group IDX under the root named at 0x184a8c: clear the dirty flag of widgets "fmt(1..4, IDX)" (format 0x184e3c) and then enable the widget for MODE (1..4 remapped 1->3, 2->4, 3->1, 4->2; 0 leaves them all cleared). */
void select_button_state_4f718(uint32_t idx, uint32_t mode)
{
    uint8_t buf[0x14];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    ef_addr32 root = find_named_or_abort_a46cc(0x00184a8cu);
    uint32_t n;
    for (n = 1u; n < 5u; n++) {
        guest_sprintf_a4342(b, 0x00184e3cu, n, idx & 0xffu);
        guest_fn_a7380(find_widget_checked_a78bc(root, b));
    }
    mode &= 0xffu;
    if (mode == 0u)
        return;
    switch (mode) {
    case 1: mode = 3u; break;
    case 2: mode = 4u; break;
    case 3: mode = 1u; break;
    case 4: mode = 2u; break;
    default: break;
    }
    guest_sprintf_a4342(b, 0x00184e3cu, mode, idx & 0xffu);
    guest_fn_a72d8(find_widget_checked_a78bc(root, b));
}

/* 0xe330c, 183 bytes. Decode a run/literal stream from the resource into DST until COUNT bytes are produced: each record is a run length (fill with FILL), a literal length and that many literal bytes; the stream (a 16-bit length followed by the data, at most 0x800 bytes) is read with 0xe3170. Reports an error through 0x132a74 when a record reads past the end of the data. */
void decode_run_literal_e330c(uint32_t fill, uint32_t dst, uint32_t count)
{
    uint8_t buf[0x800];
    uint16_t len = 0u;
    const uint8_t *p = buf;
    const uint8_t *end;
    uint16_t left = (uint16_t)count;
    read_resource_e3170((uint32_t)(uintptr_t)&len, 2u);
    read_resource_e3170((uint32_t)(uintptr_t)buf, len);
    end = buf + len;
    while (left != 0u) {
        uint16_t run = *p++;
        uint16_t lit;
        if (run != 0u) {
            do {
                EF_U8(dst) = (uint8_t)fill;
                dst++;
                run--;
                left--;
            } while (run != 0u);
        }
        lit = *p++;
        if (lit != 0u) {
            do {
                EF_U8(dst) = *p++;
                dst++;
                lit--;
                left--;
            } while (lit != 0u);
        }
        if (p > end)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x00190910u);
    }
}

/* 0x106b1c, 184 bytes. One-time runtime teardown guarded by the flag at 0x17d360: register the handler pair (0x105230, 0x106b1c) with 0x1048b4 and mark nine ranges through 0x104920 (0x17d2dc..0x4dd850), then clear the flag. */
void teardown_once_106b1c(void)
{
    if (EF_U32(0x0017d360u) == 0u)
        return;
    guest_fn_1048b4(0x00105230u, 0x00106b1cu);
    stack_range_104920(0x0017d2dcu, 0x80u);
    stack_range_104920(0x0017d35cu, 4u);
    stack_range_104920(0x004dd84cu, 4u);
    stack_range_104920(0x004dd840u, 0xcu);
    stack_range_104920(0x004dd850u, 4u);
    stack_range_104920(0x004dd83cu, 4u);
    stack_range_104920(0x004dd834u, 4u);
    stack_range_104920(0x004dd838u, 4u);
    stack_range_104920(0x004dd830u, 4u);
    EF_U32(0x0017d360u) = 0u;
}

/* 0x35f04, 184 bytes. Walk the byte list at [0x136b74] (length byte at [[0x136b78]]): for every entry whose descriptor (32-byte records at 0x135878) has type 3 and whose key byte equals [0x19f5a6] (for key 0x48 also the word at +1 must equal [0x19f594]), set the per-slot flag 0x19f318 + slot (slot = byte at descriptor +6) and count newly set flags in the word 0x19f58e. */
void mark_matching_slots_35f04(void)
{
    uint16_t found = EF_U16(0x0019f58eu);
    uint32_t list = EF_U32(0x00136b74u);
    int32_t i;
    for (i = 0; i < (int32_t)EF_U8(EF_U32(0x00136b78u)); i++) {
        uint32_t id = EF_U8(list + (uint32_t)i);
        uint32_t desc = EF_U32(0x00135878u + (id << 5));
        uint32_t key = EF_U32(0x0013587cu + (id << 5));
        uint32_t slot;
        if (EF_U16(desc + 7u) != 3u)
            continue;
        if (EF_U8(key) != EF_U8(0x0019f5a6u))
            continue;
        /* The original compares the key against 0x48 and then edx with itself, so the word test below is always taken for key 0x48. */
        if (EF_U8(0x0019f5a6u) == 0x48u && EF_U16(key + 1u) != EF_U16(0x0019f594u))
            continue;
        slot = EF_U8(EF_U32(0x00135878u + (id << 5)) + 6u);
        if (EF_U8(0x0019f318u + slot) == 0u) {
            EF_U8(0x0019f318u + slot) = 1u;
            found++;
        }
    }
    EF_U32(0x00136b74u) = list;
    EF_U16(0x0019f58eu) = found;
}

/* 0xd5d18, 188 bytes. Start the trace layer once (returns -1 when already started): set the started flag 0x15b968, configure it with A, initialise the module, open the trace file named at 0x420a6c (undoing the start and reporting through 0x132a74 when that fails), obtain the handle 0x420a64, note the level byte 0x420a71, register the handler 0xd5cc4 and finish with 0x1034b0. */
uint32_t start_trace_d5d18(uint32_t a)
{
    if (EF_U8(0x0015b968u) != 0u)
        return 0xffffffffu;
    EF_U8(0x0015b968u) = 1u;
    trace_call_103504(a);
    init_module_d6dc4();
    if (trace_call_102f60(0u, 0u, 0u, 0u, 0x00420a6cu) != 0u) {
        trace_call_101ddc();
        EF_U8(0x0015b968u) = 0u;
        EF_U8(0x0015bac5u) = 0u;
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018f858u, EF_U32(0x0015b964u));
    }
    EF_U32(0x00420a64u) = trace_call_10303c(0u, 0u, 0u, 0u, EF_U32(0x00420a6cu));
    if (EF_U32(0x00420a64u) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018f868u, 0x004dd6b0u);
    note_value_d5fc8(EF_U8(0x00420a71u));
    guest_fn_9c3cf(0x000d5cc4u);
    return trace_call_1034b0(0u, 0u, 0u, 0u, EF_U32(0x00420a6cu));
}

/* 0x105230, 191 bytes. One-time runtime init guarded by the flag at 0x17d360: register the handler pair (0x105230, 0x106b1c), mark nine ranges (0x17d2dc..0x4dd850) through 0x104908, run 0x117ace and set the flag. */
void init_once_105230(void)
{
    if (EF_U32(0x0017d360u) != 0u)
        return;
    guest_fn_104840(0x00105230u, 0x00106b1cu);
    stack_range_104908(0x0017d2dcu, 0x80u);
    stack_range_104908(0x0017d35cu, 4u);
    stack_range_104908(0x004dd84cu, 4u);
    stack_range_104908(0x004dd840u, 0xcu);
    stack_range_104908(0x004dd850u, 4u);
    stack_range_104908(0x004dd83cu, 4u);
    stack_range_104908(0x004dd834u, 4u);
    stack_range_104908(0x004dd838u, 4u);
    stack_range_104908(0x004dd830u, 4u);
    guest_fn_117ace_x();
    EF_U32(0x0017d360u) = 1u;
}

/* 0x50b68, 193 bytes. Store the state code for KIND (0..11 -> 0, 1, 2, 3, 4, 5, 0x12, 7, 6, 0x10, 0x11, 0x16) in the byte at 0x1a553b + INDEX; other kinds leave it unchanged. */
void set_slot_state_50b68(uint32_t index, uint32_t kind)
{
    static const uint8_t codes[12] = { 0, 1, 2, 3, 4, 5, 0x12, 7, 6, 0x10, 0x11, 0x16 };
    kind &= 0xffu;
    if (kind > 11u)
        return;
    EF_U8(0x001a553bu + (index & 0xffu)) = codes[kind];
}

/* 0xcf960, 194 bytes. Test whether the point (PX, PY) lies inside (or on the edge of) the triangle whose three 16-bit (x, y) vertices are stored at TRI: returns 1 when all three edge cross products are not positive, else 0. */
uint32_t point_in_triangle_cf960(ef_addr32 tri, int32_t px, int32_t py)
{
    int32_t x0 = (int16_t)EF_U16(tri);
    int32_t y0 = (int16_t)EF_U16(tri + 2u);
    int32_t x1 = (int16_t)EF_U16(tri + 4u);
    int32_t y1 = (int16_t)EF_U16(tri + 6u);
    int32_t x2 = (int16_t)EF_U16(tri + 8u);
    int32_t y2 = (int16_t)EF_U16(tri + 0xau);
    if ((x1 - x0) * (py - y1) - (px - x1) * (y1 - y0) > 0)
        return 0u;
    if ((x2 - x1) * (py - y2) - (y2 - y1) * (px - x2) > 0)
        return 0u;
    if ((x0 - x2) * (py - y0) - (px - x0) * (y0 - y2) > 0)
        return 0u;
    return 1u;
}

/* 0x9f710, 189 bytes. Like 0x9f664 for an explicit point: store Y at 0x417226, let the hook at 0x132af4 transform (A, Z), add the offset 0x417194..98 to the result (written back to 0x417224..28), build the relative position to 0x159094..9c and the three words at +2..+6 of the record at [0x417174], and run the hook at 0x132ae0 on them with the scale C. */
void transform_and_relate_9f710(int32_t a, uint32_t y, int32_t z, int32_t c)
{
    uint16_t rel[3];
    uint16_t rec[3];
    uint32_t r = EF_U32(0x00417174u);
    EF_U16(0x00417226u) = (uint16_t)y;
    guest_callptr2r_x(EF_U32(0x00132af4u), (int16_t)a, (int16_t)z);
    EF_U16(0x00417224u) = (uint16_t)(EF_U16(0x00417224u) + EF_U16(0x00417194u));
    EF_U16(0x00417226u) = (uint16_t)(EF_U16(0x00417226u) + EF_U16(0x00417196u));
    EF_U16(0x00417228u) = (uint16_t)(EF_U16(0x00417228u) + EF_U16(0x00417198u));
    rel[0] = (uint16_t)(EF_U16(0x00417224u) - EF_U16(0x00159094u));
    rel[1] = (uint16_t)(EF_U16(0x00417226u) - EF_U16(0x00159098u));
    rel[2] = (uint16_t)(EF_U16(0x00417228u) - EF_U16(0x0015909cu));
    rec[0] = EF_U16(r + 2u);
    rec[1] = EF_U16(r + 4u);
    rec[2] = EF_U16(r + 6u);
    guest_callptr3r_x(EF_U32(0x00132ae0u), (uint32_t)(uintptr_t)rel, (uint32_t)(uintptr_t)rec, (uint32_t)(int16_t)c);
}

/* 0xe5cf0, 196 bytes. Draw the 7x7 bitmap glyph GLYPH (7 row bytes at 0x15fab0 + 7 * GLYPH, bit 7 first) at (X, Y) in the colour [0x433eb0], clipped to the rectangle [0x17cf9c..0x17cfa0] x [0x17cfa4..0x17cfa8]. */
void draw_glyph_7x7_e5cf0(int32_t x, int32_t y, int32_t glyph)
{
    uint32_t dst = EF_U32(0x0015b754u) + EF_U32(0x00163200u + (uint32_t)y * 4u) + (uint32_t)x;
    uint32_t bits = 0x0015fab0u + (uint32_t)glyph * 7u;
    int32_t end = y + 7;
    int32_t left = (int32_t)EF_U32(0x0017cf9cu);
    for (; y != end; y++) {
        uint32_t row = EF_U8(bits);
        bits++;
        if (y >= (int32_t)EF_U32(0x0017cfa4u) && y <= (int32_t)EF_U32(0x0017cfa8u)) {
            uint8_t mask = 0x80u;
            uint32_t d = dst;
            int32_t cx = x;
            for (; d != dst + 7u; d++, cx++) {
                if (cx >= left && cx <= (int32_t)EF_U32(0x0017cfa0u) && (row & mask) != 0u)
                    EF_U8(d) = EF_U8(0x00433eb0u);
                mask >>= 1;
            }
        }
        dst += EF_U32(0x001631f4u);
    }
}

/* 0xd1924, 197 bytes. Run-length encode [0x15b7cc] bytes from SRC into DST (PackBits style: a control byte 1-L followed by one byte for a run of L equal bytes, or L-1 followed by L literal bytes). When the encoding is longer than the input, store the input raw instead (first byte length-1). Returns the number of bytes written. */
uint32_t rle_encode_d1924(ef_addr32 dst, ef_addr32 src)
{
    int32_t n = (int32_t)EF_U32(0x0015b7ccu);
    uint32_t in = src;
    uint32_t out = dst;
    int32_t used = 0;
    int32_t total = 0;
    while (n > 0 && total < n) {
        uint32_t ctl = out;
        if (EF_U8(in) == EF_U8(in + 1u)) {
            uint8_t v = EF_U8(in);
            EF_U8(ctl) = 0u;
            out++;
            EF_U8(out) = v;
            out++;
            do {
                EF_U8(ctl) = (uint8_t)(EF_U8(ctl) + 1u);
                in++;
                total++;
            } while (v == EF_U8(in) && total < n);
            EF_U8(ctl) = (uint8_t)(0u - (uint8_t)(EF_U8(ctl) - 1u));
            used += 2;
        } else {
            EF_U8(ctl) = 0u;
            out++;
            do {
                EF_U8(out) = EF_U8(in);
                EF_U8(ctl) = (uint8_t)(EF_U8(ctl) + 1u);
                in++;
                total++;
                out++;
            } while (EF_U8(in) != EF_U8(in + 1u) && total < n);
            used += (int32_t)EF_U8(ctl) + 1;
            EF_U8(ctl) = (uint8_t)(EF_U8(ctl) - 1u);
        }
    }
    if (used > (int32_t)EF_U32(0x0015b7ccu)) {
        EF_U8(dst) = (uint8_t)(EF_U32(0x0015b7ccu) - 1u);
        guest_fn_dc832(dst + 1u, src, EF_U32(0x0015b7ccu));
        used = (int32_t)EF_U32(0x0015b7ccu) + 1;
    }
    return (uint32_t)used;
}

/* 0x5ee80, 198 bytes. Find the entry (0x54-byte records at 0x1a6b3c, count byte at [[0x1a6b08] + 0x13]) whose position is within 0x7fff of the view origin (0x1a5bec, 0x1a5be8) and whose offset, rotated by the record's angle, lies inside the limits at +0x1a6b44/46; returns [0x1a6b08] for the first hit, else 0. Does nothing while [0x1a6b04] is zero. */
ef_addr32 find_entry_in_range_5ee80(void)
{
    int16_t i;
    int16_t n;
    if (EF_U32(0x001a6b04u) == 0u)
        return 0u;
    n = (int16_t)EF_U8(EF_U32(0x001a6b08u) + 0x13u);
    for (i = 0; i < n; i++) {
        uint32_t rec = (uint32_t)(int32_t)i * 0x54u;
        int32_t dx = (int32_t)(EF_U32(0x001a5becu) - EF_U32(0x001a6b3cu + rec));
        int32_t dy = (int32_t)(EF_U32(0x001a5be8u) - EF_U32(0x001a6b40u + rec));
        uint16_t out[2];
        if (dx >= 0x7fff || dy >= 0x7fff)
            continue;
        load_sincos_dc7bc(EF_U16(0x001a6b48u + rec));
        rotate_point_dc7e4((uint32_t)(uintptr_t)out, (int16_t)dx, (int16_t)dy);
        if ((int32_t)abs_a28f2((int16_t)EF_U16(0x001a6b44u + rec)) > (int32_t)abs_a28f2((int16_t)out[0])
            && (int32_t)abs_a28f2((int16_t)EF_U16(0x001a6b46u + rec)) > (int32_t)abs_a28f2((int16_t)out[1]))
            return EF_U32(0x001a6b08u);
    }
    return 0u;
}

/* 0x801e4, 198 bytes. Claim a UI control slot for the object INDEX: when the feature byte 0x12b070 is set and the count (0x12b07c when FLAG, else the byte 0x12b078) is non-zero, give up if the object is already shown in one of the ten slots (0x1f9088) in an active state; otherwise take a device slot for its class (0xd70a0) and dispatch the control (0x7fbec). */
void claim_ui_slot_801e4(int32_t index, uint32_t arg, uint32_t flag)
{
    uint32_t count = 0u;
    uint32_t slot;
    uint8_t i;
    if (EF_U8(0x0012b070u) == 0u)
        return;
    if ((flag & 0xffu) != 0u)
        count = EF_U32(0x0012b07cu);
    else
        count = EF_U8(0x0012b078u);
    if (count == 0u)
        return;
    if (read_field_d7748(EF_U32(EF_U32(0x001f9078u) + (uint32_t)(int32_t)(int16_t)index * 4u)) != 0u) {
        for (i = 0u; i < 10u; i++) {
            if ((uint16_t)EF_U8(0x001f9088u + i) == (uint16_t)index && (guest_fn_d7594(i) & 0xffu) == 0u)
                return;
        }
    }
    if ((flag & 0xffu) != 0u) {
        count = EF_U32(0x0012b07cu);
        if (count == 0u)
            return;
    } else {
        count = 0u;
        if (EF_U8(0x0012b078u) == 0u)
            return;
    }
    slot = claim_device_slot_d70a0((uint8_t)read_field_d7738(EF_U32(EF_U32(0x001f9078u) + (uint32_t)(int32_t)(int16_t)index * 4u)));
    if ((slot & 0xffu) == 0xeu)
        return;
    EF_U8(0x001f9088u + (slot & 0xffu)) = (uint8_t)index;
    guest_fn_7fbec(slot & 0xffu, arg & 0xffu, (uint32_t)(int32_t)(int16_t)index, count);
}

/* 0x48c90, 201 bytes. Draw the formatted string FMT (cdecl, variadic) at (X, Y) in the colour byte COLOR with the renderer 0xe605c (doubled coordinates when [0x12b369] is 1) or 0x15a14; when the shadow flag 0x127b1a is set a colour 3 copy is drawn one pixel further first. Clears the shadow flag afterwards. */
void draw_shadowed_text_48c90(uint32_t color, int32_t x, int32_t y, ef_addr32 fmt, ...)
{
    char buf[0x78];
    va_list ap;
    va_start(ap, fmt);
    format_to_buffer_e3022((uint32_t)(uintptr_t)buf, fmt, (uint32_t)(uintptr_t)&ap[0]);
    va_end(ap);
    if (EF_U8(0x0012b369u) == 1u) {
        if (EF_U8(0x00127b1au) != 0u)
            guest_fn_e605c_x(3u, (int16_t)(((int16_t)x) * 2 + 1), (int16_t)(((int16_t)y) * 2 + 1), (uint32_t)(uintptr_t)buf);
        guest_fn_e605c_x(color & 0xffu, (int16_t)(((int16_t)x) * 2), (int16_t)(((int16_t)y) * 2), (uint32_t)(uintptr_t)buf);
    } else {
        if (EF_U8(0x00127b1au) != 0u)
            guest_fn_15a14_x(3u, (int16_t)(x + 1), (int16_t)(y + 1), (uint32_t)(uintptr_t)buf);
        guest_fn_15a14_x(color & 0xffu, (int16_t)x, (int16_t)y, (uint32_t)(uintptr_t)buf);
    }
    EF_U8(0x00127b1au) = 0u;
}

/* 0xf314c, 200 bytes. Read a list of number pairs from the script into the table entry [0x1616e8]: each pair (two parsed integers, stored as words at 0x484be2/0x484be4 + 0x2a * entry + 4 * n) is followed by a token; the list ends when the token equals the string at 0x19198c. Stores the pair count at 0x484be0 + 0x2a * entry and advances the entry counter. Errors (through 0x132a74) when the entry counter reaches 0x28 or the list has 10 pairs. */
void read_pair_list_f314c(void)
{
    int16_t n = 0;
    int32_t entry = (int16_t)EF_U16(0x001616e8u);
    uint32_t base;
    if (entry >= 0x28)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0019191cu);
    read_script_token_f2cd0();
    do {
        if (n == 10)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x00191954u);
        base = (uint32_t)((int32_t)(int16_t)EF_U16(0x001616e8u) * 0x2a);
        EF_U16(0x00484be2u + base + (uint32_t)(int32_t)n * 4u) = (uint16_t)guest_fn_9d4cd(0x0048592cu);
        read_script_token_f2cd0();
        base = (uint32_t)((int32_t)(int16_t)EF_U16(0x001616e8u) * 0x2a);
        EF_U16(0x00484be4u + base + (uint32_t)(int32_t)n * 4u) = (uint16_t)guest_fn_9d4cd(0x0048592cu);
        read_script_token_f2cd0();
        n++;
    } while (guest_fn_a4380(0x0048592cu, 0x0019198cu) != 0u);
    base = (uint32_t)((int32_t)(int16_t)EF_U16(0x001616e8u) * 0x2a);
    EF_U16(0x00484be0u + base) = (uint16_t)n;
    EF_U16(0x001616e8u) = (uint16_t)(EF_U16(0x001616e8u) + 1u);
}

/* 0xcec00, 201 bytes. Fill the rectangle (X0, Y0)-(X1, Y1) with the colour byte COLOR, clipped to the rectangle [0x15bdc0..0x15bdc4] x [0x15bdc8..0x15bdcc] (ret 4; Y1 on the stack). The corners may be given in any order; a rectangle entirely outside is ignored. */
void fill_rect_clipped_cec00(uint32_t color, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int32_t left = (int32_t)EF_U32(0x0015bdc0u);
    int32_t right = (int32_t)EF_U32(0x0015bdc4u);
    int32_t top = (int32_t)EF_U32(0x0015bdc8u);
    int32_t bottom = (int32_t)EF_U32(0x0015bdccu);
    uint32_t dst;
    int32_t width;
    int32_t t;
    if (x0 > x1) {
        t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        t = y0;
        y0 = y1;
        y1 = t;
    }
    if (x0 > right || y0 > bottom || x1 < left || y1 < top)
        return;
    if (x0 < left)
        x0 = left;
    if (x1 > right)
        x1 = right;
    if (y0 < top)
        y0 = top;
    if (y1 > bottom)
        y1 = bottom;
    dst = EF_U32(0x0015b754u) + EF_U32(0x00163200u + (uint32_t)y0 * 4u) + (uint32_t)x0;
    width = x1 - x0 + 1;
    for (; y0 <= y1; y0++) {
        fill_bytes_b3fb0(dst, (uint8_t)color, (uint32_t)width);
        dst += EF_U32(0x001631f4u);
    }
}

/* 0x65570, 204 bytes. Insert the record REC (fixed point position at +0 and +8, ids at +0xc and +0xe) into the list of the six nearest records at 0x1a6d30 (8-byte entries: word at +0xe, byte at +0xc as word, squared distance to the view origin 0x1a5bec/0x1a5be8 as dword), kept sorted by distance; the count word 0x1a81b2 is capped at 6. Only active while the byte 0x1a81e5 is set. */
void insert_nearest_65570(ef_addr32 rec)
{
    int16_t count = (int16_t)EF_U16(0x001a81b2u);
    if (EF_U8(0x001a81e5u) != 0u) {
        int32_t d1 = ((int32_t)EF_U32(0x001a5be8u) - ((int32_t)EF_U32(rec + 8u) >> 8)) >> 8;
        int32_t d0 = ((int32_t)EF_U32(0x001a5becu) - ((int32_t)EF_U32(rec) >> 8)) >> 8;
        int32_t dist = d0 * d0 + d1 * d1;
        int16_t i = 0;
        int16_t k;
        if (count > 0) {
            do {
                if (dist <= (int32_t)EF_U32(0x001a6d34u + (uint32_t)(int32_t)i * 8u))
                    break;
                i++;
            } while (i < count);
        }
        if (i < count && i < 5) {
            k = 5;
            do {
                EF_U32(0x001a6d30u + (uint32_t)(int32_t)k * 8u) = EF_U32(0x001a6d28u + (uint32_t)(int32_t)k * 8u);
                EF_U32(0x001a6d34u + (uint32_t)(int32_t)k * 8u) = EF_U32(0x001a6d2cu + (uint32_t)(int32_t)k * 8u);
                k--;
            } while (k > i);
        }
        if ((int32_t)i < 6) {
            EF_U16(0x001a6d30u + (uint32_t)(int32_t)i * 8u) = EF_U16(rec + 0xeu);
            EF_U16(0x001a6d32u + (uint32_t)(int32_t)i * 8u) = EF_U8(rec + 0xcu);
            count++;
            EF_U32(0x001a6d34u + (uint32_t)(int32_t)i * 8u) = (uint32_t)dist;
        }
        if ((int32_t)count > 6)
            count = 6;
    }
    EF_U16(0x001a81b2u) = (uint16_t)count;
}

/* 0xe36bc, 205 bytes. Write the string table of the resource being built through the writer at [0x42922c]: the 2-byte count word 0x15eea0, the count * 2 bytes at 0x428090, the total length (4 bytes) of strings 1..count-1 with their terminators, and then those strings (with terminator) from the pointer table [0x428860]. Reports an error through 0x132a74 when fewer than two strings exist. */
void write_string_table_e36bc(void)
{
    uint32_t total = 0u;
    int16_t i;
    if ((int16_t)EF_U16(0x0015eea0u) < 2)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00190938u);
    guest_callptr_eaxedx_x(EF_U32(0x0042922cu), 0x0015eea0u, 2u);
    guest_callptr_eaxedx_x(EF_U32(0x0042922cu), 0x00428090u, (uint16_t)(EF_U16(0x0015eea0u) * 2u));
    for (i = 1; i < (int16_t)EF_U16(0x0015eea0u); i++)
        total += guest_fn_b1b8b(EF_U32(EF_U32(0x00428860u) + (uint32_t)(int32_t)i * 4u)) + 1u;
    guest_callptr_eaxedx_x(EF_U32(0x0042922cu), (uint32_t)(uintptr_t)&total, 4u);
    for (i = 1; i < (int16_t)EF_U16(0x0015eea0u); i++) {
        uint32_t s = EF_U32(EF_U32(0x00428860u) + (uint32_t)(int32_t)i * 4u);
        guest_callptr_eaxedx_x(EF_U32(0x0042922cu), s, (uint16_t)(guest_fn_b1b8b(s) + 1u));
    }
}

/* 0x3d950, 210 bytes. For each of the [0x1a5a86] 26-byte trigger records at 0x12b268 whose flag test (0xa0e98 on the id word) succeeds and which is not yet marked (byte 0x1a35d8 + index): mark it, count it (0x1a5a82, 0x127d14), notify the callback at +0xa when set, remember the value at +6 in 0x1a35ac unless it is -1, and advance the cursor word 0x12b360 (wrapping at the record count). */
void scan_triggers_3d950(void)
{
    int16_t i = 0;
    if ((int16_t)EF_U16(0x001a5a86u) > 0) {
        do {
            uint32_t rec = (uint32_t)(int32_t)i * 0x1au;
            if ((guest_fn_a0e98((int16_t)EF_U16(0x0012b268u + rec)) & 0xffu) != 0u && EF_U8(0x001a35d8u + (uint32_t)(int32_t)i) == 0u) {
                uint32_t cb;
                uint32_t v;
                EF_U8(0x001a35d8u + (uint32_t)(int32_t)i) = 1u;
                EF_U16(0x001a5a82u) = (uint16_t)(EF_U16(0x001a5a82u) + 1u);
                EF_U32(0x00127d14u) += 1u;
                cb = EF_U32(0x0012b272u + rec);
                if (cb != 0u)
                    guest_fn_151c4(cb);
                v = EF_U32(0x0012b26eu + rec);
                if (v != 0xffffffffu)
                    EF_U32(0x001a35acu) = v;
                EF_U16(0x0012b360u) = (uint16_t)(EF_U16(0x0012b360u) + 1u);
                if ((int16_t)EF_U16(0x0012b360u) >= (int16_t)EF_U16(0x001a5a86u))
                    EF_U16(0x0012b360u) = 0u;
            }
            i++;
        } while (i < (int16_t)EF_U16(0x001a5a86u));
    }
}

/* 0x6e74, 211 bytes. Bind the object ID (looked up with 0x7489c) to the first free entry of the 5-byte table at [0x197260] (0x121e55 entries; free means the id word at +1 is 0xff while the object's slot byte at +0x187 is still 0xff): clear the entry, store the id and the slot number in the object. Errors through 0x132a74 when no entry was free. Afterwards bump the counter byte 0x19726a and flag the owner (byte 0x1fcc63 + word at +0x15 of its record) as used. */
void bind_object_slot_6e74(uint32_t id)
{
    uint32_t obj = guest_fn_7489c((int16_t)id);
    uint8_t i;
    uint32_t table;
    uint32_t rec;
    if (obj == 0u)
        return;
    table = EF_U32(0x00197260u);
    if (EF_U8(0x00121e55u) != 0u) {
        i = 0u;
        do {
            uint32_t e = table + (uint32_t)i * 5u;
            if ((int16_t)EF_U16(e + 1u) == 0xff && EF_U8(obj + 0x187u) == 0xffu) {
                EF_U8(e) = 0u;
                EF_U16(e + 3u) = 0u;
                EF_U16(e + 1u) = (uint16_t)id;
                EF_U8(obj + 0x187u) = i;
            }
            i++;
        } while (i < EF_U8(0x00121e55u));
    }
    if (EF_U8(obj + 0x187u) == 0xffu)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018038cu);
    rec = (uint32_t)(int32_t)(int16_t)EF_U16(obj + 0x28u) * 0xcau;
    EF_U8(0x0019726au) = (uint8_t)(EF_U8(0x0019726au) + 1u);
    EF_U8(0x001fcc63u + (uint32_t)(int32_t)(int16_t)EF_U16(EF_U32(0x0019bb58u) + rec + 0x15u)) = 1u;
}

/* 0x66fc, 213 bytes. Heading from the view origin (0x1a5bec, 0x1a5be8) to the current object: the object (word 0x1f71ca) is looked up, its position is taken from the active coordinates (mode 2), the entry at [0x1a821c] (mode 6, scaled by 1/256) or computed by 0x84e3c; the offsets are shifted down until both fit in 16 bits and the angle is returned (0 when the object does not exist). ORIGINAL BUG: in mode 6 with no entry the position buffer is used uninitialised (zero here). */
uint32_t heading_to_current_object_66fc(void)
{
    int32_t pos[2] = { 0, 0 };
    uint32_t p;
    int32_t bx;
    int32_t dx;
    uint8_t mode;
    if (guest_fn_7489c((int16_t)EF_U16(0x001f71cau)) == 0u)
        return 0u;
    mode = EF_U8(0x0012814cu);
    if (mode == 2u) {
        sample_active_coordinates((uint32_t)(uintptr_t)pos);
    } else if (mode == 6u) {
        p = EF_U32(0x001a821cu);
        if (p != 0u) {
            pos[0] = (int32_t)EF_U32(p) >> 8;
            pos[1] = (int32_t)EF_U32(p + 8u) >> 8;
        }
    } else {
        uint32_t rec = (uint32_t)(int32_t)(int16_t)EF_U16(0x0019bb5eu) * 0xcau;
        guest_fn_84e3c_x((uint32_t)(uintptr_t)pos, (int16_t)EF_U16(0x001f71cau), EF_U8(EF_U32(0x0019bb58u) + rec + 0x8eu));
    }
    bx = pos[0] - (int32_t)EF_U32(0x001a5becu);
    dx = pos[1] - (int32_t)EF_U32(0x001a5be8u);
    while ((int32_t)abs_a28f2(bx) > 0x7fff || (int32_t)abs_a28f2(dx) > 0x7fff) {
        bx >>= 2;
        dx >>= 2;
    }
    return (uint32_t)angle_of_b3b1c(bx, dx);
}

/* 0x7ae88, 213 bytes. Look up the cell of the plane map for the position (X, Z) (both in units of 2048; wrapped to 400 with the original off-by-one correction for negative remainders): combine the bytes of the two planes (0x1f8eb4, 0x1f8eb8) into a word, remember it at 0x1f8ec2 and, unless it is 0xffff, run 0xd0470 for it with the cell centre coordinates. Returns 0 for an empty cell. */
uint32_t lookup_plane_cell_7ae88(uint32_t mode, int32_t x, uint32_t y, int32_t z)
{
    int32_t gx = x >> 11;
    int32_t gz = 0x18f - (z >> 11);
    int32_t col = gx % 0x190;
    int32_t row = gz % 0x190;
    uint32_t off;
    uint32_t v;
    select_plane_source((int32_t)mode);
    if (col < 0)
        col += 0x18f;
    if (row < 0)
        row += 0x18f;
    off = (uint32_t)(col * 400);
    v = ((uint32_t)EF_U8(EF_U32(0x001f8eb4u) + off + (uint32_t)row) << 8) | EF_U8(EF_U32(0x001f8eb8u) + off + (uint32_t)row);
    EF_U16(0x001f8ec2u) = 0u;
    if ((uint16_t)v == 0xffffu)
        return 0u;
    EF_U16(0x001f8ec2u) = (uint16_t)v;
    return guest_fn_d0470((int16_t)v, (int32_t)((uint32_t)(gx * 2 + 1) << 10), (int32_t)y, (int32_t)((uint32_t)((0x18f - gz) * 2 + 1) << 10));
}

/* 0x28aa8, 214 bytes. Set up the line-of-sight query between the fixed point positions A and B (both 24.8) for an object of KIND: the clearance depends on the kind (0x3c for 5 and 0x12, 0x1e for 4 and 0x13, else 5). When both positions exist the endpoints (B raised by the clearance, A raised by 2) are stored at 0x420904..0x42093a and 0x7b7d8 decides; otherwise the result is 1. Returns the result byte. */
uint8_t setup_line_query_28aa8(ef_addr32 a, ef_addr32 b, uint32_t kind)
{
    uint8_t result = 1u;
    uint8_t k = (uint8_t)kind;
    int32_t clearance;
    int32_t xa, ya, za, xb, yb, zb;
    if (k == 5u || k == 0x12u)
        clearance = 0x3c;
    else if (k == 4u || k == 0x13u)
        clearance = 0x1e;
    else
        clearance = 5;
    if (a == 0u || b == 0u)
        return result;
    xb = (int32_t)EF_U32(b) >> 8;
    yb = (int32_t)EF_U32(b + 4u) >> 8;
    zb = (int32_t)EF_U32(b + 8u) >> 8;
    EF_U32(0x00420910u) = (uint32_t)xb;
    EF_U32(0x00420918u) = (uint32_t)zb;
    xa = (int32_t)EF_U32(a) >> 8;
    yb += (int16_t)clearance;
    EF_U32(0x00420914u) = (uint32_t)xa;
    ya = (int32_t)EF_U32(a + 4u) >> 8;
    za = (int32_t)EF_U32(a + 8u) >> 8;
    EF_U32(0x00420904u) = (uint32_t)yb;
    EF_U16(0x00420932u) = (uint16_t)yb;
    EF_U32(0x0042091cu) = (uint32_t)za;
    EF_U16(0x00420930u) = (uint16_t)xb;
    ya += 2;
    EF_U16(0x00420934u) = (uint16_t)zb;
    EF_U32(0x00420908u) = (uint32_t)ya;
    EF_U16(0x00420936u) = (uint16_t)xa;
    EF_U16(0x00420938u) = (uint16_t)ya;
    EF_U16(0x0042093au) = (uint16_t)za;
    result = (uint8_t)guest_fn_7b7d8();
    return result;
}

/* 0x11014, 216 bytes. Find the entry of OBJ's list (count word at +0x2a, pointer at +0x2c, 21-byte entries with the word pair at +7/+9) that matches the cursor: with the runtime view active (0x125847 set, 0x121ec6 clear) the pair is transformed for the current orientation and tested against the window at the script cursor data, otherwise it is compared with the saved point 0x197c24/0x197c26. Returns the entry index, 0 when none matches. */
uint32_t find_entry_at_cursor_11014(ef_addr32 obj)
{
    uint16_t orient = 0u;
    uint32_t win = 0u;
    int16_t i;
    if (EF_U8(0x00125847u) != 0u && EF_U8(0x00121ec6u) == 0u) {
        orient = record_flag_bits56((int16_t)EF_U16(0x001f8ec2u));
        win = script_cursor_data_cd29c();
    }
    for (i = 0; i < (int16_t)EF_U16(obj + 0x2au); i++) {
        uint32_t e = EF_U32(obj + 0x2cu) + (uint32_t)(int32_t)i * 0x15u;
        uint32_t px = EF_U16(e + 7u);
        uint32_t py = EF_U16(e + 9u);
        if (EF_U8(0x00125847u) != 0u && EF_U8(0x00121ec6u) == 0u) {
            transform_runtime_word_pair(orient, (uint32_t)(uintptr_t)&px, (uint32_t)(uintptr_t)&py);
            if ((int16_t)px > (int16_t)EF_U16(win) && (int16_t)px < (int16_t)EF_U16(win + 6u)
                && (int16_t)py > (int16_t)EF_U16(win + 4u) && (int16_t)py < (int16_t)EF_U16(win + 0xau))
                return (uint32_t)(int32_t)i;
        } else if ((uint16_t)px == EF_U16(0x00197c24u) && (uint16_t)py == EF_U16(0x00197c26u)) {
            return (uint32_t)(int32_t)i;
        }
    }
    return 0u;
}

/* 0x5ae70, 221 bytes. Build the three 24-entry offset rings (radii 25, 20 and 15 at 0x1a69e0, 0x1a6aa0 and 0x1a6a40; two words per entry: cos and -sin scaled by the radius / 0x8000, from the sine/cosine tables at 0x141e08/0x142e08) and set the flag byte 0x1281a0. */
void build_offset_rings_5ae70(void)
{
    int32_t i;
    EF_U8(0x001281a0u) = 1u;
    for (i = 0; i < 0x18; i++) {
        uint32_t idx = ((uint32_t)((i * 0xffff) / 0x18) & 0xffffu) >> 3;
        int32_t c = (int16_t)EF_U16(0x00141e08u + idx * 2u);
        int32_t s = (int16_t)EF_U16(0x00142e08u + idx * 2u);
        uint32_t o = (uint32_t)i * 4u;
        EF_U16(0x001a69e0u + o) = (uint16_t)((c * 0x19) / 0x8000);
        EF_U16(0x001a69e2u + o) = (uint16_t)((-(s * 0x19)) / 0x8000);
        EF_U16(0x001a6aa0u + o) = (uint16_t)((c * 0x14) / 0x8000);
        EF_U16(0x001a6aa2u + o) = (uint16_t)((-(s * 0x14)) / 0x8000);
        EF_U16(0x001a6a40u + o) = (uint16_t)((c * 0xf) / 0x8000);
        EF_U16(0x001a6a42u + o) = (uint16_t)((-(s * 0xf)) / 0x8000);
    }
}

/* 0x7af60, 221 bytes. Like 0x7ae88 for the query in progress: returns 1 while the endpoint word 0x420908 is non-negative, otherwise looks up the plane map cell for (X, Z) and, unless it is empty (0xffff), runs 0xd0278 for it with the cell centre coordinates. */
uint32_t lookup_plane_cell_7af60(uint32_t mode, int32_t x, uint32_t y, int32_t z)
{
    int32_t gx;
    int32_t gz;
    int32_t col;
    int32_t row;
    uint32_t off;
    uint32_t v;
    if ((int32_t)EF_U32(0x00420908u) >= 0)
        return 1u;
    select_plane_source((int32_t)mode);
    gx = x >> 11;
    gz = 0x18f - (z >> 11);
    col = gx % 0x190;
    row = gz % 0x190;
    if (col < 0)
        col += 0x18f;
    if (row < 0)
        row += 0x18f;
    off = (uint32_t)(col * 400);
    v = ((uint32_t)EF_U8(EF_U32(0x001f8eb4u) + off + (uint32_t)row) << 8) | EF_U8(EF_U32(0x001f8eb8u) + off + (uint32_t)row);
    if ((uint16_t)v == 0xffffu)
        return 0u;
    return guest_fn_d0278_x((int16_t)v, (uint32_t)(gx * 2 + 1) << 10, y, (uint32_t)((0x18f - gz) * 2 + 1) << 10);
}

/* 0x9a544, 221 bytes. Open the next numbered log file: build the name from the template at 0x98bd0, the running number (word 0x12dda8, post-incremented) and the suffix template at 0x98be4 (appended to the same buffer), open it with the mode string at 0x18b0f0 (error through 0x132a74 when that fails) and write three header lines (formats 0x131754, 0x13177f, 0x131792 with [0x13799e] + 1) and the date line 0x18b114 (words at 0x1379a0, 0x1379a2, 0x1379a4). Returns the file handle. */
uint32_t open_numbered_log_9a544(void)
{
    uint8_t frame[0x24];
    uint32_t f = (uint32_t)(uintptr_t)frame;
    int16_t n = (int16_t)EF_U16(0x0012dda8u);
    uint32_t h;
    uint32_t i;
    for (i = 0u; i < 20u; i++)
        frame[i] = EF_U8(0x00098bd0u + i);
    for (i = 0u; i < 5u; i++)
        frame[0x14u + i] = EF_U8(0x00098be4u + i);
    EF_U16(0x0012dda8u) = (uint16_t)(n + 1);
    signed_to_digits_cbd0f(n, f + 0x1cu, 10u);
    guest_fn_9ca57(f, f + 0x1cu);
    guest_fn_9ca57(f, f + 0x14u);
    h = guest_fn_bc36a_w(f, 0x0018b0f0u);
    if (h == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018b0f4u);
    stream_printf_bc400(h, 0x00131754u);
    stream_printf_bc400(h, 0x0013177fu);
    stream_printf_bc400(h, 0x00131792u, (int16_t)EF_U16(0x0013799eu) + 1);
    stream_printf_bc400(h, 0x0018b114u, (int16_t)EF_U16(0x001379a0u), (int16_t)EF_U16(0x001379a2u), (int16_t)EF_U16(0x001379a4u));
    return h;
}

/* 0x14c5c, 224 bytes. Set the state byte of the 19-byte record IDX at 0x1225d4 to STATE; when it changed and the current object is not locked (flag 0x10 at +0x24), run 0x14d58 for the record and, for states 9 and 10, claim the UI slots (the record's own control byte at +0x12 with arg 8, then control 0x1f for state 9 or 0x1d for state 10 with arg 0x14) and push the event stored at +0xe. */
void set_record_state_14c5c(int32_t idx, uint32_t state)
{
    uint32_t rec = 0x001225d4u + (uint32_t)(int32_t)(int16_t)idx * 0x13u;
    uint8_t old = EF_U8(rec);
    uint8_t st = (uint8_t)state;
    uint32_t obj;
    uint32_t ev;
    EF_U8(rec) = st;
    if (old == st)
        return;
    obj = guest_fn_7489c((int16_t)EF_U16(0x001f71cau));
    if (obj != 0u && (EF_U8(obj + 0x24u) & 0x10u) != 0u)
        return;
    guest_fn_14d58_x((int16_t)idx);
    if (st != 9u && st != 10u)
        return;
    if (EF_U8(rec + 0x12u) != 0u)
        claim_ui_slot_801e4(EF_U8(rec + 0x12u), 8u, 0u);
    claim_ui_slot_801e4((st == 9u) ? 0x1f : 0x1d, 0x14u, 0u);
    ev = EF_U32(rec + 0xeu);
    if (ev != 0u)
        push_event_1f878(ev);
}

/* Draw a string with a font that has an extended glyph range: characters of 0x80 and up (0xa5 is remapped to 'N') use the pointer table ext_glyph and width table ext_w (a zero width means no glyph and no advance), drawn EXT_DY above the line; the others (low 6 bits index the map at 0x15e1be, -1 for none) use reg_glyph/reg_w. Spaces and tabs advance by gap, drawn glyphs by width + extra. */
static void draw_text_ext_glyphs(uint32_t colour, uint32_t x, uint32_t y, uint32_t str, uint32_t gap, uint32_t ext_glyph, uint32_t ext_w, uint32_t ext_dy, uint32_t reg_glyph, uint32_t reg_w, uint32_t extra)
{
    int16_t i;
    for (i = 0;; i++) {
        uint8_t c = EF_U8(str + (uint32_t)(int32_t)i);
        uint32_t idx;
        uint32_t w;
        if (c == 0u)
            return;
        if (c == 0xa5u)
            c = 0x4eu;
        if (c == 0x20u || c == 9u) {
            x += gap;
            continue;
        }
        if (c > 0x7fu) {
            idx = (uint8_t)(c - 0x80u);
            w = EF_U8(ext_w + idx);
            if (w == 0u)
                continue;
            guest_fn_cf7c4_x(EF_U32(ext_glyph + idx * 4u), (uint32_t)(int32_t)(int16_t)y - ext_dy, (uint32_t)(int32_t)(int16_t)x, colour & 0xffu);
        } else {
            int32_t m = (int32_t)(int8_t)EF_U8(0x0015e1beu + (uint32_t)(c & 0x3fu));
            if (m == -1)
                continue;
            idx = (uint32_t)m;
            guest_fn_cf7c4_x(EF_U32(reg_glyph + idx * 4u), (uint32_t)(int32_t)(int16_t)y, (uint32_t)(int32_t)(int16_t)x, colour & 0xffu);
            w = EF_U8(reg_w + idx);
        }
        x += w + extra;
    }
}

/* 0xdd788, 218 bytes. Draw a string with the first extended font (regular glyphs 0x15e7cc / widths 0x15e2ea, extended glyphs 0x15ec70 / widths 0x15edf8, space 6, spacing 2). */
void draw_text_font0_dd788(uint32_t colour, uint32_t x, uint32_t y, uint32_t str)
{
    draw_text_ext_glyphs(colour, x, y, str, 6u, 0x0015ec70u, 0x0015edf8u, 0u, 0x0015e7ccu, 0x0015e2eau, 2u);
}

/* 0xdd370, 238 bytes. Draw a string with the narrow extended font (regular glyphs 0x15e4e8 / widths 0x15e31d, extended glyphs 0x15e898 / widths 0x15ea20 drawn 2 lines higher, space 3, spacing 1). */
void draw_text_font6_dd370(uint32_t colour, uint32_t x, uint32_t y, uint32_t str)
{
    draw_text_ext_glyphs(colour, x, y, str, 3u, 0x0015e898u, 0x0015ea20u, 2u, 0x0015e4e8u, 0x0015e31du, 1u);
}

/* 0xdd26c, 133 bytes. Draw a string through the glyph routine 0xdcf28 (upper-cased, spaces and tabs advance by 6). ORIGINAL BUG: the character is read at index (i & 0x3f) instead of i, so strings longer than 64 characters repeat. */
void draw_text_upper_dd26c(uint32_t colour, uint32_t x, uint32_t y, uint32_t str)
{
    int16_t i;
    int16_t sx = (int16_t)x;
    for (i = 0;; i++) {
        uint8_t c = EF_U8(str + (uint32_t)(int32_t)i);
        if (c == 0u)
            return;
        if (c == 0x20u || c == 9u) {
            sx = (int16_t)(sx + 6);
            continue;
        }
        sx = (int16_t)(sx + draw_glyph_dcf28((uint8_t)to_upper_ascii(EF_U8(str + (uint32_t)(i & 0x3f))), sx, (int16_t)y, (int16_t)(colour & 0xffu)));
    }
}

/* 0xa8578, 229 bytes. Draw the text at ECX with the font selected by the byte at [[0x4191d0] + 0x10] (0..9: 0xdd788, 0xf23ac, 0xf261c, 0xdccd8, 0xdd26c, 0xdd1b0, 0xdd370, 0xdd4c4, 0xdd5bc, 0xdd6a0) at (X, Y) in the colour byte COLOUR; fonts above 9 draw nothing. X and Y are sign extended from 16 bits. */
void draw_text_by_font_a8578(int32_t x, int32_t y, uint32_t colour, uint32_t text)
{
    int32_t sx = (int16_t)x;
    int32_t sy = (int16_t)y;
    uint32_t col = colour & 0xffu;
    switch (EF_U8(EF_U32(0x004191d0u) + 0x10u)) {
    case 0: draw_text_font0_dd788(col, (uint32_t)sx, (uint32_t)sy, text); break;
    case 1: draw_text_f23ac((uint8_t)col, sx, sy, text); break;
    case 2: draw_text_f261c(col, (uint32_t)sx, (uint32_t)sy, text); break;
    case 3: draw_text_4_dccd8((uint8_t)col, sx, (uint16_t)sy, text); break;
    case 4: draw_text_upper_dd26c(col, (uint32_t)sx, (uint32_t)sy, text); break;
    case 5: draw_text_5_dd1b0((uint8_t)col, sx, (uint16_t)sy, text); break;
    case 6: draw_text_font6_dd370(col, (uint32_t)sx, (uint32_t)sy, text); break;
    case 7: draw_text_large_dd4c4(col, (uint32_t)sx, (uint32_t)sy, text); break;
    case 8: draw_text_medium_dd5bc(col, (uint32_t)sx, (uint32_t)sy, text); break;
    case 9: draw_text_small_dd6a0(col, (uint32_t)sx, (uint32_t)sy, text); break;
    default: break;
    }
}

/* 0x9ca57, 55 bytes. strcat: append the string SRC to DST; returns DST. */
ef_addr32 strcat_9ca57(ef_addr32 dst, ef_addr32 src)
{
    uint32_t d = dst;
    uint8_t c;
    while (EF_U8(d) != 0u)
        d++;
    do {
        c = EF_U8(src);
        EF_U8(d) = c;
        src++;
        d++;
    } while (c != 0u);
    return dst;
}

/* 0x117b0c, 10 bytes. outp: write the byte VALUE to the I/O port PORT. */
void outp_117b0c(uint32_t port, uint32_t value)
{
    guest_outp_x(port, value & 0xffu);
}

/* 0x9c430, 8 bytes. inp: read a byte from the I/O port PORT. */
uint32_t inp_9c430(uint32_t port)
{
    return guest_inp_x(port) & 0xffu;
}

/* 0xdc832, 37 bytes. memcpy: copy COUNT bytes from SRC to DST (dwords first, then the tail bytes); returns DST. */
ef_addr32 memcpy_dc832(ef_addr32 dst, ef_addr32 src, uint32_t count)
{
    uint32_t i;
    uint32_t n = count >> 2;
    uint32_t d = dst;
    for (i = 0u; i < n; i++) {
        EF_U32(d) = EF_U32(src);
        d += 4u;
        src += 4u;
    }
    for (i = 0u; i < (count & 3u); i++) {
        EF_U8(d) = EF_U8(src);
        d++;
        src++;
    }
    return dst;
}

/* 0x9c858, 90 bytes. getenv: search the environment table at [0x160a18] (null terminated array of "NAME=value" strings) for NAME, ignoring case; returns the address of the value or 0. */
ef_addr32 getenv_9c858(ef_addr32 name)
{
    uint32_t env = EF_U32(0x00160a18u);
    uint32_t len = 0u;
    uint32_t entry;
    if (env == 0u || name == 0u)
        return 0u;
    while (EF_U8(name + len) != 0u)
        len++;
    for (entry = EF_U32(env); entry != 0u; entry = EF_U32(env)) {
        if (compare_nocase_eb374(entry, name, (int32_t)len) == 0 && EF_U8(entry + len) == 0x3du)
            return entry + len + 1u;
        env += 4u;
    }
    return 0u;
}

/* 0xd099c, 66 bytes. Transpose the 3x3 matrix of dwords at SRC into DST. */
void transpose_matrix3_d099c(ef_addr32 dst, ef_addr32 src)
{
    uint32_t t[9];
    int32_t i;
    for (i = 0; i < 3; i++) {
        t[i] = EF_U32(src + (uint32_t)i * 12u);
        t[i + 3] = EF_U32(src + (uint32_t)i * 12u + 4u);
        t[i + 6] = EF_U32(src + (uint32_t)i * 12u + 8u);
    }
    for (i = 0; i < 9; i++)
        EF_U32(dst + (uint32_t)i * 4u) = t[i];
}

/* 0xdc2e0, 49 bytes. Wait for the vertical retrace: poll the VGA status port 0x3da (at most 100000 reads each) until the display-enable bit (1) clears and then until the retrace bit (8) is set. */
void wait_vsync_dc2e0(void)
{
    int32_t n = 100000;
    do {
        if ((guest_inp_x(0x3dau) & 1u) == 0u)
            break;
        n--;
    } while (n > 0);
    n = 100000;
    do {
        if ((guest_inp_x(0x3dau) & 8u) != 0u)
            break;
        n--;
    } while (n > 0);
}

/* 0xfa95f, 65 bytes. DOS lseek (int 21h, AH = 0x42): move the file HANDLE to OFFSET relative to WHENCE. Returns the new position (DX:AX), or -1 after reporting the DOS error through 0x1101e8 when the carry flag is set. */
uint32_t lseek_fa95f(uint32_t handle, uint32_t offset, uint32_t whence)
{
    uint32_t in[7];
    uint32_t out[7];
    uint8_t sregs[12];
    segread_9be0d((uint32_t)(uintptr_t)sregs);
    in[0] = 0x4200u | (whence & 0xffu);
    in[1] = handle;
    in[2] = offset >> 16;
    in[3] = offset & 0xffffu;
    in[4] = 0u;
    in[5] = 0u;
    in[6] = 0u;
    guest_dos_int_x(0x21u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
    if (out[6] != 0u) {
        guest_fn_1101e8_x(out[0] & 0xffffu);
        return 0xffffffffu;
    }
    return ((out[3] & 0xffffu) << 16) | (out[0] & 0xffffu);
}

/* 0x2b0, 116 bytes. intdos: run DOS interrupt 0x21 with the registers from the block IN (EAX, EBX, ECX, EDX, ESI, EDI at +0..+0x14) and the current segment registers, storing the resulting registers into OUT (+0..+0x14; unlike 0x9c47a the carry flag word at +0x18 is not written). */
void intdos_2b0(uint32_t in, uint32_t out)
{
    uint8_t sregs[12];
    segread_9be0d((uint32_t)(uintptr_t)sregs);
    guest_dos_intdos_x(in, out, (uint32_t)(uintptr_t)sregs);
}

/* 0x104840, 113 bytes. Run DOS service 0x252b / 0x501 over the address range [min(FIRST, LAST), max(...)] (cdecl). Returns 1 when the carry flag in the result is clear. */
uint32_t dos_range_104840(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t first, uint32_t last)
{
    uint32_t in[4];
    uint32_t out[7];
    uint8_t sregs[12];
    uint32_t lo = (first < last) ? first : last;
    uint32_t hi = (first < last) ? last : first;
    (void)u0; (void)u1; (void)u2; (void)u3;
    segread_9be0d((uint32_t)(uintptr_t)sregs);
    in[0] = 0x252bu;
    in[1] = 0x501u;
    in[2] = lo;
    in[3] = hi - lo + 1u;
    int386x_9c461(0x21u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
    return out[6] == 0u;
}

/* 0x1048b4, 82 bytes. Like 0x104840 for the service 0x252b / 0x61, issued through intdos (0x2b0). ORIGINAL BUG: 0x2b0 does not store the carry word, so the result tests an uninitialised stack word (always taken as "carry clear" here). */
uint32_t dos_range_1048b4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t first, uint32_t last)
{
    uint32_t in[4];
    uint32_t out[7] = { 0, 0, 0, 0, 0, 0, 0 };
    uint32_t lo = (first < last) ? first : last;
    uint32_t hi = (first < last) ? last : first;
    (void)u0; (void)u1; (void)u2; (void)u3;
    in[0] = 0x252bu;
    in[1] = 0x61u;
    in[2] = lo;
    in[3] = hi - lo + 1u;
    intdos_2b0((uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out);
    return out[6] == 0u;
}

/* 0x9e508, 175 bytes. Run the INDEXth text of the page PAGE: select the page (word 0x41722a, record 0x417174 from the id lookup, table entry 0x41717c from the table at 0x40d200), and when INDEX is below the page's text count (byte +8) run 0x9e7bc on the text at offset-table entry INDEX (6-byte records after +0xa) with the handler 0xa0b5c in 0x417178. The previous page state is restored afterwards. */
void run_page_text_9e508(int32_t page, uint32_t index)
{
    uint16_t saved_page = EF_U16(0x0041722au);
    uint32_t saved_rec = EF_U32(0x00417174u);
    uint32_t saved_handler = EF_U32(0x00417178u);
    uint32_t saved_table = EF_U32(0x0041717cu);
    uint32_t rec;
    uint32_t table;
    uint32_t p;
    EF_U16(0x0041722au) = (uint16_t)page;
    rec = guest_fn_b3f58((int16_t)page);
    EF_U32(0x00417174u) = rec;
    table = EF_U32(0x0040d200u + (uint32_t)EF_U16(rec) * 4u);
    EF_U32(0x0041717cu) = table;
    if ((index & 0xffffu) < (uint32_t)EF_U8(table + 8u)) {
        EF_U32(0x00417178u) = 0x000a0b5cu;
        p = (uint32_t)EF_U8(table + 9u) * 6u + (table + 0xau) + (uint32_t)(int32_t)(int16_t)index * 2u;
        p += (uint32_t)((int32_t)(int16_t)EF_U16(p) * 2);
        guest_fn_9e7bc_x(p);
    }
    EF_U16(0x0041722au) = saved_page;
    EF_U32(0x00417174u) = saved_rec;
    EF_U32(0x00417178u) = saved_handler;
    EF_U32(0x0041717cu) = saved_table;
}

/* 0xbef2c, 99 bytes. Sum, over up to 8 leading characters of the string returned by 0xfb87c (stopping at '%'), of the difference of two per-character byte tables at OBJ+0x33 and OBJ+0x37. */
int32_t sum_char_table_diff_bef2c(uint32_t a, uint32_t b, uint32_t obj)
{
    uint32_t kind = EF_U8(obj + 0xbu);
    uint32_t str = guest_fn_fb87c_x(a & 0xffu, b & 0xffu, kind, obj);
    uint32_t sum = 0u;
    int16_t i;
    for (i = 0; i < 8; i++) {
        uint32_t c = EF_U8(str + (uint32_t)(int32_t)i);
        uint32_t t1 = EF_U32(obj + 0x33u);
        uint32_t d;
        if (c == 0x25u)
            break;
        /* ORIGINAL BUG: only the low byte of the table pointer is replaced, so bits 16..31 of the pointer leak into the difference */
        d = (t1 & 0xffff0000u) | EF_U8(t1 + c);
        d -= EF_U8(EF_U32(obj + 0x37u) + c);
        sum += d;
    }
    guest_fn_cc768(str);
    return (int32_t)sum;
}

/* 0xeb2c0, 99 bytes. Printf-style message (cdecl, FORMAT and varargs on the stack) formatted into a 0x280-byte buffer, then, if the counter byte 0x160794 became 1 after being incremented, 0x1915e0 is appended and the buffer is handed to 0xeb448. */
void format_and_enter_eb2c0(ef_addr32 fmt, ...)
{
    char buf[0x284];
    va_list ap;
    EF_U8(0x00160794u)++;
    if (EF_U8(0x00160794u) == 1u) {
        va_start(ap, fmt);
        format_to_buffer_e3022((uint32_t)(uintptr_t)buf, fmt, (uint32_t)(uintptr_t)&ap[0]);
        va_end(ap);
        strcat_9ca57((uint32_t)(uintptr_t)buf, 0x001915e0u);
        enter_handler_eb448((uint32_t)(uintptr_t)buf);
    }
}

/* 0xeb174, 123 bytes. Copy COUNT rows through 0xe9a24: the destination starts at base 0x15b754 + row-offset table 0x163200[Y] + X and advances by the pitch 0x1631f4; the source starts at X0 + row*pitch + base and advances by the row step (cdecl, eight stack arguments). */
void copy_rows_eb174(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3,
                     uint32_t x, uint32_t y, uint32_t len, uint32_t count,
                     uint32_t step, uint32_t x0, uint32_t rows, uint32_t base)
{
    uint32_t dst;
    uint32_t src;
    (void)u0; (void)u1; (void)u2; (void)u3;
    dst = EF_U32(0x0015b754u) + EF_U32(0x00163200u + (y & 0xffffu) * 4u) + (x & 0xffffu);
    src = (x0 & 0xffffu) + (step & 0xffffu) * (rows & 0xffffu) + base;
    if ((uint16_t)count == 0u)
        return;
    do {
        guest_fn_e9a24(len & 0xffffu, src, dst);
        dst += EF_U32(0x001631f4u);
        src += step & 0xffffu;
        count--;
    } while ((uint16_t)count != 0u);
}

/* 0x5c9d4, 140 bytes. Draw the horizontal tick marks of a cursor at the point stored in OBJ (x word, y word): refresh through 0x39a0c, plot the centre via 0xdec18, then two short lines left and right of it (gaps 2 and 6, scaled by the shift byte 0x12b369) with 0xc8ccc in the colour byte 0x128148. */
void draw_cursor_ticks_5c9d4(uint32_t obj, uint32_t arg)
{
    int32_t x;
    int32_t y;
    uint32_t color = EF_U8(0x00128148u);
    uint32_t s;
    guest_fn_39a0c_x(obj, (uint32_t)(int32_t)(int16_t)arg);
    x = (int16_t)EF_U16(obj);
    y = (int16_t)EF_U16(obj + 2u);
    guest_fn_dec18_x(color, (uint32_t)x, (uint32_t)y);
    s = EF_U8(0x0012b369u);
    guest_fn_c8ccc_x((uint32_t)y, (uint32_t)(x - (int32_t)(2u << s)), (uint32_t)y, (uint32_t)(x - (int32_t)(6u << s)), color);
    s = EF_U8(0x0012b369u);
    guest_fn_c8ccc_x((uint32_t)y, (uint32_t)(x + (int32_t)(2u << s)), (uint32_t)y, (uint32_t)(x + (int32_t)(6u << s)), color);
}

/* 0xca200, 142 bytes. Load the record file NAME: build the default path with NAME appended and the suffix 0x18f058, open it through the indirect 0x132ab4 (error 0x18f05c on failure), read it into an arena block (0x41eea8, error 0x18f084 when empty) and validate it through 0x132ab8 (error 0x18f0c0 unless it returns 1). */
void load_record_file_ca200(uint32_t name)
{
    char buf[0x90];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    uint32_t h;
    build_default_path_abaf0(b);
    strcat_9ca57(b, name);
    strcat_9ca57(b, 0x0018f058u);
    h = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), b);
    if (h == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018f05cu, b);
    EF_U32(0x0041eea8u) = arena_take_e29e8(h);
    if (EF_U32(0x0041eea8u) == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018f084u);
    if ((guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), b, EF_U32(0x0041eea8u)) & 0xffu) != 1u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018f0c0u);
}

/* 0xb4db0, 151 bytes. Find the first node of type 0x31 whose byte +0xb passes 0xcc1ac for KIND and PARAM. KIND 0, 2 and 5 walk forward from the head pointer at *HEAD_A (next link +4); 1, 3 and 4 walk backward from *HEAD_B (link +0). Returns the node or 0. */
uint32_t find_node_b4db0(uint32_t head_a, uint32_t head_b, uint32_t kind, uint32_t param)
{
    uint32_t node;
    uint32_t link;
    uint8_t k = (uint8_t)kind;
    if (k > 5u)
        return 0u;
    if (k == 0u || k == 2u || k == 5u) {
        node = EF_U32(head_a);
        link = 4u;
    } else {
        node = EF_U32(head_b);
        link = 0u;
    }
    while (node != 0u) {
        if (EF_U8(node + 0xcu) == 0x31u &&
            (guest_fn_cc1ac(k, EF_U8(node + 0xbu), param) & 0xffffu) != 0u)
            return node;
        node = EF_U32(node + link);
    }
    return 0u;
}

/* 0x89ac0, 151 bytes. When the object's target record (object+0x30 -> +0x4b) is in range (0x89b58), may be swapped in as the object's own target: done when the target's heading object passes 0x7574c and the period check 0x89bf0, or else by a random roll (word from 0x9c960 modulo 0x5c not above the step word 0x127d92 of at most 10). The takeover copies the link, clears +0x59, copies the target word +0x16 to +0x56 and points the target's back-link +0x1c at the object. */
void try_take_target_89ac0(uint32_t obj)
{
    uint32_t cur = EF_U32(obj + 0x30u);
    uint32_t tgt = EF_U32(cur + 0x4bu);
    uint32_t rem;
    uint32_t step;
    if (tgt == 0u)
        return;
    if ((guest_fn_89b58_x(obj) & 0xffu) == 0u)
        return;
    if ((guest_fn_7574c((int32_t)(EF_S32(tgt + 0x14u) >> 16)) & 0xffu) != 0u && (period_elapsed_89bf0() & 0xffu) != 0u) {
        EF_U32(obj + 0x30u) = EF_U32(cur + 0x4bu);
        EF_U8(obj + 0x59u) = 0u;
        EF_U16(obj + 0x56u) = EF_U16(tgt + 0x16u);
        EF_U32(tgt + 0x1cu) = obj;
    }
    rem = (guest_fn_9c960() & 0xffffu) % 0x5cu;
    step = EF_U16(0x00127d92u);
    if ((int32_t)rem > (int32_t)step || (int32_t)step > 10)
        return;
    EF_U32(obj + 0x30u) = EF_U32(cur + 0x4bu);
    EF_U8(obj + 0x59u) = 0u;
    EF_U16(obj + 0x56u) = EF_U16(tgt + 0x16u);
    EF_U32(tgt + 0x1cu) = obj;
}

/* 0xe3b7c, 158 bytes. Check a data file: open NAME, read a 2-byte header into 0x15f900 and a 4-byte length, and succeed (1) only when the length equals the file size (0xa7ae0) and the header word is nonzero. The handle is always closed. */
uint8_t check_data_file_e3b7c(uint32_t name)
{
    int32_t h = open_slot_a7900(0u, 0u, 0u, 0u, name);
    uint32_t len;
    if (h == -1)
        return 0u;
    if (read_handle_a79d4(0u, 0u, 0u, 0u, (uint32_t)h, 2u, 0x0015f900u) != 2u) {
        close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)h);
        return 0u;
    }
    {
        uint32_t tmp = 0u;
        if (read_handle_a79d4(0u, 0u, 0u, 0u, (uint32_t)h, 4u, (uint32_t)(uintptr_t)&tmp) != 4u) {
            close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)h);
            return 0u;
        }
        len = tmp;
    }
    if (seek_end_a7ae0(0u, 0u, 0u, 0u, (uint32_t)h, 0u) != len) {
        close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)h);
        return 0u;
    }
    close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)h);
    return EF_U16(0x0015f900u) != 0u ? 1u : 0u;
}

/* 0xe2ba0, 157 bytes. Fill a diamond of colour COLOR (byte) into the 8-bit frame buffer: top row at Y - SIZE, starting 1 pixel wide at column X and widening by two pixels per row (shifting one column left) until 2 * SIZE, then narrowing again. */
void fill_diamond_e2ba0(uint32_t color, uint32_t x, uint32_t y, uint32_t size)
{
    uint32_t pitch = EF_U32(0x001631f4u);
    int16_t cx = (int16_t)size;
    int16_t row = (int16_t)(y - (uint32_t)(int32_t)cx);
    uint32_t p = (uint32_t)(int32_t)(int16_t)x + EF_U32(0x0015b754u) + EF_U32(0x00163200u + (uint32_t)(int32_t)row * 4u);
    int16_t w = 1;
    int16_t n;
    while ((int32_t)w < (int32_t)cx * 2) {
        for (n = 0; n < w; n++)
            EF_U8(p++) = (uint8_t)color;
        p += pitch - (uint32_t)(int32_t)n - 1u;
        w = (int16_t)(w + 2);
    }
    while (w > 0) {
        for (n = 0; n < w; n++)
            EF_U8(p++) = (uint8_t)color;
        p += pitch - (uint32_t)(int32_t)n + 1u;
        w = (int16_t)(w - 2);
    }
    EF_U32(0x001631f4u) = pitch;
}

/* 0xe8484, 159 bytes. Draw the outline boxes of the scene entry KEY at offset (X, Y, Z): the offset is stored in 0x464370/74/78, the entry kind byte in 0x46437c, and for every record in the entry's list (stride in words at +0, vertex index word at +4) a box is outlined with 0xe853c from the vertex table. */
void draw_entry_boxes_e8484(int32_t key, uint32_t x, uint32_t y, uint32_t z)
{
    int16_t k = (int16_t)key;
    uint32_t group = find_scene_named_group(0x00190c7cu, (int32_t)x);
    uint32_t entry;
    uint32_t verts;
    uint32_t base;
    uint32_t node;
    if (group == 0u)
        return;
    entry = find_scene_named_entry(k, group);
    if (entry == 0u)
        return;
    EF_U32(0x00464370u) = x;
    EF_U32(0x00464374u) = y;
    EF_U32(0x00464378u) = z;
    EF_U8(0x0046437cu) = record_flag_bits56(k);
    verts = select_page_slot_a0de0(k, 0);
    base = record_entry_address(k);
    node = entry;
    if (EF_U16(entry) == 0u)
        return;
    do {
        int32_t idx = (int16_t)EF_U16(node + 4u);
        int32_t v = (int32_t)(int16_t)EF_U16(verts + (uint32_t)idx * 2u + 2u) * 2;
        draw_box_outline_e853c(base + (uint32_t)v, base + (uint32_t)v + 6u);
        node += (uint32_t)((int32_t)(int16_t)EF_U16(node) * 2);
    } while (EF_U16(node) != 0u);
}

/* 0x8d330, 161 bytes. Retire the unit INDEX: skipped when its owner byte (+0x18) equals the current player byte 0x1a666b or the owner word is 0xff; otherwise clears the UI links (0x8d0e0, when 0x127da8 is set, also zeroing +0x1c), runs the page texts 0 and 3 of the unit's page (word +0x20) around 0xcd150/0xcd20c, releases its named references (0x8ef38), clears flag bits 0x14 of +0x24 and re-applies the word at +0x28 through 0x8ca24. */
void retire_unit_8d330(int32_t index)
{
    int16_t i = (int16_t)index;
    uint32_t rec = guest_fn_7489c(i);
    int16_t page;
    if (EF_U8(0x001a666bu) == EF_U16(rec + 0x18u))
        return;
    if ((int16_t)EF_U16(rec + 0x18u) == 0xff)
        return;
    if (EF_U8(0x00127da8u) != 0u) {
        mark_ui_links_by_slot(i);
        EF_U32(rec + 0x1cu) = 0u;
    }
    page = (int16_t)EF_U16(rec + 0x20u);
    select_page_9e3f8((uint32_t)(int32_t)page);
    run_page_text_9e508((int32_t)page, 0u);
    guest_fn_cd150((int32_t)page);
    guest_fn_cd20c_w(0u);
    run_page_text_9e508((int32_t)page, 3u);
    clear_named_references_8ef38(rec);
    EF_U8(rec + 0x24u) &= 0xebu;
    guest_fn_8ca24_x(rec, (int32_t)(int16_t)EF_U16(rec + 0x28u));
}

/* 0xeab44, 162 bytes. Timed record pump: when the clock 0x10dfe0 has advanced by at least the interval word 0x484466 since 0x484154, re-reads the data block (0xeaaf4), hands the record buffers to 0xf9ee0 and 0x10dfec, and then either steps the pass counter word 0x484468, or, when it reaches the limit word 0x484162, resets it to 2 and rewinds the stream by the position held in 0x484150. Returns 1 while running, 0 after the rewind. */
uint32_t pump_records_eab44(void)
{
    int32_t now = (int32_t)field_10dfe0();
    uint32_t ctx;
    uint32_t pos;
    uint16_t pass;
    EF_U32(0x00484158u) = (uint32_t)now;
    if (now - EF_S32(0x00484154u) < (int32_t)EF_U16(0x00484466u))
        return 1u;
    EF_U32(0x00484154u) = (uint32_t)now;
    read_data_block_eaaf4();
    guest_fn_f9ee0_x(0x00464580u, 0x00474360u);
    guest_fn_10dfec_x(EF_U32(0x0048415cu), 0x00464580u);
    pass = EF_U16(0x00484468u);
    if (pass != EF_U16(0x00484162u)) {
        EF_U16(0x00484468u) = (uint16_t)(pass + 1u);
        return 1u;
    }
    EF_U16(0x00484468u) = 2u;
    ctx = EF_U32(0x00160644u);
    pos = seek_context_9cee8(0u, ctx);
    seek_context_9cee8(EF_U32(0x00484150u) - pos, ctx);
    return 0u;
}

/* 0xe605c, 188 bytes. Draw the NUL-terminated string STR at (X, Y) in colour COLOR (stored in 0x433ec0) with 8-pixel cells, clipped against the rectangle 0x17cf9c/a0/a4/a8. Glyphs fully inside the clip use 0xe5e90 (only when the text also fits vertically), partially visible ones 0xe5f64. */
void draw_clipped_text_e605c(uint32_t color, int32_t x, int32_t y, uint32_t str)
{
    int32_t gy = y - 2;
    int32_t top = EF_S32(0x0017cfa4u);
    int32_t bottom = EF_S32(0x0017cfa8u);
    int32_t right_edge;
    EF_U8(0x00433ec0u) = (uint8_t)color;
    if (y + 7 < top)
        return;
    if (gy > bottom)
        return;
    if (gy < top || gy + 9 > bottom) {
        right_edge = x + 6;
        while (EF_U8(str) != 0u) {
            if (right_edge > EF_S32(0x0017cf9cu) && x < EF_S32(0x0017cfa0u))
                guest_fn_e5f64_x((uint32_t)x, (uint32_t)gy, EF_U8(str));
            right_edge += 8;
            x += 8;
            str++;
        }
        return;
    }
    right_edge = x + 6;
    while (EF_U8(str) != 0u) {
        if (x >= EF_S32(0x0017cf9cu) && right_edge < EF_S32(0x0017cfa0u))
            draw_glyph_7x9_e5e90((uint32_t)x, (uint32_t)gy, EF_U8(str));
        else if (right_edge > EF_S32(0x0017cf9cu) && x < EF_S32(0x0017cfa0u))
            guest_fn_e5f64_x((uint32_t)x, (uint32_t)gy, EF_U8(str));
        right_edge += 8;
        x += 8;
        str++;
    }
}

/* 0x106644, 196 bytes. Reset the stream control block P (cdecl): fixed defaults in the fields up to +0x40 (the clock word 0x4dd7e0 goes into +0x40, 0x2b11 into +0x3c), +0x44 is 0 when the owner's (+0) mode word at +0x18 is 0 or 1 and 0x40 otherwise, the buffer counters at +0x848/+0x84c/+0x850 are cleared, and the block is passed to 0x10535c. */
void reset_stream_block_106644(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t p)
{
    uint32_t mode;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (p == 0u)
        return;
    EF_U32(p + 0x04u) = 2u;
    EF_U32(p + 0x08u) = 0u;
    EF_U32(p + 0x10u) = 0u;
    EF_U32(p + 0x18u) = 0u;
    EF_U32(p + 0x20u) = 0u;
    EF_U32(p + 0x0cu) = 0u;
    EF_U32(p + 0x14u) = 0u;
    EF_U32(p + 0x1cu) = 0u;
    EF_U32(p + 0x24u) = 1u;
    EF_U32(p + 0x28u) = 0u;
    EF_U32(p + 0x2cu) = 0xfffffffeu;
    EF_U32(p + 0x30u) = 1u;
    EF_U32(p + 0x34u) = 0u;
    EF_U32(p + 0x38u) = 0u;
    EF_U32(p + 0x40u) = EF_U32(0x004dd7e0u);
    EF_U32(p + 0x3cu) = 0x2b11u;
    mode = EF_U32(EF_U32(p) + 0x18u);
    EF_U32(p + 0x44u) = (mode == 0u || mode == 1u) ? 0u : 0x40u;
    EF_U32(p + 0x848u) = 0u;
    EF_U32(p + 0x84cu) = 0u;
    EF_U32(p + 0x850u) = 0u;
    guest_fn_10535c(p);
}

/* 0x7fc34, 166 bytes. Remaining share of entry INDEX in the table at 0x1f9078: when the record's size (0xd7730) is 1 the value is the record's own 0xd771c result; with a nonzero SCALE and a positive size the value is the own result minus SCALE * own / size (clamped at 0), otherwise the plain own result. */
uint32_t remaining_share_7fc34(uint32_t scale, uint32_t index)
{
    uint32_t rec = EF_U32(EF_U32(0x001f9078u) + index * 4u);
    uint32_t own;
    uint32_t q;
    if (get_dword_d7730(rec) == 1u)
        return guest_fn_d771c(rec);
    if (scale != 0u && (int32_t)get_dword_d7730(EF_U32(EF_U32(0x001f9078u) + index * 4u)) > 0) {
        uint32_t prod = guest_fn_d771c(EF_U32(EF_U32(0x001f9078u) + index * 4u)) * scale;
        uint32_t size = get_dword_d7730(EF_U32(EF_U32(0x001f9078u) + index * 4u));
        q = prod / size;
        own = guest_fn_d771c(EF_U32(EF_U32(0x001f9078u) + index * 4u));
        if (q < own)
            return guest_fn_d771c(EF_U32(EF_U32(0x001f9078u) + index * 4u)) - q;
        return 0u;
    }
    return guest_fn_d771c(EF_U32(EF_U32(0x001f9078u) + index * 4u));
}

/* 0x101ba8, 171 bytes. Write the status line to the stream 0x4dd620: with the mode word 0x4dd634 equal to 1 one formatted line from the values at 0x4dd62c/3c/38/40 (format 0x19378c), otherwise 14 blank lines (0x1937a8) followed by mode-1 more separators (0x1937ac). Always returns 1. */
uint32_t write_status_lines_101ba8(void)
{
    uint32_t value;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t n;
    uint32_t i;
    guest_tail_110dcc(0u);
    value = EF_U32(0x004dd62cu);
    b = EF_U32(0x004dd63cu);
    c = EF_U32(0x004dd638u);
    d = EF_U32(0x004dd640u);
    guest_tail_110dd9(0u);
    if (EF_U32(0x004dd634u) == 1u) {
        guest_fn_bc400(EF_U32(0x004dd620u), 0x0019378cu, value, b, c, d);
    } else {
        for (i = 0u; i < 14u; i++)
            guest_fn_bc400(EF_U32(0x004dd620u), 0x001937a8u);
    }
    n = EF_U32(0x004dd634u);
    for (i = 1u; i < n; i++) {
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001937acu);
        n = EF_U32(0x004dd634u);
    }
    return 1u;
}

/* 0xf2ac0, 188 bytes. Frame update of the remote view when either activity flag (0x41f7c6, 0x41f7ba) and the enable flag 0x41f7a0 are set: fetch the view state, run 0xdd870 and decode the 0x39180 result: 0 sends the parameter block (0xf2a30), 1 reports 0x1918e8, 2 reloads the palette 0x425c8c and sets 0x161e20. Unless 0x4859de is set the screen is then refreshed with 0xc386c, 0xdc250 and 0xc3990. */
void update_remote_view_f2ac0(void)
{
    uint32_t r;
    if (EF_U8(0x0041f7c6u) == 0u && EF_U8(0x0041f7bau) == 0u)
        return;
    if (EF_U8(0x0041f7a0u) == 0u)
        return;
    get_view_state_dd988(0x00485920u, 0x0048591cu, 0x0048594eu, 0x0048594au,
                         0x00485954u, 0x00485952u, 0x00485950u, 0x0048594cu);
    guest_fn_dd870_s();
    r = tail_jump_39180(0u) & 0xffffu;
    if (r == 0u) {
        send_param_block_f2a30();
    } else if (r == 1u) {
        guest_callptr1_x(EF_U32(0x00132a74u), 0x001918e8u);
    } else if (r == 2u) {
        guest_fn_a7bd4_x();
        apply_palette_d663c(0x00425c8cu);
        EF_U8(0x00161e20u) = 1u;
    }
    if (EF_U8(0x004859deu) == 0u) {
        guest_fn_c386c();
        guest_fn_dc250();
        guest_fn_c3990();
    }
}

/* 0xe5db4, 207 bytes. Draw the NUL-terminated string STR at (X, Y) in colour COLOR (stored in 0x433eb0) with 6-pixel cells (glyph = character & 0x3f), clipped against the rectangle 0x17cf9c/a0/a4/a8: glyphs fully inside use 0xe5c30 (only when the text also fits vertically), partially visible ones 0xe5cf0. */
void draw_clipped_text_e5db4(uint32_t color, int32_t x, int32_t y, uint32_t str)
{
    int32_t top = EF_S32(0x0017cfa4u);
    int32_t bottom = EF_S32(0x0017cfa8u);
    EF_U8(0x00433eb0u) = (uint8_t)color;
    if (y + 7 < top)
        return;
    if (y > bottom)
        return;
    if (y < top || y + 7 > bottom) {
        while (EF_U8(str) != 0u) {
            if (x + 6 > EF_S32(0x0017cf9cu) && x < EF_S32(0x0017cfa0u))
                draw_glyph_7x7_e5cf0(x, y, (int32_t)(EF_U8(str) & 0x3fu));
            x += 6;
            str++;
        }
        return;
    }
    while (EF_U8(str) != 0u) {
        if (x >= EF_S32(0x0017cf9cu) && x + 6 < EF_S32(0x0017cfa0u))
            draw_glyph_e5c30((uint32_t)x, (uint32_t)y, EF_U8(str) & 0x3fu);
        else if (x + 6 > EF_S32(0x0017cf9cu) && x < EF_S32(0x0017cfa0u))
            draw_glyph_7x7_e5cf0(x, y, (int32_t)(EF_U8(str) & 0x3fu));
        x += 6;
        str++;
    }
}

/* 0xb4ee0, 211 bytes. Pick the behaviour class of the object OBJ from its type byte (+2) and store the matching handler table in +0x74. Types 2 and 10 map the code byte at *(+0x51) through 0xba0e4 and use (result - 1): 0 and 6 give 1 (table 0x133a70), 1 gives 1 or, when bit 0x20 of the 0xa45b3 word is set, 3 (0x133e70), 2 gives 3, 4 gives 4 (0x1338ac) when +0x5b is 10 and else 3, 5 gives 2 (0x133c34), and 3 or anything above 6 gives -1 without a table. Type 1 gives 5 (0x133ec0), type 11 gives 4 (0x1338ac), others -1. */
int32_t classify_object_b4ee0(uint32_t obj)
{
    uint32_t t = EF_U8(obj + 2u);
    uint32_t cls;
    int32_t r;
    uint32_t table;
    if (t == 2u || t == 10u) {
        cls = (uint8_t)(map_code_ba0e4(EF_U8(EF_U32(obj + 0x51u))) - 1u);
        if (cls > 6u)
            return -1;
        switch (cls) {
        case 0: case 6:
            r = 1; table = 0x00133a70u; break;
        case 1:
            if ((guest_fn_a45b3() & 0x20u) != 0u) { r = 3; table = 0x00133e70u; }
            else { r = 1; table = 0x00133a70u; }
            break;
        case 2:
            r = 3; table = 0x00133e70u; break;
        case 3:
            return -1;
        case 4:
            if (EF_U8(obj + 0x5bu) == 10u) { r = 4; table = 0x001338acu; }
            else { r = 3; table = 0x00133e70u; }
            break;
        default:
            r = 2; table = 0x00133c34u; break;
        }
        EF_U32(obj + 0x74u) = table;
        return r;
    }
    if (t == 1u) {
        EF_U32(obj + 0x74u) = 0x00133ec0u;
        return 5;
    }
    if (t == 11u) {
        EF_U32(obj + 0x74u) = 0x001338acu;
        return 4;
    }
    return -1;
}

/* 0x15a14, 216 bytes. Draw STR in colour COLOR with the proportional bitmap font at (X, Y): a space advances 3, characters with bit 7 set use the glyph map 0x122b08 (index = c & 0x1f, drawn 2 pixels higher), others the map 0x122ac8 (index = c & 0x3f); an index of -1 draws nothing and does not advance. Each drawn glyph (0xcf7c4 with the bitmap pointer from 0x122b64) advances by its width byte 0x122b28[index] + 1. */
void draw_prop_text_15a14(uint32_t color, int32_t x, int32_t y, uint32_t str)
{
    int16_t px = (int16_t)x;
    int16_t i;
    for (i = 0;; i++) {
        uint32_t c = EF_U8(str + (uint32_t)(int32_t)i);
        int32_t idx;
        if (c == 0u)
            break;
        if (c == 0x20u) {
            px = (int16_t)(px + 3);
            continue;
        }
        if ((c & 0x80u) != 0u) {
            idx = (int8_t)EF_U8(0x00122b08u + (c & 0x1fu));
            if (idx == -1)
                continue;
            guest_fn_cf7c4_x(EF_U32(0x00122b64u + (uint32_t)idx * 4u), (uint32_t)((int32_t)(int16_t)y - 2), (uint32_t)(int32_t)px, color & 0xffu);
        } else {
            idx = (int8_t)EF_U8(0x00122ac8u + (c & 0x3fu));
            if (idx == -1)
                continue;
            guest_fn_cf7c4_x(EF_U32(0x00122b64u + (uint32_t)idx * 4u), (uint32_t)(int32_t)(int16_t)y, (uint32_t)(int32_t)px, color & 0xffu);
        }
        px = (int16_t)(px + EF_U8(0x00122b28u + (uint32_t)idx) + 1);
    }
}

/* 0xc90e0, 230 bytes. Show the splash picture named by the string at 0x41ef06 (dropped when 0x9d524 rejects the name): allocate the 0x3e800-byte picture buffer 0x41eeac on first use, load "<default path><name><suffix>" through the driver call 0xc34cc, copy a 640x400 block into the buffer with 0xeb1f0, blit it with 0xe9a24 and refresh the screen (0xc3974 before, 0xc39ac after). */
void show_splash_c90e0(void)
{
    char buf[0x90];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    if (EF_U8(0x0041ef06u) == 0u)
        return;
    if (guest_fn_9d524(0x0041ef06u, 0x0018efdcu) == 0u) {
        EF_U8(0x0041ef06u) = 0u;
        return;
    }
    if (EF_U32(0x0041eeacu) == 0u) {
        EF_U32(0x0041eeacu) = arena_take_e29e8(0x0003e800u);
        if (EF_U32(0x0041eeacu) == 0u)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x0018efe0u);
    }
    guest_fn_c3974();
    build_path_9c9c0(b, 0x0018f010u);
    strcat_9ca57(b, 0x0041ef06u);
    strcat_9ca57(b, 0x0018f01cu);
    if ((start_driver_c34cc(b) & 0xffffu) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018f024u, b);
    get_screen_rows_eb1f0(0u, 0u, 0u, 0u, 0u, 0u, 0x280u, 0x190u, EF_U32(0x0041eeacu));
    guest_fn_e9a24(0x2d0u, 0x00425cbcu, 0x00419600u);
    guest_fn_c39ac();
}

/* 0xcdd18, 230 bytes. Plot a four-plane sprite (width 0x4201a0, height 0x4201a4) read from SRC into the frame buffer at DST: pass P (0..3) writes the column P + 4 * i of every row whose index lies in [YMIN, YMAX] when that column lies in [XMIN, XMAX], skipping zero source bytes; the source keeps advancing from pass to pass. Registers EAX/EDX/EBX/ECX plus two stack words (ret 8). */
void draw_planar_sprite_cdd18(uint32_t dst, uint32_t src, uint32_t xmin, uint32_t xmax, uint32_t ymin, uint32_t ymax)
{
    int32_t pass;
    for (pass = 0; pass < 4; pass++) {
        uint32_t row_ptr = dst + (uint32_t)pass;
        int32_t row;
        int32_t height = EF_S32(0x004201a4u);
        if (height <= 0)
            continue;
        for (row = 0; row < height; row++) {
            if (!((int32_t)(int16_t)ymin > row || (int32_t)(int16_t)ymax < row)) {
                int32_t width = EF_S32(0x004201a0u);
                int32_t i;
                int32_t x = pass;
                for (i = 0; i < width; i++, x += 4) {
                    uint8_t c = EF_U8(src + (uint32_t)i);
                    if (c != 0u && x >= (int32_t)(int16_t)xmin && x <= (int32_t)(int16_t)xmax)
                        EF_U8(row_ptr + (uint32_t)i * 4u) = c;
                }
            }
            src += EF_U32(0x004201a0u);
            row_ptr += EF_U32(0x001631f4u);
            height = EF_S32(0x004201a4u);
        }
    }
}

/* 0xd8528, 231 bytes. Compute the five projected corner words 0x15baf2/f6/fa/fe (and the constants 0x15baf4, 0x15bb00 = 0, 0x15baf8 = 0x15bafc = 0x15bdcc + 1) of the view rectangle from the window words at 0x420e76..0x420e7e, the scale pair 0x420eaa/0x420eac and the range 0x15bdcc. */
void compute_view_corners_d8528(void)
{
    int32_t a = (int16_t)EF_U16(0x00420e7au);
    int32_t b = (int16_t)EF_U16(0x00420eaau);
    int32_t c = (int16_t)EF_U16(0x00420eacu);
    int32_t d0 = (int16_t)EF_U16(0x00420e78u);
    int32_t one = (int32_t)EF_U32(0x0015bdccu) + 1;
    int32_t e = (int16_t)EF_U16(0x00420e7eu);
    int32_t f = (int16_t)EF_U16(0x00420e7cu);
    int32_t k;
    EF_U16(0x0015bb00u) = 0u;
    EF_U16(0x0015baf4u) = 0u;
    EF_U16(0x0015bafcu) = (uint16_t)(EF_U16(0x0015bdccu) + 1u);
    EF_U16(0x0015baf8u) = (uint16_t)(EF_U16(0x0015bdccu) + 1u);
    k = (c * 5) >> 15;
    EF_U16(0x0015baf2u) = (uint16_t)((a * b) / c + d0 - k);
    EF_U16(0x0015baf6u) = (uint16_t)(d0 - ((one - a) * b) / c - k);
    EF_U16(0x0015bafau) = (uint16_t)(f - ((one - e) * b) / c - k);
    EF_U16(0x0015bafeu) = (uint16_t)((e * b) / c + f - k);
}

/* 0xdde80, 231 bytes. Issue the DOS call 0x2f for the date/time request with the low byte of VALUE: fills the request block 0x4260b0 (bytes 0xb, VALUE) and the register block 0x426098 (0x1a, fields +0x10/+0xe from the block address, +0x12 = 7), stores the converted address in 0x4260c4/0x4260d6 and the call parameters 0x4260c8..0x4260d0, runs 0x8b8 on 0x4260b4 and returns the packed result 4500 * word(+4) + 75 * byte(+3) + byte(+2) - 150 of the reply block. */
int32_t dos_date_request_dde80(uint32_t value)
{
    uint32_t blk = EF_U32(0x004260b0u);
    uint32_t regs;
    uint32_t r = guest_fn_604();
    int32_t v;
    EF_U8(blk) = 0x0bu;
    EF_U8(blk + 1u) = (uint8_t)value;
    regs = EF_U32(0x00426098u);
    EF_U8(regs) = 0x1au;
    EF_U16(regs + 0x12u) = 7u;
    EF_U8(regs + 2u) = 3u;
    EF_U8(regs + 1u) = EF_U8(0x0015ee98u);
    EF_U16(regs + 0x10u) = (uint16_t)(r >> 16);
    EF_U16(regs + 0x0eu) = (uint16_t)(r & 0xfu);
    r = guest_fn_604();
    EF_U16(0x004260d6u) = (uint16_t)(r >> 16);
    EF_U32(0x004260c4u) = r & 0xffffu;
    EF_U32(0x004260d0u) = 0x1510u;
    EF_U32(0x004260ccu) = (uint32_t)(int32_t)(int16_t)EF_U16(0x00426120u);
    EF_U32(0x004260c8u) = 0u;
    guest_fn_8b8(0x2fu, 0x004260b4u);
    blk = EF_U32(0x004260b0u);
    v = (int16_t)EF_U16(blk + 4u);
    return v * 4500 + (int32_t)EF_U8(blk + 3u) * 75 + (int32_t)EF_U8(blk + 2u) - 150;
}

/* 0x73734, 234 bytes. Record that unit A now targets unit B (word +0x143 of A's record). When A is the player unit (word 0x1f71ca) the target's class byte (+0xe9, record stride 0xe5 in 0x128636) goes to 0x65030; otherwise, for unit types 1, 3, 4 and 6 that already had a target, an event 0x2a (different owner word +0x15 in the 0x19bb58 table) or 0x2b (same owner) is raised through 0x46168. */
void set_unit_target_73734(int32_t a, int32_t b)
{
    int16_t ia = (int16_t)a;
    int16_t ib = (int16_t)b;
    uint32_t ra = guest_fn_7489c(ia);
    uint32_t rb;
    uint32_t t;
    if (ra == 0u)
        return;
    rb = guest_fn_7489c(ib);
    if (rb == 0u)
        return;
    if ((uint16_t)ia == EF_U16(0x001f71cau)) {
        uint32_t cls = EF_U8(rb + 0xe9u) * 0xe5u;
        guest_fn_65030(1u, (int32_t)ib, EF_U8(0x00128636u + cls));
    } else {
        t = EF_U8(ra + 0x16cu);
        if ((t == 1u || t == 3u || t == 4u || t == 6u) && EF_U16(ra + 0x143u) != 0u) {
            uint32_t base = EF_U32(0x0019bb58u);
            uint32_t p = (uint32_t)((int32_t)(int16_t)EF_U16(rb + 0x28u) * 0xca);
            uint32_t q = (uint32_t)((int32_t)(int16_t)EF_U16(ra + 0x28u) * 0xca);
            if (EF_U16(base + p + 0x15u) != EF_U16(base + q + 0x15u))
                guest_fn_46168((int32_t)ia, 0x2au, (uint32_t)(int32_t)ib, 0u, 0u);
            else
                guest_fn_46168((int32_t)ia, 0x2bu, (uint32_t)(int32_t)ib, 0u, 0u);
        }
    }
    EF_U16(ra + 0x143u) = (uint16_t)ib;
}

/* 0xcb6fc, 235 bytes. Walk the list whose head pointer is at HEAD (links at +4); for every node of kind 0 (byte +0xb) with all four group flag tests passing (0xcc1ac/0xcc1ec), the word 0x13799e above 2 and a 0xb0954 result below 7, update the group flags with 0xcc188 (twice) and 0xcc164 (twice). */
void update_group_flags_cb6fc(uint32_t head, uint32_t arg)
{
    uint32_t node = EF_U32(head);
    while (node != 0u) {
        if (EF_U8(node + 0xbu) == 0u &&
            (guest_fn_cc1ac(0u, 1u, head) & 0xffffu) != 0u &&
            (guest_fn_cc1ac(2u, 1u, head) & 0xffffu) != 0u &&
            (guest_fn_cc1ec_x(0u, 2u, head) & 0xffffu) != 0u &&
            (guest_fn_cc1ec_x(2u, 0u, head) & 0xffffu) != 0u &&
            (int16_t)EF_U16(0x0013799eu) > 2 &&
            (int16_t)guest_fn_b0954_x(0u, head, arg) < 7) {
            guest_fn_cc188_x(0u, 2u, head);
            guest_fn_cc188_x(2u, 0u, head);
            guest_fn_cc164_x(2u, 1u, head);
            guest_fn_cc164_x(1u, 2u, head);
        }
        node = EF_U32(node + 4u);
    }
}

/* 0x78848, 235 bytes. Draw the numbered entry list kept at 0x1a521c (byte ids terminated by 0xff, at most 0x7f): unless the mode byte 0x127da8 is set, wrap the cursor byte 0x1a5229 to 0 when above 12 or when it points at the terminator, then print each entry as "<number mod 10> <name from the table 0x126a5c>" (format 0x187b1c) at x = 10, y = 0x32 + 10 * row, in colour 12 for the cursor row and 15 otherwise. */
void draw_entry_list_78848(void)
{
    char buf[0x50];
    int16_t i;
    int32_t y;
    if (EF_U8(0x00127da8u) != 0u)
        return;
    if (EF_U8(0x001a5229u) > 12u)
        EF_U8(0x001a5229u) = 0u;
    if (EF_U8(0x001a521cu + EF_U8(0x001a5229u)) == 0xffu)
        EF_U8(0x001a5229u) = 0u;
    y = 0x32;
    for (i = 0; i < 0x7f; i++) {
        uint32_t id = EF_U8(0x001a521cu + (uint32_t)(int32_t)i);
        if (id == 0xffu)
            break;
        format_ui_status_pair((uint32_t)(uintptr_t)buf, 0x00187b1cu, (uint32_t)(((int32_t)i + 1) % 10), EF_U32(0x00126a5cu + id * 7u));
        EF_U8(0x00127b1au) = 1u;
        draw_shadowed_text_48c90(EF_U8(0x001a5229u) == (uint16_t)i ? 12u : 15u, 10, (int16_t)y, (uint32_t)(uintptr_t)buf);
        y += 10;
    }
}

/* 0xa48ac, 237 bytes. Mark the item ID (1..count in 0x41eed8, flag 8 set in byte +0x3b of its record in the table 0x41edfc) as ordered with flag 4. When ID is already in the id list 0x41eda8 only the flag is set; otherwise, if the list contains AFTER, ID is inserted behind it and the tail is shifted right by one word (the last entry drops out). */
void insert_ordered_id_a48ac(uint32_t id, uint32_t after)
{
    uint16_t count = EF_U16(0x0041eed8u);
    uint16_t i;
    uint16_t j;
    uint8_t found = 0u;
    uint16_t id16 = (uint16_t)id;
    if (id16 == 0u || id16 > count)
        return;
    if ((EF_U8(EF_U32(0x0041edfcu + (uint32_t)id16 * 4u) + 0x3bu) & 8u) == 0u)
        return;
    for (i = 0; i < count; i++) {
        uint16_t e = EF_U16(0x0041eda8u + (uint32_t)i * 2u);
        if (id16 == e) {
            EF_U8(EF_U32(0x0041edfcu + (uint32_t)e * 4u) + 0x3bu) |= 4u;
            return;
        }
    }
    i = 0;
    while (i < count && !found) {
        if ((uint16_t)after == EF_U16(0x0041eda8u + (uint32_t)i * 2u))
            found = 1u;
        i++;
    }
    if (!found)
        return;
    EF_U8(EF_U32(0x0041edfcu + (uint32_t)id16 * 4u) + 0x3bu) |= 4u;
    j = (uint16_t)(count - 1u);
    while (j > i) {
        EF_U16(0x0041eda8u + (uint32_t)j * 2u) = EF_U16(0x0041eda6u + (uint32_t)j * 2u);
        j--;
    }
    EF_U16(0x0041eda8u + (uint32_t)i * 2u) = id16;
}

/* 0x39cb4, 238 bytes. Distance between the player unit (word 0x1f71ca) and the unit named by 0x1a823e, from the position longs (+0, +4, +8) scaled by >> 8; any axis difference above 0x444 (or a negative sum) saturates the squared distance at 0x123210. Stores 100 * sqrt / 0x444 as a word at OUT_PCT and asks 0x86968 to fill the two-word result at OUT; when it declines both words become 0xffff. */
void compute_unit_distance_39cb4(uint32_t out, uint32_t out_pct)
{
    uint32_t a = guest_fn_7489c((int16_t)EF_U16(0x001f71cau));
    uint32_t b = guest_fn_7489c((int16_t)EF_U16(0x001a823eu));
    int32_t dx;
    int32_t dy;
    int32_t dz;
    int32_t d2;
    int32_t root;
    if (a == 0u || b == 0u)
        return;
    dx = (int32_t)(EF_U32(a) - EF_U32(b)) >> 8;
    dy = (int32_t)(EF_U32(a + 8u) - EF_U32(b + 8u)) >> 8;
    dz = (int32_t)(EF_U32(a + 4u) - EF_U32(b + 4u)) >> 8;
    if (dx > 0x444 || dy > 0x444 || dz > 0x444)
        d2 = 0x123210;
    else
        d2 = dy * dy + dx * dx + dz * dz;
    if (d2 > 0x123210 || d2 < 0)
        d2 = 0x123210;
    root = (int32_t)(uint16_t)guest_square_root_of(d2);
    EF_U16(out_pct) = (uint16_t)((root * 100) / 0x444);
    if (d2 > 0x123210 || d2 < 0)
        d2 = 0x123210;
    if ((guest_fn_86968_x((uint32_t)d2, out) & 0xffu) == 0u) {
        EF_U16(out) = 0xffffu;
        EF_U16(out + 2u) = 0xffffu;
    }
}

/* 0xa35bc, 242 bytes. Fold the per-slot samples into running statistics: for each entry of the id list 0x136b74 (count byte at *0x136b78) take the table record 0x135878[id * 32], the clamped (-100..100) difference of the clock word 0x1357d6 and the record's stamp (+0xb), and add it to the per-kind sum (0x1357ec) and count (0x1357da). Once more than 500 + kind ticks have passed since the kind's last report (0x13580c) the packed 7-byte report (kind, count, sum) is sent with 0xa3e50 (code 0xd, length 6) and the kind's counters are reset. */
void accumulate_samples_a35bc(void)
{
    int32_t i;
    for (i = 0; i < (int32_t)EF_U8(EF_U32(0x00136b78u)); i++) {
        uint32_t id = EF_U8(EF_U32(0x00136b74u) + (uint32_t)i);
        uint32_t rec = EF_U32(0x00135878u + id * 32u);
        int16_t diff = (int16_t)(EF_U16(0x001357d6u) - EF_U16(rec + 0xbu));
        uint32_t kind = EF_U8(rec + 6u);
        uint32_t sum;
        if (diff > 100)
            diff = 100;
        if (diff < -100)
            diff = -100;
        EF_U16(0x001357dau + kind * 2u)++;
        sum = EF_U32(0x001357ecu + kind * 4u) + (uint32_t)(int32_t)diff;
        EF_U32(0x001357ecu + kind * 4u) = sum;
        if ((uint32_t)(uint16_t)(EF_U16(0x001357d6u) - EF_U16(0x0013580cu + kind * 2u)) > kind + 0x1f4u) {
            uint8_t pkt[8];
            uint16_t n = EF_U16(0x001357dau + kind * 2u);
            pkt[0] = (uint8_t)kind;
            pkt[1] = (uint8_t)n;
            pkt[2] = (uint8_t)(n >> 8);
            pkt[3] = (uint8_t)sum;
            pkt[4] = (uint8_t)(sum >> 8);
            pkt[5] = (uint8_t)(sum >> 16);
            pkt[6] = (uint8_t)(sum >> 24);
            guest_fn_a3e50_x((uint32_t)(uintptr_t)pkt, 0xdu, 6u);
            EF_U16(0x001357dau + kind * 2u) = 0u;
            EF_U32(0x001357ecu + kind * 4u) = 0u;
            EF_U16(0x0013580cu + kind * 2u) = EF_U16(0x001357d6u);
        }
    }
}

/* 0x49bdc, 245 bytes. Rebuild the group tables from the list node reached by following the next links (+4) of P up to the word 0x125835 times: collect the class byte (+0x4e) of the units named in the group's roster (byte +0x12 entries at 0x1fcb40[group * 0x11] + 1) into the word table 0x1a53b0, run 0x49cd4, and then for every group copy each unit's slot word (+0x50) into 0x1a5330 and the matching unit's word +0x47 into 0x1a52b0 (rows of 16 bytes). */
void rebuild_group_tables_49bdc(uint32_t p)
{
    int16_t n = (int16_t)EF_U16(0x00125835u);
    int16_t i;
    int16_t j;
    int16_t groups;
    int16_t cnt = 0;
    if (p != 0u) {
        while (cnt < n) {
            p = EF_U32(p + 4u);
            cnt++;
            if (p == 0u)
                break;
        }
    }
    cnt = 0;
    for (i = 0; i < (int16_t)EF_U8(p + 0x12u); i++) {
        uint32_t roster = EF_U32(0x001fcb40u) + (uint32_t)((int32_t)(int16_t)EF_U16(0x00125835u) * 0x11);
        int16_t unit = (int16_t)EF_U16(roster + (uint32_t)(int32_t)i * 2u + 1u);
        uint32_t rec = (uint32_t)((int32_t)unit * 0xca) + EF_U32(0x0019bb58u);
        EF_U16(0x001a53b0u + (uint32_t)(int32_t)cnt * 2u) = EF_U8(rec + 0x4eu);
        cnt++;
    }
    copy_row_words_49cd4(p);
    groups = (int16_t)EF_U16(0x001a53c0u);
    for (i = 0; i < groups; i++) {
        for (j = 0; j < (int16_t)EF_U16(0x001a53b0u + (uint32_t)(int32_t)i * 2u); j++) {
            int16_t k = (int16_t)EF_U16(0x001a5280u + (uint32_t)(int32_t)i * 2u);
            uint32_t rec = (uint32_t)((int32_t)k * 0xca) + EF_U32(0x0019bb58u);
            uint32_t off = (uint32_t)(int32_t)j * 2u + (uint32_t)(int32_t)i * 16u;
            uint32_t u;
            EF_U16(0x001a5330u + off) = EF_U16(rec + 0x50u + (uint32_t)(int32_t)j * 2u);
            u = guest_fn_7489c((int16_t)EF_U16(0x001a5330u + off));
            EF_U16(0x001a52b0u + off) = EF_U16(u + 0x47u);
        }
        groups = (int16_t)EF_U16(0x001a53c0u);
    }
}

/* 0xe5f64, 248 bytes. Draw one 7x9 monochrome glyph (CODE bit 7 selects the table 0x15feb0 with index & 0x1f, otherwise 0x15fc70 with index & 0x3f; 9 bytes per glyph, MSB first) at (X, Y) in colour 0x433ec0 into the 8-bit frame buffer, clipped to the rectangle 0x17cf9c/a0/a4/a8 pixel by pixel. */
void draw_glyph_clipped_e5f64(int32_t x, int32_t y, uint32_t code)
{
    int32_t left = EF_S32(0x0017cf9cu);
    uint32_t glyph;
    uint32_t rowptr = 0u;
    int32_t yend = y + 9;
    if ((code & 0x80u) != 0u)
        glyph = 0x0015feb0u + (code & 0x1fu) * 9u;
    else
        glyph = 0x0015fc70u + (code & 0x3fu) * 9u;
    while (y != yend) {
        uint32_t bits = EF_U8(glyph++);
        if (y >= EF_S32(0x0017cfa4u) && y <= EF_S32(0x0017cfa8u)) {
            uint32_t mask = 0x80u;
            int32_t xx = x;
            uint32_t px;
            if (rowptr == 0u)
                rowptr = EF_U32(0x0015b754u) + EF_U32(0x00163200u + (uint32_t)y * 4u) + (uint32_t)x;
            for (px = rowptr; px != rowptr + 7u; px++, xx++) {
                if (xx >= left && xx <= EF_S32(0x0017cfa0u) && (bits & mask) != 0u)
                    EF_U8(px) = EF_U8(0x00433ec0u);
                mask = (mask & 0xffu) >> 1;
            }
        }
        if (rowptr != 0u)
            rowptr += EF_U32(0x001631f4u);
        y++;
    }
    EF_U32(0x0017cf9cu) = (uint32_t)left;
}

/* 0x50658, 248 bytes. Tally the roster kinds: for each of the 0x1a5531 entries (kind byte 0x1a5510[i], 1..4) count the kind in 0x1a5536[kind - 1] and mark the first entry of every kind in 0x1a5520[i]; 0x1a5534 receives the number of marked entries and 0x1a5535 the number of distinct kinds seen. */
void tally_roster_kinds_50658(void)
{
    uint8_t seen[4] = { 0u, 0u, 0u, 0u };
    int16_t i;
    EF_U8(0x001a5534u) = 0u;
    for (i = 0; i < 4; i++)
        EF_U8(0x001a5536u + (uint32_t)i) = 0u;
    for (i = 0; i < (int16_t)EF_U8(0x001a5531u); i++) {
        uint8_t k;
        EF_U8(0x001a5520u + (uint32_t)i) = 0u;
        k = (uint8_t)(EF_U8(0x001a5510u + (uint32_t)i) - 1u);
        if (k > 3u)
            continue;
        EF_U8(0x001a5536u + k)++;
        if (seen[k] == 0u) {
            EF_U8(0x001a5520u + (uint32_t)i) = 1u;
            EF_U8(0x001a5534u)++;
        }
        seen[k] = 1u;
    }
    EF_U8(0x001a5535u) = (uint8_t)(seen[0] + seen[1] + seen[2] + seen[3]);
}

/* 0xf2b7c, 250 bytes. Reset the remote cursor state: run 0xcdf60, copy the buffer pointer 0x485928 to 0x485924, clear the words/bytes 0x1616ea, 0x161e16..1a, 0x1616ec/ed, 0x1616e6/e8, copy the byte 0x1918fc to 0x485956 and 0x48592c, and clamp the cursor words 0x425fc8 (x) to 0..319 and 0x425fc6 (y) to 0..199. */
void reset_remote_cursor_f2b7c(void)
{
    int32_t v;
    uint8_t b;
    mouse_button_count_cdf60();
    EF_U32(0x00485924u) = EF_U32(0x00485928u);
    EF_U16(0x001616eau) = 0u;
    EF_U16(0x00161e16u) = 0u;
    EF_U16(0x00161e18u) = 0u;
    EF_U16(0x00161e1au) = 0u;
    EF_U8(0x001616ecu) = 0u;
    EF_U8(0x001616edu) = 0u;
    b = EF_U8(0x001918fcu);
    EF_U16(0x001616e6u) = 0u;
    EF_U8(0x00485956u) = b;
    EF_U8(0x0048592cu) = b;
    EF_U16(0x001616e8u) = 0u;
    v = (int16_t)EF_U16(0x00425fc8u);
    if (v < 0)
        v = 0;
    if (v > 0x13f)
        v = 0x13f;
    EF_U16(0x00425fc8u) = (uint16_t)v;
    v = (int16_t)EF_U16(0x00425fc6u);
    if (v < 0)
        v = 0;
    if (v > 0xc7)
        v = 0xc7;
    EF_U16(0x00425fc6u) = (uint16_t)v;
}

/* 0x3739c, 250 bytes. Compact the nine slot words (+0x28, stride 0x45 per group in the table 0x1a14f8) of the group UNIT: a slot is kept when it names a live record (0x8ffe8) without flag 2; an empty or dead slot is replaced by the next nonempty slot, whose record then gets the new slot index in byte +0x3b. */
void compact_slots_3739c(int32_t unit)
{
    int16_t u = (int16_t)unit;
    uint32_t tbl = EF_U32(0x001a14f8u);
    int32_t k;
    for (k = 0; k < 9; k++) {
        uint32_t base = tbl + (uint32_t)((int32_t)u * 0x45);
        uint16_t w = EF_U16(base + (uint32_t)k * 2u + 0x28u);
        uint32_t rec;
        int32_t j;
        if (w != 0u) {
            rec = guest_fn_8ffe8_x((uint32_t)(int32_t)(int16_t)w);
            if (rec != 0u && (EF_U8(rec + 0x30u) & 2u) == 0u)
                continue;
        }
        j = k + 1;
        while (EF_U16(base + (uint32_t)j * 2u + 0x28u) == 0u && j * 2 < 0x12)
            j++;
        if (j < 9) {
            EF_U16(base + (uint32_t)k * 2u + 0x28u) = EF_U16(base + (uint32_t)j * 2u + 0x28u);
            rec = guest_fn_8ffe8_x((uint32_t)(int32_t)(int16_t)EF_U16(base + (uint32_t)k * 2u + 0x28u));
            if (rec != 0u)
                EF_U8(rec + 0x3bu) = (uint8_t)k;
            EF_U16(base + (uint32_t)j * 2u + 0x28u) = 0u;
        }
    }
}

/* 0x8ae6c, 251 bytes. Map the key code 1..36 to its command number through a jump table (0 and anything above 36 report 0x18a7a4 through the error hook 0x132a74 and give 0). */
uint32_t map_key_to_command_8ae6c(uint32_t key)
{
    static const uint8_t map[37] = {
        0, 2, 3, 4, 5, 17, 18, 19, 6, 7, 8, 9, 21, 20, 22, 23, 10, 11,
        12, 13, 14, 15, 24, 25, 26, 27, 28, 1, 29, 30, 33, 34, 35, 42, 41, 38, 39
    };
    if (key <= 0x24u && map[key] != 0u)
        return map[key];
    guest_callptr2_x(EF_U32(0x00132a74u), 0x0018a7a4u, key);
    return 0u;
}

/* Shared body of the two script-record starters 0xa060c and 0x9fb00: set up the 3-word position 0x417188 from the page origin (0x41718e..92, with EXTRA added to the middle word), arm the call block 0x1600a8..c0 for the record REC and call it through 0x132b14. */
static void start_script_record(int32_t n, uint32_t extra)
{
    int16_t idx = (int16_t)n;
    uint32_t rec = (uint32_t)((int32_t)idx * 0x1d) + EF_U32(0x00417174u) + 0xbu;
    uint32_t pos = EF_U32(0x0041717cu) + (uint32_t)((int32_t)(int16_t)EF_U16(rec + 0x12u) * 2);
    guest_callptr4_x(EF_U32(0x00132af4u), 0u, 0u, 0u, pos);
    EF_U16(0x00417188u) = EF_U16(0x0041718eu);
    EF_U16(0x0041718au) = (uint16_t)(EF_U16(0x00417190u) + extra);
    EF_U16(0x0041718cu) = EF_U16(0x00417192u);
    EF_U32(0x001600b4u) = 0x00417188u;
    EF_U32(0x001600a8u) = (uint32_t)(int32_t)(int16_t)EF_U16(rec);
    EF_U32(0x001600b8u) = 1u;
    EF_U32(0x001600acu) = rec + 0x14u;
    EF_U32(0x001600b0u) = 0x00417154u;
    if (EF_U8(rec + 0x1cu) != 0u) {
        EF_U16(0x00417188u) = (uint16_t)(EF_U16(0x00417188u) + EF_U16(pos));
        EF_U16(0x0041718au) = (uint16_t)(EF_U16(0x0041718au) + EF_U16(pos + 2u));
        EF_U16(0x0041718cu) = (uint16_t)(EF_U16(0x0041718cu) + EF_U16(pos + 4u));
        EF_U32(0x001600b8u) = EF_U8(rec + 0x1cu);
    }
    guest_callptr1_x(EF_U32(0x00132b14u), (uint32_t)(int32_t)(int16_t)EF_U16(rec));
    guest_fn_160255_x();
}

/* 0xa060c, 245 bytes. Start the script record number N of the current page. */
void start_script_record_a060c(int32_t n)
{
    start_script_record(n, 0u);
}

/* 0x9fb00, 254 bytes. Start the script record number N of the current page, shifted down by the offset word 0x417222. */
void start_script_record_9fb00(int32_t n)
{
    start_script_record(n, EF_U16(0x00417222u));
}

/* 0xfb954, 254 bytes. Allocate a 4-byte route record (tag 0x1935bc) and fill it from the class A, sub-class B and slot C: class 0xe with B = 9 gives {12, 11, 17, 17}; class 0xe with B = 0 gives {3, 5, 9, 17} for slots 0, 2, 5 and {6, 4, 7, 8} for slots 1, 3, 4; classes 0x15..0x17 with B = 2 give {0, 1, 17, 17} (slots 0, 2, 5) or {4, 2, 17, 17}, and with B = 0 give {3, 5, 9, 17} or {7, 6, 7, 8}. Any other combination leaves the record untouched. Returns the record. */
uint32_t make_route_record_fb954(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t rec = guest_alloc_fmt_cc6c0(4u, 0x001935bcu);
    uint8_t cls = (uint8_t)a;
    uint8_t sub = (uint8_t)b;
    uint8_t slot = (uint8_t)c;
    int even = (slot == 0u || slot == 2u || slot == 5u);
    uint32_t v = 0u;
    int set = 0;
    if (cls == 0x0eu) {
        if (sub != 0u) {
            if (sub == 9u) { v = 0x1111060cu | 0u; set = 2; }
        } else if (slot <= 5u) {
            set = 1;
            v = even ? 0x11090503u : 0x08070406u;
        }
    } else if (cls >= 0x15u && cls <= 0x17u) {
        if (sub != 0u) {
            if (sub == 2u && slot <= 5u) {
                set = 1;
                v = even ? 0x11110100u : 0x11110204u;
            }
        } else if (slot <= 5u) {
            set = 1;
            v = even ? 0x11090503u : 0x08070607u;
        }
    }
    if (set == 2)
        v = 0x11110b0cu;
    if (set != 0) {
        EF_U8(rec) = (uint8_t)v;
        EF_U8(rec + 1u) = (uint8_t)(v >> 8);
        EF_U8(rec + 2u) = (uint8_t)(v >> 16);
        EF_U8(rec + 3u) = (uint8_t)(v >> 24);
    }
    return rec;
}

/* 0x1a47c, 259 bytes. Dispatch the pending key byte 0x41f833: refresh the flag words 0x19843c (0x41f7ba | 0x41f7c6), 0x198420 (0x41f7c8) and 0x198422 (0x41f7ad), then, if a key is pending, either (word 0x12288a set) forward it to 0x11de0 when the mode byte 0x122c53 is 1..4, or call the handler for it from the table 0x198000 (flag 0x19843c), 0x197e00 (flag 0x198420) or 0x198200, if present. The pending key is cleared in every case. */
void dispatch_pending_key_1a47c(void)
{
    uint32_t key;
    uint32_t fn;
    EF_U16(0x0019843cu) = (uint16_t)(EF_U8(0x0041f7bau) | EF_U8(0x0041f7c6u));
    EF_U16(0x00198420u) = EF_U8(0x0041f7c8u);
    EF_U16(0x00198422u) = EF_U8(0x0041f7adu);
    if (EF_U8(0x0041f833u) == 0u)
        return;
    key = EF_U8(0x0041f833u);
    if (EF_U16(0x0012288au) != 0u) {
        int32_t mode = (int8_t)EF_U8(0x00122c53u);
        if (mode >= 1 && mode <= 4)
            guest_fn_11de0_x(key);
    } else if (EF_U16(0x0019843cu) != 0u) {
        fn = EF_U32(0x00198000u + key * 4u);
        if (fn != 0u)
            guest_callptr0_x(fn);
    } else if (EF_U16(0x00198420u) != 0u) {
        fn = EF_U32(0x00197e00u + key * 4u);
        if (fn != 0u)
            guest_callptr0_x(fn);
    } else {
        fn = EF_U32(0x00198200u + key * 4u);
        if (fn != 0u)
            guest_callptr0_x(fn);
    }
    EF_U8(0x0041f833u) = 0u;
}

/* 0x11de0, 155 bytes. Look the key KEY up in the letter table of the current mode (mode byte 0x122c53: 1, 2 and 4 select table 0, 1 and 2): scan up to 18 entries (stride 0x1d from 0x121fcc, starting at the table's base word 0x121ec8[table]) for a first byte equal to KEY, and on a hit call 0x12d90 with the entry and table numbers, set the word 0x12288a and step the letter table (0x11d44). */
void find_letter_entry_11de0(uint32_t key)
{
    uint8_t mode = EF_U8(0x00122c53u);
    int16_t table;
    int16_t i = 0;
    int found = 0;
    if (mode == 1u)
        table = 0;
    else if (mode == 2u)
        table = 1;
    else if (mode == 4u)
        table = 2;
    else
        table = -1;
    if (table < 0 || table >= 3)
        return;
    while (i < 0x12 && !found) {
        int32_t base = (int16_t)EF_U16(0x00121ec8u + (uint32_t)(int32_t)table * 2u);
        if ((uint16_t)key == EF_U8(0x00121fccu + (uint32_t)((i + base) * 0x1d)))
            found = 1;
        else
            i++;
    }
    if (!found)
        return;
    guest_fn_12d90((uint32_t)(int32_t)i, (uint32_t)(int32_t)table);
    EF_U16(0x0012288au) = 1u;
    step_letter_table_11d44();
}

/* 0x74480, 259 bytes. Credit a hit by the unit A on the unit named by the word 0x197c28 (ignored when either is 0xff/-1, missing, or flagged 4 or 2 in byte +0x24): bumps the victim's owner counter byte (+0xc9 of its 0xca-byte owner record, owner word +0x28). Remote (non-local owner, 0x127da9 set) hits are reported through 0x571f0, local ones raise event 0x23 through 0x46168. */
void credit_hit_74480(int32_t a)
{
    int16_t ia = (int16_t)a;
    int16_t tgt = (int16_t)EF_U16(0x00197c28u);
    uint32_t ra;
    uint32_t rb;
    uint32_t owner;
    uint32_t flags;
    if (tgt == -1 || tgt == 0xff || ia == 0xff)
        return;
    ra = guest_fn_7489c(ia);
    rb = guest_fn_7489c(tgt);
    if (ra == 0u || rb == 0u)
        return;
    flags = EF_U8(ra + 0x24u);
    if ((flags & 4u) != 0u || (flags & 2u) != 0u)
        return;
    owner = (uint32_t)((int32_t)(int16_t)EF_U16(rb + 0x28u) * 0xca) + EF_U32(0x0019bb58u);
    EF_U8(owner + 0xc9u)++;
    if (EF_U8(0x00127da9u) != 0u && EF_U16(rb + 0x18u) != EF_U8(0x001a666bu)) {
        send_byte_word_byte_packet_3e(EF_U8(rb + 0x18u), (uint32_t)(int32_t)(int16_t)EF_U16(rb + 0x16u), EF_U8(owner + 0xc9u));
        return;
    }
    guest_fn_46168((int32_t)(int16_t)EF_U16(rb + 0x16u), 0x23u, EF_U8(owner + 0xc9u), 0u, 0u);
}

/* 0x757bc, 260 bytes. Blend COUNT palette triples starting at triple START between the palettes FROM and TO into DEST: each 6-bit component becomes (to - from) * T / 50 + from (T is the signed word in EAX); triples of exactly (63, 0, 63) in FROM are left untouched. Sets the palette-dirty byte 0x128548 (registers EAX/EDX/EBX/ECX plus two stack words, ret 8). */
void blend_palette_757bc(int32_t t, uint32_t to, uint32_t from, uint32_t dest, uint32_t count, uint32_t start)
{
    int16_t tt = (int16_t)t;
    int16_t i = (int16_t)start;
    uint16_t n = (uint16_t)count;
    while (n != 0u) {
        uint32_t o = (uint32_t)((int32_t)i * 3);
        uint32_t pf = from + o;
        uint32_t pt = to + o;
        if (!((int8_t)EF_U8(pf) == 0x3f && EF_U8(pf + 1u) == 0u && (int8_t)EF_U8(pf + 2u) == 0x3f)) {
            int32_t c;
            for (c = 0; c < 3; c++) {
                int32_t a = (int8_t)EF_U8(pf + (uint32_t)c);
                int32_t b = (int8_t)EF_U8(pt + (uint32_t)c);
                EF_U8(dest + o + (uint32_t)c) = (uint8_t)(((b - a) * (int32_t)tt / 50 + a) & 0x3f);
            }
        }
        i++;
        n--;
    }
    EF_U8(0x00128548u) = 1u;
}

/* 0x111c90, 262 bytes. One-time registration (flag 0x17e25c) of the data ranges of this module: the code range 0x111c90..0x114ad8 through 0x104840 and the buffers 0x17e1d8 (0x80 bytes), 0x17e258 and the dwords 0x4ddccc, d0, d4, d8, dc, b8, bc, b4, c0, c8, c4, b0 (4 bytes each) through 0x104908. */
void register_ranges_111c90(void)
{
    static const uint32_t words[] = {
        0x17e258u, 0x4ddcccu, 0x4ddcd0u, 0x4ddcd4u, 0x4ddcd8u, 0x4ddcdcu, 0x4ddcb8u,
        0x4ddcbcu, 0x4ddcb4u, 0x4ddcc0u, 0x4ddcc8u, 0x4ddcc4u, 0x4ddcb0u
    };
    unsigned i;
    if (EF_U32(0x0017e25cu) != 0u)
        return;
    guest_fn_104840(0x00111c90u, 0x00114ad8u);
    stack_range_104908(0x0017e1d8u, 0x80u);
    for (i = 0; i < sizeof(words) / sizeof(words[0]); i++)
        stack_range_104908(words[i], 4u);
    EF_U32(0x0017e25cu) = 1u;
}

/* 0x779f4, 264 bytes. Randomise the pilot record OBJ: a random 1-in-3 (word / 0x3333 equal to 1) picks the first name table (flag +0x34 = 1) instead of the second, and the strings of the name tables are copied to +0, +0x24, +0xc and +0x18; +0x32 = 3, the counters +0x35/37/39 and bytes +0x3b/3c are cleared, +0x33 = 0x14 + random / 0xa3d and the words +0x3d/3f/41 get random / 0x1111. */
void randomize_pilot_779f4(uint32_t obj)
{
    uint32_t t;
    if ((guest_fn_a45b3() & 0xffffu) / 0x3333u == 1u) {
        EF_U8(obj + 0x34u) = 1u;
        copy_string_9d749(obj, random_table_77b20());
        t = random_table_77bb0();
    } else {
        EF_U8(obj + 0x34u) = 0u;
        copy_string_9d749(obj, lookup_table_by_quotient());
        t = random_table_77b8c();
    }
    copy_string_9d749(obj + 0x24u, t);
    copy_string_9d749(obj + 0xcu, random_table_77b44());
    copy_string_9d749(obj + 0x18u, random_table_77b68());
    EF_U8(obj + 0x32u) = 3u;
    EF_U16(obj + 0x35u) = 0u;
    EF_U16(obj + 0x37u) = 0u;
    EF_U16(obj + 0x39u) = 0u;
    EF_U8(obj + 0x3bu) = 0u;
    EF_U8(obj + 0x3cu) = 0u;
    EF_U8(obj + 0x33u) = (uint8_t)((guest_fn_a45b3() & 0xffffu) / 0xa3du + 0x14u);
    EF_U16(obj + 0x3du) = (uint16_t)((guest_fn_a45b3() & 0xffffu) / 0x1111u);
    EF_U16(obj + 0x3fu) = (uint16_t)((guest_fn_a45b3() & 0xffffu) / 0x1111u);
    EF_U16(obj + 0x41u) = (uint16_t)((guest_fn_a45b3() & 0xffffu) / 0x1111u);
}

/* 0xa207c, 266 bytes. Step the animation script of OBJ (script pointer +0, time +4, wait +6): once the time word is at least 0x4000 the wait is cleared and opcodes are executed until one sets a wait: 0 loads the 3 words at +0xe, 1 copies the 6 bytes +0x14 to +0xe, 2 loads the 3 words at +0x14, 3 selects the frame handler +0x1a from the table 0x132b44, 4 restarts the time and loads the wait, 5 jumps by a signed word offset, 6 flags OTHER (+0x1c |= 0x40) and ends the step at once; unknown opcodes are skipped. Afterwards the handler is called and the wait is added to the time. */
void step_animation_script_a207c(uint32_t obj, uint32_t other)
{
    uint32_t ptr;
    uint16_t op;
    if (EF_U16(obj + 4u) >= 0x4000u) {
        EF_U16(obj + 6u) = 0u;
        while (EF_U16(obj + 6u) == 0u) {
            ptr = EF_U32(obj);
            EF_U32(obj) = ptr + 2u;
            op = EF_U16(ptr);
            ptr += 2u;
            switch (op) {
            case 0:
                EF_U16(obj + 0xeu) = EF_U16(ptr);
                EF_U16(obj + 0x10u) = EF_U16(ptr + 2u);
                EF_U16(obj + 0x12u) = EF_U16(ptr + 4u);
                EF_U32(obj) = ptr + 6u;
                break;
            case 1:
                EF_U32(obj + 0xeu) = EF_U32(obj + 0x14u);
                EF_U16(obj + 0x12u) = EF_U16(obj + 0x18u);
                break;
            case 2:
                EF_U16(obj + 0x14u) = EF_U16(ptr);
                EF_U16(obj + 0x16u) = EF_U16(ptr + 2u);
                EF_U16(obj + 0x18u) = EF_U16(ptr + 4u);
                EF_U32(obj) = ptr + 6u;
                break;
            case 3:
                EF_U32(obj) = ptr + 2u;
                EF_U32(obj + 0x1au) = EF_U32(0x00132b44u + (uint32_t)(int32_t)(int16_t)EF_U16(ptr) * 4u);
                break;
            case 4:
                EF_U16(obj + 4u) = 0u;
                EF_U32(obj) = ptr + 2u;
                EF_U16(obj + 6u) = EF_U16(ptr);
                break;
            case 5:
                EF_U32(obj) = ptr + (uint32_t)((int32_t)(int16_t)EF_U16(ptr) * 2);
                break;
            case 6:
                EF_U8(other + 0x1cu) |= 0x40u;
                return;
            default:
                break;
            }
        }
    }
    guest_callptr_eax_x(EF_U32(obj + 0x1au), obj);
    EF_U16(obj + 4u) = (uint16_t)(EF_U16(obj + 4u) + EF_U16(obj + 6u));
}

/* 0xd19ec, 272 bytes. Pack the 8 bit planes of an image row block: SRC holds 0x15b7cc groups of 8 bytes (bit 0 of each is one pixel's plane bit); for each of the 8 planes the low bits of every group form one byte (first byte in bit 7), the bytes are run-length coded behind DST with 0xd1924 and the source bytes are shifted right. Returns the total number of coded bytes. The original takes its scratch row from the stack (error 0x18f6f4 when it does not fit). */
uint32_t pack_bit_planes_d19ec(uint32_t dst, uint32_t src)
{
    uint8_t row[0x800];
    int32_t groups = EF_S32(0x0015b7ccu);
    uint32_t total = 0u;
    int plane;
    int32_t i;
    int k;
    if (groups > (int32_t)sizeof(row))
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018f6f4u);
    for (plane = 0; plane < 8; plane++) {
        uint32_t p = src;
        uint32_t n;
        for (i = 0; i < groups; i++, p += 8u) {
            uint8_t b = 0u;
            for (k = 0; k < 8; k++)
                b |= (uint8_t)((EF_U8(p + (uint32_t)k) & 1u) << (7 - k));
            row[i] = b;
            for (k = 0; k < 8; k++)
                EF_U8(p + (uint32_t)k) = (uint8_t)(EF_U8(p + (uint32_t)k) >> 1);
        }
        n = rle_encode_d1924(dst, (uint32_t)(uintptr_t)row);
        total += n;
        dst += n;
    }
    return total;
}

/* 0x740e4, 263 bytes. Assign the unit A to the slot SLOT (stack byte, 0xff = none) of its owner's roster: the unit's previous slot (byte +0x20b) is cleared (word +0xd of the 25-byte slot entry in the owner's table 0x1972e0[owner byte +0x5f]) when the new slot is 0xff, a new slot receives the unit number; then the bytes +0x209 (B), +0x20a (C) and +0x16d (D) and the slot number are stored in the unit record. (Registers EAX/EDX/EBX/ECX plus one stack byte, ret 4.) */
void assign_roster_slot_740e4(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t slot)
{
    uint32_t rec = guest_fn_7489c((int16_t)a);
    uint32_t owner;
    uint32_t prev;
    uint32_t table;
    if (rec == 0u || EF_U8(0x00125847u) == 0u)
        return;
    slot &= 0xffu;
    owner = (uint32_t)((int32_t)(int16_t)EF_U16(rec + 0x28u) * 0xca) + EF_U32(0x0019bb58u);
    table = EF_U32(0x001972e0u) + (uint32_t)EF_U8(owner + 0x5fu) * 0x22u;
    if (slot == 0xffu) {
        prev = EF_U8(rec + 0x20bu);
        if (prev != 0xffu) {
            /* the original recomputes the owner record here; same value */
            EF_U16(EF_U32(table + 0x1eu) + prev * 25u + 0xdu) = 0xffu;
        }
    }
    if (slot != 0xffu)
        EF_U16(EF_U32(table + 0x1eu) + slot * 25u + 0xdu) = (uint16_t)a;
    EF_U8(rec + 0x209u) = (uint8_t)b;
    EF_U8(rec + 0x20au) = (uint8_t)c;
    EF_U8(rec + 0x20bu) = (uint8_t)slot;
    EF_U8(rec + 0x16du) = (uint8_t)d;
}

/* 0x104580, 273 bytes. Reset the 19 runtime settings (via 0x101e14: index, value) to their defaults after 0x103d80, clear the 16 dwords at 0x4dd670..0x4dd6ac and the byte 0x4dd6b0, and continue into 0x110fd4 (tail jump). */
void reset_settings_104580(void)
{
    static const uint16_t defaults[19] = {
        0xc8, 1, 0x8000, 0x64, 0x10, 0x64, 0x28f, 0, 0, 1, 0, 0x78, 8, 0x7f, 1, 0, 2, 1, 1
    };
    uint32_t i;
    init_locked_ranges_103d80();
    for (i = 0; i < 19u; i++)
        trace_call_101e14(0u, 0u, 0u, 0u, i, defaults[i]);
    for (i = 4u; i != 0x44u; i += 4u)
        EF_U32(0x004dd66cu + i) = 0u;
    EF_U8(0x004dd6b0u) = 0u;
    guest_fn_110fd4_x();
}

/* 0xcdb20, 277 bytes. Set up the 320x200 video mode: on the first call read the environment string at 0x18f620, flag the options named 0x18f62c, 0x18f634 and 0x18f644 in the bytes 0x146e48..4a, store the driver word 0xfe700 in 0x15b75c and register the exit handler 0xcdc64. Every call selects mode 0x13 (0xfe710), sets the size words 0x15b744/48 to 320 x 200, applies it with 0xfc39c, picks the draw handlers, points the frame base 0x15b758 at 0xa0000 + the DOS base (option 0x146e4a) or at a fresh 0xfc84-byte block (error 0x18f64c), initialises the work buffers and finally calls the two driver hooks 0x15b750 and 0x15b74c (clearing the screen in between). */
void init_video_mode_cdb20(void)
{
    if (EF_U8(0x0015b75eu) == 0u) {
        uint32_t env;
        EF_U8(0x0015b75eu) = 1u;
        env = getenv_9c858(0x0018f620u);
        if (env != 0u) {
            uppercase_string(env);
            if (guest_fn_a4380(env, 0x0018f62cu) == 0u)
                EF_U8(0x00146e48u) = 1u;
            if (guest_fn_a4380(env, 0x0018f634u) == 0u)
                EF_U8(0x00146e49u) = 1u;
            if (guest_fn_a4380(env, 0x0018f644u) == 0u)
                EF_U8(0x00146e4au) = 1u;
        }
        EF_U16(0x0015b75cu) = (uint16_t)guest_fn_fe700_w();
        guest_fn_9c3cf(0x000cdc64u);
    }
    guest_fn_fe710_s(0x13u);
    EF_U32(0x0015b744u) = 0x140u;
    EF_U32(0x0015b748u) = 0xc8u;
    guest_fn_fc39c_s(0xc8u, 0x140u);
    select_draw_handlers_c3a74();
    if (EF_U8(0x00146e4au) != 0u)
        EF_U32(0x0015b758u) = EF_U32(0x00120cc6u) + 0xa0000u;
    if (EF_U32(0x0015b758u) == 0u) {
        EF_U32(0x0015b758u) = guest_fn_c3f70(0xfc84u);
        if (EF_U32(0x0015b758u) == 0u)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x0018f64cu);
    }
    guest_fn_c3770();
    guest_callptr_eax_x(EF_U32(0x0015b750u), 0u);
    guest_fn_dc2c0();
    guest_callptr_eax_x(EF_U32(0x0015b74cu), 0u);
}

/* 0x9ab6c, 280 bytes. Dump the page header table at 0x40d006..0x40d024 as text lines to the stream STREAM through 0xbc400 (formats 0x13179b..0x131846 and the closing line 0x18b128); the values are the signed words of the table. */
void dump_page_header_9ab6c(uint32_t stream)
{
    guest_fn_bc400(stream, 0x0013179bu);
    guest_fn_bc400(stream, 0x001317b8u, (int32_t)(int16_t)EF_U16(0x0040d008u));
    guest_fn_bc400(stream, 0x001317cau, (int32_t)(int16_t)EF_U16(0x0040d00au));
    guest_fn_bc400(stream, 0x001317e0u, (int32_t)(int16_t)EF_U16(0x0040d00cu));
    guest_fn_bc400(stream, 0x0013180cu);
    guest_fn_bc400(stream, 0x001317f9u, (int32_t)(int16_t)EF_U16(0x0040d00eu), (int32_t)(int16_t)EF_U16(0x0040d016u), (int32_t)(int16_t)EF_U16(0x0040d01eu));
    guest_fn_bc400(stream, 0x00131823u, (int32_t)(int16_t)EF_U16(0x0040d010u), (int32_t)(int16_t)EF_U16(0x0040d018u), (int32_t)(int16_t)EF_U16(0x0040d020u));
    guest_fn_bc400(stream, 0x00131835u, (int32_t)(int16_t)EF_U16(0x0040d012u), (int32_t)(int16_t)EF_U16(0x0040d01au), (int32_t)(int16_t)EF_U16(0x0040d022u));
    guest_fn_bc400(stream, 0x00131846u, (int32_t)(int16_t)EF_U16(0x0040d014u), (int32_t)(int16_t)EF_U16(0x0040d01cu), (int32_t)(int16_t)EF_U16(0x0040d024u));
    guest_fn_bc400(stream, 0x0018b128u);
}

/* 0x77a8, 280 bytes. Initialise the roster state for N units: word 0x1972e4 = N, the table 0x1972e0 from 0x7780(N, 0x22), the anchor/limit longs 0x197270..0x1972bc (0xc0200000, 0xc0240000, 0xc0490000, 0x40200000, 0xc04e0000, ... zeros elsewhere; 0x1972b4 and 0x1972bc are stored twice, the later values win) and the mode bytes 0x1972e8..ed (1, 1, 2, 2, 3, 3). */
void init_roster_state_77a8(uint32_t n)
{
    EF_U16(0x001972e4u) = (uint16_t)(n & 0xffu);
    EF_U32(0x001972e0u) = guest_fn_7780_x(n & 0xffu, 0x22u);
    EF_U32(0x0019727cu) = 0xc0200000u;
    EF_U32(0x00197284u) = 0xc0240000u;
    EF_U32(0x00197290u) = 0u;
    EF_U32(0x00197294u) = 0xc0490000u;
    EF_U32(0x00197298u) = 0u;
    EF_U32(0x0019729cu) = 0xc0200000u;
    EF_U32(0x001972a0u) = 0u;
    EF_U32(0x001972a8u) = 0u;
    EF_U16(0x001972e6u) = 0u;
    EF_U8(0x001972e8u) = 1u;
    EF_U8(0x001972e9u) = 1u;
    EF_U32(0x001972acu) = 0x40200000u;
    EF_U32(0x001972b0u) = 0u;
    EF_U32(0x001972b8u) = 0u;
    EF_U32(0x00197270u) = 0u;
    EF_U32(0x00197274u) = 0u;
    EF_U32(0x00197278u) = 0u;
    EF_U32(0x00197280u) = 0u;
    EF_U32(0x00197288u) = 0u;
    EF_U32(0x001972a4u) = 0xc04e0000u;
    EF_U8(0x001972ecu) = 3u;
    EF_U32(0x0019728cu) = 0x40200000u;
    EF_U32(0x001972bcu) = 0x40200000u;
    EF_U8(0x001972eau) = 2u;
    EF_U8(0x001972ebu) = 2u;
    EF_U32(0x001972b4u) = 0xc05b8000u;
    EF_U8(0x001972edu) = 3u;
}

/* 0x102028, 283 bytes. Traced lookup (cdecl, two stack arguments): calls 0x103f04 and, with tracing on (0x17d2c8 == 1), logs the request (format 0x1939e4) and then either the six fields of the found record (name at +0x80, the pointer, the signed words +0x100..+0x106; formats 0x193a00..0x193a50) or a failure line (0x193a60). Returns the record. */
uint32_t traced_lookup_102028(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t p1, uint32_t p2)
{
    uint32_t r;
    uint32_t s;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x001939e4u, p1, p2);
    r = guest_fn_103f04_x(p1, p2);
    if (r != 0u) {
        if (EF_U32(0x0017d2c8u) == 1u) {
            s = EF_U32(0x004dd620u);
            guest_fn_bc400(s, 0x00193a00u, p1 + 0x80u);
            guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a10u, p1);
            guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a20u, (int32_t)(int16_t)EF_U16(p1 + 0x100u));
            guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a30u, (int32_t)(int16_t)EF_U16(p1 + 0x102u));
            guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a40u, (int32_t)(int16_t)EF_U16(p1 + 0x104u));
            guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a50u, (int32_t)(int16_t)EF_U16(p1 + 0x106u));
        }
        return r;
    }
    if (EF_U32(0x0017d2c8u) == 1u)
        guest_fn_bc400(EF_U32(0x004dd620u), 0x00193a60u, r);
    return r;
}

/* 0x3b58, 284 bytes. Draw the three status lines of the tactical header: the caption 0x1320d7 (colour 12, x = 0x112, y = 1), the target/mode line (built from the unit named by the pointer at 0x197061 and the mode tables, upper-cased and right-aligned at x = 0x13b - width, y = 10, colour 7) and the key hint line (y = 0x12, same alignment). */
void draw_tactical_header_3b58(void)
{
    char buf[0x3c];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    uint32_t w;
    uint8_t live;
    guest_fn_3ea8_x();
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(12u, 0x112, 1, 0x001320d7u);
    live = guest_fn_7574c((int32_t)(int16_t)EF_U16(EF_U32(0x00197061u) + 0x16u)) != 0u ? 1u : 0u;
    buf[0] = 0;
    if (EF_U8(0x00197088u) == 0xffu)
        guest_fn_a4342_va(b, 0x00131fe6u);
    if (!live)
        guest_fn_a4342_va(b, 0x0017f620u, 0x0013208fu, EF_U32(0x00121110u + (uint32_t)EF_U8(0x00197080u) * 4u));
    uppercase_string(b);
    w = guest_fn_48d5c_x(b);
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(7u, (int16_t)(0x13bu - w), 10, b);
    guest_fn_a4342_va(b, 0x0017f628u, 0x00132088u, 0x00196ffbu, 0x0019702du);
    uppercase_string(b);
    w = guest_fn_48d5c_x(b);
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(7u, (int16_t)(0x13bu - w), 0x12, b);
}

/* 0x45574, 285 bytes. Run the enabled stages of the cockpit update for the view object A (EAX) with the secondary values S (EDX) and B (EBX): nothing happens unless one of the words 0x126554/56/58 is set; then each stage word (0x126560, 62, 64, 66, 5e, 5a, 5c) enables its group of 0x43xxx/0x44xxx/0x45xxx routines. */
void update_view_stages_45574(uint32_t a, uint32_t s, uint32_t b)
{
    if (EF_U16(0x00126554u) == 0u && EF_U16(0x00126556u) == 0u && EF_U16(0x00126558u) == 0u)
        return;
    if (EF_U16(0x00126560u) != 0u) {
        guest_fn_447ec_x(a, s);
        guest_fn_44c68_x(a, s);
        guest_fn_453f4_x(a, s);
    }
    if (EF_U16(0x00126562u) != 0u) {
        guest_fn_43f48_x(a, b);
        guest_fn_44f68_x(a, s);
    }
    if (EF_U16(0x00126564u) != 0u) {
        guest_fn_450f4_x(a, s);
        guest_fn_44aec_x(a, s);
        guest_fn_4496c_x(a, s);
    }
    if (EF_U16(0x00126566u) != 0u) {
        guest_fn_44de8_x(a, s);
        guest_fn_443fc_x(a, s);
    }
    if (EF_U16(0x0012655eu) != 0u) {
        guest_fn_436dc_x(a, b);
        guest_fn_43b14_x(a, b);
        guest_fn_45274_x(a, s);
    }
    if (EF_U16(0x0012655au) != 0u) {
        guest_fn_4357c_x(a, b);
        guest_fn_44aec_x(a, s);
    }
    if (EF_U16(0x0012655cu) != 0u) {
        guest_fn_433fc_x(a, b);
        guest_fn_43c84_x(a, b);
        guest_fn_43df4_x(a, b);
        guest_fn_441fc_x(a, s);
    }
}

/* 0xa5ef8, 285 bytes. Redraw the item ID (1..count, flag 4 set in byte +0x3b of its record): outside a draw (byte 0x13725c clear) the state is set to 2 and 0xe5870 decides (1: save the rect list, 2: abort and clear the state again, 0: proceed); then the record is rendered through 0xe5960, 0xe58a8, 0xa5b70, 0xa7260, 0xe599c, 0xe58a8 and 0xa5ca4, flushing with 0xab954 after the two passes when 0x137268 or 0x13725d is set. A started draw restores the rect list (0xe516c, when 0xe5870 returned 1) and clears the state. Reports error 0x18d03c when called while 0x41f2e5 is 1. */
void redraw_item_a5ef8(uint32_t id)
{
    uint32_t rec;
    uint8_t bl = 0u;
    uint8_t outer;
    if ((uint16_t)id == 0u || (uint16_t)id > EF_U16(0x0041eed8u))
        return;
    if (EF_U8(0x0041f2e5u) == 1u)
        guest_callptr3_x(EF_U32(0x00132a74u), 0x0018d03cu, EF_U32(0x0041edfcu + (uint32_t)(uint16_t)id * 4u), EF_U32(0x0041eea0u));
    rec = EF_U32(0x0041edfcu + (uint32_t)(uint16_t)id * 4u);
    if ((EF_U8(rec + 0x3bu) & 4u) == 0u)
        return;
    outer = EF_U8(0x0013725cu);
    if (outer == 0u) {
        EF_U8(0x0013725cu) = 2u;
        bl = (uint8_t)guest_fn_e5870_x(rec);
        if (bl == 1u) {
            save_rect_list_e5114();
        } else if (bl == 2u) {
            EF_U8(0x0013725cu) = outer;
            return;
        }
    }
    guest_fn_e5960(rec);
    clip_rect_to_screen_e58a8();
    guest_fn_a5b70_x(rec);
    if (EF_U8(0x00137268u) != 0u || EF_U8(0x0013725du) != 0u)
        tail_jump_ab954(0u);
    redraw_all_children_a7260(rec);
    copy_record_words_e599c(rec);
    clip_rect_to_screen_e58a8();
    guest_fn_a5ca4_x(rec);
    if (EF_U8(0x00137268u) != 0u || EF_U8(0x0013725du) != 0u)
        tail_jump_ab954(0u);
    if (EF_U8(0x0013725cu) == 2u) {
        if (bl == 1u)
            restore_rect_list_e516c();
        EF_U8(0x0013725cu) = 0u;
    }
}

/* 0x78e1c, 288 bytes. Draw the mission clock at (10, 0xbe) in colour 15: in mode 1 (byte 0x125846) the time is taken from the record at 0x1379c4 (its two words minus the epoch words 0x12583e/0x125840/0x1379a2, borrowing a minute when the seconds go negative, zero when the result is negative) and formatted as "0 h m" with 0x187b40, otherwise the hour is the word 0x12583c + 0x12b054 wrapped into 0..23 and formatted with the words 0x12583e and 0x125840. With the blink flag 0x1a3647 set the blink counter 0x12aeb4 appends 0x187b50 after its first tick (reset to -10 above 10). */
void draw_mission_clock_78e1c(void)
{
    char buf[0x14];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    if (EF_U8(0x00125846u) == 1u) {
        uint16_t hour = 0u;
        uint16_t minute = 0u;
        uint32_t rec = EF_U32(0x001379c4u);
        if (rec != 0u && ((int16_t)EF_U16(rec) > 0 || (int16_t)EF_U16(rec + 2u) > 0)) {
            hour = (uint16_t)(EF_U16(rec) - (EF_U16(0x0012583eu) - EF_U16(0x001379a2u)));
            minute = (uint16_t)(EF_U16(rec + 2u) - EF_U16(0x00125840u));
            if ((int16_t)minute < 0) {
                minute = (uint16_t)(minute + 0x3cu);
                hour--;
            }
            if ((int16_t)hour < 0) {
                hour = 0u;
                minute = 0u;
            }
        }
        guest_fn_a4342_va(b, 0x00187b40u, 0u, (int32_t)(int16_t)hour, (int32_t)(int16_t)minute);
    } else {
        int16_t h = (int16_t)(EF_U16(0x0012583cu) + EF_U16(0x0012b054u));
        while (h > 0x17)
            h = (int16_t)(h - 0x18);
        while (h < 0)
            h = (int16_t)(h + 0x18);
        guest_fn_a4342_va(b, 0x00187b40u, (int32_t)h, (int32_t)(int16_t)EF_U16(0x0012583eu), (int32_t)(int16_t)EF_U16(0x00125840u));
    }
    if (EF_U8(0x001a3647u) != 0u) {
        if ((int16_t)EF_U16(0x0012aeb4u) > 10) {
            EF_U16(0x0012aeb4u) = 0xfff6u;
        } else {
            if ((int16_t)EF_U16(0x0012aeb4u) > 0)
                strcat_9ca57(b, 0x00187b50u);
            EF_U16(0x0012aeb4u)++;
        }
    }
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(15u, 10, 0xbe, b);
}

/* 0x3a148, 288 bytes. Occasionally (1 in 100, or always in mode 3 of 0x125846) advance the follow slot of the unit A's owner target: the target unit (record word +0x50 of the owner record) must exist and 0x1a1506 be nonzero; its counter word in the table 0x1a1860 (indexed by byte +0x187) is incremented and wraps to 0 at the limit 0x1a1506. For an active entry of the table 0x1a14f8 (byte +0x13) that belongs to another owner (word +0x1d against the owner word +0x15), an entry whose byte +1 is 0xff is taken over through 0x39edc when 0x3a268 allows, otherwise 0x3a000 is called. */
void advance_follow_slot_3a148(int32_t a)
{
    int16_t ia = (int16_t)a;
    uint32_t owner = (uint32_t)((int32_t)ia * 0xca) + EF_U32(0x0019bb58u);
    uint32_t unit = guest_fn_7489c((int16_t)EF_U16(owner + 0x50u));
    uint32_t r;
    uint32_t cnt;
    uint32_t e;
    int32_t v;
    if (unit == 0u || EF_U16(0x001a1506u) == 0u)
        return;
    r = guest_fn_9c960() & 0xffffu;
    if (r % 100u == 0u && EF_U8(0x00125846u) != 3u)
        return;
    cnt = EF_U32(0x001a1860u) + (uint32_t)EF_U8(unit + 0x187u) * 4u;
    EF_U16(cnt)++;
    if ((int16_t)EF_U16(0x001a1506u) <= (int16_t)EF_U16(cnt))
        EF_U16(cnt) = 0u;
    v = (int16_t)EF_U16(cnt);
    e = EF_U32(0x001a14f8u) + (uint32_t)(v * 0x45);
    if (EF_U8(e + 0x13u) == 0u)
        return;
    if (EF_U16(owner + 0x15u) == EF_U16(e + 0x1du))
        return;
    if (EF_U8(e + 1u) == 0xffu) {
        if ((guest_fn_3a268_x((uint32_t)v) & 0xffu) != 0u)
            guest_fn_39edc_x((uint32_t)(int32_t)ia, (uint32_t)v);
    } else {
        guest_fn_3a000_x((uint32_t)(int32_t)ia);
    }
}

/* 0xac2e4, 289 bytes. Register the text file NAME in the recent-file list (21-byte names at 0x41e960, count word 0x41eecc, limit 16 with error 0x18dbac; names above 8 characters give error 0x18dbe4): a name already present (0x9d524 == 0) ends the call; otherwise it is appended and, when FLAG is set, also stored in the first free slot of the second list at 0x41eab0. The file is then loaded (0xf819c), the widgets and id tables are rebuilt and the list-changed byte 0x137255 is set. */
void add_recent_file_ac2e4(uint32_t name, uint32_t flag)
{
    uint16_t n = EF_U16(0x0041eeccu);
    uint16_t i;
    if (n >= 0x10u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018dbacu, 0x10u);
    if (guest_string_length(name) > 8u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018dbe4u, 8u);
    n = EF_U16(0x0041eeccu);
    for (i = 0; i < n; i++) {
        if (guest_fn_9d524(0x0041e960u + (uint32_t)i * 0x15u, name) == 0u)
            return;
        n = EF_U16(0x0041eeccu);
    }
    copy_string_9d749(0x0041e960u + (uint32_t)EF_U16(0x0041eeccu) * 0x15u, name);
    if ((uint8_t)flag != 0u) {
        uint32_t k = 0u;
        while (EF_U8(0x0041eab0u + k * 0x15u) != 0u && k < 0x10u)
            k++;
        if (k < 0x10u)
            copy_string_9d749(0x0041eab0u + k * 0x15u, name);
    }
    EF_U32(0x0041eea0u) = 0u;
    reload_text_file_f819c(name);
    update_all_widgets_a5980();
    clear_id_table_a4e6c();
    rebuild_id_table_a4e14();
    EF_U8(0x00137255u) = 1u;
    EF_U16(0x0041eeccu) = (uint16_t)(EF_U16(0x0041eeccu) + 1u);
}

/* 0x56318, 298 bytes. Tick the broadcast ring entry 0x127db8 (55-byte entries from 0x1a63c0, at most 10): its countdown word (+8) falls by the frame step 0x127d92; on expiry it restarts at 20, the repeat counter (+0xa) drops by one and, while it was nonzero, every other player (below the count byte 0x12583a, not the local player 0x1a666b) whose flag byte in the entry is clear gets the message 0x2b through 0xa3f74 (the player is recorded in +0x31). The ring position advances (wrapping at 10) and the idle word 0x127dae counts down to 0. */
void tick_broadcast_ring_56318(void)
{
    uint32_t e = (uint32_t)EF_S32(0x00127db8u) * 0x37u;
    uint32_t base = 0x001a63c0u + e;
    int16_t i;
    if (EF_U16(base + 8u) != 0u || EF_U16(base + 0xau) != 0u) {
        EF_U16(base + 8u) = (uint16_t)(EF_U16(base + 8u) - EF_U16(0x00127d92u));
        if ((int16_t)EF_U16(base + 8u) < 0) {
            uint16_t rep = EF_U16(base + 0xau);
            EF_U16(base + 8u) = 0x14u;
            if (rep != 0u) {
                EF_U16(base + 0xau) = (uint16_t)(rep - 1u);
                for (i = 0; i < (int16_t)EF_U8(0x0012583au); i++) {
                    if ((uint16_t)i != EF_U8(0x001a666bu) && EF_U8(base + (uint32_t)(int32_t)i) == 0u) {
                        EF_U16(0x001a63f1u + e) = (uint16_t)i;
                        guest_fn_a3f74_x(base + 0xcu, 0x2bu);
                    }
                }
            } else {
                EF_U16(base + 8u) = rep;
            }
        }
    }
    EF_S32(0x00127db8u) = EF_S32(0x00127db8u) + 1;
    if (EF_S32(0x00127db8u) >= 10)
        EF_S32(0x00127db8u) = 0;
    if (EF_U16(0x00127daeu) != 0u)
        EF_U16(0x00127daeu) = (uint16_t)(EF_U16(0x00127daeu) - EF_U16(0x00127d92u));
    if ((int16_t)EF_U16(0x00127daeu) < 0)
        EF_U16(0x00127daeu) = 0u;
}

/* 0x89698, 300 bytes. Pick the closest enemy for the unit OBJ: for every group of the table 0x1a14f8 (0x45-byte entries, 0x1a1506 of them) whose squared distance (position longs at +7, +0xb, +0xf scaled by >> 8 against the unit's position words) is inside the squared range taken from the unit-type table 0x12bca3[type * 0x91], test its ten member slots (words at +0x28): a live record (0x8ffe8) accepted by 0x906b4 that is not flagged 2 and closer than the current best (+0x63) and approved by 0x89060 becomes the new target (+0x5b) with its distance (+0x63). Returns whether a target was found. */
uint8_t find_nearest_target_89698(uint32_t obj)
{
    int16_t g;
    int16_t s;
    EF_U32(obj + 0x5bu) = 0u;
    for (g = 0; g < (int16_t)EF_U16(0x001a1506u); g++) {
        uint32_t grp = EF_U32(0x001a14f8u) + (uint32_t)((int32_t)g * 0x45);
        int32_t dx = (int32_t)(int16_t)EF_U16(obj + 2u) - (EF_S32(grp + 7u) >> 8);
        int32_t dy = (int32_t)(int16_t)EF_U16(obj + 6u) - (EF_S32(grp + 0xbu) >> 8);
        int32_t dz = (int32_t)(int16_t)EF_U16(obj + 0xau) - (EF_S32(grp + 0xfu) >> 8);
        int32_t d2 = dx * dx + dy * dy + dz * dz;
        int32_t range = EF_S32(0x0012bca3u + (uint32_t)EF_U8(obj + 0x18u) * 0x91u) >> 8;
        if (g == 0xff)
            continue;
        if (d2 >= range * range)
            continue;
        for (s = 0; s < 10; s++) {
            uint32_t slot = grp + (uint32_t)(int32_t)s * 2u;
            uint32_t unit;
            if (EF_U16(slot + 0x28u) == 0u)
                continue;
            unit = guest_lookup_object_8ffe8((int32_t)(int16_t)EF_U16(slot + 0x28u));
            if (unit == 0u)
                continue;
            if ((guest_fn_906b4_x((uint32_t)(int32_t)(int16_t)EF_U16(unit + 0x16u)) & 0xffu) == 0u)
                continue;
            if ((EF_U8(unit + 0x30u) & 2u) != 0u)
                continue;
            if (d2 >= EF_S32(obj + 0x63u))
                continue;
            if ((guest_fn_89060_x(obj, unit) & 0xffu) == 0u)
                continue;
            EF_U32(obj + 0x5bu) = unit;
            EF_U32(obj + 0x63u) = (uint32_t)d2;
        }
    }
    return EF_U32(obj + 0x5bu) != 0u ? 1u : 0u;
}

/* 0x9ccb8, 303 bytes. Open the resource NAME: first through the archive handles 0x40d1f0 (option bit 1 of 0x132ab0, result | 0x10000) and 0x40d1f4 (bit 2, result | 0x20000) via 0xebb58, otherwise as a plain file. A file whose 8-byte header carries the packed-data magic 0x524a or 0x4152 is closed again and loaded whole into a fresh buffer (0x40d1e8, size + 0x10 for 0x524a; errors 0x18b178 when the buffer was still in use and 0x18b190 when it cannot be allocated) with the end at 0x40d1e4 and the read pointer at 0x40d1ec, returning handle | 0x40000; any other file is rewound by 8 bytes and its handle returned. */
uint32_t open_resource_9ccb8(uint32_t name)
{
    uint8_t hdr[8];
    int32_t h;
    uint32_t r;
    uint32_t magic;
    uint32_t size;
    if (EF_U32(0x0040d1f0u) != 0u && (EF_U8(0x00132ab0u) & 1u) != 0u) {
        r = guest_fn_ebb58_x(name, EF_U32(0x0040d1f0u));
        if (r != 0xffffffffu)
            return r | 0x10000u;
    }
    if (EF_U32(0x0040d1f4u) != 0u && (EF_U8(0x00132ab0u) & 2u) != 0u) {
        r = guest_fn_ebb58_x(name, EF_U32(0x0040d1f4u));
        if (r != 0xffffffffu)
            return r | 0x20000u;
    }
    h = open_slot_a7900(0u, 0u, 0u, 0u, name);
    if (h == -1)
        return (uint32_t)h;
    read_handle_a79d4(0u, 0u, 0u, 0u, (uint32_t)h, 8u, (uint32_t)(uintptr_t)hdr);
    magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8);
    size = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) | ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
    if (magic != 0x524au && magic != 0x4152u) {
        guest_fn_a7ac8_x((uint32_t)h, 0xfffffff8u);
        return (uint32_t)h;
    }
    close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)h);
    if (EF_U32(0x0040d1e8u) != 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018b178u);
    if (magic == 0x524au)
        size += 0x10u;
    EF_U32(0x0040d1e8u) = guest_fn_c3f70(size);
    if (EF_U32(0x0040d1e8u) == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018b190u);
    guest_callptr_eaxedx_x(EF_U32(0x00132ab8u), name, EF_U32(0x0040d1e8u));
    EF_U32(0x0040d1ecu) = EF_U32(0x0040d1e8u);
    EF_U32(0x0040d1e4u) = EF_U32(0x0040d1e8u) + size;
    return (uint32_t)h | 0x40000u;
}

/* 0xb0b00, 306 bytes. Count the nodes of the list whose head pointer is at HEAD (links +4) that are of type 0x31 (byte +0xc), pass the filter 0xbfcd8(node, ARG) -> 0xbc7ec, and whose type word (+9) is one of the kind's classes: KIND 0 counts the types 0x11..0x16, 0x18..0x1a, KIND 1 the types 1..6, 8, 9 and 0xb..0xe; any other KIND counts nothing. */
uint32_t count_typed_nodes_b0b00(uint32_t kind, uint32_t head, uint32_t arg)
{
    uint32_t node = EF_U32(head);
    uint32_t count = 0u;
    uint8_t k = (uint8_t)kind;
    while (node != 0u) {
        if (k <= 1u && EF_U8(node + 0xcu) == 0x31u) {
            uint32_t t;
            if ((guest_fn_bc7ec(guest_fn_bfcd8_x(node, arg)) & 0xffffu) != 0u) {
                t = EF_U16(node + 9u);
                if (k == 0u) {
                    if ((t >= 0x11u && t <= 0x16u) || (t >= 0x18u && t <= 0x1au))
                        count++;
                } else {
                    if ((t >= 1u && t <= 6u) || t == 8u || t == 9u || (t >= 0xbu && t <= 0xeu))
                        count++;
                }
            }
        }
        node = EF_U32(node + 4u);
    }
    return count;
}

/* 0xf3a1c, 139 bytes. Skip script lines until the token buffer 0x48592c matches NAME (0xa4380 == 0) or the end marker string 0x1919c8; if the end marker was reached, format the "section not found" message (format 0x1919cc, NAME) into a 100-byte stack buffer and pass it to the error callback at 0x132a74. Remembers the previous script position (0x485928 -> 0x485924) and clears byte 0x1616ec first. */
void skip_to_section_f3a1c(uint32_t name)
{
    uint8_t buf[0x64];
    EF_U32(0x00485924u) = EF_U32(0x00485928u);
    EF_U8(0x001616ecu) = 0u;
    read_script_line_f2c78();
    read_script_token_f2cd0();
    while (guest_fn_a4380(name, 0x0048592cu) != 0u) {
        if (guest_fn_a4380(0x0048592cu, 0x001919c8u) == 0u)
            break;
        read_script_line_f2c78();
        read_script_token_f2cd0();
    }
    if (guest_fn_a4380(0x0048592cu, 0x001919c8u) == 0u) {
        guest_sprintf_a4342((uint32_t)(uintptr_t)buf, 0x001919ccu, name);
        guest_callptr1_x(EF_U32(0x00132a74u), (uint32_t)(uintptr_t)buf);
    }
}

/* 0xa6808, 309 bytes. Load the per-object geometry for REC from two 25-byte table entries at 0x13727e indexed by words +0x55 and +0x57 (nothing if both are 0): copy A's words +0x15/+0x17 to REC +0x37/+0x39, compute the two data pointers REC +0x8d/+0x91 (base 0x41eea8 + word +0x11 + (word +0x13 << 8)), set word +0x3b = 0x100, and derive the deltas +0x33 (x) and +0x35 (y) from the mode bits of byte +0x1c: 0 half the difference, 1 zero, 2 the full difference, 3 unchanged. X uses bits 4..5, Y bits 6..7. */
void load_object_geometry_a6808(uint32_t rec)
{
    uint32_t ia = EF_U16(rec + 0x55u);
    uint32_t ib = EF_U16(rec + 0x57u);
    uint32_t a, b, base, m;
    int32_t d;
    if (ia == 0u && ib == 0u)
        return;
    a = 0x0013727eu + ia * 25u;
    b = 0x0013727eu + ib * 25u;
    EF_U16(rec + 0x37u) = EF_U16(a + 0x15u);
    EF_U16(rec + 0x39u) = EF_U16(a + 0x17u);
    base = EF_U32(0x0041eea8u);
    EF_U32(rec + 0x8du) = base + EF_U16(a + 0x11u) + (EF_U16(a + 0x13u) << 8);
    EF_U32(rec + 0x91u) = base + EF_U16(b + 0x11u) + (EF_U16(b + 0x13u) << 8);
    EF_U16(rec + 0x3bu) = 0x100u;
    m = (EF_U8(rec + 0x1cu) >> 4) & 3u;
    if (m == 0u) {
        d = (int32_t)EF_U16(rec + 4u) - (int32_t)EF_U16(rec + 0x37u);
        d -= (d >> 31);
        EF_U16(rec + 0x33u) = (uint16_t)(d >> 1);
    } else if (m == 1u) {
        EF_U16(rec + 0x33u) = 0u;
    } else if (m == 2u) {
        EF_U16(rec + 0x33u) = (uint16_t)(EF_U16(rec + 4u) - EF_U16(rec + 0x37u));
    }
    m = EF_U8(rec + 0x1cu) >> 6;
    if (m == 0u) {
        d = (int32_t)EF_U16(rec + 6u) - (int32_t)EF_U16(rec + 0x39u);
        d -= (d >> 31);
        EF_U16(rec + 0x35u) = (uint16_t)(d >> 1);
    } else if (m == 1u) {
        EF_U16(rec + 0x35u) = 0u;
    } else if (m == 2u) {
        EF_U16(rec + 0x35u) = (uint16_t)(EF_U16(rec + 6u) - EF_U16(rec + 0x39u));
    }
}

/* 0x4db60, 309 bytes. Fill two text widgets of the dialog named at 0x184b0c (widgets 0x184cf8 and 0x184d38, or 0x184d30 for the second when byte 0x1a5532 is 0). Otherwise the first gets the 17-byte name entry selected by byte 0x1a5598 (table 0x1a53e3) and the second a description: "none" string 0x1323a3 when the selected slot byte (0x1a5544[sel]) is 0xff, the 21-byte-record type name via table 0x13315c when the object at 0x1a54f6 exists, else a string chosen by byte 0x1a559a (1 -> 0x13058d, 2 -> 0x13059e, 3 -> 0x1323c8, 4 -> 0x1323b4). */
void fill_selection_dialog_4db60(void)
{
    uint32_t root = find_named_or_abort_a46cc(0x00184b0cu);
    uint32_t w;
    uint32_t sel, slot, obj;
    if (EF_U8(0x001a5532u) == 0u) {
        w = find_widget_checked_a78bc(root, 0x00184cf8u);
        guest_bind_dialog_widget(w, 0x00184d30u);
        w = find_widget_checked_a78bc(root, 0x00184d38u);
        guest_bind_dialog_widget(w, 0x00184d30u);
        return;
    }
    sel = EF_U8(0x001a5598u);
    w = find_widget_checked_a78bc(root, 0x00184cf8u);
    guest_bind_dialog_widget(w, 0x001a53e3u + sel * 17u);
    w = find_widget_checked_a78bc(root, 0x00184d38u);
    slot = EF_U8(0x001a5544u + EF_U8(0x001a5598u));
    if (slot == 0xffu) {
        guest_bind_dialog_widget(w, 0x001323a3u);
        return;
    }
    obj = EF_U32(0x001a54f6u);
    if (obj != 0u) {
        int32_t i = (int32_t)(int16_t)slot;
        guest_bind_dialog_widget(w, EF_U32(0x0013315cu + EF_U8(EF_U32(obj + 0x2cu) + (uint32_t)(i * 21)) * 4u));
        return;
    }
    switch ((uint8_t)(EF_U8(0x001a559au) - 1u)) {
    case 0: guest_bind_dialog_widget(w, 0x0013058du); break;
    case 1: guest_bind_dialog_widget(w, 0x0013059eu); break;
    case 2: guest_bind_dialog_widget(w, 0x001323c8u); break;
    case 3: guest_bind_dialog_widget(w, 0x001323b4u); break;
    default: break;
    }
}

/* 0x91df4, 314 bytes. Apply the key-driven orientation nudges (byte flags 0x2144aa: 1/2 adjust the scratch matrix 0x21439c by +/-0x800, 4/8 rotate the camera matrix 0x12dbec by +/-0x200) and then clamp: if the pitch angle (0xd13ac) exceeds 0x2000 rotate by the word at 0x127d92 instead, and if the pitch or the second angle (0xd13dc) leave their allowed ranges restore the saved matrix and, for a pitch above 0x808, rotate back by -pitch. Sets byte 0x1a6d1f when either angle was nonzero on entry. */
void nudge_view_orientation_91df4(void)
{
    uint8_t saved[0x24];
    int32_t pitch, a, b;
    guest_fn_d099c(0x0021439cu, 0x0012dbecu);
    pitch = angle_d13ac(0x0012dbecu);
    b = (int16_t)forward_pair_d13dc(0x0012dbecu);
    if ((int16_t)pitch != 0)
        EF_U8(0x001a6d1fu) = 1u;
    if (b != 0)
        EF_U8(0x001a6d1fu) = 1u;
    if (EF_U8(0x002144aau) & 1u)
        guest_fn_d0ebc(0x0021439cu, 0x800u);
    if (EF_U8(0x002144aau) & 2u)
        guest_fn_d0ebc(0x0021439cu, 0xf800u);
    guest_fn_d099c(0x0012dbecu, 0x0021439cu);
    copy_record9_d0858((uint32_t)(uintptr_t)saved, 0x0012dbecu);
    if ((int16_t)pitch > 0x2000) {
        guest_fn_d0dbc(0x0012dbecu, EF_U16(0x00127d92u), 0u);
        return;
    }
    if (EF_U8(0x002144aau) & 4u)
        guest_fn_d0dbc(0x0012dbecu, 0x200u, 0u);
    if (EF_U8(0x002144aau) & 8u)
        guest_fn_d0dbc(0x0012dbecu, 0xfe00u, 0u);
    pitch = (int16_t)angle_d13ac(0x0012dbecu);
    a = pitch;
    if (a >= 0x2000 || a < -0x3000)
        copy_record9_d0858(0x0012dbecu, (uint32_t)(uintptr_t)saved);
    if (b >= 0x5200 || b < -0x5200) {
        if ((int16_t)pitch > 0x808)
            guest_fn_d0dbc(0x0012dbecu, (uint16_t)(-pitch), 0u);
    }
}

/* 0x117bfc, 314 bytes. Serial port interrupt service for the channel record REC (port base at +0x1c, state block at +0x2c, receive ring descriptor at +0xa with buffer +0xe, transmit-enable at +0x14): mark the state flags |= 0x10, then loop on the interrupt identification register (port+2) until no interrupt is pending, handling modem status (0: CTS low sets flag 2, CTS high with flag 2 set calls the transmit pump 0x117b18), THR empty (2: pump), received data (4: drain the receiver into the ring while the line status (port+5) shows data and the ring is not full) and line status (6: read port+5). Finishes by sending end-of-interrupt (0x20 to port 0x20); the same EOI is sent when the record has no port or state block. */
void serial_interrupt_117bfc(uint32_t rec)
{
    uint32_t port, st;
    if (rec == 0u)
        return;
    port = EF_U32(rec + 0x1cu);
    st = EF_U32(rec + 0x2cu);
    if (port == 0u || st == 0u) {
        guest_fn_117b0c(0x20u, 0x20u);
        return;
    }
    EF_U16(st + 4u) = (uint16_t)(EF_U16(st + 4u) | 0x10u);
    for (;;) {
        uint32_t iir = guest_fn_9c430(EF_U32(rec + 0x1cu) + 2u) & 0xffu;
        uint32_t id = iir & 7u;
        if (iir & 1u)
            break;
        if (id > 6u)
            continue;
        switch (id) {
        case 0: {
            uint32_t ms = guest_fn_9c430(EF_U32(rec + 0x1cu) + 6u) & 0xffu;
            if (EF_U32(rec + 0x14u) == 0u)
                break;
            if ((ms & 0x10u) == 0u) {
                EF_U16(EF_U32(rec + 0x2cu) + 4u) = (uint16_t)(EF_U16(EF_U32(rec + 0x2cu) + 4u) | 2u);
            } else if (EF_U16(EF_U32(rec + 0x2cu) + 4u) & 2u) {
                guest_fn_117b18_x(rec);
            }
            break;
        }
        case 2:
            guest_fn_117b18_x(rec);
            break;
        case 4:
            while (guest_fn_9c430(EF_U32(rec + 0x1cu) + 5u) & 1u) {
                uint32_t b = guest_fn_9c430(EF_U32(rec + 0x1cu));
                uint32_t q = EF_U32(rec + 0xau);
                if ((int16_t)EF_U16(q + 4u) < (int16_t)EF_U16(q + 6u)) {
                    uint16_t idx = EF_U16(q + 2u);
                    EF_U16(q + 2u) = (uint16_t)(idx + 1u);
                    EF_U8(EF_U32(rec + 0xeu) + (uint32_t)(int32_t)(int16_t)idx) = (uint8_t)b;
                    if (EF_U16(q + 2u) == EF_U16(q + 6u))
                        EF_U16(q + 2u) = 0u;
                    EF_U16(q + 4u) = (uint16_t)(EF_U16(q + 4u) + 1u);
                }
            }
            break;
        case 6:
            (void)guest_fn_9c430(EF_U32(rec + 0x1cu) + 5u);
            break;
        default:
            break;
        }
    }
    guest_fn_117b0c(0x20u, 0x20u);
}

/* 0x57820, 315 bytes. Build the status message for the event record REC (bytes: +1 first slot, +2 second slot, signed, -1 = none; +3/+4/+5 select the message form) into the text buffer 0x1a5d20 from the 17-byte names at 0x1a62f0 (an empty name shows as "UNKNOWN"), store the matching colour byte (table 0x12584c) at 0x1a6667 and set the message timers (word 0x127dac = 300, word 0x1a662c = 200). Does nothing when a slot is out of range of the word at 0x1357b0. */
void format_event_message_57820(uint32_t rec)
{
    char unknown[8] = { 'U', 'N', 'K', 'N', 'O', 'W', 'N', 0 };
    uint32_t n1, n2, name1, name2;
    uint32_t colour;
    int32_t limit = (int16_t)EF_U16(0x001357b0u);
    EF_U8(0x001a5d20u) = 0u;
    if ((int32_t)EF_U8(rec + 1u) >= limit)
        return;
    if ((int8_t)EF_U8(rec + 2u) < 0)
        return;
    if ((int32_t)(int8_t)EF_U8(rec + 2u) >= limit)
        return;
    n1 = 0x001a62f0u + EF_U8(rec + 1u) * 17u;
    n2 = 0x001a62f0u + (uint32_t)((int32_t)(int8_t)EF_U8(rec + 2u) * 17);
    name1 = EF_U8(n1) != 0u ? n1 : (uint32_t)(uintptr_t)unknown;
    name2 = EF_U8(n2) != 0u ? n2 : (uint32_t)(uintptr_t)unknown;
    if (EF_U8(rec + 3u) != 0u) {
        guest_sprintf_a4342(0x001a5d20u, 0x00132727u, name2);
        colour = EF_U8(0x0012584cu + (uint32_t)(int32_t)(int8_t)EF_U8(rec + 2u));
    } else if (EF_U8(rec + 4u) != 0u) {
        guest_sprintf_a4342(0x001a5d20u, 0x00132748u, name1);
        colour = EF_U8(0x0012584cu + EF_U8(rec + 1u));
    } else {
        if (EF_U8(rec + 5u) != 0u) {
            uint32_t s = EF_U8(0x0012584cu + EF_U8(rec + 1u)) != 0u ? 0x00185c68u : 0x00185c70u;
            guest_sprintf_a4342(0x001a5d20u, 0x00132761u, name1, s);
        } else {
            guest_sprintf_a4342(0x001a5d20u, 0x0012fee7u, name2, name1);
        }
        colour = EF_U8(0x0012584cu + EF_U8(rec + 1u));
    }
    EF_U8(0x001a6667u) = (uint8_t)colour;
    EF_U16(0x00127dacu) = 0x12cu;
    EF_U16(0x001a662cu) = 0xc8u;
}

/* 0x319ac, 318 bytes. Update the camera position (dwords 0x19e5c1/5c5/5c9): rotate the offset (0, 0, -dword 0x19e597) by the angle pair at words 0x19e5a5/5a7 into the position, then, when byte 0x19e558 is 2, rotate the offset (0x19e58b/58f/593 + position) by the angles at words 0x19e56c/56e/570 and add the velocity dwords 0x19e560/564/568 to the result; otherwise just add the velocity to the position. */
void update_camera_position_319ac(void)
{
    uint16_t angles[4];
    int32_t vec[3];
    int32_t out[3];
    angles[0] = EF_U16(0x0019e5a5u);
    angles[1] = EF_U16(0x0019e5a7u);
    angles[2] = 0u;
    vec[0] = 0;
    vec[1] = 0;
    vec[2] = -(int32_t)EF_U32(0x0019e597u);
    rotate_vector_by_angles_32414((uint32_t)(uintptr_t)angles, (uint32_t)(uintptr_t)vec, 0x0019e5c1u);
    if (EF_U8(0x0019e558u) == 2u) {
        angles[0] = EF_U16(0x0019e56cu);
        angles[1] = EF_U16(0x0019e56eu);
        angles[2] = EF_U16(0x0019e570u);
        vec[0] = (int32_t)(EF_U32(0x0019e58bu) + EF_U32(0x0019e5c1u));
        vec[1] = (int32_t)(EF_U32(0x0019e58fu) + EF_U32(0x0019e5c5u));
        vec[2] = (int32_t)(EF_U32(0x0019e593u) + EF_U32(0x0019e5c9u));
        rotate_vector_by_angles_32414((uint32_t)(uintptr_t)angles, (uint32_t)(uintptr_t)vec, (uint32_t)(uintptr_t)out);
        EF_U32(0x0019e5c1u) = EF_U32(0x0019e560u) + (uint32_t)out[0];
        EF_U32(0x0019e5c5u) = EF_U32(0x0019e564u) + (uint32_t)out[1];
        EF_U32(0x0019e5c9u) = EF_U32(0x0019e568u) + (uint32_t)out[2];
    } else {
        EF_U32(0x0019e5c1u) += EF_U32(0x0019e560u);
        EF_U32(0x0019e5c5u) += EF_U32(0x0019e564u);
        EF_U32(0x0019e5c9u) += EF_U32(0x0019e568u);
    }
}

/* 0x76b80, 321 bytes. Bind unit ID (16-bit) to the first free 25-byte entry (word +1 == 0xff) of the table at dword 0x1f8918 (count in byte 0x1f8920): report an error through the callback at 0x132a74 when the table is full (string 0x186d44) or the unit does not exist (0x186d58), store the entry index in the unit (byte +0x187) and the ID in the entry, set the three words at +0x13 to 0xff, reset the unit's eight slots to 0xff and flag byte 0 of the entry when the unit's type byte (+0xe9) is 0x21. */
void bind_unit_entry_76b80(uint32_t id)
{
    uint32_t tbl = EF_U32(0x001f8918u);
    uint32_t n = EF_U8(0x001f8920u);
    uint32_t i, slot = 0xffu, unit;
    for (i = 0u; i < n; i++) {
        if ((int16_t)EF_U16(tbl + i * 25u + 1u) == 0xff && slot == 0xffu)
            slot = i;
    }
    if (slot == 0xffu)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00186d44u);
    unit = guest_fn_7489c((int16_t)id);
    if (unit == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00186d58u, (uint32_t)(int32_t)(int16_t)id);
    tbl = EF_U32(0x001f8918u);
    EF_U8(unit + 0x187u) = (uint8_t)slot;
    EF_U16(tbl + slot * 25u + 1u) = (uint16_t)id;
    for (i = 0u; i < 3u; i++)
        EF_U16(tbl + slot * 25u + 0x13u + i * 2u) = 0xffu;
    for (i = 0u; i < 8u; i++)
        guest_fn_76b30(unit, i, 0xffu);
    EF_U8(tbl + slot * 25u) = EF_U8(unit + 0xe9u) == 0x21u ? 1u : 0u;
}

/* 0xa94e4, 321 bytes. Advance the playlist LIST (flag array at +0xe, entry count word +8, played count word +0xa, sequential flag byte +0xd, pending-track word +0x12): when byte 0x1371ff is 0 and a track is pending, step to it (0xa9490) and clear the request; otherwise pick the next entry - the first unplayed one in sequential mode, else a random unplayed one (five random tries, then a linear scan) with the last remaining one chosen directly - mark it played, and once every entry has been played clear all the flags and the counter. */
void advance_playlist_a94e4(uint32_t list)
{
    uint32_t flags, i;
    int16_t idx;
    if (EF_U8(0x001371ffu) == 0u && EF_U16(list + 0x12u) != 0u) {
        step_list_cursor_a9490(list, (int16_t)(EF_U16(list + 0x12u) - 1u));
        EF_U16(list + 0x12u) = 0u;
        return;
    }
    if (EF_U8(list + 0xdu) != 0u) {
        i = 0u;
        while (EF_U8(EF_U32(list + 0xeu) + (uint32_t)(int32_t)(int16_t)i) != 0u)
            i++;
        idx = (int16_t)i;
        step_list_cursor_a9490(list, idx);
        EF_U8(EF_U32(list + 0xeu) + (uint32_t)(int32_t)idx) = 1u;
        EF_U16(list + 0xau) = (uint16_t)(EF_U16(list + 0xau) + 1u);
        if (EF_U16(list + 0xau) == EF_U16(list + 8u)) {
            if ((int16_t)EF_U16(list + 8u) > 0) {
                i = 0u;
                do {
                    EF_U8(EF_U32(list + 0xeu) + (uint32_t)(int32_t)(int16_t)i) = 0u;
                    i++;
                } while ((int16_t)i < (int16_t)EF_U16(list + 8u));
            }
            EF_U16(list + 0xau) = 0u;
        }
        return;
    }
    if ((int32_t)(int16_t)EF_U16(list + 0xau) == (int32_t)(int16_t)EF_U16(list + 8u) - 1) {
        i = 0u;
        while (EF_U8(EF_U32(list + 0xeu) + (uint32_t)(int32_t)(int16_t)i) != 0u)
            i++;
        step_list_cursor_a9490(list, (int16_t)i);
        if ((int16_t)EF_U16(list + 8u) > 0) {
            i = 0u;
            do {
                EF_U8(EF_U32(list + 0xeu) + (uint32_t)(int32_t)(int16_t)i) = 0u;
                i++;
            } while ((int16_t)i < (int16_t)EF_U16(list + 8u));
        }
        EF_U16(list + 0xau) = 0u;
        return;
    } else {
        uint32_t tries = 0u, r;
        flags = EF_U32(list + 0xeu);
        do {
            r = (uint32_t)random_scaled((int16_t)EF_U16(list + 8u));
            tries++;
            if (EF_U8(EF_U32(list + 0xeu) + (uint32_t)(int32_t)(int16_t)r) == 0u)
                break;
        } while ((int16_t)tries < 6);
        if ((int16_t)tries >= 6) {
            do {
                r++;
                if ((int16_t)r >= (int16_t)EF_U16(list + 8u))
                    r = 0u;
            } while (EF_U8(EF_U32(list + 0xeu) + (uint32_t)(int32_t)(int16_t)r) != 0u);
        }
        (void)flags;
        idx = (int16_t)r;
        step_list_cursor_a9490(list, idx);
        EF_U8(EF_U32(list + 0xeu) + (uint32_t)(int32_t)idx) = 1u;
        EF_U16(list + 0xau) = (uint16_t)(EF_U16(list + 0xau) + 1u);
    }
}

/* 0xe6848, 326 bytes. Reorder the payload (bytes +9..+0x33) of the list whose head pointer is *HEAD_PTR (link +4) so that the node types (byte +0xb) follow the word array ORDER[0..COUNT-1]: the leading nodes of type KEY are skipped, then for each wanted type the first later node of that type is swapped into the current position. ORIGINAL BUG: when the current position already holds the wanted type it is not advanced, so later matches are swapped into the same node. */
void sort_nodes_by_order_e6848(uint32_t order, uint32_t count, uint32_t head_ptr, uint32_t key)
{
    uint32_t cur = EF_U32(head_ptr);
    int16_t i;
    int16_t n = (int16_t)count;
    key &= 0xffu;
    if (EF_U8(cur + 0xbu) == key) {
        do {
            cur = EF_U32(cur + 4u);
        } while (EF_U8(cur + 0xbu) == key);
    }
    for (i = 0; n > 0; ) {
        uint32_t a = cur;
        while (a != 0u) {
            if (EF_U8(a + 0xbu) == EF_U16(order + (uint32_t)(int32_t)i * 2u) && a != cur) {
                uint32_t k;
                for (k = 9u; k <= 0x33u; k++) {
                    uint8_t t = EF_U8(cur + k);
                    EF_U8(cur + k) = EF_U8(a + k);
                    EF_U8(a + k) = t;
                }
                cur = EF_U32(cur + 4u);
            }
            a = EF_U32(a + 4u);
        }
        i++;
        if (!(i < n))
            break;
    }
}

/* 0xcde00, 327 bytes. Draw sprite number IDX from the sprite table TABLE (byte count, then word offsets from the table) centred at (X, Y): the sprite header holds half-width byte and height byte; reject it when entirely outside the clip rectangle (dwords 0x17cf9c/a0/a4/a8 = xmin/xmax/ymin/ymax), compute the destination (row table 0x163200 or negative rows by stride 0x1631f4 on frame base 0x15b754), and either blit it whole (0xcdc90, sprite width/4 and height in 0x4201a0/a4) when it is fully inside, or call the clipping blitter 0xcdd18 with the clip bounds relative to the sprite. Stack-only (ret 0x10): Y, X, IDX, TABLE. */
void draw_table_sprite_cde00(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t y, uint32_t x, uint32_t idx, uint32_t table)
{
    uint32_t src, dst, w, h;
    int32_t sx = (int32_t)x, sy = (int32_t)y;
    uint16_t rx0, rx1, ry0, ry1;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (idx >= EF_U8(table))
        return;
    src = table + EF_U16(table + idx * 2u + 1u);
    w = EF_U8(src) * 2u;
    h = EF_U8(src + 1u);
    sx -= (int32_t)(w >> 1);
    sy -= (int32_t)(h >> 1);
    src += 2u;
    if (sx + (int32_t)w < (int32_t)EF_U32(0x0017cf9cu))
        return;
    if (sy + (int32_t)h < (int32_t)EF_U32(0x0017cfa4u))
        return;
    if (sx > (int32_t)EF_U32(0x0017cfa0u))
        return;
    if (sy > (int32_t)EF_U32(0x0017cfa8u))
        return;
    rx0 = (uint16_t)(EF_U16(0x0017cf9cu) - (uint32_t)sx);
    rx1 = (uint16_t)(EF_U16(0x0017cfa0u) - (uint32_t)sx);
    ry0 = (uint16_t)(EF_U16(0x0017cfa4u) - (uint32_t)sy);
    ry1 = (uint16_t)(EF_U16(0x0017cfa8u) - (uint32_t)sy);
    if (sy >= 0)
        dst = EF_U32(0x00163200u + (uint32_t)sy * 4u);
    else
        dst = (uint32_t)sy * EF_U32(0x001631f4u);
    dst += EF_U32(0x0015b754u) + (uint32_t)sx;
    EF_U32(0x004201a0u) = (uint32_t)((int32_t)w >> 2);
    EF_U32(0x004201a4u) = h;
    if ((int16_t)rx0 < 0 && (int16_t)rx1 > (int32_t)w && (int16_t)ry0 < 0 && (int16_t)ry1 > (int32_t)h) {
        interleave_planes_cdc90(dst, src);
    } else {
        draw_planar_sprite_cdd18(dst, src, (uint32_t)(int32_t)(int16_t)rx0, (uint32_t)(int32_t)(int16_t)rx1,
                                 (uint32_t)(int32_t)(int16_t)ry0, (uint32_t)(int32_t)(int16_t)ry1);
    }
}

/* 0x6abac, 331 bytes. Pick a free object for the unit ID (16-bit) from candidate list K (word index into the 69-byte table at dword 0x1a14f8, ten object IDs at +0x28): the first object that exists, has flags 1 and 2 clear in byte +0x30, no owner (dword +0x1c) is stored in the unit's word +0x16a (the object's word +0x16); in the first pass the choice is undone (word 0xff) when one of the seven peers listed in the 202-byte table at dword 0x19bb58 (indexed by the unit's word +0x28, IDs at +0x50) already uses the same value, and the scan continues; a second pass without the peer check follows when nothing was chosen. Returns 1 when an object was assigned, 0 otherwise (also when the unit does not exist). */
uint32_t assign_unit_object_6abac(uint32_t id, uint32_t k)
{
    uint32_t unit = guest_fn_7489c((int16_t)id);
    int16_t i, j;
    uint16_t found = 0u;
    if (unit == 0u)
        return 0u;
    for (i = 0; i < 10; i++) {
        uint32_t tbl = EF_U32(0x001a14f8u) + (uint32_t)((int32_t)(int16_t)k * 0x45);
        uint32_t p = guest_lookup_object_8ffe8((int16_t)EF_U16(tbl + 0x28u + (uint32_t)i * 2u));
        if (p == 0u)
            continue;
        if ((EF_U8(p + 0x30u) & 1u) || (EF_U8(p + 0x30u) & 2u))
            continue;
        if (EF_U32(p + 0x1cu) != 0u || found != 0u)
            continue;
        found = 1u;
        EF_U16(unit + 0x16au) = EF_U16(p + 0x16u);
        for (j = 0; j < 7; j++) {
            uint32_t t2 = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)(int16_t)EF_U16(unit + 0x28u) * 0xca);
            uint32_t peer = guest_fn_7489c((int16_t)EF_U16(t2 + 0x50u + (uint32_t)j * 2u));
            if (peer != 0u && peer != unit && EF_U16(peer + 0x16au) == EF_U16(p + 0x16u)) {
                EF_U16(unit + 0x16au) = 0xffu;
                found = 0u;
            }
        }
    }
    if (found == 0u) {
        for (i = 0; i < 10; i++) {
            uint32_t tbl = EF_U32(0x001a14f8u) + (uint32_t)((int32_t)(int16_t)k * 0x45);
            uint32_t p = guest_lookup_object_8ffe8((int16_t)EF_U16(tbl + 0x28u + (uint32_t)i * 2u));
            if (p == 0u)
                continue;
            if ((EF_U8(p + 0x30u) & 1u) || (EF_U8(p + 0x30u) & 2u))
                continue;
            if (EF_U32(p + 0x1cu) != 0u || found != 0u)
                continue;
            EF_U16(unit + 0x16au) = EF_U16(p + 0x16u);
            found = 1u;
        }
    }
    return found;
}

/* 0xe1570, 337 bytes. Install the draw routine table (dwords 0x136ba0..0x136bc4, code addresses) for the renderer mode in byte 0x13579a: 0, 1 and 2 select three sets of routines (modes 1 and 2 also fill 0x136bb0, mode 2 additionally 0x136bb4/b8), any other mode only sets 0x136ba0 to the default 0xe1568. */
void install_render_table_e1570(void)
{
    switch (EF_U8(0x0013579au)) {
    case 0:
        EF_U32(0x00136ba8u) = 0x000ecb84u;
        EF_U32(0x00136bacu) = 0x000ecb80u;
        EF_U32(0x00136ba4u) = 0x000ed108u;
        EF_U32(0x00136bbcu) = 0x000ecdf8u;
        EF_U32(0x00136bc0u) = 0x000ecb8cu;
        EF_U32(0x00136bc4u) = 0x000e156cu;
        EF_U32(0x00136ba0u) = 0x000ed090u;
        break;
    case 1:
        EF_U32(0x00136ba8u) = 0x000e0c40u;
        EF_U32(0x00136bacu) = 0x000e0c94u;
        EF_U32(0x00136bb0u) = 0x000e105cu;
        EF_U32(0x00136ba4u) = 0x000e0a90u;
        EF_U32(0x00136bbcu) = 0x000e0e84u;
        EF_U32(0x00136bc0u) = 0x000e0cc0u;
        EF_U32(0x00136bc4u) = 0x000e156cu;
        EF_U32(0x00136ba0u) = 0x000e0984u;
        break;
    case 2:
        EF_U32(0x00136ba8u) = 0x000e0c40u;
        EF_U32(0x00136bacu) = 0x000e0c94u;
        EF_U32(0x00136bb0u) = 0x000e105cu;
        EF_U32(0x00136ba4u) = 0x000e0a64u;
        EF_U32(0x00136bb4u) = 0x000e12d4u;
        EF_U32(0x00136bb8u) = 0x000e0a64u;
        EF_U32(0x00136bbcu) = 0x000e0e84u;
        EF_U32(0x00136bc0u) = 0x000e0cc0u;
        EF_U32(0x00136bc4u) = 0x000e156cu;
        EF_U32(0x00136ba0u) = 0x000e11fcu;
        break;
    default:
        EF_U32(0x00136ba0u) = 0x000e1568u;
        break;
    }
}

/* 0xf4a88, 337 bytes. Start the script section at the top level: push a fresh node (0xf57d4), set its default view words (+0x4f/+0x51 = 0x14, +0x5b/5d/67/69 = 0 and the screen-centre pairs from dwords 0x15b744/48 halved at +0x4b..+0x65), make it current (dword 0x485de0), run the script reader until it is done, then complain through the callback at 0x132a74 about each missing part (flag bytes 0x485f77, 74, 75, 76, 78 with strings 0x191e3c, 0x191e70, 0x191ea4, 0x191ed8, 0x191f0c). */
void run_top_script_f4a88(void)
{
    uint32_t node = push_node_f57d4();
    int32_t cx, cy;
    EF_U16(node + 0x4fu) = 0x14u;
    EF_U16(node + 0x51u) = 0x14u;
    EF_U16(node + 0x5bu) = 0u;
    EF_U16(node + 0x5du) = 0u;
    EF_U16(node + 0x67u) = 0u;
    EF_U16(node + 0x69u) = 0u;
    cx = (int32_t)EF_U32(0x0015b744u) >> 1;
    cy = (int32_t)EF_U32(0x0015b748u) >> 1;
    EF_U16(node + 0x4bu) = (uint16_t)cx;
    EF_U16(node + 0x4du) = (uint16_t)cy;
    EF_U16(node + 0x53u) = (uint16_t)cx;
    EF_U16(node + 0x55u) = (uint16_t)cy;
    EF_U16(node + 0x57u) = (uint16_t)cx;
    EF_U16(node + 0x59u) = (uint16_t)cy;
    EF_U16(node + 0x5fu) = (uint16_t)cx;
    EF_U16(node + 0x61u) = (uint16_t)cy;
    EF_U16(node + 0x63u) = (uint16_t)cx;
    EF_U32(0x00485de0u) = node;
    EF_U16(node + 0x65u) = (uint16_t)cy;
    EF_U16(0x00485dfcu) = 0u;
    EF_U8(0x00485f77u) = 0u;
    EF_U8(0x00485f74u) = 0u;
    EF_U8(0x00485f75u) = 0u;
    EF_U8(0x00485f76u) = 0u;
    EF_U8(0x00485f78u) = 0u;
    guest_fn_f46cc();
    while (EF_U8(0x00485e08u) != 0u) {
        run_matching_handler_f478c();
        guest_fn_f46cc();
    }
    if (EF_U8(0x00485f77u) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00191e3cu, EF_U32(0x00161ea8u));
    if (EF_U8(0x00485f74u) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00191e70u, EF_U32(0x00161ea8u));
    if (EF_U8(0x00485f75u) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00191ea4u, EF_U32(0x00161ea8u));
    if (EF_U8(0x00485f76u) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00191ed8u, EF_U32(0x00161ea8u));
    if (EF_U8(0x00485f78u) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00191f0cu, EF_U32(0x00161ea8u));
}

/* 0x777f4, 339 bytes. Test the position POS (three dwords) against the entries of the pointer table at dword 0x1f8900 (count word 0x1f891c, scanned from the end, key word 0x1f891e must not be 0xffff): for each entry without flag 8 (byte +0x24) and within 0x40000 units on all three axes, call 0xa0ea8; then ask 0xd0470 (coordinates >> 8) whether the entry is hit and, if so, trigger the two impact effects (0x106b0 at the entry and at POS, each followed by 0x748f4 on the object ID word +0x16 with kind 0x64) and stop. Returns 1 when something was hit, else 0. */
uint32_t check_position_hit_777f4(uint32_t pos)
{
    uint16_t n = EF_U16(0x001f891cu);
    uint8_t found = 0u;
    uint32_t e;
    if (EF_U16(0x001f891eu) == 0xffffu)
        return 0u;
    while (n != 0u && found == 0u) {
        n--;
        e = EF_U32(EF_U32(0x001f8900u) + (uint32_t)(int32_t)(int16_t)n * 4u);
        if (!(EF_U8(e + 0x24u) & 8u)) {
            if ((int32_t)abs_a28f2((int32_t)(EF_U32(e) - EF_U32(pos))) < 0x40000 &&
                (int32_t)abs_a28f2((int32_t)(EF_U32(e + 4u) - EF_U32(pos + 4u))) < 0x40000 &&
                (int32_t)abs_a28f2((int32_t)(EF_U32(e + 8u) - EF_U32(pos + 8u))) < 0x40000) {
                guest_fn_a0ea8((int16_t)EF_U16(0x001f891eu), (uint32_t)(int32_t)(int16_t)EF_U16(e + 0xcu),
                               (int16_t)EF_U16(e + 0xeu), (uint32_t)(int32_t)(int16_t)EF_U16(e + 0x10u));
            }
        }
        if (guest_fn_d0470((int16_t)EF_U16(0x001f891eu), (int32_t)EF_U32(e) >> 8, (int32_t)EF_U32(e + 4u) >> 8,
                           (int32_t)EF_U32(e + 8u) >> 8) != 0u) {
            found = 1u;
            guest_fn_106b0_x(0xffffffffu, (uint32_t)((int32_t)EF_U32(e) >> 8), (uint32_t)((int32_t)EF_U32(e + 4u) >> 8),
                             (uint32_t)((int32_t)EF_U32(e + 8u) >> 8), 0u);
            send_or_defer_748f4((int16_t)EF_U16(e + 0x16u), 0u, 0u, 0x64u);
            guest_fn_106b0_x(0xffffffffu, (uint32_t)((int32_t)EF_U32(pos) >> 8), (uint32_t)((int32_t)EF_U32(pos + 4u) >> 8),
                             (uint32_t)((int32_t)EF_U32(pos + 8u) >> 8), 0u);
            send_or_defer_748f4((int16_t)EF_U16(pos + 0x16u), 0u, 0u, 0x64u);
        }
    }
    return found;
}

/* 0x84788, 342 bytes. Latch the control bits of the flight record REC into the global control word 0x1fcbe2: clear bits 0 and 7 of its low byte, clear word 0x1fcbe0, then copy REC dword +0x22 bit 1 to bit 0, bits 3:2 to bits 2:1, bit 7 to bit 6 and bits 6:4 to bits 5:3 (clearing bits 6:4 in the record); the field in bits 10:9 steps the byte 0x1fcc42 by +2 (value 1) or -2 (value 2), clamped to 6..10, and bits 9 and 10 are cleared in the record. Finally copy the words +0x26/+0x2a to 0x1fcbd0/0x1fcbd2, zero both dwords and latch dword 0x197c14 into 0x1fcbbc. */
void latch_control_bits_84788(uint32_t rec)
{
    uint32_t d, g, v;
    EF_U8(0x001fcbe2u) &= 0x7eu;
    EF_U16(0x001fcbe0u) = 0u;
    d = EF_U32(rec + 0x22u);
    g = EF_U16(0x001fcbe2u);
    g |= (d >> 1) & 1u;
    EF_U16(0x001fcbe2u) = (uint16_t)g;
    d = EF_U32(rec + 0x22u);
    g = EF_U16(0x001fcbe2u) & ~6u;
    g |= ((d >> 2) & 3u) * 2u;
    EF_U16(0x001fcbe2u) = (uint16_t)g;
    d = EF_U32(rec + 0x22u);
    g = EF_U16(0x001fcbe2u) & ~0x40u;
    g |= ((d >> 7) & 1u) << 6;
    EF_U16(0x001fcbe2u) = (uint16_t)g;
    d = EF_U32(rec + 0x22u);
    g = EF_U16(0x001fcbe2u) & ~0x38u;
    g |= ((d >> 4) & 7u) << 3;
    EF_U8(rec + 0x22u) &= 0x8fu;
    EF_U16(0x001fcbe2u) = (uint16_t)g;
    v = (EF_U32(rec + 0x22u) >> 9) & 3u;
    if (v == 1u)
        EF_U8(0x001fcc42u) = (uint8_t)(EF_U8(0x001fcc42u) + 2u);
    else if (v == 2u)
        EF_U8(0x001fcc42u) = (uint8_t)(EF_U8(0x001fcc42u) - 2u);
    EF_U8(rec + 0x23u) &= 0xf9u;
    if (EF_U8(0x001fcc42u) < 6u)
        EF_U8(0x001fcc42u) = 6u;
    if (EF_U8(0x001fcc42u) > 10u)
        EF_U8(0x001fcc42u) = 10u;
    EF_U16(0x001fcbd0u) = EF_U16(rec + 0x26u);
    EF_U16(0x001fcbd2u) = EF_U16(rec + 0x2au);
    EF_U32(rec + 0x2au) = 0u;
    EF_U32(rec + 0x26u) = 0u;
    EF_U32(0x001fcbbcu) = EF_U32(0x00197c14u);
}

/* 0x436dc, 342 bytes. Draw the map markers for the objects of the list at *LIST (nodes of type 0x0e and 0x17, link +4; group flag tables checked against HEAD): when the node's index byte (+0xb) is flagged positive (cc22c, enabled by word 0x126554), negative (cc1ac, 0x126556) or zero (cc1ec, 0x126558) the target address is the base dword 0x126570 plus the matching position word pair (0x134325/27, 0x13431c/1e or 0x13432e/30; second word times 0x280); a 20x20 marker is then drawn at the object's screen position (words +8/+0xc of the structure at +0x13, y inverted from 0x190) with 0xca620 and, when word 0x12656a is set, a label with 0x440f4. */
void draw_map_markers_436dc(uint32_t head, uint32_t list)
{
    uint32_t node = EF_U32(list);
    while (node != 0u) {
        uint32_t type = EF_U8(node + 0xau);
        if (type == 0xeu || type == 0x17u) {
            uint32_t idx = EF_U8(node + 0xbu);
            uint32_t pos = 0u;
            uint8_t have = 0u;
            if ((guest_fn_cc22c(0u, idx, head) & 0xffffu) != 0u && EF_U16(0x00126554u) != 0u) {
                pos = EF_U32(0x00126570u) + (uint32_t)(int32_t)(int16_t)EF_U16(0x00134325u) +
                      (uint32_t)((int32_t)(int16_t)EF_U16(0x00134327u) * 0x280);
                have = 1u;
            } else if ((guest_fn_cc1ac(0u, idx, head) & 0xffffu) != 0u && EF_U16(0x00126556u) != 0u) {
                pos = EF_U32(0x00126570u) + (uint32_t)(int32_t)(int16_t)EF_U16(0x0013431cu) +
                      (uint32_t)((int32_t)(int16_t)EF_U16(0x0013431eu) * 0x280);
                have = 1u;
            } else if ((group_flag_zero_cc1ec(0u, idx, head) & 0xffffu) != 0u && EF_U16(0x00126558u) != 0u) {
                pos = EF_U32(0x00126570u) + (uint32_t)(int32_t)(int16_t)EF_U16(0x0013432eu) +
                      (uint32_t)((int32_t)(int16_t)EF_U16(0x00134330u) * 0x280);
                have = 1u;
            }
            if (have) {
                uint32_t obj = EF_U32(node + 0x13u);
                int32_t x = (int16_t)(EF_U16(obj + 8u) - 10u);
                int32_t y = (int16_t)(0x190u - EF_U16(obj + 0xcu) - 10u);
                guest_fn_ca620((int32_t)pos, x, y, 0x14u, 0x14u, 0x280u);
                if (EF_U16(0x0012656au) != 0u)
                    guest_fn_440f4_x(idx, x, y, 0x14u, 0x1au, 0x280u);
            }
        }
        node = EF_U32(node + 4u);
    }
}

/* 0xd7158, 348 bytes. Start sample channel CHAN (< 0xe, else report through the callback at 0x132a74 with string 0x18f998) playing the sound record REC (data pointer +0x1c, length +0x18, volume word +0xf, flag byte +0xd; nothing unless the data exists, byte 0x15bac4 is set and REC flag +0xd is set; a zero length falls back to the record at +0x1c): touch every 4 KiB page of the data, lock the range, then program the channel handle (table 0x420dd0): reset, set data/length, parameter 0x5622, volume (VOL, or the record's byte +0x11), word +0xf, the second argument PAN, the completion callback 0xd710c, count the playing sound (dword 0x15bad0) and start. */
void start_sample_d7158(uint32_t chan, uint32_t pan, uint32_t rec, uint32_t vol)
{
    uint32_t r = rec;
    uint32_t len, i, h, v;
    uint8_t c = (uint8_t)chan;
    if (EF_U32(r + 0x1cu) == 0u || EF_U8(0x0015bac4u) == 0u || EF_U8(r + 0xdu) == 0u)
        return;
    if (c >= 0xeu)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018f998u, c);
    if (EF_U32(r + 0x18u) == 0u)
        r = EF_U32(r + 0x1cu);
    len = EF_U32(r + 0x18u);
    for (i = 0u; (int32_t)i < (int32_t)len; i += 0x1000u)
        (void)EF_U8(EF_U32(r + 0x1cu) + i);
    stack_range_104908(EF_U32(r + 0x1cu), EF_U32(r + 0x18u) - 1u);
    h = EF_U32(0x00420dd0u + c * 4u);
    trace_call_102738(0u, 0u, 0u, 0u, h, 0u, 0u);
    trace_call_1026fc(0u, 0u, 0u, 0u, h, EF_U32(r + 0x1cu), EF_U32(r + 0x18u));
    trace_call_102824(h, 0x5622u);
    v = vol != 0u ? vol : (uint32_t)((int32_t)EF_U32(r + 0xfu) >> 16);
    trace_call_102858(h, v & 0xffu);
    trace_call_102824(h, EF_U16(r + 0xfu));
    trace_call_1028c0(h, pan);
    trace_call_102dbc(0u, 0u, 0u, 0u, h, 0x000d710cu);
    EF_U32(0x0015bad0u) = EF_U32(0x0015bad0u) + 1u;
    trace_call_102774(h);
}

/* 0x1df90, 348 bytes. Ground impact test for the projectile REC (owner byte +0x27 must match byte 0x1a666b and the done flag +0x47 must be clear): convert its position pairs (+0x1b/1f/23 and +9/0d/11, >> 8) into the scratch dwords 0x420904..0x42091c and the word copies 0x420930..0x42093a, look the cell up with 0x7ae88; on a hit mark the record done, store the current word (0xcd24c) in 0x1a6634, trigger the crater effect (0x106b0, kind 0x26) and the sound 0x110ec; then look the position up again with 0x7af60 and, if solid, optionally relocate the record to the point stored at 0x1f8ea8/ac/b0 (<< 8) when 0x7b7d8 agrees. When the record is done, clear byte +0x48 and dispatch the event (3 = hit, 2 = relocated) through 0x1e0ec. */
void projectile_ground_test_1df90(uint32_t rec)
{
    uint32_t event = 0u;
    if (EF_U8(rec + 0x27u) != EF_U8(0x001a666bu) || EF_U8(rec + 0x47u) != 0u)
        return;
    EF_U32(0x00420910u) = (uint32_t)((int32_t)EF_U32(rec + 0x1bu) >> 8);
    EF_U32(0x00420904u) = (uint32_t)((int32_t)EF_U32(rec + 0x1fu) >> 8);
    EF_U32(0x00420918u) = (uint32_t)((int32_t)EF_U32(rec + 0x23u) >> 8);
    EF_U32(0x00420914u) = (uint32_t)((int32_t)EF_U32(rec + 9u) >> 8);
    EF_U32(0x00420908u) = (uint32_t)((int32_t)EF_U32(rec + 0xdu) >> 8);
    EF_U32(0x0042091cu) = (uint32_t)((int32_t)EF_U32(rec + 0x11u) >> 8);
    EF_U16(0x00420930u) = (uint16_t)EF_U32(0x00420910u);
    EF_U16(0x00420932u) = (uint16_t)EF_U32(0x00420904u);
    EF_U16(0x00420936u) = (uint16_t)EF_U32(0x00420914u);
    EF_U16(0x00420934u) = (uint16_t)EF_U32(0x00420918u);
    EF_U16(0x00420938u) = (uint16_t)EF_U32(0x00420908u);
    EF_U16(0x0042093au) = (uint16_t)EF_U32(0x0042091cu);
    if (lookup_plane_cell_7ae88(0u, EF_U32(0x00420914u), 0u, EF_U32(0x0042091cu)) != 0u) {
        EF_U8(rec + 0x47u) = 1u;
        EF_U16(0x001a6634u) = cursor_word_at_2();
        guest_fn_106b0_x(0xffffffffu, (uint32_t)((int32_t)EF_U32(rec + 9u) >> 8), 0u,
                         (uint32_t)((int32_t)EF_U32(rec + 0x11u) >> 8), 0x26u);
        guest_fn_110ec_x(1u, 0x64u);
        event = 3u;
    }
    if (lookup_plane_cell_7af60(0u, (uint32_t)((int32_t)EF_U32(rec + 9u) >> 8), 0u,
                                (uint32_t)((int32_t)EF_U32(rec + 0x11u) >> 8)) != 0u) {
        if (guest_fn_7b7d8() != 0u) {
            EF_U32(rec + 9u) = EF_U32(0x001f8eb0u) << 8;
            EF_U32(rec + 0xdu) = EF_U32(0x001f8eacu) << 8;
            EF_U32(rec + 0x11u) = EF_U32(0x001f8ea8u) << 8;
        }
        event = 2u;
        EF_U8(rec + 0x47u) = 1u;
    }
    if (EF_U8(rec + 0x47u) != 0u) {
        EF_U8(rec + 0x48u) = 0u;
        dispatch_record_event_1e0ec(rec, (int16_t)event);
    }
}

/* 0x4357c, 352 bytes. Variant of 0x436dc for the nodes of type 0x14: the position words come from 0x1344f9/fb (positive flag), 0x1344f0/f2 (negative) and 0x134502/04 (zero), and the marker is placed from the words +0 and +4 of the structure reached through +0x13 and +0x20 of the node. */
void draw_map_markers_4357c(uint32_t head, uint32_t list)
{
    uint32_t node = EF_U32(list);
    while (node != 0u) {
        if (EF_U8(node + 0xau) == 0x14u) {
            uint32_t idx = EF_U8(node + 0xbu);
            uint32_t pos = 0u;
            uint8_t have = 0u;
            if ((guest_fn_cc22c(0u, idx, head) & 0xffffu) != 0u && EF_U16(0x00126554u) != 0u) {
                pos = EF_U32(0x00126570u) + (uint32_t)(int32_t)(int16_t)EF_U16(0x001344f9u) +
                      (uint32_t)((int32_t)(int16_t)EF_U16(0x001344fbu) * 0x280);
                have = 1u;
            } else if ((guest_fn_cc1ac(0u, idx, head) & 0xffffu) != 0u && EF_U16(0x00126556u) != 0u) {
                pos = EF_U32(0x00126570u) + (uint32_t)(int32_t)(int16_t)EF_U16(0x001344f0u) +
                      (uint32_t)((int32_t)(int16_t)EF_U16(0x001344f2u) * 0x280);
                have = 1u;
            } else if ((group_flag_zero_cc1ec(0u, idx, head) & 0xffffu) != 0u && EF_U16(0x00126558u) != 0u) {
                pos = EF_U32(0x00126570u) + (uint32_t)(int32_t)(int16_t)EF_U16(0x00134502u) +
                      (uint32_t)((int32_t)(int16_t)EF_U16(0x00134504u) * 0x280);
                have = 1u;
            }
            if (have) {
                uint32_t obj = EF_U32(EF_U32(node + 0x13u) + 0x20u);
                int32_t x = (int16_t)(EF_U16(obj) - 10u);
                int32_t y = (int16_t)(0x190u - EF_U16(obj + 4u) - 10u);
                guest_fn_ca620((int32_t)pos, x, y, 0x14u, 0x14u, 0x280u);
                if (EF_U16(0x0012656au) != 0u)
                    guest_fn_440f4_x(idx, x, y, 0x14u, 0x1au, 0x280u);
            }
        }
        node = EF_U32(node + 4u);
    }
}

/* 0xbb324, 353 bytes. Clear the 0x14-byte record REC and initialise it for event KIND (0..0x24) by calling the matching initialiser (EAX = REC, EDX = ARG byte); kinds without an initialiser (0x18..0x1b, 0x1d, 0x1e, 0x20..0x24) leave it cleared. */
void init_event_record_bb324(uint32_t rec, uint32_t kind, uint32_t arg)
{
    static const uint32_t handlers[0x25] = {
        0xba13cu, 0xba77cu, 0xba6a0u, 0xba580u, 0xba460u, 0xba40cu, 0xbad70u, 0xba804u,
        0xba8dcu, 0xba9c4u, 0xbaa4cu, 0xbaad4u, 0xbab5cu, 0xbabd0u, 0xbac90u, 0xba2c0u,
        0xbae34u, 0xbae4cu, 0xbae78u, 0xbaf24u, 0xbaf48u, 0xbafe4u, 0xbb1acu, 0xbb0acu,
        0u, 0u, 0u, 0u, 0xba3bcu, 0u, 0u, 0xba3e4u, 0u, 0u, 0u, 0u, 0u
    };
    uint32_t fn = 0u;
    fill_bytes_b3fb0(rec, 0u, 0x14u);
    if ((kind & 0xffu) <= 0x24u)
        fn = handlers[kind & 0xffu];
    if (fn != 0u)
        guest_callptr_eaxedx_x(fn, rec, arg & 0xffu);
}

/* 0xfe700, 14 bytes. BIOS get video mode (int 10h, AH = 0xf): returns the current mode number in AL (EBX, ESI and EDI are preserved). */
uint32_t get_video_mode_fe700(void)
{
    uint32_t in[7];
    uint32_t out[7];
    uint8_t sregs[12];
    segread_9be0d((uint32_t)(uintptr_t)sregs);
    in[0] = 0x0f00u;
    in[1] = 0u;
    in[2] = 0u;
    in[3] = 0u;
    in[4] = 0u;
    in[5] = 0u;
    in[6] = 0u;
    guest_dos_int_x(0x10u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
    return out[0] & 0xffu;
}

/* 0xa45e4, 16 bytes. Store the stack argument VALUE into the code-patch dword at 0xa45a4 (stdcall-style frame, ret 4). */
void set_patch_dword_a45e4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t value)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    EF_U32(0x000a45a4u) = value;
}

/* 0x1111ca, 23 bytes. Set the dword at 0x17d7c0 to -1 (frame with saved registers, no arguments). */
void reset_marker_1111ca(void)
{
    EF_U32(0x0017d7c0u) = 0xffffffffu;
}

/* 0x1111b1, 25 bytes. Compare the dword at 0x17d7c0 with -1 and do nothing either way (an empty check left over in the runtime). */
void check_marker_1111b1(void)
{
    (void)EF_U32(0x0017d7c0u);
}

/* 0xeb2a8, 24 bytes. DOS allocate memory with the impossible request BX = 0xffff (int 21h, AH = 0x48), which fails and returns the size of the largest free block in BX: returns that paragraph count. */
uint32_t largest_free_block_eb2a8(void)
{
    uint32_t in[7];
    uint32_t out[7];
    uint8_t sregs[12];
    segread_9be0d((uint32_t)(uintptr_t)sregs);
    in[0] = 0x4800u;
    in[1] = 0xffffu;
    in[2] = 0u;
    in[3] = 0u;
    in[4] = 0u;
    in[5] = 0u;
    in[6] = 0u;
    guest_dos_int_x(0x21u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
    return out[1] & 0xffffu;
}

/* 0x111142, 26 bytes. DOS4G set real-mode interrupt vector (int 21h, AX = 0x2505) for interrupt number CL = NUM (low byte) with EBX = VALUE; cdecl. */
void set_rm_vector_111142(uint32_t num, uint32_t value)
{
    uint32_t in[7];
    uint32_t out[7];
    uint8_t sregs[12];
    segread_9be0d((uint32_t)(uintptr_t)sregs);
    in[0] = 0x2505u;
    in[1] = value;
    in[2] = num & 0xffu;
    in[3] = 0u;
    in[4] = 0u;
    in[5] = 0u;
    in[6] = 0u;
    guest_dos_int_x(0x21u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
}

/* 0xdff4c, 24 bytes. Poll the serial line status register (port dword 0x15eed0 + 5): when its data-ready bit is set call the receive handler 0xdfac8. */
void poll_serial_dff4c(void)
{
    if (guest_fn_9c430(EF_U32(0x0015eed0u) + 5u) & 1u)
        guest_fn_dfac8_x();
}

/* 0xcd3c0, 83 bytes. Send the byte VALUE to the keyboard controller (port 0x60) and wait for the 0xfa acknowledge, retrying up to three times: wait for the input buffer to empty (port 0x64 bit 1 clear), write, wait for output data (bit 0 set) and read the answer. */
void send_keyboard_byte_cd3c0(uint32_t value)
{
    uint32_t tries;
    for (tries = 0u; tries < 3u; tries++) {
        while (guest_fn_9c430(0x64u) & 2u)
            ;
        guest_fn_117b0c(0x60u, value & 0xffu);
        while (!(guest_fn_9c430(0x64u) & 1u))
            ;
        if ((guest_fn_9c430(0x60u) & 0xffu) == 0xfau)
            break;
    }
}

/* 0xcd414, 27 bytes. Set the keyboard LEDs: command 0xed followed by the LED mask byte, with interrupts disabled. */
void set_keyboard_leds_cd414(uint32_t mask)
{
    send_keyboard_byte_cd3c0(0xedu);
    send_keyboard_byte_cd3c0(mask & 0xffu);
}

/* 0x11d63f, 29 bytes. Return the pending pushed-back character in dword 0x132a50 (clearing it) or, when none is pending, read one character from the console (int 21h, AH = 1). */
uint32_t read_console_char_11d63f(void)
{
    uint32_t c = EF_U32(0x00132a50u);
    EF_U32(0x00132a50u) = 0u;
    if (c == 0u) {
        uint32_t in[7];
        uint32_t out[7];
        uint8_t sregs[12];
        segread_9be0d((uint32_t)(uintptr_t)sregs);
        in[0] = 0x0100u;
        in[1] = 0u;
        in[2] = 0u;
        in[3] = 0u;
        in[4] = 0u;
        in[5] = 0u;
        in[6] = 0u;
        guest_dos_int_x(0x21u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
        c = out[0] & 0xffu;
    }
    return c;
}

/* Run int 21h/10h with the given EAX, EBX, ECX, EDX (ESI/EDI zero); the result block is returned in OUT (EAX, EBX, ECX, EDX, ESI, EDI, carry). */
static void run_dos_int_ex(uint32_t intno, uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t *out)
{
    uint32_t in[7];
    uint8_t sregs[12];
    segread_9be0d((uint32_t)(uintptr_t)sregs);
    in[0] = eax;
    in[1] = ebx;
    in[2] = ecx;
    in[3] = edx;
    in[4] = 0u;
    in[5] = 0u;
    in[6] = 0u;
    guest_dos_int_x(intno, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
}

/* 0x1101be, 30 bytes. DOS IOCTL get device information (int 21h, AX = 0x4400) for HANDLE: true when bit 7 of the device word (a character device) is set. */
uint32_t is_device_handle_1101be(uint32_t handle)
{
    uint32_t out[7];
    run_dos_int_ex(0x21u, 0x4400u, handle, 0u, 0u, out);
    return (out[3] & 0x80u) != 0u ? 1u : 0u;
}

/* 0xfe541, 31 bytes. DOS delete file (int 21h, AH = 0x41) NAME: 0 on success, otherwise the translated DOS error (0x1101e8) of the failing code. */
uint32_t delete_file_fe541(uint32_t name)
{
    uint32_t out[7];
    run_dos_int_ex(0x21u, 0x4100u, 0u, 0u, name, out);
    if (out[6] != 0u)
        return (uint32_t)translate_key_1101e8(out[0] & 0xffffu);
    return 0u;
}

/* 0x10f0e8, 31 bytes. DOS read (int 21h, AH = 0x3f): read COUNT bytes from HANDLE into BUFFER. Returns the byte count, or the translated DOS error (0x1101e8) when the carry flag is set. */
uint32_t dos_read_10f0e8(uint32_t handle, uint32_t buffer, uint32_t count)
{
    uint32_t out[7];
    run_dos_int_ex(0x21u, 0x3f00u, handle, count, buffer, out);
    if (out[6] != 0u)
        return (uint32_t)translate_key_1101e8(out[0] & 0xffffu);
    return out[0];
}

/* 0xfe710, 31 bytes. Set the BIOS video mode MODE (int 10h) and read it back (AH = 0xf): returns 1 when the mode now equals the requested low byte, else 0. Stack argument (ret 4). */
uint32_t set_and_check_video_mode_fe710(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t mode)
{
    uint32_t out[7];
    (void)u0; (void)u1; (void)u2; (void)u3;
    run_dos_int_ex(0x10u, mode, 0u, 0u, 0u, out);
    run_dos_int_ex(0x10u, 0x0f00u, 0u, 0u, 0u, out);
    return ((out[0] - mode) & 0xffu) == 0u ? 1u : 0u;
}

/* 0xeb288, 32 bytes. DOS allocate memory (int 21h, AH = 0x48) for PARAGRAPHS (16-bit): returns the segment/selector, or 0 when the allocation failed. Cdecl, one stack argument. */
uint32_t dos_alloc_eb288(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t paragraphs)
{
    uint32_t out[7];
    (void)u0; (void)u1; (void)u2; (void)u3;
    run_dos_int_ex(0x21u, 0x4800u, paragraphs & 0xffffu, 0u, 0u, out);
    if (out[6] != 0u)
        return 0u;
    return out[0];
}

/* 0xded1a, 35 bytes. When the file-search mode byte 0x132a5e is 9, copy the 0x2b-byte DOS DTA (address from int 21h, AH = 0x2f) to the buffer DST. */
void copy_dta_out_ded1a(uint32_t u0, uint32_t dst)
{
    uint32_t out[7];
    uint32_t i;
    (void)u0;
    if (EF_U8(0x00132a5eu) != 9u)
        return;
    run_dos_int_ex(0x21u, 0x2f00u, 0u, 0u, 0u, out);
    for (i = 0u; i < 0x2bu; i++)
        EF_U8(dst + i) = EF_U8(out[1] + i);
}

/* 0xded3d, 35 bytes. The reverse of 0xded1a: copy the 0x2b bytes at SRC into the DOS DTA. */
void copy_dta_in_ded3d(uint32_t u0, uint32_t src)
{
    uint32_t out[7];
    uint32_t i;
    (void)u0;
    if (EF_U8(0x00132a5eu) != 9u)
        return;
    run_dos_int_ex(0x21u, 0x2f00u, 0u, 0u, 0u, out);
    for (i = 0u; i < 0x2bu; i++)
        EF_U8(out[1] + i) = EF_U8(src + i);
}

/* 0xed542, 36 bytes. Length of the string at SEG:OFFSET (the selector SEG is ignored in the flat model), limited to MAX: counts characters until a zero byte or MAX is reached. */
uint32_t strnlen_far_ed542(uint32_t offset, uint32_t seg, uint32_t max)
{
    uint32_t n = 0u;
    (void)seg;
    for (;;) {
        uint8_t c = EF_U8(offset + n);
        if (c == 0u)
            break;
        if (n == max)
            break;
        n++;
    }
    return n;
}

/* 0xea49a, 13 bytes. Signed divide helper with implicit registers (EAX = dividend, EDX = divisor, ESI = result pointer): store the quotient at [ESI] and the remainder at [ESI+4]. Parameters are given in that register order. */
void divmod_store_ea49a(int32_t dividend, int32_t divisor, ef_addr32 result)
{
    EF_U32(result) = (uint32_t)(dividend / divisor);
    EF_U32(result + 4u) = (uint32_t)(dividend % divisor);
}

/* 0xfedcc, 16 bytes. Plot one pixel with implicit registers (ESI = row, EBX = x offset, CH = colour): address = row table 0x163200[row] + X + frame base 0x15b754. Parameters are given in that register order. */
void put_pixel_fedcc(uint32_t row, uint32_t x, uint32_t colour)
{
    EF_U8(x + EF_U32(0x00163200u + row * 4u) + EF_U32(0x0015b754u)) = (uint8_t)colour;
}

/* 0xfeddc, 29 bytes. Plot a two-pixel-wide, two-row block with implicit registers (ESI = row, EBX = x offset, CH = colour): both bytes of the word at the row address and at the next row (stride dword 0x1631f4) get the colour. Parameters are given in that register order. */
void put_block2_feddc(uint32_t row, uint32_t x, uint32_t colour)
{
    uint32_t addr = x + EF_U32(0x00163200u + row * 4u) + EF_U32(0x0015b754u);
    uint16_t w = (uint16_t)(((colour & 0xffu) << 8) | (colour & 0xffu));
    EF_U16(addr) = w;
    EF_U16(addr + EF_U32(0x001631f4u)) = w;
}

/* 0xc59b7, 35 bytes. Shift three consecutive dword triples left by SHIFT (CL) with implicit registers (ESI = source, EDI = destination): the three values of each triple go to destination offsets +0, +0xc and +0x18, the destination advancing by 4 and the source by 12 per triple. Parameters are given in the order source, destination, shift. */
void shift_triples_c59b7(ef_addr32 src, ef_addr32 dst, uint32_t shift)
{
    uint32_t i;
    for (i = 0u; i < 3u; i++) {
        EF_U32(dst) = EF_U32(src) << (shift & 31u);
        EF_U32(dst + 0xcu) = EF_U32(src + 4u) << (shift & 31u);
        EF_U32(dst + 0x18u) = EF_U32(src + 8u) << (shift & 31u);
        src += 0xcu;
        dst += 4u;
    }
}

/* 0xc5409, 56 bytes. Divide the three table entries at index ESI (dword arrays 0x150e58, 0x152e58 and 0x154e58) by the signed byte DIV (AL); implicit registers, parameters in the order DIV (AL), INDEX (ESI). */
void divide_table_entries_c5409(uint32_t div, uint32_t index)
{
    int32_t d = (int8_t)div;
    uint32_t off = index * 4u;
    EF_U32(0x00150e58u + off) = (uint32_t)((int32_t)EF_U32(0x00150e58u + off) / d);
    EF_U32(0x00152e58u + off) = (uint32_t)((int32_t)EF_U32(0x00152e58u + off) / d);
    EF_U32(0x00154e58u + off) = (uint32_t)((int32_t)EF_U32(0x00154e58u + off) / d);
}

/* 0xc5441, 62 bytes. Multiply the three table entries at index ESI (arrays 0x150e58, 0x152e58, 0x154e58) by the signed byte MUL (AL, sign extended); nothing is stored when any of the products overflows 32 bits. Implicit registers, parameters in the order MUL (AL), INDEX (ESI). */
void multiply_table_entries_c5441(uint32_t mul, uint32_t index)
{
    int64_t m = (int8_t)mul;
    uint32_t off = index * 4u;
    int64_t a = (int64_t)(int32_t)EF_U32(0x00150e58u + off) * m;
    int64_t b = (int64_t)(int32_t)EF_U32(0x00152e58u + off) * m;
    int64_t c = (int64_t)(int32_t)EF_U32(0x00154e58u + off) * m;
    if (a != (int32_t)a || b != (int32_t)b || c != (int32_t)c)
        return;
    EF_U32(0x00150e58u + off) = (uint32_t)a;
    EF_U32(0x00152e58u + off) = (uint32_t)b;
    EF_U32(0x00154e58u + off) = (uint32_t)c;
}

/* 0xc54ef, 41 bytes. Fetch the scaled value for table index EDI (implicit register): clear byte 0x158ed8, take the signed word 0x14fe58[index], shift it right by (byte 0x15912e - 0x10), store the low word in 0x15912c and return the shifted value (also left in EDX). */
uint32_t fetch_scaled_value_c54ef(uint32_t index)
{
    int32_t v;
    EF_U8(0x00158ed8u) = 0u;
    v = (int16_t)EF_U16(0x0014fe58u + index * 2u);
    v >>= (uint8_t)(EF_U8(0x0015912eu) - 0x10u) & 31u;
    EF_U16(0x0015912cu) = (uint16_t)v;
    return (uint32_t)v;
}

/* 0xc47e0, 66 bytes. Select the shade table for table index EDI (implicit register): take the signed word 0x14fe58[index] >> 5 clamped to 0..255, look up that entry in the byte table 0x463f60 and set the shading base dword 0x159084 to (byte << 8) + dword 0x164604; clear byte 0x159140. */
void select_shade_table_c47e0(uint32_t index)
{
    int32_t v = (int32_t)(int16_t)EF_U16(0x0014fe58u + index * 2u) >> 5;
    uint32_t b;
    if (v < 0)
        v = 0;
    if ((uint32_t)v > 0xffu)
        v = 0xff;
    b = EF_U8(0x00463f60u + (uint32_t)v);
    EF_U32(0x00159084u) = (b << 8) + EF_U32(0x00164604u);
    EF_U8(0x00159140u) = 0u;
}

/* 0xc48dc, 112 bytes. Load COUNT 2D vertices (word pairs) starting at SRC (implicit registers ESI = source, ECX = count) into the 48-byte vertex records at dword 0x17bf70: x and y are translated by the words 0x15bdb8/0x15bdbc and scaled left by 0x18 minus the bytes 0x164660/0x164664, depth +8 is set to 0x1000000, flag word +0x20 to 1 and the raw coordinates shifted left 16 stored at +0x18/+0x1c. Parameters in the order source, count. */
void load_vertices_c48dc(ef_addr32 src, uint32_t count)
{
    ef_addr32 dst = EF_U32(0x0017bf70u);
    ef_addr32 end = src + count * 4u;
    int32_t bx = (int16_t)EF_U16(0x0015bdb8u);
    int32_t dx = (int16_t)EF_U16(0x0015bdbcu);
    uint32_t sx = (0x18u - EF_U8(0x00164660u)) & 0x1fu;
    uint32_t sy = (0x18u - EF_U8(0x00164664u)) & 0x1fu;
    while (src != end) {
        EF_U32(dst) = (uint32_t)(((int32_t)(int16_t)EF_U16(src) - bx) << sx);
        EF_U32(dst + 4u) = (uint32_t)(((int32_t)(int16_t)EF_U16(src + 2u) - dx) << sy);
        EF_U32(dst + 8u) = 0x1000000u;
        EF_U16(dst + 0x20u) = 1u;
        EF_U32(dst + 0x18u) = (uint32_t)EF_U16(src) << 16;
        EF_U32(dst + 0x1cu) = (uint32_t)EF_U16(src + 2u) << 16;
        src += 4u;
        dst += 0x30u;
    }
}

/* 0x11afb4, 46 bytes. Box shape ratio for the doubles at BOX: 2 * area (0x11af60) / (extent sum (0x11af9c))^2; the constant 2.0 sits at 0x195814. Returns the result in ST0. */
double box_shape_ratio_11afb4(uint32_t box)
{
    double num = abs_area_11af60(box) * 2.0;
    double ext = box_extent_sum_11af9c(box);
    return num / square_double_b3fd0(ext);
}

/* 0xc737c, 50 bytes. Project the sum of two 3D points to a 2D screen point (implicit registers ESI = first point, EBX = second point, EDI = destination word pair): depth = (z1 + z2) >> 16, x = ((x1 + x2) >> 8) / depth + word 0x15bdb8, y = ((y1 + y2) >> 8) / depth + word 0x15bdbc, stored as two words; returns the advanced destination pointer. Parameters in the order ESI, EBX, EDI. */
uint32_t project_point_pair_c737c(ef_addr32 p1, ef_addr32 p2, ef_addr32 dst)
{
    int32_t depth = ((int32_t)EF_U32(p2 + 8u) + (int32_t)EF_U32(p1 + 8u)) >> 16;
    int32_t x = (((int32_t)EF_U32(p2) + (int32_t)EF_U32(p1)) >> 8) / depth;
    int32_t y = (((int32_t)EF_U32(p2 + 4u) + (int32_t)EF_U32(p1 + 4u)) >> 8) / depth;
    EF_U16(dst) = (uint16_t)(x + (int32_t)EF_U16(0x0015bdb8u));
    EF_U16(dst + 2u) = (uint16_t)(y + (int32_t)EF_U16(0x0015bdbcu));
    return dst + 4u;
}

/* 0x4e7b8, 83 bytes. Update the counter widget (0x184d4c) of the dialog named at 0x184b3c: with SHOW (AL) nonzero it shows the decimal value of dword 0x1a560a[byte 0x1a559f], otherwise the placeholder string 0x184d58. */
void update_counter_widget_4e7b8(uint32_t show)
{
    uint8_t buf[0x14];
    uint32_t w = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00184b3cu), 0x00184d4cu);
    if ((show & 0xffu) != 0u) {
        signed_to_digits_cbd0f((int32_t)EF_U32(0x001a560au + EF_U8(0x001a559fu) * 4u), (uint32_t)(uintptr_t)buf, 10u);
        guest_bind_dialog_widget(w, (uint32_t)(uintptr_t)buf);
    } else {
        guest_bind_dialog_widget(w, 0x00184d58u);
    }
}

/* 0x1047e4, 88 bytes. Try to allocate PARAGRAPHS of DOS memory through int386 (int 21h, AH = 0x48). Cdecl (PARAGRAPHS, BLOCKS_OUT, SEGMENT_OUT, ERROR_OUT): on failure return 0; on success store the BX result in *BLOCKS_OUT, the AX result shifted left 16 in *SEGMENT_OUT, the (zero) carry word in *ERROR_OUT and return 1. */
uint32_t dos_alloc_block_1047e4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3,
                                uint32_t paragraphs, uint32_t blocks_out, uint32_t segment_out, uint32_t error_out)
{
    uint32_t in[7];
    uint32_t out[7];
    (void)u0; (void)u1; (void)u2; (void)u3;
    in[0] = 0x4800u;
    in[1] = paragraphs;
    in[2] = 0u;
    in[3] = 0u;
    in[4] = 0u;
    in[5] = 0u;
    in[6] = 0u;
    int386_9c446(0x21u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out);
    if (out[6] != 0u)
        return 0u;
    EF_U32(segment_out) = out[0] << 16;
    EF_U32(blocks_out) = out[1];
    EF_U32(error_out) = out[6];
    return 1u;
}

/* 0xcd82c, 105 bytes. Masked blit from a 256-byte-wide source bitmap to the screen (colour 0 is transparent). Stack-only (ret 0x1c): Y, X (destination, signed words), HEIGHT, WIDTH (words), SRC_Y, SRC_X (signed words) and the SRC bitmap base. The destination row comes from the row table 0x163200 plus frame base 0x15b754, rows advance by the stride dword 0x1631f4. ORIGINAL BUG: a zero WIDTH or HEIGHT loops 2^32 times (do-while on the counters). */
void blit_masked_cd82c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3,
                       uint32_t y, uint32_t x, uint32_t height, uint32_t width,
                       uint32_t src_y, uint32_t src_x, uint32_t src)
{
    uint32_t s = src + (uint32_t)((int32_t)(int16_t)src_y << 8) + (uint32_t)(int32_t)(int16_t)src_x;
    uint32_t d = EF_U32(0x00163200u + (uint32_t)(int32_t)(int16_t)y * 4u) + EF_U32(0x0015b754u) +
                 (uint32_t)(int32_t)(int16_t)x;
    uint32_t stride = EF_U32(0x001631f4u);
    uint32_t rows = height & 0xffffu;
    (void)u0; (void)u1; (void)u2; (void)u3;
    do {
        uint32_t i = 0u;
        uint32_t cols = width & 0xffffu;
        do {
            uint8_t c = EF_U8(s + i);
            if (c != 0u)
                EF_U8(d + i) = c;
            i++;
            cols--;
        } while (cols != 0u);
        d += stride;
        s += 0x100u;
        rows--;
    } while (rows != 0u);
}

/* 0xe8784, 105 bytes. Variadic cdecl diagnostic: when byte 0x1600a6 is set, format FMT with its arguments into a 0x140-byte stack buffer and pass the format string 0x190cc0, dword 0x464390, word 0x16007c and the buffer to the error callback at 0x132a74. Returns 0. */
uint32_t report_formatted_e8784(ef_addr32 fmt, ...)
{
    if (EF_U8(0x001600a6u) != 0u) {
        char buf[0x140];
        va_list ap;
        va_start(ap, fmt);
        format_to_buffer_e3022((uint32_t)(uintptr_t)buf, fmt, (uint32_t)(uintptr_t)&ap[0]);
        va_end(ap);
        guest_callptr4c_x(EF_U32(0x00132a74u), 0x00190cc0u, EF_U32(0x00464390u), EF_U16(0x0016007cu), (uint32_t)(uintptr_t)buf);
    }
    return 0u;
}

/* 0x56268, 109 bytes. Append a message entry to the ring of ten 55-byte records at 0x1a63c0 (next index in dword 0x127db4): clear the first eight bytes, set the words +8 = 0x14 and +0xa = 5, copy the 43 bytes at SRC to +0xc, advance the index modulo ten. */
void push_message_entry_56268(ef_addr32 src)
{
    uint32_t idx = EF_U32(0x00127db4u);
    uint32_t rec = 0x001a63c0u + idx * 55u;
    uint32_t i;
    for (i = 0u; i < 8u; i++)
        EF_U8(rec + i) = 0u;
    EF_U16(rec + 8u) = 0x14u;
    EF_U16(rec + 0xau) = 5u;
    for (i = 0u; i < 43u; i++)
        EF_U8(rec + 0xcu + i) = EF_U8(src + i);
    idx++;
    if ((int32_t)idx >= 10)
        idx = 0u;
    EF_U32(0x00127db4u) = idx;
}

/* 0x10e2f5, 106 bytes. Convert the unsigned 64-bit value at VALUE_PTR to exactly N decimal digits (zero padded, high digits truncated) at BUF, followed by a zero byte. */
void u64_to_digits_10e2f5(ef_addr32 value_ptr, ef_addr32 buf, uint32_t n)
{
    uint64_t v = ((uint64_t)EF_U32(value_ptr + 4u) << 32) | EF_U32(value_ptr);
    uint32_t i;
    EF_U8(buf + n) = 0u;
    for (i = n; i > 0u; i--) {
        EF_U8(buf + i - 1u) = (uint8_t)(0x30u + (uint32_t)(v % 10u));
        v /= 10u;
    }
}

/* 0x10d9e3, 107 bytes. bsearch: find KEY in the sorted array BASE of N elements of SIZE bytes using the comparison callback CMP (stack argument, EAX/EDX = key and element pointers, result sign in EAX). Returns the matching element or 0. */
uint32_t bsearch_10d9e3(uint32_t key, uint32_t base, uint32_t n, uint32_t size, uint32_t cmp)
{
    uint32_t lo, hi, mid;
    int32_t r;
    if (n == 0u)
        return 0u;
    lo = base;
    hi = base + (n - 1u) * size;
    while (lo < hi) {
        mid = lo + (((hi - lo) / size) >> 1) * size;
        r = (int32_t)guest_callptr_eaxedx_ret_x(cmp, key, mid);
        if (r == 0)
            return mid;
        if (r < 0)
            hi = mid;
        else
            lo = mid + size;
    }
    if (lo == hi && (int32_t)guest_callptr_eaxedx_ret_x(cmp, key, lo) == 0)
        return lo;
    return 0u;
}

/* 0xc486c, 112 bytes. Set the scale pair from the stack argument VALUE (ret 4): for VALUE < 10 word 0x158ed0 = VALUE and word 0x158ed4 = 1 << (VALUE - 2); otherwise 0x158ed0 = VALUE / 10, 0x158ed4 = 1 << (VALUE / 10 - 2) and, when the remainder R is nonzero, 0x158ed4 += (0x158ed4 * 2 - 0x158ed4) * R / 10. */
void set_scale_pair_c486c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t value)
{
    int32_t v = (int32_t)value;
    int32_t q, rem;
    uint16_t scale;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (v < 10) {
        EF_U16(0x00158ed0u) = (uint16_t)v;
        EF_U16(0x00158ed4u) = (uint16_t)(1u << (uint8_t)(v - 2));
        return;
    }
    q = v / 10;
    rem = v % 10;
    EF_U16(0x00158ed0u) = (uint16_t)q;
    scale = (uint16_t)(1u << (uint8_t)(q - 2));
    EF_U16(0x00158ed4u) = scale;
    if (rem != 0) {
        uint16_t dbl = (uint16_t)(scale << 1);
        dbl = (uint16_t)(dbl - EF_U16(0x00158ed4u));
        EF_U16(0x00158ed4u) = (uint16_t)(EF_U16(0x00158ed4u) + (uint16_t)(((int32_t)dbl * rem) / 10));
    }
}

/* 0x3519c, 122 bytes. Unpack the settings record at SRC into the game globals: copy the first 44 bytes to 0x125828, scatter the bytes/words at +0x2c..+0x3d to 0x15b7e6, 0x1f88fa, 0x1f88f0, 0x1f88ee, 0x1f88fb, 0x1a3636, 0x1a3640 and 0x15b7e4, convert the double at +0x33 to a single float stored at 0x128550 and copy the 18 bytes at +0x3e to 0x19bbf0. */
void unpack_settings_3519c(ef_addr32 src)
{
    uint32_t i;
    double d;
    float f;
    for (i = 0u; i < 0x2cu; i += 4u)
        EF_U32(0x00125828u + i) = EF_U32(src + i);
    EF_U8(0x0015b7e6u) = EF_U8(src + 0x2cu);
    EF_U8(0x001f88fau) = EF_U8(src + 0x2du);
    EF_U16(0x001f88f0u) = EF_U16(src + 0x2eu);
    EF_U16(0x001f88eeu) = EF_U16(src + 0x30u);
    d = *(double *)ef_guest_memory_ptr(src + 0x33u);
    EF_U8(0x001f88fbu) = EF_U8(src + 0x32u);
    EF_U8(0x001a3636u) = EF_U8(src + 0x3bu);
    EF_U8(0x001a3640u) = EF_U8(src + 0x3cu);
    f = (float)d;
    *(float *)ef_guest_memory_ptr(0x00128550u) = f;
    EF_U8(0x0015b7e4u) = EF_U8(src + 0x3du);
    for (i = 0u; i < 0x10u; i += 4u)
        EF_U32(0x0019bbf0u + i) = EF_U32(src + 0x3eu + i);
    EF_U16(0x0019bc00u) = EF_U16(src + 0x4eu);
}

/* 0xdf738, 122 bytes. Poll the modem status register (port dword 0x15eed0 + 6, saved in byte 0x15eed4): in mode 2 (byte 0x13579a) with the delta bit 0 set, mirror the CTS bit 4 onto bit 1 of the next register (clear it when CTS is low; set it when CTS is high and byte 0x15ef28 allows). Sets byte 0x13581e when carrier detect (bit 7) is low and byte 0x13578c is set. */
void poll_modem_status_df738(void)
{
    uint32_t base = EF_U32(0x0015eed0u);
    uint32_t st = guest_fn_9c430(base + 6u) & 0xffu;
    EF_U8(0x0015eed4u) = (uint8_t)st;
    if (EF_U8(0x0013579au) == 2u && (st & 1u)) {
        if ((st & 0x10u) == 0u) {
            guest_fn_117b0c(base + 1u, (guest_fn_9c430(base + 1u) & 0xfdu));
        } else if (EF_U8(0x0015ef28u) != 0u) {
            guest_fn_117b0c(base + 1u, (guest_fn_9c430(base + 1u) & 0xffu) | 2u);
        }
    }
    if ((EF_U8(0x0015eed4u) & 0x80u) == 0u && EF_U8(0x0013578cu) != 0u)
        EF_U8(0x0013581eu) = 1u;
}

/* 0x11458c, 138 bytes. Cdecl (REC, KEY): look KEY up in the 6-byte entry table of the animation set at REC+0xc (count: signed word +8, entries from +0xa, key word first); when found, clear REC dword +0x30, set REC +0x14 to REC +0x10 + 8 + the entry's dword +2 and, unless dword 0x4dd808 is set, reset the four dwords at REC +0x84..+0x90 to -1. Does nothing when REC or its set is missing or KEY is not found. */
void select_animation_11458c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t rec, uint32_t key)
{
    uint32_t set, ent, i;
    int32_t n;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (rec == 0u)
        return;
    set = EF_U32(rec + 0xcu);
    if (set == 0u)
        return;
    n = (int16_t)EF_U16(set + 8u);
    ent = set + 0xau;
    i = 0u;
    if (n > 0) {
        uint32_t e = ent;
        do {
            if (EF_U16(e) == key)
                break;
            i++;
            e += 6u;
        } while ((int32_t)i < n);
    }
    if ((int32_t)i == n)
        return;
    ent += i * 6u;
    EF_U32(rec + 0x30u) = 0u;
    EF_U32(rec + 0x14u) = EF_U32(rec + 0x10u) + 8u + EF_U32(ent + 2u);
    if (EF_U32(0x004dd808u) == 0u) {
        uint32_t p = rec + 4u;
        for (;;) {
            EF_U32(p + 0x80u) = 0xffffffffu;
            if (p == rec + 0x10u)
                break;
            p += 4u;
        }
    }
}

/* 0x31774, 161 bytes. Reposition the camera from the angles at 0x19e572/0x19e560 (0xdc91c) by the offset (0, 0, -dword 0x19e597) rotated into 0x19e5c1, then add the velocity (0x19e560/64/68) plus the extra offsets 0x19e57e/0x19e582 to the position dwords 0x19e5c1/5c5/5c9. Sets byte 0x1a6d1f. */
void reposition_camera_31774(void)
{
    uint16_t angles[4];
    int32_t vec[3];
    EF_U8(0x001a6d1fu) = 1u;
    guest_fn_dc91c((uint32_t)(uintptr_t)angles, 0x0019e572u, 0x0019e560u);
    vec[0] = 0;
    vec[1] = 0;
    vec[2] = -(int32_t)EF_U32(0x0019e597u);
    rotate_vector_by_angles_32414((uint32_t)(uintptr_t)angles, (uint32_t)(uintptr_t)vec, 0x0019e5c1u);
    EF_U32(0x0019e5c1u) += EF_U32(0x0019e560u) + EF_U32(0x0019e57eu);
    EF_U32(0x0019e5c5u) += EF_U32(0x0019e564u) + EF_U32(0x0019e582u);
    EF_U32(0x0019e5c9u) += EF_U32(0x0019e568u);
}

/* 0xdfeb8, 145 bytes. Serial line status handler: store (line status (port 0x15eed0 + 5) >> 1) & 0xf in byte 0x15eed5; when bit 3 of that is set, switch the receive state byte 0x15f089 from 1 to 2 (setting byte 0x15f08f and word 0x15f090 = 0xe) and count the error in word 0x15f0a0 while clearing word 0x15f09c; otherwise, in state 3 with byte 0x15f088 set, set dword 0x15f084 = 5 and latch the menu entry (0xdf65c). */
void serial_status_dfeb8(void)
{
    uint32_t st = ((guest_fn_9c430(EF_U32(0x0015eed0u) + 5u) & 0xffu) >> 1) & 0xfu;
    EF_U8(0x0015eed5u) = (uint8_t)st;
    if ((EF_U8(0x0015eed5u) & 8u) != 0u) {
        if (EF_U8(0x0015f089u) == 1u) {
            EF_U8(0x0015f08fu) = 1u;
            EF_U16(0x0015f090u) = 0xeu;
            EF_U8(0x0015f089u) = 2u;
        }
        EF_U16(0x0015f09cu) = 0u;
        EF_U16(0x0015f0a0u) = (uint16_t)(EF_U16(0x0015f0a0u) + 1u);
        return;
    }
    if (EF_U8(0x0015f089u) == 3u && EF_U8(0x0015f088u) != 0u) {
        EF_U32(0x0015f084u) = 5u;
        (void)latch_menu_entry_df65c();
    }
}

/* 0x55f54, 174 bytes. Build and send the 43-byte command packet of type 0x29: fields A (EAX), B (EDX), C (EBX) and D (ECX) at +1/+5/+9/+0xd, then the seven stack arguments at +0x11 (dword), +0x15 (dword), +0x19 (word), +0x21 (dword), +0x1d (word), +0x1f (word) and +0x1b (word); word +0x25 is 0xffff and dword +0x27 is (running counter 0x127db0 << 4) + player byte 0x1a666b (the counter is incremented). The packet is emitted (0xa3f74), pushed on the message ring (0x56268) and copied to 0x1a663c. Stack-only parameters with ret 0x1c. */
void send_command_packet_55f54(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                               uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3,
                               uint32_t s4, uint32_t s5, uint32_t s6)
{
    uint8_t p[0x2b];
    uint32_t i, counter;
    p[0] = 0x29u;
    *(uint32_t *)&p[1] = a;
    *(uint32_t *)&p[5] = b;
    *(uint32_t *)&p[9] = c;
    *(uint32_t *)&p[0xd] = d;
    *(uint32_t *)&p[0x11] = s0;
    *(uint32_t *)&p[0x15] = s1;
    *(uint16_t *)&p[0x19] = (uint16_t)s2;
    *(uint32_t *)&p[0x21] = s3;
    *(uint16_t *)&p[0x1d] = (uint16_t)s4;
    *(uint16_t *)&p[0x25] = 0xffffu;
    *(uint16_t *)&p[0x1f] = (uint16_t)s5;
    *(uint16_t *)&p[0x1b] = (uint16_t)s6;
    counter = EF_U32(0x00127db0u);
    *(uint32_t *)&p[0x27] = (counter << 4) + EF_U8(0x001a666bu);
    EF_U32(0x00127db0u) = counter + 1u;
    append_packet_a3f74((uint32_t)(uintptr_t)p, 0x2bu);
    push_message_entry_56268((uint32_t)(uintptr_t)p);
    for (i = 0u; i < 0x2bu; i++)
        EF_U8(0x001a663cu + i) = p[i];
}

/* 0x4d24c, 176 bytes. Draw the stacked-count icon for the record REC at (X, Y) in game modes 1 and 2 (byte 0x1a53dc) when REC word +0x36 is set: the count COUNT is replaced by the difference of the two bytes referenced by the last list node (head dword 0x137958, link +4) whose dwords +0x11/+0x15 equal REC +0x16/+0x1a; a nonzero count is clamped to 16 and the icon from the table at 0x126570 + 0x23f00 (20 bytes per count) is drawn at (X - 10, Y + 3) with 0xca620. */
void draw_count_icon_4d24c(uint32_t x, uint32_t y, uint32_t rec, uint32_t count)
{
    uint32_t mode = EF_U8(0x001a53dcu);
    uint32_t node = EF_U32(0x00137958u);
    int16_t cx;
    if (mode != 2u && mode != 1u)
        return;
    if (EF_U16(rec + 0x36u) == 0u)
        return;
    while (node != 0u) {
        if (EF_U32(node + 0x11u) == EF_U32(rec + 0x16u) && EF_U32(node + 0x15u) == EF_U32(rec + 0x1au))
            count = (uint32_t)EF_U8(EF_U32(node + 0x33u)) - (uint32_t)EF_U8(EF_U32(node + 0x37u));
        node = EF_U32(node + 4u);
    }
    cx = (int16_t)count;
    if (cx == 0)
        return;
    if (cx > 0x10)
        cx = 0x10;
    guest_fn_ca620((int32_t)(EF_U32(0x00126570u) + 0x23f00u + (uint32_t)((int32_t)cx - 1) * 20u),
                   (int16_t)(x - 10u), (int16_t)(y + 3u), 0x14u, 0xdu, 0x280u);
}

/* 0x81394, 179 bytes. Reserve cost from the budget dword 0x12b118 for the entries of the word list LIST (first word = count, then entry indices terminated by 0xffff): each entry's cost comes from 0xd7754 on the record in the table at dword 0x1fca00; the entry is accepted (index appended to the list at 0x1fc97e, counter 0x1fc9f8, at most 0x14) only while the cost fits the remaining budget. Returns the running total SUM plus the accepted costs; returns 0 when byte 0x12b104 is clear and SUM unchanged when SUM or LIST is null. */
uint32_t reserve_entries_81394(uint32_t unused, uint32_t list, uint32_t sum)
{
    uint32_t p = list;
    int16_t i = 1;
    (void)unused;
    if (EF_U8(0x0012b104u) == 0u)
        return 0u;
    if (sum == 0u || list == 0u)
        return sum;
    if (!((int16_t)i <= (int16_t)EF_U16(list)))
        return sum;
    p = list + 2u;
    for (;;) {
        uint32_t cost, slot;
        if (EF_U16(p) == 0xffffu)
            return sum;
        if ((int32_t)EF_U32(0x001fc9f8u) >= 0x14)
            return sum;
        cost = get_dword_d7754(EF_U32(EF_U32(0x001fca00u) + (uint32_t)(int32_t)(int16_t)EF_U16(p) * 4u));
        if ((int32_t)cost > (int32_t)EF_U32(0x0012b118u) - 1)
            return sum;
        slot = EF_U32(0x001fc9f8u) + 1u;
        EF_U16(0x001fc97eu + slot * 2u) = EF_U16(p);
        EF_U32(0x001fc9f8u) = slot;
        EF_U32(0x0012b118u) = EF_U32(0x0012b118u) - cost;
        sum += cost;
        i++;
        p += 2u;
        if (!((int16_t)i <= (int16_t)EF_U16(list)))
            return sum;
    }
}

/* 0x8ca24, 194 bytes. Initialise the 20 three-byte weapon slots of the unit REC from the loadout record INDEX (202-byte entries at dword 0x19bb58, slot types at +0xa1): each slot's word +0x19c is 1, 0 for type 0 and 0x13 for types 9 and 0x14, and its type byte +0x19b is copied. Then set the unit's word +0x1d5 to 0 or 0x6e0 depending on its class byte (+0xe9 minus 0x18: 0..3 and 8..12 give 0, everything else 0x6e0), byte +0x1d4 = 1, words +0x1d8/+0x1da = 0x96 and refresh the unit (0x8f07c). */
void init_weapon_slots_8ca24(uint32_t rec, uint32_t index)
{
    uint32_t i;
    uint32_t cls;
    for (i = 0u; (int16_t)i < 0x14; i++) {
        uint32_t src = (uint32_t)((int32_t)(int16_t)index * 0xca) + EF_U32(0x0019bb58u) + i;
        uint32_t slot = i * 3u + rec;
        uint8_t type = EF_U8(src + 0xa1u);
        EF_U16(slot + 0x19cu) = 1u;
        EF_U8(slot + 0x19bu) = type;
        if (type == 0u)
            EF_U16(slot + 0x19cu) = 0u;
        else if (type == 9u || type == 0x14u)
            EF_U16(slot + 0x19cu) = 0x13u;
    }
    cls = (uint8_t)(EF_U8(rec + 0xe9u) - 0x18u);
    if (cls <= 0xcu && (cls < 4u || cls >= 8u))
        EF_U16(rec + 0x1d5u) = 0u;
    else
        EF_U16(rec + 0x1d5u) = 0x6e0u;
    EF_U8(rec + 0x1d4u) = 1u;
    EF_U16(rec + 0x1d8u) = 0x96u;
    EF_U16(rec + 0x1dau) = 0x96u;
    guest_fn_8f07c(rec);
}

/* 0x369fc, 180 bytes. Open the stream file NAME for the player (flag byte MODE goes to 0x19f5a0): complain through the callback at 0x132a74 when one is already open (byte 0x19f5a9), reset the buttons (0x36964), initialise the stream state (bytes 0x19f5a1 = 1, 0x19f5a2 = 0x48, 0x19f5a5 = 0, 0x19f59f = 0, 0x19f59e = 1, word 0x19f586 = 1, state 0x19f5a9 = 2, name 0x19f330), run the setup 0x35de0 (its result goes to 0x19f32c), open the file (0xa7948 -> 0x19f35c, error 0x183220 on -1) and allocate the buffer of (signed word 0x125900) * 8 bytes into 0x19f354. */
void open_stream_369fc(uint32_t name, uint32_t mode)
{
    if (EF_U8(0x0019f5a9u) != 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x001831fcu);
    set_buttons_36964();
    EF_U32(0x0019f330u) = name;
    EF_U8(0x0019f5a1u) = 1u;
    EF_U8(0x0019f5a2u) = 0x48u;
    EF_U8(0x0019f5a0u) = (uint8_t)mode;
    EF_U8(0x0019f5a5u) = 0u;
    EF_U16(0x0019f586u) = 1u;
    EF_U8(0x0019f5a9u) = 2u;
    guest_fn_35de0_x(0u, 0u); /* result in EDX is stored in 0x19f32c */
    EF_U8(0x0019f59fu) = 0u;
    EF_U32(0x0019f35cu) = (uint32_t)open_slot_a7948(0u, 0u, 0u, 0u, EF_U32(0x0019f330u));
    if (EF_U32(0x0019f35cu) == 0xffffffffu)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00183220u);
    EF_U8(0x0019f59eu) = 1u;
    EF_U32(0x0019f354u) = arena_alloc_e2a94((uint32_t)((int32_t)(int16_t)EF_U16(0x00125900u) << 3));
}

/* 0xb2758, 183 bytes. Print the records of the list whose head pointer is at HEAD_PTR (link +4) that are of kind 0 (byte +0xb) and whose coordinate word pairs (+0x19/+0x21, +0x1b/+0x23, +0x1d/+0x25, +0x1f/+0x27) match (X, Y): each match is printed with 0xb21ec to the handle OUT (stack argument, ret 4) followed by two copies of the separator format 0x18e060. Stops after MAX matches. Returns the number printed. */
uint32_t print_matching_records_b2758(uint32_t x, uint32_t y, uint32_t max, uint32_t head_ptr, uint32_t out)
{
    uint32_t node = EF_U32(head_ptr);
    uint32_t n = 0u;
    while (node != 0u) {
        if (EF_U8(node + 0xbu) == 0u) {
            if ((EF_U16(node + 0x19u) == (x & 0xffffu) && EF_U16(node + 0x21u) == (y & 0xffffu)) ||
                (EF_U16(node + 0x1bu) == (x & 0xffffu) && EF_U16(node + 0x23u) == (y & 0xffffu)) ||
                (EF_U16(node + 0x1du) == (x & 0xffffu) && EF_U16(node + 0x25u) == (y & 0xffffu)) ||
                (EF_U16(node + 0x1fu) == (x & 0xffffu) && EF_U16(node + 0x27u) == (y & 0xffffu))) {
                n++;
                print_record_b21ec(EF_U32(node + 0x4du), out);
                guest_fn_b1bb0(out, 0x0018e060u);
                guest_fn_b1bb0(out, 0x0018e060u);
                if ((n & 0xffffu) == (max & 0xffffu))
                    return n;
            }
        }
        node = EF_U32(node + 4u);
    }
    return n;
}

/* 0xd2584, 176 bytes. Build the render object at 0x420970 from COUNT vertex indices (word list at LIST, implicit ESI; COUNT in ECX): set dword 0x42099c = 1, the count at 0x4209d0, 0x420974 = 0, the vertex pointer array 0x4209d0 (pointers stored from +4), the callback 0x1016e0 and the shade table entry selected by dword 0x15907c; every vertex (record in the buffer at dword 0x17bf70 indexed by the word) gets flag 4 set and flag 2 cleared, its dword +0x2c set to the shade value 0x15977c[i] >> 8 << 16, and the object's clip mask (+0x2c) is ANDed with the vertex flags. Finishes with 0xd23d8 on the object. Parameters in the order LIST, COUNT. */
void build_render_object_d2584(ef_addr32 list, uint32_t count)
{
    uint32_t obj = 0x00420970u;
    uint32_t slot = 0x004209d0u;
    int32_t i;
    EF_U32(0x0042099cu) = 1u;
    EF_U32(0x004209d0u) = count;
    EF_U32(0x00420974u) = 0u;
    EF_U32(0x004209bcu) = slot;
    EF_U32(0x00420990u) = 0x001016e0u;
    EF_U32(0x00420994u) = EF_U32(((EF_U32(0x0015907cu) >> 2) * 4u) + 0x0015b86cu);
    for (i = 0; i < (int32_t)count; i++) {
        uint32_t v = EF_U32(0x0017bf70u) + EF_U16(list);
        uint32_t shade = (uint32_t)((int32_t)EF_U32(0x0015977cu + (uint32_t)i * 4u) >> 8);
        EF_U8(v + 0x20u) = (uint8_t)(EF_U8(v + 0x20u) | 4u);
        EF_U8(v + 0x20u) = (uint8_t)(EF_U8(v + 0x20u) & 0xfdu);
        EF_U32(v + 0x2cu) = shade << 16;
        EF_U32(obj + 0x2cu) = EF_U32(obj + 0x2cu) & EF_U32(v + 0x20u);
        list += 2u;
        EF_U32(slot + 4u) = v;
        slot += 4u;
    }
    process_render_object_d23d8(obj);
}

/* 0x510d0, 175 bytes. Load the language text file: build the path (default directory + 0x18515c + the decimal word from the high half of dword 0x1a5a86 (format 0x18516c) + suffix 0x185170) in a stack buffer, ask the size callback at 0x132ab4 (error 0x185178 when 0), allocate that many bytes into 0x1a58b0 (error 0x18519c when out of memory), read the file with 0xb3c04 and fall back to the load callback at 0x132ab8 when that fails, then reset the text state (0x50c70). */
void load_text_file_510d0(void)
{
    char path[0x64];
    char num[8];
    uint32_t size;
    build_default_path_9c980((uint32_t)(uintptr_t)path);
    strcat_9ca57((uint32_t)(uintptr_t)path, 0x0018515cu);
    guest_sprintf_a4342((uint32_t)(uintptr_t)num, 0x0018516cu, (uint32_t)((int32_t)EF_U32(0x001a5a86u) >> 16));
    strcat_9ca57((uint32_t)(uintptr_t)path, (uint32_t)(uintptr_t)num);
    strcat_9ca57((uint32_t)(uintptr_t)path, 0x00185170u);
    size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), (uint32_t)(uintptr_t)path);
    if (size == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00185178u, (uint32_t)(uintptr_t)path);
    EF_U32(0x001a58b0u) = arena_alloc_e2a94(size);
    if (EF_U32(0x001a58b0u) == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018519cu);
    if ((guest_fn_b3c04_x((uint32_t)(uintptr_t)path, EF_U32(0x001a58b0u)) & 0xffu) == 0u)
        guest_callptr_eaxedx_x(EF_U32(0x00132ab8u), (uint32_t)(uintptr_t)path, EF_U32(0x001a58b0u));
    reset_text_50c70();
}

/* 0x150d4, 196 bytes. Draw the 0x22 label entries of the table at 0x1225d4 (19 bytes each: kind byte +1, box words +2/+4/+6, height word +8, text at +0xa): for each entry whose kind has a text slot (0xcd7f0) enter scroll mode with the text pointer, measure the text (0x48c34) and draw it in colour 0xe centred in the box (x = box x + (box width - text width) / 2, y = box y + height / 2 - 2) with 0x48bb0; finally leave scroll mode (0xc39ac). Scroll mode is entered with EDX = 0x100 and EBX = 0xc0 (not modelled by the one-argument declaration). */
void draw_label_entries_150d4(void)
{
    int16_t i;
    for (i = 0; i < 0x22; i++) {
        uint32_t ent = 0x001225d4u + (uint32_t)i * 0x13u;
        int16_t bx = (int16_t)EF_U16(ent + 2u);
        int16_t by = (int16_t)EF_U16(ent + 4u);
        int16_t bw = (int16_t)EF_U16(ent + 6u);
        int16_t bh = (int16_t)EF_U16(ent + 8u);
        uint32_t text = ent + 0xau;
        int32_t width, x, y;
        if (get_text_slot_cd7f0((int16_t)EF_U8(ent + 1u)) == 0u)
            continue;
        enter_scroll_mode_c39f0(text);
        width = (int16_t)formatted_text_width_48c34(1u, text);
        y = (int16_t)(by + (bh >> 1) - 2);
        x = (int16_t)(bx + (((int32_t)bw - width) >> 1));
        draw_formatted_text_mode_48bb0(0xeu, 1u, x, y, text);
    }
    leave_scroll_mode_c39ac();
}

/* 0x95704, 196 bytes. Set up the external view from the angle words A, B, C (EAX, EDX, EBX), D (ECX) and the stack argument E (ret 4): reset the view matrix 0x12dbec (0xd0b68), rotate it by -E (0xd0ebc) and -D (0xd0dbc), store the angles at words 0x12dba2/a4/a6 (negated, word 0x12dba0 = 0), copy the target's position (dwords 0 .. 8 of the record at dword 0x20f15c) to 0x21446c/70/74 and, when the target's class (high word of +0x2c) passes 0x9125c, store the same lookup result for it in word 0x21448a. Finishes with 0x9357c. */
void setup_external_view_95704(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t tgt;
    init_bounds_d0b68(0x0012dbecu);
    guest_fn_d0ebc(0x0012dbecu, (uint16_t)(0u - e));
    guest_fn_d0dbc(0x0012dbecu, (uint16_t)(0u - d), 0u);
    tgt = EF_U32(0x0020f15cu);
    EF_U16(0x0012dba2u) = (uint16_t)a;
    EF_U16(0x0012dba4u) = (uint16_t)b;
    EF_U16(0x0012dba6u) = (uint16_t)c;
    EF_U16(0x0012dba2u) = (uint16_t)(0u - a);
    EF_U16(0x0012dba4u) = (uint16_t)(0u - b);
    EF_U32(0x0021446cu) = EF_U32(tgt);
    EF_U16(0x0012dba0u) = 0u;
    EF_U32(0x00214470u) = EF_U32(tgt + 4u);
    EF_U32(0x00214474u) = EF_U32(tgt + 8u);
    EF_U16(0x0012dba6u) = (uint16_t)(0u - c);
    if ((guest_fn_9125c((int32_t)EF_U32(tgt + 0x2cu) >> 16) & 0xffffu) != 0u)
        EF_U16(0x0021448au) = (uint16_t)guest_fn_9125c((int32_t)EF_U32(EF_U32(0x0020f15cu) + 0x2cu) >> 16);
    guest_fn_9357c();
}

/* 0x1e6f0, 207 bytes. Draw an icon (sprite pair) for COLOR (EAX) with SHADOW flag (EDX) at the position (X, Y) (stack arguments, ret 8) displaced by the word pair 0x123688[IDX]: the sprite index comes from the word table 0x1235e0[KIND] (0 = nothing, otherwise minus one); the sprite from the pointer table 0x123640 is drawn centred (position minus half its byte width/height) with 0xcf7c4 in colour 3 when SHADOW is set, else COLOR; with SHADOW set the sprite from table 0x123664 is drawn on top in COLOR. */
void draw_icon_1e6f0(uint32_t color, uint32_t shadow, uint32_t idx, uint32_t kind, uint32_t x, uint32_t y)
{
    int16_t i = (int16_t)idx;
    int16_t k = (int16_t)kind;
    int16_t px = (int16_t)(x + EF_U16(0x00123688u + (uint32_t)(int32_t)i * 4u));
    int16_t py = (int16_t)(y + EF_U16(0x0012368au + (uint32_t)(int32_t)i * 4u));
    int16_t s = (int16_t)EF_U16(0x001235e0u + (uint32_t)(int32_t)k * 2u);
    uint32_t off, spr;
    if (s == 0)
        return;
    s--;
    off = (uint32_t)(int32_t)s * 4u;
    spr = EF_U32(0x00123640u + off);
    guest_fn_cf7c4_x(spr, (uint32_t)((int32_t)py - ((int32_t)EF_U8(spr + 1u) >> 1)),
                     (uint32_t)((int32_t)px - ((int32_t)EF_U8(spr) >> 1)),
                     (shadow & 0xffffu) == 0u ? (uint32_t)(int32_t)(int16_t)color : 3u);
    if ((shadow & 0xffffu) == 0u)
        return;
    spr = EF_U32(0x00123664u + off);
    guest_fn_cf7c4_x(spr, (uint32_t)((int32_t)py - ((int32_t)EF_U8(spr + 1u) >> 1)),
                     (uint32_t)((int32_t)px - ((int32_t)EF_U8(spr) >> 1)),
                     (uint32_t)(int32_t)(int16_t)color);
}

/* 0xbc287, 200 bytes. fopen core: open the file NAME with the mode string MODE into the stream record FILE (flags dword +0xc, handle +0x10, buffer fields +4/+8/+0x14), SHARE being passed on to the low-level open (0xfa4a9). The first mode character selects read (open flags from the write bit, no creation) or write/append (flags 0x21/0x22 plus 0x10 for append, 0x40 otherwise, permissions 0x180); the text/binary bit 0x40 of the stream flags adds 0x200 or 0x100. On failure (handle -1) release the record and return 0; for append seek to the end (0xf2183). Updates the stream flags (0xfa7d2) and returns FILE. */
uint32_t fopen_core_bc287(uint32_t name, uint32_t mode, uint32_t file, uint32_t share)
{
    uint32_t flags, perm, c;
    EF_U8(file + 0xcu) = (uint8_t)(EF_U8(file + 0xcu) & 0xfcu);
    EF_U32(file + 0xcu) = EF_U32(file + 0xcu) | parse_open_mode_bc1d3(mode);
    c = guest_fn_e7835(EF_U8(mode)) & 0xffu;
    if (c == 0x72u) {
        flags = (EF_U8(file + 0xcu) & 2u) ? 2u : 0u;
        flags |= (EF_U8(file + 0xcu) & 0x40u) ? 0x200u : 0x100u;
        perm = 0u;
    } else {
        flags = ((EF_U8(file + 0xcu) & 1u) != 0u ? 1u : 0u) + 0x21u;
        flags |= (c == 0x61u) ? 0x10u : 0x40u;
        flags |= (EF_U8(file + 0xcu) & 0x40u) ? 0x200u : 0x100u;
        perm = 0x180u;
    }
    EF_U32(file + 0x10u) = (uint32_t)guest_fn_fa4a9_x(name, flags, share, perm);
    if (EF_U32(file + 0x10u) == 0xffffffffu) {
        release_list_node_fa77b(file);
        return 0u;
    }
    EF_U32(file + 4u) = 0u;
    EF_U32(file + 8u) = 0u;
    EF_U32(file + 0x14u) = 0u;
    if (c == 0x61u)
        guest_fn_f2183(file, 0u, 2u);
    update_flags_fa7d2(file);
    return file;
}

/* 0x4dee0, 209 bytes. Hit test the 24 map icons (index 1..24) against the point (X, Y) (16-bit signed): icon I has its centre at the words 0x1a55a2[I] / 0x1a55d6[I] and half-size 8.0 / (float 0x1a53df); the first icon whose box contains the point and whose type byte 0x1a5672[I] is not 0x11 is returned, else -1. */
int32_t find_icon_at_4dee0(uint32_t x, uint32_t y)
{
    int16_t i;
    float half = *(float *)ef_guest_memory_ptr(0x00184d3fu) / *(float *)ef_guest_memory_ptr(0x001a53dfu);
    float px = (float)(int16_t)x;
    float py = (float)(int16_t)y;
    for (i = 1; i < 0x19; i++) {
        float cx = (float)EF_U16(0x001a55a2u + (uint32_t)i * 2u);
        float cy = (float)EF_U16(0x001a55d6u + (uint32_t)i * 2u);
        if ((double)cx - (double)half >= (double)px)
            continue;
        if ((double)cx + (double)half <= (double)px)
            continue;
        if ((double)cy - (double)half >= (double)py)
            continue;
        if ((double)cy + (double)half <= (double)py)
            continue;
        if (EF_U8(0x001a5672u + (uint32_t)i) == 0x11u)
            continue;
        return i;
    }
    return -1;
}

/* 0x1014a4, 214 bytes. Set up the scan-line interpolation between the vertex records P0 (EBX) and P1 (ECX): N = the integer part (>> 16) of the y fields (+0x1c) of P1 minus P0; when N <= 0 return it unchanged, otherwise copy P0's x, y, +0x24, +0x28, +0x2c and +8 into the start record OUT (EAX) and fill the step record STEP (EDX) with the per-line deltas of +0x18 (x), of +0x24/+0x28 when P0 flag 2 is set and of +0x2c when flag 4 is set (zero otherwise), plus the reciprocal depth 1/P0[+8] (float +0x10) and its per-line change (float +0x14). Returns N. */
int32_t setup_edge_step_1014a4(uint32_t out, uint32_t step, uint32_t p0, uint32_t p1)
{
    int32_t n = ((int32_t)EF_U32(p1 + 0x1cu) >> 16) - ((int32_t)EF_U32(p0 + 0x1cu) >> 16);
    float r0, r1;
    if (n <= 0)
        return n;
    EF_U32(out) = EF_U32(p0 + 0x18u);
    EF_U32(out + 4u) = EF_U32(p0 + 0x1cu);
    EF_U32(out + 8u) = EF_U32(p0 + 0x24u);
    EF_U32(out + 0xcu) = EF_U32(p0 + 0x28u);
    EF_U32(out + 0x10u) = EF_U32(p0 + 0x2cu);
    EF_U32(out + 0x14u) = EF_U32(p0 + 8u);
    EF_U32(step + 4u) = 0u;
    EF_U32(step + 8u) = 0u;
    EF_U32(step + 0xcu) = 0u;
    EF_U32(step) = (uint32_t)(((int32_t)EF_U32(p1 + 0x18u) - (int32_t)EF_U32(p0 + 0x18u)) / n);
    if (EF_U8(p0 + 0x20u) & 2u) {
        EF_U32(step + 4u) = (uint32_t)(((int32_t)EF_U32(p1 + 0x24u) - (int32_t)EF_U32(p0 + 0x24u)) / n);
        EF_U32(step + 8u) = (uint32_t)(((int32_t)EF_U32(p1 + 0x28u) - (int32_t)EF_U32(p0 + 0x28u)) / n);
    }
    if (EF_U8(p0 + 0x20u) & 4u)
        EF_U32(step + 0xcu) = (uint32_t)(((int32_t)EF_U32(p1 + 0x2cu) - (int32_t)EF_U32(p0 + 0x2cu)) / n);
    r0 = (float)(1.0 / (double)(int32_t)EF_U32(p0 + 8u));
    *(float *)ef_guest_memory_ptr(step + 0x10u) = r0;
    r1 = (float)(((1.0 / (double)(int32_t)EF_U32(p1 + 8u)) - (double)r0) / (double)n);
    *(float *)ef_guest_memory_ptr(step + 0x14u) = r1;
    return n;
}

static uint32_t project_axis_fc8cc(int32_t v, uint32_t shift, int32_t depth, uint32_t center)
{
    uint32_t lo = (uint32_t)v << (shift & 31u);
    int32_t hi = v >> ((0u - shift) & 31u);
    int64_t num = ((int64_t)hi << 32) | lo;
    int32_t q = (int32_t)(num / depth);
    return ((uint32_t)q << 8) + center;
}

/* 0xfc8cc, 210 bytes. Cdecl-style (COUNT, LIST; ret 8): project COUNT (byte) vertex records whose pointers are listed at LIST: skip those already flagged (word +0x20 bit 0), otherwise flag them and divide the x and y fields (+0, +4) shifted left by the bytes 0x164660/0x164664 (plus 8 when the depth dword +8 is below 0x800000) by the depth (+8 >> 8) and store the screen position (<< 8, plus the centre dwords 0x164658/0x16465c) in +0x18 and +0x1c. */
void project_vertices_fc8cc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t count, uint32_t list)
{
    uint8_t n = (uint8_t)count;
    (void)u0; (void)u1; (void)u2; (void)u3;
    do {
        uint32_t v = EF_U32(list);
        uint32_t d;
        list += 4u;
        if (EF_U16(v + 0x20u) & 1u)
            continue;
        d = EF_U32(v + 8u);
        EF_U16(v + 0x20u) = (uint16_t)(EF_U16(v + 0x20u) | 1u);
        if ((d & 0xff800000u) != 0u) {
            int32_t depth = (int32_t)d >> 8;
            EF_U32(v + 0x18u) = project_axis_fc8cc((int32_t)EF_U32(v), EF_U8(0x00164660u), depth, EF_U32(0x00164658u));
            EF_U32(v + 0x1cu) = project_axis_fc8cc((int32_t)EF_U32(v + 4u), EF_U8(0x00164664u), depth, EF_U32(0x0016465cu));
        } else {
            int32_t depth = (int32_t)d;
            EF_U32(v + 0x18u) = project_axis_fc8cc((int32_t)EF_U32(v), (uint32_t)(EF_U8(0x00164660u) + 8u) & 0xffu, depth, EF_U32(0x00164658u));
            EF_U32(v + 0x1cu) = project_axis_fc8cc((int32_t)EF_U32(v + 4u), (uint32_t)(EF_U8(0x00164664u) + 8u) & 0xffu, depth, EF_U32(0x0016465cu));
        }
    } while (--n != 0u);
}

/* 0xd2634, 205 bytes. Textured variant of 0xd2584: build the render object at 0x420970 from COUNT vertex indices (word list at LIST, implicit ESI; count in ECX; parameters in the order LIST, COUNT) with the span routine 0xd3cb0 when dword 0x158f0c is set, else 0xff3bc (stored at object +0x24); every vertex gets its texture coordinates (+0x24 / +0x28 = word pair 0x15997c[i] << 9), flag 2 set and flag 4 cleared, and the clip mask is ANDed with the vertex flags. Finishes with 0xd23d8. */
void build_textured_object_d2634(ef_addr32 list, uint32_t count)
{
    uint32_t obj = 0x00420970u;
    uint32_t slot = 0x004209d0u;
    int32_t i;
    EF_U32(0x0042099cu) = 1u;
    EF_U32(0x00420974u) = 0u;
    EF_U32(0x004209bcu) = slot;
    EF_U32(0x004209d0u) = count;
    EF_U32(0x00420990u) = 0x001016e0u;
    EF_U32(obj + 0x24u) = EF_U32(0x00158f0cu) != 0u ? 0x000d3cb0u : 0x000ff3bcu;
    for (i = 0; i < (int32_t)count; i++) {
        uint32_t v = EF_U32(0x0017bf70u) + EF_U16(list);
        EF_U32(v + 0x24u) = (uint32_t)EF_U16(0x0015997cu + (uint32_t)i * 4u) << 9;
        EF_U32(v + 0x28u) = (uint32_t)EF_U16(0x0015997eu + (uint32_t)i * 4u) << 9;
        EF_U8(v + 0x20u) = (uint8_t)((EF_U8(v + 0x20u) | 2u) & 0xfbu);
        EF_U32(obj + 0x2cu) = EF_U32(obj + 0x2cu) & EF_U32(v + 0x20u);
        list += 2u;
        EF_U32(slot + 4u) = v;
        slot += 4u;
    }
    process_render_object_d23d8(obj);
}

/* 0xe51c4, 224 bytes. Add a clipped rectangle to the dirty-rectangle list at 0x4339a0 (8-byte entries, running index in dword 0x433ea0, count word 0x433eac): the rectangle given by A (EAX), C (EDX), B (EBX), D (ECX) is rejected unless A < word 0x433ea4, B > word 0x433ea6, C < word 0x433ea8 and D > word 0x433eaa; with the stack flag ADD (ret 4) nonzero it is clamped to those bounds and stored as (A', C', B' - A', D' - C') at the current entry, the index and the count are advanced; the byte 0x433eae is set to 1 whenever the rectangle passed the bounds. */
void add_dirty_rect_e51c4(uint32_t a, uint32_t c, uint32_t b, uint32_t d, uint32_t add)
{
    uint32_t idx = EF_U32(0x00433ea0u);
    int16_t va = (int16_t)a, vb = (int16_t)b, vc = (int16_t)c, vd = (int16_t)d;
    if (va >= (int16_t)EF_U16(0x00433ea4u) || vb <= (int16_t)EF_U16(0x00433ea6u) ||
        vc >= (int16_t)EF_U16(0x00433ea8u) || vd <= (int16_t)EF_U16(0x00433eaau)) {
        EF_U32(0x00433ea0u) = idx;
        return;
    }
    if ((add & 0xffu) != 0u) {
        int16_t lim;
        uint32_t e = 0x004339a0u + idx * 8u;
        lim = (int16_t)EF_U16(0x00433ea6u);
        if (!(va > lim)) va = lim;
        EF_U16(e) = (uint16_t)va;
        lim = (int16_t)EF_U16(0x00433eaau);
        if (!(vc > lim)) vc = lim;
        EF_U16(e + 2u) = (uint16_t)vc;
        lim = (int16_t)EF_U16(0x00433ea4u);
        if (!(vb < lim)) vb = lim;
        EF_U16(e + 4u) = (uint16_t)(vb - va);
        lim = (int16_t)EF_U16(0x00433ea8u);
        if (!(vd < lim)) vd = lim;
        EF_U16(e + 6u) = (uint16_t)(vd - vc);
        idx++;
        EF_U16(0x00433eacu) = (uint16_t)(EF_U16(0x00433eacu) + 1u);
    }
    EF_U8(0x00433eaeu) = 1u;
    EF_U32(0x00433ea0u) = idx;
}

/* 0xa38f8, 225 bytes. Find the lowest rank (byte +5 of the entry record) among the flagged entries of two index lists: with byte 0x135798 set, scan the byte lists at dwords 0x136b84 (count byte at the pointer in 0x136b88; flag bit 0 of (byte +0xd of the record >> 5) &3) and 0x136b74 (count at the pointer in 0x136b78; same test), each index selecting a record pointer in the table at 0x135878 (32-byte stride); every entry whose rank is below the running minimum (word 0x1357b0) and whose flag is set updates the minimum and sets byte 0x13583c. */
void find_lowest_rank_a38f8(void)
{
    int16_t best = (int16_t)EF_U16(0x001357b0u);
    uint32_t list_b = EF_U32(0x00136b74u);
    uint32_t list_a = EF_U32(0x00136b84u);
    uint16_t i;
    if (EF_U8(0x00135798u) != 0u) {
        for (i = 0u; i < EF_U8(EF_U32(0x00136b88u)); i++) {
            uint32_t rec = EF_U32(0x00135878u + ((uint32_t)EF_U8(list_a + i) << 5));
            int16_t rank = (int16_t)EF_U8(rec + 5u);
            if (best >= rank)
                continue;
            if ((((uint32_t)EF_U8(rec + 0xdu) * 2u) & 0xffu) >> 6 & 1u) {
                EF_U8(0x0013583cu) = 1u;
                best = rank;
            }
        }
        for (i = 0u; i < EF_U8(EF_U32(0x00136b78u)); i++) {
            uint32_t rec = EF_U32(0x00135878u + ((uint32_t)EF_U8(list_b + i) << 5));
            int16_t rank = (int16_t)EF_U8(rec + 5u);
            if (best >= rank)
                continue;
            if ((((uint32_t)EF_U8(rec + 0xdu) * 2u) & 0xffu) >> 6 & 1u) {
                EF_U8(0x0013583cu) = 1u;
                best = rank;
            }
        }
    }
    EF_U32(0x00136b84u) = list_a;
    EF_U32(0x00136b74u) = list_b;
    EF_U16(0x001357b0u) = (uint16_t)best;
}

/* 0x8fe4, 220 bytes. Assign object INDEX (sign-extended word) to a free sub-slot of table TABLE: find a slot with 0x825c, clear its byte +0x25, set byte +0x26, flag it via 0x8394, and when the index is below the object count (word 0x19bb5c) and the object (0xca-byte records at 0x19bb58) has type 0xc, store the index at slot +0x20, clear +0x22 and post event 0x899 (dword +0x4e of the object >> 16). Slot records are 0x38 bytes in the array at +0x16 of the 0x22-byte table entries at 0x1972e0. */
void assign_table_slot_8fe4(uint32_t index, uint32_t table)
{
    uint32_t tbl = table & 0xffu;
    uint32_t slot = find_free_slot_825c((uint8_t)tbl) & 0xffu;
    int16_t idx = (int16_t)index;
    uint32_t ent, base, obj;
    if (slot == 0xffu || slot == 0xfeu || (int32_t)idx == 0xff)
        return;
    ent = EF_U32(0x001972e0u) + tbl * 34u;
    base = slot * 56u;
    EF_U8(EF_U32(ent + 0x16u) + base + 0x25u) = 0u;
    EF_U8(base + EF_U32(ent + 0x16u) + 0x26u) = 1u;
    flag_sub_entry_side_8394((uint8_t)tbl, (uint8_t)slot);
    if (idx >= (int16_t)EF_U16(0x0019bb5cu))
        return;
    obj = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)idx * 0xca);
    if (EF_U8(obj + 0x1au) != 0xcu)
        return;
    ent = EF_U32(0x001972e0u) + tbl * 34u;
    EF_U16(base + EF_U32(ent + 0x16u) + 0x20u) = (uint16_t)idx;
    EF_U8(EF_U32(ent + 0x16u) + base + 0x22u) = 0u;
    guest_fn_46254(0x899u, (int32_t)EF_U32(obj + 0x4eu) >> 16, 0x4eu, slot, 0u, 0u);
}

/* 0xdc43e, 219 bytes. Implicit-register fragment (EBX, ESI, EDI = three angles; params in that order). Builds eight rotation-matrix words at 0xdc597 (inside the code segment, patched in place) from the sine table at 0x137e08 and cosine table at 0x139e08 (dword entries indexed by (angle >> 1) & 0x7ffc). The three reduced angles are also stored in 0xdc438/0xdc43a/0xdc43c. */
void build_rotation_words_dc43e(uint32_t ebx, uint32_t esi, uint32_t edi)
{
    uint32_t a = (ebx >> 1) & 0x7ffcu;
    uint32_t b = (esi >> 1) & 0x7ffcu;
    uint32_t c = (edi >> 1) & 0x7ffcu;
    uint32_t out = 0x000dc597u;
    int32_t p, q, s_a, c_a, s_b, c_b;
    uint32_t i;
    int16_t w[8];
    EF_U16(0x000dc438u) = (uint16_t)a;
    EF_U16(0x000dc43au) = (uint16_t)b;
    EF_U16(0x000dc43cu) = (uint16_t)c;
    w[0] = (int16_t)((int32_t)EF_U32(0x00139e08u + c) >> 9);
    p = (int32_t)EF_U32(0x00137e08u + (((a + c) & 0xffffu) & 0x7ffcu)) >> 10;
    q = (int32_t)EF_U32(0x00139e08u + (((a + c) & 0xffffu) & 0x7ffcu)) >> 10;
    s_a = (int32_t)EF_U32(0x00137e08u + (((a - c) & 0xffffu) & 0x7ffcu)) >> 10;
    c_a = (int32_t)EF_U32(0x00139e08u + (((a - c) & 0xffffu) & 0x7ffcu)) >> 10;
    w[1] = (int16_t)(p - s_a);
    w[2] = (int16_t)(c_a - q);
    w[3] = (int16_t)(-((int32_t)EF_U32(0x00137e08u + c) >> 9));
    w[4] = (int16_t)(q + c_a);
    w[5] = (int16_t)(p + s_a);
    s_b = (int32_t)EF_U32(0x00137e08u + a) >> 9;
    c_b = (int32_t)EF_U32(0x00139e08u + a) >> 9;
    w[6] = (int16_t)(-s_b);
    w[7] = (int16_t)c_b;
    for (i = 0; i < 8u; i++)
        EF_U16(out + i * 2u) = (uint16_t)w[i];
}

/* 0xed5ea, 219 bytes. Format the 16.16 fixed-point VALUE as decimal text at BUF with the precision in dword +8 of the format record FMT (-1 selects 4): optional '-', the integer digits (0xcbd0f on the high word), '.', PREC fraction digits, then round up using bit 15 of the leftover fraction, propagating a carry through 9s. */
void format_fixed_ed5ea(uint32_t buf, int32_t value, uint32_t fmt)
{
    uint32_t v = (uint32_t)value;
    uint32_t p = buf, start, after, n;
    uint8_t c;
    if (value < 0) {
        v = (uint32_t)-value;
        EF_U8(p) = 0x2du;
        p++;
    }
    if (EF_U32(fmt + 8u) == 0xffffffffu)
        EF_U32(fmt + 8u) = 4u;
    (void)signed_to_digits_cbd0f((int32_t)((v >> 16) & 0xffffu), p, 10u);
    start = p;
    while (EF_U8(p) != 0u)
        p++;
    after = p + 1u;
    if (EF_U32(fmt + 8u) != 0u) {
        EF_U8(p) = 0x2eu;
        p = after;
        for (n = 0; (int32_t)n < (int32_t)EF_U32(fmt + 8u); n++) {
            v &= 0x0000ffffu;
            v = v * 10u;
            EF_U8(p) = (uint8_t)(((v >> 16) & 0xffu) + 0x30u);
            p++;
        }
        EF_U8(p) = 0u;
    }
    if (v & 0x8000u) {
        for (;;) {
            if (p == start) {
                EF_U8(start) = 0x31u;
                p = start + 1u;
                for (;;) {
                    c = EF_U8(p);
                    after = p + 1u;
                    if (c != 0x30u)
                        break;
                    p = after;
                }
                if (c == 0x2eu) {
                    EF_U8(p) = 0x30u;
                    p = after + 1u;
                    EF_U8(after) = c;
                    while (EF_U8(p) == 0x30u)
                        p++;
                }
                EF_U8(p) = 0x30u;
                p++;
                EF_U8(p) = 0u;
                break;
            }
            p--;
            if (EF_U8(p) == 0x2eu)
                p--;
            c = EF_U8(p);
            if (c != 0x39u) {
                EF_U8(p) = (uint8_t)(c + 1u);
                break;
            }
            EF_U8(p) = 0x30u;
        }
    }
}

/* 0x10e4dd, 219 bytes. Evaluate a binary-operation record: FLAGS (EAX) selects the operation kind (bits 0x40, 0x80, then 0x100, 0x800, 0x200, 0x400 give kinds 1,2,3,4,6,5), the handler is the dword at 0x17d49c[FLAGS & 0x1f], and FLAGS bits 0x1000/0x2000/0x4000/0x8000 pick the result operand (-const at 0x195bf4, 0.0, 1.0, +const, else the double at B). A, B are 8-byte operands copied into the record; 0x45b60 evaluates it and its double result is returned. */
double eval_binary_record_10e4dd(uint32_t flags, uint32_t a, uint32_t b)
{
    uint32_t rec[10];
    uint32_t kind = 0u; /* ORIGINAL BUG: ESI is uninitialised when no kind bit is set */
    uint32_t lo, hi;
    rec[2] = EF_U32(a);
    rec[3] = EF_U32(a + 4u);
    rec[4] = EF_U32(b);
    rec[5] = EF_U32(b + 4u);
    if (flags & 0x40u)
        kind = 1u;
    else if (flags & 0x80u)
        kind = 2u;
    else if (flags & 0x100u)
        kind = 3u;
    else if (flags & 0x800u)
        kind = 4u;
    else if (flags & 0x200u)
        kind = 6u;
    else if (flags & 0x400u)
        kind = 5u;
    rec[0] = kind;
    rec[1] = EF_U32(0x0017d49cu + (flags & 0x1fu) * 4u);
    if (flags & 0x1000u) {
        lo = EF_U32(0x00195bf4u);
        hi = EF_U32(0x00195bf8u) ^ 0x80000000u;
    } else if (flags & 0x2000u) {
        lo = 0u;
        hi = 0u;
    } else if (flags & 0x4000u) {
        lo = 0u;
        hi = 0x3ff00000u;
    } else if (flags & 0x8000u) {
        lo = EF_U32(0x00195bf4u);
        hi = EF_U32(0x00195bf8u);
    } else {
        lo = EF_U32(b);
        hi = EF_U32(b + 4u);
    }
    rec[6] = lo;
    rec[7] = hi;
    return object_double_value_45b60((ef_addr32)(uintptr_t)rec);
}

/* 0x2c454, 220 bytes. Search the 0xca-byte object records (count word 0x19bb5c, base dword 0x19bb58) for the match nearest POS (distance from the position at +0x61 via 0xb415c) whose word +0x15 equals ID, byte +0x1b equals SIDE, byte +0x1a equals TYPE (or TYPE is 0) and byte +0x3f is 0. Returns the record index, or 0xff. */
uint32_t find_nearest_object_2c454(uint32_t pos, uint32_t id, uint32_t side, uint32_t type)
{
    double best = 0.0;
    uint32_t result = 0xffu;
    int16_t i;
    for (i = 0; i < (int16_t)EF_U16(0x0019bb5cu); i++) {
        uint32_t obj, base;
        double d;
        if ((int32_t)i == 0xff)
            continue;
        base = EF_U32(0x0019bb58u);
        d = distance_2d_b415c(base + (uint32_t)((int32_t)i * 0xca) + 0x61u, pos);
        if (!(d < best) && (int32_t)(int16_t)result != 0xff)
            continue;
        base = EF_U32(0x0019bb58u);
        obj = base + (uint32_t)((int32_t)i * 0xca);
        if ((uint16_t)id != EF_U16(obj + 0x15u))
            continue;
        if (EF_U8(obj + 0x1bu) != (uint8_t)side)
            continue;
        if (EF_U8(obj + 0x1au) != (uint8_t)type && (uint8_t)type != 0u)
            continue;
        if (EF_U8(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)i * 0xca) + 0x3fu) != 0u)
            continue;
        best = d;
        result = (uint32_t)(uint16_t)i;
    }
    return result;
}

/* 0xe378c, 220 bytes. Open the resource file NAME (handle stored in dword 0x15f8fc), read its count word into 0x15f900 and, when non-zero, allocate and fill with 0xff two tables (0x7d0 and 0xfa0 bytes, pointers in 0x429224/0x429228), read two dwords (the second wins), seek to it, load the string table (0xe3868) and seek to offset 10. Returns 1 on success, 0 when the open fails or the count is zero (the handle is then closed and reset to -1). */
uint8_t open_resource_file_e378c(uint32_t name)
{
    uint32_t pos[2];
    uint32_t h = guest_fn_a7900(name);
    EF_U32(0x0015f8fcu) = h;
    if (h == 0xffffffffu)
        return 0u;
    read_resource_e3170(0x0015f900u, 2u);
    if (EF_U16(0x0015f900u) == 0u) {
        guest_fn_a7990(EF_U32(0x0015f8fcu));
        EF_U32(0x0015f8fcu) = 0xffffffffu;
        return 0u;
    }
    EF_U32(0x00429224u) = arena_take_e29e8(0x7d0u);
    fill_bytes_b3fb0(EF_U32(0x00429224u), 0xffu, 0x7d0u);
    EF_U32(0x00429228u) = arena_take_e29e8(0xfa0u);
    fill_bytes_b3fb0(EF_U32(0x00429228u), 0xffu, 0xfa0u);
    read_resource_e3170((uint32_t)(uintptr_t)&pos[0], 4u);
    read_resource_e3170((uint32_t)(uintptr_t)&pos[0], 4u);
    seek_start_a7ab4(0u, 0u, 0u, 0u, EF_U32(0x0015f8fcu), pos[0]);
    load_string_table_e3868();
    seek_start_a7ab4(0u, 0u, 0u, 0u, EF_U32(0x0015f8fcu), 10u);
    return 1u;
}

/* 0x6bca8, 221 bytes. Normalise the three angle-like doubles of the current object (dword pointer 0x1f71bc; fields +0x69, +0x71, +0x61 = X, Y, Z) into range: if X is above the limit at 0x1863cc it is mirrored (X = k - X, with Y and Z each advanced by k = double 0x1863d4); if X is below 0x1863dc it is mirrored about 0x1863e4 with the same Y/Z advance; then Z and Y are wrapped by the period (0x1863ec / 0x1863f4) when above 0x1863d4 or below 0x1863e4. */
void normalise_angles_6bca8(void)
{
    uint32_t o = EF_U32(0x001f71bcu);
    double *x = (double *)ef_guest_memory_ptr(o + 0x69u);
    double *y = (double *)ef_guest_memory_ptr(o + 0x71u);
    double *z = (double *)ef_guest_memory_ptr(o + 0x61u);
    double k = *(double *)ef_guest_memory_ptr(0x001863d4u);
    double lim_hi = *(double *)ef_guest_memory_ptr(0x001863ccu);
    double lim_lo = *(double *)ef_guest_memory_ptr(0x001863dcu);
    double d = *(double *)ef_guest_memory_ptr(0x001863e4u);
    double wrap_dn = *(double *)ef_guest_memory_ptr(0x001863ecu);
    double wrap_up = *(double *)ef_guest_memory_ptr(0x001863f4u);
    if (*x > lim_hi) {
        *x = k - *x;
        *y = *y + k;
        *z = k + *z;
    }
    if (*x < lim_lo) {
        *x = d - *x;
        *y = *y + k;
        *z = k + *z;
    }
    if (*z > k)
        *z = *z + wrap_dn;
    if (*z < d)
        *z = *z + wrap_up;
    if (*y > k)
        *y = *y + wrap_dn;
    if (*y < d)
        *y = *y + wrap_up;
}

/* 0x7548c, 221 bytes. Build the display name of unit INDEX (16-bit, looked up with 0x7489c) into DEST. When the record's word +0x28 equals word 0x19bb5e, copy its name (+0x2b), or the table string 0x12684c[byte +0x2a], or the default format 0x12f44d when that byte is 0. Otherwise copy the name of the object whose index is the high half of dword +0x26 (0xca-byte records at 0x19bb58, name at +1), append the separator string 0x186c10 and then the unit's name as above. */
void build_unit_name_7548c(uint32_t dest, uint32_t index)
{
    char tmp[0x50];
    uint32_t rec = guest_fn_7489c((int32_t)(int16_t)index);
    uint32_t name, kind;
    if (rec == 0u)
        return;
    name = rec + 0x2bu;
    kind = EF_U8(rec + 0x2au);
    if (EF_U16(0x0019bb5eu) == EF_U16(rec + 0x28u)) {
        if (name != 0u) {
            copy_string_9d749(dest, name);
        } else if (kind == 0u) {
            guest_sprintf_a4342((uint32_t)(uintptr_t)tmp, 0x0012f44du);
        } else {
            copy_string_9d749((uint32_t)(uintptr_t)tmp, EF_U32(0x0012684cu + kind * 4u));
        }
        return;
    }
    copy_string_9d749(dest, EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(rec + 0x26u) >> 16) * 0xca) + 1u);
    strcat_9ca57(dest, 0x00186c10u);
    if (name != 0u) {
        copy_string_9d749((uint32_t)(uintptr_t)tmp, name);
    } else {
        if (kind == 0u) {
            guest_sprintf_a4342((uint32_t)(uintptr_t)tmp, 0x0012f44du);
        } else {
            copy_string_9d749((uint32_t)(uintptr_t)tmp, EF_U32(0x0012684cu + kind * 4u));
        }
    }
    strcat_9ca57(dest, (uint32_t)(uintptr_t)tmp);
}

/* 0x7b08c, 221 bytes. Scale two 3-double vectors (A = EAX, B = EDX; components used in the order +8, +0x10, +0) by the double at 0x187c60, truncate each to an integer and store them as the dwords 0x420910/0x420904/0x420918 and 0x420914/0x420908/0x42091c (the +0x10 and second-vector +0x10 values minus 10) and as the words 0x420930..0x42093a, then call 0x7b7d8 and return its result. */
uint32_t store_scaled_vectors_7b08c(uint32_t a, uint32_t b)
{
    double s = *(double *)ef_guest_memory_ptr(0x00187c60u);
    int32_t i0 = (int32_t)truncate_9d72c(*(double *)ef_guest_memory_ptr(a + 8u) * s);
    int32_t i1 = (int32_t)truncate_9d72c(*(double *)ef_guest_memory_ptr(a + 0x10u) * s);
    int32_t i2 = (int32_t)truncate_9d72c(*(double *)ef_guest_memory_ptr(a) * s);
    int32_t i3 = (int32_t)truncate_9d72c(*(double *)ef_guest_memory_ptr(b + 8u) * s);
    int32_t i4 = (int32_t)truncate_9d72c(*(double *)ef_guest_memory_ptr(b + 0x10u) * s);
    int32_t i5 = (int32_t)truncate_9d72c(*(double *)ef_guest_memory_ptr(b) * s);
    EF_U32(0x00420910u) = (uint32_t)i0;
    EF_U32(0x00420904u) = (uint32_t)(i1 - 10);
    EF_U32(0x00420918u) = (uint32_t)i2;
    EF_U32(0x00420914u) = (uint32_t)i3;
    EF_U32(0x00420908u) = (uint32_t)(i4 - 10);
    EF_U32(0x0042091cu) = (uint32_t)i5;
    EF_U16(0x00420930u) = (uint16_t)i0;
    EF_U16(0x00420932u) = (uint16_t)(i1 - 10);
    EF_U16(0x00420934u) = (uint16_t)i2;
    EF_U16(0x00420936u) = (uint16_t)i3;
    EF_U16(0x00420938u) = (uint16_t)(i4 - 10);
    EF_U16(0x0042093au) = (uint16_t)i5;
    return guest_fn_7b7d8();
}

/* 0xe6d78, 221 bytes. Fuel/energy-style update on the double globals: 0x4327e8 = 0x4327d8*0x432770*0x432da0/0x432da8*0x4327e0 + 0x432808*0x4327b8*0x4327f8; then 0x4327e8 = (0x432800 + 0x432810) * 0x190be4 * 0x4327e8 and 0x432f58 -= 0x4327e8 * 0x4330d0, clamped at 0. When the stock reaches 0 the rates 0x4327f8, 0x432800 and 0x432810 are zeroed. */
void update_stock_e6d78(void)
{
#define EF_D(a) (*(double *)ef_guest_memory_ptr(a))
    double t1 = EF_D(0x004327d8u) * EF_D(0x00432770u) * EF_D(0x00432da0u) / EF_D(0x00432da8u) * EF_D(0x004327e0u);
    double t2 = EF_D(0x00432808u) * EF_D(0x004327b8u) * EF_D(0x004327f8u);
    double v;
    EF_D(0x004327e8u) = t1 + t2;
    v = (EF_D(0x00432800u) + EF_D(0x00432810u)) * EF_D(0x00190be4u) * EF_D(0x004327e8u);
    EF_D(0x004327e8u) = v;
    EF_D(0x00432f58u) = EF_D(0x00432f58u) - v * EF_D(0x004330d0u);
    if (0.0 > EF_D(0x00432f58u))
        EF_D(0x00432f58u) = 0.0;
    if ((EF_U32(0x00432f5cu) & 0x7fffffffu) == 0u && EF_U32(0x00432f58u) == 0u) {
        EF_D(0x004327f8u) = 0.0;
        EF_D(0x00432800u) = 0.0;
        EF_D(0x00432810u) = 0.0;
    }
#undef EF_D
}

/* 0x1132b8, 221 bytes. cdecl (OBJ, REC): copy the 24-byte record REC, fill every part of it that is below 1 (four signed words, then four signed dwords) from the default table addressed by dword +0xc of the object's descriptor (real-mode seg:off plus 0x120cc6), provided word +0x10 of that descriptor is non-zero, store the result at descriptor +0x16 and send message 0x304 to the object. */
void apply_default_record_1132b8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t obj, uint32_t rec)
{
    uint16_t w[4];
    uint32_t d[4];
    uint32_t desc, p, def, i;
    (void)u0; (void)u1; (void)u2; (void)u3;
    for (i = 0; i < 4u; i++)
        w[i] = EF_U16(rec + i * 2u);
    for (i = 0; i < 4u; i++)
        d[i] = EF_U32(rec + 8u + i * 4u);
    desc = EF_U32(EF_U32(obj) + 0x10u);
    if (EF_U16(desc + 0x10u) != 0u) {
        p = EF_U32(desc + 0xcu);
        def = EF_U32(0x00120cc6u) + (p & 0xffffu) + ((p >> 16) << 4);
        for (i = 0; i < 4u; i++)
            if ((int16_t)w[i] < 1)
                w[i] = EF_U16(def + i * 2u);
        for (i = 0; i < 4u; i++)
            if ((int32_t)d[i] < 1)
                d[i] = EF_U32(def + 8u + i * 4u);
    }
    desc = EF_U32(EF_U32(obj) + 0x10u);
    for (i = 0; i < 4u; i++)
        EF_U16(desc + 0x16u + i * 2u) = w[i];
    for (i = 0; i < 4u; i++)
        EF_U32(desc + 0x1eu + i * 4u) = d[i];
    guest_fn_101f88(EF_U32(obj), 0x304u, 0u, 0u);
}

/* 0x18a68, 222 bytes. Advance the scene-node scan cursor (word 0x211ae2, starting at 0x2001) for the unit whose index is the signed word at 0x1f71ca: step through the nodes (0x8ffe8) until one has its position (three ints scaled by the double at 0x181830, taken from the 0x45-byte records at 0x1a14f8 indexed by the high word of node +0x36) within the distance (double 0x181838) of the unit's position at +0x49, or the node list runs out (cursor reset to 0x2001, at most twice). */
void scan_nodes_near_unit_18a68(void)
{
    uint32_t unit = guest_fn_7489c((int32_t)(int16_t)EF_U16(0x001f71cau));
    uint8_t left = 2u, found = 0u;
    double pos[3];
    if (unit == 0u)
        return;
    if (EF_U16(0x00211ae2u) == 0u)
        EF_U16(0x00211ae2u) = 0x2001u;
    else
        EF_U16(0x00211ae2u) = (uint16_t)(EF_U16(0x00211ae2u) + 1u);
    unit += 0x49u;
    do {
        uint32_t node = guest_fn_8ffe8((int32_t)(int16_t)EF_U16(0x00211ae2u));
        if (node == 0u) {
            left--;
            EF_U16(0x00211ae2u) = 0x2001u;
        } else {
            uint32_t base = EF_U32(0x001a14f8u);
            uint32_t rec = base + (uint32_t)((int32_t)(int16_t)EF_U16(node + 0x38u) * 0x45);
            double k = *(double *)ef_guest_memory_ptr(0x00181830u);
            pos[0] = (double)(int32_t)EF_U32(rec + 0xfu) * k;
            pos[1] = (double)(int32_t)EF_U32(rec + 0x7u) * k;
            pos[2] = (double)(int32_t)EF_U32(rec + 0xbu) * k;
            if (distance_2d_b415c((uint32_t)(uintptr_t)pos, unit) < *(double *)ef_guest_memory_ptr(0x00181838u))
                found = 1u;
            else
                EF_U16(0x00211ae2u) = (uint16_t)(EF_U16(0x00211ae2u) + 1u);
        }
    } while (!found && left != 0u);
}

/* 0xc8e58, 224 bytes. When a data-set name is present (byte 0x41f5f0), build the two paths prefix+name+suffix (0x18eee0/0x18eeec, loaded into 0x425c8c, and 0x18ef1c/0x18ef24, loaded into 0x433f34) through the loader pointer at 0x132ab8, reporting a failure through the pointer at 0x132a74; on success of the second load set dword 0x433f30 to 1, rebuild the remap table and clear dword 0x160000. Finally call 0xe9a24(0x2d0, 0x425cbc, 0x419600). */
void load_dataset_files_c8e58(void)
{
    char path[0x90];
    uint32_t p = (uint32_t)(uintptr_t)path;
    if (EF_U8(0x0041f5f0u) == 0u)
        return;
    build_path_9c9c0(p, 0x0018eee0u);
    strcat_9ca57(p, 0x0041f5f0u);
    strcat_9ca57(p, 0x0018eeecu);
    if ((guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), p, 0x00425c8cu) & 0xffu) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018eef4u, p);
    build_path_9c9c0(p, 0x0018ef1cu);
    strcat_9ca57(p, 0x0041f5f0u);
    strcat_9ca57(p, 0x0018ef24u);
    if ((guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), p, 0x00433f34u) & 0xffu) == 0u) {
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018ef2cu, p);
    } else {
        EF_U32(0x00433f30u) = 1u;
        rebuild_remap_table_e7e30(0u, 0u, 0u);
        EF_U32(0x00160000u) = 0u;
    }
    guest_fn_e9a24(0x2d0u, 0x00425cbcu, 0x00419600u);
}

/* 0xa81e4, 225 bytes. Open a named script/data file: take the first whitespace-delimited token of NAME (EAX), format "<fmt 0x18d668>" from the default path (0xa7b70), the string pointed to by table 0x1371c0[byte 0x1371ef] and the token, copy 0x18 bytes from SRC (EDX) to the record 0x418678, call the size hook at 0x132ab4 (word result to 0x41868a, complaining through 0x132a74 when zero), load the record with the hook at 0x132ab8 into 0x417278, store that buffer at 0x41868c, run the section parser 0xa82f0 on the record and set byte 0x1371fa to 1. */
void open_named_data_a81e4(uint32_t name, uint32_t src)
{
    char tok[0x98];
    int16_t n = 0;
    uint8_t c;
    for (;;) {
        c = EF_U8(name + (uint32_t)(int32_t)n);
        if (c == 0xau || c == 0xdu || c == 0x20u || c == 0x9u || c == 0u)
            break;
        tok[n] = (char)c;
        n++;
    }
    tok[n] = 0;
    {
        char buf[0x98];
        guest_sprintf_a4342((uint32_t)(uintptr_t)buf, 0x0018d668u, default_path_a7b70(),
                            EF_U32(0x001371c0u + (uint32_t)EF_U8(0x001371efu) * 4u), (uint32_t)(uintptr_t)tok);
        memcpy_dc832(0x00418678u, src, 0x18u);
        EF_U16(0x0041868au) = (uint16_t)guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), (uint32_t)(uintptr_t)buf);
        if (EF_U16(0x0041868au) == 0u)
            guest_callptr2_x(EF_U32(0x00132a74u), 0x0018d670u, (uint32_t)(uintptr_t)buf);
        guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), (uint32_t)(uintptr_t)buf, 0x00417278u);
    }
    EF_U32(0x0041868cu) = 0x00417278u;
    guest_fn_a82f0_w(0x00418678u);
    EF_U8(0x001371fau) = 1u;
}

/* 0x80c3c, 226 bytes. Reset the 16 twelve-byte slots at 0x1fc8a0 (word +0 = 0xffff), then walk the 256 fifty-six-byte effect records at 0x1f90a0: clear byte +1; for each active record (byte +0) draw it with 0x9e0c4(signed word +2, X, Y, Z) where X, Y, Z are the floats at +0x24, +0x28, +0x2c truncated and shifted left 8. Unless byte 0x122c5c is set, age the record (dword +8) and deactivate it once the age passes 15. */
void draw_effect_records_80c3c(void)
{
    uint32_t i, rec;
    for (i = 0; i < 16u; i++)
        EF_U16(0x001fc8a0u + i * 12u) = 0xffffu;
    for (i = 0; i < 256u; i++) {
        int32_t x, y, z;
        rec = 0x001f90a0u + i * 0x38u;
        EF_U8(rec + 1u) = 0u;
        if (EF_U8(rec) == 0u)
            continue;
        z = (int32_t)truncate_9d72c((double)*(float *)ef_guest_memory_ptr(rec + 0x2cu));
        y = (int32_t)truncate_9d72c((double)*(float *)ef_guest_memory_ptr(rec + 0x28u));
        x = (int32_t)truncate_9d72c((double)*(float *)ef_guest_memory_ptr(rec + 0x24u));
        guest_fn_9e0c4((int32_t)EF_U32(rec) >> 16, (uint32_t)x << 8, (uint32_t)y << 8, (uint32_t)z << 8);
        if (EF_U8(0x00122c5cu) != 0u)
            continue;
        EF_U32(rec + 8u) = EF_U32(rec + 8u) + 1u;
        if ((int32_t)EF_U32(rec + 8u) > 0xf) {
            EF_U32(rec + 8u) = 0xfu;
            EF_U8(rec) = 0u;
        }
    }
}

/* 0x35cfc, 227 bytes. Prepare the briefing/mission screen: clear bytes 0x19f5aa and 0x19f5af, apply the key string 0x182f4c, refresh the status (0x34a68), counts (0x35bb8) and 0xa42c4, set byte 0x127da9, then open the data file named at 0x125828 into dword 0x19f364. When byte 0x125846 is 3 a random variant (0xa45b3 modulo 3, stored in word 0x125844) is chosen and the 12-byte name template at 0x183094 is copied in; otherwise the name is built by 0xa4380 from 0x182f7c, and a zero result skips opening. Open failures are reported through the pointer at 0x132a74. Finally clear byte 0x19f5a8. */
void prepare_briefing_file_35cfc(void)
{
    uint32_t i;
    EF_U8(0x0019f5aau) = 0u;
    EF_U8(0x0019f5afu) = 0u;
    guest_fn_a4d90(0x00182f4cu);
    show_status_34a68();
    copy_counts_35bb8();
    guest_fn_a42c4_x();
    EF_U8(0x00127da9u) = 1u;
    if (EF_U8(0x00125846u) == 3u) {
        uint32_t r = guest_fn_a45b3() & 0xffffu;
        EF_U16(0x00125844u) = (uint16_t)(r % 3u);
        for (i = 0; i < 12u; i++)
            EF_U8(0x00125828u + i) = EF_U8(0x00183094u + i);
        EF_U32(0x0019f364u) = guest_fn_a7b3c(0x00125828u);
        if (EF_U32(0x0019f364u) == 0u)
            guest_callptr2_x(EF_U32(0x00132a74u), 0x001830a0u, 0x00125828u);
    } else if (guest_fn_a4380(0x00125828u, 0x00182f7cu) == 0u) {
        EF_U32(0x0019f364u) = guest_fn_a7b3c(0x00125828u);
        if (EF_U32(0x0019f364u) == 0u)
            guest_callptr2_x(EF_U32(0x00132a74u), 0x001830ccu, 0x00125828u);
    } else {
        EF_U32(0x0019f364u) = 0u;
    }
    EF_U8(0x0019f5a8u) = 0u;
}

/* 0x39874, 227 bytes. Proximity level of the point (X = EAX word, Y = EDX word signed, Z = EBX word) to the reference position (dwords 0x19e5b5, 0x19e5b9, 0x19e5bd, each >> 8): take the largest axis distance (capped at 250), turn it into a percentage 100*(250-d)/250 (low byte, capped at 100) and raise the maximum in word 0x12651c to it. */
void update_proximity_39874(uint32_t x, uint32_t y, uint32_t z)
{
    uint32_t rx = (uint32_t)((int32_t)EF_U32(0x0019e5b5u) >> 8);
    uint32_t ry = (uint32_t)((int32_t)EF_U32(0x0019e5b9u) >> 8);
    uint32_t rz = (uint32_t)((int32_t)EF_U32(0x0019e5bdu) >> 8);
    uint32_t best = abs_a28f2((int32_t)((x & 0xffffu) - (rx & 0xffffu)));
    int32_t e, q;
    uint8_t al;
    if (abs_a28f2((int32_t)(int16_t)y - (int32_t)(ry & 0xffffu)) > (uint32_t)(int32_t)(int16_t)best)
        best = abs_a28f2((int32_t)(int16_t)y - (int32_t)(ry & 0xffffu));
    if (abs_a28f2((int32_t)((z & 0xffffu) - (rz & 0xffffu))) > (uint32_t)(int32_t)(int16_t)best)
        best = abs_a28f2((int32_t)((z & 0xffffu) - (rz & 0xffffu)));
    if ((int32_t)(int16_t)best > 0xfa)
        best = 0xfau;
    e = 0xfa - (int32_t)(int16_t)best;
    q = (e * 100) / 0xfa;
    al = (uint8_t)q;
    if (al > 0x64u)
        al = 0x64u;
    if ((int16_t)al > (int16_t)EF_U16(0x0012651cu))
        EF_U16(0x0012651cu) = al;
}

/* 0x87fe4, 227 bytes. Draw the nearby entries of the linked list whose head is dword 0x20f160 (sentinel 0x20f164, next link at +0x34): with the metric origin (0x1a5be8/0x1a5bec/0x1a5bf0) temporarily set from 0x214450/0x21444c/0x214448, call 0x866d4 on each entry, measure its position (dwords +0, +4, +8, each >> 8) with 0x23760 and, when closer than 10000, call 0xa0ea8 (signed words +0x2e, +0xa.. high halves) and 0x9e0c4 (position << 8). The origin is restored afterwards. */
void draw_nearby_entries_87fe4(void)
{
    uint32_t save_a = EF_U32(0x001a5becu);
    uint32_t save_b = EF_U32(0x001a5bf0u);
    uint32_t save_c = EF_U32(0x001a5be8u);
    uint32_t e = EF_U32(0x0020f160u);
    uint32_t vec[3];
    EF_U32(0x001a5becu) = EF_U32(0x0021444cu);
    EF_U32(0x001a5bf0u) = EF_U32(0x00214448u);
    EF_U32(0x001a5be8u) = EF_U32(0x00214450u);
    for (;;) {
        e = EF_U32(e + 0x34u);
        if (e == EF_U32(0x0020f164u))
            break;
        nop_866d4();
        vec[0] = (uint32_t)((int32_t)EF_U32(e) >> 8);
        vec[1] = (uint32_t)((int32_t)EF_U32(e + 4u) >> 8);
        vec[2] = (uint32_t)((int32_t)EF_U32(e + 8u) >> 8);
        if (guest_fn_23760_x((uint32_t)(uintptr_t)vec) >= 10000)
            continue;
        guest_fn_a0ea8((int32_t)EF_U32(e + 0x2cu) >> 16, (uint32_t)((int32_t)EF_U32(e + 0xau) >> 16),
                       (int32_t)EF_U32(e + 0xcu) >> 16, (uint32_t)((int32_t)EF_U32(e + 0xeu) >> 16));
        guest_fn_9e0c4((int32_t)EF_U32(e + 0x2cu) >> 16, EF_U32(e) << 8, EF_U32(e + 4u) << 8, EF_U32(e + 8u) << 8);
    }
    EF_U32(0x001a5becu) = save_a;
    EF_U32(0x001a5bf0u) = save_b;
    EF_U32(0x001a5be8u) = save_c;
}

/* 0x76620, 228 bytes. Set up the view window for the current viewport record (28-byte records at 0x125ad6, index byte 0x12db9e; X = dword +0 high half, Y = dword +0xe high half, both shifted left by byte 0x12b369) over the screen size (width dword 0x15b744, height 0x15b748): set the view origin (0xdc21c), run the box update (0xdc71b), latch the vertical origin (0xce328) and the full-screen box (0xcdf48). */
void setup_viewport_window_76620(void)
{
    uint32_t rec = (uint32_t)EF_U8(0x0012db9eu) * 28u;
    uint32_t w = EF_U32(0x0015b744u);
    uint32_t sh = EF_U8(0x0012b369u);
    uint32_t x = (uint32_t)((int32_t)EF_U32(0x00125ad6u + rec) >> 16) << sh;
    uint32_t y = (uint32_t)((int32_t)EF_U32(0x00125ae4u + rec) >> 16) << sh;
    set_view_origin_dc21c(y, w - 1u, x, (uint32_t)((int32_t)w >> 1));
    guest_fn_dc71b_x(x, y, 0u, EF_U32(0x0015b744u) - 1u, 0u);
    latch_values_ce328(0u, EF_U32(0x0015b744u) - 1u, 0u, y);
    guest_fn_cdf48(0u, EF_U32(0x0015b744u) - 1u, 0u, EF_U32(0x0015b748u) - 1u);
}

/* 0x9dcf8, 228 bytes. Register the resource NAME (EAX) under the key string TEXT (EDX): obtain its data either from the cached chunk (0xea3ec on descriptor 0x16040c; size 0xea414, decode 0xea420) or by sizing it with the hook at 0x132ab4 (zero reports through 0x132a74) and loading it with 0x132ab8 into a fresh arena block; duplicate TEXT into an arena block, take the next slot (word 0x132b10, overflow at 0x7d0 reported), store the key hash (0xebfd0) at 0x412fc0, the key copy at 0x411080, the data at 0x40d200 and the end-of-data pointer (data + size - 2 when flag 2 of data byte +5 is set, else 0) at 0x414f00. Returns the slot number. */
uint32_t register_resource_9dcf8(uint32_t name, uint32_t text)
{
    uint32_t size, data, key, slot, off;
    if (refresh_cached_chunk_ea3ec(0x0016040cu) & 0xffu) {
        size = read_link_ea414();
        data = arena_take_e29e8(size);
        decode_chunk_ea420(data);
    } else {
        size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), name);
        if (size == 0u)
            guest_callptr2_x(EF_U32(0x00132a74u), 0x0018b2a8u, name);
        data = arena_take_e29e8(size);
        guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), name, data);
    }
    key = arena_take_e29e8((uint32_t)((int32_t)(int16_t)guest_fn_b1b8b(text) + 1));
    copy_string_9d749(key, text);
    slot = EF_U16(0x00132b10u);
    EF_U16(0x00132b10u) = (uint16_t)(slot + 1u);
    if ((int32_t)EF_U32(0x00132b0eu) >> 16 == 0x7d0)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018b2c0u);
    off = (uint32_t)(int32_t)(int16_t)slot << 2;
    EF_U32(0x00412fc0u + off) = name_hash_ebfd0(key);
    EF_U32(0x00411080u + off) = key;
    EF_U32(0x0040d200u + off) = data;
    EF_U32(0x00414f00u + off) = 0u;
    if (EF_U8(data + 5u) & 2u)
        EF_U32(0x00414f00u + off) = size + data - 2u;
    return slot;
}

/* 0x16018a, 203 bytes. Implicit-global fragment: append one entry to the table set indexed by word 0x146e52: bytes 0x149e58 (byte 0x1600b8), 0x149658 (byte at pointer 0x1600ac), 0x14a658 (0); dwords 0x150e58/0x152e58/0x154e58 (the three dwords at pointer 0x1600b0), 0x156e58 (pointer 0x1600ac); words 0x14ae58 (low word of dword 0x1600a8), 0x14fe58 (absolute value of the high word of the third dword), 0x14be58/0x14ce58/0x14de58 (three words at pointer 0x1600b4) and 0x14ee58 (low word of dword 0x1600bc). Then run the insertion routine 0xc552e when all three words are zero, else 0xc565e, with ESI = the index. */
void append_table_entry_16018a(void)
{
    uint32_t src = EF_U32(0x001600b0u);
    uint32_t d0 = EF_U32(src), d1 = EF_U32(src + 4u), d2 = EF_U32(src + 8u);
    uint32_t idx = EF_U16(0x00146e52u);
    uint32_t wp = EF_U32(0x001600b4u);
    uint16_t w0 = EF_U16(wp), w1 = EF_U16(wp + 2u), w2 = EF_U16(wp + 4u);
    int16_t hi = (int16_t)(d2 >> 16);
    EF_U8(0x00149e58u + idx) = EF_U8(0x001600b8u);
    EF_U8(0x00149658u + idx) = EF_U8(EF_U32(0x001600acu));
    EF_U8(0x0014a658u + idx) = 0u;
    EF_U32(0x00150e58u + idx * 4u) = d0;
    EF_U32(0x00152e58u + idx * 4u) = d1;
    EF_U32(0x00154e58u + idx * 4u) = d2;
    EF_U32(0x00156e58u + idx * 4u) = EF_U32(0x001600acu);
    EF_U16(0x0014ae58u + idx * 2u) = (uint16_t)EF_U32(0x001600a8u);
    EF_U16(0x0014fe58u + idx * 2u) = (uint16_t)(hi < 0 ? -hi : hi);
    EF_U16(0x0014be58u + idx * 2u) = w0;
    EF_U16(0x0014ce58u + idx * 2u) = w1;
    EF_U16(0x0014de58u + idx * 2u) = w2;
    EF_U16(0x0014ee58u + idx * 2u) = (uint16_t)EF_U32(0x001600bcu);
    if ((uint16_t)(w0 | w1 | w2) == 0u)
        guest_fn_c552e_x(EF_U16(0x00146e52u));
    else
        guest_fn_c565e_x(EF_U16(0x00146e52u));
}

/* 0xebee4, 231 bytes. Stack (ret 0xc): scale the double X by 2^N-style factors from a table. The special values at 0x195bfc / 0x195c04 are first replaced by the constants at 0x195c0c / 0x195c14; then for |N| above 0x134 X is multiplied (N >= 0) or divided (N < 0) by the double at 0x195c62 and the count reduced by 0xd8; the remainder is consumed greedily against the (word count, double factor) 10-byte table at 0x195c60 and X is multiplied (N >= 0) or divided (N < 0) by the product. Returns X in ST0. */
double scale_by_table_ebee4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double x, int32_t n)
{
#define EF_D(a) (*(double *)ef_guest_memory_ptr(a))
    uint32_t p = 0x00195c60u;
    int32_t e;
    double f = 1.0;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (x == EF_D(0x00195bfcu))
        x = EF_D(0x00195c0cu);
    else if (x == EF_D(0x00195c04u))
        x = EF_D(0x00195c14u);
    if (n < 0) {
        e = -n;
        if (e > 0x134) {
            x = x / EF_D(0x00195c62u);
            e -= 0xd8;
        }
    } else {
        e = n;
        if (n > 0x134) {
            x = x * EF_D(0x00195c62u);
            e = n - 0xd8;
        }
    }
    for (;;) {
        int32_t cnt = (int32_t)(int16_t)EF_U16(p);
        if (e >= cnt) {
            f = f * EF_D(p + 2u);
            e -= cnt;
        }
        if (e == 0)
            break;
        if (EF_U16(p) != 1u)
            p += 10u;
    }
    if (n < 0)
        x = x / f;
    else
        x = x * f;
    return x;
#undef EF_D
}

/* 0x6eaf4, 200 bytes. Stack (ret 8): clamp the double at 0x1f7110 to +-the limit at 0x1f7098, take its difference to the current object's field +0x71 (object pointer 0x1f71bc), wrap it with the constants 0x1866cc/0x1866d4 and 0x1866dc/0x1866e4 into 0x1f7090 and store (difference * ARG * 0x1866ec * 0x1f70b8) in 0x1f7080. */
void compute_turn_rate_6eaf4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double arg)
{
#define EF_D(a) (*(double *)ef_guest_memory_ptr(a))
    double t;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_D(0x001f7110u) > EF_D(0x001f7098u))
        EF_D(0x001f7110u) = EF_D(0x001f7098u);
    if (!(EF_D(0x001f7110u) >= -EF_D(0x001f7098u)))
        EF_D(0x001f7110u) = -EF_D(0x001f7098u);
    t = EF_D(0x001f7110u) - EF_D(EF_U32(0x001f71bcu) + 0x71u);
    EF_D(0x001f7090u) = t;
    if (EF_D(0x001f7090u) > EF_D(0x001866ccu))
        EF_D(0x001f7090u) = EF_D(0x001f7090u) + EF_D(0x001866d4u);
    if (EF_D(0x001f7090u) < EF_D(0x001866dcu))
        EF_D(0x001f7090u) = EF_D(0x001f7090u) + EF_D(0x001866e4u);
    EF_D(0x001f7080u) = EF_D(0x001f7090u) * arg * EF_D(0x001866ecu) * EF_D(0x001f70b8u);
#undef EF_D
}

/* 0x6c1f0, 178 bytes. Stack (ret 8): rational estimate of a coefficient from the double X. R = (X + k) / (k - X) * 1.225 with k = double 0x18641c (X is forced to 0 first when it is positive); when X is below the limit 0x186424 the polynomial term u*u*c*0x186444 with u = -X * 0x18642c + 0x186434 and c = 0x18643c is added, and a result below c is replaced by the constant 0.0055. Returns R in ST0. */
double estimate_coefficient_6c1f0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double x)
{
#define EF_D(a) (*(double *)ef_guest_memory_ptr(a))
    double r, u, c;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (0.0 < x)
        x = 0.0;
    r = (x + EF_D(0x0018641cu)) / (EF_D(0x0018641cu) - x) * 1.225;
    if (x < EF_D(0x00186424u)) {
        u = -x * EF_D(0x0018642cu) + EF_D(0x00186434u);
        c = EF_D(0x0018643cu);
        r = r + u * (u * c) * EF_D(0x00186444u);
        if (!(r >= c))
            r = 0.0055;
    }
    return r;
#undef EF_D
}

/* 0xe7d27, 194 bytes. strstr(HAY = EAX, NEEDLE = EDX): the first occurrence of NEEDLE in HAY, or 0. An empty needle returns HAY; a one-character needle is a plain character scan (the terminator never matches). */
uint32_t strstr_e7d27(uint32_t hay, uint32_t needle)
{
    uint32_t n, end, p, i;
    uint8_t c0 = EF_U8(needle);
    if (c0 == 0u)
        return hay;
    if (EF_U8(needle + 1u) == 0u) {
        for (p = hay;; p++) {
            uint8_t c = EF_U8(p);
            if (c == c0)
                return p;
            if (c == 0u)
                return 0u;
        }
    }
    for (end = hay; EF_U8(end) != 0u; end++)
        ;
    for (n = 0; EF_U8(needle + n) != 0u; n++)
        ;
    p = hay;
    for (;;) {
        uint32_t left = end - p;
        if ((int32_t)left < (int32_t)n)
            return 0u;
        for (i = 0; i < left && EF_U8(p + i) != c0; i++)
            ;
        if (i >= left)
            return 0u;
        p += i;
        for (i = 0; i < n && EF_U8(p + i) == EF_U8(needle + i); i++)
            ;
        if (i == n)
            return p;
        p++;
    }
}

/* 0xd4510, 197 bytes. Fill one horizontal span with the colour byte 0x159138: from the signed word 0x485fea to the signed word 0x485fd2 (inclusive; nothing when the end is before the start) on the row signed word 0x485fee, whose start offset is the dword at 0x163200[row] plus the frame base dword 0x15b754. The original uses aligned dword stores and a computed jump for short spans; the stored bytes are the same. */
void fill_span_d4510(void)
{
    int32_t x0 = (int16_t)EF_U16(0x00485feau);
    int32_t n = (int32_t)(int16_t)EF_U16(0x00485fd2u) - x0;
    uint32_t dst, i;
    uint8_t col = EF_U8(0x00159138u);
    if (n < 0)
        return;
    n++;
    dst = EF_U32(0x00163200u + (uint32_t)(int32_t)(int16_t)EF_U16(0x00485feeu) * 4u) + EF_U32(0x0015b754u) + (uint32_t)x0;
    for (i = 0; i < (uint32_t)n; i++)
        EF_U8(dst + i) = col;
}

/* 0xf10ca, 195 bytes. Fill the broken-down time record TM (EAX) from DOS: dwords +0 seconds, +4 minutes, +8 hours, +0xc day, +0x10 month - 1, +0x14 year - 1900, +0x20 = -1 (DST unknown). The date (INT 21h/2Ah) is read before and after the time (2Ch); when the day rolled over meanwhile and the hour is not 23, the later date is used. Returns the hundredths of a second times 10. */
uint32_t dos_get_time_f10ca(uint32_t tm)
{
    uint32_t r[7];
    uint32_t d0, d1, t;
    run_dos_int_ex(0x21u, 0x2a00u, 0u, 0u, 0u, r);
    d0 = ((((r[2] & 0xffffu) - 0x76cu) & 0xffu) << 16) | (((r[0] & 0xffu)) << 24) | (r[3] & 0xffffu);
    EF_U32(tm + 0x14u) = (d0 >> 16) & 0xffu;
    EF_U32(tm + 0x10u) = ((d0 >> 8) & 0xffu) - 1u;
    EF_U32(tm + 0xcu) = d0 & 0xffu;
    run_dos_int_ex(0x21u, 0x2c00u, 0u, 0u, 0u, r);
    t = ((r[2] & 0xffffu) << 16) | (r[3] & 0xffffu);
    EF_U32(tm + 8u) = t >> 24;
    EF_U32(tm + 4u) = (t >> 16) & 0xffu;
    EF_U32(tm) = (t >> 8) & 0xffu;
    run_dos_int_ex(0x21u, 0x2a00u, 0u, 0u, 0u, r);
    d1 = ((((r[2] & 0xffffu) - 0x76cu) & 0xffu) << 16) | (((r[0] & 0xffu)) << 24) | (r[3] & 0xffffu);
    if ((d1 & 0xffu) != (d0 & 0xffu) && EF_U32(tm + 8u) != 0x17u) {
        EF_U32(tm + 0x14u) = (d1 >> 16) & 0xffu;
        EF_U32(tm + 0x10u) = ((d1 >> 8) & 0xffu) - 1u;
        EF_U32(tm + 0xcu) = d1 & 0xffu;
    }
    EF_U32(tm + 0x20u) = 0xffffffffu;
    return (t & 0xffu) * 10u;
}

/* 0xe7ea4, 177 bytes. Build the 256-entry brightness ramp at 0x463f60 for the range LO..HI (EAX, EDX; LO >= 0 and HI < 256, else return 0): entries 1..LO are 0, entries HI+1..0x100 are 0x3f, and entry LO+1+k (k = 0 .. HI-LO-1) is the truncation of k divided by the float (HI - LO) * double 0x190c54. Returns 1. */
uint32_t build_ramp_table_e7ea4(int32_t lo, int32_t hi)
{
    float step;
    int32_t i, k;
    if (lo < 0 || hi >= 0x100)
        return 0u;
    step = (float)((double)(hi - lo) * *(double *)ef_guest_memory_ptr(0x00190c54u));
    for (i = 1; lo > 0 && i <= lo; i++)
        EF_U8(0x00463f5fu + (uint32_t)i) = 0u;
    for (i = hi + 1; i <= 0x100; i++)
        EF_U8(0x00463f5fu + (uint32_t)i) = 0x3fu;
    for (k = 0, i = lo + 1; lo < hi && i <= hi; i++, k++)
        EF_U8(0x00463f5fu + (uint32_t)i) = (uint8_t)(int32_t)truncate_9d72c((double)k / (double)step);
    return 1u;
}

/* 0xfe560, 174 bytes. High-resolution timer read. The first call (dword 0x17cf98 clear) points 0x485fc4 at the BIOS tick counter (0x120cc6 + 0x46c), programs PIT channel 0 for a full 16-bit count unless byte 0x15bac5 is set, records the tick in 0x485fc8, sets the flag and returns 0. Later calls latch the PIT count (ports 0x43/0x40, re-read until the tick is stable) and store ((ticks since 0x485fc8) : -(count)) << 16 divided by 0x5d380000 in 0x485fc0, also returning it. */
uint32_t read_hires_timer_fe560(void)
{
    uint32_t tick1, tick2, lo, hi, count;
    if (EF_U32(0x0017cf98u) == 0u) {
        EF_U32(0x00485fc4u) = EF_U32(0x00120cc6u) + 0x46cu;
        if (EF_U8(0x0015bac5u) == 0u) {
            guest_fn_117b0c(0x43u, 0x34u);
            guest_fn_117b0c(0x40u, 0u);
            guest_fn_117b0c(0x40u, 0u);
        }
        EF_U32(0x0017cf98u) = 1u;
        EF_U32(0x00485fc8u) = EF_U32(EF_U32(0x00485fc4u));
        return 0u;
    }
    do {
        tick1 = EF_U32(EF_U32(0x00485fc4u));
        guest_fn_117b0c(0x43u, 0u);
        lo = guest_fn_9c430(0x40u) & 0xffu;
        hi = guest_fn_9c430(0x40u) & 0xffu;
        tick2 = EF_U32(EF_U32(0x00485fc4u));
    } while (tick2 != tick1);
    count = (0u - ((hi << 8) | lo)) & 0xffffu;
    {
        int64_t num = (int64_t)(((uint64_t)(tick2 - EF_U32(0x00485fc8u)) << 32) | (uint64_t)(count << 16));
        EF_U32(0x00485fc0u) = (uint32_t)(int32_t)(num / (int64_t)(int32_t)0x5d380000);
    }
    return EF_U32(0x00485fc0u);
}

/* 0xd6cac, 176 bytes. When bit 0 of byte 0x15bac0 is set, store 1 - clamp(r, 0, 1) as a float at OUT (EAX), r being the float ratio dword 0x420db0 / dword 0x420db4, and return 1; otherwise return 0. */
uint8_t store_inverse_ratio_d6cac(uint32_t out)
{
    float r;
    double v, t;
    if (!(EF_U8(0x0015bac0u) & 1u))
        return 0u;
    r = (float)((double)(int32_t)EF_U32(0x00420db0u) / (double)(int32_t)EF_U32(0x00420db4u));
    v = (0.0 < (double)r) ? (double)r : 0.0;
    if (1.0 > v)
        t = (0.0 < (double)r) ? (double)r : 0.0;
    else
        t = 1.0;
    *(float *)ef_guest_memory_ptr(out) = (float)(1.0 - t);
    return 1u;
}

/* 0xd08fc, 160 bytes. 3x3 fixed-point matrix product OUT = A * B (OUT = EAX, A = EDX, B = EBX; nine dwords each, row major, results arithmetically shifted right by 15). The product is built in a temporary so OUT may alias an input. */
void matrix_multiply_d08fc(uint32_t out, uint32_t a, uint32_t b)
{
    int32_t t[9];
    uint32_t r, c;
    for (r = 0; r < 3u; r++) {
        for (c = 0; c < 3u; c++) {
            int32_t s = (int32_t)(EF_U32(a + r * 12u) * EF_U32(b + c * 4u))
                      + (int32_t)(EF_U32(a + r * 12u + 4u) * EF_U32(b + 12u + c * 4u))
                      + (int32_t)(EF_U32(a + r * 12u + 8u) * EF_U32(b + 24u + c * 4u));
            t[r * 3u + c] = s >> 15;
        }
    }
    for (r = 0; r < 9u; r++)
        EF_U32(out + r * 4u) = (uint32_t)t[r];
}

/* 0x109a80, 151 bytes. Normalise the double globals: with k = 1 / double 0x433050, 0x432378/0x432380/0x432388 = 0x432a90/98/a0 * k and 0x432360/0x432368/0x432370 = 0x432a78/80/88 * k; then 0x432398 = 0x432b08 / 0x432f30, 0x4323a0 = 0x432b10 / 0x432ce0 and 0x4323a8 = 0x432b18 / 0x432d38. */
void normalise_globals_109a80(void)
{
#define EF_D(a) (*(double *)ef_guest_memory_ptr(a))
    double k = 1.0 / EF_D(0x00433050u);
    EF_D(0x00432378u) = EF_D(0x00432a90u) * k;
    EF_D(0x00432380u) = EF_D(0x00432a98u) * k;
    EF_D(0x00432388u) = EF_D(0x00432aa0u) * k;
    EF_D(0x00432360u) = EF_D(0x00432a78u) * k;
    EF_D(0x00432368u) = EF_D(0x00432a80u) * k;
    EF_D(0x00432370u) = EF_D(0x00432a88u) * k;
    EF_D(0x00432398u) = EF_D(0x00432b08u) / EF_D(0x00432f30u);
    EF_D(0x004323a0u) = EF_D(0x00432b10u) / EF_D(0x00432ce0u);
    EF_D(0x004323a8u) = EF_D(0x00432b18u) / EF_D(0x00432d38u);
#undef EF_D
}

/* 0xd4430, 117 bytes. Draw one dithered shaded span: from the signed word 0x485fea to the signed word 0x485fd2 (nothing unless the end is after the start) on the row signed word 0x485fee, stepping the shade value from dword 0x485ff8 towards dword 0x485fe0 (signed step = difference / length), fetching each pixel from the shade table whose base is dword 0x159084 at (dither + shade) >> 16. The dither word comes from 0x486014[(row * 2) ^ x0] and is advanced with the add/adc pair on its two bytes. The span is length + 1 pixels long. */
void draw_dithered_span_d4430(void)
{
    int32_t x0 = (int16_t)EF_U16(0x00485feau);
    int32_t n = (int32_t)(int16_t)EF_U16(0x00485fd2u) - x0;
    int32_t row, step;
    uint32_t dst, end, v, base;
    uint8_t cl, ch, c;
    if (n <= 0)
        return;
    row = (int16_t)EF_U16(0x00485feeu);
    dst = EF_U32(0x00163200u + (uint32_t)row * 4u) + EF_U32(0x0015b754u) + (uint32_t)x0;
    step = ((int32_t)EF_U32(0x00485fe0u) - (int32_t)EF_U32(0x00485ff8u)) / n;
    v = EF_U32(0x00485ff8u);
    end = dst + (uint32_t)n;
    {
        uint32_t d = EF_U16(0x00486014u + ((((uint32_t)row * 2u) ^ (uint32_t)x0)));
        cl = (uint8_t)d;
        ch = (uint8_t)(d >> 8);
    }
    base = EF_U32(0x00159084u);
    do {
        uint32_t idx = (((uint32_t)ch << 8 | cl) + v) >> 16;
        v += (uint32_t)step;
        EF_U8(dst) = EF_U8(base + idx);
        c = (uint8_t)(((uint32_t)ch + cl) > 0xffu);
        ch = (uint8_t)(ch + cl);
        dst++;
        cl = (uint8_t)(cl + ch + c);
    } while (dst <= end);
}

/* 0xf0034, 127 bytes. Convert the 80-bit extended value at PTR (EAX) to the bits of a 32-bit float (returned in EAX): round the mantissa to 24 bits (ties to even), re-bias the exponent by 0x3f80, saturate to infinity above 0xfe, flush to 0 below the float range and keep the sign. ORIGINAL BUG: a zero exponent (zero or denormal) returns the masked high mantissa dword without the sign. */
uint32_t extended_to_float_f0034(uint32_t ptr)
{
    uint32_t mask = 0xffffff00u;
    uint32_t hi = EF_U32(ptr + 4u);
    uint32_t lo = EF_U32(ptr);
    uint16_t se = EF_U16(ptr + 8u);
    uint32_t eax = hi;
    uint32_t sign;
    int16_t e;
    if (hi & 0x80u) {
        if ((hi & 0x7fu) == 0u && lo == 0u)
            mask += mask;
        eax = hi + 0x100u;
        if (eax < hi) {
            eax = 0x80000000u;
            se = (uint16_t)(se + 1u);
        }
    }
    eax &= mask;
    sign = (uint32_t)(se >> 15) << 31;
    if ((se & 0x7fffu) == 0u)
        return eax;
    if ((se & 0x7fffu) == 0x7fffu)
        return sign | ((((eax + eax) >> 8) | 0xff000000u) >> 1);
    e = (int16_t)((se & 0x7fffu) + 0xc080u);
    if (e < 0)
        return 0u;
    if (e >= 0xff)
        return sign | (0xff000000u >> 1);
    eax = eax + eax;
    eax = (eax >> 8) | ((uint32_t)(uint16_t)e << 24);
    return sign | (eax >> 1);
}

/* 0x118b10, 76 bytes. Convert the double whose bits are EDX:EAX (EAX = low dword, EDX = high dword) to the bits of a float (returned in EAX): rounds at the 2^29 bit, overflow gives infinity with the sign, a zero or too-small exponent gives plain 0 (sign dropped). */
uint32_t double_bits_to_float_118b10(uint32_t lo, uint32_t hi)
{
    uint64_t v;
    uint32_t sign;
    if ((hi & 0x7ff00000u) == 0u)
        return 0u;
    sign = hi & 0x80000000u;
    v = (((uint64_t)hi << 32) | lo) << 1;
    v += 0x20000000u;
    hi = (uint32_t)(v >> 32);
    lo = (uint32_t)v;
    if (hi == 0u || hi >= 0x8fe00000u)
        return 0x7f800000u | sign;
    if (hi < 0x70200000u)
        return 0u;
    hi -= 0x70000000u;
    v = (((uint64_t)hi << 32) | lo) << 2;
    return (uint32_t)(v >> 32) | sign;
}

/* 0xd44b0, 84 bytes. Draw one shaded span: from the signed word 0x485fea to the signed word 0x485fd2 (nothing unless the end is after the start) on the row signed word 0x485fee, stepping the shade value from dword 0x485ff8 towards dword 0x485fe0 (signed step = difference / length) and fetching each of the length + 1 pixels from the shade table at dword 0x159084 indexed by the shade value >> 16. */
void draw_shaded_span_d44b0(void)
{
    int32_t x0 = (int16_t)EF_U16(0x00485feau);
    int32_t n = (int32_t)(int16_t)EF_U16(0x00485fd2u) - x0;
    int32_t step, k;
    uint32_t v, dst, base;
    if (n <= 0)
        return;
    v = EF_U32(0x00485ff8u);
    step = ((int32_t)EF_U32(0x00485fe0u) - (int32_t)v) / n;
    dst = EF_U32(0x00163200u + (uint32_t)(int32_t)(int16_t)EF_U16(0x00485feeu) * 4u) + EF_U32(0x0015b754u) + (uint32_t)x0;
    base = EF_U32(0x00159084u);
    for (k = n; k >= 0; k--) {
        EF_U8(dst) = EF_U8(base + (v >> 16));
        v += (uint32_t)step;
        dst++;
    }
}

/* 0xc46f8, 67 bytes. Implicit-register fragment (EAX = MARGIN, EBX = vertex record pointer; params in that order). Remember the record in dword 0x158f40; when its dword +8 is at least 0x10000 and the point (dwords +0, +4, +8) is not inside the view volume (0xceb40 with MARGIN), project it (0xfc8cc, count 1, list 0x158f40) and report whether bit 0 of the word at +0x20 of the (reloaded) record is set. The original returns this in the zero flag: the result here is 1 for "flag set" (NZ) and 0 otherwise. */
uint32_t project_if_outside_c46f8(uint32_t margin, uint32_t rec)
{
    EF_U32(0x00158f40u) = rec;
    if ((int32_t)EF_U32(rec + 8u) >= 0x10000) {
        if ((point_in_view_volume_ceb40(0u, 0u, 0u, 0u, (int32_t)margin, (int32_t)EF_U32(rec + 8u),
                                        (int32_t)EF_U32(rec + 4u), (int32_t)EF_U32(rec)) & 0xffu) == 0u) {
            project_vertices_fc8cc(0u, 0u, 0u, 0u, 1u, 0x00158f40u);
            return (EF_U16(EF_U32(0x00158f40u) + 0x20u) & 1u) != 0u;
        }
    }
    return 0u;
}

/* 0x103e0c, 79 bytes. cdecl (OBJ): one-shot guarded notification. When the guard dword 0x17d2d0 is clear, set it, take the handle from 0x101f70 (stored in 0x4dd7c8), send message 0x302 to OBJ (0x101f88), release the handle with 0x101f78 and clear the guard again. */
void send_guarded_message_103e0c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t obj)
{
    uint32_t h;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0017d2d0u) != 0u)
        return;
    EF_U32(0x0017d2d0u) = 1u;
    h = tail_jump_101f70(0u); /* EAX is not set by the original; the callee ignores it */
    EF_U32(0x004dd7c8u) = h;
    guest_fn_101f88(obj, 0x302u, 0u, 0u);
    forward_to_1111e5_101f78(0u, 0u, 0u, 0u, EF_U32(0x004dd7c8u));
    EF_U32(0x0017d2d0u) = 0u;
}

/* 0x10dc1d, 30 bytes. Implicit-register fragment (ESI = data pointer, ECX = byte count; params in that order): the two-byte running checksum used by the compressed streams. Per byte: BL += byte; BH += BL. Returns BX (zero-extended). ORIGINAL BUG: a count of 0 or 1 makes the LOOP instruction run 2^32 times; not modelled, the word loop is skipped. */
uint32_t checksum_bytes_10dc1d(uint32_t p, uint32_t count)
{
    uint8_t bl = 0u, bh = 0u;
    uint32_t words = count >> 1, i;
    for (i = 0; i < words; i++) {
        uint8_t al = EF_U8(p + i * 2u), ah = EF_U8(p + i * 2u + 1u);
        bl = (uint8_t)(bl + al);
        bh = (uint8_t)(bh + bl);
        bl = (uint8_t)(bl + ah);
        bh = (uint8_t)(bh + bl);
    }
    if (count & 1u) {
        bl = (uint8_t)(bl + EF_U8(p + words * 2u));
        bh = (uint8_t)(bh + bl);
    }
    return ((uint32_t)bh << 8) | bl;
}

/* 0xded60, 38 bytes. Implicit-register fragment (ESI = first buffer, EDI = second buffer, ECX = byte count; params in that order): swap COUNT bytes between the two buffers, dword-wise first, then the remaining bytes. */
void swap_buffers_ded60(uint32_t a, uint32_t b, uint32_t count)
{
    uint32_t i, n4 = count >> 2;
    for (i = 0; i < n4; i++) {
        uint32_t t = EF_U32(b + i * 4u);
        EF_U32(b + i * 4u) = EF_U32(a + i * 4u);
        EF_U32(a + i * 4u) = t;
    }
    for (i = 0; i < (count & 3u); i++) {
        uint8_t t = EF_U8(b + n4 * 4u + i);
        EF_U8(b + n4 * 4u + i) = EF_U8(a + n4 * 4u + i);
        EF_U8(a + n4 * 4u + i) = t;
    }
}

/* 0xfc39c, 48 bytes. Stack (ret 8): fill the row-offset table at 0x163200 with COUNT dwords 0, STEP, 2*STEP, ...; store COUNT*STEP in 0x164600 and STEP in 0x1631f4, then append two more dwords equal to (COUNT-1)*STEP after the table. ORIGINAL BUG: a count of 0 makes the LOOP run 2^32 times; not modelled. */
void build_row_table_fc39c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t count, uint32_t step)
{
    uint32_t v = 0u, i, p = 0x00163200u;
    (void)u0; (void)u1; (void)u2; (void)u3;
    for (i = 0; i < count; i++) {
        EF_U32(p) = v;
        p += 4u;
        v += step;
    }
    EF_U32(0x00164600u) = v;
    EF_U32(0x001631f4u) = step;
    v -= step;
    EF_U32(p) = v;
    EF_U32(p + 4u) = v;
}

/* 0x117dd8, 50 bytes. Return the code address (EAX; EDX is set to the code segment) of the INDEX-th (0..3) handler 0x117d38, 0x117d5c, 0x117d80, 0x117da4, or 0 for any larger index. */
uint32_t handler_address_117dd8(uint32_t index)
{
    static const uint32_t tbl[4] = { 0x00117d38u, 0x00117d5cu, 0x00117d80u, 0x00117da4u };
    return index > 3u ? 0u : tbl[index];
}

/* 0xc89cc, 40 bytes. Implicit-register fragment (params EBX = source point, ESI = output vector, EDI = screen destination): ESI = EBX - camera position (dwords 0x15b590, 0x15b594, 0x15b598), then project the pair (0xc737c, ESI/EBX/EDI). Returns the advanced destination. */
uint32_t project_relative_point_c89cc(uint32_t src, uint32_t out, uint32_t dst)
{
    EF_U32(out) = EF_U32(src) - EF_U32(0x0015b590u);
    EF_U32(out + 4u) = EF_U32(src + 4u) - EF_U32(0x0015b594u);
    EF_U32(out + 8u) = EF_U32(src + 8u) - EF_U32(0x0015b598u);
    return project_point_pair_c737c(out, src, dst);
}

/* 0xc89f4, 40 bytes. As 0xc89cc but ESI = EBX + camera position. */
uint32_t project_offset_point_c89f4(uint32_t src, uint32_t out, uint32_t dst)
{
    EF_U32(out) = EF_U32(src) + EF_U32(0x0015b590u);
    EF_U32(out + 4u) = EF_U32(src + 4u) + EF_U32(0x0015b594u);
    EF_U32(out + 8u) = EF_U32(src + 8u) + EF_U32(0x0015b598u);
    return project_point_pair_c737c(out, src, dst);
}

/* 0xfa2e4, 55 bytes. Implicit-register fragment (ESI = data pointer, EDX = byte count; params in that order): the two-byte running checksum (BL += byte; BH += BL per byte) over COUNT bytes. Returns BX zero-extended. */
uint32_t checksum_bytes_fa2e4(uint32_t p, uint32_t count)
{
    uint8_t bl = 0u, bh = 0u;
    uint32_t i;
    for (i = 0; i < count; i++) {
        bl = (uint8_t)(bl + EF_U8(p + i));
        bh = (uint8_t)(bh + bl);
    }
    return ((uint32_t)bh << 8) | bl;
}

/* 0xc4654, 44 bytes. Stack (ret 0xc): store the three arguments in dwords 0x1590b0 (first), 0x1590ac (second), 0x1590a8 (third) and run 0xe7a7c. */
void set_params_and_run_c4654(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a, uint32_t b, uint32_t c)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    EF_U32(0x001590a8u) = c;
    EF_U32(0x001590acu) = b;
    EF_U32(0x001590b0u) = a;
    guest_fn_e7a7c_x();
}

/* 0xc473c, 15 bytes. Implicit-register fragment (EAX = MARGIN word, ESI = x, EDI = y, EBP = z; params in that order): run the view-volume test 0xceb40 on the point and return its result halved. */
uint32_t view_volume_half_c473c(uint32_t margin, uint32_t x, uint32_t y, uint32_t z)
{
    return point_in_view_volume_ceb40(0u, 0u, 0u, 0u, (int32_t)(margin & 0xffffu), (int32_t)z, (int32_t)y, (int32_t)x) >> 1;
}

/* 0x10e08d, 17 bytes. Implicit-register fragment (ESI = source words, EDI = destination, ECX = count; params in that order): split each source word into its low byte at DEST and its high byte at DEST + 4, advancing the destination by 8 per word. */
void split_words_10e08d(uint32_t src, uint32_t dst, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        EF_U8(dst + i * 8u) = EF_U8(src + i * 2u);
        EF_U8(dst + i * 8u + 4u) = EF_U8(src + i * 2u + 1u);
    }
}

/* 0x26670, 133 bytes. Set a target position (X = EDX, Y = EBX, Z = ECX) on the scene object INDEX (EAX, signed word, looked up with 0x8ffe8): remember it in dwords 0x19b388/0x19b38c/0x19b390 and in the object (+0x54, +0x58, +0x5c), store the offsets from the object position (X - dword +0, Z - dword +8) in 0x19b380/0x19b384 and, for objects whose byte +0x3b is clear, the heading data (dword +0x12 in 0x19b394, word +0xe in 0x19b39a and its sine/cosine table entries in 0x19b398/0x19b39c). Finally clear the part flags (0x266f8). */
void set_object_target_26670(uint32_t index, uint32_t x, uint32_t y, uint32_t z)
{
    uint32_t obj = guest_fn_8ffe8((int32_t)(int16_t)index);
    if (obj == 0u)
        return;
    EF_U32(0x0019b388u) = x;
    EF_U32(0x0019b38cu) = y;
    EF_U32(obj + 0x58u) = y;
    EF_U32(0x0019b390u) = z;
    EF_U32(obj + 0x5cu) = z;
    EF_U32(obj + 0x54u) = x;
    EF_U32(0x0019b380u) = x - EF_U32(obj);
    EF_U32(0x0019b384u) = z - EF_U32(obj + 8u);
    if (EF_U8(obj + 0x3bu) == 0u) {
        uint32_t heading = EF_U16(obj + 0xeu);
        EF_U32(0x0019b394u) = EF_U32(obj + 0x12u);
        EF_U16(0x0019b39au) = (uint16_t)heading;
        EF_U16(0x0019b398u) = EF_U16(0x00141e08u + (heading >> 3) * 2u);
        EF_U16(0x0019b39cu) = EF_U16(0x00142e08u + (heading >> 3) * 2u);
    }
    clear_part_flags_266f8(obj);
}

/* 0xcc6c0, 132 bytes. cdecl, variadic (SIZE, FMT, ...): allocate SIZE bytes (0xc3f70); when that fails report through the pointer at 0x132a74 either the formatted message FMT (with the following arguments) or, when FMT is 0, the default text at 0x18f408; then zero the SIZE bytes at the returned pointer (0xb3fb0) and return it. */
uint32_t alloc_zeroed_cc6c0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t size, uint32_t fmt, ...)
{
    char msg[0x84];
    uint32_t p, i;
    va_list ap;
    (void)u0; (void)u1; (void)u2; (void)u3;
    p = guest_fn_c3f70(size);
    if (p == 0u) {
        if (fmt != 0u) {
            va_start(ap, fmt);
            format_to_buffer_e3022((uint32_t)(uintptr_t)msg, fmt, (uint32_t)(uintptr_t)&ap[0]);
            va_end(ap);
        } else {
            for (i = 0; i < 17u; i++)
                msg[i] = (char)EF_U8(0x0018f408u + i);
        }
        guest_callptr1_x(EF_U32(0x00132a74u), (uint32_t)(uintptr_t)msg);
    }
    fill_bytes_b3fb0(p, 0u, size);
    return p;
}

/* 0x112808, 132 bytes. cdecl (OBJ): flush the 32 pending slot dwords at OBJ + 0x558 (one per 4 bytes of OBJ): every slot not equal to -1 is posted (0x1123a4 with OBJ, slot | 0x80, the matching dword at +0x5d8, 0, 0) and reset to -1; afterwards dword +0x554 is cleared, 0x111df0 is told about the object's first dword and, when something was posted and 0x102020 returns 0, trace code 3 is emitted. */
void flush_pending_slots_112808(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t obj)
{
    uint32_t s, posted = 0u;
    (void)u0; (void)u1; (void)u2; (void)u3;
    for (s = obj; s != obj + 0x80u; s += 4u) {
        uint32_t v = EF_U32(s + 0x558u);
        if (v == 0xffffffffu)
            continue;
        guest_callptr5_x(0x001123a4u, obj, v | 0x80u, EF_U32(s + 0x5d8u), 0u, 0u);
        posted++;
        EF_U32(s + 0x558u) = 0xffffffffu;
    }
    EF_U32(obj + 0x554u) = 0u;
    send_pending_count_message_111df0(0u, 0u, 0u, 0u, EF_U32(obj));
    if (posted != 0u && tail_jump_102020(0u) == 0u)
        trace_call_101ff4(3u);
}

/* 0xf4640, 138 bytes. Read the next significant script line into the buffer at 0x4859e0: leading spaces/tabs are skipped, characters are stored up to a NUL or newline (a line longer than 0x400 reports 0x191c38 through the pointer at 0x132a74), the line counter dword 0x161ea8 is advanced, and comment lines (first byte ';') and empty lines that did not end the input are skipped. Word 0x161eac is cleared first. */
void read_script_line_f4640(void)
{
    uint32_t idx;
    uint8_t c, first;
    EF_U16(0x00161eacu) = 0u;
    for (;;) {
        idx = 0u;
        c = read_buffered_char_f45d8();
        while (c == 0x20u || c == 0x9u)
            c = read_buffered_char_f45d8();
        while (c != 0u && c != 0xau) {
            if ((int32_t)idx >= 0x400)
                guest_callptr1_x(EF_U32(0x00132a74u), 0x00191c38u);
            EF_U8(0x004859e0u + idx) = c;
            idx++;
            c = read_buffered_char_f45d8();
        }
        EF_U8(0x004859e0u + idx) = 0u;
        EF_U32(0x00161ea8u) = EF_U32(0x00161ea8u) + 1u;
        first = EF_U8(0x004859e0u);
        if (first == 0x3bu)
            continue;
        if (first == 0u && c != 0u)
            continue;
        break;
    }
}

/* 0xd5f0c, 154 bytes. Load a list of resource records (13 bytes each: name pointer dword +0, size dword +5, data pointer dword +9; ended by a zero name pointer) starting at LIST (EAX; nothing for 0, the list pointer is also kept in 0x420a60): for each record build the path default + 0x18f89c + name + 0x18f8a4, size it with the hook at 0x132ab4 (zero reported through 0x132a74 with 0x18f8ac), allocate the data (0xc3f70, failure reported with 0x18f8cc) and load it with the hook at 0x132ab8. */
void load_resource_list_d5f0c(uint32_t list)
{
    char path[0x50];
    uint32_t p = (uint32_t)(uintptr_t)path;
    uint32_t r = list;
    EF_U32(0x00420a60u) = list;
    if (list == 0u)
        return;
    while (EF_U32(r) != 0u) {
        build_default_path_9c980(p);
        strcat_9ca57(p, 0x0018f89cu);
        strcat_9ca57(p, EF_U32(r));
        strcat_9ca57(p, 0x0018f8a4u);
        EF_U32(r + 5u) = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), p);
        if (EF_U32(r + 5u) == 0u)
            guest_callptr2_x(EF_U32(0x00132a74u), 0x0018f8acu, p);
        EF_U32(r + 9u) = guest_fn_c3f70(EF_U32(r + 5u));
        if (EF_U32(r + 9u) == 0u)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x0018f8ccu);
        guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), p, EF_U32(r + 9u));
        r += 0xdu;
        if (r == 0u)
            break;
    }
}

/* 0x955c0, 157 bytes. Camera set-up (EAX, EDX, EBX, ECX and one stack argument, ret 4; named A, B, C, D, S): rebuild the rotation record at 0x12dbec (bounds init 0xd0b68, 0xd0ebc with -S, 0xd0dbc with -D scaled by C), store the negated rotation words A, B, C at 0x12dba2/0x12dba4/0x12dba6 (and 0 at 0x12dba0), copy the three position dwords at the pointer 0x214458 to 0x21446c.. and refresh the view (0x9357c). */
void setup_camera_955c0(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s0)
{
    uint32_t p = EF_U32(0x00214458u);
    init_bounds_d0b68(0x0012dbecu);
    guest_fn_d0ebc(0x0012dbecu, (0u - s0) & 0xffffu);
    guest_fn_d0dbc(0x0012dbecu, (0u - d) & 0xffffu, c);
    EF_U16(0x0012dba2u) = (uint16_t)(0u - a);
    EF_U16(0x0012dba4u) = (uint16_t)(0u - b);
    EF_U16(0x0012dba0u) = 0u;
    EF_U16(0x0012dba6u) = (uint16_t)(0u - c);
    EF_U32(0x0021446cu) = EF_U32(p);
    EF_U32(0x00214470u) = EF_U32(p + 4u);
    EF_U32(0x00214474u) = EF_U32(p + 8u);
    guest_fn_9357c();
}

/* 0xc8f38, 163 bytes. Load the two fixed data files: the path 0x18ef60 is loaded with the hook at 0x132ab8 into 0x425c8c (failure reported through 0x132a74 with 0x18ef70), 0xe9a24(0x300, 0x425c8c, 0x4195d0) is run, then the path 0x18efa0 is loaded into 0x433f34; on success set dword 0x433f30 to 1, rebuild the remap table and clear dword 0x160000, otherwise report 0x18efac. */
void load_fixed_files_c8f38(void)
{
    char path[0x90];
    uint32_t p = (uint32_t)(uintptr_t)path;
    build_path_9c9c0(p, 0x0018ef60u);
    if ((guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), p, 0x00425c8cu) & 0xffu) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018ef70u, p);
    guest_fn_e9a24(0x300u, 0x00425c8cu, 0x004195d0u);
    build_path_9c9c0(p, 0x0018efa0u);
    if ((guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), p, 0x00433f34u) & 0xffu) == 0u) {
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018efacu, p);
    } else {
        EF_U32(0x00433f30u) = 1u;
        rebuild_remap_table_e7e30(0u, 0u, 0u);
        EF_U32(0x00160000u) = 0u;
    }
}

/* 0x41314, 155 bytes. Show the picture NAME (EAX) in the preview widget: when the name differs from the current one at 0x1a49d8 (0xa4380) or no preview buffer exists yet (dword 0x12656c, allocated here as pool block 0x3e800), draw it into the buffer (0xc36b4 with 0x190 x 0x200), then remember the name, look the widget up (name 0x1843f0, or 0x1843f4 when byte 0x1a53dc is set) and attach the buffer to it (0xa5b1c, 0x190 x 0x190, stack 0x200). */
void show_preview_picture_41314(uint32_t name)
{
    uint32_t w;
    if (guest_fn_a4380(0x001a49d8u, name) != 0u || EF_U32(0x0012656cu) == 0u) {
        if (EF_U32(0x0012656cu) == 0u)
            EF_U32(0x0012656cu) = guest_fn_b3f58((int16_t)guest_fn_b3db0(0x3e800u));
        guest_fn_c36b4(name, 0u, 0u, 0x200u, 0x190u, EF_U32(0x0012656cu));
    }
    copy_string_9d749(0x001a49d8u, name);
    w = guest_fn_a46cc(EF_U8(0x001a53dcu) == 0u ? 0x001843f0u : 0x001843f4u);
    set_widget_params_a5b1c(w, EF_U32(0x0012656cu), 0x190u, 0x190u, 0x200u);
}

/* 0xe28e8, 160 bytes. One-time timer service start-up (guard byte 0x15f751): init the module (0xd6dc4), lock the handler code range (0xe28c4, 0xe28d8 - 0xe28c0 bytes) and its data (0x15f73c, 0x14 bytes) with 0x104908, hook the handler 0xe28c4 (0x102144, handle in 0x15f74c, -1 reported through 0x132a74 with 0x190860), configure it (0x102228 with 0x32), start it (0x1022dc) and install the callback 0xe28e0 in dword 0x15b620. */
void start_timer_service_e28e8(void)
{
    if (EF_U8(0x0015f751u) != 0u)
        return;
    EF_U8(0x0015f751u) = 1u;
    init_module_d6dc4();
    stack_range_104908(0x000e28c4u, 0x000e28d8u - 0x000e28c0u);
    stack_range_104908(0x0015f73cu, 0x0015f750u - 0x0015f73cu);
    EF_U32(0x0015f74cu) = trace_call_102144(0u, 0u, 0u, 0u, 0x000e28c4u);
    if (EF_U32(0x0015f74cu) == 0xffffffffu)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00190860u);
    trace_call_102228(EF_U32(0x0015f74cu), 0x32u);
    EF_U32(0x0015b620u) = 0x000e28e0u;
    trace_call_1022dc(EF_U32(0x0015f74cu));
}

/* 0x49fd4, 237 bytes. For every member I (byte +0x12 of the group record P = EAX) look up the object index in the row table at 0x1fcb40 (rows of 0x11 bytes selected by the signed word +0x10 of P, words from byte offset 1): objects of type 0, 7 or 9 (byte +0x1b of the 0xca-byte records at 0x19bb58) receive the word and byte read from the same table at the position given by the member entry (0x7c-byte entries at dword +0x13 of P, signed words +0x6e / +0x70) into their fields +0xb9 and +0xbb; all other objects get 0xff in both. */
void assign_group_slots_49fd4(uint32_t p)
{
    uint8_t i;
    if (EF_U8(p + 0x12u) == 0u)
        return;
    i = 0u;
    do {
        uint32_t row = EF_U32(0x001fcb40u) + (uint32_t)((int32_t)(int16_t)EF_U16(p + 0x10u) * 0x11);
        uint16_t w = EF_U16(row + (uint32_t)i * 2u + 1u);
        int32_t obj = (int16_t)w;
        uint8_t type = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(obj * 0xca) + 0x1bu);
        uint32_t orec;
        if (type == 0u || type == 7u || type == 9u) {
            uint32_t ent = EF_U32(p + 0x13u) + (uint32_t)i * 0x7cu;
            uint32_t r0 = EF_U32(0x001fcb40u) + (uint32_t)((int32_t)(int16_t)EF_U16(ent + 0x6eu) * 0x11)
                        + (uint32_t)((int32_t)(int16_t)EF_U16(ent + 0x70u) * 2);
            orec = EF_U32(0x0019bb58u) + (uint32_t)(obj * 0xca);
            EF_U16(orec + 0xb9u) = EF_U16(r0 + 1u);
            EF_U8(orec + 0xbbu) = EF_U8(EF_U32(0x001fcb40u) + (uint32_t)((int32_t)(int16_t)EF_U16(ent + 0x6eu) * 0x11));
        } else {
            orec = EF_U32(0x0019bb58u) + (uint32_t)(obj * 0xca);
            EF_U16(orec + 0xb9u) = 0xffu;
            EF_U8(orec + 0xbbu) = 0xffu;
        }
        i++;
    } while (i < EF_U8(p + 0x12u));
}

/* 0x4e5bc, 256 bytes. Insert an interpolated entry into the 24-slot waypoint arrays when byte 0x1a568a is 0x11 and the entry kind at 0x1a5673[byte 0x1a559f] is not: shift slots (word arrays 0x1a55a2 and 0x1a55d6, byte array 0x1a5672, dword array 0x1a560a) up by one from the end down to the current index, advance the index (byte 0x1a559f, 8-bit wrap) and store at the new index the midpoints (rounded toward zero) of the neighbouring word pairs 0x1a55a0/0x1a55a4 and 0x1a55d4/0x1a55d8, with kind 0x12. */
void insert_midpoint_4e5bc(void)
{
    int16_t k;
    uint8_t idx;
    uint32_t n;
    int32_t a, c, d, e, f;
    if (EF_U8(0x001a568au) != 0x11u)
        return;
    if (EF_U8(0x001a5673u + EF_U8(0x001a559fu)) == 0x11u)
        return;
    for (k = 0x17; k > (int16_t)EF_U8(0x001a559fu); k--) {
        uint32_t i = (uint32_t)(int32_t)k;
        EF_U16(0x001a55a4u + i * 2u) = EF_U16(0x001a55a2u + i * 2u);
        EF_U16(0x001a55d8u + i * 2u) = EF_U16(0x001a55d6u + i * 2u);
        EF_U8(0x001a5673u + i) = EF_U8(0x001a5672u + i);
        EF_U32(0x001a560eu + i * 4u) = EF_U32(0x001a560au + i * 4u);
    }
    idx = (uint8_t)(EF_U8(0x001a559fu) + 1u);
    EF_U8(0x001a559fu) = idx;
    n = idx;
    a = (int16_t)EF_U16(0x001a55a0u + n * 2u);
    c = (int16_t)EF_U16(0x001a55a4u + n * 2u);
    d = a - c;
    EF_U16(0x001a55a2u + n * 2u) = (uint16_t)(d / 2 + c);
    e = (int16_t)EF_U16(0x001a55d4u + n * 2u);
    f = (int16_t)EF_U16(0x001a55d8u + n * 2u);
    EF_U16(0x001a55d6u + (uint32_t)EF_U8(0x001a559fu) * 2u) = (uint16_t)((e - f) / 2 + f);
    EF_U8(0x001a5672u + EF_U8(0x001a559fu)) = 0x12u;
}

/* 0xcef3c, 264 bytes. Advance the 3-dword position V (EAX) by SCALE (EBX) along the direction given by the two angle words at ANG (EDX; a = +0, b = +2), using the sine table at 0x141e08 and cosine table at 0x142e08 (words indexed by angle >> 3): x += SCALE * (sin(a+b) - sin(a-b)) >> 16, y += SCALE * 2 sin(a) >> 16, z += SCALE * (cos(a+b) + cos(a-b)) >> 16. Afterwards y is clamped between (dword 0x15b7c0 << 8) and (dword 0x15b7c4 << 8). */
void advance_position_cef3c(uint32_t v, uint32_t ang, uint32_t scale)
{
    uint32_t a = EF_U16(ang), b = EF_U16(ang + 2u);
    uint32_t i1 = ((a + b) & 0xffffu) >> 3;
    uint32_t i2 = ((a - b) & 0xffffu) >> 3;
    uint32_t ia = a >> 3;
    int32_t c1 = (int16_t)EF_U16(0x00142e08u + i1 * 2u);
    int32_t c2 = (int16_t)EF_U16(0x00142e08u + i2 * 2u);
    int32_t s1 = (int16_t)EF_U16(0x00141e08u + i1 * 2u);
    int32_t s2 = (int16_t)EF_U16(0x00141e08u + i2 * 2u);
    int32_t sa = (int16_t)EF_U16(0x00141e08u + ia * 2u);
    int64_t p;
    int32_t m;
    p = (int64_t)(int32_t)scale * (int64_t)(s1 - s2);
    EF_U32(v) = EF_U32(v) + (uint32_t)(p >> 16);
    p = (int64_t)(int32_t)scale * (int64_t)(sa * 2);
    EF_U32(v + 4u) = EF_U32(v + 4u) + (uint32_t)(p >> 16);
    p = (int64_t)(int32_t)scale * (int64_t)(c1 + c2);
    EF_U32(v + 8u) = EF_U32(v + 8u) + (uint32_t)(p >> 16);
    m = (int32_t)(EF_U32(0x0015b7c4u) << 8);
    if (m < (int32_t)EF_U32(v + 4u))
        EF_U32(v + 4u) = (uint32_t)m;
    m = (int32_t)(EF_U32(0x0015b7c0u) << 8);
    if (m > (int32_t)EF_U32(v + 4u))
        EF_U32(v + 4u) = (uint32_t)m;
}

/* 0xffe78, 211 bytes. Draw one texture-displaced span: from x = high word of dword 0x485fe8 to x = high word of dword 0x485fd0 (clipped to dword 0x15bdc4 - 3) on the row high word of 0x485fec, with the texture coordinates stepping from the dwords 0x485ff0 (u) / 0x485ff4 (v) towards 0x485fd8 / 0x485fdc. Each of the n + 1 pixels is replaced by the screen byte (dword 0x159080 texture at (v >> 8 & 0xff00) + (u >> 16), low two bits) that many bytes further along the row. */
void draw_displaced_span_ffe78(void)
{
    int32_t y = (int32_t)EF_U32(0x00485fecu) >> 16;
    int32_t x0 = (int32_t)EF_U32(0x00485fe8u) >> 16;
    int32_t x1 = (int32_t)EF_U32(0x00485fd0u) >> 16;
    int32_t lim = (int32_t)EF_U32(0x0015bdc4u) - 3;
    int32_t n, du, dv, k;
    uint32_t u = EF_U32(0x00485ff0u), v = EF_U32(0x00485ff4u), dst;
    if (x1 > lim)
        x1 = lim;
    n = x1 - x0;
    if (n <= 0)
        return;
    dst = EF_U32(0x0015b754u) + EF_U32(0x00163200u + (uint32_t)y * 4u) + (uint32_t)x0;
    du = ((int32_t)EF_U32(0x00485fd8u) - (int32_t)u) / n;
    dv = ((int32_t)EF_U32(0x00485fdcu) - (int32_t)v) / n;
    for (k = 0; k <= n; k++) {
        uint32_t off = ((uint32_t)((int32_t)v >> 8) & 0xff00u) + ((uint32_t)((int32_t)u >> 16));
        uint32_t t = EF_U8(EF_U32(0x00159080u) + off) & 3u;
        EF_U8(dst) = EF_U8(dst + t);
        dst++;
        v += (uint32_t)dv;
        u += (uint32_t)du;
    }
}

/* 0xd2500, 129 bytes. Implicit-register fragment (ESI = list of object word offsets, ECX = count; params in that order): fill the render object at 0x420970 (member table at 0x4209d0: count, then the object pointers; fields 0x42099c = 1, 0x420974 = 0, 0x4209bc = table, 0x420990 = 0x1016e0, 0x420994 = the dword 0x15b800[byte... dword 0x15918c]); for each listed object (base dword 0x17bf70 + word) clear flag bits 1 and 2 of byte +0x20, AND its flags dword into the render object's flags (+0x2c) and store its pointer in the table; then run 0xd23d8 on the render object. */
void build_render_list_d2500(uint32_t list, uint32_t count)
{
    uint32_t ro = 0x00420970u, tbl = 0x004209d0u, i, obj, t = tbl;
    EF_U32(tbl) = count;
    EF_U32(0x00420990u) = 0x001016e0u;
    EF_U32(0x0042099cu) = 1u;
    EF_U32(0x00420974u) = 0u;
    EF_U32(0x004209bcu) = tbl;
    EF_U32(0x00420994u) = EF_U32(0x0015b800u + EF_U32(0x0015918cu) * 4u);
    if ((int32_t)count > 0) {
        for (i = 0; i < count; i++) {
            obj = EF_U32(0x0017bf70u) + EF_U16(list);
            EF_U8(obj + 0x20u) = (uint8_t)(EF_U8(obj + 0x20u) & 0xf9u);
            list += 2u;
            EF_U32(ro + 0x2cu) = EF_U32(ro + 0x2cu) & EF_U32(obj + 0x20u);
            t += 4u;
            EF_U32(t) = obj;
        }
    }
    process_render_object_d23d8(ro);
}

/* 0xd2704, 191 bytes. Implicit-register fragment (ESI = list of object word offsets, ECX = count; params in that order): as 0xd2500 but with the fixed value 0xffdb0 in 0x420994, and each object also gets its fields +0x24 / +0x28 from the word pairs at 0x15997c[4 * i] / 0x15997e[4 * i] shifted left 9, and flag bit 1 set, bit 2 cleared. */
void build_render_list_d2704(uint32_t list, uint32_t count)
{
    uint32_t ro = 0x00420970u, tbl = 0x004209d0u, i, obj, t = tbl;
    EF_U32(0x0042099cu) = 1u;
    EF_U32(0x00420974u) = 0u;
    EF_U32(0x004209bcu) = tbl;
    EF_U32(tbl) = count;
    EF_U32(0x00420990u) = 0x001016e0u;
    EF_U32(0x00420994u) = 0x000ffdb0u;
    if ((int32_t)count > 0) {
        for (i = 0; i < count; i++) {
            uint8_t fl;
            obj = EF_U32(0x0017bf70u) + EF_U16(list);
            EF_U32(obj + 0x24u) = (uint32_t)EF_U16(0x0015997cu + i * 4u) << 9;
            EF_U32(obj + 0x28u) = (uint32_t)EF_U16(0x0015997eu + i * 4u) << 9;
            fl = (uint8_t)(EF_U8(obj + 0x20u) | 2u);
            EF_U8(obj + 0x20u) = (uint8_t)(fl & 0xfbu);
            EF_U32(ro + 0x2cu) = EF_U32(ro + 0x2cu) & EF_U32(obj + 0x20u);
            list += 2u;
            t += 4u;
            EF_U32(t) = obj;
        }
    }
    process_render_object_d23d8(ro);
}

/* Shared body of 0xd2c84 / 0xd2d7c: like 0xd2704 with the coordinate shift 9 when dword 0x158f0c is set (else 0x10) and a caller-chosen handler value in render object +0x24 (0x420994). */
static void build_render_list_shifted(uint32_t list, uint32_t count, uint32_t handler)
{
    uint32_t ro = 0x00420970u, tbl = 0x004209d0u, i, obj, t = tbl;
    uint32_t sh = EF_U32(0x00158f0cu) != 0u ? 9u : 0x10u;
    EF_U32(0x0042099cu) = 1u;
    EF_U32(0x00420974u) = 0u;
    EF_U32(0x004209bcu) = tbl;
    EF_U32(tbl) = count;
    EF_U32(ro + 0x20u) = 0x001016e0u;
    EF_U32(ro + 0x24u) = handler;
    if ((int32_t)count > 0) {
        for (i = 0; i < count; i++) {
            uint8_t fl;
            obj = EF_U32(0x0017bf70u) + EF_U16(list);
            EF_U32(obj + 0x24u) = (uint32_t)EF_U16(0x0015997cu + i * 4u) << sh;
            fl = (uint8_t)(EF_U8(obj + 0x20u) | 2u);
            EF_U32(obj + 0x28u) = (uint32_t)EF_U16(0x0015997eu + i * 4u) << sh;
            EF_U8(obj + 0x20u) = (uint8_t)(fl & 0xfbu);
            list += 2u;
            EF_U32(ro + 0x2cu) = EF_U32(ro + 0x2cu) & EF_U32(obj + 0x20u);
            t += 4u;
            EF_U32(t) = obj;
        }
    }
    process_render_object_d23d8(ro);
}

/* 0xd2c84, 245 bytes. Implicit-register fragment (ESI = list of object word offsets, ECX = count; params in that order): build the render list with handler value 0xffe78. */
void build_render_list_d2c84(uint32_t list, uint32_t count)
{
    build_render_list_shifted(list, count, 0x000ffe78u);
}

/* 0xd2d7c, 267 bytes. As 0xd2c84 but the handler value is the dword 0x15b884 when dword 0x158f0c is set, else 0x15b888. */
void build_render_list_d2d7c(uint32_t list, uint32_t count)
{
    build_render_list_shifted(list, count, EF_U32(0x00158f0cu) != 0u ? EF_U32(0x0015b884u) : EF_U32(0x0015b888u));
}

/* 0x91c98, 257 bytes. Advance the camera-preset queue: when the read index (word 0x2144ae) has not caught up with the write index (word 0x2144ac), switch the viewpoint byte 0x12db9e to the queued entry (byte table 0x214428, 2 bytes per entry; the previous value is remembered in 0x214486), set byte 0x1f88fc to 3, move the pending words 0x122c7a / 0x122c7c to 0x1f88f4 / 0x1f88f2 (unless -1), reset them to -1 and set word 0x2144b2 to 0xfa; otherwise reset both indices. Then apply a pending roll (word 0x122c54 into 0x12dba0) and, if word 0x2144a2 is set, copy the record 0x12dc10 over 0x12dbec; finally clear 0x2144a2, 0x122c54 and byte 0x12dbc4. */
void advance_view_queue_91c98(void)
{
    uint16_t roll;
    if (EF_U16(0x002144aeu) == EF_U16(0x002144acu)) {
        EF_U16(0x002144acu) = 0u;
        EF_U16(0x002144aeu) = 0u;
    } else {
        int16_t idx;
        if (EF_U8(0x00214486u) != EF_U8(0x0012db9eu))
            EF_U16(0x00214486u) = EF_U8(0x0012db9eu);
        idx = (int16_t)EF_U16(0x002144aeu);
        EF_U8(0x0012db9eu) = EF_U8(0x00214428u + (uint32_t)(int32_t)idx * 2u);
        EF_U8(0x001f88fcu) = 3u;
        EF_U16(0x002144aeu) = (uint16_t)(idx + 1);
        if ((int16_t)EF_U16(0x00122c7au) != -1)
            EF_U16(0x001f88f4u) = EF_U16(0x00122c7au);
        if ((int16_t)EF_U16(0x00122c7cu) != -1)
            EF_U16(0x001f88f2u) = EF_U16(0x00122c7cu);
        EF_U16(0x00122c7au) = 0xffffu;
        EF_U16(0x00122c7cu) = 0xffffu;
        EF_U16(0x002144b2u) = 0xfau;
    }
    roll = EF_U16(0x00122c54u);
    if (roll != 0u)
        EF_U16(0x0012dba0u) = roll;
    if (EF_U16(0x002144a2u) != 0u)
        copy_record9_d0858(0x0012dbecu, 0x0012dc10u);
    EF_U16(0x002144a2u) = 0u;
    EF_U16(0x00122c54u) = 0u;
    EF_U8(0x0012dbc4u) = 0u;
}

/* 0x47764, 259 bytes. Queue a message in the 9-slot table at 0x12668e (0x14-byte slots: word +2 = SI id, +4 = DI, dwords +0xa/+0xe/+0x12 = EBX, ECX and the stack argument, dword +6 = serial number from 0x12668c) for the unit DI (signed word, must exist or be 0x899 / 0xff) when SI is in 1..0x84 and the entry 0x126a5c[SI * 7] is not 0x12f3d2. Nothing is queued when a slot already holds the same SI / DI pair; the first free slot (id 0xff) is used. Also sets word 0x126658 to 0x6e and byte 0x126655 to 1. */
void queue_message_47764(uint32_t di, uint32_t si, uint32_t ebx, uint32_t ecx, uint32_t s0)
{
    int32_t unit = (int16_t)di;
    int32_t id = (int16_t)si;
    uint8_t dl, done = 0u;
    if (!(guest_fn_7489c(unit) != 0u || unit == 0x899 || unit == 0xff))
        return;
    if (id <= 0 || id >= 0x85)
        return;
    if (EF_U32(0x00126a5cu + (uint32_t)id * 7u) == 0x0012f3d2u)
        return;
    for (dl = 0; dl < 9u; dl++) {
        uint32_t rec = 0x0012668eu + (uint32_t)dl * 0x14u;
        if ((uint16_t)si == EF_U16(rec + 2u) && (uint16_t)di == EF_U16(rec + 4u))
            return;
        if ((int16_t)EF_U16(rec + 2u) == 0xff && !done) {
            EF_U16(rec + 2u) = (uint16_t)si;
            EF_U16(rec + 4u) = (uint16_t)di;
            EF_U32(rec + 0xeu) = ecx;
            EF_U32(rec + 0xau) = ebx;
            EF_U32(rec + 0x12u) = s0;
            EF_U32(rec + 6u) = EF_U32(0x0012668cu);
            EF_U16(0x00126658u) = 0x6eu;
            EF_U32(0x0012668cu) = EF_U32(0x0012668cu) + 1u;
            EF_U8(0x00126655u) = 1u;
            done = 1u;
        }
    }
}

/* 0x348e4, 264 bytes. Refresh the 8-row list widget (when byte 0x13578c is set): walk the two parallel widget chains (found under 0xa46cc with the names 0x182e5c / 0x182e60; link dword +0x21) over the 8 entries of the table 0x1368bc; an entry with bit 0 of byte +0x1c set is shown (0xa7588) and both rows are activated with state 0 (counted), the others get state 1. Then resize the list button when its height (signed word +0x47) is below 18 * word 0x1357b0 + 0x1e, and remember the selection (byte 0x1357b0 into 0x19f59b) when the number of active rows equals word 0x1357b0. */
void refresh_list_widget_348e4(void)
{
    uint32_t root, a, b, item, i;
    uint8_t active = 0u;
    int32_t limit;
    if (EF_U8(0x0013578cu) == 0u)
        return;
    root = guest_fn_a46cc(0x00182d48u);
    a = find_widget_checked_a78bc(root, 0x00182e5cu);
    b = find_widget_checked_a78bc(root, 0x00182e60u);
    for (i = 0; i < 8u; i++) {
        item = EF_U32(0x001368bcu + i * 4u);
        if (EF_U8(item + 0x1cu) & 1u) {
            guest_fn_a7588_x(a, item);
            guest_fn_a739c(a, 0u);
            guest_fn_a739c(b, 0u);
            active++;
        } else {
            guest_fn_a739c(a, 1u);
            guest_fn_a739c(b, 1u);
        }
        a = EF_U32(a + 0x21u);
        b = EF_U32(b + 0x21u);
    }
    limit = (int32_t)(int16_t)EF_U16(0x001357b0u) * 18 + 0x1e;
    if (((int32_t)EF_U32(root + 0x45u) >> 16) < limit) {
        int16_t h = (int16_t)((int16_t)EF_U16(0x001357b0u) * 0x12 + 0x1e);
        guest_fn_a520c(root, (int32_t)EF_U32(root + 0x43u) >> 16, (uint32_t)(int32_t)h, 1u);
    }
    if ((uint16_t)active == EF_U16(0x001357b0u))
        EF_U8(0x0019f59bu) = EF_U8(0x001357b0u);
}

/* 0x440f4, 262 bytes. Draw a sprite from the table selected by SEL (EAX low byte; EDX, EBX, ECX, then the two stack arguments S0, S1, ret 8; the implicit ESI input is the last parameter and is used unchanged for SEL > 5): the sprite address is word x + word y * 640 + the frame base dword 0x126570, where x/y are the words at BASE and BASE + 2 with BASE = 0x134565, 0x134553, 0x134541, 0x13454a, 0x13456e, 0x13455c for SEL 0..5. Calls 0xca620 with the address, the sign-extended EDX, EBX, ECX and the low words of S0 (signed) and S1. */
void draw_table_sprite_440f4(uint32_t sel, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1, uint32_t esi_in)
{
    static const uint32_t base[6] = { 0x00134565u, 0x00134553u, 0x00134541u, 0x0013454au, 0x0013456eu, 0x0013455cu };
    uint32_t frame = EF_U32(0x00126570u);
    uint32_t addr = esi_in;
    if ((sel & 0xffu) <= 5u) {
        uint32_t b = base[sel & 0xffu];
        int32_t x = (int16_t)EF_U16(b), y = (int16_t)EF_U16(b + 2u);
        addr = (uint32_t)(x + y * 5 * 128) + frame;
    }
    EF_U32(0x00126570u) = frame;
    guest_fn_ca620((int32_t)addr, (int32_t)(int16_t)edx, (int32_t)(int16_t)ebx, (uint32_t)(int32_t)(int16_t)ecx,
                   (uint32_t)(int32_t)(int16_t)s0, s1 & 0xffffu);
}

/* 0x8b6e0, 262 bytes. Fetch the next token of a word list into OUT (ECX): with MODE (EAX, signed word) 1 the list file is (re)loaded first (default path + 0x18a854, sized with the hook 0x132ab4, read with 0x132ab8 into a fresh arena block kept in 0x20f424, NUL terminated) and the read cursor 0x20f42c is rewound to it. Then OUT = PREFIX (EDX) followed by the next characters up to the next separator (ctype table 0x195af0, bit 2 of entry byte + 1), the following separators are skipped and the cursor stored. Returns 1 when a token was produced; returns 0 with OUT empty at the end of the data (the block is freed) or when nothing is loaded. */
uint32_t next_list_token_8b6e0(uint32_t mode, uint32_t prefix, uint32_t out)
{
    uint32_t cur = EF_U32(0x0020f42cu);
    uint32_t size, blk;
    if ((int32_t)(int16_t)mode == 1) {
        build_default_path_9c980(out);
        cur = 0u;
        strcat_9ca57(out, 0x0018a854u);
        EF_U32(0x0020f42cu) = 0u;
        size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), out);
        cur = EF_U32(0x0020f42cu);
        if (size != 0u) {
            blk = guest_fn_e2a94(size + 1u);
            EF_U32(0x0020f424u) = blk;
            guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), out, blk);
            cur = EF_U32(0x0020f424u);
            EF_U8(cur + size) = 0u;
        }
    }
    if (cur != 0u) {
        uint8_t c = EF_U8(cur);
        EF_U32(0x0020f42cu) = cur;
        if (c != 0u) {
            copy_string_9d749(out, prefix);
            out += guest_fn_b1b8b(out);
            for (;;) {
                cur = EF_U32(0x0020f42cu);
                if (EF_U8(0x00195af0u + (uint8_t)(EF_U8(cur) + 1u)) & 2u)
                    break;
                cur++;
                out++;
                EF_U8(out - 1u) = EF_U8(cur - 1u);
                EF_U32(0x0020f42cu) = cur;
            }
            while (EF_U8(0x00195af0u + (uint8_t)(EF_U8(cur) + 1u)) & 2u)
                cur++;
            EF_U8(out) = 0u;
            EF_U32(0x0020f42cu) = cur;
            return 1u;
        }
        guest_fn_e2af4(EF_U32(0x0020f424u));
        cur = 0u;
    }
    EF_U8(out) = 0u;
    EF_U32(0x0020f42cu) = cur;
    return 0u;
}

/* 0xa60a8, 262 bytes. Redraw the widget OBJ (EAX): the mode byte comes from 0xe588c (0 when flag 2 of byte +0x3b is set); mode 1 saves the rectangle list first and mode 2 does nothing at all. Refresh the object (0xabb00, view rect 0xc9f28), draw its background (0xe4c9c when dword +0x95 is set, else a filled rectangle from the rectangle words 0x41eef6/0x41eef8/0x41eefa/0x41ef00 in colour byte +0xc2 when flag 1 of byte +0xc4 is set; both only if flag 1 of byte +0x3c), then call its draw hook (dword +0xb5) with the draw-in-progress markers 0x41eea0 / 0x41f2e5 set, rebuild the view corners (0xc9ecc), tidy the chain (0xa7204) with byte 0x13725c forced to 1 and, for mode 1, restore the rectangle list. */
void redraw_widget_a60a8(uint32_t obj)
{
    uint8_t mode, saved;
    if (EF_U8(obj + 0x3bu) & 2u)
        mode = 0u;
    else
        mode = (uint8_t)(guest_fn_e588c_ret_x(obj) & 0xffu);
    if (mode == 2u)
        return;
    if (mode == 1u)
        save_rect_list_e5114();
    guest_fn_abb00_x(obj);
    set_view_rect_c9f28(obj);
    if (EF_U8(obj + 0x3cu) & 1u) {
        if (EF_U32(obj + 0x95u) != 0u) {
            guest_fn_e4c9c_w(obj);
        } else if (EF_U8(obj + 0xc4u) & 1u) {
            uint32_t x0 = EF_U16(0x0041eef6u), y0 = EF_U16(0x0041eef8u);
            uint32_t x1 = (uint16_t)(EF_U16(0x0041eef6u) + EF_U16(0x0041eefau));
            uint32_t y1 = (uint16_t)(EF_U16(0x0041eef8u) + EF_U16(0x0041ef00u));
            fill_rect_clipped_c91c8(EF_U8(obj + 0xc2u), x0, y0, x1, y1);
        }
    }
    EF_U32(0x0041eea0u) = obj;
    EF_U8(0x0041f2e5u) = 1u;
    guest_callptr_eax_ret_x(EF_U32(obj + 0xb5u), obj);
    EF_U32(0x0041eea0u) = 0u;
    EF_U8(0x0041f2e5u) = 0u;
    init_view_corners_c9ecc();
    saved = EF_U8(0x0013725cu);
    EF_U8(0x0013725cu) = 1u;
    tidy_chain_a7204(obj);
    EF_U8(0x0013725cu) = saved;
    if (mode == 1u)
        restore_rect_list_e516c();
}

/* 0x5de14, 243 bytes. Draw a small direction marker: with SIZE (EBX, signed word) the two points (0, 2 * SIZE) and (0, SIZE * 4 / 3) are rotated by the heading of the object ECX (word +0xe, plus 0x8000; sine/cosine via 0xdc7bc / 0xdc7e4), offset by the screen position (X = EAX, Y = EDX) and joined with a line in the colour byte 0x128148 (0xc8ccc, stack arguments y1, x1, y0, x0, colour). The scratch words 0x1281fa..0x128200 receive 0, 2 * SIZE, 0, SIZE * 4 / 3. */
void draw_heading_marker_5de14(uint32_t x, uint32_t y, uint32_t size, uint32_t obj)
{
    int16_t p0[2], p1[2];
    int32_t s = (int16_t)size;
    EF_U16(0x001281fau) = 0u;
    EF_U16(0x001281fcu) = (uint16_t)((s * 6) / 3);
    EF_U16(0x001281feu) = 0u;
    EF_U16(0x00128200u) = (uint16_t)((s * 4) / 3);
    load_sincos_dc7bc((uint32_t)(uint16_t)(EF_U16(obj + 0xeu) + 0x8000u));
    rotate_point_dc7e4((uint32_t)(uintptr_t)p0, (int16_t)EF_U16(0x001281fau), (int16_t)EF_U16(0x001281fcu));
    rotate_point_dc7e4((uint32_t)(uintptr_t)p1, (int16_t)EF_U16(0x001281feu), (int16_t)EF_U16(0x00128200u));
    guest_fn_c8ccc_x(
                     (uint32_t)(int32_t)(int16_t)(p1[1] + (int16_t)y), (uint32_t)(int32_t)(int16_t)(p1[0] + (int16_t)x),
                     (uint32_t)(int32_t)(int16_t)(p0[1] + (int16_t)y), (uint32_t)(int32_t)(int16_t)(p0[0] + (int16_t)x),
                     EF_U8(0x00128148u));
}

/* 0x34298, 237 bytes. Update the item-list dialog: without a name match (0xa4380 of 0x125937 against 0x182b34) just activate and redraw the widget 0x182dd0. Otherwise, unless one of the bytes 0x19f5ae, 0x125902, 0x1357c8 is set, query 0xe1758 and then either lay out the list widget 0x182dc4 for dword 0x429210 rows (rows * 20 height at word +0x67; fewer than 5 rows show the children and resize to rows * 20 + 0x18 high, otherwise hide them and use the fixed size 0x7c) and activate it, or (no rows) activate and redraw the widget 0x182de0. */
void update_item_dialog_34298(void)
{
    uint32_t w;
    if (guest_fn_a4380(0x00125937u, 0x00182b34u) == 0u) {
        w = guest_fn_a46cc(0x00182dd0u);
        guest_fn_a4cb4(EF_U8(w + 0x10u));
        redraw_widget_ac9a0(w);
        return;
    }
    if (EF_U8(0x0019f5aeu) != 0u || EF_U8(0x00125902u) != 0u || EF_U8(0x001357c8u) != 0u)
        return;
    (void)unsupported_query_e1758();
    if (EF_U32(0x00429210u) == 0u) {
        w = guest_fn_a46cc(0x00182de0u);
        guest_fn_a4cb4(EF_U8(w + 0x10u));
        redraw_widget_ac9a0(w);
        return;
    }
    w = guest_fn_a46cc(0x00182dc4u);
    EF_U16(w + 0x67u) = (uint16_t)(EF_U16(0x00429210u) * 0x14u);
    if ((int32_t)EF_U32(0x00429210u) < 5) {
        int16_t h;
        toggle_children_a570c(w, 0u);
        h = (int16_t)((int16_t)EF_U16(0x00429210u) * 0x14 + 0x18);
        guest_fn_a520c(w, 0x11c, (uint32_t)(int32_t)h, 0u);
    } else {
        toggle_children_a570c(w, 1u);
        guest_fn_a520c(w, 0x12c, 0x7cu, 0u);
    }
    guest_fn_a4cb4(EF_U8(w + 0x10u));
}

/* 0xc3364, 194 bytes. Find the size of the data file NAME (EDX) trying several locations; BUF (EAX) is the work buffer and the result is stored in dword 0x41f3c8. The name is first lower-cased and checked for the substring 0x18ee2c (found: the last four characters of the buffer are cut off); then the hook at 0x132ab4 is tried on the plain name, on default path + 0x18ee34 + name, on default path + 0x18ee40 + name and finally on the plain name again, stopping at the first non-zero size. */
void find_data_size_c3364(uint32_t buf, uint32_t name)
{
    uint32_t size;
    copy_string_9d749(buf, name);
    guest_fn_eb478(buf);
    if (strstr_e7d27(buf, 0x0018ee2cu) != 0u)
        EF_U8(buf + guest_fn_b1b8b(buf) - 4u) = 0u;
    copy_string_9d749(buf, name);
    size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), buf);
    EF_U32(0x0041f3c8u) = size;
    if (size != 0u)
        return;
    build_default_path_9c980(buf);
    if (EF_U8(buf) != 0u)
        strcat_9ca57(buf, 0x0018ee34u);
    strcat_9ca57(buf, name);
    size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), buf);
    EF_U32(0x0041f3c8u) = size;
    if (size != 0u)
        return;
    build_default_path_9c980(buf);
    if (EF_U8(buf) != 0u)
        strcat_9ca57(buf, 0x0018ee40u);
    strcat_9ca57(buf, name);
    size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), buf);
    EF_U32(0x0041f3c8u) = size;
    if (size != 0u)
        return;
    copy_string_9d749(buf, name);
    EF_U32(0x0041f3c8u) = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), buf);
}

/* 0xc768, 207 bytes. Lay out the four numbered slot widgets of the dialog 0x180728 for the selection INDEX (EAX, signed word): the widget 0x1809a8 and the widgets named "1".."4" are first moved to (0x32, 0x14); then, starting at the slot number stored in word 0x1a53b0[INDEX], the widgets "n+1" up to "4" are moved to (0, 0); the widget 0x180580 is enabled (0xa72d8) and, when the signed word 0x1a53c0 is below 2, the widget 0x1809a8 is moved to (0, 0) as well. */
void layout_slot_widgets_c768(uint32_t index)
{
    char name[12];
    uint32_t nm = (uint32_t)(uintptr_t)name;
    uint32_t root = guest_fn_a46cc(0x00180728u);
    int16_t n;
    move_widget_a74bc(find_widget_checked_a78bc(root, 0x001809a8u), 0x32, 0x14);
    for (n = 0; n < 4; n++) {
        (void)signed_to_digits_cbd0f((int32_t)n + 1, nm, 10u);
        move_widget_a74bc(find_widget_checked_a78bc(root, nm), 0x32, 0x14);
    }
    n = (int16_t)EF_U16(0x001a53b0u + (uint32_t)(int32_t)(int16_t)index * 2u);
    while (n < 4) {
        (void)signed_to_digits_cbd0f((int32_t)n + 1, nm, 10u);
        move_widget_a74bc(find_widget_checked_a78bc(root, nm), 0, 0);
        n++;
    }
    enable_widget_a72d8(find_widget_checked_a78bc(root, 0x00180580u));
    if ((int16_t)EF_U16(0x001a53c0u) < 2)
        move_widget_a74bc(find_widget_checked_a78bc(root, 0x001809a8u), 0, 0);
}

/* 0xe4010, 211 bytes. Load the text file NAME (EAX) into a local 0x400-byte buffer (default path + 0x190ad4 + name through the hooks 0x132ab4 / 0x132ab8; an oversized file is reported with 0x190ad8, a missing one falls back to the default text at dword 0x15f934), reset dword 0x15f93c, free and clear the previous table block (dword 0x15f940), clear the 0x20 bytes at 0x431ed0, run the text script (0x1093d8, table 0x15f944, 7 entries) and rebuild the pattern buffer (0xe40e4). */
void load_pattern_file_e4010(uint32_t name)
{
    char text[0x400];
    char path[0x90];
    uint32_t t = (uint32_t)(uintptr_t)text;
    uint32_t p = (uint32_t)(uintptr_t)path;
    uint32_t size, old;
    build_default_path_9c980(p);
    strcat_9ca57(p, 0x00190ad4u);
    strcat_9ca57(p, name);
    size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), p);
    if (size != 0u) {
        if (size > 0x3ffu)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x00190ad8u);
        guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), p, t);
        text[size] = 0;
    } else {
        copy_string_9d749(t, EF_U32(0x0015f934u));
    }
    old = EF_U32(0x0015f940u);
    EF_U32(0x0015f93cu) = 0u;
    if (old != 0u)
        guest_fn_c3f80(old);
    EF_U32(0x0015f940u) = 0u;
    fill_bytes_b3fb0(0x00431ed0u, 0u, 0x20u);
    run_text_script_1093d8(t, 0x0015f944u, 7u);
    init_pattern_buffer_e40e4();
}

/* 0x4f7d0, 215 bytes. Fill the 16 button widgets "<n>" (format 0x184a9c) of the dialog 0x184a8c: the first byte 0x1a5531 buttons get the label 0x1a53e3 + n * 0x11 and the state byte 0x1a5510[n] (via 0x4f718); a state of 3 or 4 sets byte 0x1a5532. The remaining buttons up to 16 get the empty label 0x184e44 and state 0. */
void fill_button_widgets_4f7d0(void)
{
    char name[0x20];
    uint32_t nm = (uint32_t)(uintptr_t)name;
    uint32_t root = guest_fn_a46cc(0x00184a8cu);
    int16_t n;
    EF_U8(0x001a5532u) = 0u;
    for (n = 0; n < (int16_t)EF_U8(0x001a5531u); n++) {
        (void)guest_sprintf_a4342(nm, 0x00184a9cu, (int32_t)n);
        guest_fn_a7588_x(find_widget_checked_a78bc(root, nm), 0x001a53e3u + (uint32_t)n * 0x11u);
        select_button_state_4f718((uint32_t)n, EF_U8(0x001a5510u + (uint32_t)n));
        if (EF_U8(0x001a5510u + (uint32_t)n) == 3u || EF_U8(0x001a5510u + (uint32_t)n) == 4u)
            EF_U8(0x001a5532u) = 1u;
    }
    n = (int16_t)EF_U8(0x001a5531u);
    while (n < 0x10) {
        (void)guest_sprintf_a4342(nm, 0x00184a9cu, (int32_t)n);
        guest_fn_a7588_x(find_widget_checked_a78bc(root, nm), 0x00184e44u);
        select_button_state_4f718((uint32_t)n, 0u);
        n++;
    }
}

/* 0xab174, 233 bytes. Read a (group, item, kind) selector from the stream HANDLE (EAX): word A, word B and byte C (0xa79d4). A kind above 2 or a group B not below word 0x4194c2 clears byte 0x41955f and stops; otherwise the byte is set to C, the B-th element of the linked list headed by dword 0x419200 is taken (link at +0), its item count (word +0xc) must exceed A, and the A-th element (link at +1) of its sub-list (dword +8) is stored in dword 0x4191d4 (clearing the byte 0x41955f when the item index is out of range). */
void read_selector_ab174(uint32_t handle)
{
    uint16_t a = 0u, b = 0u;
    uint8_t c = 0u;
    uint32_t node = EF_U32(0x00419200u);
    int32_t i;
    guest_fn_a79d4(handle, 2u, (uint32_t)(uintptr_t)&a);
    guest_fn_a79d4(handle, 2u, (uint32_t)(uintptr_t)&b);
    guest_fn_a79d4(handle, 1u, (uint32_t)(uintptr_t)&c);
    if (c > 2u) {
        EF_U8(0x0041955fu) = 0u;
        return;
    }
    EF_U8(0x0041955fu) = c;
    if ((int16_t)b >= (int16_t)EF_U16(0x004194c2u)) {
        EF_U8(0x0041955fu) = 0u;
        return;
    }
    if ((int16_t)b > 0) {
        i = 0;
        do {
            i++;
            node = EF_U32(node);
        } while ((int16_t)i < (int16_t)b);
    }
    if ((int16_t)a >= (int16_t)EF_U16(node + 0xcu)) {
        EF_U8(0x0041955fu) = 0u;
        return;
    }
    node = EF_U32(node + 8u);
    if ((int16_t)a > 0) {
        i = 0;
        do {
            i++;
            node = EF_U32(node + 1u);
        } while ((int16_t)i < (int16_t)a);
    }
    EF_U32(0x004191d4u) = node;
}

/* 0xddae0, 232 bytes. Poll the mouse: add the motion (0xce02c) to the pointer words 0x425fc8 (x) and 0x425fc6 (y), clamp x to the dword range 0x15bdc0..0x15bdc4 and y to 0x15bdc8..0x15bdcc (an out-of-range value is replaced by the limit), read the button word (0xce064) into 0x425fc4 and split bits 1, 2 and 4 into the bytes 0x425fcc, 0x425fca and 0x425fcb. */
void poll_mouse_ddae0(void)
{
    int32_t d[2];
    uint16_t x, y;
    mouse_read_motion_ce02c((uint32_t)(uintptr_t)&d[0], (uint32_t)(uintptr_t)&d[1]);
    x = (uint16_t)(EF_U16(0x00425fc8u) + (uint16_t)d[0]);
    y = (uint16_t)(EF_U16(0x00425fc6u) + (uint16_t)d[1]);
    EF_U16(0x00425fc8u) = x;
    EF_U16(0x00425fc6u) = y;
    if ((int32_t)(int16_t)EF_U16(0x00425fc8u) < (int32_t)EF_U32(0x0015bdc0u))
        EF_U16(0x00425fc8u) = (uint16_t)EF_U32(0x0015bdc0u);
    if ((int32_t)(int16_t)EF_U16(0x00425fc8u) > (int32_t)EF_U32(0x0015bdc4u))
        EF_U16(0x00425fc8u) = (uint16_t)EF_U32(0x0015bdc4u);
    if ((int32_t)(int16_t)EF_U16(0x00425fc6u) < (int32_t)EF_U32(0x0015bdc8u))
        EF_U16(0x00425fc6u) = (uint16_t)EF_U32(0x0015bdc8u);
    if ((int32_t)(int16_t)EF_U16(0x00425fc6u) > (int32_t)EF_U32(0x0015bdccu))
        EF_U16(0x00425fc6u) = (uint16_t)EF_U32(0x0015bdccu);
    EF_U16(0x00425fc4u) = (uint16_t)mouse_read_buttons_ce064();
    EF_U8(0x00425fccu) = (uint8_t)(EF_U8(0x00425fc4u) & 1u);
    EF_U8(0x00425fcau) = (uint8_t)(EF_U8(0x00425fc4u) & 2u);
    EF_U8(0x00425fcbu) = (uint8_t)(EF_U8(0x00425fc4u) & 4u);
}

/* 0xea50c, 234 bytes. cdecl, variadic (FMT, ...): increment the nesting counter byte 0x160638; only when it becomes 1 format the message; when it is not empty and the log handle (dword 0x132a78) is open, rewrite the template (0xea4c0), build message + 0x19154c + 0x1329ec + 0x19154c and write it to the log (0xb3d00 with the handle, the text and its length). The formatted message is then handed to 0xeb448. */
void log_message_ea50c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t fmt, ...)
{
    char msg[0x280];
    char line[0x280];
    va_list ap;
    uint32_t m = (uint32_t)(uintptr_t)msg, l = (uint32_t)(uintptr_t)line;
    (void)u0; (void)u1; (void)u2; (void)u3;
    EF_U8(0x00160638u) = (uint8_t)(EF_U8(0x00160638u) + 1u);
    if ((int8_t)EF_U8(0x00160638u) != 1)
        return;
    va_start(ap, fmt);
    format_to_buffer_e3022(m, fmt, (uint32_t)(uintptr_t)&ap[0]);
    va_end(ap);
    if (msg[0] != 0 && EF_U32(0x00132a78u) != 0u) {
        rewrite_template_ea4c0();
        copy_string_9d749(l, m);
        strcat_9ca57(l, 0x0019154cu);
        strcat_9ca57(l, 0x001329ecu);
        strcat_9ca57(l, 0x0019154cu);
        (void)write_whole_file_b3d00(EF_U32(0x00132a78u), l, guest_fn_b1b8b(l));
    }
    enter_handler_eb448(m);
}

/* 0x4f8a8, 236 bytes. Refresh the button dialog 0x184ae4: label the widgets 0x184e4c and 0x184e54 with the numbers of bytes 0x1a5530 and 0x1a5531 (format 0x184c94), label the widget 0x184e60 with dword +0xc of the record at dword 0x1a5506, find the entry of that record's table (dword +0x2c, 0x15-byte entries, count word +0x2a, entries 0xfe / 0xff skipped) whose name (table 0x13315c) equals the text 0x184e68 into byte 0x1a5533, and refill the buttons (0x4f7d0). */
void refresh_button_dialog_4f8a8(void)
{
    char buf[0x10];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    uint32_t root = guest_fn_a46cc(0x00184ae4u);
    uint32_t w, rec;
    int16_t i;
    w = find_widget_checked_a78bc(root, 0x00184e4cu);
    (void)guest_sprintf_a4342(b, 0x00184c94u, (uint32_t)EF_U8(0x001a5530u));
    guest_fn_a7588_x(w, b);
    w = find_widget_checked_a78bc(root, 0x00184e54u);
    (void)guest_sprintf_a4342(b, 0x00184c94u, (uint32_t)EF_U8(0x001a5531u));
    guest_fn_a7588_x(w, b);
    w = find_widget_checked_a78bc(root, 0x00184e60u);
    guest_fn_a7588_x(w, EF_U32(EF_U32(0x001a5506u) + 0xcu));
    EF_U8(0x001a5533u) = 0u;
    for (i = 0; i < (int16_t)EF_U16(EF_U32(0x001a5506u) + 0x2au); i++) {
        uint32_t v;
        rec = EF_U32(0x001a5506u);
        v = EF_U8(EF_U32(rec + 0x2cu) + (uint32_t)((int32_t)i * 0x15));
        if (v == 0xffu || v == 0xfeu)
            continue;
        if (guest_fn_a4380(0x00184e68u, EF_U32(0x0013315cu + v * 4u)) == 0u)
            EF_U8(0x001a5533u) = (uint8_t)i;
    }
    fill_button_widgets_4f7d0();
}

/* 0xf41dc, 236 bytes. Show the splash picture NAME (EAX): clear byte 0x161e20, set byte 0x161e22, remember the request (0xf41a8), size the file "<0x191a7c with NAME>" with the hook at 0x132ab4 (falling back to the directory named by the environment variable 0x191a88 + 0x191a8c + NAME + 0x191a94, failure reported through 0x132a74 with 0x191a9c), allocate the data (0xc3f70, dword 0x485928, failure reported with 0x191ad0), load it with the hook at 0x132ab8, latch the 0x100 x 0xc8 picture rectangle (0xce328), reset the flags (0xf4178) and free the data. */
void show_splash_picture_f41dc(uint32_t name)
{
    char path[0xc8];
    uint32_t p = (uint32_t)(uintptr_t)path;
    uint32_t size;
    EF_U8(0x00161e20u) = 0u;
    EF_U8(0x00161e22u) = 1u;
    remember_request_f41a8();
    (void)guest_sprintf_a4342(p, 0x00191a7cu, name);
    size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), p);
    if (size == 0u) {
        copy_string_9d749(p, getenv_9c858(0x00191a88u));
        strcat_9ca57(p, 0x00191a8cu);
        strcat_9ca57(p, name);
        strcat_9ca57(p, 0x00191a94u);
        size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), p);
        if (size == 0u)
            guest_callptr2_x(EF_U32(0x00132a74u), 0x00191a9cu, p);
    }
    EF_U32(0x00485928u) = guest_fn_c3f70(size);
    if (EF_U32(0x00485928u) == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00191ad0u);
    guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), p, EF_U32(0x00485928u));
    latch_values_ce328(0u, 0x13fu, 0u, 0xc7u);
    reset_flags_f4178();
    guest_fn_c3f80(EF_U32(0x00485928u));
}

/* 0xabe44, 127 bytes. Split the string at STR (EAX) into whitespace-separated tokens (character class table 0x195af0 indexed by c+1, flag 2 = blank); after clearing the 0x150-byte token buffer at 0x41eab0 (0xabe24) each token is copied to its start, so on return the buffer holds the last token of the string. */
void copy_last_token_abe44(ef_addr32 str)
{
    ef_addr32 p = str;
    uint32_t n;
    uint32_t i;
    uint8_t c;
    clear_block_abe24();
    if (EF_U8(p) == 0u)
        return;
    for (;;) {
        n = 0u;
        for (;;) {
            i = n & 0xffffu;
            c = EF_U8(p + i);
            if (c == 0u || (EF_U8(0x00195af0u + (uint8_t)(c + 1u)) & 2u) != 0u)
                break;
            EF_U8(0x0041eab0u + i) = c;
            n++;
        }
        EF_U8(0x0041eab0u + (n & 0xffffu)) = 0u;
        p += n & 0xffffu;
        while ((EF_U8(0x00195af0u + (uint8_t)(EF_U8(p) + 1u)) & 2u) != 0u)
            p++;
        if (EF_U8(p) == 0u)
            return;
    }
}

/* 0xeff04, 106 bytes. Convert the 80-bit extended value at P (EAX) to a truncated int32: exponents below 0.5 give 0, magnitudes of 2^32 and above give 0x80000000, otherwise the top mantissa bits shifted down, negated for a set sign. */
uint32_t extended_to_int_eff04(ef_addr32 p)
{
    uint16_t se = EF_U16(p + 8u);
    int32_t e = (int32_t)(se & 0x7fffu) - 0x3ffe;
    uint32_t hi = EF_U32(p + 4u);
    uint32_t r;
    if (e < 0)
        return 0u;
    if (e > 0x20)
        return 0x80000000u;
    if (e == 0)
        r = 0u;
    else if (e == 0x20)
        r = hi;
    else
        r = hi >> (32 - e);
    if ((se & 0x8000u) != 0u)
        r = (uint32_t)(-(int32_t)r);
    return r;
}

/* 0xefb16, 153 bytes. Compare the 80-bit extended values at A (EAX) and B (EDX): 2 if either is a NaN, otherwise -1, 0 or 1 for A below, equal to or above B (+0 and -0 compare equal). */
uint32_t compare_extended_efb16(ef_addr32 a, ef_addr32 b)
{
    uint16_t sa = EF_U16(a + 8u);
    uint16_t sb = EF_U16(b + 8u);
    uint32_t below;
    if ((sa & 0x7fffu) == 0x7fffu && ((EF_U32(a + 4u) << 1) | EF_U32(a)) != 0u)
        return 2u;
    if ((sb & 0x7fffu) == 0x7fffu && ((EF_U32(b + 4u) << 1) | EF_U32(b)) != 0u)
        return 2u;
    if (((sa ^ sb) & 0x8000u) != 0u) {
        uint32_t any = EF_U32(a) | EF_U32(b) | EF_U32(a + 4u) | EF_U32(b + 4u)
                     | ((((uint32_t)sa << 16) | sb) & 0x7fff7fffu);
        if (any == 0u)
            return 0u;
        return (sa & 0x8000u) != 0u ? 0xffffffffu : 1u;
    }
    if (sa != sb) {
        below = sa < sb;
    } else if (EF_U32(a + 4u) != EF_U32(b + 4u)) {
        below = EF_U32(a + 4u) < EF_U32(b + 4u);
    } else if (EF_U32(a) != EF_U32(b)) {
        below = EF_U32(a) < EF_U32(b);
    } else {
        return 0u;
    }
    return (((sa >> 15) ^ below) & 1u) != 0u ? 0xffffffffu : 1u;
}

/* 0xceda0, 231 bytes. Move the 3-dword vector at VEC (EAX) along the direction given by the two angle words at ANG (EDX) by SCALE (EBX, low 16 bits): the angles are negated and reduced to 13-bit indexes into the word tables at 0x141e08 and 0x142e08, x/y/z get 15-bit fixed-point products added, then y is clamped to the signed range [dword 0x15b7c0, dword 0x15b7c4]. */
void offset_vector_by_angles_ceda0(ef_addr32 vec, ef_addr32 ang, uint32_t scale)
{
    uint32_t i0 = (uint32_t)(uint16_t)(0u - (uint32_t)EF_U16(ang)) >> 3;
    uint32_t i1 = (uint32_t)(uint16_t)(0u - (uint32_t)EF_U16(ang + 2u)) >> 3;
    uint16_t a0 = EF_U16(0x00141e08u + i0 * 2u);
    int32_t b0 = (int16_t)EF_U16(0x00142e08u + i0 * 2u);
    int32_t c1 = (int16_t)EF_U16(0x00141e08u + i1 * 2u);
    int32_t d1 = (int16_t)EF_U16(0x00142e08u + i1 * 2u);
    int32_t s = (int16_t)scale;
    int32_t tx = (int16_t)(-((c1 * b0) >> 15));
    int32_t ty = (int16_t)(0u - (uint32_t)a0);
    int32_t tz = (int16_t)((b0 * d1) >> 15);
    int32_t y;
    EF_U32(vec) = EF_U32(vec) + (uint32_t)((s * tx) >> 15);
    y = (int32_t)(EF_U32(vec + 4u) + (uint32_t)((s * ty) >> 15));
    EF_U32(vec + 8u) = EF_U32(vec + 8u) + (uint32_t)((s * tz) >> 15);
    EF_U32(vec + 4u) = (uint32_t)y;
    if (y > (int32_t)EF_U32(0x0015b7c4u))
        EF_U32(vec + 4u) = EF_U32(0x0015b7c4u);
    if ((int32_t)EF_U32(0x0015b7c0u) > (int32_t)EF_U32(vec + 4u))
        EF_U32(vec + 4u) = EF_U32(0x0015b7c0u);
}

/* 0x326d8, 263 bytes. Build the label text at 0x19e4da: copy a short constant chosen by byte 0x19e555 (0..7, else a default; 5 to 8 bytes each from 0x182708 and 0x182a44..0x182a7c), then when byte 0x19e558 is 4 or 5 append the suffix chosen by byte 0x19e556 (1..7 from 0x182a84.., values above 7 use 0x182acc, 0 appends nothing). */
void build_mode_label_326d8(void)
{
    static const uint32_t src[9] = {
        0x00182708u, 0x00182a44u, 0x00182a54u, 0x00182a4cu, 0x00182a5cu,
        0x00182a64u, 0x00182a6cu, 0x00182a74u, 0x00182a7cu
    };
    static const uint8_t len[9] = { 5u, 7u, 8u, 7u, 6u, 8u, 7u, 7u, 6u };
    static const uint32_t suffix[8] = {
        0u, 0x00182a84u, 0x00182a90u, 0x00182acc, 0x00182a9cu,
        0x00182aa8u, 0x00182ab4u, 0x00182ac0u
    };
    uint32_t k = EF_U8(0x0019e555u);
    uint32_t i;
    uint32_t m;
    uint32_t s;
    if (k > 7u)
        k = 8u;
    for (i = 0u; i < len[k]; i++)
        EF_U8(0x0019e4dau + i) = EF_U8(src[k] + i);
    m = EF_U8(0x0019e558u);
    if (m == 4u || m == 5u) {
        s = EF_U8(0x0019e556u);
        if (s > 7u)
            s = 3u;
        if (s != 0u)
            (void)strcat_9ca57(0x0019e4dau, suffix[s]);
    }
}

/* 0x84e3c, 289 bytes. Fetch two packed words for sub-entry SUB (BL) of the class record owned by unit UNIT (DX, signed) into OUT[0..1] (EAX): the unit's class (high half of dword +0x26) indexes the 0xca-byte records at dword 0x19bb58, a missing table (+0x8a) is reported through 0x132a74 with 0x189bf8; entries whose byte +0x18 is 5 or more are then replaced by 0x400 + (word << 11) style packed values built from the record at +0xb5. */
void fetch_entry_pair_84e3c(ef_addr32 out, uint32_t unit, uint32_t sub)
{
    int32_t u16 = (int16_t)unit;
    ef_addr32 u = guest_fn_7489c(u16);
    int32_t cls;
    ef_addr32 rec;
    ef_addr32 tbl;
    ef_addr32 p2;
    uint32_t off;
    uint32_t b;
    if (u == 0u)
        return;
    cls = (int32_t)EF_U32(u + 0x26u) >> 16;
    rec = EF_U32(0x0019bb58u) + (uint32_t)(cls * 0xca);
    if (EF_U32(rec + 0x8au) == 0u)
        guest_callptr3_x(EF_U32(0x00132a74u), 0x00189bf8u, (uint32_t)u16, (uint32_t)cls);
    off = (sub & 0xffu) * 25u;
    tbl = EF_U32(rec + 0x8au);
    EF_U32(out) = EF_U32(tbl + off + 4u);
    EF_U32(out + 4u) = EF_U32(tbl + off);
    if (EF_U8(tbl + off + 0x18u) < 5u)
        return;
    b = EF_U8(u + 0x2au);
    p2 = EF_U32(rec + 0xb5u);
    EF_U32(out + 4u) = (uint32_t)((int32_t)(int16_t)EF_U16(p2) << 11) + 0x400u
                     + (uint32_t)((int32_t)EF_U32(p2 + b * 6u + 0x16u) >> 16);
    EF_U32(out) = (uint32_t)(((int32_t)EF_U32(p2) >> 16) << 11) + 0x400u
                + (uint32_t)((int32_t)EF_U32(p2 + b * 6u + 0x18u) >> 16);
}

/* 0x9f850, 240 bytes. Queue the 29-byte record INDEX (AX, signed; table at [0x417174] + 0xb) as a table entry: run the hook at 0x132af4 with (0, 0), copy the words 0x41718e/0x417190 (+ word 0x417222)/0x417192 to 0x417188..8c, fill the descriptor 0x1600a8..b8 (record word, record + 0x14, scratch 0x417154, position 0x417188, count 1) and, when the record byte +0x1c is non-zero, add the first point of its list (at [0x41717c] + 2 * (dword +0x10 >> 16)) to the position and use that byte as the count; then append the entry (0x16018a). */
void queue_record_entry_9f850(uint32_t index)
{
    ef_addr32 rec = EF_U32(0x00417174u) + 0xbu + (uint32_t)((int32_t)(int16_t)index * 29);
    ef_addr32 list = EF_U32(0x0041717cu) + (uint32_t)(((int32_t)EF_U32(rec + 0x10u) >> 16) * 2);
    guest_callptr2r_x(EF_U32(0x00132af4u), 0, 0);
    EF_U16(0x00417188u) = EF_U16(0x0041718eu);
    EF_U16(0x0041718au) = (uint16_t)(EF_U16(0x00417190u) + EF_U16(0x00417222u));
    EF_U16(0x0041718cu) = EF_U16(0x00417192u);
    EF_U32(0x001600b4u) = 0x00417188u;
    EF_U32(0x001600b8u) = 1u;
    EF_U32(0x001600a8u) = (uint32_t)(int32_t)(int16_t)EF_U16(rec);
    EF_U32(0x001600acu) = rec + 0x14u;
    EF_U32(0x001600b0u) = 0x00417154u;
    if (EF_U8(rec + 0x1cu) != 0u) {
        EF_U16(0x00417188u) = (uint16_t)(EF_U16(0x00417188u) + EF_U16(list));
        EF_U16(0x0041718au) = (uint16_t)(EF_U16(0x0041718au) + EF_U16(list + 2u));
        EF_U16(0x0041718cu) = (uint16_t)(EF_U16(0x0041718cu) + EF_U16(list + 4u));
        EF_U32(0x001600b8u) = EF_U8(rec + 0x1cu);
    }
    append_table_entry_16018a();
}

/* 0xa348c, 247 bytes. Handle the event record INDEX (EAX; low 16 bits index the 32-byte records at 0x135878) by the type in the low nibble of its first record's byte +0xd: type 4 copies the name and flag (0xa3424); types 6, 8, 9 and 10 only act when the byte at the record's pointer (+0x13587c) equals word 0x1357ae: type 6 flushes the pending slots with the low byte of EAX (0xa374c), type 8 decrements and type 9 increments dword 0x136dd4, type 10 latches the clock hook 0x15b620 into word 0x1357d6 from the record word +0xb and stores the elapsed time in 0x136dd4; other types do nothing. */
void handle_event_record_a348c(uint32_t index)
{
    uint32_t i = index & 0xffffu;
    ef_addr32 rec = EF_U32(0x00135878u + (i << 5));
    ef_addr32 ptr = EF_U32(0x0013587cu + (i << 5));
    uint32_t type = (uint8_t)((EF_U8(rec + 0xdu) & 0xfu) - 4u);
    if (type > 6u)
        return;
    switch (type) {
    case 0:
        copy_name_and_flag_a3424((uint16_t)i);
        return;
    case 2:
        if (EF_U8(ptr) == EF_U16(0x001357aeu))
            flush_pending_a374c(index & 0xffu);
        return;
    case 4:
        if (EF_U8(ptr) == EF_U16(0x001357aeu))
            EF_U32(0x00136dd4u) = EF_U32(0x00136dd4u) - 1u;
        return;
    case 5:
        if (EF_U8(ptr) == EF_U16(0x001357aeu))
            EF_U32(0x00136dd4u) = EF_U32(0x00136dd4u) + 1u;
        return;
    case 6:
        if (EF_U8(ptr) == EF_U16(0x001357aeu)) {
            EF_U16(0x001357d6u) = EF_U16(rec + 0xbu);
            EF_U32(0x00136dd4u) = ((uint32_t (*)(void))EF_U32(0x0015b620u))() - EF_U16(0x001357d6u);
        }
        return;
    default:
        return;
    }
}

/* 0x7a194, 251 bytes. Full-screen choice prompt: clip to the screen (0xce328, 0xcdf48), draw the page chosen by byte 0x125846 (1: 0x12aeb8, 2: 0x12aef0, else 0x12af28) with 0x79ff0, then wait for a key code in byte 0x41f833 (reset to 0 before every wait): 0x01 or 0x2e gives 0, 0x10 gives 1, 0x12 gives 3 unless 0x125846 is 1, 0x1e gives 3 only when 0x125846 is 1; any other key is ignored. Returns the choice in AL. */
uint32_t run_choice_prompt_7a194(void)
{
    uint8_t result = 0u;
    uint8_t done = 0u;
    uint32_t w = EF_U32(0x0015b744u) - 1u;
    uint32_t h = EF_U32(0x0015b748u) - 1u;
    uint32_t page;
    uint8_t key;
    latch_values_ce328(0u, w, 0u, h);
    guest_fn_cdf48(0u, w, 0u, h);
    if (EF_U8(0x00125846u) == 1u)
        page = 0x0012aeb8u;
    else if (EF_U8(0x00125846u) == 2u)
        page = 0x0012aef0u;
    else
        page = 0x0012af28u;
    (void)guest_fn_79ff0(page);
    while (!done) {
        EF_U8(0x0041f833u) = 0u;
        do {
            key = EF_U8(0x0041f833u);
        } while (key == 0u);
        switch (key) {
        case 0x01: case 0x2e:
            done = 1u;
            result = 0u;
            break;
        case 0x12:
            if (EF_U8(0x00125846u) != 1u) {
                done = 1u;
                result = 3u;
            }
            break;
        case 0x1e:
            if (EF_U8(0x00125846u) == 1u) {
                done = 1u;
                result = 3u;
            }
            break;
        case 0x10:
            done = 1u;
            result = 1u;
            break;
        default:
            break;
        }
    }
    return result;
}

/* 0xde3b4, 251 bytes. Send the queued mouse request blocks starting at index START (EAX, low 16 bits signed; dword table at 0x425fd0): while the byte 0x15ee99 is set and the table entry is not -1, reset the mouse state (0xde4b0), build the 0x16-byte request at [0x42609c] (command 0x16, device byte 0x15ee98, subcommand 0x84, the entry at +0xe and the distance 0x4260ac - entry at +0x12), fill the real-mode call block 0x4260b4 (0x1510, the encoded request address, the dword 0x42611e high half) and issue call 0x2f (0x8b8); stop when the reply word at +3 is 0x300 or the index reaches the word at 0x426122, otherwise advance to the next entry. */
void send_mouse_requests_de3b4(uint32_t start)
{
    uint32_t ecx = start;
    if (EF_U8(0x0015ee99u) == 0u)
        return;
    for (;;) {
        uint32_t ent = 0x00425fd0u + (uint32_t)((int32_t)(int16_t)ecx * 4);
        ef_addr32 blk;
        uint32_t tab;
        uint32_t enc;
        if (EF_U32(ent) == 0xffffffffu)
            return;
        mouse_request_de4b0();
        EF_U8(0x00426147u) = 1u;
        blk = EF_U32(0x0042609cu);
        fill_bytes_b3fb0(blk, 0u, 0x16u);
        tab = EF_U32(ent);
        EF_U8(blk + 1u) = EF_U8(0x0015ee98u);
        EF_U32(blk + 0xeu) = tab;
        EF_U8(blk + 2u) = 0x84u;
        EF_U8(blk) = 0x16u;
        EF_U32(blk + 0x12u) = EF_U32(0x004260acu) - tab;
        enc = encode_runtime_segment_offset(blk);
        EF_U32(0x004260d0u) = 0x1510u;
        EF_U32(0x004260c4u) = enc & 0xffffu;
        EF_U16(0x004260d6u) = (uint16_t)((int32_t)enc >> 16);
        EF_U32(0x004260ccu) = (uint32_t)((int32_t)EF_U32(0x0042611eu) >> 16);
        EF_U32(0x004260c8u) = 0u;
        guest_fn_8b8(0x2fu, 0x004260b4u);
        blk = EF_U32(0x0042609cu);
        if (((int32_t)EF_U32(blk + 1u) >> 16) == 0x300)
            return;
        if ((int16_t)ecx >= (int16_t)EF_U16(0x00426122u))
            return;
        ecx++;
    }
}

/* 0x38be8, 251 bytes. Draw the framed text panel for slot ID (AX, signed), vertical position Y (EDX) and X (EBX): fetch the panel image of the slot in the top byte of dword 0x125acc (0xcd7f0, nothing is drawn when absent), enter scroll mode with it, clip to 0x100 x 0xc0 (0xcdf48, 0xce328), draw the title record 0x125a24 sized from the rectangle words 0x125ac4..0x125ac8 (0xcf7c4), fetch the image of slot ID and, when present, enter scroll mode with it and blit the three border pieces sized from word 0x125aca (0xcd82c); finally leave scroll mode (0xc39ac). */
void draw_framed_panel_38be8(uint32_t id, uint32_t y, uint32_t x)
{
    uint32_t img = guest_fn_cd7f0((int32_t)EF_U32(0x00125accu) >> 24);
    int32_t hi_c4;
    int32_t hi_c8;
    int32_t hi_c6;
    uint32_t t;
    uint32_t b;
    uint32_t e1;
    uint32_t e2;
    uint32_t e3;
    if (img == 0u)
        return;
    enter_scroll_mode_c39f0(img);
    guest_fn_cdf48(0u, 0xffu, 0u, 0xbfu);
    latch_values_ce328(0u, 0xffu, 0u, 0xbfu);
    hi_c4 = (int32_t)EF_U32(0x00125ac4u) >> 16;
    hi_c8 = (int32_t)EF_U32(0x00125ac8u) >> 16;
    hi_c6 = (int32_t)EF_U32(0x00125ac6u) >> 16;
    guest_fn_cf7c4_x(0x00125a24u, (uint32_t)(hi_c6 - hi_c8), (uint32_t)(hi_c4 - hi_c8), 0u);
    t = guest_fn_cd7f0((int16_t)id);
    if (t != 0u) {
        enter_scroll_mode_c39f0(t);
        b = EF_U16(0x00125acau);
        e1 = (EF_U16(0x00125ac6u) - b) & 0xffffu;
        e2 = (EF_U16(0x00125ac8u) - b) & 0xffffu;
        e3 = (b + b) & 0xffffu;
        blit_masked_cd82c(0u, 0u, 0u, 0u, (x - b) & 0xffffu, (y - b) & 0xffffu, e3, e3, e2, e1, img);
    }
    leave_scroll_mode_c39ac();
}

/* 0x107310, 246 bytes. Load the 0x4000-byte colour table from the file named at 0x17d368: on first use allocate 0x10100 bytes (dword 0x4dd860, failure reported with 0x194c18), register the cleanup 0x1072f0 (0x9c3cf) and set the 256-aligned table base words 0x164604/0x164608; then fill the first 0x4000 bytes with 64 identity ramps, build the path (0x9c9c0), size it with the hook at 0x132ab4 (above 0x4000 reported with 0x194c30), load it over the ramps with the hook at 0x132ab8, and duplicate the table to base + 0x4000 and then the first 0x8000 bytes to base + 0x8000. */
void load_colour_table_107310(void)
{
    char path[0x50];
    uint32_t p = (uint32_t)(uintptr_t)path;
    uint32_t base;
    uint32_t blk;
    uint32_t i;
    if (EF_U32(0x004dd860u) == 0u) {
        EF_U32(0x004dd860u) = guest_fn_c3f70(0x10100u);
        if (EF_U32(0x004dd860u) == 0u)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x00194c18u);
        guest_fn_9c3cf(0x001072f0u);
        EF_U32(0x00164604u) = (EF_U32(0x004dd860u) + 0xffu) & 0xffffff00u;
        EF_U32(0x00164608u) = EF_U32(0x00164604u);
    }
    base = EF_U32(0x00164604u);
    for (blk = base; blk != base + 0x4000u; blk += 0x100u)
        for (i = 0u; i < 0x100u; i++)
            EF_U8(blk + i) = (uint8_t)i;
    build_path_9c9c0(p, 0x0017d368u);
    if (guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), p) > 0x4000u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00194c30u);
    guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), p, base);
    for (i = 0u; i < 0x4000u; i++)
        EF_U8(base + 0x4000u + i) = EF_U8(base + i);
    (void)memcpy_dc832(EF_U32(0x00164604u) + 0x8000u, EF_U32(0x00164604u), 0x8000u);
}

/* 0x114ad8, 244 bytes. Release state when dword 0x17e25c is set: unregister the range 0x111c90..0x114ad8 (0x1048b4), unlock the data blocks 0x17e1d8 (0x80 bytes) and the 4-byte cells 0x17e258, 0x4ddccc, 0x4ddcd0, 0x4ddcd8, 0x4ddcdc, 0x4ddcb8, 0x4ddcbc, 0x4ddcb4, 0x4ddcc0, 0x4ddcc8, 0x4ddcc4 and 0x4ddcb0 (0x104920), then clear the flag. */
void release_locked_ranges_114ad8(void)
{
    static const uint32_t cell[12] = {
        0x0017e258u, 0x004ddcccu, 0x004ddcd0u, 0x004ddcd8u, 0x004ddcdcu, 0x004ddcb8u,
        0x004ddcbcu, 0x004ddcb4u, 0x004ddcc0u, 0x004ddcc8u, 0x004ddcc4u, 0x004ddcb0u
    };
    uint32_t i;
    if (EF_U32(0x0017e25cu) == 0u)
        return;
    guest_fn_1048b4(0x00111c90u, 0x00114ad8u);
    guest_fn_104920(0x0017e1d8u, 0x80u);
    for (i = 0u; i < 12u; i++)
        guest_fn_104920(cell[i], 4u);
    EF_U32(0x0017e25cu) = 0u;
}

/* 0xa4b80, 259 bytes. Close the widget ID (AX; valid 1..word 0x41eed8 in the pointer table 0x41edfc, only when flag 4 of its byte +0x3b is set): remove it with its children (0xa4a94), run its close hook (dword +0xbd, called with the widget in EAX and EDX), and for widgets whose byte +0xc1 is 1 find it in the open-stack words at 0x41ed84 (count word 0x41eed6), close every later entry and truncate the stack to it; an empty stack clears byte 0x41f2e0. Finally redraw (0xa6038). */
void close_widget_a4b80(uint32_t id)
{
    uint32_t w = id & 0xffffu;
    ef_addr32 obj;
    int32_t idx;
    int32_t k;
    if (w == 0u || (uint16_t)w > EF_U16(0x0041eed8u))
        return;
    obj = EF_U32(0x0041edfcu + w * 4u);
    if ((EF_U8(obj + 0x3bu) & 4u) == 0u)
        return;
    remove_entry_and_children_a4a94(w);
    obj = EF_U32(0x0041edfcu + w * 4u);
    if (EF_U32(obj + 0xbdu) != 0u)
        guest_callptr_eaxedx_x(EF_U32(obj + 0xbdu), obj, obj);
    if (EF_U8(EF_U32(0x0041edfcu + w * 4u) + 0xc1u) == 1u) {
        idx = -1;
        for (k = 0; k < (int32_t)EF_U16(0x0041eed6u) && idx == -1; k++) {
            if ((uint16_t)w == EF_U16(0x0041ed84u + (uint32_t)k * 2u))
                idx = k;
        }
        if (idx >= 0) {
            if (idx < (int32_t)EF_U16(0x0041eed6u) - 1) {
                for (k = idx + 1; k < (int32_t)EF_U16(0x0041eed6u); k++)
                    remove_entry_and_children_a4a94(EF_U16(0x0041ed84u + (uint32_t)k * 2u));
            }
            EF_U16(0x0041eed6u) = (uint16_t)idx;
        }
        if (EF_U16(0x0041eed6u) == 0u)
            EF_U8(0x0041f2e0u) = 0u;
    }
    redraw_entries_a6038();
}

/* 0x7bc0, 231 bytes. Pick a roster slot for the unit UNIT (AX, signed) in group GROUP (DL): keep the unit's own slot (byte +0x20b) when it still matches (0x7b38), otherwise take the first slot of the group (count in byte +0x15 of the 0x22-byte record at [0x1972e0]) that matches, reporting a missing slot table (dword +0x1e, 0x1803d4 through 0x132a74) on the way; then assign it (0x740e4 with the unit's bytes +0x209, +0x20a, +0x16d). Returns the slot, or 0xff when the unit does not exist or no slot fits. */
uint32_t pick_roster_slot_7bc0(uint32_t unit, uint32_t group)
{
    int32_t id = (int16_t)unit;
    uint8_t g = (uint8_t)group;
    uint8_t best = 0xffu;
    ef_addr32 u = guest_fn_7489c(id);
    ef_addr32 rec;
    int16_t i;
    if (u == 0u)
        return 0xffu;
    if (EF_U8(u + 0x20bu) != 0xffu && slot_matches_unit_7b38(g, EF_U8(u + 0x20bu), (int16_t)id) != 0u)
        best = EF_U8(u + 0x20bu);
    for (i = 0;; i++) {
        rec = EF_U32(0x001972e0u) + (uint32_t)g * 0x22u;
        if (i >= (int16_t)EF_U8(rec + 0x15u))
            break;
        if (EF_U32(rec + 0x1eu) == 0u)
            guest_callptr3_x(EF_U32(0x00132a74u), 0x001803d4u, best, g);
        if (best == 0xffu && slot_matches_unit_7b38(g, (uint8_t)i, (int16_t)id) != 0u)
            best = (uint8_t)i;
    }
    assign_roster_slot_740e4((uint32_t)id, EF_U8(u + 0x209u), EF_U8(u + 0x20au), EF_U8(u + 0x16du), best);
    return best;
}

/* 0x9b668, 278 bytes. Rebuild the per-side tallies: clear the two 0x18-byte tables at 0x40ce70 and 0x40cf78, then for every node of the list at 0x137970 whose type byte +0xd is 0, 2, 3 or 4 test its entry byte +0xe against the list 0x137980 (positive 0xcc22c, else negative 0xcc1ac, else zero 0xcc1ec) and increment the matching word (+0, +4 or +2) of the 6-byte row selected by the node's side byte +0xb in both tables. */
void count_side_tallies_9b668(void)
{
    ef_addr32 node;
    uint32_t row;
    uint32_t off;
    uint32_t t;
    uint32_t idx;
    fill_bytes_b3fb0(0x0040ce70u, 0u, 0x18u);
    fill_bytes_b3fb0(0x0040cf78u, 0u, 0x18u);
    for (node = EF_U32(0x00137970u); node != 0u; node = EF_U32(node + 4u)) {
        t = EF_U8(node + 0xdu);
        if (!(t == 0u || t == 2u || t == 3u || t == 4u))
            continue;
        idx = EF_U8(node + 0xeu);
        if ((uint16_t)group_flag_positive_cc22c(0u, idx, 0x00137980u) != 0u)
            off = 0u;
        else if ((uint16_t)group_flag_negative_cc1ac(0u, idx, 0x00137980u) != 0u)
            off = 4u;
        else if ((uint16_t)group_flag_zero_cc1ec(0u, idx, 0x00137980u) != 0u)
            off = 2u;
        else
            continue;
        row = (uint32_t)EF_U8(node + 0xbu) * 6u + off;
        EF_U16(0x0040ce70u + row) = (uint16_t)(EF_U16(0x0040ce70u + row) + 1u);
        EF_U16(0x0040cf78u + row) = (uint16_t)(EF_U16(0x0040cf78u + row) + 1u);
    }
}

/* 0xb49b4, 288 bytes. Search two linked lists for a type-0x31 node of side GROUP (BL, 0..5) that is negative (0xcc1ac) in the list LIST (ECX), followed by a positive (0xcc22c) type-0x31 node along the second link; returns the word at +9 of the first positive node, or 0. Groups 0, 2 and 5 start from the head pointed to by A (EAX) and walk the outer list by link +4 and the inner by link +0; groups 1, 3 and 4 start from the head pointed to by B (EDX) with the links swapped; other groups return 0. */
uint32_t find_paired_node_b49b4(ef_addr32 a, ef_addr32 b, uint32_t group, ef_addr32 list)
{
    uint8_t g = (uint8_t)group;
    uint32_t outer_off;
    uint32_t inner_off;
    ef_addr32 node;
    ef_addr32 in;
    if (g > 5u)
        return 0u;
    if (g == 0u || g == 2u || g == 5u) {
        node = EF_U32(a);
        outer_off = 4u;
        inner_off = 0u;
    } else {
        node = EF_U32(b);
        outer_off = 0u;
        inner_off = 4u;
    }
    if (node == 0u)
        return 0u;
    while (node != 0u) {
        if (EF_U8(node + 0xcu) == 0x31u
            && (uint16_t)group_flag_negative_cc1ac(g, EF_U8(node + 0xbu), list) != 0u) {
            in = EF_U32(node + inner_off);
            if (in == 0u)
                return EF_U16(node + 9u);
            while (in != 0u) {
                if (EF_U8(in + 0xcu) == 0x31u
                    && (uint16_t)group_flag_positive_cc22c(g, EF_U8(in + 0xbu), list) != 0u)
                    return EF_U16(in + 9u);
                in = EF_U32(in + inner_off);
            }
            /* ORIGINAL BUG: the outer step reuses the exhausted (null) inner pointer, reading the link at guest address 0 or 4 instead of the outer node's. */
            node = EF_U32(0u + outer_off);
        } else {
            node = EF_U32(node + outer_off);
        }
    }
    return 0u;
}

/* 0x9b780, 287 bytes. Remove the contribution of each active entry to the side tallies: for every 0x36-byte entry of the table at [0x1379bc] (count dword 0x1379b0) whose dword +0xa has high half 1, find the first node of the list 0x137970 whose word +0x13 equals the entry's word +0x1a and decrement the word (+0 positive, +4 negative or +2 zero, tested with 0xcc22c, 0xcc1ac, 0xcc1ec against 0x137980) of the 6-byte row at 0x40ce70 chosen by the high half of the entry's dword +0xe. */
void uncount_side_tallies_9b780(void)
{
    int16_t k;
    for (k = 0; (int32_t)k < (int32_t)EF_U32(0x001379b0u); k++) {
        ef_addr32 rec = EF_U32(0x001379bcu) + (uint32_t)((int32_t)k * 0x36);
        ef_addr32 node;
        uint16_t found = 0u;
        uint32_t row;
        if (((int32_t)EF_U32(rec + 0xau) >> 16) != 1)
            continue;
        for (node = EF_U32(0x00137970u); node != 0u && found == 0u; node = EF_U32(node + 4u)) {
            if (EF_U16(node + 0x13u) != EF_U16(rec + 0x1au))
                continue;
            found = 1u;
            row = (uint32_t)(((int32_t)EF_U32(EF_U32(0x001379bcu) + (uint32_t)((int32_t)k * 0x36) + 0xeu) >> 16) * 6);
            if ((uint16_t)group_flag_positive_cc22c(0u, EF_U8(node + 0xeu), 0x00137980u) != 0u)
                EF_U16(0x0040ce70u + row) = (uint16_t)(EF_U16(0x0040ce70u + row) - 1u);
            else if ((uint16_t)group_flag_negative_cc1ac(0u, EF_U8(node + 0xeu), 0x00137980u) != 0u)
                EF_U16(0x0040ce74u + row) = (uint16_t)(EF_U16(0x0040ce74u + row) - 1u);
            else if ((uint16_t)group_flag_zero_cc1ec(0u, EF_U8(node + 0xeu), 0x00137980u) != 0u)
                EF_U16(0x0040ce72u + row) = (uint16_t)(EF_U16(0x0040ce72u + row) - 1u);
        }
    }
}

/* 0xbe208, 278 bytes. Assign free nodes to the request record REC (ECX; node count in byte +3, kind byte +8): walk the node list whose head pointer is stored at HEADPTR (stack argument, ret 4) and, for every idle node (word +0x31 equal to OWNER (EAX low 16 bits), bytes +0xb and +0xd zero, byte +0xc equal to the record's kind, word +9 zero) while fewer than the record's count have been taken, mark it busy (word +9 = 1), store the values TARGET (EDX) and VALUE (EBX) and the running index into the first of its four 2-byte link groups whose three words (+0x17, +0x1f, +0x27 high halves) are all -1, write the index to byte +4 of the record at dword +0x4d, and clear words +0x51 and +0x53. */
void claim_idle_nodes_be208(uint32_t owner, uint32_t target, uint32_t value, ef_addr32 rec, ef_addr32 headptr)
{
    ef_addr32 node = EF_U32(headptr);
    uint16_t k = 0u;
    while (node != 0u && (int16_t)EF_U8(rec + 3u) > (int16_t)k) {
        if ((uint16_t)EF_U16(node + 0x31u) == (uint16_t)owner
            && EF_U8(node + 0xbu) == 0u
            && EF_U8(node + 0xcu) == EF_U8(rec + 8u)
            && EF_U16(node + 9u) == 0u
            && EF_U8(node + 0xdu) == 0u) {
            uint16_t flag = 1u;
            int16_t i = 0;
            EF_U16(node + 9u) = 1u;
            while ((int32_t)i < 4 && flag != 0u) {
                ef_addr32 p = node + (uint32_t)(uint16_t)i * 2u;
                if (((int32_t)EF_U32(p + 0x27u) >> 16) == -1
                    && ((int32_t)EF_U32(p + 0x17u) >> 16) == -1
                    && ((int32_t)EF_U32(p + 0x1fu) >> 16) == -1) {
                    EF_U16(p + 0x19u) = (uint16_t)target;
                    EF_U16(p + 0x29u) = k;
                    EF_U16(p + 0x21u) = (uint16_t)value;
                    flag = 0u;
                }
                i++;
            }
            EF_U8(EF_U32(node + 0x4du) + 4u) = (uint8_t)k;
            k++;
            EF_U16(node + 0x51u) = 0u;
            EF_U16(node + 0x53u) = 0u;
        }
        node = EF_U32(node + 4u);
    }
}

/* 0xcaea0, 296 bytes. Refresh the cached 18-byte blocks of a node list from a table of COUNT (EBX, signed 16 bits) 0x1b-byte entries at TABLE (EDX): for every node of the list whose head pointer is stored at HEADPTR (EAX) and every entry, test the word pairs (node +0x1f/+0x27, +0x1d/+0x25, +0x1b/+0x23, +0x19/+0x21) against the entry words +3/+5 in that order; the first pair that matches and whose cached block at node +0x3b differs from the entry's block at +9 gets the block copied over, matching blocks leave it alone and the search continues. */
void refresh_node_blocks_caea0(ef_addr32 headptr, ef_addr32 table, uint32_t count)
{
    static const uint8_t pa[4] = { 0x1fu, 0x1du, 0x1bu, 0x19u };
    static const uint8_t pb[4] = { 0x27u, 0x25u, 0x23u, 0x21u };
    ef_addr32 node = EF_U32(headptr);
    int16_t i;
    int j;
    uint32_t n;
    for (; node != 0u; node = EF_U32(node + 4u)) {
        for (i = 0; (int16_t)count > 0 && i < (int16_t)count; i++) {
            ef_addr32 e = table + (uint32_t)((int32_t)i * 0x1b);
            for (j = 0; j < 4; j++) {
                if (EF_U16(node + pa[j]) != EF_U16(e + 3u) || EF_U16(node + pb[j]) != EF_U16(e + 5u))
                    continue;
                if (EF_U32(node + 0x3bu) != EF_U32(e + 9u) || EF_U32(node + 0x3fu) != EF_U32(e + 0xdu)) {
                    for (n = 0u; n < 18u; n++)
                        EF_U8(node + 0x3bu + n) = EF_U8(e + 9u + n);
                    break;
                }
            }
        }
    }
}

/* 0x6563c, 267 bytes. Apply the pending 8-byte notices at 0x1a6d2e (count word 0x1a81b2, only while byte 0x1a81e5 is set): look up each node (0x653d8; kind = word +4, id = word +2), mark it with byte +0xd = 1, and by kind set byte +0x13f of the unit (kind 1, 0x7489c), byte +0x3d of the object (kind 2, 0x8ffe8) or run 0x87f9c with the top nibble of the first word and the low 12 bits of the id (kind 4); afterwards advance the clock word 0x1a81be by word 0x127d92, wrapping by 0x19 when it exceeds 0x19. */
void apply_pending_notices_6563c(void)
{
    int16_t i;
    if (EF_U8(0x001a81e5u) == 0u || EF_U16(0x001a81b2u) == 0u)
        return;
    for (i = 0; i < (int16_t)EF_U16(0x001a81b2u); i++) {
        ef_addr32 b = 0x001a6d2eu + (uint32_t)((int32_t)i * 8);
        int32_t id = (int16_t)EF_U16(b + 2u);
        uint16_t kind = EF_U16(b + 4u);
        ef_addr32 node = find_node_653d8(kind, (uint32_t)id);
        ef_addr32 o;
        if (node == 0u)
            continue;
        EF_U8(node + 0xdu) = 1u;
        switch (kind) {
        case 1:
            o = guest_fn_7489c(id);
            if (o != 0u)
                EF_U8(o + 0x13fu) = 1u;
            break;
        case 2:
            o = guest_fn_8ffe8(id);
            if (o != 0u)
                EF_U8(o + 0x3du) = 1u;
            break;
        case 4:
            (void)guest_fn_87f9c((uint32_t)(((int32_t)EF_U32(b) >> 28) & 0xff), (uint32_t)(int32_t)(int16_t)(EF_U16(b + 2u) & 0x0fffu));
            break;
        default:
            break;
        }
    }
    EF_U16(0x001a81beu) = (uint16_t)(EF_U16(0x001a81beu) + EF_U16(0x00127d92u));
    if ((int16_t)EF_U16(0x001a81beu) > 0x19)
        EF_U16(0x001a81beu) = (uint16_t)(EF_U16(0x001a81beu) - 0x19u);
}

/* 0x110a08, 296 bytes. Decode a column-compressed sprite from SRC (EAX, word stream) into DST (EDX) with row stride STRIDE (BX, signed): the first word is the column count; each column starts with words whose top two bits are set (skip |w| rows), then a span count n (a negative word w means n is the next word and a trailing byte w is stored after the spans); each span is a byte skip, a signed word count c, then c literal words (c > 0) or one word repeated -c times (c < 0). Columns advance by STRIDE from where their data started. */
void decode_column_sprite_110a08(ef_addr32 src, ef_addr32 dst, uint32_t stride)
{
    int32_t st = (int16_t)stride;
    int16_t ncols = (int16_t)EF_U16(src);
    int16_t col;
    src += 2u;
    for (col = 0; col < ncols; col++) {
        uint16_t w = EF_U16(src);
        int16_t n;
        int16_t j;
        ef_addr32 start;
        src += 2u;
        while ((w & 0xc000u) == 0xc000u) {
            dst -= (uint32_t)((int32_t)(int16_t)w * st);
            w = EF_U16(src);
            src += 2u;
        }
        start = dst;
        n = (int16_t)w;
        if (n < 0) {
            n = (int16_t)EF_U16(src);
            src += 2u;
        }
        for (j = 0; n > 0 && j < n; j++) {
            int8_t c;
            int16_t b;
            dst += EF_U8(src);
            src++;
            c = (int8_t)EF_U8(src);
            src++;
            if (c > 0) {
                for (b = 0; b < (int16_t)c; b++) {
                    EF_U16(dst) = EF_U16(src);
                    dst += 2u;
                    src += 2u;
                }
            }
            if (c < 0) {
                uint16_t v = EF_U16(src);
                src += 2u;
                for (b = 0; b < (int16_t)-c; b++) {
                    EF_U16(dst) = v;
                    dst += 2u;
                }
            }
        }
        if ((int16_t)w < 0)
            EF_U8(dst) = (uint8_t)w;
        dst = (uint32_t)st + start;
    }
}

/* 0xf3904, 277 bytes. Draw a box outline from the signed 16-bit coordinates A0 (EAX), A1 (EDX), A2 (EBX) and A3 (ECX): after saving the screen block (0xdd870, plus 0xc386c while byte 0x4859de is set) four lines of colour 0 are drawn one pixel inside at A1+1/A3+1 by A0+1/A2+1, then four lines in the colour held in the high half of dword 0x161e1a on the outer rectangle, using the line routine 0xc8ccc; finally the block is redrawn (0xdd8dc, plus 0xdc250 while the flag is set). */
void draw_shadow_box_f3904(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    int32_t v0 = (int16_t)a0;
    int32_t u0 = (int16_t)a1;
    int32_t v1 = (int16_t)a2;
    int32_t u1 = (int16_t)a3;
    uint32_t c;
    restore_saved_block_dd870();
    if (EF_U8(0x004859deu) != 0u)
        guest_fn_c386c();
    guest_fn_c8ccc_x((uint32_t)(u0 + 1), (uint32_t)(v1 + 1), (uint32_t)(u0 + 1), (uint32_t)(v0 + 1), 0u);
    guest_fn_c8ccc_x((uint32_t)(u1 + 1), (uint32_t)(v1 + 1), (uint32_t)(u0 + 1), (uint32_t)(v1 + 1), 0u);
    guest_fn_c8ccc_x((uint32_t)(u1 + 1), (uint32_t)(v0 + 1), (uint32_t)(u1 + 1), (uint32_t)(v1 + 1), 0u);
    guest_fn_c8ccc_x((uint32_t)(u0 + 1), (uint32_t)(v0 + 1), (uint32_t)(u1 + 1), (uint32_t)(v0 + 1), 0u);
    c = (uint32_t)((int32_t)EF_U32(0x00161e1au) >> 16);
    guest_fn_c8ccc_x((uint32_t)u0, (uint32_t)v1, (uint32_t)u0, (uint32_t)v0, c);
    c = (uint32_t)((int32_t)EF_U32(0x00161e1au) >> 16);
    guest_fn_c8ccc_x((uint32_t)u1, (uint32_t)v1, (uint32_t)u0, (uint32_t)v1, c);
    c = (uint32_t)((int32_t)EF_U32(0x00161e1au) >> 16);
    guest_fn_c8ccc_x((uint32_t)u1, (uint32_t)v0, (uint32_t)u1, (uint32_t)v1, c);
    c = (uint32_t)((int32_t)EF_U32(0x00161e1au) >> 16);
    guest_fn_c8ccc_x((uint32_t)u0, (uint32_t)v0, (uint32_t)u1, (uint32_t)v0, c);
    save_and_draw_block_dd8dc();
    if (EF_U8(0x004859deu) != 0u)
        guest_fn_dc250();
}

/* 0xfe380, 284 bytes. Draw the arc from angle LO (ECX) to angle HI (stack, ret 8) of radius RADIUS (stack) around (CX = EBX added to the cosine term, CY = EDX added to the sine term) in colour COLOUR (EAX). The recursion depth counter dword 0x485fb0 is decremented; while it stays positive the arc is split at the midpoint angle (toward-zero average of HI and LO) and both halves are drawn, otherwise one line segment between the two end points (13-bit angle index into the word tables 0x141e08 and 0x142e08, product with the radius divided by 0x8000 truncating toward zero, cosine term negated) is drawn with 0xc8ccc. The counter is restored on exit. */
void draw_arc_fe380(uint32_t colour, uint32_t cy, uint32_t cx, uint32_t lo, uint32_t hi, uint32_t radius)
{
    uint32_t d;
    EF_U32(0x00485fb0u) = EF_U32(0x00485fb0u) - 1u;
    d = EF_U32(0x00485fb0u);
    if ((int32_t)d > 0) {
        int32_t sum = (int32_t)hi + (int32_t)lo;
        uint32_t mid = (uint32_t)((sum - (sum >> 31)) >> 1);
        draw_arc_fe380(colour, cy, cx, lo, mid, radius);
        draw_arc_fe380(colour, cy, cx, mid, hi, radius);
    } else {
        uint32_t i0 = ((uint32_t)(uint16_t)lo >> 3) * 2u;
        uint32_t i1 = ((uint32_t)(uint16_t)hi >> 3) * 2u;
        int32_t s0 = (int16_t)EF_U16(0x00141e08u + i0);
        int32_t c0 = (int16_t)EF_U16(0x00142e08u + i0);
        int32_t s1 = (int16_t)EF_U16(0x00141e08u + i1);
        int32_t c1 = (int16_t)EF_U16(0x00142e08u + i1);
        int32_t r = (int32_t)radius;
        uint32_t y0 = (uint32_t)((int32_t)((int64_t)s0 * r / 0x8000)) + cy;
        uint32_t x0 = (uint32_t)((int32_t)((int64_t)c0 * r / -0x8000)) + cx;
        uint32_t y1 = (uint32_t)((int32_t)((int64_t)s1 * r / 0x8000)) + cy;
        uint32_t x1 = (uint32_t)((int32_t)((int64_t)c1 * r / -0x8000)) + cx;
        guest_fn_c8ccc_x(x1, y1, x0, y0, colour);
    }
    EF_U32(0x00485fb0u) = EF_U32(0x00485fb0u) + 1u;
}

/* 0x4cff4, 285 bytes. Fill in the labels of the dialog 0x184a58. Without a current entry (dword 0x1a54f6 zero) set the first label (0x184cf8) from byte 0x1a559a (1: 0x13058d, 2: 0x13059e, 3 and 4: 0x131fcc, otherwise unchanged) and the other two (0x184d00, 0x184d0c) to the empty/default texts 0x13231c and 0x132326. With an entry, set the first label from the entry's text pointer (+0xc), the second from "%d" (format 0x184c94) of its byte +0x30, and the third from 0x132335 when 0xbc7ec says so for the entry, else 0x132326. */
void fill_dialog_labels_4cff4(void)
{
    char buf[0x34];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    uint32_t root = guest_fn_a46cc(0x00184a58u);
    uint32_t node = EF_U32(0x001a54f6u);
    uint32_t w;
    uint32_t text;
    if (node == 0u) {
        w = find_widget_checked_a78bc(root, 0x00184cf8u);
        switch (EF_U8(0x001a559au)) {
        case 1: text = 0x0013058du; break;
        case 2: text = 0x0013059eu; break;
        case 3: case 4: text = 0x00131fccu; break;
        default: text = 0u; break;
        }
        if (text != 0u)
            guest_fn_a7588_x(w, text);
        w = find_widget_checked_a78bc(root, 0x00184d00u);
        guest_fn_a7588_x(w, 0x0013231cu);
        w = find_widget_checked_a78bc(root, 0x00184d0cu);
        guest_fn_a7588_x(w, 0x00132326u);
        return;
    }
    w = find_widget_checked_a78bc(root, 0x00184cf8u);
    guest_fn_a7588_x(w, EF_U32(node + 0xcu));
    w = find_widget_checked_a78bc(root, 0x00184d00u);
    (void)guest_sprintf_a4342(b, 0x00184c94u, (uint32_t)EF_U8(node + 0x30u));
    guest_fn_a7588_x(w, b);
    copy_string_9d749(b, (uint16_t)guest_fn_bc7ec(node) != 0u ? 0x00132335u : 0x00132326u);
    w = find_widget_checked_a78bc(root, 0x00184d0cu);
    guest_fn_a7588_x(w, b);
}

/* 0xfe49c, 165 bytes. Draw an arc from angle LO (ECX) to angle HI (stack, ret 8; both reduced to 16 bits, HI moved a full turn up when it is not above LO) with radius RADIUS (stack) around (CX = EBX, CY = EDX) in colour COLOUR (EAX): pick the subdivision depth for dword 0x485fb0 from the scaled arc length (span * 44 * radius / 0x70000: 5 when negative or above 0xfde8, otherwise 1 plus the number of times it can be shifted right by two before reaching zero) and draw it with 0xfe380. */
void draw_arc_by_length_fe49c(uint32_t colour, uint32_t cy, uint32_t cx, uint32_t lo, uint32_t hi, uint32_t radius)
{
    int32_t l = (int32_t)(lo & 0xffffu);
    int32_t h = (int32_t)(hi & 0xffffu);
    int32_t span;
    int32_t q;
    uint32_t depth = 1u;
    if (l > h)
        h += 0x10000;
    span = h - l;
    q = (int32_t)((int64_t)(int32_t)((uint32_t)span * (radius * 44u)) / 0x70000);
    if (q < 0 || q > 0xfde8) {
        depth = 5u;
    } else {
        while (q != 0) {
            q >>= 2;
            depth++;
        }
    }
    EF_U32(0x00485fb0u) = depth;
    draw_arc_fe380(colour, cy, cx, (uint32_t)l, (uint32_t)h, radius);
}

/* 0x654b8, 181 bytes. Per-frame update of the notice list headed by dword 0x1a7c54 (link at +0x16): clear the notice count word 0x1a81b2, then for every record by its kind byte +0xc: kinds 1 and 2 insert it into the nearest list (0x65570) unless byte +0x10 is 1, kind 3 runs the range check 0x657c4, kind 4 inserts it and runs 0x65748; every record then ages its word +0x1a by word 0x127d92, clears byte +0xd and is released (0x65360, kind and the id word +0xe) once the age reaches 250. Afterwards advance both countdowns (0x658cc, 0x6583c) and apply the pending notices (0x6563c). */
void update_notice_list_654b8(void)
{
    ef_addr32 node;
    ef_addr32 next;
    uint8_t kind;
    EF_U16(0x001a81b2u) = 0u;
    for (node = EF_U32(0x001a7c54u); node != 0u; node = next) {
        kind = EF_U8(node + 0xcu);
        next = EF_U32(node + 0x16u);
        switch ((uint8_t)(kind - 1u)) {
        case 0: case 1:
            if (EF_U8(node + 0x10u) != 1u)
                insert_nearest_65570(node);
            break;
        case 2:
            check_range_657c4(node);
            break;
        case 3:
            insert_nearest_65570(node);
            check_range_65748(node);
            break;
        default:
            break;
        }
        EF_U16(node + 0x1au) = (uint16_t)(EF_U16(node + 0x1au) + EF_U16(0x00127d92u));
        EF_U8(node + 0xdu) = 0u;
        if ((int16_t)EF_U16(node + 0x1au) >= 0xfa)
            release_entry_65360(EF_U8(node + 0xcu), EF_U16(node + 0xeu));
    }
    advance_countdown_x_658cc();
    advance_countdown_y_6583c();
    apply_pending_notices_6563c();
}

/* 0xd550, 216 bytes. Refresh the two status widgets (0x180850, 0x180728) while word 0x1f71d2 is set and byte 0x126576 is clear: refresh the first (0xa4b08); for the second, when byte 0x125839 is 1 set its button value to mode 0xa1 and show the entry text 0x1a62f0 + 17 * index (index = high half of dword 0x121e98, up to byte 0x12583a - 1, else the text 0x13111b) in its child 0x180aec, otherwise set its button to mode 0x83; then activate (0xa4cb4) with its byte +0x10 and refresh it. */
void refresh_status_widgets_d550(void)
{
    uint32_t w1;
    uint32_t w;
    uint32_t child;
    int32_t idx;
    if (EF_U16(0x001f71d2u) == 0u || EF_U8(0x00126576u) != 0u)
        return;
    w1 = guest_fn_a46cc(0x00180850u);
    refresh_flagged_object_a4b08(w1);
    w = guest_fn_a46cc(0x00180728u);
    if (EF_U8(0x00125839u) == 1u) {
        guest_fn_a520c(w, (int32_t)EF_U32(w + 0x43u) >> 16, 0xa1u, 0u);
        idx = (int32_t)EF_U32(0x00121e98u) >> 16;
        child = find_widget_checked_a78bc(w, 0x00180aecu);
        if (idx <= (int32_t)EF_U8(0x0012583au) - 1)
            guest_fn_a7588_x(child, 0x001a62f0u + (uint32_t)(idx * 17));
        else
            guest_fn_a7588_x(child, 0x0013111bu);
    } else {
        guest_fn_a520c(w, (int32_t)EF_U32(w + 0x43u) >> 16, 0x83u, 0u);
    }
    guest_fn_a4cb4(EF_U8(w + 0x10u));
    refresh_flagged_object_a4b08(w);
}

/* 0x11f368, 248 bytes. Fixed-point arctangent of A (EAX) over B (EDX), angle units of 0x10000 per turn: 0 for (0, 0); with |A| < |B| the table lookup (0x11f308) of (A << 14) / B, offset by +0xc90f when only B is negative and by -0xc90f when both are negative; otherwise 0x6487 (or -0x6487 for negative A) minus the lookup of (B << 14) / A. */
int32_t atan2_fixed_11f368(int32_t a, int32_t b)
{
    int32_t sa;
    int32_t sb;
    uint32_t flag = 0u;
    int32_t q;
    int32_t r;
    if (a == 0 && b == 0)
        return 0;
    if (a < 0) {
        sa = -a;
        flag = 1u;
    } else {
        sa = a;
    }
    if (b < 0) {
        sb = -b;
        flag |= 2u;
    } else {
        sb = b;
    }
    if (sa < sb) {
        q = (int32_t)(((int64_t)a << 14) / b);
        r = trig_lookup_11f308(q);
        switch (flag) {
        case 0: case 1:
            return r;
        case 2:
            return r + 0xc90f;
        case 3:
            return r - 0xc90f;
        default:
            return 0;
        }
    }
    if (flag > 3u)
        return 0;
    q = (int32_t)(((int64_t)b << 14) / a);
    r = trig_lookup_11f308(q);
    if ((flag & 1u) == 0u)
        return 0x6487 - r;
    return -0x6487 - r;
}

/* 0xebb58, 247 bytes. Start streaming KEY (EAX) into the stream object OBJ (EDX): look the key's record up (0xeb520) and seek it (0xeb698); for object types 0x524a and 0x4152 (dword +0x146 high half) allocate the buffer (size dword +0x14c, plus 0x10 for 0x524a) into +0x154 (reported with 0x191620 when one already exists), fill it (0xeba64), set the window +0x158/+0x15c and return 0x80000; for other types open the slot named at +4 (0xa7900), seek it (0xa7ab4), set the window to 0 .. record size and return the slot. Returns -1 for a missing object, record or slot. */
uint32_t start_stream_ebb58(uint32_t key, ef_addr32 obj)
{
    ef_addr32 rec;
    uint32_t pos;
    int32_t type;
    uint32_t size;
    uint32_t buf;
    uint32_t slot;
    if (obj == 0u)
        return 0xffffffffu;
    rec = guest_fn_eb520(key, obj);
    if (rec == 0u)
        return 0xffffffffu;
    pos = EF_U32(rec + 4u);
    seek_record_eb698(pos, obj);
    type = (int32_t)EF_U32(obj + 0x146u) >> 16;
    if (type == 0x524a || type == 0x4152) {
        if (EF_U32(obj + 0x154u) != 0u)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x00191620u);
        type = (int32_t)EF_U32(obj + 0x146u) >> 16;
        size = EF_U32(obj + 0x14cu);
        if (type == 0x524a)
            size += 0x10u;
        buf = alloc_block_checked_eb4a0(size);
        EF_U32(obj + 0x154u) = buf;
        (void)guest_fn_eba64_x(key, buf, obj);
        EF_U32(obj + 0x158u) = EF_U32(obj + 0x154u);
        EF_U32(obj + 0x15cu) = EF_U32(obj + 0x154u) + size;
        return 0x80000u;
    }
    slot = open_slot_a7900(0u, 0u, 0u, 0u, obj + 4u);
    if (slot == 0xffffffffu)
        return slot;
    (void)seek_start_a7ab4(0u, 0u, 0u, 0u, slot, pos);
    EF_U32(obj + 0x158u) = 0u;
    EF_U32(obj + 0x15cu) = EF_U32(rec + 8u);
    return slot;
}

/* 0x9338, 306 bytes. Choose the mode byte 0x12583b for the kind KIND (AX, signed) and store it in the keyed cell named 0x180548 (0xa7ec4): the kind falls into class A (5..7, 20, 21), class B (8..19) or class C (everything else) and the high half of dword 0x41f32c selects the column (2, 1 or other); the nine class/column pairs give 9, 7, 8 / 3, 1, 2 / 6, 4, 5. */
void select_mode_byte_9338(uint32_t kind)
{
    int32_t k = (int16_t)kind;
    int32_t col;
    uint8_t cls;
    uint8_t c;
    if ((k >= 5 && k < 8) || (k > 0x13 && k <= 0x15))
        cls = 0u;
    else if (k >= 8 && k <= 0x13)
        cls = 1u;
    else
        cls = 2u;
    col = (int32_t)EF_U32(0x0041f32cu) >> 16;
    c = (col == 2) ? 0u : (col == 1) ? 1u : 2u;
    {
        static const uint8_t mode[3][3] = { { 9u, 7u, 8u }, { 3u, 1u, 2u }, { 6u, 4u, 5u } };
        EF_U8(0x0012583bu) = mode[cls][c];
    }
    EF_U32(lookup_key_a7ec4(0x00180548u)) = EF_U8(0x0012583bu);
}

/* 0xd72b4, 259 bytes. Start the sample OBJ (EDX; data pointer at +0x1c) of SIZE bytes (EBX) on voice IDX (AL): nothing happens (returns 0) unless byte 0x15bac4 is set, OBJ is non-null and 0xd7594 accepts the voice. Touch one byte per 0x1000 of the data, lock the range (0x104908), then program the voice handle from dword table 0x420dd0 (0x102670 reset, 0x102738 clear, 0x102824 rate 0x5622, 0x102858 volume 0x78, 0x102dbc callback 0xd7138, 0x1026fc data and length, 0x102774 start) and return 1. */
uint32_t start_voice_sample_d72b4(uint32_t idx, ef_addr32 obj, uint32_t size)
{
    uint32_t voice = idx & 0xffu;
    uint32_t off;
    uint32_t p;
    volatile uint8_t touch;
    if (EF_U8(0x0015bac4u) == 0u || obj == 0u)
        return 0u;
    if ((uint8_t)guest_fn_d7594(voice) == 0u)
        return 0u;
    off = 0u;
    if ((int32_t)size > 0) {
        p = EF_U32(obj + 0x1cu);
        do {
            touch = EF_U8(p);
            off += 0x1000u;
            p += 0x1000u;
        } while ((int32_t)off < (int32_t)size);
    }
    (void)touch;
    stack_range_104908(obj, size - 1u);
    trace_call_102670(EF_U32(0x00420dd0u + voice * 4u));
    (void)trace_call_102738(0u, 0u, 0u, 0u, EF_U32(0x00420dd0u + voice * 4u), 0u, 0u);
    trace_call_102824(EF_U32(0x00420dd0u + voice * 4u), 0x5622u);
    trace_call_102858(EF_U32(0x00420dd0u + voice * 4u), 0x78u);
    (void)trace_call_102dbc(0u, 0u, 0u, 0u, EF_U32(0x00420dd0u + voice * 4u), 0x000d7138u);
    (void)trace_call_1026fc(0u, 0u, 0u, 0u, EF_U32(0x00420dd0u + voice * 4u), EF_U32(obj + 0x1cu), size);
    trace_call_102774(EF_U32(0x00420dd0u + voice * 4u));
    return 1u;
}

/* 0xfc0ec, 250 bytes. Allocate the slot-set record for the pair (A = AL, B = DL): a 10-byte header (a at +0, b at +1, the slot count from 0xfc050 at +2, +3 and +5, the 10-byte slot array pointer at +6; allocated through 0xcc6c0 with the tags 0x193614 and 0x193630) and fill every slot: word +0 = 1, byte +9 the mapped mode (0xfc08c), bytes +3/+2 the slot count for the variant A (0xfb1a0 with divisor 0 and list 0) and dword +4 the result of 0xfb87c for the 1-based slot number. Returns the record. */
ef_addr32 create_slot_record_fc0ec(uint32_t a, uint32_t b)
{
    uint8_t av = (uint8_t)a;
    uint8_t bv = (uint8_t)b;
    ef_addr32 rec = guest_fn_cc6c0_x(0xau, 0x00193614u);
    ef_addr32 arr;
    uint32_t n;
    int16_t i;
    uint32_t off;
    EF_U8(rec + 1u) = bv;
    EF_U8(rec) = av;
    n = classify_kind_fc050(bv);
    EF_U8(rec + 5u) = (uint8_t)n;
    EF_U8(rec + 2u) = (uint8_t)n;
    EF_U8(rec + 3u) = (uint8_t)n;
    arr = guest_fn_cc6c0_x((n & 0xffu) * 10u, 0x00193630u);
    EF_U32(rec + 6u) = arr;
    for (i = 0; (int16_t)EF_U8(rec + 5u) > i; i++) {
        off = (uint32_t)((int32_t)i * 10);
        EF_U16(EF_U32(rec + 6u) + off) = 1u;
        EF_U8(EF_U32(rec + 6u) + off + 9u) = (uint8_t)map_mode_fc08c(EF_U8(rec + 1u), 0u);
        EF_U8(EF_U32(rec + 6u) + off + 3u) = (uint8_t)select_slot_count_by_variant_fb1a0(
            EF_U8(EF_U32(rec + 6u) + off + 9u), EF_U8(rec + 1u), EF_U8(rec), 0u, 0u);
        EF_U8(EF_U32(rec + 6u) + off + 2u) = EF_U8(EF_U32(rec + 6u) + off + 3u);
        EF_U32(EF_U32(rec + 6u) + off + 4u) = guest_fn_fb87c_x(
            EF_U8(rec + 1u), EF_U8(EF_U32(rec + 6u) + off + 9u), av, (uint32_t)(uint16_t)(i + 1));
    }
    return rec;
}

/* 0xe17f8, 307 bytes. Rebuild the 0x800-byte entry table at 0x4198c8 for the page in byte 0x15f714: initialise every 8-byte entry from index 2 on with the default bytes 0x84, 0, 0xa, 9 at +0, +1, +2, +3, then walk the descriptor array at 0x15f714 (8 bytes each: page byte, start column, two parameters at +2/+3, string pointer at +4, ended by an empty string) and for each character of every string find the next run of rows that are clear (0xe17b8, first skipping set ones), recording for the character the run start (+0), the run length (+2) and the two parameters (+1, +3) in the entry at 0x4198d0 + page * 0x800 + char * 8. */
void build_glyph_runs_e17f8(void)
{
    uint32_t page = (uint32_t)EF_U8(0x0015f714u) << 11;
    uint32_t e;
    uint32_t k;
    for (e = page + 0x110u; e <= page + 0x800u; e += 8u) {
        EF_U8(e + 0x4198c8u) = 0x84u;
        EF_U8(e + 0x4198cau) = 0xau;
        EF_U8(e + 0x4198cbu) = 9u;
        EF_U8(e + 0x4198c9u) = 0u;
    }
    for (k = 0u;; k++) {
        uint32_t d = k * 8u;
        ef_addr32 str = EF_U32(0x0015f718u + d);
        uint32_t vc;
        uint32_t vd;
        uint32_t col;
        uint32_t start;
        uint32_t base;
        uint32_t ch;
        if (EF_U8(str) == 0u)
            return;
        vc = EF_U8(0x0015f716u + d);
        vd = EF_U8(0x0015f717u + d);
        col = EF_U8(0x0015f715u + d);
        if (EF_U8(str) == 0u)
            continue;
        for (;;) {
            while (rows_clear_e17b8(col, (int32_t)vc, (int32_t)vd) != 0u)
                col++;
            start = col;
            while (rows_clear_e17b8(col, (int32_t)vc, (int32_t)vd) == 0u)
                col++;
            base = (uint32_t)EF_U8(0x0015f714u + d) << 11;
            ch = EF_U8(str);
            EF_U8(base + ch * 8u + 0x4198d0u) = (uint8_t)start;
            EF_U8(base + ch * 8u + 0x4198d2u) = (uint8_t)((uint8_t)col - (uint8_t)start);
            EF_U8(base + ch * 8u + 0x4198d1u) = (uint8_t)vc;
            EF_U8(base + ch * 8u + 0x4198d3u) = (uint8_t)vd;
            ch = EF_U8(str + 1u);
            str++;
            if (ch == 0u)
                break;
        }
    }
}

/* 0x90940, 297 bytes. Find the id of the first object near POS (EAX; three dwords): scan the 0x45-byte records (count word 0x1a1506, table at [0x1a14f8], index 0xff skipped) whose byte +0x1c is at least 0x1b and byte +0 is 1, and for each of their ten 2-byte object ids (words from +0x28) look the object up (0x8ffe8); objects without flag 2 in byte +0x30 and within 0x40000 of POS on every axis (|difference| via 0xa28f2) that pass the visibility test 0xd0470 (hull id = high half of dword +0x1e, position >> 8) return their word +0x16. Returns 0 when none qualifies. */
uint32_t find_object_near_90940(ef_addr32 pos)
{
    int16_t k;
    int16_t j;
    for (k = 0; k < (int16_t)EF_U16(0x001a1506u); k++) {
        ef_addr32 rec;
        if (k == 0xff)
            continue;
        rec = EF_U32(0x001a14f8u) + (uint32_t)((int32_t)k * 0x45);
        if ((int32_t)EF_U8(rec + 0x1cu) < 0x1b)
            continue;
        if (EF_U8(rec) != 1u)
            continue;
        for (j = 0; j < 10; j++) {
            ef_addr32 ent = EF_U32(0x001a14f8u) + (uint32_t)((int32_t)k * 0x45) + (uint32_t)((int32_t)j * 2);
            ef_addr32 obj;
            if (EF_U16(ent + 0x28u) == 0u)
                continue;
            obj = guest_fn_8ffe8((int16_t)EF_U16(ent + 0x28u));
            if (obj == 0u)
                continue;
            if ((EF_U8(obj + 0x30u) & 2u) != 0u)
                continue;
            if ((int32_t)abs_a28f2(EF_U32(obj) - EF_U32(pos)) >= 0x40000)
                continue;
            if ((int32_t)abs_a28f2(EF_U32(obj + 4u) - EF_U32(pos + 4u)) >= 0x40000)
                continue;
            if ((int32_t)abs_a28f2(EF_U32(obj + 8u) - EF_U32(pos + 8u)) >= 0x40000)
                continue;
            if (guest_fn_d0470((int32_t)EF_U32(obj + 0x1eu) >> 16, (int32_t)EF_U32(obj) >> 8,
                               (int32_t)EF_U32(obj + 4u) >> 8, (int32_t)EF_U32(obj + 8u) >> 8) != 0u)
                return EF_U16(obj + 0x16u);
        }
    }
    return 0u;
}

/* 0x76e2c, 297 bytes. Refresh the roster of unit B (DX, signed) for class A (AX, signed): look the unit up (0x7489c) and take its roster table row (byte +0x187, 25-byte rows at [0x1f8918]) and the class record A (0xca-byte rows at [0x19bb58]); classes with byte +0x1a equal to 0xe are skipped. For every one of the class's count (byte +0x4f) entries find the first of the eight roster columns whose word (row +3 + 2 * column) is 0xff, bind it with 0x76b30 (column, entry word +0x50) and note whether that entry equals the high half of dword 0x1f71c8. Then redraw the record (0x461e8) with 0x84 when A matches word 0x19bb5e and no entry matched, otherwise with 0x42. */
void refresh_unit_roster_76e2c(uint32_t a, uint32_t b)
{
    int32_t cls = (int16_t)a;
    int32_t unit = (int16_t)b;
    ef_addr32 u = guest_fn_7489c(unit);
    ef_addr32 row;
    ef_addr32 crec;
    uint8_t matched = 0u;
    uint8_t n;
    uint8_t ch;
    uint8_t cl;
    uint8_t found;
    if (u == 0u)
        return;
    row = EF_U32(0x001f8918u) + (uint32_t)EF_U8(u + 0x187u) * 25u;
    crec = EF_U32(0x0019bb58u) + (uint32_t)(cls * 0xca);
    if (EF_U8(crec + 0x1au) == 0xeu)
        return;
    n = EF_U8(crec + 0x4fu);
    for (ch = 0u; ch < n; ch++) {
        found = 0u;
        for (cl = 0u; cl < 8u; cl++) {
            if (found != 0u)
                continue;
            if ((int16_t)EF_U16(row + (uint32_t)cl * 2u + 3u) != 0xff)
                continue;
            guest_fn_76b30(u, cl, (uint32_t)(int32_t)(int16_t)EF_U16(crec + (uint32_t)ch * 2u + 0x50u));
            found = 1u;
            if (EF_U16(crec + (uint32_t)ch * 2u + 0x50u) == (uint32_t)((int32_t)EF_U32(0x001f71c8u) >> 16))
                matched = 1u;
        }
    }
    if ((uint16_t)a == EF_U16(0x0019bb5eu) && matched == 0u)
        draw_record_entries_461e8((uint32_t)unit, (uint32_t)cls, 0x84u, 0u, 0u, 0u);
    else
        draw_record_entries_461e8((uint32_t)unit, (uint32_t)cls, 0x42u, 0u, 0u, 0u);
}

/* 0xac854, 297 bytes. Poll the pointer device: when byte 0x41f2e1 is set read the motion counters and the button bits (copied to 0x41f2f1, with bit 0 to 0x41f2f2, bit 1 to 0x41f2f0 and bit 2 to 0x41f2f3); then move the cursor (0x13725e, 0x137260) either by the motion or, when 0xce08c returns 1, to the pointer position reported by the service (0xcdfe8). The word 0x41eec2 gains 8 * (|dx| + |dy|) - 0x20, clamped to 0..0x2ff; when its 0x300 bits change and word 0x41eede is zero the default video entry is reselected (0xaca14). Finally clamp the cursor (0xac7f0). */
void poll_pointer_device_ac854(void)
{
    uint16_t dx = 0u;
    uint16_t dy = 0u;
    uint16_t tmp = 0u;
    uint16_t old = EF_U16(0x0041eec2u);
    uint32_t t;
    uint32_t s;
    if (EF_U8(0x0041f2e1u) != 0u) {
        uint8_t bt;
        mouse_read_motion_ce02c((uint32_t)(uintptr_t)&dx, (uint32_t)(uintptr_t)&dy);
        bt = (uint8_t)mouse_read_buttons_ce064();
        EF_U8(0x0041f2f1u) = bt;
        EF_U8(0x0041f2f2u) = bt & 1u;
        EF_U8(0x0041f2f0u) = bt & 2u;
        EF_U8(0x0041f2f3u) = bt & 4u;
    }
    if (get_global_ce08c() == 1u) {
        query_pointer_cdfe8(0x0013725eu, 0x00137260u, (uint32_t)(uintptr_t)&tmp);
    } else {
        EF_U16(0x0013725eu) = (uint16_t)(EF_U16(0x0013725eu) + dx);
        EF_U16(0x00137260u) = (uint16_t)(EF_U16(0x00137260u) + dy);
    }
    t = abs_a28f2((int16_t)dx);
    s = abs_a28f2((int16_t)dy) + t;
    EF_U16(0x0041eec2u) = (uint16_t)(EF_U16(0x0041eec2u) + (s * 8u - 0x20u));
    if ((int16_t)EF_U16(0x0041eec2u) < 0)
        EF_U16(0x0041eec2u) = 0u;
    if ((int16_t)EF_U16(0x0041eec2u) > 0x2ff)
        EF_U16(0x0041eec2u) = 0x2ffu;
    if ((int16_t)(old & 0x300u) != (int16_t)(EF_U16(0x0041eec2u) & 0x300u) && EF_U16(0x0041eedeu) == 0u)
        select_default_video_aca14();
    clamp_cursor_ac7f0();
}

/* 0x5af50, 302 bytes. Draw the closed outline number N (AL, looked up in the pointer table 0x1281a4; nothing is drawn for N = 0) as 24 line segments between its 4-byte points (two words each): after building the rings once (0x5ae70, while byte 0x1281a0 is clear) every segment is written to the line record at 0x4208e4 (colour byte 0x128148, end points shifted left by byte 0x12b369 and offset by the words 0x1a6cfc and 0x1a6cfa) and drawn with 0xcf78c; the last segment joins the final point back to the first. */
void draw_outline_5af50(uint32_t n)
{
    uint8_t nn = (uint8_t)n;
    uint16_t sx = EF_U16(0x001a6cfau);
    ef_addr32 tab = EF_U32(0x001281a4u + (uint32_t)nn * 4u);
    uint16_t sy = EF_U16(0x001a6cfcu);
    uint32_t sh;
    ef_addr32 p;
    int16_t i;
    if (EF_U8(0x001281a0u) == 0u)
        build_offset_rings_5ae70();
    if (nn < 1u)
        return;
    for (i = 0; i < 0x18; i++) {
        if (i != 0) {
            p = tab + (uint32_t)((int32_t)i * 4);
            sh = EF_U8(0x0012b369u);
            EF_U16(0x004208e4u) = EF_U8(0x00128148u);
            EF_U16(0x004208e6u) = (uint16_t)(((uint32_t)EF_U16(p - 4u) << sh) + sy);
            EF_U16(0x004208e8u) = (uint16_t)(((uint32_t)EF_U16(p - 2u) << sh) + sx);
            EF_U16(0x004208eau) = (uint16_t)(((uint32_t)EF_U16(p) << sh) + sy);
            EF_U16(0x004208ecu) = (uint16_t)(((uint32_t)EF_U16(p + 2u) << sh) + sx);
            forward_record_cf78c();
        }
    }
    p = tab + 0x18u * 4u;
    sh = EF_U8(0x0012b369u);
    EF_U16(0x004208e4u) = EF_U8(0x00128148u);
    EF_U16(0x004208e6u) = (uint16_t)(((uint32_t)EF_U16(p - 4u) << sh) + sy);
    EF_U16(0x004208e8u) = (uint16_t)(((uint32_t)EF_U16(p - 2u) << sh) + sx);
    EF_U16(0x004208eau) = (uint16_t)(((uint32_t)EF_U16(tab) << sh) + sy);
    EF_U16(0x004208ecu) = (uint16_t)(((uint32_t)EF_U16(tab + 2u) << sh) + sx);
    forward_record_cf78c();
}

/* 0xb4aec, 306 bytes. Sibling of 0xb49b4 returning a node: for group GROUP (BL, 0..5) walk the list headed by *A (groups 0, 2, 5, links +4) or *B (groups 1, 3, 4, links +0) for a type-0x31 node that is negative (0xcc1ac) in LIST (ECX); a negative node without a follower is returned itself, otherwise the follower chain (outer link walk) is searched for a type-0x31 node of side GROUP, which is returned (0 when the chain ends). Groups 1, 3 and 4 whose first walk finds nothing then look through *A for the first type-0x31 node that is positive (0xcc22c). Returns 0 otherwise. */
ef_addr32 find_group_node_b4aec(ef_addr32 a, ef_addr32 b, uint32_t group, ef_addr32 list)
{
    uint8_t g = (uint8_t)group;
    ef_addr32 node;
    ef_addr32 nxt;
    if (g > 5u)
        return 0u;
    if (g == 0u || g == 2u || g == 5u) {
        for (node = EF_U32(a); node != 0u; node = EF_U32(node + 4u)) {
            if (EF_U8(node + 0xcu) != 0x31u || (uint16_t)group_flag_negative_cc1ac(g, EF_U8(node + 0xbu), list) == 0u)
                continue;
            if (EF_U32(node) == 0u)
                return node;
            for (;;) {
                nxt = EF_U32(node);
                if (nxt == 0u)
                    return 0u;
                if (EF_U8(nxt + 0xbu) == g && EF_U8(nxt + 0xcu) == 0x31u)
                    return nxt;
                node = EF_U32(node);
            }
        }
        return 0u;
    }
    for (node = EF_U32(b); node != 0u; node = EF_U32(node)) {
        if (EF_U8(node + 0xcu) != 0x31u || (uint16_t)group_flag_negative_cc1ac(g, EF_U8(node + 0xbu), list) == 0u)
            continue;
        if (EF_U32(node + 4u) == 0u)
            return node;
        for (;;) {
            nxt = EF_U32(node + 4u);
            if (nxt == 0u)
                return 0u;
            if (EF_U8(nxt + 0xbu) == g && EF_U8(nxt + 0xcu) == 0x31u)
                return nxt;
            node = EF_U32(node + 4u);
        }
    }
    for (node = EF_U32(a); node != 0u; node = EF_U32(node + 4u)) {
        if (EF_U8(node + 0xcu) == 0x31u && (uint16_t)group_flag_positive_cc22c(g, EF_U8(node + 0xbu), list) != 0u)
            return node;
    }
    return 0u;
}

/* 0xad440, 81 bytes. Open the resource NAME (EAX) from the default directory plus the subdirectory 0x18dc8c: build the path (0x9c980, strcat 0x9ca57), switch the engine state on first (0xe4188, while byte 0x15f930 is clear), activate the file (0x3cd40), store the handle at OUT (EDX) and reset the engine state (0xe4264) when byte 0x15f930 was clear. */
void open_resource_by_name_ad440(ef_addr32 name, ef_addr32 out)
{
    char path[0x50];
    uint32_t p = (uint32_t)(uintptr_t)path;
    uint32_t h;
    build_default_path_9c980(p);
    (void)strcat_9ca57(p, 0x0018dc8cu);
    (void)strcat_9ca57(p, name);
    if (EF_U8(0x0015f930u) == 0u)
        guest_fn_e4188();
    h = guest_fn_3cd40_x(p);
    EF_U32(out) = h;
    if (EF_U8(0x0015f930u) == 0u)
        reset_engine_e4264();
}

/* 0xa1d50, 376 bytes. Write the current page record (pointer 0x417174, entry table 0x41717c) to the save stream with 0xa1b28: first the 13-byte header (name hash of the record's name from the pointer table 0x411080, the table's two leading bytes, the flag byte holding the low five bits of record byte +0xa, and the record words +2, +4 and +6), then for every entry of the table (count = table byte 1) a 43-byte block (8 bytes from +0x14, byte +0x1c, the 16-byte name of the entry's word +0 from 0xdead0, 16 bytes from +2 and word +0x12), and finally the table's first-byte count of bytes of entry data from record + 0xb + 29 * count. Returns that count (left in EDX by the original). */
uint32_t write_page_record_a1d50(void)
{
    uint8_t buf[0x3c];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    ef_addr32 rec = EF_U32(0x00417174u);
    ef_addr32 tab = EF_U32(0x0041717cu);
    ef_addr32 ent;
    ef_addr32 name;
    uint32_t hash;
    uint32_t n;
    uint32_t cnt;
    int16_t i;
    memset(buf, 0, sizeof(buf));
    hash = name_hash_ebfd0(EF_U32(0x00411080u + (uint32_t)EF_U16(rec) * 4u));
    memcpy(&buf[0x2c], &hash, 4);
    buf[0x30] = EF_U8(tab);
    buf[0x31] = EF_U8(tab + 1u);
    buf[0x32] = (uint8_t)(EF_U8(rec + 0xau) & 0x1fu);
    { uint16_t w = EF_U16(rec + 2u); memcpy(&buf[0x33], &w, 2); }
    { uint16_t w = EF_U16(rec + 4u); memcpy(&buf[0x35], &w, 2); }
    { uint16_t w = EF_U16(rec + 6u); memcpy(&buf[0x37], &w, 2); }
    write_checked_a1b28(b + 0x2cu, 0xdu);
    for (i = 0; (int16_t)i < (int16_t)EF_U8(EF_U32(0x0041717cu) + 1u); i++) {
        ent = rec + 0xbu + (uint32_t)((int32_t)i * 0x1d);
        (void)memcpy_dc832(b, ent + 0x14u, 8u);
        buf[8] = EF_U8(ent + 0x1cu);
        (void)memcpy_dc832(b + 0x19u, ent + 2u, 0x10u);
        { uint16_t w = EF_U16(ent + 0x12u); memcpy(&buf[0x29], &w, 2); }
        name = find_table_entry_dead0(0u, 0u, 0u, 0u, (uint32_t)(int32_t)(int16_t)EF_U16(ent));
        (void)memcpy_dc832(b + 9u, name, 0x10u);
        write_checked_a1b28(b, 0x2bu);
    }
    tab = EF_U32(0x0041717cu);
    n = EF_U8(tab + 1u);
    cnt = EF_U8(tab);
    write_checked_a1b28(rec + 0xbu + n * 29u, cnt);
    return cnt;
}

/* 0xa1d00, 80 bytes. Select the record ID (AX, signed) as the current page: store the id in word 0x41722a, look the record up (0xb3f58), set the page cells 0x41717c (dword table 0x40d200 indexed by the record's first word) and 0x417174 (the record); when the record's byte +0xa has its top bit set write 4 bytes of state at 0x417160 (0xa1b28), reset the value (0xa1d50) and store it, then increment dword 0x417160. */
void select_page_a1d00(uint32_t id)
{
    ef_addr32 rec;
    uint32_t v;
    EF_U16(0x0041722au) = (uint16_t)id;
    rec = guest_fn_b3f58((int16_t)id);
    EF_U32(0x0041717cu) = EF_U32(0x0040d200u + (uint32_t)EF_U16(rec) * 4u);
    EF_U32(0x00417174u) = rec;
    if ((EF_U8(rec + 0xau) & 0x80u) != 0u) {
        write_checked_a1b28(0x00417160u, 4u);
        v = write_page_record_a1d50();
        EF_U32(0x00417160u) = v;
    }
    EF_U32(0x00417160u) = EF_U32(0x00417160u) + 1u;
}

/* 0xf118d, 340 bytes. mktime: convert the broken-down time at TM (EAX; sec, min, hour, mday, mon, year since 1900 at +0..+0x14, isdst at +0x20) to seconds since 1970. The month is normalised into the year (tables 0x195cb0 and 0x195cca for leap years), the day count and the seconds of the day are built, the structure is normalised (0x10ec62), the zone offset dword 0x17d554 is added (0x10eda1 sets it up, 0x10e97f resolves a negative isdst) and the daylight saving shift dword 0x17d558 is removed when isdst is positive; returns -1 when the date is before the epoch or out of range. */
uint32_t mktime_f118d(ef_addr32 tm)
{
    int32_t mon = (int32_t)EF_U32(tm + 0x10u);
    int32_t year = (int32_t)EF_U32(tm + 0x14u);
    int32_t r = mon % 12;
    int32_t days;
    int32_t secs;
    uint32_t tbl = 0x00195cb0u;
    int32_t y;
    if (year < (int32_t)0xf4fb7ea1u)
        return 0xffffffffu;
    year += mon / 12;
    while (r < 0) {
        year--;
        r += 12;
    }
    if (year < 0)
        return 0xffffffffu;
    if (is_leap_year(year + 0x76c) != 0u)
        tbl = 0x00195ccau;
    y = year;
    days = (int32_t)EF_U32(tm + 0xcu) + (int32_t)(int16_t)EF_U16(tbl + (uint32_t)r * 2u) + 365 * y + ((y + 3) >> 2) - 1;
    if (year != 0)
        days--;
    secs = (int32_t)EF_U32(tm) + ((int32_t)EF_U32(tm + 4u) + (int32_t)EF_U32(tm + 8u) * 60) * 60;
    while (secs < 0) {
        days--;
        secs += 0x15180;
    }
    (void)break_down_time_10ec62((uint32_t)days, (uint32_t)secs, 0, tm);
    guest_fn_10eda1_x();
    secs += (int32_t)EF_U32(0x0017d554u);
    if ((int32_t)EF_U32(tm + 0x20u) < 0)
        guest_fn_10e97f_x(tm);
    if ((int32_t)EF_U32(tm + 0x20u) > 0)
        secs -= (int32_t)EF_U32(0x0017d558u);
    while (secs < 0) {
        days--;
        secs += 0x15180;
    }
    if (days < 0x63de)
        return 0xffffffffu;
    if (days == 0x63de) {
        secs -= 0x15180;
        if ((int32_t)EF_U32(0x0017d554u) <= 0 || secs < 0)
            return 0xffffffffu;
        return (uint32_t)secs;
    }
    return (uint32_t)(secs + (days - 0x63df) * 0x15180);
}

/* 0xa4574, 45 bytes. Get the current time as seconds (0xf118d over the broken-down time from 0xf10ca, whose first field is bumped when the fraction returned is 500 or more) and store it at OUT (EDX) when non-null. */
uint32_t get_time_seconds_a4574(ef_addr32 out)
{
    uint32_t tm[9];
    uint32_t t = (uint32_t)(uintptr_t)tm;
    uint32_t r;
    memset(tm, 0, sizeof(tm));
    if ((int32_t)dos_get_time_f10ca(t) >= 0x1f4)
        tm[0]++;
    r = mktime_f118d(t);
    if (out != 0u)
        EF_U32(out) = r;
    return r;
}

/* 0x2a8ac, 136 bytes. Take a node from the free list at dword 0x19bb40 (refilled by recycling the oldest, 0x2a934) and queue it at the tail of the list headed by 0x19bb38/0x19bb3c: its words +0, +2, +4 come from the words +0, +4, +8 of the record at S1, word +6 from its own word table (+8, index S0 signed 16 bits), and with byte 0x125847 set the value at +6 is announced with 0x9e508. EBX is left clobbered by the original. */
void queue_grid_node_2a8ac(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, ef_addr32 s1)
{
    ef_addr32 node;
    ef_addr32 tail;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(0x0019bb40u) == 0u)
        recycle_oldest_grid_node();
    node = EF_U32(0x0019bb40u);
    EF_U32(0x0019bb40u) = EF_U32(node + 0x12u);
    EF_U32(node + 0x12u) = 0u;
    EF_U16(node) = EF_U16(s1);
    EF_U16(node + 2u) = EF_U16(s1 + 4u);
    EF_U16(node + 4u) = EF_U16(s1 + 8u);
    EF_U16(node + 6u) = EF_U16(node + (uint32_t)((int32_t)(int16_t)s0 * 2) + 8u);
    if (EF_U8(0x00125847u) != 0u)
        guest_fn_9e508((int16_t)EF_U16(node + 6u), 0u);
    tail = EF_U32(0x0019bb38u);
    if (tail != 0u) {
        EF_U32(0x0019bb38u) = node;
        EF_U32(tail + 0x12u) = node;
    } else {
        EF_U32(0x0019bb38u) = node;
        EF_U32(0x0019bb3cu) = node;
    }
}

/* 0xc868, 127 bytes. Rebuild the current page: clear the word tables (0x92f0), lay out the slots up to the count in word 0x121e98 (0xc768), draw the page entries (0xc8e8), total the first count words of the table at 0x1a53b0 into word 0x121e9c and redraw the child 0x180580 of the dialog 0x180728 (0xcccc). */
void rebuild_page_c868(void)
{
    int16_t n;
    int16_t i;
    uint32_t total = 0u;
    uint32_t w;
    clear_word_tables_92f0();
    layout_slot_widgets_c768((uint32_t)(int32_t)(int16_t)EF_U16(0x00121e98u));
    draw_page_entries_c8e8();
    n = (int16_t)EF_U16(0x00121e98u);
    for (i = 0; n > 0 && i < n; i++)
        total += EF_U16(0x001a53b0u + (uint32_t)(uint16_t)i * 2u);
    EF_U16(0x00121e9cu) = (uint16_t)total;
    EF_U16(0x00121e98u) = (uint16_t)n;
    w = guest_fn_a46cc(0x00180728u);
    guest_fn_cccc_x(find_widget_checked_a78bc(w, 0x00180580u));
}

/* 0x56890, 152 bytes. Apply the order record REC (EAX; unit id word +1) to its unit (0x8ffe8) unless the unit's word +0x18 equals byte 0x1a666b: select the unit's cell (high half of dword +0x1e, 0xcd150), add the difference of the record word +6 and the cell value (0xcd258) to the progress (0xcd2e8), clear the unit's scene children (0x90074) when the record word +8 is 1, run 0x106b0 with the record word +0xa, the unit position >> 8 and the record byte +0xc, and finally 0x10a9c with the unit id, record word +8 and word +6. */
void apply_order_record_56890(ef_addr32 rec)
{
    int32_t id = (int16_t)EF_U16(rec + 1u);
    ef_addr32 obj = guest_fn_8ffe8(id);
    uint32_t v;
    if (obj == 0u)
        return;
    if ((uint16_t)EF_U8(0x001a666bu) == EF_U16(obj + 0x18u))
        return;
    (void)guest_fn_cd150((int32_t)EF_U32(obj + 0x1eu) >> 16);
    v = guest_fn_cd258() & 0xffffu;
    guest_fn_cd2e8((int16_t)(EF_U16(rec + 6u) - v));
    if (((int32_t)EF_U32(rec + 6u) >> 16) == 1)
        clear_scene_node_children((int16_t)EF_U16(rec + 1u));
    guest_fn_106b0_x((uint32_t)((int32_t)EF_U32(rec + 8u) >> 16), (uint32_t)((int32_t)EF_U32(obj) >> 8),
                     (uint32_t)((int32_t)EF_U32(obj + 4u) >> 8), (uint32_t)((int32_t)EF_U32(obj + 8u) >> 8),
                     EF_U8(rec + 0xcu));
    guest_fn_10a9c_x((int16_t)EF_U16(rec + 1u), (int32_t)EF_U32(rec + 6u) >> 16, (int32_t)EF_U32(rec + 4u) >> 16);
}

/* 0xe3e50, 153 bytes. Parse a text-slot definition "N=name" at TEXT (EAX): read the slot number N, expect '=' and skip blanks (a number below 1 is reported with 0x190a7c through 0x132a74), then build default path + 0x15f910 (when the path is non-empty) + the remaining text + 0x190a54 and store it as slot N - 1 (0xcd724). */
void define_text_slot_e3e50(ef_addr32 text)
{
    char buf[0x90];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    ef_addr32 cur = text;
    uint32_t c = (uint32_t)(uintptr_t)&cur;
    uint32_t n = parse_decimal_1092b0(c);
    expect_byte_109328(c, 0x3du);
    skip_class_chars(c);
    if ((int32_t)n < 1)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00190a7cu, n);
    build_default_path_9c980(b);
    if (buf[0] != 0)
        (void)strcat_9ca57(b, 0x0015f910u);
    (void)strcat_9ca57(b, cur);
    (void)strcat_9ca57(b, 0x00190a54u);
    set_text_slot_cd724((int16_t)(n - 1u), b);
}

/* 0x31d7c, 173 bytes. Recompute the cached bounds record: skipped when byte 0x19e557 is set, and then guarded by a test that compares the address constant 0x19e58b with 1 (ORIGINAL BUG: it was meant to test the byte at that address, so the body below never runs). The body builds a 0x24-byte bounds record on the stack from the cells 0x19e560/0x19e56c/0x19e572 (0x324b0), initialises it (0xd0b68), applies the words 0x19e56e, 0x19e56c, 0x19e570 and the two results (0xd0ebc, 0xd0dbc, 0xd0fb8) and stores the angle (0xd13ac) in word 0x19e5d3 and the pair (0xd13dc) in word 0x19e5d5. */
void recompute_bounds_31d7c(void)
{
    uint8_t rec[0x2c];
    uint32_t r = (uint32_t)(uintptr_t)rec;
    if (EF_U8(0x0019e557u) != 0u)
        return;
    if (0x0019e58bu != 1u)
        return;
    memset(rec, 0, sizeof(rec));
    guest_fn_324b0_x(0x0019e560u, 0x0019e56cu, 0x0019e572u, r + 0x24u);
    init_bounds_d0b68(r);
    guest_fn_d0ebc(r, EF_U16(0x0019e56eu));
    guest_fn_d0dbc(r, EF_U16(0x0019e56cu), 0x0019e572u);
    guest_fn_d0fb8(r, EF_U16(0x0019e570u), r + 0x24u);
    guest_fn_d0ebc(r, *(uint16_t *)&rec[0x26]);
    guest_fn_d0dbc(r, *(uint16_t *)&rec[0x24], 0x0019e572u);
    EF_U16(0x0019e5d3u) = (uint16_t)angle_d13ac(r);
    EF_U16(0x0019e5d5u) = (uint16_t)forward_pair_d13dc(r);
}

/* 0xf2183, 252 bytes. fseek: reposition STREAM (EAX) by OFFSET (EDX) from WHENCE (EBX; 0 start, 1 current, 2 end). A stream with write/read state (flags 6 in byte +0xc) is flushed first when flag 0x10 is set (a failed flush with whence 0 and a negative offset reports error 9); otherwise a relative seek is made absolute and the buffer emptied, then the mode bits are cleared and the file seeks (0xfa95f). For idle streams whence 0/1 first try to move the buffer window (0xf2141) and only seek the file when that fails, whence 2 resets the buffer and seeks, other values report error 9. Returns 0, or -1 on failure. */
uint32_t fseek_f2183(ef_addr32 stream, uint32_t offset, uint32_t whence)
{
    uint32_t r;
    uint32_t edi;
    if ((EF_U8(stream + 0xcu) & 6u) != 0u) {
        if ((EF_U8(stream + 0xdu) & 0x10u) != 0u) {
            if (stream_flush_fa8a0(stream) != 0) {
                if (whence == 0u && (int32_t)offset < 0)
                    guest_fn_ebe6d(9u);
                return 0xffffffffu;
            }
        } else {
            if (whence == 1u)
                offset -= EF_U32(stream + 4u);
            EF_U32(stream) = EF_U32(stream + 8u);
            EF_U32(stream + 4u) = 0u;
        }
        EF_U8(stream + 0xcu) &= 0xebu;
        r = lseek_fa95f(EF_U32(stream + 0x10u), offset, whence);
        return r == 0xffffffffu ? r : 0u;
    }
    if (whence == 1u) {
        edi = EF_U32(stream + 4u);
        if (advance_window_f2141((int32_t)offset, stream) == 0u)
            return 0u;
        r = lseek_fa95f(EF_U32(stream + 0x10u), offset - edi, whence);
        return r == 0xffffffffu ? r : 0u;
    }
    if (whence == 0u) {
        edi = offset - (guest_fn_10f4df(EF_U32(stream + 0x10u)) - EF_U32(stream + 4u));
        if (advance_window_f2141((int32_t)edi, stream) == 0u)
            return 0u;
        r = lseek_fa95f(EF_U32(stream + 0x10u), offset, whence);
        return r == 0xffffffffu ? r : 0u;
    }
    if (whence == 2u) {
        EF_U32(stream) = EF_U32(stream + 8u);
        EF_U8(stream + 0xcu) &= 0xefu;
        EF_U32(stream + 4u) = 0u;
        r = lseek_fa95f(EF_U32(stream + 0x10u), offset, whence);
        return r == 0xffffffffu ? r : 0u;
    }
    guest_fn_ebe6d(9u);
    return 0xffffffffu;
}

/* 0xa5b70, 306 bytes. Draw the widget W (EAX) when it is visible: its size words +0x75/+0x77 must be positive and its rectangle (position high halves +0x3f/+0x41 plus offsets +0x6f/+0x71, extents +0x73/+0x75) must lie inside the screen (dwords 0x15b744, 0x15b748). With flag 1 of byte +0xc4 it first fills the rectangle 0x41ec08.. (colour byte +0xc2, 0xc91c8); then runs the unit hook +0x95 (0xe4c9c), the overlap visit (0xe2564, flag 1 of +0xcb and pointer +0xc7) and the draw callback +0xb1: set up the view (0xabb00, 0xc9f28), raise byte 0x41f2e5 and the current widget 0x41eea0 around the call, then restore the view corners (0xc9ecc). */
void draw_widget_a5b70(ef_addr32 w)
{
    int32_t x;
    int32_t y;
    if ((int16_t)EF_U16(w + 0x75u) <= 0 || (int16_t)EF_U16(w + 0x77u) <= 0)
        return;
    x = ((int32_t)EF_U32(w + 0x3fu) >> 16) + ((int32_t)EF_U32(w + 0x6fu) >> 16);
    if (x >= (int32_t)EF_U32(0x0015b744u))
        return;
    x += (int32_t)EF_U32(w + 0x73u) >> 16;
    if (x <= 0)
        return;
    y = ((int32_t)EF_U32(w + 0x41u) >> 16) + ((int32_t)EF_U32(w + 0x71u) >> 16);
    if (y >= (int32_t)EF_U32(0x0015b748u))
        return;
    y += (int32_t)EF_U32(w + 0x75u) >> 16;
    if (y <= 0)
        return;
    if ((EF_U8(w + 0xc4u) & 1u) != 0u) {
        uint16_t x1 = (uint16_t)(EF_U16(0x0041ec08u) + EF_U16(0x0041ec0cu));
        uint16_t y1 = (uint16_t)(EF_U16(0x0041ec0au) + EF_U16(0x0041ec0eu));
        fill_rect_clipped_c91c8(EF_U8(w + 0xc2u), EF_U16(0x0041ec08u), EF_U16(0x0041ec0au), x1, y1);
    }
    if (EF_U32(w + 0x95u) != 0u)
        guest_fn_e4c9c_w(w);
    if ((EF_U8(w + 0xcbu) & 1u) != 0u && EF_U32(w + 0xc7u) != 0u)
        visit_overlapping_nodes_e2564(w);
    if (EF_U32(w + 0xb1u) != 0u) {
        guest_fn_abb00_x(w);
        set_view_rect_c9f28(w);
        EF_U8(0x0041f2e5u) = 1u;
        EF_U32(0x0041eea0u) = w;
        guest_callptr_eax_x(EF_U32(w + 0xb1u), w);
        EF_U8(0x0041f2e5u) = 0u;
        EF_U32(0x0041eea0u) = 0u;
        init_view_corners_c9ecc();
    }
}

/* 0xa80b0, 308 bytes. Expand the %NAME placeholders of the text at TEXT (EAX) in place: copy characters to a 0x50-byte work buffer and, at every '%', read the word up to the next blank (upper-cased with 0xe7835), look it up in the 0x15-byte name table at 0x419218 (count word 0x4194c4, compared with 0xa4380) and append its value: the string pointer at dword table 0x418708 for entries whose byte 0x41948e is 1, otherwise the number formatted with 0x18d664. An unknown name is reported (through 0x132a74 with 0x18d634 when byte 0x1371f9 is set) or replaced by the text 0x18d658. The result is copied back over TEXT; returns its length. */
uint32_t expand_placeholders_a80b0(ef_addr32 text)
{
    char L[0x74];
    uint32_t l = (uint32_t)(uintptr_t)L;
    uint32_t in = 0u;
    int16_t out = 0;
    int16_t cx;
    uint32_t si;
    uint8_t c;
    memset(L, 0, sizeof(L));
    for (;;) {
        c = EF_U8(text + (uint32_t)(int32_t)(int16_t)in);
        if (c == 0u)
            break;
        if (c != 0x25u) {
            in++;
            L[(int16_t)out] = (char)c;
            out++;
            continue;
        }
        in++;
        L[(int16_t)out] = 0;
        si = 0u;
        for (;;) {
            c = EF_U8(text + (uint32_t)(int32_t)(int16_t)in);
            if (c == 0x20u || c == 0u)
                break;
            c = (uint8_t)guest_fn_e7835(c);
            in++;
            L[0x50 + (int16_t)si] = (char)c;
            si++;
        }
        L[0x50 + (int16_t)si] = 0;
        cx = 0;
        while (guest_fn_a4380(l + 0x50u, 0x00419218u + (uint32_t)((int32_t)cx * 0x15)) != 0u
               && cx < (int16_t)EF_U16(0x004194c4u))
            cx++;
        if (cx >= (int16_t)EF_U16(0x004194c4u)) {
            if (EF_U8(0x001371f9u) != 0u) {
                (void)guest_sprintf_a4342(l, 0x0018d634u, l + 0x50u);
                guest_callptr1_x(EF_U32(0x00132a74u), l);
            } else {
                (void)strcat_9ca57(l, 0x0018d658u);
            }
        } else if (EF_U8(0x0041948eu + (uint32_t)(int32_t)cx) == 1u) {
            (void)strcat_9ca57(l, EF_U32(0x00418708u + (uint32_t)((int32_t)cx * 4)));
        } else {
            (void)guest_sprintf_a4342(l + 0x68u, 0x0018d664u, EF_U32(0x00418708u + (uint32_t)((int32_t)cx * 4)));
            (void)strcat_9ca57(l, l + 0x68u);
        }
        out = (int16_t)guest_fn_b1b8b(l);
    }
    L[(int16_t)out] = 0;
    copy_string_9d749(text, l);
    return guest_fn_b1b8b(text);
}

/* 0x3617c, 303 bytes. Send TOTAL (EAX) bytes from the buffer at dword 0x19f34c to the link in packets: set word 0x1357b2 = 2 and byte 0x19f5a6 = 0x48, then for every packet number (chunk size = high half of dword 0x1258fe) build a 3-byte header (packet number in the low six bits of byte 0, bit 0x40 on the last packet, bit 0x80 when FLAG (EDX) is not positive, word 0x19f594 at +1) followed by the chunk, issue the request (0xa3eac) between idle callbacks (0xa41a0) and after every fourth packet recompute the totals (0xa4030) and mark slots (0x35f04). Finally set byte 0x19f5aa = 6, flush the same way and store the clock (hook 0x15b620) in dword 0x19f338. */
void send_packets_3617c(uint32_t total, uint32_t flag)
{
    uint8_t buf[0x1fc];
    uint32_t b = (uint32_t)(uintptr_t)buf;
    int32_t left = (int32_t)total;
    uint32_t idx = 0u;
    int32_t csz;
    int32_t chunk;
    memset(buf, 0, sizeof(buf));
    EF_U16(0x001357b2u) = 2u;
    EF_U8(0x0019f5a6u) = 0x48u;
    while (left > 0) {
        csz = (int32_t)EF_U32(0x001258feu) >> 16;
        chunk = (left > csz) ? csz : left;
        buf[0] = (uint8_t)((buf[0] & 0xc0u) | (idx & 0x3fu));
        {
            uint16_t w = EF_U16(0x0019f594u);
            memcpy(&buf[1], &w, 2);
        }
        if (left - csz <= 0)
            buf[0] |= 0x40u;
        else
            buf[0] &= 0xbfu;
        if ((int32_t)flag <= 0)
            buf[0] |= 0x80u;
        else
            buf[0] &= 0x7fu;
        (void)memcpy_dc832(b + 3u, EF_U32(0x0019f34cu) + (uint32_t)(csz * (int32_t)idx), (uint32_t)chunk);
        run_idle_callback_a41a0();
        issue_request_ready_a3eac(b, (uint16_t)(chunk + 3));
        run_idle_callback_a41a0();
        if ((idx & 3u) == 3u) {
            recompute_total_a4030();
            mark_matching_slots_35f04();
        }
        idx++;
        left -= csz;
    }
    EF_U8(0x0019f5aau) = 6u;
    recompute_total_a4030();
    mark_matching_slots_35f04();
    run_idle_callback_a41a0();
    EF_U32(0x0019f338u) = ((uint32_t (*)(void))EF_U32(0x0015b620u))();
}

/* 0xd6e58, 303 bytes. Initialise the sound driver. When both configuration bytes 0x15bac6 and 0x15bac7 are zero clear the three state bytes (0x15bac4, 0x15bac6, 0x15bac7) and return -1. Otherwise, unless already initialised (byte 0x15bac4), initialise the module (0xd6dc4), program the driver settings (0x101e14: 1/0x5622, 0/0x64, 8/0, 7/1, 4/0xe), open the device description at 0x420e0c (0x1024b4; a failure shuts down 0x101ddc, clears the state bytes and reports 0x18f938 with dword 0x15bac8) and create, check (report 0x18f950 when missing) and reset the 14 voice handles in dword table 0x420dd0 (0x102a98, 0x102590, 0x102670), then set byte 0x15bac4. Returns 0 on success. */
uint32_t init_sound_driver_d6e58(void)
{
    uint32_t i;
    if (EF_U8(0x0015bac7u) == 0u && EF_U8(0x0015bac6u) == 0u) {
        EF_U8(0x0015bac6u) = 0u;
        EF_U8(0x0015bac4u) = 0u;
        EF_U8(0x0015bac7u) = 0u;
        return 0xffffffffu;
    }
    if (EF_U8(0x0015bac4u) != 0u)
        return 0u;
    init_module_d6dc4();
    (void)trace_call_101e14(0u, 0u, 0u, 0u, 1u, 0x5622u);
    (void)trace_call_101e14(0u, 0u, 0u, 0u, 0u, 0x64u);
    (void)trace_call_101e14(0u, 0u, 0u, 0u, 8u, 0u);
    (void)trace_call_101e14(0u, 0u, 0u, 0u, 7u, 1u);
    (void)trace_call_101e14(0u, 0u, 0u, 0u, 4u, 0xeu);
    if (trace_call_1024b4(0u, 0u, 0u, 0u, 0x00420e0cu) != 0u) {
        (void)trace_call_101ddc();
        EF_U8(0x0015bac4u) = 0u;
        EF_U8(0x0015bac6u) = 0u;
        EF_U8(0x0015bac7u) = 0u;
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018f938u, EF_U32(0x0015bac8u));
    }
    for (i = 0u; i != 0x38u; i += 4u) {
        trace_call_102a98(EF_U32(0x00420e0cu), EF_U8(0x00420e10u));
        EF_U32(0x00420dd0u + i) = trace_call_102590(0u, 0u, 0u, 0u, EF_U32(0x00420e0cu));
        if (EF_U32(0x00420dd0u + i) == 0u)
            guest_callptr1_x(EF_U32(0x00132a74u), 0x0018f950u);
        trace_call_102670(EF_U32(0x00420dd0u + i));
    }
    EF_U8(0x0015bac4u) = 1u;
    return 0u;
}

/* 0x111118, 42 bytes. cdecl: look up interrupt INTNO with the DOS extender calls 0x2502 (protected-mode vector, result discarded) and 0x2503 (real-mode vector) and return the real-mode vector held in EBX. */
uint32_t get_realmode_vector_111118(uint32_t intno)
{
    uint32_t out[7];
    run_dos_int_ex(0x21u, 0x2502u, 0u, intno & 0xffu, 0u, out);
    run_dos_int_ex(0x21u, 0x2503u, 0u, intno & 0xffu, 0u, out);
    return out[1];
}

/* 0x117ace, 47 bytes. Register the two memory ranges 0x17e270..0x17e290 and 0x1159c8..0x117ace with the lock service (0x104840, cdecl) around saved DS/ES. */
void register_locked_ranges_117ace(void)
{
    guest_fn_104840(0x0017e270u, 0x0017e290u);
    guest_fn_104840(0x001159c8u, 0x00117aceu);
}

/* 0xfa9a0, 50 bytes. close: close the DOS handle HANDLE (EAX, int 21h AH = 0x3e); on success clear its table entry (0x1102a3 with 0) and return 0, on failure set error 4 (0xebe6d) and return -1. */
uint32_t close_handle_fa9a0(uint32_t handle)
{
    uint32_t out[7];
    run_dos_int_ex(0x21u, 0x3e00u, handle, 0u, handle, out);
    if ((out[0] & 0x80000000u) == 0u && out[6] == 0u) {
        store_flagged_value(handle, 0u);
        return 0u;
    }
    guest_fn_ebe6d(4u);
    return 0xffffffffu;
}

/* 0x110f0c, 48 bytes. cdecl: program the system timer with the reload value DIVISOR (channel 0, mode 3: command 0x36 to port 0x43, low then high byte to port 0x40) with interrupts disabled, and remember it in dword 0x17d7ae. */
void set_timer_divisor_110f0c(uint32_t divisor)
{
    guest_fn_117b0c(0x43u, 0x36u);
    EF_U32(0x0017d7aeu) = divisor;
    guest_fn_117b0c(0x40u, divisor & 0xffu);
    guest_fn_117b0c(0x40u, (divisor >> 8) & 0xffu);
}

/* 0xe10a0, 63 bytes. Send the text at TEXT (EAX) to the serial port at base dword 0x15eed0: remember the pointer in dword 0x15f690, set byte 0x15f694 = 1 and, while characters remain, wait for the line status register (base + 5) to show the transmitter ready (bit 0x20) and write the next character. The text pointer variable itself is not advanced. */
void send_serial_text_e10a0(ef_addr32 text)
{
    ef_addr32 p = text;
    EF_U32(0x0015f690u) = text;
    EF_U8(0x0015f694u) = 1u;
    if (EF_U8(p) == 0u)
        return;
    do {
        while ((guest_fn_9c430(EF_U32(0x0015eed0u) + 5u) & 0x20u) == 0u)
            ;
        guest_fn_117b0c(EF_U32(0x0015eed0u), EF_U8(p));
        p++;
    } while (EF_U8(p) != 0u);
}

/* 0xe0930, 81 bytes. Program the serial port at base dword 0x15eed0 (nothing when it is zero): clear the modem control (base + 4), set the interrupt enable from byte 0x15eeac, open the divisor latch (base + 3 = 0x80), write the divisor bytes 0x15eeae/0x15eeaf (base and base + 1), close the latch (base + 3 = 0x7f) and set the line control from byte 0x15eead. */
void init_serial_port_e0930(void)
{
    uint32_t base = EF_U32(0x0015eed0u);
    if (base == 0u)
        return;
    guest_fn_117b0c(base + 4u, 0u);
    guest_fn_117b0c(EF_U32(0x0015eed0u) + 1u, EF_U8(0x0015eeacu));
    guest_fn_117b0c(EF_U32(0x0015eed0u) + 3u, 0x80u);
    base = EF_U32(0x0015eed0u);
    guest_fn_117b0c(base, EF_U8(0x0015eeaeu));
    guest_fn_117b0c(base + 1u, EF_U8(0x0015eeafu));
    guest_fn_117b0c(base + 3u, 0x7fu);
    guest_fn_117b0c(base + 3u, EF_U8(0x0015eeadu));
}

/* 0x1100d2, 81 bytes. localtime: break the time_t at TIMEP (EAX) into TM (EDX) relative to the epoch day 0x63df using the zone offset dword 0x17d554 (0x10eda1 sets the zone up), and again with the daylight saving shift dword 0x17d558 removed when 0x10e97f reports daylight time for the result. Returns TM. */
ef_addr32 localtime_1100d2(ef_addr32 timep, ef_addr32 tm)
{
    uint32_t secs;
    guest_fn_10eda1_x();
    secs = EF_U32(timep);
    (void)break_down_time_10ec62(0x63dfu, secs, (int32_t)EF_U32(0x0017d554u), tm);
    if (guest_fn_10e97f_ret_x(tm) != 0u)
        (void)break_down_time_10ec62(0x63dfu, secs, (int32_t)(EF_U32(0x0017d554u) - EF_U32(0x0017d558u)), tm);
    return tm;
}

/* 0xe06ac, 77 bytes. Set the serial divisor for the state record at dword 0x15eec0: take the divisor word from the table at 0xdf290 indexed by byte +0xa, store it at +0xc, set byte +5 = 3, open the divisor latch (base + 3 = 0x80), write the low and high divisor bytes to base and base + 1 and set the line control (base + 3) to 3. */
void set_serial_divisor_e06ac(void)
{
    ef_addr32 st = EF_U32(0x0015eec0u);
    uint32_t div = EF_U16(0x000df290u + (uint32_t)EF_U8(st + 0xau) * 2u);
    uint32_t base;
    EF_U8(st + 5u) = 3u;
    EF_U16(st + 0xcu) = (uint16_t)div;
    guest_fn_117b0c(EF_U32(0x0015eed0u) + 3u, 0x80u);
    base = EF_U32(0x0015eed0u);
    guest_fn_117b0c(base, div & 0xffu);
    guest_fn_117b0c(base + 1u, (div >> 8) & 0xffu);
    guest_fn_117b0c(base + 3u, 3u);
}

/* 0x8fbc8, 91 bytes. Append the 24-byte record at SRC (EAX) to the ring of ten records at 0x20f460 (index word 0x2119f0), announce the high half of its dword +0x14 with 0x9e508 and advance the index, wrapping to 0 at ten. */
void push_ring_record_8fbc8(ef_addr32 src)
{
    int32_t idx = (int32_t)EF_U32(0x002119eeu) >> 16;
    ef_addr32 dst = 0x0020f460u + (uint32_t)(idx * 24);
    uint32_t i;
    for (i = 0u; i < 24u; i++)
        EF_U8(dst + i) = EF_U8(src + i);
    guest_fn_9e508((int32_t)EF_U32(dst + 0x14u) >> 16, 0u);
    EF_U16(0x002119f0u) = (uint16_t)(EF_U16(0x002119f0u) + 1u);
    if (((int32_t)EF_U32(0x002119eeu) >> 16) >= 10)
        EF_U16(0x002119f0u) = 0u;
}

/* 0xb2d04, 135 bytes. Debug dump of the list LIST (EAX; first node at +0, links at +4) to the file handle FILE (EBX): print the header text FMT (ECX), the separator 0x18e384, the list's id word (0xbc5d0, signed) with 0x18e3a4 and the four signed words S0..S3 (stack, ret 0x10) with 0x18e3c8, then dump every node with 0xb2b00 (passing EDX = E and the file). */
void dump_list_b2d04(ef_addr32 list, uint32_t e, uint32_t file, ef_addr32 fmt, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    ef_addr32 node = EF_U32(list);
    guest_fn_b1bb0(file, fmt);
    guest_fn_b1bb0(file, 0x0018e384u);
    guest_fn_b1bb0(file, 0x0018e3a4u, (uint32_t)(int32_t)(int16_t)guest_fn_bc5d0_x(list));
    guest_fn_b1bb0(file, 0x0018e3c8u, (uint32_t)(int32_t)(int16_t)s0, (uint32_t)(int32_t)(int16_t)s1,
                   (uint32_t)(int32_t)(int16_t)s2, (uint32_t)(int32_t)(int16_t)s3);
    while (node != 0u) {
        guest_fn_b2b00_x(node, e, file);
        node = EF_U32(node + 4u);
    }
}

/* 0x35de0, 136 bytes. Rescale a pair of widgets of the dialog (name chosen by byte 0x1357c8: 0x1830f8 or 0x182fb0). The word at +4 of child 0x183104 times SCALE (stack double, ret 8), rounded, becomes the x position of child 0x18310c (y kept from its high word); finally redraw the item whose id is the byte at +0x10 of the dialog. */
void rescale_dialog_widgets_35de0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, double scale)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    ef_addr32 root = guest_fn_a46cc(EF_U8(0x001357c8u) != 0u ? 0x001830f8u : 0x00182fb0u);
    if (root != 0u) {
        ef_addr32 w = find_widget_checked_a78bc(root, 0x00183104u);
        if (w != 0u) {
            int32_t x = (int32_t)truncate_9d72c((double)EF_U16(w + 4u) * scale);
            ef_addr32 w2 = find_widget_checked_a78bc(root, 0x0018310cu);
            if (w2 != 0u)
                move_widget_a74bc(w2, (int16_t)x, (int16_t)((int32_t)EF_U32(w2 + 4u) >> 16));
        }
    }
    /* ORIGINAL BUG: ROOT is dereferenced even when it is NULL. */
    redraw_item_a5ef8(EF_U8(root + 0x10u));
}

/* 0xc594, 155 bytes. Close out the current trace: unless byte 0x126576 or either word 0x137998/0x13799a is set, dump the pending record (0xb3084 with text 0x18086c and the records at 0x137988, 0x137970, 0x137980 plus six signed high words), then reset the view tables, clear bytes 0x121e8c, 0x126576 and 0x126577 and set the pending name 0x18099c. */
void finish_trace_c594(void)
{
    if (EF_U8(0x00126576u) == 0u && EF_U16(0x00137998u) == 0u && EF_U16(0x0013799au) == 0u) {
        guest_fn_b3084_x(0x0018086cu, 0x00137988u, 0x00137970u, 0x00137980u, 0x00134d8au,
                         (uint32_t)((int32_t)EF_U32(0x0013799cu) >> 16), (uint32_t)((int32_t)EF_U32(0x0013799eu) >> 16),
                         (uint32_t)((int32_t)EF_U32(0x001379a0u) >> 16), (uint32_t)((int32_t)EF_U32(0x0041f32cu) >> 16),
                         (uint32_t)((int32_t)EF_U32(0x0041f328u) >> 16), (uint32_t)((int32_t)EF_U32(0x0041f32au) >> 16));
    }
    reset_view_tables_b1b10();
    EF_U8(0x00121e8cu) = 0u;
    EF_U8(0x00126576u) = 0u;
    EF_U8(0x00126577u) = 0u;
    set_pending_name_ac7a8(0x0018099cu);
}

/* 0x942cc, 162 bytes. Rebuild the 9-dword record at 0x21439c from the template 0x12dbec: set its word at +0x0e from 0xd13dc(0x400), classify the angle (0x92070), rotate by 0xd0ebc(0), and if the signed high word at 0x12db98 exceeds 0x6800 call 0x95018 and rotate again by 0xfc00. Then rotate by 0xf800 (0xd0dbc, EBX undefined in the original), back-copy the record to 0x12dbec, run 0x93a8c and finally store the record in the second buffer 0x2143c0 via 0xd099c. */
void rebuild_angle_record_942cc(void)
{
    guest_fn_d099c(0x0021439cu, 0x0012dbecu);
    EF_U16(0x0012db9au) = (uint16_t)guest_fn_d13dc(0x0021439cu, 0x400u);
    guest_fn_92070();
    guest_fn_d0ebc(0x0021439cu, 0u);
    if (((int32_t)EF_U32(0x0012db98u) >> 16) > 0x6800) {
        guest_fn_95018();
        guest_fn_d0ebc(0x0021439cu, 0xfc00u);
    }
    memcpy_dc832(0x002143c0u, 0x0021439cu, 36u);
    guest_fn_d0dbc(0x0021439cu, 0xf800u, 0u);
    guest_fn_d099c(0x0012dbecu, 0x0021439cu);
    guest_fn_93a8c();
    guest_fn_d099c(0x0012dbecu, 0x002143c0u);
}

/* 0x94370, 162 bytes. Mirror of 0x942cc for the opposite direction: the angle starts at 0xfc00, and when the signed high word at 0x12db98 is below -0x6800 the record is rotated by 0x400 after 0x95018. */
void rebuild_angle_record_rev_94370(void)
{
    guest_fn_d099c(0x0021439cu, 0x0012dbecu);
    EF_U16(0x0012db9au) = (uint16_t)guest_fn_d13dc(0x0021439cu, 0xfc00u);
    guest_fn_92070();
    guest_fn_d0ebc(0x0021439cu, 0u);
    if (((int32_t)EF_U32(0x0012db98u) >> 16) < -0x6800) {
        guest_fn_95018();
        guest_fn_d0ebc(0x0021439cu, 0x400u);
    }
    memcpy_dc832(0x002143c0u, 0x0021439cu, 36u);
    guest_fn_d0dbc(0x0021439cu, 0xf800u, 0u);
    guest_fn_d099c(0x0012dbecu, 0x0021439cu);
    guest_fn_93a8c();
    guest_fn_d099c(0x0012dbecu, 0x002143c0u);
}

/* 0x16b0c, 159 bytes. Range test between table entries A (EAX) and B (EDX) of the 0xca-byte records at [0x19bb58]: the type byte at +0x19 selects a 0xe5-byte class record; the threshold is pow(double at class(B)+0x128637, 0.25) * 1000 * dword at class(A)+0x12863f, stored as a float. The grid distance 0x2bb3c(A, B) is saved in the float 0x197df0 and the result is 1 when it does not exceed the threshold. */
uint32_t within_range_16b0c(uint32_t a, uint32_t b)
{
    uint32_t tab = EF_U32(0x0019bb58u);
    uint32_t ca = (uint32_t)EF_U8(tab + a * 0xcau + 0x19u) * 0xe5u;
    uint32_t cb = (uint32_t)EF_U8(tab + b * 0xcau + 0x19u) * 0xe5u;
    double base;
    float thr;
    double dist;
    int32_t scaled = (int32_t)(EF_U32(ca + 0x0012863fu) * 1000u);
    memcpy(&base, ef_guest_memory_ptr(cb + 0x00128637u), sizeof base);
    thr = (float)(pow_b3fec(base, 0.25) * (double)scaled);
    dist = grid_cell_distance((int16_t)a, (int16_t)b);
    *(float *)ef_guest_memory_ptr(0x00197df0u) = (float)dist;
    return dist <= thr ? 1u : 0u;
}

/* 0x168d8, 169 bytes. Try an engagement of record A (EAX) against record B (EDX); SKIP (BL) suppresses the actual attack. Proceeds only when B is within range (0x16b0c) and A's best weapon value (0x16bac) is not below the distance stored by it at 0x197df0. While the word 0x126518 is zero and either record passes 0x17474, byte 0x1f9011 is set instead; otherwise, unless SKIP, a best-weapon type of 1 first runs 0x16c7c and 0x173a4 on B, then 0x16e60 is called for the pair. */
void try_engagement_168d8(uint32_t a, uint32_t b, uint32_t skip)
{
    uint32_t kind[2];
    uint16_t id[2];
    uint8_t b_kind;
    double value;
    if (!within_range_16b0c(a, b))
        return;
    b_kind = EF_U8(EF_U32(0x0019bb58u) + b * 0xcau + 0x1bu);
    value = pick_best_weapon_16bac((int32_t)a, (ef_addr32)(uintptr_t)kind, (ef_addr32)(uintptr_t)id);
    if (value < (double)*(float *)ef_guest_memory_ptr(0x00197df0u))
        return;
    if (EF_U16(0x00126518u) == 0u && ((uint16_t)call_2dba4_17474((int16_t)a) != 0u || (uint16_t)call_2dba4_17474((int16_t)b) != 0u)) {
        EF_U8(0x001f9011u) = 1u;
        return;
    }
    if ((uint8_t)skip == 0u) {
        if (b_kind == 1u) {
            guest_fn_16c7c_x(b);
            decrement_grid_row_link_wide((int32_t)b);
        }
        guest_fn_16e60_x(a, b);
    }
}

/* 0x16c7c, 226 bytes. Strip the missile-like stores (types 7, 9..0xf, 0x15..0x1c) from record A (EAX) in the table at [0x19bb58]: for each of its seven object words at +0x50 (0xff skips), walk the 20 three-byte slots at obj+0x19b; a matching slot gets its count word cleared, after first calling 0x8f22c (and 0x56f0c when byte 0x127da9 is set) with the owner (obj+0x14 high word) and the running slot counter, unless byte 0x125847 is clear or the type is 9. The slot counter is not reset between objects (original behaviour). */
void clear_missile_slots_16c7c(uint32_t a)
{
    uint32_t rec = EF_U32(0x0019bb58u) + a * 0xcau;
    uint16_t counter = 0u;
    uint32_t k;
    for (k = 0u; k != 0xeu; k += 2u) {
        uint32_t wid = EF_U16(rec + k + 0x50u);
        ef_addr32 obj;
        ef_addr32 p;
        uint32_t i;
        if (wid == 0xffu)
            continue;
        obj = guest_fn_7489c((int32_t)(int16_t)wid);
        if (obj == 0u)
            continue;
        p = obj + 0x19bu;
        for (i = 0u; i < 0x14u; i++, p += 3u, counter++) {
            int32_t t = (int32_t)(int8_t)EF_U8(p);
            if ((uint32_t)t < 7u)
                continue;
            if ((uint32_t)t != 7u) {
                if ((uint32_t)t < 9u)
                    continue;
                if ((uint32_t)t > 0xfu && ((uint32_t)t < 0x15u || (uint32_t)t > 0x1cu))
                    continue;
            }
            if (EF_U8(0x00125847u) != 0u && t != 9 && t != 0x14) {
                guest_fn_8f22c((int32_t)EF_U32(obj + 0x14u) >> 16, (uint32_t)(uint8_t)counter);
                if (EF_U8(0x00127da9u) != 0u)
                    guest_fn_56f0c_x((int32_t)EF_U32(obj + 0x14u) >> 16, (uint32_t)(uint8_t)counter);
            }
            EF_U16(p + 1u) = 0u;
        }
    }
}

/* 0x2631c, 182 bytes. Per-frame update of the selected target when FLAG (EAX) is set: run 0x73084 and 0x2479c on the two signed high words at 0x1f71c6 (A) and 0x1f71ce (B); if 0x6a1b4(B) is zero finish with 0x24f94 and 0x24f24, otherwise a value from 0x6a5f0(A, B) above the double 0x181e88 calls 0x2541c(A, 2) and returns, else the ratio of the doubles 0x1f7000 / 0x19b2c8 picks 0x2611c(B) (below the double 0x181e90) or 0x24c40(B) before 0x24f24. */
void update_target_2631c(uint32_t flag)
{
    int32_t a;
    int32_t b;
    double c1;
    double c2;
    double num;
    double den;
    if (flag == 0u)
        return;
    guest_fn_73084_x();
    a = (int32_t)EF_U32(0x001f71c6u) >> 16;
    b = (int32_t)EF_U32(0x001f71ceu) >> 16;
    guest_fn_2479c_x(a, b, 0u);
    b = (int32_t)EF_U32(0x001f71ceu) >> 16;
    if (guest_fn_6a1b4(b) == 0u) {
        guest_fn_24f94_x();
    } else {
        a = (int32_t)EF_U32(0x001f71c6u) >> 16;
        b = (int32_t)EF_U32(0x001f71ceu) >> 16;
        memcpy(&c1, ef_guest_memory_ptr(0x00181e88u), sizeof c1);
        if (guest_fn_6a5f0_x(a, b) > c1) {
            guest_fn_2541c_x((int32_t)EF_U32(0x001f71c6u) >> 16, 2u);
            return;
        }
        memcpy(&num, ef_guest_memory_ptr(0x001f7000u), sizeof num);
        memcpy(&den, ef_guest_memory_ptr(0x0019b2c8u), sizeof den);
        memcpy(&c2, ef_guest_memory_ptr(0x00181e90u), sizeof c2);
        if (num / den < c2)
            guest_fn_2611c_x((int32_t)EF_U32(0x001f71ceu) >> 16);
        else
            guest_fn_24c40_x((int32_t)EF_U32(0x001f71ceu) >> 16);
    }
    guest_fn_24f24_x();
}

/* 0xc9680, 180 bytes. Draw a full circle of colour COLOUR (EAX, byte) in the viewport record at [0x41eea0] (nothing when NULL). D (EDX), B (EBX) and R (ECX) are signed words scaled by the double at viewport+0x9f; the circle centre is (cy = D*scale + (w41 + w71 - w69), cx = B*scale + (w43 + w73 - w6b)) with 16 bit offsets and the radius R*scale, each rounded with 0x9d72c; the arc runs from 0 to 0xffff via 0xfe49c. */
void draw_circle_scaled_c9680(uint32_t colour, uint32_t d, uint32_t b, uint32_t r)
{
    ef_addr32 vp = EF_U32(0x0041eea0u);
    double scale;
    int32_t radius;
    int32_t cx;
    int32_t cy;
    int16_t off_y;
    int16_t off_x;
    if (vp == 0u)
        return;
    memcpy(&scale, ef_guest_memory_ptr(vp + 0x9fu), sizeof scale);
    off_y = (int16_t)(EF_U16(vp + 0x41u) + EF_U16(vp + 0x71u) - EF_U16(vp + 0x69u));
    off_x = (int16_t)(EF_U16(vp + 0x43u) + EF_U16(vp + 0x73u) - EF_U16(vp + 0x6bu));
    radius = (int32_t)truncate_9d72c((double)(int16_t)r * scale);
    cx = (int32_t)truncate_9d72c((double)(int16_t)b * scale + (double)off_x);
    cy = (int32_t)truncate_9d72c((double)(int16_t)d * scale + (double)off_y);
    draw_arc_by_length_fe49c(colour & 0xffu, (uint32_t)cy, (uint32_t)cx, 0u, 0xffffu, (uint32_t)radius);
}

/* 0xd68, 183 bytes. Report a bad record P (stack, cdecl): when its word at +0x26 is set and the wrapping byte counter 0x120d2c reaches zero after incrementing, dump the record and its linked record at +0x34 (fields at +0x00..+0x18 of each plus the words and dwords at +0x1c..+0x38, with 0xd08 applied to the word at +0x24) through the error hook at 0x132a74 with the format 0x17f268. */
void report_bad_record_d68(ef_addr32 p)
{
    ef_addr32 q;
    if (EF_U16(p + 0x26u) == 0u)
        return;
    EF_U8(0x00120d2cu) = (uint8_t)(EF_U8(0x00120d2cu) + 1u);
    if (EF_U8(0x00120d2cu) != 0u)
        return;
    q = EF_U32(p + 0x34u);
    guest_callptr26_x(EF_U32(0x00132a74u), 0x0017f268u, EF_U16(p + 0x24u), guest_fn_d08_x(EF_U16(p + 0x24u)),
                      EF_U32(p + 0x28u), EF_U32(p + 0x30u), EF_U16(p + 0x2cu), EF_U16(p + 0x38u),
                      EF_U16(p + 0x1cu), EF_U16(p + 0x1eu), EF_U16(p + 0x20u), EF_U16(p + 0x22u),
                      EF_U32(p), EF_U32(p + 4u), EF_U32(p + 8u), EF_U32(p + 0xcu), EF_U32(p + 0x10u),
                      EF_U32(p + 0x14u), EF_U32(p + 0x18u), q, EF_U32(q), EF_U32(q + 4u), EF_U32(q + 8u),
                      EF_U32(q + 0xcu), EF_U32(q + 0x10u), EF_U32(q + 0x14u), EF_U32(q + 0x18u));
}

/* 0xe3f00, 188 bytes. Parse one "KEY=path" setting (TEXT in EAX): copy the identifier KEY, skip the '=' and build PATH with a leading and a trailing separator (0x190aac) unless it already starts or ends with a backslash. KEY is upper-cased and compared (0xa4380) with 0x190ab0, which stores PATH at 0x15f910, or else with 0x190ab4, which stores it at 0x15f920. The 16 byte KEY and PATH buffers live back to back with the cursor in one frame, so a long value overruns into it (original behaviour). */
void parse_path_setting_e3f00(ef_addr32 text)
{
    uint32_t frame[9];
    uint8_t *f = (uint8_t *)frame;
    ef_addr32 key = (ef_addr32)(uintptr_t)f;
    ef_addr32 buf = (ef_addr32)(uintptr_t)(f + 0x10);
    ef_addr32 cursor = (ef_addr32)(uintptr_t)(f + 0x20);
    memcpy(f + 0x20, &text, sizeof text);
    copy_identifier_10934c(key, cursor);
    expect_byte_109328(cursor, 0x3du);
    f[0x10] = 0u;
    memcpy(&text, f + 0x20, sizeof text);
    if (EF_U8(text) != 0x5cu)
        strcat_9ca57(buf, 0x00190aacu);
    memcpy(&text, f + 0x20, sizeof text);
    strcat_9ca57(buf, text);
    if (f[0x10 + guest_fn_b1b8b(buf) - 1u] != 0x5cu)
        strcat_9ca57(buf, 0x00190aacu);
    uppercase_string(key);
    if (guest_fn_a4380(key, 0x00190ab0u) == 0u) {
        copy_string_9d749(0x0015f910u, buf);
    } else if (guest_fn_a4380(key, 0x00190ab4u) == 0u) {
        copy_string_9d749(0x0015f920u, buf);
    }
}

/* 0x35c08, 189 bytes. Pack the game settings into the outgoing record at 0x19f370 (inverse of 0x3519c): copy the first 44 bytes from 0x125828, gather the bytes/words from 0x15b7e6, 0x1f88fa, 0x1f88f0, 0x1f88ee, 0x1f88fb, 0x1a3636, 0x1a3640 and 0x15b7e4 to +0x2c..+0x3d, widen the float at 0x128550 to a double at +0x33, copy the 18 bytes at 0x19bbf0 to +0x3e and queue the record as request 0x45 (0x36100, stack 3 and 1) with the high word of 0x1258fe. */
void pack_settings_record_35c08(void)
{
    double d;
    memcpy_dc832(0x0019f370u, 0x00125828u, 44u);
    EF_U8(0x0019f39cu) = EF_U8(0x0015b7e6u);
    EF_U8(0x0019f39du) = EF_U8(0x001f88fau);
    EF_U16(0x0019f39eu) = EF_U16(0x001f88f0u);
    EF_U16(0x0019f3a0u) = EF_U16(0x001f88eeu);
    EF_U8(0x0019f3a2u) = EF_U8(0x001f88fbu);
    d = (double)*(float *)ef_guest_memory_ptr(0x00128550u);
    memcpy(ef_guest_memory_ptr(0x0019f3a3u), &d, sizeof d);
    EF_U8(0x0019f3abu) = EF_U8(0x001a3636u);
    EF_U8(0x0019f3acu) = EF_U8(0x001a3640u);
    EF_U8(0x0019f3adu) = EF_U8(0x0015b7e4u);
    memcpy_dc832(0x0019f3aeu, 0x0019bbf0u, 18u);
    queue_request_36100(0x0019f370u, (uint32_t)((int32_t)EF_U32(0x001258feu) >> 16), 0x45u, 6u, 3u, 1u);
}

/* 0x6be28, 190 bytes. Follow-up for the current ship (record at [0x1f71bc]) when its mode byte at +0x16c is 6: look up the object named by the high word at +0x168 (0x8ffe8). Unless the lookup is empty or has a nonzero +0x1c or flags 1/2 at +0x30 clear and 0x6ae30(target) reports done, send the target (high word of 0x1f71c6) through 0x6a1d0(.., 1); then, if an object was found, scale its three dwords (+8, +0, +4) by the doubles 0x18640c and 0x186414 into a vector and pass it to 0x2461c with the target. */
void follow_target_6be28(void)
{
    ef_addr32 cur = EF_U32(0x001f71bcu);
    ef_addr32 obj;
    int32_t target;
    double vec[3];
    double c1;
    double c2;
    if (EF_U8(cur + 0x16cu) != 6u)
        return;
    obj = guest_fn_8ffe8((int32_t)EF_U32(cur + 0x168u) >> 16);
    target = (int32_t)EF_U32(0x001f71c6u) >> 16;
    if (obj == 0u) {
        if ((uint8_t)guest_fn_6ae30_x(target) == 0u)
            guest_fn_6a1d0(target, 1u);
    } else {
        uint8_t fl = EF_U8(obj + 0x30u);
        if (EF_U32(obj + 0x1cu) != 0u || (fl & 1u) != 0u || (fl & 2u) != 0u) {
            if ((uint8_t)guest_fn_6ae30_x(target) == 0u)
                guest_fn_6a1d0(target, 1u);
        }
    }
    if (obj == 0u)
        return;
    memcpy(&c1, ef_guest_memory_ptr(0x0018640cu), sizeof c1);
    memcpy(&c2, ef_guest_memory_ptr(0x00186414u), sizeof c2);
    vec[0] = (double)(int32_t)EF_U32(obj + 8u) * c1 * c2;
    vec[1] = (double)(int32_t)EF_U32(obj) * c1 * c2;
    vec[2] = (double)(int32_t)EF_U32(obj + 4u) * c1 * c2;
    guest_fn_2461c_x(target, (ef_addr32)(uintptr_t)vec);
}

/* 0x5414c, 195 bytes. Frame tick of the first cycling list: when the signed count word 0x19bb5c is nonzero, add the step byte 0x127d92 to the accumulator byte 0x127da0; once it exceeds 5 subtract 5 and run up to 1 (5 when byte 0x1a3647 is set) steps, each advancing the cursor word 0x1a5c18 (wrapping to 0 at the count), storing it as a double at 0x1a3148 and calling 0x2e740 with the cursor, count * double 0x185bac and 0. */
void tick_cycle_list_5414c(void)
{
    double scale;
    double c;
    int32_t n;
    int32_t j;
    uint8_t sum;
    int16_t count = (int16_t)EF_U16(0x0019bb5cu);
    memcpy(&c, ef_guest_memory_ptr(0x00185bacu), sizeof c);
    scale = (double)count * c;
    if (count == 0)
        return;
    n = EF_U8(0x001a3647u) != 0u ? 5 : 1;
    sum = (uint8_t)(EF_U8(0x00127da0u) + EF_U8(0x00127d92u));
    EF_U8(0x00127da0u) = sum;
    if ((int32_t)sum <= 5)
        return;
    EF_U8(0x00127da0u) = (uint8_t)(sum - 5u);
    for (j = 1; j <= n; j++) {
        double cur;
        uint16_t pos = (uint16_t)(EF_U16(0x001a5c18u) + 1u);
        EF_U16(0x001a5c18u) = pos;
        if ((int16_t)pos >= (int16_t)EF_U16(0x0019bb5cu))
            EF_U16(0x001a5c18u) = 0u;
        cur = (double)(int16_t)EF_U16(0x001a5c18u);
        memcpy(ef_guest_memory_ptr(0x001a3148u), &cur, sizeof cur);
        guest_fn_2e740_x((int32_t)(int16_t)EF_U16(0x001a5c18u), scale, 0u);
    }
}

/* 0x54274, 180 bytes. Twin of 0x5414c for the second list: count word 0x1a1506 (double 0x185bb4), accumulator byte 0x127da2 and cursor word 0x1a5c1c, calling 0x37eac with the cursor, count * scale and 0 for each step. */
void tick_cycle_list_54274(void)
{
    double scale;
    double c;
    int32_t n;
    int32_t j;
    uint8_t sum;
    uint16_t pos = EF_U16(0x001a5c1cu);
    int16_t count = (int16_t)EF_U16(0x001a1506u);
    memcpy(&c, ef_guest_memory_ptr(0x00185bb4u), sizeof c);
    scale = (double)count * c;
    if (count == 0)
        return;
    n = EF_U8(0x001a3647u) != 0u ? 5 : 1;
    sum = (uint8_t)(EF_U8(0x00127da2u) + EF_U8(0x00127d92u));
    EF_U8(0x00127da2u) = sum;
    if ((int32_t)sum <= 5)
        return;
    EF_U8(0x00127da2u) = (uint8_t)(sum - 5u);
    for (j = 1; j <= n; j++) {
        pos = (uint16_t)(pos + 1u);
        if ((int16_t)pos >= (int16_t)EF_U16(0x001a1506u))
            pos = 0u;
        EF_U16(0x001a5c1cu) = pos;
        guest_fn_37eac_x((int32_t)(int16_t)pos, scale, 0u);
        pos = EF_U16(0x001a5c1cu);
    }
    EF_U16(0x001a5c1cu) = pos;
}

/* 0x87db8, 196 bytes. Create and initialise a unit record: allocate one (0x879c4), mark the words at +0x47/+0x49 as 0x1234 and remember it in 0x1fd9f8, then fill it in with 0x87a20 (EAX = A, EDX = B, EBX = C, ECX = D plus the six stack arguments S0..S5, several reduced to signed words) while the word 0x20f194 is zero. Afterwards set +0x50 from byte 0x1a666b and +0x56 to 0xff, give a class-1 unit (class byte at +0x18 indexing the 0x91 byte table at 0x12bca7) its word at +0x68 from 0x80abc, run 0x88100 and, when byte 0x127da9 is set and bit 0 of +0x67 is clear, send its state with 0x55aac. */
void create_unit_87db8(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5)
{
    ef_addr32 unit = guest_fn_879c4_x();
    EF_U16(unit + 0x47u) = 0x1234u;
    EF_U32(0x001fd9f8u) = unit;
    EF_U16(unit + 0x49u) = 0x1234u;
    EF_U16(0x0020f194u) = 0u;
    guest_fn_87a20_x(unit, a, b, c, (uint32_t)(int32_t)(int16_t)d, (uint32_t)(int32_t)(int16_t)s0,
                     (uint32_t)(int32_t)(int16_t)s1, s2, (uint32_t)(int32_t)(int16_t)s3, s4, s5);
    EF_U16(0x0020f194u) = 1u;
    EF_U8(unit + 0x50u) = EF_U8(0x001a666bu);
    EF_U16(unit + 0x56u) = 0xffu;
    if (EF_U8((uint32_t)EF_U8(unit + 0x18u) * 0x91u + 0x0012bca7u) == 1u)
        EF_U16(unit + 0x68u) = (uint16_t)guest_fn_80abc_x();
    guest_fn_88100_x(unit);
    if (EF_U8(0x00127da9u) != 0u && (EF_U8(unit + 0x67u) & 1u) == 0u)
        send_record_state_55aac(unit);
}

/* 0x117b18, 198 bytes. Transmit pump of a serial port record P (EAX; I/O base at +0x1c, ring buffer record at +0, data at +4, flow control flag at +0x14, FIFO flag at +0x18, status record at +0x2c). With flow control on, bit 1 of the status word at +4 follows the modem line CTS (inb base+6 bit 4). When the transmitter holding register is empty (inb base+5 bit 5) send up to 16 bytes (1 without FIFO) from the ring: head word at +0, count word at +4, wrap size at +6. */
void pump_serial_transmit_117b18(ef_addr32 p)
{
    ef_addr32 base;
    ef_addr32 ring;
    uint32_t limit;
    uint32_t sent = 0u;
    if (EF_U32(p + 0x1cu) == 0u)
        return;
    if (EF_U32(p + 0x14u) != 0u) {
        ef_addr32 st = EF_U32(p + 0x2cu);
        if ((uint8_t)(guest_fn_9c430(EF_U32(p + 0x1cu) + 6u) & 0x10u) == 0u) {
            EF_U16(st + 4u) = (uint16_t)(EF_U16(st + 4u) | 2u);
            return;
        }
        EF_U16(st + 4u) = (uint16_t)(EF_U16(st + 4u) & ~2u);
    }
    base = EF_U32(p + 0x1cu);
    if ((uint8_t)(guest_fn_9c430(base + 5u) & 0x20u) == 0u)
        return;
    limit = EF_U32(p + 0x18u) != 0u ? 0x10u : 1u;
    ring = EF_U32(p);
    while ((int16_t)EF_U16(ring + 4u) > 0) {
        uint8_t b;
        int16_t head = (int16_t)EF_U16(ring);
        EF_U16(ring) = (uint16_t)(EF_U16(ring) + 1u);
        b = EF_U8(EF_U32(p + 4u) + (uint32_t)(int32_t)head);
        if (EF_U16(ring) == EF_U16(ring + 6u))
            EF_U16(ring) = 0u;
        EF_U16(ring + 4u) = (uint16_t)(EF_U16(ring + 4u) - 1u);
        sent = (sent + 1u) & 0xffu;
        guest_fn_117b0c(EF_U32(p + 0x1cu), b);
        if ((int16_t)EF_U16(ring + 4u) <= 0)
            return;
        if ((int32_t)sent >= (int32_t)limit)
            return;
    }
}

/* 0xc02ec, 197 bytes. For every index J below the count of list LIST (EDX, 0xbc5d0, signed word) and each of the kinds 0xf, 0x10 and 0x11, find the node (0xcc51c) and, if present, fold it into the running value with 0xc03b4 (EAX = running value, EDX = kind, EBX = J, ECX = LIST, stack: B, C, S0..S3 with B = EBX and C = ECX of this call). Returns the final running value (starts as AX = A). */
uint32_t fold_list_kinds_c02ec(uint32_t a, uint32_t list, uint32_t b, uint32_t c, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    uint16_t cur = (uint16_t)a;
    int16_t count = (int16_t)guest_fn_bc5d0_x(list);
    int16_t j;
    for (j = 0; j < count; j++) {
        int16_t k;
        for (k = 0; k < 3; k++) {
            uint32_t kind = k == 0 ? 0xfu : (k == 1 ? 0x10u : 0x11u);
            if ((uint16_t)find_node_matching_cc51c((uint32_t)(int32_t)j, list, kind) != 0u)
                cur = (uint16_t)guest_fn_c03b4_x((int32_t)(int16_t)cur, kind, (uint32_t)(uint8_t)j, list, b, c, s0, s1, s2, s3);
        }
    }
    return cur;
}

/* 0x284ec, 204 bytes. Startup of the data-file check: initialise the subsystems (0xd65e8, 0xc3980, 0xc34cc with 0x18235c, 0xc39ac, 0xc3888, 0xdc250, 0xde580 on 0x425c8c), then ask 0x27d18 about the request record {0x125200, word 2, dword 0}. If it answers 1 run 0x28224(1) and, for a nonnegative result, format it into 0x125208 with 0x1822ec; a negative result r runs 0x283c8 and formats -r - 1; then open the named file (0x287bc). Otherwise run 0x283c8, ask 0x28224(0), format its result likewise and use 0x28744. Finally release 0x425c8c with 0xde648. */
void check_data_files_284ec(void)
{
    uint8_t req[16];
    int16_t answer;
    int16_t r;
    guest_fn_d65e8();
    select_buffer_146e44();
    guest_fn_c34cc(0x0018235cu);
    guest_fn_c39ac();
    call_e9a24_c3888();
    guest_fn_dc250();
    guest_fn_de580(0x00425c8cu);
    memset(req, 0, sizeof req);
    {
        uint32_t base = 0x00125200u;
        uint16_t kind = 2u;
        memcpy(req, &base, sizeof base);
        memcpy(req + 8, &kind, sizeof kind);
    }
    answer = (int16_t)guest_fn_27d18_x((ef_addr32)(uintptr_t)req);
    if (answer == 1) {
        r = (int16_t)guest_fn_28224_x(1u);
        if (r < 0) {
            check_data_file_283c8();
            r = (int16_t)(-r - 1);
        }
        guest_sprintf_a4342(0x00125208u, 0x001822ecu, (uint32_t)(int32_t)r);
        open_named_file_287bc();
    } else {
        check_data_file_283c8();
        r = (int16_t)guest_fn_28224_x(0u);
        if (r < 0)
            r = (int16_t)(-r - 1);
        guest_sprintf_a4342(0x00125208u, 0x001822ecu, (uint32_t)(int32_t)r);
        open_named_file_28744();
    }
    guest_fn_de648(0x00425c8cu, 0u);
}

/* 0x7a66c, 204 bytes. Load or build a directory list for PATH (EAX) into the save buffer: with the flag 0x12afc8 set the count comes from the buffer (0x7a5a0), otherwise from listing PATH with the mask 0x187bc4 (0x32de8, 13 byte names at 0x19e5f0); the count is written back (0x7a584) and added to the word of the current section record ([0x1f8ea4]; 6 words per record, selected by its first word). For each entry take the name from the buffer (0x7a54c) or parse it from the listing with 0xdf26f(format 0x187bcc), append it to the pool (0x7a51c), copy it into the 0x11 byte slot [0x1f8e98] of the table at [0x1f8e9c], mark word +0xf of that slot with 1 and advance the slot counter. */
void load_directory_list_7a66c(ef_addr32 path)
{
    char name[0x50];
    ef_addr32 namep = (ef_addr32)(uintptr_t)name;
    int32_t count;
    int32_t i;
    ef_addr32 sec;
    ef_addr32 src = 0x0019e5f0u;
    if (EF_U32(0x0012afc8u) != 0u)
        count = buffer_read_word();
    else
        count = (int16_t)list_directory_names_32de8(path, 0x00187bc4u);
    buffer_write_word((int16_t)count);
    sec = EF_U32(0x001f8ea4u);
    {
        int32_t rec = (int32_t)(int16_t)EF_U16(sec) * 3;
        EF_U16(sec + (uint32_t)(rec * 2) + 4u) = (uint16_t)(EF_U16(sec + (uint32_t)(rec * 2) + 4u) + (uint32_t)count);
    }
    for (i = 0; i < count; i++) {
        uint32_t slot;
        if (EF_U32(0x0012afc8u) != 0u)
            copy_string_9d749(namep, pool_take_7a54c());
        else
            guest_fn_df26f_x(src, 0x00187bccu, namep);
        pool_append_7a51c(namep);
        copy_string_9d749(EF_U32(0x001f8e98u) * 0x11u + EF_U32(0x001f8e9cu), namep);
        slot = EF_U32(0x001f8e98u);
        EF_U16(slot * 0x11u + EF_U32(0x001f8e9cu) + 0xfu) = 1u;
        EF_U32(0x001f8e98u) = slot + 1u;
        src += 0xdu;
    }
}

/* 0x322ac, 207 bytes. Jitter the word 0x19e5cd (e.g. a shake offset) while the flags allow it (byte 0x19e557 is not 1, byte 0x19e5de is set, the object named by the high word of 0x1f71c8 exists and has +0x18d set). The amplitude is the double product 0x1a3078 * 0x182a31 * 0x182a39 rounded by 0x9d72c, plus 0x3c when byte 0x1a362e is set and plus 0x3c more when bit 0x20 of object+0x24 is set (16 bit); unless it is zero a random value (0xa45b3, 16 bit) modulo the amplitude minus half the amplitude is added to the word. */
void jitter_offset_322ac(void)
{
    ef_addr32 obj;
    double a, b, c;
    int32_t amp;
    uint16_t w;
    int32_t ws;
    int32_t r;
    int32_t half;
    if (EF_U8(0x0019e557u) == 1u || EF_U8(0x0019e5deu) == 0u)
        return;
    obj = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    if (obj == 0u || EF_U8(obj + 0x18du) == 0u)
        return;
    memcpy(&a, ef_guest_memory_ptr(0x001a3078u), sizeof a);
    memcpy(&b, ef_guest_memory_ptr(0x00182a31u), sizeof b);
    memcpy(&c, ef_guest_memory_ptr(0x00182a39u), sizeof c);
    amp = (int32_t)truncate_9d72c(a * b * c);
    w = (uint16_t)amp;
    if (EF_U8(0x001a362eu) != 0u)
        w = (uint16_t)(amp + 0x3c);
    if ((EF_U8(obj + 0x24u) & 0x20u) != 0u)
        w = (uint16_t)(w + 0x3c);
    if (w == 0u)
        return;
    ws = (int16_t)w;
    r = (int32_t)(guest_fn_a45b3() & 0xffffu) % ws;
    half = (ws - (ws >> 31)) >> 1;
    EF_U16(0x0019e5cdu) = (uint16_t)(EF_U16(0x0019e5cdu) + (uint32_t)(r - half));
}

/* 0x1debc, 209 bytes. Convert the floating point state of the record R (EAX) into its integer mirror: save the dwords at +9, +0xd and +0x11 to +0x1b, +0x1f and +0x23, then store the doubles at +0x59, +0x61 and +0x51 scaled by 0x1819f8 * 0x181a00 (rounded by 0x9d72c) into +9, +0xd and +0x11, and the doubles at +0x99, +0x89 and +0x81 scaled by 0x181a08 * 0x181a10 (the first negated) as signed words into +0x15, +0x17 and +0x19. */
void mirror_record_floats_1debc(ef_addr32 r)
{
    double c1, c2, c3, c4;
    double a51, a59, a61, a81, a89, a99;
    uint32_t old9 = EF_U32(r + 9u);
    uint32_t oldd = EF_U32(r + 0xdu);
    uint32_t old11 = EF_U32(r + 0x11u);
    memcpy(&c1, ef_guest_memory_ptr(0x001819f8u), sizeof c1);
    memcpy(&c2, ef_guest_memory_ptr(0x00181a00u), sizeof c2);
    memcpy(&c3, ef_guest_memory_ptr(0x00181a08u), sizeof c3);
    memcpy(&c4, ef_guest_memory_ptr(0x00181a10u), sizeof c4);
    memcpy(&a51, ef_guest_memory_ptr(r + 0x51u), sizeof a51);
    memcpy(&a59, ef_guest_memory_ptr(r + 0x59u), sizeof a59);
    memcpy(&a61, ef_guest_memory_ptr(r + 0x61u), sizeof a61);
    memcpy(&a81, ef_guest_memory_ptr(r + 0x81u), sizeof a81);
    memcpy(&a89, ef_guest_memory_ptr(r + 0x89u), sizeof a89);
    memcpy(&a99, ef_guest_memory_ptr(r + 0x99u), sizeof a99);
    EF_U32(r + 0x1bu) = old9;
    EF_U32(r + 0x1fu) = oldd;
    EF_U32(r + 0x23u) = old11;
    EF_U32(r + 9u) = (uint32_t)(int32_t)truncate_9d72c(a59 * c1 * c2);
    EF_U32(r + 0xdu) = (uint32_t)(int32_t)truncate_9d72c(a61 * c1 * c2);
    EF_U32(r + 0x11u) = (uint32_t)(int32_t)truncate_9d72c(a51 * c1 * c2);
    EF_U16(r + 0x15u) = (uint16_t)(int32_t)truncate_9d72c(-(a99 * c3 * c4));
    EF_U16(r + 0x17u) = (uint16_t)(int32_t)truncate_9d72c(a89 * c3 * c4);
    EF_U16(r + 0x19u) = (uint16_t)(int32_t)truncate_9d72c(a81 * c3 * c4);
}

/* 0xce9a4, 203 bytes. Clip the segment stored in the record R (EAX; segment record at +0x4c with its two end points at +4 and +8) against the up to five planes whose mask bits at 0x15b798 + 8*i match the dword at R+0x2c (plane object at 0x15b79c + 8*i, its outside test is the hook at +0x14). If both end points are outside one plane return 0; otherwise replace an outside end point by the intersection (0xce5f8) and, after all planes, store the clipped end points back and return 1. Sets 0x4208ac to 0x420508 first. */
uint32_t clip_segment_ce9a4(ef_addr32 r)
{
    ef_addr32 seg = EF_U32(r + 0x4cu);
    ef_addr32 p0 = EF_U32(seg + 4u);
    ef_addr32 p1 = EF_U32(seg + 8u);
    uint32_t i;
    EF_U32(0x004208acu) = 0x00420508u;
    for (i = 0u; i < 0x28u; i += 8u) {
        if ((EF_U32(i + 0x0015b798u) & EF_U32(r + 0x2cu)) != 0u) {
            ef_addr32 plane = EF_U32(i + 0x0015b79cu);
            uint32_t out0 = guest_callptr4_ret_x(EF_U32(plane + 0x14u), plane, EF_U32(p0), EF_U32(p0 + 4u), EF_U32(p0 + 8u)) & 0xffu;
            uint32_t out1 = guest_callptr4_ret_x(EF_U32(plane + 0x14u), plane, EF_U32(p1), EF_U32(p1 + 4u), EF_U32(p1 + 8u)) & 0xffu;
            if (out0 != 0u && out1 != 0u)
                return 0u;
            if (out0 != 0u)
                p0 = guest_fn_ce5f8_x(plane, p0, p1);
            if (out1 != 0u)
                p1 = guest_fn_ce5f8_x(plane, p0, p1);
        }
    }
    EF_U32(EF_U32(r + 0x4cu) + 4u) = p0;
    EF_U32(EF_U32(r + 0x4cu) + 8u) = p1;
    return 1u;
}

/* 0xc95ac, 212 bytes. Draw a rectangle in colour COLOUR (EAX, byte) given in viewport units: the signed words X1 (EDX), Y1 (EBX), X2 (ECX) and Y2 (stack, ret 4) are scaled by the double at viewport+0x9f of the record at [0x41eea0] (nothing when NULL), offset by the viewport words (x: +0x41 + +0x71 - +0x69, y: +0x43 + +0x73 - +0x6b), rounded with 0x9d72c and drawn with 0xc8ccc(y2, x2, y1, x1, colour). */
void draw_scaled_rect_c95ac(uint32_t colour, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
    ef_addr32 vp = EF_U32(0x0041eea0u);
    double scale;
    double ox;
    double oy;
    int32_t r1;
    int32_t r2;
    int32_t r3;
    int32_t r4;
    if (vp == 0u)
        return;
    memcpy(&scale, ef_guest_memory_ptr(vp + 0x9fu), sizeof scale);
    ox = (double)(int16_t)EF_U16(vp + 0x41u);
    r1 = (int32_t)truncate_9d72c((double)(int16_t)x1 * scale + ox + (double)(int16_t)EF_U16(vp + 0x71u) - (double)(int16_t)EF_U16(vp + 0x69u));
    oy = (double)(int16_t)EF_U16(vp + 0x43u);
    r2 = (int32_t)truncate_9d72c((double)(int16_t)y1 * scale + oy + (double)(int16_t)EF_U16(vp + 0x73u) - (double)(int16_t)EF_U16(vp + 0x6bu));
    r3 = (int32_t)truncate_9d72c((double)(int16_t)x2 * scale + ox + (double)(int16_t)EF_U16(vp + 0x71u) - (double)(int16_t)EF_U16(vp + 0x69u));
    r4 = (int32_t)truncate_9d72c((double)(int16_t)y2 * scale + oy + (double)(int16_t)EF_U16(vp + 0x73u) - (double)(int16_t)EF_U16(vp + 0x6bu));
    guest_fn_c8ccc_x((uint32_t)r4, (uint32_t)r3, (uint32_t)r2, (uint32_t)r1, colour & 0xffu);
}

/* 0xe9c00, 214 bytes. Toggle the mode byte 0x43399d (hysteresis on a distance): in mode 1 run the first constraint solver 0xe9e7c on 0x464480 (six dwords from 0x4321f0, 0x4321f4, 0x432288, 0x43228c, 0x432220, 0x432224) and 0xe9f84 and clear the mode when |double at 0x432298| drops below the double 0x191494; otherwise run 0xe9cd8 (0x432230, 0x432234, 0x432298, 0x43229c, 0x4321f8, 0x4321fc) and 0xe9dcc and set the mode when |double at 0x432298| exceeds the double 0x19148c. */
void update_mode_e9c00(void)
{
    double d;
    double limit;
    if (EF_U8(0x0043399du) == 1u) {
        guest_fn_e9e7c_x(0x00464480u, EF_U32(0x004321f0u), EF_U32(0x004321f4u), EF_U32(0x00432288u),
                         EF_U32(0x0043228cu), EF_U32(0x00432220u), EF_U32(0x00432224u));
        guest_fn_e9f84_x();
        memcpy(&d, ef_guest_memory_ptr(0x00432298u), sizeof d);
        memcpy(&limit, ef_guest_memory_ptr(0x00191494u), sizeof limit);
        if (abs_double_b427c(d) < limit)
            EF_U8(0x0043399du) = 0u;
    } else {
        guest_fn_e9cd8_x(0x00464480u, EF_U32(0x00432230u), EF_U32(0x00432234u), EF_U32(0x00432298u),
                         EF_U32(0x0043229cu), EF_U32(0x004321f8u), EF_U32(0x004321fcu));
        guest_fn_e9dcc_x();
        memcpy(&d, ef_guest_memory_ptr(0x00432298u), sizeof d);
        memcpy(&limit, ef_guest_memory_ptr(0x0019148cu), sizeof limit);
        if (abs_double_b427c(d) > limit)
            EF_U8(0x0043399du) = 1u;
    }
}

/* 0x872c8, 217 bytes. Spawn a unit at the current position of the selected object: look up the object named by the high word of 0x1f71c8 (nothing if missing), build the position vector from the dwords 0x1a5bec, 0x1a5bf0 and 0x1a5be8 shifted left by 8, run 0xe9b38 with the vector, the three signed high words at 0x127d3a, 0x127d3c, 0x127d3e and 0x85c90 of the top byte of object+0x1d4, then (with the flag word 0x20f194 set around it) create the unit record at 0x20f1a0 through 0x87a20 with the vector and the stack values: the three high words, 0x1a5be4, the signed word A (EAX), B (EDX) and the object. */
void spawn_at_selected_872c8(uint32_t a, uint32_t b)
{
    ef_addr32 obj = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    uint32_t vec[3];
    uint32_t w3a;
    uint32_t w3c;
    uint32_t w3e;
    if (obj == 0u)
        return;
    vec[0] = EF_U32(0x001a5becu) << 8;
    vec[1] = EF_U32(0x001a5bf0u) << 8;
    vec[2] = EF_U32(0x001a5be8u) << 8;
    w3a = (uint32_t)((int32_t)EF_U32(0x00127d3au) >> 16);
    w3c = (uint32_t)((int32_t)EF_U32(0x00127d3cu) >> 16);
    w3e = (uint32_t)((int32_t)EF_U32(0x00127d3eu) >> 16);
    guest_fn_e9b38_x(page_row_85c90((int16_t)((int32_t)EF_U32(obj + 0x1d4u) >> 24)), w3e, w3c, w3a,
                     (uint32_t)(uintptr_t)vec);
    EF_U16(0x0020f194u) = 1u;
    guest_fn_87a20_x(0x0020f1a0u, vec[0], vec[1], vec[2], w3a, w3c, w3e, EF_U32(0x001a5be4u),
                     (uint32_t)(int32_t)(int16_t)a, b, obj);
    EF_U16(0x0020f194u) = 0u;
}

/* 0xb686c, 216 bytes. Fill the slots of the record A (EAX; slot table pointer at +0x13, slot count byte at +0x12, 0x7c byte slots) from the nearest node: walk the node list whose head pointer is at *LISTHEAD (EBX) for a type 0x31 node (+0xc) other than OBJ (EDX) that passes the group test 0xcc22c(OBJ.+0xb, node.+0xb, MASK = ECX) and is closer than 100000.0 by 0xf83d0 (positions at +0x11 and +0x15); the nearest, or OBJ itself when none, then fills every slot: +0x24 gets a copy (0xcc744) of the pointer at node+0xd, +0x28 the 18 bytes at node+0x11. */
void fill_slots_from_nearest_b686c(ef_addr32 rec, ef_addr32 obj, ef_addr32 listhead, uint32_t mask)
{
    ef_addr32 node = EF_U32(listhead);
    ef_addr32 best = 0u;
    double bestd = 100000.0;
    ef_addr32 src;
    int16_t i;
    while (node != 0u) {
        if ((uint16_t)group_flag_positive_cc22c(EF_U8(obj + 0xbu), EF_U8(node + 0xbu), mask) != 0u && node != obj &&
            EF_U8(node + 0xcu) == 0x31u) {
            double d = int_distance_2d_f83d0((int32_t)EF_U32(obj + 0x11u), (int32_t)EF_U32(node + 0x11u),
                                             (int32_t)EF_U32(obj + 0x15u), (int32_t)EF_U32(node + 0x15u));
            if (d < bestd) {
                bestd = d;
                best = node;
            }
        }
        node = EF_U32(node + 4u);
    }
    src = best != 0u ? best : obj;
    for (i = 0; i < (int32_t)EF_U8(rec + 0x12u); i++) {
        ef_addr32 name = EF_U32(src + 0xdu);
        uint32_t copy = name != 0u ? alloc_checked_cc744(name) : 0u;
        ef_addr32 slot = EF_U32(rec + 0x13u) + (uint32_t)((int32_t)i * 0x7c);
        EF_U32(slot + 0x24u) = copy;
        memcpy(ef_guest_memory_ptr(slot + 0x28u), ef_guest_memory_ptr(src + 0x11u), 18u);
    }
}

/* 0x92e30, 222 bytes. Load the camera record 0x214378 from the object UNIT (word in EDX, 0x7489c): convert the float 0x1a5bb8 to the dword 0x1a5bf0 (0x9d72c) and initialise the bounds of 0x214404 and 0x214378. Without an object set word 0x21449c to 1 and copy byte 0x214486 to 0x12db9e. Otherwise copy the position (dwords +0..+8) to 0x21446c, run 0x95bf4 when byte +0xe9 is zero, rotate by the words at +0xe, +0xc (only when byte 0x21454b is set) and +0x10 and, if the target named by the high word of +0x1e resolves (0x9125c), store that result in word 0x21448a; finish with 0x9357c. The EBX and ECX arguments of the rotate helpers are not set by the original (EBX is the object for 0xd0dbc). */
void load_camera_92e30(uint32_t unit)
{
    ef_addr32 obj;
    double v = (double)*(float *)ef_guest_memory_ptr(0x001a5bb8u);
    EF_U32(0x001a5bf0u) = (uint32_t)(int32_t)truncate_9d72c(v);
    guest_fn_d0b68(0x00214404u, 0u);
    guest_fn_d0b68(0x00214378u, 0u);
    obj = guest_fn_7489c((int32_t)(int16_t)unit);
    if (obj == 0u) {
        EF_U16(0x0021449cu) = 1u;
        EF_U8(0x0012db9eu) = EF_U8(0x00214486u);
        return;
    }
    EF_U32(0x0021446cu) = EF_U32(obj);
    EF_U32(0x00214470u) = EF_U32(obj + 4u);
    EF_U32(0x00214474u) = EF_U32(obj + 8u);
    if (EF_U8(obj + 0xe9u) == 0u)
        guest_fn_95bf4_x();
    guest_fn_d0ebc(0x00214378u, EF_U16(obj + 0xeu));
    if (EF_U8(0x0021454bu) != 0u)
        guest_fn_d0dbc(0x00214378u, EF_U16(obj + 0xcu), obj);
    guest_fn_d0fb8(0x00214378u, EF_U16(obj + 0x10u), 0u);
    if ((uint16_t)guest_fn_9125c((int32_t)EF_U32(obj + 0x1eu) >> 16) != 0u)
        EF_U16(0x0021448au) = (uint16_t)guest_fn_9125c((int32_t)EF_U32(obj + 0x1eu) >> 16);
    guest_fn_9357c();
}

/* 0xd299c, 223 bytes. Clip the segment between two points of the table at [0x17bf70] (byte offsets A = AX and B = EDX, 16 bit) against the view planes and return the result: build the clip record 0x420970 (flags 0x42099c = 1 & the dwords at +0x20 of both points after clearing bits 1 and 2 there, point count 2, points at 0x4209d4/0x4209d8), classify it with 0xd2130 and return 0 when bit 3 of the flag byte is set or the clip 0xce9a4 rejects it; otherwise run 0xd2110 and copy the words +0x1a and +0x1e of the two clipped points into the four words at *OUT (EBX) and return 1. */
uint8_t clip_segment_points_d299c(uint32_t a, uint32_t b, ef_addr32 out)
{
    ef_addr32 base = EF_U32(0x0017bf70u);
    ef_addr32 p1 = base + (a & 0xffffu);
    ef_addr32 p2 = base + (b & 0xffffu);
    uint32_t flags;
    ef_addr32 pts;
    EF_U32(0x0042099cu) = 1u;
    EF_U32(0x00420974u) = 0u;
    EF_U32(0x004209bcu) = 0x004209d0u;
    EF_U32(0x004209d0u) = 2u;
    EF_U32(0x004209d4u) = p1;
    EF_U32(0x004209d8u) = p2;
    EF_U8(p1 + 0x20u) = (uint8_t)(EF_U8(p1 + 0x20u) & 0xf9u);
    EF_U8(p2 + 0x20u) = (uint8_t)(EF_U8(p2 + 0x20u) & 0xf9u);
    flags = EF_U32(0x0042099cu) & EF_U32(p1 + 0x20u);
    EF_U32(0x0042099cu) = flags;
    flags &= EF_U32(p2 + 0x20u);
    EF_U32(0x0042099cu) = flags;
    guest_fn_d2130_x(0x00420970u);
    if ((EF_U8(0x0042099cu) & 8u) != 0u)
        return 0u;
    if ((uint8_t)clip_segment_ce9a4(0x00420970u) == 0u)
        return 0u;
    forward_record_d2110(0x00420970u);
    pts = EF_U32(0x004209bcu);
    EF_U16(out) = EF_U16(EF_U32(pts + 4u) + 0x1au);
    EF_U16(out + 2u) = EF_U16(EF_U32(pts + 4u) + 0x1eu);
    EF_U16(out + 4u) = EF_U16(EF_U32(pts + 8u) + 0x1au);
    EF_U16(out + 6u) = EF_U16(EF_U32(pts + 8u) + 0x1eu);
    return 1u;
}

/* 0x89060, 221 bytes. Test whether the unit record R (EAX) can reach the point: store the target dword T (EDX, when nonzero and R's class byte +0x18 is 6) at +0x30, fetch three vectors of R (0x88c54, 0x88c80, 0x88d4c), build the direction 0x88fb4 from two of them against 0x1fcf58/0x1fcf88, subtract (0x88d30), normalise (0xb4424) and return 1 when the distance 0x88ff8 is below the class limit: the double at class*0x91+0x12bcd5 times 0x18a4d0 times 0x18a4d8. */
uint8_t can_reach_target_89060(ef_addr32 rec, uint32_t target)
{
    double v48[3];
    double v60[3];
    double v78[3];
    double v80[3];
    double v2c[6];
    double v18[3];
    double v00[3];
    double v30[3];
    double c1;
    double c2;
    double limit;
    uint32_t cls;
    if (target != 0u && EF_U8(rec + 0x18u) == 6u)
        EF_U32(rec + 0x30u) = target;
    guest_fn_88c54_x(rec, (ef_addr32)(uintptr_t)v48);
    guest_fn_88c80_x(rec, (ef_addr32)(uintptr_t)v60, (ef_addr32)(uintptr_t)v80);
    guest_fn_88d4c_x(rec, (ef_addr32)(uintptr_t)v78);
    direction_vector_88fb4((ef_addr32)(uintptr_t)v2c, 0x001fcf58u, 0x001fcf88u, rec, v80[0], v78[0]);
    subtract_scene_vectors((ef_addr32)(uintptr_t)v18, 0x001fcf58u, 0x001fcf88u);
    if (normalize_vector_b4424((ef_addr32)(uintptr_t)v00, (ef_addr32)(uintptr_t)v18) == 0u)
        return 0u;
    cls = EF_U8(rec + 0x18u);
    memcpy(&limit, ef_guest_memory_ptr(cls * 0x91u + 0x0012bcd5u), sizeof limit);
    memcpy(&c1, ef_guest_memory_ptr(0x0018a4d0u), sizeof c1);
    memcpy(&c2, ef_guest_memory_ptr(0x0018a4d8u), sizeof c2);
    limit = limit * c1 * c2;
    return guest_fn_88ff8_x((ef_addr32)(uintptr_t)v30, (ef_addr32)(uintptr_t)v00) < limit ? 1u : 0u;
}

/* 0x5a220, 176 bytes. Refresh the map cursor: latch (0xce328) and apply (0xcdf48) the four signed high words 0x1a6cf0, 0x1a6cee, 0x1a6cf6 and 0x1a6cf4. When the selected node [0x1a821c] exists and projects (0x5ca88) to a point (two signed words), update it with 0x5d520 and, if it is also the current node [0x1a8210], run 0x5dbc8 on the point and 0x5df08. */
void refresh_map_cursor_5a220(void)
{
    uint32_t a = (uint32_t)((int32_t)EF_U32(0x001a6cf0u) >> 16);
    uint32_t b = (uint32_t)((int32_t)EF_U32(0x001a6cf6u) >> 16);
    uint32_t c = (uint32_t)((int32_t)EF_U32(0x001a6ceeu) >> 16);
    uint32_t d = (uint32_t)((int32_t)EF_U32(0x001a6cf4u) >> 16);
    ef_addr32 node;
    int16_t pt[2];
    latch_values_ce328(a, b, c, d);
    guest_fn_cdf48(a, b, c, d);
    node = EF_U32(0x001a821cu);
    if (node == 0u)
        return;
    if ((uint8_t)guest_fn_5ca88_ret_x(node, (ef_addr32)(uintptr_t)pt) == 0u)
        return;
    guest_fn_5d520_x(pt[0], pt[1], node);
    if (EF_U32(0x001a8210u) == EF_U32(0x001a821cu)) {
        guest_fn_5dbc8_x(pt[0], pt[1]);
        guest_fn_5df08_x();
    }
}

/* 0x24e58, 175 bytes. Point the view at object ID (AX, word; nothing for an unknown object): select it for the current target (0x2479c with the high word of 0x1f71c6, ID and 1) and, unless ID is 0xff, store the bearing atan2(dy, dx) to the object, relative to the current ship ([0x1f71bc], doubles at +0x51 / +0x49), at 0x1f70f8, copy the double at object+0x59 to 0x1f7088 clamped to at most double 0x1284d4 + double 0x181d30, and run 0x24f24. Always finish with 0x6d948. */
void select_view_object_24e58(uint32_t id)
{
    int32_t sid = (int16_t)id;
    ef_addr32 obj = guest_fn_7489c(sid);
    ef_addr32 cur;
    double dx, dy, bearing, v, limit, t;
    double ox, oy, cx, cy;
    if (obj == 0u)
        return;
    guest_fn_2479c_x((int32_t)EF_U32(0x001f71c6u) >> 16, sid, 1u);
    if (sid != 0xff) {
        cur = EF_U32(0x001f71bcu);
        memcpy(&ox, ef_guest_memory_ptr(obj + 0x49u), sizeof ox);
        memcpy(&cx, ef_guest_memory_ptr(cur + 0x49u), sizeof cx);
        memcpy(&oy, ef_guest_memory_ptr(obj + 0x51u), sizeof oy);
        memcpy(&cy, ef_guest_memory_ptr(cur + 0x51u), sizeof cy);
        dx = ox - cx;
        dy = oy - cy;
        bearing = atan2_b4620(dy, dx);
        memcpy(ef_guest_memory_ptr(0x001f70f8u), &bearing, sizeof bearing);
        EF_U32(0x001f7088u) = EF_U32(obj + 0x59u);
        EF_U32(0x001f708cu) = EF_U32(obj + 0x5du);
        memcpy(&t, ef_guest_memory_ptr(0x001284d4u), sizeof t);
        memcpy(&limit, ef_guest_memory_ptr(0x00181d30u), sizeof limit);
        limit += t;
        memcpy(&v, ef_guest_memory_ptr(0x001f7088u), sizeof v);
        if (v > limit)
            memcpy(ef_guest_memory_ptr(0x001f7088u), &limit, sizeof limit);
        guest_fn_24f24_x();
    }
    guest_fn_6d948_x();
}

/* 0x2d9a4, 176 bytes. Check whether the record ID (AX; table 0xca byte records at [0x19bb58]) is in range of its target: skipped for ID 0xff, a target word (+0xb9) of 0xff or a kind byte (+0xbb) of 0xff. The distance comes from 0x37bc8 (kind 1) or 0x2bae0 of the target block at +0x61 (EDX is the target word, left over in the original); when the record's state byte +0x1b is 6 or 7 and the distance is below the double at (+0xbd)->+4, run 0x2b1f4(ID, 6) and, if 0x2dba4 then reports true, set byte 0x1f9011. */
void check_target_range_2d9a4(uint32_t id)
{
    int32_t sid = (int16_t)id;
    ef_addr32 rec;
    int32_t tw;
    uint32_t kind;
    double d;
    double limit;
    ef_addr32 lim;
    if (sid == 0xff)
        return;
    rec = EF_U32(0x0019bb58u) + (uint32_t)sid * 0xcau;
    tw = (int32_t)EF_U32(rec + 0xb7u) >> 16;
    if (tw == 0xff)
        return;
    kind = EF_U8(rec + 0xbbu);
    if (kind == 0xffu)
        return;
    if (kind == 1u)
        d = scaled_pair_distance_37bc8(rec + 0x61u, (int16_t)tw);
    else
        d = guest_fn_2bae0_x(rec + 0x61u, tw);
    rec = EF_U32(0x0019bb58u) + (uint32_t)sid * 0xcau;
    if (EF_U8(rec + 0x1bu) < 6u || EF_U8(rec + 0x1bu) > 7u)
        return;
    lim = EF_U32(rec + 0xbdu);
    memcpy(&limit, ef_guest_memory_ptr(lim + 4u), sizeof limit);
    if (d >= limit)
        return;
    guest_fn_2b1f4_x(sid, 6u);
    if ((uint8_t)guest_fn_2dba4(sid) != 0u)
        EF_U8(0x001f9011u) = 1u;
}

/* 0xc98f8, 177 bytes. Arc version of 0xc9680: draw an arc in colour COLOUR (EAX, byte) from angle LO (ECX, passed straight through) to HI (stack dword) in the viewport record at [0x41eea0] (nothing when NULL). D (EDX), B (EBX) and the radius R (second stack word, ret 8) are signed words scaled by the double at viewport+0x9f; the centre is (cy = D*scale + (w41 + w71 - w69), cx = B*scale + (w43 + w73 - w6b)), all rounded with 0x9d72c, and the arc is drawn with 0xfe49c. */
void draw_arc_scaled_c98f8(uint32_t colour, uint32_t d, uint32_t b, uint32_t lo, uint32_t hi, uint32_t r)
{
    ef_addr32 vp = EF_U32(0x0041eea0u);
    double scale;
    int32_t radius;
    int32_t cx;
    int32_t cy;
    int16_t off_y;
    int16_t off_x;
    if (vp == 0u)
        return;
    memcpy(&scale, ef_guest_memory_ptr(vp + 0x9fu), sizeof scale);
    off_y = (int16_t)(EF_U16(vp + 0x41u) + EF_U16(vp + 0x71u) - EF_U16(vp + 0x69u));
    off_x = (int16_t)(EF_U16(vp + 0x43u) + EF_U16(vp + 0x73u) - EF_U16(vp + 0x6bu));
    radius = (int32_t)truncate_9d72c((double)(int16_t)r * scale);
    cx = (int32_t)truncate_9d72c((double)(int16_t)b * scale + (double)off_x);
    cy = (int32_t)truncate_9d72c((double)(int16_t)d * scale + (double)off_y);
    draw_arc_by_length_fe49c(colour & 0xffu, (uint32_t)cy, (uint32_t)cx, lo, hi, (uint32_t)radius);
}

/* 0x6a760, 202 bytes. Signed distance of unit B (EDX) along the axis from unit A (EAX): look both up (0x6a1b4); when either is missing or they are the same unit the result is 0.0; otherwise take the position difference (B - A, doubles at +0x49/+0x51/+0x59), the axis vector (0x6a6e8) and the separation (0x6a66c), and return -(dot product of difference and axis) / separation, or 0.0 when the separation is not positive. */
double axis_offset_6a760(uint32_t a, uint32_t b)
{
    int16_t ia = (int16_t)a;
    int16_t ib = (int16_t)b;
    ef_addr32 pa = guest_fn_6a1b4(ia);
    ef_addr32 pb = guest_fn_6a1b4(ib);
    double axis[3];
    double diff[3];
    double len;
    double xa, xb;
    int i;
    if (pa == 0u || pb == 0u || pa == pb)
        return 0.0;
    for (i = 0; i < 3; i++) {
        memcpy(&xb, ef_guest_memory_ptr(pb + 0x49u + (uint32_t)i * 8u), sizeof xb);
        memcpy(&xa, ef_guest_memory_ptr(pa + 0x49u + (uint32_t)i * 8u), sizeof xa);
        diff[i] = xb - xa;
    }
    unit_axis_difference_6a6e8((ef_addr32)(uintptr_t)axis, ia, ib);
    len = unit_distance_6a66c(ia, ib);
    if (0.0 >= len)
        return 0.0;
    return ((-diff[1]) * axis[1] + (-diff[0]) * axis[0] + (-diff[2]) * axis[2]) / len;
}

/* 0x7308, 201 bytes. Occasionally fire a message from object B (EDX) at the nearest record of the owner of A (EAX; owner word at +0x15 of... high word of +0x13): search the table with 0x2c454 at B+0x61 (first with mode 0xd, then 0), skip when none (0xff) or the record's flag byte +0x1a is clear or its grid distance to the target word at 0x197262 scaled by the double 0x1803b4 exceeds the double 0x1803bc, and fire only when a random number (0x9c960) is a multiple of the byte 0x197268 (the original divides by it unchecked). Then call 0x46254 with the words at +0x4e of A and the record, the code 0x3a, the target word and two zero stack values. */
void maybe_fire_message_7308(ef_addr32 a, ef_addr32 b)
{
    ef_addr32 p = b + 0x61u;
    int32_t hw = (int32_t)EF_U32(a + 0x13u) >> 16;
    int32_t id = (int16_t)guest_fn_2c454(p, hw, 0xdu, 1u);
    ef_addr32 rec;
    double d;
    double limit;
    double scale;
    uint32_t r;
    uint32_t div;
    int32_t target = (int32_t)EF_U32(0x00197262u) >> 16;
    if (id == 0xff) {
        id = (int16_t)guest_fn_2c454(p, (int32_t)EF_U32(a + 0x13u) >> 16, 0u, 1u);
        if (id == 0xff)
            return;
    }
    rec = EF_U32(0x0019bb58u) + (uint32_t)id * 0xcau;
    if (rec == 0u || EF_U8(rec + 0x1au) == 0u)
        return;
    memcpy(&scale, ef_guest_memory_ptr(0x001803b4u), sizeof scale);
    memcpy(&limit, ef_guest_memory_ptr(0x001803bcu), sizeof limit);
    d = grid_cell_distance((int16_t)id, (int16_t)target) * scale;
    if (d > limit)
        return;
    r = guest_fn_9c960() & 0xffffu;
    div = EF_U8(0x00197268u);
    if (r % div != 0u)
        return;
    guest_fn_46254((uint32_t)((int32_t)EF_U32(a + 0x4eu) >> 16), (int32_t)EF_U32(rec + 0x4eu) >> 16, 0x3au,
                   (uint32_t)target, 0u, 0u);
}

/* 0x172b4, 211 bytes. Release one pending item of record ID (EAX; 0xca byte table at [0x19bb58]): needs a nonzero count byte +0x4f and flag byte +0x1a, otherwise (and when the item word at +0x50 is 0xff, or its object is gone) returns 0xff. The item word is cleared; its object (0x7489c) gets bit 3 of +0x24 set, with 0x6a1d0(item, 0) and 0x741f0, and 0x56444(item, 0x64, -1) when byte 0x127da9 is set. Then the count is decremented, at zero 0x2b0b4(ID, 0) runs and the count byte is cleared; finally 0x2b708(ID). Returns the item word. */
uint32_t release_pending_item_172b4(uint32_t id)
{
    ef_addr32 rec = EF_U32(0x0019bb58u) + id * 0xcau;
    uint16_t item;
    int32_t sitem;
    ef_addr32 obj;
    uint8_t cnt;
    if (EF_U8(rec + 0x4fu) == 0u || EF_U8(rec + 0x1au) == 0u)
        return 0xffu;
    item = EF_U16(rec + 0x50u);
    if (item == 0xffu)
        return 0xffu;
    sitem = (int16_t)item;
    EF_U16(rec + 0x50u) = 0xffu;
    obj = guest_fn_7489c(sitem);
    if (obj == 0u)
        return 0xffu;
    if ((EF_U8(obj + 0x24u) & 8u) == 0u) {
        EF_U8(obj + 0x24u) = (uint8_t)(EF_U8(obj + 0x24u) | 8u);
        guest_fn_6a1d0(sitem, 0u);
        guest_fn_741f0(obj);
        if (EF_U8(0x00127da9u) != 0u)
            send_unit_options_56444((int16_t)sitem, 0x64u, 0xffffu);
    }
    rec = EF_U32(0x0019bb58u) + id * 0xcau;
    cnt = (uint8_t)(EF_U8(rec + 0x4fu) - 1u);
    EF_U8(rec + 0x4fu) = cnt;
    if (cnt == 0u) {
        guest_fn_2b0b4_x((int16_t)id, 0u);
        EF_U8(EF_U32(0x0019bb58u) + id * 0xcau + 0x4fu) = 0u;
    }
    guest_fn_2b708((int16_t)id);
    return item;
}

/* 0xcfa9c, 204 bytes. Distance of the point (XB = ECX, YB = stack, ret 4) from the entry found for the point (XA = EDX, YA = EBX) in the shape of page ID (AX): fetch the page pair (0xa1a40 key 0x18f690, 0xa1a90) and the orientation 0xa1858(ID, YB); rotate the offset (XB - XA, YB - YA) by that quarter turn (0 = as is, 1 = (-dy, dx), 2 = (-dx, -dy), 3 = (dy, -dx)); look the rotated point up with 0xcfa24 (entry count word first, entries after it). Returns |(hi(+0x28) * X + hi(+0x2c) * Y + (hi(+0x2e) << 15)) / hi(+0x2a)| (hi = signed high word of the dword), or 0 when nothing is found. */
uint32_t page_point_distance_cfa9c(uint32_t id, uint32_t xa, uint32_t ya, uint32_t xb, uint32_t yb)
{
    int32_t sid = (int16_t)id;
    uint32_t key = guest_fn_a1a40_key(0x0018f690u);
    ef_addr32 page;
    ef_addr32 entries;
    int32_t count;
    int32_t dx;
    int32_t dy;
    int32_t x;
    int32_t y;
    ef_addr32 e;
    int32_t num;
    int32_t den;
    if (key == 0u)
        return 0u;
    page = guest_fn_a1a90(sid, key);
    if (page == 0u)
        return 0u;
    entries = page + 2u;
    count = (int16_t)EF_U16(page);
    dx = (int32_t)(xb - xa);
    dy = (int32_t)(yb - ya);
    switch ((uint16_t)guest_fn_a1858_x(sid, yb)) {
    case 1:
        x = -dy;
        y = dx;
        break;
    case 2:
        x = -dx;
        y = -dy;
        break;
    case 3:
        x = dy;
        y = -dx;
        break;
    default:
        x = dx;
        y = dy;
        break;
    }
    e = find_entry_at_point_cfa24((uint32_t)count, entries, x, y);
    if (e == 0u)
        return 0u;
    num = ((int32_t)EF_U32(e + 0x28u) >> 16) * x + ((int32_t)EF_U32(e + 0x2cu) >> 16) * y +
          (((int32_t)EF_U32(e + 0x2eu) >> 16) << 15);
    den = (int32_t)EF_U32(e + 0x2au) >> 16;
    return abs_a28f2(num / den);
}

/* 0x899d8, 231 bytes. Let the unit record R (EAX) lock on to its target T (+0x30) when T is valid (0x8c09c) and has neither bit 2 nor bit 3 of +0x24 set: fetch T's vectors (0x8fa58) and R's (0x88d4c), build the direction 0x88fb4 against 0x1fcf58 into 0x1fcfd8, subtract, normalise (0xb4424) and, when the distance 0x88ff8 is at least the class limit (double at class*0x91+0x12bcd5 times 0x18a5b0 times 0x18a5b8), clear the word R+0x56 to 0xff, set byte R+0x5a and call 0x8be54(T, R). */
void lock_on_target_899d8(ef_addr32 rec)
{
    ef_addr32 t = EF_U32(rec + 0x30u);
    double v00[3];
    double v50[1];
    double v48[1];
    double v30[3];
    double v18[3];
    double limit;
    double c1;
    double c2;
    if ((uint8_t)guest_fn_8c09c_x(t) == 0u)
        return;
    if ((EF_U8(t + 0x24u) & 4u) != 0u || (EF_U8(t + 0x24u) & 8u) != 0u)
        return;
    guest_fn_8fa58_x(t, (ef_addr32)(uintptr_t)v00, (ef_addr32)(uintptr_t)v50);
    guest_fn_88d4c_x(rec, (ef_addr32)(uintptr_t)v48);
    direction_vector_88fb4(0x001fcfd8u, 0x001fcf58u, (ef_addr32)(uintptr_t)v00, rec, v50[0], v48[0]);
    subtract_scene_vectors((ef_addr32)(uintptr_t)v30, 0x001fcf58u, (ef_addr32)(uintptr_t)v00);
    if (normalize_vector_b4424((ef_addr32)(uintptr_t)v18, (ef_addr32)(uintptr_t)v30) == 0u)
        return;
    memcpy(&limit, ef_guest_memory_ptr((uint32_t)EF_U8(rec + 0x18u) * 0x91u + 0x0012bcd5u), sizeof limit);
    memcpy(&c1, ef_guest_memory_ptr(0x0018a5b0u), sizeof c1);
    memcpy(&c2, ef_guest_memory_ptr(0x0018a5b8u), sizeof c2);
    limit = limit * c1 * c2;
    if (guest_fn_88ff8_x(0x001fcfd8u, (ef_addr32)(uintptr_t)v18) < limit)
        return;
    EF_U16(rec + 0x56u) = 0xffu;
    EF_U8(rec + 0x5au) = 1u;
    guest_fn_8be54_x(t, rec);
}

/* 0xfecc, 231 bytes. Load a resource of group records: NAME (EBX) is checked with the size hook 0x132ab4 and opened (0x9ccb8, kept in 0x197ab0), reporting through the error hook 0x132a74 when either fails. After the setup calls 0xe7dc, 0xe810 and 0xe89c the record count from 0xe95c is stored to *COUNTP (EAX); each of the records is allocated with the hook at 0x121ebc (initially 0xe764, 0x24 bytes), initialised with 0xfc54 and filled with 0xbc570(record, ARG = EDX, local state), or reported with 0xcd12e when the allocation failed. The handle is closed with 0x9ce84; returns 1. */
uint32_t load_group_records_fecc(ef_addr32 countp, uint32_t arg, ef_addr32 name)
{
    uint32_t state = 0u;
    int32_t i;
    EF_U32(0x00121ebcu) = 0xe764u;
    EF_U32(0x00121eacu) = 0u;
    if (guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), name) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00181174u, name);
    EF_U32(0x00197ab0u) = guest_fn_9ccb8(name);
    if (EF_U32(0x00197ab0u) == 0xffffffffu)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x001811a0u, name);
    guest_fn_e7dc_x();
    guest_fn_e810();
    guest_fn_e89c();
    EF_U32(countp) = guest_fn_e95c();
    for (i = 0; i < (int32_t)EF_U32(countp); i++) {
        ef_addr32 p = guest_callptr_eax_ret_x(EF_U32(0x00121ebcu), 0x24u);
        if (p == 0u)
            guest_callptr3_x(EF_U32(0x00132a74u), 0x001811ccu, 0x24u, (uint32_t)i);
        read_group_header_fc54(p);
        if (p != 0u)
            guest_fn_bc570_x(p, arg, (ef_addr32)(uintptr_t)&state);
        else
            guest_fn_cd12e(0x001811f8u, 0u, 0u);
    }
    guest_fn_9ce84(EF_U32(0x00197ab0u));
    return 1u;
}

/* 0xc99ac, 235 bytes. Fill a rectangle in colour COLOUR (EAX, byte) given in viewport units: X0 (EDX), Y0 (EBX), X1 (ECX) and Y1 (stack, ret 4) as signed words, scaled by the viewport scale (double at +0x9f of the record at [0x41eea0]; nothing when NULL, X0 == X1 or Y0 == Y1) with the same offsets as 0xc95ac, rounded by 0x9d72c and filled with the clipped fill 0xcec00. */
void fill_scaled_rect_c99ac(uint32_t colour, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    ef_addr32 vp = EF_U32(0x0041eea0u);
    double scale;
    double xoff;
    double yoff;
    int32_t rx0;
    int32_t ry0;
    int32_t rx1;
    int32_t ry1;
    if (vp == 0u)
        return;
    if ((int16_t)x1 == (int16_t)x0 || (int16_t)y0 == (int16_t)y1)
        return;
    memcpy(&scale, ef_guest_memory_ptr(vp + 0x9fu), sizeof scale);
    xoff = (double)(int16_t)(EF_U16(vp + 0x41u) + EF_U16(vp + 0x71u) - EF_U16(vp + 0x69u));
    yoff = (double)(int16_t)(EF_U16(vp + 0x43u) + EF_U16(vp + 0x73u) - EF_U16(vp + 0x6bu));
    ry1 = (int32_t)truncate_9d72c((double)(int16_t)y1 * scale + yoff);
    rx1 = (int32_t)truncate_9d72c((double)(int16_t)x1 * scale + xoff);
    ry0 = (int32_t)truncate_9d72c((double)(int16_t)y0 * scale + yoff);
    rx0 = (int32_t)truncate_9d72c((double)(int16_t)x0 * scale + xoff);
    fill_rect_clipped_cec00(colour & 0xffu, rx0, ry0, rx1, ry1);
}

/* 0x293b0, 236 bytes. Choose which of the 19 three-byte slots (+0x198 of the unit UNIT, AX) to fire at the range D (double 0x1f7000): a slot qualifies when its type byte (+0x19b, class record of 0x91 bytes at 0x12bc94) has class byte 1, a nonzero count word (+0x19c), a maximum range (dword +0x12bca3 times the double 0x18246c) above D and a minimum range (word +0x12bca1 times the same factor) below D; the qualifying slot with the largest dword wins and its index is returned. Without a winner return 0x13 when D is positive, the double 0x1f70a0 is below the double 0x182464 and the square of the double 0x1f7018 is below 20 squared, else 0x14. */
uint32_t pick_weapon_slot_293b0(uint32_t unit)
{
    ef_addr32 obj = guest_fn_7489c((int32_t)(int16_t)unit);
    uint32_t best = 0xffu;
    int32_t bestv = 0;
    uint32_t i;
    double factor;
    double range;
    double a;
    double b;
    double c;
    memcpy(&factor, ef_guest_memory_ptr(0x0018246cu), sizeof factor);
    for (i = 0u; i < 0x13u; i++) {
        ef_addr32 slot = obj + i * 3u;
        uint32_t k = (uint32_t)((int32_t)EF_U32(slot + 0x198u) >> 24) * 0x91u;
        int32_t maxr;
        memcpy(&range, ef_guest_memory_ptr(0x001f7000u), sizeof range);
        if (EF_U8(k + 0x0012bc94u) != 1u || EF_U16(slot + 0x19cu) == 0u)
            continue;
        maxr = (int32_t)EF_U32(k + 0x0012bca3u);
        if (!((double)maxr * factor > range))
            continue;
        if (!((double)(int16_t)EF_U16(k + 0x0012bca1u) * factor < range))
            continue;
        if (bestv < maxr) {
            bestv = maxr;
            best = i;
        }
    }
    if (best != 0xffu)
        return best;
    memcpy(&range, ef_guest_memory_ptr(0x001f7000u), sizeof range);
    memcpy(&a, ef_guest_memory_ptr(0x001f70a0u), sizeof a);
    memcpy(&b, ef_guest_memory_ptr(0x00182464u), sizeof b);
    if (!(0.0 < range) || !(a < b))
        return 0x14u;
    memcpy(&c, ef_guest_memory_ptr(0x001f7018u), sizeof c);
    return square_double_b3fd0(20.0) > square_double_b3fd0(c) ? 0x13u : 0x14u;
}

/* 0x2dee4, 239 bytes. Is the record ID (AX; 0xca byte table at [0x19bb58]) clear of its neighbours? ID 0xff gives 1 and a record whose state byte +0x1a is 2 gives 0. Otherwise it fails (0) when its position (+0x61) is within double 0x1252a4 times double 0x1825fd (0xb415c distance) of the point at 0x1a3020 and, with byte 0x127da9 set, of any of the objects whose id words (high word at 0x1a5c2e + 0x1b * i, byte count 0x12583a) resolve with 0x7489c; an id word of 0xff ends the scan with 1. */
uint32_t position_clear_2dee4(uint32_t id)
{
    int32_t sid = (int16_t)id;
    ef_addr32 rec;
    uint8_t ok = 1u;
    double limit;
    double k;
    int16_t i;
    if (sid == 0xff)
        return 1u;
    rec = EF_U32(0x0019bb58u) + (uint32_t)sid * 0xcau;
    if (EF_U8(rec + 0x1au) == 2u)
        return 0u;
    memcpy(&limit, ef_guest_memory_ptr(0x001252a4u), sizeof limit);
    memcpy(&k, ef_guest_memory_ptr(0x001825fdu), sizeof k);
    limit *= k;
    if (limit > distance_2d_b415c(0x001a3020u, rec + 0x61u))
        ok = 0u;
    if (EF_U8(0x00127da9u) == 0u)
        return ok;
    for (i = 0; i < (int32_t)EF_U8(0x0012583au); i++) {
        int32_t oid = (int32_t)EF_U32((uint32_t)(int32_t)i * 0x1bu + 0x001a5c2eu) >> 16;
        ef_addr32 obj;
        if (oid == 0xff)
            return 1u;
        obj = guest_fn_7489c(oid);
        if (obj != 0u) {
            ef_addr32 r2 = EF_U32(0x0019bb58u) + (uint32_t)sid * 0xcau;
            if (limit > distance_2d_b415c(r2 + 0x61u, obj + 0x49u))
                ok = 0u;
        }
    }
    return ok;
}

/* 0x65d24, 235 bytes. Advance the target cursor 0x1a820c to the next visible candidate for the selected object (high word of 0x1f71c8, looked up with 0x7489c; when it is missing the cursor is cleared). Step through the ring (0x907f0, wrapping at most once more) until a node passes: 0x2e910(side byte of the owner record, node byte +0x3e) is true, bit 2 of node+0x30 is clear, both |dx| and |dz| to the object are below 0x486c00 and the line of sight request 0x66bf4 is false. If none passes the previous cursor is restored. */
void next_target_65d24(void)
{
    ef_addr32 obj = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    ef_addr32 saved;
    uint8_t wraps = 0u;
    uint8_t found = 0u;
    if (obj == 0u) {
        EF_U32(0x001a820cu) = 0u;
        return;
    }
    saved = EF_U32(0x001a820cu);
    do {
        ef_addr32 cur = guest_fn_907f0_x(EF_U32(0x001a820cu));
        EF_U32(0x001a820cu) = cur;
        if (cur == 0u) {
            cur = guest_fn_907f0_x(0u);
            EF_U32(0x001a820cu) = cur;
            wraps++;
        }
        cur = EF_U32(0x001a820cu);
        if (cur != 0u) {
            uint32_t side = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)EF_U32(obj + 0x26u) >> 16) * 0xcau + 0x15u);
            if ((uint8_t)guest_fn_2e910_x(side, EF_U8(cur + 0x3eu)) != 0u && (EF_U8(cur + 0x30u) & 2u) == 0u &&
                (int32_t)abs_a28f2((int32_t)(EF_U32(cur) - EF_U32(obj))) < 0x486c00 &&
                (int32_t)abs_a28f2((int32_t)(EF_U32(cur + 8u) - EF_U32(obj + 8u))) < 0x486c00 &&
                request_los_66bf4(cur) == 0u)
                found = 1u;
        }
    } while (found == 0u && wraps < 2u);
    if (found == 0u)
        EF_U32(0x001a820cu) = saved;
}

/* 0x597d8, 237 bytes. Refresh the weapon readout for the selected object (high word of 0x1f71c8, 0x7489c; nothing if missing): take its current slot (signed byte at +0x1d4 >> 24, 0x14 means none), the slot's type byte (signed word, 0 for none) and the word at the high half of the class dword at class*0x91 + 0x12bc9f (stored to 0x197240), store the rounded value of 0x8a6d4 times the double 0x185e80 in 0x197234, and then: byte 0x12814c == 6 only runs 0x58d04; otherwise type 4 or 0x12 runs 0x58d24 (mode 1), type 5 runs 0x58d04 (mode 2) and anything else 0x58d44 (mode 0), recording the mode in byte 0x1a6d1c. */
void refresh_weapon_readout_597d8(void)
{
    ef_addr32 obj = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    int32_t slot;
    int16_t type;
    int32_t cls;
    double v;
    double k;
    if (obj == 0u)
        return;
    slot = (int32_t)EF_U32(obj + 0x1d4u) >> 24;
    if (slot != 0x14)
        type = (int16_t)(int8_t)EF_U8(obj + (uint32_t)slot * 3u + 0x19bu);
    else
        type = 0;
    slot = (int32_t)EF_U32(obj + 0x1d4u) >> 24;
    cls = (int32_t)EF_U32(obj + (uint32_t)slot * 3u + 0x198u) >> 24;
    EF_U32(0x00197240u) = (uint32_t)((int32_t)EF_U32((uint32_t)cls * 0x91u + 0x0012bc9fu) >> 16);
    v = guest_fn_8a6d4_x();
    memcpy(&k, ef_guest_memory_ptr(0x00185e80u), sizeof k);
    EF_U32(0x00197234u) = (uint32_t)(int32_t)truncate_9d72c(v * k);
    if (EF_U8(0x0012814cu) == 6u) {
        guest_fn_58d04_x();
        return;
    }
    if ((uint16_t)type == 4u || (uint16_t)type == 0x12u) {
        guest_fn_58d24_x();
        EF_U8(0x001a6d1cu) = 1u;
    } else if ((uint16_t)type == 5u) {
        guest_fn_58d04_x();
        EF_U8(0x001a6d1cu) = 2u;
    } else {
        guest_fn_58d44_x();
        EF_U8(0x001a6d1cu) = 0u;
    }
}

/* 0x73c38, 228 bytes. Round-robin hail: for the unit UNIT (AX) advance its cursor byte +0x142 (wrapping to 0 at the signed word 0x2119f4) and look up the unit it names; when that unit exists, 0x2e910(its side, own side) holds (sides are the bytes at +0x15 of the owner records, owner word at +0x26 of each) and the distance 0x6a5f0(UNIT, cursor) is below byte(+0x140) * 0x9c40, send message 0x72 with 0x46254(UNIT, cursor, 0x72, byte(+0x140), 0, 0). */
void hail_next_unit_73c38(uint32_t unit)
{
    int32_t sid = (int16_t)unit;
    ef_addr32 obj = guest_fn_7489c(sid);
    ef_addr32 other;
    uint32_t tab;
    int16_t cursor;
    if (obj == 0u)
        return;
    EF_U8(obj + 0x142u) = (uint8_t)(EF_U8(obj + 0x142u) + 1u);
    cursor = (int16_t)EF_U8(obj + 0x142u);
    if (cursor >= (int16_t)EF_U16(0x002119f4u))
        EF_U8(obj + 0x142u) = 0u;
    other = guest_fn_7489c((int32_t)EF_U8(obj + 0x142u));
    if (other == 0u)
        return;
    tab = EF_U32(0x0019bb58u);
    if ((uint8_t)guest_fn_2e910_x(EF_U8(tab + (uint32_t)((int32_t)EF_U32(other + 0x26u) >> 16) * 0xcau + 0x15u),
                                  EF_U8(tab + (uint32_t)((int32_t)EF_U32(obj + 0x26u) >> 16) * 0xcau + 0x15u)) == 0u)
        return;
    if (!(guest_fn_6a5f0_x(sid, (int32_t)EF_U8(obj + 0x142u)) < (double)(int32_t)((uint32_t)EF_U8(obj + 0x140u) * 0x9c40u)))
        return;
    guest_fn_46254((uint32_t)sid, (int32_t)EF_U8(obj + 0x142u), 0x72u, EF_U8(obj + 0x140u), 0u, 0u);
}

/* 0x6f48, 229 bytes. Maybe fire a radio call from unit A (EAX) in answer to B (EDX): B's owner record is the one named by the high word of B+0x20 (nothing when missing); it must have the same side word (+0x15) as A. If the word 0x197264 differs from its word +0x24 or its byte +0x26 is set, B's word +0x22 is set to 0xff and nothing is sent. Otherwise, with probability 1/byte(0x197269), send message 0x3c with 0x46254(A's id, owner's id, 0x3c, angle from the distance 0x2bab0, the rounded value 0x2bae0 * double 0x18039c, the byte derived from the double at B+0x71). */
void maybe_radio_call_6f48(ef_addr32 a, ef_addr32 b)
{
    ef_addr32 owner = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)EF_U32(b + 0x20u) >> 16) * 0xcau;
    ef_addr32 pos;
    int32_t angle;
    int32_t hw;
    double v;
    double k;
    double b71;
    int32_t rounded;
    uint32_t ctl;
    if (owner == 0u)
        return;
    if (EF_U16(owner + 0x15u) != EF_U16(a + 0x15u))
        return;
    if (EF_U16(0x00197264u) != EF_U16(owner + 0x24u) || EF_U8(owner + 0x26u) != 0u) {
        EF_U16(b + 0x22u) = 0xffu;
        return;
    }
    hw = (int32_t)EF_U32(0x00197262u) >> 16;
    pos = owner + 0x61u;
    angle = (int16_t)scale_to_angle_45fc4(guest_fn_2bab0_x(pos, hw));
    memcpy(&k, ef_guest_memory_ptr(0x0018039cu), sizeof k);
    v = guest_fn_2bae0_x(pos, (int32_t)EF_U32(0x00197262u) >> 16) * k;
    rounded = (int32_t)truncate_9d72c(v);
    memcpy(&b71, ef_guest_memory_ptr(b + 0x71u), sizeof b71);
    ctl = scale_to_byte_4600c(b71);
    if ((guest_fn_9c960() & 0xffffu) % EF_U8(0x00197269u) != 0u)
        return;
    guest_fn_46254((uint32_t)((int32_t)EF_U32(a + 0x4eu) >> 16), (int32_t)EF_U32(owner + 0x4eu) >> 16, 0x3cu,
                   (uint32_t)angle, (uint32_t)rounded, ctl & 0xffu);
}

/* 0x7e670, 229 bytes. Run the unit setup passes for up to 0x4074 clock ticks (clock hook 0x15b620): after 0xe2988(0x1e), repeat { for every unit (count word 0x2119f4, 0x7489c) that is not flagged (bit 3 of +0x24), whose owner word (+0x16 high) is not 0xff and differs from byte 0x1a666b (an owner of 0xfe first becomes 0x1a666b), count it and run 0x545b8(unit, 1); then 0x581b0 and 0x58094 } while some unit was handled, word 0x126518 is zero and the deadline has not passed. Report the number of units still handled through the error hook (0x18866c) when the word 0x126518 is zero. Returns 1. */
uint32_t run_unit_setup_7e670(void)
{
    uint32_t deadline = ((uint32_t (*)(void))EF_U32(0x0015b620u))() + 0x4074u;
    uint32_t count;
    int16_t i;
    guest_fn_e2988(0x1eu);
    do {
        count = 0u;
        for (i = 0; i < (int16_t)EF_U16(0x002119f4u); i++) {
            ef_addr32 u = guest_fn_7489c((int32_t)i);
            if (u == 0u)
                continue;
            if (((int32_t)EF_U32(u + 0x16u) >> 16) == 0xfe)
                EF_U16(u + 0x18u) = (uint16_t)EF_U8(0x001a666bu);
            if ((uint16_t)EF_U8(0x001a666bu) == EF_U16(u + 0x18u))
                continue;
            if (((int32_t)EF_U32(u + 0x16u) >> 16) == 0xff)
                continue;
            if ((EF_U8(u + 0x24u) & 8u) != 0u)
                continue;
            count++;
            guest_fn_545b8_x((int32_t)i, 1u);
        }
        guest_fn_581b0();
        guest_fn_58094();
    } while ((uint16_t)count != 0u && EF_U16(0x00126518u) == 0u && ((uint32_t (*)(void))EF_U32(0x0015b620u))() < deadline);
    if ((uint16_t)count != 0u && EF_U16(0x00126518u) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018866cu, (uint32_t)(int32_t)(int16_t)count);
    return 1u;
}

/* 0x98198, 230 bytes. Pick the music/sound mode for the selected object (high word of 0x1f71c8, 0x7489c) and switch to it: only while byte 0x420a72 is set and byte 0x420a70 is 1. The mode follows the object's state byte +0x16c minus one (0 for unknown states, 1 for 1/2/4/5/16, 3 for 8/13, 2 when there is no object or for 3/7/11/12/14, and for 0/6/9/10/15 1 + ... 2 when byte 0x12dd0e or word 0x1a81ce is set, else 0, clearing byte 0x12dd0e). Unless the mode equals byte 0x12dd0d: when the pair from 0xd5fa8 is (0, multiple of 8) or byte 0x12dd0c is clear, reset the old slot (0x980e8), select the new (0x98050) and remember it in 0x12dd0d with 0x12dd0c set. */
void update_sound_mode_98198(void)
{
    static const uint8_t modes[17] = {4, 1, 1, 2, 1, 1, 4, 2, 3, 4, 4, 2, 2, 3, 2, 4, 1};
    ef_addr32 obj;
    uint32_t pair[2];
    uint8_t mode;
    if (EF_U8(0x00420a72u) == 0u || EF_U8(0x00420a70u) != 1u)
        return;
    obj = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    guest_fn_d5fa8_x((ef_addr32)(uintptr_t)&pair[1], (ef_addr32)(uintptr_t)&pair[0]);
    if (obj == 0u) {
        mode = 2u;
    } else {
        uint8_t k = (uint8_t)(EF_U8(obj + 0x16cu) - 1u);
        if (k > 0x10u) {
            mode = 0u;
        } else {
            switch (modes[k]) {
            case 1: mode = 1u; break;
            case 2: mode = 2u; break;
            case 3: mode = 3u; break;
            default:
                mode = (EF_U8(0x0012dd0eu) != 0u || EF_U16(0x001a81ceu) != 0u) ? 2u : 0u;
                EF_U8(0x0012dd0eu) = 0u;
                break;
            }
        }
    }
    if (mode == EF_U8(0x0012dd0du))
        return;
    if (!(pair[1] == 0u && pair[0] != 0u && (int32_t)pair[0] % 8 == 0) && EF_U8(0x0012dd0cu) != 0u)
        return;
    reset_sound_slot_980e8(EF_U8(0x0012dd0du));
    select_sound_slot_98050(mode);
    EF_U8(0x0012dd0du) = mode;
    EF_U8(0x0012dd0cu) = 1u;
}

/* 0xc0534, 240 bytes. Twin of 0xc02ec for the kinds 0xe and 0x16: for the first two indices J (0 and 1) of the list LIST (EDX, count by 0xbc5d0) try the two kinds in turn (starting with 0xe, the kind byte toggles on every attempt), find the node (0xcc51c) and fold it into the running value with 0xc0624 (EAX = running value, EDX = kind, EBX = J, ECX = LIST, stack: B, C, S0..S3). Returns the final running value (starts as AX = A). */
uint32_t fold_list_kinds_c0534(uint32_t a, uint32_t list, uint32_t b, uint32_t c, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    uint16_t cur = (uint16_t)a;
    uint8_t type = 0x16u;
    int16_t count = (int16_t)guest_fn_bc5d0_x(list);
    int16_t j;
    for (j = 0; j < count; j++) {
        int16_t k;
        if (j != 0 && j != 1)
            continue;
        for (k = 0; k < 2; k++) {
            uint32_t kind;
            type = type == 0xeu ? 0x16u : 0xeu;
            kind = type;
            if ((uint16_t)find_node_matching_cc51c((uint32_t)(int32_t)j, list, kind) != 0u)
                cur = (uint16_t)guest_fn_c0624_x((int32_t)(int16_t)cur, kind, (uint32_t)(uint8_t)j, list, b, c, s0, s1, s2, s3);
        }
    }
    return cur;
}

/* 0xe9cd8, 243 bytes. Build the 3x3 rotation matrix of three angles A1, A2, A3 (stack doubles, ret 0x18) into the nine doubles at OUT (EAX), row major: with ci = cos(Ai), si = sin(Ai) the entries are c1*c2, c1*s2*s3 - s1*c3, c1*s2*c3 + s1*s3, s1*c2, s1*s2*s3 + c1*c3, s1*s2*c3 - c1*s3, -s2, s3*c2, c2*c3. */
void build_rotation_matrix_e9cd8(ef_addr32 out, double a1, double a2, double a3)
{
    double c1 = cos_b45a2(a1);
    double s1 = sin_b45ac(a1);
    double c2 = cos_b45a2(a2);
    double s2 = sin_b45ac(a2);
    double c3 = cos_b45a2(a3);
    double s3 = sin_b45ac(a3);
    double m[9];
    m[0] = c1 * c2;
    m[1] = (c1 * s2) * s3 - s1 * c3;
    m[2] = (c1 * s2) * c3 + s1 * s3;
    m[3] = s1 * c2;
    m[4] = (s1 * s2) * s3 + c1 * c3;
    m[5] = (s1 * s2) * c3 - c1 * s3;
    m[6] = -s2;
    m[7] = s3 * c2;
    m[8] = c2 * c3;
    memcpy(ef_guest_memory_ptr(out), m, sizeof m);
}

/* 0x2b0b4, 259 bytes. Switch the record ID (AX; 0xca byte table at [0x19bb58]) to the state MODE (DL): run 0x2afcc(ID, MODE) and then by MODE (anything above 0xe does nothing): 0, 10 and 12 clear the dwords +0x71, +0x75, +0x79 and +0x7d; 2 runs 0x2b95c, clears byte +0xc9 and commands the linked units with 0x2b3b0(ID, 2); 3 does 0x2b3b0(ID, 3); 4 runs 0x2b95c; 6 sets byte +0xbb to 1 when the word +0xb9 is not 0xff and falls into the check shared with 5, 7 and 14, which sets byte 0x1f9011 when 0x2dba4(ID) holds. Finally the state byte +0x1a is set to MODE. */
void set_record_state_2b0b4(uint32_t id, uint32_t mode)
{
    int32_t sid = (int16_t)id;
    uint8_t m = (uint8_t)mode;
    ef_addr32 rec;
    guest_fn_2afcc_x(sid, m);
    rec = EF_U32(0x0019bb58u) + (uint32_t)sid * 0xcau;
    switch (m) {
    case 0: case 10: case 12:
        EF_U32(rec + 0x79u) = 0u;
        EF_U32(rec + 0x7du) = 0u;
        EF_U32(rec + 0x71u) = 0u;
        EF_U32(rec + 0x75u) = 0u;
        break;
    case 2:
        guest_fn_2b95c_x(sid);
        EF_U8(EF_U32(0x0019bb58u) + (uint32_t)sid * 0xcau + 0xc9u) = 0u;
        command_linked_units_2b3b0((int16_t)sid, 2u);
        break;
    case 3:
        command_linked_units_2b3b0((int16_t)sid, 3u);
        break;
    case 4:
        guest_fn_2b95c_x(sid);
        break;
    case 6:
        if (((int32_t)EF_U32(rec + 0xb7u) >> 16) != 0xff)
            EF_U8(rec + 0xbbu) = 1u;
        if ((uint8_t)guest_fn_2dba4(sid) != 0u)
            EF_U8(0x001f9011u) = 1u;
        break;
    case 5: case 7: case 14:
        if ((uint8_t)guest_fn_2dba4(sid) != 0u)
            EF_U8(0x001f9011u) = 1u;
        break;
    default:
        break;
    }
    EF_U8(EF_U32(0x0019bb58u) + (uint32_t)sid * 0xcau + 0x1au) = m;
}

/* 0x711f0, 248 bytes. Altitude check of the player ship (record at [0x1f71bc]; altitude double at +0x59): below the double 0x1868d1 the ship is dropped from its roster slot (0x740e4(.., 0xff, 0xff, 0, 0xff) and 0x6a1d0(.., 1) for the id in the high word of 0x1f71c6). The double 0x1f6f68 is set to (double 0x1868d9 - altitude) * double 0x1868e1; below the double 0x1284fc the byte +0x18d is cleared. When bit 6 of +0x24 is set and the altitude is below 0x1284fc - 0x12847c, send 0x46168(id, 0x5b, 0, 0, 0) and clear that bit. Always finish by zeroing the controls (dwords 0x1f7100/04/10/14 and 0x1f70b0/b4) and running 0x6eaf4(1.3) and 0x6e8d8(2.5). */
void check_player_altitude_711f0(void)
{
    ef_addr32 cur = EF_U32(0x001f71bcu);
    double alt;
    double lim;
    double a;
    double b;
    double c;
    int32_t id = (int32_t)EF_U32(0x001f71c6u) >> 16;
    memcpy(&alt, ef_guest_memory_ptr(cur + 0x59u), sizeof alt);
    memcpy(&lim, ef_guest_memory_ptr(0x001868d1u), sizeof lim);
    if (alt < lim) {
        guest_fn_740e4(id, 0xffu, 0xffu, 0u, 0xffu);
        guest_fn_6a1d0((int32_t)EF_U32(0x001f71c6u) >> 16, 1u);
    }
    cur = EF_U32(0x001f71bcu);
    memcpy(&a, ef_guest_memory_ptr(0x001868d9u), sizeof a);
    memcpy(&b, ef_guest_memory_ptr(0x001868e1u), sizeof b);
    memcpy(&alt, ef_guest_memory_ptr(cur + 0x59u), sizeof alt);
    c = (a - alt) * b;
    memcpy(ef_guest_memory_ptr(0x001f6f68u), &c, sizeof c);
    memcpy(&lim, ef_guest_memory_ptr(0x001284fcu), sizeof lim);
    memcpy(&alt, ef_guest_memory_ptr(cur + 0x59u), sizeof alt);
    if (alt < lim)
        EF_U8(cur + 0x18du) = 0u;
    cur = EF_U32(0x001f71bcu);
    if ((EF_U8(cur + 0x24u) & 0x40u) != 0u) {
        double l2;
        memcpy(&lim, ef_guest_memory_ptr(0x001284fcu), sizeof lim);
        memcpy(&l2, ef_guest_memory_ptr(0x0012847cu), sizeof l2);
        memcpy(&alt, ef_guest_memory_ptr(cur + 0x59u), sizeof alt);
        if (lim - l2 > alt) {
            draw_object_entries_46168((uint32_t)((int32_t)EF_U32(0x001f71c6u) >> 16), 0x5bu, 0u, 0u, 0u);
            cur = EF_U32(0x001f71bcu);
            EF_U8(cur + 0x24u) = (uint8_t)(EF_U8(cur + 0x24u) & 0xbfu);
        }
    }
    EF_U32(0x001f7100u) = 0u;
    EF_U32(0x001f7104u) = 0u;
    EF_U32(0x001f7110u) = 0u;
    EF_U32(0x001f7114u) = 0u;
    EF_U32(0x001f70b0u) = 0u;
    EF_U32(0x001f70b4u) = 0u;
    compute_turn_rate_6eaf4(0u, 0u, 0u, 0u, 1.3);
    guest_fn_6e8d8_x(0u, 0u, 0u, 0u, 2.5);
}

/* 0x65c1c, 249 bytes. Pick the nearest unit of kind 3 (0x6acc) to the viewpoint (dwords 0x1a5bec, 0x1a5bf0, 0x1a5be8 divided by 256) among the ring of units walked with 0x663a4(.., 1), starting from the first, and store its id in word 0x1a8242 (0xffff when there is none or the selected object, high word of 0x1f71c8, is missing). The squared distance uses the signed high words of the unit's first three dwords. */
void pick_nearest_unit_65c1c(void)
{
    ef_addr32 sel = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    int32_t first = -1;
    int32_t best = -1;
    int32_t bestd = 0x7fffffff;
    int32_t id;
    EF_U16(0x001a8242u) = 0xffffu;
    if (sel == 0u)
        return;
    id = (int16_t)find_next_unit_663a4((uint32_t)-1, 1u);
    while (id != (int16_t)first) {
        ef_addr32 u;
        if (first == -1)
            first = id;
        u = guest_fn_7489c(id);
        if ((uint8_t)guest_fn_6acc_x(u) == 3u) {
            int32_t x = (int32_t)EF_U32(u) >> 16;
            int32_t y = (int32_t)EF_U32(u + 4u) >> 16;
            int32_t z = (int32_t)EF_U32(u + 8u) >> 16;
            int32_t dy = y - ((int32_t)EF_U32(0x001a5bf0u) >> 8);
            int32_t dx = x - ((int32_t)EF_U32(0x001a5becu) >> 8);
            int32_t dz = z - ((int32_t)EF_U32(0x001a5be8u) >> 8);
            int32_t d = dx * dx + dy * dy + dz * dz;
            if (d <= bestd) {
                bestd = d;
                best = id;
            }
        }
        id = (int16_t)find_next_unit_663a4((uint32_t)id, 1u);
    }
    EF_U16(0x001a8242u) = (uint16_t)best;
}

/* 0x2c358, 251 bytes. Is any record near the current one flagged by 0x2c224? Only when byte 0x196fe8 is 1: take the current record P (high word of 0x19bb5c) and scan the records P - 4 .. P + 4 (clamped to 0 and the count at 0x19bb54) whose group word (+0x1c) equals P's, then P itself unless it is 0xff, and with byte 0x127da9 set the owner records (+0x26 high word) of the objects listed at 0x1a5c2e + 0x1b * i (count byte 0x12583a); each is passed to 0x2c224 and a true result makes the function return 1. */
uint32_t any_nearby_flagged_2c358(void)
{
    uint8_t found = 0u;
    int32_t p;
    int32_t start;
    int32_t end;
    int32_t i;
    uint32_t group;
    uint8_t n;
    if (EF_U8(0x00196fe8u) != 1u)
        return 0u;
    p = (int32_t)EF_U32(0x0019bb5cu) >> 16;
    group = EF_U16(EF_U32(0x0019bb58u) + (uint32_t)p * 0xcau + 0x1cu);
    start = p - 4 > 0 ? p - 4 : 0;
    end = p + 4 < (int32_t)EF_U32(0x0019bb54u) ? p + 4 : (int32_t)EF_U32(0x0019bb54u);
    for (i = start; (int16_t)i <= (int16_t)end && (int16_t)start <= (int16_t)end; i++) {
        if (EF_U16(EF_U32(0x0019bb58u) + (uint32_t)(int32_t)(int16_t)i * 0xcau + 0x1cu) == (uint16_t)group &&
            (uint8_t)guest_fn_2c224_x((int32_t)(int16_t)i) != 0u)
            found = 1u;
    }
    p = (int32_t)EF_U32(0x0019bb5cu) >> 16;
    if (p != 0xff && (uint8_t)guest_fn_2c224_x(p) != 0u)
        found = 1u;
    if (EF_U8(0x00127da9u) != 0u) {
        for (n = 0u; n < EF_U8(0x0012583au); n++) {
            ef_addr32 u = guest_fn_7489c((int32_t)EF_U32((uint32_t)n * 0x1bu + 0x001a5c2eu) >> 16);
            if (u != 0u && (uint8_t)guest_fn_2c224_x((int32_t)EF_U32(u + 0x26u) >> 16) != 0u)
                found = 1u;
        }
    }
    return found;
}

/* 0xbc0dc, 247 bytes. Pick a response code from the 6 x 7 byte table at 0xb9e20: the column follows the difference D of the rating bytes 0x134012[A] - 0x134012[B] (A = EAX, B = DL; bands: <= -12, <= -7, <= -3, below 3, below 7, below 12, else 0..6) and the row is a random number (0xa45b3, 16 bit) modulo 6. */
uint32_t pick_response_bc0dc(uint32_t a, uint32_t b)
{
    uint8_t table[42];
    int16_t d = (int16_t)((uint32_t)EF_U8((a & 0xffu) + 0x00134012u) - (uint32_t)EF_U8((b & 0xffu) + 0x00134012u));
    uint32_t band;
    uint32_t row;
    memcpy(table, ef_guest_memory_ptr(0x000b9e20u), sizeof table);
    if (d <= -12)
        band = 0u;
    else if (d <= -7)
        band = 1u;
    else if (d <= -3)
        band = 2u;
    else if (d < 3)
        band = 3u;
    else if (d < 7)
        band = 4u;
    else if (d < 12)
        band = 5u;
    else
        band = 6u;
    row = (guest_fn_a45b3() & 0xffffu) % 6u;
    return table[row * 7u + band];
}

/* 0x7eda4, 246 bytes. Draw the roster list centred on the 320x200 screen (after 0xdc2c0, before 0xdc250): for each of the byte 0x12583a entries (names at 0x1368bc, flag byte at 0x1a5c32 + 0x1b * i) copy and upper-case the name, format it with 0x1886d0 (colour 0xf, flag set) or 0x1886dc (colour 0xc), and draw it with 0x48c90 centred horizontally at row 8 * (i + 1) below the vertical start (200 - 8 * count) / 2. */
void draw_roster_list_7eda4(void)
{
    uint8_t frame[0x3c];
    ef_addr32 text = (ef_addr32)(uintptr_t)frame;
    ef_addr32 name = (ef_addr32)(uintptr_t)(frame + 0x28);
    int32_t y = (0xc8 - (int32_t)(EF_U8(0x0012583au) << 3)) >> 1;
    int16_t i;
    guest_fn_dc2c0();
    for (i = 0; i < (int16_t)EF_U8(0x0012583au); i++) {
        uint32_t flag = EF_U8((uint32_t)(int32_t)i * 0x1bu + 0x001a5c32u);
        int32_t w;
        int32_t x;
        copy_string_9d749(name, EF_U32((uint32_t)(int32_t)i * 4u + 0x001368bcu));
        uppercase_string(name);
        guest_sprintf_a4342(text, flag != 0u ? 0x001886d0u : 0x001886dcu, name);
        y += 8;
        w = (int16_t)formatted_width_48d5c(text);
        x = (int16_t)((0x140 - w) >> 1);
        draw_shadowed_text_48c90(flag != 0u ? 0xfu : 0xcu, x, (int16_t)y, text);
    }
    guest_fn_dc250();
}

/* 0x538e4, 252 bytes. Integrate the floating drift state for one frame of DT = word 0x127d92 ticks: the positions 0x1a5c00, 0x1a5bb8 and 0x1a5bbc (floats) advance by the velocities 0x1a5ba0, 0x1a5ba4 and 0x1a5ba8 times DT, and every velocity decays by itself * DT * the float 0x185b6c. Then 0x1a5ba4 gains scale(10.0) * DT * (float 0x185b70)^2 and, when scale(5.0) * float 0x185b70 is below it, byte 0x1a364e is incremented and dword 0x1a5b5c cleared. */
void integrate_drift_538e4(void)
{
    float *v0 = (float *)ef_guest_memory_ptr(0x001a5ba0u);
    float *v1 = (float *)ef_guest_memory_ptr(0x001a5ba4u);
    float *v2 = (float *)ef_guest_memory_ptr(0x001a5ba8u);
    float *p0 = (float *)ef_guest_memory_ptr(0x001a5c00u);
    float *p1 = (float *)ef_guest_memory_ptr(0x001a5bb8u);
    float *p2 = (float *)ef_guest_memory_ptr(0x001a5bbcu);
    float k = *(float *)ef_guest_memory_ptr(0x00185b6cu);
    float k2 = *(float *)ef_guest_memory_ptr(0x00185b70u);
    double dt = (double)EF_U16(0x00127d92u);
    double r;
    *p0 = (float)((double)*v0 * dt + (double)*p0);
    *p1 = (float)((double)*v1 * dt + (double)*p1);
    *p2 = (float)((double)*v2 * dt + (double)*p2);
    *v0 = (float)((double)*v0 - (double)*v0 * (double)k * dt);
    *v1 = (float)((double)*v1 - (double)*v1 * (double)k * dt);
    *v2 = (float)((double)*v2 - dt * ((double)k * (double)*v2));
    r = scale_float_52eb0(10.0f);
    dt = (double)EF_U16(0x00127d92u);
    *v1 = (float)(r * dt * (double)k2 * (double)k2 + (double)*v1);
    r = scale_float_52eb0(5.0f);
    if (!(r * (double)k2 >= (double)*v1)) {
        EF_U8(0x001a364eu) = (uint8_t)(EF_U8(0x001a364eu) + 1u);
        EF_U32(0x001a5b5cu) = 0u;
    }
}

/* 0x88ff8, 103 bytes. Unsigned angle between the unit vectors A (EAX) and B (EDX): the dot product (0xb43dc) clamped to [-1, 1], its arccosine (0xe7df4), wrapped by 0xb42f0 and made positive by 0xb427c. */
double angle_between_88ff8(ef_addr32 a, ef_addr32 b)
{
    double d = dot_product_b43dc(a, b);
    if (!(1.0 >= d))
        d = 1.0;
    if (!(d >= -1.0))
        d = -1.0;
    return abs_double_b427c(wrap_angle_b42f0(guest_fn_e7df4_x(d)));
}

/* 0x89f50, 105 bytes. Project the aim vector: with the angles A = double 0x1fd020 and B = double 0x1fcfd0 and the range D = double 0x1fcf48, store D * cos(B) * cos(A) at 0x1fcff0, cos(A) * (sin(B) * D) at 0x1fcff8 and (-sin(A)) * D + double 0x1fcf10 * double 0x1fcca0 at 0x1fd000. */
void project_aim_vector_89f50(void)
{
    double a;
    double b;
    double d;
    double e;
    double f;
    double ca;
    double sa;
    double cb;
    double sb;
    double r;
    memcpy(&a, ef_guest_memory_ptr(0x001fd020u), sizeof a);
    ca = cos_b45a2(a);
    sa = sin_b45ac(a);
    memcpy(&b, ef_guest_memory_ptr(0x001fcfd0u), sizeof b);
    cb = cos_b45a2(b);
    sb = sin_b45ac(b);
    memcpy(&d, ef_guest_memory_ptr(0x001fcf48u), sizeof d);
    r = (cb * d) * ca;
    memcpy(ef_guest_memory_ptr(0x001fcff0u), &r, sizeof r);
    r = ca * (sb * d);
    memcpy(ef_guest_memory_ptr(0x001fcff8u), &r, sizeof r);
    memcpy(&e, ef_guest_memory_ptr(0x001fcf10u), sizeof e);
    memcpy(&f, ef_guest_memory_ptr(0x001fcca0u), sizeof f);
    r = d * (-sa) + e * f;
    memcpy(ef_guest_memory_ptr(0x001fd000u), &r, sizeof r);
}

/* 0x8bde8, 83 bytes. Sum of two scores for the same arguments A (EAX), B (EDX), C (EBX), D (ECX) and the signed word S (stack, ret 4): the byte result of 0x8ecdc(A, B, S) plus the result of 0x90438(A, B, C, D, S). */
uint32_t sum_scores_8bde8(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t s)
{
    uint32_t sw = (uint32_t)(int32_t)(int16_t)s;
    uint32_t r1 = guest_fn_8ecdc_x(a, b, sw) & 0xffu;
    return r1 + guest_fn_90438_x(a, b, c, d, sw);
}

/* 0x5c7dc, 258 bytes. Like 0x5a220 (apply 0xcdf48 then latch 0xce328 with the four signed high words at 0x1a6cf4/0x1a6cee/0x1a6cf6/0x1a6cf0; project the selected node [0x1a821c] with 0x5ca88 and update it with 0x5d520) but then draw the label: format 0x185fac with dword 0x1971b0 and print it with 0x48bb0 (colour byte 0x128148, mode (1 - shift) & 0xff), centred on the projected x (half the text width, 0x48da0) and placed above the projected y by (word 0x1a6ce2 + 7) << shift, shift being the byte 0x12b369. */
void refresh_map_label_5c7dc(void)
{
    uint32_t a = (uint32_t)((int32_t)EF_U32(0x001a6cf0u) >> 16);
    uint32_t b = (uint32_t)((int32_t)EF_U32(0x001a6cf6u) >> 16);
    uint32_t c = (uint32_t)((int32_t)EF_U32(0x001a6ceeu) >> 16);
    uint32_t d = (uint32_t)((int32_t)EF_U32(0x001a6cf4u) >> 16);
    ef_addr32 node;
    char text[16];
    int16_t pt[2];
    ef_addr32 textp = (ef_addr32)(uintptr_t)text;
    int32_t half;
    uint32_t shift;
    int32_t y;
    int32_t x;
    guest_fn_cdf48(a, b, c, d);
    latch_values_ce328(a, b, c, d);
    node = EF_U32(0x001a821cu);
    if (node == 0u)
        return;
    if ((uint8_t)guest_fn_5ca88_ret_x(node, (ef_addr32)(uintptr_t)pt) == 0u)
        return;
    guest_fn_5d520_x(pt[0], pt[1], node);
    guest_sprintf_a4342(textp, 0x00185facu, EF_U32(0x001971b0u));
    half = (int16_t)formatted_width_48da0(textp) >> 1;
    shift = EF_U8(0x0012b369u);
    y = (int16_t)((int32_t)pt[1] - (int32_t)((EF_U16(0x001a6ce2u) + 7u) << shift));
    x = (int16_t)((int32_t)pt[0] - half);
    draw_formatted_text_mode_48bb0(EF_U8(0x00128148u), (1u - shift) & 0xffu, x, y, textp);
}

/* 0x539e0, 265 bytes. Variant of 0x538e4 for the second drift mode: the tick counter 0x1a5b5c advances by DT (word 0x127d92), the positions and velocities integrate the same way with the decay float 0x185b74, 0x1a5ba4 gains scale(10.0) * DT * (float 0x185b78)^2 and when the counter exceeds 0x19 byte 0x1a364e is incremented and dword 0x1a5b38 set to 1. */
void integrate_drift_539e0(void)
{
    float *v0 = (float *)ef_guest_memory_ptr(0x001a5ba0u);
    float *v1 = (float *)ef_guest_memory_ptr(0x001a5ba4u);
    float *v2 = (float *)ef_guest_memory_ptr(0x001a5ba8u);
    float *p0 = (float *)ef_guest_memory_ptr(0x001a5c00u);
    float *p1 = (float *)ef_guest_memory_ptr(0x001a5bb8u);
    float *p2 = (float *)ef_guest_memory_ptr(0x001a5bbcu);
    float k = *(float *)ef_guest_memory_ptr(0x00185b74u);
    float k2 = *(float *)ef_guest_memory_ptr(0x00185b78u);
    uint32_t counter = EF_U32(0x001a5b5cu) + EF_U16(0x00127d92u);
    double dt = (double)EF_U16(0x00127d92u);
    double r;
    *p0 = (float)((double)*v0 * dt + (double)*p0);
    *p1 = (float)((double)*v1 * dt + (double)*p1);
    *p2 = (float)((double)*v2 * dt + (double)*p2);
    *v0 = (float)((double)*v0 - (double)*v0 * (double)k * dt);
    *v1 = (float)((double)*v1 - (double)*v1 * (double)k * dt);
    *v2 = (float)((double)*v2 - dt * ((double)k * (double)*v2));
    EF_U32(0x001a5b5cu) = counter;
    r = scale_float_52eb0(10.0f);
    dt = (double)EF_U16(0x00127d92u);
    *v1 = (float)((double)k2 * (r * dt * (double)k2) + (double)*v1);
    if ((int32_t)EF_U32(0x001a5b5cu) > 0x19) {
        EF_U8(0x001a364eu) = (uint8_t)(EF_U8(0x001a364eu) + 1u);
        EF_U32(0x001a5b38u) = 1u;
    }
}

/* 0x71fc, 266 bytes. Hand a task to the nearest record for object B (EDX) of unit A (EAX): search with 0x2c454 at B+0x61 (owner word from A's +0x13 high half, modes 0xd and 0xc), skip when none (0xff), its flag byte +0x1a is clear or its grid distance (0x2bb3c from the word 0x197262) exceeds the double 0x1803ac. Otherwise fill its task block (pointer at +0x8a): byte +0x18 = 1, words +0x10..+0x16 from 0x12583c, 0x12583e, 0x125840 + 3 and 0x125842, advance the clock at +0x10 by ((word 0x121e82 * 0x1e + 3) * 0x32) with 0x407c4, set the record's words +0x24 = word 0x197264 and byte +0x26 = 0, mark B (+0x22 = id, only when byte 0x127da9 is clear or byte 0x1a666b equals B's first byte) and finish with 0x2b634(id). */
void assign_task_71fc(ef_addr32 a, ef_addr32 b)
{
    int32_t id = (int16_t)guest_fn_2c454(b + 0x61u, (int32_t)EF_U32(a + 0x13u) >> 16, 0xdu, 0xcu);
    uint32_t idw = (uint32_t)id;
    ef_addr32 rec;
    ef_addr32 task;
    int16_t amount;
    if (id == 0xff)
        return;
    rec = EF_U32(0x0019bb58u) + idw * 0xcau;
    if (rec == 0u || EF_U8(rec + 0x1au) == 0u)
        return;
    {
        double lim;
        memcpy(&lim, ef_guest_memory_ptr(0x001803acu), sizeof lim);
        if (grid_cell_distance((int16_t)((int32_t)EF_U32(0x00197262u) >> 16), (int16_t)id) > lim)
            return;
    }
    task = EF_U32(rec + 0x8au);
    EF_U8(task + 0x18u) = 1u;
    EF_U16(EF_U32(rec + 0x8au) + 0x10u) = EF_U16(0x0012583cu);
    EF_U16(EF_U32(rec + 0x8au) + 0x12u) = EF_U16(0x0012583eu);
    EF_U16(EF_U32(rec + 0x8au) + 0x14u) = (uint16_t)(EF_U16(0x00125840u) + 3u);
    EF_U16(EF_U32(rec + 0x8au) + 0x16u) = EF_U16(0x00125842u);
    amount = (int16_t)((((int16_t)(EF_U16(0x00121e82u) * 0x1eu)) + 3) * 0x32);
    guest_fn_407c4_x(EF_U32(rec + 0x8au) + 0x10u, amount);
    EF_U8(rec + 0x26u) = 0u;
    EF_U16(rec + 0x24u) = EF_U16(0x00197264u);
    if (EF_U8(0x00127da9u) == 0u || EF_U8(0x001a666bu) == EF_U8(b))
        EF_U16(b + 0x22u) = (uint16_t)idw;
    guest_fn_2b634_x(id);
}

/* 0x843b0, 255 bytes. Per-frame wander of the view reference point. While either word at 0x1fcbd0/0x1fcbd2 is set, recentre (0x842b4) and refresh (0x84370); if the point at the high words of 0x1fcc3c/0x1fcc3e fails 0x834e8, do both again; either way restart the 0x28-tick timer at 0x12b4c0 from the clock 0x1fcbbc. Otherwise, once 0x28 ticks have passed, draw a new random word into 0x12b4bc; each frame move the point (0x1fcc2c, 0x1fcc34) by a random 0..0x1ff offset per axis (sign from bits 0 and 1 of that word) when 0x83294 accepts the new position, then recentre and refresh. Always returns 0. */
uint8_t wander_view_point_843b0(void)
{
    if (EF_U16(0x001fcbd0u) != 0u || EF_U16(0x001fcbd2u) != 0u) {
        center_view_842b4();
        update_ui_composite();
        if (guest_fn_834e8_w((int32_t)EF_U32(0x001fcc3cu) >> 16, (int32_t)EF_U32(0x001fcc3eu) >> 16) != 0u) {
            center_view_842b4();
            update_ui_composite();
        }
        EF_U32(0x0012b4c0u) = EF_U32(0x001fcbbcu) - 0x28u;
    } else {
        int32_t dx, dy;
        if ((int32_t)(EF_U32(0x001fcbbcu) - EF_U32(0x0012b4c0u)) >= 0x28) {
            EF_U32(0x0012b4c0u) = EF_U32(0x001fcbbcu);
            EF_U16(0x0012b4bcu) = (uint16_t)guest_fn_a45b3();
        }
        dx = (int32_t)(guest_fn_a45b3() & 0x1ffu);
        if ((EF_U16(0x0012b4bcu) & 0x1u) != 0u)
            dx = -dx;
        dy = (int32_t)(guest_fn_a45b3() & 0x1ffu);
        if ((EF_U16(0x0012b4bcu) & 0x2u) != 0u)
            dy = -dy;
        if (ui_point_near_reference((int32_t)EF_U32(0x001fcc2cu) + dx, (int32_t)EF_U32(0x001fcc34u) + dy) != 0u) {
            EF_U32(0x001fcc2cu) = EF_U32(0x001fcc2cu) + (uint32_t)dx;
            EF_U32(0x001fcc34u) = EF_U32(0x001fcc34u) + (uint32_t)dy;
        }
        center_view_842b4();
        update_ui_composite();
    }
    return 0u;
}

/* 0x9d834, 258 bytes. Initialise the C-runtime style globals: zero 0x132b08/0x132b0c and byte 0x132ac2, point 0x411080 at 0x18b22c, set -1 into 0x417160/0x132b18/words 0x132ac4, 0x132ad0, 0x132ad2, clear byte 0x132b1c and words 0x40f140/0x40f142, set word 0x132b10 = 1, and store the table pointers 0x9d770..0x9d780 into 0x132ad4..0x132af0. On the first call only (byte 0x132b12 clear) build the default path in 0x41719a, append 0x18b234 to it, run 0x9da80 and mark the byte. */
void init_runtime_globals_9d834(void)
{
    EF_U32(0x00132b08u) = 0u;
    EF_U32(0x00132b0cu) = 0u;
    EF_U32(0x00411080u) = 0x0018b22cu;
    EF_U8(0x00132ac2u) = 0u;
    EF_U32(0x00417160u) = 0xffffffffu;
    EF_U32(0x00132b18u) = 0xffffffffu;
    EF_U8(0x00132b1cu) = 0u;
    EF_U32(0x00132ad4u) = 0x0009d778u;
    EF_U32(0x00132ae0u) = 0x0009d77cu;
    EF_U32(0x00132aecu) = 0x0009d77cu;
    EF_U16(0x0040f140u) = 0u;
    EF_U16(0x0040f142u) = 0u;
    EF_U16(0x00132b10u) = 1u;
    EF_U16(0x00132ac4u) = 0xffffu;
    EF_U16(0x00132ad0u) = 0xffffu;
    EF_U16(0x00132ad2u) = 0xffffu;
    EF_U32(0x00132ad8u) = 0x0009d770u;
    EF_U32(0x00132af0u) = 0x0009d780u;
    EF_U32(0x00132adcu) = 0x0009d774u;
    EF_U32(0x00132ae4u) = 0x0009d77cu;
    EF_U32(0x00132ae8u) = 0x0009d77cu;
    if (EF_U8(0x00132b12u) == 0u) {
        build_default_path_9c980(0x0041719au);
        strcat_9ca57(0x0041719au, 0x0018b234u);
        guest_fn_9da80_x();
        EF_U8(0x00132b12u) = 1u;
    }
}

/* 0x7f6d8, 260 bytes. Squared distance from the camera (words 0x21444c, 0x214448, 0x214450) to the position at REC (three 8.8-ish coordinates, bits 8..23 of each dword); 0 gives 0. For the tracked record 0x12b0c8 in play mode (byte 0x127da9, record_mismatches_marker nonzero) the result is cached in 0x12b0c0 with the z delta in word 0x12b0c4: a cache hit with a z delta too far from the stored one (|abs(word 0x12b0c2 >> 16) - abs(dz)| > 0x1f4) returns the cached value, and byte 0x1a3647 forces the constant 0x40. A different record just becomes the tracked one. */
uint32_t camera_distance_sq_7f6d8(ef_addr32 rec)
{
    int32_t dx, dy, dz;
    int32_t sx, sy, sz;
    if (rec == 0u)
        return 0u;
    dx = (int32_t)((EF_U32(0x0021444cu) & 0xffffu) - ((EF_U32(rec) >> 8) & 0xffffu));
    dy = (int32_t)((EF_U32(0x00214448u) & 0xffffu) - ((EF_U32(rec + 4u) >> 8) & 0xffffu));
    dz = (int32_t)((EF_U32(0x00214450u) & 0xffffu) - ((EF_U32(rec + 8u) >> 8) & 0xffffu));
    sx = (int16_t)dx;
    sy = (int16_t)dy;
    sz = (int16_t)dz;
    if (EF_U8(0x00127da9u) != 0u && record_mismatches_marker(rec) != 0u) {
        if (rec == EF_U32(0x0012b0c8u)) {
            if (EF_U8(0x001a3647u) != 0u)
                return 0x40u;
            {
                int32_t diff = (int32_t)abs_a28f2((int32_t)EF_U32(0x0012b0c2u) >> 16) - (int32_t)abs_a28f2(sz);
                if ((int32_t)abs_a28f2(diff) > 0x1f4)
                    return EF_U32(0x0012b0c0u);
            }
            EF_U16(0x0012b0c4u) = (uint16_t)dz;
            EF_U32(0x0012b0c0u) = (uint32_t)(sz * sz + sx * sx + sy * sy);
        } else {
            EF_U32(0x0012b0c8u) = rec;
        }
    }
    return (uint32_t)(sx * sx + sy * sy + sz * sz);
}

/* 0x3c5b8, 261 bytes. Parse a table from the text reader: the first line (read_line_3c558 into 0x1a2df0, format 0x18365c) gives the entry count stored as a word at the table 0x1a2e78; then per entry read a line for a 4-byte item (format 0x183660) stored at table + 2 + i * 0x49, and a second line (format 0x1264a8, ten values) filling the word at +0x3c and the bytes +0x40..+0x48. */
void load_table_3c5b8(void)
{
    uint32_t v[10];
    uint32_t count = 0;
    int32_t i;
    read_line_3c558();
    guest_fn_df26f_x(0x001a2df0u, 0x0018365cu, (ef_addr32)(uintptr_t)&count);
    EF_U16(EF_U32(0x001a2e78u)) = (uint16_t)count;
    for (i = 0; (int32_t)(int16_t)EF_U16(EF_U32(0x001a2e78u)) > i; i++) {
        ef_addr32 e = EF_U32(0x001a2e78u) + 2u + (uint32_t)i * 0x49u;
        read_line_3c558();
        guest_fn_df26f_x(0x001a2df0u, 0x00183660u, e);
        read_line_3c558();
        guest_fn_df26f_x(0x001a2df0u, 0x001264a8u,
                         (ef_addr32)(uintptr_t)&v[9], (ef_addr32)(uintptr_t)&v[8],
                         (ef_addr32)(uintptr_t)&v[7], (ef_addr32)(uintptr_t)&v[6],
                         (ef_addr32)(uintptr_t)&v[5], (ef_addr32)(uintptr_t)&v[4],
                         (ef_addr32)(uintptr_t)&v[3], (ef_addr32)(uintptr_t)&v[2],
                         (ef_addr32)(uintptr_t)&v[1], (ef_addr32)(uintptr_t)&v[0]);
        EF_U16(e + 0x3cu) = (uint16_t)v[9];
        EF_U8(e + 0x40u) = (uint8_t)v[8];
        EF_U8(e + 0x41u) = (uint8_t)v[7];
        EF_U8(e + 0x42u) = (uint8_t)v[6];
        EF_U8(e + 0x43u) = (uint8_t)v[5];
        EF_U8(e + 0x44u) = (uint8_t)v[4];
        EF_U8(e + 0x45u) = (uint8_t)v[3];
        EF_U8(e + 0x46u) = (uint8_t)v[2];
        EF_U8(e + 0x47u) = (uint8_t)v[1];
        EF_U8(e + 0x48u) = (uint8_t)v[0];
    }
}

/* 0xf3378, 263 bytes. Draw the outlined polygons of the overlay list: when word 0x1616e8 and byte 0x4859dc are both set run 0xf3214 first; then, between 0xc3974 and 0xc39ac, for each of the word 0x1616e6 entries (0x16 bytes each at 0x4853fe) draw four edges between the corner words at +0 (A), +2 (B), +4 (C) and +6 (D), each >> 16, with 0xc8ccc in the colour from the high half of 0x161e1a */
void draw_overlay_polys_f3378(void)
{
    int16_t i;
    int16_t n;
    if (EF_U16(0x001616e8u) != 0u && EF_U8(0x004859dcu) != 0u)
        guest_fn_f3214_x();
    guest_fn_c3974();
    n = (int16_t)EF_U16(0x001616e6u);
    for (i = 0; n > 0 && i < (int16_t)EF_U16(0x001616e6u); i++) {
        ef_addr32 p = 0x004853feu + (uint32_t)((int32_t)i * 0x16);
        uint32_t col = (uint32_t)((int32_t)EF_U32(0x00161e1au) >> 16);
        uint32_t a = (uint32_t)((int32_t)EF_U32(p) >> 16);
        uint32_t b = (uint32_t)((int32_t)EF_U32(p + 2u) >> 16);
        uint32_t c = (uint32_t)((int32_t)EF_U32(p + 4u) >> 16);
        uint32_t d = (uint32_t)((int32_t)EF_U32(p + 6u) >> 16);
        guest_fn_c8ccc_x(b, c, b, a, col);
        col = (uint32_t)((int32_t)EF_U32(0x00161e1au) >> 16);
        guest_fn_c8ccc_x(d, c, b, c, col);
        col = (uint32_t)((int32_t)EF_U32(0x00161e1au) >> 16);
        guest_fn_c8ccc_x(d, a, d, c, col);
        col = (uint32_t)((int32_t)EF_U32(0x00161e1au) >> 16);
        guest_fn_c8ccc_x(b, a, d, a, col);
    }
    guest_fn_c39ac();
}

/* 0x45a54, 265 bytes. Hit-test the point (x = EAX, y = EDX, both 16-bit) against the marker table of the current map record (pointer 0x1a4a9c, count word +0x1c, entries of 0x19 bytes at the pointer +0x20: x word +0, y word +4 measured from 0x190, kind byte +0x18). Each entry owns a square of half-size 8 / (high word of 0x19769c); returns the first hit's index, or -1 when none, when byte 0x121e8c or 0x126576 is set, or when the kind byte of the last entry examined is 1, 2, 5 or 8. */
int32_t hit_test_markers_45a54(uint32_t x, uint32_t y)
{
    int32_t px = (int16_t)x;
    int32_t py = (int16_t)y;
    int32_t result = -1;
    uint8_t kind = 0u; /* ORIGINAL BUG: uninitialised stack byte when the table is not scanned */
    if (EF_U8(0x00121e8cu) == 0u && EF_U8(0x00126576u) == 0u) {
        ef_addr32 map = EF_U32(0x001a4a9cu);
        EF_U16(0x001a4b9cu) = EF_U16(map + 0x1cu);
        if (EF_U32(map + 0x20u) != 0u) {
            int32_t i;
            for (i = 0; (int16_t)i < (int16_t)EF_U16(EF_U32(0x001a4a9cu) + 0x1cu); i++) {
                ef_addr32 e = EF_U32(EF_U32(0x001a4a9cu) + 0x20u) + (uint32_t)((int32_t)(int16_t)i * 0x19);
                int32_t ex = (int16_t)EF_U16(e);
                int32_t ey = (int16_t)(uint16_t)(0x190 - (int32_t)(int16_t)EF_U16(e + 4u));
                int32_t m = 8 / ((int32_t)EF_U32(0x0019769cu) >> 16);
                kind = EF_U8(e + 0x18u);
                if (px > ex - m && px < ex + m && py > ey - m && py < ey + m) {
                    result = i;
                    break;
                }
            }
        }
    }
    if (kind == 1u || kind == 2u || kind == 8u || kind == 5u)
        result = -1;
    return result;
}

/* 0x1d5f4, 266 bytes. In play mode (byte 0x125847) queue a sound/effect event for the vector VEC (EAX, three dwords; a positive second component is clamped to 0) and type TYPE (DX): look up an orientation record for VEC with 0x7bb00, take the next free slot from the ring pointed to by 0x425b24 (types in words 0x425bde, 0x425bea, 0x425bee, 0x425bd4) or by 0x425b88 (all other types) via its next-pointer at +0x6a, copy the vector into +0, +4, +8 and the orientation words (or zeros) into +0xc, +0xe, +0x10, load word +0x12 from the per-type table at +0x14 + 2 * TYPE, make the slot current and finally trigger 0x9e508 with that word. */
void queue_effect_event_1d5f4(ef_addr32 vec, uint32_t type)
{
    int16_t t = (int16_t)type;
    ef_addr32 orient;
    ef_addr32 slot;
    ef_addr32 ring;
    if (EF_U8(0x00125847u) == 0u)
        return;
    if ((int32_t)EF_U32(vec + 4u) > 0)
        EF_U32(vec + 4u) = 0u;
    orient = guest_fn_7bb00_x(vec);
    if (t == (int16_t)EF_U16(0x00425bdeu) || t == (int16_t)EF_U16(0x00425beau)
        || t == (int16_t)EF_U16(0x00425beeu) || t == (int16_t)EF_U16(0x00425bd4u))
        ring = 0x00425b24u;
    else
        ring = 0x00425b88u;
    slot = EF_U32(EF_U32(ring) + 0x6au);
    EF_U32(slot) = EF_U32(vec);
    EF_U32(slot + 4u) = EF_U32(vec + 4u);
    EF_U32(ring) = slot;
    EF_U32(slot + 8u) = EF_U32(vec + 8u);
    if (orient != 0u) {
        EF_U16(slot + 0xcu) = EF_U16(orient);
        EF_U16(slot + 0xeu) = EF_U16(orient + 2u);
        EF_U16(slot + 0x10u) = EF_U16(orient + 4u);
    } else {
        EF_U16(slot + 0xcu) = 0u;
        EF_U16(slot + 0xeu) = 0u;
        EF_U16(slot + 0x10u) = 0u;
    }
    EF_U16(slot + 0x12u) = EF_U16(slot + (uint32_t)((int32_t)t * 2) + 0x14u);
    guest_fn_9e508((int32_t)(int16_t)EF_U16(slot + 0x12u), 0u);
}

/* 0x254e4, 268 bytes. Player/leader contact check for the record at 0x1f71bc: look up the unit of its +0x144 word and of its +0x41 word (kind byte +0x16c must be 2 and flag bits 2, 4, 8 of +0x24 clear). If the second unit's mode byte +0x16d is 3 and the record's double +0x15a is positive and +0x150 negative, send message code 0x12 (otherwise return); otherwise compare the unit's double +0x150 with the record's +0x150: not greater sends 0x13, greater sends 0x14 and also sets mode 3 on the object of word 0x1f71c6. Messages go through 0x46254 with the word 0x1f71c6 as source, the +0x41 word as target and two zero stack arguments. */
void check_contact_254e4(void)
{
    ef_addr32 p = EF_U32(0x001f71bcu);
    ef_addr32 u;
    uint32_t flags;
    uint32_t code;
    double m150, m15a;
    if (guest_fn_7489c((int32_t)EF_U32(p + 0x144u) >> 16) == 0u)
        return;
    u = guest_fn_7489c((int32_t)EF_U32(EF_U32(0x001f71bcu) + 0x41u) >> 16);
    if (u == 0u || EF_U8(u + 0x16cu) != 2u)
        return;
    flags = EF_U8(u + 0x24u);
    if ((flags & 2u) != 0u || (flags & 4u) != 0u || (flags & 8u) != 0u)
        return;
    p = EF_U32(0x001f71bcu);
    if (EF_U8(u + 0x16du) == 3u) {
        memcpy(&m15a, ef_guest_memory_ptr(p + 0x15au), sizeof m15a);
        if (0.0 >= m15a)
            return;
        memcpy(&m150, ef_guest_memory_ptr(p + 0x150u), sizeof m150);
        if (0.0 <= m150)
            return;
        code = 0x12u;
    } else {
        double um150;
        memcpy(&um150, ef_guest_memory_ptr(u + 0x150u), sizeof um150);
        memcpy(&m150, ef_guest_memory_ptr(p + 0x150u), sizeof m150);
        if (um150 <= m150) {
            code = 0x13u;
        } else {
            guest_fn_46254((uint32_t)((int32_t)EF_U32(0x001f71c6u) >> 16), (int32_t)EF_U32(p + 0x41u) >> 16, 0x14u, 0u, 0u, 0u);
            guest_fn_2541c_x((int32_t)EF_U32(0x001f71c6u) >> 16, 3u);
            return;
        }
    }
    guest_fn_46254((uint32_t)((int32_t)EF_U32(0x001f71c6u) >> 16), (int32_t)EF_U32(p + 0x41u) >> 16, code, 0u, 0u, 0u);
}

/* 0xd6b80, 268 bytes. Read an axis from the latched position 0x420dac against the calibration words 0x420db8 (centre), 0x420da8 (max) and 0x420dc0 (min) when bit 4 of byte 0x15bac0 is set (otherwise return 0): ratio = (centre - pos) / (pos <= centre ? max - pos : pos - min), squared with its sign kept (-ratio * |ratio|) times the double 0x18f914; if that float is not above the float 0x18f91c the value becomes -1.0, it is capped at 1.0, negated and stored as a float at OUT (EAX). Returns 1 when written. */
uint8_t read_axis_d6b80(ef_addr32 out)
{
    int32_t pos = (int32_t)EF_U32(0x00420dacu);
    int32_t centre = (int32_t)EF_U32(0x00420db8u);
    int32_t a, b;
    float ratio, shaped, thr;
    double k, v, x;
    if ((EF_U8(0x0015bac0u) & 0x4u) == 0u)
        return 0u;
    a = centre - pos;
    if (centre >= pos)
        b = (int32_t)EF_U32(0x00420da8u) - pos;
    else
        b = pos - (int32_t)EF_U32(0x00420dc0u);
    ratio = (float)((double)a / (double)b);
    memcpy(&k, ef_guest_memory_ptr(0x0018f914u), sizeof k);
    memcpy(&thr, ef_guest_memory_ptr(0x0018f91cu), sizeof thr);
    shaped = (float)((-(double)ratio * abs_double_b427c((double)ratio)) * k);
    if (shaped > thr)
        v = (double)shaped;
    else
        v = -1.0;
    if (1.0 > v)
        x = v;
    else
        x = 1.0;
    {
        float o = (float)(-x);
        memcpy(ef_guest_memory_ptr(out), &o, sizeof o);
    }
    EF_U32(0x00420dacu) = (uint32_t)pos;
    return 1u;
}

/* 0x897c4, 270 bytes. Per-frame step of the approach controller of unit U (EAX) driven by the phase byte 0x20f22f. Phase 1: while the double 0x1fcf68 exceeds the double 0x18a580 integrate d = ((0x1fcf08 * 0x1fcf10) / ((0x18a588 - 0x1fd020) * 0x1fcf48, a zero divisor replaced by 1.0) * 0x18a590), store it in 0x1fcef0 and add d * 0x1fcca0 to 0x1fd020; otherwise finish with 0x87970(U). Phase 2: raise 0x1fcf48 by the double 0x18a5a0 while it is above 0x18a598 and, once 0x1fd020 drops below 0x18a5a8 and 0x89698(U) succeeds, jump to the unit's alternate target (+0x5b into +0x30, clearing +0x5a, then 0x87970) or, without one, mark it unreachable (+0x63 = 0x7fffffff, +0x5f = 0). */
void step_approach_897c4(ef_addr32 u)
{
    double a, b, c, d, e, f, g, h;
    if (EF_U8(0x0020f22fu) == 1u) {
        memcpy(&a, ef_guest_memory_ptr(0x001fcf68u), sizeof a);
        memcpy(&b, ef_guest_memory_ptr(0x0018a580u), sizeof b);
        if (a > b) {
            double t;
            memcpy(&c, ef_guest_memory_ptr(0x0018a588u), sizeof c);
            memcpy(&d, ef_guest_memory_ptr(0x001fd020u), sizeof d);
            memcpy(&e, ef_guest_memory_ptr(0x001fcf48u), sizeof e);
            t = (c - d) * e;
            if (t == 0.0)
                t = 1.0;
            memcpy(&f, ef_guest_memory_ptr(0x001fcf08u), sizeof f);
            memcpy(&g, ef_guest_memory_ptr(0x001fcf10u), sizeof g);
            memcpy(&h, ef_guest_memory_ptr(0x0018a590u), sizeof h);
            f = ((f * g) / t) * h;
            memcpy(ef_guest_memory_ptr(0x001fcef0u), &f, sizeof f);
            memcpy(&g, ef_guest_memory_ptr(0x001fcca0u), sizeof g);
            d = f * g + d;
            memcpy(ef_guest_memory_ptr(0x001fd020u), &d, sizeof d);
        } else {
            guest_fn_87970(u);
        }
    }
    if (EF_U8(0x0020f22fu) == 2u) {
        double e2, lim, inc, z, zl;
        memcpy(&e2, ef_guest_memory_ptr(0x001fcf48u), sizeof e2);
        memcpy(&lim, ef_guest_memory_ptr(0x0018a598u), sizeof lim);
        if (e2 > lim) {
            memcpy(&inc, ef_guest_memory_ptr(0x0018a5a0u), sizeof inc);
            e2 = e2 + inc;
            memcpy(ef_guest_memory_ptr(0x001fcf48u), &e2, sizeof e2);
        }
        memcpy(&z, ef_guest_memory_ptr(0x001fd020u), sizeof z);
        memcpy(&zl, ef_guest_memory_ptr(0x0018a5a8u), sizeof zl);
        if (z < zl && find_nearest_target_89698(u) != 0u) {
            ef_addr32 alt = EF_U32(u + 0x5bu);
            if (alt != 0u) {
                EF_U8(u + 0x5au) = 0u;
                EF_U32(u + 0x30u) = alt;
                guest_fn_87970(u);
            } else {
                EF_U32(u + 0x63u) = 0x7fffffffu;
                EF_U32(u + 0x5fu) = alt;
            }
        }
    }
}

/* 0xb04e8, 107 bytes. Is record B (EDX) within the range limit of record A (EAX)? Distance of the integer coordinates at +0x11/+0x15 (0xf83d0) is compared with a double picked by A's kind byte +0xb: kinds 0, 2 and 5 use 0x18dddf, kinds 1, 3 and 4 use 0x18ddd7, other kinds always fail. Returns 1 when the distance is below the limit. */
uint32_t in_kind_range_b04e8(ef_addr32 a, ef_addr32 b)
{
    double d = int_distance_2d_f83d0((int32_t)EF_U32(a + 0x11u), (int32_t)EF_U32(b + 0x11u), (int32_t)EF_U32(a + 0x15u), (int32_t)EF_U32(b + 0x15u));
    double lim;
    switch (EF_U8(a + 0xbu)) {
    case 0: case 2: case 5:
        memcpy(&lim, ef_guest_memory_ptr(0x0018dddfu), sizeof lim);
        return d < lim ? 1u : 0u;
    case 1: case 3: case 4:
        memcpy(&lim, ef_guest_memory_ptr(0x0018ddd7u), sizeof lim);
        return d < lim ? 1u : 0u;
    default:
        return 0u;
    }
}

/* 0x5c2d8, 123 bytes. Show the scaled value (double 0x1a2ef0 * 0x185f9a * 0x185fa2, rounded to float): format it into a 16 byte buffer with 0x185f8c when it is negative or 0x185f94 otherwise, and display the text through 0x5e21c using the word at 0x128170 + 15 * the byte 0x1a6d1b. */
void show_scaled_value_5c2d8(void)
{
    char buf[16];
    double a, b, c, z;
    float f;
    uint16_t id;
    uint32_t idx = EF_U8(0x001a6d1bu);
    memcpy(&a, ef_guest_memory_ptr(0x001a2ef0u), sizeof a);
    memcpy(&b, ef_guest_memory_ptr(0x00185f9au), sizeof b);
    memcpy(&c, ef_guest_memory_ptr(0x00185fa2u), sizeof c);
    f = (float)(a * b * c);
    id = EF_U16(0x00128170u + idx * 15u);
    z = 0.0;
    if (z > (double)f)
        guest_sprintf_a4342((ef_addr32)(uintptr_t)buf, 0x00185f8cu, (double)f);
    else
        guest_sprintf_a4342((ef_addr32)(uintptr_t)buf, 0x00185f94u, (double)f);
    guest_fn_5e21c_x((uint32_t)(int32_t)(int16_t)id, 0u, (uint32_t)(uintptr_t)buf, 0u);
}

/* 0x5fc80, 168 bytes. Label the box ENTRY (AX) of the table at 0x127a98 (10 byte entries: x, y, w, h words, 5th word unused): draw the double 0x1a3078 * 0x186072 with format 0x18604c at (x - w/2 + 5, y - h/2 + 2) and the dword 0x197238 with format 0x186064 at (x + w/2 - 0x1e, same y), both in colour 0xf mode 1, then run 0x5b288 with value 0xf, word x, zero. */
void label_box_5fc80(uint32_t entry)
{
    ef_addr32 e = 0x00127a98u + (uint32_t)((int32_t)(int16_t)entry * 10);
    int32_t sx = (int16_t)EF_U16(e);
    int32_t sy = (int16_t)EF_U16(e + 2u);
    int32_t hw = (int32_t)(int16_t)EF_U16(e + 4u) >> 1;
    int32_t hh = (int32_t)(int16_t)EF_U16(e + 6u) >> 1;
    int32_t y = (int16_t)(uint16_t)(sy - hh + 2);
    double a, b;
    memcpy(&a, ef_guest_memory_ptr(0x001a3078u), sizeof a);
    memcpy(&b, ef_guest_memory_ptr(0x00186072u), sizeof b);
    draw_formatted_text_mode_48bb0(0xfu, 1u, (int16_t)(uint16_t)(sx - hw + 5), y, 0x0018604cu, a * b);
    draw_formatted_text_mode_48bb0(0xfu, 1u, (int16_t)(uint16_t)(hw + sx - 0x1e), y, 0x00186064u, EF_U32(0x00197238u));
    run_with_temp_state_5b288(0xfu, (uint16_t)sx, 0u, 0u);
}

/* 0x83200, 145 bytes. Convert the world point (EDX, EBX, ECX) to camera-relative 8.8 fixed point for OUT (EAX): subtract the camera dwords 0x1fcc14, 0x1fcc18, 0x1fcc1c, halve all three differences until none needs more than 16 bits, add the camera back and pass the values shifted left by 8 to 0x15c0ea (z, y, x order of the stack arguments, result pointer last). */
void to_camera_space_83200(ef_addr32 out, int32_t x, int32_t y, int32_t z)
{
    int32_t cx = (int32_t)EF_U32(0x001fcc14u);
    int32_t cy = (int32_t)EF_U32(0x001fcc18u);
    int32_t cz = (int32_t)EF_U32(0x001fcc1cu);
    int32_t dx = x - cx;
    int32_t dy = y - cy;
    int32_t dz = z - cz;
    uint32_t t = (uint32_t)((dx >> 31) ^ (dx >> 15)) | (uint32_t)((dy >> 31) ^ (dy >> 15)) | (uint32_t)((dz >> 31) ^ (dz >> 15));
    if (t != 0u) {
        do {
            dx >>= 1;
            dy >>= 1;
            dz >>= 1;
            t >>= 1;
        } while (t != 0u);
    }
    guest_fn_15c0ea_x((uint32_t)((dz + (int32_t)EF_U32(0x001fcc1cu)) << 8),
                      (uint32_t)((dy + (int32_t)EF_U32(0x001fcc18u)) << 8),
                      (uint32_t)((dx + (int32_t)EF_U32(0x001fcc14u)) << 8), out);
}

/* 0x89b58, 151 bytes. Is the unit U (EAX) still pointing at its target? Take the target word at [[U+0x30]+0x4b] (0 gives 0), convert the navigation vector with 0x8fa58, subtract it from 0x1fcf58, normalise and compare the angle to the aim vector 0x1fcfd8 against the double at 0x12bcd5 + 145 * (byte U+0x18) scaled by 0x18a5c0 and 0x18a5c8. A degenerate vector counts as aligned (1); within the limit returns 1; outside it clears byte U+0x59 and returns 0. */
uint8_t still_aligned_89b58(ef_addr32 u)
{
    uint8_t v0[0x18];
    uint8_t v1[0x18];
    uint8_t v2[0x18];
    ef_addr32 t = EF_U32(EF_U32(u + 0x30u) + 0x4bu);
    double k1, k2, lim;
    if (t == 0u)
        return 0u;
    guest_fn_8fa58_x(t, (ef_addr32)(uintptr_t)v0, (ef_addr32)(uintptr_t)v0);
    subtract_scene_vectors((ef_addr32)(uintptr_t)v2, 0x001fcf58u, (ef_addr32)(uintptr_t)v0);
    if (normalize_vector_b4424((ef_addr32)(uintptr_t)v1, (ef_addr32)(uintptr_t)v2) == 0u)
        return 1u;
    memcpy(&lim, ef_guest_memory_ptr(0x0012bcd5u + (uint32_t)EF_U8(u + 0x18u) * 145u), sizeof lim);
    memcpy(&k1, ef_guest_memory_ptr(0x0018a5c0u), sizeof k1);
    memcpy(&k2, ef_guest_memory_ptr(0x0018a5c8u), sizeof k2);
    lim = lim * k1 * k2;
    if (angle_between_88ff8(0x001fcfd8u, (ef_addr32)(uintptr_t)v1) < lim)
        return 1u;
    EF_U8(u + 0x59u) = 0u;
    return 0u;
}

/* 0x6c2a4, 297 bytes. Derive the flight-model drag terms from the player's double at [0x1f71bc]+0x59 (v). g = -v * K(0x18644c) is stored at 0x1f6fc8 and the ratio (K(0x186454) - g) / (K(0x186454) + g) * double 0x1284a4 at 0x12849c; when g exceeds 0x18645c a quadratic correction t * t * 0x18646c * 0x186474 + 0x12849c (t = g + 0x186464) is added there, and the result is reset to the constant 0x3fb6872b020c49ba when it stays below 0x18646c. The lift term 0x1f6f90 is 0x1284ac * 0x1284b4 + 0x1284bc, recomputed with v in place of 0x1284b4 when v exceeds 0x1284b4. Finally 0x1f6fc0 = 0xa4470(max(0, 0x1f6f90 * 0x18647c)) and 0x1f6fb0 = double 0x1f71b0 / 0x1f6fc0. */
void update_drag_terms_6c2a4(void)
{
    double v, g, k1, k2, ratio, a, b;
    double t, c, kq, s;
    double ca, cb, cc, lift, h, r, num;
    ef_addr32 p = EF_U32(0x001f71bcu);
    memcpy(&v, ef_guest_memory_ptr(p + 0x59u), sizeof v);
    memcpy(&k1, ef_guest_memory_ptr(0x0018644cu), sizeof k1);
    g = (-v) * k1;
    memcpy(ef_guest_memory_ptr(0x001f6fc8u), &g, sizeof g);
    memcpy(&k2, ef_guest_memory_ptr(0x00186454u), sizeof k2);
    memcpy(&a, ef_guest_memory_ptr(0x001284a4u), sizeof a);
    ratio = ((k2 - g) / (g + k2)) * a;
    memcpy(ef_guest_memory_ptr(0x0012849cu), &ratio, sizeof ratio);
    memcpy(&b, ef_guest_memory_ptr(0x0018645cu), sizeof b);
    if (g > b) {
        memcpy(&t, ef_guest_memory_ptr(0x00186464u), sizeof t);
        t = g + t;
        memcpy(&c, ef_guest_memory_ptr(0x0018646cu), sizeof c);
        memcpy(&kq, ef_guest_memory_ptr(0x00186474u), sizeof kq);
        s = ((t * c) * t) * kq + ratio;
        memcpy(ef_guest_memory_ptr(0x0012849cu), &s, sizeof s);
        if (!(s >= c)) {
            EF_U32(0x0012849cu) = 0x020c49bau;
            EF_U32(0x001284a0u) = 0x3fb6872bu;
        }
    }
    memcpy(&cb, ef_guest_memory_ptr(0x001284b4u), sizeof cb);
    memcpy(&ca, ef_guest_memory_ptr(0x001284acu), sizeof ca);
    memcpy(&cc, ef_guest_memory_ptr(0x001284bcu), sizeof cc);
    lift = ca * cb + cc;
    memcpy(ef_guest_memory_ptr(0x001f6f90u), &lift, sizeof lift);
    p = EF_U32(0x001f71bcu);
    memcpy(&v, ef_guest_memory_ptr(p + 0x59u), sizeof v);
    if (v > cb) {
        lift = ca * v + cc;
        memcpy(ef_guest_memory_ptr(0x001f6f90u), &lift, sizeof lift);
    }
    memcpy(&h, ef_guest_memory_ptr(0x0018647cu), sizeof h);
    memcpy(&lift, ef_guest_memory_ptr(0x001f6f90u), sizeof lift);
    h = lift * h;
    if (0.0 > h)
        h = 0.0;
    r = guest_fn_a4470(h);
    memcpy(ef_guest_memory_ptr(0x001f6fc0u), &r, sizeof r);
    memcpy(&num, ef_guest_memory_ptr(0x001f71b0u), sizeof num);
    num = num / r;
    memcpy(ef_guest_memory_ptr(0x001f6fb0u), &num, sizeof num);
}

/* 0x6e0b0, 273 bytes. Place a table offset in the frame of unit B (EBX): both units A (EDX) and B must exist. Copy B's dwords +0x71, +0x75, +0x69, +0x6d to 0x1f6fd0, 0x1f6fd4, 0x1f7060, 0x1f7064 and its double +0x61 (heading) to 0x1f6ff0, then take the offset (x, y, z doubles at 0x1252ac, 0x125304, 0x12535c plus 279 * ECX + 8 * S0) and write OUT (EAX) = (cos(h) * x - sin(h) * y + B.x, sin(h) * x + cos(h) * y + B.y, z + B.z) with B's position at +0x49, +0x51, +0x59. Returns 1 on success, 0 when a unit is missing. */
uint8_t offset_in_unit_frame_6e0b0(ef_addr32 out, int32_t a_id, int32_t b_id, uint32_t index, uint32_t sel)
{
    ef_addr32 ua = guest_fn_7489c((int16_t)a_id);
    ef_addr32 ub = guest_fn_7489c((int16_t)b_id);
    ef_addr32 row;
    double heading, mx, my, mz, c, s, x, y, z, px, py, pz;
    if (ua == 0u || ub == 0u)
        return 0u;
    EF_U32(0x001f6fd0u) = EF_U32(ub + 0x71u);
    EF_U32(0x001f6fd4u) = EF_U32(ub + 0x75u);
    EF_U32(0x001f7060u) = EF_U32(ub + 0x69u);
    EF_U32(0x001f7064u) = EF_U32(ub + 0x6du);
    row = 0x001252acu + (index & 0xffu) * 279u + (sel & 0xffu) * 8u;
    memcpy(&heading, ef_guest_memory_ptr(ub + 0x61u), sizeof heading);
    memcpy(ef_guest_memory_ptr(0x001f6ff0u), &heading, sizeof heading);
    memcpy(&mx, ef_guest_memory_ptr(row), sizeof mx);
    memcpy(&my, ef_guest_memory_ptr(row + 0x58u), sizeof my);
    memcpy(&mz, ef_guest_memory_ptr(row + 0xb0u), sizeof mz);
    c = cos_b45a2(heading);
    s = sin_b45ac(heading);
    x = c * mx - s * my;
    y = s * mx + c * my;
    z = mz;
    memcpy(&px, ef_guest_memory_ptr(ub + 0x49u), sizeof px);
    memcpy(&py, ef_guest_memory_ptr(ub + 0x51u), sizeof py);
    memcpy(&pz, ef_guest_memory_ptr(ub + 0x59u), sizeof pz);
    x = px + x;
    y = py + y;
    z = pz + z;
    memcpy(ef_guest_memory_ptr(out), &x, sizeof x);
    memcpy(ef_guest_memory_ptr(out + 8u), &y, sizeof y);
    memcpy(ef_guest_memory_ptr(out + 0x10u), &z, sizeof z);
    return 1u;
}

/* 0x72390, 277 bytes. Move unit ID (AX) one tick: dt = word 0x127d92 * double 0x186a69 (stored at 0x1f7188); the velocity (doubles +0xa1, +0xb9, +0xd1 times the double +0x91, stored at 0x1f7170, 0x1f7178, 0x1f7180) gains the second set +0xb1, +0xc9, +0xe1 times the double +0x99 when the type table byte 0x1285a1 + 0xe5 * (byte +0xe9) is set. Unless bit 0x10 of byte +0x26 is set the position doubles +0x49, +0x51, +0x59 advance by velocity * dt; the bit is then cleared and 0x73d50(ID) runs. */
void move_unit_72390(uint32_t id)
{
    ef_addr32 u = guest_fn_7489c((int16_t)id);
    double dt, k, m, s, vx, vy, vz, px, py, pz;
    uint32_t type;
    if (u == 0u)
        return;
    {
        int32_t ticks = (int32_t)EF_U16(0x00127d92u);
        memcpy(&k, ef_guest_memory_ptr(0x00186a69u), sizeof k);
        dt = (double)ticks * k;
    }
    memcpy(ef_guest_memory_ptr(0x001f7188u), &dt, sizeof dt);
    type = (uint32_t)EF_U8(u + 0xe9u) * 0xe5u;
    memcpy(&m, ef_guest_memory_ptr(u + 0x91u), sizeof m);
    memcpy(&vx, ef_guest_memory_ptr(u + 0xa1u), sizeof vx);
    memcpy(&vy, ef_guest_memory_ptr(u + 0xb9u), sizeof vy);
    memcpy(&vz, ef_guest_memory_ptr(u + 0xd1u), sizeof vz);
    vx = vx * m;
    vy = vy * m;
    vz = vz * m;
    if (EF_U8(0x001285a1u + type) != 0u) {
        double ex, ey, ez;
        memcpy(&s, ef_guest_memory_ptr(u + 0x99u), sizeof s);
        memcpy(&ex, ef_guest_memory_ptr(u + 0xb1u), sizeof ex);
        memcpy(&ey, ef_guest_memory_ptr(u + 0xc9u), sizeof ey);
        memcpy(&ez, ef_guest_memory_ptr(u + 0xe1u), sizeof ez);
        vx = ex * s + vx;
        vy = ey * s + vy;
        vz = ez * s + vz;
    }
    memcpy(ef_guest_memory_ptr(0x001f7170u), &vx, sizeof vx);
    memcpy(ef_guest_memory_ptr(0x001f7178u), &vy, sizeof vy);
    memcpy(ef_guest_memory_ptr(0x001f7180u), &vz, sizeof vz);
    if ((EF_U8(u + 0x26u) & 0x10u) == 0u) {
        memcpy(&px, ef_guest_memory_ptr(u + 0x49u), sizeof px);
        memcpy(&py, ef_guest_memory_ptr(u + 0x51u), sizeof py);
        memcpy(&pz, ef_guest_memory_ptr(u + 0x59u), sizeof pz);
        px = vx * dt + px;
        py = vy * dt + py;
        pz = vz * dt + pz;
        memcpy(ef_guest_memory_ptr(u + 0x49u), &px, sizeof px);
        memcpy(ef_guest_memory_ptr(u + 0x51u), &py, sizeof py);
        memcpy(ef_guest_memory_ptr(u + 0x59u), &pz, sizeof pz);
    }
    EF_U8(u + 0x26u) = (uint8_t)(EF_U8(u + 0x26u) & 0xefu);
    guest_fn_73d50_w((int16_t)id);
}

/* 0x848e0, 280 bytes. Refresh the view cursor record REC (EDX) from the camera block SRC (EAX: 12 bytes copied to 0x1fcc14, 6 bytes from +0xc to 0x1fcc20, dword 0x1fcc26 = SRC+0x12 >> 16). Does nothing unless the unit of word 0x1f71c8 has a slot (0x84a80). Resets the view when REC bit 0x10 of +0x23 is set, latches the control bits, runs 0x83a40 and clears the "placed" bits (0x1fcbe3 bit 0, REC+0x22 bit 0). When the pick point 0x1fcc2c.. is active (byte 0x1fcc43, words 0x1fcbd0/2 clear) and either bit 0x80 of 0x1fcbe2 is set or the point lies outside the bounds (0x832b8) it clears bits 0x38 of 0x1fcbe2 and re-initialises the UI. If bit 0x40 (and not 0x80) of 0x1fcbe2 is then set with the pick point active, mark it placed and copy the point into REC +0, +4, +8; otherwise invalidate REC+4 with 0x80000000. */
void refresh_view_cursor_848e0(ef_addr32 src, ef_addr32 rec)
{
    uint8_t active;
    uint8_t flags;
    if (unit_has_slot_84a80((int16_t)((int32_t)EF_U32(0x001f71c8u) >> 16)) == 0u)
        return;
    memcpy_dc832(0x001fcc14u, src, 12u);
    memcpy_dc832(0x001fcc20u, src + 0xcu, 6u);
    EF_U32(0x001fcc26u) = (uint32_t)((int32_t)EF_U32(src + 0x12u) >> 16);
    if ((EF_U8(rec + 0x23u) & 0x10u) != 0u)
        reset_unit_view_846c8(rec);
    latch_control_bits_84788(rec);
    guest_fn_83a40_x();
    EF_U8(0x001fcbe3u) = (uint8_t)(EF_U8(0x001fcbe3u) & 0xfeu);
    active = EF_U8(0x001fcc43u);
    EF_U8(rec + 0x22u) = (uint8_t)(EF_U8(rec + 0x22u) & 0xfeu);
    if (active != 0u && EF_U16(0x001fcbd0u) == 0u && EF_U16(0x001fcbd2u) == 0u) {
        if ((EF_U8(0x001fcbe2u) & 0x80u) != 0u
            || box_outside_bounds_832b8((int32_t)EF_U32(0x001fcc2cu), (int32_t)EF_U32(0x001fcc30u), (int32_t)EF_U32(0x001fcc34u)) != 0u) {
            EF_U8(0x001fcbe2u) = (uint8_t)(EF_U8(0x001fcbe2u) & 0xc7u);
            initialize_ui_component();
            run_12d90_pair_13950();
        }
    }
    flags = EF_U8(0x001fcbe2u);
    if ((flags & 0x40u) != 0u && (flags & 0x80u) == 0u && EF_U8(0x001fcc43u) != 0u) {
        EF_U8(0x001fcbe3u) = (uint8_t)(EF_U8(0x001fcbe3u) | 1u);
        EF_U8(rec + 0x22u) = (uint8_t)((EF_U8(rec + 0x22u) & 0xfeu) | 1u);
        EF_U32(rec) = EF_U32(0x001fcc2cu);
        EF_U32(rec + 4u) = EF_U32(0x001fcc30u);
        EF_U32(rec + 8u) = EF_U32(0x001fcc34u);
        return;
    }
    EF_U32(rec + 4u) = 0x80000000u;
}

/* 0x910f4, 284 bytes. Try to hit one of the 0x28 map entries (0x30 bytes each at 0x211af0: x dword +0, z +4, y +8, kind word +0x1e, flags byte +0x24) with the point (x = EDX, y = ECX) in play mode (byte 0x125847): entries with flag bits 1, 2 or 4 are skipped, as are those more than 0x80000 away on either axis; an entry accepted by 0xd0470 (its kind word >> 16 and the position >> 8) is consumed: 0xcd150 for the kind, flag 4 set, word +0x22 = 0xffff, 0xcd258 and score 0x64 (0xcd2e8); in play mode a 0x64 notification for its index goes out through 0x575fc. Returns 1 when an entry was hit. S0 only feeds the unused 0x8ac64 lookup of the original (a pure table mapping, not called here). */
uint8_t hit_map_entry_910f4(uint32_t u0, int32_t x, uint32_t u2, int32_t y, uint32_t s0)
{
    int32_t i;
    (void)u0; (void)u2; (void)s0;
    if (EF_U8(0x00125847u) == 0u)
        return 0u;
    for (i = 0; i < 0x28; i++) {
        ef_addr32 e = 0x00211af0u + (uint32_t)i * 0x30u;
        uint8_t fl = EF_U8(e + 0x24u);
        if ((fl & 2u) != 0u || (fl & 1u) != 0u || (fl & 4u) != 0u)
            continue;
        if ((int32_t)guest_fn_cf94a(EF_U32(e) - (uint32_t)x) > 0x80000)
            continue;
        if ((int32_t)guest_fn_cf94a(EF_U32(e + 8u) - (uint32_t)y) > 0x80000)
            continue;
        if (guest_fn_d0470((int32_t)EF_U32(e + 0x1eu) >> 16, (int32_t)EF_U32(e) >> 8, (int32_t)EF_U32(e + 4u) >> 8, (int32_t)EF_U32(e + 8u) >> 8) == 0u)
            continue;
        guest_fn_cd150((int32_t)EF_U32(e + 0x1eu) >> 16);
        EF_U8(e + 0x24u) = (uint8_t)(EF_U8(e + 0x24u) | 4u);
        EF_U16(e + 0x22u) = 0xffffu;
        guest_fn_cd258();
        guest_fn_cd2e8(0x64);
        if (EF_U8(0x00127da9u) != 0u)
            send_two_word_packet_40((uint16_t)i, 0x64u);
        return 1u;
    }
    return 0u;
}

/* 0xe93dc, 272 bytes. Run the text parser over the script TEXT (EAX, stored at 0x464380; DL goes to byte 0x1600a6): the cursor 0x464384 starts at dword 0x1600a0, the line counter word 0x16007c at 1. Complains through the error hook when the parser mode byte 0x1600a4 is 0 (0x1912c0) or 2 (0x191304). Then loops: skip blanks and tabs, count CR LF pairs as lines, hand every other token to 0xe882c (a zero result makes the whole run fail) and skip the rest of its line. Returns 1 when all tokens parsed. */
uint8_t parse_script_text_e93dc(ef_addr32 text, uint32_t flag)
{
    uint8_t ok = 1u;
    uint16_t line = 1u;
    ef_addr32 cur;
    EF_U32(0x00464380u) = text;
    EF_U8(0x001600a6u) = (uint8_t)flag;
    cur = EF_U32(0x001600a0u);
    EF_U32(0x00464384u) = cur;
    EF_U16(0x0016007cu) = 1u;
    if (EF_U8(0x001600a4u) == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x001912c0u);
    if (EF_U8(0x001600a4u) == 2u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00191304u);
    line = EF_U16(0x0016007cu);
    cur = EF_U32(0x00464384u);
    while (EF_U8(cur) != 0u) {
        uint8_t c = EF_U8(cur);
        if (c == 0x20u || c == 0x09u) {
            cur++;
            continue;
        }
        EF_U32(0x00464384u) = cur;
        EF_U16(0x0016007cu) = line;
        if (c == 0x0du) {
            cur++;
            line++;
            if (EF_U8(cur) == 0x0au)
                cur++;
            continue;
        }
        if (guest_fn_e882c_x() == 0u)
            ok = 0u;
        line = EF_U16(0x0016007cu);
        cur = EF_U32(0x00464384u);
        while (EF_U8(cur) != 0u && EF_U8(cur) != 0x0du)
            cur++;
        if (EF_U8(cur) == 0x0du) {
            cur++;
            line++;
            if (EF_U8(cur) == 0x0au)
                cur++;
        }
    }
    EF_U16(0x0016007cu) = line;
    EF_U32(0x00464384u) = cur;
    return ok;
}

/* 0xddd48, 288 bytes. Build the real-mode call block for a disk request: the request record at 0x4260a0 gets command byte 0x0a and the control record at 0x426098 command 0x1a, drive byte 0x15ee98, 3, offset word +0xe = low 4 bits of the real-mode address of the first record (0x604), segment word +0x10 and +0x12 = 7; the real-mode registers at 0x4260b4 take the segment/offset of the control record (0x4260d6/0x4260c4), 0x1510 in 0x4260d0, word 0x42611e >> 16 in 0x4260cc and 0 in 0x4260c8 before the interrupt shim 0x8b8(0x2f). The request's sector is then computed as 4500 * (+3 dword >> 16) + 75 * byte +4 + byte +3 (stored in 0x4260ac minus 0x96 at the end), the start/end words 0x42611c/0x42611a come from bytes +1/+2 and the count word 0x426122 is end - start + 1. */
void setup_disk_request_ddd48(void)
{
    ef_addr32 req = EF_U32(0x004260a0u);
    ef_addr32 ctl;
    uint32_t r = guest_fn_604_x(req);
    uint32_t r2;
    int32_t hi;
    uint32_t sector;
    uint32_t start, end;
    EF_U8(req) = 0x0au;
    ctl = EF_U32(0x00426098u);
    EF_U8(ctl) = 0x1au;
    EF_U8(ctl + 1u) = EF_U8(0x0015ee98u);
    EF_U8(ctl + 2u) = 3u;
    EF_U16(ctl + 0x12u) = 7u;
    EF_U16(ctl + 0xeu) = (uint16_t)(r & 0xfu);
    EF_U16(ctl + 0x10u) = (uint16_t)((int32_t)r >> 16);
    r2 = guest_fn_604_x(ctl);
    EF_U16(0x004260d6u) = (uint16_t)((int32_t)r2 >> 16);
    EF_U32(0x004260c4u) = r2 & 0xffffu;
    EF_U32(0x004260d0u) = 0x1510u;
    EF_U32(0x004260ccu) = (uint32_t)((int32_t)EF_U32(0x0042611eu) >> 16);
    EF_U32(0x004260c8u) = 0u;
    guest_fn_8b8(0x2fu, 0x004260b4u);
    req = EF_U32(0x004260a0u);
    hi = (int32_t)EF_U32(req + 3u) >> 16;
    sector = (uint32_t)hi * 4500u;
    sector = sector + (uint32_t)EF_U8(req + 4u) * 75u;
    EF_U32(0x004260acu) = sector;
    sector = sector + EF_U8(req + 3u);
    start = EF_U8(req + 1u);
    EF_U16(0x0042611cu) = (uint16_t)start;
    EF_U32(0x004260acu) = sector;
    end = EF_U8(req + 2u);
    EF_U16(0x0042611au) = (uint16_t)end;
    EF_U32(0x004260acu) = sector - 0x96u;
    EF_U16(0x00426122u) = (uint16_t)(end - (start & 0xffffu) + 1u);
}

/* 0x10f70, 164 bytes. Is the point INDEX (DL) of the 6-byte point list at BASE (EAX; words at +0x18, +0x1a hold y and x) inside the current script window? When byte 0x121ec6 is clear, orient the pair with the mode from the unit of word 0x1f8ec0 (0xa1858, then 0x10f28) and test it strictly inside the rectangle at 0xcd29c (left +0, right +6, top +4, bottom +0xa); otherwise it must equal the cursor words 0x197c24/0x197c26. */
uint8_t point_in_window_10f70(ef_addr32 base, uint32_t index)
{
    uint32_t v[2];
    ef_addr32 p = base + (index & 0xffu) * 6u;
    ef_addr32 r;
    int16_t x, y;
    v[0] = EF_U16(p + 0x18u);
    v[1] = EF_U16(p + 0x1au);
    if (EF_U8(0x00121ec6u) == 0u) {
        uint32_t mode = record_flag_bits56((int16_t)((int32_t)EF_U32(0x001f8ec0u) >> 16));
        transform_runtime_word_pair((uint16_t)(int16_t)mode, (ef_addr32)(uintptr_t)&v[1], (ef_addr32)(uintptr_t)&v[0]);
        r = script_cursor_data_cd29c();
        x = (int16_t)v[1];
        y = (int16_t)v[0];
        if (x > (int16_t)EF_U16(r) && x < (int16_t)EF_U16(r + 6u)
            && y > (int16_t)EF_U16(r + 4u) && y < (int16_t)EF_U16(r + 0xau))
            return 1u;
        return 0u;
    }
    if ((int16_t)v[1] == (int16_t)EF_U16(0x00197c24u) && (int16_t)v[0] == (int16_t)EF_U16(0x00197c26u))
        return 1u;
    return 0u;
}

/* 0x7b16c, 196 bytes. Look up the map cell under the point (X = EDX, Y = EBX, 11-bit cell size) for plane MODE (EAX, 0x7aac0): the cell column is X >> 11 and the row 0x18f - (Y >> 11), both wrapped into 0..0x18f (a negative remainder gets 0x18f added, as the original does). The tile id is built from the two plane bytes (hi from 0x1f8eb4, lo from 0x1f8eb8, 400 bytes per row); 0xffff gives 0, otherwise the result is the 0xcfa9c distance of the point to the cell centre. */
uint32_t cell_distance_at_7b16c(uint32_t mode, int32_t x, int32_t y)
{
    int32_t cx = x >> 11;
    int32_t ry = 0x18f - (y >> 11);
    int32_t col = cx % 0x190;
    int32_t row = ry % 0x190;
    uint32_t id;
    select_plane_source((int32_t)mode);
    if (col < 0)
        col += 0x18f;
    if (row < 0)
        row += 0x18f;
    {
        uint32_t off = (uint32_t)col * 400u + (uint32_t)row;
        id = ((uint32_t)EF_U8(EF_U32(0x001f8eb4u) + off) << 8) | EF_U8(EF_U32(0x001f8eb8u) + off);
    }
    if ((uint16_t)id == 0xffffu)
        return 0u;
    return page_point_distance_cfa9c((uint32_t)(int32_t)(int16_t)id,
                                      (uint32_t)((cx * 2 + 1) << 10),
                                      (uint32_t)(((y >> 11) * 2 + 1) << 10),
                                      (uint32_t)x, (uint32_t)y);
}

/* 0x817a8, 188 bytes. Per-frame hook of the tracked view target (record at 0x425b*): when its mode word 0x425bc8 is clear, or the dword 0x425bce >> 16 exceeds the dword 0x1a5bf0, and word 0x425bb8 is set, load the call block 0x15bddc.. (0x15bde0 = 0x425bb6 >> 16, 0x15bde8 = camera x 0x21444c, 0x15bdec = camera y 0x214448 - (0x425bb2 >> 16), 0x15bdf0 = camera z 0x214450 - (0x425bae >> 16), zeros in 0x15bddc/0x15bde4, 0xfa0 in 0x15be0c) and run 0x15be10. With the mode word set and within range, run 0x81680 when bit 0x80 of byte 0x425bc4 is set. Finally run 0x2a4c8 when word 0x1a5a8e is set. */
void update_view_target_817a8(void)
{
    int taken = 0;
    if (EF_U16(0x00425bc8u) != 0u && (int32_t)((int32_t)EF_U32(0x00425bceu) >> 16) > (int32_t)EF_U32(0x001a5bf0u))
        taken = 1;
    else if (EF_U16(0x00425bc8u) == 0u)
        taken = 1;
    if (taken) {
        if (EF_U16(0x00425bb8u) != 0u) {
            EF_U32(0x0015bde0u) = (uint32_t)((int32_t)EF_U32(0x00425bb6u) >> 16);
            EF_U32(0x0015bde8u) = EF_U32(0x0021444cu);
            EF_U32(0x0015bddcu) = 0u;
            EF_U32(0x0015bdecu) = EF_U32(0x00214448u) - (uint32_t)((int32_t)EF_U32(0x00425bb2u) >> 16);
            EF_U32(0x0015bde4u) = 0u;
            EF_U32(0x0015bdf0u) = EF_U32(0x00214450u) - (uint32_t)((int32_t)EF_U32(0x00425baeu) >> 16);
            EF_U32(0x0015be0cu) = 0xfa0u;
            guest_fn_15be10_x();
        }
    } else if ((EF_U8(0x00425bc4u) & 0x80u) != 0u) {
        guest_fn_81680_x();
    }
    if (EF_U16(0x001a5a8eu) != 0u)
        guest_fn_2a4c8_x();
}

/* 0x381a0, 225 bytes. Update the follower record INDEX (AX; 69 byte records at 0x1a14f8): fetch the scene object of the word +0x26 >> 16 (0x8ffe8), copy its position >> 8 into +7, +0xb, +0xf and, if the record has a link (+0x18) with the position at its +0, +4, +8, measure the squared distance (saturated to 0x7fffffff when any axis differs by 0x8000 or more). Within 0x800 the link is refreshed with 0x3856c, except that a link of kind 1 (+0xc) is skipped when its stamp +0xd is -1 or above the clock 0x1a14f0. */
void update_follower_381a0(uint32_t index)
{
    ef_addr32 rec = EF_U32(0x001a14f8u) + (uint32_t)((int32_t)(int16_t)index * 69);
    ef_addr32 u = guest_lookup_object_8ffe8((int32_t)EF_U32(rec + 0x26u) >> 16);
    ef_addr32 link;
    int32_t dx, dy, dz, d2;
    if (u == 0u)
        return;
    EF_U32(rec + 7u) = (uint32_t)((int32_t)EF_U32(u) >> 8);
    EF_U32(rec + 0xbu) = (uint32_t)((int32_t)EF_U32(u + 4u) >> 8);
    EF_U32(rec + 0xfu) = (uint32_t)((int32_t)EF_U32(u + 8u) >> 8);
    link = EF_U32(rec + 0x18u);
    if (link == 0u)
        return;
    dx = ((int32_t)EF_U32(u) >> 8) - (int32_t)EF_U32(link);
    dy = ((int32_t)EF_U32(u + 4u) >> 8) - (int32_t)EF_U32(link + 4u);
    dz = ((int32_t)EF_U32(u + 8u) >> 8) - (int32_t)EF_U32(link + 8u);
    if ((int32_t)abs_a28f2(dx) < 0x8000 && (int32_t)abs_a28f2(dy) < 0x8000 && (int32_t)abs_a28f2(dz) < 0x8000)
        d2 = dx * dx + dy * dy + dz * dz;
    else
        d2 = 0x7fffffff;
    if (d2 > 0x800)
        return;
    link = EF_U32(rec + 0x18u);
    if (EF_U8(link + 0xcu) == 1u) {
        int32_t stamp = (int32_t)EF_U32(link + 0xdu);
        if (stamp == -1 || stamp > (int32_t)EF_U32(0x001a14f0u))
            return;
    }
    refresh_link_pointer(rec);
}

/* 0x83a40, 245 bytes. Classify how directly the pick point 0x1fcc2c.. lies along the view axis: build the rotation from the three angle words (high halves of 0x1fcc1e, 0x1fcc20, 0x1fcc22, 0xe994c), transform the forward vector (0, 0, 0x4000) with 0x83734 and the offset from the camera 0x1fcc14.. to the pick point with 0x831a4, then store 2 * dot(forward, offset) / |offset| in 0x1fcbcc and set bit 0x80 of byte 0x1fcbe2 when the high word of 0x143ee8 exceeds that value (cleared otherwise). */
void classify_view_axis_83a40(void)
{
    int32_t fwd[3];
    int32_t off[3];
    int32_t dot, len, q;
    uint8_t cond;
    build_rotation_e994c(0u, (uint32_t)((int32_t)EF_U32(0x001fcc1eu) >> 16), (uint32_t)((int32_t)EF_U32(0x001fcc20u) >> 16), (uint32_t)((int32_t)EF_U32(0x001fcc22u) >> 16));
    guest_fn_83734_x((ef_addr32)(uintptr_t)fwd, 0, 0x4000, 0);
    store_scaled_vector_831a4((ef_addr32)(uintptr_t)off,
                              (int32_t)(EF_U32(0x001fcc2cu) - EF_U32(0x001fcc14u)),
                              (int32_t)(EF_U32(0x001fcc30u) - EF_U32(0x001fcc18u)),
                              (int32_t)(EF_U32(0x001fcc34u) - EF_U32(0x001fcc1cu)));
    dot = (int32_t)((uint32_t)fwd[1] * (uint32_t)off[1] + (uint32_t)fwd[0] * (uint32_t)off[0] + (uint32_t)fwd[2] * (uint32_t)off[2]);
    len = (int32_t)length_3d_decc4(off[0], off[1], off[2]);
    q = (int32_t)((uint32_t)dot * 2u) / len;
    EF_U32(0x001fcbccu) = (uint32_t)q;
    cond = ((int32_t)EF_U32(0x00143ee8u) >> 16) > q ? 1u : 0u;
    EF_U8(0x001fcbe2u) = (uint8_t)((EF_U8(0x001fcbe2u) & 0x7fu) | (uint8_t)(cond << 7));
}

/* 0x1b70, 359 bytes. Start the mission state machine step selected by byte 0x196fe8. Does nothing in network mode (byte 0x127da8) with phase 3. State 1 (running): wait until word 0x126516 and byte 0x132ac2 are clear, then reset the state byte and set the status byte 0x12db9e to 0x12. Otherwise initialise the request block (0x1a3649 = 0, 0x197080 = 0x1a, words 0x197089 = 0, 0x196fea = 0xfa, 0x196fec = 0x196fee = 1, dwords 0x197061 and 0x19708b = 0, byte 0x19707f = 0), set state 1 and run 0x2e04. If it produced no record (0x197061 still 0) outside play mode, set word 0x126518 = 1 and go idle; otherwise run 0x1f64 and 0x20ec (and 0x3cf0 when byte 0x196ff0), seed the camera blocks 0x19e5b5.. and 0x19e5c1.. from the record's position (y lowered by 0x493e0), mark them valid, hand 0x17f5c4 to 0x2fbc4 and set the status byte to 0x49. */
void start_mission_step_1b70(void)
{
    ef_addr32 rec;
    uint32_t x, y, z;
    if (EF_U8(0x00127da8u) != 0u && EF_U8(0x0012584bu) == 3u)
        return;
    if (EF_U8(0x00196fe8u) == 1u) {
        if (EF_U16(0x00126516u) != 0u)
            return;
        if (EF_U8(0x00132ac2u) != 0u)
            return;
        EF_U8(0x00196fe8u) = 0u;
        EF_U8(0x0012db9eu) = 0x12u;
        return;
    }
    EF_U8(0x001a3649u) = 0u;
    EF_U8(0x00197080u) = 0x1au;
    EF_U16(0x00197089u) = 0u;
    EF_U16(0x00196feau) = 0xfau;
    EF_U32(0x00197061u) = 0u;
    EF_U8(0x0019707fu) = 0u;
    EF_U8(0x00196fe8u) = 1u;
    EF_U32(0x0019708bu) = 0u;
    EF_U16(0x00196fecu) = 1u;
    EF_U16(0x00196feeu) = 1u;
    guest_fn_2e04_x();
    if (EF_U32(0x00197061u) == 0u && EF_U8(0x00127da9u) == 0u) {
        EF_U16(0x00126518u) = 1u;
        EF_U8(0x00196fe8u) = 0u;
        return;
    }
    select_active_entry_1f64();
    guest_fn_20ec_x();
    if (EF_U8(0x00196ff0u) != 0u)
        reset_selection_3cf0();
    rec = EF_U32(0x00197061u);
    x = EF_U32(rec);
    EF_U32(0x0019e5c1u) = x;
    EF_U32(0x0019e5b5u) = x;
    y = EF_U32(rec + 4u) - 0x493e0u;
    EF_U32(0x0019e54du) = rec;
    EF_U32(0x0019e5c5u) = y;
    EF_U32(0x0019e5b9u) = y;
    z = EF_U32(rec + 8u);
    EF_U32(0x0019e551u) = 0u;
    EF_U32(0x0019e5c9u) = z;
    EF_U32(0x0019e5bdu) = z;
    EF_U8(0x0019e5e1u) = 1u;
    EF_U8(0x0019e5e2u) = 1u;
    EF_U8(0x0019e555u) = 1u;
    EF_U8(0x0019e5e3u) = 1u;
    guest_fn_2fbc4(0x0017f5c4u);
    EF_U8(0x0012db9eu) = 0x49u;
}

/* 0x76cc4, 358 bytes. Initialise the table of 30 (0x1e) 25 byte slots with 0x76b10 (count byte 0x1f8920), set the word at +1 of every slot to 0xff and fill the constant block at 0x12a6cc.. (type byte 2, then pairs of IEEE doubles; byte 0x12a715 = 3) used by the table's model parameters. */
void init_slot_table_76cc4(void)
{
    uint8_t n;
    uint8_t i;
    EF_U8(0x001f8920u) = 0x1eu;
    alloc_table_25_76b10(0x1eu);
    n = EF_U8(0x001f8920u);
    if (n != 0u) {
        i = 0u;
        do {
            EF_U16(EF_U32(0x001f8918u) + (uint32_t)i * 0x19u + 1u) = 0xffu;
            i++;
        } while (i < n);
    }
    EF_U8(0x0012a6ccu) = 2u;
    EF_U32(0x0012a6cdu) = 0xcccccccdu;
    EF_U32(0x0012a6d1u) = 0xc0430cccu;
    EF_U32(0x0012a6d5u) = 0x9fbe76c9u;
    EF_U32(0x0012a6d9u) = 0xc0339f1au;
    EF_U32(0x0012a6ddu) = 0x6a7ef9dbu;
    EF_U32(0x0012a6e5u) = 0xcccccccdu;
    EF_U32(0x0012a6e9u) = 0xc0430cccu;
    EF_U32(0x0012a6edu) = 0x9fbe76c9u;
    EF_U32(0x0012a6f5u) = 0x6a7ef9dbu;
    EF_U32(0x0012a6e1u) = 0x401cf4bcu;
    EF_U32(0x0012a6f1u) = 0x40339f1au;
    EF_U32(0x0012a6f9u) = 0x401cf4bcu;
    EF_U32(0x0012a74au) = 0xc04b6e97u;
    EF_U32(0x0012a74eu) = 0x00000000u;
    EF_U32(0x0012a752u) = 0x00000000u;
    EF_U32(0x0012a756u) = 0xcc81e6d8u;
    EF_U32(0x0012a746u) = 0x8d4fdf3bu;
    EF_U32(0x0012a75au) = 0x3ffbde0fu;
    EF_U32(0x0012a732u) = 0xc0447713u;
    EF_U32(0x0012a736u) = 0x602393a2u;
    EF_U32(0x0012a73au) = 0x40313e33u;
    EF_U32(0x0012a73eu) = 0xd6ce5247u;
    EF_U32(0x0012a71au) = 0xc0447713u;
    EF_U32(0x0012a71eu) = 0x602393a2u;
    EF_U32(0x0012a726u) = 0xd6ce5247u;
    EF_U32(0x0012a72eu) = 0x9a2f6587u;
    EF_U32(0x0012a742u) = 0x40064b3fu;
    EF_U32(0x0012a716u) = 0x9a2f6587u;
    EF_U32(0x0012a722u) = 0xc0313e33u;
    EF_U32(0x0012a72au) = 0x40064b3fu;
    EF_U8(0x0012a715u) = 3u;
    EF_U8(0x001f8920u) = n;
}

/* 0x45dd4, 358 bytes. Spell the number VALUE (EAX) as a list of glyph words at 0x1a522a (word 0 holds the count, the glyphs follow at +2, +4, ...): thousands from the table pointed to by 0x1a51f4 (glyph = word +2 of the entry, followed by the word of 0x1a5208 +2 in the second slot), hundreds likewise with the "hundred" glyph of 0x1a520c, then 0..19 straight from the table, or the tens digit from 0x1a5204 plus the unit digit. The quotient bytes are truncated to 8 bits as in the original. Returns 0x1a522a. */
ef_addr32 spell_number_45dd4(int32_t value)
{
    ef_addr32 tab = EF_U32(0x001a51f4u);
    int32_t rest = value;
    uint8_t n = 1u;
    uint8_t q;
    q = (uint8_t)(rest / 1000);
    if (q != 0u) {
        EF_U16(0x001a522cu) = EF_U16(tab + (uint32_t)q * 4u + 2u);
        EF_U16(0x001a522eu) = EF_U16(EF_U32(0x001a5208u) + 2u);
        n = 3u;
        rest -= (int32_t)q * 1000;
    }
    q = (uint8_t)(rest / 100);
    if (q != 0u) {
        EF_U16(0x001a522au + (uint32_t)n * 2u) = EF_U16(tab + (uint32_t)q * 4u + 2u);
        n++;
        EF_U16(0x001a522au + (uint32_t)n * 2u) = EF_U16(EF_U32(0x001a520cu) + 2u);
        n++;
        rest -= (int32_t)q * 100;
    }
    if (rest <= 0x13) {
        EF_U16(0x001a522au + (uint32_t)n * 2u) = EF_U16(tab + (uint32_t)rest * 4u + 2u);
        n++;
    } else {
        q = (uint8_t)(rest / 10);
        EF_U16(0x001a522au + (uint32_t)n * 2u) = EF_U16(EF_U32(0x001a5204u) + (uint32_t)q * 4u + 2u);
        n++;
        rest -= (int32_t)q * 10;
        if (rest != 0) {
            EF_U16(0x001a522au + (uint32_t)n * 2u) = EF_U16(tab + (uint32_t)rest * 4u + 2u);
            n++;
        }
    }
    EF_U16(0x001a522au) = (uint16_t)(n - 1u);
    EF_U32(0x001a51f4u) = tab;
    return 0x001a522au;
}

/* 0x1d474, 343 bytes. Replay the queued effect events (see queue_effect_event_1d5f4) that are close to the camera (0x21444c, 0x214448, 0x214450 as x, y, z): walk the circular list headed at 0x425b88 (limit 0x2000 on each axis) and then the one at 0x425b24 (limit 0x1000). A slot is played when the scene gate (0x8bf1c on y << 8) allows it and its sound word (+0x10 >> 16) is not -1; slots of the first list first load their orientation words (+0xa, +0xc, +0xe >> 16) into the sound record with 0xa0ea8, then both lists start the sound with 0x9e0c4 using the position shifted left by 16. */
void play_queued_effects_1d474(void)
{
    ef_addr32 head = EF_U32(0x00425b88u);
    ef_addr32 s;
    if (head != 0u) {
        s = head;
        do {
            if ((int32_t)guest_fn_cf94a(EF_U32(s) - EF_U32(0x0021444cu)) < 0x2000
                && (int32_t)guest_fn_cf94a(EF_U32(s + 4u) - EF_U32(0x00214448u)) < 0x2000
                && (int32_t)guest_fn_cf94a(EF_U32(s + 8u) - EF_U32(0x00214450u)) < 0x2000
                && scene_gate_allows((int32_t)(EF_U32(s + 4u) << 8)) != 0u
                && ((int32_t)EF_U32(s + 0x10u) >> 16) != -1) {
                guest_fn_a0ea8((int32_t)EF_U32(s + 0x10u) >> 16, (uint32_t)((int32_t)EF_U32(s + 0xau) >> 16),
                               (int32_t)EF_U32(s + 0xcu) >> 16, (uint32_t)((int32_t)EF_U32(s + 0xeu) >> 16));
                guest_fn_9e0c4((int32_t)EF_U32(s + 0x10u) >> 16, EF_U32(s) << 16, EF_U32(s + 4u) << 16, EF_U32(s + 8u) << 16);
            }
            head = EF_U32(0x00425b88u);
            s = EF_U32(s + 0x6au);
        } while (s != head);
    }
    head = EF_U32(0x00425b24u);
    if (head != 0u) {
        s = head;
        do {
            if ((int32_t)guest_fn_cf94a(EF_U32(s) - EF_U32(0x0021444cu)) < 0x1000
                && (int32_t)guest_fn_cf94a(EF_U32(s + 4u) - EF_U32(0x00214448u)) < 0x1000
                && (int32_t)guest_fn_cf94a(EF_U32(s + 8u) - EF_U32(0x00214450u)) < 0x1000
                && scene_gate_allows((int32_t)(EF_U32(s + 4u) << 8)) != 0u
                && ((int32_t)EF_U32(s + 0x10u) >> 16) != -1) {
                guest_fn_9e0c4((int32_t)EF_U32(s + 0x10u) >> 16, EF_U32(s) << 16, EF_U32(s + 4u) << 16, EF_U32(s + 8u) << 16);
            }
            head = EF_U32(0x00425b24u);
            s = EF_U32(s + 0x6au);
        } while (s != head);
    }
}

/* 0x27348, 361 bytes. Release the parts of the unit U (EAX) that are marked as detached (bit 4 of byte +0x16 in the 0x44 byte part records at [U+0x48], counted by the type byte table 0x123819 + 0xa4 * (byte U+0x3a)). The unit's rotation matrix is built from its angle words +0xe, +0xc, +0x10 (0xd0b68, 0xd0ebc, 0xd0dbc, 0xd0fb8); each marked part gets the world position U.pos + (offset(+0x28, +0x2c, +0x30) * matrix) >> 15, is created as a unit of its own with 0x87db8 (its +0xa and +0xc words, byte +0x19, owner U) and loses the mark. */
void detach_parts_27348(ef_addr32 u)
{
    int32_t m[9];
    uint16_t i;
    ef_addr32 mp = (ef_addr32)(uintptr_t)m;
    initialize_scene_angle_record(mp);
    update_scene_angle_first(mp, EF_U16(u + 0xeu));
    update_scene_angle_second(mp, EF_U16(u + 0xcu));
    update_scene_angle_third(mp, EF_U16(u + 0x10u));
    for (i = 0u; (int16_t)EF_U8(0x00123819u + (uint32_t)EF_U8(u + 0x3au) * 0xa4u) > (int16_t)i; i++) {
        ef_addr32 r = EF_U32(u + 0x48u) + (uint32_t)i * 0x44u;
        int32_t ox, oy, oz;
        if ((EF_U8(r + 0x16u) & 0x4u) == 0u)
            continue;
        ox = (int32_t)EF_U32(r + 0x28u);
        oy = (int32_t)EF_U32(r + 0x2cu);
        oz = (int32_t)EF_U32(r + 0x30u);
        EF_U32(r) = EF_U32(u)
            + (uint32_t)(((int32_t)((uint32_t)ox * (uint32_t)m[0])) >> 15)
            + (uint32_t)(((int32_t)((uint32_t)oy * (uint32_t)m[1])) >> 15)
            + (uint32_t)(((int32_t)((uint32_t)oz * (uint32_t)m[2])) >> 15);
        EF_U32(r + 4u) = EF_U32(u + 4u)
            + (uint32_t)(((int32_t)((uint32_t)ox * (uint32_t)m[3])) >> 15)
            + (uint32_t)(((int32_t)((uint32_t)oy * (uint32_t)m[4])) >> 15)
            + (uint32_t)(((int32_t)((uint32_t)oz * (uint32_t)m[5])) >> 15);
        EF_U32(r + 8u) = EF_U32(u + 8u)
            + (uint32_t)(((int32_t)((uint32_t)ox * (uint32_t)m[6])) >> 15)
            + (uint32_t)(((int32_t)((uint32_t)oy * (uint32_t)m[7])) >> 15)
            + (uint32_t)(((int32_t)((uint32_t)oz * (uint32_t)m[8])) >> 15);
        create_unit_87db8(EF_U32(r), EF_U32(r + 4u), EF_U32(r + 8u), (uint32_t)((int32_t)EF_U32(r + 0xau) >> 16),
                          (uint32_t)((int32_t)EF_U32(r + 0xcu) >> 16), 0u, 0u, EF_U8(r + 0x19u), 0u, u);
        EF_U8(r + 0x16u) = (uint8_t)(EF_U8(r + 0x16u) & 0xfbu);
    }
}

/* 0x55928, 385 bytes. Build (or refresh) the unit described by the mission record R (EAX) of type TYPE (DL): look for an existing unit with the tag byte +0x13 / word +0x14 (0x87f9c) or take a fresh one (0x879c4) whose +0x50 comes from the type table 0x135878 (entry + 6) and +0x51 from R+0x14. Copy class (+0x18 -> +0x18, with its word from the 145 byte table at 0x12bc8c into +0x2e), position (+1, +5, +9), heading words (+0xd, +0xf, +0x11), clear +0x53, +0x42, +0x19, mark +0x47/+0x49 as 0x1234; classes 0x1f and 0x20 get +0x56 = 0xff and the unit of the dword +0x14 >> 16 as +0x4b, others take +0x56 from R+0x16. A valid word in +0x54 >> 16 links that unit back (+0x30, its +0x1c). Then copy +0x14 to +0x51, +0x19 to +0x26, +0x1d to +0x58, give the type's word +0xb to +0x8b (+0x8d = 0) and, for a new class-1 unit (table byte 0x12bca7), refresh its +0x68 strip word via 0x80d40/0x80abc. Finishes with 0x89e08. */
void build_unit_from_record_55928(ef_addr32 r, uint32_t type)
{
    uint8_t created = 0u;
    uint8_t t = (uint8_t)type;
    ef_addr32 u = find_ui_linked_tag(EF_U8(r + 0x13u), (int16_t)((int32_t)EF_U32(r + 0x12u) >> 16));
    uint32_t cls;
    ef_addr32 ti;
    if (u == 0u) {
        u = guest_fn_879c4_x();
        ti = EF_U32(0x00135878u + (uint32_t)t * 32u);
        EF_U8(u + 0x50u) = EF_U8(ti + 6u);
        EF_U16(u + 0x51u) = EF_U16(r + 0x14u);
        created = 1u;
    }
    cls = EF_U8(r + 0x18u);
    EF_U8(u + 0x18u) = (uint8_t)cls;
    EF_U16(u + 0x2eu) = EF_U16(0x0012bc8cu + cls * 145u);
    EF_U32(u) = EF_U32(r + 1u);
    EF_U32(u + 4u) = EF_U32(r + 5u);
    EF_U32(u + 8u) = EF_U32(r + 9u);
    EF_U16(u + 0xcu) = EF_U16(r + 0xdu);
    EF_U16(u + 0xeu) = EF_U16(r + 0xfu);
    EF_U8(u + 0x53u) = 0u;
    EF_U16(u + 0x47u) = 0x1234u;
    EF_U16(u + 0x49u) = 0x1234u;
    EF_U8(u + 0x42u) = 0u;
    EF_U8(u + 0x19u) = 0u;
    EF_U16(u + 0x10u) = EF_U16(r + 0x11u);
    if (EF_U8(r + 0x18u) == 0x20u || EF_U8(r + 0x18u) == 0x1fu) {
        EF_U16(u + 0x56u) = 0xffu;
        EF_U32(u + 0x4bu) = guest_fn_7489c((int32_t)EF_U32(r + 0x14u) >> 16);
    } else {
        EF_U16(u + 0x56u) = EF_U16(r + 0x16u);
    }
    {
        int32_t link = (int32_t)EF_U32(u + 0x54u) >> 16;
        if (link != 0xff) {
            ef_addr32 other = guest_fn_7489c(link);
            if (other != 0u) {
                EF_U32(u + 0x30u) = other;
                EF_U32(other + 0x1cu) = u;
            }
        }
    }
    EF_U16(u + 0x51u) = EF_U16(r + 0x14u);
    EF_U32(u + 0x26u) = EF_U32(r + 0x19u);
    ti = EF_U32(0x00135878u + (uint32_t)t * 32u);
    EF_U16(u + 0x8du) = 0u;
    EF_U16(u + 0x8bu) = EF_U16(ti + 0xbu);
    EF_U8(u + 0x58u) = EF_U8(r + 0x1du);
    if (created != 0u && EF_U8(0x0012bca7u + (uint32_t)EF_U8(u + 0x18u) * 145u) == 1u) {
        update_ui_record_from_template((int16_t)((int32_t)EF_U32(u + 0x2cu) >> 16), u + 0x68u);
        EF_U16(u + 0x68u) = (uint16_t)guest_fn_80abc_x();
    }
    guest_fn_89e08(u);
}

/* 0x74718, 382 bytes. Destroy the unit ID (AX). Units that use the secondary grid (0x7574c) are removed with the effect 0x5f/2 (0x106b0 with the position >> 8, then 0x10750; in play mode also announced with packet 0x56974); other units first unlink from their leader (the unit of dword +0x144 >> 16 loses its +0x158 back reference when it points at ID) and, unless bit 0x10 of +0x24 is already set, explode: 0x106b0, bit 0x10 set, 0x10750(ID, 1, 0x64) and the matching packet. Units owned by us (word +0x18 equal 0xff or byte 0x1a666b) that are not yet flagged (bit 2 of +0x24) and not on the secondary grid get the wreck timer +0x22 = 0x96 and the wreck model byte +0x187 from the map 0x74448 of byte +0xe9. Bit 2 is set in every case. */
void destroy_unit_74718(uint32_t id)
{
    int32_t uid = (int16_t)id;
    ef_addr32 u = guest_fn_7489c(uid);
    if (u == 0u)
        return;
    if (object_uses_secondary_grid((int16_t)uid) != 0u) {
        guest_fn_106b0_x(0xffffffffu, (uint32_t)((int32_t)EF_U32(u) >> 8), (uint32_t)((int32_t)EF_U32(u + 4u) >> 8), (uint32_t)((int32_t)EF_U32(u + 8u) >> 8), 0u);
        guest_fn_10750_x(uid, 2, 0x5f);
        if (EF_U8(0x00127da9u) != 0u)
            send_packet_2b((int16_t)uid, 2, 0x5f);
    } else {
        if (EF_U16(u + 0x146u) != 0xffu) {
            ef_addr32 p = guest_fn_7489c((int32_t)EF_U32(u + 0x144u) >> 16);
            if (p != 0u && EF_U16(p + 0x158u) == (uint16_t)uid) {
                EF_U32(p + 0x15au) = 0u;
                EF_U32(p + 0x15eu) = 0u;
                EF_U16(p + 0x158u) = 0xffu;
            }
        }
        if ((EF_U8(u + 0x24u) & 0x10u) == 0u) {
            guest_fn_106b0_x(0xffffffffu, (uint32_t)((int32_t)EF_U32(u) >> 8), (uint32_t)((int32_t)EF_U32(u + 4u) >> 8), (uint32_t)((int32_t)EF_U32(u + 8u) >> 8), 0u);
            EF_U8(u + 0x24u) = (uint8_t)(EF_U8(u + 0x24u) | 0x10u);
            guest_fn_10750_x(uid, 1, 0x64);
            if (EF_U8(0x00127da9u) != 0u)
                send_packet_2b((int16_t)uid, 1, 0x64);
        }
    }
    if (((int32_t)EF_U32(u + 0x16u) >> 16) == 0xff || (uint16_t)EF_U8(0x001a666bu) == EF_U16(u + 0x18u)) {
        if ((EF_U8(u + 0x24u) & 0x2u) == 0u && object_uses_secondary_grid((int16_t)uid) == 0u) {
            uint32_t m = map_code_74448(EF_U8(u + 0xe9u));
            EF_U16(u + 0x22u) = 0x96u;
            EF_U8(u + 0x187u) = (uint8_t)m;
        }
    }
    EF_U8(u + 0x24u) = (uint8_t)(EF_U8(u + 0x24u) | 0x2u);
}

/* 0x9021c, 345 bytes. Per-frame handling of a ground sound source U (EAX): when it lies within 0x100000 of the camera (fixed point, compare with 0x21444c, 0x214448, 0x214450 shifted by 8) set its height +4 to the negative cell distance (0x7b16c, plane 0) shifted left by 8, load its orientation words into the sound record (0xa0ea8) and, if the scene gate (0x8bf1c) allows the height, snapshot the camera and source positions into the line-of-sight block 0x420904.. (source y raised by -10) and its word copies 0x420930... Unless 0x7b7d8 reports it as muted start the sound with 0x9e0c4 using the position shifted left by 8. */
void update_ground_sound_9021c(ef_addr32 u)
{
    uint32_t d;
    if ((int32_t)guest_fn_cf94a(EF_U32(u) - (EF_U32(0x0021444cu) << 8)) >= 0x100000)
        return;
    if ((int32_t)guest_fn_cf94a(EF_U32(u + 4u) - (EF_U32(0x00214448u) << 8)) >= 0x100000)
        return;
    if ((int32_t)guest_fn_cf94a(EF_U32(u + 8u) - (EF_U32(0x00214450u) << 8)) >= 0x100000)
        return;
    d = cell_distance_at_7b16c(0u, (int32_t)EF_U32(u) >> 8, (int32_t)EF_U32(u + 8u) >> 8);
    EF_U32(u + 4u) = (0u - d) << 8;
    guest_fn_a0ea8((int32_t)EF_U32(u + 0x1eu) >> 16, (uint32_t)((int32_t)EF_U32(u + 0xau) >> 16),
                   (int32_t)EF_U32(u + 0xcu) >> 16, (uint32_t)((int32_t)EF_U32(u + 0xeu) >> 16));
    if (scene_gate_allows((int32_t)EF_U32(u + 4u)) != 0u) {
        EF_U32(0x00420910u) = EF_U32(0x0021444cu);
        EF_U32(0x00420904u) = EF_U32(0x00214448u);
        EF_U32(0x00420918u) = EF_U32(0x00214450u);
        EF_U32(0x00420914u) = (uint32_t)((int32_t)EF_U32(u) >> 8);
        EF_U32(0x00420908u) = (uint32_t)(((int32_t)EF_U32(u + 4u) >> 8) - 0xa);
        EF_U32(0x0042091cu) = (uint32_t)((int32_t)EF_U32(u + 8u) >> 8);
        EF_U16(0x00420930u) = EF_U16(0x0021444cu);
        EF_U16(0x00420932u) = EF_U16(0x00214448u);
        EF_U16(0x00420934u) = EF_U16(0x00214450u);
        EF_U16(0x00420936u) = EF_U16(0x00420914u);
        EF_U16(0x00420938u) = EF_U16(0x00420908u);
        EF_U16(0x0042093au) = EF_U16(0x0042091cu);
    }
    if (guest_fn_7b7d8_x() == 0u)
        guest_fn_9e0c4((int32_t)EF_U32(u + 0x1eu) >> 16, EF_U32(u) << 8, EF_U32(u + 4u) << 8, EF_U32(u + 8u) << 8);
}

/* 0x7a844, 375 bytes. Populate the 400 x 400 scenery map: for every cell whose id (hi byte plane 0x1f8eb4, lo byte plane 0x1f8eb8) is not 0xff, look the id up in the 6 byte variant table (0x1f8ea4: count word >> 16 at +2, base word >> 16 at +0, flag bits 14.. of +6), pick a random variant (the 16 bit generator 0xa45b3 modulo the count; 0x7a428 selects the active buffer first), and replace the id by the one 0x9de08 produces for that variant (17 byte entries from 0x1f8e9c); cells without a source get 0xffff. An id past the table count (0x1a2e74) is reported through the error hook (0x187bec). Runs inside the state file bracket 0xa1bf4(0x1f8db0, 2) / 0xa1cb4 when byte 0x12afbc is 1 and dword 0x1f8ea0 is 0, reseeds the generator (0xa45a8) and reports progress per column (0x985b0 / 0x985f4). Returns 0. */
uint32_t build_scenery_map_7a844(void)
{
    int32_t col;
    nop_9dddc();
    if (EF_U8(0x0012afbcu) == 1u && EF_U32(0x001f8ea0u) == 0u)
        open_state_file_a1bf4(0x001f8db0u, 2u);
    reseed_generator_a45a8();
    start_transfer_985b0(2u, 0x0013292cu, 0u, 0x18fu);
    for (col = 0; col < 0x190; col++) {
        uint32_t off;
        update_progress_985f4(2u, col);
        for (off = 0u; off != 0x27100u; off += 0x190u) {
            uint16_t result;
            uint32_t hi = EF_U8(EF_U32(0x001f8eb4u) + off + (uint32_t)col);
            if (hi != 0xffu) {
                uint32_t lo = EF_U8(EF_U32(0x001f8eb8u) + off + (uint32_t)col);
                int32_t id = (int32_t)((hi << 8) | lo);
                int32_t count = (int32_t)EF_U32(0x001a2e74u);
                ef_addr32 ent;
                int32_t n, rem, base;
                uint32_t r;
                if (id >= count)
                    guest_callptr3_x(EF_U32(0x00132a74u), 0x00187becu, (uint32_t)id, (uint32_t)count);
                ent = EF_U32(0x001f8ea4u) + (uint32_t)id * 6u;
                n = (int32_t)EF_U32(ent + 2u) >> 16;
                r = guest_fn_a45b3() & 0xffffu;
                rem = (int32_t)r % n;
                (void)copy_active_buffer_pointer();
                base = (int32_t)EF_U32(ent) >> 16;
                result = (uint16_t)guest_fn_9de08(EF_U32(0x001f8e9cu) + (uint32_t)((rem + base) * 0x11),
                                                  (uint32_t)((EF_U16(ent + 6u) >> 14) & 0xffu));
            } else {
                result = 0xffffu;
            }
            EF_U8(EF_U32(0x001f8eb8u) + off + (uint32_t)col) = (uint8_t)result;
            EF_U8(EF_U32(0x001f8eb4u) + off + (uint32_t)col) = (uint8_t)(result >> 8);
        }
    }
    if (EF_U8(0x0012afbcu) == 1u && EF_U32(0x001f8ea0u) == 0u)
        guest_fn_a1cb4();
    nop_9dddc();
    return 0u;
}

/* 0x45bfc, 412 bytes. Select the language tables for LANGUAGE (AL): store the byte in dword 0x1a51f0 and point the ten table slots 0x1a5218, 0x1a51f4, 0x1a5204, 0x1a520c, 0x1a5208, 0x1a51f8, 0x1a51fc, 0x1a5214, 0x1a5210, 0x1a5200 at the set for language 0, 1 or 2; any other value reports 0x184688 through the error hook. */
void select_language_tables_45bfc(uint32_t language)
{
    static const uint32_t sets[3][10] = {
        { 0x126e06u, 0x1268c0u, 0x126960u, 0x126a2cu, 0x126a38u, 0x12b12cu, 0x12b150u, 0x12b120u, 0x12b138u, 0x12b15cu },
        { 0x127236u, 0x126910u, 0x126988u, 0x126a30u, 0x126a3cu, 0x12b130u, 0x12b154u, 0x12b124u, 0x12b13cu, 0x12b160u },
        { 0x127666u, 0x1269b0u, 0x126a00u, 0x126a34u, 0x126a40u, 0x12b134u, 0x12b158u, 0x12b128u, 0x12b140u, 0u }
    };
    static const uint32_t slots[10] = {
        0x001a5218u, 0x001a51f4u, 0x001a5204u, 0x001a520cu, 0x001a5208u,
        0x001a51f8u, 0x001a51fcu, 0x001a5214u, 0x001a5210u, 0x001a5200u
    };
    uint32_t lang = language & 0xffu;
    uint32_t i;
    EF_U32(0x001a51f0u) = lang;
    if (lang > 2u) {
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00184688u);
        return;
    }
    for (i = 0u; i < 10u; i++)
        EF_U32(slots[i]) = sets[lang][i];
}

/* 0x3c994, 407 bytes. Split one text line at LINE (EAX) into its four fields by overwriting the separators with zeros: name (pointer kept in 0x1a2e8c), second field (0x1a2e90), the angle number (parsed by 0x9d4cd into word 0x1a2ea4), third field (0x1a2e80) and a quoted string (0x1a2e88, the closing quote is zeroed). Every missing field (newline reached first) is reported through the error hook with 0x183728, 0x183750, 0x183778 and 0x1837a0. The angle 90, 180 or 270 selects the variant 1, 2 or 3 (0 otherwise) for 0x9de08 on the third field, whose result goes to word 0x1a2ea2. Returns the pointer after the line's newline. */
ef_addr32 parse_table_line_3c994(ef_addr32 line)
{
    ef_addr32 p = line;
    uint32_t variant;
    uint16_t angle;
    EF_U32(0x001a2e8cu) = line;
    while (EF_U8(p) != 0x20u && EF_U8(p) != 0x09u && EF_U8(p) != 0x0au)
        p++;
    if (EF_U8(p) == 0x0au)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00183728u);
    while (EF_U8(p) == 0x20u || EF_U8(p) == 0x09u) {
        EF_U8(p) = 0u;
        p++;
    }
    EF_U32(0x001a2e90u) = p;
    while (EF_U8(p) != 0x20u && EF_U8(p) != 0x09u && EF_U8(p) != 0x0au)
        p++;
    if (EF_U8(p) == 0x0au)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00183750u);
    while (EF_U8(p) == 0x20u || EF_U8(p) == 0x09u) {
        EF_U8(p) = 0u;
        p++;
    }
    EF_U16(0x001a2ea4u) = (uint16_t)guest_fn_9d4cd(p);
    while (EF_U8(p) != 0x20u && EF_U8(p) != 0x09u && EF_U8(p) != 0x0au)
        p++;
    if (EF_U8(p) == 0x0au)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00183778u);
    while (EF_U8(p) == 0x20u || EF_U8(p) == 0x09u) {
        EF_U8(p) = 0u;
        p++;
    }
    EF_U32(0x001a2e80u) = p;
    while (EF_U8(p) != 0x20u && EF_U8(p) != 0x09u && EF_U8(p) != 0x0au)
        p++;
    if (EF_U8(p) == 0x0au)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x001837a0u);
    while (EF_U8(p) == 0x20u || EF_U8(p) == 0x09u || EF_U8(p) == 0x22u) {
        EF_U8(p) = 0u;
        p++;
    }
    EF_U32(0x001a2e88u) = p;
    while (EF_U8(p) != 0x22u)
        p++;
    EF_U8(p) = 0u;
    p++;
    while (EF_U8(p) != 0x0au)
        p++;
    angle = EF_U16(0x001a2ea4u);
    p++;
    if (angle == 0x5au)
        variant = 1u;
    else if (angle == 0xb4u)
        variant = 2u;
    else if (angle == 0x10eu)
        variant = 3u;
    else
        variant = 0u;
    EF_U16(0x001a2ea2u) = (uint16_t)guest_fn_9de08(EF_U32(0x001a2e80u), variant);
    return p;
}

/* 0xc9360, 404 bytes. Outline the visible box of the clip record at 0x41ec00 (clip words at 0x41ec00..0x41ec06; the box proper is x = 0x41ec08, y = 0x41ec0a, w = 0x41ec0c, h = 0x41ec0e). Colour COLOR (AL; values up to 0x10 go through the map at 0x13793c) and the pattern word 0x15b5cc[STYLE] are used. Nothing is drawn when the box lies entirely outside the screen limits (dwords 0x15b744, 0x15b748) or is empty; each of the four edges is drawn with 0xe7a34 only when it lies on the corresponding clip border. */
void outline_box_c9360(uint32_t color, uint32_t style)
{
    int32_t col = (int32_t)(color & 0xffu);
    uint8_t c;
    uint32_t pat;
    int32_t x, y, w, h;
    if (col <= 0x10)
        col = EF_U8(0x0013793cu + (uint32_t)col);
    c = (uint8_t)col;
    pat = EF_U16(0x0015b5ccu + (style & 0xffu) * 2u);
    x = (int32_t)EF_U32(0x0041ec06u) >> 16;
    y = (int32_t)EF_U32(0x0041ec08u) >> 16;
    w = (int32_t)EF_U32(0x0041ec0au) >> 16;
    h = (int32_t)EF_U32(0x0041ec0cu) >> 16;
    if (x >= (int32_t)EF_U32(0x0015b744u))
        return;
    if (y >= (int32_t)EF_U32(0x0015b748u))
        return;
    if (w + x <= 0)
        return;
    if (h + y <= 0)
        return;
    if (EF_U16(0x0041ec08u) == EF_U16(0x0041ec00u))
        guest_fn_e7a34_x((uint32_t)(h + y - 1), (uint32_t)x, (uint32_t)y, (uint32_t)x, pat, c);
    y = (int32_t)EF_U32(0x0041ec08u) >> 16;
    h = (int32_t)EF_U32(0x0041ec0cu) >> 16;
    {
        int32_t b = y + h;
        if (b == ((int32_t)EF_U32(0x0041ec00u) >> 16) + ((int32_t)EF_U32(0x0041ec04u) >> 16)) {
            int32_t xx = (int32_t)EF_U32(0x0041ec06u) >> 16;
            int32_t ww = (int32_t)EF_U32(0x0041ec0au) >> 16;
            guest_fn_e7a34_x((uint32_t)(b - 1), (uint32_t)(xx + ww - 1), (uint32_t)(b - 1), (uint32_t)xx, pat, c);
        }
    }
    {
        int32_t r = ((int32_t)EF_U32(0x0041ec0au) >> 16) + ((int32_t)EF_U32(0x0041ec06u) >> 16);
        if (r == ((int32_t)EF_U32(0x0041ec02u) >> 16) + ((int32_t)EF_U32(0x0041ebfeu) >> 16)) {
            int32_t yy = (int32_t)EF_U32(0x0041ec08u) >> 16;
            int32_t hh = (int32_t)EF_U32(0x0041ec0cu) >> 16;
            guest_fn_e7a34_x((uint32_t)yy, (uint32_t)(r - 1), (uint32_t)(hh + yy - 1), (uint32_t)(r - 1), pat, c);
        }
    }
    if (EF_U16(0x0041ec0au) == EF_U16(0x0041ec02u)) {
        int32_t xx = (int32_t)EF_U32(0x0041ec06u) >> 16;
        int32_t yy = (int32_t)EF_U32(0x0041ec08u) >> 16;
        int32_t ww = (int32_t)EF_U32(0x0041ec0au) >> 16;
        guest_fn_e7a34_x((uint32_t)yy, (uint32_t)xx, (uint32_t)yy, (uint32_t)(ww + xx - 1), pat, c);
    }
}

/* 0x5a2d0, 405 bytes. Redraw the map cursor for the player unit (word 0x1f71c8): latch the camera (dwords 0x1a6cee.. as high words, the first minus half of the tile size word 0x128172 + 15 * byte 0x1a6d1b, shifted by 1 - byte 0x12b369) with 0xce328 and 0xcdf48, look up the player's map position (0x84e3c with the owner's byte +0x8e of the record 0x19bb5c), find the ground height (0x7b16c) and project it (0x5cb1c). When the point is visible draw a cross of half-size 5 << byte 0x12b369 in colour byte 0x128148 with two 0xc8ccc lines. */
void draw_map_cursor_5a2d0(void)
{
    uint32_t pos[2];
    uint16_t pr[2];
    ef_addr32 u;
    uint32_t idx;
    uint32_t sh;
    int32_t half;
    uint32_t d;
    uint32_t owner;
    int32_t px, py, m;
    u = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    if (u == 0u)
        return;
    idx = EF_U8(0x001a6d1bu);
    sh = 1u - EF_U8(0x0012b369u);
    half = ((int32_t)EF_U32(0x00128172u + idx * 15u) >> 16) >> sh;
    latch_values_ce328((uint32_t)((int32_t)EF_U32(0x001a6cf0u) >> 16), (uint32_t)((int32_t)EF_U32(0x001a6cf6u) >> 16),
                       (uint32_t)(((int32_t)EF_U32(0x001a6ceeu) >> 16) - half), (uint32_t)((int32_t)EF_U32(0x001a6cf4u) >> 16));
    idx = EF_U8(0x001a6d1bu);
    sh = 1u - EF_U8(0x0012b369u);
    half = ((int32_t)EF_U32(0x00128172u + idx * 15u) >> 16) >> sh;
    guest_fn_cdf48((uint32_t)((int32_t)EF_U32(0x001a6cf0u) >> 16), (uint32_t)((int32_t)EF_U32(0x001a6cf6u) >> 16),
                   (uint32_t)(((int32_t)EF_U32(0x001a6ceeu) >> 16) - half), (uint32_t)((int32_t)EF_U32(0x001a6cf4u) >> 16));
    owner = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)EF_U32(0x0019bb5cu) >> 16) * 0xcau + 0x8eu);
    fetch_entry_pair_84e3c((ef_addr32)(uintptr_t)pos, (uint32_t)((int32_t)EF_U32(0x001f71c8u) >> 16), owner);
    d = cell_distance_at_7b16c(0u, (int32_t)pos[0], (int32_t)pos[1]);
    if (guest_fn_5cb1c_x((uint32_t)(uintptr_t)pr, (int32_t)pos[0], (int32_t)(0u - d), (int32_t)pos[1]) == 0u)
        return;
    px = (int16_t)pr[0];
    py = (int16_t)pr[1];
    m = 5 << EF_U8(0x0012b369u);
    guest_fn_c8ccc_x((uint32_t)(py + m), (uint32_t)(px + m), (uint32_t)(py - m), (uint32_t)(px - m), EF_U8(0x00128148u));
    guest_fn_c8ccc_x((uint32_t)(py + m), (uint32_t)(px - m), (uint32_t)(py - m), (uint32_t)(px + m), EF_U8(0x00128148u));
}

/* 0xe9dcc, 175 bytes. Derive three view angles from the direction doubles at 0x464490, 0x4644a8, 0x4644b0, 0x4644b8 and 0x4644c0: 0x432220 = atan2(0x464490, -0x4644a8), 0x4321f0 = atan2(0x4644b0, 0x4644b8) and, with 0x4644c0 clamped to [double 0x19149c, 1.0] in place, 0x432288 = acos-style 0xe7df4 of it. */
void derive_view_angles_e9dcc(void)
{
    double a, b, c, d, v, lo;
    double r;
    memcpy(&a, ef_guest_memory_ptr(0x00464490u), sizeof a);
    memcpy(&b, ef_guest_memory_ptr(0x004644a8u), sizeof b);
    r = atan2_b4620(a, -b);
    memcpy(&c, ef_guest_memory_ptr(0x004644b0u), sizeof c);
    memcpy(&d, ef_guest_memory_ptr(0x004644b8u), sizeof d);
    memcpy(ef_guest_memory_ptr(0x00432220u), &r, sizeof r);
    r = atan2_b4620(c, d);
    memcpy(ef_guest_memory_ptr(0x004321f0u), &r, sizeof r);
    memcpy(&v, ef_guest_memory_ptr(0x004644c0u), sizeof v);
    if (!(1.0 >= v)) {
        v = 1.0;
        memcpy(ef_guest_memory_ptr(0x004644c0u), &v, sizeof v);
    }
    memcpy(&v, ef_guest_memory_ptr(0x004644c0u), sizeof v);
    memcpy(&lo, ef_guest_memory_ptr(0x0019149cu), sizeof lo);
    if (!(v >= lo)) {
        EF_U32(0x004644c0u) = 0u;
        EF_U32(0x004644c4u) = 0xbff00000u;
    }
    memcpy(&v, ef_guest_memory_ptr(0x004644c0u), sizeof v);
    r = guest_fn_e7df4_x(v);
    memcpy(ef_guest_memory_ptr(0x00432288u), &r, sizeof r);
}

/* 0xe9f84, 177 bytes. Second set of view angles (sibling of 0xe9dcc): 0x4321f8 = atan2(double 0x4644b8, double 0x4644c0), 0x432230 = atan2(double 0x464498, double 0x464480) and, with the double at 0x4644b0 clamped to [0x1914a4, 1.0] in place, 0x432298 = 0xdeb1a(-0x4644b0). */
void derive_view_angles_e9f84(void)
{
    double a, b, c, d, v, lo, r;
    memcpy(&a, ef_guest_memory_ptr(0x004644b8u), sizeof a);
    memcpy(&b, ef_guest_memory_ptr(0x004644c0u), sizeof b);
    r = atan2_b4620(a, b);
    memcpy(ef_guest_memory_ptr(0x004321f8u), &r, sizeof r);
    memcpy(&c, ef_guest_memory_ptr(0x00464498u), sizeof c);
    memcpy(&d, ef_guest_memory_ptr(0x00464480u), sizeof d);
    r = atan2_b4620(c, d);
    memcpy(ef_guest_memory_ptr(0x00432230u), &r, sizeof r);
    memcpy(&v, ef_guest_memory_ptr(0x004644b0u), sizeof v);
    if (!(1.0 >= v)) {
        EF_U32(0x004644b0u) = 0u;
        EF_U32(0x004644b4u) = 0x3ff00000u;
    }
    memcpy(&v, ef_guest_memory_ptr(0x004644b0u), sizeof v);
    memcpy(&lo, ef_guest_memory_ptr(0x001914a4u), sizeof lo);
    if (!(v >= lo)) {
        EF_U32(0x004644b0u) = 0u;
        EF_U32(0x004644b4u) = 0xbff00000u;
    }
    memcpy(&v, ef_guest_memory_ptr(0x004644b0u), sizeof v);
    r = guest_fn_deb1a_x(-v);
    memcpy(ef_guest_memory_ptr(0x00432298u), &r, sizeof r);
}

/* 0x59334, 234 bytes. Status overlay: unless word 0x126516 is set, latch the full screen (0xcdf48 with 0..width-1, 0..height-1), print the dword 0x197238 with format 0x185e48 (0x185e54 when byte 0x197255 is set) at (0, 0xaa), then, with byte 0x127b1a set, either the scaled double (0x1a3078 * 0x185e78, format 0x185e60) when the double 0x1a30a8 is below 1.0, or 0x1a30a8 itself (format 0x185e6c), together with the string 0x12f43e at (0, 0xb4). */
void draw_status_overlay_59334(void)
{
    char buf[64];
    double lim, a, b, v;
    if (EF_U16(0x00126516u) != 0u)
        return;
    guest_fn_cdf48(0u, EF_U32(0x0015b744u) - 1u, 0u, EF_U32(0x0015b748u) - 1u);
    if (EF_U8(0x00197255u) == 0u)
        guest_sprintf_a4342((ef_addr32)(uintptr_t)buf, 0x00185e48u, EF_U32(0x00197238u));
    else
        guest_sprintf_a4342((ef_addr32)(uintptr_t)buf, 0x00185e54u, EF_U32(0x00197238u));
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(0xfu, 0, 0xaa, (ef_addr32)(uintptr_t)buf);
    memcpy(&lim, ef_guest_memory_ptr(0x001a30a8u), sizeof lim);
    if (1.0 > lim) {
        memcpy(&a, ef_guest_memory_ptr(0x001a3078u), sizeof a);
        memcpy(&b, ef_guest_memory_ptr(0x00185e78u), sizeof b);
        v = a * b;
        guest_sprintf_a4342((ef_addr32)(uintptr_t)buf, 0x00185e60u, 0x0012f43eu, v);
    } else {
        guest_sprintf_a4342((ef_addr32)(uintptr_t)buf, 0x00185e6cu, 0x0012f43eu, lim);
    }
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(0xfu, 0, 0xb4, (ef_addr32)(uintptr_t)buf);
}

/* 0x88574, 239 bytes. Track the closest candidate for the current target link: for the record REC (EAX) of class other than 1, 0x1f or 0x20 whose link +0x30 equals the target 0x20f150, compute the squared distance (positions >> 8) to the target and, when it beats the best so far (0x20f14c) and REC's byte +0x59 is clear, convert REC to a scene vector (0x88c54), build the player's position (doubles +0x49, +0x51 and +0x59 raised by double 0x18a456) and, unless 0x7b08c rejects the line (nonzero result), remember REC in 0x20f154. */
void track_closest_link_88574(ef_addr32 rec)
{
    uint8_t buf[0x30];
    ef_addr32 player = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    ef_addr32 t = EF_U32(0x0020f150u);
    uint8_t cls;
    int32_t dx, dy, dz, d2;
    double z, add;
    if (t == 0u)
        return;
    cls = EF_U8(rec + 0x18u);
    if (cls == 1u || cls == 0x1fu || cls == 0x20u)
        return;
    t = EF_U32(0x0020f150u);
    if (EF_U32(rec + 0x30u) != t)
        return;
    dx = ((int32_t)EF_U32(rec) >> 8) - ((int32_t)EF_U32(t) >> 8);
    dy = ((int32_t)EF_U32(rec + 4u) >> 8) - ((int32_t)EF_U32(t + 4u) >> 8);
    dz = ((int32_t)EF_U32(rec + 8u) >> 8) - ((int32_t)EF_U32(t + 8u) >> 8);
    d2 = dx * dx + dy * dy + dz * dz;
    if (d2 >= (int32_t)EF_U32(0x0020f14cu))
        return;
    EF_U32(0x0020f14cu) = (uint32_t)d2;
    if (EF_U8(rec + 0x59u) != 0u)
        return;
    guest_fn_88c54_x(rec, (ef_addr32)(uintptr_t)buf);
    memcpy(buf + 0x18, ef_guest_memory_ptr(player + 0x49u), 16u);
    memcpy(&z, ef_guest_memory_ptr(player + 0x59u), sizeof z);
    memcpy(&add, ef_guest_memory_ptr(0x0018a456u), sizeof add);
    z = z + add;
    memcpy(buf + 0x28, &z, sizeof z);
    if ((uint8_t)store_scaled_vectors_7b08c((uint32_t)(uintptr_t)(buf + 0x18), (uint32_t)(uintptr_t)buf) == 0u)
        EF_U32(0x0020f154u) = rec;
}

/* 0x9775c, 222 bytes. Settle a falling object U (EAX) onto the ground: t = ~(cell distance (0x7b16c, plane 0) of its x/y (+0xc, +0x14 >> 8) << 8). A drop of more than 10000 below the stored height +0x10 raises word 0x12dc76; when t lies below +0x10 the object lands: clear word +0xa, store t in +0x10, run 0x975f0, clear the vector at +0x24 (0xea864), start the effect 0x44 (0x801e4), and depending on the speed (0xea884 of +0x18 against the float 0x18ac6a) send tactical code 4 or 5 for the word 0x21448c (5 also clears byte 0x132ac2). A zero timer 0x2143e8 becomes 0xfa; finally 0x9783c(U, 4) supplies the return value. Returns 0 when nothing landed. */
uint32_t land_object_9775c(ef_addr32 u)
{
    uint32_t d = cell_distance_at_7b16c(0u, (int32_t)EF_U32(u + 0xcu) >> 8, (int32_t)EF_U32(u + 0x14u) >> 8);
    int32_t t = (int32_t)(0xffffffffu - (d << 8));
    float lim;
    double speed;
    uint32_t result = 0u;
    if ((int32_t)EF_U32(u + 0x10u) - t > (int32_t)0xffffd8f0)
        EF_U16(0x0012dc76u) = 1u;
    if (t >= (int32_t)EF_U32(u + 0x10u))
        return result;
    EF_U16(u + 0xau) = 0u;
    EF_U32(u + 0x10u) = (uint32_t)t;
    guest_fn_975f0_x(u);
    guest_fn_ea864(u + 0x24u, 0u, 0u, 0u);
    guest_fn_801e4(0x44u, 1u, 1u);
    speed = guest_fn_ea884_x(u + 0x18u);
    memcpy(&lim, ef_guest_memory_ptr(0x0018ac6au), sizeof lim);
    if (speed < (double)lim) {
        guest_fn_9e508((int32_t)EF_U32(0x0021448au) >> 16, 4u);
    } else {
        guest_fn_9e508((int32_t)EF_U32(0x0021448au) >> 16, 5u);
        EF_U8(0x00132ac2u) = 0u;
    }
    if (EF_U32(0x002143e8u) == 0u)
        EF_U32(0x002143e8u) = 0xfau;
    result = guest_fn_9783c_ret_x(u, 4u);
    return result;
}

/* 0x4e0d0, 306 bytes. Pointer click X (EAX), Y (EDX; both signed words) on the current panel slot (byte 0x1a559f): slot state 5 ignores the click; state 8 clears the pending flag 0x1a5708. The slot state 0x1a5672[slot] becomes 0x12 and, unless the flag is already 1, the click must hit the cursor box (centre words 0x1a54fe / 0x1a5500, half size = float 0x184d47 / float 0x1a53df); a hit stores the centre words and the dword 0x1a5502 for the slot (0x1a55a2, 0x1a55d6, 0x1a560a), sets the state to 8 and the flag to 1. */
void click_panel_slot_4e0d0(uint32_t x, uint32_t y)
{
    uint32_t slot = EF_U8(0x001a559fu);
    uint32_t state = EF_U8(0x001a5672u + slot);
    float hw, den;
    double cx, cy, px, py;
    if (state == 5u)
        return;
    if (state == 8u)
        EF_U8(0x001a5708u) = 0u;
    EF_U8(0x001a5672u + EF_U8(0x001a559fu)) = 0x12u;
    if (EF_U8(0x001a5708u) == 1u)
        return;
    memcpy(&hw, ef_guest_memory_ptr(0x00184d47u), sizeof hw);
    memcpy(&den, ef_guest_memory_ptr(0x001a53dfu), sizeof den);
    hw = hw / den;
    cx = (double)(int16_t)EF_U16(0x001a54feu);
    px = (double)(float)(int16_t)x;
    if (cx - (double)hw > px)
        return;
    if (cx + (double)hw < px)
        return;
    cy = (double)(int16_t)EF_U16(0x001a5500u);
    py = (double)(float)(int16_t)y;
    if (cy - (double)hw > py)
        return;
    if (cy + (double)hw < py)
        return;
    slot = EF_U8(0x001a559fu);
    EF_U16(0x001a55a2u + slot * 2u) = EF_U16(0x001a54feu);
    slot = EF_U8(0x001a559fu);
    EF_U16(0x001a55d6u + slot * 2u) = EF_U16(0x001a5500u);
    slot = EF_U8(0x001a559fu);
    EF_U32(0x001a560au + slot * 4u) = EF_U32(0x001a5502u);
    slot = EF_U8(0x001a559fu);
    EF_U8(0x001a5672u + slot) = 8u;
    EF_U8(0x001a5708u) = 1u;
}

/* 0x53aec, 289 bytes. Third drift mode step. Convert the three signed words 0x1a5bda, 0x1a5bce, 0x1a5bc2 (high halves) with 0x53308, add the floats 0x1a5ba8, 0x1a5ba4, 0x1a5ba0 and pass the three sums to 0x53350. Then, with dt = word 0x127d92, advance the position floats 0x1a5c00, 0x1a5bb8, 0x1a5bbc by velocity * dt, damp the velocity floats 0x1a5ba0, 0x1a5ba4, 0x1a5ba8 by velocity * float 0x185b7c * dt and finally add the 0x52eb0(10.0) * dt * float 0x185b7c squared term to 0x1a5ba4. */
void integrate_drift_53aec(void)
{
    float damp, t1, t2, t3, fv;
    double v1, v2, v3, dt, r;
    uint32_t b1, b3;
    float *p0 = (float *)ef_guest_memory_ptr(0x001a5ba0u);
    float *p1 = (float *)ef_guest_memory_ptr(0x001a5ba4u);
    float *p2 = (float *)ef_guest_memory_ptr(0x001a5ba8u);
    float *q0 = (float *)ef_guest_memory_ptr(0x001a5c00u);
    float *q1 = (float *)ef_guest_memory_ptr(0x001a5bb8u);
    float *q2 = (float *)ef_guest_memory_ptr(0x001a5bbcu);
    v1 = convert_signed_short_scale((int16_t)((int32_t)EF_U32(0x001a5bdau) >> 16));
    t1 = (float)(v1 + (double)*p2);
    v2 = convert_signed_short_scale((int16_t)((int32_t)EF_U32(0x001a5bceu) >> 16));
    t2 = (float)(v2 + (double)*p1);
    v3 = convert_signed_short_scale((int16_t)((int32_t)EF_U32(0x001a5bc2u) >> 16));
    t3 = (float)(v3 + (double)*p0);
    memcpy(&b1, &t1, sizeof b1);
    memcpy(&b3, &t3, sizeof b3);
    guest_fn_53350_w(0u, 0u, 0u, 0u, b3, t2, b1);
    dt = (double)(int32_t)EF_U16(0x00127d92u);
    *q0 = (float)((double)*p0 * dt + (double)*q0);
    *q1 = (float)((double)*p1 * dt + (double)*q1);
    *q2 = (float)((double)*p2 * dt + (double)*q2);
    memcpy(&damp, ef_guest_memory_ptr(0x00185b7cu), sizeof damp);
    *p0 = (float)((double)*p0 - ((double)*p0 * (double)damp) * dt);
    *p1 = (float)((double)*p1 - ((double)*p1 * (double)damp) * dt);
    *p2 = (float)((double)*p2 - ((double)*p2 * (double)damp) * dt);
    r = scale_float_52eb0(10.0f);
    dt = (double)(int32_t)EF_U16(0x00127d92u);
    fv = (float)(((r * dt) * (double)damp) * (double)damp + (double)*p1);
    *p1 = fv;
}

/* 0x67d4, 315 bytes. Measure the selected unit's range and bearing to the view point. Clears the result dword 0x19721c to -1, looks up the unit named by the high half of 0x1f71c8 and returns when it does not exist. The target point (two dwords) comes from 0x5f1ec in mode 2, from the record at [0x1a821c] (>> 8) in mode 6 and from the entry pair 0x84e3c otherwise. The range is the approximate length 0xb3988 to the point at 0x1a5be8/0x1a5bec, scaled by 3/2048, and the bearing is word 0x19724a minus word 0x127d3e. The truncated range goes to 0x19722c, the sine-table row (word table at 0x142e08, index bearing >> 3) times [0x1a3048]/32768 to float 0x1a357c and the range to float 0x1a356c; when that row value exceeds 10.0 the dword 0x19721c is set to range * 1854 / row. */
void measure_view_range_67d4(void)
{
    uint32_t pt[2];
    uint32_t unit, rec, mode, flag;
    int32_t dx, dy, idx;
    uint16_t ang;
    double range, row, k;
    float f;
    int32_t hi = (int32_t)EF_U32(0x001f71c8u) >> 16;

    EF_U32(0x0019721cu) = 0xffffffffu;
    unit = guest_fn_7489c(hi);
    if (unit == 0u)
        return;
    pt[0] = 0u;
    pt[1] = 0u;   /* ORIGINAL BUG: mode 6 with a null record leaves the point uninitialised */
    mode = EF_U8(0x0012814cu);
    if (mode == 2u) {
        sample_active_coordinates((ef_addr32)(uintptr_t)pt);
    } else if (mode == 6u) {
        rec = EF_U32(0x001a821cu);
        if (rec != 0u) {
            pt[0] = (uint32_t)((int32_t)EF_U32(rec) >> 8);
            pt[1] = (uint32_t)((int32_t)EF_U32(rec + 8u) >> 8);
        }
    } else {
        idx = (int32_t)EF_U32(0x0019bb5cu) >> 16;
        flag = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(idx * 0xca) + 0x8eu);
        fetch_entry_pair_84e3c((ef_addr32)(uintptr_t)pt, (uint32_t)((int32_t)EF_U32(0x001f71c8u) >> 16), flag);
    }
    dy = (int32_t)pt[1] - (int32_t)EF_U32(0x001a5be8u);
    dx = (int32_t)pt[0] - (int32_t)EF_U32(0x001a5becu);
    range = (double)approx_length_b3988((uint32_t)dx, (uint32_t)dy) * 3.0 * 0.00048828125;
    ang = (uint16_t)(EF_U16(0x0019724au) - EF_U16(0x00127d3eu));
    EF_U32(0x0019722cu) = (uint32_t)(int32_t)truncate_9d72c(range);
    row = (double)(int16_t)EF_U16(0x00142e08u + (uint32_t)(ang >> 3) * 2u);
    memcpy(&k, ef_guest_memory_ptr(0x001a3048u), sizeof k);
    row = row * k * 3.0517578125e-05;
    f = (float)row;
    memcpy(ef_guest_memory_ptr(0x001a357cu), &f, sizeof f);
    f = (float)range;
    memcpy(ef_guest_memory_ptr(0x001a356cu), &f, sizeof f);
    if (row > 10.0)
        EF_U32(0x0019721cu) = (uint32_t)(int32_t)truncate_9d72c(range * 1854.0 / row);
}

/* 0x1e28c, 311 bytes. Walk the list at [0x137988] (next link at +4) and set flag byte 0x137a00. For every node read the plane cell for (+0x16, +0x1a) through 0x7b2f4; when 0xcd150 rejects it, report both values through the error hook with format 0x181a18. Then for each of the node's entries (count = high half of +0x28, 0x15-byte records from pointer +0x2c) other than kinds 0xff, 0xfe, 0x38 and 0x37 that the entry boxes test 0x1e214 accepts, compare the value of 0xcd258 with 100 minus the entry's byte +5 (capped at 100); when they differ add the difference to the progress counter (0xcd2e8) and select page layout 0x9dfc0 for the cell. */
void refresh_node_progress_1e28c(void)
{
    uint32_t node, esi, cell;
    int32_t i, count;
    int16_t w, cur, want, cnt;
    uint32_t b;

    node = EF_U32(0x00137988u);
    EF_U8(0x00137a00u) = 1u;
    while (node != 0u) {
        w = (int16_t)guest_fn_7b2f4(0u, (int32_t)EF_U32(node + 0x16u), (int32_t)EF_U32(node + 0x1au));
        if (guest_fn_cd150((int32_t)w) == 0u)
            guest_callptr3_x(EF_U32(0x00132a74u), 0x00181a18u, EF_U32(node + 0x16u), EF_U32(node + 0x1au));
        esi = EF_U32(node + 0x2cu);
        for (i = 0; ; i++, esi += 0x15u) {
            count = (int32_t)EF_U32(node + 0x28u) >> 16;
            if (i >= count)
                break;
            b = EF_U8(esi);
            if (b == 0xffu || b == 0xfeu || b == 0x38u || b == 0x37u)
                continue;
            cell = (uint32_t)(int32_t)w;
            if (guest_fn_1e214((int32_t)cell, (int32_t)EF_U32(esi + 5u) >> 16, (int32_t)EF_U32(esi + 9u) >> 16,
                               (int32_t)EF_U32(esi + 7u) >> 16) == 0u)
                continue;
            cur = (int16_t)guest_fn_cd258();
            cnt = (int16_t)EF_U8(esi + 5u);
            if (cnt >= 100)
                cnt = 100;
            want = (int16_t)(100 - cnt);
            if (want == cur)
                continue;
            guest_fn_cd2e8((int32_t)(int16_t)(want - cur));
            select_page_layout_9dfc0(w, 0u, 0u, 0u);
        }
        node = EF_U32(node + 4u);
    }
}

/* 0x29dd0, 386 bytes. Per-unit damage tick for unit AX (signed). Does nothing for a missing unit. A unit without a target (+0x1c) with a nonzero counter (word +0x1d8) raises event 0x88664 and decrements the counter; with a zero counter and byte 0x41f833 equal to 0x74 it triggers sound 0x42. With a target and a nonzero counter: when the larger axis distance (x or z, dwords +0 and +8) to the target exceeds 0x2b40d5, raise the event with 0 and return without decrementing; otherwise compute a level 0..100, either from the byte table at 0x1264e5 (5-byte rows indexed by byte 0x1264e2) when 0x7574c accepts the unit or as 10.0 minus the double at record +0x128637 (0xe5-byte rows, row from the high half of 0x126302, capped at 9.0), raise the event with that level and decrement the counter. */
void unit_damage_tick_29dd0(uint32_t ax)
{
    uint32_t unit, target, dist, row;
    int32_t dx, dz;
    int16_t level;
    double d;

    unit = guest_fn_7489c((int32_t)(int16_t)ax);
    if (unit == 0u)
        return;
    target = EF_U32(unit + 0x1cu);
    if (target == 0u) {
        if (EF_U16(unit + 0x1d8u) != 0u) {
            unit_event_88664(unit, 0u);
            EF_U16(unit + 0x1d8u) = (uint16_t)(EF_U16(unit + 0x1d8u) - 1u);
        } else if (EF_U8(0x0041f833u) == 0x74u) {
            trigger_sound_8160c(0x42u);
        }
        return;
    }
    if (EF_U16(unit + 0x1d8u) == 0u)
        return;
    dx = (int32_t)(EF_U32(target) - EF_U32(unit));
    dz = (int32_t)(EF_U32(target + 8u) - EF_U32(unit + 8u));
    if (abs_cf94a(dx) > abs_cf94a(dz))
        dist = abs_cf94a(dx);
    else
        dist = abs_cf94a(dz);
    if ((int32_t)dist > 0x2b40d5) {
        unit_event_88664(unit, 0u);
        return;
    }
    if (object_uses_secondary_grid((int16_t)ax)) {
        level = (int16_t)EF_U8(0x001264e5u + EF_U8(0x001264e2u) * 5u);
    } else {
        row = (uint32_t)(((int32_t)EF_U32(0x00126302u) >> 16) * 0xe5);
        memcpy(&d, ef_guest_memory_ptr(0x00128637u + row), sizeof d);
        if (d > 9.0)
            d = 9.0;
        level = (int16_t)(int32_t)truncate_9d72c(10.0 - d);
    }
    if (level < 0)
        level = 0;
    if (level > 100)
        level = 100;
    unit_event_88664(unit, (uint32_t)(uint16_t)level);
    EF_U16(unit + 0x1d8u) = (uint16_t)(EF_U16(unit + 0x1d8u) - 1u);
}

/* 0x9a348, 400 bytes. Clear the 0x40d00x state block: words 0x40d008, 0x40d00a and 0x40d00c, 24 bytes at 0x40d00e, nineteen 6-byte records between 0x40cf90 and 0x40d002, then words 0x40d02c, 0x40d026, 0x40d02a and 0x40d02e. */
void clear_state_block_9a348(void)
{
	static const uint32_t rec[19] = {
		0x0040cfb4u, 0x0040cfbau, 0x0040cfc0u, 0x0040cfc6u, 0x0040cfccu, 0x0040cfd2u, 0x0040cf90u, 0x0040cf96u, 0x0040cf9cu, 0x0040cfa2u, 0x0040cfa8u, 0x0040cfaeu, 0x0040cfdeu, 0x0040cfe4u, 0x0040cfeau, 0x0040cff0u, 0x0040cff6u, 0x0040cffcu, 0x0040d002u
	};
	int i;

	EF_U16(0x0040d008u) = 0u;
	EF_U16(0x0040d00au) = 0u;
	EF_U16(0x0040d00cu) = 0u;
	fill_bytes_b3fb0(0x0040d00eu, 0u, 24u);
	for (i = 0; i < 19; i++)
		fill_bytes_b3fb0(rec[i], 0u, 6u);
	EF_U16(0x0040d02cu) = 0u;
	EF_U16(0x0040d026u) = 0u;
	EF_U16(0x0040d02au) = 0u;
	EF_U16(0x0040d02eu) = 0u;
}

/* 0x74228, 393 bytes. Transform the origin of record EAX by an attitude from the scene table. The attitude is five words found through the scene group named 0x186ba4 (or 0x186bb8 when dword +0x12 is nonzero) and the word key at +0x20 (0 or 0xffff means none); a missing entry falls back to the words at 0x12853c. With dword +0x12 equal to zero every word is halved. The words build a rotation matrix (0xd0b68 clears the 36-byte record, 0xd0ebc and 0xd0dbc apply words 4 and 3), the first three words form a vector that the 3x3 matrix maps, and each result is divided by 0x6900 and added to record dwords +0, +4 and +8; the sums go to 0x21443c, 0x214440 and 0x214444. */
void transform_by_attitude_74228(ef_addr32 rec)
{
	uint32_t mat_u[12];
	uint8_t *mat = (uint8_t *)mat_u;
	int16_t w[5];
	int32_t m[9];
	int32_t w0, w1, w2;
	uint32_t grp, ent;
	uint16_t key;
	int i;

	grp = guest_fn_a1a40_key(EF_U32(rec + 0x12u) == 0u ? 0x00186ba4u : 0x00186bb8u);
	ent = 0u;
	if (grp != 0u) {
		key = EF_U16(rec + 0x20u);
		if (key != 0u && key != 0xffffu)
			ent = find_scene_named_entry((int16_t)((int32_t)EF_U32(rec + 0x1eu) >> 16), grp);
	}
	if (ent == 0u)
		ent = 0x0012853cu;
	for (i = 0; i < 5; i++)
		w[i] = (int16_t)EF_U16(ent + (uint32_t)i * 2u);
	if (EF_U32(rec + 0x12u) == 0u) {
		for (i = 0; i < 5; i++)
			w[i] = (int16_t)(w[i] >> 1);
	}
	initialize_scene_angle_record((ef_addr32)(uintptr_t)mat);
	update_scene_angle_first((ef_addr32)(uintptr_t)mat, (uint16_t)w[4]);
	update_scene_angle_second((ef_addr32)(uintptr_t)mat, (uint16_t)w[3]);
	memcpy(m, mat, sizeof m);
	w0 = w[0];
	w1 = w[1];
	w2 = w[2];
	EF_U32(0x0021443cu) = (uint32_t)((m[0] * w0 + m[1] * w1 + m[2] * w2) / 0x6900 + (int32_t)EF_U32(rec));
	EF_U32(0x00214440u) = (uint32_t)((m[3] * w0 + m[4] * w1 + m[5] * w2) / 0x6900 + (int32_t)EF_U32(rec + 4u));
	EF_U32(0x00214444u) = (uint32_t)((m[6] * w0 + m[7] * w1 + m[8] * w2) / 0x6900 + (int32_t)EF_U32(rec + 8u));
}

/* 0x44aec, 379 bytes. Draw the markers for every node of the list whose head pointer sits at EDX (next link at +4). Nodes with byte +0xb nonzero are skipped. A marker position (a screen address, 0x126570 + row * 0x280 + column) is taken from the screen-position pairs at 0x134443/0x134445, 0x13443a/0x13443c or 0x13444c/0x13444e when the group flag tests 0xcc22c, 0xcc1ac or 0xcc1ec of the node's byte +9 (against EAX) hold and the matching enable word (0x126554, 0x126556, 0x126558) is nonzero; the pair at 0x134740/0x134742 is used as well when 0xbc7ec accepts the node. With a position the frame is drawn at (word +0x16 - 10, 0x190 - word +0x1a - 10) with 0xca620 and, when word 0x12656a is nonzero, the node's byte +9 sprite is drawn through 0x440f4. */
void draw_node_markers_44aec(uint32_t group, ef_addr32 head_addr)
{
	uint32_t node, pos = 0u;
	int32_t x, y;
	int have;

	for (node = EF_U32(head_addr); node != 0u; node = EF_U32(node + 4u)) {
		if (EF_U8(node + 0xbu) != 0u)
			continue;
		have = 0;
		if ((guest_fn_cc22c(0u, EF_U8(node + 9u), group) & 0xffffu) != 0u && EF_U16(0x00126554u) != 0u) {
			pos = EF_U32(0x00126570u) + (uint32_t)(((int32_t)EF_U32(0x00134445u) >> 16) * 0x280)
				+ (uint32_t)((int32_t)EF_U32(0x00134443u) >> 16);
			have = 1;
		} else if ((guest_fn_cc1ac(0u, EF_U8(node + 9u), group) & 0xffffu) != 0u && EF_U16(0x00126556u) != 0u) {
			pos = EF_U32(0x00126570u) + (uint32_t)(((int32_t)EF_U32(0x0013443cu) >> 16) * 0x280)
				+ (uint32_t)((int32_t)EF_U32(0x0013443au) >> 16);
			have = 1;
		} else if ((guest_fn_cc1ec_x(0u, EF_U8(node + 9u), group) & 0xffffu) != 0u && EF_U16(0x00126558u) != 0u) {
			pos = EF_U32(0x00126570u) + (uint32_t)(((int32_t)EF_U32(0x0013444eu) >> 16) * 0x280)
				+ (uint32_t)((int32_t)EF_U32(0x0013444cu) >> 16);
			have = 1;
		}
		if ((guest_fn_bc7ec(node) & 0xffffu) != 0u) {
			pos = EF_U32(0x00126570u) + (uint32_t)(((int32_t)EF_U32(0x00134742u) >> 16) * 0x280)
				+ (uint32_t)((int32_t)EF_U32(0x00134740u) >> 16);
			have = 1;
		}
		if (!have)
			continue;
		y = (int16_t)(0x190 - EF_U16(node + 0x1au) - 10);
		x = (int16_t)EF_U16(node + 0x16u) - 10;
		guest_fn_ca620((int32_t)pos, x, y, 0x14u, 0x14u, 0x280u);
		if (EF_U16(0x0012656au) != 0u)
			guest_fn_440f4_x(EF_U8(node + 9u), x, y, 0x14u, 0x1au, 0x280u);
	}
}

/* 0x7cf64, 465 bytes. Read six button states from the dialog named 0x187f94 (bit 0 of byte +0x1d of the widgets named 0x188048 = A, 0x187fac = B, 0x188054 = C, 0x18805c = D, 0x188064 = E, 0x187fb0 = F), derive the selection byte 0x12583b from the pairs (last matching pair wins: A+D 9, A+E 7, A+F 8, B+D 3, B+E 1, B+F 2, C+D 6, C+E 4, C+F 5), store it in the entry returned by 0xa7ec4 for key 0x187ee0 and copy the flags to bytes 0x1f9000 (A), 0x1f9001 (B), 0x1f9002 (C), 0x1f9005 (D), 0x1f9003 (E) and 0x1f8fff (F). */
void read_button_selection_7cf64(void)
{
	static const uint32_t names[6] = { 0x00188048u, 0x00187facu, 0x00188054u, 0x0018805cu, 0x00188064u, 0x00187fb0u };
	uint32_t dlg = guest_fn_a46cc(0x00187f94u);
	uint16_t f[6];
	int i;

	for (i = 0; i < 6; i++)
		f[i] = (EF_U8(find_widget_checked_a78bc(dlg, names[i]) + 0x1du) & 1u) ? 1u : 0u;
	if (f[0] && f[3]) EF_U8(0x0012583bu) = 9u;
	if (f[0] && f[4]) EF_U8(0x0012583bu) = 7u;
	if (f[0] && f[5]) EF_U8(0x0012583bu) = 8u;
	if (f[1] && f[3]) EF_U8(0x0012583bu) = 3u;
	if (f[1] && f[4]) EF_U8(0x0012583bu) = 1u;
	if (f[1] && f[5]) EF_U8(0x0012583bu) = 2u;
	if (f[2] && f[3]) EF_U8(0x0012583bu) = 6u;
	if (f[2] && f[4]) EF_U8(0x0012583bu) = 4u;
	if (f[2] && f[5]) EF_U8(0x0012583bu) = 5u;
	EF_U32(guest_fn_a7ec4(0x00187ee0u)) = EF_U8(0x0012583bu);
	EF_U8(0x001f9005u) = (uint8_t)f[3];
	EF_U8(0x001f8fffu) = (uint8_t)f[5];
	EF_U8(0x001f9000u) = (uint8_t)f[0];
	EF_U8(0x001f9001u) = (uint8_t)f[1];
	EF_U8(0x001f9003u) = (uint8_t)f[4];
	EF_U8(0x001f9002u) = (uint8_t)f[2];
}

/* 0x1c16c, 609 bytes. Pack the low bits of two groups of key-state bytes into the bit sets at 0x2144aa and 0x2144ab. Byte 0x2144aa takes bit 0 of the bytes 0x41f805, 0x41f808, 0x41f809, 0x41f801, 0x41f7d7, 0x41f7df, 0x41f7d9 and 0x41f7e1 (bits 0..7) when word 0x19843c is nonzero and the pair probe 0x913dc (0xff, 2) fails, otherwise it is cleared. When byte 0x41f7ad is set the low six bits of 0x2144ab are replaced by bit 0 of 0x41f7db, 0x41f7dd, 0x41f7e1, 0x41f7d9, 0x41f7d8 and 0x41f7e0, otherwise they are cleared; bits 6 and 7 are kept. */
void pack_key_state_bits_1c16c(void)
{
	static const uint32_t a[8] = { 0x0041f805u, 0x0041f808u, 0x0041f809u, 0x0041f801u,
		0x0041f7d7u, 0x0041f7dfu, 0x0041f7d9u, 0x0041f7e1u };
	static const uint32_t b[6] = { 0x0041f7dbu, 0x0041f7ddu, 0x0041f7e1u, 0x0041f7d9u, 0x0041f7d8u, 0x0041f7e0u };
	uint8_t v;
	int i;

	if (EF_U16(0x0019843cu) != 0u && probe_scene_pair_state(0xff, 2, 0, 0) == 0u) {	/* EBX/ECX are not set by the original */
		v = EF_U8(0x002144aau);
		for (i = 0; i < 8; i++)
			v = (uint8_t)((v & ~(1u << i)) | ((EF_U8(a[i]) & 1u) << i));
		EF_U8(0x002144aau) = v;
	} else {
		EF_U8(0x002144aau) = 0u;
	}
	v = EF_U8(0x002144abu);
	if (EF_U8(0x0041f7adu) != 0u) {
		for (i = 0; i < 6; i++)
			v = (uint8_t)((v & ~(1u << i)) | ((EF_U8(b[i]) & 1u) << i));
	} else {
		v = (uint8_t)(v & 0xc0u);
	}
	EF_U8(0x002144abu) = v;
}

/* 0x2aa34, 285 bytes. Populate the slots of class AX (signed). Clears byte +0x4f of the class record (0xca-byte rows at [0x19bb58]) and, for each of DL slots, builds a name in a 40-byte buffer initialised from the data at 0x2a9e0 (for DL > 1: slot 0 gets the text 0x12f44d, the others the string at [0x12684c + 4 * slot]), upper-cases it and looks the slot value up as the word at [STACK + 2 * slot] (the slot number itself without a table). With a record array ECX the slot is passed to 0x8c49c with the record's fields (dword +0, high halves of +0xe and +0x10); otherwise two random values reduced modulo 5 are passed instead, together with the name buffer. BL is the byte passed on to every call. */
void populate_slots_2aa34(uint32_t ax, uint32_t dl, uint32_t bl, ef_addr32 recs, ef_addr32 table)
{
	uint32_t buf_u[10];
	uint8_t *buf = (uint8_t *)buf_u;
	int16_t idx = (int16_t)ax;
	int16_t si;
	int16_t w;
	uint32_t row, t, rem1, rem2;

	memcpy(buf_u, ef_guest_memory_ptr(0x0002a9e0u), sizeof buf_u);
	EF_U8(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)idx * 0xca) + 0x4fu) = 0u;
	for (si = 0; si < (int16_t)(dl & 0xffu); si++) {
		if ((dl & 0xffu) > 1u) {
			if (si == 0)
				guest_sprintf_a4342((ef_addr32)(uintptr_t)buf, 0x0012f44du);
			else
				guest_fn_9d749((ef_addr32)(uintptr_t)buf, EF_U32(0x0012684cu + (uint32_t)si * 4u));
		}
		uppercase_string((ef_addr32)(uintptr_t)buf);
		w = table != 0u ? (int16_t)EF_U16(table + (uint32_t)(int32_t)si * 2u) : si;
		if (recs != 0u) {
			row = recs + (uint32_t)((int32_t)si * 0x16);
			guest_fn_8c49c_x((uint32_t)(int32_t)idx, EF_U32(row), bl & 0xffu,
				(uint32_t)((int32_t)EF_U32(row + 0xeu) >> 16),
				(uint32_t)((int32_t)EF_U32(row + 0x10u) >> 16), (uint32_t)(int32_t)w);
		} else {
			t = guest_random_word();
			rem1 = (uint32_t)(int32_t)(int16_t)(t % 5u);
			t = guest_random_word();
			rem2 = (uint32_t)(int32_t)(int16_t)(t % 5u);
			guest_fn_8c49c_x((uint32_t)(int32_t)idx, (uint32_t)(uintptr_t)buf, bl & 0xffu, rem2, rem1, (uint32_t)(int32_t)w);
		}
	}
}

/* 0xac5f8, 391 bytes. Main frame step of the front-end state machine (AL = loop flag). Leaves a pending redraw state (byte 0x41f2e4), then repeats until a result code is produced or, with AL clear, once: move the pending key byte 0x41f833 to 0x41f2e6; when word 0x41eeda is set update the record 0x41f284 (0xacb68, 0xf8248 -> dword 0x41eea4, 0xacbe4, 0xacfb8, 0xf1c58) and take the result of 0xf18a4; handle the pending key (0x58 runs the save-slot search 0xac57c when bytes 0x41f7ba or 0x41f7c6 are set; 0xf1a70 supplies the result when none exists yet); apply the selection (0xf1d54), relabel (0xabc34), 0xa6038, run the entry callbacks (0xabde4) and 0xac780 when flagged; redraw (0xc9fc4) and let byte 0x41f2e8 override the result. With AL set the engine is reset (0xe4264) at the end and result 0x3e7 reports the fatal text 0x18dc54. Returns the result code. */
uint32_t run_frame_step_ac5f8(uint32_t al)
{
	uint32_t result = 0u, t;
	uint8_t loop = (uint8_t)al;

	EF_U32(0x0041eea0u) = 0u;
	if (EF_U8(0x0041f2e4u) != 0u) {
		tail_jump_c9ec4();
		guest_fn_d663c(0x004195d0u);
		redraw_ca17c();
		EF_U8(0x0041f2e4u) = 0u;
	}
	EF_U8(0x0041f2e8u) = 0u;
	if (loop != 0u)
		EF_U8(0x0041f833u) = 0u;
	EF_U8(0x0041f2e6u) = 0u;
	do {
		if (EF_U8(0x0041f833u) != 0u) {
			EF_U8(0x0041f2e6u) = EF_U8(0x0041f833u);
			EF_U8(0x0041f833u) = 0u;
		}
		result = 0u;
		EF_U16(0x0041eed2u) = 0u;
		if (EF_U16(0x0041eedau) != 0u) {
			latch_previous_acb68(0x0041f284u);
			EF_U32(0x0041eea4u) = guest_fn_f8248();
			guest_fn_acbe4_x(0x0041f284u);
			guest_fn_acfb8_x(0x0041f284u);
			switch_active_object_f1c58(0x0041f284u);
			result = guest_fn_f18a4_x(0x0041f284u);
		}
		if (EF_U8(0x0041f2e6u) != 0u) {
			if (EF_U8(0x0041f2e6u) == 0x58u && ((EF_U8(0x0041f7bau) | EF_U8(0x0041f7c6u)) != 0u))
				find_free_save_slot_ac57c();
			t = guest_fn_f1a70_x();
			if (result == 0u)
				result = t;
			EF_U8(0x0041f2e6u) = 0u;
		}
		if (EF_U16(0x0041eedau) != 0u) {
			apply_selection_f1d54();
			if (EF_U8(0x0041f2e3u) != 0u)
				relabel_named_widget_abc34();
			if (EF_U8(0x0013727cu) != 0u)
				guest_fn_a6038();
		}
		if (EF_U8(0x0041f2deu) == 0u)
			run_entry_callbacks_abde4();
		if (EF_U16(0x0041eed2u) != 0u)
			guest_fn_ac780_x();
		guest_fn_c9fc4();
		if (EF_U8(0x0041f2e8u) != 0u)
			result = EF_U8(0x0041f2e8u);
		EF_U8(0x0041f2e4u) = 0u;
	} while (result == 0u && loop != 0u);
	if (loop != 0u)
		reset_engine_e4264();
	if (result == 0x3e7u)
		report_fatal_ac7c8(0x0018dc54u);
	return result;
}

/* 0xac408, 372 bytes. Show the text EAX in the front-end: error hook 0x18dc14 when byte 0x137254 is clear, then reset the display state (0xabec4, 0xc8fdc, 0xc90c0, 0xc8e38, 0xa4e6c), run the page callbacks for 0x18dc40 (0xac240) and 0xaca94. An empty text runs the callbacks for each of the up to sixteen 0x15-byte records at 0x41eab0 whose first byte is nonzero. Otherwise (clearing the block 0xabe24 first when byte 0x41f2e7 is set) the text is split into words at characters whose class byte (table 0x195af0 indexed by char + 1) has bit 1 set and every word is passed to the callbacks together with 0x41f2e7. Finally restore the display (0xc8f38, 0xc90e0, 0xc8e58), set byte 0x13727d, clear dword 0x41eea0 and the 0x59-byte record 0x41f284 and word 0x41eec2, then drain the event queue (0xce064) and clear the pending key bytes. ORIGINAL BUG: the word buffer is only 12 bytes; a longer word overruns it (a larger buffer is used here). */
void show_text_ac408(ef_addr32 text)
{
	char word[256];
	uint32_t i, n;
	uint8_t c;

	if (EF_U8(0x00137254u) == 0u)
		guest_callptr1_x(EF_U32(0x00132a74u), 0x0018dc14u);
	guest_tail_abec4(0u);
	clear_byte_c8fdc();
	clear_byte_c90c0();
	clear_byte_c8e38();
	clear_id_table_a4e6c();
	run_page_callbacks_ac240(0x0018dc40u, 0u);
	guest_fn_aca94_w();
	if (EF_U8(text) == 0u) {
		for (i = 0; i < 0x10u && EF_U8(0x0041eab0u + i * 0x15u) != 0u; i++)
			run_page_callbacks_ac240(0x0041eab0u + i * 0x15u, 0u);
	} else {
		if (EF_U8(0x0041f2e7u) != 0u)
			clear_block_abe24();
		while (EF_U8(text) != 0u) {
			n = 0u;
			while ((c = EF_U8(text + n)) != 0u && (EF_U8(0x00195af0u + (uint8_t)(c + 1u)) & 2u) == 0u) {
				if (n < sizeof word - 1u)
					word[n] = (char)c;
				n++;
			}
			word[n < sizeof word - 1u ? n : sizeof word - 1u] = 0;
			run_page_callbacks_ac240((ef_addr32)(uintptr_t)word, EF_U8(0x0041f2e7u));
			text += n;
			while ((EF_U8(0x00195af0u + (uint8_t)(EF_U8(text) + 1u)) & 2u) != 0u)
				text++;
		}
	}
	guest_fn_c8f38_x();
	guest_fn_c90e0_x();
	guest_fn_c8e58_x();
	EF_U8(0x0013727du) = 1u;
	EF_U32(0x0041eea0u) = 0u;
	guest_fn_c9ecc_x();
	fill_bytes_b3fb0(0x0041f284u, 0u, 0x59u);
	EF_U16(0x0041eec2u) = 0u;
	guest_fn_ac9e8_x();
	EF_U8(0x0041f2e0u) = 0u;
	if (EF_U8(0x0041f2e3u) != 0u)
		guest_fn_abc98_x();
	EF_U8(0x0041f833u) = 0u;
	EF_U8(0x0041f2e6u) = 0u;
	while ((guest_fn_ce064_x() & 0xffffu) != 0u)
		;
}

/* 0xf260, 180 bytes. Parse one record from the text parser into the record EAX: skip the line and token, byte +0xb from the enum reader (0xe9bc), byte +0xc from the 106-name enum reader (0xec8c), dword +0xd a duplicated token, words +0x9 and +0x23 and the double +0x27 (converted from the decimal reader) and byte +0x2f from integers, then the six-field record at +0x11 after a new line; after another line and token read the entry count into word +0x48, allocate nine bytes per entry through the allocation hook at 0x121ebc into dword +0x4a and read each nine-byte entry (0xee0c). Returns 1. */
uint32_t parse_record_f260(ef_addr32 rec)
{
	double d;
	int32_t n, i;
	uint32_t base;

	read_parser_line();
	read_parser_token();
	EF_U8(rec + 0xbu) = (uint8_t)read_parser_enum();
	EF_U8(rec + 0xcu) = (uint8_t)read_parser_enum_106_names();
	EF_U32(rec + 0xdu) = duplicate_parser_token();
	EF_U16(rec + 0x9u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x23u) = (uint16_t)read_parser_integer();
	d = (double)(int32_t)read_parser_decimal();
	memcpy(ef_guest_memory_ptr(rec + 0x27u), &d, sizeof d);
	EF_U8(rec + 0x2fu) = (uint8_t)read_parser_integer();
	read_parser_line();
	read_parser_six_field_record(rec + 0x11u);
	read_parser_line();
	read_parser_token();
	EF_U16(rec + 0x48u) = (uint16_t)read_parser_integer();
	n = (int32_t)EF_U32(rec + 0x46u) >> 16;
	EF_U32(rec + 0x4au) = guest_callptr_eax_ret_x(EF_U32(0x00121ebcu), (uint32_t)(n * 9));
	base = 0u;
	for (i = 0; i < ((int32_t)EF_U32(rec + 0x46u) >> 16); i++, base += 9u)
		read_record_ee0c(EF_U32(rec + 0x4au) + base);
	return 1u;
}

/* 0xfcf4, 343 bytes. Parse one record from the text parser into the record EAX: skip line and token, byte +0xb from the two-family enum reader (0xea0c) selects the name reader for byte +0xc (0 the 39-name, 1 the 18-name, 2 the 27-name list; anything else reports the unknown name through the error hook, format 0x18113c with 0x197abe and dword 0x121eac), byte +0xd the 10-name enum, byte +0xe an enum, dword +0xf a duplicated token, then integers: word +0x13 (overwritten by the running counter word 0x121ea6), bytes +0x15 and +0x16, words +0x17, +0x29, +0x2b, +0x2d, +0x2f, +0x19, +0x1b, +0x1d, +0x1f, +0x21, +0x23, +0x25, +0x27, three reads into +0x31 (the last wins) and +0x51. Word +0x53 is cleared, the text record whose pointer is dword +0x4d is parsed (0xfe4c) and the six-field record at +0x3b is read. Returns 1. */
uint32_t parse_record_fcf4(ef_addr32 rec)
{
	uint32_t kind;
	uint16_t counter;

	read_parser_line();
	read_parser_token();
	kind = read_parser_enum_two_families() & 0xffu;
	EF_U8(rec + 0xbu) = (uint8_t)kind;
	switch (kind) {
	case 0:
		EF_U8(rec + 0xcu) = (uint8_t)read_parser_enum_39_names();
		break;
	case 1:
		EF_U8(rec + 0xcu) = (uint8_t)read_parser_enum_18_names();
		break;
	case 2:
		EF_U8(rec + 0xcu) = (uint8_t)read_parser_enum_27_names();
		break;
	default:
		guest_callptr3_x(EF_U32(0x00132a74u), 0x0018113cu, 0x00197abeu, EF_U32(0x00121eacu));
		break;
	}
	EF_U8(rec + 0xdu) = (uint8_t)read_parser_enum_10_names();
	EF_U8(rec + 0xeu) = (uint8_t)read_parser_enum();
	EF_U32(rec + 0xfu) = duplicate_parser_token();
	EF_U16(rec + 0x13u) = (uint16_t)read_parser_integer();
	counter = EF_U16(0x00121ea6u);
	EF_U16(rec + 0x13u) = counter;
	EF_U16(0x00121ea6u) = (uint16_t)(counter + 1u);
	EF_U8(rec + 0x15u) = (uint8_t)read_parser_integer();
	EF_U8(rec + 0x16u) = (uint8_t)read_parser_integer();
	EF_U16(rec + 0x17u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x29u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x2bu) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x2du) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x2fu) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x19u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x1bu) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x1du) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x1fu) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x21u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x23u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x25u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x27u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x31u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x31u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x31u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x51u) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x53u) = 0u;
	read_parser_text_record(EF_U32(rec + 0x4du));
	read_parser_six_field_record(rec + 0x3bu);
	return 1u;
}

/* 0xf314, 749 bytes. Parse one component record from the text parser into the record EAX (DL = parent kind byte). Byte +1 is the count; byte +0 is a name read with the 39-, 18- or 27-name reader according to the family word 0x197aba (0, 1, 2; the matching running total word 0x197ab4, 0x197ab8 or 0x197ab6 grows by the count; other families report through the error hook, format 0x180f74). Bytes +2 and +3 come from the enum readers 0xec3c and 0xece0. Family 0 reads twenty enum bytes into +0x55..+0x68 (0xeea0). Then the six-field record at +8, words +0x1c, +0x1a and +0x1e (the first must be nonzero, format 0x180fac, and the third below the first, format 0x180fd4), and word-count (+0x1c) 25-byte entries allocated through the hook at 0x121ebc into +0x20 (0xed80). After skipping tokens words +0x6e and +0x70 follow. Kinds 2, 10 and 4 with a parent of 6, 7 or 10 read byte +0x3a, a token copy in +0x3b, the six-field record at +0x3f and, except for kind 5, count entries of 21 bytes into +0x51 (0xeefc); otherwise +0x3b and +0x51 are cleared. Kinds 2, 10, 1 and 11 get a 7-byte group list in +0x74 (0xf08c). With family 0 and byte 0x121eb8 set, count 22-byte text records are read into +0x69 (0xfe4c). Dword +0x78 is cleared. Returns 1. */
uint32_t parse_component_record_f314(ef_addr32 rec, uint32_t parent)
{
	uint32_t fam, kind, i, n, p;
	int16_t cnt, lim;

	read_parser_line();
	read_parser_token();
	EF_U8(rec + 1u) = (uint8_t)read_parser_integer();
	fam = EF_U16(0x00197abau);
	if (fam == 0u) {
		EF_U16(0x00197ab4u) = (uint16_t)(EF_U16(0x00197ab4u) + EF_U8(rec + 1u));
		EF_U8(rec) = (uint8_t)read_parser_enum_39_names();
	} else if (fam == 1u) {
		EF_U16(0x00197ab8u) = (uint16_t)(EF_U16(0x00197ab8u) + EF_U8(rec + 1u));
		EF_U8(rec) = (uint8_t)read_parser_enum_18_names();
	} else if (fam == 2u) {
		EF_U16(0x00197ab6u) = (uint16_t)(EF_U16(0x00197ab6u) + EF_U8(rec + 1u));
		EF_U8(rec) = (uint8_t)read_parser_enum_27_names();
	} else {
		guest_callptr3_x(EF_U32(0x00132a74u), 0x00180f74u, 0x00197abeu, EF_U32(0x00121eacu));
	}
	EF_U8(rec + 2u) = (uint8_t)parse_enum_ec3c();
	EF_U8(rec + 3u) = (uint8_t)read_parser_enum_10_names();
	if (EF_U16(0x00197abau) == 0u) {
		for (p = rec + 1u; p != rec + 0x14u; ) {
			p++;
			EF_U8(p + 0x54u) = (uint8_t)parse_enum_eea0();
		}
	}
	read_parser_line();
	read_parser_six_field_record(rec + 8u);
	read_parser_line();
	read_parser_token();
	EF_U16(rec + 0x1cu) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x1au) = (uint16_t)read_parser_integer();
	EF_U16(rec + 0x1eu) = (uint16_t)read_parser_integer();
	if (EF_U16(rec + 0x1cu) == 0u)
		guest_callptr2_x(EF_U32(0x00132a74u), 0x00180facu, EF_U32(0x00121eacu));
	lim = (int16_t)EF_U16(rec + 0x1cu);
	if ((int16_t)EF_U16(rec + 0x1eu) >= lim)
		guest_callptr2_x(EF_U32(0x00132a74u), 0x00180fd4u, EF_U32(0x00121eacu));
	cnt = (int16_t)EF_U16(rec + 0x1cu);
	EF_U32(rec + 0x20u) = guest_callptr_eax_ret_x(EF_U32(0x00121ebcu), (uint32_t)((int32_t)cnt * 25));
	for (i = 0, n = 0u; (int32_t)i < (int16_t)EF_U16(rec + 0x1cu); i++, n += 0x19u)
		read_record_ed80(EF_U32(rec + 0x20u) + n);
	read_parser_line();
	read_parser_token();
	read_parser_token();
	EF_U16(rec + 0x6eu) = (uint16_t)read_parser_integer();
	read_parser_token();
	EF_U16(rec + 0x70u) = (uint16_t)read_parser_integer();
	kind = EF_U8(rec + 2u);
	if ((kind == 2u || kind == 10u || kind == 4u) && ((parent & 0xffu) == 6u || (parent & 0xffu) == 7u || (parent & 0xffu) == 10u)) {
		read_parser_line();
		read_parser_token();
		EF_U8(rec + 0x3au) = (uint8_t)read_parser_enum_106_names();
		EF_U32(rec + 0x3bu) = duplicate_parser_token();
		read_parser_line();
		read_parser_six_field_record(rec + 0x3fu);
		if (EF_U8(rec + 2u) == 5u) {
			EF_U32(rec + 0x51u) = 0u;
		} else {
			EF_U32(rec + 0x51u) = guest_callptr_eax_ret_x(EF_U32(0x00121ebcu), (uint32_t)EF_U8(rec + 1u) * 21u);
			for (i = 0, n = 0u; i < EF_U8(rec + 1u); i++, n += 0x15u)
				read_record_eefc(EF_U32(rec + 0x51u) + n);
		}
	} else {
		EF_U32(rec + 0x3bu) = 0u;
		EF_U32(rec + 0x51u) = 0u;
	}
	kind = EF_U8(rec + 2u);
	if (kind == 2u || kind == 10u || kind == 1u || kind == 11u) {
		EF_U32(rec + 0x74u) = guest_callptr_eax_ret_x(EF_U32(0x00121ebcu), 7u);
		read_group_list_f08c(EF_U32(rec + 0x74u));
	} else {
		EF_U32(rec + 0x74u) = 0u;
	}
	if (EF_U16(0x00197abau) == 0u && EF_U8(0x00121eb8u) != 0u) {
		EF_U32(rec + 0x69u) = guest_callptr_eax_ret_x(EF_U32(0x00121ebcu), (uint32_t)EF_U8(rec + 1u) * 22u);
		for (i = 0, n = 0u; i < EF_U8(rec + 1u); i++, n += 0x16u)
			read_parser_text_record(EF_U32(rec + 0x69u) + n);
	} else {
		EF_U32(rec + 0x69u) = 0u;
	}
	EF_U32(rec + 0x78u) = 0u;
	return 1u;
}

/* 0xf6a8, 591 bytes. Load a text data file into the scene: EAX and EBX are guest pointers to words that receive the header and record counts, EDX and ECX are the owner arguments for the header and 0x53-byte records, the four stack words (s0..s3) are guest pointers receiving four integers and the last stack argument (ret 0x14) is the file name. Sets the allocation hook 0x121ebc to 0xe744, clears 0x121eac and the running totals 0x197ab4/0x197ab8/0x197ab6, checks the name through the size hook at 0x132ab4 (error format 0x180ffc) and opens it (0x9ccb8, error format 0x181028; handle in 0x197ab0). Then reads the header count into word EAX, flag 0x121eb8 (first keyword equals 0x181054), four integers into the stack-word destinations, optionally (second keyword equals 0x181064) the header list through 0xf198 with hook 0xe764, otherwise clears 0x1379c4. Each header record (0x1b bytes, 0xf604) is attached with 0xbc570; the second count (word EBX) 0x53-byte records parsed with 0xf260 and attached the same way (error formats 0x181078 and 0x18109c for failed allocations); a final count (word 0x1a5a8e) allocates sixteen-byte entries at 0x1a5904 filled by 0xef5c. The file is closed (0x9ce84). Returns 1. */
uint32_t load_data_file_f6a8(ef_addr32 count_a, ef_addr32 owner_a, ef_addr32 count_b, ef_addr32 owner_b,
	ef_addr32 dst0, ef_addr32 dst1, ef_addr32 dst2, ef_addr32 dst3, ef_addr32 name)
{
	uint32_t state_a[2] = { 0u, 0u };
	uint32_t i, p;

	EF_U32(0x00121ebcu) = 0x0000e744u;
	EF_U32(0x00121eacu) = 0u;
	EF_U16(0x00197ab4u) = 0u;
	EF_U16(0x00197ab8u) = 0u;
	EF_U16(0x00197ab6u) = 0u;
	if (guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), name) == 0u)
		guest_callptr2_x(EF_U32(0x00132a74u), 0x00180ffcu, name);
	EF_U32(0x00197ab0u) = open_resource_9ccb8(name);
	if (EF_U32(0x00197ab0u) == 0xffffffffu)
		guest_callptr2_x(EF_U32(0x00132a74u), 0x00181028u, name);
	initialize_parser_records();
	read_parser_line();
	read_parser_token();
	EF_U16(count_a) = (uint16_t)read_parser_integer();
	read_parser_token();
	EF_U8(0x00121eb8u) = guest_fn_a4380(0x00197abeu, 0x00181054u) == 0u ? 1u : 0u;
	read_parser_line();
	read_parser_token();
	EF_U16(dst0) = (uint16_t)read_parser_integer();
	EF_U16(dst1) = (uint16_t)read_parser_integer();
	EF_U16(dst2) = (uint16_t)read_parser_integer();
	EF_U16(dst3) = (uint16_t)read_parser_integer();
	read_parser_token();
	release_parser_buffers();
	if (guest_fn_a4380(0x00197abeu, 0x00181064u) == 0u) {
		EF_U32(0x00121ebcu) = 0x0000e764u;
		read_header_list_f198();
		EF_U32(0x00121ebcu) = 0x0000e744u;
	} else {
		EF_U32(0x001379c4u) = 0u;
	}
	for (i = 0; (int32_t)i < (int16_t)EF_U16(count_a); i++) {
		p = guest_callptr_eax_ret_x(EF_U32(0x00121ebcu), 0x1bu);
		read_record_header_f604(p);
		if (p != 0u)
			guest_fn_bc570_x(p, owner_a, (uint32_t)(uintptr_t)&state_a[1]);
		else
			guest_callptr1_x(EF_U32(0x00132a74u), 0x00181078u);
	}
	read_parser_line();
	read_parser_token();
	EF_U16(count_b) = (uint16_t)read_parser_integer();
	for (i = 0; (int32_t)i < (int16_t)EF_U16(count_b); i++) {
		p = guest_callptr_eax_ret_x(EF_U32(0x00121ebcu), 0x53u);
		parse_record_f260(p);
		if (p != 0u)
			guest_fn_bc570_x(p, owner_b, (uint32_t)(uintptr_t)&state_a[0]);
		else
			guest_callptr1_x(EF_U32(0x00132a74u), 0x0018109cu);
	}
	read_parser_line();
	EF_U32(0x001a5904u) = 0u;
	read_parser_token();
	EF_U16(0x001a5a8eu) = (uint16_t)read_parser_integer();
	if ((int16_t)EF_U16(0x001a5a8eu) > 0) {
		EF_U32(0x001a5904u) = guest_callptr_eax_ret_x(EF_U32(0x00121ebcu), (uint32_t)(int32_t)(int16_t)EF_U16(0x001a5a8eu) << 4);
		for (i = 0; (int32_t)i < (int16_t)EF_U16(0x001a5a8eu); i++)
			fill_random_ef5c(EF_U32(0x001a5904u) + i * 16u);
	}
	guest_fn_9ce84(EF_U32(0x00197ab0u));
	return 1u;
}

/* 0x3527c, 682 bytes. Process the received packet list: the byte at [0x136b78] gives the packet count and the bytes at [0x136b74] index 32-byte descriptors (header pointer at 0x135878, payload pointer at 0x13587c, header word +7 is the type and word +9 the payload length). Byte 0x19f598 is set for each packet. Type 5 runs 0x35218, type 7 runs 0x34d94(0); type 4 with state byte 0x19f5a1 equal to 1 or 2 and byte 0x19f5a5 set sets byte 0x19f5a9 to 9. In state 2 a packet of the expected type (word 0x19f586) copies its payload to the buffer 0x19f370 (more than 0x1f6 bytes reports the error text 0x182f54), sets 0x19f5a5 and 0x19f5a9 = 7. In state 1 type 1 with expected type 1 sets 0x19f5a9 = 3, dword 0x19f344 from the payload and byte 0x19f59f; expected type 2 also sets 0x19f5a9 = 3; a type 2 packet with expected type 2 and a destination buffer at 0x19f354 is a numbered fragment: the payload byte 0 holds the fragment number (bits 0..5), a first-fragment flag (bit 6) and a last flag (bit 7), the sequence word is at payload +1 and compared with word 0x19f592; a matching fragment not yet seen (flag bytes at 0x19f310) is copied to destination + number * (high half of dword 0x1258fe), counters 0x19f584 and 0x19f33c are updated and, when all fragments have arrived, 0x19f592 advances and 0x19f59f (and, for the last-flag, 0x19f5a5) are set; an older sequence with the first-fragment flag sets 0x19f5a9 = 7. */
void process_packets_3527c(void)
{
	uint32_t esi, count, idx, hdr, pay, type, st, b0, n, bit6, last, seq, e, off, len, dst;

	count = EF_U8(EF_U32(0x00136b78u));
	for (esi = 0; (int32_t)esi < (int32_t)count; esi++) {
		EF_U8(0x0019f598u) = 1u;
		idx = EF_U8(EF_U32(0x00136b74u) + esi);
		hdr = EF_U32(0x00135878u + idx * 32u);
		pay = EF_U32(0x0013587cu + idx * 32u);
		type = EF_U16(hdr + 7u);
		if (type == 5u) {
			guest_fn_35218();
			continue;
		}
		if (type == 7u) {
			guest_fn_34d94(0u);
			continue;
		}
		st = EF_U8(0x0019f5a1u);
		if (type == 4u) {
			if ((st == 2u || st == 1u) && EF_U8(0x0019f5a5u) != 0u)
				EF_U8(0x0019f5a9u) = 9u;
			continue;
		}
		if (st == 2u) {
			if ((uint16_t)type != EF_U16(0x0019f586u))
				continue;
			if (EF_U32(0x0019f340u) == 0u) {
				EF_U16(0x0019f58au) = EF_U16(hdr + 9u);
				EF_U32(0x0019f340u) = 0x0019f370u;
				if (EF_U16(0x0019f58au) > 0x1f6u)
					guest_callptr1_x(EF_U32(0x00132a74u), 0x00182f54u);
				memcpy_dc832(EF_U32(0x0019f340u), pay, EF_U16(0x0019f58au));
				EF_U8(0x0019f5a5u) = 1u;
			}
			EF_U8(0x0019f5a9u) = 7u;
		} else if (st == 1u) {
			if (type == 1u) {
				if (EF_U16(0x0019f586u) == 1u) {
					EF_U8(0x0019f5a9u) = 3u;
					EF_U32(0x0019f344u) = EF_U32(pay);
					EF_U8(0x0019f59fu) = 1u;
				}
				if (EF_U16(0x0019f586u) == 2u)
					EF_U8(0x0019f5a9u) = 3u;
			}
			if (type != 2u || EF_U16(0x0019f586u) != 2u || EF_U32(0x0019f354u) == 0u)
				continue;
			b0 = EF_U8(pay);
			n = b0 & 0x3fu;
			bit6 = (b0 >> 6) & 1u;
			last = b0 >> 7;
			seq = EF_U16(pay + 1u);
			EF_U16(0x0019f590u) = (uint16_t)seq;
			if (seq != EF_U16(0x0019f592u)) {
				if (seq < EF_U16(0x0019f592u) && bit6 != 0u)
					EF_U8(0x0019f5a9u) = 7u;
				continue;
			}
			if (bit6 != 0u)
				EF_U16(0x0019f588u) = (uint16_t)(n + 1u);
			if (EF_U8(0x0019f310u + n) == 1u)
				continue;
			e = (uint32_t)((int32_t)EF_U32(0x001258feu) >> 16);
			EF_U8(0x0019f310u + n) = 1u;
			off = n * e;
			len = EF_U16(hdr + 9u) - 3u;
			dst = off + EF_U32(0x0019f354u);
			EF_U16(0x0019f584u) = (uint16_t)(EF_U16(0x0019f584u) + 1u);
			memcpy_dc832(dst, pay + 3u, len);
			EF_U32(0x0019f33cu) += (uint32_t)(EF_U16(hdr + 9u) - 3u);
			if (EF_U16(0x0019f584u) < EF_U16(0x0019f588u))
				continue;
			EF_U16(0x0019f592u) = (uint16_t)(EF_U16(0x0019f592u) + 1u);
			EF_U8(0x0019f59fu) = 1u;
			if (last != 0u)
				EF_U8(0x0019f5a5u) = 1u;
		}
	}
}

/* 0x367f0, 369 bytes. One step of the connection state machine in byte 0x19f5a9 (states 1..11, the dispatch is a jump table in the code segment). 1 sets state 4; 4 and 8 recompute the total (0xa4030) and process the received packets (0x3527c); 6 does the same and moves to state 8; 7 sends the key request for byte 0x19f5a2 (0x35220) and moves to 6; 9 starts the retry timer (state 10, ten retries in word 0x19f58e, clock into dword 0x19f338, random delay 18 + random % 10 in dword 0x19f328) and sends request 0x46; 10 recomputes the total and, once the delay has elapsed, restarts the timer, sends request 0x46 again and counts the retries down to state 11; 11 resets state, byte 0x19f5a1 and copies byte 0x19f5a0 to 0x19f5a7. When the state or byte 0x19f598 changed the state widget is refreshed (0x36340). */
void step_connection_367f0(void)
{
	uint8_t state = EF_U8(0x0019f5a9u);
	uint8_t flag = EF_U8(0x0019f598u);
	uint32_t r;

	switch (state) {
	case 1:
		EF_U8(0x0019f5a9u) = 4u;
		break;
	case 4:
	case 8:
		recompute_total_a4030();
		process_packets_3527c();
		break;
	case 6:
		recompute_total_a4030();
		process_packets_3527c();
		EF_U8(0x0019f5a9u) = 8u;
		break;
	case 7:
		send_key_request_35220(EF_U8(0x0019f5a2u));
		EF_U8(0x0019f5a9u) = 6u;
		break;
	case 9:
		EF_U8(0x0019f5a9u) = 0xau;
		EF_U16(0x0019f58eu) = 10u;
		EF_U32(0x0019f338u) = guest_read_runtime_tick();
		r = guest_fn_a45b3() & 0xffffu;
		EF_U32(0x0019f328u) = r % 10u + 0x12u;
		send_key_request_35220(0x46u);
		break;
	case 10:
		recompute_total_a4030();
		if (guest_read_runtime_tick() - EF_U32(0x0019f338u) > EF_U32(0x0019f328u)) {
			EF_U32(0x0019f338u) = guest_read_runtime_tick();
			r = guest_fn_a45b3() & 0xffffu;
			EF_U32(0x0019f328u) = r % 10u + 0x12u;
			send_key_request_35220(0x46u);
			EF_U16(0x0019f58eu) = (uint16_t)(EF_U16(0x0019f58eu) - 1u);
			if (EF_U16(0x0019f58eu) == 0u)
				EF_U8(0x0019f5a9u) = 0xbu;
		}
		break;
	case 11:
		EF_U8(0x0019f5a9u) = 0u;
		EF_U8(0x0019f5a1u) = 0u;
		EF_U8(0x0019f5a7u) = EF_U8(0x0019f5a0u);
		break;
	default:
		break;
	}
	if (state != EF_U8(0x0019f5a9u) || flag != EF_U8(0x0019f598u))
		enable_state_widget_36340();
}

/* 0x36f84, 406 bytes. Setup-screen state machine in byte 0x19f5a8. Updates the frame delta (0xa4138), stores word 0x1357ac - 3 in word 0x125900, then runs 0x36438 when byte 0x19f5af is 1, 0x36d30 when it is 2 and otherwise steps the state: 0 -> 1; 1 queues request 0x36100 (0x12590e, 0x13, 0x44, 5, 2, 0); 2 packs the settings record (0x35c08); 3 goes to 4 when byte 0x125846 is 3, to 5 when the name at 0x125828 equals the text 0x182f7c and to 6 otherwise; 4 opens the stream 0x183260 (volume 5); 5 opens the stream named at 0x125828 (volume 6); 6 refreshes the widgets (0xaca88, 0xa4c84), applies the text 0x182f94 (byte 0x125846 equal to 3) or 0x182fa0 (0xa4d90) and sets byte 0x41f2e8. When the state changed the label widget (key 0x182fa8 in the dialog named 0x1830f8) is bound to the state text (0x130fde, 0x130ff1, none, 0x13101f, 0x13100a, 0x130fa8 for states 1..6). */
void step_setup_screen_36f84(void)
{
	uint8_t saved = EF_U8(0x0019f5a8u);
	uint32_t dlg, widget, text;

	update_frame_delta_a4138();
	EF_U16(0x00125900u) = (uint16_t)(EF_U16(0x001357acu) - 3u);
	if (EF_U8(0x0019f5afu) == 1u) {
		guest_fn_36438_x();
	} else if (EF_U8(0x0019f5afu) == 2u) {
		guest_fn_36d30_x();
	} else {
		switch (EF_U8(0x0019f5a8u)) {
		case 0:
			EF_U8(0x0019f5a8u) = 1u;
			break;
		case 1:
			queue_request_36100(0x0012590eu, 0x13u, 0x44u, 5u, 2u, 0u);
			break;
		case 2:
			pack_settings_record_35c08();
			break;
		case 3:
			if (EF_U8(0x00125846u) == 3u)
				EF_U8(0x0019f5a8u) = 4u;
			else if (guest_fn_a4380(0x00125828u, 0x00182f7cu) == 0u)
				EF_U8(0x0019f5a8u) = 5u;
			else
				EF_U8(0x0019f5a8u) = 6u;
			break;
		case 4:
			open_stream_36054(0x00183260u, 5u);
			break;
		case 5:
			open_stream_36054(0x00125828u, 6u);
			break;
		case 6:
			guest_fn_aca88_w();
			refresh_all_widgets_a4c84();
			guest_fn_a4d90(EF_U8(0x00125846u) == 3u ? 0x00182f94u : 0x00182fa0u);
			EF_U8(0x0041f2e8u) = 1u;
			break;
		default:
			break;
		}
	}
	if (saved != EF_U8(0x0019f5a8u)) {
		dlg = guest_fn_a46cc(0x001830f8u);
		widget = find_widget_checked_a78bc(dlg, 0x00182fa8u);
		if (widget != 0u) {
			switch (EF_U8(0x0019f5a8u)) {
			case 1: text = 0x00130fdeu; break;
			case 2: text = 0x00130ff1u; break;
			case 4: text = 0x0013101fu; break;
			case 5: text = 0x0013100au; break;
			case 6: text = 0x00130fa8u; break;
			default: return;
			}
			guest_bind_dialog_widget(widget, text);
		}
	}
}

/* 0x35590, 438 bytes. Menu-screen state machine in byte 0x19f5a7, the client twin of 0x36f84. Stores word 0x1357ac - 3 in word 0x125900, updates the frame delta (0xa4138) and clears bytes 0x19f598 and 0x19f599, then runs 0x36ad8 when byte 0x19f5a1 is 1, the connection step 0x367f0 when it is 2 and otherwise steps the state: 0 -> 1 and shows the status (0x34a38); 1 arms timer 0x45 for 6 ticks with flag 2 (0x35748); 2 unpacks the settings at dword 0x19f340 (0x3519c), commits the mode selection (0x35b2c), mirrors the reference state (0x37294), sets byte 0x19f5ab and goes to 3 (byte 0x125846 equal to 3), 4 (name 0x125828 equals the text 0x182f7c) or 5; 3 opens stream 0x182f88 (mode 4) and 4 opens the stream named at 0x125828 (mode 5) with 0x369fc; 5 refreshes the widgets (0xaca88, 0xa4c84), applies the text 0x182f94 or 0x182fa0 (0xa4d90), sets byte 0x41f2e8 and runs 0xabe44 on 0x182f2c. When the state changed the label widget (key 0x182fa8 in the dialog named 0x182fb0) is bound to the state text (0x130f5c, 0x130f78, 0x131030, 0x130f92, 0x130fa8 for states 1..5). */
void step_menu_screen_35590(void)
{
	uint8_t saved = EF_U8(0x0019f5a7u);
	uint32_t dlg, widget, text;

	EF_U16(0x00125900u) = (uint16_t)(EF_U16(0x001357acu) - 3u);
	update_frame_delta_a4138();
	EF_U8(0x0019f598u) = 0u;
	EF_U8(0x0019f599u) = 0u;
	if (EF_U8(0x0019f5a1u) == 1u) {
		guest_fn_36ad8_x();
	} else if (EF_U8(0x0019f5a1u) == 2u) {
		step_connection_367f0();
	} else {
		switch (EF_U8(0x0019f5a7u)) {
		case 0:
			EF_U8(0x0019f5a7u) = 1u;
			show_status_34a38();
			break;
		case 1:
			arm_timer_record_35748(0x45u, 6u, 2u);
			break;
		case 2:
			unpack_settings_3519c(EF_U32(0x0019f340u));
			commit_mode_selection_35b2c();
			mirror_reference_state();
			EF_U8(0x0019f5abu) = 1u;
			if (EF_U8(0x00125846u) == 3u)
				EF_U8(0x0019f5a7u) = 3u;
			else if (guest_fn_a4380(0x00125828u, 0x00182f7cu) == 0u)
				EF_U8(0x0019f5a7u) = 4u;
			else
				EF_U8(0x0019f5a7u) = 5u;
			break;
		case 3:
			open_stream_369fc(0x00182f88u, 4u);
			break;
		case 4:
			open_stream_369fc(0x00125828u, 5u);
			break;
		case 5:
			guest_fn_aca88_w();
			refresh_all_widgets_a4c84();
			guest_fn_a4d90(EF_U8(0x00125846u) == 3u ? 0x00182f94u : 0x00182fa0u);
			EF_U8(0x0041f2e8u) = 1u;
			guest_fn_abe44(0x00182f2cu);
			break;
		default:
			break;
		}
	}
	if (saved != EF_U8(0x0019f5a7u)) {
		dlg = guest_fn_a46cc(0x00182fb0u);
		widget = find_widget_checked_a78bc(dlg, 0x00182fa8u);
		if (widget != 0u) {
			switch (EF_U8(0x0019f5a7u)) {
			case 1: text = 0x00130f5cu; break;
			case 2: text = 0x00130f78u; break;
			case 3: text = 0x00131030u; break;
			case 4: text = 0x00130f92u; break;
			case 5: text = 0x00130fa8u; break;
			default: return;
			}
			guest_bind_dialog_widget(widget, text);
		}
	}
}

/* 0x36ad8, 564 bytes. Transfer-side connection state machine in byte 0x19f5a9 (states 2..11 through a jump table; 6 and 8 do nothing). 2 recomputes the total (0xa4030), processes the packets (0x3527c) and moves to 3 once byte 0x19f59f is set; 3 sends request 0x47 and moves to 5; 4 recomputes, processes the packets and, when byte 0x19f59f is exactly 1, moves to 7, writes the received block to the handle in dword 0x19f35c (0xa7a1c; length dword 0x19f33c, buffer dword 0x19f354), adds the length to dword 0x19f32c, resets the transfer state (0x35528) and rescales the progress widgets (0x35de0) by 0x19f32c / dword 0x19f344; 5 resets the transfer state, clears word 0x19f592, goes to 4 and sets word 0x19f586 = 2; 7 sends request 0x48 and moves to 4; 9 clears the progress (0x35de0 with 0), closes the handle in dword 0x19f35c (0xa7990) when byte 0x19f59e is set, then starts the retry timer exactly as 0x367f0; 10 is the retry step of 0x367f0; 11 resets the state, byte 0x19f5a1, copies 0x19f5a0 to 0x19f5a7 and refreshes the buttons (0x369ac). The state widget is refreshed (0x36340) when the state or byte 0x19f598 changed. */
void step_transfer_36ad8(void)
{
	uint8_t state = EF_U8(0x0019f5a9u);
	uint8_t flag = EF_U8(0x0019f598u);
	uint32_t r;
	double ratio, d;

	switch (state) {
	case 2:
		recompute_total_a4030();
		process_packets_3527c();
		if (EF_U8(0x0019f59fu) != 0u)
			EF_U8(0x0019f5a9u) = 3u;
		break;
	case 3:
		send_key_request_35220(0x47u);
		EF_U8(0x0019f5a9u) = 5u;
		break;
	case 4:
		recompute_total_a4030();
		process_packets_3527c();
		if (EF_U8(0x0019f59fu) == 1u) {
			EF_U8(0x0019f5a9u) = 7u;
			guest_fn_a7a1c(EF_U32(0x0019f35cu), EF_U32(0x0019f33cu), EF_U32(0x0019f354u));
			EF_U32(0x0019f32cu) += EF_U32(0x0019f33cu);
			reset_state_35528();
			ratio = (double)(int32_t)EF_U32(0x0019f32cu) / (double)(int32_t)EF_U32(0x0019f344u);
			rescale_dialog_widgets_35de0(0u, 0u, 0u, 0u, ratio);
		}
		break;
	case 5:
		reset_state_35528();
		EF_U16(0x0019f592u) = 0u;
		EF_U8(0x0019f5a9u) = 4u;
		EF_U16(0x0019f586u) = 2u;
		break;
	case 7:
		send_key_request_35220(0x48u);
		EF_U8(0x0019f5a9u) = 4u;
		break;
	case 9:
		d = 0.0;
		rescale_dialog_widgets_35de0(0u, 0u, 0u, 0u, d);
		if (EF_U8(0x0019f59eu) != 0u) {
			EF_U8(0x0019f59eu) = 0u;
			guest_fn_a7990(EF_U32(0x0019f35cu));
		}
		EF_U8(0x0019f5a9u) = 0xau;
		EF_U16(0x0019f58eu) = 10u;
		EF_U32(0x0019f338u) = guest_read_runtime_tick();
		r = guest_fn_a45b3() & 0xffffu;
		EF_U32(0x0019f328u) = r % 10u + 0x12u;
		send_key_request_35220(0x46u);
		break;
	case 10:
		recompute_total_a4030();
		if (guest_read_runtime_tick() - EF_U32(0x0019f338u) > EF_U32(0x0019f328u)) {
			EF_U32(0x0019f338u) = guest_read_runtime_tick();
			r = guest_fn_a45b3() & 0xffffu;
			EF_U32(0x0019f328u) = r % 10u + 0x12u;
			send_key_request_35220(0x46u);
			EF_U16(0x0019f58eu) = (uint16_t)(EF_U16(0x0019f58eu) - 1u);
			if (EF_U16(0x0019f58eu) == 0u)
				EF_U8(0x0019f5a9u) = 0xbu;
		}
		break;
	case 11:
		EF_U8(0x0019f5a9u) = 0u;
		EF_U8(0x0019f5a1u) = 0u;
		EF_U8(0x0019f5a7u) = EF_U8(0x0019f5a0u);
		set_buttons_369ac();
		break;
	default:
		break;
	}
	if (state != EF_U8(0x0019f5a9u) || flag != EF_U8(0x0019f598u))
		enable_state_widget_36340();
}

/* 0x36d30, 543 bytes. Host-lobby state machine in byte 0x19f5aa (states 1..9 through a jump table; 2, 3 and 4 do nothing). 1 -> 5; 5 marks the slot (0x35cc8), moves to 6 and issues the request in dword 0x19f34c with the size in word 0x19f350 (0xa3eac between idle callbacks 0xa41a0) and stamps the clock into dword 0x19f338; 6 recomputes the total (0xa4030), marks the matching slots (0x35f04) and, while the count word 0x19f58e is below the high half of dword 0x1357ae, waits 16 ticks before incrementing dword 0x19f358 and moving to 7, otherwise syncs the list (0x363c4) and moves to 8; 7 re-issues the request, stamps the clock, syncs the list and returns to 6; 8 with byte 0x19f5ad set sends the 11-byte request at 0x125903 (word 0x1357b2 = 4, byte 0x19f5a6 = 0x46, counter word 0x19f596 cleared) and moves to 9, otherwise ends the lobby (state 0, byte 0x19f5af cleared, byte 0x19f5a8 = byte 0x19f5ac); 9 resends the request every 16 ticks until byte 0x12583a players have joined or the counter exceeds 30, then ends the lobby the same way. When the state changed the state widget is refreshed (0x362c4). */
void step_lobby_36d30(void)
{
	uint8_t state = EF_U8(0x0019f5aau);
	int32_t limit;

	switch (state) {
	case 1:
		EF_U8(0x0019f5aau) = 5u;
		break;
	case 5:
		mark_slot_35cc8();
		EF_U8(0x0019f5aau) = 6u;
		run_idle_callback_a41a0();
		guest_fn_a3eac_args(EF_U32(0x0019f34cu), EF_U16(0x0019f350u));
		run_idle_callback_a41a0();
		EF_U32(0x0019f338u) = guest_read_runtime_tick();
		break;
	case 6:
		recompute_total_a4030();
		mark_matching_slots_35f04();
		limit = (int32_t)EF_U32(0x001357aeu) >> 16;
		if ((int32_t)EF_U16(0x0019f58eu) < limit) {
			if (guest_read_runtime_tick() - EF_U32(0x0019f338u) > 0xfu) {
				EF_U32(0x0019f358u) += 1u;
				EF_U8(0x0019f5aau) = 7u;
			}
		} else {
			sync_list_363c4();
			EF_U8(0x0019f5aau) = 8u;
		}
		break;
	case 7:
		run_idle_callback_a41a0();
		guest_fn_a3eac_args(EF_U32(0x0019f34cu), EF_U16(0x0019f350u));
		run_idle_callback_a41a0();
		EF_U32(0x0019f338u) = guest_read_runtime_tick();
		sync_list_363c4();
		EF_U8(0x0019f5aau) = 6u;
		break;
	case 8:
		if (EF_U8(0x0019f5adu) != 0u) {
			mark_slot_35cc8();
			EF_U16(0x001357b2u) = 4u;
			EF_U8(0x0019f5a6u) = 0x46u;
			run_idle_callback_a41a0();
			guest_fn_a3eac_args(0x00125903u, 0xbu);
			run_idle_callback_a41a0();
			EF_U32(0x0019f338u) = guest_read_runtime_tick();
			EF_U16(0x0019f596u) = 0u;
			EF_U8(0x0019f5aau) = 9u;
		} else {
			EF_U8(0x0019f5aau) = 0u;
			EF_U8(0x0019f5afu) = 0u;
			EF_U8(0x0019f5a8u) = EF_U8(0x0019f5acu);
		}
		break;
	case 9:
		recompute_total_a4030();
		mark_matching_slots_35f04();
		if ((int32_t)EF_U16(0x0019f58eu) < (int32_t)EF_U8(0x0012583au)) {
			if (guest_read_runtime_tick() - EF_U32(0x0019f338u) > 0xfu) {
				uint16_t old;
				run_idle_callback_a41a0();
				guest_fn_a3eac_args(0x00125903u, 0xbu);
				run_idle_callback_a41a0();
				EF_U32(0x0019f338u) = guest_read_runtime_tick();
				old = EF_U16(0x0019f596u);
				EF_U16(0x0019f596u) = (uint16_t)(old + 1u);
				if (old > 0x1eu) {
					EF_U8(0x0019f5aau) = 0u;
					EF_U8(0x0019f5afu) = 0u;
					EF_U8(0x0019f5a8u) = EF_U8(0x0019f5acu);
				}
			}
		} else {
			EF_U8(0x0019f5aau) = 0u;
			EF_U8(0x0019f5afu) = 0u;
			EF_U8(0x0019f5a8u) = EF_U8(0x0019f5acu);
		}
		break;
	default:
		break;
	}
	if (state != EF_U8(0x0019f5aau))
		enable_state_widget_362c4();
}

/* 0x36438, 905 bytes. Host-side transfer state machine in byte 0x19f5aa (states 2..9 through a jump table). 2 saves dword 0x19f350 in 0x19f324, marks the slot (0x35cc8), sets word 0x1357b2 = 1 and byte 0x19f5a6 = 0x47, sends the 4-byte request at 0x19f324 (0xa3eac between idle callbacks 0xa41a0), stamps the clock into 0x19f338 and moves to 4; 4 recomputes the total (0xa4030) and marks the matching slots (0x35f04) and, while word 0x19f58e is below byte 0x12583a, waits 16 ticks and moves to 3 (else syncs the list 0x363c4 and moves to 5); 3 resends the request, syncs the list and returns to 4; 5 reads the next block from the handle in dword 0x19f360 (0xa79d4, size = high half of dword 0x1258fe * 8, buffer dword 0x19f34c), keeps the length in dword 0x19f368, subtracts it from the remaining count 0x19f350 and sends the packets (0x3617c); 6 waits for the receivers as in 0x36d30 and then either, with nothing left, closes the handle (0xa7990), clears byte 0x19f5a3, frees the buffer (0xe2af4), clears the progress and moves to 8 or increments word 0x19f594, shows the progress (0x35de0, (total - remaining) / total) and returns to 5; 7 resends the packets and syncs; 8 and 9 end the transfer like the lobby states 8 and 9 of 0x36d30 (state 9 also refreshes the buttons 0x36004 when it ends). The state widget is refreshed (0x362c4) when the state changed. */
void step_host_transfer_36438(void)
{
	uint8_t state = EF_U8(0x0019f5aau);
	int32_t remaining;
	uint32_t n;
	uint16_t old;
	double ratio;

	switch (state) {
	case 2:
		EF_U32(0x0019f324u) = EF_U32(0x0019f350u);
		mark_slot_35cc8();
		EF_U16(0x001357b2u) = 1u;
		EF_U8(0x0019f5a6u) = 0x47u;
		EF_U8(0x0019f5aau) = 4u;
		run_idle_callback_a41a0();
		guest_fn_a3eac_args(0x0019f324u, 4u);
		run_idle_callback_a41a0();
		EF_U32(0x0019f338u) = guest_read_runtime_tick();
		break;
	case 4:
		recompute_total_a4030();
		mark_matching_slots_35f04();
		if ((int32_t)EF_U16(0x0019f58eu) < (int32_t)EF_U8(0x0012583au)) {
			if (guest_read_runtime_tick() - EF_U32(0x0019f338u) > 0xfu)
				EF_U8(0x0019f5aau) = 3u;
		} else {
			sync_list_363c4();
			EF_U8(0x0019f5aau) = 5u;
		}
		break;
	case 3:
		run_idle_callback_a41a0();
		guest_fn_a3eac_args(0x0019f324u, 4u);
		run_idle_callback_a41a0();
		EF_U32(0x0019f338u) = guest_read_runtime_tick();
		sync_list_363c4();
		EF_U8(0x0019f5aau) = 4u;
		break;
	case 5:
		EF_U16(0x001357b2u) = 2u;
		n = guest_fn_a79d4_ret(EF_U32(0x0019f360u), (uint32_t)(((int32_t)EF_U32(0x001258feu) >> 16) << 3), EF_U32(0x0019f34cu));
		EF_U32(0x0019f368u) = n;
		EF_U32(0x0019f350u) = EF_U32(0x0019f350u) - n;
		recompute_total_a4030();
		mark_slot_35cc8();
		send_packets_3617c(EF_U32(0x0019f368u), EF_U32(0x0019f350u));
		EF_U32(0x0019f338u) = guest_read_runtime_tick();
		break;
	case 6:
		recompute_total_a4030();
		mark_matching_slots_35f04();
		if ((int32_t)EF_U16(0x0019f58eu) < ((int32_t)EF_U32(0x001357aeu) >> 16)) {
			if (guest_read_runtime_tick() - EF_U32(0x0019f338u) > 0xfu) {
				EF_U8(0x0019f5aau) = 7u;
				EF_U32(0x0019f358u) += 1u;
			}
			break;
		}
		sync_list_363c4();
		remaining = (int32_t)EF_U32(0x0019f350u);
		if (remaining <= 0) {
			guest_fn_a7990(EF_U32(0x0019f360u));
			EF_U8(0x0019f5a3u) = 0u;
			arena_free_e2af4(EF_U32(0x0019f34cu));
			EF_U8(0x0019f5aau) = 8u;
			rescale_dialog_widgets_35de0(0u, 0u, 0u, 0u, 0.0);
		} else {
			ratio = (double)(uint32_t)(EF_U32(0x0019f348u) - (uint32_t)remaining) / (double)EF_U32(0x0019f348u);
			EF_U8(0x0019f5aau) = 5u;
			EF_U16(0x0019f594u) = (uint16_t)(EF_U16(0x0019f594u) + 1u);
			rescale_dialog_widgets_35de0(0u, 0u, 0u, 0u, ratio);
		}
		break;
	case 7:
		send_packets_3617c(EF_U32(0x0019f368u), EF_U32(0x0019f350u));
		sync_list_363c4();
		break;
	case 8:
		if (EF_U8(0x0019f5adu) != 0u) {
			mark_slot_35cc8();
			EF_U16(0x001357b2u) = 4u;
			EF_U8(0x0019f5a6u) = 0x46u;
			run_idle_callback_a41a0();
			guest_fn_a3eac_args(0x00125903u, 0xbu);
			run_idle_callback_a41a0();
			EF_U32(0x0019f338u) = guest_read_runtime_tick();
			EF_U16(0x0019f596u) = 0u;
			EF_U8(0x0019f5aau) = 9u;
		} else {
			EF_U8(0x0019f5aau) = 0u;
			EF_U8(0x0019f5afu) = 0u;
			EF_U8(0x0019f5a8u) = EF_U8(0x0019f5acu);
		}
		break;
	case 9:
		recompute_total_a4030();
		mark_matching_slots_35f04();
		if ((int32_t)EF_U16(0x0019f58eu) < (int32_t)EF_U8(0x0012583au)) {
			if (guest_read_runtime_tick() - EF_U32(0x0019f338u) > 0xfu) {
				run_idle_callback_a41a0();
				guest_fn_a3eac_args(0x00125903u, 0xbu);
				run_idle_callback_a41a0();
				EF_U32(0x0019f338u) = guest_read_runtime_tick();
				old = EF_U16(0x0019f596u);
				EF_U16(0x0019f596u) = (uint16_t)(old + 1u);
				if (old > 0x1eu) {
					EF_U8(0x0019f5aau) = 0u;
					EF_U8(0x0019f5afu) = 0u;
					EF_U8(0x0019f5a8u) = EF_U8(0x0019f5acu);
					set_buttons_36004();
				}
			}
		} else {
			EF_U8(0x0019f5aau) = 0u;
			EF_U8(0x0019f5afu) = 0u;
			EF_U8(0x0019f5a8u) = EF_U8(0x0019f5acu);
			set_buttons_36004();
		}
		break;
	default:
		break;
	}
	if (state != EF_U8(0x0019f5aau))
		enable_state_widget_362c4();
}

/* 0xa312c, 337 bytes. Find or create the session entry for packet EAX (low word; descriptors at 0x135878/0x13587c, the sender name is at payload +6). Scans the entries (pointers at 0x1368bc, count = high half of dword 0x1357ae) that have flag bit 0 set: A is the last index whose name equals the sender name and B the last index for which 0xa3108 forwards (checked only when byte [0x136f04 + byte 0x13579a] is set). An index A above zero that equals B, or with that byte clear, gets flag bits 0 and 2 (values 1 and 2) set and is returned (sign-extended). With neither A nor B found and room left (word 0x1357b4 above the count) a new entry is announced through 0xe9a24 (0x21, name, entry), flagged and word 0x1357b0 is incremented; the old value of that word is returned. Otherwise returns 0xffff. ORIGINAL BUG: index 0 can never be matched because the test is "A > 0". */
uint32_t find_session_entry_a312c(uint32_t packet)
{
	int8_t a = -1, b = -1;
	uint32_t i, ent, name, n, old;
	int32_t cnt;

	packet &= 0xffffu;
	cnt = (int32_t)EF_U32(0x001357aeu) >> 16;
	for (i = 0; (int32_t)(i & 0xffffu) < cnt; i++) {
		ent = EF_U32(0x001368bcu + (i & 0xffffu) * 4u);
		if ((EF_U8(ent + 0x1cu) & 1u) == 0u)
			continue;
		name = EF_U32(0x0013587cu + packet * 32u) + 6u;
		if (guest_fn_a4380(ent, name) == 0u)
			a = (int8_t)i;
		if (EF_U8(0x00136f04u + EF_U8(0x0013579au)) != 0u) {
			if ((forward_if_clear_a3108((uint16_t)i, (uint16_t)packet) & 0xffu) != 0u)
				b = (int8_t)i;
		}
	}
	if (a > 0 && (a == b || EF_U8(0x00136f04u + EF_U8(0x0013579au)) == 0u)) {
		ent = EF_U32(0x001368bcu + (uint32_t)(int32_t)a * 4u);
		EF_U8(ent + 0x1cu) |= 1u;
		EF_U8(ent + 0x1cu) |= 2u;
		return (uint32_t)(int32_t)(int16_t)a & 0xffffu;
	}
	if (a == -1 && b == -1) {
		n = (uint32_t)((int32_t)EF_U32(0x001357aeu) >> 16);
		if ((int32_t)n < (int32_t)EF_U16(0x001357b4u)) {
			ent = EF_U32(0x001368bcu + n * 4u);
			name = EF_U32(0x0013587cu + packet * 32u) + 6u;
			guest_fn_e9a24(0x21u, name, ent);
			n = (uint32_t)((int32_t)EF_U32(0x001357aeu) >> 16);
			EF_U8(EF_U32(0x001368bcu + n * 4u) + 0x1cu) |= 1u;
			EF_U8(EF_U32(0x001368bcu + n * 4u) + 0x1cu) |= 2u;
			old = EF_U16(0x001357b0u);
			EF_U16(0x001357b0u) = (uint16_t)(old + 1u);
			return old;
		}
	}
	return 0xffffu;
}

/* 0xa2f70, 406 bytes. Client-side session discovery step. While byte 0x1357c9 is clear, scan the received packets (count: byte at [0x136b88], indices: bytes at [0x136b84]) and stop at the first announcement (descriptor byte +0xd & 0xf equal to 2) whose payload name at +0xa equals the text 0x13579b and whose first eight bytes equal those at 0x137158: take the descriptor word +0xb into word 0x1357d6, store the clock minus it in dword 0x136dd4, copy payload word +8 to word 0x1357ae, format that entry (0xa2e90), set byte 0x1357c9 and take descriptor byte +5 as word 0x1357b0; a packet of kind 0xb copies its first payload byte to byte 0x13583d. Then, every ten clock ticks (dword 0x136bcc), while no session was found a broadcast is sent (0xe9a24 with 0x21, 0x135840, 0x137136, then request 0x137130, code 0x27, flag 1 through 0xa3e50) and with byte 0x1357c9 equal to 1 the entry scan 0xa2ef4 runs and its result is returned; otherwise returns 0. */
uint32_t discover_session_a2f70(void)
{
	uint32_t i, count, idx, desc, pay, now;
	int found = 0;

	if (EF_U8(0x001357c9u) == 0u) {
		count = EF_U8(EF_U32(0x00136b88u));
		for (i = 0; (i & 0xffffu) < count && !found; i++) {
			idx = EF_U8(EF_U32(0x00136b84u) + (i & 0xffffu));
			desc = EF_U32(0x00135878u + idx * 32u);
			pay = EF_U32(0x0013587cu + idx * 32u);
			if ((EF_U8(desc + 0xdu) & 0xfu) == 2u) {
				if (guest_fn_a4380(pay + 0xau, 0x0013579bu) != 0u)
					continue;
				if (guest_memcmp(pay, 0x00137158u, 8u) != 0)
					continue;
				EF_U16(0x001357d6u) = EF_U16(desc + 0xbu);
				EF_U32(0x00136dd4u) = guest_read_runtime_tick() - EF_U16(0x001357d6u);
				EF_U16(0x001357aeu) = EF_U16(pay + 8u);
				format_entry_a2e90(EF_U16(0x001357aeu));
				EF_U8(0x001357c9u) = 1u;
				found = 1;
				EF_U16(0x001357b0u) = EF_U8(desc + 5u);
			} else if ((EF_U8(desc + 0xdu) & 0xfu) == 0xbu) {
				EF_U8(0x0013583du) = EF_U8(pay);
			}
		}
	}
	now = guest_read_runtime_tick();
	if (now > EF_U32(0x00136bccu)) {
		EF_U32(0x00136bccu) = guest_read_runtime_tick() + 10u;
		if (EF_U8(0x001357c9u) == 0u) {
			guest_fn_e9a24(0x21u, 0x00135840u, 0x00137136u);
			issue_request_a3e50(0x00137130u, 0x27u, 1u);
			return 0u;
		}
		if (EF_U8(0x001357c9u) == 1u)
			return scan_entries_a2ef4() & 0xffu;
	}
	return 0u;
}

/* 0xa2bdc, 621 bytes. Write the multiplayer settings file: open it (0xa7948, text 0x18cc18; handle in dword 0x136df8, failure reported through the error hook with 0x18cc74), write the header/section lines (0x18cc94, 0x18cab0, 0x18ccc4, ...) and the entries with 0xa2a90 (name, pointer, type, extra): text 0x13579b (type 4), byte 0x1357c8 (3, table 0x136f3c), byte 0x13579a (3, table 0x136f0c), 0x1357b6 (1), 0x1357b8 (2), every nonzero dword of the table at 0x13577c (type 5), the byte 0x15f07f incremented around the entry 0x18cb54 and decremented again for 0x15f07d, 0x15f07e (3, table 0x136f3c), 0x15f099 (3, table 0x136f84), 0x15f09a (1), 0x15f4fd (4), 0x15f688 (3, table 0x136f9c), 0x15f53d and 0x15f57d (4). The file is closed (0xa7990). */
void write_settings_file_a2bdc(void)
{
	uint32_t z = 0u;
	uint32_t zp = (uint32_t)(uintptr_t)&z;
	uint32_t h, esi;
	uint8_t v;

	h = guest_fn_a7948(0x0018cc18u);
	EF_U32(0x00136df8u) = h;
	if (h == 0xffffffffu)
		guest_callptr1_x(EF_U32(0x00132a74u), 0x0018cc74u);
	guest_fn_a2a90_x(0x0018cc94u, zp, 0u, 0u);
	guest_fn_a2a90_x(0x0018cab0u, zp, 0u, 0u);
	guest_fn_a2a90_x(0x0018ccc4u, zp, 0u, 0u);
	guest_fn_a2a90_x(0x0018cb28u, 0x0013579bu, 0u, 4u);
	guest_fn_a2a90_x(0x0018cb14u, 0x001357c8u, 0x00136f3cu, 3u);
	guest_fn_a2a90_x(0x0018cb00u, 0x0013579au, 0x00136f0cu, 3u);
	guest_fn_a2a90_x(0x0018cb44u, 0x001357b6u, 0u, 1u);
	guest_fn_a2a90_x(0x0018cab0u, zp, 0u, 0u);
	guest_fn_a2a90_x(0x0018ccd4u, zp, 0u, 0u);
	guest_fn_a2a90_x(0x0018cb4cu, 0x001357b8u, 0u, 2u);
	for (esi = 0u; esi != 0x10u; esi += 4u) {
		if (EF_U32(0x0013577cu + esi) != 0u)
			guest_fn_a2a90_x(0x0018cb38u, 0x0013577cu + esi, 0u, 5u);
	}
	guest_fn_a2a90_x(0x0018cab0u, zp, 0u, 0u);
	guest_fn_a2a90_x(0x0018cce0u, zp, 0u, 0u);
	v = EF_U8(0x0015f07fu);
	EF_U8(0x0015f07fu) = (uint8_t)(v + 1u);
	guest_fn_a2a90_x(0x0018cb54u, 0x0015f07fu, 0u, 1u);
	EF_U8(0x0015f07fu) = (uint8_t)(EF_U8(0x0015f07fu) - 1u);
	guest_fn_a2a90_x(0x0018cb5cu, 0x0015f07du, 0u, 1u);
	guest_fn_a2a90_x(0x0018cb6cu, 0x0015f07eu, 0x00136f3cu, 3u);
	guest_fn_a2a90_x(0x0018ccf0u, zp, 0u, 0u);
	guest_fn_a2a90_x(0x0018cb74u, 0x0015f099u, 0x00136f84u, 3u);
	guest_fn_a2a90_x(0x0018cb7cu, 0x0015f09au, 0u, 1u);
	guest_fn_a2a90_x(0x0018cab0u, zp, 0u, 0u);
	guest_fn_a2a90_x(0x0018ccfcu, zp, 0u, 0u);
	guest_fn_a2a90_x(0x0018cb88u, 0x0015f4fdu, 0u, 4u);
	guest_fn_a2a90_x(0x0018cb94u, 0x0015f688u, 0x00136f9cu, 3u);
	guest_fn_a2a90_x(0x0018cba0u, 0x0015f53du, 0u, 4u);
	guest_fn_a2a90_x(0x0018cbacu, 0x0015f57du, 0u, 4u);
	guest_fn_a7990(EF_U32(0x00136df8u));
}

/* 0xa9aa8, 273 bytes. Report the fatal text EAX. With byte 0x1371ff set only byte 0x137200 is set. Otherwise clear byte 0x1371ee and, unless byte 0x1371f9 is set, clear the screen buffer (0xdc2c0), blacken the palette (0xd65e8), print the upper-cased text with format 0x18d818 at (15, 20) and the hint 0x18d82c at (15, 20, colour 0x12) with 0xdd370, show the page 0x18d698 (0xf2660, 0xdc250), load the palette 0x464060 (0xd663c) and wait for the key code 0x15 or 0x31 in byte 0x41f833; code 0x31 raises the error text 0x18d840 through the error hook. Then, with byte 0x137206 set, run 0xde4b0, format the text with 0x18d7e8 into 0x419160 and set byte 0x1371f8. */
void report_fatal_text_a9aa8(ef_addr32 text)
{
	char buf[0x64];
	uint8_t key;

	if (EF_U8(0x001371ffu) != 0u) {
		EF_U8(0x00137200u) = 1u;
		return;
	}
	EF_U8(0x001371eeu) = 0u;
	if (EF_U8(0x001371f9u) == 0u) {
		clear_buffer_dc2c0();
		set_black_palette_d65e8();
		guest_sprintf_a4342((ef_addr32)(uintptr_t)buf, 0x0018d818u, uppercase_string(text));
		guest_fn_dd370(0xfu, 0x14u, 0xau, (ef_addr32)(uintptr_t)buf);
		guest_fn_dd370(0xfu, 0x14u, 0x12u, 0x0018d82cu);
		guest_fn_f2660_w(0x0018d698u);
		guest_fn_dc250();
		guest_fn_d663c(0x00464060u);
		EF_U8(0x0041f833u) = 0u;
		for (;;) {
			key = EF_U8(0x0041f833u);
			if (key == 0x15u || key == 0x31u)
				break;
		}
		set_black_palette_d65e8();
		if (EF_U8(0x0041f833u) == 0x31u)
			guest_callptr1_x(EF_U32(0x00132a74u), 0x0018d840u);
	}
	if (EF_U8(0x00137206u) != 0u)
		mouse_request_de4b0();
	guest_sprintf_a4342(0x00419160u, 0x0018d7e8u, text);
	EF_U8(0x001371f8u) = 1u;
}

/* 0xed589, 97 bytes. Format EAX as hexadecimal text in the buffer EDX (0xcbd0f, base 16) and right-justify it to EBX characters with leading '0's; the text is terminated at offset EBX. ORIGINAL BUG: digits beyond the width are not handled (the copy then runs below the buffer start). */
void format_hex_padded_ed589(int32_t value, ef_addr32 buf, uint32_t width)
{
	int32_t len = 0, src, dst, fill;

	signed_to_digits_cbd0f(value, buf, 16u);
	while (EF_U8(buf + (uint32_t)len) != 0u)
		len++;
	src = len;
	dst = (int32_t)width - 1;
	fill = dst;
	while (src != 0) {
		src--;
		EF_U8(buf + (uint32_t)dst) = EF_U8(buf + (uint32_t)src);
		dst--;
		fill--;
	}
	for (; fill >= 0; fill--)
		EF_U8(buf + (uint32_t)fill) = 0x30u;
	EF_U8(buf + width) = 0u;
}

/* 0x487c, 206 bytes. Select the first entry of the table named 0x17f6f0: look its number up (0x433c; failure reported through the error hook with 0x17f6f4), store it in word 0x121e48, copy byte 0x121e44 to byte +0 of the 25-byte row at 0x1970b8 + 25 * number, run 0x40dc(0) and 0x42a4, apply the texts 0x17f714 and 0x17f648 (0xa4d90), format the one-based number with 0x17f720 into a buffer, look the widget of that name up in the dialog returned for 0x17f648 (0xa78bc) and enable it (0xa72d8), apply 0x17f728 and 0x17f734 (0xa4db0) and 0x17f740 (0xa4d90), then bind the widget named 0x17f74c in that dialog to the text 0x130bac (0xa7588). */
void select_first_entry_487c(void)
{
	char buf[0x28];
	int16_t num;
	uint32_t dlg, widget;

	num = (int16_t)guest_fn_433c_x(0x0017f6f0u);
	EF_U16(0x00121e48u) = (uint16_t)num;
	if (num < 0)
		guest_callptr1_x(EF_U32(0x00132a74u), 0x0017f6f4u);
	num = (int16_t)EF_U16(0x00121e48u);
	EF_U8(0x001970b8u + (uint32_t)((int32_t)num * 25)) = EF_U8(0x00121e44u);
	guest_fn_40dc_x(0u);
	guest_fn_42a4_x();
	guest_fn_a4d90(0x0017f714u);
	dlg = apply_a4cb4_a4d90(0x0017f648u);
	guest_sprintf_a4342((ef_addr32)(uintptr_t)buf, 0x0017f720u, (uint32_t)((int32_t)num + 1));
	widget = find_widget_checked_a78bc(dlg, (ef_addr32)(uintptr_t)buf);
	enable_widget_a72d8(widget);
	guest_fn_a4db0(0x0017f728u);
	guest_fn_a4db0(0x0017f734u);
	dlg = apply_a4cb4_a4d90(0x0017f740u);
	widget = find_widget_checked_a78bc(dlg, 0x0017f74cu);
	guest_bind_dialog_widget(widget, 0x00130bacu);
}

/* 0xa99c0, 229 bytes. Load the saved state named EAX (number). Reports the error text 0x18d798 when byte 0x1371ed is already set. Builds the file name from the default path (0xa7b70), the optional 0xa7b80 prefix and the number (formats 0x18d7cc and 0x18d7dc), saves the current state (0xa98d0), formats the number with 0x18d7e8 into a second buffer, opens that with 0xa7900 and falls back to the name buffer (0x9ccb8, error format 0x18d7f0). The file is read (0xaaa50) and closed (0x9ce84), dword 0x419200 is copied to 0x4191f8 and byte 0x1371ed is set to 2. */
void load_saved_state_a99c0(uint32_t number)
{
	char name[0x98];
	char alt[0x98];
	uint32_t h;

	if (EF_U8(0x001371edu) != 0u)
		guest_callptr1_x(EF_U32(0x00132a74u), 0x0018d798u);
	if (call_9c858_18d57c() != 0u) {
		uint32_t r80 = call_9c858_18d57c();
		uint32_t r70 = guest_fn_a7b70_x();
		guest_sprintf_a4342((ef_addr32)(uintptr_t)name, 0x0018d7ccu, r70, r80, number);
	} else {
		guest_sprintf_a4342((ef_addr32)(uintptr_t)name, 0x0018d7dcu, guest_fn_a7b70_x(), number);
	}
	save_state_a98d0();
	guest_sprintf_a4342((ef_addr32)(uintptr_t)alt, 0x0018d7e8u, number);
	h = guest_fn_a7900((ef_addr32)(uintptr_t)alt);
	if (h == 0xffffffffu) {
		h = open_resource_9ccb8((uint32_t)(uintptr_t)name);
		if (h == 0xffffffffu)
			guest_callptr2_x(EF_U32(0x00132a74u), 0x0018d7f0u, (uint32_t)(uintptr_t)name);
	}
	guest_fn_aaa50_x(h);
	guest_fn_9ce84(h);
	EF_U32(0x004191f8u) = EF_U32(0x00419200u);
	EF_U8(0x001371edu) = 2u;
}

/* 0xaad7c, 238 bytes. Load the named saved state EAX: the name is cut at its first '.' (in place), formatted with 0x18d7e8 into a lookup buffer and the data file name is built from the default path (0xa7b70), the optional 0xa7b80 prefix and the name (formats 0x18d7cc and 0x18d7dc). The lookup name is opened with 0xa7900, falling back to the data file name (0x9ccb8, error format 0x18d930). The file is read (0xaa5c4) and closed (0x9ce84), the follow-up steps 0xaa3dc, 0xaa4d4 and 0xaa350 run, dword 0x419200 is copied to 0x4191f8 and byte 0x18d544 to 0x419160. Returns 1. */
uint32_t load_named_state_aad7c(ef_addr32 name)
{
	char data[0xb4];
	char look[0x60];
	uint32_t i = 0u, h;

	while (EF_U8(name + i) != 0x2eu && EF_U8(name + i) != 0u)
		i++;
	EF_U8(name + i) = 0u;
	guest_sprintf_a4342((ef_addr32)(uintptr_t)look, 0x0018d7e8u, name);
	if (call_9c858_18d57c() != 0u) {
		uint32_t r80 = call_9c858_18d57c();
		uint32_t r70 = guest_fn_a7b70_x();
		guest_sprintf_a4342((ef_addr32)(uintptr_t)data, 0x0018d7ccu, r70, r80, name);
	} else {
		guest_sprintf_a4342((ef_addr32)(uintptr_t)data, 0x0018d7dcu, guest_fn_a7b70_x(), name);
	}
	h = guest_fn_a7900((ef_addr32)(uintptr_t)look);
	if (h == 0xffffffffu) {
		h = open_resource_9ccb8((uint32_t)(uintptr_t)data);
		if (h == 0xffffffffu)
			guest_callptr2_x(EF_U32(0x00132a74u), 0x0018d930u, (uint32_t)(uintptr_t)data);
	}
	guest_fn_aa5c4_x(h);
	guest_fn_9ce84(h);
	guest_fn_aa3dc_x();
	guest_fn_aa4d4_x();
	guest_fn_aa350_x();
	EF_U32(0x004191f8u) = EF_U32(0x00419200u);
	EF_U8(0x00419160u) = EF_U8(0x0018d544u);
	return 1u;
}

/* 0xf2660, 264 bytes. Load the palette pair for the page named EAX. Builds the file name (default path 0x9c980 plus the separator 0x1918c4 when the path is not empty plus the name), and if the file exists (hook 0x132ab4) reads it into 0x464060 with the load hook 0x132ab8, otherwise 0x464063..0x464065 are set to 0xff; bytes 0x464060..0x464062 are cleared in any case. Then the name is split (0xe7fc0) and rebuilt (0xe80d0) with the extension 0x1918d8 after the second default path (separator 0x1918d0), and when that file exists it is loaded into 0x433f34 and dword 0x433f30 is set to 1. */
void load_page_palette_f2660(ef_addr32 name)
{
	uint8_t b[0x12c];
	ef_addr32 base = (ef_addr32)(uintptr_t)b;

	build_default_path_9c980(base);
	if (b[0] != 0u)
		strcat_9ca57(base, 0x001918c4u);
	strcat_9ca57(base, name);
	if (guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), base) != 0u) {
		guest_callptr_eaxedx_x(EF_U32(0x00132ab8u), base, 0x00464060u);
	} else {
		EF_U8(0x00464063u) = 0xffu;
		EF_U8(0x00464064u) = 0xffu;
		EF_U8(0x00464065u) = 0xffu;
	}
	EF_U8(0x00464060u) = 0u;
	EF_U8(0x00464061u) = 0u;
	EF_U8(0x00464062u) = 0u;
	build_default_path_9c980(base);
	if (b[0] != 0u)
		strcat_9ca57(base, 0x001918d0u);
	strcat_9ca57(base, name);
	split_path_e7fc0(base, base + 0x128u, base + 0x90u, base + 0x114u, base + 0x120u);
	make_path_e80d0(base, base + 0x128u, base + 0x90u, base + 0x114u, 0x001918d8u);
	if (guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), base) != 0u) {
		guest_callptr_eaxedx_x(EF_U32(0x00132ab8u), base, 0x00433f34u);
		EF_U32(0x00433f30u) = 1u;
	}
}

/* 0x69fac, 227 bytes. Rebuild the tactical screen state: clear bits 2 and 6 of byte 0x159130, install the handlers (0xd1f10), clear byte 0x1f6f4f, set byte 0x137a00, reset the view (0x6900c) and reload the setup (0x69080, 0x68bf4, 0x68efc, 0x69b70), clear the paired object marks of the twenty slots (0x68c18), notify the tactical action 5 for the unit in the high half of dword 0x1282da (0x9e508), run 0x68fe0(0, 0), build the widget parameters from 0xc3670 (0x186370, stack 0x12c) and the dialog 0x18637c (0xa5b1c with 0x1e0, 0x12c, 0x1e0), clear the twenty three-byte slot records at 0x1f6eac, run 0x69094 on the name 0x125828 and, when byte 0x1f6f4e is set, copy the table (0x689f0). */
void rebuild_tactical_screen_69fac(void)
{
	uint32_t i, r, widget;

	EF_U8(0x00159130u) = (uint8_t)(EF_U8(0x00159130u) & 0xbbu);
	install_handlers_d1f10();
	EF_U8(0x001f6f4fu) = 0u;
	EF_U8(0x00137a00u) = 1u;
	reset_view_6900c();
	load_word_0x1282dc();
	load_halfword_pair();
	resolve_weapon_names_68efc();
	refresh_weapon_widgets_69b70();
	for (i = 0; i < 0x14u; i++)
		clear_paired_object_marks((int16_t)i);
	notify_tactical_action((int32_t)EF_U32(0x001282dau) >> 16, 5u);
	guest_fn_68fe0_x(0u, 0u);
	r = guest_fn_c3670(0x00186370u, 0u, 0u, 0x1e0u, 0x12cu);
	widget = guest_fn_a46cc(0x0018637cu);
	set_widget_params_a5b1c(widget, r, 0x1e0u, 0x12cu, 0x1e0u);
	for (i = 0; i < 0x14u; i++) {
		EF_U16(0x001f6eadu + i * 3u) = 0u;
		EF_U8(0x001f6eacu + i * 3u) = 0u;
	}
	guest_fn_69094_x(0x00125828u);
	if (EF_U8(0x001f6f4eu) != 0u)
		copy_table_689f0();
}

/* 0x69540, 317 bytes. Latch a box defined by two position triples (EAX: three words, EDX: three words) into the cursor state when word 0x1f6f4a is set: dwords 0x420910, 0x420918, 0x420914, 0x420908, 0x42091c and 0x420904 and words 0x420930..0x42093a. Then, when 0xd0470 for the unit in the high half of dword 0x1282da (all other arguments zero) returns 1, take the selection index from 0xcd24c into dword 0x1282e0; if it is valid, 0x68ccc accepts the pair (high half of dwords 0x1f6f48 and 0x1282de) and the slot word at 0x1f6ef5 + 3 * index is zero, clear the object pair reference (0xe7c50 with the unit, the high half of dword 0x1f6f40 and of 0x1282de). Dword 0x1282e0 is reset to -1. */
void latch_box_69540(ef_addr32 first, ef_addr32 second)
{
	int32_t a0, a1, a2, b0, b1, b2, idx;

	if (EF_U16(0x001f6f4au) == 0u)
		return;
	a0 = (int16_t)EF_U16(first);
	a1 = (int16_t)EF_U16(first + 2u);
	a2 = (int16_t)EF_U16(first + 4u);
	b0 = (int16_t)EF_U16(second);
	b1 = (int16_t)EF_U16(second + 2u);
	b2 = (int16_t)EF_U16(second + 4u);
	EF_U32(0x00420910u) = (uint32_t)a0;
	EF_U32(0x00420918u) = (uint32_t)a2;
	EF_U32(0x00420914u) = (uint32_t)b0;
	EF_U32(0x00420908u) = (uint32_t)b1;
	EF_U32(0x0042091cu) = (uint32_t)b2;
	EF_U16(0x00420930u) = (uint16_t)a0;
	EF_U16(0x00420932u) = (uint16_t)a1;
	EF_U16(0x00420934u) = (uint16_t)a2;
	EF_U16(0x00420936u) = (uint16_t)b0;
	EF_U16(0x00420938u) = (uint16_t)b1;
	EF_U16(0x0042093au) = (uint16_t)b2;
	EF_U32(0x00420904u) = (uint32_t)a1;
	if (guest_fn_d0470((int32_t)EF_U32(0x001282dau) >> 16, 0, 0, 0) != 1u)
		return;
	idx = (int16_t)guest_fn_cd24c_x();
	EF_U32(0x001282e0u) = (uint32_t)idx;
	if (idx != -1
	    && (guest_fn_68ccc((int16_t)((int32_t)EF_U32(0x001f6f48u) >> 16), (uint16_t)((int32_t)EF_U32(0x001282deu) >> 16)) & 0xffu) != 0u
	    && EF_U16(0x001f6ef5u + (uint32_t)(idx * 3)) == 0u)
		guest_fn_e7c50((int32_t)EF_U32(0x001282dau) >> 16, (int32_t)EF_U32(0x001f6f40u) >> 16, (uint32_t)((int32_t)EF_U32(0x001282deu) >> 16));
	EF_U32(0x001282e0u) = 0xffffffffu;
}

/* 0x6aa70, 316 bytes. Advance the next-object link (word +0x16a, selected through the free-object finder 0x9075c) of unit AX (signed) until an object is found that is close to the unit. Returns 0 for a missing unit. The link is advanced once and the new value remembered; the loop then advances it again and, for a link other than 0xff, looks the object up (0x8ffe8) and, when the class test 0x2e910 (class byte +0x15 of the unit's record, kind byte +0x1d of the object's table row) accepts it and the scaled distance (0x37bc8) is below the double at 0x18638c, sets a found flag. It stops after twice the count in the high half of dword 0x211ade steps (returning 0), when the link wraps back to the remembered value, when the flag is set or when the link reaches 0xff; returns 1 unless the final link is 0xff. */
uint32_t advance_to_nearby_object_6aa70(uint32_t ax)
{
	uint32_t unit, obj, remembered, cls, kind, rec;
	int32_t steps = 0;
	uint8_t found = 0;
	double d, limit;
	int16_t cur;

	unit = guest_fn_7489c((int32_t)(int16_t)ax);
	if (unit == 0u)
		return 0u;
	remembered = next_free_object_9075c((uint32_t)((int32_t)EF_U32(unit + 0x168u) >> 16));
	EF_U16(unit + 0x16au) = (uint16_t)remembered;
	for (;;) {
		EF_U16(unit + 0x16au) = (uint16_t)next_free_object_9075c((uint32_t)((int32_t)EF_U32(unit + 0x168u) >> 16));
		cur = (int16_t)((int32_t)EF_U32(unit + 0x168u) >> 16);
		if (cur != 0xff) {
			obj = guest_lookup_object_8ffe8(cur);
			if (obj != 0u) {
				kind = EF_U8(EF_U32(0x001a14f8u) + (uint32_t)(((int32_t)EF_U32(obj + 0x36u) >> 16) * 0x45) + 0x1du);
				rec = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca);
				cls = EF_U8(rec + 0x15u);
				if ((guest_fn_2e910_x(cls, kind) & 0xffu) != 0u) {
					d = scaled_pair_distance_37bc8(unit + 0x49u, (int16_t)((int32_t)EF_U32(obj + 0x36u) >> 16));
					memcpy(&limit, ef_guest_memory_ptr(0x0018638cu), sizeof limit);
					if (d < limit)
						found = 1u;
				}
			}
		}
		steps++;
		if ((int32_t)(int16_t)steps > ((int32_t)EF_U32(0x00211adeu) >> 16) * 2)
			return 0u;
		if ((uint16_t)remembered == EF_U16(unit + 0x16au) || found != 0u
		    || ((int32_t)EF_U32(unit + 0x168u) >> 16) == 0xff)
			break;
	}
	return ((int32_t)EF_U32(unit + 0x168u) >> 16) == 0xff ? 0u : 1u;
}

/* 0x6a8fc, 369 bytes. Choose the nearest valid candidate in the eight-entry candidate list (words at +0x12b, 2 bytes apart; 0xff means empty) of unit AX (signed). A candidate counts when 0x6a82c accepts the pair, the candidate unit exists, has no target (+0x1c) and bit 1 of byte +0x24 clear, each coordinate difference (doubles at +0x49, +0x51, +0x59; truncated by 0x9d72c and made absolute by 0xa28f2) is below the best distance so far and the indexed distance 0x6a5f0 is below it too. The best distance starts at 100000.0; the closest candidate's word is stored in word +0x146 of the unit. Returns 1 when any candidate was chosen, otherwise 0 (also for a missing unit). */
uint32_t pick_nearest_candidate_6a8fc(uint32_t ax)
{
	uint32_t unit, p, other_u;
	int32_t j, other, diff;
	double best = 100000.0, d;
	int found = 0;
	static const uint32_t axis[3] = { 0x49u, 0x51u, 0x59u };
	double a, b;
	int k, skip;

	unit = guest_fn_7489c((int32_t)(int16_t)ax);
	if (unit == 0u)
		return 0u;
	for (j = 0; j < 8; j++) {
		p = unit + (uint32_t)(j * 2);
		other = (int32_t)EF_U32(p + 0x129u) >> 16;
		if (other == 0xff)
			continue;
		if (is_valid_target_6a82c((int16_t)ax, (int16_t)other) == 0u)
			continue;
		other_u = guest_fn_7489c(other);
		if (EF_U32(other_u + 0x1cu) != 0u || (EF_U8(other_u + 0x24u) & 2u) != 0u)
			continue;
		skip = 0;
		for (k = 0; k < 3 && !skip; k++) {
			memcpy(&a, ef_guest_memory_ptr(unit + axis[k]), sizeof a);
			memcpy(&b, ef_guest_memory_ptr(other_u + axis[k]), sizeof b);
			diff = (int32_t)truncate_9d72c(a - b);
			diff = (int32_t)abs_a28f2(diff);
			if (!((double)diff < best))
				skip = 1;
		}
		if (skip)
			continue;
		d = indexed_object_distance((int16_t)ax, (int16_t)other);
		if (!(d < best))
			continue;
		EF_U16(unit + 0x146u) = EF_U16(p + 0x12bu);
		best = d;
		found = 1;
	}
	return found ? 1u : 0u;
}

/* 0x69374, 458 bytes. Variant of 0x69540 that acts on the selection: latches the same two position triples into the cursor state (the latch is unconditional here), and when 0xd0470 for the unit in the high half of dword 0x1282da returns 1 and the selection index from 0xcd24c (dword 0x1282e0) is valid and 0x68ccc accepts (high halves of 0x1f6f48 and 0x1282de): with word 0x1282d8 set and the first slot equal to 0x1d (and 0x1e accepted too) run 0x68a60 on the target with word 0x1f6f4a temporarily set to 0x1e and the high half of dword 0x12cd86, otherwise on the target with the dword at 0x12bc88 + 145 * slot (high half) and return; with word 0x1282d8 clear, or when 0x68ccc rejected, clear the paired object marks (0x68c18) of the target if word 0x1282da is nonzero. Dword 0x1282e0 is reset to -1. */
void apply_selection_69374(ef_addr32 first, ef_addr32 second)
{
	int32_t a0, a1, a2, b0, b1, b2, idx, slot, tgt;

	a0 = (int16_t)EF_U16(first);
	a1 = (int16_t)EF_U16(first + 2u);
	a2 = (int16_t)EF_U16(first + 4u);
	b0 = (int16_t)EF_U16(second);
	b1 = (int16_t)EF_U16(second + 2u);
	b2 = (int16_t)EF_U16(second + 4u);
	EF_U32(0x00420910u) = (uint32_t)a0;
	EF_U32(0x00420918u) = (uint32_t)a2;
	EF_U32(0x00420914u) = (uint32_t)b0;
	EF_U32(0x00420908u) = (uint32_t)b1;
	EF_U32(0x0042091cu) = (uint32_t)b2;
	EF_U16(0x00420930u) = (uint16_t)a0;
	EF_U16(0x00420932u) = (uint16_t)a1;
	EF_U16(0x00420934u) = (uint16_t)a2;
	EF_U16(0x00420936u) = (uint16_t)b0;
	EF_U16(0x00420938u) = (uint16_t)b1;
	EF_U16(0x0042093au) = (uint16_t)b2;
	EF_U32(0x00420904u) = (uint32_t)a1;
	if (guest_fn_d0470((int32_t)EF_U32(0x001282dau) >> 16, 0, 0, 0) != 1u)
		return;
	idx = (int16_t)guest_fn_cd24c_x();
	EF_U32(0x001282e0u) = (uint32_t)idx;
	if (idx == -1)
		return;
	slot = (int32_t)EF_U32(0x001f6f48u) >> 16;
	tgt = (int32_t)EF_U32(0x001282deu) >> 16;
	if ((guest_fn_68ccc((int16_t)slot, (uint16_t)tgt) & 0xffu) != 0u) {
		if (EF_U16(0x001282d8u) != 0u) {
			if (slot == 0x1d && (guest_fn_68ccc(0x1e, (uint16_t)tgt) & 0xffu) != 0u) {
				EF_U16(0x001f6f4au) = 0x1eu;
				guest_fn_68a60_x(tgt, (int32_t)EF_U32(0x0012cd86u) >> 16);
				EF_U16(0x001f6f4au) = 0x1du;
			} else {
				guest_fn_68a60_x(tgt, (int32_t)EF_U32((uint32_t)slot * 145u + 0x0012bc88u) >> 16);
				EF_U32(0x001282e0u) = 0xffffffffu;
				return;
			}
		} else if (EF_U16(0x001282dau) != 0u) {
			clear_paired_object_marks((int16_t)tgt);
		}
	} else if (EF_U16(0x001282dau) != 0u) {
		clear_paired_object_marks((int16_t)tgt);
	}
	EF_U32(0x001282e0u) = 0xffffffffu;
}

/* 0x69740, 466 bytes. Per-frame update of the tactical cursor window. Updates the view scale (0xdc314), calls 0xdc334(2, 2) and resets the view (0xc4105). Moves the window corner words 0x1f6f46, 0x1f6f44 and 0x1f6f3a one eighth of the way (signed, truncating) towards their targets (words 0x1f6f3c, 0x1f6f3e and 0x1f6f48), rotates the offset (0xc8d30 with the new corners), sets the scene angles (0xc4654), calls 0x9e0c4 for the unit in the high half of dword 0x1282da, transforms the cursor point (0x69680, from words 0x41eefc/0x41eefe minus 0x15bdbc/0x15bdb8), clears the pair references of the thirteen slots (0xe7c50 with -1), reassigns the page words (0x68e84), re-attaches every active slot (byte 0x1f6ef4 + 3 * slot set and word 0x1f6ef5 nonzero) through 0xe7c50 and finally runs 0x69374 (word 0x12830e set) or 0x69540 on the transformed point. Clears word 0x12830e after 0xc4167. */
void update_cursor_window_69740(void)
{
	uint8_t pt[16];
	int16_t t3c, t3e, t3a, t44, t46, t48, r1, r2, r3, x, y;
	int32_t i, slot;
	uint32_t unit;

	update_view_scale_dc314();
	guest_mode_36_hook(2u, 2u);
	reset_view_c4105();
	t3c = (int16_t)EF_U16(0x001f6f3cu);
	t46 = (int16_t)EF_U16(0x001f6f46u);
	r1 = (int16_t)(t46 + (int16_t)(t3c - t46) / 8);
	t3e = (int16_t)EF_U16(0x001f6f3eu);
	t44 = (int16_t)EF_U16(0x001f6f44u);
	r2 = (int16_t)(t44 + (int16_t)(t3e - t44) / 8);
	EF_U16(0x001f6f46u) = (uint16_t)r1;
	t48 = (int16_t)EF_U16(0x001f6f48u);
	t3a = (int16_t)EF_U16(0x001f6f3au);
	r3 = (int16_t)(t3a + (int16_t)(t48 - t3a) / 8);
	EF_U16(0x001f6f44u) = (uint16_t)r2;
	EF_U16(0x001f6f3au) = (uint16_t)r3;
	rotate_offset_c8d30(0u, 0u, 0u, (uint32_t)(int32_t)r1, (uint32_t)(int32_t)r2, (uint32_t)(int32_t)r3);
	guest_fn_c4654(0u, (uint32_t)(int32_t)r2, (uint32_t)(int32_t)r1);
	unit = (uint32_t)((int32_t)EF_U32(0x001282dau) >> 16);
	guest_fn_9e0c4((int32_t)unit, 0u, 0u, 0u);
	x = (int16_t)(EF_U16(0x0041eefcu) - EF_U16(0x0015bdbcu));
	y = (int16_t)(EF_U16(0x0041eefeu) - EF_U16(0x0015bdb8u));
	transform_point_69680((ef_addr32)(uintptr_t)pt, (ef_addr32)(uintptr_t)(pt + 8), (int16_t)y, (int16_t)x);
	for (i = 0; i < 13; i++)
		guest_fn_e7c50((int32_t)unit, -1, (uint32_t)i);
	assign_page_words_68e84((uint32_t)((int32_t)EF_U32(0x001f6f48u) >> 16));
	for (i = 0; i < 13; i++) {
		if (EF_U8(0x001f6ef4u + (uint32_t)(i * 3)) == 0u || EF_U16(0x001f6ef5u + (uint32_t)(i * 3)) == 0u)
			continue;
		slot = (int32_t)EF_U32(0x001f6ef1u + (uint32_t)(i * 3)) >> 24;
		guest_fn_e7c50((int32_t)unit, (int32_t)EF_U32(0x0012bc88u + (uint32_t)(slot * 0x91)) >> 16, (uint32_t)i);
	}
	if (EF_U16(0x0012830eu) != 0u)
		apply_selection_69374((ef_addr32)(uintptr_t)pt, (ef_addr32)(uintptr_t)(pt + 8));
	else
		latch_box_69540((ef_addr32)(uintptr_t)pt, (ef_addr32)(uintptr_t)(pt + 8));
	guest_fn_c4167_x();
	EF_U16(0x0012830eu) = 0u;
}

/* 0x69d10, 524 bytes. Refresh the slot bytes of the list at dword 0x137960 (next link +4) and redraw it through 0xb3168. For every node that is selected (mode byte 0x125846 equal to 1, the first node, or the second node when more than four players are present in the high half of dword 0x1357ae) the twenty bytes at node-data +0x55 (rows of 0x7c bytes, row byte 0x125837, data pointer at node +0x13) are cleared for unused slots (byte 0x1f6ef4 + 3 * slot clear) and otherwise set from the mapping 0x8ac64 of the slot's top byte (dword 0x1f6ef1 + 3 * slot >> 24). Mode 3 only updates the node whose index equals the high half of dword 0x125833. The list is then drawn with 0xb3168 using the table 0x186284 (modes 1 and 2) or 0x186348 (mode 3) and the four window words 0x1f6f32, 0x1f6f2e, 0x1f6f30 and 0x1f6f34 (high halves). */
void refresh_slot_list_69d10(void)
{
	uint32_t mode = EF_U8(0x00125846u), p, i, k, row;
	int32_t hi_players;
	uint32_t w34 = (uint32_t)((int32_t)EF_U32(0x001f6f34u) >> 16);
	uint32_t w30 = (uint32_t)((int32_t)EF_U32(0x001f6f30u) >> 16);
	uint32_t w2e = (uint32_t)((int32_t)EF_U32(0x001f6f2eu) >> 16);
	uint32_t w32 = (uint32_t)((int32_t)EF_U32(0x001f6f32u) >> 16);

	if (mode != 3u) {
		i = 0;
		for (p = EF_U32(0x00137960u); p != 0u; p = EF_U32(p + 4u), i++) {
			hi_players = (int32_t)EF_U32(0x001357aeu) >> 16;
			if (!(mode == 1u || i == 0u || (hi_players > 4 && i == 1u)))
				continue;
			row = EF_U32(p + 0x13u) + EF_U8(0x00125837u) * 0x7cu;
			for (k = 0; k < 0x14u; k++) {
				if (EF_U8(0x001f6ef4u + k * 3u) == 0u)
					EF_U8(row + k + 0x55u) = 0u;
				else
					EF_U8(row + k + 0x55u) = (uint8_t)guest_fn_8ac64_x((int32_t)EF_U32(0x001f6ef1u + k * 3u) >> 24);
			}
		}
		guest_fn_b3168_x(0x00186284u, 0x00137960u, 0x00137970u, 0x00137958u, mode == 1u ? 2u : 1u, w32, w2e, w30, w34);
		return;
	}
	i = 0;
	p = EF_U32(0x00137960u);
	while (p != 0u && (int32_t)i < ((int32_t)EF_U32(0x00125833u) >> 16)) {
		p = EF_U32(p + 4u);
		i++;
	}
	if (p == 0u)
		return;
	row = EF_U32(p + 0x13u) + EF_U8(0x00125837u) * 0x7cu;
	for (k = 0; k < 0x14u; k++) {
		if (EF_U8(0x001f6ef4u + k * 3u) == 0u)
			EF_U8(row + k + 0x55u) = 0u;
		else
			EF_U8(row + k + 0x55u) = (uint8_t)guest_fn_8ac64_x((int32_t)EF_U32(0x001f6ef1u + k * 3u) >> 24);
	}
	guest_fn_b3168_x(0x00186348u, 0x00137960u, 0x00137970u, 0x00137958u, (uint32_t)((int32_t)EF_U32(0x0041f31eu) >> 16), w32, w2e, w30, w34);
}

/* 0x68a60, 401 bytes. Assign slot EAX (signed) of the three-byte slot table at 0x1f6ef1 (kind byte +3, word +4) to the mode byte of word 0x1f6f4a and attach it with value EDX (signed) through 0xe7c50. With word 0x1f6f4a clear both the slot and its partner (word at 0x1282e4 + 2 * slot) are marked with byte 0x1f6f4a, their words cleared and both detached (0xe7c50 with -1). Otherwise the slot is skipped when a special slot exists (0x6a090) and its kind is 0x22; else it is marked with the byte and word 1 and attached; the partner is then treated the same way (skipped for kind 0x22 when a special slot exists) and, after the partner is attached, 0x68a2c runs when the byte at 0x12bc97 + 145 * high half of dword 0x1f6f48 equals 3. */
void assign_slot_68a60(int32_t slot, int32_t value)
{
	int32_t k = (int16_t)slot, j, unit = (int32_t)EF_U32(0x001282dau) >> 16;
	int32_t v = (int16_t)value;

	j = (int32_t)EF_U32(0x001282e2u + (uint32_t)(k * 2)) >> 16;
	if (EF_U16(0x001f6f4au) == 0u) {
		EF_U8(0x001f6ef4u + (uint32_t)(k * 3)) = EF_U8(0x001f6f4au);
		EF_U8(0x001f6ef4u + (uint32_t)(j * 3)) = EF_U8(0x001f6f4au);
		EF_U16(0x001f6ef5u + (uint32_t)(k * 3)) = 0u;
		EF_U16(0x001f6ef5u + (uint32_t)(j * 3)) = 0u;
		guest_fn_e7c50(unit, -1, (uint32_t)k);
		guest_fn_e7c50(unit, -1, (uint32_t)j);
		return;
	}
	if (any_special_slot_6a090() != 0u && ((int32_t)EF_U32(0x001f6ef1u + (uint32_t)(k * 3)) >> 24) == 0x22)
		return;
	EF_U8(0x001f6ef4u + (uint32_t)(k * 3)) = EF_U8(0x001f6f4au);
	EF_U16(0x001f6ef5u + (uint32_t)(k * 3)) = 1u;
	guest_fn_e7c50(unit, v, (uint32_t)k);
	if (any_special_slot_6a090() != 0u && ((int32_t)EF_U32(0x001f6ef1u + (uint32_t)(j * 3)) >> 24) == 0x22)
		return;
	EF_U8(0x001f6ef4u + (uint32_t)(j * 3)) = EF_U8(0x001f6f4au);
	EF_U16(0x001f6ef5u + (uint32_t)(j * 3)) = 1u;
	guest_fn_e7c50(unit, v, (uint32_t)j);
	if (EF_U8(0x0012bc97u + (uint32_t)(((int32_t)EF_U32(0x001f6f48u) >> 16) * 145)) == 3u)
		request_68a2c();
}

/* 0x6b620, 196 bytes. Copy the pending view state into unit AX (signed): dwords 0x1f71b0/0x1f71b4 to +0x91/+0x95, 0x12850c/0x128510 to +0x16f/+0x173, the double at 0x1284cc to +0x79, dwords 0x1284c4/0x1284c8 to +0x81/+0x85 and 0x1f71a8/0x1f71ac to +0x89/+0x8d (after normalising the angles with 0x6bca8). Bit 0 of byte +0x24 is set when the double at 0x1f6f50 is nonzero (ignoring its sign) and cleared otherwise; dwords 0x1f7018/0x1f701c are stored at +0x148/+0x14c. Nothing happens for a missing unit. */
void copy_view_state_6b620(uint32_t ax)
{
	uint32_t unit = guest_fn_7489c((int32_t)(int16_t)ax);

	if (unit == 0u)
		return;
	EF_U32(unit + 0x91u) = EF_U32(0x001f71b0u);
	EF_U32(unit + 0x95u) = EF_U32(0x001f71b4u);
	EF_U32(unit + 0x16fu) = EF_U32(0x0012850cu);
	EF_U32(unit + 0x173u) = EF_U32(0x00128510u);
	normalise_angles_6bca8();
	EF_U32(unit + 0x79u) = EF_U32(0x001284ccu);
	EF_U32(unit + 0x7du) = EF_U32(0x001284d0u);
	EF_U32(unit + 0x81u) = EF_U32(0x001284c4u);
	EF_U32(unit + 0x85u) = EF_U32(0x001284c8u);
	EF_U32(unit + 0x89u) = EF_U32(0x001f71a8u);
	EF_U32(unit + 0x8du) = EF_U32(0x001f71acu);
	if ((EF_U32(0x001f6f54u) & 0x7fffffffu) != 0u || EF_U32(0x001f6f50u) != 0u)
		EF_U8(unit + 0x24u) |= 1u;
	else
		EF_U8(unit + 0x24u) &= 0xfeu;
	EF_U32(unit + 0x148u) = EF_U32(0x001f7018u);
	EF_U32(unit + 0x14cu) = EF_U32(0x001f701cu);
}

/* 0x288c8, 225 bytes. Age the list of effect nodes at dword 0x19b8c4 (next link +0xa; position words +0, +2, +4, kind word +6, age word +8). Each node's age is incremented and capped at 7; a node within 10000 (largest axis difference, 0x2888c) of the viewer position (dwords 0x21444c, 0x214448, 0x214450) is published in the effect globals 0x15bde0 (kind), 0x15bde4 (age), 0x15bde8, 0x15bdec, 0x15bdf0 (position), dword 0x15bddc is cleared, 0x15be0c set to 100 and the effect is started (0x15be10). Afterwards nodes at the head of the list whose age is 7 are freed (0x28a34). */
void age_effect_nodes_288c8(void)
{
	uint32_t node, head;

	node = EF_U32(0x0019b8c4u);
	if (node == 0u)
		return;
	do {
		EF_U16(node + 8u) = (uint16_t)(EF_U16(node + 8u) + 1u);
		if (((int32_t)EF_U32(node + 6u) >> 16) > 7)
			EF_U16(node + 8u) = 7u;
		if ((int32_t)max_magnitude_2888c((uint32_t)((int32_t)(int16_t)EF_U16(node) - (int32_t)EF_U32(0x0021444cu)),
		    (uint32_t)((int32_t)(int16_t)EF_U16(node + 2u) - (int32_t)EF_U32(0x00214448u)),
		    (uint32_t)((int32_t)(int16_t)EF_U16(node + 4u) - (int32_t)EF_U32(0x00214450u))) < 10000) {
			EF_U32(0x0015bde0u) = (uint32_t)(int32_t)(int16_t)EF_U16(node + 6u);
			EF_U32(0x0015bde4u) = (uint32_t)(int32_t)(int16_t)EF_U16(node + 8u);
			EF_U32(0x0015bde8u) = (uint32_t)(int32_t)(int16_t)EF_U16(node);
			EF_U32(0x0015bdecu) = (uint32_t)(int32_t)(int16_t)EF_U16(node + 2u);
			EF_U32(0x0015bdf0u) = (uint32_t)(int32_t)(int16_t)EF_U16(node + 4u);
			EF_U32(0x0015bddcu) = 0u;
			EF_U32(0x0015be0cu) = 100u;
			guest_fn_15be10_x();
		}
		node = EF_U32(node + 0xau);
	} while (node != 0u);
	head = EF_U32(0x0019b8c4u);
	if (((int32_t)EF_U32(head + 6u) >> 16) != 7)
		return;
	do {
		free_list_node_28a34();
		head = EF_U32(0x0019b8c4u);
	} while (head != 0u && ((int32_t)EF_U32(head + 6u) >> 16) == 7);
}

/* 0x242b8, 243 bytes. Propagate unit A's target record to unit B (AX, DX are signed 16-bit unit ids). Nothing happens if either unit is missing or both class rows (0xca bytes at dword 0x19bb58, indexed by the high word of unit+0x26) have the same word at +0x15. If B's word +0x158 equals the high word of A's dword +0x14, A's 8 bytes at +0x150 are copied to B's +0x15a. Otherwise, unless A's byte +0x16d is 3, B is only updated when its stored double at +0x15a plus the constant at 0x181c90 is below A's double at +0x150 or B cannot be supported (0x238c0); the update copies word +0x16 to B's +0x158 and the double +0x150 to +0x15a and notifies the unit (0x6b444). */
void propagate_target_record_242b8(uint32_t ax, uint32_t dx)
{
	int32_t ua = (int32_t)(int16_t)ax;
	int32_t ub = (int32_t)(int16_t)dx;
	uint32_t a = guest_fn_7489c(ua);
	uint32_t b = guest_fn_7489c(ub);
	uint32_t rows, ra, rb;
	double d1, d2, c;

	if (a == 0u || b == 0u)
		return;
	rows = EF_U32(0x0019bb58u);
	rb = (uint32_t)(((int32_t)EF_U32(b + 0x26u) >> 16) * 0xca) + rows;
	ra = (uint32_t)(((int32_t)EF_U32(a + 0x26u) >> 16) * 0xca) + rows;
	if (EF_U16(rb + 0x15u) == EF_U16(ra + 0x15u))
		return;
	if (EF_U16(b + 0x158u) == (uint32_t)(((int32_t)EF_U32(a + 0x14u) >> 16) & 0xffff)) {
		EF_U32(b + 0x15au) = EF_U32(a + 0x150u);
		EF_U32(b + 0x15eu) = EF_U32(a + 0x154u);
		return;
	}
	if (EF_U8(a + 0x16du) == 3u)
		return;
	memcpy(&d1, ef_guest_memory_ptr(b + 0x15au), 8);
	memcpy(&c, ef_guest_memory_ptr(0x00181c90u), 8);
	memcpy(&d2, ef_guest_memory_ptr(a + 0x150u), 8);
	if (!(d1 + c < d2) && can_support_238c0(ub) != 0u)
		return;
	EF_U16(b + 0x158u) = EF_U16(a + 0x16u);
	memcpy(ef_guest_memory_ptr(b + 0x15au), ef_guest_memory_ptr(a + 0x150u), 8);
	notify_unit_6b444((int16_t)ub);
}

/* 0x2b708, 379 bytes. Compact the seven-entry slot list of class row AX (signed 16-bit; rows of 0xca bytes at dword 0x19bb58): words at row+0x50 equal to 0xff are filled by moving up the next used entry, together with its three words at +0x18/+0x1a/+0x1c of the 6-byte records behind dword row+0xb5, then the old entry is set to 0xff. Afterwards every entry's unit (0x7489c) gets its byte +0x2a set to the entry index. The loop test reuses the last search index, as in the original. */
void compact_slot_list_2b708(uint32_t ax)
{
	int32_t row = (int32_t)(int16_t)ax;
	uint32_t rows = EF_U32(0x0019bb58u);
	int32_t i = 0, j = 0, k;
	uint32_t base, ptr, u;

	while (j < 7) {
		base = rows + (uint32_t)(row * 0xca);
		if (EF_U16(base + (uint32_t)(i * 2) + 0x50u) == 0xffu) {
			j = i + 1;
			while (EF_U16(base + (uint32_t)(j * 2) + 0x50u) == 0xffu && j * 2 < 0xe)
				j++;
			if (j < 7) {
				EF_U16(base + (uint32_t)(i * 2) + 0x50u) = EF_U16(base + (uint32_t)(j * 2) + 0x50u);
				ptr = EF_U32(base + 0xb5u);
				if (ptr != 0u) {
					EF_U16(ptr + (uint32_t)(i * 6) + 0x18u) = EF_U16(ptr + (uint32_t)(j * 6) + 0x18u);
					EF_U16(ptr + (uint32_t)(i * 6) + 0x1au) = EF_U16(ptr + (uint32_t)(j * 6) + 0x1au);
					EF_U16(ptr + (uint32_t)(i * 6) + 0x1cu) = EF_U16(ptr + (uint32_t)(j * 6) + 0x1cu);
				}
				EF_U16(base + (uint32_t)(j * 2) + 0x50u) = 0xffu;
			}
		}
		i++;
		if (i >= 7)
			break;
	}
	for (k = 0; k < 7; k++) {
		base = rows + (uint32_t)(row * 0xca);
		u = guest_fn_7489c((int32_t)(int16_t)EF_U16(base + (uint32_t)(k * 2) + 0x50u));
		rows = EF_U32(0x0019bb58u);
		if (u != 0u)
			EF_U8(u + 0x2au) = (uint8_t)k;
	}
}

/* 0x2c224, 308 bytes. Test whether class row AX (signed 16-bit) needs attention: true when the row's byte +0x1a is 2, 5 or 6; when the entry (byte +0x8e) of its table (dword +0x8a, 25-byte entries) has kind 5 and the distance (0xb415c) from the row position (+0x61) to the point formed by the entry's two dwords scaled by the double at 0x18255a is below 64890.0; when the row's linked row (high word of dword +0x20, if not 0xff and of a different side, word +0x15) is closer than 74160.0 (0x2bb3c); or when 0x2bfcc or grid_has_object_mode(row, 2) report true. */
uint8_t row_needs_attention_2c224(uint32_t ax)
{
	int32_t cx = (int32_t)(int16_t)ax;
	uint32_t rows = EF_U32(0x0019bb58u);
	uint32_t r = rows + (uint32_t)(cx * 0xca);
	uint8_t bl = 0u;
	uint8_t al = EF_U8(r + 0x1au);
	uint32_t k, tab, other, r2;
	double pt[3], scale, d;

	if (al == 2u || al == 5u || al == 6u)
		bl = 1u;
	rows = EF_U32(0x0019bb58u);
	r = rows + (uint32_t)(cx * 0xca);
	k = EF_U8(r + 0x8eu);
	tab = EF_U32(r + 0x8au);
	if (EF_U8(tab + k * 25u + 0x18u) == 5u) {
		memcpy(&scale, ef_guest_memory_ptr(0x0018255au), 8);
		pt[0] = (double)EF_S32(tab + k * 25u) * scale;
		tab = EF_U32(r + 0x8au);
		pt[1] = (double)EF_S32(tab + k * 25u + 4u) * scale;
		pt[2] = 0.0;
		memcpy(&scale, ef_guest_memory_ptr(0x00182562u), 8);
		d = distance_2d_b415c(r + 0x61u, (ef_addr32)(uintptr_t)pt);
		if (d < scale)
			bl = 1u;
	}
	rows = EF_U32(0x0019bb58u);
	r = rows + (uint32_t)(cx * 0xca);
	other = (uint32_t)((int32_t)EF_U32(r + 0x20u) >> 16);
	if ((int32_t)other != 0xff) {
		r2 = (uint32_t)((int32_t)other * 0xca) + rows;
		if (EF_U16(r + 0x15u) != EF_U16(r2 + 0x15u)) {
			d = grid_cell_distance((int16_t)cx, (int16_t)other);
			memcpy(&scale, ef_guest_memory_ptr(0x0018256au), 8);
			if (d < scale)
				bl = 1u;
		}
	}
	if (guest_fn_2bfcc(cx) != 0u || grid_has_object_mode((int16_t)cx, 2u) != 0u)
		bl = 1u;
	return bl;
}

/* 0x2bfcc, 287 bytes. Scan the six unit entries (signed words at row+0x50) of class row AX (signed 16-bit; 0xff gives 0) and return 1 when any unit: moves faster than 0.045 (double +0x15a), or faster than 0.03 while the row's byte +0x1b is 2, or faster than 0.02 and grid_is_current_or_external(row) holds; has a non-zero dword +0x1c; or has a linked unit (word +0x146) while its double +0x150 exceeds 0.06 and the linked unit belongs to another side (class row word +0x15). */
uint8_t row_has_active_unit_2bfcc(uint32_t ax)
{
	int32_t row = (int32_t)(int16_t)ax;
	uint8_t bl = 0u;
	int32_t k;
	uint32_t base, u, t, r2;
	double v, c;

	if (row == 0xff)
		return 0u;
	for (k = 0; k < 6; k++) {
		base = EF_U32(0x0019bb58u) + (uint32_t)(row * 0xca);
		u = guest_fn_7489c((int32_t)(int16_t)EF_U16(base + (uint32_t)(k * 2) + 0x50u));
		if (u == 0u)
			continue;
		t = guest_fn_7489c((int32_t)(int16_t)EF_U16(u + 0x146u));
		memcpy(&v, ef_guest_memory_ptr(u + 0x15au), 8);
		memcpy(&c, ef_guest_memory_ptr(0x0018253au), 8);
		if (v > c && EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(row * 0xca) + 0x1bu) == 2u) {
			bl = 1u;
		} else {
			memcpy(&c, ef_guest_memory_ptr(0x00182542u), 8);
			if (v > c) {
				bl = 1u;
			} else {
				memcpy(&c, ef_guest_memory_ptr(0x0018254au), 8);
				if (v > c && grid_is_current_or_external((int16_t)row) != 0u)
					bl = 1u;
			}
		}
		if (EF_U32(u + 0x1cu) != 0u)
			bl = 1u;
		if (t == 0u)
			continue;
		memcpy(&v, ef_guest_memory_ptr(u + 0x150u), 8);
		memcpy(&c, ef_guest_memory_ptr(0x00182552u), 8);
		if (v <= c)
			continue;
		r2 = EF_U32(0x0019bb58u);
		if (EF_U16(r2 + (uint32_t)(row * 0xca) + 0x15u) != EF_U16(r2 + (uint32_t)(((int32_t)EF_U32(t + 0x26u) >> 16) * 0xca) + 0x15u))
			bl = 1u;
	}
	return bl;
}

/* 0x24074, 577 bytes. Update unit AX's accumulated threat value (double +0x150) with respect to unit DX (both signed 16-bit ids, units via 0x6a1b4). Does nothing when unit A is missing or A == B; with B missing the selector double 0x1f6fe8 is cleared. If both units belong to the same class row side (word +0x15 of the rows, B's row not 0xff) the value is reset to 0. Otherwise the selector 0x1f6fe8 is set from the angle double 0x1f7018 and the sign of the dot product of the two unit vectors (doubles +0xa1, +0xb9, +0xd1): above 2.0943951 gives 3.0 (negative dot) or 5.0; below 0.7853981634 gives 2.0 or 4.0; otherwise 1.0. Then 0x1f7050 = 2*[0x1f7058] + [0x1f6ff8] + 1 (clamped to 3.0 when negative) and the value becomes cos(angle)*0.1 / (distance*0.0005 + 1) + 0.9 * value, reset to 0 when the selector is 3.0. */
void update_threat_value_24074(uint32_t ax, uint32_t dx)
{
	int32_t a = (int32_t)(int16_t)ax;
	int32_t b = (int32_t)(int16_t)dx;
	uint32_t ua = guest_fn_6a1b4(a);
	uint32_t ub = guest_fn_6a1b4(b);
	uint32_t rows, rb, ra;
	double g, c, dot, sel, v, t, x;

	if (ua == 0u)
		return;
	guest_fn_243ac_x(a);
	if ((int16_t)a == (int16_t)b)
		return;
	if (ub == 0u) {
		EF_U32(0x001f6fe8u) = 0u;
		EF_U32(0x001f6fecu) = 0u;
		return;
	}
	if (((int32_t)EF_U32(ub + 0x26u) >> 16) != 0xff) {
		rows = EF_U32(0x0019bb58u);
		rb = (uint32_t)(((int32_t)EF_U32(ub + 0x26u) >> 16) * 0xca) + rows;
		ra = (uint32_t)(((int32_t)EF_U32(ua + 0x26u) >> 16) * 0xca) + rows;
		if (EF_U16(rb + 0x15u) == EF_U16(ra + 0x15u)) {
			EF_U32(ua + 0x150u) = 0u;
			EF_U32(ua + 0x154u) = 0u;
			return;
		}
	}
	memcpy(&g, ef_guest_memory_ptr(0x001f7018u), 8);
	{
		double p, q, r, s, u2, w;
		memcpy(&p, ef_guest_memory_ptr(ub + 0xa1u), 8);
		memcpy(&q, ef_guest_memory_ptr(ua + 0xa1u), 8);
		memcpy(&r, ef_guest_memory_ptr(ub + 0xb9u), 8);
		memcpy(&s, ef_guest_memory_ptr(ua + 0xb9u), 8);
		memcpy(&u2, ef_guest_memory_ptr(ub + 0xd1u), 8);
		memcpy(&w, ef_guest_memory_ptr(ua + 0xd1u), 8);
		dot = p * q + r * s + u2 * w;
	}
	memcpy(&c, ef_guest_memory_ptr(0x00181c58u), 8);
	if (g > c) {
		sel = (dot < 0.0) ? 3.0 : 5.0;
	} else {
		memcpy(&c, ef_guest_memory_ptr(0x00181c60u), 8);
		if (g < c)
			sel = (dot < 0.0) ? 2.0 : 4.0;
		else
			sel = 1.0;
	}
	memcpy(ef_guest_memory_ptr(0x001f6fe8u), &sel, 8);
	memcpy(&t, ef_guest_memory_ptr(0x001f7058u), 8);
	memcpy(&x, ef_guest_memory_ptr(0x001f6ff8u), 8);
	t = t * 2.0 + x + 1.0;
	memcpy(ef_guest_memory_ptr(0x001f7050u), &t, 8);
	if (0.0 > t) {
		t = 3.0;
		memcpy(ef_guest_memory_ptr(0x001f7050u), &t, 8);
	}
	x = indexed_object_distance((int16_t)a, (int16_t)b) * 0.0005 + 1.0;
	memcpy(&g, ef_guest_memory_ptr(0x001f7018u), 8);
	v = cos_b45a2(g) * 0.1;
	v = v / x;
	memcpy(&t, ef_guest_memory_ptr(ua + 0x150u), 8);
	v = v + t * 0.9;
	memcpy(ef_guest_memory_ptr(ua + 0x150u), &v, 8);
	memcpy(&sel, ef_guest_memory_ptr(0x001f6fe8u), 8);
	if (sel == 3.0) {
		EF_U32(ua + 0x150u) = 0u;
		EF_U32(ua + 0x154u) = 0u;
	}
}

/* 0x9ac84, 448 bytes. Debug dump (fprintf-style, 0xbc400) of the state block A: the high words of the dwords at 0x40cfb2..0x40cfd4 are printed to the stream in AX, one format string per field with a separator string (0x18b12c) after each group. */
void dump_state_block_a_9ac84(ef_addr32 stream)
{
	guest_fn_bc400(stream, 0x0013185cu);
	guest_fn_bc400(stream, 0x00131871u);
	guest_fn_bc400(stream, 0x00131894u, (uint32_t)((int32_t)EF_U32(0x0040cfb2u) >> 16));
	guest_fn_bc400(stream, 0x001318abu, (uint32_t)((int32_t)EF_U32(0x0040cfb4u) >> 16));
	guest_fn_bc400(stream, 0x001318c5u, (uint32_t)((int32_t)EF_U32(0x0040cfb6u) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x001318deu, (uint32_t)((int32_t)EF_U32(0x0040cfb8u) >> 16));
	guest_fn_bc400(stream, 0x001318f0u, (uint32_t)((int32_t)EF_U32(0x0040cfbau) >> 16));
	guest_fn_bc400(stream, 0x00131905u, (uint32_t)((int32_t)EF_U32(0x0040cfbcu) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131919u, (uint32_t)((int32_t)EF_U32(0x0040cfbeu) >> 16));
	guest_fn_bc400(stream, 0x00131930u, (uint32_t)((int32_t)EF_U32(0x0040cfc0u) >> 16));
	guest_fn_bc400(stream, 0x0013194au, (uint32_t)((int32_t)EF_U32(0x0040cfc2u) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131963u, (uint32_t)((int32_t)EF_U32(0x0040cfcau) >> 16));
	guest_fn_bc400(stream, 0x0013197cu, (uint32_t)((int32_t)EF_U32(0x0040cfccu) >> 16));
	guest_fn_bc400(stream, 0x00131998u, (uint32_t)((int32_t)EF_U32(0x0040cfceu) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x001319b3u, (uint32_t)((int32_t)EF_U32(0x0040cfd0u) >> 16));
	guest_fn_bc400(stream, 0x001319c4u, (uint32_t)((int32_t)EF_U32(0x0040cfd2u) >> 16));
	guest_fn_bc400(stream, 0x001319d8u, (uint32_t)((int32_t)EF_U32(0x0040cfd4u) >> 16));
	guest_fn_bc400(stream, 0x0018b128u);
}

/* 0x9ae44, 517 bytes. Debug dump (fprintf-style, 0xbc400) of the state block B: the high words of the dwords at 0x40cf8e..0x40cfb0 are printed to the stream in AX, one format string per field with a separator string (0x18b12c) after each group. */
void dump_state_block_b_9ae44(ef_addr32 stream)
{
	guest_fn_bc400(stream, 0x001319ebu);
	guest_fn_bc400(stream, 0x001319fdu, (uint32_t)((int32_t)EF_U32(0x0040cf8eu) >> 16));
	guest_fn_bc400(stream, 0x00131a14u, (uint32_t)((int32_t)EF_U32(0x0040cf90u) >> 16));
	guest_fn_bc400(stream, 0x00131a2du, (uint32_t)((int32_t)EF_U32(0x0040cf92u) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131a46u, (uint32_t)((int32_t)EF_U32(0x0040cf94u) >> 16));
	guest_fn_bc400(stream, 0x00131a5du, (uint32_t)((int32_t)EF_U32(0x0040cf96u) >> 16));
	guest_fn_bc400(stream, 0x00131a77u, (uint32_t)((int32_t)EF_U32(0x0040cf98u) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131a90u, (uint32_t)((int32_t)EF_U32(0x0040cf9au) >> 16));
	guest_fn_bc400(stream, 0x00131aa3u, (uint32_t)((int32_t)EF_U32(0x0040cf9cu) >> 16));
	guest_fn_bc400(stream, 0x00131ab9u, (uint32_t)((int32_t)EF_U32(0x0040cf9eu) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131aceu, (uint32_t)((int32_t)EF_U32(0x0040cfa0u) >> 16));
	guest_fn_bc400(stream, 0x00131aedu, (uint32_t)((int32_t)EF_U32(0x0040cfa2u) >> 16));
	guest_fn_bc400(stream, 0x00131b0fu, (uint32_t)((int32_t)EF_U32(0x0040cfa4u) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131b30u, (uint32_t)((int32_t)EF_U32(0x0040cfa6u) >> 16));
	guest_fn_bc400(stream, 0x00131b4au, (uint32_t)((int32_t)EF_U32(0x0040cfa8u) >> 16));
	guest_fn_bc400(stream, 0x00131b66u, (uint32_t)((int32_t)EF_U32(0x0040cfaau) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131b82u, (uint32_t)((int32_t)EF_U32(0x0040cfacu) >> 16));
	guest_fn_bc400(stream, 0x00131b9eu, (uint32_t)((int32_t)EF_U32(0x0040cfaeu) >> 16));
	guest_fn_bc400(stream, 0x00131bbdu, (uint32_t)((int32_t)EF_U32(0x0040cfb0u) >> 16));
	guest_fn_bc400(stream, 0x0018b128u);
}

/* 0x9b04c, 600 bytes. Debug dump (fprintf-style, 0xbc400) of the state block C: the high words of the dwords at 0x40cfdc..0x40d004 are printed to the stream in AX, one format string per field with a separator string (0x18b12c) after each group. */
void dump_state_block_c_9b04c(ef_addr32 stream)
{
	guest_fn_bc400(stream, 0x00131bdbu);
	guest_fn_bc400(stream, 0x00131bf6u, (uint32_t)((int32_t)EF_U32(0x0040cfdcu) >> 16));
	guest_fn_bc400(stream, 0x00131c11u, (uint32_t)((int32_t)EF_U32(0x0040cfdeu) >> 16));
	guest_fn_bc400(stream, 0x00131c2fu, (uint32_t)((int32_t)EF_U32(0x0040cfe0u) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131c4cu, (uint32_t)((int32_t)EF_U32(0x0040cfe2u) >> 16));
	guest_fn_bc400(stream, 0x00131c67u, (uint32_t)((int32_t)EF_U32(0x0040cfe4u) >> 16));
	guest_fn_bc400(stream, 0x00131c85u, (uint32_t)((int32_t)EF_U32(0x0040cfe6u) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131ca2u, (uint32_t)((int32_t)EF_U32(0x0040cfe8u) >> 16));
	guest_fn_bc400(stream, 0x00131cbcu, (uint32_t)((int32_t)EF_U32(0x0040cfeau) >> 16));
	guest_fn_bc400(stream, 0x00131cd9u, (uint32_t)((int32_t)EF_U32(0x0040cfecu) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131cf5u, (uint32_t)((int32_t)EF_U32(0x0040cfeeu) >> 16));
	guest_fn_bc400(stream, 0x00131d0eu, (uint32_t)((int32_t)EF_U32(0x0040cff0u) >> 16));
	guest_fn_bc400(stream, 0x00131d2au, (uint32_t)((int32_t)EF_U32(0x0040cff2u) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131d45u, (uint32_t)((int32_t)EF_U32(0x0040cff4u) >> 16));
	guest_fn_bc400(stream, 0x00131d5bu, (uint32_t)((int32_t)EF_U32(0x0040cff6u) >> 16));
	guest_fn_bc400(stream, 0x00131d72u, (uint32_t)((int32_t)EF_U32(0x0040cff8u) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131d88u, (uint32_t)((int32_t)EF_U32(0x0040cffau) >> 16));
	guest_fn_bc400(stream, 0x00131d99u, (uint32_t)((int32_t)EF_U32(0x0040cffcu) >> 16));
	guest_fn_bc400(stream, 0x00131dadu, (uint32_t)((int32_t)EF_U32(0x0040cffeu) >> 16));
	guest_fn_bc400(stream, 0x0018b12cu);
	guest_fn_bc400(stream, 0x00131dc0u, (uint32_t)((int32_t)EF_U32(0x0040d000u) >> 16));
	guest_fn_bc400(stream, 0x00131dd1u, (uint32_t)((int32_t)EF_U32(0x0040d002u) >> 16));
	guest_fn_bc400(stream, 0x00131de5u, (uint32_t)((int32_t)EF_U32(0x0040d004u) >> 16));
	guest_fn_bc400(stream, 0x0018b128u);
}

/* 0xa520c, 281 bytes. Resize widget OBJ (EAX) to the given width DX and height BX (16-bit, negative values become 0; stored at +0x45 / +0x47), refresh it (0xa5070, 0xa558c) and, unless flag bit 0x10 of byte +0x3a is set, clamp it to the screen limits (words 0x15b744 / 0x15b748; origin words +0x41 / +0x43): without CL the size is shortened to fit, with CL non-zero the widget is moved instead (0xa5484). Finally update children and redraw (0xa51dc, 0xa5764, 0xa66a0, 0xa6038). */
void resize_widget_a520c(ef_addr32 obj, uint32_t dx, uint32_t bx, uint32_t cl)
{
	int16_t w = (int16_t)dx;
	int16_t h = (int16_t)bx;
	int32_t ox, oy, ow, oh;

	EF_U16(obj + 0x45u) = (uint16_t)(w > 0 ? (int32_t)w : 0);
	EF_U16(obj + 0x47u) = (uint16_t)(h > 0 ? (int32_t)h : 0);
	guest_fn_a5070(obj);
	guest_fn_a558c(obj);
	if ((EF_U8(obj + 0x3au) & 0x10u) == 0u) {
		if ((uint8_t)cl == 0u) {
			ox = (int16_t)EF_U16(obj + 0x41u);
			ow = (int16_t)EF_U16(obj + 0x45u);
			if (ox + ow >= (int32_t)EF_U32(0x0015b744u))
				EF_U16(obj + 0x45u) = (uint16_t)(EF_U16(0x0015b744u) - EF_U16(obj + 0x41u));
			oy = (int16_t)EF_U16(obj + 0x43u);
			oh = (int16_t)EF_U16(obj + 0x47u);
			if (oy + oh >= (int32_t)EF_U32(0x0015b748u))
				EF_U16(obj + 0x47u) = (uint16_t)(EF_U16(0x0015b748u) - EF_U16(obj + 0x43u));
		} else {
			ox = (int16_t)EF_U16(obj + 0x41u);
			ow = (int16_t)EF_U16(obj + 0x45u);
			if (ox + ow >= (int32_t)EF_U32(0x0015b744u))
				move_widget_a5484(obj, (int16_t)(EF_U16(0x0015b744u) - EF_U16(obj + 0x45u)), (int16_t)EF_U16(obj + 0x43u));
			oy = (int16_t)EF_U16(obj + 0x43u);
			oh = (int16_t)EF_U16(obj + 0x47u);
			if (oy + oh >= (int32_t)EF_U32(0x0015b748u))
				move_widget_a5484(obj, (int16_t)EF_U16(obj + 0x41u), (int16_t)(EF_U16(0x0015b748u) - EF_U16(obj + 0x47u)));
		}
	}
	guest_fn_a51dc(obj);
	guest_fn_a5764(obj);
	guest_fn_a66a0(obj);
	guest_fn_a6038();
}

/* 0xded86, 88 bytes. Median of three: return whichever of A (EAX), B (EDX) and C (EBX) lies between the other two according to the comparison callback CMP (ECX; called with EAX/EDX, result > 0 when the first is larger). */
uint32_t median_of_three_ded86(uint32_t a, uint32_t b, uint32_t c, uint32_t cmp)
{
	if ((int32_t)guest_callptr_eaxedx_ret_x(cmp, a, b) > 0) {
		if ((int32_t)guest_callptr_eaxedx_ret_x(cmp, a, c) > 0) {
			if ((int32_t)guest_callptr_eaxedx_ret_x(cmp, b, c) > 0)
				return b;
			return c;
		}
		return a;
	}
	if ((int32_t)guest_callptr_eaxedx_ret_x(cmp, a, c) >= 0)
		return a;
	if ((int32_t)guest_callptr_eaxedx_ret_x(cmp, b, c) > 0)
		return c;
	return b;
}

/* 0x42568, 314 bytes. Debug overlay of the 40 x 40 cell map at dword 0x12657c (nothing when NULL): every cell (row i, column j) is outlined by four lines (0xc95ac) over the 10 x 10 unit square at (10*j, 10*i), coloured with the cell's byte (value 10 is drawn as 15). */
void draw_cell_grid_42568(void)
{
	int16_t i, j, y0, y1, x0, x1;
	uint32_t colour;

	if (EF_U32(0x0012657cu) == 0u)
		return;
	y1 = 9;
	for (i = 0; i < 0x28; i++) {
		y0 = (int16_t)(i * 10);
		x1 = 9;
		for (j = 0; j < 0x28; j++) {
			colour = EF_U8(EF_U32(0x0012657cu) + (uint32_t)(i * 0x28 + j));
			if (colour == 0xau)
				colour = 0xfu;
			x0 = (int16_t)(j * 10);
			draw_scaled_rect_c95ac(colour, (uint32_t)(int32_t)x0, (uint32_t)(int32_t)y0, (uint32_t)(int32_t)x1, (uint32_t)(int32_t)y0);
			draw_scaled_rect_c95ac(colour, (uint32_t)(int32_t)x1, (uint32_t)(int32_t)y0, (uint32_t)(int32_t)x1, (uint32_t)(int32_t)y1);
			draw_scaled_rect_c95ac(colour, (uint32_t)(int32_t)x0, (uint32_t)(int32_t)y1, (uint32_t)(int32_t)x1, (uint32_t)(int32_t)y1);
			draw_scaled_rect_c95ac(colour, (uint32_t)(int32_t)x0, (uint32_t)(int32_t)y0, (uint32_t)(int32_t)x0, (uint32_t)(int32_t)y1);
			x1 = (int16_t)(x1 + 10);
		}
		y1 = (int16_t)(y1 + 10);
	}
}

/* 0x8b7e8, 317 bytes. Scan the UI record list file: clear the three counters (words 0x12db50, 0x12db4e, 0x12db4c), build the default path plus 0x18a864 in a local buffer and enumerate the entries with the scanner 0x8b684 (or the token reader 0x8b6e0 when dword 0x12afc8 is set; the choice is kept in 0x20f428). Each entry is validated with 0x8b650 and filed by its kind byte (0, 1, 2) into the pointer tables 0x20f240, 0x20f2e0 and 0x20f380 (at most 40 entries each, counted by the words 0x12db50, 0x12db4e, 0x12db4c respectively). */
void scan_ui_record_files_8b7e8(void)
{
	uint8_t path[0x78];
	uint8_t name[0x78];
	ef_addr32 pathp = (ef_addr32)(uintptr_t)path;
	ef_addr32 namep = (ef_addr32)(uintptr_t)name;
	uint32_t fn, p, kind, n;

	EF_U16(0x0012db50u) = 0u;
	EF_U16(0x0012db4eu) = 0u;
	EF_U16(0x0012db4cu) = 0u;
	build_default_path_9c980(pathp);
	strcat_9ca57(pathp, 0x0018a864u);
	fn = 0x0008b684u;
	EF_U32(0x0020f428u) = fn;
	if (EF_U32(0x0012afc8u) != 0u) {
		fn = 0x0008b6e0u;
		EF_U32(0x0020f428u) = fn;
	}
	if ((guest_callptr4_ret_x(EF_U32(0x0020f428u), 1u, pathp, 0x0018a860u, namep) & 0xffffu) == 0u)
		return;
	do {
		p = select_validated_ui_record(namep);
		if (p != 0u) {
			kind = EF_U8(p);
			if (kind == 0u) {
				if (((int32_t)EF_U32(0x0012db4eu) >> 16) < 0x28) {
					n = EF_U16(0x0012db50u);
					EF_U32(0x0020f240u + (uint32_t)((int32_t)(int16_t)n * 4)) = p;
					EF_U16(0x0012db50u) = (uint16_t)(n + 1u);
				}
			} else if (kind == 1u) {
				if (((int32_t)EF_U32(0x0012db4cu) >> 16) < 0x28) {
					n = EF_U16(0x0012db4eu);
					EF_U32(0x0020f2e0u + (uint32_t)((int32_t)(int16_t)n * 4)) = p;
					EF_U16(0x0012db4eu) = (uint16_t)(n + 1u);
				}
			} else if (kind == 2u) {
				if (((int32_t)EF_U32(0x0012db4au) >> 16) < 0x28) {
					n = EF_U16(0x0012db4cu);
					EF_U32(0x0020f380u + (uint32_t)((int32_t)(int16_t)n * 4)) = p;
					EF_U16(0x0012db4cu) = (uint16_t)(n + 1u);
				}
			}
		}
	} while ((guest_callptr4_ret_x(EF_U32(0x0020f428u), 0u, pathp, 0x0018a860u, namep) & 0xffffu) != 0u);
}

/* 0x5d080, 321 bytes. Draw a marker for the map point PTR (EAX; signed words x at +0, y at +2): the bearing from the map centre (words 0x1a6cfc / 0x1a6cfa) to the point is converted with 0xb3b1c, the sine/cosine tables (word tables 0x141e08 / 0x142e08, index angle/8) rotate the scaled radius (word 0x1a6cf4 << shift byte 0x12b369) around the centre, and six lines (0xc8ccc, colour byte 0x128148) form a diamond-like cross of half size 5 << shift around the resulting point (X, Y). */
void draw_bearing_marker_5d080(ef_addr32 ptr)
{
	uint32_t sh = EF_U8(0x0012b369u);
	int32_t r = (int32_t)(int16_t)(5u << sh);
	int32_t da = (int32_t)(int16_t)EF_U16(ptr) - (int32_t)(int16_t)EF_U16(0x001a6cfcu);
	int32_t db = (int32_t)(int16_t)EF_U16(ptr + 2u) - (int32_t)(int16_t)EF_U16(0x001a6cfau);
	uint32_t idx = guest_fn_b3b1c((uint32_t)da, (uint32_t)db) & 0xffffu;
	int32_t sn, cs, a2, X, Y, XP, YP, XM, YM;
	uint32_t col;

	idx >>= 3;
	sn = (int32_t)(int16_t)EF_U16(0x00141e08u + idx * 2u);
	cs = (int32_t)(int16_t)EF_U16(0x00142e08u + idx * 2u);
	a2 = (int32_t)((uint32_t)(int32_t)(int16_t)EF_U16(0x001a6cf4u) << sh);
	X = (int32_t)(int16_t)(((sn * a2) >> 16) + (int32_t)(int16_t)EF_U16(0x001a6cfcu));
	Y = (int32_t)(int16_t)(((a2 * cs) >> 16) + (int32_t)(int16_t)EF_U16(0x001a6cfau));
	XP = X + r;
	YP = Y + r;
	XM = X - r;
	YM = Y - r;
	col = EF_U8(0x00128148u);
	guest_fn_c8ccc_x((uint32_t)YM, (uint32_t)XM, (uint32_t)YP, (uint32_t)XP, col);
	guest_fn_c8ccc_x((uint32_t)YP, (uint32_t)XM, (uint32_t)YM, (uint32_t)XP, col);
	guest_fn_c8ccc_x((uint32_t)Y, (uint32_t)XP, (uint32_t)YM, (uint32_t)X, col);
	guest_fn_c8ccc_x((uint32_t)YP, (uint32_t)X, (uint32_t)Y, (uint32_t)XP, col);
	guest_fn_c8ccc_x((uint32_t)Y, (uint32_t)XM, (uint32_t)YP, (uint32_t)X, col);
	guest_fn_c8ccc_x((uint32_t)YM, (uint32_t)X, (uint32_t)Y, (uint32_t)XM, col);
}

/* 0x59998, 321 bytes. Draw the label of the selected tactical unit (unit id: signed word 0x1f71ca, via 0x7489c; nothing when missing). The anchor comes from the 15-byte entry 0x128170 + 15 * byte 0x1a6d1b: X = word +0, base Y = word +2, offset word +6 (times 3, divided by 6). When byte 0x1a81e5 is set a text (format 0x185e88) is drawn first at (word +2 minus the metric of that text, 0x594a4; offset - 0x18). Then the unit's weapon slot (signed byte at unit +0x1d7, 0x14 = none) selects a name pointer at 0x12bc6c + 145 * (signed byte at unit + 3 * slot + 0x19b); with a name, the count (word at unit + 3 * slot + 0x19c for slot 0x13, else 0x5aa10) is formatted with "%s %d"-style format 0x185e8c and drawn at (X, offset) (without a count the name pointer itself is the aux argument), and finally 0x59adc runs. */
void draw_selected_unit_label_59998(void)
{
	uint8_t buf[0x14];
	ef_addr32 bufp = (ef_addr32)(uintptr_t)buf;
	uint32_t row = (uint32_t)EF_U8(0x001a6d1bu) * 15u;
	int32_t off = (int32_t)EF_U32(0x00128174u + row) >> 16;
	int16_t x = (int16_t)EF_U16(0x00128170u + row);
	int32_t esi = (off * 3) / 6;
	uint32_t unit = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
	int32_t k, slot;
	uint32_t name, count;

	if (unit == 0u)
		return;
	if (EF_U8(0x001a81e5u) != 0u) {
		guest_fn_a4342_plain(bufp, 0x00185e88u);
		draw_tactical_coordinates((int16_t)(EF_U16(0x00128172u + row) - (uint16_t)draw_tactical_item_metric(bufp)),
		    (int16_t)(esi - 0x18), bufp);
	}
	k = (int32_t)EF_U32(unit + 0x1d4u) >> 24;
	if (k != 0x14) {
		slot = (int32_t)EF_U32(unit + (uint32_t)(k * 3) + 0x198u) >> 24;
		name = EF_U32(0x0012bc6cu + (uint32_t)(slot * 145));
	} else {
		name = 0u;
	}
	if (name == 0u)
		return;
	k = (int32_t)EF_U32(unit + 0x1d4u) >> 24;
	if (k == 0x13)
		count = EF_U16(unit + (uint32_t)(k * 3) + 0x19cu);
	else
		count = count_matching_activity_slots(unit) & 0xffffu;
	if ((uint16_t)count != 0u) {
		guest_fn_a4342_va(bufp, 0x00185e8cu, name, (uint32_t)(int32_t)(int16_t)count);
		draw_tactical_coordinates(x, (int16_t)esi, bufp);
	} else {
		draw_tactical_coordinates(x, (int16_t)esi, name);
	}
	guest_fn_59adc_w();
}

/* 0x5acf0, 384 bytes. Draw the three-line target readout of the tactical display. Skipped when the signed words 0x1a823e and 0x1a8238 are both -1, or when 0x1a823e is -1 and byte 0x1a81e1 is set. Latches the view words 0x1a6cf2, 0x1a6cf8, 0x1a6cf0, 0x1a6cf6 with 0xce328 and 0xcdf48, then draws three formatted texts (0xa4342) with 0x59420 at the entry's anchor X (word +2 of the 15-byte entry 0x128170 + 15 * byte 0x1a6d1b, minus the text metric) and Y = (3 * word +6) / 6, moved up by 6 for each following line: dword 0x19723c with 0x185ee8; table name dword 0x12818c[byte 0x197258] and dword 0x1971b0 with 0x185ef0; dword 0x197220 / 0x2aa with 0x185ef8. */
void draw_target_readout_5acf0(void)
{
	uint8_t buf[0x0c];
	ef_addr32 bufp = (ef_addr32)(uintptr_t)buf;
	uint32_t row = (uint32_t)EF_U8(0x001a6d1bu) * 15u;
	int32_t off = (int32_t)EF_U32(0x00128174u + row) >> 16;
	uint16_t di = EF_U16(0x00128172u + row);
	int32_t esi = (off * 3) / 6;
	int32_t a = (int32_t)EF_U32(0x001a823cu) >> 16;
	int32_t b = (int32_t)EF_U32(0x001a8236u) >> 16;

	if (a == -1 && b == -1)
		return;
	if (a == -1 && EF_U8(0x001a81e1u) != 0u)
		return;
	guest_fn_ce328_w((int32_t)EF_U32(0x001a6cf0u) >> 16, (int32_t)EF_U32(0x001a6cf6u) >> 16,
	    (int32_t)EF_U32(0x001a6ceeu) >> 16, (int32_t)EF_U32(0x001a6cf4u) >> 16);
	guest_fn_cdf48((uint32_t)((int32_t)EF_U32(0x001a6cf0u) >> 16), (uint32_t)((int32_t)EF_U32(0x001a6cf6u) >> 16),
	    (uint32_t)((int32_t)EF_U32(0x001a6ceeu) >> 16), (uint32_t)((int32_t)EF_U32(0x001a6cf4u) >> 16));
	guest_fn_a4342_va(bufp, 0x00185ee8u, EF_U32(0x0019723cu));
	draw_tactical_coordinates((int16_t)(di - (uint16_t)draw_tactical_item_metric(bufp)), (int16_t)esi, bufp);
	guest_fn_a4342_va(bufp, 0x00185ef0u, EF_U32(0x0012818cu + (uint32_t)EF_U8(0x00197258u) * 4u), EF_U32(0x001971b0u));
	esi -= 6;
	draw_tactical_coordinates((int16_t)(di - (uint16_t)draw_tactical_item_metric(bufp)), (int16_t)esi, bufp);
	guest_fn_a4342_va(bufp, 0x00185ef8u, (uint32_t)((int32_t)EF_U32(0x00197220u) / 0x2aa));
	esi -= 6;
	draw_tactical_coordinates((int16_t)(di - (uint16_t)draw_tactical_item_metric(bufp)), (int16_t)esi, bufp);
}

/* 0x433fc, 381 bytes. Walk the linked list whose head pointer is stored at HEAD (EDX; next link +4) for the group value A (EAX) and draw an icon for every entry of kind 0x10 (byte +0xa, index byte +0xb): when the positive, negative or zero group flag of the entry (0xcc22c, 0xcc1ac, 0xcc1ec; enabled by the words 0x126554, 0x126556, 0x126558) holds, the destination offset is dword 0x126570 plus the matching position pair (words 0x1342b9/0x1342bb, 0x1342b0/0x1342b2, 0x1342c2/0x1342c4; x + y * 640). The entry's record (dword +0x13; words +8 and +0xc) gives the icon position (x - 10, 400 - h - 10); the 20-pixel tile is blitted with 0xca620 and, when word 0x12656a is set and byte 0x1a53dc is clear, the sprite 0x1a is drawn at the same place (0x440f4). */
void draw_group_icons_433fc(uint32_t a, ef_addr32 head)
{
	uint32_t node = EF_U32(head);
	uint32_t ebp = 0u;
	uint32_t rec;
	int32_t x, y;
	uint8_t flag;

	while (node != 0u) {
		if (EF_U8(node + 0xau) == 0x10u) {
			flag = 0u;
			if ((uint16_t)group_flag_positive_cc22c(0u, EF_U8(node + 0xbu), a) != 0u && EF_U16(0x00126554u) != 0u) {
				ebp = EF_U32(0x00126570u) + (uint32_t)((int32_t)(int16_t)EF_U16(0x001342b9u) + (int32_t)(int16_t)EF_U16(0x001342bbu) * 0x280);
				flag = 1u;
			} else if ((uint16_t)group_flag_negative_cc1ac(0u, EF_U8(node + 0xbu), a) != 0u && EF_U16(0x00126556u) != 0u) {
				ebp = EF_U32(0x00126570u) + (uint32_t)((int32_t)(int16_t)EF_U16(0x001342b0u) + (int32_t)(int16_t)EF_U16(0x001342b2u) * 0x280);
				flag = 1u;
			} else if ((uint16_t)group_flag_zero_cc1ec(0u, EF_U8(node + 0xbu), a) != 0u && EF_U16(0x00126558u) != 0u) {
				ebp = EF_U32(0x00126570u) + (uint32_t)((int32_t)(int16_t)EF_U16(0x001342c2u) + (int32_t)(int16_t)EF_U16(0x001342c4u) * 0x280);
				flag = 1u;
			}
			if (flag != 0u) {
				rec = EF_U32(node + 0x13u);
				y = (int32_t)(int16_t)(0x190u - EF_U16(rec + 0xcu) - 10u);
				x = (int32_t)(int16_t)(EF_U16(rec + 8u) - 10u);
				guest_fn_ca620((int32_t)ebp, x, y, 0x14u, 0x14u, 0x280u);
				if (EF_U16(0x0012656au) != 0u && EF_U8(0x001a53dcu) == 0u)
					guest_fn_440f4_x(EF_U8(node + 0xbu), x, y, 0x14u, 0x1au, 0x280u);
			}
		}
		node = EF_U32(node + 4u);
	}
}

/* 0x44f68, 396 bytes. Sibling of 0x433fc for entries of kind 0x44, 0x45 and 0x46 (byte +0xb; index byte +0x9): walk the list at HEAD (EDX; link +4) for the group value A (EAX); the positive, negative and zero group flags (0xcc22c, 0xcc1ac, 0xcc1ec; enabled by the words 0x126554, 0x126556 and again 0x126556 - ORIGINAL BUG: the zero case tests the negative-flag enable word) pick the destination offset 0x126570 + x + y * 640 from the word pairs 0x134496/0x134498 or 0x13448d/0x13448f, and a true 0xbc7ec test picks the pair 0x134742/0x134744. The icon (words +0x16 and +0x1a of the entry, 400 - h - 10) is blitted with 0xca620 and, when word 0x12656a is set, sprite 0x1a is drawn (0x440f4). */
void draw_unit_icons_44f68(uint32_t a, ef_addr32 head)
{
	uint32_t node = EF_U32(head);
	uint32_t ebp = 0u;
	int32_t x, y;
	uint8_t flag, kind;

	while (node != 0u) {
		kind = EF_U8(node + 0xbu);
		if (kind == 0x44u || kind == 0x45u || kind == 0x46u) {
			flag = 0u;
			if ((uint16_t)group_flag_positive_cc22c(0u, EF_U8(node + 9u), a) != 0u && EF_U16(0x00126554u) != 0u) {
				ebp = EF_U32(0x00126570u) + (uint32_t)((int32_t)(int16_t)EF_U16(0x00134496u) + (int32_t)(int16_t)EF_U16(0x00134498u) * 0x280);
				flag = 1u;
			} else if ((uint16_t)group_flag_negative_cc1ac(0u, EF_U8(node + 9u), a) != 0u && EF_U16(0x00126556u) != 0u) {
				ebp = EF_U32(0x00126570u) + (uint32_t)((int32_t)(int16_t)EF_U16(0x0013448du) + (int32_t)(int16_t)EF_U16(0x0013448fu) * 0x280);
				flag = 1u;
			} else if ((uint16_t)group_flag_zero_cc1ec(0u, EF_U8(node + 9u), a) != 0u && EF_U16(0x00126556u) != 0u) {
				ebp = EF_U32(0x00126570u) + (uint32_t)((int32_t)(int16_t)EF_U16(0x0013448du) + (int32_t)(int16_t)EF_U16(0x0013448fu) * 0x280);
				flag = 1u;
			}
			if ((uint16_t)guest_fn_bc7ec(node) != 0u) {
				ebp = EF_U32(0x00126570u) + (uint32_t)((int32_t)(int16_t)EF_U16(0x00134742u) + (int32_t)(int16_t)EF_U16(0x00134744u) * 0x280);
				flag = 1u;
			}
			if (flag != 0u) {
				y = (int32_t)(int16_t)(0x190u - EF_U16(node + 0x1au) - 10u);
				x = (int32_t)(int16_t)(EF_U16(node + 0x16u) - 10u);
				guest_fn_ca620((int32_t)ebp, x, y, 0x14u, 0x14u, 0x280u);
				if (EF_U16(0x0012656au) != 0u)
					guest_fn_440f4_x(EF_U8(node + 9u), x, y, 0x14u, 0x1au, 0x280u);
			}
		}
		node = EF_U32(node + 4u);
	}
}

/* 0x90f50, 419 bytes. When byte 0x125847 is set, walk the 40 effect entries of 0x30 bytes at 0x211af0 (skipping entries with flag 2 in byte +0x24). An entry whose position dwords (+0, +4, +8; >> 8) are each within 0x1000 of the viewer position (dwords 0x21444c, 0x214448, 0x214450) and whose +4 value passes the scene gate (0x8bf1c) is published as a segment in the globals at 0x420904..0x42091c (viewer and entry positions) and the words 0x420930..0x42093a. If the check 0x7b7d8 then returns zero the entry's three words (+0xa, +0xc, +0xe) are stored with 0xa0ea8 for the record in the high word at +0x1e and the entry position (<< 8) is passed to 0x9e0c4. */
void update_effect_entries_90f50(void)
{
	int32_t i;
	uint32_t e;

	if (EF_U8(0x00125847u) == 0u)
		return;
	for (i = 0; i < 0x28; i++) {
		e = 0x00211af0u + (uint32_t)(i * 0x30);
		if ((EF_U8(e + 0x24u) & 2u) != 0u)
			continue;
		if ((int32_t)abs_cf94a(((int32_t)EF_U32(e) >> 8) - (int32_t)EF_U32(0x0021444cu)) > 0x1000)
			continue;
		if ((int32_t)abs_cf94a(((int32_t)EF_U32(e + 4u) >> 8) - (int32_t)EF_U32(0x00214448u)) > 0x1000)
			continue;
		if ((int32_t)abs_cf94a(((int32_t)EF_U32(e + 8u) >> 8) - (int32_t)EF_U32(0x00214450u)) > 0x1000)
			continue;
		if (scene_gate_allows((int32_t)EF_U32(e + 4u)) == 0u)
			continue;
		EF_U32(0x00420910u) = EF_U32(0x0021444cu);
		EF_U32(0x00420904u) = EF_U32(0x00214448u);
		EF_U32(0x00420918u) = EF_U32(0x00214450u);
		EF_U32(0x00420914u) = (uint32_t)((int32_t)EF_U32(e) >> 8);
		EF_U32(0x00420908u) = (uint32_t)((int32_t)EF_U32(e + 4u) >> 8);
		EF_U32(0x0042091cu) = (uint32_t)((int32_t)EF_U32(e + 8u) >> 8);
		EF_U16(0x00420930u) = (uint16_t)EF_U32(0x0021444cu);
		EF_U16(0x00420932u) = (uint16_t)EF_U32(0x00214448u);
		EF_U16(0x00420934u) = (uint16_t)EF_U32(0x00214450u);
		EF_U16(0x00420936u) = (uint16_t)EF_U32(0x00420914u);
		EF_U16(0x00420938u) = (uint16_t)EF_U32(0x00420908u);
		EF_U16(0x0042093au) = (uint16_t)EF_U32(0x0042091cu);
		if (guest_fn_7b7d8() != 0u)
			continue;
		guest_fn_a0ea8((int32_t)EF_U32(e + 0x1eu) >> 16, (uint32_t)((int32_t)EF_U32(e + 0xau) >> 16),
		    (int32_t)EF_U32(e + 0xcu) >> 16, (uint32_t)((int32_t)EF_U32(e + 0xeu) >> 16));
		guest_fn_9e0c4((int32_t)EF_U32(e + 0x1eu) >> 16, EF_U32(e) << 8, EF_U32(e + 4u) << 8, EF_U32(e + 8u) << 8);
	}
}

/* 0x564f4, 446 bytes. Handle an incoming unit state packet REC (EAX; signed unit id word +1, kind byte +3, reply code byte +4, words +5, +7, +9, +0xb, +0xd, byte +0xf). Ignored when the unit is missing, or when it is the selected unit (word 0x1f71ca), flag 0x127da8 is set and word 0x127dae is non-zero. Otherwise the packet words are copied to 0x197c28, 0x197c2a, 0x197c2c, 0x197c24, 0x197c26 and byte 0x121ec6 and the unit is updated with 0x74970. With flag 0x127da8 set, when byte 0x1a666b equals the unit's word +0x18 and the unit's bit 2 of word +0x24 differs from its value before the update, a reply is sent (0x577ac): -1 for another unit; for the selected unit code 0 when byte 0x12584b is 1 or when the unit named by word 0x197c28 is on another side, otherwise the score word 0x1a6398[owner of that unit] is reduced by 5. The player's counter word 0x1a6378[0x1a666b] is then incremented and word 0x127dae is set to 1500. */
void handle_unit_state_packet_564f4(ef_addr32 rec)
{
	int32_t id = (int32_t)(int16_t)EF_U16(rec + 1u);
	uint32_t unit = guest_fn_7489c(id);
	uint32_t other, rowa, rowb, rows;
	uint8_t before;
	uint32_t player;

	if (unit == 0u)
		return;
	if (EF_U16(0x001f71cau) == EF_U16(unit + 0x16u) && EF_U8(0x00127da8u) != 0u && EF_U16(0x00127daeu) != 0u)
		return;
	before = (uint8_t)((EF_U16(unit + 0x24u) >> 2) & 1u);
	EF_U16(0x00197c28u) = EF_U16(rec + 5u);
	EF_U16(0x00197c2au) = EF_U16(rec + 7u);
	EF_U16(0x00197c2cu) = EF_U16(rec + 9u);
	EF_U16(0x00197c24u) = EF_U16(rec + 0xbu);
	EF_U16(0x00197c26u) = EF_U16(rec + 0xdu);
	EF_U8(0x00121ec6u) = EF_U8(rec + 0xfu);
	guest_fn_74970(id, 0u, 0u, EF_U8(rec + 3u));
	if (EF_U8(0x00127da8u) == 0u)
		return;
	if (EF_U8(0x001a666bu) != EF_U16(unit + 0x18u))
		return;
	if ((uint32_t)((EF_U16(unit + 0x24u) >> 2) & 1u) == before)
		return;
	if (EF_U16(0x001f71cau) != EF_U16(rec + 1u)) {
		send_packet_2c(EF_U8(rec + 4u), 0xffu, 0u, 0u, 0u);
		return;
	}
	player = EF_U8(0x001a666bu);
	if (EF_U8(0x0012584bu) == 1u) {
		send_packet_2c(EF_U8(rec + 4u), (uint8_t)player, 0u, 0u, 0u);
	} else {
		other = guest_fn_7489c((int32_t)EF_U32(0x00197c26u) >> 16);
		if (other != 0u) {
			rows = EF_U32(0x0019bb58u);
			rowa = rows + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca);
			rowb = rows + (uint32_t)(((int32_t)EF_U32(other + 0x26u) >> 16) * 0xca);
			if (EF_U16(rowa + 0x15u) != EF_U16(rowb + 0x15u))
				send_packet_2c(EF_U8(rec + 4u), (uint8_t)player, 0u, 0u, 0u);
			else
				EF_U16(0x001a6398u + (uint32_t)(((int32_t)EF_U32(other + 0x16u) >> 16) * 2)) =
				    (uint16_t)(EF_U16(0x001a6398u + (uint32_t)(((int32_t)EF_U32(other + 0x16u) >> 16) * 2)) - 5u);
		}
	}
	EF_U16(0x001a6378u + player * 2u) = (uint16_t)(EF_U16(0x001a6378u + player * 2u) + 1u);
	EF_U16(0x00127daeu) = 0x5dcu;
}

/* 0x3c790, 441 bytes. Load the placement list of page AX (signed 16-bit) from the table at 0x1836d8 (0x3c730): allocate (0xcc6c0, tag 0x1836e0) one 21-byte record per count entry (word +0x2a of the record at dword 0x1264c8) into dword 0x1a2e84 and fill them from the 16-bit word stream. Each entry starts with its marker string (0x1836f4 is compared, 0x9d524, and a mismatch reports 0x1836fc with the entry number through the error hook); the zero words after the string are skipped, then a kind byte (+0), three coordinate words and an angle word follow. The coordinates are stored at +7, +0x9, +0xb rotated according to the view angle word 0x1a2ea4 (0x5a: +9 = -w1, +0xb = w2, +7 = w3; 0xb4: +7 = -w1, +0xb = w2, +9 = -w3; 0x10e: +9 = w1, +0xb = w2, +7 = -w3; otherwise +7 = w1, +0xb = w2, +9 = w3). The record's angle (+0x11) is the last word plus the view angle, wrapped by 0x168. */
void load_placement_list_3c790(uint32_t ax)
{
	uint32_t rec = EF_U32(0x001264c8u);
	uint32_t p = guest_table_entry_3c730(0x001836d8u, (int32_t)(int16_t)ax);
	uint32_t base, dest, p0, p1;
	int32_t cnt;
	int16_t i;
	uint16_t mode, w1, w2, w3;

	cnt = (int32_t)EF_U32(rec + 0x28u) >> 16;
	base = guest_fn_cc6c0_x((uint32_t)(cnt * 21), 0x001836e0u);
	EF_U32(0x001a2e84u) = base;
	for (i = 0; (int16_t)i < (int16_t)EF_U16(EF_U32(0x001264c8u) + 0x2au); i++) {
		if (guest_fn_9d524(0x001836f4u, p) != 0u)
			guest_callptr3_x(EF_U32(0x00132a74u), 0x001836fcu, EF_U32(0x001a2e80u), (uint32_t)((int32_t)i + 1));
		while (EF_U16(p) != 0u)
			p += 2u;
		while (EF_U16(p) == 0u)
			p += 2u;
		p0 = p;
		p1 = p0 + 2u;
		dest = base + (uint32_t)((int32_t)i * 21);
		EF_U8(dest) = EF_U8(p0);
		mode = EF_U16(0x001a2ea4u);
		w1 = EF_U16(p1);
		w2 = EF_U16(p1 + 2u);
		w3 = EF_U16(p1 + 4u);
		if (mode == 0x5au) {
			EF_U16(dest + 9u) = (uint16_t)(0u - (uint32_t)w1);
			EF_U16(dest + 0xbu) = w2;
			EF_U16(dest + 7u) = w3;
		} else if (mode == 0xb4u) {
			EF_U16(dest + 7u) = (uint16_t)(0u - (uint32_t)w1);
			EF_U16(dest + 0xbu) = w2;
			EF_U16(dest + 9u) = (uint16_t)(0u - (uint32_t)w3);
		} else if (mode == 0x10eu) {
			EF_U16(dest + 9u) = w1;
			EF_U16(dest + 0xbu) = w2;
			EF_U16(dest + 7u) = (uint16_t)(0u - (uint32_t)w3);
		} else {
			EF_U16(dest + 7u) = w1;
			EF_U16(dest + 0xbu) = w2;
			EF_U16(dest + 9u) = w3;
		}
		p = p1 + 6u;
		EF_U16(dest + 0x11u) = (uint16_t)(EF_U16(0x001a2ea4u) + EF_U16(p));
		p += 2u;
		if ((int32_t)(int16_t)EF_U16(dest + 0x11u) >= 0x168)
			EF_U16(dest + 0x11u) = (uint16_t)(EF_U16(dest + 0x11u) - 0x168u);
	}
}

/* 0x4430, 369 bytes. Apply price/score adjustments to dword 0x1970a0. SEL (AX) 0: find every entry of the 26-byte table at 0x121654 (37 entries; word +0 equal to ID, EDX) and add its dword (+2) times (2 - LEVEL, BL) to the total (negated when MODE, ECX, is 1), reporting each with the text 0x130b9d (name from the table at 0x132dd0, amount) through 0x78aa8. SEL 2: the same with the 40-entry table 0x121a18 and the names 0x1334f4 (index < 27) or 0x1333d4. SEL 3: add (2 - byte 0x197c2f) * 1000. */
void apply_total_adjustment_4430(uint32_t sel, uint32_t id, uint32_t level, uint32_t mode)
{
	uint8_t buf[0x50];
	ef_addr32 bufp = (ef_addr32)(uintptr_t)buf;
	int32_t i, v;
	uint16_t sid = (uint16_t)id;
	uint8_t lvl = (uint8_t)level;

	switch ((uint16_t)sel) {
	case 0:
		for (i = 0; i < 0x25; i++) {
			uint32_t row = 0x00121654u + (uint32_t)(i * 0x1a);
			if (EF_U16(row) != sid)
				continue;
			v = (int32_t)EF_U32(row + 2u) * (2 - (int32_t)lvl);
			if ((int16_t)mode == 1)
				v = -v;
			EF_U32(0x001970a0u) = EF_U32(0x001970a0u) + (uint32_t)v;
			guest_fn_a4342_va(bufp, 0x00130b9du, EF_U32(0x00132dd0u + (uint32_t)(i * 4)), (uint32_t)v);
			guest_fn_78aa8(bufp);
		}
		break;
	case 2:
		for (i = 0; i < 0x28; i++) {
			uint32_t row = 0x00121a18u + (uint32_t)(i * 0x1a);
			if (EF_U16(row) != sid)
				continue;
			v = (int32_t)EF_U32(row + 2u) * (2 - (int32_t)lvl);
			if ((int16_t)mode == 1)
				v = -v;
			EF_U32(0x001970a0u) = EF_U32(0x001970a0u) + (uint32_t)v;
			guest_fn_a4342_va(bufp, 0x00130b9du,
			    EF_U32((i < 0x1b ? 0x001334f4u : 0x001333d4u) + (uint32_t)(i * 4)), (uint32_t)v);
			guest_fn_78aa8(bufp);
		}
		break;
	case 3:
		EF_U32(0x001970a0u) = EF_U32(0x001970a0u) + (uint32_t)((2 - (int32_t)EF_U8(0x00197c2fu)) * 1000);
		break;
	default:
		break;
	}
}

/* 0x85d00, 369 bytes. Select the next weapon slot of the selected unit (signed word 0x1f71ca) whose weapon class byte (0x12bc94 + 0x91 * type) equals TAG (AL). The current slot is the signed byte at unit +0x1d7 (0x14 = none: the slot becomes 0x13 and 0x14 is returned); slots 0..0x13 are tried cyclically after it (slot 0x13 stores TAG in byte 0x12bd25 and is returned immediately). A slot qualifies when its count word (+0x19c) is non-zero, no earlier slot holds the same type with a count, its type differs from the current slot's and is not 0x21 or 0x22. Returns the slot number in BX, or 0x14 when the search wrapped round to the current slot. */
uint32_t next_weapon_slot_85d00(uint32_t tag)
{
	uint32_t unit = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
	int32_t cur = (int32_t)(int8_t)EF_U8(unit + 0x1d7u);
	int16_t bx = (int16_t)cur;
	int16_t cursel, ax;
	uint32_t edx, ebp;
	int32_t type;
	uint8_t ch;

	if (((int32_t)EF_U32(unit + 0x1d4u) >> 24) == 0x14) {
		EF_U8(unit + 0x1d7u) = 0x13u;
		return 0x14u;
	}
	cursel = (int16_t)(int8_t)EF_U8(unit + (uint32_t)(((int32_t)EF_U32(unit + 0x1d4u) >> 24) * 3) + 0x19bu);
	for (;;) {
		bx = (int16_t)(bx + 1);
		if ((int32_t)bx >= 0x14)
			bx = 0;
		if ((int32_t)bx == 0x13) {
			EF_U8(0x0012bd25u) = (uint8_t)tag;
			return (uint32_t)(uint16_t)bx;
		}
		if (bx == (int16_t)(int8_t)EF_U8(unit + 0x1d7u))
			return 0x14u;
		ch = 0u;
		for (ax = 0; ax < bx; ax++) {
			uint32_t e = unit + (uint32_t)((int32_t)ax * 3);
			if (EF_U8(e + 0x19bu) == EF_U8(unit + (uint32_t)((int32_t)bx * 3) + 0x19bu) && EF_U16(e + 0x19cu) != 0u)
				ch = 1u;
		}
		edx = unit + (uint32_t)((int32_t)bx * 3);
		type = (int32_t)EF_U32(edx + 0x198u) >> 24;
		ebp = (uint32_t)(type * 0x91);
		if (EF_U8(ebp + 0x0012bc94u) != (uint8_t)tag)
			continue;
		if (EF_U16(edx + 0x19cu) == 0u)
			continue;
		if (ch != 0u)
			continue;
		if ((int16_t)(int8_t)EF_U8(edx + 0x19bu) == cursel)
			continue;
		if (type == 0x21 || type == 0x22)
			continue;
		if (bx != (int16_t)(int8_t)EF_U8(unit + 0x1d7u) && (uint8_t)tag == EF_U8(ebp + 0x0012bc94u) && EF_U16(edx + 0x19cu) != 0u)
			return (uint32_t)(uint16_t)bx;
		return 0x14u;
	}
}

/* 0xca874, 391 bytes. Reset the entries of the list whose head pointer is stored at HEAD1 (EAX; next link +4) except those of state 7 (byte +0xd). With word 0x137992 set an entry is first cleared (state 0, byte +0x15 = 100, word +9 = 0). Word +9 is cleared; when words 0x137994 and 0x137992 are both zero the second list at HEAD2 (EDX) is searched: for every entry of mode 0 or 1 (byte +9) and kind other than 0x1b..0x1d (byte +0xa), each sub-record (0x7c bytes at dword +0x13, count byte +0x12) with a zero byte +3 lists word ids (dword +0x78, count byte +1); when one equals the entry's word +0x13 the entry's word +9 is set to 1. Then the three 4-byte fields at +0x29, +0x19, +0x21 are filled with 0xff (0xb3fb0), word +0x31 = 0xffff, words +0x51 and +0x53 = 0, the linked record at dword +0x4d gets byte +9 = 0, and unless the state is 1 or 2 it is reset to 0 (kept when 7) and byte +0x15 set to 100. */
void reset_list_entries_ca874(ef_addr32 head1, ef_addr32 head2)
{
	uint32_t node = EF_U32(head1);
	uint32_t p, ent, ptr, rec;
	uint32_t st;
	int16_t i;
	int32_t ax;

	while (node != 0u) {
		if (EF_U8(node + 0xdu) != 7u) {
			if (EF_U16(0x00137992u) != 0u) {
				EF_U8(node + 0xdu) = 0u;
				EF_U8(node + 0x15u) = 0x64u;
				EF_U16(node + 9u) = 0u;
			}
			EF_U16(node + 9u) = 0u;
			if (EF_U16(0x00137994u) == 0u && EF_U16(0x00137992u) == 0u) {
				p = EF_U32(head2);
				while (p != 0u) {
					if ((EF_U8(p + 9u) == 0u || EF_U8(p + 9u) == 1u) &&
					    EF_U8(p + 0xau) != 0x1bu && EF_U8(p + 0xau) != 0x1cu && EF_U8(p + 0xau) != 0x1du) {
						for (i = 0; (int32_t)i < (int32_t)EF_U8(p + 0x12u); i++) {
							ent = EF_U32(p + 0x13u) + (uint32_t)((int32_t)i * 0x7c);
							if (EF_U8(ent + 3u) != 0u)
								continue;
							for (ax = 0; ax < (int32_t)EF_U8(ent + 1u); ax++) {
								ptr = EF_U32(ent + 0x78u);
								if (EF_U16(node + 0x13u) == EF_U16(ptr + (uint32_t)(ax * 2)))
									EF_U16(node + 9u) = 1u;
							}
						}
					}
					p = EF_U32(p + 4u);
				}
			}
			guest_fn_b3fb0(node + 0x29u, 0xffffffffu, 4u);
			guest_fn_b3fb0(node + 0x19u, 0xffffffffu, 4u);
			guest_fn_b3fb0(node + 0x21u, 0xffffffffu, 4u);
			EF_U16(node + 0x31u) = 0xffffu;
			EF_U16(node + 0x51u) = 0u;
			rec = EF_U32(node + 0x4du);
			EF_U16(node + 0x53u) = 0u;
			if (rec != 0u)
				EF_U8(rec + 9u) = 0u;
			st = EF_U8(node + 0xdu);
			if (st != 1u && st != 2u) {
				if (st != 7u)
					EF_U8(node + 0xdu) = 0u;
				EF_U8(node + 0x15u) = 0x64u;
			}
		}
		node = EF_U32(node + 4u);
	}
}

/* 0xc4412, 121 bytes. Renderer setup (ret 4): VALUE (stack, byte) is stored in bytes 0x159138 and 0x15908a; the immediate operands of three self-patched instructions at 0xc6438, 0xc6497 and 0xc64e5 are set to 0x15913a and the eight dwords 0x159160, 0x159164, 0x15916c, 0x159170, 0x159178, 0x15917c, 0x159184 and 0x159188 to 0xd2500. */
void setup_renderer_tables_c4412(uint32_t value)
{
	EF_U32(0x000c6438u) = 0x0015913au;
	EF_U32(0x000c6497u) = 0x0015913au;
	EF_U32(0x000c64e5u) = 0x0015913au;
	EF_U8(0x00159138u) = (uint8_t)value;
	EF_U8(0x0015908au) = (uint8_t)value;
	EF_U32(0x0015916cu) = 0x000d2500u;
	EF_U32(0x00159178u) = 0x000d2500u;
	EF_U32(0x00159170u) = 0x000d2500u;
	EF_U32(0x0015917cu) = 0x000d2500u;
	EF_U32(0x00159184u) = 0x000d2500u;
	EF_U32(0x00159188u) = 0x000d2500u;
	EF_U32(0x00159160u) = 0x000d2500u;
	EF_U32(0x00159164u) = 0x000d2500u;
}

/* 0x53270, 149 bytes. Advance the smoothed frame time: the elapsed ticks (0x10688, low word as signed, at least 1) are stored in word 0x12afd4; above 10 the byte flag 0x127d9c is set and the step capped to 10, otherwise the flag is cleared. The double at 0x127d94 becomes (old * 3.0 + step) * 0.25 and the double 0x1a5b30 is set to step * 0.02. Finally word 0x127d92 receives the step (multiplied by 5 when byte 0x1a3647 is non-zero). */
void update_frame_time_53270(void)
{
	int32_t step = (int32_t)(int16_t)elapsed_ticks();
	double d, n, v;

	if (step < 1)
		step = 1;
	EF_U16(0x0012afd4u) = (uint16_t)step;
	if ((int32_t)(uint16_t)step > 10) {
		EF_U8(0x00127d9cu) = 1u;
		step = 10;
	} else {
		EF_U8(0x00127d9cu) = 0u;
	}
	n = (double)(int32_t)(uint16_t)step;
	memcpy(&d, ef_guest_memory_ptr(0x00127d94u), 8);
	v = (d * 3.0 + n) * 0.25;
	memcpy(ef_guest_memory_ptr(0x00127d94u), &v, 8);
	v = n * 0.02;
	memcpy(ef_guest_memory_ptr(0x001a5b30u), &v, 8);
	if (EF_U8(0x001a3647u) != 0u)
		step *= 5;
	EF_U16(0x00127d92u) = (uint16_t)step;
}

/* 0x4dfb4, 281 bytes. Find the first slot 1..0x17 whose box contains the point (X = low word of EAX, Y = low word of EDX; signed): the slot centre is the midpoint of its word pairs (x0 = 0x1a55a0, x1 = 0x1a55a2; y0 = 0x1a55d4, y1 = 0x1a55d6, indexed by slot * 2; halves truncated towards zero) and the half size is the float at 0x184d43 divided by the float at 0x1a53df. Slots whose type byte (0x1a5672 + slot) is 0x11 are skipped. Returns the slot number or -1. */
int32_t find_slot_at_point_4dfb4(uint32_t x, uint32_t y)
{
	int32_t slot;
	int16_t wa, wb;
	int32_t x0, y0, hx, hy;
	float num, den, r, cxf, cyf, xf, yf;
	double v;

	memcpy(&num, ef_guest_memory_ptr(0x00184d43u), 4);
	for (slot = 1; (int16_t)slot < 0x18; slot++) {
		x0 = (int16_t)EF_U16(0x001a55a0u + (uint32_t)slot * 2u);
		wa = (int16_t)EF_U16(0x001a55a2u + (uint32_t)slot * 2u);
		hx = ((int32_t)wa - x0) / 2;
		memcpy(&den, ef_guest_memory_ptr(0x001a53dfu), 4);
		r = (float)((double)num / (double)den);
		y0 = (int16_t)EF_U16(0x001a55d4u + (uint32_t)slot * 2u);
		wb = (int16_t)EF_U16(0x001a55d6u + (uint32_t)slot * 2u);
		hy = ((int32_t)wb - y0) / 2;
		wa = (int16_t)(x0 + hx);
		cxf = (float)wa;
		xf = (float)(int16_t)x;
		v = (double)wa - (double)r;
		if (v >= (double)xf)
			continue;
		if ((double)cxf + (double)r <= (double)xf)
			continue;
		wb = (int16_t)(y0 + hy);
		cyf = (float)wb;
		yf = (float)(int16_t)y;
		v = (double)wb - (double)r;
		if (v >= (double)yf)
			continue;
		if ((double)cyf + (double)r <= (double)yf)
			continue;
		if (EF_U8(0x001a5672u + (uint32_t)slot) == 0x11u)
			continue;
		return slot;
	}
	return -1;
}

/* 0x724a8, 305 bytes. Advance the position of unit object OBJ (EDX; NULL or byte 0x122c5c set: nothing) by its velocity for the time since the last call: the signed difference of word 0x1357d6 and the object's word +0x234 (stored back; negative counts as 0, capped at 150) is added to the accumulated word +0x236 (capped: at 150 the update stops there), and unless flag 8 of byte +0x24 is set the doubles +0x49, +0x51, +0x59 are advanced by the unit vector (+0xa1, +0xb9, +0xd1) times the speed (dword +0x12 * 7.857254924047374e-06) times the step (ticks * 0.02, times 5.0 when byte 0x1f9012 is set); a positive height +0x59 is clamped to 0.0. Then the object INDEX (EAX, signed word) is refreshed with 0x73d50. */
void advance_object_position_724a8(uint32_t index, uint32_t obj)
{
	uint32_t dt;
	int32_t diff, total;
	double a, b, v, p;

	if (obj == 0u || EF_U8(0x00122c5cu) != 0u)
		return;
	diff = (int32_t)(int16_t)(EF_U16(0x001357d6u) - EF_U16(obj + 0x234u));
	EF_U16(obj + 0x234u) = EF_U16(0x001357d6u);
	dt = (uint32_t)(uint16_t)diff;
	if ((dt & 0x8000u) != 0u)
		dt = 0u;
	if ((int32_t)dt > 0x96)
		dt = 0x96u;
	total = (int32_t)dt + (int32_t)EF_U16(obj + 0x236u);
	if (total >= 0x96) {
		EF_U16(obj + 0x236u) = 0x96u;
		return;
	}
	EF_U16(obj + 0x236u) = (uint16_t)total;
	if ((EF_U8(obj + 0x24u) & 8u) != 0u)
		return;
	a = (double)(int32_t)dt * 0.02;
	b = (double)(int32_t)EF_U32(obj + 0x12u) * 7.857254924047374e-06;
	if (EF_U8(0x001f9012u) != 0u)
		a = a * 5.0;
	memcpy(&v, ef_guest_memory_ptr(obj + 0xa1u), 8);
	memcpy(&p, ef_guest_memory_ptr(obj + 0x49u), 8);
	p = v * b * a + p;
	memcpy(ef_guest_memory_ptr(obj + 0x49u), &p, 8);
	memcpy(&v, ef_guest_memory_ptr(obj + 0xb9u), 8);
	memcpy(&p, ef_guest_memory_ptr(obj + 0x51u), 8);
	p = v * b * a + p;
	memcpy(ef_guest_memory_ptr(obj + 0x51u), &p, 8);
	memcpy(&v, ef_guest_memory_ptr(obj + 0xd1u), 8);
	memcpy(&p, ef_guest_memory_ptr(obj + 0x59u), 8);
	p = b * v * a + p;
	memcpy(ef_guest_memory_ptr(obj + 0x59u), &p, 8);
	if (!(0.0 >= p)) {
		EF_U32(obj + 0x59u) = 0u;
		EF_U32(obj + 0x5du) = 0u;
	}
	guest_fn_73d50_w((int32_t)(int16_t)index);
}

/* 0x89478, 321 bytes. Per-frame damping of the heading double at 0x1fd020 (X) by mode byte 0x12d7b2 (1..4; the mode is rewritten unchanged). Mode 1: X above -0.7453981634000001 or below -0.8253981634 becomes X - (X + 0.7853981634) * 0.16 * 0.3183098861827565. Mode 2: the double 0x1fcf98 gets -2200.0 added and, when X < -0.1745329252, X becomes X - X * 0.135 * 0.3183098861827565. Mode 3: like mode 1 with the bounds -0.2455993321454546 / -0.3255993321454546 and the offset 0.2855993321454546. Mode 4: 0x1fcf98 gets -10.0 added and, when X < -0.1745329252, X becomes X - X * 0.225 * 0.3183098861827565. */
void damp_heading_89478(void)
{
	uint8_t mode = EF_U8(0x0012d7b2u);
	double x, y;

	memcpy(&x, ef_guest_memory_ptr(0x001fd020u), 8);
	if (mode == 1u) {
		if (x > -0.7453981634000001 || x < -0.8253981634)
			x = x - (x + 0.7853981634) * 0.16 * 0.3183098861827565;
	}
	if (mode == 2u) {
		memcpy(&y, ef_guest_memory_ptr(0x001fcf98u), 8);
		y = y + -2200.0;
		memcpy(ef_guest_memory_ptr(0x001fcf98u), &y, 8);
		if (x < -0.1745329252)
			x = x - x * 0.135 * 0.3183098861827565;
	}
	if (mode == 3u) {
		if (x > -0.2455993321454546 || x < -0.3255993321454546)
			x = x - (x + 0.2855993321454546) * 0.16 * 0.3183098861827565;
	}
	if (mode == 4u) {
		memcpy(&y, ef_guest_memory_ptr(0x001fcf98u), 8);
		y = y + -10.0;
		memcpy(ef_guest_memory_ptr(0x001fcf98u), &y, 8);
		if (x < -0.1745329252)
			x = x - x * 0.225 * 0.3183098861827565;
	}
	memcpy(ef_guest_memory_ptr(0x001fd020u), &x, 8);
	EF_U8(0x0012d7b2u) = mode;
}

static double rd_f32_53618(uint32_t addr)
{
	float f;

	memcpy(&f, ef_guest_memory_ptr(addr), 4);
	return (double)f;
}

static void wr_f32_53618(uint32_t addr, double v)
{
	float f = (float)v;

	memcpy(ef_guest_memory_ptr(addr), &f, 4);
}

/* 0x53618, 322 bytes. Integrate the camera motion state. After refreshing the scene angle record 0x1a5bc0 (value 0xfa00, 0xd0dbc), three float accumulators are decreased by the scaled words at 0x1a5bc4, 0x1a5bd0, 0x1a5bdc (0x53308: word * float 0x185b50, times -1.0) added to 0x1a5c00, 0x1a5bb8, 0x1a5bbc, and the three rates 0x1a5ba0, 0x1a5ba4, 0x1a5ba8 are set from the scaled words 0x1a5bc8, 0x1a5bd4, 0x1a5be0 times dword 0x1a5be4 times 1/256. With the frame time T (word 0x127d92), 0x1a5c00 += rate0 * T, 0x1a5bb8 += rate1 * T and 0x1a5bbc += T * rate2. The dwords 0x1a5b4c, 0x1a5b38, 0x1a5b5c are cleared and byte 0x1a364e incremented. */
void integrate_camera_motion_53618(void)
{
	double t, s;

	update_scene_angle_second(0x001a5bc0u, 0xfa00u);
	t = convert_signed_short_scale((int16_t)EF_U16(0x001a5bc4u)) * -1.0;
	wr_f32_53618(0x001a5c00u, t + rd_f32_53618(0x001a5c00u));
	t = convert_signed_short_scale((int16_t)EF_U16(0x001a5bd0u)) * -1.0;
	wr_f32_53618(0x001a5bb8u, t + rd_f32_53618(0x001a5bb8u));
	t = convert_signed_short_scale((int16_t)EF_U16(0x001a5bdcu)) * -1.0;
	wr_f32_53618(0x001a5bbcu, t + rd_f32_53618(0x001a5bbcu));
	t = convert_signed_short_scale((int16_t)EF_U16(0x001a5bc8u)) * (double)(int32_t)EF_U32(0x001a5be4u);
	wr_f32_53618(0x001a5ba0u, t * 0.00390625);
	t = convert_signed_short_scale((int16_t)EF_U16(0x001a5bd4u)) * (double)(int32_t)EF_U32(0x001a5be4u);
	wr_f32_53618(0x001a5ba4u, t * 0.00390625);
	t = convert_signed_short_scale((int16_t)EF_U16(0x001a5be0u)) * (double)(int32_t)EF_U32(0x001a5be4u);
	wr_f32_53618(0x001a5ba8u, t * 0.00390625);
	t = (double)(int32_t)EF_U16(0x00127d92u);
	s = rd_f32_53618(0x001a5ba0u) * t;
	wr_f32_53618(0x001a5c00u, s + rd_f32_53618(0x001a5c00u));
	s = rd_f32_53618(0x001a5ba4u) * t;
	wr_f32_53618(0x001a5bb8u, s + rd_f32_53618(0x001a5bb8u));
	s = t * rd_f32_53618(0x001a5ba8u);
	EF_U32(0x001a5b4cu) = 0u;
	EF_U32(0x001a5b38u) = 0u;
	EF_U32(0x001a5b5cu) = 0u;
	EF_U8(0x001a364eu) = (uint8_t)(EF_U8(0x001a364eu) + 1u);
	wr_f32_53618(0x001a5bbcu, s + rd_f32_53618(0x001a5bbcu));
}

/* 0x6a3e4, 223 bytes. Bearing of unit B (EDX, signed id) as seen from unit A (EAX, signed id), returned in ST0; 0.0 when either unit is missing. When B's word +0x146 equals A's id the result is the wrapped angle pi - double +0x148 of B (0xb42f0). Otherwise the vector from A to B (doubles +0x49, +0x51, +0x59) is converted with 0x6a538 and the result is atan2(sqrt(y*y + z*z), x) (0xb4620, with squares from 0xb3fd0 and the root 0xa4470). */
double unit_bearing_6a3e4(uint32_t a, uint32_t b)
{
	int32_t ia = (int32_t)(int16_t)a;
	int32_t ib = (int32_t)(int16_t)b;
	uint32_t ua = guest_fn_7489c(ia);
	uint32_t ub = guest_fn_7489c(ib);
	double d[3], o[3], v, w, r;

	if (ua == 0u || ub == 0u)
		return 0.0;
	if (EF_U16(ub + 0x146u) == (uint32_t)ia) {
		memcpy(&v, ef_guest_memory_ptr(ub + 0x148u), 8);
		return wrap_angle_b42f0(3.1415926536 - v);
	}
	memcpy(&v, ef_guest_memory_ptr(ub + 0x49u), 8);
	memcpy(&w, ef_guest_memory_ptr(ua + 0x49u), 8);
	d[0] = v - w;
	memcpy(&v, ef_guest_memory_ptr(ub + 0x51u), 8);
	memcpy(&w, ef_guest_memory_ptr(ua + 0x51u), 8);
	d[1] = v - w;
	memcpy(&v, ef_guest_memory_ptr(ub + 0x59u), 8);
	memcpy(&w, ef_guest_memory_ptr(ua + 0x59u), 8);
	d[2] = v - w;
	guest_fn_6a538_x((ef_addr32)(uintptr_t)d, (ef_addr32)(uintptr_t)o, ib);
	v = square_double_b3fd0(o[1]);
	r = guest_fn_a4470(square_double_b3fd0(o[2]) + v);
	return atan2_b4620(r, o[0]);
}

/* 0xe6c54, 289 bytes. Update the atmosphere model from the altitude-like double P at 0x432270. 0x432da0 = (20.0 - P) / (20.0 + P) * [0x432da8]; above P = 10.0 it gets the correction (P - 10.0)^2 * 0.088 * 0.01 added and is raised to 0.088 when below that value. The dword pair 0x432dd8 is set to 288.15, 0x432dc0 = 288.15 - [0x432dc8] * [0x432dd0] (or [0x432dc8] * [0x432330] + 288.15 when -[0x432330] < [0x432dd0]). The derived values are 0x432d98 = sqrt([0x432dc0] * 401.8) (0xa4470), 0x432db0 = [0x432da0] * 0.287 * [0x432dc0] and 0x432db8 = [0x432dc0] * (0.287 * [0x432da8]). */
void update_atmosphere_e6c54(void)
{
	double p, m, q, w, r, s, z, v, f, t;

	memcpy(&p, ef_guest_memory_ptr(0x00432270u), 8);
	memcpy(&m, ef_guest_memory_ptr(0x00432da8u), 8);
	q = (20.0 - p) / (20.0 + p);
	q = q * m;
	memcpy(ef_guest_memory_ptr(0x00432da0u), &q, 8);
	if (p > 10.0) {
		memcpy(&p, ef_guest_memory_ptr(0x00432270u), 8);
		t = p + -10.0;
		w = t * (t * 0.088) * 0.01;
		memcpy(&q, ef_guest_memory_ptr(0x00432da0u), 8);
		w = w + q;
		memcpy(ef_guest_memory_ptr(0x00432da0u), &w, 8);
		if (w < 0.088) {
			w = 0.088;
			memcpy(ef_guest_memory_ptr(0x00432da0u), &w, 8);
		}
	}
	memcpy(&r, ef_guest_memory_ptr(0x00432dd0u), 8);
	memcpy(&s, ef_guest_memory_ptr(0x00432dc8u), 8);
	t = 288.15;
	memcpy(ef_guest_memory_ptr(0x00432dd8u), &t, 8);
	v = 288.15 - s * r;
	memcpy(ef_guest_memory_ptr(0x00432dc0u), &v, 8);
	memcpy(&z, ef_guest_memory_ptr(0x00432330u), 8);
	if (!(-z >= r)) {
		memcpy(&s, ef_guest_memory_ptr(0x00432dc8u), 8);
		v = s * z + 288.15;
		memcpy(ef_guest_memory_ptr(0x00432dc0u), &v, 8);
	}
	memcpy(&v, ef_guest_memory_ptr(0x00432dc0u), 8);
	f = guest_fn_a4470(v * 401.8);
	memcpy(ef_guest_memory_ptr(0x00432d98u), &f, 8);
	memcpy(&q, ef_guest_memory_ptr(0x00432da0u), 8);
	w = q * 0.287 * v;
	memcpy(ef_guest_memory_ptr(0x00432db0u), &w, 8);
	memcpy(&m, ef_guest_memory_ptr(0x00432da8u), 8);
	w = v * (0.287 * m);
	memcpy(ef_guest_memory_ptr(0x00432db8u), &w, 8);
}

/* 0x6f2bc, 290 bytes. Aim the chase camera at the point stored in the class row of the player unit (dword 0x1f71bc; row 0xca bytes, doubles +0x27, +0x2f, +0x37), unless the double 0x1f71b0 is below 1.0 or (for unit kinds 1 and 3, byte +0x2a) the check 0x6e390(1, 1) holds: heading 0x1f70f8 = atan2(y distance, x distance) (0xb4620), target height 0x1f7088 = z + (-304.8), distance 0x1f70a0 = 2d distance to the unit (0xb415c), the double 0x1f7100 is set to 100000.0 and the camera is started with 0x6ebe0 (3.0, 1.3, 0.0, 0.0, 1000.0 passed as ten stack dwords). */
void aim_chase_camera_6f2bc(void)
{
	uint32_t unit = EF_U32(0x001f71bcu);
	uint32_t row = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca);
	double p[3], lim, v, w, h;

	memcpy(&p[0], ef_guest_memory_ptr(row + 0x27u), 8);
	memcpy(&p[1], ef_guest_memory_ptr(row + 0x2fu), 8);
	memcpy(&p[2], ef_guest_memory_ptr(row + 0x37u), 8);
	memcpy(&lim, ef_guest_memory_ptr(0x001f71b0u), 8);
	if (1.0 > lim)
		return;
	if (EF_U8(unit + 0x2au) == 1u || EF_U8(unit + 0x2au) == 3u) {
		if ((uint8_t)guest_fn_6e390(1u, 1u) != 0u)
			return;
	}
	unit = EF_U32(0x001f71bcu);
	memcpy(&v, ef_guest_memory_ptr(unit + 0x49u), 8);
	memcpy(&w, ef_guest_memory_ptr(unit + 0x51u), 8);
	h = atan2_b4620(p[1] - w, p[0] - v);
	memcpy(ef_guest_memory_ptr(0x001f70f8u), &h, 8);
	v = p[2] + -304.8;
	memcpy(ef_guest_memory_ptr(0x001f7088u), &v, 8);
	h = distance_2d_b415c(unit + 0x49u, (ef_addr32)(uintptr_t)p);
	memcpy(ef_guest_memory_ptr(0x001f70a0u), &h, 8);
	EF_U32(0x001f7104u) = 0x40f86a00u;
	EF_U32(0x001f7100u) = 0u;
	guest_fn_6ebe0_x(0u, 0x40080000u, 0xcccccccdu, 0x3ff4ccccu, 0u, 0u, 0u, 0u, 0u, 0x408f4000u);
}

/* 0x6f3e0, 289 bytes. Variant of 0x6f2bc for a fixed height: when the double 0x1f71b0 is at least 1.0 and (for unit kinds 1 and 3) 0x6e390(0, byte row+0x5e) is false, the heading 0x1f70f8 is the bearing from the unit to the row point (doubles +0x27, +0x2f), the target 0x1f7088 is set to -5000.0, 0x1f70a0 is the distance to the unit and the camera is started with 0x6ebe0 like 0x6f2bc. */
void aim_camera_fixed_6f3e0(void)
{
	uint32_t unit, row;
	double p[3], lim, v, w, h;

	memcpy(&lim, ef_guest_memory_ptr(0x001f71b0u), 8);
	if (1.0 > lim)
		return;
	unit = EF_U32(0x001f71bcu);
	row = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca);
	memcpy(&p[0], ef_guest_memory_ptr(row + 0x27u), 8);
	memcpy(&p[1], ef_guest_memory_ptr(row + 0x2fu), 8);
	p[2] = -5000.0;
	if (EF_U8(unit + 0x2au) == 1u || EF_U8(unit + 0x2au) == 3u) {
		row = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(EF_U32(0x001f71bcu) + 0x26u) >> 16) * 0xca);
		if ((uint8_t)guest_fn_6e390(0u, EF_U8(row + 0x5eu)) != 0u)
			return;
	}
	unit = EF_U32(0x001f71bcu);
	memcpy(&v, ef_guest_memory_ptr(unit + 0x49u), 8);
	memcpy(&w, ef_guest_memory_ptr(unit + 0x51u), 8);
	h = atan2_b4620(p[1] - w, p[0] - v);
	memcpy(ef_guest_memory_ptr(0x001f7088u), &p[2], 8);
	memcpy(ef_guest_memory_ptr(0x001f70f8u), &h, 8);
	h = distance_2d_b415c(unit + 0x49u, (ef_addr32)(uintptr_t)p);
	memcpy(ef_guest_memory_ptr(0x001f70a0u), &h, 8);
	guest_fn_6ebe0_x(0u, 0x40080000u, 0xcccccccdu, 0x3ff4ccccu, 0u, 0u, 0u, 0u, 0u, 0x408f4000u);
}

/* 0x85604, 410 bytes (ret 8). Decide whether the class row ID (EAX, signed word) needs its target refreshed (0x857bc), given the factor SCALE (stack double). A row without a table (dword +0x8a zero) is reported through the error hook (0x189cb0). Nothing happens for the player's own row (word 0x19bb5e) while word 0x126516 and byte 0x132ac2 are clear and byte 0x125847 is set and the current entry (byte +0x8e of 25-byte entries) has kind 5. Otherwise the distance (0xb415c) from the row position (+0x61) to the entry point (dwords +0 and +4 times 2.4384) is compared: the refresh happens when double row+0x79 * SCALE exceeds the distance, or by row kind (byte 0x128568 + 0xe5 * byte row+0x19): 2 below 5000.0, 3 below 10000.0, 4 below 2000.0, otherwise when row+0x79 * 20.0 * 1.5 exceeds the distance. */
void check_row_target_85604(uint32_t id, double scale)
{
	int32_t ib = (int32_t)(int16_t)id;
	uint32_t rows = EF_U32(0x0019bb58u);
	uint32_t row = rows + (uint32_t)(ib * 0xca);
	uint32_t k, tbl, kind;
	double pt[3], dist, h, hd, c;

	if (EF_U32(row + 0x8au) == 0u)
		guest_callptr2_x(EF_U32(0x00132a74u), 0x00189cb0u, (uint32_t)ib);
	rows = EF_U32(0x0019bb58u);
	k = EF_U8(rows + (uint32_t)(ib * 0xca) + 0x8eu);
	if ((uint16_t)id == EF_U16(0x0019bb5eu) && EF_U16(0x00126516u) == 0u && EF_U8(0x00132ac2u) == 0u &&
	    EF_U8(0x00125847u) != 0u) {
		row = rows + (uint32_t)((int32_t)(int16_t)id * 0xca);
		if (EF_U8(EF_U32(row + 0x8au) + k * 25u + 0x18u) == 5u)
			return;
	}
	row = EF_U32(0x0019bb58u) + (uint32_t)(ib * 0xca);
	tbl = EF_U32(row + 0x8au);
	pt[0] = (double)(int32_t)EF_U32(tbl + k * 25u) * 2.4384;
	tbl = EF_U32(row + 0x8au);
	pt[1] = (double)(int32_t)EF_U32(tbl + k * 25u + 4u) * 2.4384;
	pt[2] = 0.0;
	dist = distance_2d_b415c(row + 0x61u, (ef_addr32)(uintptr_t)pt);
	row = EF_U32(0x0019bb58u) + (uint32_t)(ib * 0xca);
	memcpy(&hd, ef_guest_memory_ptr(row + 0x79u), 8);
	h = hd * 20.0;
	if (hd * scale > dist) {
		guest_fn_857bc_x(ib);
		return;
	}
	kind = EF_U8(0x00128568u + (uint32_t)EF_U8(row + 0x19u) * 0xe5u);
	if (kind == 3u) {
		c = 10000.0;
		if (dist >= c)
			return;
	} else if (kind == 2u) {
		c = 5000.0;
		if (dist >= c)
			return;
	} else if (kind == 4u) {
		c = 2000.0;
		if (dist >= c)
			return;
	} else {
		if (!(h * 1.5 > dist))
			return;
	}
	guest_fn_857bc_x(ib);
}

/* 0x89c64, 418 bytes. Load the flight model of aircraft record REC (EAX; type byte +0x18 selects the 145-byte row at 0x12bc99 + 145 * type): double 0x1fcca0 = frame time word 0x127d92 * 0.02, double 0x1fcf50 = row double +0, the dword pairs 0x1fcef8, 0x1fcc80 (or the row's alternates when byte 0x20f230 is set), 0x1fcf08, 0x1fcc88 are copied from the row, double 0x1fcee8 = row double (+0x3c) * pi * (1/180). The derived blocks are then computed (0x88c54 -> 0x1fcf58, 0x88c80 -> 0x1fcf88/0x1fcfd0, 0x88d4c -> 0x1fd020, 0x88d84 -> 0x1fcf48), the coefficient 0x1fcc90 = 0x6c1f0(double 0x1fcf68), the doubles 0x1fcc98 and 0x1fcf10 are set to constants (the latter 9.81 unless the type is 1, 9 or 0x14 and byte 0x20f230 is clear: then 0.0). */
void load_flight_model_89c64(ef_addr32 rec)
{
	uint32_t t = EF_U8(rec + 0x18u);
	uint32_t row = t * 145u;
	double d, x;

	d = (double)(uint32_t)EF_U16(0x00127d92u) * 0.02;
	memcpy(ef_guest_memory_ptr(0x001fcca0u), &d, 8);
	memcpy(ef_guest_memory_ptr(0x001fcf50u), ef_guest_memory_ptr(row + 0x0012bc99u), 8);
	EF_U32(0x001fcef8u) = EF_U32(row + 0x0012bccdu);
	EF_U32(0x001fcefcu) = EF_U32(row + 0x0012bcd1u);
	EF_U32(0x001fcc80u) = EF_U32(row + 0x0012bcbdu);
	EF_U32(0x001fcc84u) = EF_U32(row + 0x0012bcc1u);
	if (EF_U8(0x0020f230u) != 0u) {
		EF_U32(0x001fcc80u) = EF_U32(row + 0x0012bcb5u);
		EF_U32(0x001fcc84u) = EF_U32(row + 0x0012bcb9u);
	}
	EF_U32(0x001fcf08u) = EF_U32(row + 0x0012bcadu);
	EF_U32(0x001fcf0cu) = EF_U32(row + 0x0012bcb1u);
	EF_U32(0x001fcc88u) = EF_U32(row + 0x0012bcc5u);
	EF_U32(0x001fcc8cu) = EF_U32(row + 0x0012bcc9u);
	memcpy(&x, ef_guest_memory_ptr(row + 0x0012bcd5u), 8);
	d = x * 3.1415926536 * 0.005555555555555555;
	memcpy(ef_guest_memory_ptr(0x001fcee8u), &d, 8);
	guest_fn_88c54_x(rec, 0x001fcf58u);
	guest_fn_88c80_x(rec, 0x001fcf88u, 0x001fcfd0u);
	guest_fn_88d4c_x(rec, 0x001fd020u);
	convert_scene_scalar(rec, 0x001fcf48u);
	memcpy(&x, ef_guest_memory_ptr(0x001fcf68u), 8);
	d = estimate_coefficient_6c1f0(0u, 0u, 0u, 0u, x);
	memcpy(ef_guest_memory_ptr(0x001fcc90u), &d, 8);
	EF_U32(0x001fcc98u) = 0x211975d4u;
	EF_U32(0x001fcf14u) = 0x40239eb8u;
	EF_U32(0x001fcc9cu) = 0x3ee24d15u;
	EF_U32(0x001fcf10u) = 0x51eb851fu;
	t = EF_U8(rec + 0x18u);
	if ((t == 1u || t == 9u || t == 0x14u) && EF_U8(0x0020f230u) == 0u) {
		EF_U32(0x001fcf10u) = 0u;
		EF_U32(0x001fcf14u) = 0u;
	}
}

/* 0x4ce4c, 422 bytes. Hit-test the list at HEAD (EDX; link +4) for the group value A (EAX): the first entry of kind 0x31 (byte +0xb) with a non-zero word +0x36 and a true positive group flag (0xcc22c with index byte +9) whose box centre (word +0x16, 400 - word +0x1a; half size = float 0x184cf2 / float 0x1a53df) contains the cursor (words 0x41eef4 and 0x41eef2) is selected: the tactical slot state is set (bytes 0x1a54f3 = 1, 0x1a5672 = 1, 0x1a568b = 2, 0x1a559f = 1, the box words 0x1a55a2/0x1a55d4 and 0x1a55d6/0x1a5608, the entry in dword 0x1a5506), the record from 0xbfcfc(0x137958, entry) is stored in 0x1a550a with its byte (dword +0x33) in 0x1a5530 and the difference to the byte at dword +0x37 (capped at 16) in 0x1a5531, word 0x1a550e = lookup (0x9ddfc) of the entry's dword +0x10, and the slots are reset and the dialog refreshed (0x4d8f4, 0x4f8a8). */
void select_list_entry_at_cursor_4ce4c(uint32_t a, ef_addr32 head)
{
	uint32_t node = EF_U32(head);
	uint32_t rec, p1, p2;
	uint16_t xw, yw;
	float r, num, den, xf, yf, cx, cy;
	uint32_t d;

	while (node != 0u) {
		if (EF_U8(node + 0xbu) == 0x31u && EF_U16(node + 0x36u) != 0u &&
		    (uint16_t)group_flag_positive_cc22c(0u, EF_U8(node + 9u), a) != 0u) {
			memcpy(&num, ef_guest_memory_ptr(0x00184cf2u), 4);
			memcpy(&den, ef_guest_memory_ptr(0x001a53dfu), 4);
			r = (float)((double)num / (double)den);
			xw = EF_U16(node + 0x16u);
			yw = (uint16_t)(0x190u - EF_U16(node + 0x1au));
			cx = (float)(int16_t)xw;
			cy = (float)(int16_t)yw;
			xf = (float)(int16_t)EF_U16(0x0041eef4u);
			yf = (float)(int16_t)EF_U16(0x0041eef2u);
			if ((double)cx + (double)r > (double)xf && (double)cx - (double)r < (double)xf &&
			    (double)cy + (double)r > (double)yf && (double)cy - (double)r < (double)yf) {
				EF_U8(0x001a54f3u) = 1u;
				EF_U8(0x001a5672u) = 1u;
				EF_U16(0x001a55a2u) = xw;
				EF_U8(0x001a568bu) = 2u;
				EF_U16(0x001a55d6u) = yw;
				EF_U8(0x001a559fu) = 1u;
				EF_U16(0x001a55d4u) = xw;
				EF_U16(0x001a5608u) = yw;
				EF_U32(0x001a5506u) = node;
				rec = guest_fn_bfcfc(0x00137958u, node);
				p1 = EF_U32(rec + 0x33u);
				EF_U8(0x001a5530u) = EF_U8(p1);
				p2 = EF_U32(rec + 0x37u);
				EF_U32(0x001a550au) = rec;
				d = (uint32_t)(uint8_t)(EF_U8(p1) - EF_U8(p2));
				EF_U8(0x001a5531u) = (uint8_t)d;
				if (d > 0x10u)
					EF_U8(0x001a5531u) = 0x10u;
				EF_U16(0x001a550eu) = (uint16_t)guest_lookup_value(EF_U32(EF_U32(0x001a5506u) + 0x10u));
				reset_tactical_slots();
				refresh_button_dialog_4f8a8();
				return;
			}
		}
		node = EF_U32(node + 4u);
	}
}

/* 0x4c78c, 371 bytes. Sibling of 0x4ce4c: hit-test the list at HEAD (EDX; link +4) for the group value A (EAX) and the kind in word 0x1a54f4 (byte +0xb) with a true negative or zero group flag (0xcc1ac, 0xcc1ec; index byte +9). When the cursor (words 0x41eef4, 0x41eef2) lies within the box centre (word +0x16, 400 - word +0x1a) +- (float 0x184c74 / float 0x1a53df) the entry is selected: byte 0x1a54f3 = 1, dword 0x1a54f6 = entry, byte 0x1a559c = 1 when the negative flag holds, else 2, words 0x1a54fe and 0x1a5500 = centre, dword 0x1a54fa = 0, dword 0x1a5502 = 500, byte 0x1a559a = byte 0x1a559b, and 0x4dc98, 0x4d8f4, 0x4cff4 run. */
void select_list_entry_by_kind_4c78c(uint32_t a, ef_addr32 head)
{
	uint32_t node = EF_U32(head);
	uint16_t xw, yw;
	float r, num, den, xf, yf, cx, cy;

	while (node != 0u) {
		if ((uint16_t)EF_U8(node + 0xbu) == EF_U16(0x001a54f4u) &&
		    ((uint16_t)group_flag_negative_cc1ac(0u, EF_U8(node + 9u), a) != 0u ||
		     (uint16_t)group_flag_zero_cc1ec(0u, EF_U8(node + 9u), a) != 0u)) {
			memcpy(&num, ef_guest_memory_ptr(0x00184c74u), 4);
			memcpy(&den, ef_guest_memory_ptr(0x001a53dfu), 4);
			r = (float)((double)num / (double)den);
			xw = EF_U16(node + 0x16u);
			yw = (uint16_t)(0x190u - EF_U16(node + 0x1au));
			cx = (float)(int16_t)xw;
			cy = (float)(int16_t)yw;
			xf = (float)(int16_t)EF_U16(0x0041eef4u);
			yf = (float)(int16_t)EF_U16(0x0041eef2u);
			if ((double)cx + (double)r > (double)xf && (double)cx - (double)r < (double)xf &&
			    (double)cy + (double)r > (double)yf && (double)cy - (double)r < (double)yf) {
				EF_U8(0x001a54f3u) = 1u;
				EF_U32(0x001a54f6u) = node;
				EF_U8(0x001a559cu) = (uint8_t)(((uint16_t)group_flag_negative_cc1ac(0u, EF_U8(node + 9u), a) == 0u) + 1u);
				EF_U16(0x001a54feu) = xw;
				EF_U32(0x001a54fau) = 0u;
				EF_U16(0x001a5500u) = yw;
				EF_U8(0x001a559au) = EF_U8(0x001a559bu);
				EF_U32(0x001a5502u) = 0x1f4u;
				advance_first_tactical_status();
				reset_tactical_slots();
				fill_dialog_labels_4cff4();
				return;
			}
		}
		node = EF_U32(node + 4u);
	}
}

/* 0x2bcb8, 448 bytes. Compare class row AX (signed 16-bit) with its neighbour row N (0x2bbd0; 0xff or a row with byte +0x1a zero: result 0). The rows' current entries (byte +0x8e) are compared: if the row's is higher and its byte +0x1b is 2 the grid activity is decremented (0x85358) and 1 is returned; if lower and byte +0x1b is 2 the refresh 0x8516c runs and 0 is returned. With equal entries the distances (0xb415c) of both rows to their table points (dwords +0 and +4 of the 25-byte entries times 2.4384) decide: for byte +0x1b = 2 the result is 1 when the neighbour's distance - 3000.0 exceeds the row's distance, for 0xb when neighbour distance - 9000.0 exceeds it; otherwise 0. */
uint8_t row_wins_over_neighbour_2bcb8(uint32_t ax)
{
	int32_t x = (int32_t)(int16_t)ax;
	int32_t s = (int32_t)(int16_t)guest_fn_2bbd0(x);
	uint32_t rows = EF_U32(0x0019bb58u);
	uint32_t rx, rs, tab;
	uint8_t cl, ch;
	uint32_t k;
	double pt[3], dx, ds;

	if (s == 0xff)
		return 0u;
	rs = rows + (uint32_t)(s * 0xca);
	if (EF_U8(rs + 0x1au) == 0u)
		return 0u;
	rx = rows + (uint32_t)(x * 0xca);
	cl = EF_U8(rx + 0x8eu);
	ch = EF_U8(rs + 0x8eu);
	if (cl > ch) {
		if (EF_U8(rx + 0x1bu) == 2u)
			decrement_grid_activity((int16_t)x);
		return 1u;
	}
	if (cl < ch) {
		if (EF_U8(rx + 0x1bu) == 2u)
			guest_fn_8516c_x(x);
		return 0u;
	}
	k = cl;
	tab = EF_U32(rx + 0x8au);
	pt[0] = (double)(int32_t)EF_U32(tab + k * 25u) * 2.4384;
	tab = EF_U32(rx + 0x8au);
	pt[1] = (double)(int32_t)EF_U32(tab + k * 25u + 4u) * 2.4384;
	pt[2] = 0.0;
	dx = distance_2d_b415c(rx + 0x61u, (ef_addr32)(uintptr_t)pt);
	rs = EF_U32(0x0019bb58u) + (uint32_t)(s * 0xca);
	k = EF_U8(rs + 0x8eu);
	tab = EF_U32(rs + 0x8au);
	pt[0] = (double)(int32_t)EF_U32(tab + k * 25u) * 2.4384;
	tab = EF_U32(rs + 0x8au);
	pt[1] = (double)(int32_t)EF_U32(tab + k * 25u + 4u) * 2.4384;
	pt[2] = 0.0;
	ds = distance_2d_b415c(rs + 0x61u, (ef_addr32)(uintptr_t)pt);
	if (EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(x * 0xca) + 0x1bu) == 2u) {
		if (ds + -3000.0 > dx)
			return 1u;
		return 0u;
	}
	if (EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(x * 0xca) + 0x1bu) == 0xbu) {
		if (ds + -9000.0 > dx)
			return 1u;
	}
	return 0u;
}

/* 0x70278, 416 bytes. Per-frame update of the player unit's (dword 0x1f71bc) gun mount: look up the mount record M (0x8ffe8 with the signed word at unit +0x16a); nothing when missing. If M has flag 2 in byte +0x30 or a non-zero dword +0x1c and the unit has no grid link (0x6ae30 on the word at unit +0x16), the unit is released (0x6a1d0(word 0x1f71c8, 1)) and the function ends. For a row of kind 0xb (byte +0x1b) whose features 0xbc-byte, 6, 7, 9 and 0xf (0x2949c) all return 0x14 the unit is released the same way. Finally the vector (M dwords +8, +0, +4) * 2.4384 * (1/256) is passed with the row's byte +0xbc and its double +0xc1 to 0x6f758. */
void update_gun_mount_70278(void)
{
	uint32_t unit = EF_U32(0x001f71bcu);
	uint32_t m = guest_fn_8ffe8((int32_t)EF_U32(unit + 0x168u) >> 16);
	uint32_t row;
	double v[3];
	int ok;
	static const uint32_t feats[4] = { 6u, 7u, 9u, 0xfu };
	int i;

	if (m == 0u)
		return;
	if ((EF_U8(m + 0x30u) & 2u) != 0u || EF_U32(m + 0x1cu) != 0u) {
		if (guest_fn_6ae30_x((int32_t)EF_U32(unit + 0x14u) >> 16) == 0u) {
			guest_fn_6a1d0((int32_t)EF_U32(0x001f71c6u) >> 16, 1u);
			return;
		}
	}
	unit = EF_U32(0x001f71bcu);
	row = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca);
	if (EF_U8(row + 0x1bu) == 0xbu) {
		ok = (guest_fn_2949c((int32_t)EF_U32(unit + 0x14u) >> 16, EF_U8(row + 0xbcu)) & 0xffu) == 0x14u;
		for (i = 0; ok && i < 4; i++)
			ok = (guest_fn_2949c((int32_t)EF_U32(EF_U32(0x001f71bcu) + 0x14u) >> 16, feats[i]) & 0xffu) == 0x14u;
		if (ok)
			guest_fn_6a1d0((int32_t)EF_U32(0x001f71c6u) >> 16, 1u);
	}
	v[0] = (double)(int32_t)EF_U32(m + 8u) * 2.4384 * 0.00390625;
	v[1] = (double)(int32_t)EF_U32(m) * 2.4384 * 0.00390625;
	v[2] = (double)(int32_t)EF_U32(m + 4u) * 2.4384 * 0.00390625;
	row = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(EF_U32(0x001f71bcu) + 0x26u) >> 16) * 0xca);
	guest_fn_6f758_x((ef_addr32)(uintptr_t)v, EF_U8(row + 0xbcu), EF_U32(row + 0xc1u), EF_U32(row + 0xc5u));
}

/* 0x74d98, 320 bytes. Apply an impulse to unit AX (signed 16-bit id) from the force vector pair A (EDX) and B (EBX) (pointers to three doubles each; nothing when the unit is missing, either pointer is NULL or the double 0x1a5b30 is not positive). Both vectors are converted for the unit (0x6a538), their cross product (0xb43f4) O is taken and the unit's doubles +0x81, +0x79, +0x89 are increased by O0 * 0.05 / m, O1 * 0.05 / m and O2 * 0.003 / m (m = double 0x128569 + 0xe5 * byte unit+0xe9). For the selected unit (word 0x1f71ca) the camera accumulators 0x1a2ef8, 0x1a2f00, 0x1a2f08 also get O0 * 10.0 / [0x1a5b30], O1 / [0x1a5b30] and O2 / [0x1a5b30] added. */
void apply_impulse_74d98(uint32_t ax, ef_addr32 va, ef_addr32 vb)
{
	int32_t id = (int32_t)(int16_t)ax;
	uint32_t unit = guest_fn_7489c(id);
	double t0[3], t1[3], o[3], g, m, c1, v, w, inv;

	if (unit == 0u)
		return;
	memcpy(&g, ef_guest_memory_ptr(0x001a5b30u), 8);
	if (0.0 >= g)
		return;
	if (va == 0u || vb == 0u)
		return;
	guest_fn_6a538_x(va, (ef_addr32)(uintptr_t)t1, id);
	guest_fn_6a538_x(vb, (ef_addr32)(uintptr_t)t0, id);
	cross_product_doubles((ef_addr32)(uintptr_t)o, (ef_addr32)(uintptr_t)t1, (ef_addr32)(uintptr_t)t0);
	memcpy(&m, ef_guest_memory_ptr(0x00128569u + (uint32_t)EF_U8(unit + 0xe9u) * 0xe5u), 8);
	c1 = 0.05;
	v = o[0] * c1 / m;
	memcpy(&w, ef_guest_memory_ptr(unit + 0x81u), 8);
	w = v + w;
	memcpy(ef_guest_memory_ptr(unit + 0x81u), &w, 8);
	v = c1 * o[1] / m;
	memcpy(&w, ef_guest_memory_ptr(unit + 0x79u), 8);
	w = v + w;
	memcpy(ef_guest_memory_ptr(unit + 0x79u), &w, 8);
	v = o[2] * 0.003 / m;
	memcpy(&w, ef_guest_memory_ptr(unit + 0x89u), 8);
	w = v + w;
	memcpy(ef_guest_memory_ptr(unit + 0x89u), &w, 8);
	if ((uint16_t)ax == EF_U16(0x001f71cau)) {
		inv = 1.0 / g;
		v = o[0] * 10.0 * inv;
		memcpy(&w, ef_guest_memory_ptr(0x001a2ef8u), 8);
		w = v + w;
		memcpy(ef_guest_memory_ptr(0x001a2ef8u), &w, 8);
		v = o[1] * inv;
		memcpy(&w, ef_guest_memory_ptr(0x001a2f00u), 8);
		w = v + w;
		memcpy(ef_guest_memory_ptr(0x001a2f00u), &w, 8);
		v = inv * o[2];
		memcpy(&w, ef_guest_memory_ptr(0x001a2f08u), 8);
		w = v + w;
		memcpy(ef_guest_memory_ptr(0x001a2f08u), &w, 8);
	}
}

/* 0x6e3e0, 443 bytes (ret 0x10). Chase camera update with two stack doubles D1 (first) and D2 (second). Target height 0x1f7088 = player unit double +0x59 + [0x1f6f80]; distance 0x1f7100 starts as [0x1f6fe0] and grows by [0x1f6f70] * D2 (when [0x1f6f70] is positive; for unit mode byte +0x16c = 0x10 it is raised to [0x1f6fe0] + 1.0) or by [0x1f6f70] * D1 otherwise, and is capped by the aircraft limit (double 0x128599 + 0xe5 * byte unit+0xe9). Heading 0x1f70f8 = atan2([0x1f6f78], [0x1f71b0] * 15.0) + [0x1f6ff0]; 0x1f70a0 = 2000.0, 0x1f7050 = 1.0, and the camera runs through 0x6ebe0 with 3.0, 1.3, tan([0x1f6fd0]) * 9.81 / [0x1f6fe0] (0.0 when [0x1f6fe0] <= 1.0), [0x1f7060] and 2000.0 (ten stack dwords). */
void update_chase_camera_6e3e0(double d1, double d2)
{
	uint32_t unit = EF_U32(0x001f71bcu);
	double g, v, w, lim, tmp, a, b;
	uint32_t k;
	uint32_t lo4, hi4;

	memcpy(&v, ef_guest_memory_ptr(unit + 0x59u), 8);
	memcpy(&w, ef_guest_memory_ptr(0x001f6f80u), 8);
	v = v + w;
	memcpy(ef_guest_memory_ptr(0x001f7088u), &v, 8);
	EF_U32(0x001f7100u) = EF_U32(0x001f6fe0u);
	EF_U32(0x001f7104u) = EF_U32(0x001f6fe4u);
	memcpy(&g, ef_guest_memory_ptr(0x001f6f70u), 8);
	if (!(0.0 >= g)) {
		memcpy(&w, ef_guest_memory_ptr(0x001f7100u), 8);
		v = g * d2 + w;
		memcpy(ef_guest_memory_ptr(0x001f7100u), &v, 8);
		if (EF_U8(unit + 0x16cu) == 0x10u) {
			memcpy(&w, ef_guest_memory_ptr(0x001f6fe0u), 8);
			lim = 1.0 + w;
			if (!(v >= lim))
				memcpy(ef_guest_memory_ptr(0x001f7100u), &lim, 8);
		}
	} else {
		memcpy(&w, ef_guest_memory_ptr(0x001f7100u), 8);
		v = g * d1 + w;
		memcpy(ef_guest_memory_ptr(0x001f7100u), &v, 8);
	}
	memcpy(&a, ef_guest_memory_ptr(0x001f71b0u), 8);
	memcpy(&b, ef_guest_memory_ptr(0x001f6f78u), 8);
	v = atan2_b4620(b, a * 15.0);
	memcpy(&w, ef_guest_memory_ptr(0x001f6ff0u), 8);
	v = v + w;
	memcpy(ef_guest_memory_ptr(0x001f70f8u), &v, 8);
	k = EF_U8(EF_U32(0x001f71bcu) + 0xe9u);
	memcpy(&lim, ef_guest_memory_ptr(0x00128599u + k * 0xe5u), 8);
	memcpy(&v, ef_guest_memory_ptr(0x001f7100u), 8);
	if (!(v < lim))
		memcpy(ef_guest_memory_ptr(0x001f7100u), &lim, 8);
	EF_U32(0x001f70a0u) = 0u;
	EF_U32(0x001f70a4u) = 0x409f4000u;
	EF_U32(0x001f7050u) = 0u;
	EF_U32(0x001f7054u) = 0x3ff00000u;
	memcpy(&w, ef_guest_memory_ptr(0x001f6fe0u), 8);
	if (1.0 < w) {
		memcpy(&v, ef_guest_memory_ptr(0x001f6fd0u), 8);
		tmp = tan_b45fa(v) * 9.81 / w;
	} else {
		tmp = 0.0;
	}
	memcpy(&lo4, &tmp, 4);
	memcpy(&hi4, (const uint8_t *)&tmp + 4, 4);
	guest_fn_6ebe0_x(0u, 0x40080000u, 0xcccccccdu, 0x3ff4ccccu, lo4, hi4,
	    EF_U32(0x001f7060u), EF_U32(0x001f7064u), EF_U32(0x001f70a0u), EF_U32(0x001f70a4u));
}

/* 0x24c40, 533 bytes. Point the chase camera at unit AX (signed 16-bit id; 0x6a1b4; nothing but the height clamp when missing). The target height 0x1f7088 is limited to [0x1284d4] - 200.0. The unit is registered with 0x2479c (word 0x1f71c8, id, 0), the heading 0x1f70f8 is the wrapped bearing (0xb42f0 of atan2) from the player unit (dword 0x1f71bc) to it, and the height is the unit's double +0x59 (clamped again). If the player's class row (dword +0x26) lists another current unit (word +0x50 differing from word 0x1f71c8) that is alive (byte +0x24 flags 2 and 4 clear) the height comes from that unit and the distance 0x1f7100 from its double +0x91, raised to at least [0x1f7010]. The shake value 0x1f7050 = [0x1f7058] * 3.0 + [0x1f6ff8] + 1.0 (3.0 when negative) is set and the camera starts via 0x6ebe0 (2.5, 1.3, 0.0 x4, double 0x1f70a0 as ten stack dwords). */
void focus_camera_on_unit_24c40(uint32_t ax)
{
	int32_t id = (int32_t)(int16_t)ax;
	uint32_t u = guest_fn_6a1b4(id);
	uint32_t player, row, v;
	double lim, h, base, c, dx, dy;

	memcpy(&base, ef_guest_memory_ptr(0x001284d4u), 8);
	lim = base + -200.0;
	memcpy(&h, ef_guest_memory_ptr(0x001f7088u), 8);
	if (h > lim)
		memcpy(ef_guest_memory_ptr(0x001f7088u), &lim, 8);
	if (u == 0u)
		return;
	guest_fn_2479c_x((int32_t)EF_U32(0x001f71c6u) >> 16, id, 0u);
	player = EF_U32(0x001f71bcu);
	memcpy(&dx, ef_guest_memory_ptr(u + 0x49u), 8);
	memcpy(&c, ef_guest_memory_ptr(player + 0x49u), 8);
	dx = dx - c;
	memcpy(&dy, ef_guest_memory_ptr(u + 0x51u), 8);
	memcpy(&c, ef_guest_memory_ptr(player + 0x51u), 8);
	dy = dy - c;
	h = wrap_angle_b42f0(atan2_b4620(dy, dx));
	memcpy(ef_guest_memory_ptr(0x001f70f8u), &h, 8);
	EF_U32(0x001f7088u) = EF_U32(u + 0x59u);
	EF_U32(0x001f708cu) = EF_U32(u + 0x5du);
	memcpy(&base, ef_guest_memory_ptr(0x001284d4u), 8);
	lim = base + -200.0;
	memcpy(&h, ef_guest_memory_ptr(0x001f7088u), 8);
	if (h > lim)
		memcpy(ef_guest_memory_ptr(0x001f7088u), &lim, 8);
	row = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(EF_U32(0x001f71bcu) + 0x26u) >> 16) * 0xca);
	if ((int32_t)EF_U16(row + 0x50u) != ((int32_t)EF_U32(0x001f71c6u) >> 16)) {
		v = guest_fn_7489c((int32_t)EF_U32(row + 0x4eu) >> 16);
		if (v != 0u && (EF_U8(v + 0x24u) & 2u) == 0u && (EF_U8(v + 0x24u) & 4u) == 0u) {
			EF_U32(0x001f7088u) = EF_U32(v + 0x59u);
			EF_U32(0x001f708cu) = EF_U32(v + 0x5du);
			memcpy(&base, ef_guest_memory_ptr(0x001284d4u), 8);
			lim = base + -200.0;
			memcpy(&h, ef_guest_memory_ptr(0x001f7088u), 8);
			if (h > lim)
				memcpy(ef_guest_memory_ptr(0x001f7088u), &lim, 8);
			EF_U32(0x001f7100u) = EF_U32(v + 0x91u);
			EF_U32(0x001f7104u) = EF_U32(v + 0x95u);
			memcpy(&h, ef_guest_memory_ptr(0x001f7100u), 8);
			memcpy(&c, ef_guest_memory_ptr(0x001f7010u), 8);
			if (h < c)
				memcpy(ef_guest_memory_ptr(0x001f7100u), &c, 8);
		}
	}
	memcpy(&h, ef_guest_memory_ptr(0x001f7058u), 8);
	memcpy(&c, ef_guest_memory_ptr(0x001f6ff8u), 8);
	h = h * 3.0 + c + 1.0;
	memcpy(ef_guest_memory_ptr(0x001f7050u), &h, 8);
	if (0.0 > h) {
		EF_U32(0x001f7050u) = 0u;
		EF_U32(0x001f7054u) = 0x40080000u;
	}
	guest_fn_6ebe0_x(0u, 0x40040000u, 0xcccccccdu, 0x3ff4ccccu, 0u, 0u, 0u, 0u, EF_U32(0x001f70a0u), EF_U32(0x001f70a4u));
}

/* 0x2611c, 512 bytes. Variant of 0x24c40 for a side view of unit AX (signed 16-bit id; nothing when missing): the unit is registered with 0x2479c (word 0x1f71c8, id, 3), the heading 0x1f70f8 is the wrapped bearing from the player unit minus pi/2 (-1.5707963268 added), and the height 0x1f7088 comes from the unit's double +0x59 (limited to [0x1284d4] - 200.0). Another current unit of the player's row (word +0x50 differing from word 0x1f71c8) replaces height and distance (0x1f7100 from double +0x91, at least [0x1f7010]) unless its flags 4 or 8 are set, or flag 2 is set without 0x7574c holding for it. The shake value 0x1f7050 = 3.0 * ([0x1f7058] * 3.0 + [0x1f6ff8] + 1.0) (3.0 when negative) is set and the camera starts via 0x6ebe0 (5.0, 0.0, 2.5, 0.3, 0.0 x3, 1000.0). */
void focus_camera_side_2611c(uint32_t ax)
{
	int32_t id = (int32_t)(int16_t)ax;
	uint32_t u = guest_fn_6a1b4(id);
	uint32_t player, row, v;
	double lim, h, base, c, dx, dy;

	if (u == 0u)
		return;
	guest_fn_2479c_x((int32_t)EF_U32(0x001f71c6u) >> 16, id, 3u);
	player = EF_U32(0x001f71bcu);
	memcpy(&dx, ef_guest_memory_ptr(u + 0x49u), 8);
	memcpy(&c, ef_guest_memory_ptr(player + 0x49u), 8);
	dx = dx - c;
	memcpy(&dy, ef_guest_memory_ptr(u + 0x51u), 8);
	memcpy(&c, ef_guest_memory_ptr(player + 0x51u), 8);
	dy = dy - c;
	h = wrap_angle_b42f0(atan2_b4620(dy, dx) + -1.5707963268);
	memcpy(ef_guest_memory_ptr(0x001f70f8u), &h, 8);
	EF_U32(0x001f7088u) = EF_U32(u + 0x59u);
	EF_U32(0x001f708cu) = EF_U32(u + 0x5du);
	memcpy(&base, ef_guest_memory_ptr(0x001284d4u), 8);
	lim = base + -200.0;
	memcpy(&h, ef_guest_memory_ptr(0x001f7088u), 8);
	if (h > lim)
		memcpy(ef_guest_memory_ptr(0x001f7088u), &lim, 8);
	row = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(EF_U32(0x001f71bcu) + 0x26u) >> 16) * 0xca);
	if ((int32_t)EF_U16(row + 0x50u) != ((int32_t)EF_U32(0x001f71c6u) >> 16)) {
		v = guest_fn_7489c((int32_t)EF_U32(row + 0x4eu) >> 16);
		if (v != 0u && (EF_U8(v + 0x24u) & 4u) == 0u && (EF_U8(v + 0x24u) & 8u) == 0u &&
		    ((EF_U8(v + 0x24u) & 2u) == 0u || (uint8_t)guest_fn_7574c((int32_t)EF_U32(v + 0x14u) >> 16) != 0u)) {
			EF_U32(0x001f7088u) = EF_U32(v + 0x59u);
			EF_U32(0x001f708cu) = EF_U32(v + 0x5du);
			memcpy(&base, ef_guest_memory_ptr(0x001284d4u), 8);
			lim = base + -200.0;
			memcpy(&h, ef_guest_memory_ptr(0x001f7088u), 8);
			if (h > lim)
				memcpy(ef_guest_memory_ptr(0x001f7088u), &lim, 8);
			EF_U32(0x001f7100u) = EF_U32(v + 0x91u);
			EF_U32(0x001f7104u) = EF_U32(v + 0x95u);
			memcpy(&h, ef_guest_memory_ptr(0x001f7100u), 8);
			memcpy(&c, ef_guest_memory_ptr(0x001f7010u), 8);
			if (h < c)
				memcpy(ef_guest_memory_ptr(0x001f7100u), &c, 8);
		}
	}
	memcpy(&h, ef_guest_memory_ptr(0x001f7058u), 8);
	memcpy(&c, ef_guest_memory_ptr(0x001f6ff8u), 8);
	h = 3.0 * (h * 3.0 + c + 1.0);
	memcpy(ef_guest_memory_ptr(0x001f7050u), &h, 8);
	if (0.0 > h) {
		EF_U32(0x001f7050u) = 0u;
		EF_U32(0x001f7054u) = 0x40080000u;
	}
	guest_fn_6ebe0_x(0u, 0x40140000u, 0u, 0x40040000u, 0x33333333u, 0x3fd33333u, 0u, 0u, 0u, 0x408f4000u);
}

/* 0x26960, 483 bytes. Update the speed (dword +0x12) of the moving record REC (EAX) towards its target and advance it. The limit L is the signed word at 0x123813 + 164 * byte +0x3a. Three cases: when the owner's controller (dword owner +0x18, byte +0xc) is set and byte +0x3a is below 0x1b the speed is reduced by ticks * L / 100 (not below 0); when byte +0x3b is zero it grows by ticks * L / 100 up to 3 * L / 5; otherwise the position error to the target (dwords +0x54 - +0, +0x5c - +8, clamped to +-0x3fff) is projected onto the mount record's heading (word +0xe, sine/cosine tables 0x141e08 / 0x142e08), scaled (>> 15, / 64, clamped to +-L) and added to the mount record's speed (dword +0x12) with the result capped at L and floored at 0. Finally the record is advanced by speed * ticks (0xcef3c with +0xc). */
void update_record_speed_26960(ef_addr32 rec)
{
	uint32_t owner = EF_U32(rec + 0x34u);
	uint32_t n = EF_U8(rec + 0x3au);
	uint32_t m = EF_U8(rec + 0x3bu);
	uint32_t t, ctl, idx;
	int32_t lim, q, a, b, x, speed, ticks;
	uint32_t fid;

	if (EF_U8(0x00125996u + (uint32_t)EF_U8(owner + 0x25u) * 70u + m * 7u) == 0u)
		fid = EF_U32(owner + 0x26u) >> 16;
	else
		fid = EF_U32(owner + m * 2u + 0x24u) >> 16;
	t = guest_fn_8ffe8((int32_t)fid);
	lim = (int32_t)EF_U32(n * 164u + 0x00123811u) >> 16;
	ctl = EF_U32(EF_U32(rec + 0x34u) + 0x18u);
	ticks = (int32_t)EF_U16(0x00127d92u);
	if (EF_U8(ctl + 0xcu) != 0u && (int32_t)n < 0x1b) {
		q = (ticks * lim) / 100;
		speed = (int32_t)EF_U32(rec + 0x12u) - q;
		EF_U32(rec + 0x12u) = (uint32_t)speed;
		if (speed < 0)
			EF_U32(rec + 0x12u) = 0u;
	} else if (EF_U8(rec + 0x3bu) == 0u) {
		q = (ticks * lim) / 100;
		speed = (int32_t)EF_U32(rec + 0x12u) + q;
		EF_U32(rec + 0x12u) = (uint32_t)speed;
		if (lim * 3 / 5 < speed)
			EF_U32(rec + 0x12u) = (uint32_t)(lim * 3 / 5);
	} else {
		a = (int32_t)EF_U32(rec + 0x5cu) - (int32_t)EF_U32(rec + 8u);
		b = (int32_t)EF_U32(rec + 0x54u) - (int32_t)EF_U32(rec);
		EF_U32(0x0019b384u) = (uint32_t)a;
		EF_U32(0x0019b380u) = (uint32_t)b;
		if ((int32_t)EF_U32(0x0019b384u) > 0x3fff)
			EF_U32(0x0019b384u) = 0x3fffu;
		if ((int32_t)EF_U32(0x0019b384u) < -0x3fff)
			EF_U32(0x0019b384u) = (uint32_t)-0x3fff;
		if ((int32_t)EF_U32(0x0019b380u) > 0x3fff)
			EF_U32(0x0019b380u) = 0x3fffu;
		if ((int32_t)EF_U32(0x0019b380u) < -0x3fff)
			EF_U32(0x0019b380u) = (uint32_t)-0x3fff;
		idx = (uint32_t)EF_U16(t + 0xeu) >> 3;
		x = (((int32_t)(int16_t)EF_U16(0x00141e08u + idx * 2u) * (int32_t)EF_U32(0x0019b380u)) +
		     ((int32_t)(int16_t)EF_U16(0x00142e08u + idx * 2u) * (int32_t)EF_U32(0x0019b384u))) >> 15;
		q = x / 64;
		if (q > lim)
			q = lim;
		if (q < -lim)
			q = -lim;
		speed = (int32_t)EF_U32(t + 0x12u) + q;
		EF_U32(rec + 0x12u) = (uint32_t)speed;
		if (lim < speed)
			EF_U32(rec + 0x12u) = (uint32_t)lim;
		if (0 > (int32_t)EF_U32(rec + 0x12u))
			EF_U32(rec + 0x12u) = 0u;
	}
	advance_position_cef3c(rec, rec + 0xcu, (uint32_t)((int32_t)EF_U16(0x00127d92u) * (int32_t)EF_U32(rec + 0x12u)));
}

/* 0x255f0, 461 bytes. Chase camera for the player unit's (dword 0x1f71bc) wingman: the unit named by word +0x146 is looked up (0x7489c; nothing if missing), registered with 0x2479c (word 0x1f71c8, id, 0) and the weight 0x1f7068 = exp(-distance * 0.0025) (0xb415c, 0xdcb96) is stored. The heading 0x1f70f8 is the wrapped bearing to that unit plus weight * pi. The unit named by the high word of dword +0x41 (word +0x43) is then looked up; if it is missing, is not of mode 2 (byte +0x16c) with sub-mode 1 or 2 (byte +0x16d) or has flags 2, 4 or 8 set the current mode is updated (0x2546c) instead. Otherwise the height 0x1f7088 is that unit's double +0x59 (limited to [0x1284d4] - 200.0), the distance 0x1f7100 comes from the aircraft table (double 0x128591 + 0xe5 * byte +0xe9), the player's wingman word +0x146 is replaced when its double +0x15a is negative, 0x1f7050 = 1.0 and the camera starts via 0x6ebe0 (2.5, 1.3, 0.0 x4, double 0x1f70a0). */
void camera_follow_target_255f0(void)
{
	uint32_t p = EF_U32(0x001f71bcu);
	uint32_t t = guest_fn_7489c((int32_t)EF_U32(p + 0x144u) >> 16);
	uint32_t u, k;
	double dist, wgt, dx, dy, c, h, lim, base;
	uint8_t kind, sub, fl;

	if (t == 0u)
		return;
	guest_fn_2479c_x((int32_t)EF_U32(0x001f71c6u) >> 16, (int32_t)EF_U32(p + 0x144u) >> 16, 0u);
	dist = distance_2d_b415c(p + 0x49u, t + 0x49u);
	wgt = guest_fn_dcb96_x(-dist * 0.0025);
	memcpy(ef_guest_memory_ptr(0x001f7068u), &wgt, 8);
	p = EF_U32(0x001f71bcu);
	memcpy(&dx, ef_guest_memory_ptr(t + 0x49u), 8);
	memcpy(&c, ef_guest_memory_ptr(p + 0x49u), 8);
	dx = dx - c;
	memcpy(&dy, ef_guest_memory_ptr(t + 0x51u), 8);
	memcpy(&c, ef_guest_memory_ptr(p + 0x51u), 8);
	dy = dy - c;
	h = wrap_angle_b42f0(atan2_b4620(dy, dx) + wgt * 3.1415926536);
	memcpy(ef_guest_memory_ptr(0x001f70f8u), &h, 8);
	u = guest_fn_7489c((int32_t)EF_U32(EF_U32(0x001f71bcu) + 0x41u) >> 16);
	if (u == 0u) {
		update_current_mode_2546c();
		return;
	}
	kind = EF_U8(u + 0x16cu);
	sub = EF_U8(u + 0x16du);
	fl = EF_U8(u + 0x24u);
	if (kind != 2u || (sub != 2u && sub != 1u) || (fl & 2u) != 0u || (fl & 4u) != 0u || (fl & 8u) != 0u) {
		update_current_mode_2546c();
		return;
	}
	EF_U32(0x001f7088u) = EF_U32(u + 0x59u);
	EF_U32(0x001f708cu) = EF_U32(u + 0x5du);
	memcpy(&base, ef_guest_memory_ptr(0x001284d4u), 8);
	lim = base + -200.0;
	memcpy(&h, ef_guest_memory_ptr(0x001f7088u), 8);
	if (h > lim)
		memcpy(ef_guest_memory_ptr(0x001f7088u), &lim, 8);
	p = EF_U32(0x001f71bcu);
	k = EF_U8(p + 0xe9u);
	memcpy(ef_guest_memory_ptr(0x001f7100u), ef_guest_memory_ptr(0x00128591u + k * 0xe5u), 8);
	if (EF_U16(p + 0x146u) != EF_U16(u + 0x146u)) {
		memcpy(&c, ef_guest_memory_ptr(p + 0x15au), 8);
		if (0.0 > c)
			EF_U16(p + 0x146u) = EF_U16(u + 0x146u);
	}
	EF_U32(0x001f7050u) = 0u;
	EF_U32(0x001f7054u) = 0x3ff00000u;
	guest_fn_6ebe0_x(0u, 0x40040000u, 0xcccccccdu, 0x3ff4ccccu, 0u, 0u, 0u, 0u, EF_U32(0x001f70a0u), EF_U32(0x001f70a4u));
}

/* 0x26744, 539 bytes. Steer the moving record REC (EAX): turn its heading word +0xe towards the target. The owner is entry (signed word +0x38) of the 69-byte table at dword 0x1a14f8 and the mount record M is found as in 0x26960 (0x8ffe8). The turn gain is the word at 0x123811 + 164 * byte +0x3a. With byte +0x3b zero the bearing (0xb3b1c) goes to the point (dwords 0x19b388 / 0x19b390 minus the record's +0 / +8, >> 8); otherwise the bearing is taken in the mount's frame from the clamped (+-0x3fff) offsets to the target (dwords +0x54 / +0x5c, 0x19b380 / 0x19b384) plus M's heading. The turn is (bearing - heading) * gain / 0x7fff, quartered when the new heading would be farther from M's heading than the current one, and zeroed for a controlled owner (dword owner +0x18 byte +0xc) when byte +0x3a is below 0x1b or both offsets are below 10. Word +0x24 receives the turn and word +0xe grows by turn * ticks (word 0x127d92). */
void steer_record_26744(ef_addr32 rec)
{
	int32_t w = (int32_t)EF_U32(rec + 0x36u) >> 16;
	uint32_t owner = EF_U32(0x001a14f8u) + (uint32_t)(w * 69);
	uint32_t m = EF_U8(rec + 0x3bu);
	uint32_t n = EF_U8(rec + 0x3au);
	uint32_t fid, t, ctl, idx;
	int32_t dx, dz, gain, turn, a, ang;
	int16_t di;
	uint32_t e1, e2;

	if (EF_U8(0x00125996u + (uint32_t)EF_U8(owner + 0x25u) * 70u + m * 7u) == 0u)
		fid = EF_U32(owner + 0x26u) >> 16;
	else
		fid = EF_U32(owner + m * 2u + 0x24u) >> 16;
	t = guest_fn_8ffe8((int32_t)fid);
	gain = (int16_t)EF_U16(n * 164u + 0x00123811u);
	dx = ((int32_t)EF_U32(0x0019b388u) - (int32_t)EF_U32(rec)) >> 8;
	dz = ((int32_t)EF_U32(0x0019b390u) - (int32_t)EF_U32(rec + 8u)) >> 8;
	if (m == 0u) {
		ang = (int32_t)(int16_t)guest_fn_b3b1c((uint32_t)(int32_t)(int16_t)dx, (uint32_t)(int32_t)(int16_t)dz);
		ang = (int16_t)ang;
	} else {
		EF_U32(0x0019b384u) = EF_U32(rec + 0x5cu) - EF_U32(rec + 8u);
		EF_U32(0x0019b380u) = EF_U32(rec + 0x54u) - EF_U32(rec);
		if ((int32_t)EF_U32(0x0019b384u) > 0x3fff)
			EF_U32(0x0019b384u) = 0x3fffu;
		if ((int32_t)EF_U32(0x0019b384u) < -0x3fff)
			EF_U32(0x0019b384u) = (uint32_t)-0x3fff;
		if ((int32_t)EF_U32(0x0019b380u) > 0x3fff)
			EF_U32(0x0019b380u) = 0x3fffu;
		if ((int32_t)EF_U32(0x0019b380u) < -0x3fff)
			EF_U32(0x0019b380u) = (uint32_t)-0x3fff;
		idx = (uint32_t)EF_U16(t + 0xeu) >> 3;
		a = (((int32_t)(int16_t)EF_U16(0x00142e08u + idx * 2u) * (int32_t)EF_U32(0x0019b380u)) -
		     ((int32_t)(int16_t)EF_U16(0x00141e08u + idx * 2u) * (int32_t)EF_U32(0x0019b384u))) >> 15;
		if (a > 0x3fff)
			a = 0x3fff;
		if (a < -0x3fff)
			a = -0x3fff;
		ang = (int32_t)(uint16_t)((uint16_t)guest_fn_b3b1c((uint32_t)a, 0x3fffu) + EF_U16(t + 0xeu));
	}
	di = (int16_t)(uint16_t)((uint16_t)ang - EF_U16(rec + 0xeu));
	turn = ((int32_t)di * gain) / 0x7fff;
	di = (int16_t)(EF_U16(rec + 0xeu) + (uint32_t)turn);
	e1 = abs_a28f2(((int32_t)EF_U32(rec + 0xcu) >> 16) - ((int32_t)EF_U32(t + 0xcu) >> 16));
	e2 = abs_a28f2((int32_t)di - ((int32_t)EF_U32(t + 0xcu) >> 16));
	if ((int32_t)e1 < (int32_t)e2)
		turn = turn / 4;
	ctl = EF_U32(EF_U32(rec + 0x34u) + 0x18u);
	if (EF_U8(ctl + 0xcu) != 0u) {
		if ((int32_t)EF_U8(rec + 0x3au) < 0x1b ||
		    ((int32_t)EF_U32(0x0019b380u) < 0xa && (int32_t)EF_U32(0x0019b384u) < 0xa))
			turn = 0;
	}
	EF_U16(rec + 0x24u) = (uint16_t)turn;
	EF_U16(rec + 0xeu) = (uint16_t)(EF_U16(rec + 0xeu) + (uint16_t)(turn * (int32_t)EF_U16(0x00127d92u)));
}

/* 0x8883c, 142 bytes. Classify the unit record REC (EAX) for the radio menu: *OUT1 (EDX) is set to 4 and *OUT2 (EBX) to 0. Nothing more happens when 0xcd258 returns 100 or word 0x1f8ec2 is zero. Otherwise the scene group named 0x18a460 (0xa1a40) is searched for the entry numbered by that word (0xa1a90); without an entry *OUT1 becomes 6. With an entry its first byte, the unit's type byte +0x18 mapped through 0x8ac64 are looked up with 0xbbdc4 (kind 3) into *OUT2, which is passed to 0xcd2e8; *OUT1 is the percentage class (0x75620) of the new 0xcd258 result. */
void classify_radio_target_8883c(ef_addr32 rec, ef_addr32 out1, ef_addr32 out2)
{
	uint32_t name, entry;
	uint32_t dl, bl;

	EF_U8(out1) = 4u;
	EF_U8(out2) = 0u;
	if ((int16_t)guest_fn_cd258() == 0x64)
		return;
	if (EF_U16(0x001f8ec2u) == 0u)
		return;
	name = (uint32_t)lookup_name_value_a1a40(0x0018a460u);
	entry = guest_fn_a1a90((int32_t)EF_U32(0x001f8ec0u) >> 16, name);
	if (entry == 0u) {
		EF_U8(out2) = 0u;
		EF_U8(out1) = 6u;
		return;
	}
	dl = EF_U8(entry);
	bl = guest_fn_8ac64_x(EF_U8(rec + 0x18u)) & 0xffu;
	EF_U8(out2) = (uint8_t)lookup_mapped_code_bbdc4(3u, dl, bl);
	guest_fn_cd2e8(EF_U8(out2));
	EF_U8(out1) = (uint8_t)classify_object_percentage((uint8_t)(guest_fn_cd258() & 0xffu));
}

/* 0xa558c, 292 bytes. Recompute the scroll extent words of widget OBJ (EAX) along both axes. Horizontally N = word +0x45 + word +0x69 - word +0x61 - word +0x63 is compared with the visible size word +0x65 scaled by the double +0x9f; when N exceeds it the word +0x69 becomes trunc(size * scale - word +0x45 + word +0x63 + word +0x61) (0x9d72c). Vertically the same is done with words +0x47, +0x6b, +0x5d, +0x5f and +0x67 for word +0x6b. Both result words are clamped to non-negative values. */
void update_widget_extent_a558c(ef_addr32 obj)
{
	int32_t n, w;
	double p, scale;

	memcpy(&scale, ef_guest_memory_ptr(obj + 0x9fu), 8);
	n = (int32_t)(int16_t)EF_U16(obj + 0x45u) + ((int32_t)EF_U32(obj + 0x67u) >> 16) - (int32_t)EF_U16(obj + 0x61u) -
	    (int32_t)EF_U16(obj + 0x63u);
	w = (int32_t)EF_U16(obj + 0x65u);
	p = (double)w * scale;
	if ((double)n > p) {
		p = p - (double)(int32_t)(int16_t)EF_U16(obj + 0x45u) + (double)(int32_t)EF_U16(obj + 0x63u) +
		    (double)(int32_t)EF_U16(obj + 0x61u);
		EF_U16(obj + 0x69u) = (uint16_t)(int32_t)truncate_9d72c(p);
	}
	if ((int16_t)EF_U16(obj + 0x69u) < 0)
		EF_U16(obj + 0x69u) = 0u;
	n = ((int32_t)EF_U32(obj + 0x45u) >> 16) + ((int32_t)EF_U32(obj + 0x69u) >> 16) - (int32_t)EF_U16(obj + 0x5du) -
	    (int32_t)EF_U16(obj + 0x5fu);
	w = (int32_t)EF_U16(obj + 0x67u);
	p = (double)w * scale;
	if ((double)n > p) {
		p = p - (double)(int32_t)(int16_t)EF_U16(obj + 0x47u) + (double)(int32_t)EF_U16(obj + 0x5du) +
		    (double)(int32_t)EF_U16(obj + 0x5fu);
		EF_U16(obj + 0x6bu) = (uint16_t)(int32_t)truncate_9d72c(p);
	}
	if ((int16_t)EF_U16(obj + 0x6bu) < 0)
		EF_U16(obj + 0x6bu) = 0u;
}

/* 0x3cb2c, 250 bytes. Parse the placement script TEXT (EAX): normalise line endings, clear the list heads at 0x1a2e94/0x1a2e98, then for each line (0x3c994 parses it into the scratch fields at 0x1a2e80..0x1a2ea4) allocate a 0x38-byte record (pointer kept in 0x1264c8), link it at the list head and fill it from the scratch fields; finally expand the table entry and placement list named by the high word of 0x1a2ea0. */
void load_placement_script_3cb2c(ef_addr32 text)
{
	ef_addr32 cur, rec;
	uint32_t v;

	cur = text;
	normalize_script_line_endings(text);
	EF_U32(0x001a2e98u) = 0u;
	EF_U32(0x001a2e94u) = 0u;
	if (EF_U8(text) == 0u)
		return;
	do {
		cur = parse_table_line_3c994(cur);
		rec = alloc_zeroed_cc6c0(0u, 0u, 0u, 0u, 0x38u, 0x001837c8u);
		EF_U32(0x001264c8u) = rec;
		list_insert_head(rec, 0x001a2e98u, 0x001a2e94u);
		EF_U32(rec + 0xcu) = alloc_checked_cc744(EF_U32(0x001a2e88u));
		EF_U32(rec + 0x10u) = alloc_checked_cc744(EF_U32(0x001a2e80u));
		EF_U32(rec + 0x16u) = (uint32_t)parse_int(EF_U32(0x001a2e8cu));
		EF_U32(rec + 0x1au) = 0u;
		v = (uint32_t)parse_int(EF_U32(0x001a2e90u));
		rec = EF_U32(0x001264c8u);
		EF_U16(rec + 0x22u) = 0u;
		EF_U16(rec + 0x24u) = 0u;
		EF_U16(rec + 0x26u) = 0u;
		EF_U32(rec + 0x1eu) = v;
		EF_U16(rec + 0x14u) = EF_U16(0x001a2ea4u);
		copy_table_entry_to_record((int16_t)((int32_t)EF_U32(0x001a2ea0u) >> 16));
		load_placement_list_3c790((uint32_t)((int32_t)EF_U32(0x001a2ea0u) >> 16));
		EF_U32(EF_U32(0x001264c8u) + 0x2cu) = EF_U32(0x001a2e84u);
	} while (EF_U8(cur) != 0u);
}

static double rd_f64_73d50(ef_addr32 base, uint32_t off)
{
	double d;
	memcpy(&d, ef_guest_memory_ptr(base + off), 8);
	return d;
}

static uint32_t magic_low_73d50(double v)
{
	double t = v + 4503603922337792.0;
	uint64_t bits;
	memcpy(&bits, &t, 8);
	return (uint32_t)bits;
}

/* 0x73d50, 397 bytes. Convert the unit (AX, sign extended) record's floating point velocity/orientation fields to fixed point words using the add-2^52 rounding trick; the low dwords land at +0x12 (dword), +0x0c, +0x0e, +0x10 (words), +0x00, +0x04, +0x08 (dwords). When the byte at +0xe9 is zero the positions include the +0xa1/+0xb1/+0xb9/+0xc9/+0xd1/+0xe1 offsets scaled by 3.4834 and -1.0683. */
void store_fixed_point_state_73d50(int32_t unit)
{
	ef_addr32 u;
	double t20, t28, t18, t10, t08, t30, t00;
	const double c1 = 0.3183098861827565, c2 = 0.4101049868766404;
	const double k1 = 3.483428571428572, k2 = -1.068251428571429;

	u = guest_fn_7489c((int32_t)(int16_t)unit);
	if (u == 0u)
		return;
	t20 = rd_f64_73d50(u, 0x91u) * 65536.0 * 1.942;
	t28 = -rd_f64_73d50(u, 0x69u) * c1 * 32767.0;
	t18 = rd_f64_73d50(u, 0x61u) * c1 * 32767.0;
	t10 = -rd_f64_73d50(u, 0x71u) * c1 * 32767.0;
	if (EF_U8(u + 0xe9u) == 0u) {
		double x = rd_f64_73d50(u, 0x49u) + rd_f64_73d50(u, 0xa1u) * k1 + rd_f64_73d50(u, 0xb1u) * k2;
		double y = rd_f64_73d50(u, 0x51u) + rd_f64_73d50(u, 0xb9u) * k1 + rd_f64_73d50(u, 0xc9u) * k2;
		double z = rd_f64_73d50(u, 0x59u) + rd_f64_73d50(u, 0xd1u) * k1 + rd_f64_73d50(u, 0xe1u) * k2;
		t08 = x * c2 * 256.0;
		t30 = y * c2 * 256.0;
		t00 = z * c2 * 256.0;
	} else {
		t08 = rd_f64_73d50(u, 0x49u) * c2 * 256.0;
		t30 = rd_f64_73d50(u, 0x51u) * c2 * 256.0;
		t00 = rd_f64_73d50(u, 0x59u) * c2 * 256.0;
	}
	EF_U32(u + 0x12u) = magic_low_73d50(t20);
	EF_U16(u + 0x0cu) = (uint16_t)magic_low_73d50(t28);
	EF_U16(u + 0x0eu) = (uint16_t)magic_low_73d50(t18);
	EF_U16(u + 0x10u) = (uint16_t)magic_low_73d50(t10);
	EF_U32(u) = magic_low_73d50(t30);
	EF_U32(u + 4u) = magic_low_73d50(t00);
	EF_U32(u + 8u) = magic_low_73d50(t08);
}

static int32_t sin_tab_e95c0(uint32_t angle)
{
	return (int32_t)EF_U32(0x00141e06u + 2u * ((angle & 0xffffu) >> 3)) >> 16;
}

static int32_t cos_tab_e95c0(uint32_t angle)
{
	return (int32_t)EF_U32(0x00142e06u + 2u * ((angle & 0xffffu) >> 3)) >> 16;
}

/* 0xe95c0, 444 bytes. Build the 3x3 rotation matrix for the angles A (EAX), B (EDX), C (EBX) from the sine/cosine tables at 0x141e06/0x142e06 (product-to-sum identities, fixed point 15 bits) into dwords 0x464440..0x464460; 0x46445c holds sin(A). */
void build_rotation_matrix_e95c0(uint32_t a, uint32_t b, uint32_t c)
{
	int32_t p, q0, q1, q2, q3, r, m, t;
	uint32_t bc = b + c, bmc = b - c;

	p = sin_tab_e95c0(a);
	q0 = (sin_tab_e95c0(bc) + sin_tab_e95c0(bmc)) >> 1;
	q1 = (sin_tab_e95c0(bc) - sin_tab_e95c0(bmc)) >> 1;
	q2 = (cos_tab_e95c0(bmc) - cos_tab_e95c0(bc)) >> 1;
	q3 = (cos_tab_e95c0(bmc) + cos_tab_e95c0(bc)) >> 1;
	EF_U32(0x00464440u) = (uint32_t)(((q2 * p) >> 15) + q3);
	m = sin_tab_e95c0(a + c);
	EF_U32(0x00464444u) = (uint32_t)(-((m - sin_tab_e95c0(a - c)) >> 1));
	EF_U32(0x00464448u) = (uint32_t)(((q1 * p) >> 15) - q0);
	r = (cos_tab_e95c0(a + c) + cos_tab_e95c0(a - c)) >> 1;
	EF_U32(0x00464450u) = (uint32_t)r;
	t = ((q3 * p) >> 15) + q2;
	EF_U32(0x00464454u) = (uint32_t)(-t);
	EF_U32(0x0046444cu) = (uint32_t)(q1 - ((q0 * p) >> 15));
	EF_U32(0x0046445cu) = (uint32_t)p;
	EF_U32(0x00464458u) = (uint32_t)((sin_tab_e95c0(a + b) - sin_tab_e95c0(a - b)) >> 1);
	EF_U32(0x00464460u) = (uint32_t)((cos_tab_e95c0(a - b) + cos_tab_e95c0(a + b)) >> 1);
}

/* 0xe977c, 463 bytes. Sibling of 0xe95c0 with a different angle order: builds the rotation matrix for A (EAX), B (EDX), C (EBX) into dwords 0x464440..0x464460 (0x46445c holds -sin(A), 0x464458 is negated). */
void build_rotation_matrix_e977c(uint32_t a, uint32_t b, uint32_t c)
{
	int32_t p, u0, u1, v, w;
	uint32_t bc = b + c, bmc = b - c;

	p = sin_tab_e95c0(a);
	u1 = (sin_tab_e95c0(bc) + sin_tab_e95c0(bmc)) >> 1;
	u0 = (sin_tab_e95c0(bc) - sin_tab_e95c0(bmc)) >> 1;
	w = (cos_tab_e95c0(bc) + cos_tab_e95c0(bmc)) >> 1;
	v = (cos_tab_e95c0(bmc) - cos_tab_e95c0(bc)) >> 1;
	EF_U32(0x00464440u) = (uint32_t)(w - ((p * v) >> 15));
	EF_U32(0x00464450u) = (uint32_t)((cos_tab_e95c0(a - c) + cos_tab_e95c0(a + c)) >> 1);
	EF_U32(0x00464444u) = (uint32_t)((sin_tab_e95c0(a + c) - sin_tab_e95c0(a - c)) >> 1);
	EF_U32(0x00464448u) = (uint32_t)(((u0 * p) >> 15) + u1);
	EF_U32(0x00464454u) = (uint32_t)(((w * p) >> 15) - v);
	EF_U32(0x0046444cu) = (uint32_t)(u0 - ((u1 * p) >> 15));
	EF_U32(0x00464458u) = (uint32_t)(-((sin_tab_e95c0(a + b) - sin_tab_e95c0(a - b)) >> 1));
	EF_U32(0x0046445cu) = (uint32_t)(-p);
	EF_U32(0x00464460u) = (uint32_t)((cos_tab_e95c0(a - b) + cos_tab_e95c0(a + b)) >> 1);
}

static int32_t sin32_e7a7c(uint32_t off)
{
	return (int32_t)EF_U32(0x00137e08u + (off & 0x7ffcu));
}

static int32_t cos32_e7a7c(uint32_t off)
{
	return (int32_t)EF_U32(0x00139e08u + (off & 0x7ffcu));
}

static int32_t mulshift24_e7a7c(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)a * (int64_t)b) >> 24);
}

/* 0xe7a7c, 459 bytes. Higher precision rotation matrix builder: the angles in EBX, ESI, EDI (halved and masked to table offsets, kept as words at 0x15ffd4/6/8) index the dword sine/cosine tables at 0x137e08/0x139e08; products use 64-bit multiplies shifted by 24. Nine entries go to 0x1590c0..0x1590e0, then three rows of three are shifted right by 9 into 0x15ffda/0x15ffde/0x15ffe2 (stride 12). */
void build_rotation_matrix_e7a7c(uint32_t ebx, uint32_t esi, uint32_t edi)
{
	uint32_t x0 = (ebx >> 1) & 0x7ffcu, x1 = (esi >> 1) & 0x7ffcu, x2 = (edi >> 1) & 0x7ffcu;
	int32_t p0, s, d, t0, t1, c0, c1;
	uint32_t i;

	EF_U16(0x0015ffd4u) = (uint16_t)x0;
	EF_U16(0x0015ffd6u) = (uint16_t)x1;
	EF_U16(0x0015ffd8u) = (uint16_t)x2;
	p0 = sin32_e7a7c(x0);
	EF_U32(0x0015ffd0u) = (uint32_t)p0;
	s = sin32_e7a7c(x1 + x2);
	d = sin32_e7a7c(x1 - x2);
	EF_U32(0x001590ccu) = (uint32_t)(-((mulshift24_e7a7c(s + d, p0) + s - d) >> 1));
	EF_U32(0x001590c8u) = (uint32_t)((mulshift24_e7a7c(s - d, p0) + s + d) >> 1);
	s = cos32_e7a7c(x1 + x2);
	d = cos32_e7a7c(x1 - x2);
	EF_U32(0x001590d4u) = (uint32_t)((mulshift24_e7a7c(s + d, p0) + s - d) >> 1);
	EF_U32(0x001590c0u) = (uint32_t)((-mulshift24_e7a7c(d - s, p0) + s + d) >> 1);
	t0 = sin32_e7a7c(x0 - x2);
	c0 = cos32_e7a7c(x0 - x2);
	t1 = sin32_e7a7c(x0 + x2);
	c1 = cos32_e7a7c(x0 + x2);
	EF_U32(0x001590c4u) = (uint32_t)((t1 - t0) >> 1);
	EF_U32(0x001590d0u) = (uint32_t)((c1 + c0) >> 1);
	t0 = sin32_e7a7c(x0 - x1);
	c0 = cos32_e7a7c(x0 - x1);
	t1 = sin32_e7a7c(x0 + x1);
	c1 = cos32_e7a7c(x0 + x1);
	EF_U32(0x001590d8u) = (uint32_t)(-((t1 - t0) >> 1));
	EF_U32(0x001590dcu) = (uint32_t)(-p0);
	EF_U32(0x001590e0u) = (uint32_t)((c0 + c1) >> 1);
	for (i = 0u; i < 36u; i += 12u) {
		EF_U32(0x0015ffdau + i) = (uint32_t)((int32_t)EF_U32(0x001590c0u + i) >> 9);
		EF_U32(0x0015ffdeu + i) = (uint32_t)((int32_t)EF_U32(0x001590c4u + i) >> 9);
		EF_U32(0x0015ffe2u + i) = (uint32_t)((int32_t)EF_U32(0x001590c8u + i) >> 9);
	}
}

#define W_D7CB8(a) EF_U16(0x00420e00u + (a))

/* 0xd7cb8, 474 bytes. Run a fixed XOR-mixing pass over the word block at 0x420e80 (words at +2..+0x10), load the extra words at 0x420e92..0x420ea0 from 0x15baf2..0x15bb00, call 0xc4a50 on the block, mix the first eight words a second (different) way and finish with 0xd7bf0. All work is on 16-bit values. */
void mix_word_block_d7cb8(void)
{
	uint16_t ax, bx, cx, dx, si, di;

	ax = W_D7CB8(0x8e); dx = W_D7CB8(0x82); si = W_D7CB8(0x86);
	bx = ax; dx ^= ax; ax = W_D7CB8(0x8a); W_D7CB8(0x82) = dx;
	bx ^= dx; cx = dx; W_D7CB8(0x8e) = bx; di = ax; cx ^= bx; si ^= ax;
	ax = W_D7CB8(0x90); bx = W_D7CB8(0x84); W_D7CB8(0x82) = cx; W_D7CB8(0x86) = si;
	di ^= si; dx = si; W_D7CB8(0x8a) = di; cx = ax; dx ^= di; bx ^= ax;
	ax = W_D7CB8(0x8c); di = W_D7CB8(0x88); W_D7CB8(0x86) = dx; W_D7CB8(0x84) = bx;
	cx ^= bx; si = bx; W_D7CB8(0x90) = cx; dx = ax; di ^= ax;
	ax = EF_U16(0x0015bafeu); si ^= cx; W_D7CB8(0x92) = ax;
	ax = EF_U16(0x0015bb00u); W_D7CB8(0x84) = si; W_D7CB8(0x94) = ax;
	ax = EF_U16(0x0015bafau); W_D7CB8(0x88) = di; W_D7CB8(0x96) = ax;
	ax = EF_U16(0x0015bafcu); bx = di; W_D7CB8(0x98) = ax;
	ax = EF_U16(0x0015baf6u); dx ^= di; W_D7CB8(0x9a) = ax;
	ax = EF_U16(0x0015baf8u); W_D7CB8(0x8c) = dx; W_D7CB8(0x9c) = ax;
	ax = EF_U16(0x0015baf2u); bx ^= dx; W_D7CB8(0x9e) = ax;
	ax = EF_U16(0x0015baf4u); W_D7CB8(0x88) = bx; W_D7CB8(0xa0) = ax;
	guest_fn_c4a50(0x00420e80u);

	ax = W_D7CB8(0x8e); cx = W_D7CB8(0x82); dx = W_D7CB8(0x86);
	si = ax; cx ^= ax; ax = W_D7CB8(0x8a); W_D7CB8(0x82) = cx;
	si ^= cx; di = cx; W_D7CB8(0x8e) = si; bx = ax; di ^= si; dx ^= ax;
	ax = W_D7CB8(0x90); si = W_D7CB8(0x84); W_D7CB8(0x82) = di; W_D7CB8(0x86) = dx;
	bx ^= dx; cx = dx; W_D7CB8(0x8a) = bx; di = ax; cx ^= bx; si ^= ax;
	ax = W_D7CB8(0x8c); bx = W_D7CB8(0x88); W_D7CB8(0x86) = cx; W_D7CB8(0x84) = si;
	di ^= si; dx = si; W_D7CB8(0x90) = di; cx = ax; dx ^= di; bx ^= ax;
	W_D7CB8(0x84) = dx; si = bx; cx ^= bx; W_D7CB8(0x88) = bx;
	si ^= cx; W_D7CB8(0x8c) = cx; W_D7CB8(0x88) = si;
	guest_fn_d7bf0();
}
#undef W_D7CB8

/* 0xfcd20, 402 bytes. Variant of 0xe7a7c (angles in EBX, ESI, EDI) that stops after the nine matrix entries, written to 0x1590e4..0x159104, with the halved table offsets kept as words at 0x17bf80/82/84 and sin(A) at 0x17bf7c. */
void build_rotation_matrix_fcd20(uint32_t ebx, uint32_t esi, uint32_t edi)
{
	uint32_t x0 = (ebx >> 1) & 0x7ffcu, x1 = (esi >> 1) & 0x7ffcu, x2 = (edi >> 1) & 0x7ffcu;
	int32_t p0, s, d, t0, t1, c0, c1;

	EF_U16(0x0017bf80u) = (uint16_t)x0;
	EF_U16(0x0017bf82u) = (uint16_t)x1;
	EF_U16(0x0017bf84u) = (uint16_t)x2;
	p0 = sin32_e7a7c(x0);
	EF_U32(0x0017bf7cu) = (uint32_t)p0;
	s = sin32_e7a7c(x1 + x2);
	d = sin32_e7a7c(x1 - x2);
	EF_U32(0x001590f0u) = (uint32_t)((-mulshift24_e7a7c(s + d, p0) + s - d) >> 1);
	EF_U32(0x001590ecu) = (uint32_t)((mulshift24_e7a7c(s - d, p0) - s - d) >> 1);
	s = cos32_e7a7c(x1 + x2);
	d = cos32_e7a7c(x1 - x2);
	EF_U32(0x001590f8u) = (uint32_t)(-((mulshift24_e7a7c(s + d, p0) + d - s) >> 1));
	EF_U32(0x001590e4u) = (uint32_t)((mulshift24_e7a7c(d - s, p0) + s + d) >> 1);
	t0 = sin32_e7a7c(x0 - x2);
	c0 = cos32_e7a7c(x0 - x2);
	t1 = sin32_e7a7c(x0 + x2);
	c1 = cos32_e7a7c(x0 + x2);
	EF_U32(0x001590e8u) = (uint32_t)((-(t1 - t0)) >> 1);
	EF_U32(0x001590f4u) = (uint32_t)((c1 + c0) >> 1);
	t0 = sin32_e7a7c(x0 - x1);
	c0 = cos32_e7a7c(x0 - x1);
	t1 = sin32_e7a7c(x0 + x1);
	c1 = cos32_e7a7c(x0 + x1);
	EF_U32(0x001590fcu) = (uint32_t)((t1 - t0) >> 1);
	EF_U32(0x00159100u) = (uint32_t)p0;
	EF_U32(0x00159104u) = (uint32_t)((c0 + c1) >> 1);
}

/* 0xe9e7c, 264 bytes. Build a 3x3 double rotation matrix at OUT (EAX) from the three Euler angles X, Y, Z (stack doubles in this order, ret 0x18). Row-major entries at +0x00..+0x40 (nine doubles). */
void euler_to_matrix_e9e7c(ef_addr32 out, double x, double y, double z)
{
	double sz = sin_b45ac(z), cz = cos_b45a2(z);
	double cy = cos_b45a2(y), sy = sin_b45ac(y);
	double sx = sin_b45ac(x), cx = cos_b45a2(x);
	double a = sz * cy;
	double m[9];

	m[0] = cz * cx - a * sx;
	m[1] = -cz * sx - a * cx;
	m[2] = sz * sy;
	m[3] = cz * cy * sx + sz * cx;
	m[4] = cz * cy * cx - sz * sx;
	m[5] = -cz * sy;
	m[6] = sx * sy;
	m[7] = sy * cx;
	m[8] = cy;
	memcpy(ef_guest_memory_ptr(out), m, sizeof m);
}

/* 0x72f68, 283 bytes. Predict the position of unit UNIT (AX, sign extended) after time DT (stack double): when the turn rate +0x79 exceeds 0.01745 the point moves on an arc (X = q*sin(w*dt), Z = q*(cos(w*dt) - 1) with q = speed(+0x91)/rate), otherwise on a straight line X = speed*dt. The local offset (X, 0, Z) is rotated by the matrix at +0xa1.. and added to the position at +0x49; the three result doubles go to OUT (ret 0xc). */
void predict_unit_position_72f68(int32_t unit, double dt, ef_addr32 out)
{
	ef_addr32 u = guest_fn_7489c((int32_t)(int16_t)unit);
	double v79, v91, ang, q, x, y, z, r[3];

	if (u == 0u)
		return;
	memcpy(&v79, ef_guest_memory_ptr(u + 0x79u), 8);
	memcpy(&v91, ef_guest_memory_ptr(u + 0x91u), 8);
	ang = v79 * dt;
	if (abs_double_b427c(v79) > 0.01745329252) {
		q = v91 / v79;
		x = q * sin_b45ac(ang);
		y = 0.0;
		z = q * (cos_b45a2(ang) + -1.0);
	} else {
		x = v91 * dt;
		y = 0.0;
		z = 0.0;
	}
	{
		static const uint32_t col[3] = { 0xa1u, 0xb9u, 0xd1u };
		static const uint32_t pos[3] = { 0x49u, 0x51u, 0x59u };
		int i;
		for (i = 0; i < 3; i++) {
			double m0, m1, m2, p;
			memcpy(&m0, ef_guest_memory_ptr(u + col[i]), 8);
			memcpy(&m1, ef_guest_memory_ptr(u + col[i] + 8u), 8);
			memcpy(&m2, ef_guest_memory_ptr(u + col[i] + 16u), 8);
			memcpy(&p, ef_guest_memory_ptr(u + pos[i]), 8);
			r[i] = m0 * x + p + m1 * y + m2 * z;
		}
	}
	memcpy(ef_guest_memory_ptr(out), r, sizeof r);
}

/* 0x1db08, 280 bytes. Slew the two aim angles of object CTX (EAX), at +0x81 and +0x89, toward their targets. The first target is the offset from 0x1d844; the step is limited to rate(+0x69)*dt (dt is the double at 0x19af70) and applied only when larger than 5e-05. The second target is -atan(sign*step*(+0x71)/dt*0.10194) with a step limit of rate*dt*10. */
void slew_aim_angles_1db08(ef_addr32 ctx)
{
	double dt, rate, a81, a89, d1, a1, lim, d2, a2, v71;

	d1 = report_waypoint_offset_1d844(ctx);
	memcpy(&a81, ef_guest_memory_ptr(ctx + 0x81u), 8);
	memcpy(&a89, ef_guest_memory_ptr(ctx + 0x89u), 8);
	memcpy(&rate, ef_guest_memory_ptr(ctx + 0x69u), 8);
	memcpy(&v71, ef_guest_memory_ptr(ctx + 0x71u), 8);
	memcpy(&dt, ef_guest_memory_ptr(0x0019af70u), 8);
	d1 = d1 - a81;
	a1 = abs_double_b427c(d1);
	if (a1 > 5e-05) {
		lim = rate * dt;
		if (a1 > lim)
			a1 = lim;
		a81 = (0.0 >= d1) ? a81 - a1 : a81 + a1;
		memcpy(ef_guest_memory_ptr(ctx + 0x81u), &a81, 8);
	}
	d2 = -atan_dc1d4(sign_double_b42b0(d1) * a1 * v71 / dt * 0.1019367991845056) - a89;
	a2 = abs_double_b427c(d2);
	if (a2 > 5e-05) {
		lim = rate * dt * 10.0;
		if (a2 > lim)
			a2 = lim;
		a89 = (0.0 >= d2) ? a89 - a2 : a89 + a2;
		memcpy(ef_guest_memory_ptr(ctx + 0x89u), &a89, 8);
	}
}

/* 0xa9724, 277 bytes. Handle the command line TEXT (EAX). When 0x1371ff is set only a leading '%' matters (it sets 0x137200). Otherwise clear 0x1371ee/0x1371fb, build "<0x18d780><text><0x18d784>" in a local buffer, split it on white space (ctype table 0x195af0, bit 2) into an argv array whose slot 0 is the program name at 0x137209, run the option parser 0x3ea0c (argc in EAX, argv in EDX), reset the display if it set 0x41955d, and finally call 0x28744 when 0x1371fb was set by the parser. */
void parse_command_line_a9724(ef_addr32 text)
{
	uint8_t buf[0x64];
	uint32_t argv[0x10];
	uint32_t argc;
	uint8_t *p;

	if (EF_U8(0x001371ffu) != 0u) {
		if (EF_U8(text) == 0x25u)
			EF_U8(0x00137200u) = 1u;
		return;
	}
	EF_U8(0x001371eeu) = 0u;
	EF_U8(0x001371fbu) = 0u;
	memset(buf, 0, sizeof buf);
	memcpy(buf, ef_guest_memory_ptr(0x0018d780u), 4);
	guest_fn_9ca57((ef_addr32)(uintptr_t)buf, text);
	guest_fn_9ca57((ef_addr32)(uintptr_t)buf, 0x0018d784u);
	argv[0] = 0x00137209u;
	argc = 1u;
	p = buf;
	if (buf[0] != 0u) {
		do {
			while ((EF_U8(0x00195af0u + (uint8_t)(*p + 1u)) & 2u) != 0u)
				p++;
			if (*p == 0u)
				break;
			argv[argc++] = (uint32_t)(uintptr_t)p;
			for (;;) {
				uint8_t sp = (uint8_t)(EF_U8(0x00195af0u + (uint8_t)(*p + 1u)) & 2u);
				uint8_t *next = p + 1;
				if (sp != 0u) {
					*p = 0u;
					p = next;
					break;
				}
				p = next;
			}
		} while (*p != 0u);
	}
	EF_U8(0x0041955du) = 0u;
	guest_fn_3ea0c_x(argc, (ef_addr32)(uintptr_t)argv);
	if (EF_U8(0x0041955du) != 0u) {
		set_black_palette_d65e8();
		guest_fn_dc250();
		clear_buffer_dc2c0();
		guest_fn_dc250();
	}
	if (EF_U8(0x001371fbu) != 0u) {
		open_named_file_28744();
		EF_U8(0x001371fbu) = 0u;
	}
}

/* 0x8cf10, 287 bytes. Remove unit UNIT (AX, sign extended) from play unless its flag +0x24 bit 2 is set. When it is the local player's unit (word 0x1f71ce) the working state is reset and, in a network game (0x127da8), the owner's loss counter (word array 0x1a6378) is bumped, its score (0x1a6398) lowered by 4 (8 in game mode 3) and the owner summary and a packet are sent. Then the unit's +0x12/+0x91/+0x95 are cleared, bit 3 of +0x26 dropped, 0x7ef14 run, the position packed and a kind-0x64 message sent; a linked panel strip (+0x20b) is cleared. */
void destroy_unit_8cf10(int32_t unit)
{
	ef_addr32 u = guest_fn_7489c((int32_t)(int16_t)unit);
	int32_t player;

	if (u == 0u)
		return;
	if ((EF_U8(u + 0x24u) & 4u) != 0u)
		return;
	if ((uint16_t)unit == EF_U16(0x001f71ceu)) {
		EF_U8(0x001a363au) = 1u;
		reset_working_state_e76c8();
		EF_U8(0x001a3638u) = 0u;
		player = (int32_t)EF_U8(0x001a666bu);
		EF_U8(0x001a363fu) = 0u;
		if (player < 8 && EF_U8(0x00127da8u) != 0u) {
			EF_U16(0x001a6378u + (uint32_t)player * 2u) = (uint16_t)(EF_U16(0x001a6378u + (uint32_t)player * 2u) + 1u);
			if (EF_U8(0x0012584bu) == 3u)
				EF_U16(0x001a6398u + (uint32_t)player * 2u) = (uint16_t)(EF_U16(0x001a6398u + (uint32_t)player * 2u) - 8u);
			else
				EF_U16(0x001a6398u + (uint32_t)player * 2u) = (uint16_t)(EF_U16(0x001a6398u + (uint32_t)player * 2u) - 4u);
			send_owner_summary_56a4c();
			send_packet_2c(0u, EF_U8(0x001a666bu), 1u, 0u, 0u);
		}
	}
	EF_U32(u + 0x12u) = 0u;
	EF_U32(u + 0x91u) = 0u;
	EF_U32(u + 0x95u) = 0u;
	EF_U8(u + 0x26u) = (uint8_t)(EF_U8(u + 0x26u) & 0xf7u);
	guest_fn_7ef14_x();
	pack_runtime_position(-1, (int32_t)EF_U32(u) >> 8, (int32_t)EF_U32(u + 8u) >> 8, 0u);
	send_or_defer_748f4((int16_t)unit, 0u, 0u, 0x64u);
	if (EF_U8(u + 0x22fu) != 0u)
		clear_panel_strip((int16_t)((int32_t)EF_U32(u + 0x20bu) >> 16));
}

/* 0x8935c, 283 bytes. Choose how to treat scene object OBJ (EAX) from its distance to the viewer. An object with +0x5a set is force-selected. One whose range word (+0x3e high) exceeds 0x5b4f is activated. Otherwise the object and its parent are converted into scene vectors (0x1fcf58, 0x1fcf88), their difference goes to 0x1fcfa0, and the flat distance and height (double at 0x1fcf68) pick the mode stored in 0x12d7b2: below 9000 units 3 (force-select) when the height is under -100 else 4 (activate); at 9000 or more 1 (force-select) when under -2700 else 2 (activate). */
void classify_scene_object_8935c(ef_addr32 obj)
{
	double dx, dy, dist, z;

	if (EF_U8(obj + 0x5au) != 0u) {
		force_select_898d4(obj);
		return;
	}
	if (((int32_t)EF_U32(obj + 0x3eu) >> 16) > 0x5b4f) {
		activate_89990(obj);
		return;
	}
	convert_scene_vector(obj, 0x001fcf58u);
	transform_child_coordinates(obj, 0x001fcf88u);
	subtract_scene_vectors(0x001fcfa0u, 0x001fcf58u, 0x001fcf88u);
	memcpy(&dx, ef_guest_memory_ptr(0x001fcfa0u), 8);
	memcpy(&dy, ef_guest_memory_ptr(0x001fcfa8u), 8);
	dist = guest_fn_a4470(square_double_b3fd0(dy) + square_double_b3fd0(dx));
	memcpy(&z, ef_guest_memory_ptr(0x001fcf68u), 8);
	if (dist >= 9000.0) {
		if (z < -2700.0) {
			EF_U8(0x0012d7b2u) = 1u;
			force_select_898d4(obj);
		} else {
			EF_U8(0x0012d7b2u) = 2u;
			activate_89990(obj);
		}
	} else if (z < -100.0) {
		EF_U8(0x0012d7b2u) = 3u;
		force_select_898d4(obj);
	} else {
		EF_U8(0x0012d7b2u) = 4u;
		activate_89990(obj);
	}
}

/* 0x882b0, 282 bytes. Fire the placement effects of object OBJ (EAX) with parameter ARG (DX, sign extended). The object position >> 8 is copied to 0x20f16c..0x20f174. For kind 1 (+0x18) the result of 0x39548 (-1 = none) is dispatched through 0x1d5cc. For other kinds the map entry at the object's x/z is looked up (0x82b8) and, if found, its parameters are stored (0x10710) and 0x110ec run; afterwards kinds whose table byte (0x12bc94 + 145*kind) is 1, 2 or 3 dispatch the 0x39548 result for effect 0xf. */
void fire_object_effects_882b0(ef_addr32 obj, int32_t arg)
{
	uint32_t kind = EF_U8(obj + 0x18u);
	ef_addr32 e;
	int32_t r;
	uint32_t k;

	EF_U32(0x0020f16cu) = (uint32_t)((int32_t)EF_U32(obj) >> 8);
	EF_U32(0x0020f170u) = (uint32_t)((int32_t)EF_U32(obj + 4u) >> 8);
	EF_U32(0x0020f174u) = (uint32_t)((int32_t)EF_U32(obj + 8u) >> 8);
	if (kind == 1u) {
		r = (int32_t)(int16_t)guest_fn_39548_x(obj, kind, (int32_t)(int16_t)arg);
		if (r != -1)
			guest_fn_1d5cc(0x0020f16cu, r);
		return;
	}
	e = mark_entry_at_point_82b8((int32_t)EF_U32(obj), (int32_t)EF_U32(obj + 8u));
	if (e != 0u) {
		uint32_t b = guest_fn_8ac64_x((int32_t)kind) & 0xffu;
		store_params_10710((uint16_t)((int32_t)EF_U32(EF_U32(obj + 0x4bu) + 0x14u) >> 16),
		                   (uint16_t)(int16_t)EF_U16(e),
		                   (uint16_t)((int32_t)EF_U32(e + 2u) >> 16),
		                   (uint16_t)((int32_t)EF_U32(e + 6u) >> 16),
		                   (uint16_t)((int32_t)EF_U32(e + 0xau) >> 16), (uint8_t)b);
		guest_fn_110ec_x(1u, 0x64u);
	}
	k = EF_U8(0x0012bc94u + kind * 145u);
	if (k == 1u || k == 2u || k == 3u) {
		r = (int32_t)(int16_t)guest_fn_39548_x(obj, 0xfu, (int32_t)(int16_t)arg);
		if (r != -1)
			guest_fn_1d5cc(0x0020f16cu, r);
	}
}

/* 0x9fea8, 288 bytes. Claim the lowest rated 78-byte slot of the table at 0x416e40 (0x9fdd4) for a new record: word +2 = A (EAX), +0 = word 0x41722a, +4 = previous word 0x132b26 (which then becomes the slot index), +6 = 10, +0x3c..+0x44 = dwords 0x132b28/2c/30. The vector at +0x18 is loaded from the three words of the entry B (EDX) of the table at pointer 0x41717c (entries of 6 bytes starting at +0xa) and scaled by 100.0f; the vector at +0x30 comes from entry C (BX, stack word) at offset +0xa/+0xc/+0xe and is scaled by 1000.0f; the vectors at +0x0c and +0x24 are cleared. */
void init_record_slot_9fea8(uint32_t a, int32_t b, uint32_t c)
{
	int32_t idx = (int32_t)(int16_t)select_lowest_rated_entry_9fdd4();
	ef_addr32 r = 0x00416e40u + (uint32_t)(idx * 78);
	ef_addr32 ent;
	uint16_t old;

	EF_U16(r + 2u) = (uint16_t)a;
	EF_U16(r) = EF_U16(0x0041722au);
	old = EF_U16(0x00132b26u);
	EF_U16(0x00132b26u) = (uint16_t)idx;
	EF_U16(r + 4u) = old;
	EF_U16(r + 6u) = 10u;
	EF_U32(r + 0x3cu) = EF_U32(0x00132b28u);
	EF_U32(r + 0x40u) = EF_U32(0x00132b2cu);
	EF_U32(r + 0x44u) = EF_U32(0x00132b30u);
	ent = EF_U32(0x0041717cu) + 0xau + (uint32_t)((int32_t)(int16_t)b * 6);
	store_triple_ea864(r + 0x18u, (uint32_t)(int32_t)(int16_t)EF_U16(ent),
	                   (uint32_t)(int32_t)(int16_t)EF_U16(ent + 2u),
	                   (uint32_t)(int32_t)(int16_t)EF_U16(ent + 4u));
	scale_vector3_ea8cc(r + 0x18u, (uint32_t)(int32_t)(int16_t)c, 0u, 0u, 0x42c80000u);
	ent = EF_U32(0x0041717cu) + (uint32_t)((int32_t)(int16_t)c * 6);
	store_triple_ea864(r + 0x30u, (uint32_t)(int32_t)(int16_t)EF_U16(ent + 0xau),
	                   (uint32_t)(int32_t)(int16_t)EF_U16(ent + 0xcu),
	                   (uint32_t)(int32_t)(int16_t)EF_U16(ent + 0xeu));
	scale_vector3_ea8cc(r + 0x30u, 0u, 0u, 0u, 0x447a0000u);
	clear_vector3(r + 0xcu);
	clear_vector3(r + 0x24u);
}

/* 0x39edc, 289 bytes. Let class row A (EAX, rows of 0xca bytes at dword 0x19bb58) with object entry B (EDX, 69-byte entries at dword 0x1a14f8) look for the nearest object of side 6 and type 1: the entry position (dwords +7/+0xb/+0xf scaled by 2.4384) is passed to 0x2c454 with the row's id (+0x13 high word). When one is found (not 0xff) the countdown dword 0x126354 is reduced by word 0x127d92 and, once it goes negative, reset to 0x226 and a message with the bearing (0x37c0c -> 0x45fc4) and the scaled distance (0x37bc8, factor 0.00053937) between the two rows is sent through 0x46254. */
void report_nearest_object_39edc(uint32_t a, uint32_t b)
{
	ef_addr32 rows, entry, row_a, row_b;
	double pos[3], k = 2.4384, dist;
	uint32_t found, angle;
	int32_t d;
	int32_t ia = (int32_t)(int16_t)a, ib = (int32_t)(int16_t)b;

	entry = EF_U32(0x001a14f8u) + (uint32_t)(ib * 69);
	pos[0] = (double)(int32_t)EF_U32(entry + 0xfu) * k;
	pos[1] = (double)(int32_t)EF_U32(entry + 7u) * k;
	pos[2] = (double)(int32_t)EF_U32(entry + 0xbu) * k;
	rows = EF_U32(0x0019bb58u);
	row_a = rows + (uint32_t)(ia * 0xca);
	found = (uint32_t)(int32_t)(int16_t)find_nearest_object_2c454((uint32_t)(uintptr_t)pos, (uint32_t)((int32_t)EF_U32(row_a + 0x13u) >> 16), 6u, 1u);
	if (found == 0xffu)
		return;
	EF_U32(0x00126354u) = EF_U32(0x00126354u) - (uint32_t)EF_U16(0x00127d92u);
	if ((int32_t)EF_U32(0x00126354u) >= 0)
		return;
	row_b = EF_U32(0x0019bb58u) + found * 0xcau;
	EF_U32(0x00126354u) = 0x226u;
	angle = (uint32_t)scale_to_angle_45fc4(report_point_offset_37c0c(row_b + 0x61u, (int16_t)ib));
	dist = scaled_pair_distance_37bc8(EF_U32(0x0019bb58u) + found * 0xcau + 0x61u, (int16_t)ib);
	d = (int32_t)truncate_9d72c(dist * 0.0005393743257820927);
	rows = EF_U32(0x0019bb58u);
	guest_fn_46254((uint32_t)((int32_t)EF_U32(row_a + 0x4eu) >> 16),
	               (int32_t)((int32_t)EF_U32(rows + found * 0xcau + 0x4eu) >> 16), 0x64u, (uint32_t)ib,
	               (uint32_t)(int32_t)(int16_t)angle, (uint32_t)(int32_t)(int16_t)d);
}

/* 0x85420, 292 bytes. Return 1 when unit row UNIT (AX; rows of 0xca bytes at dword 0x19bb58) is within reach of its target: the flat distance between the row position (+0x61) and the map point taken from the two scaled words of its type (+0x5f indexes 34-byte entries at dword 0x1972e0, words +6/+0xa) divided by the class (+0x19, 0xe5-byte records at 0x128591) value is below half of (+0x99 / the 0x2ccf0 estimate). Rows whose +0x20 is 1 first get their grid limit recomputed (0x2e08c). */
uint8_t row_within_reach_85420(int32_t unit)
{
	int32_t u = (int32_t)(int16_t)unit;
	ef_addr32 rows = EF_U32(0x0019bb58u);
	ef_addr32 row = rows + (uint32_t)(u * 0xca);
	uint32_t t = EF_U8(row + 0x5fu);
	double vec[3], dist, x, a, b, c99, cls;
	uint32_t cl;

	vec[0] = word_to_scaled_b4230((int16_t)((int32_t)EF_U32(EF_U32(0x001972e0u) + t * 34u + 0xau) >> 16));
	vec[1] = word_to_scaled_b4230((int16_t)((int32_t)EF_U32(EF_U32(0x001972e0u) + t * 34u + 6u) >> 16));
	vec[2] = 0.0;
	dist = distance_2d_b415c(row + 0x61u, (ef_addr32)(uintptr_t)vec);
	if (EF_U8(row + 0x20u) == 1u)
		recompute_grid_limit(u);
	rows = EF_U32(0x0019bb58u);
	row = rows + (uint32_t)(u * 0xca);
	cl = EF_U8(row + 0x19u);
	memcpy(&a, ef_guest_memory_ptr(row + 0x71u), 8);
	memcpy(&b, ef_guest_memory_ptr(0x00128591u + cl * 0xe5u), 8);
	memcpy(&c99, ef_guest_memory_ptr(row + 0x99u), 8);
	x = c99 / guest_fn_2ccf0_x(u, a, b);
	cls = b;
	return (x * 0.5 > dist / cls) ? 1u : 0u;
}

/* 0x81680, 273 bytes. Periodic pseudo-random effect update: steps a small word generator and spawns a record at a rotated offset. */
void update_random_effect_81680(void)
{
	int16_t out[2];
	uint32_t old;
	int32_t s;

	if (EF_U8(0x00137a00u) == 0u)
		return;
	EF_U16(0x0012b254u) = (uint16_t)(EF_U16(0x0012b254u) - 1u);
	if (EF_U16(0x0012b254u) != 0u)
		return;
	old = EF_U16(0x0012b250u);
	EF_U16(0x0012b254u) = (uint16_t)(old & 0xfu);
	s = (int32_t)(int16_t)old;
	EF_U16(0x0012b250u) = (uint16_t)(old + EF_U16(0x0012b252u));
	EF_U16(0x0012b252u) = (uint16_t)((s >> 1) + (int32_t)((uint32_t)s << 15) + ((int32_t)EF_U32(0x0012b250u) >> 16));
	load_sincos_dc7bc(old);
	rotate_point_dc7e4((ef_addr32)(uintptr_t)out, (int16_t)((int32_t)EF_U32(0x0012b250u) >> 16), 0);
	EF_U32(0x001fcae0u) = EF_U32(0x001a5becu) + (uint32_t)((int32_t)out[0] * 4);
	EF_U32(0x001fcae4u) = EF_U32(0x001a5be8u) + (uint32_t)((int32_t)out[1] * 4);
	EF_U32(0x0015be0cu) = 0xfa0u;
	EF_U32(0x0015bddcu) = 0u;
	EF_U32(0x0015bde0u) = (uint32_t)((int32_t)EF_U32(0x001fcaeau) >> 16);
	EF_U32(0x0015bde4u) = 0u;
	EF_U32(0x0015bde8u) = EF_U32(0x001fcae0u);
	EF_U32(0x0015bdecu) = 0u;
	EF_U32(0x0015bdf0u) = EF_U32(0x001fcae4u);
	guest_fn_15be10_x();
	EF_U8(0x00128549u) = 2u;
}

/* 0x7bb00, 251 bytes. Look up the terrain record under a world position and publish its slope and heading in a global triple. */
ef_addr32 terrain_slope_at_7bb00(ef_addr32 pos)
{
	int32_t x = EF_S32(pos);
	int32_t z = EF_S32(pos + 8u);
	int32_t row = 0x18f - (z >> 11);
	ef_addr32 rec = 0u;
	uint32_t m;

	select_plane_source(0);
	m = map_value_7ae0c(x, z);
	if ((uint16_t)m != 0xffffu) {
		rec = guest_fn_cfc68_x((int32_t)(int16_t)m, (x >> 11) * 2 + 1 << 10, (0x18f - row) * 2 + 1 << 10, x, z);
	}
	if (rec != 0u) {
		double v = (double)(int16_t)EF_U16(rec + 2u) * 3.0517578125e-05;
		double ang;
		int32_t a;
		if (!(1.0 >= v))
			v = 1.0;
		if (!(v >= -1.0))
			v = -1.0;
		ang = guest_fn_deb1a_x(v) * 32768.0 * 0.3183098861827565 + -16384.0;
		a = (int32_t)truncate_9d72c(ang);
		EF_U16(0x001f8ebcu) = (uint16_t)a;
		EF_U16(0x001f8ebeu) = (uint16_t)angle_of_b3b1c((int16_t)EF_U16(rec), (int16_t)EF_U16(rec + 4u));
		EF_U16(0x001f8ec0u) = 0u;
		rec = 0x001f8ebcu;
	}
	return rec;
}

/* 0xb3c04, 252 bytes. Load a named file into a buffer, unpacking RJ/AR compressed payloads. */
uint8_t load_file_unpacked_b3c04(ef_addr32 name, ef_addr32 dest)
{
	uint32_t size = read_file_payload_size_b3b90(name);
	uint32_t h, tag, n, buf;

	if (size == 0u)
		return 0u;
	h = guest_fn_a7900(name);
	if (h == 0xffffffffu)
		return 0u;
	EF_U16(0x0041f360u) = 0u;
	guest_fn_a79d4(h, 8u, 0x0041f360u);
	seek_start_a7ab4(0u, 0u, 0u, 0u, h, 0u);
	tag = EF_U16(0x0041f360u);
	if (tag == 0x524au) {
		n = guest_fn_a7ae0(h, 0u) + 0x10u;
		buf = guest_fn_c3f70(n);
		if (buf == 0u) {
			buf = size + dest - n;
			guest_fn_a79d4(h, n, buf);
			guest_fn_f9ee0_x(dest, buf);
		} else {
			guest_fn_a79d4(h, n, buf);
			guest_fn_f9ee0_x(dest, buf);
			guest_fn_c3f80(buf);
		}
		guest_fn_a7990(h);
		return 1u;
	}
	if (tag == 0x4152u) {
		guest_fn_fa02c_x(h, dest);
		guest_fn_a7990(h);
		return 1u;
	}
	guest_fn_a79d4(h, size, dest);
	guest_fn_a7990(h);
	return 1u;
}

/* 0xd9cb4, 256 bytes. Poll the attached device and convert its latest position sample to three scaled integers. */
uint32_t read_device_angles_d9cb4(ef_addr32 out1, ef_addr32 out2, ef_addr32 out3)
{
	const double f = 6.103515625e-05;
	double a, b, c;
	int32_t base1, base2, base3;

	if (EF_S32(0x0015bb4cu) == 0x7fffffff)
		read_stream_header_d9dc0();
	EF_U8(0x00420f54u) = guest_fn_107070_w(0x00420f00u, (uint32_t)times_100(1));
	if (EF_U8(0x00420f54u) != 0u) {
		EF_U8(0x00420f54u) = guest_fn_106d34_w(0x00420f00u, (uint32_t)times_100_div_10(1));
		if (EF_U8(0x00420f54u) != 0u)
			return 0u;
	}
	send_command_107060(0x00420f00u);
	base1 = EF_S32(0x0015bb50u);
	base2 = EF_S32(0x0015bb4cu);
	base3 = EF_S32(0x0015bb54u);
	a = (double)(EF_S32(0x00420f1cu) - base1) * f;
	EF_S32(out1) = (int32_t)truncate_9d72c((360.0 - a) * 182.0);
	b = (double)(EF_S32(0x00420f18u) - base2) * f;
	EF_S32(out2) = (int32_t)truncate_9d72c((360.0 - b) * 192.0);
	c = (double)(EF_S32(0x00420f20u) - base3);
	EF_S32(out3) = (int32_t)truncate_9d72c(192.0 * (f * c));
	return 1u;
}

/* 0x4e14, 260 bytes. Enter the high-score screen: load the table, pick the arcade text file for the difficulty and enable the summary widgets. */
void enter_hiscore_screen_4e14(void)
{
	ef_addr32 root, widget, src, node;
	uint32_t i;

	EF_U8(0x00137a00u) = 1u;
	guest_fn_41fc_x();
	guest_fn_42a4_x();
	EF_U8(0x00125847u) = 1u;
	guest_fn_e710();
	EF_U32(0x001970a0u) = 0u;
	EF_U16(0x00121e46u) = 1u;
	guest_fn_a4d90(0x0017f648u);
	switch (EF_S32(0x00121e42u) >> 16) {
	case 1:
		root = find_named_or_abort_a46cc(0x0017f6acu);
		widget = find_widget_checked_a78bc(root, 0x0017f6b8u);
		src = 0x0017f7b8u;
		break;
	case 2:
		root = find_named_or_abort_a46cc(0x0017f6acu);
		widget = find_widget_checked_a78bc(root, 0x0017f6c0u);
		src = 0x0017f7c4u;
		break;
	default:
		root = find_named_or_abort_a46cc(0x0017f6acu);
		widget = find_widget_checked_a78bc(root, 0x0017f6c4u);
		src = 0x0017f7d0u;
		break;
	}
	for (i = 0; i < 11u; i++)
		EF_U8(0x001f8fc8u + i) = EF_U8(src + i);
	enable_widget_a72d8(widget);
	root = find_named_or_abort_a46cc(0x0017f7e8u);
	widget = find_widget_checked_a78bc(root, 0x0017f7f4u);
	enable_widget_a72d8(widget);
	guest_fn_7bee8_x(0x00121e40u);
	node = find_named_or_abort_a46cc(0x0017f7a4u);
	EF_U16(node + 0xd6u) = 0u;
	set_session_text_7c4fc(EF_U32(0x00121e40u) + 0x28u, node);
	guest_fn_a5ef8(EF_U8(node + 0x10u));
	guest_fn_40dc_x(0u);
}

/* 0xf34cc, 266 bytes. Present one frame of the status screen: redraw the overlay when the frame is live, otherwise rebuild the caption, call the save routine and finish the palette fade. */
void present_status_frame_f34cc(void)
{
	uint32_t i;

	if (EF_U8(0x004859dcu) == 0u)
		guest_fn_d65e8();
	if (EF_U8(0x004859deu) == 0u) {
		guest_fn_dc250();
		if (EF_U8(0x004859ddu) != 0u)
			draw_overlay_polys_f3378();
		guest_fn_c386c();
		guest_fn_dc250();
		if (EF_U8(0x004859dcu) == 0u)
			guest_fn_de580(0x00425c8cu);
		else
			guest_fn_d663c(0x00425c8cu);
		guest_fn_c3990();
		guest_fn_dd8dc();
		guest_fn_dd8c0_x();
		return;
	}
	guest_fn_c3974();
	guest_fn_9d749(0x00485908u, EF_U32(0x00417254u));
	guest_sprintf_a4342(EF_U32(0x00417258u), 0x00191990u, (uint32_t)EF_U8(0x004859dfu));
	for (i = 0; i < 10u; i++)
		EF_U8(EF_U32(0x00417260u) + i) = EF_U8(0x00191994u + i);
	if ((uint16_t)guest_fn_eac48_x(7u, 0x00417250u) != 0u) {
		guest_fn_c3f80(EF_U32(0x00485928u));
		guest_callptr2_x(EF_U32(0x00132a74u), 0x001919a0u, 0x00485908u);
	}
	refresh_f3480();
	if (EF_U8(0x004859dcu) == 0u)
		guest_fn_de580(0x00425c8cu);
	else
		guest_fn_d663c(0x00425c8cu);
}

/* 0xf8c98, 270 bytes. (ret 4) Walk the node list, claim each unclaimed matching node for the next index of the request and record the owner in its first free slot. */
void claim_list_nodes_f8c98(uint32_t key, uint32_t owner, uint32_t slot_value, ef_addr32 request, ef_addr32 list)
{
	ef_addr32 node = EF_U32(list);
	uint16_t idx = 0u;

	while (node != 0u && (int16_t)EF_U8(request + 3u) > (int16_t)idx) {
		if (EF_U16(node + 0x31u) == (uint16_t)key
		    && EF_U8(node + 0x0bu) == 1u
		    && EF_U8(node + 0x0cu) == EF_U8(request + 8u)
		    && EF_U16(node + 0x09u) == 0u) {
			uint16_t pending = 1u;
			int16_t i;

			EF_U16(node + 0x09u) = 1u;
			for (i = 0; i < 4 && pending != 0u; i++) {
				ef_addr32 e = node + (uint32_t)(i * 2);

				if ((EF_S32(e + 0x27u) >> 16) == -1
				    && (EF_S32(e + 0x17u) >> 16) == -1
				    && (EF_S32(e + 0x1fu) >> 16) == -1) {
					EF_U16(e + 0x21u) = (uint16_t)slot_value;
					EF_U16(e + 0x29u) = idx;
					EF_U16(e + 0x19u) = (uint16_t)owner;
					pending = 0u;
				}
			}
			EF_U8(EF_U32(node + 0x4du) + 4u) = (uint8_t)idx;
			idx++;
			EF_U16(node + 0x51u) = 0u;
			EF_U16(node + 0x53u) = 0u;
		}
		node = EF_U32(node + 4u);
	}
}

/* 0xce830, 275 bytes. Clip the polygon at obj+0x4c against the test callback at ctx+0x14: build the clipped vertex list in the scratch pool at 0x4208b0 and clear bit 0 of obj+0x2c when any edge was cut. */
void clip_polygon_ce830(ef_addr32 obj, ef_addr32 ctx)
{
	ef_addr32 out = EF_U32(0x004208b0u);
	ef_addr32 in, prev, cur, slot;
	int32_t n, i;
	uint32_t state, newstate, changed = 0u;

	EF_U32(out) = 0u;
	EF_U32(0x004208b0u) = out + 0x84u;
	in = EF_U32(obj + 0x4cu);
	n = EF_S32(in);
	prev = EF_U32(in + 4u);
	EF_U32(in + (uint32_t)n * 4u + 4u) = prev;
	state = guest_callptr4_ret_x(EF_U32(ctx + 0x14u), ctx, EF_U32(prev), EF_U32(prev + 4u), EF_U32(prev + 8u)) & 0xffu;
	slot = in;
	for (i = 0; i < n; i++) {
		cur = EF_U32(slot + 8u);
		newstate = guest_callptr4_ret_x(EF_U32(ctx + 0x14u), ctx, EF_U32(cur), EF_U32(cur + 4u), EF_U32(cur + 8u)) & 0xffu;
		if ((state ^ newstate) != 0u) {
			if (newstate != 0u)
				counted_array_append(out, prev);
			counted_array_append(out, guest_fn_ce5f8_x(ctx, prev, cur));
			changed = 1u;
		} else if (state == 0u) {
			counted_array_append(out, prev);
		}
		state = newstate;
		prev = cur;
		slot += 4u;
	}
	EF_U32(obj + 0x4cu) = out;
	if (changed != 0u)
		EF_U8(obj + 0x2cu) = (uint8_t)(EF_U8(obj + 0x2cu) & 0xfeu);
}

/* 0x5eaa4, 276 bytes. Advance the nearest-record scan: for the current index compute the squared distance to the view position and, when it beats the best so far, make it current and run the 0x5ebb8 update. */
void scan_nearest_record_5eaa4(void)
{
	int16_t cx = (int16_t)EF_U16(0x001a6cccu);
	int16_t di = (int16_t)EF_U16(0x001a6cceu);
	ef_addr32 base, rec;
	int32_t dx, dy, dist;

	if (cx >= (int16_t)EF_U16(0x001972e6u))
		cx = 0;
	base = EF_U32(0x001972e0u);
	if (base != 0u) {
		if (di >= 0) {
			rec = base + (uint32_t)((int32_t)di * 34);
			dx = EF_S32(rec + 8u) - (EF_S32(0x001a5becu) >> 11);
			dy = EF_S32(rec + 0xcu) - (EF_S32(0x001a5be8u) >> 11);
			EF_U32(0x001a6b00u) = (uint32_t)(dx * dx + dy * dy);
		}
		rec = base + (uint32_t)((int32_t)cx * 34);
		dx = EF_S32(rec + 8u) - (EF_S32(0x001a5becu) >> 11);
		dy = EF_S32(rec + 0xcu) - (EF_S32(0x001a5be8u) >> 11);
		dist = dx * dx + dy * dy;
		if (dist < EF_S32(0x001a6b00u) && cx != di) {
			EF_U32(0x001a6b00u) = (uint32_t)dist;
			EF_U16(0x001a6cfeu) = (uint16_t)cx;
			EF_U16(0x001a6cccu) = (uint16_t)cx;
			EF_U16(0x001a6cceu) = (uint16_t)cx;
			EF_U32(0x001a6b08u) = rec;
			EF_U32(0x001a6b04u) = EF_U32(rec + 0x16u);
			guest_fn_5ebb8_x();
			di = (int16_t)EF_U16(0x001a6cceu);
			cx = (int16_t)EF_U16(0x001a6cccu);
		}
		cx++;
	}
	EF_U16(0x001a6cceu) = (uint16_t)di;
	EF_U16(0x001a6cccu) = (uint16_t)cx;
}

/* 0xd35b0, 276 bytes. Queue effect object P: reset the build state at 0x420974/0x42099c/0x4209bc/0x4209d0/0x4209d4, clear flag bits 1-2 of P+0x20, then (while fewer than 200 slots are used, counter at 0x17d2c0) fill a 0x6ec-byte slot at 0x486db0: 20 dwords from 0x420970, 12 dwords from P, a double scaled from the int at slot+0x8c, and two value pairs from the node chain at 0x4209bc. Over the limit it reports through the function pointer at 0x132a74. Returns P (ret 4). */
ef_addr32 queue_effect_slot_d35b0(ef_addr32 p)
{
	uint32_t n, i;
	ef_addr32 slot, node;
	uint32_t v;
	int32_t iv;
	double d, scale;

	EF_U32(0x0042099cu) = 1u;
	EF_U32(0x00420974u) = 0u;
	EF_U32(0x004209bcu) = 0x004209d0u;
	EF_U8(p + 0x20u) = (uint8_t)(EF_U8(p + 0x20u) & 0xf9u);
	EF_U32(0x004209d0u) = 1u;
	EF_U32(0x004209d4u) = p;
	n = EF_U8(0x0017d2c0u);
	if (n >= 0xc8u) {
		guest_callptr3_x(EF_U32(0x00132a74u), 0x0018f7e0u, n, 0xc8u);
		return p;
	}
	slot = 0x00486db0u + n * 0x6ecu;
	EF_U8(0x0017d2c0u) = (uint8_t)(n + 1u);
	for (i = 0; i < 20u; i++)
		EF_U32(slot + 0x684u + i * 4u) = EF_U32(0x00420970u + i * 4u);
	EF_U32(slot) = 1u;
	EF_U32(slot + 0x6d4u) = 0x000d37f4u;
	for (i = 0; i < 12u; i++)
		EF_U32(slot + 0x84u + i * 4u) = EF_U32(p + i * 4u);
	EF_U32(slot + 0x6d0u) = slot;
	iv = EF_S32(slot + 0x8cu);
	{
		uint32_t lo = EF_U32(0x0018f80cu), hi = EF_U32(0x0018f810u);
		uint64_t bits = ((uint64_t)hi << 32) | lo, out;
		memcpy(&scale, &bits, 8);
		d = (double)iv * scale;
		memcpy(&out, &d, 8);
		EF_U32(slot + 0x6d8u) = (uint32_t)out;
		EF_U32(slot + 0x6dcu) = (uint32_t)(out >> 32);
	}
	node = EF_U32(EF_U32(0x004209bcu) + 4u);
	v = EF_U32(node + 0x18u);
	EF_U32(slot + 0x694u) = v;
	EF_U32(slot + 0x698u) = v;
	node = EF_U32(EF_U32(0x004209bcu) + 4u);
	v = EF_U32(node + 0x1cu);
	EF_U32(slot + 0x6e0u) = 1u;
	EF_U32(slot + 0x69cu) = v;
	EF_U32(slot + 0x6a0u) = v;
	EF_U32(slot + 0x6e8u) = EF_U32(0x00159144u);
	return p;
}

/* 0xbe480, 281 bytes. Build a 0x78-byte record DST from SRC1 (EDX), SRC2 (EBX) and SRC3 (ECX): copy header bytes and 18 bytes from SRC2+0x11, then fill the type-specific part (types 2/10 via 0xbdeec, 1 via 0xbe438, 11, 5/0, others) and finish with 0xbb324(DST+0x55, kind, DST[0]) (ret 4, s0 = extra argument). */
void build_record_be480(ef_addr32 dst, ef_addr32 src1, ef_addr32 src2, ef_addr32 src3, uint32_t s0)
{
	uint32_t type, kind, i;
	ef_addr32 t;

	EF_U8(dst) = EF_U8(src1 + 8u);
	EF_U8(dst + 1u) = EF_U8(src1 + 3u);
	type = EF_U8(src1 + 9u);
	EF_U8(dst + 3u) = 0u;
	EF_U8(dst + 2u) = (uint8_t)type;
	for (i = 0; i < 18u; i++)
		EF_U8(dst + 8u + i) = EF_U8(src2 + 0x11u + i);
	type = EF_U8(dst + 2u);
	EF_U32(dst + 0x69u) = 0u;
	if (type == 2u || type == 0xau) {
		EF_U8(dst + 0x3au) = EF_U8(src3 + 0xbu);
		EF_U32(dst + 0x3bu) = EF_U32(src3 + 0xcu);
		for (i = 0; i < 18u; i++)
			EF_U8(dst + 0x3fu + i) = EF_U8(src3 + 0x16u + i);
		kind = guest_fn_bdeec_x(src3, dst, s0, EF_U32(src3 + 0xcu)) & 0xffu;
	} else if (type == 1u) {
		EF_U8(dst + 0x3au) = EF_U8(src3 + 0xbu);
		for (i = 0; i < 18u; i++)
			EF_U8(dst + 0x3fu + i) = EF_U8(src3 + 0x16u + i);
		EF_U32(dst + 0x3bu) = EF_U32(src3 + 0xcu);
		EF_U32(dst + 0x51u) = 0u;
		EF_U32(dst + 0x74u) = 0u;
		EF_U8(dst + 0x6du) = 2u;
		t = find_target_be438(EF_U32(src3 + 0x16u), EF_U32(src3 + 0x1au), s0);
		EF_U16(dst + 0x6eu) = EF_U16(t + 0x10u);
		EF_U16(dst + 0x70u) = 0u;
		kind = 6u;
	} else if (type == 0xbu) {
		EF_U32(dst + 0x3bu) = 0u;
		kind = 5u;
		EF_U32(dst + 0x51u) = 0u;
	} else {
		if (type == 5u || type == 0u) {
			if (src3 != 0u)
				EF_U32(dst + 0x3bu) = EF_U32(src3 + 0xcu);
		} else {
			EF_U32(dst + 0x3bu) = 0u;
		}
		EF_U32(dst + 0x51u) = 0u;
		kind = 4u;
		EF_U32(dst + 0x74u) = 0u;
	}
	init_event_record_bb324(dst + 0x55u, kind, EF_U8(dst));
}

/* 0x80b14, 293 bytes. Register the update message P (EAX) in the 16-entry table at 0x1fc8a2 (12-byte entries keyed by word P[0] % 16): when the key matches, mark the 56-byte record 0x1f90a0 + 56 * (entry.hi + entry.idx) as active (byte 0 = 1, dword 8 = 1), build a rotation from the angles P[0xe..0x12] >> 16, transform the vector P[0x16..0x1e], store float positions (P[2], P[6], P[0xa] plus the transformed values >> 8) in the record at +0x24/+0x28/+0x2c, advance entry.idx modulo 16 and copy word P[0xe] to entry+8. All registers are preserved. */
void register_update_message_80b14(ef_addr32 p)
{
	int32_t out[3];
	int32_t sum, v, rem;
	ef_addr32 ent, rec;
	int16_t idx;
	float f;

	rem = (int32_t)(int16_t)EF_U16(p) % 16;
	ent = 0x001fc8a2u + (uint32_t)(rem * 12);
	if (EF_U16(p) != EF_U16(ent))
		return;
	idx = (int16_t)EF_U16(ent + 4u);
	sum = ((int32_t)EF_U32(ent) >> 16) + idx;
	rec = 0x001f90a0u + (uint32_t)(sum * 56);
	EF_U8(rec) = 1u;
	EF_U32(rec + 8u) = 1u;
	build_rotation_e994c(1u, (uint32_t)((int32_t)EF_U32(p + 0xeu) >> 16), (uint32_t)((int32_t)EF_U32(p + 0x10u) >> 16), (uint32_t)((int32_t)EF_U32(p + 0x12u) >> 16));
	transform_vector_e99b8((ef_addr32)(uintptr_t)out, (int32_t)EF_U32(p + 0x16u), (int32_t)EF_U32(p + 0x1au), (int32_t)EF_U32(p + 0x1eu));
	v = (int32_t)EF_U32(p + 2u) + (out[0] >> 8);
	f = (float)v;
	memcpy(&sum, &f, 4);
	EF_U32(rec + 0x24u) = (uint32_t)sum;
	v = (int32_t)EF_U32(p + 6u) + (out[1] >> 8);
	f = (float)v;
	memcpy(&sum, &f, 4);
	EF_U32(rec + 0x28u) = (uint32_t)sum;
	v = (int32_t)EF_U32(p + 0xau) + (out[2] >> 8);
	f = (float)v;
	memcpy(&sum, &f, 4);
	EF_U32(rec + 0x2cu) = (uint32_t)sum;
	EF_U16(ent + 4u) = (uint16_t)(((int32_t)idx + 1) % 16);
	EF_U16(ent + 8u) = EF_U16(p + 0xeu);
}

/* 0x49e00, 294 bytes. Rebuild the word map at 0x1a5290. In mode 1 (byte 0x125849) clear 0x20 bytes, then for each of the (word 0x1357b0) entries store its 1-based number at the slot given by the sum of the first B[0x19bbf2+i] words of 0x1a53b0 plus B[0x19bbfa+i]. Otherwise walk the record list 0x137960 (node selected by the high word of 0x125833): for each group, a flagged group (byte at P[0x13] + 0x7c * group) is zero filled for W[0x1a53b0+2*group] slots, an unflagged group is filled with the running number 1, 2, ... (zero once it exceeds word 0x1357b0). Registers are preserved. */
void rebuild_word_map_49e00(void)
{
	int16_t si, cx, ax, dx, bx;
	int32_t acc, n;
	ef_addr32 p;

	if (EF_U8(0x00125849u) == 1u) {
		fill_bytes_b3fb0(0x001a5290u, 0u, 0x20u);
		n = (int16_t)EF_U16(0x001357b0u);
		si = 0;
		if (n <= 0)
			return;
		do {
			acc = 0;
			ax = 0;
			while (ax < (int16_t)EF_U8(0x0019bbf2u + (uint32_t)(int32_t)si)) {
				acc += EF_U16(0x001a53b0u + (uint32_t)(int32_t)ax * 2u);
				ax++;
			}
			acc += EF_U8(0x0019bbfau + (uint32_t)(int32_t)si);
			dx = (int16_t)acc;
			EF_U16(0x001a5290u + (uint32_t)(int32_t)dx * 2u) = (uint16_t)(si + 1);
			si++;
		} while (si < (int16_t)EF_U16(0x001357b0u));
		return;
	}
	p = list_nth(0x00137960u, (uint32_t)((int32_t)EF_U32(0x00125833u) >> 16));
	ax = 0;
	cx = 0;
	bx = 1;
	for (;;) {
		if (EF_U8(EF_U32(p + 0x13u) + (uint32_t)((int32_t)cx * 0x7c)) != 0u) {
			dx = 0;
			while (dx < (int16_t)EF_U16(0x001a53b0u + (uint32_t)(int32_t)cx * 2u)) {
				EF_U16(0x001a5290u + (uint32_t)(int32_t)ax * 2u) = 0u;
				ax++;
				dx++;
			}
			cx++;
			continue;
		}
		if (cx >= (int16_t)EF_U8(p + 0x12u))
			return;
		dx = 0;
		while (dx < (int16_t)EF_U16(0x001a53b0u + (uint32_t)(int32_t)cx * 2u)) {
			if (bx > (int16_t)EF_U16(0x001357b0u)) {
				EF_U16(0x001a5290u + (uint32_t)(int32_t)ax * 2u) = 0u;
				ax++;
			} else {
				EF_U16(0x001a5290u + (uint32_t)(int32_t)ax * 2u) = (uint16_t)bx;
				ax++;
				bx++;
			}
			dx++;
		}
		cx++;
		if (cx >= (int16_t)EF_U8(p + 0x12u))
			return;
	}
}

/* 0xabb00, 294 bytes. Compute a clipped view window from record p into the word block at 0x41eee2..0x41ef00 and scale two of its extents by 1/(double at p+0x9f), truncated toward zero. Registers are preserved. */
void compute_view_window_abb00(ef_addr32 p)
{
	int16_t si, cx, dx;
	int32_t i1, i2;
	uint32_t t;
	double inv;

	EF_U16(0x0041eef6u) = EF_U16(0x0041ec08u);
	EF_U16(0x0041eef8u) = EF_U16(0x0041ec0au);
	EF_U16(0x0041eefau) = EF_U16(0x0041ec0cu);
	EF_U16(0x0041ef00u) = EF_U16(0x0041ec0eu);
	si = (int16_t)(EF_U16(p + 0x41u) + EF_U16(p + 0x71u));
	cx = (int16_t)(EF_U16(p + 0x73u) + EF_U16(p + 0x43u));
	EF_U16(0x0041eeeau) = EF_U16(p + 0x75u);
	EF_U16(0x0041eeecu) = EF_U16(p + 0x77u);
	dx = (int16_t)(si - (int16_t)EF_U16(p + 0x69u));
	EF_U16(0x0041eee6u) = (uint16_t)dx;
	dx = (int16_t)(cx - (int16_t)EF_U16(p + 0x6bu));
	EF_U16(0x0041eee8u) = (uint16_t)dx;
	EF_U16(0x0041eee4u) = EF_U16(p + 0x65u);
	EF_U16(0x0041eee2u) = EF_U16(p + 0x67u);
	i1 = ((int32_t)EF_U32(0x0041eefcu) >> 16) - (int32_t)si + ((int32_t)EF_U32(p + 0x67u) >> 16);
	double d;
	memcpy(&d, ef_guest_memory_ptr(p + 0x9fu), sizeof d);
	inv = 1.0 / d;
	t = EF_U32(0x0041eefau);
	EF_U16(0x0041eef4u) = (uint16_t)(int32_t)truncate_9d72c((double)i1 * inv);
	i2 = ((int32_t)t >> 16) - (int32_t)cx + ((int32_t)EF_U32(p + 0x69u) >> 16);
	EF_U16(0x0041eef0u) = (uint16_t)si;
	EF_U16(0x0041eef2u) = (uint16_t)(int32_t)truncate_9d72c(inv * (double)i2);
	EF_U16(0x0041eeeeu) = (uint16_t)cx;
}

/* 0xd36c4, 302 bytes. (ret 4) Allocate the next 0x6ec byte entry (byte counter 0x17d2c0, table 0x486db0, at most 200) for the object s0: clear flag bits 1-2 of byte s0+0x20, set up the scratch state at 0x4209xx, copy 0x50 bytes from 0x420970 to entry+0x684 and 0x30 bytes from s0 to entry+0x84, then fill the entry's links and extents. Over the limit the error handler at 0x132a74 is called. Returns s0. */
uint32_t alloc_object_entry_d36c4(uint32_t s0)
{
	uint32_t i, n;
	ef_addr32 e, q;
	int32_t v;
	uint64_t bits;
	double d, k;

	EF_U32(0x0042099cu) = 1u;
	EF_U32(0x00420974u) = 0u;
	EF_U32(0x004209bcu) = 0x004209d0u;
	EF_U8(s0 + 0x20u) = (uint8_t)(EF_U8(s0 + 0x20u) & 0xf9u);
	EF_U32(0x004209d0u) = 1u;
	EF_U32(0x004209d4u) = s0;
	n = EF_U8(0x0017d2c0u);
	if ((int32_t)n >= 0xc8) {
		guest_callptr3_x(EF_U32(0x00132a74u), 0x0018f814u, n, 0xc8u);
		return s0;
	}
	e = 0x00486db0u + n * 0x6ecu;
	EF_U8(0x0017d2c0u) = (uint8_t)(n + 1u);
	for (i = 0; i < 0x14u; i++)
		EF_U32(e + 0x684u + i * 4u) = EF_U32(0x00420970u + i * 4u);
	EF_U32(e) = 1u;
	EF_U32(e + 0x6d4u) = 0x000d3844u;
	for (i = 0; i < 0xcu; i++)
		EF_U32(e + 0x84u + i * 4u) = EF_U32(s0 + i * 4u);
	EF_U32(e + 0x6d0u) = e;
	v = (int32_t)EF_U32(e + 0x8cu);
	memcpy(&k, ef_guest_memory_ptr(0x0018f840u), sizeof k);
	d = (double)v * k;
	memcpy(&bits, &d, sizeof bits);
	EF_U32(e + 0x6d8u) = (uint32_t)bits;
	EF_U32(e + 0x6dcu) = (uint32_t)(bits >> 32);
	q = EF_U32(EF_U32(0x004209bcu) + 4u);
	EF_U32(e + 0x694u) = EF_U32(q + 0x18u);
	q = EF_U32(EF_U32(0x004209bcu) + 4u);
	EF_U32(e + 0x698u) = EF_U32(q + 0x18u) + 1u;
	q = EF_U32(EF_U32(0x004209bcu) + 4u);
	EF_U32(e + 0x69cu) = EF_U32(q + 0x1cu);
	q = EF_U32(EF_U32(0x004209bcu) + 4u);
	EF_U32(e + 0x6e0u) = 4u;
	EF_U32(e + 0x6a0u) = EF_U32(q + 0x1cu) + 1u;
	EF_U32(e + 0x6e8u) = EF_U32(0x00159144u);
	return s0;
}

/* 0xd6478, 303 bytes. Builds the 256-entry remap tables (one per shade group, 0x100 bytes each, starting at [0x15b958]+0x100) from the 3-byte palette entries at 0x420a90 and the group sizes at 0x431ed0. */
void build_shade_tables_d6478(void)
{
	uint32_t sums[256];
	uint32_t base = EF_U32(0x0015b958u);
	uint32_t first = 0;
	uint32_t group = 0;
	uint32_t dst;
	uint32_t n;
	uint32_t i;
	uint32_t j;

	if (base == 0u)
		return;
	dst = base + 0x100u;
	while (EF_U8(0x00431ed0u + group) != 0) {
		n = EF_U8(0x00431ed0u + group);
		memset(sums, 2, sizeof sums);
		for (j = 0; (int32_t)j < (int32_t)n; j++)
			sums[j] = sum_three_bytes(0x00420a90u + (first + j) * 3u);
		for (i = 0; i < 0x100u; i++) {
			if (i < first || i >= first + n) {
				int32_t v = (int32_t)sum_three_bytes(0x00420a90u + i * 3u);
				uint32_t best = 0;
				int32_t bestd = 0x100;
				for (j = 0; (int32_t)j < (int32_t)n; j++) {
					int32_t d = (int32_t)sums[j] - v;
					if (d < 0)
						d = -d;
					if (d < bestd) {
						best = j;
						bestd = d;
					}
				}
				EF_U8(dst + i) = (uint8_t)(best + first);
			} else {
				EF_U8(dst + i) = (uint8_t)i;
			}
		}
		first += EF_U8(0x00431ed0u + group);
		group++;
		dst += 0x100u;
	}
}

/* 0xcf788, 4 bytes. Implicit-register fragment (ESI = record pointer): jumps into 0xcf794, the body of 0xcf78c, with EAX = ESI. Forward the five words of the record at RECORD to cf3a4. */
void forward_record_cf788(ef_addr32 record)
{
	guest_fn_cf3a4(EF_U16(record + 2u), EF_U16(record + 4u), EF_U16(record + 6u), EF_U16(record + 8u), EF_U16(record));
}

/* 0x1111e1, 4 bytes. Save the flags register and disable interrupts (PUSHF; POP EAX; CLI). The flags image is modelled as 0x202 (interrupts enabled), as the guest runs with them enabled. */
uint32_t save_flags_disable_interrupts_1111e1(void)
{
	disable_interrupts_11f2d3();
	return 0x202u;
}

/* 0x222, 9 bytes. Load the data segment register from the word at code-segment offset 0x120cd4. Segments are flat in this port, so there is no observable effect. */
void load_ds_from_120cd4_222(void)
{
}

/* 0x10e414, 2 bytes. Epilogue fragment (POP EDI; RET) of the neighbouring routine; the register pop has no observable effect here. */
void pop_edi_ret_10e414(void)
{
}

/* 0x1111e5, 21 bytes. Restore the interrupt flag from a saved flags image (cdecl FLAGS): interrupts are enabled when bit 9 (0x200) is set, otherwise left disabled. */
void restore_flags_1111e5(uint32_t flags)
{
	disable_interrupts_11f2d3();
	if (flags & 0x200u)
		enable_interrupts_11f2d5();
}

/* 0xfa332, 35 bytes. Implicit-register fragment (ESI, EDX out; EAX, EBX, ECX preserved; carry set on return): refill the bit-stream buffer. Reads from the handle at 0x16278c into the 0x162fe0 buffer (size at 0x1631e0), points ESI at the buffer and sets EDX to bytes-read minus one. */
void bitstream_refill_fa332(uint32_t *esi, int32_t *edx)
{
	uint32_t n = guest_fn_a79d4_ret(EF_U32(0x16278cu), EF_U32(0x1631e0u), 0x162fe0u);
	*esi = 0x162fe0u;
	*edx = (int32_t)n - 1;
}

/* 0xfa31b, 23 bytes. Implicit-register fragment (ESI = stream pointer, EDX = remaining bytes minus one; both updated): fetch the next two bytes of the buffered stream, refilling when empty; returns them as a little-endian word (first byte in AL). */
uint16_t bitstream_read_word_fa31b(uint32_t *esi, int32_t *edx)
{
	uint8_t lo, hi;
	if (--*edx < 0)
		bitstream_refill_fa332(esi, edx);
	lo = EF_U8((*esi)++);
	if (--*edx < 0)
		bitstream_refill_fa332(esi, edx);
	hi = EF_U8((*esi)++);
	return (uint16_t)(lo | (hi << 8));
}

/* 0xfa2ce, 22 bytes. Implicit-register fragment (CL = bit count, AL = bit buffer with a trailing 1 sentinel, ESI, EDX as in 0xfa31b; AL/ESI/EDX updated): read CL bits MSB first and return them in AH (starting from 0). */
uint8_t bitstream_read_bits_fa2ce(uint8_t count, uint8_t *buf, uint32_t *esi, int32_t *edx)
{
	uint8_t acc = 0;
	uint8_t al = *buf;
	unsigned carry;
	while (count--) {
		carry = al >> 7;
		al = (uint8_t)(al << 1);
		if (al == 0) {
			if (--*edx < 0)
				bitstream_refill_fa332(esi, edx);
			al = EF_U8((*esi)++);
			carry = al >> 7;
			al = (uint8_t)((al << 1) | 1u);
		}
		acc = (uint8_t)((acc << 1) | carry);
	}
	*buf = al;
	return acc;
}

/* 0xc4680, 27 bytes. Stack argument SRC (callee cleanup): shift the three triples at SRC left by 9 into the table at 0x1590c0. */
void shift_triples_to_1590c0_c4680(ef_addr32 src)
{
	shift_triples_c59b7(src, 0x1590c0u, 9u);
}

/* 0xe0b6c, 28 bytes. Opens the device named at 0x15f4fd, waits 0x96 ticks, then falls through into 0xe0b88 with EAX = 0x15f5bd (a second device open). */
void open_devices_e0b6c(void)
{
	open_device_e0b88(0x15f4fdu);
	wait_ticks_or_press_e115c(0x96u);
	open_device_e0b88(0x15f5bdu);
}

/* 0x1204b6, 21 bytes. Debugger hook: when the flag byte at 0x17ee30 is set the original executes INT 3 (a debugger-only breakpoint call) and returns its result; otherwise returns 0. Without a debugger attached the result is 0, so this draft always returns 0. */
uint32_t debugger_hook_1204b6(void)
{
	return 0u;
}

/* 0xed566, 35 bytes. Wide-character counterpart of 0xed542: length in 16-bit units of the string at SEG:OFFSET (selector ignored in the flat model), stopping at a zero word or when MAX is reached. Parameters in the order offset (EAX), seg (EDX), max (EBX). */
uint32_t wcsnlen_far_ed566(uint32_t offset, uint32_t seg, uint32_t max)
{
	uint32_t n = 0u;
	(void)seg;
	for (;;) {
		if (EF_U16(offset) == 0u)
			break;
		offset += 2u;
		if (n == max)
			break;
		n++;
	}
	return n;
}

/* 0xfa1d4, 44 bytes. Implicit-register fragment (AL = bit buffer with trailing 1 sentinel, ESI = byte pointer, EDX = bytes left, params in that order; AL/ESI/EDX updated): decode one Huffman-style symbol. Walks the bit tree table at 0x1627a0 one bit (MSB first) at a time until an entry is zero, then returns the byte at 0x1629a0 indexed by the final node. */
uint8_t bitstream_decode_symbol_fa1d4(uint8_t *buf, uint32_t *esi, int32_t *edx)
{
	uint8_t al = *buf;
	uint32_t node = 0u;
	uint8_t entry;
	do {
		unsigned carry = al >> 7;
		al = (uint8_t)(al << 1);
		if (al == 0) {
			if (--*edx < 0)
				bitstream_refill_fa332(esi, edx);
			al = EF_U8((*esi)++);
			carry = al >> 7;
			al = (uint8_t)((al << 1) | 1u);
		}
		node = (node * 2u) + carry;
		entry = EF_U8(0x1627a0u + node);
		if (entry != 0u)
			node = entry;
	} while (entry != 0u);
	*buf = al;
	return EF_U8(0x1629a0u + node);
}

/* 0x10db58, 38 bytes. Implicit-register fragment (AL = bit buffer with trailing 1 sentinel, ESI = byte pointer, EBP = table base, params in that order; AL/ESI updated): same decode as 0xfa1d4 but without refill, using the tables at EBP-0x410 (tree) and EBP-0x210 (symbols). */
uint8_t bitstream_decode_symbol_10db58(uint8_t *buf, uint32_t *esi, uint32_t ebp)
{
	uint8_t al = *buf;
	uint32_t node = 0u;
	uint8_t entry;
	do {
		unsigned carry = al >> 7;
		al = (uint8_t)(al << 1);
		if (al == 0) {
			al = EF_U8((*esi)++);
			carry = al >> 7;
			al = (uint8_t)((al << 1) | 1u);
		}
		node = (node * 2u) + carry;
		entry = EF_U8(ebp + node - 0x410u);
		if (entry != 0u)
			node = entry;
	} while (entry != 0u);
	*buf = al;
	return EF_U8(ebp + node - 0x210u);
}

/* 0x11d614, 43 bytes. Register parameters AL = interrupt vector, EBX = handler offset, ECX = handler selector. Sets a DOS interrupt vector (INT 21h AH=25h). When the extender mode byte at 0x132a5e is 2..8, the vector number is passed in CL and AL is 4 (AX = 0x2504) instead. EBX is clobbered by the mode byte, the handler offset travels in EDX. */
void set_dos_vector_11d614(uint32_t vector, uint32_t offset, uint32_t selector)
{
	uint32_t in[7] = {0};
	uint32_t out[7] = {0};
	uint8_t sregs[12];
	uint8_t mode = EF_U8(0x132a5eu);
	segread_9be0d((uint32_t)(uintptr_t)sregs);
	in[3] = offset;
	if (mode >= 2u && mode <= 8u) {
		in[0] = 0x2504u;
		in[2] = vector & 0xffu;
	} else {
		in[0] = 0x2500u | (vector & 0xffu);
		in[2] = selector;
	}
	in[1] = mode;
	guest_dos_int_x(0x21u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
}

/* 0x10dc0d, 16 bytes. Implicit-register fragment (CL = bit count, AL = bit buffer with trailing 1 sentinel, ESI = byte pointer, params in that order; AL/ESI updated): like 0xfa2ce but without refill; reads CL bits MSB first and returns them in AH. */
uint8_t bitstream_read_bits_10dc0d(uint8_t count, uint8_t *buf, uint32_t *esi)
{
	uint8_t acc = 0;
	uint8_t al = *buf;
	do {
		unsigned carry = al >> 7;
		al = (uint8_t)(al << 1);
		if (al == 0) {
			al = EF_U8((*esi)++);
			carry = al >> 7;
			al = (uint8_t)((al << 1) | 1u);
		}
		acc = (uint8_t)((acc << 1) | carry);
	} while (--count != 0);
	*buf = al;
	return acc;
}

/* 0xc474c, 9 bytes. Implicit-register fragment (EAX, EBX, ECX in that order, all updated): shift each left by 16. */
void shift_left16_three_c474c(uint32_t *eax, uint32_t *ebx, uint32_t *ecx)
{
	*eax <<= 16;
	*ebx <<= 16;
	*ecx <<= 16;
}

/* 0x10d040, 325 bytes. Register parameters PATTERN (EAX, 32-bit dash mask rotated right one bit per pixel), COLOR (DL) and RECT (EBX, four words x0, y0, x1, y1). Bresenham line from (x0,y0) to (x1,y1) into the frame buffer whose base pointer is at 0x15b754 and whose row offsets are the dwords at 0x163200; a pixel is drawn only when the rotated pattern's low bit is set. */
void draw_patterned_line_10d040(uint32_t pattern, uint8_t color, ef_addr32 rect)
{
	int32_t x = EF_U16(rect + 0u);
	int32_t y = EF_U16(rect + 2u);
	int32_t dx = (int32_t)(EF_U16(rect + 4u) - EF_U16(rect + 0u));
	int32_t dy = (int32_t)(EF_U16(rect + 6u) - EF_U16(rect + 2u));
	int32_t adx2 = 2 * (dx < 0 ? -dx : dx);
	int32_t ady2 = 2 * (dy < 0 ? -dy : dy);
	int32_t sx = (dx < 0) ? -1 : 1;
	int32_t sy = (dy < 0) ? -1 : 1;
	int32_t err;

	if (adx2 > ady2) {
		err = ady2 - (adx2 >> 1);
		for (;;) {
			pattern = (pattern >> 1) | (pattern << 31);
			if (pattern & 1u)
				EF_U8(EF_U32(0x15b754u) + EF_U32(0x163200u + (uint32_t)y * 4u) + (uint32_t)x) = color;
			if ((uint32_t)x == EF_U16(rect + 4u))
				break;
			if (err >= 0) {
				y += sy;
				err -= adx2;
			}
			x += sx;
			err += ady2;
		}
	} else {
		err = adx2 - (ady2 >> 1);
		for (;;) {
			pattern = (pattern >> 1) | (pattern << 31);
			if (pattern & 1u)
				EF_U8(EF_U32(0x163200u + (uint32_t)y * 4u) + (uint32_t)x + EF_U32(0x15b754u)) = color;
			if ((uint32_t)y == EF_U16(rect + 6u))
				break;
			if (err >= 0) {
				x += sx;
				err -= ady2;
			}
			y += sy;
			err += adx2;
		}
	}
}

/* 0x10d188, 45 bytes. Implicit-register fragment (ESI = pointer just past a record whose byte at -4 is a color and byte at -2 is a pattern byte, rect at ESI-? passed in EBX = ESI): draws the line at rect EBX = ESI with pattern = byte[ESI-4] replicated in all four bytes and color = byte[ESI-2]. */
void draw_patterned_line_10d188(ef_addr32 esi)
{
	uint32_t b = EF_U8(esi - 4u);
	uint32_t pat = b | (b << 8) | (b << 16) | (b << 24);
	draw_patterned_line_10d040(pat, EF_U8(esi - 2u), esi);
}

/* 0xc6800, 11 bytes. Implicit-register fragment (EAX, EDX, EBX pass through unchanged to the callee): call the function pointer at 0x159158 with ECX = 1; the code after it belongs to the next routine. */
void call_hook_159158_c6800(uint32_t eax, uint32_t edx, uint32_t ebx)
{
	guest_callptr4_x(EF_U32(0x159158u), eax, edx, ebx, 1u);
}

/* 0xc6428, 15 bytes. Implicit-register fragment (ESI is an in/out pointer; result in AL): load the word at ESI (advancing ESI by 2), add the byte 0x159140 to its low byte and translate it through the shade table whose base is dword 0x159084. */
uint8_t shade_lookup_c6428(uint32_t *esi)
{
	uint8_t al = (uint8_t)EF_U16(*esi);
	*esi += 2u;
	al = (uint8_t)(al + EF_U8(0x159140u));
	return EF_U8(EF_U32(0x159084u) + al);
}

/* 0xc64d0, 20 bytes. Implicit-register fragment (ESI is an in/out pointer; result in AL): like 0xc6428 but also stores the loaded low byte to byte 0x15913c first. */
uint8_t shade_lookup_store_c64d0(uint32_t *esi)
{
	uint8_t al = (uint8_t)EF_U16(*esi);
	*esi += 2u;
	EF_U8(0x15913cu) = al;
	al = (uint8_t)(al + EF_U8(0x159140u));
	return EF_U8(EF_U32(0x159084u) + al);
}

/* 0x9bdbb, 79 bytes. Run the registered exit handlers with priority between MIN (AL) and MAX (DL): the 6-byte records at 0x195ade..0x195af0 hold a state byte (2 = already run), a priority byte and a handler pointer (dword at +2). Repeatedly pick the pending record chosen by the scan (each candidate whose priority is not below the running value replaces the choice), call its handler if the chosen priority is not above MAX and the pointer is non-zero, mark it done, and rescan until nothing is pending. */
void run_exit_handlers_9bdbb(uint32_t eax, uint32_t edx)
{
	uint8_t max = (uint8_t)edx;
	uint8_t min = (uint8_t)eax;

	for (;;) {
		uint32_t esi = 0x195adeu;
		uint32_t edi = 0x195af0u;
		uint32_t ebx = edi;
		uint8_t al = min;

		while (esi < edi) {
			if (EF_U8(esi) != 2u && !(al > EF_U8(esi + 1u))) {
				ebx = esi;
				al = EF_U8(esi + 1u);
			}
			esi += 6u;
		}
		if (ebx == edi)
			return;
		if (!(al > max)) {
			uint32_t fn = EF_U32(ebx + 2u);
			if (fn != 0u)
				guest_callptr0_x(fn);
		}
		EF_U8(ebx) = 2u;
	}
}

/* 0x1e9, 57 bytes. Fatal exit with a message: STRING (EDX, zero-terminated) is written to the file opened by name 0x120d1b (INT 21h AX=3D01h) with AH=40h and the string length, then the exit handlers run for priorities 0..0xff and the program terminates through INT 21h AH=4Ch with the exit code EAX (cdecl-less register convention: EAX = code, EDX = message). */
void fatal_exit_1e9(uint32_t code, uint32_t message)
{
	uint32_t in[7], out[7];
	uint8_t sregs[12];
	uint32_t len = 0u;

	segread_9be0d((uint32_t)(uintptr_t)sregs);
	in[0] = 0x3d01u; in[1] = 0u; in[2] = 0u; in[3] = 0x120d1bu; in[4] = 0u; in[5] = 0u; in[6] = 0u;
	guest_dos_int_x(0x21u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
	while (EF_U8(message + len) != 0u)
		len++;
	in[0] = 0x4000u; in[1] = out[0] & 0xffffu; in[2] = len; in[3] = message; in[4] = 0u; in[5] = 0u; in[6] = 0u;
	guest_dos_int_x(0x21u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
	run_exit_handlers_9bdbb(0u, 0xffu);
	in[0] = 0x4c00u | (code & 0xffu); in[1] = 0u; in[2] = 0u; in[3] = 0u; in[4] = 0u; in[5] = 0u; in[6] = 0u;
	guest_dos_int_x(0x21u, (uint32_t)(uintptr_t)in, (uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)sregs);
}

/* 0x11d258, 29 bytes. Report an error: if the debugger hook (0x1204b6) returns zero, call the fatal exit 0x1e9 with the same two register arguments (EAX = code, EDX = message). */
void report_error_11d258(uint32_t code, uint32_t message)
{
	if (debugger_hook_1204b6() == 0u)
		fatal_exit_1e9(code, message);
}

/* 0xac780, 37 bytes. Tail fragment of the preceding routine (reached when its test succeeds; the original jumps back to the return at 0xac77e otherwise): when the word at 0x41eed2 equals 1, clear the registered callbacks and the UI state, show the text at 0x41f262 and finish by tail-calling the redraw. */
void show_status_ac780(void)
{
	if (EF_U16(0x41eed2u) != 1u)
		return;
	guest_tail_abec4(0u);
	clear_ui_state_ac16c();
	show_text_ac408(0x41f262u);
	redraw_ca17c();
}

/* 0xc4755, 136 bytes. Implicit-register fragment (in: EAX, EBX, ECX; out: ESI, EDI, EBP via out[0..2]): subtract the self-modified camera origin immediates (0xc4756/0xc475c/0xc4762) from the vector, then multiply by the 3x3 fixed-point matrix at 0x1590c0 (64-bit accumulate, >>25). */
void rotate_vector_c4755(int32_t x, int32_t y, int32_t z, int32_t *out)
{
    int32_t v[3];
    int i;
    v[0] = x - (int32_t)EF_U32(0x000c4756u);
    v[1] = y - (int32_t)EF_U32(0x000c475cu);
    v[2] = z - (int32_t)EF_U32(0x000c4762u);
    for (i = 0; i < 3; i++) {
        int64_t s = (int64_t)(int32_t)EF_S32(0x1590c0u + 12u * i) * v[0]
                  + (int64_t)(int32_t)EF_S32(0x1590c4u + 12u * i) * v[1]
                  + (int64_t)(int32_t)EF_S32(0x1590c8u + 12u * i) * v[2];
        out[i] = (int32_t)(uint32_t)(((uint64_t)s >> 25) | ((uint64_t)s >> 32 << 7));
    }
}

/* 0x1101e3, 5 bytes. Implicit-register fragment (EAX, EDX): returns immediately when EDX is zero, otherwise falls into the key translator 0x1101e8 with EAX; the result (EAX, -1 after translation) is returned. When EDX is zero EAX is passed through unchanged. */
int32_t translate_key_if_1101e3(uint32_t code, uint32_t flag)
{
    if (flag == 0u) {
        return (int32_t)code;
    }
    return translate_key_1101e8(code);
}

/* 0xc46bc, 60 bytes. Patch the three self-modified origin immediates at 0xc4756/0xc475c/0xc4762 from the dwords at 0x1590b4/0x1590b8/0x1590bc, then run 0xc4654 with the dwords 0x1590b0, 0x1590ac, 0x1590a8 (all registers preserved). */
void load_origin_and_run_c46bc(void)
{
    EF_U32(0x000c4756u) = EF_U32(0x001590b4u);
    EF_U32(0x000c475cu) = EF_U32(0x001590b8u);
    EF_U32(0x000c4762u) = EF_U32(0x001590bcu);
    set_params_and_run_c4654(0u, 0u, 0u, 0u, EF_U32(0x001590b0u), EF_U32(0x001590acu), EF_U32(0x001590a8u));
}

/* 0x275a4, 60 bytes. Draw the fixed centered text list: the 120-byte entry table at 0x274c0 is copied to the stack (read-only use, so the guest table is passed directly), 0x19b430/0x19b434 are set to 0xdd650/0xdd6a0, and the list is drawn with y = step = 10. */
void draw_fixed_text_list_275a4(void)
{
    store_dword_pair_19b430(0xdd6a0u, 0xdd650u);
    draw_centered_entries_27544(0x274c0u, 10, 10);
}

/* 0xc8c78, 63 bytes. Implicit-register fragment (ESI = source vertex words, ECX = count; params in that order): temporarily point the vertex-buffer base 0x17bf70 at 0x159244, load COUNT vertices, build the identity word list 0, 0x30, 0x60, ... at 0x159190 and render it through 0xd2500, then restore the base. */
void render_vertex_list_c8c78(ef_addr32 src, uint32_t count)
{
    uint32_t saved = EF_U32(0x0017bf70u);
    uint32_t i;
    EF_U32(0x0017bf70u) = 0x159244u;
    load_vertices_c48dc(src, count);
    for (i = 0; i < count; i++) {
        EF_U16(0x159190u + 2u * i) = (uint16_t)(0x30u * i);
    }
    build_render_list_d2500(0x159190u, count);
    EF_U32(0x0017bf70u) = saved;
}

/* 0xfc354, 69 bytes. strdup: allocate strlen+1 bytes through 0xfc795 and copy the string including the NUL; returns 0 if allocation fails. */
ef_addr32 strdup_fc354(ef_addr32 s)
{
    uint32_t len = 0;
    ef_addr32 p;
    uint32_t i;
    while (EF_U8(s + len) != 0) {
        len++;
    }
    len++;
    p = guest_fn_fc795(len);
    if (p != 0) {
        for (i = 0; i < len; i++) {
            EF_U8(p + i) = EF_U8(s + i);
        }
    }
    return p;
}

/* 0x15c071, 83 bytes. Implicit-register fragment (in: ECX = depth (16.16), ESI = x, EDI = y, EBX = output address; out: EAX): perspective-project; if the depth is outside 16..20000 store -1 to *OUT and return 0, else store two words (screen x,y offset by 0x15bdb8/0x15bdbc) and return 1. */
uint32_t project_point_15c071(uint32_t depth, int32_t x, int32_t y, ef_addr32 outp)
{
    int32_t d = (int32_t)depth >> 16;
    int32_t q;
    int64_t p;
    if (d < 0x10 || d > 0x4e20) {
        EF_U32(outp) = 0xffffffffu;
        return 0;
    }
    q = (int32_t)((int64_t)0x100000000LL / d);
    p = (int64_t)(x >> 8) * q;
    EF_U16(outp) = (uint16_t)((uint32_t)(p >> 32) + (uint32_t)EF_U16(0x0015bdb8u));
    p = (int64_t)(y >> 8) * q;
    EF_U16(outp + 2u) = (uint16_t)((uint32_t)(p >> 32) + (uint32_t)EF_U16(0x0015bdbcu));
    return 1;
}

/* 0x15c0ea, 38 bytes. Stack-argument wrapper (ret 0x10): rotate the vector (C, B, A) through 0xc4755 and project it with 0x15c071 into OUTP; returns the projection result. */
uint32_t rotate_project_15c0ea(uint32_t a, uint32_t b, uint32_t c, ef_addr32 outp)
{
    int32_t r[3];
    rotate_vector_c4755((int32_t)c, (int32_t)b, (int32_t)a, r);
    return project_point_15c071((uint32_t)r[2], r[0], r[1], outp);
}

/* 0xa46f0, 38 bytes. Implicit-register fragment (ESI = destination pair, EAX = name, EDX = widget key; params in that order): look up NAME (aborting if missing), look up the widget KEY inside it, store both pointers at DST[0], DST[1] and return DST. */
ef_addr32 lookup_named_widget_pair_a46f0(ef_addr32 dst, ef_addr32 name, ef_addr32 key)
{
    ef_addr32 root = find_named_or_abort_a46cc(name);
    ef_addr32 widget = find_widget_checked_a78bc(root, key);
    EF_U32(dst) = root;
    EF_U32(dst + 4u) = widget;
    return dst;
}

/* 0x160122, 52 bytes. Stack-argument wrapper (ret 0x14: OUT, Z, Y, X, MARGIN): shift X, Y, Z left by 16 and rotate them through 0xc474c/0xc4755, store the three results at OUT, then run the view-volume test with MARGIN; the low bit shifted out of the test result drives the return (-1 if it was clear, else 0). */
int32_t rotate_view_test_160122(ef_addr32 out, uint32_t z, uint32_t y, uint32_t x, uint32_t margin)
{
    int32_t r[3];
    uint32_t t;
    rotate_vector_c4755((int32_t)(x << 16), (int32_t)(y << 16), (int32_t)(z << 16), r);
    EF_U32(out) = (uint32_t)r[0];
    EF_U32(out + 4u) = (uint32_t)r[1];
    EF_U32(out + 8u) = (uint32_t)r[2];
    t = point_in_view_volume_ceb40(0u, 0u, 0u, 0u, (int32_t)(margin & 0xffffu), r[2], r[1], r[0]);
    return (t & 1u) ? 0 : -1;
}

/* 0x160156, 52 bytes. Same as 0x160122 but without the left shift by 16 of X, Y, Z (enters 0xc4755 directly). */
int32_t rotate_view_test_160156(ef_addr32 out, uint32_t z, uint32_t y, uint32_t x, uint32_t margin)
{
    int32_t r[3];
    uint32_t t;
    rotate_vector_c4755((int32_t)x, (int32_t)y, (int32_t)z, r);
    EF_U32(out) = (uint32_t)r[0];
    EF_U32(out + 4u) = (uint32_t)r[1];
    EF_U32(out + 8u) = (uint32_t)r[2];
    t = point_in_view_volume_ceb40(0u, 0u, 0u, 0u, (int32_t)(margin & 0xffffu), r[2], r[1], r[0]);
    return (t & 1u) ? 0 : -1;
}

/* 0xfdf60, 349 bytes. (ret 12; three stack arguments R, Y, X) Draw a filled disc of radius R-1 centred at (X, Y) (signed words) with colour byte 0x159138, clipped against the window words 0x15bdc0..0x15bdcc (x min/max, y min/max, all exclusive for the single-pixel cases). R <= 0 draws nothing; R == 1 plots one pixel (pointer 0xfc894, 0xfedcc), R == 2 a 2x2 block (pointer 0xfc898, 0xfeddc), larger discs (radius capped at 200) must lie completely inside the window and are filled row by row with the two circle walks 0xfde20/0xfdec0 and the range callback 0xfddf0, whose centre dword lives in the original's stack frame (here the scratch dword 0x485ff4). */
void draw_disc_fdf60(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t r, uint32_t y, uint32_t x)
{
    int32_t s, xs, ys, xl, xr, yt, yb, err;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if ((int16_t)r < 0)
        return;
    r = (r - 1u) & 0xffffu;
    s = (int16_t)r;
    xs = (int16_t)x;
    ys = (int16_t)y;
    if (s < 2) {
        if (xs <= (int32_t)EF_U32(0x0015bdc0u) || xs >= (int32_t)EF_U32(0x0015bdc4u))
            return;
        if (ys <= (int32_t)EF_U32(0x0015bdc8u) || ys >= (int32_t)EF_U32(0x0015bdccu))
            return;
        if (r == 0u)
            put_pixel_fedcc((uint32_t)ys, (uint32_t)xs, EF_U8(0x00159138u));
        else if (r == 1u)
            put_block2_feddc((uint32_t)ys, (uint32_t)xs, EF_U8(0x00159138u));
        return;
    }
    if (s > 0xc8)
        r = 0xc8u;
    s = (int16_t)r;
    xl = (int16_t)(x - (uint32_t)s);
    xr = (int16_t)(x + (uint32_t)s);
    yt = (int16_t)(y - (uint32_t)s);
    yb = (int16_t)(y + (uint32_t)s);
    if (xr < (int32_t)EF_U32(0x0015bdc0u))
        return;
    if (xl > (int32_t)EF_U32(0x0015bdc4u))
        return;
    if (yb < (int32_t)EF_U32(0x0015bdc8u))
        return;
    if (yt > (int32_t)EF_U32(0x0015bdccu))
        return;
    EF_U32(0x00485ff4u) = (uint32_t)xs;
    EF_U32(0x00485fecu) = (uint32_t)yt << 16;
    if (xr > (int32_t)EF_U32(0x0015bdc4u))
        return;
    if (xl < (int32_t)EF_U32(0x0015bdc0u))
        return;
    if (yb > (int32_t)EF_U32(0x0015bdccu))
        return;
    if (yt < (int32_t)EF_U32(0x0015bdc8u))
        return;
    err = (1 - s) * 2;
    circle_walk_up_fde20(0, s, err, 0, 0x000fddf0u, 0x00485ff4u);
    circle_walk_down_fdec0(s, 0, err, s, 0x000fddf0u, 0x00485ff4u);
}
