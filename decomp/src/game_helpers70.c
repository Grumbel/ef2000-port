/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Contact icon selection. */

static uint8_t contact_target_valid_7f9e4(ef_addr32 unit)
{
    ef_addr32 target = EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(unit + 0x26u) >> 16) * 0xcau;

    return EF_U8(target + 0x20u) != 0 && EF_U8(unit + 0x16cu) != 0xc;
}

/* 0x7f9e4, 440 bytes. Pick the icon code for a unit record (0x47 for none). */
uint8_t select_contact_icon_7f9e4(ef_addr32 unit)
{
    uint8_t kind;
    uint8_t flag;

    if (unit == 0)
        return 0x47;
    kind = EF_U8(unit + 0xe9u);
    flag = EF_U8(0x0021454au);
    if (kind >= 0x18) {
        if (kind <= 0x1b || (kind > 0x1f && kind <= 0x24)) {
            if (!contact_target_valid_7f9e4(unit))
                return 0x47;
            return 0x13;
        }
        if (kind <= 0x1f) {
            if (!contact_target_valid_7f9e4(unit))
                return 0x47;
            return 0x38;
        }
    }
    if (kind != 0) {
        if (!contact_target_valid_7f9e4(unit))
            return 0x47;
        if ((EF_U8(unit + 0x26u) & 8u) == 0)
            return 0x47;
        if ((EF_U8(unit + 0x24u) & 0x20u) != 0)
            return 0x43;
        return flag ? 0x42 : 0x26;
    }
    if (EF_U16(0x001f71cau) == EF_U16(unit + 0x16u)) {
        guest_fn_7f850_x(unit);
        if (EF_U8(0x001a363fu) != 0 || EF_U8(0x001a3638u) != 0) {
            if ((EF_U8(unit + 0x24u) & 0x20u) == 0)
                return flag ? 0x42 : 0x3e;
            return flag ? 0x43 : 0x26;
        }
        if (EF_U16(0x0019724eu) == 0)
            return 0x47;
        return flag ? 0x42 : 0x3e;
    }
    if ((EF_U8(unit + 0x26u) & 8u) == 0)
        return 0x47;
    if ((EF_U8(unit + 0x24u) & 0x20u) == 0)
        return flag ? 0x42 : 0x3e;
    return flag ? 0x43 : 0x26;
}
