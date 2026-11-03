/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Effect-ring drawing and handler installation, 0x89bf0..0x8f9e8. */

/* 0x89bf0, 115 bytes. True when a random value modulo the configured period is within the tick budget. */
uint8_t period_elapsed_89bf0(void)
{
    uint8_t result = 0;
    int16_t period = (int16_t)EF_U8(0x001264e6u + (uint32_t)EF_U8(0x001264e2u) * 5u);

    if (period != 0) {
        int32_t value = (int32_t)(half_word_9c960() & 0xffffu);

        if (!((value % (int32_t)period) > (int32_t)EF_U16(0x00127d92u)))
            result = 1;
    }
    return result;
}

/* 0x8b988, 95 bytes. Draw every effect of the ring at 0x12db5c that is close to (x, y). */
void draw_nearby_effects_8b988(int32_t x, int32_t y)
{
    ef_addr32 effect = EF_U32(0x0012db5cu);

    while (effect != 0) {
        if ((int32_t)guest_fn_cf94a(EF_U32(effect + 0xfu) - (uint32_t)x) < 0x400000
            && (int32_t)guest_fn_cf94a(EF_U32(effect + 0x17u) - (uint32_t)y) < 0x400000) {
            guest_fn_9e0c4((int16_t)EF_U16(effect + 0x1bu), EF_U32(effect + 0xfu) << 8,
                           EF_U32(effect + 0x13u) << 8, EF_U32(effect + 0x17u) << 8);
        }
        effect = EF_U32(effect + 0x1du);
    }
}

/* 0x8ba30, 70 bytes. Allocate the sprite for an effect and link it at the head of the ring. */
void link_effect_8ba30(ef_addr32 effect)
{
    uint32_t base = guest_fn_8fdf0((int16_t)EF_U16(effect + 0x1bu));

    EF_U16(effect + 0x1bu) = (uint16_t)call_9de08_9ddfc(base + 0x15u);
    if (EF_U16(effect + 0x21u) != 0)
        guest_fn_9e508((int16_t)EF_U16(effect + 0x1bu), (uint32_t)(int32_t)(int16_t)EF_U16(effect + 0x21u));
    EF_U32(effect + 0x1du) = EF_U32(0x0012db5cu);
    EF_U32(0x0012db5cu) = effect;
}

/* 0x8bb18, 54 bytes. Draw the effects around the camera position. */
void draw_effects_at_camera_8bb18(void)
{
    uint32_t x = EF_U32(0x0021444cu) << 8;
    uint32_t y = EF_U32(0x00214448u) << 8;
    uint32_t z = EF_U32(0x00214450u) << 8;

    guest_fn_8e664(x, y, z);
    draw_nearby_effects_8b988((int32_t)x, (int32_t)z);
}

/* 0x8c930, 52 bytes. Select the page slot for a page id, or 0 when the page table lacks it. */
uint32_t select_slot_8c930(int16_t page_id)
{
    uint32_t table = guest_fn_a1a40_key(0x0018a8dcu);
    uint32_t entry;

    if (table == 0)
        return 0;
    entry = find_page_pair_a1a90(page_id, (int32_t)table);
    if (entry == 0)
        return 0;
    return select_page_slot_a0de0(page_id, (int16_t)EF_U16(entry));
}

/* 0x8e5f0, 115 bytes. Draw a record: low records use the primary routine, deeper ones the offset one. */
void draw_record_8e5f0(ef_addr32 record)
{
    int16_t id = (int16_t)EF_U16(record + 0x20u);
    int32_t depth;

    guest_fn_a0ea8(id, (uint32_t)(int32_t)(int16_t)EF_U16(record + 0xcu), (int16_t)EF_U16(record + 0xeu),
                   (uint32_t)(int32_t)(int16_t)EF_U16(record + 0x10u));
    depth = (int32_t)EF_U32(record + 4u);
    if (depth < (int32_t)0xfffffd00)
        guest_fn_9e0c4(id, EF_U32(record) << 8, EF_U32(record + 4u) << 8, EF_U32(record + 8u) << 8);
    else
        guest_fn_9e094(id, EF_U32(record) << 8, EF_U32(record + 4u) << 8, EF_U32(record + 8u) << 8, 0xfffffe06u);
}

/* 0x8f9e8, 111 bytes. Install the table of nine handler addresses at 0x158f4c. */
void install_handlers_8f9e8(void)
{
    EF_U32(0x00158f4cu) = 0x8e0f0u;
    EF_U32(0x00158f50u) = 0x8e1acu;
    EF_U32(0x00158f54u) = 0x8e1fcu;
    EF_U32(0x00158f58u) = 0x8e2b8u;
    EF_U32(0x00158f5cu) = 0x8e430u;
    EF_U32(0x00158f60u) = 0x8e374u;
    EF_U32(0x00158f64u) = 0x8e494u;
    EF_U32(0x00158f68u) = 0x8e518u;
    EF_U32(0x00158f6cu) = 0x8e584u;
}
