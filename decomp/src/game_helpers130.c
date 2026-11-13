/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Sub-record initialisation and area list dump. */

/* 0xb4fb4, 391 bytes. Initialise the sub-records (0x7c bytes each, count at +0x12, array at +0x13) of an object from a template: allocate each one's 0x19-byte state block, copy the template's position and extent words, and clear the remaining fields. */
void init_sub_records_b4fb4(ef_addr32 obj, ef_addr32 tmpl)
{
    int16_t i;

    for (i = 0; i < (int16_t)EF_U8(obj + 0x12u); i++) {
        uint32_t off = (uint32_t)((int32_t)i * 0x7c);
        ef_addr32 block;

        EF_U16(EF_U32(obj + 0x13u) + off + 0x1cu) = 1;
        EF_U16(EF_U32(obj + 0x13u) + off + 0x1au) = 0;
        EF_U16(EF_U32(obj + 0x13u) + off + 0x1eu) = 0;
        EF_U32(EF_U32(obj + 0x13u) + off + 0x20u) = guest_alloc_fmt_cc6c0(0x19u, 0x0018e8d0u, 0x19u);
        block = EF_U32(EF_U32(obj + 0x13u) + off + 0x20u);
        EF_U32(block) = EF_U32(tmpl + 0x11u);
        block = EF_U32(EF_U32(obj + 0x13u) + off + 0x20u);
        EF_U32(block + 4u) = EF_U32(tmpl + 0x15u);
        block = EF_U32(EF_U32(obj + 0x13u) + off + 0x20u);
        EF_U16(block + 8u) = EF_U16(tmpl + 0x1du);
        block = EF_U32(EF_U32(obj + 0x13u) + off + 0x20u);
        EF_U16(block + 0xau) = EF_U16(tmpl + 0x1fu);
        block = EF_U32(EF_U32(obj + 0x13u) + off + 0x20u);
        EF_U16(block + 0xcu) = 0;
        EF_U16(block + 0xeu) = 0;
        EF_U16(block + 0x10u) = 0;
        EF_U16(block + 0x12u) = 0;
        EF_U16(block + 0x14u) = 0;
        EF_U16(block + 0x16u) = 0;
        EF_U8(block + 0x18u) = 0x0d;
        EF_U8(EF_U32(obj + 0x13u) + off + 0x3au) = 0;
        EF_U32(EF_U32(obj + 0x13u) + off + 0x3bu) = 0;
        EF_U32(EF_U32(obj + 0x13u) + off + 0x3fu) = 0;
        EF_U32(EF_U32(obj + 0x13u) + off + 0x43u) = 0;
        EF_U32(EF_U32(obj + 0x13u) + off + 0x47u) = 0;
        EF_U16(EF_U32(obj + 0x13u) + off + 0x4bu) = 0;
        EF_U16(EF_U32(obj + 0x13u) + off + 0x4du) = 0;
        EF_U16(EF_U32(obj + 0x13u) + off + 0x4fu) = 0;
        EF_U32(EF_U32(obj + 0x13u) + off + 0x51u) = 0;
        EF_U8(EF_U32(obj + 0x13u) + off + 0x6du) = 0;
        EF_U16(EF_U32(obj + 0x13u) + off + 0x6eu) = 0;
        EF_U16(EF_U32(obj + 0x13u) + off + 0x70u) = 0;
        EF_U16(EF_U32(obj + 0x13u) + off + 0x72u) = 0;
        EF_U32(EF_U32(obj + 0x13u) + off + 0x74u) = 0;
    }
}

/* Clamp a coordinate to 0..0x18f. */
static int32_t clamp_screen_b31f0(int32_t v)
{
    if (v > 0x18f)
        v = 0x18f;
    if (v < 0)
        v = 0;
    return v;
}

/* 0xb31f0, 388 bytes. Dump the area list: the number of matching entries (type 0x14 whose first byte pairs with the entry's kind), then each match's clamped box (x-12, y+12, x+12, y-12) from the extent record behind +0x13. */
void dump_area_list_b31f0(ef_addr32 list_holder, uint32_t handle)
{
    ef_addr32 node;
    int16_t count = 0;

    for (node = EF_U32(list_holder); node != 0; node = EF_U32(node + 4u)) {
        if (EF_U8(node + 0xau) == 0x14
            && (uint16_t)pair_matches_cc81c(EF_U8(node + 9u), EF_U8(EF_U32(node + 0x13u))) != 0)
            count++;
    }
    print_to_handle_b1bb0(handle, 0x0018e5b0u, (int32_t)count);
    for (node = EF_U32(list_holder); node != 0; node = EF_U32(node + 4u)) {
        if (EF_U8(node + 0xau) == 0x14
            && (uint16_t)pair_matches_cc81c(EF_U8(node + 9u), EF_U8(EF_U32(node + 0x13u))) != 0) {
            ef_addr32 ext = EF_U32(EF_U32(node + 0x13u) + 0x20u);
            int32_t x = EF_S32(ext);
            int32_t y = EF_S32(ext + 4u);
            print_to_handle_b1bb0(handle, 0x0018e5bcu, clamp_screen_b31f0(x - 0xc), clamp_screen_b31f0(y + 0xc),
                                  clamp_screen_b31f0(x + 0xc), clamp_screen_b31f0(y - 0xc));
        }
    }
}

/* 0xb3168, 136 bytes. (ret 0x14) Write a debug dump to the file NAME (EAX): the list LIST (EDX; first node at +0, links at +4) with header text from 0xb3374, the window words S1..S4 (low halves, signed) and the word S0 for the point list POINTS (ECX), then the area list of LIST; EDX/EBX of 0xb2d04 are passed through. The file is closed afterwards. */
void dump_lists_to_file_b3168(ef_addr32 name, ef_addr32 list, uint32_t e, ef_addr32 points,
                              uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4)
{
    int32_t handle = open_slot_a7948(0u, 0u, 0u, 0u, name);
    ef_addr32 header = report_message_b3374(0x0018e56cu);

    dump_list_b2d04(list, e, (uint32_t)handle, header, (uint32_t)(int32_t)(int16_t)s1,
                    (uint32_t)(int32_t)(int16_t)s2, (uint32_t)(int32_t)(int16_t)s3,
                    (uint32_t)(int32_t)(int16_t)s4);
    guest_fn_cc768(header);
    dump_point_list_b2e28((uint32_t)handle, points, (int16_t)s0, 0x0018e590u);
    dump_area_list_b31f0(list, (uint32_t)handle);
    close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
}

/* 0xbfb88, 336 bytes. Test whether the record OBJ (byte +0xb is its side, dwords +0xf/+0x13 and +0x11/+0x15 two positions, the high words of the first pair index a cell) falls inside a box: a base size (12 or 36 wide, 12 or 24 high, depending on the side) is shifted by the bytes 0x1a364b (when 0x1a364c is set; scaled by 1/3 above 0x46 or below 0x1e) and 0x1264e2 (9 - value), in opposite directions for the two sides. HEAD (EBX) is the cell list for 0xbfb2c and B (EDX) for 0xfba6c. Returns 1 when the cell count exceeds the box width and the match count is below its height. */
uint32_t position_in_box_bfb88(ef_addr32 obj, ef_addr32 b, ef_addr32 head)
{
    int32_t width = 0xc;
    int32_t height = 0xc;
    int32_t shift;
    int32_t half;
    int32_t extra;
    int16_t cells;
    int16_t matches;
    uint32_t first_side = (uint32_t)EF_U8(obj + 0xbu);

    if (first_side != 1u) {
        width = 0x24;
        height = 0x18;
    }
    if (EF_U8(0x001a364cu) != 0u) {
        int32_t level = (int32_t)EF_U8(0x001a364bu);

        shift = 0;
        if (level >= 0x46)
            shift = (level - 0x46) / 3;
        level = (int32_t)EF_U8(0x001a364bu);
        if (level <= 0x1e)
            shift = -(0x1e - level) / 3;
        half = (int32_t)(int16_t)shift;
        half = half / 2;
        if (EF_U8(obj + 0xbu) != 1u) {
            width -= shift;
            height = (int32_t)(int16_t)height - half;
        } else {
            width += shift;
            height = (int32_t)(int16_t)height + half;
        }
    }
    extra = 9 - (int32_t)EF_U8(0x001264e2u);
    half = extra / 2;
    if (EF_U8(obj + 0xbu) != 1u) {
        width -= extra;
        height = (int32_t)(int16_t)height - half;
    } else {
        width += extra;
        height = (int32_t)(int16_t)height + half;
    }
    cells = (int16_t)count_records_at_cell_bfb2c((int16_t)((int32_t)EF_U32(obj + 0xfu) >> 16),
                                                 (int16_t)((int32_t)EF_U32(obj + 0x13u) >> 16), head);
    matches = (int16_t)count_matching_entries_fba6c(EF_U32(obj + 0x11u), EF_U32(obj + 0x15u), b);
    return (int16_t)width > cells && matches < (int16_t)height;
}

/* 0xc1034, 142 bytes. For each index 0..count-1 of the object list LIST (count from 0xbc5d0), when a node of kind 0x19 exists for that index (0xcc51c) the running value (EDI, initially DI) is replaced by the result of 0xc1210, called with the sign-extended running value, the index byte, BP and LIST in registers and ARG4 plus five stack arguments. Returns the running value. */
uint32_t loop_objects_c1034(int32_t di, uint32_t bp, ef_addr32 list, uint32_t arg4, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5)
{
    uint32_t value = (uint32_t)di;
    int16_t count = (int16_t)guest_fn_bc5d0_x(list);
    int16_t i;

    for (i = 0; i < count; i++) {
        if ((uint16_t)find_node_matching_cc51c((uint32_t)(int32_t)i, list, 0x19u) != 0u)
            value = guest_fn_c1210((int16_t)value, (uint8_t)i, bp, list, arg4, s1, s2, s3, s4, s5);
    }
    return value;
}

/* 0x418c4, 210 bytes. When the input-state copy (0xd530) leaves its flag byte zero and either 0x19769a or 0x126576 is set, walk the OBJ records (count word +0x1c, array at +0x20, stride 0x19): store each record's first word and 0x190 minus its word at +4 into the 5-byte table at 0x1a4aa0 (+0 previous record's byte at +0x18, +1/+3 the two words) and call 0x4161c with ARG, the index, the byte, the word and the second value. */
void draw_segment_records_418c4(ef_addr32 obj, int16_t arg)
{
    uint16_t input_a;
    uint16_t input_b;
    uint8_t flag;
    int16_t i;
    int16_t count;

    copy_runtime_input_state((ef_addr32)(uintptr_t)&input_a, (ef_addr32)(uintptr_t)&input_b,
                             (ef_addr32)(uintptr_t)&flag);
    if (flag != 0u)
        return;
    if (EF_U16(0x0019769au) == 0u && EF_U8(0x00126576u) == 0u)
        return;
    count = EF_S16(obj + 0x1cu);
    EF_U16(0x001a4b9cu) = (uint16_t)count;
    if (EF_U32(obj + 0x20u) == 0u || count <= 0)
        return;
    for (i = 0; i < count; i = (int16_t)(i + 1)) {
        uint32_t rec = EF_U32(obj + 0x20u) + (uint32_t)((int32_t)i * 0x19);
        uint16_t first = EF_U16(rec);
        uint16_t second = (uint16_t)(0x190u - EF_U16(rec + 4u));
        uint32_t slot = 0x001a4aa0u + (uint32_t)((int32_t)i * 5);
        uint8_t byte;

        EF_U16(slot + 1u) = first;
        EF_U16(slot + 3u) = second;
        if (i > 0)
            byte = EF_U8(EF_U32(obj + 0x20u) + (uint32_t)(((int32_t)i - 1) * 0x19) + 0x18u);
        else
            byte = EF_U8(EF_U32(obj + 0x20u) + 0x18u);
        EF_U8(slot) = byte;
        guest_fn_4161c((int32_t)arg, (int32_t)i, byte, (int16_t)first, (int16_t)second);
        count = EF_S16(obj + 0x1cu);
    }
}

/* 0xbfa58, 212 bytes. Walk the list whose head pointer is at HEAD (next at +4); for each node of type 0x31 (byte +0xc) with sign word +0x4e equal to 1 and count byte +0x32 above 4, create a slot record (0xfc0ec), and if 0xbdbf8 accepts it call 0xbe59c with the node and the arguments, replacing VALUE with its result; the slot record is then freed (0xb16c0). Returns VALUE. */
uint16_t scan_nodes_bfa58(uint16_t value, ef_addr32 head, uint32_t arg_b, uint32_t arg_c, uint32_t s1, uint32_t s2, uint32_t s3)
{
    uint32_t node = EF_U32(head);

    while (node != 0u) {
        if (EF_U8(node + 0xcu) == 0x31u && ((int32_t)EF_U32(node + 0x4eu) >> 16) == 1 &&
            EF_U8(node + 0x32u) > 4u) {
            int32_t hi = (int32_t)(int16_t)value;
            uint32_t rec = create_slot_record_fc0ec(EF_U8(node + 0xbu), 1u);

            if (guest_fn_bdbf8(rec, node, hi, s3) != 0u)
                value = guest_fn_be59c(rec, 0u, 0u, node, (uint32_t)hi, 0u, 1u, arg_c, s1, 0u, s2, head, arg_b, 0u, s3);
            free_entry_array_b16c0(rec);
        }
        node = EF_U32(node + 4u);
    }
    return value;
}

/* 0xb3084, 226 bytes. (ret 0x1c) Write a debug dump to the file NAME: the status lines (0xb1bf8), a line with the six words S1..S6 (low halves, signed), then four sections each started by a header from 0xb3374 (0xb3038 for LIST_A, 0xb1c34 for LIST_B, 0xb3648 for BLOCK, 0xb3580 for S1), and close the file. S7 is not used. */
void dump_state_b3084(ef_addr32 name, ef_addr32 list_a, ef_addr32 list_b, uint32_t block, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5, uint32_t s6, uint32_t s7)
{
    int32_t handle = open_slot_a7948(0u, 0u, 0u, 0u, name);
    ef_addr32 header;

    (void)s7;
    print_status_lines_b1bf8((uint32_t)handle);
    print_to_handle_b1bb0((uint32_t)handle, 0x0018e4fcu, (int32_t)(int16_t)s1, (int32_t)(int16_t)s2,
                          (int32_t)(int16_t)s3, (int32_t)(int16_t)s4, (int32_t)(int16_t)s5, (int32_t)(int16_t)s6);
    header = report_message_b3374(0x0018e524u);
    dump_group_list_b3038(list_a, (uint32_t)handle, header);
    guest_fn_cc768(header);
    header = report_message_b3374(0x0018e534u);
    dump_entry_list_b1c34(list_b, (uint32_t)handle, header);
    guest_fn_cc768(header);
    header = report_message_b3374(0x0018e544u);
    guest_fn_b3648(block, (uint32_t)handle, header);
    guest_fn_cc768(header);
    header = report_message_b3374(0x0018e558u);
    dump_buffer_b3580(s1, (uint32_t)handle, header);
    guest_fn_cc768(header);
    close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
}

/* 0xdb32c, 244 bytes. Parse one text line of the parameter file at the cursor 0x425af8 (split at LF/NUL; as in the 15 sibling readers the byte before the terminator is cleared too): scan a word plus three integers, append a suffix string, store the word value at 0x425bb8, then rotate a point by the two angles (the integers scaled from degrees by 1/360 of a full turn, 16-bit) and store the resulting words at 0x425bb2, 0x425bb4 and 0x425bb0. */
void parse_angle_line_db32c(void)
{
    char text[0x14];
    int16_t pt[2];
    int32_t a18;
    int32_t a1c;
    int32_t a20;
    ef_addr32 start = EF_U32(0x425af8u);
    ef_addr32 end = start;
    int16_t saved;

    while (EF_U8(end) != 0u && EF_U8(end) != 10u)
        ++end;
    EF_U8(end) = 0u;
    ++end;
    EF_U8(end - 2u) = 0u;
    EF_U32(0x425af8u) = end;
    sscanf_df26f(0u, 0u, 0u, 0u, start, 0x00190328u, (uint32_t)(uintptr_t)text, (uint32_t)(uintptr_t)&a20,
                 (uint32_t)(uintptr_t)&a1c, (uint32_t)(uintptr_t)&a18);
    strcat_9ca57((ef_addr32)(uintptr_t)text, 0x00190324u);
    EF_U16(0x425bb8u) = guest_load_halfword((ef_addr32)(uintptr_t)text);
    load_sincos_dc7bc((uint32_t)((int32_t)((uint32_t)a18 << 16) / 0x168) & 0xffffu);
    rotate_point_dc7e4((ef_addr32)(uintptr_t)pt, (int16_t)a20, 0);
    load_sincos_dc7bc((uint32_t)((int32_t)((uint32_t)a1c << 16) / 0x168) & 0xffffu);
    saved = pt[1];
    rotate_point_dc7e4((ef_addr32)(uintptr_t)pt, 0, pt[0]);
    EF_U16(0x425bb2u) = (uint16_t)pt[0];
    EF_U16(0x425bb4u) = (uint16_t)saved;
    EF_U16(0x425bb0u) = (uint16_t)pt[1];
}

/* 0x4e204, 347 bytes. Pick a target at the screen point (X, Y) for the slot byte at 0x1a559f: unless its state byte (0x1a5672 + slot) is 8 (done) or the latch 0x1a5709 is already set, set the state to 0x12 (state 5 first clears the latch) and scan the list at 0x137960 for type-0x10 nodes accepted by 0xcc22c whose box words (+8, 0x190 - +0xc of the record at +0x13) are within 7 of the point; the last match's words and its value (a word at +0x55 of the record at +0x20, times 100) are stored in the slot's tables (0x1a55a2, 0x1a55d6, 0x1a560a), the state becomes 5 and the latch is set. */
void pick_target_at_4e204(int16_t x, int16_t y)
{
    uint32_t node = EF_U32(0x00137960u);
    uint32_t slot = EF_U8(0x001a559fu);
    int32_t value = 0;
    uint32_t found_x = 0u;
    uint32_t found_y = 0u;

    if (EF_U8(0x001a5672u + slot) == 8u)
        return;
    if (EF_U8(0x001a5672u + slot) == 5u)
        EF_U8(0x001a5709u) = 0u;
    EF_U8(0x001a5672u + EF_U8(0x001a559fu)) = 0x12u;
    if (EF_U8(0x001a5709u) == 1u)
        return;
    for (; node != 0u; node = EF_U32(node + 4u)) {
        if (EF_U8(node + 0xau) == 0x10u
            && (uint16_t)group_flag_positive_cc22c(0u, EF_U8(node + 0xbu), 0x00137980u) != 0u) {
            ef_addr32 ext = EF_U32(node + 0x13u);
            int16_t bx = EF_S16(ext + 8u);
            int16_t by = (int16_t)(0x190u - EF_U16(ext + 0xcu));

            if (x > (int32_t)bx - 7 && x < (int32_t)bx + 7 && y > (int32_t)by - 7 && y < (int32_t)by + 7) {
                value = ((int32_t)EF_U32(EF_U32(ext + 0x20u) + 0x55u) >> 16) * 100;
                found_x = (uint16_t)bx;
                found_y = (uint16_t)by;
            }
        }
    }
    if ((uint16_t)found_x == 0u && (uint16_t)found_y == 0u)
        return;
    slot = EF_U8(0x001a559fu);
    EF_U16(0x001a55a2u + slot * 2u) = (uint16_t)found_x;
    EF_U16(0x001a55d6u + EF_U8(0x001a559fu) * 2u) = (uint16_t)found_y;
    EF_U8(0x001a5672u + EF_U8(0x001a559fu)) = 5u;
    EF_U32(0x001a560au + EF_U8(0x001a559fu) * 4u) = (uint32_t)value;
    EF_U8(0x001a5709u) = 1u;
}

/* 0xcfde4, 381 bytes. Intersect the ray ORIGIN + t*DIR (three dwords each) with the plane record PLANE (signed words: point at +0, normal at +0x2a..+0x2e, offset at +0x30, basis words from +0x32, polygon at +0x3e). The ray parameter is a fixed-point quotient (shifts from the bytes 0x15b7c8 and 0x15b7c9); when the plane is not parallel to the ray the hit point is stored at OUT and the hit is projected onto the basis and tested with 0xcf960. Returns 1 when that test succeeds, else 0. */
uint8_t ray_hits_plane_cfde4(ef_addr32 out, ef_addr32 dir, ef_addr32 origin, ef_addr32 plane)
{
    int32_t pa = EF_S16(plane + 0x2cu);
    int32_t pb = EF_S16(plane + 0x2au);
    int32_t pc = EF_S16(plane + 0x2eu);
    int32_t denom = (int32_t)((uint32_t)EF_S32(dir + 4u) * (uint32_t)pa + (uint32_t)EF_S32(dir) * (uint32_t)pb
                              + (uint32_t)EF_S32(dir + 8u) * (uint32_t)pc);
    int32_t numer;
    int32_t t;
    int32_t relx;
    int32_t rely;
    int32_t relz;
    int32_t u;
    int32_t v;

    denom >>= (EF_U8(0x0015b7c8u) & 31u);
    if (denom == 0)
        return 0;
    numer = (int32_t)((uint32_t)EF_S32(origin + 4u) * (uint32_t)pa + (uint32_t)pb * (uint32_t)EF_S32(origin)
                      + (uint32_t)pc * (uint32_t)EF_S32(origin + 8u)
                      + ((uint32_t)(int32_t)EF_S16(plane + 0x30u) << 15));
    t = numer / denom;
    {
        uint32_t sh = EF_U8(0x0015b7c9u) & 31u;

        EF_S32(out) = EF_S32(origin) - ((int32_t)((uint32_t)EF_S32(dir) * (uint32_t)t) >> sh);
        EF_S32(out + 4u) = EF_S32(origin + 4u) - ((int32_t)((uint32_t)EF_S32(dir + 4u) * (uint32_t)t) >> sh);
        EF_S32(out + 8u) = EF_S32(origin + 8u) - ((int32_t)((uint32_t)EF_S32(dir + 8u) * (uint32_t)t) >> sh);
    }
    relx = EF_S32(out) - EF_S16(plane);
    rely = EF_S32(out + 4u) - EF_S16(plane + 2u);
    relz = EF_S32(out + 8u) - EF_S16(plane + 4u);
    u = (int32_t)((uint32_t)rely * (uint32_t)(int32_t)EF_S16(plane + 0x34u)
                  + (uint32_t)EF_S16(plane + 0x32u) * (uint32_t)relx
                  + (uint32_t)relz * (uint32_t)(int32_t)EF_S16(plane + 0x36u));
    v = (int32_t)((uint32_t)EF_S16(plane + 0x3au) * (uint32_t)rely
                  + (uint32_t)EF_S16(plane + 0x38u) * (uint32_t)relx
                  + (uint32_t)EF_S16(plane + 0x3cu) * (uint32_t)relz);
    v = -v;
    return point_in_triangle_cf960(plane + 0x3eu, u >> 15, v >> 15) != 0u ? 1u : 0u;
}

/* 0xcaa28, 392 bytes. Initialise every node of the list whose head pointer is at HEAD (next at +4): when 0x137992 or 0x137994 is set swap the dwords at +0x1a and +0x1e, pick the words +0x32/+0x34 (random 1000..5000 for node types 0x31, 0x49, 0x0b, 0x56, else 1000/500), the word +0x28 from 0xca9fc and the byte +0xa from TABLE_A at the cell index (y/10*0x28 + x/10, y = 0x190 - dword +0x1a, x = dword +0x16); then always clear +0x30/+0x36, set the byte +9 from TABLE_B at the same index and reset the sub-records (count word +0x2a, array at +0x2c, stride 0x15: weight byte +6 from 0xcbd30, byte +5 = 100 when 0x137992 is set, dword +1 and byte +0x14 cleared). */
void init_nodes_from_grids_caa28(ef_addr32 head, ef_addr32 table_a, ef_addr32 table_b)
{
    ef_addr32 node;

    for (node = EF_U32(head); node != 0u; node = EF_U32(node + 4u)) {
        int32_t cell;
        int16_t count;
        int16_t i;

        if (EF_U16(0x00137992u) != 0u || EF_U16(0x00137994u) != 0u) {
            uint32_t first = EF_U32(node + 0x1eu);
            uint32_t second = EF_U32(node + 0x1au);
            uint8_t type;

            EF_U32(node + 0x1au) = first;
            type = EF_U8(node + 0xbu);
            EF_U32(node + 0x1eu) = second;
            if (type == 0x31u || type == 0x49u || type == 0x0bu || type == 0x56u) {
                uint32_t r = (uint16_t)step_generator_a45b3();
                int32_t v;

                EF_U16(node + 0x32u) = (uint16_t)(((int32_t)(r % 5u) + 1) * 1000);
                v = EF_S16(node + 0x32u);
                EF_U16(node + 0x34u) = (uint16_t)(v / 2);
            } else {
                EF_U16(node + 0x32u) = 0x3e8u;
                EF_U16(node + 0x34u) = 0x1f4u;
            }
            EF_U16(node + 0x28u) = (uint16_t)grid_index_ca9fc((int32_t)EF_U32(node + 0x16u),
                                                              0x190 - (int32_t)EF_U32(node + 0x1au));
            cell = ((0x190 - (int32_t)EF_U32(node + 0x1au)) / 10) * 0x28 + (int32_t)EF_U32(node + 0x16u) / 10;
            EF_U8(node + 0xau) = EF_U8(table_a + (uint32_t)cell);
        }
        cell = ((0x190 - (int32_t)EF_U32(node + 0x1au)) / 10) * 0x28 + (int32_t)EF_U32(node + 0x16u) / 10;
        EF_U8(node + 0x30u) = 0u;
        EF_U16(node + 0x36u) = 0u;
        EF_U8(node + 9u) = EF_U8(table_b + (uint32_t)cell);
        count = EF_S16(node + 0x2au);
        for (i = 0; i < count; i = (int16_t)(i + 1)) {
            uint32_t rec = EF_U32(node + 0x2cu) + (uint32_t)((int32_t)i * 0x15);

            EF_U8(rec + 6u) = scan_code_weight_cbd30(EF_U8(rec));
            if (EF_U16(0x00137992u) != 0u)
                EF_U8(rec + 5u) = 0x64u;
            EF_U32(rec + 1u) = 0u;
            EF_U8(rec + 0x14u) = 0u;
            count = EF_S16(node + 0x2au);
        }
    }
}

/* 0x65a88, 402 bytes. Advance the current target selection (word 0x1a823e, mirrored to 0x1a8238): starting from the retry budget 0x2119f4 (decremented by 0x1a822a per round), step to the next unit with 0x663a4 (mode 0 when 0x1a8246 is set, or 0x127da8 with 0x12584b == 1; otherwise mode 2 keeping the old value on failure) and reject the candidate object (0x7489c of the index in the high word of 0x1a823c) when it is the player's own record (+0x16 == 0x1f71ca), has byte +0x18d set, its record is inactive or farther than 0x2800 (0x23760 of the position >> 8), has flag 8 at +0x24, or is refused by 0x66bf4. Ends with 0xffff when no candidate is left. */
void select_next_target_65a88(void)
{
    uint16_t budget = EF_U16(0x002119f4u);
    ef_addr32 obj = guest_fn_7489c((int16_t)((int32_t)EF_S32(0x001f71c8u) >> 16));

    if (obj == 0u) {
        EF_U16(0x001a823eu) = 0xffffu;
        EF_U16(0x001a8238u) = 0xffffu;
        return;
    }
    for (;;) {
        int32_t index;

        if (EF_U8(0x001a8246u) != 0u || (EF_U8(0x00127da8u) != 0u && EF_U8(0x0012584bu) == 1u)) {
            EF_U16(0x001a823eu) = (uint16_t)find_next_unit_663a4((uint32_t)((int32_t)EF_S32(0x001a823cu) >> 16), 0u);
        } else {
            uint16_t old = EF_U16(0x001a823eu);
            uint16_t found = (uint16_t)find_next_unit_663a4((uint32_t)(int32_t)(int16_t)old, 2u);

            EF_U16(0x001a823eu) = found;
            if ((int16_t)found == -1)
                EF_U16(0x001a823eu) = old;
        }
        index = (int32_t)EF_S32(0x001a823cu) >> 16;
        if (index == -1) {
            EF_U16(0x001a823eu) = 0xffffu;
            EF_U16(0x001a8238u) = 0xffffu;
            obj = 0u;
        } else {
            obj = guest_fn_7489c(index);
            if (obj != 0u) {
                int32_t vec[3];
                int32_t metric;

                vec[0] = (int32_t)EF_S32(obj) >> 8;
                vec[1] = (int32_t)EF_S32(obj + 4u) >> 8;
                vec[2] = (int32_t)EF_S32(obj + 8u) >> 8;
                metric = guest_fn_23760_x((ef_addr32)(uintptr_t)vec);
                if (EF_U16(obj + 0x16u) == EF_U16(0x001f71cau))
                    obj = 0u;
                if (obj != 0u && EF_U8(obj + 0x18du) != 0u)
                    obj = 0u;
                if (obj != 0u) {
                    uint8_t active = active_unit_record_contains((int16_t)((int32_t)EF_S32(obj + 0x14u) >> 16));

                    if (!(active != 0u && EF_U8(0x00122886u) != 0u) && metric > 0x2800)
                        obj = 0u;
                }
                if (obj != 0u && (EF_U8(obj + 0x24u) & 8u) != 0u)
                    obj = 0u;
                if (obj != 0u && request_los_66bf4(obj) != 0u)
                    obj = 0u;
            }
        }
        budget = (uint16_t)(budget - EF_U16(0x001a822au));
        if (obj != 0u || (int16_t)budget <= 0)
            break;
    }
    if (obj != 0u && (EF_U8(obj + 0x24u) & 8u) != 0u)
        obj = 0u;
    if (obj == 0u)
        EF_U16(0x001a823eu) = 0xffffu;
    EF_U16(0x001a8238u) = EF_U16(0x001a823eu);
}

