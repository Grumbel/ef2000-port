/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Original aircraft-view presets are three adjacent halfwords. */
static void set_view(uint16_t distance, uint16_t yaw, uint16_t pitch)
{
    EF_U16(0x1f6f48u) = distance;
    EF_U16(0x1f6f3cu) = yaw;
    EF_U16(0x1f6f3eu) = pitch;
}

/* 0x68f40, 43 bytes: side-on view. */
void gpw_plane_side_on_68f40(void) { set_view(0x730u, 0x810u, 0x8001u); }

/* 0x68f6c, 43 bytes: underside view. */
void gpw_plane_underneath_68f6c(void) { set_view(0xbe0u, 0x820u, 0x4000u); }

/* 0x68f98, 36 bytes: default angle. */
void gpw_plane_angled_68f98(void) { set_view(0xe38u, 0x4000u, 0x4000u); }

/* 0x68fbc, 36 bytes: byte-identical default-angle body at another entry. */
void gpw_use_defaults_68fbc(void) { set_view(0xe38u, 0x4000u, 0x4000u); }

/* 0x68fe0, 43 bytes: placing a missile uses a more distant perspective. */
void gpw_place_missile_68fe0(void) { set_view(0xaf0u, 0x1920u, 0x66e0u); }

/* 0x6931c, 26 bytes: set mode pair 1,0. */
void gpw_set_clear_missile_6931c(void)
{
    EF_U16(0x1282d8u) = 1u;
    EF_U16(0x1282dau) = 0u;
}

/* 0x69338, 26 bytes: set mode pair 0,1. */
void gpw_set_clear_all_missiles_69338(void)
{
    EF_U16(0x1282d8u) = 0u;
    EF_U16(0x1282dau) = 1u;
}

