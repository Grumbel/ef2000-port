/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Fade, state reset and target-eligibility helpers, 0x6322c..0x76f58. */

/* 0x6322c, 206 bytes. Step the damage level down when the weakest live part dropped far enough. */
void step_damage_level_6322c(void)
{
    uint16_t count = EF_U16(0x001a81c2u);

    if (EF_U8(0x001a81e6u) != 0) {
        if (EF_U16(0x001a81ceu) == 0) {
            if ((int8_t)EF_U8(0x0012821eu) != 4)
                EF_U8(0x0012821eu) = (uint8_t)(EF_U8(0x0012821eu) + 1u);
        } else {
            int32_t lowest = EF_S32(0x001a7d10u);
            int16_t i;

            for (i = 0; i < (int16_t)count; i++) {
                uint32_t offset = (uint32_t)((int32_t)i * 0x17);
                uint8_t kind = EF_U8(offset + 0x001a7d33u);

                if (kind != 3 && kind != 4 && EF_U8(offset + 0x001a7d31u) == 0) {
                    int32_t value = EF_S32(offset + 0x001a7d36u);

                    if (lowest > value)
                        lowest = value;
                }
            }
            if (lowest < EF_S32(0x001a7d10u) && EF_U8(0x0012821eu) != 0) {
                int32_t level = (int8_t)EF_U8(0x0012821eu);
                int32_t base = EF_S32(0x0012823cu + (uint32_t)(level * 4));

                base = (int32_t)((uint32_t)base * (uint32_t)base);
                if (lowest < (int32_t)((uint32_t)base * 9u) / 10)
                    EF_U8(0x0012821eu) = (uint8_t)(EF_U8(0x0012821eu) - 1u);
            }
        }
    }
    EF_U16(0x001a81c2u) = count;
}

/* 0x65960, 202 bytes. Reset the target-tracking state. */
void reset_target_state_65960(void)
{
    int32_t owner;

    EF_U32(0x001a821cu) = 0;
    EF_U32(0x001a8210u) = 0;
    EF_U8(0x001282c5u) = 0;
    EF_U8(0x001a8245u) = 0;
    owner = (int32_t)EF_U32(0x001f71cau) >> 16;
    EF_U16(0x001a823au) = 0xffffu;
    EF_U16(0x001a8238u) = 0xffffu;
    EF_U16(0x001a823eu) = 0xffffu;
    EF_U16(0x001a8240u) = 0xffffu;
    EF_U16(0x001a8242u) = 0xffffu;
    EF_U16(0x001a8234u) = 0xffffu;
    EF_U16(0x001a8230u) = 0xffffu;
    EF_U16(0x001a822cu) = 0xffffu;
    if (owner != 0xff)
        EF_U16(0x001a823cu) = EF_U16(EF_U32(0x0019bb58u) + (uint32_t)(owner * 0xca) + 0x50u);
    else
        EF_U16(0x001a823cu) = 1;
    EF_U8(0x001282c4u) = 0;
    EF_U32(0x001a8224u) = 0;
    EF_U32(0x001a8218u) = 0;
    EF_U32(0x001a820cu) = 0;
    EF_U32(0x001a8214u) = 0;
    EF_U32(0x001a8220u) = 0;
    EF_U32(0x001a8208u) = 0;
    EF_U16(0x001a8236u) = 0xff;
}

