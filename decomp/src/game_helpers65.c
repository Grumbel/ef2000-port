/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Menu entry activation and table refresh. */

/* 0xdf65c, 218 bytes. Latch the selected menu entry's geometry; returns 1 when an entry was latched. */
uint8_t latch_menu_entry_df65c(void)
{
    int32_t sel = (int8_t)EF_U8(0x0015eff5u);
    uint32_t pos = EF_U32(0x0015f080u) << 8;
    ef_addr32 entry;
    ef_addr32 sub;

    if (sel == -1) {
        EF_U32(0x0015f080u) = pos + 0x22u;
        return 0;
    }
    if (EF_U8(0x0015f088u) == 0) {
        EF_U32(0x0015f080u) = pos + 0x21u;
        return 0;
    }
    entry = 0x00135870u + (uint32_t)sel * 32u;
    EF_U32(0x0015ef54u) = EF_U32(entry + 8u);
    EF_U32(0x0015ef34u) = EF_U32(entry + 8u) + 5u;
    EF_U32(0x0015ef3cu) = EF_U32(entry + 12u);
    EF_U32(0x0015ef58u) = EF_U32(0x0015ef34u);
    EF_U32(0x0015ef5cu) = EF_U32(0x0015ef38u);
    sub = EF_U32(entry);
    EF_U8(sub + 1u) = 3;
    if (EF_U8(0x0015f0a2u) != 0) {
        EF_U8(0x0015f089u) = 1;
    } else {
        EF_U8(0x0015f089u) = 2;
        EF_U8(0x0015f08fu) = 1;
        EF_U16(0x0015f090u) = 0x1f6;
    }
    EF_U8(0x0015ef60u) = 0;
    return 1;
}

/* 0xe3ad0, 134 bytes. Reload the halfword table one step at a time; returns whether more steps remain. */
uint8_t reload_halfword_table_e3ad0(void)
{
    if (EF_U16(0x0015f900u) != 0) {
        int16_t count;
        int16_t i;

        guest_fn_e393c_w();
        count = EF_S16(0x00146e50u);
        for (i = 0; i < count; i++) {
            int32_t r = guest_fn_e3908_w(EF_S32(0x0014ae56u + (uint32_t)i * 2u) >> 16);

            EF_U16(0x0014ae58u + (uint32_t)i * 2u) =
                guest_load_halfword(EF_U32(EF_U32(0x00429228u) + (uint32_t)(int32_t)(int16_t)r * 4u));
        }
        guest_fn_c4654_w(0, EF_U32(0x001590a8u), EF_U32(0x001590acu), EF_U32(0x001590b0u));
        EF_U16(0x0015f900u) = (uint16_t)(EF_U16(0x0015f900u) - 1);
    }
    return EF_U16(0x0015f900u) != 0;
}
