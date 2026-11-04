/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Unit event notifications. */

static void unit_event_common(ef_addr32 unit, uint32_t arg, uint32_t kind, uint32_t code)
{
    if (unit == 0u)
        return;
    guest_fn_87db8_x(EF_U32(unit), EF_U32(unit + 4u), EF_U32(unit + 8u), 0u, 0u, 0u, 0u, kind, 0u, unit);
    if (EF_U8(0x00127da9u) != 0u)
        send_packet_41((int16_t)EF_S16(unit + 0x16u), (int16_t)arg, (uint8_t)code,
                       (uint16_t)EF_U16(EF_U32(0x001fd9f8u) + 0x51u));
    guest_fn_8873c((int32_t)(int16_t)arg, unit, code, EF_U32(0x001fd9f8u));
}

/* 0x88664, 106 bytes. Raise unit event 0x20 (code 2) for a unit record. */
void unit_event_88664(ef_addr32 unit, uint32_t arg)
{
    unit_event_common(unit, arg, 0x20u, 2u);
}

/* 0x886d0, 106 bytes. Raise unit event 0x1f (code 1) for a unit record. */
void unit_event_886d0(ef_addr32 unit, uint32_t arg)
{
    unit_event_common(unit, arg, 0x1fu, 1u);
}
