/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Menu back-step and table stream setup. */

/* 0xdf4f4, 360 bytes. Step the menu back one level; returns 1 when an earlier entry was re-latched. */
uint8_t menu_step_back_df4f4(void)
{
    uint32_t edx = EF_U32(0x0015f080u);
    uint32_t shifted = edx << 8;
    ef_addr32 entry;
    int32_t sel;

    EF_U8(0x0015f4fcu) = 1;
    EF_U8(0x0015f089u) = 0;
    EF_U8(0x0015f093u) = 0;
    EF_U8(0x0015ef60u) = 0;
    if (EF_U8(0x0015eff4u) == 0) {
        EF_U8(0x0015f093u) = 1;
        EF_U8(0x0015f4fcu) = 0;
        EF_U32(0x0015f080u) = shifted + 0x11u;
        return 0;
    }
    if ((int8_t)EF_U8(0x0015eff5u) != -1) {
        EF_U8(0x0015f4fcu) = 0;
        EF_U32(0x0015f080u) = shifted + 0x12u;
        return 0;
    }
    if (EF_U8(0x0015f089u) != 0) {
        EF_U8(0x0015f4fcu) = 0;
        EF_U32(0x0015f080u) = shifted + 0x13u;
        return 0;
    }
    if (EF_U8(0x0015f088u) == 0) {
        EF_U8(0x0015f4fcu) = 0;
        EF_U32(0x0015f080u) = shifted + 0x14u;
        return 0;
    }
    EF_U8(0x0015eff4u) = (uint8_t)(EF_U8(0x0015eff4u) - 1);
    EF_U8(0x0015eff5u) = EF_U8(0x0015ef70u + EF_U8(0x0015eff4u));
    sel = (int8_t)EF_U8(0x0015eff5u);
    entry = 0x00135870u + (uint32_t)sel * 32u;
    EF_U32(0x0015ef54u) = EF_U32(entry + 8u);
    EF_U32(0x0015ef34u) = EF_U32(entry + 8u) + 5u;
    EF_U32(0x0015ef3cu) = EF_U32(entry + 12u);
    EF_U32(0x0015ef58u) = EF_U32(0x0015ef34u);
    EF_U32(0x0015ef5cu) = EF_U32(0x0015ef38u);
    EF_U8(EF_U32(entry) + 1u) = 3;
    if (EF_U8(0x0015f0a2u) != 0) {
        EF_U8(0x0015f089u) = 1;
    } else {
        EF_U8(0x0015f089u) = 2;
        EF_U8(0x0015f08fu) = 1;
        EF_U16(0x0015f090u) = 0x1f6;
    }
    EF_U8(0x0015f4fcu) = 0;
    EF_U32(0x0015f080u) = edx;
    return 1;
}

/* 0xe393c, 401 bytes. Stream the saved table blocks (sizes scale with the count at 0x146e50). */
void stream_saved_tables_e393c(void)
{
    uint32_t n;

    guest_fn_e3170_w(0x0015f904u, 8);
    guest_fn_e3170_w(0x001590a8u, 2);
    guest_fn_e3170_w(0x001590acu, 2);
    guest_fn_e3170_w(0x001590b0u, 2);
    guest_fn_e3170_w(0x00146e50u, 2);
    n = EF_U16(0x00146e50u);
    guest_fn_e3170_w(0x00146e56u, (n * 2u) & 0xffffu);
    guest_fn_e3170_w(0x00147e56u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    guest_fn_e3170_w(0x00148e58u, EF_U16(0x00146e50u));
    guest_fn_e330c_w(0, 0x00149658u, EF_U16(0x00146e50u));
    guest_fn_e330c_w(1, 0x00149e58u, EF_U16(0x00146e50u));
    guest_fn_e3170_w(0x0014ae58u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    guest_fn_e330c_w(0, 0x0014be58u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    guest_fn_e330c_w(0, 0x0014ce58u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    guest_fn_e330c_w(0, 0x0014de58u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    guest_fn_e330c_w(0, 0x0014ee58u, (EF_U16(0x00146e50u) * 2u) & 0xffffu);
    guest_fn_e3170_w(0x00150e58u, (EF_U16(0x00146e50u) * 4u) & 0xffffu);
    guest_fn_e3170_w(0x00152e58u, (EF_U16(0x00146e50u) * 4u) & 0xffffu);
    guest_fn_e3170_w(0x00154e58u, (EF_U16(0x00146e50u) * 4u) & 0xffffu);
}
