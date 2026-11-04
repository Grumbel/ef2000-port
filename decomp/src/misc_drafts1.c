/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <stdarg.h>
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
