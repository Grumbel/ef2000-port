/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Path assembly. */

/* 0xe80d0, 202 bytes. _makepath: join drive, directory, name and extension into path, adding the missing separators and normalising '/' to '\\'. */
void make_path_e80d0(ef_addr32 path, ef_addr32 drive, ef_addr32 dir, ef_addr32 name, ef_addr32 ext)
{
    ef_addr32 p;

    EF_U8(path) = 0;
    if (drive != 0 && EF_U8(drive) != 0) {
        guest_fn_9ca57(path, drive);
        if (EF_U8(path + guest_fn_b1b8b(path) - 1u) != 0x3a)
            guest_fn_9ca57(path, 0x00190c5cu);
    }
    if (dir != 0 && EF_U8(dir) != 0) {
        guest_fn_9ca57(path, dir);
        for (p = path; EF_U8(p) != 0; p++) {
            if (EF_U8(p) == 0x2f)
                EF_U8(p) = 0x5c;
        }
        if (EF_U8(path + guest_fn_b1b8b(path) - 1u) != 0x5c)
            guest_fn_9ca57(path, 0x00190c60u);
    }
    if (name != 0 && EF_U8(name) != 0) {
        guest_fn_9ca57(path, name);
        if (ext != 0 && EF_U8(ext) != 0) {
            if (EF_U8(ext) != 0x2e)
                guest_fn_9ca57(path, 0x00190c64u);
            guest_fn_9ca57(path, ext);
        }
    }
}
