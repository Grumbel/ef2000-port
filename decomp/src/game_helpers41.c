/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Button-chord mode, countdown slots and input latch helpers, 0x5488..0x1c3d0. */

/* 0x5488, 389 bytes. Pick the mode value from which row/column button pair is pressed. */
void select_chord_mode_5488(void)
{
    ef_addr32 root = guest_open_named_dialog(0x0017f7f8u, 0x0017f860u);
    int16_t a = (EF_U8(guest_select_dialog_widget(root, 0x0017f860u) + 0x1du) & 1u) != 0;
    int16_t b = (EF_U8(guest_select_dialog_widget(root, 0x0017f80cu) + 0x1du) & 1u) != 0;
    int16_t c = (EF_U8(guest_select_dialog_widget(root, 0x0017f86cu) + 0x1du) & 1u) != 0;
    int16_t d = (EF_U8(guest_select_dialog_widget(root, 0x0017f874u) + 0x1du) & 1u) != 0;
    int16_t e = (EF_U8(guest_select_dialog_widget(root, 0x0017f87cu) + 0x1du) & 1u) != 0;
    int16_t f = (EF_U8(guest_select_dialog_widget(root, 0x0017f810u) + 0x1du) & 1u) != 0;

    if (a && d) EF_U8(0x0012583bu) = 9;
    if (a && e) EF_U8(0x0012583bu) = 7;
    if (a && f) EF_U8(0x0012583bu) = 8;
    if (b && d) EF_U8(0x0012583bu) = 3;
    if (b && e) EF_U8(0x0012583bu) = 1;
    if (b && f) EF_U8(0x0012583bu) = 2;
    if (c && d) EF_U8(0x0012583bu) = 6;
    if (c && e) EF_U8(0x0012583bu) = 4;
    if (c && f) EF_U8(0x0012583bu) = 5;
    EF_U32(guest_fn_a7ec4_key(0x0017f6ccu, EF_U8(0x0012583bu))) = EF_U8(0x0012583bu);
}

/* 0x16d60, 255 bytes. Count down the 20 three-byte slots of the given kind by the given amount. */
void count_down_slots_16d60(ef_addr32 base, uint8_t kind, int16_t amount)
{
    int16_t i;

    for (i = 0; i < 0x14; i++) {
        ef_addr32 slot = base + (uint32_t)((int32_t)i * 3);

        if (EF_U8(slot + 0x19bu) != kind)
            continue;
        if (amount >= (int16_t)EF_U16(slot + 0x19cu)) {
            int32_t type;

            amount = (int16_t)(amount - (int16_t)EF_U16(slot + 0x19cu));
            if (EF_U8(0x00125847u) != 0) {
                type = (int32_t)EF_U32(slot + 0x198u) >> 24;
                if (type != 9 && type != 0x14) {
                    guest_fn_8f22c((int32_t)EF_U32(base + 0x14u) >> 16, (uint32_t)i);
                    if (EF_U8(0x00127da9u) != 0)
                        send_object_state_packet((int16_t)((int32_t)EF_U32(base + 0x14u) >> 16), (uint8_t)i);
                }
            }
            EF_U16(base + (uint32_t)((int32_t)i * 3) + 0x19cu) = 0;
        } else {
            int16_t left = (int16_t)(EF_U16(slot + 0x19cu) - amount);

            EF_U16(slot + 0x19cu) = (uint16_t)left;
            if (EF_U8(0x00125847u) != 0 && left == 0) {
                int32_t type = (int32_t)EF_U32(slot + 0x198u) >> 24;

                if (type != 9 && type != 0x14) {
                    guest_fn_8f22c((int32_t)EF_U32(base + 0x14u) >> 16, (uint32_t)i);
                    if (EF_U8(0x00127da9u) != 0)
                        send_object_state_packet((int16_t)((int32_t)EF_U32(base + 0x14u) >> 16), (uint8_t)i);
                }
            }
        }
    }
}

/* 0x1c3d0, 361 bytes. Latch the raw input state, clearing it when a menu or pause is active. */
void latch_input_state_1c3d0(void)
{
    uint16_t pending;

    if (EF_U8(0x00132ac2u) != 0 || EF_U8(0x001a363au) != 0 || EF_U8(0x00127d42u) != 0
        || EF_U16(0x00126516u) != 0)
        return;
    EF_U16(0x00198434u) = EF_U8(0x0041f7a8u);
    pending = EF_U16(0x00198422u);
    if (pending != 0 || EF_U16(0x00198420u) != 0 || EF_U16(0x0019843cu) != 0) {
        uint16_t latched = EF_U16(0x0019843cu);

        EF_U16(0x0019845au) = 0;
        EF_U16(0x00198452u) = 0;
        EF_U16(0x0019845cu) = 0;
        EF_U16(0x00198458u) = 0;
        EF_U16(0x00198454u) = 0;
        EF_U16(0x00198456u) = 0;
        EF_U16(0x00198440u) = 0;
        EF_U16(0x00198436u) = 0;
        EF_U16(0x0019842au) = 0;
        EF_U16(0x0019842cu) = 0;
        if (latched != 0) {
            EF_U16(0x0019843au) = EF_U8(0x0041f7c3u);
            EF_U16(0x0019843eu) = EF_U8(0x0041f7c4u);
        }
        return;
    }
    EF_U16(0x0019845au) = EF_U8(0x0041f801u);
    EF_U16(0x00198452u) = EF_U8(0x0041f809u);
    EF_U16(0x0019845cu) = EF_U8(0x0041f805u);
    EF_U16(0x00198458u) = EF_U8(0x0041f808u);
    EF_U16(0x00198454u) = (uint16_t)(EF_U8(0x0041f7deu) | EF_U8(0x0041f79du));
    EF_U16(0x00198440u) = pending;
    EF_U16(0x00198456u) = (uint16_t)(EF_U8(0x0041f7dau) | EF_U8(0x0041f79cu));
    EF_U16(0x00198436u) = 0;
    EF_U16(0x0019842au) = EF_U8(0x0041f7c3u);
    EF_U16(0x0019842cu) = EF_U8(0x0041f7c4u);
    EF_U16(0x0019843au) = pending;
    EF_U16(0x0019843eu) = pending;
}
