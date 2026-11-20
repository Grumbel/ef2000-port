/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_SAVE_SLOT_H
#define EF_STRUCTS_SAVE_SLOT_H

#include "ef_types.h"

#define EF_SAVE_SLOT_TABLE 0x0019b438u
#define EF_SAVE_SLOT_COUNT 10

/* The 0x73-byte save slot summaries at 0x19b438, filled by 0x28224
 * from the head of each save file. Field meanings are not known yet. */
typedef struct EF_PACKED ef_save_slot {
    uint8_t empty;                    /* +0x00, 1 when the file is missing */
    char text_01[0x15];               /* +0x01, second field of the file */
    char text_16[0x15];               /* +0x16, first field of the file */
    char text_2b[0x15];               /* +0x2b */
    char first_name[0x15];            /* +0x40, from the table at 0x12521c */
    uint32_t value_55[3];             /* +0x55 */
    char second_name[0x0a];           /* +0x61, from the table at 0x125238 */
    uint32_t value_6b;                /* +0x6b */
    uint32_t value_6f;                /* +0x6f */
} ef_save_slot;

EF_STATIC_ASSERT(ef_save_slot_size, sizeof(ef_save_slot) == 0x73);
EF_STATIC_ASSERT(ef_save_slot_first_name_off, offsetof(ef_save_slot, first_name) == 0x40);
EF_STATIC_ASSERT(ef_save_slot_second_name_off, offsetof(ef_save_slot, second_name) == 0x61);
EF_STATIC_ASSERT(ef_save_slot_value_6f_off, offsetof(ef_save_slot, value_6f) == 0x6f);

#endif
