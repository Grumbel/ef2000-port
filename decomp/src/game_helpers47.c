/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* View centering, unit record setup and command lookup helpers, 0x842b4..0x8600c.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0x842b4, 188 bytes. Center the view on the current camera position and publish its origin. */
void center_view_842b4(void)
{
    int16_t offset[4];

    guest_fn_dc91c((ef_addr32)(uintptr_t)offset, 0x001fcc2cu, 0x001fcc14u);
    guest_fn_91afc((int16_t)(-offset[0]), 0);
    guest_fn_c4654(0, (uint32_t)(-(int32_t)offset[1]), (uint32_t)(-(int32_t)offset[0]));
    EF_U32(0x000c4756u) = EF_U32(0x001fcc14u) << 8;
    EF_U32(0x000c475cu) = EF_U32(0x001fcc18u) << 8;
    EF_U32(0x000c4762u) = EF_U32(0x001fcc1cu) << 8;
    EF_U16(0x00159094u) = (uint16_t)((int32_t)EF_U32(0x000c4756u) >> 16);
    EF_U16(0x00159098u) = (uint16_t)((int32_t)EF_U32(0x000c475cu) >> 16);
    EF_U16(0x0015909cu) = (uint16_t)((int32_t)EF_U32(0x000c4762u) >> 16);
    EF_U16(0x001fcc38u) = (uint16_t)(offset[0] - (int16_t)EF_U16(0x001fcc20u));
    EF_U16(0x001fcc3au) = (uint16_t)(offset[1] - (int16_t)EF_U16(0x001fcc22u));
}

/* 0x846c8, 190 bytes. Reset a unit record to its idle state and rebuild the draw defaults. */
void reset_unit_view_846c8(ef_addr32 unit)
{
    uint8_t shift;

    EF_U32(0x001fcbc0u) = EF_U32(unit);
    EF_U32(0x001fcbc4u) = EF_U32(unit + 4u);
    EF_U32(0x001fcbc8u) = EF_U32(unit + 8u);
    EF_U16(unit + 0x20u) = 0xffffu;
    EF_U16(unit + 0x1au) = 3;
    EF_U8(unit + 0x22u) = (uint8_t)(EF_U8(unit + 0x22u) | 4u);
    EF_U32(unit + 0x26u) = 0;
    EF_U16(0x0012b4a4u) = 0;
    EF_U32(unit + 0x2au) = 0;
    EF_U16(unit + 0x22u) = (uint16_t)(EF_U16(unit + 0x22u) & 0xe804u);
    guest_fn_dc832(0x001fcc2cu, 0x001fcbc0u, 0xcu);
    EF_U16(0x0012b4a6u) = 1;
    EF_U16(0x001fcc3eu) = 0;
    shift = (uint8_t)EF_U16(0x0012b4a4u);
    EF_U16(0x001fcc40u) = 0;
    EF_U16(0x001fcc44u) = (uint16_t)(0xa0u << shift);
    EF_U8(0x001fcc42u) = 8;
    EF_U8(0x001fcc43u) = 0;
    EF_U16(0x001fcc46u) = (uint16_t)(0x64u << shift);
}

/* 0x84cfc, 151 bytes. Run the handler whose name matches the first argument of the list. */
void dispatch_named_handler_84cfc(int16_t count, ef_addr32 args)
{
    uint16_t i = 0;

    if (count == 0)
        ui_report_missing_record(0x00189b80u);
    EF_U16(0x001fcc54u) = (uint16_t)count;
    EF_U32(0x001fcc50u) = args;
    for (;;) {
        uint32_t offset = (uint32_t)i << 3;

        if (guest_string_length(EF_U32(0x0012bc14u + offset)) == 0) {
            guest_fatal_arg(0x00189bb8u, EF_U32(args));
            return;
        }
        if (guest_fn_9d524(EF_U32(0x0012bc14u + offset), EF_U32(EF_U32(0x001fcc50u))) == 0) {
            EF_U32(0x001fcc50u) = EF_U32(0x001fcc50u) + 4u;
            EF_U16(0x001fcc54u) = (uint16_t)(EF_U16(0x001fcc54u) - 1u);
            ((void (*)(void))EF_U32(0x0012bc18u + offset))();
            return;
        }
        i++;
    }
}

