/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Scene setup and table copy helpers (270..380 bytes). */

/* 0x57480, 380 bytes. Copy a decoded descriptor into its 0x30-byte table entry. */
void store_descriptor_entry_57480(ef_addr32 src)
{
    ef_addr32 e = 0x00211af0u + (uint32_t)(EF_S32(src + 0x13u) >> 16) * 0x30u;

    EF_U32(e) = EF_U32(src + 1u);
    EF_U32(e + 4u) = EF_U32(src + 5u);
    EF_U32(e + 8u) = EF_U32(src + 9u);
    EF_U16(e + 0xcu) = EF_U16(src + 0xdu);
    EF_U16(e + 0xeu) = EF_U16(src + 0xfu);
    EF_U16(e + 0x10u) = EF_U16(src + 0x11u);
    EF_U8(e + 0x24u) = (uint8_t)(EF_U8(e + 0x24u) & 0xfeu);
    EF_U16(e + 0x24u) = (uint16_t)(EF_U16(e + 0x24u) | (EF_U8(src + 0x13u) & 1u));
    EF_U8(e + 0x24u) = (uint8_t)(EF_U8(e + 0x24u) & 0xfdu);
    EF_U16(e + 0x24u) = (uint16_t)(EF_U16(e + 0x24u) | ((EF_U8(src + 0x14u) & 1u) * 2u));
    EF_U16(e + 0x2eu) = EF_U16(src + 0x17u);
    EF_U16(e + 0x18u) = EF_U16(src + 0x19u);
    if (EF_U8(src + 0x1bu) != 0) {
        guest_fn_cd150(EF_S32(e + 0x1eu) >> 16);
        guest_fn_cd20c_w(0);
    }
}

/* 0x8bb50, 288 bytes. Per-frame selection refresh; mirrors the saved camera into the unit table. */
void refresh_selection_frame_8bb50(void)
{
    copy_selected_object();
    guest_fn_8dd48_w();
    guest_fn_90828_w();
    if (EF_U16(0x00198444u) != 0) {
        if (EF_U8(0x00127da4u) == EF_U8(0x0012583au) || EF_U8(0x001a666bu) == 0)
            guest_fn_8f644(EF_S32(0x00198440u) >> 16);
    }
    if (EF_U8(0x00127da9u) != 0) {
        uint8_t i = EF_U8(0x001a666bu);
        uint32_t base = (uint32_t)i * 27u + 0x001a5c33u;
        uint32_t k;

        for (k = 0; k < 6; k++)
            EF_U32(base + k * 4u) = EF_U32(0x001a3020u + k * 4u);
        if (i == 0 && EF_U8(0x00127da4u) < EF_U8(0x0012583au))
            guest_fn_7eccc_w();
        if (EF_U8(0x001f9010u) != 0 && EF_U8(0x001a666bu) == 0) {
            guest_fn_7e8c0_w(0x7080u);
            EF_U8(0x001f9010u) = 0;
        }
        if (EF_U8(0x0019bc02u) != 0) {
            guest_fn_7e7e0_w(0, 0);
            EF_U8(0x001f9010u) = 0;
        }
    }
    if (EF_U8(0x00127da9u) == 0 && EF_U8(0x001f9010u) != 0) {
        guest_fn_7e8c0_w(0x7080u);
        EF_U8(0x001f9010u) = 0;
    }
}

/* 0x93060, 269 bytes. Point the scene camera at the current weapon station's object. */
void aim_scene_at_station_93060(void)
{
    int32_t idx;
    ef_addr32 rec;
    ef_addr32 obj;

    initialize_scene_angle_record(0x00214404u);
    idx = EF_S32(0x0019bb5cu) >> 16;
    if (idx == 0xff)
        return;
    rec = EF_U32(0x0019bb58u) + (uint32_t)idx * 0xcau;
    if (EF_U8(0x0012dc34u) >= EF_U8(rec + 0x4fu))
        guest_fn_92ffc_w();
    rec = EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(0x0019bb5cu) >> 16) * 0xcau;
    obj = guest_fn_7489c(EF_S32(rec + (uint32_t)EF_U8(0x0012dc34u) * 2u + 0x4eu) >> 16);
    if (obj == 0) {
        EF_U16(0x0021449cu) = 1;
        EF_U8(0x0012db9eu) = EF_U8(0x00214486u);
        return;
    }
    EF_U32(0x0021446cu) = EF_U32(obj);
    EF_U32(0x00214470u) = EF_U32(obj + 4u);
    EF_U32(0x00214474u) = EF_U32(obj + 8u);
    initialize_scene_angle_record(0x00214378u);
    update_scene_angle_first(0x00214378u, EF_U16(obj + 0xeu));
    if (EF_U8(0x0021454bu) != 0) {
        update_scene_angle_second(0x00214378u, EF_U16(obj + 0xcu));
        update_scene_angle_third(0x00214378u, EF_U16(obj + 0x10u));
    }
    if (scene_lookup_named_word(EF_S32(obj + 0x1eu) >> 16) != 0)
        EF_U16(0x0021448au) = (uint16_t)scene_lookup_named_word(EF_S32(obj + 0x1eu) >> 16);
    guest_fn_9357c();
}

/* 0x95ae0, 276 bytes. Set up the scene camera from the pending target record, or a default view. */
void setup_scene_camera_95ae0(void)
{
    ef_addr32 p;

    EF_U16(0x0012dba0u) = 7;
    p = EF_U32(0x0020f15cu);
    if (p == 0) {
        EF_U16(0x0012dba0u) = 0x14;
        initialize_scene_angle_record(0x0012dbecu);
        EF_U16(0x0021449cu) = 1;
        EF_U8(0x0012db9eu) = 0x3d;
        return;
    }
    EF_U16(0x00214464u) = 0x2000;
    EF_U16(0x00214466u) = 0;
    EF_U32(0x00214338u) = EF_U32(p + 0x30u);
    EF_U16(0x00214468u) = 0;
    EF_U32(0x00214320u) = EF_U32(p);
    EF_U16(0x0012dc88u) = 0xc8;
    EF_U32(0x00214328u) = EF_U32(p + 8u);
    EF_U32(0x00214324u) = EF_U32(p + 4u);
    EF_U32(0x0021433cu) = 0xa00u;
    EF_U16(0x00214492u) = 1;
    initialize_scene_angle_record(0x00214378u);
    update_scene_angle_first(0x00214378u, EF_U16(EF_U32(0x0020f15cu) + 0xeu));
    update_scene_angle_second(0x00214378u, EF_U16(EF_U32(0x0020f15cu) + 0xcu));
    update_scene_angle_third(0x00214378u, EF_U16(EF_U32(0x0020f15cu) + 0x10u));
    guest_fn_957c8_w(0x0018abdcu);
    request_scene_refresh();
}
