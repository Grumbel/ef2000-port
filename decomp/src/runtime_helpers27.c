/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Parser record readers, handle pools and name lists, 0x510..0xffb4. */

/* 0x510, 113 bytes. Allocate a paragraph-aligned block and register it in the 12-entry pool. */
uint32_t alloc_aligned_510(uint32_t size)
{
    uint32_t far_ptr = guest_fn_9be38(size + 0xfu);
    uint32_t linear;
    uint32_t aligned;
    uint32_t index;

    if (far_ptr == 0)
        return far_ptr;
    linear = (far_ptr & 0xffffu) + (far_ptr >> 16) * 16u;
    aligned = linear + (0x10u - (linear & 0xfu)) + EF_U32(0x00120cc6u);
    index = EF_U32(0x00120d20u);
    EF_U32(0x00195fc0u + index * 8u) = far_ptr;
    EF_U32(0x00195fc4u + index * 8u) = aligned;
    EF_U32(0x00120d20u) = index + 1u;
    if ((int32_t)(index + 1u) >= 0xc)
        ui_report_missing_record(0x0017f064u);
    return aligned;
}

/* 0x628, 144 bytes. Take a free 10-byte request slot, fill it and start the request. */
uint32_t start_request_628(uint32_t first, uint32_t second, int32_t third)
{
    uint32_t free_index;
    uint32_t slot;

    EF_U32(0x00196144u) = EF_U32(0x00196144u) - 1u;
    if ((int32_t)EF_U32(0x00196144u) < 0)
        ui_report_missing_record(0x0017f098u);
    if (third <= 0)
        guest_fatal_arg(0x0017f0acu, (uint32_t)third);
    free_index = EF_U8(0x00195fb0u + EF_U32(0x00196144u));
    slot = free_index * 5u;
    EF_U16(0x00196026u + slot * 2u) = (uint16_t)second;
    EF_U32(0x00196022u + slot * 2u) = first;
    EF_U32(0x00196028u + slot * 2u) = (uint32_t)third;
    if (guest_fn_9c220(first, second & 0xffffu, (uint32_t)third) != 0)
        guest_fatal_arg(0x0017f0ccu, (uint32_t)third);
    return free_index;
}

/* 0x6b8, 124 bytes. Finish a request slot and return it to the free list. */
void finish_request_6b8(uint32_t index)
{
    uint32_t slot = index * 5u;
    uint16_t second = EF_U16(0x00196026u + slot * 2u);
    uint32_t first = EF_U32(0x00196022u + slot * 2u);
    uint32_t third = EF_U32(0x00196028u + slot * 2u);
    uint32_t next;

    if (third == 0)
        return;
    if (guest_fn_9c23d(first, second, third) != 0)
        ui_report_missing_record(0x0017f0e8u);
    EF_U32(0x00196028u + slot * 2u) = 0;
    next = EF_U32(0x00196144u) + 1u;
    EF_U8(0x00195fafu + next) = (uint8_t)index;
    EF_U32(0x00196144u) = next;
}

/* 0x734, 120 bytes. Take a free 14-byte timer slot, fill it from the source and start it. */
uint32_t start_timer_734(uint32_t source, uint32_t unused_dx, uint32_t flags,
                         uint32_t selector)
{
    uint32_t free_index;
    uint32_t slot;
    uint64_t result;

    (void)unused_dx;
    EF_U32(0x00196140u) = EF_U32(0x00196140u) - 1u;
    if ((int32_t)EF_U32(0x00196140u) < 0)
        ui_report_missing_record(0x0017f0fcu);
    free_index = EF_U8(0x00196134u + EF_U32(0x00196140u));
    slot = free_index * 7u;
    EF_U32(0x001960c4u + slot * 2u) = source;
    EF_U32(0x001960c8u + slot * 2u) = guest_fn_324(source);
    result = guest_fn_348(source);
    EF_U16(0x001960d0u + slot * 2u) = (uint16_t)(result >> 32);
    EF_U32(0x001960ccu + slot * 2u) = (uint32_t)result;
    guest_fn_484(source, (uint32_t)(result >> 32), flags, selector);
    return free_index;
}