/* 0x14d58, 409 bytes. Draw the framed box described by record INDEX of the 19-byte table at 0x1225d4 (byte +1 text slot, +2 x, +4 y, +6 width, +8 height, byte +0 colour pair index into the 3-byte table at 0x12285a): when 0xcd7f0 accepts the slot, enter scroll mode with its result, fill the interior with the first colour and draw the four edges with the second colour through the 0x4208e4.. line record (0xcf78c), then leave scroll mode. */
void draw_framed_box_14d58(int16_t index)
{
    uint32_t rec = 0x001225d4u + (uint32_t)((int32_t)index * 19);
    int32_t x = EF_S16(rec + 2u);
    int32_t w = EF_S16(rec + 6u);
    int32_t h = EF_S16(rec + 8u);
    int32_t y = EF_S16(rec + 4u);
    uint32_t colours;
    uint32_t slot;
    int32_t right;
    int32_t bottom;

    (void)y;
    slot = get_text_slot_cd7f0((int16_t)EF_U8(rec + 1u));
    if (slot == 0u)
        return;
    enter_scroll_mode_c39f0(slot);
    colours = 0x0012285au + (uint32_t)EF_U8(rec) * 3u;
    y = (int16_t)EF_U16(rec + 4u);
    fill_rect_ceccc(EF_U8(colours), x + 1, y + 1, x + w - 2, y + h - 2);
    right = x + w - 1;
    bottom = y + h - 1;
    EF_U16(0x004208e4u) = EF_U8(colours + 1u);
    EF_U16(0x004208e6u) = (uint16_t)x;
    EF_U16(0x004208e8u) = (uint16_t)y;
    EF_U16(0x004208eau) = (uint16_t)right;
    EF_U16(0x004208ecu) = (uint16_t)y;
    forward_record_cf78c();
    EF_U16(0x004208e4u) = EF_U8(colours + 1u);
    EF_U16(0x004208e6u) = (uint16_t)x;
    EF_U16(0x004208eau) = (uint16_t)x;
    EF_U16(0x004208e8u) = (uint16_t)y;
    EF_U16(0x004208ecu) = (uint16_t)bottom;
    forward_record_cf78c();
    EF_U16(0x004208e4u) = EF_U8(colours + 1u);
    EF_U16(0x004208e6u) = (uint16_t)right;
    EF_U16(0x004208eau) = (uint16_t)right;
    EF_U16(0x004208e8u) = (uint16_t)y;
    EF_U16(0x004208ecu) = (uint16_t)bottom;
    forward_record_cf78c();
    EF_U16(0x004208e4u) = EF_U8(colours + 1u);
    EF_U16(0x004208e6u) = (uint16_t)x;
    EF_U16(0x004208e8u) = (uint16_t)bottom;
    EF_U16(0x004208eau) = (uint16_t)right;
    EF_U16(0x004208ecu) = (uint16_t)bottom;
    forward_record_cf78c();
    leave_scroll_mode_c39ac();
}

/* 0xeb520, 373 bytes. Normalise NAME into a lowercase key (strip the context prefix at +0x94/+0x124, replace a leading known directory prefix by its index digit and a known extension by its letter), hash it and binary-search the context's entry table; returns the entry or 0. */
ef_addr32 lookup_name_entry_eb520(ef_addr32 name, ef_addr32 context)
{
    char key[0x90];
    char work[0x9c];
    uint32_t hash;
    uint32_t index;
    uint32_t prefix;
    ef_addr32 dot;
    ef_addr32 keyAddr = (ef_addr32)(uintptr_t)key;
    ef_addr32 workAddr = (ef_addr32)(uintptr_t)work;

    if (context == 0u)
        return 0u;
    copy_string_9d749(workAddr, name);
    lowercase_string(workAddr);
    prefix = EF_U32(context + 0x124u);
    if (prefix != 0u && compare_strings_e5c03(workAddr, context + 0x94u, prefix) == 0)
        copy_string_9d749(workAddr, workAddr + prefix + 1u);
    copy_string_9d749(keyAddr, workAddr);
    for (index = 0u;; index++) {
        uint32_t entry;

        if ((int32_t)index >= EF_S32(context + 0x12cu))
            break;
        entry = EF_U32(EF_U32(context + 0x134u) + index * 4u);
        if (compare_strings_e5c03(keyAddr, EF_U32(EF_U32(context + 0x134u) + index * 4u),
                                  guest_fn_b1b8b(entry)) != 0)
            break;
    }
    if ((int32_t)index < EF_S32(context + 0x12cu)) {
        uint32_t len = guest_fn_b1b8b(EF_U32(EF_U32(context + 0x134u) + index * 4u));

        copy_string_9d749(keyAddr + 1u, keyAddr + len);
        key[0] = (char)(index + 0x30u);
    }
    dot = find_last_char(keyAddr, 0x2eu);
    if (dot != 0u) {
        for (index = 0u;; index++) {
            if ((int32_t)index >= EF_S32(context + 0x130u))
                break;
            if (guest_strcmp(dot, EF_U32(EF_U32(context + 0x138u) + index * 4u)) == 0)
                break;
        }
        if ((int32_t)index < EF_S32(context + 0x130u)) {
            *(uint8_t *)(uintptr_t)dot = (uint8_t)(index + 0x41u);
            *(uint8_t *)(uintptr_t)(dot + 1u) = 0u;
        } else {
            copy_string_9d749(dot, dot + 1u);
        }
    }
    hash = name_hash_ebfd0(keyAddr);
    return bsearch_entries_eb4dc((ef_addr32)(uintptr_t)&hash, EF_U32(context + 0x144u),
                                 EF_S32(context + 0x128u));
}

/* 0xb9c68, 426 bytes. Allocate COUNT (byte 0x1a55a0) 25-byte records, fill each from the parallel tables at 0x1a55a2/0x1a55d6/0x1a560a/0x1a5672 (and a time offset from add_time_offset_b54d0), mirror x/y into the 5-byte table at 0x1a4aa1, register the first record in the table entry, and return the records. */
ef_addr32 build_object_records_b9c68(uint8_t mode, uint8_t slot, uint8_t flag)
{
    uint32_t count = EF_U8(0x001a55a0u);
    ef_addr32 buf;
    int16_t i;
    uint32_t edx2;

    buf = alloc_zeroed_cc6c0(0u, 0u, 0u, 0u, count * 25u, 0x0018eb3cu, 0x19u);
    EF_U16(0x001a4b9cu) = (uint16_t)EF_U8(0x001a55a0u);
    for (i = 0; i < (int16_t)EF_U8(0x001a55a0u); i++) {
        ef_addr32 rec = buf + (uint32_t)((int32_t)i * 25);
        uint16_t x = EF_U16(0x001a55a2u + (uint32_t)i * 2u);
        uint16_t y = EF_U16(0x001a55d6u + (uint32_t)i * 2u);
        uint32_t v0 = 0u, v4 = 0u, v8 = 0u;

        EF_U32(rec) = x;
        EF_U32(rec + 4u) = y;
        EF_U16(0x001a4aa1u + (uint32_t)i * 5u) = x;
        EF_U16(0x001a4aa3u + (uint32_t)i * 5u) = (uint16_t)(0x190u - y);
        EF_U16(rec + 0xcu) = EF_U16(0x001a560au + (uint32_t)i * 4u);
        EF_U16(rec + 0x16u) = EF_U16(0x001a56f4u);
        if (slot == 3u) {
            EF_U8(rec + 0x18u) = 0u;
        } else if (mode == 1u || mode == 0u || mode == 0xbu) {
            uint8_t t = EF_U8(0x001a5672u + (uint32_t)i);

            EF_U8(rec + 0x18u) = (t == 8u || t == 3u) ? 0u : t;
        } else {
            EF_U8(rec + 0x18u) = EF_U8(0x001a5672u + (uint32_t)i);
        }
        add_time_offset_b54d0((ef_addr32)(uintptr_t)&v0, (ef_addr32)(uintptr_t)&v8,
                              (ef_addr32)(uintptr_t)&v4,
                              EF_S32(0x001a5718u + (uint32_t)flag * 2u) >> 16,
                              (uint32_t)(EF_S32(EF_U32(0x001a550au) + 0x46u) >> 16));
        EF_U16(rec + 0x10u) = (uint16_t)v0;
        EF_U16(rec + 0x14u) = 0u;
        EF_U16(rec + 0x12u) = (uint16_t)v8;
        EF_U16(rec + 0xeu) = EF_U16(0x001a5712u + (uint32_t)flag * 2u);
    }
    edx2 = EF_U32(0x001a57c9u);
    set_table_entry_b4e74((int16_t)(EF_S32(buf + 0xcu) >> 16), EF_U8(edx2 + 0xau), 1u, 0u,
                          (uint32_t)(EF_S32(edx2 + 0xeu) >> 16), EF_U32(0x001a550au));
    return buf;
}

/* 0x9a6fc, 440 bytes. Recount the 0x31-typed nodes of the list at 0x137988 into the three counters at 0x40cfd8/da/dc by flag group, then for the request (selector word at +0x10) find the target node's flag byte and group, bump the matching counters at 0x40d00e/0x40d016/0x40d01e and the result mode at 0x40d028; returns the target's byte (upper word 0xffff when none found, as in the original). */
uint32_t classify_target_flags_9a6fc(ef_addr32 request)
{
    uint32_t node;
    uint32_t result = 0xffffffffu;
    uint32_t flag;
    uint16_t selector;
    uint32_t index;

    guest_fn_b3fb0(0x0040cfd8u, 0u, 6u);
    for (node = EF_U32(0x00137988u); node != 0u; node = EF_U32(node + 4u)) {
        if (EF_U8(node + 0xbu) != 0x31u)
            continue;
        if ((uint16_t)group_flag_positive_cc22c(0u, EF_U8(node + 9u), 0x00137980u))
            EF_U16(0x0040cfd8u)++;
        else if ((uint16_t)group_flag_negative_cc1ac(0u, EF_U8(node + 9u), 0x00137980u))
            EF_U16(0x0040cfdcu)++;
        else if ((uint16_t)group_flag_zero_cc1ec(0u, EF_U8(node + 9u), 0x00137980u))
            EF_U16(0x0040cfdau)++;
    }
    selector = EF_U16(request + 0x10u);
    if (selector <= 2u) {
        ef_addr32 entry = list_nth(0x00137960u, (uint32_t)(EF_S32(request + 0x14u) >> 16));

        flag = EF_U8(entry + 0xbu);
        if ((EF_S32(request + 0x18u) >> 16) == -1)
            result = (result & 0xffff0000u) | EF_U8(EF_U32(entry + 0x13u));
    } else if (selector == 3u) {
        uint32_t walk = EF_U32(0x00137988u);

        if (EF_S16(request + 0x12u) > 0) {
            int16_t n = 0;

            do {
                n++;
                walk = EF_U32(walk + 4u);
            } while (n < EF_S16(request + 0x12u));
        }
        flag = EF_U8(walk + 9u);
        result = (result & 0xffff0000u) | EF_U8(walk + 0xbu);
    } else {
        return result;
    }
    if ((flag & 0xffu) == 0xffu)
        return result;
    index = (uint32_t)(EF_S32(request + 0xeu) >> 16);
    if ((uint16_t)group_flag_positive_cc22c(0u, flag & 0xffu, 0x00137980u)) {
        EF_U16(0x0040d028u) = 0u;
        EF_U16(0x0040d00eu + index * 2u)++;
    } else if ((uint16_t)group_flag_zero_cc1ec(0u, flag & 0xffu, 0x00137980u)) {
        EF_U16(0x0040d028u) = 1u;
        EF_U16(0x0040d016u + index * 2u)++;
    } else if ((uint16_t)group_flag_negative_cc1ac(0u, flag & 0xffu, 0x00137980u)) {
        EF_U16(0x0040d028u) = 2u;
        EF_U16(0x0040d01eu + index * 2u)++;
    }
    return result;
}

/* 0x6c1c, 461 bytes. Sum the 12 status bytes (+0x18e) of the object selected by 0x1f71c8, average them to a percentage (forced to 100 by 0x121e51), and when above 25% randomly clear or set bits in the damage-word table at 0x197244 on a countdown (0x121e52, reloaded to 50), setting the flicker byte 0x197254 from a random roll; when the byte at 0x121e50 is zero, clear the counters and refresh the label display instead. */
void update_damage_flags_6c1c(void)
{
    int32_t sum = 0;
    int16_t pct;
    int16_t step;
    int16_t i;
    ef_addr32 object = lookup_filtered_indexed_object((int16_t)(EF_S32(0x001f71c8u) >> 16));

    if (EF_U8(0x00121e50u) != 0u) {
        if (object != 0u) {
            for (i = 0; i < 12; i++)
                sum += EF_U8(object + 0x18eu + (uint32_t)i);
        }
    } else {
        EF_U16(0x00197244u) = 0u;
        EF_U16(0x00197246u) = 0u;
        EF_U16(0x00197248u) = 0u;
        EF_U8(0x00121e4eu) = 0u;
        EF_U8(0x00197254u) = 0u;
        if (EF_U8(0x00121e4fu) != 0u) {
            refresh_all_event_slots();
            draw_label_entries_150d4();
            EF_U8(0x00121e4fu) = 0u;
        }
    }
    pct = (int16_t)((int16_t)sum / 5);
    if (EF_U8(0x00121e51u) != 0u)
        pct = 100;
    if (pct <= 0x19)
        return;
    step = pct < 0x4b ? 2 : 8;
    EF_U16(0x00121e52u) = (uint16_t)(EF_U16(0x00121e52u) - EF_U16(0x00127d92u));
    if ((int16_t)EF_U16(0x00121e52u) <= 0) {
        uint32_t slot = (uint16_t)step_generator_a45b3() % 4u;

        if ((int16_t)slot < 3) {
            uint32_t offset;

            if ((uint16_t)step_generator_a45b3() % (uint32_t)(int32_t)step == 0u) {
                uint32_t bit = (uint16_t)step_generator_a45b3() % 4u;

                offset = slot * 2u;
                EF_U16(0x00197244u + offset) = (uint16_t)((uint32_t)(EF_S32(0x00197242u + offset) >> 16) & ~(1u << bit));
            } else {
                offset = slot * 2u;
                if ((uint16_t)step_generator_a45b3() % 4u == 0u) {
                    uint32_t bit = (uint16_t)step_generator_a45b3() % 4u;

                    EF_U16(0x00197244u + offset) = (uint16_t)((uint32_t)(EF_S32(0x00197242u + offset) >> 16) | (1u << bit));
                }
            }
        }
        EF_U16(0x00121e52u) = 0x32u;
    }
    if ((uint16_t)step_generator_a45b3() % (uint32_t)(10 - step) == 0u)
        EF_U8(0x00197254u) = 1u;
    else
        EF_U8(0x00197254u) = 0u;
}

/* 0x31b40, 462 bytes. Rebuild the view-angle results at 0x19e5d3/0x19e5d5/0x19e5d7 from the camera state at 0x19e55f..0x19e5b5: rotate the offset vector by the camera angles, take the direction angles to the target (or the explicit target at 0x19e572), and for the tracking mode (0x19e558 != 2) or the 2 mode use a scratch scene built from the angle words to derive the angle results. */
void update_view_angles_31b40(void)
{
    uint32_t frame[0x11];
    uint32_t *a = &frame[9];
    uint32_t *b = &frame[12];
    uint16_t *out = (uint16_t *)&frame[15];
    ef_addr32 scene = (ef_addr32)(uintptr_t)frame;
    ef_addr32 aAddr = (ef_addr32)(uintptr_t)a;
    ef_addr32 bAddr = (ef_addr32)(uintptr_t)b;
    ef_addr32 outAddr = (ef_addr32)(uintptr_t)out;

    init_bounds_d0b68(scene);
    update_scene_angle_first(scene, EF_U16(0x0019e56eu));
    update_scene_angle_second(scene, EF_U16(0x0019e56cu));
    update_scene_angle_third(scene, EF_U16(0x0019e570u));
    rotate_vector_by_angles_32414(0x0019e56cu, 0x0019e58bu, aAddr);
    a[0] += EF_U32(0x0019e560u);
    a[1] += EF_U32(0x0019e564u);
    a[2] += EF_U32(0x0019e568u);
    if (EF_U8(0x0019e58au) == 0u)
        direction_angles_dc91c(outAddr, aAddr, 0x0019e5b5u);
    else
        direction_angles_dc91c(outAddr, 0x0019e572u, 0x0019e5b5u);
    EF_U16(0x0019e5d3u) = out[0];
    EF_U16(0x0019e5d5u) = out[1];
    if (EF_U8(0x0019e55fu) == 0u && EF_U8(0x0019e558u) != 2u) {
        a[0] = EF_U32(0x0019e58bu);
        a[1] = EF_U32(0x0019e58fu);
        a[2] = EF_U32(0x0019e593u);
        b[0] = EF_U32(0x0019e57eu);
        b[1] = EF_U32(0x0019e582u);
        b[2] = EF_U32(0x0019e586u);
        direction_angles_dc91c(outAddr, aAddr, bAddr);
        update_scene_angle_first(scene, out[1]);
        update_scene_angle_second(scene, out[0]);
        EF_U16(0x0019e5d7u) = (uint16_t)angle_d13ec(scene);
    }
    if (EF_U8(0x0019e558u) == 2u) {
        update_scene_angle_first(scene, EF_U16(0x0019e5a7u));
        update_scene_angle_second(scene, EF_U16(0x0019e5a5u));
        if (EF_U8(0x0019e557u) == 0u) {
            EF_U16(0x0019e5d3u) = (uint16_t)angle_d13ac(scene);
            EF_U16(0x0019e5d5u) = (uint16_t)forward_pair_d13dc(scene);
            if (EF_U16(0x0019e5a5u) != 0u)
                EF_U8(0x001a6d1fu) = 1u;
            if (EF_U16(0x0019e5a7u) != 0u)
                EF_U8(0x001a6d1fu) = 1u;
        }
        EF_U16(0x0019e5d7u) = (uint16_t)angle_d13ec(scene);
    }
}

/* 0x324b0, 488 bytes. Rotate the (to - from) offset (x,y scaled >>8) into the frame given by the angle triple ANGLES, take its direction angles into OUT (two words) and, when the limit flags at 0x19e5a9/0x19e5af are 1, clamp each angle into its [min,max] window at 0x19e5ab/ad and 0x19e5b1/b3 by snapping to the nearer bound. */
void aim_angles_to_point_324b0(ef_addr32 from, ef_addr32 angles, ef_addr32 to, ef_addr32 out)
{
    uint32_t m[9];
    int32_t v[3];
    int32_t r[3];
    uint32_t zero[3] = {0u, 0u, 0u};
    uint16_t ang[2];
    ef_addr32 scene = (ef_addr32)(uintptr_t)m;
    int32_t dz;
    uint32_t w;
    uint32_t hi;
    uint32_t lo;

    v[0] = (EF_S32(to) - EF_S32(from)) >> 8;
    v[1] = (EF_S32(to + 4u) - EF_S32(from + 4u)) >> 8;
    dz = EF_S32(to + 8u) - EF_S32(from + 8u);
    init_bounds_d0b68(scene);
    update_scene_angle_first(scene, EF_U16(angles + 2u));
    update_scene_angle_second(scene, EF_U16(angles));
    v[2] = dz >> 8;
    update_scene_angle_third(scene, EF_U16(angles + 4u));
    r[0] = (int32_t)((uint32_t)(v[1] * (int32_t)m[3] + v[0] * (int32_t)m[0] + v[2] * (int32_t)m[6]) ) >> 15;
    r[1] = (int32_t)((uint32_t)(v[0] * (int32_t)m[1] + v[1] * (int32_t)m[4] + v[2] * (int32_t)m[7])) >> 15;
    r[2] = (int32_t)((uint32_t)(v[0] * (int32_t)m[2] + v[1] * (int32_t)m[5] + v[2] * (int32_t)m[8])) >> 15;
    direction_angles_dc91c((ef_addr32)(uintptr_t)ang, (ef_addr32)(uintptr_t)r,
                           (ef_addr32)(uintptr_t)zero);
    w = ang[0];
    hi = EF_U16(0x0019e5abu);
    lo = EF_U16(0x0019e5adu);
    if (EF_U16(0x0019e5a9u) == 1u && w < hi && w > lo) {
        int32_t d1 = (int32_t)abs_a28f2((int32_t)(hi - w));
        int32_t d2 = (int32_t)abs_a28f2((int32_t)(lo - w));

        ang[0] = (d1 > d2) ? (uint16_t)lo : (uint16_t)hi;
    }
    w = ang[1];
    hi = EF_U16(0x0019e5b1u);
    lo = EF_U16(0x0019e5b3u);
    if (EF_U16(0x0019e5afu) == 1u && w < hi && w > lo) {
        int32_t d1 = (int32_t)abs_a28f2((int32_t)(hi - w));
        int32_t d2 = (int32_t)abs_a28f2((int32_t)(lo - w));

        ang[1] = (d1 > d2) ? (uint16_t)lo : (uint16_t)hi;
    }
    EF_U16(out) = ang[0];
    EF_U16(out + 2u) = ang[1];
}

/* 0x3287c, 512 bytes. Apply the direction key bits at 0x2144aa to the camera: in mode 1 (byte 0x19e557) bits 0x10/0x20 change the zoom dword at 0x19e597 (clamped to 0x19e59b..0x19e59f) and the arrow bits step the two angle words at 0x19e5a5/0x19e5a7 by 500; otherwise only the angle words are stepped and then snapped into their limit windows (0x19e5ab/ad, 0x19e5b1/b3) when the limit flags are 1. */
void apply_camera_keys_3287c(void)
{
    uint32_t keys;
    uint32_t w;
    uint32_t hi;
    uint32_t lo;

    if (EF_U8(0x0019e557u) == 1u) {
        if (EF_U8(0x002144aau) & 0x10u)
            EF_U32(0x0019e597u) -= (uint32_t)(EF_S32(0x0019e5a1u) >> 16);
        if (EF_U8(0x002144aau) & 0x20u)
            EF_U32(0x0019e597u) += (uint32_t)(EF_S32(0x0019e5a1u) >> 16);
        if (EF_U32(0x0019e597u) < EF_U32(0x0019e59bu))
            EF_U32(0x0019e597u) = EF_U32(0x0019e59bu);
        if (EF_U32(0x0019e597u) > EF_U32(0x0019e59fu))
            EF_U32(0x0019e597u) = EF_U32(0x0019e59fu);
        if (EF_U8(0x002144aau) & 0x08u)
            EF_U16(0x0019e5a5u) = (uint16_t)(EF_U16(0x0019e5a5u) - 0x1f4u);
        if (EF_U8(0x002144aau) & 0x04u)
            EF_U16(0x0019e5a5u) = (uint16_t)(EF_U16(0x0019e5a5u) + 0x1f4u);
        if (EF_U8(0x002144aau) & 0x01u)
            EF_U16(0x0019e5a7u) = (uint16_t)(EF_U16(0x0019e5a7u) - 0x1f4u);
        if (EF_U8(0x002144aau) & 0x02u)
            EF_U16(0x0019e5a7u) = (uint16_t)(EF_U16(0x0019e5a7u) + 0x1f4u);
        return;
    }
    keys = EF_U8(0x002144aau);
    if (keys & 0x08u)
        EF_U16(0x0019e5a5u) = (uint16_t)(EF_U16(0x0019e5a5u) - 0x1f4u);
    if (EF_U8(0x002144aau) & 0x04u)
        EF_U16(0x0019e5a5u) = (uint16_t)(EF_U16(0x0019e5a5u) + 0x1f4u);
    if (EF_U8(0x002144aau) & 0x01u)
        EF_U16(0x0019e5a7u) = (uint16_t)(EF_U16(0x0019e5a7u) - 0x1f4u);
    if (EF_U8(0x002144aau) & 0x02u)
        EF_U16(0x0019e5a7u) = (uint16_t)(EF_U16(0x0019e5a7u) + 0x1f4u);
    w = EF_U16(0x0019e5a5u);
    hi = EF_U16(0x0019e5abu);
    lo = EF_U16(0x0019e5adu);
    if (EF_U16(0x0019e5a9u) == 1u && w < hi && w > lo) {
        int32_t d1 = (int32_t)abs_a28f2((int32_t)(hi - w));
        int32_t d2 = (int32_t)abs_a28f2((int32_t)(lo - w));

        EF_U16(0x0019e5a5u) = (d1 > d2) ? (uint16_t)lo : (uint16_t)hi;
    }
    w = EF_U16(0x0019e5a7u);
    hi = EF_U16(0x0019e5b1u);
    lo = EF_U16(0x0019e5b3u);
    if (EF_U16(0x0019e5afu) == 1u && w < hi && w > lo) {
        int32_t d1 = (int32_t)abs_a28f2((int32_t)(hi - w));
        int32_t d2 = (int32_t)abs_a28f2((int32_t)(lo - w));

        EF_U16(0x0019e5a7u) = (d1 > d2) ? (uint16_t)lo : (uint16_t)hi;
    }
}

/* 0xa3dc, 477 bytes. Write a text report of the selected group (index 0x125833) and member (byte 0x125837) of the list at 0x137960 to the stream opened by 0xbc36a: group number, position words, a name looked up from the object type byte, member number, the member's name from table 0x13315c (or a placeholder), its first byte, whether any of its records has state 5, and whether the group's word at +0x17 is set, then close the stream. */
void dump_selected_group_a3dc(void)
{
    int16_t member = (int16_t)EF_U8(0x00125837u);
    uint32_t stream = run_if_available_bc36a(0x00180660u, 0x0018065cu);
    uint16_t found = 0u;
    ef_addr32 node;
    ef_addr32 rec;
    ef_addr32 data;
    ef_addr32 name;
    int16_t i;

    if (stream == 0u)
        return;
    node = list_nth(0x00137960u, (uint32_t)(EF_S32(0x00125833u) >> 16));
    if (node == 0u)
        return;
    guest_fn_bc400(stream, 0x0018066cu, (int32_t)(int16_t)EF_U16(0x00125835u) + 1);
    rec = EF_U32(node + 0x13u) + (uint32_t)((int32_t)member * 124);
    data = EF_U32(rec + 0x20u);
    guest_fn_bc400(stream, 0x00180690u, (int32_t)EF_S16(data + 0x10u), (int32_t)EF_S16(data + 0x12u),
                   (int32_t)EF_S16(data + 0x14u));
    guest_fn_bc400(stream, 0x001806acu, EF_U32(0x00132ee4u + (uint32_t)EF_U8(node + 0xau) * 4u));
    guest_fn_bc400(stream, 0x001806b8u, (uint32_t)EF_U8(0x00125837u) + 1u);
    rec = EF_U32(node + 0x13u) + (uint32_t)((int32_t)member * 124);
    name = EF_U32(rec + 0x51u);
    if (name != 0u)
        guest_fn_bc400(stream, 0x001806c4u, EF_U32(0x0013315cu + (uint32_t)EF_U8(name) * 4u));
    else
        guest_fn_bc400(stream, 0x001806d0u);
    guest_fn_bc400(stream, 0x001806f8u,
                   (uint32_t)EF_U8(EF_U32(node + 0x13u) + (uint32_t)((int32_t)member * 124) + 1u));
    for (i = 0;; i++) {
        rec = EF_U32(node + 0x13u) + (uint32_t)((int32_t)member * 124);
        if (i >= (int16_t)EF_U16(rec + 0x1cu))
            break;
        if (EF_U8(EF_U32(rec + 0x20u) + (uint32_t)((int32_t)i * 25) + 0x18u) == 5u)
            found++;
    }
    guest_fn_bc400(stream, 0x00180708u);
    guest_fn_bc400(stream, found != 0u ? 0x00180714u : 0x0018071cu);
    guest_fn_bc400(stream, 0x00180720u);
    guest_fn_bc400(stream, EF_U16(node + 0x17u) != 0u ? 0x00180714u : 0x0018071cu);
    guest_fn_bc421_w(stream);
}

