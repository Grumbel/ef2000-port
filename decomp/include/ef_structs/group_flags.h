/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#ifndef EF_STRUCTS_GROUP_FLAGS_H
#define EF_STRUCTS_GROUP_FLAGS_H

#include "ef_types.h"

/* Prefix shared by the group records traversed at 0xcc1ac..0xcc268. */
typedef struct EF_PACKED ef_group_flag_record {
    uint8_t unknown_00[4];
    ef_addr32 next;
    uint8_t unknown_08[3];
    uint8_t group;
    ef_addr32 flags;
} ef_group_flag_record;
EF_STATIC_ASSERT(ef_group_flag_next_offset, offsetof(ef_group_flag_record, next) == 4);
EF_STATIC_ASSERT(ef_group_flag_group_offset, offsetof(ef_group_flag_record, group) == 0xb);
EF_STATIC_ASSERT(ef_group_flag_flags_offset, offsetof(ef_group_flag_record, flags) == 0xc);
EF_STATIC_ASSERT(ef_group_flag_prefix_size, sizeof(ef_group_flag_record) == 0x10);

#endif
