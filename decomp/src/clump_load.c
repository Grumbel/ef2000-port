/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include <stddef.h>

typedef struct ClumpDescriptor {
    ef_addr32 name, prefix, suffix;
    int32_t count;
    ef_addr32 index, data;
} ClumpDescriptor;
typedef struct ClumpIndex {
    uint32_t hash, size;
    ef_addr32 data;
    uint32_t unused;
} ClumpIndex;
typedef char clump_descriptor_layout[
    sizeof(ClumpDescriptor) == 24 &&
    offsetof(ClumpDescriptor, prefix) == 4 &&
    offsetof(ClumpDescriptor, suffix) == 8 &&
    offsetof(ClumpDescriptor, count) == 12 &&
    offsetof(ClumpDescriptor, index) == 16 &&
    offsetof(ClumpDescriptor, data) == 20 ? 1 : -1];
typedef char clump_index_layout[
    sizeof(ClumpIndex) == 16 && offsetof(ClumpIndex, hash) == 0 &&
    offsetof(ClumpIndex, size) == 4 && offsetof(ClumpIndex, data) == 8
    ? 1 : -1];

/* 0xea270, 378 bytes. Descriptor offsets: name +0, prefix +4,
 * suffix +8, count +12, index +16, packed data +20.
 * Each index entry has hash +0, size +4, data +8, unused +12.
 * ORIGINAL BUG: the entry count is the number of removed CR bytes,
 * rather than the number of newline-separated names. */
void clump_load(ef_addr32 descriptor)
{
    uint8_t path[0x50];
    ef_addr32 buffer = (ef_addr32)(uintptr_t)path;
    ef_addr32 list, cursor, entry, data;
    uint32_t size, total = 0u;
    int32_t i;

    if (EF_U32(descriptor + 12u) != 0u)
        release_record_ea240(descriptor);
    copy_string_9d749(buffer, EF_U32(descriptor));
    size = read_file_payload_size_b3b90(buffer);
    if (size == 0u) {
        build_path_9c9c0(buffer, EF_U32(descriptor));
        size = read_file_payload_size_b3b90(buffer);
        if (size == 0u)
            return;
    }
    list = alloc_zeroed_checked_ea100(size + 1u);
    load_file_unpacked_b3c04(buffer, list);
    EF_U8(list + size) = 0u;
    EF_U32(descriptor + 12u) = strip_carriage_returns(list);
    EF_U32(descriptor + 16u) =
        alloc_zeroed_checked_ea100(EF_U32(descriptor + 12u) << 4);
    cursor = list;
    for (i = 0; i < EF_S32(descriptor + 12u); ++i) {
        ef_addr32 name = cursor;
        cursor = split_line(cursor);
        build_path_from_record_ea17c(buffer, descriptor, name);
        size = lookup_in_archives_9cb70(buffer);
        if (size == 0u) {
            /* Original indirect caller-clean variadic diagnostic. */
#ifdef __WATCOMC__
            typedef void (__cdecl *Diagnostic)(ef_addr32, ef_addr32);
#else
            typedef void (*Diagnostic)(ef_addr32, ef_addr32);
#endif
            ((Diagnostic)(uintptr_t)EF_U32(0x00132a74u))(0x00191518u, buffer);
        }
        EF_U32(EF_U32(descriptor + 16u) + (uint32_t)i * 16u + 4u) = size;
        total += size;
    }
    data = alloc_zeroed_checked_ea100(total);
    EF_U32(descriptor + 20u) = data;
    cursor = list;
    for (i = 0; i < EF_S32(descriptor + 12u); ++i) {
        ef_addr32 name = cursor;
        cursor += strlen_ds(cursor) + 1u;
        build_path_from_record_ea17c(buffer, descriptor, name);
        entry = EF_U32(descriptor + 16u) + (uint32_t)i * 16u;
        EF_U32(entry + 8u) = data;
        data += EF_U32(entry + 4u);
        read_chunk_header_ea1a0(entry, buffer);
        EF_U32(EF_U32(descriptor + 16u) + (uint32_t)i * 16u) = name_hash_ebfd0(name);
    }
    guest_fn_dedde_x(EF_U32(descriptor + 16u), EF_U32(descriptor + 12u),
                     16u, 0x000ea1e8u);
    guest_release_block(list);
}
