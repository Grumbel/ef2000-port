/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Byte swap and record list scans. */

/* 0xc1bd0, 76 bytes. Reverse the byte order of a 32-bit word (xor-swap sequence). */
uint32_t byte_swap32_c1bd0(uint32_t value)
{
    return (value >> 24) | ((value >> 8) & 0xff00u) | ((value << 8) & 0xff0000u) | (value << 24);
}

/* 0xcb184, 111 bytes. Mark up to count pristine records matching type and position as used. */
void mark_matching_records_cb184(uint32_t x, uint32_t y, uint32_t type, uint32_t count, ef_addr32 list)
{
    ef_addr32 rec = EF_U32(list);
    int16_t n = 0;

    while (rec != 0 && n < (int16_t)count) {
        if (EF_U8(rec + 0xbu) == 0
            && EF_U8(rec + 0xeu) == (uint8_t)type
            && (int32_t)(int16_t)x == EF_S32(rec + 0x3bu)
            && (int32_t)(int16_t)y == EF_S32(rec + 0x3fu)
            && EF_U16(rec + 9u) == 0
            && EF_U8(rec + 0x15u) == 100
            && EF_U8(rec + 0xdu) == 0
            && n < (int16_t)count) {
            EF_U8(rec + 0xdu) = 1;
            n++;
            EF_U8(rec + 0x15u) = 0;
        }
        rec = EF_U32(rec + 4u);
    }
}

/* 0xcb614, 116 bytes. For type-3 table entries clear a flag byte of the indexed sub-entry of the n-th list record. */
void clear_indexed_subentry_flags_cb614(ef_addr32 list, ef_addr32 table, uint32_t count)
{
    int16_t i = 0;

    while ((int32_t)i < (int32_t)count) {
        ef_addr32 e = table + (uint32_t)(int32_t)i * 0x36u;
        if ((EF_S32(e + 0xeu) >> 16) == 3) {
            ef_addr32 node = EF_U32(list);
            int16_t j = 0;
            int16_t k;
            while (node != 0 && j < EF_S16(e + 0x12u)) {
                node = EF_U32(node + 4u);
                j++;
            }
            k = EF_S16(e + 0x14u);
            if (k < EF_S16(node + 0x2au) && k >= 0)
                EF_U8(EF_U32(node + 0x2cu) + (uint32_t)(int32_t)k * 0x15u + 5u) = 0;
        }
        i++;
    }
}

/* 0xcb984, 110 bytes. Return 1 when a list record sits at the same position as the object's last table entry and cc1ac accepts the pair. */
uint32_t list_has_matching_entry_cb984(ef_addr32 object, ef_addr32 list, uint32_t extra)
{
    int16_t idx = (int16_t)(EF_U16(EF_U32(object + 0x13u) + 0x1cu) - 1u);
    ef_addr32 rec = EF_U32(list);

    while (rec != 0) {
        ef_addr32 ent = EF_U32(EF_U32(object + 0x13u) + 0x20u) + (uint32_t)(int32_t)idx * 0x19u;
        if (EF_U32(rec + 0x11u) == EF_U32(ent) && EF_U32(rec + 0x15u) == EF_U32(ent + 4u)) {
            if ((uint16_t)guest_fn_cc1ac_x((uint32_t)EF_U8(object + 0xbu), (uint32_t)EF_U8(rec + 0xbu), extra) != 0)
                return 1;
        }
        rec = EF_U32(rec + 4u);
    }
    return 0;
}
