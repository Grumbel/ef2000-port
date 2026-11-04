/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Segment cursor, grid cell value and table-driven dispatch helpers, 0x49d2c..0x50fcc. */

/* 0x49d2c, 210 bytes. Walk the segment list to the node holding the budget in 0x1a666b. */
void seek_segment_49d2c(ef_addr32 list)
{
    ef_addr32 node = list;
    int16_t i = 0;
    int16_t count = 1;
    int16_t remaining;

    if (EF_U8(0x00125846u) != 3) {
        EF_U8(0x00125837u) = 0;
        EF_U16(0x00125835u) = 0;
        EF_U8(0x00125838u) = 0;
    }
    if (node != 0) {
        int16_t limit = (int16_t)EF_U16(0x00125835u);

        while (i < limit) {
            node = EF_U32(node + 4u);
            i++;
            if (node == 0)
                break;
        }
    }
    remaining = EF_U8(0x001a666bu);
    for (;;) {
        int16_t step = EF_U8(EF_U32(node + 0x13u) + (uint32_t)EF_U8(0x00125837u) * 0x7cu + 1u);

        if (step > remaining)
            break;
        remaining = (int16_t)(remaining - step);
        EF_U8(0x00125837u) = (uint8_t)(EF_U8(0x00125837u) + 1u);
        while (EF_U8(EF_U32(node + 0x13u) + (uint32_t)EF_U8(0x00125837u) * 0x7cu) != 0) {
            if (count > (int16_t)EF_U8(node + 0x12u))
                ui_report_missing_record(0x001847d0u);
            EF_U8(0x00125837u) = (uint8_t)(EF_U8(0x00125837u) + 1u);
            count++;
        }
    }
    EF_U8(0x00125838u) = (uint8_t)remaining;
}

/* 0x49f28, 171 bytes. Look up a word of the unit record owning grid cell (a, b); abort on a bad cell. */
uint16_t grid_cell_word_49f28(int16_t a, int16_t b, int16_t c)
{
    ef_addr32 row;
    int16_t unit;

    if (a >= (int16_t)EF_U16(0x001a53c2u))
        guest_fatal_arg3(0x00184804u, (uint32_t)(int32_t)a, (uint32_t)(int32_t)b, (uint32_t)(int32_t)c);
    row = EF_U32(0x001fcb40u) + (uint32_t)((int32_t)a * 17);
    unit = (int16_t)EF_U16(row + (uint32_t)((int32_t)b * 2) + 1u);
    if (EF_U8(row) != 0)
        guest_fatal_arg3(0x00184844u, (uint32_t)(int32_t)a, (uint32_t)(int32_t)b, (uint32_t)(int32_t)c);
    if (unit == 0xff || unit >= (int16_t)EF_U16(0x0019bb5cu))
        guest_fatal_arg3(0x00184884u, (uint32_t)(int32_t)a, (uint32_t)(int32_t)b, (uint32_t)(int32_t)c);
    return EF_U16(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)unit * 0xca) + (uint32_t)((int32_t)c * 2) + 0x50u);
}

/* 0x4b0d0, 183 bytes. Find the segment holding the budget in 0x1357ae and publish its cell word. */
void locate_segment_4b0d0(ef_addr32 list)
{
    ef_addr32 node = list;
    uint16_t budget = EF_U16(0x001357aeu);
    uint8_t found = 0;
    uint16_t index = 0;
    uint32_t group = 0;

    while (node != 0) {
        ef_addr32 segment = EF_U32(node + 0x13u) + (uint32_t)((int32_t)(int16_t)index * 0x7c);

        if (EF_U8(segment) == 0) {
            int16_t size = EF_U8(segment + 1u);

            if ((int16_t)budget < size)
                found = 1;
            else
                budget = (uint16_t)(budget - size);
        }
        if (found == 0) {
            index++;
            if ((int16_t)index >= (int16_t)EF_U8(node + 0x12u)) {
                group++;
                node = EF_U32(node + 4u);
                index = 0;
            }
        }
        if (found != 0)
            break;
    }
    if (node != 0) {
        EF_U16(0x00125835u) = (uint16_t)group;
        EF_U8(0x00125837u) = (uint8_t)index;
        EF_U8(0x00125838u) = (uint8_t)budget;
    } else {
        guest_fatal_arg(0x00184998u, EF_U16(0x001357aeu));
    }
    EF_U16(0x001f71ceu) = grid_cell_word_49f28((int16_t)((int32_t)EF_U32(0x00125833u) >> 16),
                                               (int16_t)EF_U8(0x00125837u), (int16_t)EF_U8(0x00125838u));
}

/* 0x50f54, 117 bytes. Dispatch the current word through the 10-byte command table. */
uint8_t dispatch_command_50f54(uint8_t mask)
{
    int16_t i = 0;

    if (guest_fn_a4380(0x00185124u, 0x001a5968u) == 0)
        return 4;
    for (;;) {
        ef_addr32 entry = 0x00127b34u + (uint32_t)((int32_t)i * 10);
        ef_addr32 name = EF_U32(entry);

        if (name == 0)
            return 2;
        if (guest_fn_a4380(name, 0x001a5968u) == 0) {
            if ((EF_U8(entry + 8u) & mask) == 0)
                return 1;
            ((void (*)(void))EF_U32(entry + 4u))();
            return 3;
        }
        i++;
    }
}

/* 0x50fcc, 193 bytes. Skip script lines until the current word equals the given name or the end marker. */
void skip_to_word_50fcc(ef_addr32 name)
{
    EF_U32(0x001a58acu) = EF_U32(0x001a58b0u);
    EF_U8(0x00127ce4u) = 0;
    do {
        guest_fn_50ca4();
    } while (EF_U8(0x001a5986u) == 0x3b || guest_string_length(0x001a5986u) < 3);
    guest_fn_50d04();
    while (guest_fn_a4380(name, 0x001a5968u) != 0 && guest_fn_a4380(0x001a5968u, 0x00185128u) != 0) {
        do {
            guest_fn_50ca4();
        } while (EF_U8(0x001a5986u) == 0x3b || guest_string_length(0x001a5986u) < 3);
        guest_fn_50d04();
    }
    if (guest_fn_a4380(0x001a5968u, 0x00185128u) == 0 && guest_fn_a4380(name, 0x00185128u) != 0)
        guest_report_parser_enum_error(0x00185130u, name, 0);
}
