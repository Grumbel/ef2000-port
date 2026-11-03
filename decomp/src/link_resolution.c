/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x38658, 0x1a bytes, two independent caller functions. EDX optionally
 * carries a candidate record with a 32-bit alternate at +0x15. The table
 * fallback sign-extends AX, without checking bounds or table nullness. */
ef_addr32 resolve_link_pointer(int16_t index, ef_addr32 candidate)
{
    if (candidate != 0) {
        const ef_link_candidate *entry =
            (const ef_link_candidate *)ef_guest_memory_ptr(candidate);
        if (entry->alternate != 0) { return entry->alternate; }
    }
    return g_link_pointer_table[index];
}

/* 0x3856c, 0x17 bytes, three independent callers. Original code saves
 * EDX/EBX, resolves the record's current link, then writes the result back
 * to the same +0x18 field. EAX retains the new pointer on return. */
ef_addr32 refresh_link_pointer(ef_addr32 reference)
{
    ef_link_reference *record =
        (ef_link_reference *)ef_guest_memory_ptr(reference);
    ef_addr32 found = resolve_link_pointer(record->index, record->current);
    record->current = found;
    return found;
}
