/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Flag tests, object reference clearing and player state switch. */

/* 0x69a40, 301 bytes. Test whether any of the low 13 flag bits of a weapon table entry is set. */
uint8_t weapon_flags_any_69a40(int16_t index)
{
    return (EF_U16((uint32_t)(int32_t)index * 0x91u + 0x0012bc95u) & 0x1fffu) != 0;
}

/* 0x8ef38, 270 bytes. Clear nine named object references for the object's id. */
void clear_named_references_8ef38(ef_addr32 object)
{
    static const uint32_t names[9] = {
        0x0018a974u, 0x0018a980u, 0x0018a98cu, 0x0018a994u, 0x0018a99cu,
        0x0018a9a8u, 0x0018a9b4u, 0x0018a9c0u, 0x0018a9ccu
    };
    uint32_t i;

    for (i = 0; i < 9; i++) {
        int16_t v = (int16_t)guest_load_halfword(names[i]);

        clear_object_pair_reference((int16_t)(EF_S32(object + 0x1eu) >> 16), (int16_t)(0x15u + i), v);
    }
}

/* 0x17f10, 289 bytes. Switch the player unit's mode when no blocking state is active. */
void switch_player_mode_17f10(void)
{
    int32_t kind;

    if (EF_U8(0x00132ac2u) != 0 || EF_U8(0x001a363au) != 0 || EF_U8(0x00127d42u) != 0 || EF_U16(0x00126516u) != 0)
        return;
    if (guest_fn_7489c(EF_S32(0x001f71c8u) >> 16) == 0)
        return;
    if (EF_U8(0x00132ac2u) != 0 || EF_U8(0x001a363au) != 0 || EF_U8(0x00127d42u) != 0 || EF_U16(0x00126516u) != 0)
        return;
    guest_fn_7eec4_w();
    if ((EF_S32(EF_U32(0x001f71c4u) + 0x1d4u) >> 24) == 0x14)
        EF_U8(EF_U32(0x001f71c4u) + 0x1d7u) = 0;
    set_secondary_object_byte((uint8_t)guest_fn_85d00_w(1));
    EF_U8(0x00128168u) = 0;
    if ((EF_S32(0x001a5a82u) >> 16) == 1 && (EF_S32(EF_U32(0x001f71c4u) + 0x1d4u) >> 24) == 0x13)
        set_secondary_object_byte((uint8_t)guest_fn_85d00_w(1));
    kind = EF_S32(EF_U32(0x001f71c4u) + 0x1d4u) >> 24;
    if (kind == 0x14) {
        EF_U8(0x00128162u) = 0;
        return;
    }
    if (kind == 0x13)
        reset_ui_panel_selection();
    EF_U8(0x00128162u) = 1;
    EF_U8(0x0012814cu) = 3;
    EF_U8(0x00128168u) = 0;
}
