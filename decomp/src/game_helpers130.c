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
