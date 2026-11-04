/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Jump-table dispatchers. */

/* 0x8c988, 103 bytes. Report a tactical action for a record whose mode word (+0x45) selects one of nine action codes. */
void notify_record_action_8c988(ef_addr32 rec)
{
    uint32_t sel;
    if (EF_U16(rec + 0x20u) == 0u)
        return;
    switch (EF_U16(rec + 0x45u)) {
    case 0: sel = EF_U8(0x00130b9cu); break;
    case 1: sel = 0x1bu; break;
    case 2: case 3: case 4: case 5: sel = 0x1eu; break;
    case 6: sel = 0x20u; break;
    case 7: sel = 0x1fu; break;
    case 8: sel = 0x1du; break;
    default: return;
    }
    notify_tactical_action((int32_t)EF_S16(rec + 0x20u), sel);
}

/* 0xcc51c, 103 bytes. Search a linked list (next at +4) for a node whose id byte (+0xb) equals id and whose state byte (+0x2e) satisfies the test chosen by kind. */
uint32_t find_node_matching_cc51c(uint32_t id, uint32_t list, uint32_t kind)
{
    uint32_t node = EF_U32(list);
    uint8_t k = (uint8_t)kind;
    while (node != 0u) {
        if (EF_U8(node + 0xbu) == (uint16_t)id && k <= 0x19u) {
            uint8_t s;
            switch (k) {
            case 0: case 1: case 2: case 4: case 13: case 14: case 15:
            case 20: case 22: case 25:
                return 1u;
            case 17:
                return (uint16_t)id == 0u;
            case 3:
                s = EF_U8(node + 0x2eu);
                if (s == 0u)
                    return 0u;
                if (s <= 3u)
                    return 1u;
                break;
            default:
                s = EF_U8(node + 0x2eu);
                if (s < 2u)
                    return 0u;
                if (s == 2u)
                    return 1u;
                if (s == 3u)
                    return 0u;
                break;
            }
        }
        node = EF_U32(node + 4u);
    }
    return 0u;
}