/* 0x84ff0, 180 bytes. Fill one 25-byte entry of a unit's table (stack arguments). */
void fill_unit_entry_84ff0(int16_t unit, ef_addr32 position, uint32_t u2, uint32_t u3,
                           uint32_t stack_a, uint32_t stack_b, uint32_t stack_index, uint32_t stack_flag,
                           ef_addr32 stack_words)
{
    ef_addr32 owner = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)unit * 0xca);
    uint32_t offset = (stack_index & 0xffu) * 25u;
    ef_addr32 entry;

    (void)u2; (void)u3;
    entry = EF_U32(owner + 0x8au) + offset;
    EF_U32(entry) = EF_U32(position);
    EF_U32(EF_U32(owner + 0x8au) + offset + 4u) = EF_U32(position + 4u);
    EF_U32(EF_U32(owner + 0x8au) + offset + 8u) = stack_a;
    EF_U32(EF_U32(owner + 0x8au) + offset + 0xcu) = stack_b;
    EF_U8(EF_U32(owner + 0x8au) + offset + 0x18u) = (uint8_t)stack_flag;
    EF_U16(EF_U32(owner + 0x8au) + offset + 0x10u) = EF_U16(stack_words);
    EF_U16(EF_U32(owner + 0x8au) + offset + 0x12u) = EF_U16(stack_words + 2u);
    EF_U16(EF_U32(owner + 0x8au) + offset + 0x14u) = EF_U16(stack_words + 4u);
    EF_U16(EF_U32(owner + 0x8au) + offset + 0x16u) = EF_U16(stack_words + 6u);
}

/* 0x850a4, 157 bytes. Allocate a unit's table of 25-byte entries. */
void alloc_unit_entries_850a4(int16_t unit, uint8_t count)
{
    ef_addr32 owner;
    ef_addr32 table;

    if (EF_U32(0x0019bb58u) == 0)
        guest_fatal_arg(0x00189c40u, (uint32_t)(int32_t)unit);
    if (EF_U32(0x001972e0u) == 0)
        guest_fatal_arg(0x00189c60u, (uint32_t)(int32_t)unit);
    table = ((uint32_t (*)(uint32_t, uint32_t))EF_U32(0x0012dd9cu))((uint32_t)count * 25u, count);
    EF_U32(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)unit * 0xca) + 0x8au) = table;
    if (table == 0)
        guest_fatal_arg(0x00189c84u, (uint32_t)(int32_t)unit);
    owner = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)unit * 0xca);
    EF_U8(owner + 0x8eu) = 0;
    EF_U8(owner + 0x8fu) = count;
}

/* 0x8600c, 179 bytes. Find the next slot after the current one that holds the given kind. */
uint16_t find_next_slot_8600c(uint8_t kind)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    int32_t type = (int32_t)EF_U32(unit + 0x1d4u) >> 24;
    int16_t index = (int8_t)EF_U8(unit + 0x1d7u);
    int16_t wanted;

    if (type == 0x14)
        return 0x14;
    wanted = (int8_t)EF_U8(unit + (uint32_t)(type * 3) + 0x19bu);
    for (;;) {
        ef_addr32 slot;
        int32_t slot_type;

        index = (int16_t)(index + 1);
        if (index >= 0x14)
            index = 0;
        if (index == (int8_t)EF_U8(unit + 0x1d7u))
            return 0x14;
        slot = unit + (uint32_t)((int32_t)index * 3);
        slot_type = (int32_t)EF_U32(slot + 0x198u) >> 24;
        if (kind != EF_U8(0x0012bc94u + (uint32_t)(slot_type * 0x91)))
            continue;
        if ((int8_t)EF_U8(slot + 0x19bu) != wanted)
            continue;
        if (EF_U16(slot + 0x19cu) == 0)
            continue;
        if (slot_type == 0x21 || slot_type == 0x22)
            continue;
        return (uint16_t)index;
    }
}
