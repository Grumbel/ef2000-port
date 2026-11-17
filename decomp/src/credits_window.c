/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef uint32_t (*ef_credits_size)(ef_addr32);
typedef uint32_t (*ef_credits_load)(ef_addr32, ef_addr32);
typedef void (EF_CDECL *ef_credits_error)(ef_addr32, ...);

/* 0x371b0, 161 bytes. EAX=widget. Error callbacks may return; the original
 * default error handler preserves EDX (the file size) and ECX (the widget). */
void load_credits_window_371b0(ef_addr32 widget)
{
    char filename[144];
    ef_addr32 path = (ef_addr32)(uintptr_t)filename;
    uint32_t size;

    build_path_9c9c0(path, 0x00183298u);
    size = ((ef_credits_size)(uintptr_t)EF_U32(0x00132ab4u))(path);
    if (size == 0)
        ((ef_credits_error)(uintptr_t)EF_U32(0x00132a74u))(0x001832b0u, path);
    if (EF_U32(0x0019f36cu) == 0) {
        EF_U32(0x0019f36cu) = alloc_zeroed_ab840(size + 16u);
        if (EF_U32(0x0019f36cu) == 0)
            ((ef_credits_error)(uintptr_t)EF_U32(0x00132a74u))(0x001832d4u);
    }
    if ((uint8_t)((ef_credits_load)(uintptr_t)EF_U32(0x00132ab8u))(
            path, EF_U32(0x0019f36cu)) == 0)
        ((ef_credits_error)(uintptr_t)EF_U32(0x00132a74u))(0x001832fcu, path);
    redraw_widget_ac9a0(widget);
    wrap_text_lines_e1e6c(widget, EF_U32(0x0019f36cu), 0);
}
