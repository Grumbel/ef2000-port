/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Free one slot of RECORD for another try: a valid entry is counted
 * away (0xbd86c, 0xbd6d8), otherwise unusable slots are cleared
 * (0xbd5f8) and counted. Returns 0 with the cleared result when nothing
 * could be freed. */
static int free_one_slot(ef_addr32 record, uint32_t *cleared)
{
    if ((uint16_t)record_has_valid_entry_bd86c(record) != 0u) {
        count_slots_bd6d8(record);
        return 1;
    }
    *cleared = clear_unusable_slots_bd5f8(record);
    if ((uint16_t)*cleared != 0u) {
        count_slots_bd6d8(record);
        return 1;
    }
    return 0;
}

/* 0xbdbf8, 340 bytes (ret 4). Fit the slot record RECORD (bytes +0, +2,
 * +3) to NODE. After resetting the slots (0xbd688) the result is 1. It
 * stays 1 unless the group test 0xcc22c passes and NODE is a type 0x31
 * node with more than four entries (+0x32) and sign word 1: then three
 * conditions are retried, freeing a slot before each retry, until they
 * hold or nothing more can be freed: the value test 0xbd728 for byte +2,
 * the free run 0xbd73c for byte +3 and the requirement reservation
 * 0xbd92c. A failed retry leaves the cleared result of 0xbd5f8 (0 in its
 * low word), which is also returned. */
uint32_t fit_slot_record_bdbf8(ef_addr32 record, ef_addr32 node, int16_t limit, ef_addr32 requirements,
                               ef_addr32 group)
{
    uint32_t result = 1u;
    uint32_t cleared;

    reset_slots_bd688(record);
    if ((uint16_t)group_flag_positive_cc22c(EF_U8(node + 0xbu), EF_U8(record), group) == 0u ||
        EF_U8(node + 0xcu) != 0x31u || EF_U8(node + 0x32u) <= 4u ||
        (EF_S32(node + 0x4eu) >> 16) != 1)
        return result;
    while ((uint16_t)value_below_word_23(node, EF_U8(record + 2u)) == 0u && (uint16_t)result != 0u) {
        if (!free_one_slot(record, &cleared))
            result = 0u;
    }
    while ((uint16_t)has_free_run_bd73c(node, (int16_t)EF_U8(record + 3u)) == 0u && (uint16_t)result != 0u) {
        if (!free_one_slot(record, &cleared))
            result = 0u;
    }
    while ((uint16_t)reserve_requirements_bd92c(node, record, requirements, (uint16_t)limit) == 0u &&
           (uint16_t)result != 0u) {
        if (!free_one_slot(record, &cleared))
            result = cleared;
    }
    return result;
}
