/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

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
        guest_fn_a8578_x((uint32_t)(int32_t)(int16_t)edx, (uint32_t)(int32_t)(int16_t)ebx, col, eax);
    } else if (mode == 1u) {
        uint32_t entry = (index & 0xffff0000u) | EF_U16(0x00418780u + (uint32_t)(int32_t)(int16_t)index * 2u);
        uint32_t x = edx + (0x13fu - entry);
        guest_fn_a8578_x((uint32_t)(int32_t)(int16_t)x, (uint32_t)(int32_t)(int16_t)ebx, col, eax);
    } else if (mode == 2u) {
        uint16_t x = (uint16_t)(edx + EF_U16(0x004187d0u + (uint32_t)(int32_t)(int16_t)index * 2u));
        guest_fn_a8578_x((uint32_t)(int32_t)(int16_t)x, (uint32_t)(int32_t)(int16_t)ebx, col, eax);
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
