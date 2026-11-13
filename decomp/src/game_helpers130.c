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
