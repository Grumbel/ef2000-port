/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Sound slot select, record counters and text line/table readers, 0x98050..0x9d990.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0x98050, 141 bytes. Switch the active sound slot (0..3), restarting the driver when it fails. */
void select_sound_slot_98050(uint8_t slot)
{
    if (EF_U8(0x00420a72u) != 0 && EF_U8(0x00420a70u) == 1) {
        if (slot >= 4)
            guest_fatal_arg(0x0018ad0cu, slot);
        if (slot != EF_U8(0x0012dd0du)) {
            reset_sound_slot_980e8(slot);
            if (guest_fn_d5e0c(EF_U32(0x0012dd98u) + (uint32_t)slot * 13u) != 0) {
                EF_U8(0x00420a70u) = 0;
                EF_U8(0x00420a72u) = 0;
                guest_fn_82e40();
            }
        }
        EF_U8(0x0012dd0du) = slot;
        EF_U8(0x0012dd0cu) = 1;
    }
}

/* 0x9a624, 213 bytes. Dispatch every active 54-byte record by kind and sum the four counters. */
void update_records_9a624(void)
{
    int16_t i;
    int32_t j;

    for (i = 0; (int32_t)i < (int32_t)EF_U32(0x001379b0u); i++) {
        ef_addr32 record = EF_U32(0x001379bcu) + (uint32_t)((int32_t)i * 0x36);

        if (((int32_t)EF_U32(record + 0xau) >> 16) == 1) {
            int16_t value = (int16_t)guest_fn_9a6fc(record);

            switch (EF_U16(record + 0x10u)) {
            case 0:
                guest_fn_9a8b4(record, (int32_t)value);
                break;
            case 1:
            case 2:
                guest_fn_9a998(record);
                break;
            case 3:
                count_event_9aa78(record, value);
                break;
            default:
                break;
            }
        }
    }
    for (j = 0; j < 4; j++) {
        EF_U16(0x0040d008u) = (uint16_t)(EF_U16(0x0040d008u) + EF_U16(0x0040d00eu + (uint32_t)j * 2u));
        EF_U16(0x0040d00au) = (uint16_t)(EF_U16(0x0040d00au) + EF_U16(0x0040d016u + (uint32_t)j * 2u));
        EF_U16(0x0040d00cu) = (uint16_t)(EF_U16(0x0040d00cu) + EF_U16(0x0040d01eu + (uint32_t)j * 2u));
    }
    guest_fn_9b780();
    guest_fn_9b8a0();
}

/* 0x9aa78, 182 bytes. Count an event kind in the counter row of the current slot. */
void count_event_9aa78(ef_addr32 record, int16_t kind)
{
    int16_t slot = (int16_t)EF_U16(0x0040d028u);
    uint32_t base;

    (void)record;
    switch (kind) {
    case 0:  base = 0x0040cfdeu; break;
    case 0x13: base = 0x0040cfe4u; break;
    case 0x1d: base = 0x0040cfeau; break;
    case 0x26: base = 0x0040cff0u; break;
    case 0x31: base = 0x0040cff6u; break;
    case 0x49: base = 0x0040cffcu; break;
    case 0x4f: base = 0x0040d002u; break;
    default:   base = 0; break;
    }
    if (base != 0)
        EF_U16(base + (uint32_t)((int32_t)slot * 2)) = (uint16_t)(EF_U16(base + (uint32_t)((int32_t)slot * 2)) + 1u);
    EF_U16(0x0040d028u) = (uint16_t)slot;
}

/* 0x9d1e8, 141 bytes. Read the next non-comment text line of a stream into its buffer. */
void read_text_line_9d1e8(ef_addr32 stream)
{
    uint8_t c;
    uint32_t length;

    EF_U16(stream + 0x556u) = 0;
    do {
        c = (uint8_t)read_stream_char_9d170(stream);
        length = 0;
        while (c == 0x20 || c == 9)
            c = (uint8_t)read_stream_char_9d170(stream);
        while (c != 0 && c != 0xa) {
            if ((int32_t)length >= 0x400)
                guest_fatal_arg0(0x0018b1f0u);
            EF_U8(stream + length) = c;
            length++;
            c = (uint8_t)read_stream_char_9d170(stream);
        }
        EF_U8(stream + length) = 0;
        EF_U32(stream + 0x552u) = EF_U32(stream + 0x552u) + 1u;
    } while (EF_U8(stream) == 0x3b || (EF_U8(stream) == 0 && c != 0));
}

/* 0x9d990, 191 bytes. Load the quoted-string file into an arena block and index its strings. */
void load_string_table_9d990(void)
{
    uint8_t path[0x90];
    ef_addr32 buf = (ef_addr32)(uintptr_t)path;
    uint32_t length;
    ef_addr32 text;
    ef_addr32 cursor;
    uint16_t count;
    ef_addr32 table;
    uint16_t i = 0;

    copy_string_9d749(buf, 0x0041719au);
    guest_fn_9ca57(buf, 0x0018b23cu);
    length = ((uint32_t (*)(ef_addr32))EF_U32(0x00132ab4u))(buf);
    if (length == 0)
        ui_report_missing_record(0x0018b248u);
    text = arena_take_e29e8(length);
    EF_U32(0x00132b0cu) = text;
    cursor = text;
    ((void (*)(ef_addr32, ef_addr32))EF_U32(0x00132ab8u))(buf, text);
    count = (uint16_t)guest_fn_9d964(text);
    table = arena_take_e29e8((uint32_t)count << 2);
    if (count != 0) {
        do {
            while (EF_U8(cursor) != 0x22)
                cursor++;
            cursor++;
            EF_U32(table + (uint32_t)i * 4u) = cursor;
            i++;
            while (EF_U8(cursor) != 0x22)
                cursor++;
            EF_U8(cursor) = 0;
            cursor++;
        } while (i < count);
    }
    EF_U32(0x00132b08u) = table;
}
