/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Stream, name-table and per-frame update helpers, 0x57714..0x9d170. */

/* 0x57714, 150 bytes. Clear the eight name slots, then store the pending name (upper-cased) in its slot. */
void store_pending_name_57714(void)
{
    int16_t i;
    uint32_t slot;

    for (i = 0; i < 8; i++) {
        EF_U8((uint32_t)((int32_t)i * 0x11) + 0x001a62f0u) = 0;
        EF_U16((uint32_t)i * 2u + 0x001a6378u) = 0;
        EF_U16((uint32_t)i * 2u + 0x001a6388u) = 0;
        EF_U16((uint32_t)i * 2u + 0x001a6398u) = 0;
        EF_U8((uint32_t)i + 0x001a63a8u) = 0;
    }
    if (EF_U16(0x001357aeu) >= 8)
        return;
    if (guest_string_length(0x0013579bu) >= 0x11u)
        return;
    slot = (uint32_t)EF_U16(0x001357aeu) * 17u + 0x001a62f0u;
    copy_string_9d749(slot, 0x0013579bu);
    uppercase_string((ef_addr32)((uint32_t)EF_U16(0x001357aeu) * 17u + 0x001a62f0u));
}

/* 0x62454, 160 bytes. Refresh the flight timers and run the per-frame entry updates. */
void update_flight_62454(void)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);

    if (unit != 0 && EF_U8(0x00122c5cu) == 0)
        EF_U8(0x001a81e1u) = EF_U8(unit + 0x13fu);
    else
        EF_U8(0x001a81e1u) = 0;
    if (EF_U8(0x00122c5cu) == 0) {
        int16_t value = (int16_t)(EF_U16(0x001282aeu) - EF_U16(0x00127d92u));

        EF_U16(0x001282aeu) = (uint16_t)value;
        if (value < 0)
            EF_U16(0x001282aeu) = 0x1964;
    }
    if (EF_U8(0x001a81e1u) != 0 && (int16_t)EF_U16(0x001282aeu) < 0x1388)
        EF_U8(0x001a81e1u) = 0;
    EF_U16(0x001a81ceu) = 0;
    guest_fn_625d8();
    guest_fn_62b84();
    guest_fn_6322c();
    process_entries_62504();
}

/* 0x7775c, 152 bytes. Collect the level-0x20 objects into a new table when the marker word is -1. */
void collect_marked_objects_7775c(void)
{
    ef_addr32 table = EF_U32(0x001f8900u);

    if (EF_U16(0x001f891eu) == 0xffffu && EF_U16(0x001f891cu) != 0) {
        uint16_t remaining;
        int32_t i = 0;

        EF_U16(0x001f891eu) = (uint16_t)guest_lookup_value(0x00186d94u);
        guest_fn_9e3f8((int32_t)(int16_t)EF_U16(0x001f891eu));
        table = guest_fn_b3f58((int32_t)(int16_t)guest_fn_b3db0(((uint32_t)((int32_t)EF_U32(0x001f891au) >> 16)) << 2));
        remaining = EF_U16(0x001f891cu);
        while (remaining != 0) {
            if ((int16_t)i != 0xff) {
                ef_addr32 object = EF_U32(0x0020f550u + (uint32_t)(int32_t)(int16_t)i * 4u);

                if (EF_U8(object + 0xe9u) == 0x20) {
                    remaining--;
                    EF_U32(table + (uint32_t)((int32_t)(int16_t)remaining * 4)) = object;
                }
            }
            i++;
        }
    }
    EF_U32(0x001f8900u) = table;
}

/* 0x97f90, 110 bytes. Pick the table for the detected input device and install the handlers. */
void install_input_tables_97f90(void)
{
    uint32_t device = 0xffffffffu;

    EF_U32(0x0015b964u) = 0x00130eadu;
    if (EF_U8(0x00420a72u) != 0)
        device = guest_fn_d5d18(0x0018ad04u);
    if (device != 0xffffffffu) {
        if (device == 0)
            EF_U32(0x0012dd98u) = 0x0012dd10u;
        else
            EF_U32(0x0012dd98u) = 0x0012dd54u;
    }
    EF_U32(0x0012dd04u) = 0x00098134u;
    EF_U32(0x0012dd08u) = 0x00098144u;
    EF_U8(0x00420a70u) = EF_U8(0x00420a72u);
}

/* 0x9d170, 118 bytes. Read the next byte of a buffered stream, skipping CRs; 0 at end of data. */
uint8_t read_stream_char_9d170(ef_addr32 stream)
{
    for (;;) {
        uint32_t position;
        uint8_t c;

        if ((int32_t)EF_U32(stream + 0x55cu) >= (int32_t)EF_U32(stream + 0x558u)) {
            uint32_t count;

            EF_U32(stream + 0x55cu) = 0;
            count = guest_fn_9cde8(stream + 0x451u, 0x100u, EF_U32(stream + 0x560u));
            EF_U32(stream + 0x558u) = count;
            if (count == 0)
                EF_U8(stream + 0x451u) = 0;
        }
        position = EF_U32(stream + 0x55cu);
        EF_U32(stream + 0x55cu) = position + 1u;
        c = EF_U8(stream + position + 0x451u);
        if (c == 0x0d)
            continue;
        if (c == 0x1a || c == 0xff)
            return 0;
        return c;
    }
}