/* 0x82e40, 483 bytes. Load the small text file named by the pointer at 0x12b36c (1..0x400 bytes), blank its 0x1a bytes, rewrite eleven keyword lines in turn (find the keyword fragment, compact it and append the current status value written by the matching 0x827xx/0x82bxx writer) and save the buffer back to the same file. */
void rewrite_status_file_82e40(void)
{
    char buffer[0x401];
    ef_addr32 buf = (ef_addr32)(uintptr_t)buffer;
    uint32_t name = EF_U32(0x0012b36cu);
    uint32_t size = read_file_payload_size_b3b90(name);
    int16_t i;

    if (size == 0u || size > 0x400u)
        return;
    load_file_unpacked_b3c04(name, buf);
    buffer[size] = 0;
    for (i = 0; (int32_t)i < (int32_t)size; i++) {
        if ((uint8_t)buffer[i] == 0x1au)
            buffer[i] = 0;
    }
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188a90u));
    write_status_byte_15bdd0(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188ad8u));
    format_state_bytes_82b6c(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188aa4u));
    write_status_byte_15b7e4(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188ae0u));
    write_ui_status_float(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188aecu));
    write_status_high_word(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188aacu));
    write_status_byte_1a5c0d(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188a9cu));
    write_status_byte_1fcb30(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188ab4u));
    write_ui_status_flags_a(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188b00u));
    write_status_queried_value(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188abcu));
    write_ui_status_flags_b(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188b14u));
    write_status_dword_122dc4(buf + guest_string_length(buf));
    write_whole_file_b3d00(EF_U32(0x0012b36cu), buf, guest_string_length(buf));
}

/* 0xb3648, 486 bytes. Print every node of the list whose head pointer is stored at LIST_HEAD to STREAM: a header with TAG and the list count, then per node its type byte, three signed byte arrays (+0xc/+0x10/+0x14, one entry per count), the 24 words of its block at +0x18 and a second line with 9 words at +0x1c.. and the bytes at +0x2e..+0x30. */
void dump_node_list_b3648(ef_addr32 list_head, uint32_t stream, uint32_t tag)
{
    ef_addr32 node = EF_U32(list_head);
    int16_t count = (int16_t)list_count(list_head);
    int16_t i;

    guest_fn_b1bb0(stream, 0x0018e678u, tag);
    guest_fn_b1bb0(stream, 0x0018e680u, (int32_t)count);
    while (node != 0u) {
        ef_addr32 extra;

        guest_fn_b1bb0(stream, 0x0018e698u, (uint32_t)EF_U8(node + 0xbu));
        if (count > 0) {
            i = 0;
            do {
                guest_fn_b1bb0(stream, 0x0018e4b0u, (int32_t)(int8_t)EF_U8(EF_U32(node + 0xcu) + (uint32_t)i));
                i++;
            } while (i < count);
        }
        if (count > 0) {
            i = 0;
            do {
                guest_fn_b1bb0(stream, 0x0018e4b0u, (int32_t)(int8_t)EF_U8(EF_U32(node + 0x10u) + (uint32_t)i));
                i++;
            } while (i < count);
        }
        if (count > 0) {
            i = 0;
            do {
                guest_fn_b1bb0(stream, 0x0018e4b0u, (int32_t)(int8_t)EF_U8(EF_U32(node + 0x14u) + (uint32_t)i));
                i++;
            } while (i < count);
        }
        extra = EF_U32(node + 0x18u);
        guest_fn_b1bb0(stream, 0x0018e69cu,
                           (int32_t)EF_S16(extra + 0x0u),
                           (int32_t)EF_S16(extra + 0x2u),
                           (int32_t)EF_S16(extra + 0x4u),
                           (int32_t)EF_S16(extra + 0x6u),
                           (int32_t)EF_S16(extra + 0x8u),
                           (int32_t)EF_S16(extra + 0xau),
                           (int32_t)EF_S16(extra + 0xcu),
                           (int32_t)EF_S16(extra + 0xeu),
                           (int32_t)EF_S16(extra + 0x10u),
                           (int32_t)EF_S16(extra + 0x12u),
                           (int32_t)EF_S16(extra + 0x14u),
                           (int32_t)EF_S16(extra + 0x16u),
                           (int32_t)EF_S16(extra + 0x18u),
                           (int32_t)EF_S16(extra + 0x1au),
                           (int32_t)EF_S16(extra + 0x1cu),
                           (int32_t)EF_S16(extra + 0x1eu),
                           (int32_t)EF_S16(extra + 0x20u),
                           (int32_t)EF_S16(extra + 0x22u),
                           (int32_t)EF_S16(extra + 0x24u),
                           (int32_t)EF_S16(extra + 0x26u),
                           (int32_t)EF_S16(extra + 0x28u),
                           (int32_t)EF_S16(extra + 0x2au),
                           (int32_t)EF_S16(extra + 0x2cu),
                           (int32_t)EF_S16(extra + 0x2eu));
        guest_fn_b1bb0(stream, 0x0018e700u,
                       (int32_t)EF_S16(node + 0x1cu), (int32_t)EF_S16(node + 0x1eu),
                       (int32_t)EF_S16(node + 0x20u), (int32_t)EF_S16(node + 0x22u),
                       (int32_t)EF_S16(node + 0x24u), (int32_t)EF_S16(node + 0x26u),
                       (int32_t)EF_S16(node + 0x28u), (int32_t)EF_S16(node + 0x2au),
                       (int32_t)EF_S16(node + 0x2cu), (uint32_t)EF_U8(node + 0x2eu),
                       (uint32_t)EF_U8(node + 0x2fu), (uint32_t)EF_U8(node + 0x30u));
        node = EF_U32(node + 4u);
    }
}

/* 0x78f3c, 489 bytes. Draw the info panel for the unit selected by the key in 0x12db9e ('-', '>' or '+'): pick its object id (the '+' key goes through the current group record table at 0x19bb58 and byte 0x12dc34), then print its uppercase name, a status word, its number plus group name and, when flag 0x127da9 is set, the labels of the eight table entries whose id matches the object's. */
void draw_unit_info_78f3c(void)
{
    char text[0x64];
    char name[0x34];
    ef_addr32 buf = (ef_addr32)(uintptr_t)text;
    ef_addr32 object;
    ef_addr32 rec;
    uint32_t id;
    uint8_t key = EF_U8(0x0012db9eu);
    uint8_t i;
    uint16_t state;
    uint32_t fmt;

    if (key == 0x2du) {
        id = EF_U32(0x001a8234u);
    } else if (key == 0x3eu) {
        id = EF_U32(0x001a823au);
    } else if (key == 0x2bu) {
        if (lookup_filtered_indexed_object((int16_t)(EF_S32(0x001f71c8u) >> 16)) == 0u)
            return;
        id = EF_U32(EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(0x0019bb5cu) >> 16) * 0xcau + 0x4eu +
                    (uint32_t)EF_U8(0x0012dc34u) * 2u);
    } else {
        return;
    }
    object = lookup_filtered_indexed_object((int16_t)((int32_t)id >> 16));
    if (object == 0u)
        return;
    build_unit_name_7548c(buf, (uint32_t)(EF_S32(object + 0x14u) >> 16));
    uppercase_string(buf);
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(0xfu, 0xa, 0x14, buf);
    rec = EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(object + 0x26u) >> 16) * 0xcau;
    state = EF_U16(rec + 0x15u);
    if (state == 1u || state == 4u)
        fmt = 0x0012f4c8u;
    else if (state == 0u)
        fmt = 0x0012f4cdu;
    else
        fmt = 0x0012f4d7u;
    guest_sprintf_a4342(buf, fmt);
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(0xfu, 0xa, 0x1e, buf);
    rec = EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(object + 0x26u) >> 16) * 0xcau;
    guest_sprintf_a4342(buf, 0x0012f4e0u, (EF_S32(rec + 0x1au) >> 16) + 1);
    rec = EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(object + 0x26u) >> 16) * 0xcau;
    copy_string_9d749((ef_addr32)(uintptr_t)name, 0x00125708u + (uint32_t)EF_U8(rec + 0x1bu) * 20u);
    strcat_9ca57(buf, (ef_addr32)(uintptr_t)name);
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(0xfu, 0xa, 0x28, buf);
    if (EF_U8(0x00127da9u) == 0u)
        return;
    for (i = 0u; i < 8u; i++) {
        if (EF_U16(object + 0x16u) == EF_U16(0x001a5c30u + (uint32_t)i * 27u)) {
            EF_U8(0x00127b1au) = 1u;
            draw_shadowed_text_48c90(0xcu, 0xa, 0x32, 0x001a62f0u + (uint32_t)i * 17u);
        }
    }
}

/* 0x275e0, 502 bytes. Draw one row of the statistics table at (X,Y) in COLOUR: when ROW is the selected row (stack word), draw the highlighted frame text and then either five dotted placeholders (when the byte at 0x19b438+row*0x73 is set) or the five formatted dwords at 0x19b48d..0x19b4a7 right-aligned at x 0x55; otherwise draw the plain row marker. */
void draw_stat_row_275e0(int16_t x, int16_t y, uint32_t colour, int16_t row, uint16_t selected_row)
{
    char text[0x20];
    ef_addr32 buf = (ef_addr32)(uintptr_t)text;
    uint32_t base;
    uint32_t i;
    uint32_t py;

    if ((uint16_t)row != selected_row) {
        guest_fn_dd370(0x31u, (uint32_t)(int32_t)x, (uint32_t)(int32_t)y, colour);
        return;
    }
    guest_fn_dd370(0u, (uint32_t)(int32_t)(int16_t)(x + 1), (uint32_t)(int32_t)(int16_t)(y + 1), colour);
    guest_fn_dd370(0x9cu, (uint32_t)(int32_t)x, (uint32_t)(int32_t)y, colour);
    base = (uint32_t)((int32_t)row * 0x73);
    if (EF_U8(0x0019b438u + base) != 0u) {
        py = 0x93u;
        for (i = 0u; (int16_t)i < 5; i++) {
            guest_fn_dd370(0x37u, (uint32_t)(int32_t)(int16_t)(0x55u - text_width_dd2f4(0x00182174u)),
                           (uint32_t)(int32_t)(int16_t)py, 0x00182174u);
            py += 8u;
        }
        return;
    }
    guest_sprintf_a4342(buf, 0x00182178u, EF_U32(0x0019b48du + base));
    guest_fn_dd370(0x37u, (uint32_t)(int32_t)(int16_t)(0x55u - text_width_dd2f4(buf)), 0x93u, buf);
    guest_sprintf_a4342(buf, 0x00182178u, EF_U32(0x0019b491u + base));
    guest_fn_dd370(0x37u, (uint32_t)(int32_t)(int16_t)(0x55u - text_width_dd2f4(buf)), 0x9bu, buf);
    guest_sprintf_a4342(buf, 0x00182178u, EF_U32(0x0019b495u + base));
    guest_fn_dd370(0x37u, (uint32_t)(int32_t)(int16_t)(0x55u - text_width_dd2f4(buf)), 0xa3u, buf);
    guest_sprintf_a4342(buf, 0x00182178u, EF_U32(0x0019b4a3u + base));
    guest_fn_dd370(0x37u, (uint32_t)(int32_t)(int16_t)(0x55u - text_width_dd2f4(buf)), 0xabu, buf);
    guest_sprintf_a4342(buf, 0x00182178u, EF_U32(0x0019b4a7u + base));
    guest_fn_dd370(0x37u, (uint32_t)(int32_t)(int16_t)(0x55u - text_width_dd2f4(buf)), 0xb3u, buf);
}

/* 0x9457c, 583 bytes. Update the scene camera for OBJECT: when OBJECT or ENABLED is zero only reset the pending flag (0x21449c = 1) and copy the mode byte 0x214486 to 0x12db9e; otherwise derive the view angles from the navigation component (0x92254), apply the zoom keys (0x2144aa bits 0x10/0x20 on 0x12dba0), scale the basis axes at 0x21440c/18/24 by the zoom, add the object position (>>8 and <<8 forms) to the camera, fall back to the remaining scene metric when the threshold check fails, and push the angles, scene pair and input triplet to the scene helpers. */
void update_scene_camera_9457c(ef_addr32 object, uint32_t enabled)
{
    uint16_t a;
    uint16_t b;
    int32_t zoom;
    uint16_t negPitch;
    uint16_t negYaw;

    if (object == 0u || enabled == 0u) {
        EF_U16(0x0021449cu) = 1u;
        EF_U8(0x0012db9eu) = EF_U8(0x00214486u);
        return;
    }
    init_bounds_d0b68(0x00214404u);
    EF_U16(0x002143e4u) = 0u;
    EF_U16(0x002143e6u) = 0u;
    prepare_navigation_component(object, enabled);
    a = EF_U16(0x002143e4u);
    EF_U16(0x002144b0u) = 0u;
    EF_U16(0x002144b6u) = a;
    b = EF_U16(0x002143e6u);
    EF_U16(0x002144b4u) = b;
    negPitch = (uint16_t)(0u - b);
    negYaw = (uint16_t)(0u - a);
    EF_U16(0x002144b4u) = negPitch;
    EF_U16(0x002144b6u) = negYaw;
    update_scene_angle_first(0x00214404u, negPitch);
    update_scene_angle_second(0x00214404u, EF_U16(0x002144b6u));
    if (EF_U8(0x002144aau) & 0x10u) {
        int16_t z = (int16_t)EF_U16(0x0012dba0u);

        if (z > (int16_t)EF_U16(0x0021448au))
            EF_U16(0x0012dba0u) = (uint16_t)(z - 1);
    }
    if (EF_U8(0x002144aau) & 0x20u)
        EF_U16(0x0012dba0u) = (uint16_t)(EF_U16(0x0012dba0u) + 1u);
    zoom = -(EF_S32(0x0012db9eu) >> 16);
    EF_U32(0x0012dba8u) = (uint32_t)(EF_S32(0x0021440cu) * zoom);
    EF_U32(0x0012dbacu) = (uint32_t)(EF_S32(0x00214418u) * zoom);
    EF_U32(0x0012dbb0u) = (uint32_t)(EF_S32(0x00214424u) * zoom);
    EF_U32(0x0021446cu) = EF_U32(object);
    EF_U32(0x00214470u) = EF_U32(object + 4u);
    EF_U32(0x00214474u) = EF_U32(object + 8u);
    if (object == EF_U32(0x00214458u))
        apply_rotated_offset_95bf4();
    EF_U32(0x0021444cu) = (uint32_t)((EF_S32(0x0021446cu) >> 8) + (EF_S32(0x0012dba8u) >> 16));
    EF_U32(0x00214448u) = (uint32_t)((EF_S32(0x00214470u) >> 8) + (EF_S32(0x0012dbacu) >> 16));
    EF_U32(0x00214450u) = (uint32_t)((EF_S32(0x00214474u) >> 8) + (EF_S32(0x0012dbb0u) >> 16));
    EF_U32(0x00214370u) = (EF_U32(0x0021446cu) << 8) + EF_U32(0x0012dba8u);
    EF_U32(0x0021436cu) = (EF_U32(0x00214470u) << 8) + EF_U32(0x0012dbacu);
    EF_U32(0x00214374u) = (EF_U32(0x00214474u) << 8) + EF_U32(0x0012dbb0u);
    if (scene_metric_reaches_threshold((int32_t)EF_U32(0x0021444cu), (int32_t)EF_U32(0x00214448u)) == 0u) {
        int32_t metric = scene_metric_remaining();

        EF_U32(0x00214448u) = (uint32_t)metric;
        EF_U32(0x0021436cu) = (uint32_t)metric << 16;
    }
    request_scene_pair((int16_t)(0u - EF_U16(0x002144b6u)), (int16_t)(0u - EF_U16(0x002144b0u)));
    set_scene_angles((int16_t)(0u - EF_U16(0x002144b6u)), (int16_t)(0u - EF_U16(0x002144b4u)),
                     (int16_t)(0u - EF_U16(0x002144b0u)));
    set_scene_input_triplet((int32_t)EF_U32(0x00214370u), (int32_t)EF_U32(0x0021436cu),
                            (int32_t)EF_U32(0x00214374u));
}

/* 0xc193c, 591 bytes. Walk the node list whose head pointer is at LISTHEAD and collect up to COLLECTION[1] unclaimed nodes whose keys at +0x3b/+0x3f equal KEYA/KEYB: allocate a word table (COLLECTION+0x78) and 22-byte record table (COLLECTION+0x69), claim each match (+9 = 1, +0x17 incremented, owner block +9 incremented), copy its owner block fields into the record and store its +0x13 id in the table. Returns the number of nodes collected. */
int32_t collect_list_entries_c193c(ef_addr32 collection, ef_addr32 listHead, uint32_t keyA, uint32_t keyB)
{
    uint32_t n = EF_U8(collection + 1u);
    ef_addr32 node;
    int32_t count;

    node = EF_U32(listHead);
    EF_U32(collection + 0x78u) = guest_fn_cc6c0_x(n * 2u, 0x0018ed98u);
    EF_U32(collection + 0x69u) = guest_fn_cc6c0_x(n * 22u, 0x0018ed98u);
    count = 0;
    while (node != 0u) {
        if ((int16_t)count >= (int16_t)n)
            break;
        if (EF_U8(node + 0xbu) == 0u && EF_U16(node + 9u) == 0u) {
            uint8_t flag = EF_U8(node + 0xdu);

            EF_U16(node + 9u) = 1u;
            if (flag == 0u && keyA == EF_U32(node + 0x3bu) && keyB == EF_U32(node + 0x3fu)) {
                int32_t index = (int16_t)count;
                ef_addr32 idTable = EF_U32(collection + 0x78u);
                ef_addr32 rec;
                ef_addr32 src;

                EF_U16(idTable + (uint32_t)index * 2u) = EF_U16(node + 0x13u);
                EF_U16(node + 0x17u) = (uint16_t)(EF_U16(node + 0x17u) + 1u);
                EF_U8(EF_U32(node + 0x4du) + 9u) = (uint8_t)(EF_U8(EF_U32(node + 0x4du) + 9u) + 1u);
                {
                    uint32_t block = alloc_checked_cc744(EF_U32(EF_U32(node + 0x4du)));

                    rec = EF_U32(collection + 0x69u) + (uint32_t)index * 0x16u;
                    EF_U32(rec) = block;
                }
                src = EF_U32(node + 0x4du);
                EF_U8(rec + 4u) = EF_U8(src + 4u);
                EF_U8(rec + 5u) = EF_U8(src + 5u);
                EF_U8(rec + 6u) = EF_U8(src + 6u);
                EF_U8(rec + 7u) = EF_U8(src + 7u);
                EF_U8(rec + 8u) = EF_U8(src + 8u);
                EF_U16(rec + 0xau) = EF_U16(src + 0xau);
                EF_U8(rec + 9u) = EF_U8(src + 9u);
                EF_U16(rec + 0xcu) = EF_U16(src + 0xcu);
                EF_U16(rec + 0xeu) = EF_U16(src + 0xeu);
                EF_U16(rec + 0x10u) = EF_U16(src + 0x10u);
                EF_U16(rec + 0x12u) = EF_U16(src + 0x12u);
                EF_U16(rec + 0x14u) = EF_U16(src + 0x14u);
                count++;
                EF_U16(EF_U32(collection + 0x78u) + (uint32_t)index * 2u) = EF_U16(node + 0x13u);
            }
        }
        node = EF_U32(node + 4u);
    }
    return count;
}

/* 0x33b4, 689 bytes. For the group record GROUP (0xca bytes each at the table pointed to by 0x19bb58) pick a target object and publish it in 0x197061 (object), 0x19705f (id word) and 0x197083 (slot): first the first member whose object lacks flags 0x08/0x04 (the last looked-up object is kept even when the scan runs out); then, when the record has a reference block at +0xb5, the member object nearest to the reference point; finally, for type 2 records, the member whose own target (object +0x144) is nearest to it. Distance is the max of the three absolute axis deltas. */
void find_nearest_object_33b4(int16_t group)
{
    int32_t best1 = -1;
    int32_t best2 = -1;
    ef_addr32 obj = 0u;
    ef_addr32 rec;
    ef_addr32 ref;
    int32_t j = 0;
    int32_t k;
    uint32_t d;

#define EF_GROUP_REC() (EF_U32(0x0019bb58u) + (uint32_t)((int32_t)group * 0xca))
    while ((int16_t)j < (int16_t)EF_U8(EF_GROUP_REC() + 0x4fu)) {
        rec = EF_GROUP_REC();
        obj = lookup_filtered_indexed_object((int16_t)EF_U16(rec + 0x50u + (uint32_t)((int32_t)(int16_t)j * 2)));
        if (obj != 0u && (EF_U8(obj + 0x24u) & 0x08u) == 0u && (EF_U8(obj + 0x24u) & 0x04u) == 0u)
            break;
        j++;
    }
    if (obj == 0u)
        return;
    rec = EF_GROUP_REC();
    EF_U32(0x00197061u) = obj;
    EF_U16(0x0019705fu) = EF_U16(rec + 0x50u + (uint32_t)((int32_t)(int16_t)j * 2));
    ref = EF_U32(rec + 0xb5u);
    EF_U16(0x00197083u) = (uint16_t)j;
    if (ref != 0u) {
        int32_t refA = (int32_t)(((uint32_t)(EF_S32(ref) >> 16) << 19) +
                                 (((uint32_t)(EF_S32(ref + 0x18u) >> 16) << 8) + 0x40000u));
        int32_t refB = (int32_t)((uint32_t)(-(EF_S32(ref + 0x1au) >> 16)) << 8);
        int32_t refC = (int32_t)(((uint32_t)(int32_t)(int16_t)EF_U16(ref) << 19) +
                                 (((uint32_t)(EF_S32(ref + 0x16u) >> 16) << 8) + 0x40000u));

        k = 0;
        while ((int16_t)k < (int16_t)EF_U8(EF_GROUP_REC() + 0x4fu)) {
            rec = EF_GROUP_REC();
            obj = lookup_filtered_indexed_object((int16_t)EF_U16(rec + 0x50u + (uint32_t)((int32_t)(int16_t)k * 2)));
            if (obj != 0u && (EF_U8(obj + 0x24u) & 0x08u) == 0u && (EF_U8(obj + 0x24u) & 0x04u) == 0u) {
                uint32_t dz = abs_a28f2((int32_t)(EF_U32(obj + 8u) - (uint32_t)refC));
                uint32_t dy = abs_a28f2((int32_t)(EF_U32(obj + 4u) - (uint32_t)refB));
                uint32_t dx = abs_a28f2((int32_t)(EF_U32(obj) - (uint32_t)refA));

                d = (uint32_t)max3_signed((int32_t)dx, (int32_t)dy, (int32_t)dz);
                if (d < (uint32_t)best1) {
                    best1 = (int32_t)d;
                    rec = EF_GROUP_REC();
                    EF_U32(0x00197061u) = obj;
                    EF_U16(0x0019705fu) = EF_U16(rec + 0x50u + (uint32_t)((int32_t)(int16_t)k * 2));
                    EF_U16(0x00197083u) = (uint16_t)k;
                }
            }
            k++;
        }
    }
    if (EF_U8(EF_GROUP_REC() + 0x1au) != 2u)
        return;
    k = 0;
    while ((int16_t)k < (int16_t)EF_U8(EF_GROUP_REC() + 0x4fu)) {
        uint16_t id = EF_U16(EF_GROUP_REC() + 0x50u + (uint32_t)((int32_t)(int16_t)k * 2));

        if (id != EF_U16(0x001f71cau)) {
            obj = lookup_filtered_indexed_object((int16_t)id);
            if (obj != 0u && (EF_U8(obj + 0x24u) & 0x08u) == 0u && (EF_U8(obj + 0x24u) & 0x04u) == 0u &&
                EF_U16(obj + 0x146u) != 0xffu) {
                ef_addr32 target = lookup_filtered_indexed_object((int16_t)(EF_S32(obj + 0x144u) >> 16));

                if (target != 0u) {
                    uint32_t dz = abs_a28f2((int32_t)(EF_U32(obj + 8u) - EF_U32(target + 8u)));
                    uint32_t dy = abs_a28f2((int32_t)(EF_U32(obj + 4u) - EF_U32(target + 4u)));
                    uint32_t dx = abs_a28f2((int32_t)(EF_U32(obj) - EF_U32(target)));

                    d = (uint32_t)max3_signed((int32_t)dx, (int32_t)dy, (int32_t)dz);
                    if (d < (uint32_t)best2) {
                        best2 = (int32_t)d;
                        EF_U32(0x00197061u) = obj;
                        EF_U16(0x0019705fu) = id;
                        EF_U16(0x00197083u) = (uint16_t)k;
                    }
                }
            }
        }
        k++;
    }
#undef EF_GROUP_REC
}

/* 0x11e7c, 706 bytes. Draw button INDEX (29-byte records at 0x121fb4): unless its flag byte +0x0e bit 0 is set, look up the two text slots, enter scroll mode, blit the masked frame cell chosen by the record, clip the view to the screen and draw the label centred in the box (colour 0xe when the record's state byte is set, else 2 when the label flag +0x0d is set, else 1; the label is the shared string 0x12288c when 0x122888>>16 == 1, otherwise the pointer at +0x14), then leave scroll mode. */
void draw_button_11e7c(int16_t index)
{
    uint32_t rec = (uint32_t)((int32_t)index * 29);
    uint32_t state;
    uint32_t cell;
    ef_addr32 src;
    ef_addr32 fmt;
    int32_t width;
    int32_t x;
    int32_t y;
    uint32_t colour;

    if (EF_U8(0x00121fc2u + rec) & 1u)
        return;
    state = EF_U8(0x00121fb4u + rec);
    src = get_text_slot_cd7f0((int16_t)EF_U8(0x00121ed5u + state * 29u));
    if (src == 0u)
        return;
    {
        uint32_t second = get_text_slot_cd7f0((int16_t)EF_U8(0x00121fb5u + rec));

        if (second == 0u)
            return;
        enter_scroll_mode_c39f0(second);
    }
    cell = ((state + EF_U8(0x00121fb6u + rec)) & 0xffu) * 29u;
    blit_masked_cd82c(0u, 0u, 0u, 0u,
                      EF_U16(0x00121fbau + rec), EF_U16(0x00121fb8u + rec),
                      EF_U16(0x00121edeu + cell), EF_U16(0x00121edcu + cell),
                      EF_U16(0x00121edau + cell), EF_U16(0x00121ed8u + cell), src);
    if ((EF_S32(0x00122888u) >> 16) == 1) {
        fmt = 0x0012288cu;
    } else {
        fmt = EF_U32(0x00121fc8u + rec);
        if (fmt == 0u) {
            leave_scroll_mode_c39ac();
            return;
        }
    }
    store_four_dwords_17cf9c(0u, EF_U32(0x0015b744u) - 1u, 0u, EF_U32(0x0015b748u) - 1u);
    width = formatted_text_width_48c34(1u, fmt);
    y = (int16_t)((EF_S32(0x00121fbcu + rec) >> 17) + (EF_S32(0x00121fb8u + rec) >> 16) - 2);
    x = (int16_t)((((EF_S32(0x00121fbau + rec) >> 16) - (int16_t)width) >> 1) + (EF_S32(0x00121fb6u + rec) >> 16));
    if (state != 0u)
        colour = 0x0eu;
    else if (EF_U8(0x00121fc1u + rec) != 0u)
        colour = 2u;
    else
        colour = 1u;
    draw_formatted_text_mode_48bb0(colour, 1u, x, y, fmt);
    leave_scroll_mode_c39ac();
}

