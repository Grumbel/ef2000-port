/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Selection state, schedule checks and slot matching, 0x1ad8..0x7b38. */

/* 0x1ad8, 150 bytes. Reset the selection state block at 0x196fe8. */
void reset_selection_state_1ad8(void)
{
    EF_U8(0x00196fe8u) = 0;
    EF_U8(0x00196fe9u) = 5;
    EF_U8(0x00196ff1u) = 0;
    EF_U8(0x00196ff2u) = 0;
    EF_U32(0x00196ff7u) = 0;
    EF_U8(0x0019707du) = 0;
    EF_U8(0x0019707eu) = 0;
    EF_U32(0x00197061u) = 0;
    EF_U32(0x00197065u) = 0;
    EF_U32(0x00197069u) = 0;
    EF_U32(0x00197079u) = 0;
    EF_U16(0x00196fecu) = 0;
    EF_U8(0x00197080u) = 0xff;
    EF_U16(0x00197089u) = 0;
    EF_U32(0x00196ff3u) = 0;
    EF_U32(0x00197071u) = 0;
    EF_U32(0x00197075u) = 0;
    EF_U8(0x0019707fu) = 0;
}

/* 0x2d78, 117 bytes. True when both owner bytes of the 62-byte record match and the time is in range. */
uint8_t schedule_allows_2d78(uint8_t owner, uint8_t other, int16_t index, uint32_t time)
{
    ef_addr32 record = 0x001963cau + (uint32_t)((int32_t)index * 62);
    uint8_t first = EF_U8(record);
    uint8_t second = EF_U8(record + 1u);
    uint32_t end;

    if (first != owner && first != other && first != 0)
        return 0;
    if (second != owner && second != other && second != 0)
        return 0;
    if (time <= EF_U32(record + 4u))
        return 0;
    end = EF_U32(record + 8u);
    if (time < end || end == 0)
        return 1;
    return 0;
}

/* 0x3d74, 131 bytes. Refresh the selected state byte from the owner and report a change. */
uint8_t refresh_selection_3d74(void)
{
    uint8_t changed = 0; /* ORIGINAL BUG: the unchanged path returns an uninitialised DL. */
    uint32_t owner = (uint32_t)((int32_t)EF_U32(0x0019707fu) >> 16);
    ef_addr32 record = EF_U32(0x0019bb58u) + owner * 0xcau;
    uint8_t state = (uint8_t)guest_fn_3aa8(EF_U8(record + 0x1au));

    record = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)EF_U32(0x0019707fu) >> 16) * 0xcau;
    if (EF_U8(record + 0x1au) == 2)
        state = (uint8_t)guest_fn_3b1c(EF_U8(EF_U32(0x00197061u) + 0x16du));
    if (state != EF_U8(0x00197080u)) {
        EF_U8(0x0019707fu) = EF_U8(0x00196fcau + state);
        EF_U8(0x00197080u) = state;
        changed = 1;
    }
    return changed;
}

/* 0x7b38, 134 bytes. True when the unit's slot entry matches its type and refers to it (or to anyone). */
uint8_t slot_matches_unit_7b38(uint8_t group, uint8_t slot, int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);
    ef_addr32 entry;
    int16_t target;

    if (unit == 0)
        return 0;
    entry = EF_U32(EF_U32(0x001972e0u) + (uint32_t)group * 34u + 0x1eu) + (uint32_t)slot * 25u;
    if (EF_U8(entry + 0xcu) != EF_U8(0x00128568u + (uint32_t)EF_U8(unit + 0xe9u) * 0xe5u))
        return 0;
    target = (int16_t)EF_U16(entry + 0xdu);
    if (target == 0xff)
        return 1;
    return (uint16_t)unit_id == EF_U16(entry + 0xdu);
}
