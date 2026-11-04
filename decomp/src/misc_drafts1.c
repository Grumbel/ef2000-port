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
