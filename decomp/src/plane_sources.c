/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x7aac0, size 0x3d, seven caller functions. The full EAX mode indexes
 * twelve-byte records. No original range check is performed. The unchanged
 * branch leaves EAX equal to the caller's mode; the changed branch ends
 * with the newly selected high-plane pointer in EAX. */
ef_addr32 select_plane_source(int32_t mode)
{
    if (mode != g_plane_selected_mode) {
        g_plane_selected_mode = mode;
        const ef_plane_source *source = &g_plane_source_table[mode];
        g_active_plane_aux = source->auxiliary;
        g_active_plane_low = source->low_plane;
        g_active_plane_high = source->high_plane;
        return source->high_plane;
    }
    return (ef_addr32)mode;
}

/* 0x7b2f4, size 0x3f, ten caller functions. The row stride is 400 bytes
 * and the horizontal lookup runs backward from 399. The original sets AL,
 * then clears only AH, leaving the pre-read upper EAX word visible for
 * out-of-range columns; preserve that oddity here. */
uint32_t read_plane_cell(int32_t mode, int32_t row, int32_t column)
{
    (void)select_plane_source(mode);
    uint32_t offset = 0x18fu - (uint32_t)column;
    ef_addr32 base_offset = (ef_addr32)((uint32_t)row * 400u + offset);
    uint8_t upper = *(const uint8_t *)ef_guest_memory_ptr(g_active_plane_high + base_offset);
    uint8_t lower = *(const uint8_t *)ef_guest_memory_ptr(g_active_plane_low + base_offset);
    return (offset & 0xffff0000u) | ((uint32_t)upper << 8) | lower;
}
