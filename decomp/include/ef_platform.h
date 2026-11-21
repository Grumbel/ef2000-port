/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#ifndef EF_PLATFORM_H
#define EF_PLATFORM_H

#include "ef_types.h"

/* Platform layer: services the game reaches through segment registers,
 * DOS/BIOS interrupts or the X-32 extender. Game code calls only these
 * named wrappers; the hybrid build implements them with the original
 * instruction, a native build supplies its own implementation. */

/* Make SELECTOR (the texture window, dword 0x485fa8) current in FS. The
 * textured span rasterizers around 0xd3a00..0xd5c00 read texels through FS;
 * 0xfc9b0 allocates the selector and 0xfc9f8 moves its base. */
void platform_select_texture_window(uint16_t selector);
#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux platform_select_texture_window = "mov fs,ax" parm [ax] modify exact []
#endif

#endif
