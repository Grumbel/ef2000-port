/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/archive_context.h"

/* 0xeba64, 169 bytes. Load the entry NAME of the open archive CONTEXT
 * into DEST. An RJ entry is read so that it ends 16 bytes past the
 * unpacked end of DEST and is unpacked in place; an RA entry goes to its
 * own decoder, whose result is returned; anything else is read as is.
 * Returns 0 for a null context or an unknown name. */
uint8_t load_archive_entry_eba64(ef_addr32 name, ef_addr32 dest, ef_addr32 context)
{
    ef_archive_context *archive = (ef_archive_context *)(uintptr_t)context;
    const ef_archive_entry *entry;
    uint32_t size;

    if (context == 0u)
        return 0u;
    entry = (const ef_archive_entry *)(uintptr_t)lookup_name_entry_eb520(name, context);
    if (entry == 0)
        return 0u;
    size = entry->size;
    seek_record_eb698(entry->position, context);
    if (archive->header_tag == EF_ARCHIVE_TAG_RJ) {
        ef_addr32 packed = archive->header_unpacked_size + 0x10u + dest - size;

        read_handle_a79d4(0u, 0u, 0u, 0u, archive->handle, size, packed);
        guest_fn_f9ee0_x(dest, packed);
    } else if (archive->header_tag == EF_ARCHIVE_TAG_RA) {
        return (uint8_t)guest_fn_fa02c_x(archive->handle, dest);
    } else {
        seek_start_a7ab4(0u, 0u, 0u, 0u, archive->handle, entry->position);
        read_handle_a79d4(0u, 0u, 0u, 0u, archive->handle, size, dest);
    }
    return 1u;
}