/* 0x1f994, 1011 bytes. Advance the scrolling text box INDEX (10-byte records at 0x127a98: centre x, centre y, width, height, slot byte): when the record names a text slot, enter scroll mode on it, otherwise draw into FALLBACKBITMAP (the caller's EDI). Latch the box as the clip rectangle, then consume one character of the current text (0x1237f0, filled from the queue when empty) at the cursor (column 0x1237ec, row 0x1237ee): CR/# and end of text start a new line, LF only ends the character, '>' jumps to the next 70-pixel stop, TAB to the next 16-pixel stop, a space wraps the line when the following word would not fit, and any other glyph is drawn upper-cased in colour 0xf. When the cursor passes the bottom the bitmap is scrolled up by one 8-pixel row and the freed rows are filled with colour 3. */
void draw_text_box_1f994(int16_t index, ef_addr32 fallbackBitmap)
{
    uint32_t rec = 0x00127a98u + (uint32_t)((int32_t)index * 10);
    int16_t cx = (int16_t)EF_U16(rec);
    int16_t yc = (int16_t)EF_U16(rec + 2u);
    int16_t width = (int16_t)EF_U16(rec + 4u);
    uint16_t heightWord = EF_U16(rec + 6u);
    int8_t slotByte = (int8_t)EF_U8(rec + 8u);
    int16_t left = (int16_t)(cx - (width >> 1));
    int16_t top = (int16_t)(yc - ((int16_t)heightWord >> 1));
    uint16_t strWord = EF_U16(0x0001e680u);
    ef_addr32 dst = fallbackBitmap;
    uint8_t glyph[4];
    uint8_t wordBuf[0x60];
    ef_addr32 text;
    int32_t advance = 0;
    int32_t x1;
    int32_t x2;
    int32_t y1;
    int32_t y2;

    if (slotByte != -1) {
        uint32_t slot = get_text_slot_cd7f0((int16_t)slotByte);

        if (slot == 0u)
            return;
        dst = slot;
        enter_scroll_mode_c39f0(slot);
    }
    dst += (uint32_t)(((int32_t)top << 8) + (int32_t)left);
    y1 = (int32_t)yc - ((int16_t)heightWord >> 1);
    y2 = (int32_t)yc + ((int16_t)heightWord >> 1);
    x1 = (int32_t)cx - (width >> 1);
    x2 = (int32_t)cx + (width >> 1);
    latch_values_ce328((uint32_t)x1, (uint32_t)x2, (uint32_t)y1, (uint32_t)y2);
    store_four_dwords_17cf9c((uint32_t)x1, (uint32_t)x2, (uint32_t)y1, (uint32_t)y2);
    left = (int16_t)(left + 4);

    text = EF_U32(0x001237f0u);
    if (text == 0u) {
        EF_U32(0x001237f0u) = queue_pop_1f8c4();
        return;
    }
    {
        uint8_t c = EF_U8(text);

        if (c == 0u) {
            EF_U32(0x001237f0u) = 0u;
            EF_U16(0x001237ecu) = 0u;
            EF_U16(0x001237eeu) = (uint16_t)(EF_U16(0x001237eeu) + 8u);
            goto after_char;
        }
        if (c == 0x0du || c == 0x23u) {
            EF_U16(0x001237ecu) = 0u;
            EF_U16(0x001237eeu) = (uint16_t)(EF_U16(0x001237eeu) + 8u);
            advance = 0;
        } else if (c == 0x3eu) {
            int32_t col = (int16_t)EF_U16(0x001237ecu);

            EF_U16(0x001237ecu) = (uint16_t)(col - col % 0x46 + 0x46);
            advance = 0;
        } else if (c == 0x09u) {
            int32_t col = (int16_t)EF_U16(0x001237ecu);

            EF_U16(0x001237ecu) = (uint16_t)(0x10 - col % 0x10 + col);
            advance = 0;
        } else if (c == 0x0au) {
            advance = 0;
        } else {
            int draw = 1;

            if (c == 0x20u) {
                int16_t n = 1;
                int16_t len;

                for (;;) {
                    uint8_t ch = EF_U8(EF_U32(0x001237f0u) + (uint32_t)(int32_t)n);

                    if (ch == 0u || ch == 0x20u || ch == 0x23u || ch == 0x0du)
                        break;
                    n++;
                }
                len = (int16_t)(n - 1);
                if (len != 0) {
                    int32_t w;
                    int32_t pos;

                    (void)memcpy_dc832((ef_addr32)(uintptr_t)wordBuf, EF_U32(0x001237f0u), (uint32_t)(int32_t)len);
                    wordBuf[len] = 0u;
                    w = formatted_text_width_48c34(1u, (ef_addr32)(uintptr_t)wordBuf);
                    pos = (int32_t)(int16_t)w + (int32_t)left + (int16_t)EF_U16(0x001237ecu);
                    if (pos > (int32_t)width - 5) {
                        EF_U16(0x001237ecu) = 0u;
                        EF_U16(0x001237eeu) = (uint16_t)(EF_U16(0x001237eeu) + 8u);
                        advance = 0;
                        draw = 0;
                    }
                }
            }
            if (draw) {
                uint8_t ch = EF_U8(EF_U32(0x001237f0u));

                glyph[0] = ch;
                glyph[1] = (uint8_t)(strWord >> 8);
                glyph[2] = 0u;
                glyph[3] = 0u;
                if (ch >= 0x61u && ch <= 0x7au)
                    glyph[0] = (uint8_t)(ch - 0x20u);
                advance = formatted_text_width_48c34(1u, (ef_addr32)(uintptr_t)glyph);
                draw_formatted_text_mode_48bb0(0x0fu, 1u,
                                               (int16_t)((uint16_t)left + EF_U16(0x001237ecu)),
                                               (int16_t)((uint16_t)top + EF_U16(0x001237eeu)),
                                               (ef_addr32)(uintptr_t)glyph);
            }
        }
        EF_U32(0x001237f0u) = EF_U32(0x001237f0u) + 1u;
        EF_U16(0x001237ecu) = (uint16_t)(EF_U16(0x001237ecu) + (uint16_t)advance);
    }
after_char:
    if ((int16_t)EF_U16(0x001237ecu) > (int32_t)width - 5) {
        EF_U16(0x001237ecu) = 0u;
        EF_U16(0x001237eeu) = (uint16_t)(EF_U16(0x001237eeu) + 8u);
    }
    if ((int16_t)EF_U16(0x001237eeu) <= (int32_t)(int16_t)heightWord - 10)
        return;
    {
        int32_t row;
        int32_t dx;

        EF_U16(0x001237eeu) = (uint16_t)(EF_U16(0x001237eeu) - 8u);
        for (row = 0; row < (int32_t)(int16_t)heightWord - 8; row++) {
            ef_addr32 d = dst + ((uint32_t)row << 8);
            ef_addr32 s = d + 0x800u;
            int16_t i;

            if (width > 0) {
                for (i = 0; i < width; i++)
                    EF_U8(d + (uint32_t)i) = EF_U8(s + (uint32_t)i);
            }
        }
        for (dx = (int32_t)(int16_t)heightWord - 8; (int16_t)dx < (int16_t)heightWord; dx++) {
            ef_addr32 d = dst + ((uint32_t)(int32_t)(int16_t)dx << 8);
            int16_t i;

            for (i = 0; i < width; i++)
                EF_U8(d + (uint32_t)i) = 3u;
        }
    }
}

/* 0x2e494, 681 bytes. Append the current position of the tracked group (S16 0x1f71cc, 0xca-byte records at 0x19bb58; doubles at +0x69 and +0x61 scaled by 0xb4250, y mirrored as 0x190 - y) to the 10-byte sample table at 0x1a3650 (count at 0x126550, limit 0x1f3) together with the three words at 0x12583c..0x125840; when the new point lies within one unit of the previous sample only the three trailing words of that sample are overwritten. Skipped when 0x126518 or 0x1a666b is set or the group is 0xff. */
static int32_t scaled_double_2e494(ef_addr32 addr)
{
    union {
        uint32_t w[2];
        double d;
    } v;

    v.w[0] = EF_U32(addr);
    v.w[1] = EF_U32(addr + 4u);
    return double_to_scaled_b4250(v.d);
}

static void append_track_sample_2e494(void)
{
    int32_t group = (int16_t)EF_U16(0x001f71ccu);
    uint32_t sample;
    uint32_t scaledX;
    uint32_t scaledY;

    scaledX = (uint32_t)scaled_double_2e494(EF_U32(0x0019bb58u) + (uint32_t)(group * 0xca) + 0x69u);
    sample = 0x001a3650u + (uint32_t)((int32_t)(int16_t)EF_U16(0x00126550u) * 10);
    EF_U16(sample) = (uint16_t)scaledX;
    group = (int16_t)EF_U16(0x001f71ccu);
    scaledY = 0x190u - (uint32_t)scaled_double_2e494(EF_U32(0x0019bb58u) + (uint32_t)(group * 0xca) + 0x61u);
    sample = 0x001a3650u + (uint32_t)((int32_t)(int16_t)EF_U16(0x00126550u) * 10);
    EF_U16(sample + 2u) = (uint16_t)scaledY;
    EF_U16(sample + 4u) = EF_U16(0x0012583cu);
    EF_U16(sample + 6u) = EF_U16(0x0012583eu);
    EF_U16(sample + 8u) = EF_U16(0x00125840u);
    EF_U16(0x00126550u) = (uint16_t)(EF_U16(0x00126550u) + 1u);
}

void record_track_sample_2e494(void)
{
    int32_t count = (int16_t)EF_U16(0x00126550u);
    int32_t group;

    if (count >= 0x1f3 || EF_U16(0x00126518u) != 0u || EF_U8(0x001a666bu) != 0u)
        return;
    group = (int16_t)EF_U16(0x001f71ccu);
    if (group == 0xff)
        return;
    if (EF_U16(0x00126550u) == 0u) {
        append_track_sample_2e494();
        return;
    }
    {
        uint32_t prev = 0x001a3650u + (uint32_t)((count - 1) * 10);
        int16_t prevX = (int16_t)EF_U16(prev);
        int16_t prevY = (int16_t)EF_U16(prev + 2u);
        uint32_t rec = EF_U32(0x0019bb58u) + (uint32_t)(group * 0xca);
        int32_t nx = scaled_double_2e494(rec + 0x69u);
        int32_t ny;

        rec = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)(int16_t)EF_U16(0x001f71ccu) * 0xca);
        ny = 0x190 - scaled_double_2e494(rec + 0x61u);
        if (abs_a28f2((int32_t)prevX - (int16_t)nx) > 1u || abs_a28f2((int32_t)prevY - (int16_t)ny) > 1u) {
            append_track_sample_2e494();
            return;
        }
        prev = 0x001a3650u + (uint32_t)(((int32_t)(int16_t)EF_U16(0x00126550u) - 1) * 10);
        EF_U16(prev + 4u) = EF_U16(0x0012583cu);
        EF_U16(prev + 6u) = EF_U16(0x0012583eu);
        EF_U16(prev + 8u) = EF_U16(0x00125840u);
    }
}

/* 0x58714, 610 bytes. Debug overlay: clip to the full screen, print the selected object's header lines (object looked up by the id word at 0x1a823c, flag bits of +0x24, owner record +0x28) and then one line per non-empty row of the 80x8-word table at 0x1a5df0. */
void draw_debug_overlay_58714(void)
{
    uint32_t width = EF_U32(0x0015b744u);
    uint32_t height = EF_U32(0x0015b748u);
    ef_addr32 obj;
    int32_t y = 0x10;
    int16_t row;

    latch_values_ce328(0u, width - 1u, 0u, height - 1u);
    store_four_dwords_17cf9c(0u, EF_U32(0x0015b744u) - 1u, 0u, EF_U32(0x0015b748u) - 1u);
    obj = lookup_filtered_indexed_object((int16_t)EF_U16(0x001a823cu));
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(0x0cu, 0, 0, 0x00185d30u);
    EF_U8(0x00127b1au) = 1u;
    draw_shadowed_text_48c90(0x0fu, 0, 8, 0x00185d40u, EF_U8(0x00127da9u), EF_U8(0x0012583au), EF_U8(0x00127da4u));
    if (obj != 0u) {
        uint16_t flags;
        int32_t owner;

        EF_U8(0x00127b1au) = 1u;
        draw_shadowed_text_48c90(0x0cu, 0, 0x10, 0x00185d64u, EF_S32(obj + 0x16u) >> 16, EF_U8(0x001a666bu));
        EF_U8(0x00127b1au) = 1u;
        draw_shadowed_text_48c90(0x0cu, 0, 0x18, 0x00185d7cu, EF_S32(obj + 0x14u) >> 16,
                                 EF_S32(0x001f71c8u) >> 16);
        flags = EF_U16(obj + 0x24u);
        EF_U8(0x00127b1au) = 1u;
        draw_shadowed_text_48c90(0x0cu, 0, 0x20, 0x00185d8cu, (uint32_t)((flags >> 3) & 1u),
                                 (uint32_t)((flags >> 2) & 1u), (uint32_t)((flags >> 1) & 1u));
        owner = EF_S32(obj + 0x26u) >> 16;
        EF_U8(0x00127b1au) = 1u;
        draw_shadowed_text_48c90(0x0cu, 0, 0x28, 0x00185da8u, owner,
                                 (uint32_t)EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(owner * 0xca) + 0x20u));
        y = 0x30;
    }
    latch_values_ce328(0u, EF_U32(0x0015b744u) - 1u, 0u, EF_U32(0x0015b748u) - 1u);
    store_four_dwords_17cf9c(0u, EF_U32(0x0015b744u) - 1u, 0u, EF_U32(0x0015b748u) - 1u);
    for (row = 0; row < 0x50; row++) {
        uint32_t base = 0x001a5df0u + ((uint32_t)(int32_t)row << 4);
        uint16_t sum = 0u;
        int32_t k;

        for (k = 0; k < 8; k++)
            sum = (uint16_t)(sum + EF_U16(base + (uint32_t)(k * 2)));
        if (sum == 0u)
            continue;
        EF_U8(0x00127b1au) = 1u;
        draw_shadowed_text_48c90(0x0fu, 0, (int16_t)y, 0x00185dc0u, (int32_t)row,
                                 EF_S32(base - 2u) >> 16, EF_S32(base) >> 16, EF_S32(base + 2u) >> 16,
                                 EF_S32(base + 4u) >> 16, EF_S32(base + 6u) >> 16, EF_S32(base + 8u) >> 16,
                                 EF_S32(base + 10u) >> 16, EF_S32(base + 12u) >> 16);
        y += 8;
    }
}

/* Original 0x5e21c (677 bytes). Odometer-style number renderer.  Inputs are
 * EAX=x offset, EDX=y offset (both shifted by the mode byte at 0x12b369) and
 * EBX=string.  The original keeps its locals packed in overlapping stack
 * words; the semantic locals below mirror them, including the stale "mode"
 * dword that the minus branch overwrites with the 16-bit x position.  The
 * string is truncated destructively one character at a time. */
void odometer_text_5e21c(int32_t xOffset, int32_t yOffset, ef_addr32 text)
{
    uint32_t shift = EF_U8(0x0012b369u) & 31u;
    int16_t x = (int16_t)(EF_U16(0x001a6cfcu) + ((uint32_t)xOffset << shift));
    int16_t y = (int16_t)(EF_U16(0x001a6cfau) + ((uint32_t)yOffset << shift));
    uint8_t buf[2];
    uint8_t flag = 0u;
    int16_t length, mode, digit, idx, width, firstWidth, X, yy, slot;
    int32_t scaled;
    uint32_t modeSlot;

    buf[0] = (uint8_t)EF_U16(0x00058cb2u);
    buf[1] = (uint8_t)(EF_U16(0x00058cb2u) >> 8);
    if (text == 0u)
        return;
    length = (int16_t)guest_string_length(text);
    if (length <= 1)
        return;
    mode = EF_U8(0x0012b369u) ? 8 : 6;
    modeSlot = (uint32_t)(int32_t)mode;
    scaled = ((int32_t)EF_U8(text + (uint32_t)(int32_t)length - 1u) - 0x30) * (int32_t)modeSlot;
    digit = (int16_t)((int16_t)scaled / 10);
    if (digit >= 10 || digit < 0)
        return;
    width = (int16_t)formatted_text_width_48c34(EF_U8(0x0012b369u) == 1u ? 0u : 1u, text);
    firstWidth = width;
    store_four_dwords_17cf9c((uint32_t)(int32_t)x, (uint32_t)((int32_t)width + x),
                             (uint32_t)(int32_t)y, (uint32_t)((int32_t)(int16_t)modeSlot + y));
    for (idx = (int16_t)(length - 2); idx >= 0; idx--) {
        uint8_t c;
        uint8_t bl;

        c = EF_U8(text + (uint32_t)(int32_t)idx);
        EF_U8(text + (uint32_t)(int32_t)idx) = 0u;
        width = (int16_t)formatted_text_width_48c34(EF_U8(0x0012b369u) == 1u ? 0u : 1u, text);
        X = (int16_t)(width + x);
        bl = (uint8_t)(1u - EF_U8(0x0012b369u));
        buf[0] = c;
        if (c == 0x2eu) {
            draw_formatted_text_mode_48bb0(EF_U8(0x00128148u), bl, X, y, 0x00185fe4u);
        } else if (c == 0x2bu) {
            draw_formatted_text_mode_48bb0(EF_U8(0x00128148u), bl, X, y, 0x00185fe8u);
        } else if (c == 0x2du) {
            yy = (int16_t)(y - digit);
            draw_formatted_text_mode_48bb0(EF_U8(0x00128148u), bl, X, yy,
                                           (ef_addr32)(uintptr_t)buf);
            modeSlot = (uint32_t)(int32_t)X;
            slot = (int16_t)(yy + (int16_t)(uint16_t)modeSlot);
            draw_formatted_text_mode_48bb0(EF_U8(0x00128148u),
                                           (uint8_t)(1u - EF_U8(0x0012b369u)),
                                           (int32_t)firstWidth,
                                           (int16_t)slot, 0x00185fe8u);
        } else {
            yy = (int16_t)(y - digit);
            draw_formatted_text_mode_48bb0(EF_U8(0x00128148u),
                                           (uint8_t)(1u - EF_U8(0x0012b369u)), X, yy,
                                           (ef_addr32)(uintptr_t)buf);
            if (digit != 0) {
                buf[0]++;
                if (buf[0] > 0x39u) {
                    buf[0] = 0x30u;
                    flag = 1u;
                } else {
                    flag = 0u;
                }
                draw_formatted_text_mode_48bb0(EF_U8(0x00128148u),
                                               (uint8_t)(1u - EF_U8(0x0012b369u)), X,
                                               (int16_t)(yy + (int16_t)(uint16_t)modeSlot),
                                               (ef_addr32)(uintptr_t)buf);
            }
            if (flag == 0u)
                digit = 0;
        }
    }
}

/* Original 0x76370 (688 bytes). Palette fade/tint update for the current
 * screen mode (byte 0x12db9e); returns the blend factor (nominally 25). */
int32_t update_screen_palette_76370(uint32_t xIndex, uint32_t yIndex)
{
    int32_t a = EF_S32(0x00142e06u + ((xIndex & 0xffffu) >> 3) * 2u) >> 31;
    int32_t b = EF_S32(0x00142e06u + ((yIndex & 0xffffu) >> 3) * 2u) >> 31;
    int32_t factor = (int32_t)((int16_t)a * (int32_t)(int16_t)b) / 0x19 + 0x19;
    uint8_t mode, slot;

    if (EF_U8(0x001f88eau) != EF_U8(0x0012db9eu))
        EF_U8(0x00128548u) = 1u;
    if ((int16_t)factor != EF_S16(0x0012855cu) || EF_U8(0x00128548u) != 0u) {
        EF_U16(0x0012855cu) = (uint16_t)factor;
        EF_U16(0x001f88eau) = (uint16_t)EF_U8(0x0012db9eu);
    }
    blend_palette_757bc((int16_t)factor, 0x001f74e0u, 0x001f71e0u, 0x001f74e0u, 0x100u, 0u);
    if (EF_U8(0x001f88fau) != 0u) {
        blend_palette_758c0(EF_U8(0x001f88fau), 0x001f74e0u, 0x001f74e0u, 0x3fu, 0x3fu, 0x3fu);
        EF_U8(0x001f88fau) = (uint8_t)(EF_U8(0x001f88fau) + 1u);
        if (EF_U8(0x001f88fau) > 0x32u)
            EF_U8(0x001f88fau) = 0u;
    }
    mode = EF_U8(0x0012db9eu);
    if (mode == 0x0cu || (mode >= 0x12u && mode <= 0x16u) || mode == 0x1bu || mode == 0x20u ||
        mode == 0x29u || mode == 0x30u || mode == 0x35u || mode == 0x36u || mode == 0x46u ||
        mode == 0x47u) {
        slot = EF_U8(0x001f88fbu);
        if (slot == 1u) {
            claim_ui_slot_801e4(0x41, 1u, 0u);
            blend_palette_758c0((uint16_t)(EF_S32(0x001f88eeu) >> 16), 0x001f74e0u,
                                0x001f74e0u, 0x3fu, 0u, 0u);
        } else if (slot == 2u) {
            claim_ui_slot_801e4(0x41, 1u, 0u);
            blend_palette_758c0((uint16_t)(EF_S32(0x001f88ecu) >> 16), 0x001f74e0u,
                                0x001f74e0u, 0u, 0u, 0u);
        }
    }
    mode = EF_U8(0x0012db9eu);
    if (mode == 0x10u || mode == 0x21u) {
        if (EF_U8(0x0012854cu) == 0u) {
            apply_palette_d663c(0x00425190u);
            EF_U8(0x0012854cu) = 1u;
        }
        return factor;
    }
    if ((EF_U8(0x00159130u) & 0x80u) != 0u && EF_U8(0x0012854au) != 0u &&
        EF_U8(0x00122c53u) == 0u &&
        (mode == 0x12u || mode == 0x0cu || mode == 0x13u || mode == 0x14u || mode == 0x36u ||
         mode == 0x15u || mode == 0x16u || mode == 0x29u || mode == 0x18u)) {
        if (EF_U8(0x0012854cu) == 0u) {
            apply_palette_d663c(0x00425190u);
            EF_U8(0x0012854cu) = 1u;
        }
        return factor;
    }
    if (EF_U8(0x00128548u) != 0u) {
        fade_palette_76948();
        apply_palette_d663c(0x001f74e0u);
        EF_U8(0x00128548u) = 0u;
        EF_U8(0x0012854cu) = 0u;
        return factor;
    }
    if (EF_U8(0x0012854cu) != 0u) {
        apply_palette_d663c(0x001f74e0u);
        EF_U8(0x00128548u) = 0u;
        EF_U8(0x0012854cu) = 0u;
    }
    return factor;
}

/* Original 0x92668 (793 bytes). Pick the current target object for the
 * active display mode; returns its guest pointer or 0. */
ef_addr32 select_target_object_92668(void)
{
    uint8_t mode;
    int32_t id;
    ef_addr32 own, target, other;
    int32_t vec[3];
    int32_t dist;

    if (EF_U32(0x0020f154u) != 0u && EF_U8(0x0012db9eu) == 0x18u)
        return EF_U32(0x0020f154u);
    mode = EF_U8(0x0012db9eu);
    if (mode == 0x14u || mode == 0x18u || mode == 0x36u) {
        id = EF_S32(0x001a823cu) >> 16;
        if (id == -1 || id == 0xff)
            return 0u;
        return lookup_filtered_indexed_object((int16_t)id);
    }
    if (mode >= 0x15u && mode <= 0x17u && (EF_S32(0x001f71c8u) >> 16) != 0xff &&
        (EF_S32(0x0019bb5cu) >> 16) != 0xff) {
        own = EF_U32(0x0019bb58u) + (uint32_t)((EF_S32(0x0019bb5cu) >> 16) * 0xca);
        own = lookup_filtered_indexed_object(
            (int16_t)(EF_S32(own + 0x4eu + (uint32_t)EF_U8(0x0012dc34u) * 2u) >> 16));
        if (own != 0u && (EF_U8(own + 0x24u) & 8u) == 0u) {
            if (mode == 0x15u) {
                target = own;
                vec[0] = EF_S32(target) >> 8;
                vec[1] = EF_S32(target + 4u) >> 8;
                vec[2] = EF_S32(target + 8u) >> 8;
                dist = guest_fn_23760_x((ef_addr32)(uintptr_t)vec);
                if (dist <= 0x5000) {
                    EF_U16(0x001a823eu) = EF_U16(target + 0x16u);
                    EF_U16(0x001a8238u) = EF_U16(target + 0x16u);
                }
                return target;
            }
            if (mode == 0x16u) {
                uint32_t partner = EF_U16(own + 0x146u);

                if (partner != 0xffu && partner != (uint32_t)(EF_S32(0x001f71c8u) >> 16)) {
                    other = lookup_filtered_indexed_object(
                        (int16_t)(EF_S32(own + 0x144u) >> 16));
                    if (other != 0u) {
                        vec[0] = EF_S32(other) >> 8;
                        vec[1] = EF_S32(other + 4u) >> 8;
                        vec[2] = EF_S32(other + 8u) >> 8;
                        dist = guest_fn_23760_x((ef_addr32)(uintptr_t)vec);
                        if (dist <= 0x2800) {
                            EF_U16(0x001a823eu) = EF_U16(own + 0x146u);
                            EF_U16(0x001a8238u) = EF_U16(own + 0x16u);
                        }
                        return other;
                    }
                }
            } else {
                if (EF_U16(own + 0x158u) != 0xffu) {
                    other = lookup_filtered_indexed_object(
                        (int16_t)(EF_S32(own + 0x156u) >> 16));
                    if (other != 0u) {
                        vec[0] = EF_S32(other) >> 8;
                        vec[1] = EF_S32(other + 4u) >> 8;
                        vec[2] = EF_S32(other + 8u) >> 8;
                        dist = guest_fn_23760_x((ef_addr32)(uintptr_t)vec);
                        if (dist <= 0x2800)
                            EF_U16(0x001a823eu) = EF_U16(own + 0x158u);
                        return other;
                    }
                }
            }
        }
    }
    id = EF_S32(0x001a8234u) >> 16;
    if (id != 0xff) {
        target = lookup_filtered_indexed_object((int16_t)id);
        if (target != 0u)
            return target;
    }
    id = EF_S32(0x001a823au) >> 16;
    if (id != 0xff) {
        target = lookup_filtered_indexed_object((int16_t)id);
        if (target != 0u)
            return target;
    }
    if (EF_U32(0x001a821cu) != 0u)
        return EF_U32(0x001a821cu);
    return EF_U32(EF_U32(0x002143f8u));
}

/* Original 0x92984 (633 bytes). Scene camera update for the node selected by
 * byte 0x12dc35: without a node reset the pending flag and mode byte, else
 * apply the direction keys (0x2144aa bits 0-3) to the angle bytes/words at
 * 0x21454e..0x214551, rebuild the angles, apply the zoom keys (bits 4/5),
 * place the camera at the node position offset by the zoom-scaled basis, and
 * push angles, pair and input triplet to the scene helpers. */
void update_scene_camera_node_92984(void)
{
    ef_addr32 node = lookup_scene_node((int16_t)EF_U8(0x0012dc35u));
    int32_t zoom;

    if (node == 0u) {
        EF_U16(0x0021449cu) = 1u;
        EF_U8(0x0012db9eu) = EF_U8(0x00214486u);
        return;
    }
    init_bounds_d0b68(0x00214404u);
    if (EF_U8(0x002144aau) & 1u)
        EF_U8(0x0021454fu) = (uint8_t)(EF_U8(0x0021454fu) + 2u);
    if (EF_U8(0x002144aau) & 2u)
        EF_U16(0x0021454eu) = (uint16_t)(EF_U16(0x0021454eu) - 0x200u);
    if (EF_U8(0x002144aau) & 4u)
        EF_U8(0x00214551u) = (uint8_t)(EF_U8(0x00214551u) + 2u);
    if (EF_U8(0x002144aau) & 8u)
        EF_U16(0x00214550u) = (uint16_t)(EF_U16(0x00214550u) - 0x200u);
    EF_U16(0x0021454cu) = 0u;
    update_scene_angle_first(0x00214404u, EF_U16(0x0021454eu));
    update_scene_angle_second(0x00214404u, EF_U16(0x00214550u));
    EF_U16(0x002144b6u) = (uint16_t)angle_d13ac(0x00214404u);
    EF_U16(0x002144b4u) = (uint16_t)forward_pair_d13dc(0x00214404u);
    EF_U16(0x002144b0u) = (uint16_t)angle_d13ec(0x00214404u);
    if (EF_U8(0x002144aau) & 0x10u)
        EF_U16(0x0012db9cu) = (uint16_t)(EF_U16(0x0012db9cu) - EF_U16(0x00127d92u));
    if (EF_U8(0x002144aau) & 0x20u)
        EF_U16(0x0012db9cu) = (uint16_t)(EF_U16(0x0012db9cu) + EF_U16(0x00127d92u));
    if ((EF_S32(0x0012db9au) >> 16) < 10)
        EF_U16(0x0012db9cu) = 10u;
    if ((EF_S32(0x0012db9au) >> 16) > 0x3e8)
        EF_U16(0x0012db9cu) = 0x3e8u;
    zoom = -(EF_S32(0x0012db9au) >> 16);
    EF_U32(0x0012dba8u) = (uint32_t)(EF_S32(0x0021440cu) * zoom);
    EF_U32(0x0012dbacu) = (uint32_t)(EF_S32(0x00214418u) * zoom);
    EF_U32(0x0012dbb0u) = (uint32_t)(EF_S32(0x00214424u) * zoom);
    EF_U32(0x0021444cu) = (uint32_t)((EF_S32(node) >> 8) + (EF_S32(0x0012dba8u) >> 16));
    EF_U32(0x00214448u) = (uint32_t)((EF_S32(node + 4u) >> 8) + (EF_S32(0x0012dbacu) >> 16));
    EF_U32(0x00214450u) = (uint32_t)((EF_S32(node + 8u) >> 8) + (EF_S32(0x0012dbb0u) >> 16));
    EF_U32(0x00214370u) = (EF_U32(node) << 8) + EF_U32(0x0012dba8u);
    EF_U32(0x0021436cu) = (EF_U32(node + 4u) << 8) + EF_U32(0x0012dbacu);
    EF_U32(0x00214374u) = (EF_U32(node + 8u) << 8) + EF_U32(0x0012dbb0u);
    if (scene_metric_reaches_threshold((int32_t)EF_U32(0x0021444cu), (int32_t)EF_U32(0x00214448u)) == 0u) {
        int32_t metric = scene_metric_remaining();

        EF_U32(0x00214448u) = (uint32_t)metric;
        EF_U32(0x0021436cu) = (uint32_t)metric << 16;
    }
    request_scene_pair((int16_t)(0u - EF_U16(0x002144b6u)), (int16_t)(0u - EF_U16(0x002144b0u)));
    set_scene_angles((int16_t)(0u - EF_U16(0x002144b6u)), (int16_t)(0u - EF_U16(0x002144b4u)),
                     (int16_t)(0u - EF_U16(0x002144b0u)));
    set_scene_input_triplet((int32_t)EF_U32(0x00214370u), (int32_t)EF_U32(0x0021436cu),
                            (int32_t)EF_U32(0x00214374u));
}

