/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define KOTS_MODE_BASES 3u
#define BASE_SCORE_BONUS 0x14u
#define OWNER_SCORES 0x001a6398u /* int16 per owner, sent by 0x56a4c */

/* 0x9114, 461 bytes. Check whether the selected object touches a base
 * area (0x5ee80) this frame. Contact is ignored while the object is
 * inactive (+0x18d) or flagged 4/8 at +0x24, which also clears the latch;
 * a dword +0x12 of 6 or more only marks the contact as blocked. The
 * first frame of a contact (latch 0x1972f1 clear) acts on the area's
 * owner word +0x10: in the multiplayer modes it refreshes the weapon
 * state or, in the bases mode, adds a score bonus to the local owner and
 * sends it; in single player an unowned area publishes the object's
 * position and a 100-point event, an area owned by 1 refreshes the
 * weapon state. Called by 0x3df8c. */
void check_base_contact_9114(void)
{
    ef_addr32 obj;
    ef_addr32 area;

    g_base_contact = 0u;
    g_base_contact_blocked = 0u;
    obj = lookup_filtered_indexed_object((int16_t)((int32_t)g_grid_secondary_selector >> 16));
    if (obj == 0u)
        return;
    if (EF_U8(obj + 0x18du) == 0u || (EF_U8(obj + 0x24u) & 0x0cu) != 0u) {
        g_base_contact_latched = 0u;
        return;
    }
    area = find_entry_in_range_5ee80();
    if (area == 0u)
        return;
    if (EF_S32(obj + 0x12u) >= 6) {
        g_base_contact_blocked = 1u;
        return;
    }
    g_base_contact = 1u;
    if (g_base_contact_latched != 0u)
        return;
    g_base_contact_latched = 1u;

    if (EF_U8(0x00127da8u) != 0u) {
        uint32_t owner = g_world_grid_marker;
        int owned = g_kots_base_side_mirror[owner] == 0u
            ? EF_S16(area + 0x10u) == 1
            : EF_U16(area + 0x10u) == 0u;

        if (owned) {
            refresh_weapon_state_8cb60();
            return;
        }
        if (g_kots_lock != KOTS_MODE_BASES)
            return;
        EF_U16(OWNER_SCORES + owner * 2u) = (uint16_t)(EF_U16(OWNER_SCORES + owner * 2u) + BASE_SCORE_BONUS);
        send_owner_summary_56a4c();
        send_packet_2c((uint8_t)g_world_grid_marker, 0u, 0u, 0u, 1u);
        return;
    }

    if (EF_U16(area + 0x10u) != 0u) {
        if (EF_U16(area + 0x10u) == 1u)
            refresh_weapon_state_8cb60();
        return;
    }
    if (g_world_grid_mode != 0u)
        return;
    EF_U16(0x00126518u) = 1u;
    pack_runtime_position(-1, EF_S32(obj) >> 8, EF_S32(obj + 8u) >> 8, 0u);
    guest_fn_10750_x(EF_S32(obj + 0x14u) >> 16, 1, 0x64);
}
