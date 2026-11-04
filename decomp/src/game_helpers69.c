/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Player weapon hold timer and the per-frame simulation step. */

static int player_blocked_17754(void)
{
    return EF_U8(0x00132ac2u) != 0 || EF_U8(0x001a363au) != 0 || EF_U8(0x00127d42u) != 0 || EF_U16(0x00126516u) != 0;
}

/* 0x17754, 517 bytes. Track the trigger hold time for the selected station, firing the release notifications. */
void update_trigger_hold_17754(void)
{
    ef_addr32 obj;
    ef_addr32 slot;
    ef_addr32 player;
    int32_t kind;

    if (player_blocked_17754())
        return;
    obj = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    if (obj == 0)
        return;
    if ((EF_U8(obj + 0x24u) & 4u) != 0 || (EF_U8(obj + 0x24u) & 8u) != 0)
        return;
    if (player_blocked_17754() || EF_U8(0x001a3634u) != 0 || EF_U8(0x0012814cu) == 6)
        return;
    slot = obj + (uint32_t)(EF_S32(obj + 0x1d4u) >> 24) * 3u;
    if (EF_U16(slot + 0x19cu) == 0)
        return;
    if ((EF_S32(slot + 0x198u) >> 24) == 1 && (EF_S32(0x00122c54u) >> 16) <= 0xaa && EF_U8(0x00198463u) != 0) {
        EF_U16(0x00122c56u) = (uint16_t)(EF_U16(0x00122c56u) + EF_U16(0x00127d92u));
        return;
    }
    EF_U16(0x00122c56u) = 0;
    if ((EF_S32(slot + 0x198u) >> 24) == 1 && EF_U8(obj + 0xe9u) == 0) {
        if (EF_U8(0x00198463u) == 0)
            notify_tactical_action(EF_S32(obj + 0x1eu) >> 16, 0xf);
        EF_U8(0x00198463u) = 2;
    }
    player = EF_U32(0x001f71c4u);
    kind = EF_S32(player + (uint32_t)(EF_S32(player + 0x1d4u) >> 24) * 3u + 0x198u) >> 24;
    if (EF_U8((uint32_t)kind * 0x91u + 0x0012bcacu) == 1)
        EF_U8(0x00198464u) = 0;
    if (EF_U8(0x00198464u) != 0)
        return;
    player = EF_U32(0x001f71c4u);
    kind = EF_S32(player + (uint32_t)(EF_S32(player + 0x1d4u) >> 24) * 3u + 0x198u) >> 24;
    if (EF_U8((uint32_t)kind * 0x91u + 0x0012bc94u) == 1)
        EF_U8(0x00128159u) = 5;
    guest_fn_86110_x();
}

/* 0x1cde8, 496 bytes. One simulation frame: input latch, unit updates and the sub-step loop. */
void simulation_frame_1cde8(void)
{
    ef_addr32 obj;
    uint32_t e;
    uint32_t d;
    uint32_t steps;
    uint32_t i;

    EF_U8(0x001a3647u) = EF_U8(0x00198430u);
    obj = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    if (obj != 0) {
        EF_U8(obj + 0x26u) = (uint8_t)(EF_U8(obj + 0x26u) & 0xdfu);
        if (EF_U8(0x00198463u) != 0) {
            EF_U8(0x00198463u) = (uint8_t)(EF_U8(0x00198463u) - 1);
            if (EF_U8(0x00198463u) == 0)
                notify_tactical_action(EF_S32(obj + 0x1eu) >> 16, 0x10);
        }
    }
    if (guest_fn_4904c_x() == 0) {
        if (EF_U8(0x0012db9eu) != 0x21)
            guest_fn_1c16c_x();
        guest_fn_1a3cc_x();
        guest_fn_1a47c_x();
    }
    guest_fn_1c3d0_x();
    EF_U32(0x001258f8u) = EF_U32(0x001258f8u) + 1u;
    EF_U8(0x001a3647u) = EF_U8(0x001a3649u);
    EF_U8(0x001a3647u) = (uint8_t)(EF_U8(0x001a3647u) | (EF_U16(0x00127daau) != 0 ? 1u : 0u));
    e = (uint32_t)(EF_S32(0x00127da8u) >> 16);
    d = EF_U16(0x00127d92u);
    if ((int32_t)d >= (int32_t)e)
        d = e;
    EF_U16(0x00127daau) = (uint16_t)(e - d);
    if (EF_U8(0x001fcb30u) == 1) {
        EF_U16(0x0019841au) = EF_U8(0x0015ba2cu);
        EF_U16(0x00198414u) = EF_U8(0x0015ba2du);
        EF_U16(0x00198416u) = EF_U8(0x0015ba2eu);
        EF_U16(0x00198418u) = EF_U8(0x0015ba2fu);
        guest_fn_1c780_x();
    }
    if (guest_fn_4904c_x() == 0) {
        if (EF_U8(0x0041f7c9u) != 0) {
            if (EF_U16(0x00198424u) == 0)
                guest_fn_1abbc_x();
        } else {
            EF_U16(0x00198424u) = 0;
        }
    }
    EF_U16(0x00198430u) = EF_U8(0x001a3647u);
    guest_fn_53270_x();
    if (EF_U8(0x0012db9eu) == 0x45 || EF_U8(0x0012db9eu) == 0x42) {
        uint32_t q = EF_U16(0x00127d92u) / 3u;

        if ((int32_t)q <= 1)
            q = 1;
        EF_U16(0x00127d92u) = (uint16_t)q;
    }
    if (EF_U8(0x001fcb30u) == 1)
        guest_fn_5331c_x();
    if ((EF_S32(0x001f71c8u) >> 16) != 0xff && EF_U8(0x00122c5du) == 0) {
        steps = EF_U8(0x001a3647u) != 0 ? 5u : 1u;
        for (i = 1; i <= steps; i++) {
            if (EF_U8(0x001a363bu) != 0)
                guest_fn_1cfd8_x();
            guest_fn_791e0_x();
            guest_fn_53ecc_x();
            guest_fn_7957c_x();
        }
    }
    guest_fn_1cd90_x();
}