/* 0x6a82c, 207 bytes. True when the second unit is a valid hostile target for the first. */
uint8_t is_valid_target_6a82c(int16_t first, int16_t second)
{
    ef_addr32 attacker;
    ef_addr32 target;
    ef_addr32 base;
    uint8_t attacker_team;
    uint8_t target_team;

    if (first == 0xff || second == 0xff)
        return 0;
    attacker = guest_fn_7489c((int32_t)first);
    if (attacker == 0)
        return 0;
    target = guest_fn_7489c((int32_t)second);
    if (target == 0)
        return 0;
    if ((EF_U8(target + 0x24u) & 4u) != 0 || (EF_U8(target + 0x24u) & 8u) != 0 || EF_U8(target + 0x18du) != 0)
        return 0;
    if ((EF_U8(target + 0x24u) & 2u) != 0 && (guest_fn_7574c((int32_t)second) & 0xffu) == 0)
        return 0;
    base = EF_U32(0x0019bb58u);
    attacker_team = EF_U8(base + (uint32_t)(((int32_t)EF_U32(attacker + 0x26u) >> 16) * 0xca) + 0x15u);
    target_team = EF_U8(base + (uint32_t)(((int32_t)EF_U32(target + 0x26u) >> 16) * 0xca) + 0x15u);
    if (grid_event_matches(target_team, attacker_team) == 0)
        return 0;
    base = EF_U32(0x0019bb58u);
    return EF_U8(base + (uint32_t)(((int32_t)EF_U32(target + 0x26u) >> 16) * 0xca) + 0x20u) == 1;
}

/* 0x76948, 213 bytes. Fade the 256-entry palette towards white by the fade counter. */
void fade_palette_76948(void)
{
    int16_t fade = (int16_t)(EF_U16(0x0012651cu) - 10u);
    int16_t i;
    int32_t factor;

    EF_U16(0x0012651cu) = (uint16_t)fade;
    if (fade < 0) {
        EF_U16(0x0012651cu) = 0;
        return;
    }
    factor = (int16_t)EF_U16(0x0012651cu);
    for (i = 0; i < 0x100; i++) {
        uint32_t offset = (uint32_t)((int32_t)i * 3);
        int32_t r = (int8_t)EF_U8(offset + 0x001f74e0u);
        int32_t g;
        int32_t b;

        EF_U8(offset + 0x001f74e0u) = (uint8_t)(((0x3f - r) * factor) / 0xc8 + r);
        g = (int8_t)EF_U8(offset + 0x001f74e1u);
        EF_U8(offset + 0x001f74e1u) = (uint8_t)(((0x3f - g) * factor) / 0xc8 + g);
        b = (int8_t)EF_U8(offset + 0x001f74e2u);
        EF_U8(offset + 0x001f74e2u) = (uint8_t)(((0x3f - b) * factor) / 0xd2 + b);
    }
    EF_U8(0x00128548u) = 1;
    EF_U16(0x001f88ecu) = 2;
}

/* 0x76f58, 218 bytes. Report the formation members and leader to the roster function 0x76b30. */
void report_formation_76f58(uint32_t roster, ef_addr32 formation)
{
    uint8_t i;
    ef_addr32 leader;

    for (i = 0; i < 7; i++) {
        ef_addr32 unit = guest_fn_7489c((int32_t)EF_U32(formation + (uint32_t)i * 2u + 3u) >> 16);
        uint32_t value = 0xffu;

        if (unit != 0) {
            ef_addr32 owner = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca);

            if (EF_U8(owner + 0x20u) == 1)
                value = (uint32_t)((int32_t)EF_U32(unit + 0x14u) >> 16);
        }
        guest_fn_76b30(roster, i, value);
    }
    guest_fn_76b30(roster, 7, 0xffu);
    leader = guest_fn_7489c((int32_t)EF_U32(formation + 0xdu) >> 16);
    if (leader != 0) {
        ef_addr32 owner = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(leader + 0x26u) >> 16) * 0xca);
        uint32_t current = EF_U16(owner + (uint32_t)EF_U8(owner + 0x4fu) * 2u + 0x4eu);

        if ((uint32_t)((int32_t)EF_U32(leader + 0x14u) >> 16) != current) {
            uint16_t word = EF_U16(owner + (uint32_t)EF_U8(leader + 0x2au) * 2u + 0x52u);

            guest_fn_76b30(roster, 7, (uint32_t)(int32_t)(int16_t)word);
        }
    }
}