/* Original 0x98658 (1126 bytes). Draw the loading/progress panel: AL selects the
 * low-resolution (0) or high-resolution (nonzero) layout.  Restores the strip
 * saved by 0x98ac0, prints the message at 0x12dda0, draws a rectangular
 * frame through the record at 0x4208e4..0x4208ec, fills the percentage bar
 * from 0x40c11c, prints the percentage and saves the strips; at 100% the
 * strip rows are copied back and the saved positions invalidated. */
void draw_progress_panel_98658(uint8_t highRes)
{
    char buf[0x50];
    ef_addr32 text = (ef_addr32)(uintptr_t)buf;
    int32_t percent;
    int32_t length;
    int32_t filled;
    uint32_t textX, textY;

    restore_strip_98ac0(highRes);
    guest_sprintf_a4342(text, 0x0018ad88u, EF_U32(0x0012dda0u));
    length = (int32_t)guest_string_length(EF_U32(0x0012dda0u));
    if (highRes == 0u) {
        draw_glyph_string_eb008(0xfu, (uint32_t)(0x140 - (length * 4 + length)) >> 1, 0xa5u, text);
        filled = (int32_t)((uint32_t)EF_S32(0x0040c11cu) * 0xdau) / 100;
        put_screen_rows_eb050(0u, 0u, 0u, 0u, 0xfu, 0xa0u, 0x23u, 0x1eu, 0x0040c124u);
        EF_U16(0x004208e6u) = 0xfu;
        EF_U16(0x004208e4u) = 7u;
        EF_U16(0x004208e8u) = 0xa0u;
        EF_U16(0x004208eau) = 0x32u;
        EF_U16(0x004208ecu) = 0xa0u;
        forward_record_cf78c();
        EF_U16(0x004208e6u) = 0x32u;
        EF_U16(0x004208eau) = 0x32u;
        EF_U16(0x004208e4u) = 7u;
        EF_U16(0x004208e8u) = 0xa0u;
        EF_U16(0x004208ecu) = 0xbeu;
        forward_record_cf78c();
        EF_U16(0x004208e6u) = 0x32u;
        EF_U16(0x004208e4u) = 7u;
        EF_U16(0x004208e8u) = 0xbeu;
        EF_U16(0x004208eau) = 0xfu;
        EF_U16(0x004208ecu) = 0xbeu;
        forward_record_cf78c();
        EF_U16(0x004208e6u) = 0xfu;
        EF_U16(0x004208eau) = 0xfu;
        EF_U16(0x004208e4u) = 7u;
        EF_U16(0x004208e8u) = 0xbeu;
        EF_U16(0x004208ecu) = 0xa0u;
        forward_record_cf78c();
        fill_rect_ceccc(7u, 0x32, 0xae, filled + 0x32, 0xaf);
        filled += 0x32;
        save_and_draw_strip_98b68((uint32_t)(int32_t)(int16_t)filled, 0xa0u);
        textY = 0xadu;
        textX = 0u;
    } else {
        draw_glyph_string_eb008(0xfu, (uint32_t)(0x280 - (length * 4 + length)) >> 1, 0x16du, text);
        filled = (int32_t)((uint32_t)EF_S32(0x0040c11cu) * 0x1b6u) / 100;
        put_screen_rows_eb050(0u, 0u, 0u, 0u, 0x41u, 0x168u, 0x23u, 0x1eu, 0x0040c124u);
        EF_U16(0x004208eau) = 0x64u;
        EF_U16(0x004208e4u) = 7u;
        EF_U16(0x004208e6u) = 0x41u;
        EF_U16(0x004208e8u) = 0x168u;
        EF_U16(0x004208ecu) = 0x168u;
        forward_record_cf78c();
        EF_U16(0x004208e6u) = 0x64u;
        EF_U16(0x004208eau) = 0x64u;
        EF_U16(0x004208e4u) = 7u;
        EF_U16(0x004208e8u) = 0x168u;
        EF_U16(0x004208ecu) = 0x186u;
        forward_record_cf78c();
        EF_U16(0x004208eau) = 0x64u;
        EF_U16(0x004208e4u) = 7u;
        EF_U16(0x004208e6u) = 0x41u;
        EF_U16(0x004208e8u) = 0x186u;
        EF_U16(0x004208ecu) = 0x186u;
        forward_record_cf78c();
        EF_U16(0x004208e4u) = 7u;
        EF_U16(0x004208e6u) = 0x41u;
        EF_U16(0x004208e8u) = 0x168u;
        EF_U16(0x004208eau) = 0x41u;
        EF_U16(0x004208ecu) = 0x186u;
        forward_record_cf78c();
        fill_rect_ceccc(7u, 0x64, 0x176, filled + 0x64, 0x177);
        filled += 0x64;
        save_and_draw_strip_98b68((uint32_t)(int32_t)(int16_t)filled, 0x168u);
        textY = 0x175u;
        textX = 1u;
    }
    guest_sprintf_a4342(text, 0x0018ad8cu, (uint32_t)EF_S32(0x0040c11cu));
    percent = EF_S32(0x0040c11cu);
    if (percent < 10)
        draw_glyph_string_eb008(0xfu, textX ? 0x4du : 0x1bu, textY, text);
    else if (percent < 100)
        draw_glyph_string_eb008(0xfu, textX ? 0x4au : 0x18u, textY, text);
    else
        draw_glyph_string_eb008(0xfu, textX ? 0x47u : 0x15u, textY, text);
    toggle_page_dc250();
    if (EF_S32(0x0040c11cu) == 100) {
        if (textX)
            put_screen_rows_eb050(0u, 0u, 0u, 0u, 0x41u, 0x168u, 0x1fcu, 0x1eu, 0x00408590u);
        else
            put_screen_rows_eb050(0u, 0u, 0u, 0u, 0xfu, 0xa0u, 0xfdu, 0x1eu, 0x00408590u);
        EF_U16(0x0040cd72u) = 0xffffu;
        EF_U16(0x0040cd74u) = 0xffffu;
    }
}

/* Original 0xacbe4 (980 bytes). Pointer hit test: poll the pointer device
 * (0xac854), store the position/button state in RESULT (+0x18..+0x1e), then
 * either keep the captured target at RESULT+0x45/0x49 (when its button bit is
 * held) or search the widget table at 0x41eda8 from the top for the first
 * enabled widget under the pointer (corner 3x3 excluded) and, inside it, for
 * the child (list head at +0x91, next at +0x21) under the pointer. */
void hit_test_pointer_acbe4(ef_addr32 result)
{
    int16_t px, py;
    uint8_t hitWidget = 0u;
    uint8_t hitInner = 0u;
    int16_t i;
    ef_addr32 obj = 0u;
    ef_addr32 child;
    int32_t slot;

    poll_pointer_device_ac854();
    px = (int16_t)EF_U16(0x0013725eu);
    py = (int16_t)EF_U16(0x00137260u);
    EF_U16(result + 0x18u) = (uint16_t)px;
    EF_U16(result + 0x1au) = (uint16_t)py;
    EF_U8(result + 0x1cu) = EF_U8(0x0041f2f1u);
    EF_U8(result + 0x1eu) = (uint8_t)((EF_U8(result + 0x1eu) & 0xfeu) | (EF_U8(0x0041f2f2u) != 0u));
    EF_U8(result + 0x1eu) = (uint8_t)((EF_U8(result + 0x1eu) & 0xfdu) |
                                      ((EF_U8(0x0041f2f0u) != 0u) << 1));
    EF_U8(result + 0x1eu) = (uint8_t)((EF_U8(result + 0x1eu) & 0xfbu) |
                                      ((EF_U8(0x0041f2f3u) != 0u) << 2));
    if (EF_U32(result + 0x49u) != 0u && (EF_U8(result + 0x1eu) & 1u) != 0u) {
        EF_U16(result) = EF_U16(result + 0x4du);
        EF_U32(result + 4u) = EF_U32(result + 0x45u);
        EF_U16(result + 0xcu) = EF_U16(result + 0x4fu);
        EF_U32(result + 0x10u) = EF_U32(result + 0x49u);
        goto done;
    }
    for (i = (int16_t)(EF_U16(0x0041eed8u) - 1u); i >= 0 && hitWidget == 0u; i--) {
        uint16_t id = EF_U16(0x0041eda8u + (uint32_t)(int32_t)i * 2u);
        int32_t x, y, w, h;

        if (id == 0u)
            continue;
        obj = EF_U32(0x0041edfcu + (uint32_t)id * 4u);
        if ((EF_U8(obj + 0x3bu) & 4u) == 0u)
            continue;
        if (EF_U8(obj + 0xc1u) < EF_U8(0x0041f2e0u))
            continue;
        x = EF_S16(obj + 0x41u);
        y = EF_S16(obj + 0x43u);
        w = EF_S16(obj + 0x45u);
        h = EF_S16(obj + 0x47u);
        if (px < x || x + w <= px || py < y || y + h <= py)
            continue;
        if (!(px >= x + 3 || py >= y + 3))
            continue;
        hitWidget = 1u;
        EF_U32(result + 4u) = obj;
        EF_U16(result) = id;
        {
            int32_t e = EF_S16(obj + 0x71u), g = EF_S16(obj + 0x73u);
            int32_t f = EF_S16(obj + 0x75u), hh = EF_S16(obj + 0x77u);

            if (px < x + e || px >= x + e + f)
                continue;
            if (py < y + g || py >= y + g + hh)
                continue;
            hitInner = 1u;
        }
    }
    if (hitWidget == 0u) {
        EF_U16(result) = 0u;
        EF_U32(result + 4u) = 0u;
        EF_U16(result + 0xcu) = 0u;
        EF_U32(result + 0x10u) = 0u;
        goto done;
    }
    EF_U16(result + 0xcu) = 0u;
    EF_U32(result + 0x10u) = 0u;
    slot = 1;
    child = EF_U32(obj + 0x91u);
    while (child != 0u) {
        int32_t cx = EF_S16(child);
        int32_t cy = EF_S16(child + 2u);
        int32_t cw = EF_U16(child + 4u);
        int32_t ch = EF_U16(child + 6u);
        int32_t ox = EF_S16(obj + 0x41u);
        int32_t oy = EF_S16(obj + 0x43u);
        int32_t lo, top;

        if ((EF_U8(child + 0x1du) & 0x40u) == 0u) {
            if ((EF_U8(child + 0x8cu) & 0x1fu) != 0u) {
                lo = ox + cx;
                top = oy + cy;
                if (px >= lo && px < lo + cw && py >= top && py < top + ch) {
                    EF_U32(result + 0x10u) = child;
                    EF_U16(result + 0xcu) = (uint16_t)slot;
                    break;
                }
            } else if (hitInner != 0u) {
                lo = EF_S16(obj + 0x71u) + ox - EF_S16(obj + 0x69u) + cx;
                top = EF_S16(obj + 0x73u) + oy - EF_S16(obj + 0x6bu) + cy;
                if (lo <= px && lo + cw > px && top <= py && top + ch > py) {
                    EF_U32(result + 0x10u) = child;
                    EF_U16(result + 0xcu) = (uint16_t)slot;
                    break;
                }
            }
        }
        child = EF_U32(child + 0x21u);
        slot++;
    }
done:
    EF_U16(0x0041eefeu) = (uint16_t)px;
    EF_U16(0x0041eefcu) = (uint16_t)py;
}

/* Original 0xb1f80 (618 bytes). Dump a record to HANDLE: the record type byte
 * at +0xb (0..2) picks one of three format strings/name tables and prints the
 * header fields, then the trailing records via 0xb21ec / 0xb2a2c. */
void dump_record_b1f80(ef_addr32 rec, uint32_t handle)
{
    uint8_t type = EF_U8(rec + 0xbu);
    uint32_t nameC, nameD, nameE, extra;
    ef_addr32 fmt;

    if (type <= 2u) {
        nameD = EF_U32(0x0013336cu + (uint32_t)EF_U8(rec + 0xdu) * 4u);
        nameE = EF_U32(0x00132f5cu + (uint32_t)EF_U8(rec + 0xeu) * 4u);
        extra = EF_U32(rec + 0xfu);
        if (type == 0u) {
            fmt = 0x0018dee4u;
            nameC = EF_U32(0x00132d34u + (uint32_t)EF_U8(rec + 0xcu) * 4u);
        } else if (type == 1u) {
            fmt = 0x0018df44u;
            nameC = EF_U32(0x001333f8u + (uint32_t)EF_U8(rec + 0xcu) * 4u);
        } else {
            fmt = 0x0018dfa4u;
            nameC = EF_U32(0x00133488u + (uint32_t)EF_U8(rec + 0xcu) * 4u);
        }
        guest_fn_b1bb0(handle, fmt, nameC, nameD, nameE, extra,
                       (uint32_t)(EF_S32(rec + 0x11u) >> 16),
                       (uint32_t)EF_U8(rec + 0x15u), (uint32_t)EF_U8(rec + 0x16u),
                       (uint32_t)(EF_S32(rec + 0x15u) >> 16),
                       (uint32_t)(EF_S32(rec + 0x27u) >> 16), (uint32_t)(EF_S32(rec + 0x29u) >> 16),
                       (uint32_t)(EF_S32(rec + 0x2bu) >> 16), (uint32_t)(EF_S32(rec + 0x2du) >> 16),
                       (uint32_t)(EF_S32(rec + 0x17u) >> 16), (uint32_t)(EF_S32(rec + 0x19u) >> 16),
                       (uint32_t)(EF_S32(rec + 0x1bu) >> 16), (uint32_t)(EF_S32(rec + 0x1du) >> 16),
                       (uint32_t)(EF_S32(rec + 0x1fu) >> 16), (uint32_t)(EF_S32(rec + 0x21u) >> 16),
                       (uint32_t)(EF_S32(rec + 0x23u) >> 16), (uint32_t)(EF_S32(rec + 0x25u) >> 16),
                       (uint32_t)(EF_S32(rec + 0x2fu) >> 16), (uint32_t)(EF_S32(rec + 0x4fu) >> 16));
    }
    print_record_b21ec(EF_U32(rec + 0x4du), handle);
    guest_fn_b1bb0(handle, 0x0018e004u);
    print_coordinate_record_b2a2c(rec + 0x3bu, handle);
}

/* Original 0xbd92c (666 bytes). Try to satisfy every entry (10-byte records at
 * DESC+6, count at DESC+5) of a requirement list against OBJ: for each entry
 * the pattern string (+4) is scanned up to 8 characters or a '%' for the first
 * character whose available amount (OBJ+0x33 minus OBJ+0x37 table) covers the
 * entry's amount (+3) and which 0xbd8c8 accepts; the choice is recorded in two
 * scratch byte tables and charged to OBJ+0x37.  When an entry cannot be met
 * the earlier charges are rolled back and 0 is returned; otherwise the claims
 * are issued through 0xbe208 and 1 is returned.  The early return for an OBJ
 * without +0x32 leaves the upper half of EDX in the result. */
uint32_t reserve_requirements_bd92c(ef_addr32 obj, ef_addr32 desc, ef_addr32 owner, uint16_t cx)
{
    ef_addr32 tableA, tableB;
    int16_t count, i, j;
    uint16_t flag;
    uint16_t claimed;

    if (EF_U8(obj + 0x32u) == 0u)
        return desc & 0xffff0000u;
    count = (int16_t)EF_U8(desc + 5u);
    tableA = guest_fn_cc6c0_x(EF_U8(desc + 5u), 0x0018ec30u);
    tableB = guest_fn_cc6c0_x(EF_U8(desc + 5u), 0x0018ec50u);
    fill_bytes_b3fb0(tableA, 0u, EF_U8(desc + 5u));
    fill_bytes_b3fb0(tableB, 0u, EF_U8(desc + 5u));
    for (i = 0; (int16_t)EF_U8(desc + 5u) > i; i++) {
        ef_addr32 entry = EF_U32(desc + 6u) + (uint32_t)((int32_t)i * 10);

        if (EF_U16(entry) == 0u)
            continue;
        flag = 1u;
        for (j = 0; j < 8 && flag != 0u; j++) {
            uint32_t ch = EF_U8(EF_U32(entry + 4u) + (uint32_t)(int32_t)j);
            uint32_t have, used, need;

            if (ch == 0x25u)
                break;
            have = EF_U8(EF_U32(obj + 0x33u) + ch);
            used = EF_U8(EF_U32(obj + 0x37u) + ch);
            need = EF_U8(entry + 3u);
            if ((int32_t)(have - used) < (int32_t)need)
                continue;
            if (nth_match_exists_bd8c8((uint8_t)ch, (int16_t)need, EF_U8(desc), obj, owner) == 0u)
                continue;
            EF_U8(entry + 8u) = (uint8_t)ch;
            EF_U8(tableA + (uint32_t)(int32_t)i) = (uint8_t)ch;
            EF_U8(tableB + (uint32_t)(int32_t)i) = EF_U8(entry + 3u);
            EF_U8(EF_U32(obj + 0x37u) + ch) = (uint8_t)(EF_U8(EF_U32(obj + 0x37u) + ch) + EF_U8(entry + 3u));
            flag = 0u;
            j++;
            break;
        }
        if (flag != 0u) {
            int16_t k;

            for (k = 0; k < i; k++) {
                if (EF_U16(EF_U32(desc + 6u) + (uint32_t)((int32_t)k * 10)) != 0u) {
                    uint32_t slot = EF_U8(tableA + (uint32_t)(int32_t)k);

                    EF_U8(EF_U32(obj + 0x37u) + slot) =
                        (uint8_t)(EF_U8(EF_U32(obj + 0x37u) + slot) - EF_U8(tableB + (uint32_t)(int32_t)k));
                }
            }
            if (tableA != 0u)
                guest_fn_cc768(tableA);
            if (tableB != 0u)
                guest_fn_cc768(tableB);
            return 0u;
        }
    }
    claimed = 0u;
    for (i = 0; i < (int16_t)EF_U8(desc + 5u); i++) {
        ef_addr32 entry = EF_U32(desc + 6u) + (uint32_t)((int32_t)i * 10);

        if (EF_U16(entry) != 0u) {
            claim_idle_nodes_be208((uint32_t)(int32_t)EF_S16(obj + 9u), (uint32_t)(int32_t)(int16_t)cx,
                                   (uint32_t)(int32_t)(int16_t)claimed, entry, owner);
            claimed++;
        }
    }
    (void)count;
    if (tableA != 0u)
        guest_fn_cc768(tableA);
    if (tableB != 0u)
        guest_fn_cc768(tableB);
    return 1u;
}

/* Original 0xbe7e4 (827 bytes). For each of the CTX+0x12 group records
 * (0x7c bytes each at CTX+0x13) allocate a 22-byte-per-slot record table
 * (+0x69) and a word table (+0x78) sized by the group's count (+1), then walk
 * the node list whose head pointer is stored at HEADPTR and, for every node
 * that matches the group (owner block byte +9 below LIMIT and the key words at
 * +0x19..+0x27 matching the group index/CTX+0x10, or VALUE for CTX+0xa == 2),
 * claim it (+0x17, owner +9 incremented), clone its owner block into the next
 * slot and remember its +0x13 id, until the group's slots are full. */
void collect_group_nodes_be7e4(ef_addr32 ctx, ef_addr32 headPtr, uint32_t value, uint8_t limit)
{
    int16_t j;

    for (j = 0; (int16_t)EF_U8(ctx + 0x12u) > j; j++) {
        ef_addr32 group = EF_U32(ctx + 0x13u) + (uint32_t)((int32_t)j * 0x7c);
        uint32_t m = EF_U8(group + 1u);
        ef_addr32 node, table, ids;
        int16_t k = 0;

        node = 0u;
        node = EF_U32(headPtr);
        EF_U32(group + 0x69u) = guest_fn_cc6c0_z(m * 0x16u, 0x0018ecbcu, m);
        EF_U32(group + 0x78u) = guest_fn_cc6c0_z(m * 2u, 0x0018ecf0u, m);
        if (node == 0u)
            continue;
        while (1) {
            ef_addr32 owner, src, slot;
            uint16_t match = 0u;

            if ((int16_t)EF_U8(group + 1u) <= k)
                break;
            if (EF_U8(node + 0xbu) == 0u) {
                owner = EF_U32(node + 0x4du);
                if (EF_U8(owner + 9u) < limit) {
                    if (EF_U8(ctx + 0xau) == 2u) {
                        if (EF_U16(node + 0x19u) == (uint16_t)value && EF_U16(node + 0x21u) == 0u)
                            match = 1u;
                    } else if (EF_U16(node + 0x19u) == EF_U16(ctx + 0x10u) &&
                               (uint16_t)j == EF_U16(node + 0x21u)) {
                        match = 1u;
                    } else if (EF_U16(node + 0x1bu) == EF_U16(ctx + 0x10u) &&
                               (uint16_t)j == EF_U16(node + 0x23u)) {
                        match = 1u;
                    } else if (EF_U16(node + 0x1du) == EF_U16(ctx + 0x10u) &&
                               (uint16_t)j == EF_U16(node + 0x25u)) {
                        match = 1u;
                    } else if (EF_U16(node + 0x1fu) == EF_U16(ctx + 0x10u) &&
                               (uint16_t)j == EF_U16(node + 0x27u)) {
                        match = 1u;
                    }
                }
            }
            if (match != 0u) {
                uint32_t copy;

                EF_U16(node + 0x17u) = (uint16_t)(EF_U16(node + 0x17u) + 1u);
                owner = EF_U32(node + 0x4du);
                EF_U8(owner + 9u) = (uint8_t)(EF_U8(owner + 9u) + 1u);
                copy = alloc_checked_cc744(EF_U32(EF_U32(node + 0x4du)));
                table = EF_U32(group + 0x69u);
                slot = table + (uint32_t)((int32_t)k * 0x16);
                EF_U32(slot) = copy;
                src = EF_U32(node + 0x4du);
                EF_U8(slot + 4u) = EF_U8(src + 4u);
                EF_U8(slot + 5u) = EF_U8(src + 5u);
                EF_U8(slot + 6u) = EF_U8(src + 6u);
                EF_U8(slot + 7u) = EF_U8(src + 7u);
                EF_U8(slot + 8u) = EF_U8(src + 8u);
                EF_U16(slot + 0xau) = EF_U16(src + 0xau);
                EF_U8(slot + 9u) = EF_U8(src + 9u);
                EF_U16(slot + 0xcu) = EF_U16(src + 0xcu);
                EF_U16(slot + 0xeu) = EF_U16(src + 0xeu);
                EF_U16(slot + 0x10u) = EF_U16(src + 0x10u);
                EF_U16(slot + 0x12u) = EF_U16(src + 0x12u);
                EF_U16(slot + 0x14u) = EF_U16(src + 0x14u);
                ids = EF_U32(group + 0x78u);
                EF_U16(ids + (uint32_t)((int32_t)k * 2)) = EF_U16(node + 0x13u);
                k++;
            }
            node = EF_U32(node + 4u);
            if (node == 0u)
                break;
        }
    }
}

/* Original 0xccdbc (882 bytes). Gather up to COUNT matching nodes from the
 * list whose head pointer is *HEADPTR (type B, unclaimed, same 64-bit key as
 * KEY, kind not 1/2 and pair-matching), claim them for request A, then create
 * the request object (type 2/0x14, 0x7c-byte group, 0x19-byte detail record),
 * insert it into the list and return A + 1. */
uint32_t build_group_request_ccdbc(uint32_t a, uint8_t b, uint8_t count, ef_addr32 key,
                                   ef_addr32 owner, uint32_t arg1, ef_addr32 headPtr,
                                   uint32_t arg3, uint32_t arg4)
{
    ef_addr32 table, node, req, grp, detail;
    int16_t n = 0;
    int16_t i;

    (void)arg3;
    table = guest_fn_cc6c0_x((uint32_t)count * 2u, 0x0018f454u);
    node = EF_U32(headPtr);
    while (node != 0u && n < (int16_t)count) {
        if (EF_U8(node + 0xcu) == b && EF_U16(node + 9u) == 0u &&
            EF_U32(node + 0x3bu) == EF_U32(key) && EF_U32(node + 0x3fu) == EF_U32(key + 4u) &&
            EF_U8(node + 0xdu) != 1u && EF_U8(node + 0xdu) != 2u &&
            ((uint16_t)pair_matches_cc7e8(EF_U8(node + 0xbu), EF_U8(node + 0xcu)) != 0u ||
             (uint16_t)pair_matches_cc81c(EF_U8(node + 0xbu), EF_U8(node + 0xcu)) != 0u)) {
            EF_U16(node + 0x21u) = 0u;
            EF_U16(node + 9u) = 1u;
            EF_U16(node + 0x19u) = (uint16_t)a;
            EF_U16(owner + 0x32u) = (uint16_t)(EF_U16(owner + 0x32u) - 1u);
            EF_U16(table + (uint32_t)((int32_t)n * 2)) = EF_U16(node + 0x13u);
            n++;
        }
        node = EF_U32(node + 4u);
    }

    req = guest_fn_cc6c0_z(0x1bu, 0x0018f484u, 0x1bu);
    EF_U8(req + 9u) = 2u;
    EF_U8(req + 0xau) = 0x14u;
    EF_U8(req + 0xcu) = 0u;
    EF_U16(req + 0xeu) = 0xffffu;
    EF_U8(req + 0x12u) = 1u;
    EF_U16(req + 0x17u) = 0u;
    EF_U16(req + 0x19u) = 0u;
    EF_U8(req + 0xbu) = EF_U8(owner + 9u);
    EF_U16(req + 0x10u) = (uint16_t)a;
    grp = guest_fn_cc6c0_z(0x7cu, 0x0018f4c0u, 0x7cu);
    EF_U32(req + 0x13u) = grp;
    EF_U8(grp) = b;
    EF_U8(grp + 1u) = count;
    EF_U8(grp + 2u) = 3u;
    EF_U8(grp + 3u) = 0u;
    EF_U32(grp + 8u) = EF_U32(owner + 0x16u);
    EF_U32(grp + 0xcu) = EF_U32(owner + 0x1au);
    EF_U32(grp + 0x10u) = EF_U32(owner + 0x1eu);
    EF_U16(grp + 0x14u) = EF_U16(key + 0xcu);
    EF_U16(grp + 0x16u) = EF_U16(key + 0xeu);
    EF_U16(grp + 0x18u) = EF_U16(key + 0x10u);
    EF_U8(grp + 0x3au) = 0xffu;
    EF_U32(grp + 0x3bu) = 0u;
    fill_bytes_b3fb0(grp + 0x3fu, 0u, 0x12u);
    EF_U32(grp + 0x69u) = 0u;
    EF_U32(grp + 0x51u) = 0u;
    fill_bytes_b3fb0(grp + 0x55u, 0u, 0x14u);
    EF_U32(grp + 0x74u) = 0u;
    EF_U8(grp + 0x6du) = 0xffu;
    EF_U16(grp + 0x6eu) = 0xffffu;
    EF_U16(grp + 0x70u) = 0xffffu;
    EF_U16(grp + 0x72u) = 1u;
    EF_U32(EF_U32(req + 0x13u) + 0x78u) = guest_fn_cc6c0_z((uint32_t)count * 2u, 0x0018f4f8u, 2u);
    for (i = 0; i < (int16_t)count; i++)
        EF_U16(EF_U32(EF_U32(req + 0x13u) + 0x78u) + (uint32_t)((int32_t)i * 2)) =
            EF_U16(table + (uint32_t)((int32_t)i * 2));
    grp = EF_U32(req + 0x13u);
    EF_U16(grp + 0x1au) = 0u;
    EF_U16(grp + 0x1cu) = 1u;
    EF_U16(grp + 0x1eu) = 0u;
    detail = guest_fn_cc6c0_z(0x19u, 0x0018f52cu, 0x19u);
    EF_U32(EF_U32(req + 0x13u) + 0x20u) = detail;
    EF_U32(detail) = EF_U32(owner + 0x16u);
    EF_U32(detail + 4u) = EF_U32(owner + 0x1au);
    EF_U16(detail + 8u) = EF_U16(key + 0xcu);
    EF_U16(detail + 0xau) = EF_U16(key + 0xeu);
    EF_U16(detail + 0xcu) = 0u;
    EF_U16(detail + 0xeu) = 0u;
    EF_U16(detail + 0x10u) = EF_U16(0x001379a0u);
    EF_U16(detail + 0x12u) = EF_U16(0x001379a2u);
    EF_U16(detail + 0x14u) = EF_U16(0x001379a4u);
    EF_U16(detail + 0x16u) = 0u;
    EF_U8(detail + 0x18u) =
        ((uint16_t)node_has_negative_flag_cc468(EF_U8(owner + 9u), arg4) != 0u) ? 0xfu : 0xeu;
    guest_fn_bc570_x(req, arg1, headPtr);
    a++;
    if (table != 0u)
        guest_fn_cc768(table);
    return a;
}

