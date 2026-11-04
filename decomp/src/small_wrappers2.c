/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small file and lookup helpers. */

/* 0xa1cb4, 76 bytes. Close the log handle at 0x132b18 (writing a -1 marker first when 0x132b1c is 1) and mark it closed. */
void close_log_handle_a1cb4(void)
{
    if (EF_U32(0x00132b18u) == 0xffffffffu)
        return;
    if (EF_U8(0x00132b1cu) == 1u) {
        EF_U32(0x00417160u) = 0xffffffffu;
        write_checked_a1b28(0x00417160u, 4u);
    }
    close_handle_a7990(0u, 0u, 0u, 0u, EF_U32(0x00132b18u));
    EF_U32(0x00132b18u) = 0xffffffffu;
}

/* 0x105024, 76 bytes. Open a file and return its size; the error code is left at 0x17d2d8 (3 open failed, 5 size failed). */
uint32_t file_size_by_name_105024(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 name)
{
    uint32_t handle, size;
    (void)u0; (void)u1; (void)u2; (void)u3;
    EF_U32(0x0017d2d8u) = 0u;
    handle = (uint32_t)open_file_fa485(name, 0x200u);
    if (handle == 0xffffffffu) {
        EF_U32(0x0017d2d8u) = 3u;
        return handle;
    }
    size = file_size_115989(handle);
    if (size == 0xffffffffu)
        EF_U32(0x0017d2d8u) = 5u;
    tail_jump_115984(handle);
    return size;
}

/* 0xa1a40, 77 bytes. Look a name up in the pointer table at 0x417180 (sentinel stored at the count slot) and return the matching 16-bit value from 0x417170, or 0 if absent. */
int32_t lookup_name_value_a1a40(ef_addr32 name)
{
    uint32_t count = EF_U32(0x00417184u);
    uint32_t table = EF_U32(0x00417180u);
    int32_t idx = -1;
    uint32_t entry;
    EF_U32(table + count * 4u) = name;
    do {
        entry = EF_U32(EF_U32(0x00417180u) + (uint32_t)(idx + 1) * 4u);
        idx++;
    } while (guest_fn_a4380(name, entry) != 0u);
    if ((uint32_t)idx == EF_U32(0x00417184u))
        return 0;
    return EF_S16(EF_U32(0x00417170u) + (uint32_t)idx * 2u);
}
