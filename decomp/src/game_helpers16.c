/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Confirmation dialog, command broadcast and owner checks, 0x277d8..0x2c6dc. */

/* 0x277d8, 127 bytes. Show the confirm dialog; Enter (0x15) marks the slot record. */
void confirm_dialog_277d8(int16_t slot)
{
    guest_fn_dd370(0, 0x6b, 0x87, 0x0018217cu);
    guest_fn_dd370(0x9c, 0x6a, 0x86, 0x001821b4u);
    guest_fn_dc250();
    do {
        EF_U8(0x0041f833u) = 0;
    } while (EF_U8(0x0041f833u) != 0x15 && EF_U8(0x0041f833u) != 0x31);
    if (EF_U8(0x0041f833u) == 0x15)
        EF_U8(0x0019b438u + (uint32_t)((int32_t)slot * 0x73)) = 1;
    EF_U8(0x0041f833u) = 0x2d;
}

/* 0x2b3b0, 147 bytes. Send a command to each of the owner's seven linked units that accept it. */
void command_linked_units_2b3b0(int16_t owner, uint8_t kind)
{
    int16_t i;

    for (i = 0; i < 7; i++) {
        ef_addr32 record = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)owner * 0xca);
        int16_t id = (int16_t)EF_U16(record + (uint32_t)((int32_t)i * 2) + 0x50u);
        ef_addr32 unit = guest_fn_7489c((int32_t)id);

        if (unit == 0)
            continue;
        if (EF_U8(unit + 0x18du) == 0 || kind == 1 || kind == 7)
            guest_fn_6a1d0((int32_t)id, kind);
    }
}

/* 0x2b444, 194 bytes. Send a command to a unit, then pair it with each accepting unit of its owner. */
void command_unit_and_peers_2b444(int16_t unit_id, uint8_t kind)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);
    int16_t i;

    if (unit == 0)
        return;
    guest_fn_6a1d0((int32_t)unit_id, kind);
    for (i = 0; i < 7; i++) {
        ef_addr32 record = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)(int16_t)EF_U16(unit + 0x28u) * 0xca);
        int16_t peer_id = (int16_t)EF_U16(record + (uint32_t)((int32_t)i * 2) + 0x50u);
        ef_addr32 peer = guest_fn_7489c((int32_t)peer_id);

        if (peer == 0 || peer == unit)
            continue;
        if (EF_U8(peer + 0x18du) == 0 || kind == 1 || kind == 7)
            guest_fn_474b8((int32_t)unit_id, (int32_t)peer_id, kind);
    }
}

/* 0x2c6dc, 127 bytes. Nonzero (1) when the owner fails the check selected by its record type. */
uint8_t owner_check_fails_2c6dc(int16_t owner)
{
    uint8_t type = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)owner * 0xca) + 0x1bu);

    if (type == 0 || type == 0xd) {
        if (guest_fn_2c0ec((int32_t)owner) & 0xffu)
            return 0;
    } else if (type == 2) {
        int16_t mapped = (int16_t)guest_fn_2bbd0((int32_t)owner);

        if (guest_fn_2bfcc((int32_t)mapped) & 0xffu)
            return 0;
        if (guest_fn_2bfcc((int32_t)owner) & 0xffu)
            return 0;
    } else if (type == 9) {
        if (guest_fn_2c17c((int32_t)owner) & 0xffu)
            return 0;
    } else {
        if (guest_fn_2bfcc((int32_t)owner) & 0xffu)
            return 0;
    }
    return 1;
}