/* Original 0xcff80 (728 bytes). Intersect the segment SEG (two 3-vectors at
 * +0 and +0xc) with COUNT 0x4a-byte planes at PLANES; the nearest hit to the
 * segment start (approximate distance) is stored at 0x4208f0..0x4208f8.
 * Returns whether any plane was hit. */
uint32_t nearest_segment_hit_cff80(ef_addr32 seg, uint32_t count, ef_addr32 planes)
{
    int32_t org[3], dir[3], out[3];
    int32_t best = 0x7fffffff;
    int32_t lo, hi, a, b;
    ef_addr32 plane = planes;
    int32_t s0, s1, s2, e0, e1, e2;

    if (count == 0u)
        return 0u;
    s0 = (int32_t)EF_U32(seg);
    s1 = (int32_t)EF_U32(seg + 4u);
    s2 = (int32_t)EF_U32(seg + 8u);
    e0 = (int32_t)EF_U32(seg + 0xcu);
    e1 = (int32_t)EF_U32(seg + 0x10u);
    e2 = (int32_t)EF_U32(seg + 0x14u);
    lo = (s1 >= e1) ? e1 : s1;
    hi = (s1 > e1) ? s1 : e1;
    a = forward_triple_cff64(seg);
    b = forward_triple_cff64(seg + 0xcu);
    if (a > b) {
        org[0] = s0; org[1] = s1; org[2] = s2;
        dir[0] = e0 - s0; dir[1] = e1 - s1; dir[2] = e2 - s2;
    } else {
        org[0] = e0; org[1] = e1; org[2] = e2;
        dir[0] = s0 - e0; dir[1] = s1 - e1; dir[2] = s2 - e2;
    }
    EF_U8(0x0015b7c9u) = 0xfu;
    EF_U8(0x0015b7c8u) = 0xfu;
    if (forward_triple_cff64((ef_addr32)(uintptr_t)dir) < 0x64) {
        EF_U8(0x0015b7c9u) = 8u;
        EF_U8(0x0015b7c8u) = 7u;
    }
    do {
        int32_t p18 = (int32_t)EF_U32(plane + 0x18u) >> 16;
        int32_t p1a = (int32_t)EF_U32(plane + 0x1au) >> 16;

        if (p1a > lo && p18 < hi) {
            int32_t p10 = (int32_t)EF_U32(plane + 0x10u) >> 16;
            int32_t p12 = (int32_t)EF_U32(plane + 0x12u) >> 16;
            int32_t p16 = (int32_t)EF_U32(plane + 0x16u) >> 16;
            int32_t p14 = (int32_t)EF_U32(plane + 0x14u) >> 16;
            uint32_t m1 = (uint32_t)(p10 > s0) | ((uint32_t)(p12 < s0) << 1) |
                          ((uint32_t)(p16 < s2) << 2) | ((uint32_t)(p14 > s2) << 3);
            uint32_t m2 = (uint32_t)(p10 > e0) | ((uint32_t)(p12 < e0) << 1) |
                          ((uint32_t)(p16 < e2) << 2) | ((uint32_t)(p14 > e2) << 3);

            if ((m1 & m2) == 0u) {
                uint32_t h28 = (uint32_t)((int32_t)EF_U32(plane + 0x28u) >> 16);
                uint32_t h2a = (uint32_t)((int32_t)EF_U32(plane + 0x2au) >> 16);
                uint32_t h2c = (uint32_t)((int32_t)EF_U32(plane + 0x2cu) >> 16);
                uint32_t h2e = (uint32_t)((int32_t)EF_U32(plane + 0x2eu) >> 16);
                uint32_t k = h2e << 15;
                uint32_t d1 = (uint32_t)s0 * h28 + (uint32_t)s1 * h2a + (uint32_t)s2 * h2c + k;
                uint32_t d2 = (uint32_t)e0 * h28 + (uint32_t)e1 * h2a + (uint32_t)e2 * h2c + k;

                if ((int32_t)(d1 ^ d2) < 0 &&
                    ray_hits_plane_cfde4((ef_addr32)(uintptr_t)out, (ef_addr32)(uintptr_t)dir,
                                         (ef_addr32)(uintptr_t)org, plane) != 0u) {
                    int32_t dist = (int16_t)approx_distance3_16_b3830(
                        (int16_t)(out[0] - (int16_t)s0), (int16_t)(out[1] - (int16_t)s1),
                        (int16_t)((uint16_t)out[2] - (uint16_t)s2));

                    if (dist < best) {
                        best = dist;
                        EF_U32(0x004208f0u) = (uint32_t)out[0];
                        EF_U32(0x004208f4u) = (uint32_t)out[1];
                        EF_U32(0x004208f8u) = (uint32_t)out[2];
                    }
                }
            }
        }
        plane += 0x4au;
    } while (--count != 0u);
    return best != 0x7fffffff;
}

/* Original 0xe1e6c (1257 bytes). Word-wrap the NUL-terminated text TEXT for
 * font FONT into the line-node chain hanging off OBJ+0xc7 (each 0x12-byte node:
 * +0 x, +2 y, +4 char count, +6 text start, +0xa next, +0xe prev), handling
 * spaces, newlines (0xd) and tabs (9), then flag the object and redraw it. */
#define E1E6C_NEXT_NODE(node, obj)                                         \
    do {                                                                   \
        ef_addr32 next_ = EF_U32((node) + 0xau);                           \
        if (next_ != 0u) {                                                 \
            (node) = next_;                                                \
        } else {                                                           \
            next_ = alloc_zeroed_ab840(0x12u);                             \
            if (next_ == 0u)                                               \
                fatal_with_arg_e1948(obj);                                 \
            EF_U32((node) + 0xau) = next_;                                 \
            (node) = next_;                                                \
        }                                                                  \
    } while (0)

void wrap_text_lines_e1e6c(ef_addr32 obj, ef_addr32 text, uint8_t font)
{
    ef_addr32 node, old;
    uint16_t cdx, x0, xs, y, n, cnt9, savedX, savedN;
    int16_t maxX;
    ef_addr32 savedT;
    uint32_t t10, c;
    int32_t xv;

    if (obj == 0u || font >= 10u || EF_U8(0x00419adau + ((uint32_t)font << 11)) == 0u)
        return;
    t10 = (uint32_t)font * 10u;
    EF_U8(obj + 0xcbu) = (uint8_t)((EF_U8(obj + 0xcbu) & 0xe1u) | (uint8_t)(((uint32_t)font & 0xfu) * 2u));
    if (text == 0u) {
        EF_U8(obj + 0xcbu) = (uint8_t)(EF_U8(obj + 0xcbu) & 0xfeu);
        if (EF_U8(0x0041f2e5u) != 0u)
            return;
        guest_fn_a5ef8(EF_U8(obj + 0x10u));
        return;
    }
    node = EF_U32(obj + 0xc7u);
    if (node == 0u) {
        node = alloc_zeroed_ab840(0x12u);
        if (node == 0u)
            fatal_with_arg_e1948(obj);
        EF_U32(obj + 0xc7u) = node;
    }
    maxX = (int16_t)(EF_U16(obj + 0x65u) - EF_U16(obj + 0xd0u));
    x0 = EF_U16(obj + 0xd2u);
    cdx = x0;
    xs = x0;
    y = EF_U16(obj + 0xd4u);
    EF_U32(node + 6u) = text;
    EF_U16(node) = cdx;
    EF_U16(node + 2u) = y;
    n = 0u;
    cnt9 = 0u;
    savedT = 0u;
    savedX = 0u;
    savedN = 0u;
    if (EF_U8(text) == 0u)
        goto post;
main_loop:
    c = EF_U8(text);
    if (c == 0x20u) {
        cdx = (uint16_t)(cdx + EF_U8(0x001378d9u + t10));
        if ((int16_t)cdx < maxX) {
            text++;
            if (n == 0u) {
                EF_U16(node) = cdx;
                EF_U32(node + 6u) = text;
                goto check;
            }
            n++;
            goto check;
        }
        text++;
        goto check;
    }
    if (c == 0xdu) {
        if (n != 0u) {
            old = node;
            E1E6C_NEXT_NODE(node, obj);
            EF_U16(old + 4u) = n;
        }
        text++;
        n = 0u;
        cnt9 = 0u;
        EF_U16(node) = x0;
        y = (uint16_t)(y + EF_U8(0x001378dbu + t10));
        EF_U32(node + 6u) = text;
        xs = x0;
        cdx = x0;
        EF_U16(node + 2u) = y;
        goto check;
    }
    if (c == 9u) {
        if (cnt9 == 0u) {
            int32_t lim = (int32_t)(int16_t)EF_U16(obj + 0xd8u) + (int32_t)(int16_t)x0;

            if (lim > (int32_t)(int16_t)cdx)
                cdx = (uint16_t)(x0 + EF_U16(obj + 0xd8u));
        } else {
            int32_t tw = (int32_t)EF_U8(0x001378dcu + t10);
            int32_t q = ((int32_t)(int16_t)cdx - (int32_t)(int16_t)x0) / tw;

            cdx = (uint16_t)((q + 1) * tw + (int32_t)(int16_t)x0);
        }
        cnt9++;
        if ((int16_t)cdx >= maxX) {
            text++;
            goto check;
        }
        if (n == 0u) {
            text++;
            EF_U16(node) = cdx;
            EF_U32(node + 6u) = text;
            goto check;
        }
        old = node;
        EF_U16(node + 4u) = n;
        E1E6C_NEXT_NODE(node, obj);
        EF_U16(node) = cdx;
        text++;
        EF_U16(node + 2u) = y;
        n = 0u;
        EF_U32(node + 6u) = text;
        goto check;
    }
    if (EF_U8(0x004198d2u + ((uint32_t)font << 11) + c * 8u) == 0u) {
        if (EF_U8(text) != 0u) {
            text++;
            n++;
        }
        goto check;
    }
    savedX = cdx;
    savedT = text;
    savedN = n;
    if (EF_U8(text) == 0u)
        goto check;
inner:
    c = EF_U8(text);
    if (EF_U8(0x004198d2u + ((uint32_t)font << 11) + c * 8u) == 0u)
        goto check;
    cdx = (uint16_t)(cdx + EF_U8(0x004198d2u + ((uint32_t)font << 11) + c * 8u) +
                     EF_U8(0x001378dau + t10));
    if ((int16_t)cdx < maxX) {
        text++;
        n++;
        goto inner_tail;
    }
    if ((int32_t)(int16_t)savedX == (int32_t)xs) {
        EF_U16(node + 4u) = n;
        E1E6C_NEXT_NODE(node, obj);
        xv = (int32_t)(int16_t)x0;
        if ((int32_t)(int16_t)EF_U16(obj + 0xd6u) + xv < (int32_t)maxX - 0xc)
            xv += (int32_t)(int16_t)EF_U16(obj + 0xd6u);
        cdx = (uint16_t)xv;
        xs = (uint16_t)xv;
        y = (uint16_t)(y + EF_U8(0x001378dbu + t10));
        savedX = cdx;
        savedT = text;
        EF_U16(node) = cdx;
        EF_U16(node + 2u) = y;
        n = 0u;
        EF_U32(node + 6u) = text;
        savedN = 0u;
        if (EF_U8(text) != 0u)
            goto inner;
        goto check;
    }
    if (savedN != 0u) {
        old = node;
        EF_U16(node + 4u) = savedN;
        E1E6C_NEXT_NODE(node, obj);
        (void)old;
    }
    xv = (int32_t)(int16_t)x0;
    if ((int32_t)(int16_t)EF_U16(obj + 0xd6u) + xv < (int32_t)maxX - 0xc)
        xv += (int32_t)(int16_t)EF_U16(obj + 0xd6u);
    cdx = (uint16_t)xv;
    xs = (uint16_t)xv;
    y = (uint16_t)(y + EF_U8(0x001378dbu + t10));
    savedX = cdx;
    text = savedT;
    n = 0u;
    EF_U16(node + 2u) = y;
    EF_U16(node) = cdx;
    EF_U32(node + 6u) = text;
    savedN = 0u;
inner_tail:
    if (EF_U8(text) != 0u)
        goto inner;
check:
    if (EF_U8(text) != 0u)
        goto main_loop;
post:
    if (n != 0u) {
        old = node;
        EF_U16(node + 4u) = n;
        E1E6C_NEXT_NODE(node, obj);
        (void)old;
    }
    EF_U16(node) = 0u;
    EF_U16(node + 2u) = 0u;
    EF_U16(node + 4u) = 0u;
    EF_U8(obj + 0xcbu) = (uint8_t)(EF_U8(obj + 0xcbu) | 1u);
    old = EF_U32(obj + 0xc7u);
    node = EF_U32(old + 0xau);
    EF_U32(old + 0xeu) = 0u;
    while (node != 0u) {
        EF_U32(node + 0xeu) = old;
        old = node;
        node = EF_U32(node + 0xau);
    }
    if (EF_U8(0x0041f2e5u) != 0u)
        return;
    guest_fn_a5ef8(EF_U8(obj + 0x10u));
}

#define E52A4_S16(a) ((int16_t)EF_U16(a))
#define E52A4_X(v) ((uint32_t)(int32_t)(int16_t)(v))

/* Original 0xe52a4 (1417 bytes). Clip the pending screen rectangle at
 * 0x41ec08 (x, y, w, h words), then test it against every overlapping
 * window of the list after window ID's slot: fully covered -> returns 2,
 * partly covered windows get their border dirty rects added (and, with
 * RECORD set, the overlap rectangles are collected at 0x4339a0). Returns 1
 * when any overlap was found, else 0. */
uint8_t clip_against_windows_e52a4(uint32_t id, uint8_t record)
{
    uint16_t di;
    ef_addr32 win;
    int16_t rx, ry, rw, rh;
    int16_t px, py, pw, ph;
    int16_t g4, g6, g8, ga, bx, cx, dx;
    uint16_t lim_c, lim_8, lim_10;
    uint8_t slot;

    EF_U8(0x00433eaeu) = 0u;
    if (record != 0u) {
        EF_U16(0x00433eacu) = 0u;
        fill_bytes_b3fb0(0x004339a0u, 0u, 0x500u);
    }
    rx = E52A4_S16(0x0041ec08u);
    ry = E52A4_S16(0x0041ec0au);
    rw = E52A4_S16(0x0041ec0cu);
    rh = E52A4_S16(0x0041ec0eu);
    if ((int32_t)rw + rx <= 0 || (int32_t)ry + rh <= 0 ||
        (int32_t)rx >= (int32_t)EF_U32(0x0015b744u) || (int32_t)ry >= (int32_t)EF_U32(0x0015b748u) ||
        rw <= 0 || rh <= 0)
        return 2u;
    clip_rect_to_screen_e58a8();
    slot = (uint8_t)find_id_a4640((uint16_t)id);
    if (EF_U16(0x0041edaau + (uint32_t)slot * 2u) == 0u)
        return 0u;
    di = (uint16_t)((uint16_t)slot + 1u);
    EF_U32(0x00433ea0u) = 0u;
    if (di >= EF_U16(0x0041eed8u))
        return EF_U8(0x00433eaeu);
    for (;;) {
        uint16_t entry = EF_U16(0x0041eda8u + (uint32_t)di * 2u);

        if (entry == 0u)
            break;
        win = EF_U32(0x0041edfcu + (uint32_t)entry * 4u);
        if (win == 0u)
            break;
        /* re-read the (possibly clipped) rectangle for every window */
        rx = E52A4_S16(0x0041ec08u);
        ry = E52A4_S16(0x0041ec0au);
        rw = E52A4_S16(0x0041ec0cu);
        rh = E52A4_S16(0x0041ec0eu);
        if (EF_U8(win + 0x10u) != (uint16_t)id) {
            px = E52A4_S16(win + 0x41u);
            py = E52A4_S16(win + 0x43u);
            pw = E52A4_S16(win + 0x45u);
            ph = E52A4_S16(win + 0x47u);
            if ((int32_t)px < (int32_t)rx + rw && (int32_t)py < (int32_t)ry + rh &&
                (int32_t)px + pw > (int32_t)rx && (int32_t)py + ph > (int32_t)ry) {
                g6 = (px > rx) ? px : rx;
                ga = (py > ry) ? py : ry;
                g4 = (int16_t)(((int32_t)px + pw < (int32_t)rx + rw) ? (int32_t)px + pw : (int32_t)rx + rw);
                g8 = (int16_t)(((int32_t)py + ph < (int32_t)ry + rh) ? (int32_t)py + ph : (int32_t)ry + rh);
                EF_U16(0x00433ea6u) = (uint16_t)g6;
                EF_U16(0x00433eaau) = (uint16_t)ga;
                EF_U16(0x00433ea4u) = (uint16_t)g4;
                EF_U16(0x00433ea8u) = (uint16_t)g8;
                lim_c = (uint16_t)(px + 2);
                cx = (int16_t)(py + 1);
                if (EF_U16(win + 0xc5u) != 0u) {
                    bx = (int16_t)(px + EF_U16(win + 0xc5u) + 4);
                    dx = (int16_t)(py + 0x11);
                } else {
                    bx = (int16_t)(px + pw);
                    dx = cx;
                }
                lim_8 = (uint16_t)(py + ph - 3);
                lim_10 = (uint16_t)(px + pw - 3);
                if (!(g4 > (int16_t)lim_c) && cx >= g8)
                    goto next;
                if (!(bx > g6) && dx >= g8)
                    goto next;
                if (!(g6 < (int16_t)lim_10) && ga >= (int16_t)lim_8)
                    goto next;
                if ((g6 > (int16_t)lim_c || cx < ga) && (bx > g4 || dx < ga) &&
                    ((int16_t)lim_10 > g4 || (int16_t)lim_8 > g8)) {
                    if (g6 == E52A4_S16(0x0041ec08u) && ga == E52A4_S16(0x0041ec0au) &&
                        (int32_t)(int16_t)(g4 - g6) == (int32_t)E52A4_S16(0x0041ec0cu) &&
                        (int32_t)(int16_t)(g8 - ga) == (int32_t)E52A4_S16(0x0041ec0eu))
                        return 2u;
                    if (record != 0u) {
                        uint32_t n = EF_U32(0x00433ea0u);
                        uint16_t cnt = (uint16_t)(EF_U16(0x00433eacu) + 1u);

                        EF_U16(0x004339a0u + n * 8u) = (uint16_t)g6;
                        EF_U16(0x004339a2u + n * 8u) = (uint16_t)ga;
                        EF_U16(0x004339a4u + n * 8u) = (uint16_t)(g4 - g6);
                        EF_U16(0x004339a6u + n * 8u) = (uint16_t)(g8 - ga);
                        EF_U32(0x00433ea0u) = n + 1u;
                        EF_U16(0x00433eacu) = cnt;
                    }
                    EF_U8(0x00433eaeu) = 1u;
                    goto next;
                }
                if (EF_U16(win + 0xc5u) != 0u) {
                    add_dirty_rect_e51c4(E52A4_X(px + 3), E52A4_X(py),
                                         E52A4_X(px + EF_U16(win + 0xc5u) + 4),
                                         E52A4_X(py + 3), record);
                    add_dirty_rect_e51c4(E52A4_X(px), E52A4_X(py + 3),
                                         E52A4_X(px + 3 + EF_U16(win + 0xc5u) + 1),
                                         E52A4_X(py + 0x12), record);
                    add_dirty_rect_e51c4(E52A4_X(px), E52A4_X(py + 0x12), E52A4_X(px + pw),
                                         E52A4_X(py + ph - 3), record);
                } else {
                    add_dirty_rect_e51c4(E52A4_X(px + 3), E52A4_X(py), E52A4_X(px + pw),
                                         E52A4_X(py + 3), record);
                    add_dirty_rect_e51c4(E52A4_X(px), E52A4_X(py + 3), E52A4_X(px + pw),
                                         E52A4_X(py + ph - 3), record);
                }
                add_dirty_rect_e51c4(E52A4_X(px), E52A4_X(py + ph - 3), E52A4_X(px + pw - 3),
                                     E52A4_X(py + ph), record);
            }
        }
next:
        di++;
        if (di >= EF_U16(0x0041eed8u))
            break;
    }
    return EF_U8(0x00433eaeu);
}

/* Original 0x91afc (259 bytes). Look for the highest 5-byte record (index
 * 0x27 down to 0) whose enable word is set and whose threshold exceeds the
 * dword at 0x214448; apply view angles (YAW, 0, PITCH) and rotation patch for
 * it, reset the angle words, call 0x9e0c4 with its value and flush the
 * queue. When no record qualifies fall back to 0xd86e0(YAW, PITCH). */
void select_view_angles_91afc(uint32_t yaw, uint32_t pitch)
{
    int16_t i;
    uint16_t id = (uint16_t)pitch;

    guest_fn_c4105_x();
    for (i = 0x27; i >= 0; i--) {
        uint32_t rec = (uint32_t)((int32_t)i * 5);

        if (EF_U16(0x0042239bu + rec) == 0u)
            continue;
        if ((int32_t)(int16_t)EF_U16(0x00422398u + rec) <= (int32_t)EF_U32(0x00214448u))
            continue;
        {
            int32_t v = ((int16_t)yaw >= 0) ? -(int32_t)(int16_t)yaw : (int32_t)(int16_t)yaw;
            uint16_t a0 = id;
            int32_t a1 = (int32_t)yaw;

            v = (int16_t)v;
            if (v <= -0x4000) {
                a1 = (int32_t)(0x8000u - yaw);
                a0 = (uint16_t)(a0 - 0x8000u);
            }
            guest_fn_dc519_x((uint32_t)(int32_t)(int16_t)a1, (uint32_t)(int32_t)(int16_t)a0);
        }
        set_scene_angles((int16_t)yaw, 0, (int16_t)id);
        EF_U32(0x000c4756u) = 0u;
        EF_U32(0x000c475cu) = 0u;
        EF_U32(0x000c4762u) = 0u;
        EF_U16(0x00159094u) = 0u;
        EF_U16(0x00159098u) = 0u;
        EF_U16(0x0015909cu) = 0u;
        guest_fn_9e0c4((int32_t)(int16_t)EF_U16(0x0042239bu + rec), 0u, 0u, 0u);
        guest_fn_c4167_x();
        guest_fn_c4105_x();
        return;
    }
    guest_fn_d86e0_x((uint16_t)yaw, id);
}

/* Original 0x39744 (303 bytes). Centre the 3D view on the screen: set the
 * clip window around the screen centre, load the camera angles (A, B-0xc8
 * shifted, 11-bit C/D) into the renderer state, sample one pixel at the
 * centre via 0xc3f50, restore the full-screen clip window and return the
 * sampled byte. */
uint8_t centre_view_sample_39744(int16_t a, uint16_t d, int16_t b, uint16_t c)
{
    int32_t half_w, half_h, cx, cy;
    uint8_t sample;
    uint32_t v;

    d = (uint16_t)(d & 0x7ffu);
    c = (uint16_t)(c & 0x7ffu);
    copy_constants_c469b();
    half_w = (int32_t)EF_U32(0x0015b744u) / 2;
    half_h = (int32_t)EF_U32(0x0015b748u) / 2;
    cx = (int16_t)half_w;
    cy = (int16_t)half_h;
    guest_fn_c4105_x();
    guest_fn_ce328_w((int32_t)(cx - 2), (int32_t)(cx + 2), (int32_t)(cy - 2), (int32_t)(cy + 2));
    set_params_and_run_c4654(0u, 0u, 0u, 0u, 0u, 0u, 0xc000u);
    EF_U32(0x000c4756u) = (uint32_t)(((uint32_t)d - 0x400u) << 16);
    EF_U32(0x000c475cu) = (uint32_t)(((int32_t)b - 0xc8) << 16);
    EF_U32(0x000c4762u) = (uint32_t)(((uint32_t)c - 0x400u) << 16);
    v = EF_U32(0x000c4756u);
    EF_U16(0x00159094u) = (uint16_t)((int32_t)v >> 16);
    v = EF_U32(0x000c475cu);
    EF_U16(0x00159098u) = (uint16_t)((int32_t)v >> 16);
    v = EF_U32(0x000c4762u);
    EF_U16(0x0015909cu) = (uint16_t)((int32_t)v >> 16);
    guest_fn_9e0c4((int32_t)a, 0u, 0u, 0u);
    guest_fn_c4167_x();
    sample = (uint8_t)guest_fn_c3f50(cx, cy);
    guest_fn_ce328_w(0, (int32_t)EF_U32(0x0015b744u) - 1, 0, (int32_t)EF_U32(0x0015b748u) - 1);
    load_origin_and_run_c46bc();
    return sample;
}

/* Original 0xc630 (311 bytes). Fast-forward the simulation by the minutes
 * elapsed since the stored clock: for every elapsed step run the three
 * per-table update loops, then rebuild the derived state. */
void replay_elapsed_steps_c630(void)
{
    int32_t steps, step;
    int16_t i, count;
    int32_t secs;

    secs = clock_words_to_seconds(0x0012583cu);
    steps = (int32_t)EF_U32(0x0019766cu) - secs / 60;
    if (steps > 0) {
        store_runtime_control_word(0x0bb8u);
        for (step = 0; (int16_t)step < steps; step++) {
            i = 0;
            if ((int16_t)EF_U16(0x0019bb5cu) > 0) {
                do {
                    guest_fn_2e740_x((int32_t)i, 60.0, 1u);
                    i++;
                    if (i == 0xff) i++;
                } while (i < (int16_t)EF_U16(0x0019bb5cu));
            }
            i = 0;
            if ((int16_t)EF_U16(0x001a1506u) > 0) {
                do {
                    guest_fn_37eac_x((int32_t)i, 60.0, 1u);
                    i++;
                    if (i == 0xff) i++;
                } while (i < (int16_t)EF_U16(0x001a1506u));
            }
            i = 0;
            if ((int16_t)EF_U16(0x001972e6u) > 0) {
                do {
                    guest_fn_8f4c((uint32_t)(uint8_t)i);
                    count = (int16_t)EF_U16(0x001972e6u);
                    i++;
                } while (i < count);
            }
            i = 0;
            while (i < (int32_t)EF_U8(0x0019726au)) {
                int32_t off = (int32_t)i * 5;
                int32_t v = (int16_t)EF_U16(EF_U32(0x00197260u) + (uint32_t)off + 1u);
                i++;
                guest_fn_759c(v);
            }
            guest_fn_407c4_x(0x0012583cu, 0x0bb8);
        }
        store_runtime_control_word(0u);
    }
    guest_fn_d290_w();
    clear_word_tables_92f0();
    guest_fn_d68c_x();
}