/* 0x6b50, 163 bytes. Copy the pending name into a fresh string slot according to the mode. */
void copy_pending_name_6b50(void)
{
    uint8_t mode = EF_U8(0x00125846u);
    int32_t length;

    EF_U32(0x00197228u) = 0;
    if (mode < 1)
        return;
    if (mode <= 2) {
        if (guest_string_length(0x001971b4u) == 0)
            return;
        length = (int16_t)((uint32_t (*)(uint32_t))EF_U32(0x00132ab4u))(0x001971b4u);
        if ((int16_t)length == 0)
            return;
        EF_U32(0x00197228u) = guest_fn_b3f58((int16_t)guest_fn_b3db0((uint32_t)(length + 1)));
        ((void (*)(uint32_t, uint32_t))EF_U32(0x00132ab8u))(0x001971b4u, EF_U32(0x00197228u));
    } else if (mode == 3) {
        length = (int16_t)guest_fn_b3b90(0x00180380u);
        if ((int16_t)length == 0)
            return;
        EF_U32(0x00197228u) = guest_fn_b3f58((int16_t)guest_fn_b3db0((uint32_t)(length + 1)));
        guest_fn_b3c04(0x00180380u, EF_U32(0x00197228u));
    } else {
        return;
    }
    EF_U8(EF_U32(0x00197228u) + (uint32_t)length) = 0;
}

/* 0xf198, 199 bytes. Read a 14-byte header and its list of 44-byte records. */
uint32_t read_header_list_f198(void)
{
    ef_addr32 header;
    int16_t i = 0;

    read_parser_line();
    read_parser_token();
    header = guest_allocate_parser_text(0xe, 0);
    EF_U32(0x001379c4u) = header;
    EF_U16(EF_U32(0x001379c4u)) = (uint16_t)guest_fn_e95c();
    EF_U16(EF_U32(0x001379c4u) + 2u) = (uint16_t)guest_fn_e95c();
    EF_U16(EF_U32(0x001379c4u) + 4u) = (uint16_t)guest_fn_e95c();
    EF_U16(EF_U32(0x001379c4u) + 6u) = (uint16_t)guest_fn_e95c();
    header = EF_U32(0x001379c4u);
    EF_U16(header + 8u) = (uint16_t)guest_fn_e95c();
    if ((int16_t)EF_U16(header + 4u) > 0) {
        int32_t count = (int32_t)EF_U32(header + 2u) >> 16;

        EF_U32(EF_U32(0x001379c4u) + 0xau) = guest_allocate_parser_text((uint32_t)(count * 44), 0);
    } else {
        EF_U32(header + 0xau) = 0;
    }
    while (i < (int16_t)EF_U16(EF_U32(0x001379c4u) + 4u)) {
        ef_addr32 entry = EF_U32(EF_U32(0x001379c4u) + 0xau) + (uint32_t)((int32_t)i * 0x2c);

        i++;
        guest_fn_f13c(entry);
    }
    return 1;
}

/* 0xf604, 163 bytes. Read a record header and its list of 124-byte sub-records. */
uint32_t read_record_header_f604(ef_addr32 record)
{
    uint32_t count;
    uint32_t i = 0;
    uint32_t offset = 0;

    read_parser_line();
    read_parser_token();
    EF_U8(record + 9u) = (uint8_t)read_parser_enum_two_families();
    EF_U16(0x00197abau) = EF_U8(record + 9u);
    EF_U8(record + 0xbu) = (uint8_t)read_parser_enum();
    EF_U8(record + 0xau) = (uint8_t)guest_fn_eaf8();
    EF_U16(0x00197abcu) = EF_U8(record + 0xau);
    EF_U8(record + 0xcu) = (uint8_t)read_parser_enum_10_names();
    EF_U16(record + 0xeu) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 0x10u) = (uint16_t)guest_fn_e95c();
    count = guest_fn_e95c() & 0xffu;
    EF_U8(record + 0x12u) = (uint8_t)count;
    EF_U32(record + 0x13u) = guest_allocate_parser_text(count * 124u, count);
    while (i < EF_U8(record + 0x12u)) {
        i++;
        guest_fn_f314(EF_U32(record + 0x13u) + offset, EF_U8(record + 0xau));
        offset += 0x7cu;
    }
    return 1;
}

