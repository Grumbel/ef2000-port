/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* printf flag parser and MIDI channel state lookup. */

/* 0xed4e7, 91 bytes. Parse the printf flag characters '-', '#', '+', ' ' and '0' into spec+0x14 / spec+0x16; returns the pointer after the flags. */
ef_addr32 parse_output_flags_ed4e7(ef_addr32 fmt, ef_addr32 spec)
{
    EF_U8(spec + 0x14u) = 0;
    for (;;) {
        uint8_t c = EF_U8(fmt);

        if (c == 0x2d) {
            EF_U8(spec + 0x14u) = (uint8_t)(EF_U8(spec + 0x14u) | 8u);
        } else if (c == 0x23) {
            EF_U8(spec + 0x14u) = (uint8_t)(EF_U8(spec + 0x14u) | 1u);
        } else if (c == 0x2b) {
            EF_U8(spec + 0x14u) = (uint8_t)((EF_U8(spec + 0x14u) | 4u) & 0xfdu);
        } else if (c == 0x20) {
            if ((EF_U8(spec + 0x14u) & 4u) == 0)
                EF_U8(spec + 0x14u) = (uint8_t)(EF_U8(spec + 0x14u) | 2u);
        } else if (c == 0x30) {
            EF_U8(spec + 0x16u) = c;
        } else {
            return fmt;
        }
        fmt++;
    }
}

/* 0x112048, 408 bytes. Read a stored MIDI channel value: program change (0xc0), pitch bend (0xe0, two 7-bit halves) or a controller (0xb0) from the per-channel tables; -1 when unknown. */
uint32_t get_channel_value_112048(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 player, uint32_t status, uint32_t controller)
{
    ef_addr32 slot = player + (status & 0xfu) * 4u;
    uint32_t type = status & 0xf0u;

    (void)u0; (void)u1; (void)u2; (void)u3;
    if (type == 0xc0)
        return EF_U32(slot);
    if (type == 0xe0)
        return (EF_U32(slot + 0x80u) << 7) | EF_U32(slot + 0x40u);
    if (type == 0xb0) {
        switch (controller) {
        case 0x01: return EF_U32(slot + 0x280u);
        case 0x06: return EF_U32(slot + 0x440u);
        case 0x07: return EF_U32(slot + 0x2c0u);
        case 0x0a: return EF_U32(slot + 0x300u);
        case 0x0b: return EF_U32(slot + 0x340u);
        case 0x40: return EF_U32(slot + 0x380u);
        case 0x5b: return EF_U32(slot + 0x3c0u);
        case 0x5d: return EF_U32(slot + 0x400u);
        case 0x6b: return EF_U32(slot + 0x140u);
        case 0x6e: return EF_U32(slot + 0xc0u);
        case 0x6f: return EF_U32(slot + 0x100u);
        case 0x70: return EF_U32(slot + 0x180u);
        case 0x72: return EF_U32(slot + 0x1c0u);
        case 0x73: return EF_U32(slot + 0x200u);
        case 0x77: return EF_U32(slot + 0x240u);
        default: break;
        }
    }
    return 0xffffffffu;
}
