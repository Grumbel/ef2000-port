/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#ifndef EF_STRUCTS_RECORD_TREE_H
#define EF_STRUCTS_RECORD_TREE_H

#include "ef_types.h"

/* Minimum shapes accessed by b1a68; other fields remain unidentified. */
typedef struct EF_PACKED ef_record_tree {
    uint8_t unknown_00[0x1e];
    int16_t group_count;
    ef_addr32 groups;
} ef_record_tree;
EF_STATIC_ASSERT(ef_record_tree_count_offset, offsetof(ef_record_tree, group_count) == 0x1e);
EF_STATIC_ASSERT(ef_record_tree_groups_offset, offsetof(ef_record_tree, groups) == 0x20);

typedef struct EF_PACKED ef_record_tree_group {
    uint8_t unknown_00[4];
    int16_t entry_count;
    ef_addr32 entries;
} ef_record_tree_group;
EF_STATIC_ASSERT(ef_record_tree_group_count_offset, offsetof(ef_record_tree_group, entry_count) == 4);
EF_STATIC_ASSERT(ef_record_tree_group_entries_offset, offsetof(ef_record_tree_group, entries) == 6);
EF_STATIC_ASSERT(ef_record_tree_group_size, sizeof(ef_record_tree_group) == 10);

typedef struct EF_PACKED ef_record_tree_entry {
    uint8_t unknown_00[2];
    ef_addr32 allocation;
} ef_record_tree_entry;
EF_STATIC_ASSERT(ef_record_tree_entry_allocation_offset, offsetof(ef_record_tree_entry, allocation) == 2);
EF_STATIC_ASSERT(ef_record_tree_entry_size, sizeof(ef_record_tree_entry) == 6);

#endif