/* 0xfb10, 187 bytes. Read a list of 25-byte points and clamp the inner ones into 12..387. */
uint32_t read_point_list_fb10(ef_addr32 list)
{
    int32_t count = (int16_t)guest_fn_e95c();
    int32_t i = 0;
    uint32_t offset = 0;

    read_parser_line();
    read_parser_token();
    EF_U16(list) = (uint16_t)count;
    EF_U32(list + 2u) = guest_allocate_parser_text((uint32_t)(count * 25), 0);
    if (EF_U32(list + 2u) == 0)
        guest_report_parser_enum_error(0x001810c0u, (uint32_t)(int32_t)(int16_t)EF_U16(list), EF_U32(0x00121eacu));
    while (i < (int16_t)EF_U16(list)) {
        ef_addr32 point = EF_U32(list + 2u) + offset;

        guest_fn_ed80(point);
        if (i > 0 && i != (int16_t)EF_U16(list) - 1) {
            if (0x183 < EF_S32(point))
                EF_U32(point) = 0x183;
            else if (0xc > EF_S32(point))
                EF_U32(point) = 0xc;
            point = EF_U32(list + 2u) + offset;
            if (0x183 < EF_S32(point + 4u))
                EF_U32(point + 4u) = 0x183;
            else if (0xc > EF_S32(point + 4u))
                EF_U32(point + 4u) = 0xc;
        }
        offset += 0x19u;
        i++;
    }
    return 1;
}

/* 0xfbcc, 136 bytes. Read a header and its list of point lists. */
uint32_t read_polyline_list_fbcc(ef_addr32 record)
{
    int32_t i = 0;
    uint32_t offset = 0;

    read_parser_line();
    read_parser_token();
    read_parser_token();
    EF_U16(record + 4u) = (uint16_t)guest_fn_e95c();
    EF_U16(record) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 2u) = (uint16_t)guest_fn_e95c();
    EF_U32(record + 6u) = guest_allocate_parser_text((uint32_t)(((int32_t)EF_U32(record + 2u) >> 16) * 6), 0);
    if (EF_U32(record + 6u) == 0)
        guest_fatal_arg(0x001810ecu, EF_U32(record + 2u) >> 16);
    while (i < ((int32_t)EF_U32(record + 2u) >> 16)) {
        ef_addr32 entry = EF_U32(record + 6u) + offset;

        i++;
        read_point_list_fb10(entry);
        offset += 6u;
    }
    return 1;
}

/* 0xfc54, 159 bytes. Read a group header and its list of 10-byte polyline records. */
uint32_t read_group_header_fc54(ef_addr32 group)
{
    int32_t i = 0;
    uint32_t offset = 0;

    read_parser_line();
    read_parser_token();
    EF_U8(group + 0x1bu) = (uint8_t)guest_fn_ec8c();
    EF_U16(group + 0x1cu) = (uint16_t)guest_fn_e95c();
    /* ORIGINAL BUG: the byte at +0x1b is overwritten by the next number. */
    EF_U8(group + 0x1bu) = (uint8_t)guest_fn_e95c();
    EF_U16(group + 0x1eu) = (uint16_t)guest_fn_e95c();
    read_parser_line();
    read_parser_six_field_record(group + 9u);
    EF_U32(group + 0x20u) = guest_allocate_parser_text((uint32_t)(((int32_t)EF_U32(group + 0x1cu) >> 16) * 10), 0);
    if (EF_U32(group + 0x20u) == 0)
        guest_report_parser_enum_error(0x0018110cu, EF_U32(group + 0x1cu) >> 16, EF_U32(0x00121eacu));
    while (i < ((int32_t)EF_U32(group + 0x1cu) >> 16)) {
        ef_addr32 entry = EF_U32(group + 0x20u) + offset;

        i++;
        read_polyline_list_fbcc(entry);
        offset += 0xau;
    }
    return 1;
}

/* 0xffb4, 103 bytes. Find the node whose name matches; abort unless exactly one matches. */
ef_addr32 find_unique_node_ffb4(ef_addr32 name, ef_addr32 head)
{
    ef_addr32 node;
    int16_t matches = 0;

    for (node = EF_U32(head); node != 0; node = EF_U32(node + 4u)) {
        if (guest_fn_a4380(name, EF_U32(node + 0xcu)) == 0)
            matches++;
    }
    if (matches != 1)
        guest_report_parser_enum_error(0x00181220u, name, (uint32_t)(int32_t)matches);
    for (node = EF_U32(head); node != 0; node = EF_U32(node + 4u)) {
        if (guest_fn_a4380(name, EF_U32(node + 0xcu)) == 0)
            return node;
    }
    return 0;
}
