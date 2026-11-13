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