/* Original 0x3d7b0 (338 bytes). Draw up to nine clipped line segments of a
 * 3D shape outline (record a, table d) in colour b. The original keeps the
 * clip results in an unaligned stack buffer; `loc` mirrors that frame. */
static int32_t loc_rd32_3d7b0(const uint8_t *loc, int off)
{
    uint32_t v = (uint32_t)loc[off] | ((uint32_t)loc[off + 1] << 8) |
                 ((uint32_t)loc[off + 2] << 16) | ((uint32_t)loc[off + 3] << 24);
    return (int32_t)v;
}

static void loc_wr32_3d7b0(uint8_t *loc, int off, uint32_t v)
{
    loc[off] = (uint8_t)v;
    loc[off + 1] = (uint8_t)(v >> 8);
    loc[off + 2] = (uint8_t)(v >> 16);
    loc[off + 3] = (uint8_t)(v >> 24);
}

void draw_outline_segments_3d7b0(uint8_t a, uint8_t d, uint16_t b)
{
    uint8_t loc[0x28];
    int16_t i;
    int w, h;

    for (w = 0; w < (int)sizeof(loc); w++) loc[w] = 0;
    h = (int)EF_U32(0x0015b748u);
    w = (int)EF_U32(0x0015b744u);
    guest_fn_ce328_w(0, w - 1, 0, h - 1);
    h = (int)EF_U32(0x0015b748u);
    w = (int)EF_U32(0x0015b744u);
    guest_fn_cdf48(0u, (uint32_t)(w - 1), 0u, (uint32_t)(h - 1));
    for (i = 0; i < 9; i++) {
        uint32_t rec = EF_U32(0x001972e0u) + (uint32_t)a * 0x22u;
        uint32_t p = EF_U32(rec + 0x1au) + (uint32_t)d * 0x2au + (uint32_t)((int32_t)i * 4);
        int32_t t8, t12, x0, y0, e4, e10, c0, cl;
        uint32_t p6;

        t8 = (int32_t)(EF_U32(rec + 8u) << 11);
        x0 = ((int32_t)EF_U32(p + 2u) >> 16) + t8;
        p6 = EF_U32(p + 6u);
        t12 = (int32_t)(EF_U32(rec + 0xcu) << 11);
        c0 = (int32_t)EF_U32(p);
        y0 = ((int32_t)EF_U32(p + 4u) >> 16) + t12;
        loc_wr32_3d7b0(loc, 8, 0u);
        loc_wr32_3d7b0(loc, 0xc, 0u);
        loc_wr32_3d7b0(loc, 0x14, (uint32_t)(y0 + 0x400));
        e10 = t8 + ((int32_t)p6 >> 16) + 0x400;
        cl = (c0 >> 16) + t12 + 0x400;
        loc_wr32_3d7b0(loc, 0x10, (uint32_t)e10);
        if (guest_fn_15c0c4_x((uint32_t)(int32_t)(int16_t)cl,
                              (uint32_t)(loc_rd32_3d7b0(loc, 6) >> 16),
                              (uint32_t)(int32_t)(int16_t)(x0 + 0x400),
                              (ef_addr32)(uintptr_t)&loc[4]) == 0u)
            continue;
        if (guest_fn_15c0c4_x((uint32_t)(loc_rd32_3d7b0(loc, 0x12) >> 16),
                              (uint32_t)(loc_rd32_3d7b0(loc, 0xa) >> 16),
                              (uint32_t)(loc_rd32_3d7b0(loc, 0xe) >> 16),
                              (ef_addr32)(uintptr_t)&loc[0]) == 0u)
            continue;
        guest_fn_c8ccc_x((uint32_t)(int32_t)(int16_t)(loc[2] | (loc[3] << 8)),
                         (uint32_t)(int32_t)(int16_t)(loc[0] | (loc[1] << 8)),
                         (uint32_t)(int32_t)(int16_t)(loc[6] | (loc[7] << 8)),
                         (uint32_t)(int32_t)(int16_t)(loc[4] | (loc[5] << 8)),
                         (uint32_t)(int32_t)(int16_t)b);
    }
}

/* Original 0x43df4 (338 bytes). Walk a node list and draw the map marker of
 * every type-0x11 node whose group flag (positive / zero / negative) matches,
 * using the cursor position of the matching panel as the framebuffer target. */
void draw_group_markers_43df4(ef_addr32 head, ef_addr32 list)
{
    ef_addr32 node, ptr = 0;
    uint8_t found;

    node = EF_U32(list);
    while (node != 0u) {
        if (EF_U8(node + 0xau) == 0x11u) {
            int32_t row, col;

            found = 0;
            if ((uint16_t)group_flag_positive_cc22c(0u, EF_U8(node + 0xbu), head) != 0u &&
                EF_U16(0x00126554u) != 0u) {
                row = (int32_t)EF_U32(0x0013436du) >> 16;
                col = (int32_t)EF_U32(0x0013436bu);
                ptr = EF_U32(0x00126570u) + (uint32_t)((col >> 16) + row * 0x280);
                found = 1;
            } else if ((uint16_t)group_flag_zero_cc1ec(0u, EF_U8(node + 0xbu), head) != 0u &&
                       EF_U16(0x00126558u) != 0u) {
                row = (int32_t)EF_U32(0x00134376u) >> 16;
                col = (int32_t)EF_U32(0x00134374u);
                ptr = EF_U32(0x00126570u) + (uint32_t)((col >> 16) + row * 0x280);
                found = 1;
            } else if ((uint16_t)group_flag_negative_cc1ac(0u, EF_U8(node + 0xbu), head) != 0u &&
                       EF_U16(0x00126558u) != 0u) {
                row = (int32_t)EF_U32(0x00134376u) >> 16;
                col = (int32_t)EF_U32(0x00134374u);
                ptr = EF_U32(0x00126570u) + (uint32_t)((col >> 16) + row * 0x280);
                found = 1;
            }
            if (found != 0) {
                ef_addr32 p13 = EF_U32(node + 0x13u);
                uint16_t dx = EF_U16(p13 + 8u);
                uint16_t cx = EF_U16(p13 + 0xcu);
                int32_t x = (int16_t)(uint16_t)(dx - 0xau);
                int32_t y = (int16_t)(uint16_t)(0x190u - cx - 0xau);

                guest_fn_ca620((int32_t)ptr, x, y, 0x14u, 0x14u, 0x280u);
                if (EF_U16(0x0012656au) != 0u)
                    guest_fn_440f4_x(EF_U8(node + 0xbu), (uint32_t)x, (uint32_t)y, 0x14u, 0x1au, 0x280u);
            }
        }
        node = EF_U32(node + 4u);
    }
}

/* Original 0x43f48 (343 bytes). As 0x43df4 for type 0x18/0xd nodes: pick the
 * marker framebuffer position by the group's positive / negative / zero flag
 * and draw the marker for the node's second-level record. */
void draw_group_markers2_43f48(ef_addr32 head, ef_addr32 list)
{
    ef_addr32 node, ptr = 0;
    uint8_t found;

    node = EF_U32(list);
    while (node != 0u) {
        uint32_t type = EF_U8(node + 0xau);

        if (type == 0x18u || type == 0xdu) {
            int32_t row, col;

            found = 0;
            if ((uint16_t)group_flag_positive_cc22c(0u, EF_U8(node + 0xbu), head) != 0u &&
                EF_U16(0x00126554u) != 0u) {
                row = (int32_t)EF_U32(0x001342efu) >> 16;
                col = (int32_t)EF_U32(0x001342edu);
                ptr = EF_U32(0x00126570u) + (uint32_t)((col >> 16) + row * 0x280);
                found = 1;
            } else if ((uint16_t)group_flag_negative_cc1ac(0u, EF_U8(node + 0xbu), head) != 0u &&
                       EF_U16(0x00126556u) != 0u) {
                row = (int32_t)EF_U32(0x001342e6u) >> 16;
                col = (int32_t)EF_U32(0x001342e4u) >> 16;
                ptr = EF_U32(0x00126570u) + (uint32_t)(col + row * 0x280);
                found = 1;
            } else if ((uint16_t)group_flag_zero_cc1ec(0u, EF_U8(node + 0xbu), head) != 0u &&
                       EF_U16(0x00126558u) != 0u) {
                row = (int32_t)EF_U32(0x001342f8u) >> 16;
                col = (int32_t)EF_U32(0x001342f6u);
                ptr = EF_U32(0x00126570u) + (uint32_t)((col >> 16) + row * 0x280);
                found = 1;
            }
            if (found != 0) {
                ef_addr32 p20 = EF_U32(EF_U32(node + 0x13u) + 0x20u);
                uint16_t cx = EF_U16(p20 + 0x1du);
                uint16_t bx = EF_U16(p20 + 0x19u);
                int32_t y = (int16_t)(uint16_t)(0x190u - cx - 0xau);
                int32_t x = (int16_t)(uint16_t)(bx - 0xau);

                guest_fn_ca620((int32_t)ptr, x, y, 0x14u, 0x14u, 0x280u);
                if (EF_U16(0x0012656au) != 0u)
                    guest_fn_440f4_x(EF_U8(node + 0xbu), (uint32_t)x, (uint32_t)y, 0x14u, 0x1au, 0x280u);
            }
        }
        node = EF_U32(node + 4u);
    }
}

/* Original 0x441fc (450 bytes). Draw the map marker, cursor and count icon
 * for every type-0x31 node whose group flag matches. The count icon call
 * receives whatever 0xca620/0x440f4 left in ECX, so those two are called
 * through ECX-returning wrappers. */
void draw_group_markers3_441fc(ef_addr32 head, ef_addr32 list)
{
    ef_addr32 node, ptr = 0;
    uint8_t found;

    node = EF_U32(list);
    while (node != 0u) {
        if (EF_U8(node + 0xbu) == 0x31u) {
            int32_t row, col;

            found = 0;
            if ((uint16_t)group_flag_positive_cc22c(0u, EF_U8(node + 9u), head) != 0u &&
                EF_U16(0x00126554u) != 0u) {
                row = (int32_t)EF_U32(0x001342d4u) >> 16;
                col = (int32_t)EF_U32(0x001342d2u) >> 16;
                ptr = EF_U32(0x00126570u) + (uint32_t)(col + row * 0x280);
                found = 1;
            } else if ((uint16_t)group_flag_negative_cc1ac(0u, EF_U8(node + 9u), head) != 0u &&
                       EF_U16(0x00126556u) != 0u) {
                row = (int32_t)EF_U32(0x001342cbu) >> 16;
                col = (int32_t)EF_U32(0x001342c9u);
                ptr = EF_U32(0x00126570u) + (uint32_t)((col >> 16) + row * 0x280);
                found = 1;
            } else if ((uint16_t)group_flag_zero_cc1ec(0u, EF_U8(node + 9u), head) != 0u &&
                       EF_U16(0x00126558u) != 0u) {
                row = (int32_t)EF_U32(0x001342ddu) >> 16;
                col = (int32_t)EF_U32(0x001342dbu);
                ptr = EF_U32(0x00126570u) + (uint32_t)((col >> 16) + row * 0x280);
                found = 1;
            }
            if ((uint16_t)guest_fn_bc7ec(node) != 0u || EF_U16(node + 0x36u) == 0u) {
                row = (int32_t)EF_U32(0x00134742u) >> 16;
                col = (int32_t)EF_U32(0x00134740u) >> 16;
                ptr = EF_U32(0x00126570u) + (uint32_t)(col + row * 0x280);
                found = 1;
            }
            if (found != 0) {
                uint16_t xs = EF_U16(node + 0x16u);
                uint16_t bx = EF_U16(node + 0x1au);
                uint16_t yv = (uint16_t)(0x190u - bx);
                int32_t y = (int16_t)(uint16_t)(0x190u - bx - 0xau);
                int32_t x = (int16_t)(uint16_t)(xs - 0xau);
                uint32_t ecx;

                ecx = guest_fn_ca620_e((int32_t)ptr, x, y, 0x14u, 0x14u, 0x280u);
                blit_cursor_4d1ec((int16_t)xs, (int16_t)yv, node);
                if (EF_U16(0x0012656au) != 0u)
                    ecx = guest_fn_440f4_e(EF_U8(node + 9u), x, y, 0x14u, 0x1au, 0x280u);
                draw_count_icon_4d24c((uint32_t)(int32_t)(int16_t)xs, (uint32_t)(int32_t)(int16_t)yv, node, ecx);
            }
        }
        node = EF_U32(node + 4u);
    }
}

/* Original 0xbf81c (356 bytes). Walk the node list LIST and, for every
 * type-0x31 node with a pending state (+0x4e high word 1, +0x32 > 4) that
 * passes the mode-K filter (K = low byte of P8), build a slot record via
 * 0xfc0ec and run the 0xbdbf8 / 0xbe59c request pair for it; the record is
 * freed again. Returns the running 16-bit accumulator ACC. */
uint16_t process_pending_nodes_bf81c(uint16_t acc, ef_addr32 list, uint32_t p3, uint32_t p4,
                                     uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, uint32_t p9)
{
    ef_addr32 node, rec;
    uint8_t k = (uint8_t)p8;

    node = EF_U32(list);
    while (node != 0u) {
        if (EF_U8(node + 0xcu) == 0x31u && ((int32_t)EF_U32(node + 0x4eu) >> 16) == 1 &&
            (int32_t)EF_U8(node + 0x32u) > 4) {
            uint16_t ok = 1;

            if (k < 0x10u) {
                if (k == 0xfu) {
                    if ((uint16_t)records_differ_bf9d4(node) == 0u)
                        ok = 0;
                }
            } else if (k == 0x10u) {
                if ((uint16_t)records_differ_bf980(node) == 0u)
                    ok = 0;
                else if (((int32_t)EF_U32(node + 0x4cu) >> 16) != 2)
                    ok = 0;
            } else if (k == 0x11u) {
                if (EF_U8(node + 0xbu) != 0u)
                    ok = 0;
                else if ((uint16_t)linked_records_differ(node) == 0u)
                    ok = 0;
            }
            if (ok != 0u && (uint16_t)find_node_matching_cc51c(EF_U8(node + 0xbu), p9, k) != 0u) {
                int32_t acc32;

                rec = create_slot_record_fc0ec(EF_U8(node + 0xbu), k);
                acc32 = (int16_t)acc;
                if (guest_fn_bdbf8(rec, node, acc32, p9) != 0u)
                    acc = guest_fn_be59c(rec, 0u, 0u, node, (uint32_t)acc32, 0u, 1u, p4, p5, p7, p6, list, p3, 0u, p9);
                free_entry_array_b16c0(rec);
            }
        }
        node = EF_U32(node + 4u);
    }
    return acc;
}

/* Original 0xb0df4 (359 bytes). Pick a random free slot number (13 - v, v
 * drawn from the cursor row plus 0..2 and clamped to 0..9, never 7, 8 or
 * 10), mark that node of LIST and update its lookup tile, then for every
 * type-0x31/0x49 node of LIST set the state byte (+0xd) of the matching
 * entries of the list at *PTR: 7 when the negative group flag is set (and
 * the state is 0), otherwise count the positive-flag matches: state 0 for the
 * first 0x31 of them, 7 after that. */
void assign_slot_flags_b0df4(ef_addr32 ptr, ef_addr32 list, ef_addr32 head)
{
    ef_addr32 node, c, rec;
    int32_t v, d;
    uint16_t count;

    do {
        uint32_t r = guest_fn_9c960();

        v = (int32_t)EF_U8(0x001264e2u) + (int32_t)(r & 0xffffu) % 3 - 1;
        if ((int16_t)v > 9) v = 9;
        if ((int16_t)v < 0) v = 0;
        d = (int16_t)(uint16_t)(13 - v);
    } while (d == 7 || d == 10 || d == 8);
    rec = list_nth(list, (uint32_t)d);
    EF_U8(rec + 9u) = 1;
    guest_fn_cb3c4_x(EF_U32(rec + 0x16u), EF_U32(rec + 0x1au), EF_U8(rec + 9u), 0x00134d8au, 0x00137980u);
    lookup_tiles_cbc54(0x00134d8au, 0x00137988u);
    node = EF_U32(list);
    while (node != 0u) {
        uint32_t type = EF_U8(node + 0xbu);

        if (type == 0x31u || type == 0x49u) {
            for (c = EF_U32(EF_U32(ptr)); c != 0u; c = EF_U32(c + 4u)) {
                if (EF_U32(c + 0x3bu) == EF_U32(node + 0x16u) && EF_U32(c + 0x3fu) == EF_U32(node + 0x1au)) {
                    if ((uint16_t)group_flag_negative_cc1ac(EF_U8(c + 0xeu), EF_U8(node + 9u), head) != 0u &&
                        EF_U8(c + 0xdu) == 0u)
                        EF_U8(c + 0xdu) = 7;
                }
            }
            count = 0;
            for (c = EF_U32(EF_U32(ptr)); c != 0u; c = EF_U32(c + 4u)) {
                if (EF_U32(c + 0x3bu) == EF_U32(node + 0x16u) && EF_U32(c + 0x3fu) == EF_U32(node + 0x1au)) {
                    if ((uint16_t)group_flag_positive_cc22c(EF_U8(c + 0xeu), EF_U8(node + 9u), head) != 0u) {
                        if ((int16_t)count < 0x31) {
                            count++;
                            EF_U8(c + 0xdu) = 0;
                        } else {
                            EF_U8(c + 0xdu) = 7;
                        }
                    }
                }
            }
        }
        node = EF_U32(node + 4u);
    }
}

/* Original 0x43b14 (365 bytes). As 0x43f48 for type-0x16 nodes (group flag
 * read from the node's +0xb byte): pick the marker framebuffer position by
 * the group's positive / negative / zero flag and draw the marker for the
 * node's second-level record. */
void draw_group_markers4_43b14(ef_addr32 head, ef_addr32 list)
{
    ef_addr32 node, ptr = 0;
    uint8_t found;

    node = EF_U32(list);
    while (node != 0u) {
        if (EF_U8(node + 0xau) == 0x16u) {
            int32_t row, col;

            found = 0;
            if ((uint16_t)group_flag_positive_cc22c(0u, EF_U8(node + 0xbu), head) != 0u &&
                EF_U16(0x00126554u) != 0u) {
                row = (int32_t)EF_U32(0x00134325u) >> 16;
                col = (int32_t)EF_U32(0x00134323u);
                ptr = EF_U32(0x00126570u) + (uint32_t)((col >> 16) + row * 0x280);
                found = 1;
            } else if ((uint16_t)group_flag_negative_cc1ac(0u, EF_U8(node + 0xbu), head) != 0u &&
                       EF_U16(0x00126556u) != 0u) {
                row = (int32_t)EF_U32(0x0013431cu) >> 16;
                col = (int32_t)EF_U32(0x0013431au) >> 16;
                ptr = EF_U32(0x00126570u) + (uint32_t)(col + row * 0x280);
                found = 1;
            } else if ((uint16_t)group_flag_zero_cc1ec(0u, EF_U8(node + 0xbu), head) != 0u &&
                       EF_U16(0x00126558u) != 0u) {
                row = (int32_t)EF_U32(0x0013432eu) >> 16;
                col = (int32_t)EF_U32(0x0013432cu);
                ptr = EF_U32(0x00126570u) + (uint32_t)((col >> 16) + row * 0x280);
                found = 1;
            }
            if (found != 0) {
                ef_addr32 p20 = EF_U32(EF_U32(node + 0x13u) + 0x20u);
                uint16_t cx = EF_U16(p20 + 0x1du);
                uint16_t bx = EF_U16(p20 + 0x19u);
                int32_t y = (int16_t)(uint16_t)(0x190u - cx - 0xau);
                int32_t x = (int16_t)(uint16_t)(bx - 0xau);

                guest_fn_ca620((int32_t)ptr, x, y, 0x14u, 0x14u, 0x280u);
                if (EF_U16(0x0012656au) != 0u)
                    guest_fn_440f4_x(EF_U8(node + 0xbu), (uint32_t)x, (uint32_t)y, 0x14u, 0x1au, 0x280u);
            }
        }
        node = EF_U32(node + 4u);
    }
}

/* Original 0xc0624 (377 bytes). For every node of LIST whose group flag (read
 * with GROUP/HEAD through 0xcc1ac) is negative and whose type byte (+0xa)
 * equals TYPE and whose record (+0x13) holds two values in 16..384, create a
 * slot record (0xfbad8; its selector byte is 9 for type 0xe, 0x17 for type
 * 0x16 and otherwise an untouched stack byte, modelled as 0), and if 0xbdd64
 * finds a match run the 0xbe59c request with it, replacing VALUE with its
 * result; the slot record is then freed (0xb16c0). Returns VALUE. */
uint16_t process_group_nodes_c0624(uint16_t value, uint8_t type, uint8_t group, ef_addr32 head,
                                   uint32_t p0, uint32_t p1, ef_addr32 list, uint32_t p3,
                                   uint32_t p4, uint32_t p5)
{
    ef_addr32 node = EF_U32(list);
    uint8_t sel = 0;

    while (node != 0u) {
        if ((uint16_t)group_flag_negative_cc1ac(group, EF_U8(node + 0xbu), head) != 0u &&
            EF_U8(node + 0xau) == type) {
            ef_addr32 p13 = EF_U32(node + 0x13u);
            int32_t a = (int32_t)EF_U32(p13 + 8u);
            int32_t b = (int32_t)EF_U32(p13 + 0xcu);

            if (a > 0xf && a < 0x181 && b > 0xf && b < 0x181) {
                int16_t count = 0;

                while (count < 1) {
                    ef_addr32 rec, hit;
                    int32_t val;

                    if (type == 0xeu)
                        sel = 9;
                    else if (type == 0x16u)
                        sel = 0x17;
                    rec = guest_fn_fbad8_x(0u, group, list, node, sel);
                    val = (int32_t)(int16_t)value;
                    hit = guest_fn_bdd64_x(rec, p0, p1, (uint32_t)a, (uint32_t)b, (uint32_t)val, p4, head, list);
                    if (hit != 0u)
                        value = guest_fn_be59c(rec, 0u, node, hit, (uint32_t)val, 0u, 1u, list, p3, p5, p4, p0, p1, 0u, head);
                    free_entry_array_b16c0(rec);
                    count++;
                }
            }
        }
        node = EF_U32(node + 4u);
    }
    return value;
}

/* Original 0x74584 (404 bytes). Release object ID: drop the back-link of the
 * object it was attached to, on first call mark it (bit 0x10 of +0x24) and
 * publish its position / state, then run the one-off teardown (state word
 * +0x22 counts down from 0x113 / 0x2d) and finally set bit 4. */
void release_object_74584(int16_t id)
{
    ef_addr32 obj, other;
    uint8_t flags;

    obj = lookup_filtered_indexed_object(id);
    if (obj == 0u)
        return;
    if (EF_U16(obj + 0x146u) != 0xffu) {
        other = lookup_filtered_indexed_object((int16_t)((int32_t)EF_U32(obj + 0x144u) >> 16));
        if (other != 0u && (uint32_t)EF_U16(other + 0x158u) == (uint32_t)(int32_t)id) {
            EF_U32(other + 0x15au) = 0;
            EF_U32(other + 0x15eu) = 0;
            EF_U16(other + 0x158u) = 0xffu;
        }
    }
    flags = EF_U8(obj + 0x24u);
    if ((flags & 0x10u) == 0u) {
        EF_U8(obj + 0x24u) = (uint8_t)(flags | 0x10u);
        pack_runtime_position(-1, (int32_t)EF_U32(obj) >> 8, (int32_t)EF_U32(obj + 8u) >> 8, 0u);
        guest_fn_10750_x((int32_t)id, 1, 0x64);
        if (EF_U8(0x00127da9u) != 0u)
            send_packet_2b(id, 1, 0x64);
    }
    if ((EF_U8(obj + 0x24u) & 2u) != 0u) {
        EF_U16(obj + 0x22u) = 0;
        EF_U8(obj + 0x24u) = (uint8_t)(EF_U8(obj + 0x24u) & 0xfdu);
    }
    if (EF_U16(obj + 0x22u) == 0u && (EF_U8(obj + 0x24u) & 4u) == 0u) {
        if (EF_U8(0x00127da9u) != 0u) {
            if ((uint32_t)EF_U8(0x001a666bu) == (uint32_t)EF_U16(obj + 0x18u) ||
                ((int32_t)EF_U32(obj + 0x16u) >> 16) == 0xff)
                send_word_packet_2e((uint16_t)id);
        }
        EF_U16(obj + 0x22u) = 0x113u;
        if (object_uses_secondary_grid(id) != 0u && EF_U8(0x00127da8u) == 0u)
            EF_U16(obj + 0x18u) = 0xffu;
        report_tactical_object_action(obj);
        if (EF_U16(0x001f71cau) == (uint16_t)id) {
            if (EF_U8(0x00127da8u) == 0u)
                EF_U16(0x00126516u) = 1u;
            EF_U16(obj + 0x22u) = 0x2du;
            EF_U8(0x001a3647u) = 0;
            transform_by_attitude_74228(obj);
            record_scene_event(EF_U32(obj + 0x12u) == 0u ? 0x45 : 0x42);
        }
    }
    EF_U8(obj + 0x24u) = (uint8_t)(EF_U8(obj + 0x24u) | 4u);
}

/* Original 0x7ac74 (405 bytes). Walk a (0x12afd6)-square window of the
 * 400x400 two-plane tile map starting at the cell below MODE's plane source
 * (centre X/Y shifted down 11 bits, wrapped modulo 400) and draw every
 * non-empty cell (combined 16-bit tile != 0xffff) with 0x9e0c4, passing the
 * 12-bit-fraction cell position on each axis nudged by +/-0x1000 according to
 * the cell's side of the two position globals. S0 is the draw argument
 * (shifted into the high half of EBX). */
void draw_tile_window_7ac74(int32_t mode, int32_t x, uint32_t unused_ebx, int32_t y, uint32_t s0)
{
    int32_t a, b, half, edi, ebp;
    int16_t outer, inner, extent;
    uint32_t arg = s0 << 16;

    (void)unused_ebx;
    (void)select_plane_source(mode);
    half = (int32_t)EF_U32(0x0012afd4u) >> 0x11;
    a = (0x18f - (y >> 11)) - half;
    b = (x >> 11) - half;
    if (a < 0) a += 0x190;
    if (b < 0) b += 0x190;
    a %= 0x190;
    b %= 0x190;
    outer = 0;
    if ((int16_t)EF_U16(0x0012afd6u) <= 0)
        return;
    do {
        edi = (a + (int32_t)outer) % 0x190;
        inner = 0;
        if ((int16_t)EF_U16(0x0012afd6u) > 0) {
            ebp = (int32_t)((uint32_t)(((0x18f - edi) << 11) + 0x400) << 16);
            do {
                int32_t row = (b + (int32_t)inner) % 0x190;
                uint32_t off = (uint32_t)(row * 0x190);
                uint32_t hi = EF_U8(EF_U32(0x001f8eb4u) + off + (uint32_t)edi);
                uint32_t lo = EF_U8(EF_U32(0x001f8eb8u) + off + (uint32_t)edi);
                uint16_t tile = (uint16_t)((hi << 8) | lo);

                if (tile != 0xffffu) {
                    int32_t edx = (row << 11) + 0x400;
                    int32_t ecx, ex;

                    edx = (int32_t)((uint32_t)edx << 16);
                    ecx = (int32_t)EF_U32(0x000c4756u) - edx;
                    ex = (ecx >= 0 ? 1 : -1) * 0x1000;
                    edx += ex;
                    ecx = (int32_t)EF_U32(0x000c4762u) - ebp;
                    ex = (ecx >= 0 ? 1 : -1) * 0x1000;
                    ecx = ex + ebp;
                    guest_fn_9e0c4((int32_t)(int16_t)tile, (uint32_t)edx, arg, (uint32_t)ecx);
                }
                extent = (int16_t)EF_U16(0x0012afd6u);
                inner++;
            } while (inner < extent);
        }
        outer++;
    } while (outer < (int16_t)EF_U16(0x0012afd6u));
}

/* Original 0x79ff0 (420 bytes). Draw a centred ten-pixel-per-row text menu:
 * MENU holds a word count at +0 followed by 9-byte entries whose first dword
 * (+2) is the string pointer. Latch the full-screen clip, measure the widest
 * string, fill the ten 256-byte background rows at 0x1f77e0, fill in the box
 * rectangle words at 0x12afaa..0x12afb8 (doubled in 8 slots when 0x12b369 is
 * set), render the background through 0xc4b95 with block 0x12af98, draw each
 * string with 0x48c90 and flip the page. */
