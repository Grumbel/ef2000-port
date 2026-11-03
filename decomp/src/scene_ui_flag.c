/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

/* 0x95018, size 0x08, fourteen caller functions. */
void request_scene_refresh(void)
{
    g_scene_refresh_pending = 1;
}
