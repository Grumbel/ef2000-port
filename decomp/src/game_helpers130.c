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