void draw_menu_box_79ff0(ef_addr32 menu)
{
    int16_t count, i, x0, n;
    int32_t edi;
    uint32_t edx = 0, v;
    ef_addr32 ptr;

    latch_values_ce328(0u, EF_U32(0x0015b744u) - 1u, 0u, EF_U32(0x0015b748u) - 1u);
    store_four_dwords_17cf9c(0u, EF_U32(0x0015b744u) - 1u, 0u, EF_U32(0x0015b748u) - 1u);
    count = (int16_t)EF_U16(menu);
    if (count > 0) {
        i = 0;
        do {
            uint32_t len = guest_string_length(EF_U32(menu + 2u + (uint32_t)((int32_t)i * 9)));

            if ((int16_t)len > (int16_t)edx)
                edx = len;
            i++;
        } while (i < (int16_t)EF_U16(menu));
    }
    edi = ((0xc8 - 10 * (int32_t)(int16_t)EF_U16(menu)) >> 1) - 4;
    edx <<= 2;
    x0 = (int16_t)((0x140 - (int32_t)(int16_t)edx) >> 1);
    EF_U16(0x0012afb0u) = (uint16_t)(edi - 4);
    EF_U16(0x0012afacu) = (uint16_t)(edi - 4);
    v = (uint32_t)(edi + 4 + 10 * (int32_t)(int16_t)EF_U16(menu));
    EF_U16(0x0012afb8u) = (uint16_t)v;
    EF_U16(0x0012afb4u) = (uint16_t)v;
    EF_U16(0x0012afb6u) = (uint16_t)(x0 - 4);
    edx += (uint32_t)(int32_t)(x0 - 4);
    EF_U16(0x0012afaau) = (uint16_t)(x0 - 4);
    edx += 8u;
    EF_U16(0x0012afb2u) = (uint16_t)edx;
    EF_U16(0x0012afaeu) = (uint16_t)edx;
    ptr = 0x001f77e0u;
    n = 10;
    do {
        fill_bytes_b3fb0(ptr, 6u, 10u);
        n--;
        ptr += 0x100u;
    } while (n > 0);
    if (EF_U8(0x0012b369u) != 0u) {
        for (i = 0; i < 8; i++)
            EF_U16(0x0012afaau + 2u * (uint32_t)i) = (uint16_t)(EF_U16(0x0012afaau + 2u * (uint32_t)i) << 1);
    }
    edi += 2;
    EF_U32(0x00159080u) = 0x001f77e0u;
    guest_fn_c4b95_x(0x0012af98u);
    count = (int16_t)EF_U16(menu);
    i = 0;
    if (count > 0) {
        do {
            EF_U8(0x00127b1au) = 1;
            draw_shadowed_text_48c90(0xfu, (int32_t)x0, (int32_t)(int16_t)edi,
                                     EF_U32(menu + 2u + (uint32_t)((int32_t)i * 9)));
            i++;
            edi += 10;
        } while (i < (int16_t)EF_U16(menu));
    }
    cycle_page_dc280();
}

/* Original 0x7b3fc: draw a cross marker on the tile map cell (x, y). */
uint32_t draw_tile_cross_7b3fc(int32_t x, int32_t y)
{
	uint32_t esi = ((uint32_t)(2 * x + 1)) << 10;
	uint32_t edi = ((uint32_t)(2 * (0x18f - y) + 1)) << 10;
	uint32_t row;
	uint32_t tile;
	int32_t tile16;
	int32_t bx, by, a;

	if (x > 0x18f || x < 0 || y > 0x18f || y < 0)
		return 0;
	row = (uint32_t)(400 * x);
	tile = ((uint32_t)EF_U8(EF_U32(0x001f8eb4u) + row + (uint32_t)y) << 8) |
	       (uint32_t)EF_U8(EF_U32(0x001f8eb8u) + row + (uint32_t)y);
	if ((uint16_t)tile == 0xffffu)
		return 0;
	tile16 = (int16_t)(uint16_t)tile;
	if (EF_U8(0x00122c52u) == 1u) {
		store_bytes_46437d(0xe, 0x88);
		rotate_page_entries_e8368((uint32_t)tile16, esi, 0u, edi);
		store_bytes_46437d(0xf, 0x88);
		draw_entry_boxes_e8484(tile16, esi, 0u, edi);
	}
	if (guest_fn_d0278_x((uint32_t)tile16, esi, 0u, edi) == 0u)
		return 0;
	EF_U32(0x001f8eb0u) = EF_U32(0x0042090cu) + esi;
	EF_U32(0x001f8eacu) = EF_U32(0x004208fcu);
	EF_U32(0x001f8ea8u) = EF_U32(0x00420900u) + edi;
	if (EF_U8(0x00122c52u) == 1u) {
		bx = (int16_t)EF_U16(0x001f8eb0u);
		by = (int16_t)EF_U16(0x001f8eacu);
		a = (int32_t)EF_U32(0x001f8ea6u) >> 16;
		setup_rotation_args_16000c(0, 0, 0, 0, (uint32_t)a, (uint32_t)(int16_t)(by + 5),
			(uint32_t)(int16_t)(bx + 5), (uint32_t)a, (uint32_t)(int16_t)(by - 5),
			(uint32_t)(int16_t)(bx - 5), 0xffu, 0xfu);
		bx = (int16_t)EF_U16(0x001f8eb0u);
		by = (int16_t)EF_U16(0x001f8eacu);
		a = (int32_t)EF_U32(0x001f8ea6u) >> 16;
		setup_rotation_args_16000c(0, 0, 0, 0, (uint32_t)a, (uint32_t)(int16_t)(by + 5),
			(uint32_t)(int16_t)(bx - 5), (uint32_t)a, (uint32_t)(int16_t)(by - 5),
			(uint32_t)(int16_t)(bx + 5), 0xffu, 0xfu);
	}
	return 1;
}

/* Original 0x7b5d4: walk the tile cells crossed by the line (ax,ay)-(bx,by),
 * drawing each with 0x7b3fc until one draws (1) or the end cell is reached (0). */
uint32_t trace_line_cells_7b5d4(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
	int32_t dx = bx - ax, dy = by - ay;
	int32_t cx = ax >> 11, cy = ay >> 11;
	int32_t endx = bx >> 11, endy = by >> 11;
	int32_t ccy = 0x18f - cy;
	int32_t px = cx << 11, py = cy << 11;
	int32_t quad;
	int32_t flag;

	if (dx >= 0)
		quad = (dy > 0) ? 1 : 4;
	else
		quad = (dy > 0) ? 2 : 3;
	flag = abs_a28f2(dx) > abs_a28f2(dy);
	quad -= 1;
	for (;;) {
		int32_t pa, pb, qa, qb;

		if (draw_tile_cross_7b3fc(cx, ccy) != 0u)
			return 1;
		if (flag) {
			if (cx == endx)
				return 0;
		} else if (cy == endy) {
			return 0;
		}
		if ((uint32_t)quad > 3u)
			continue;
		pa = (px - ax) * dy;
		qa = (py - ay) * dx;
		pb = (px + 0x800 - ax) * dy;
		qb = (py + 0x800 - ay) * dx;
		switch (quad) {
		case 0:
			if (qb - pb > 0) { cx++; px += 0x800; }
			else { cy++; ccy--; py += 0x800; }
			break;
		case 1:
			if (qb - pa > 0) { cy++; ccy--; py += 0x800; }
			else { cx--; px -= 0x800; }
			break;
		case 2:
			if (qa - pa > 0) { cx--; px -= 0x800; }
			else { cy--; ccy++; py -= 0x800; }
			break;
		default:
			if (qa - pb > 0) { cx++; px += 0x800; }
			else { cy--; ccy++; py -= 0x800; }
			break;
		}
	}
}

/* Original 0x7b7d8: draw the marker line (endpoints at 0x420900..0x42091c) on the tile map. */
uint32_t draw_marker_line_7b7d8(void)
{
	int32_t l4, ebx, esi, edi;
	int32_t bx, by, a;
	int32_t neg4, neg8;

	if ((int32_t)EF_U32(0x00420904u) < -0x465 && (int32_t)EF_U32(0x00420908u) < -0x465)
		return 0;
	l4 = (int32_t)EF_U32(0x00420910u) >> 11;
	ebx = 0x18f - ((int32_t)EF_U32(0x00420918u) >> 11);
	esi = 0x18f - ((int32_t)EF_U32(0x0042091cu) >> 11);
	edi = (int32_t)EF_U32(0x00420914u) >> 11;
	if (EF_U8(0x00122c52u) == 1u) {
		setup_rotation_args_16000c(0, 0, 0, 0,
			(uint32_t)((int32_t)EF_U32(0x0042091au) >> 16),
			(uint32_t)((int32_t)EF_U32(0x00420906u) >> 16),
			(uint32_t)((int32_t)EF_U32(0x00420912u) >> 16),
			(uint32_t)((int32_t)EF_U32(0x00420916u) >> 16),
			(uint32_t)((int32_t)EF_U32(0x00420902u) >> 16),
			(uint32_t)((int32_t)EF_U32(0x0042090eu) >> 16),
			0xffu, 0xeu);
		setup_rotation_args_16000c(0, 0, 0, 0,
			(uint32_t)(int32_t)(int16_t)((((0x18f - esi) * 2 + 1) << 10)),
			0u,
			(uint32_t)(int32_t)(int16_t)((edi * 2 + 1) << 10),
			(uint32_t)(int32_t)(int16_t)((((0x18f - ebx) * 2 + 1) << 10)),
			0u,
			(uint32_t)(int32_t)(int16_t)((l4 * 2 + 1) << 10),
			0xffu, 0xcu);
	}
	if (trace_line_cells_7b5d4(EF_U32(0x00420910u), EF_U32(0x00420918u),
	                           EF_U32(0x00420914u), EF_U32(0x0042091cu)) != 0u)
		return 1;
	neg4 = (int32_t)EF_U32(0x00420904u) < 0;
	neg8 = (int32_t)EF_U32(0x00420908u) < 0;
	if (neg4 == neg8)
		return 0;
	EF_U32(0x001f8eacu) = 0u;
	interpolate_line_e85f0(0x001f8eb0u, 0x001f8ea8u);
	if (EF_U8(0x00122c52u) == 1u) {
		bx = (int16_t)EF_U16(0x001f8eb0u);
		by = (int16_t)EF_U16(0x001f8eacu);
		a = (int32_t)EF_U32(0x001f8ea6u) >> 16;
		setup_rotation_args_16000c(0, 0, 0, 0, (uint32_t)a, (uint32_t)(int16_t)(by + 5),
			(uint32_t)(int16_t)(bx + 5), (uint32_t)a, (uint32_t)(int16_t)(by - 5),
			(uint32_t)(int16_t)(bx - 5), 0xffu, 0xfu);
		bx = (int16_t)EF_U16(0x001f8eb0u);
		by = (int16_t)EF_U16(0x001f8eacu);
		a = (int32_t)EF_U32(0x001f8ea6u) >> 16;
		setup_rotation_args_16000c(0, 0, 0, 0, (uint32_t)a, (uint32_t)(int16_t)(by + 5),
			(uint32_t)(int16_t)(bx - 5), (uint32_t)a, (uint32_t)(int16_t)(by - 5),
			(uint32_t)(int16_t)(bx + 5), 0xffu, 0xfu);
	}
	return 1;
}

/* Original 0x759c: per-frame update of the tracked target object pair (low-level AI hit bookkeeping). */
void update_tracked_pair_759c(int32_t id)
{
	uint32_t p, base, obj, obj2;
	uint32_t slot;
	int32_t v;
	uint8_t q;

	if (EF_U8(0x0019726au) == 0u)
		return;
	if (EF_U16(0x0019bb5cu) == 0u)
		return;
	if (EF_U8(0x00121e54u) != 0u) {
		v = (int16_t)EF_U16(0x0019bb5cu);
		EF_U8(0x00121e54u) = 0;
		if (v > 0x12c)
			EF_U8(0x00197269u) = 1;
		else
			EF_U8(0x00197269u) = (uint8_t)(600 / (v * (int32_t)EF_U8(0x0019726au)));
		if (EF_U8(0x00197269u) == 0u)
			EF_U8(0x00197269u) = 1;
		q = (uint8_t)((uint32_t)EF_U8(0x00197269u) / 10u);
		EF_U8(0x00197268u) = q;
		if (q == 0u)
			EF_U8(0x00197268u) = 1;
	}
	p = lookup_filtered_indexed_object((int16_t)id);
	if (p == 0u)
		return;
	slot = EF_U8(p + 0x187u);
	if (slot == 0xffu)
		return;
	EF_U16(0x00197266u) = (uint16_t)id;
	base = EF_U32(0x00197260u);
	EF_U16(base + slot * 5u + 3u) = (uint16_t)(EF_U16(base + slot * 5u + 3u) + 1u);
	{
		uint32_t e = base + (uint32_t)EF_U8(p + 0x187u) * 5u;
		if (!((int16_t)EF_U16(0x0019bb5cu) > (int16_t)EF_U16(e + 3u)))
			EF_U16(e + 3u) = 0;
	}
	v = (int32_t)EF_U32(p + 0x26u) >> 16;
	base = EF_U32(0x0019bb58u);
	obj = (uint32_t)(v * 0xca) + base;
	if (obj == 0u)
		return;
	EF_U16(0x00197264u) = EF_U16(EF_U32(0x00197260u) + (uint32_t)EF_U8(p + 0x187u) * 5u + 3u);
	v = (int32_t)EF_U32(0x00197262u) >> 16;
	if (v == 0xff)
		return;
	obj2 = base + (uint32_t)(v * 0xca);
	if (obj2 == 0u)
		return;
	if (EF_U8(obj2 + 0x1au) == 0u)
		return;
	v = (int32_t)EF_U32(0x0019bb5cu) >> 16;
	if (v != 0xff) {
		uint32_t obj3 = base + (uint32_t)(v * 0xca);
		if (EF_U16(obj + 0x15u) == EF_U16(obj3 + 0x15u)) {
			EF_U32(0x00121e5cu) = 0u;
			EF_U32(0x00121e60u) = 0x3ff00000u;
		} else {
			EF_U32(0x00121e5cu) = 0u;
			EF_U32(0x00121e60u) = 0u;
		}
	}
	if (record_value_not_above_limit(obj2) == 0u) {
		EF_U8(obj2 + 0x21u) = 0;
		return;
	}
	guest_fn_73ec_x(obj, obj2);
}

/* Original 0x39548: classify the view sample at a fixed-point position and return a table value. */
int16_t classify_view_sample_39548(ef_addr32 p, int16_t d, int16_t m)
{
	int32_t x, z, cell;
	int16_t y16, t, si, di;
	uint16_t du = (uint16_t)d;

	if (m == 1) {
		if (d == 1)
			return (int16_t)EF_U16(0x00425bdeu);
		return -1;
	}
	x = (int32_t)EF_U32(p) >> 8;
	y16 = (int16_t)((int32_t)EF_U32(p + 4u) >> 8);
	z = (int32_t)EF_U32(p + 8u) >> 8;
	if (m == 3) {
		if (d == 1)
			return (int16_t)EF_U16(0x00425bdeu);
		update_proximity_39874((uint16_t)x, (uint32_t)(int32_t)y16, (uint16_t)z);
		return (int16_t)EF_U16(0x00425be6u);
	}
	cell = (int16_t)guest_fn_7b2f4((uint32_t)((int32_t)EF_U32(0x001a35f8u) >> 16),
	                               (int32_t)EF_U32(p) >> 19, (int32_t)EF_U32(p + 8u) >> 19);
	if (cell == -1)
		return -1;
	t = (int16_t)guest_fn_39744(cell, (uint16_t)x, (int32_t)y16, (uint16_t)z);
	si = -1;
	di = -1;
	if (t >= 0xb0 && t <= 0xdf)
		si = 3;
	if (t >= 0x20 && t <= 0x4f)
		si = 0;
	if (t >= 0x70 && t <= 0x8f)
		si = 2;
	if (du == 1u) {
		if (si == 0)
			di = (int16_t)EF_U16(0x00425beau);
		if (si == 3)
			di = (int16_t)EF_U16(0x00425beeu);
		if (si == 2)
			di = (int16_t)EF_U16(0x00425bd4u);
	} else if (du == 4u) {
		if (si == 0 || si == 3)
			di = (int16_t)EF_U16(0x00425bdcu);
		if (si == 2)
			di = (int16_t)EF_U16(0x00425bdau);
	} else {
		if (si == 0) {
			di = (int16_t)EF_U16(0x00425becu);
			update_proximity_39874((uint16_t)x, (uint32_t)(int32_t)y16, (uint16_t)z);
		}
		if (si == 3) {
			di = (int16_t)EF_U16(0x00425bd6u);
			update_proximity_39874((uint16_t)x, (uint32_t)(int32_t)y16, (uint16_t)z);
		}
		if (si == 2)
			di = (int16_t)EF_U16(0x00425bd8u);
	}
	return di;
}

/* Original 0x3bd50: build the 37 per-angle sin/cos marker tables (8 words each). */
void init_angle_tables_3bd50(void)
{
	int32_t ang = (int32_t)0xfff70000u;
	int32_t i = 0;
	uint32_t lc = 0u, l14 = 0u;

	do {
		uint16_t out[2];
		uint32_t e = (uint32_t)i * 0x48u;
		uint32_t ecx, ptr, ebx;
		int32_t scale;

		load_sincos_dc7bc((uint32_t)(ang / 0x24) & 0xffffu);
		rotate_point_dc7e4((ef_addr32)(uintptr_t)out, 0x1000, 0);
		EF_U16(0x001a23bau + e) = out[0];
		EF_U16(0x001a23b8u + e) = out[1];
		EF_U16(0x001a1952u + e) = out[0];
		EF_U16(0x001a1950u + e) = out[1];
		EF_U32(0x001a23bcu + e) = EF_U32(0x00126384u + lc);
		EF_U32(0x001a1954u + e) = EF_U32(0x00126384u + lc);
		ecx = (guest_fn_9d524(EF_U32(0x001a23bcu + e), 0x001835ecu) == 0u) ? 0x0012643cu : 0x0012641cu;
		scale = (int16_t)EF_U16(0x0012645cu + l14);
		ptr = EF_U32(0x00126384u + lc);
		for (ebx = e; ebx != e + 0x10u; ebx += 2u) {
			EF_U16(0x001a2378u + ebx) = (uint16_t)(((int32_t)(int16_t)EF_U16(ecx) * scale) / 100);
			ecx += 2u;
			EF_U16(0x001a2388u + ebx) = EF_U16(ecx);
			ecx += 2u;
			if (EF_U8(ptr) != 0x2du)
				EF_U16(0x001a2388u + ebx) = (uint16_t)(0u - EF_U16(0x001a2388u + ebx));
			EF_U16(0x001a2398u + ebx) = (uint16_t)(0u - EF_U16(0x001a2378u + ebx));
			EF_U16(0x001a23a8u + ebx) = EF_U16(0x001a2388u + ebx);
			EF_U16(0x001a1910u + ebx) = (uint16_t)((int32_t)(int16_t)EF_U16(0x001a2378u + ebx) / 2);
			EF_U16(0x001a1920u + ebx) = (uint16_t)((int32_t)(int16_t)EF_U16(0x001a2388u + ebx) / 2);
			EF_U16(0x001a1930u + ebx) = (uint16_t)((int32_t)(int16_t)EF_U16(0x001a2398u + ebx) / 2);
			EF_U16(0x001a1940u + ebx) = (uint16_t)((int32_t)(int16_t)EF_U16(0x001a23a8u + ebx) / 2);
		}
		lc += 4u;
		l14 += 2u;
		i++;
		ang += 0x8000;
	} while (i <= 0x24);
}

/* Original 0x3bf44: draw the compass ladder strip (outline boxes and labels) for 7 table rows. */
void draw_compass_strip_3bf44(int16_t col, int16_t dy, int16_t ang)
{
	uint32_t tbl;
	int32_t q, first, last, i;
	uint32_t l44, l48;
	uint16_t l50[2];

	tbl = EF_U32(0x0012637cu + (uint32_t)EF_U8(0x0012b369u) * 4u);
	if (EF_U8(0x001a2de0u) == 0u)
		return;
	load_sincos_dc7bc((uint32_t)(uint16_t)(0 - (int32_t)ang));
	q = (0 - (int32_t)dy) / 0x38e;
	first = q + 15;
	last = q + 0x15;
	l44 = (uint32_t)(first * 72);
	l48 = tbl + l44;
	if (last < first)
		return;
	i = first;
	for (;;) {
		if (i >= 0 && i <= 0x24) {
			pair_word_dc5a9((int16_t)EF_U16(l48 + 0x42u), (int16_t)EF_U16(l48 + 0x40u),
			                (ef_addr32)(uintptr_t)l50);
			if (!(EF_U8(0x001a2de0u) == 2u && (i & 1) != 0)) {
				uint16_t o1[16], o2[16];
				uint32_t rec, name;
				int16_t di;
				int16_t c = col;

				for (di = 0; di < 8; di++) {
					rec = tbl + l44 + (uint32_t)di * 2u;
					rotate_point_dc7e4((ef_addr32)(uintptr_t)&o1[2 * di],
					                   (int16_t)EF_U16(rec), (int16_t)EF_U16(rec + 0x10u));
					o1[2 * di] = (uint16_t)(o1[2 * di] + l50[0]);
					o1[2 * di + 1] = (uint16_t)(o1[2 * di + 1] + l50[1]);
					rotate_point_dc7e4((ef_addr32)(uintptr_t)&o2[2 * di],
					                   (int16_t)EF_U16(rec + 0x20u), (int16_t)EF_U16(rec + 0x30u));
					o2[2 * di] = (uint16_t)(o2[2 * di] + l50[0]);
					o2[2 * di + 1] = (uint16_t)(o2[2 * di + 1] + l50[1]);
				}
				rec = tbl + l44;
				name = EF_U32(rec + 0x44u);
				if (EF_U8(name) == 0x2du) {
					for (di = 1; di < 8; di++) {
						if (di == 3)
							continue;
						renderer_viewport_five_c8ccc(o1[2 * di + 1], o1[2 * di], o1[2 * di - 1],
						                             o1[2 * di - 2], (uint16_t)c);
						renderer_viewport_five_c8ccc(o2[2 * di + 1], o2[2 * di], o2[2 * di - 1],
						                             o2[2 * di - 2], (uint16_t)c);
					}
				} else {
					renderer_viewport_five_c8ccc(o1[3], o1[2], o1[1], o1[0], (uint16_t)c);
					renderer_viewport_five_c8ccc(o1[15], o1[14], o1[3], o1[2], (uint16_t)c);
					renderer_viewport_five_c8ccc(o2[3], o2[2], o2[1], o2[0], (uint16_t)c);
					renderer_viewport_five_c8ccc(o2[15], o2[14], o2[3], o2[2], (uint16_t)c);
				}
				if (guest_fn_9d524(EF_U32(l48 + 0x44u), 0x001835ecu) != 0u) {
					uint32_t sh = EF_U8(0x0012b369u) & 31u;
					int32_t x = (int16_t)((2u << sh) + o1[0]);
					int32_t y = (int16_t)((0xfffffffeu << sh) + o1[1]);

					draw_formatted_text_mode_48bb0((uint8_t)col,
					                               (uint32_t)(1u - EF_U8(0x0012b369u)) & 0xffu,
					                               x, y, EF_U32(l48 + 0x44u));
				}
			}
		}
		l48 += 0x48u;
		l44 += 0x48u;
		i++;
		if (i > last)
			break;
	}
}

/* Original 0x84b8: refresh an object's best sub-entry. ESI is an extra
 * register input that is forwarded as ECX to the 0x46254 notification on the
 * "no free slot" paths. */
void assign_object_sub_entry_84b8(int16_t obj, uint32_t esi_in)
{
	uint32_t off = (uint32_t)((int32_t)obj * 0xca);
	uint32_t base = EF_U32(0x0019bb58u);
	uint8_t best, slot;
	uint32_t rec, ent;
	int i;

	best = find_min_metric_index_8184(base + off + 0x61u);
	base = EF_U32(0x0019bb58u);
	EF_U8(base + off + 0x5fu) = best;
	if (EF_U8(0x001285a1u + (uint32_t)EF_U8(base + off + 0x19u) * 0xe5u) != 0u) {
		for (i = 0; i < 6; i++)
			guest_fn_46254(0x899u, (int32_t)EF_S16(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)obj * 0xca) + 0x50u + (uint32_t)(2 * i)),
			               0x55u, 0u, 0u, 0u);
		return;
	}
	slot = find_free_slot_825c(best);
	if (slot == 0xfe) {
		guest_fn_46254(0x899u, (int32_t)EF_S16(EF_U32(0x0019bb58u) + off + 0x50u), 0x57u, esi_in, 0xfeu, 0u);
		return;
	}
	if (slot == 0xff) {
		guest_fn_46254(0x899u, (int32_t)EF_S16(EF_U32(0x0019bb58u) + off + 0x50u), 0x56u, esi_in, 0xffu, 0u);
		return;
	}
	rec = EF_U32(0x001972e0u) + (uint32_t)best * 34u;
	ent = EF_U32(rec + 0x16u) + (uint32_t)slot * 56u;
	if (EF_S16(ent + 0x20u) != 0xff && EF_U16(ent + 0x20u) != (uint16_t)obj)
		return;
	flag_sub_entry_side_8394(best, slot);
	rec = EF_U32(0x001972e0u) + (uint32_t)best * 34u;
	EF_U8(EF_U32(rec + 0x16u) + (uint32_t)slot * 56u + 0x25u) = 1;
	EF_U16(EF_U32(rec + 0x16u) + (uint32_t)slot * 56u + 0x20u) = (uint16_t)obj;
	EF_U8(EF_U32(rec + 0x16u) + (uint32_t)slot * 56u + 0x26u) = 1;
	{
		int32_t h = sub_entry_heading_8424(best, slot);
		guest_fn_46254(0x899u, (int32_t)EF_S16(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)obj * 0xca) + 0x50u),
		               0x54u, (uint32_t)(int32_t)(int16_t)h, (uint32_t)slot, 0u);
	}
}

/* Original 0xafbc: scatter `count` random probes around (x, y) with spread
 * w (x) and s (y); when the probed 3x3 neighbourhood of the read_buffer_byte
 * map is all ones, spawn a record through 0xbefb8. */
void scatter_probe_afbc(int16_t x, int16_t y, int16_t count, int16_t w, int16_t s)
{
	int32_t i = 0;
	int16_t px = x, py = y;
	int32_t xs, ys, cy, sum, nx;

	if (count <= 0)
		return;
	do {
		int32_t r = (int32_t)(uint16_t)step_generator_a45b3();
		int32_t rem = r % (int32_t)w;
		int32_t half = ((int32_t)w - ((int32_t)w >> 31)) >> 1;

		px = (int16_t)(px + (rem - half));
		r = (int32_t)(uint16_t)step_generator_a45b3();
		rem = r % (int32_t)s;
		half = ((int32_t)s - ((int32_t)s >> 31)) >> 1;
		py = (int16_t)(py + (rem - half));
		xs = px;
		if (xs > 0x18b)
			px = 0x18b;
		else if (xs < 5)
			px = 5;
		ys = py;
		if (ys > 0x18b)
			py = 0x18b;
		else if (ys < 5)
			py = 5;
		nx = px;
		cy = 0x190 - (int32_t)py;
		sum = 0;
		sum += read_buffer_byte((uint32_t)nx, (uint32_t)cy);
		sum += read_buffer_byte((uint32_t)nx, (uint32_t)(cy - 1));
		sum += read_buffer_byte((uint32_t)nx, (uint32_t)(cy + 1));
		sum += read_buffer_byte((uint32_t)(nx - 1), (uint32_t)cy);
		sum += read_buffer_byte((uint32_t)(nx - 1), (uint32_t)(cy - 1));
		sum += read_buffer_byte((uint32_t)(nx - 1), (uint32_t)(cy + 1));
		sum += read_buffer_byte((uint32_t)(nx + 1), (uint32_t)cy);
		sum += read_buffer_byte((uint32_t)(nx + 1), (uint32_t)(cy - 1));
		sum += read_buffer_byte((uint32_t)(nx + 1), (uint32_t)(cy + 1));
		if (sum == 9) {
			int32_t v = (int32_t)(int16_t)EF_U16(0x0041f328u);
			uint32_t rnd = guest_random_word();
			int32_t k = (int32_t)(uint16_t)rnd % 2 + 1;

			EF_U16(0x0041f328u) = (uint16_t)guest_fn_befb8_x(
				0x00137960u, 0x00137964u, 0x00137988u, 0x00137980u,
				0x1du, 6u, 1u, 3u, 4u, (uint32_t)(int32_t)(int16_t)k,
				(uint32_t)nx, (uint32_t)(int32_t)py, 0u, (uint32_t)v);
		}
		i++;
	} while ((int16_t)i < count);
}
