/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xf8b14, 387 bytes. Maintain the requested entry's group and free slots. */
#pragma aux validate_entry_slots_f8b14 parm [eax] [edx] [ebx] [ecx] routine value [eax] modify [eax ebx ecx edx]
uint32_t validate_entry_slots_f8b14(ef_addr32 list, ef_addr32 object,
                                    ef_addr32 key, uint32_t packed,
                                    uint32_t claim_context,
                                    ef_addr32 group_head,
                                    ef_addr32 find_key)
{
    uint32_t found = 0;
    uint32_t saved_found;
    uint32_t result;

    if ((guest_fn_cc22c(EF_U8(object + 0x0bu), EF_U8(list), group_head) & 0xffffu) == 0u ||
        EF_U8(object + 0x0cu) != 'I' || EF_U8(object + 0x32u) <= 2u)
        return 0;

    reset_slots_f8938(list);
    if (EF_U8(list + 1u) == 0x0eu &&
        find_node_b55f8(find_key, object, key, 0x49u) == 0u)
        return 0;

    found = 1;
    while ((value_below_word_23(object, EF_U8(list + 2u)) & 0xffffu) == 0u &&
           (int16_t)found != 0) {
        if ((decrement_last_entry_f8acc(list) & 0xffffu) == 0u) {
            if ((clear_failed_entry_f8988(list) & 0xffffu) == 0u)
                found = 0;
            else
                count_slots_f88e8(list);
        } else {
            count_slots_f88e8(list);
        }
    }

    for (;;) {
        result = has_free_run_bd73c(object, EF_U8(list + 3u));
        saved_found = found;
        if ((result & 0xffffu) != 0u || (int16_t)found == 0)
            break;

        if ((decrement_last_entry_f8acc(list) & 0xffffu) == 0u) {
            if ((clear_failed_entry_f8988(list) & 0xffffu) == 0u)
                found = 0;
            else
                count_slots_f88e8(list);
        } else {
            count_slots_f88e8(list);
        }
    }

    for (;;) {
        found = saved_found;
        result = guest_fn_f8624(object, list, claim_context,
                                (int32_t)(int16_t)packed);
        if ((result & 0xffffu) != 0u || (int16_t)found == 0)
            break;

        if ((decrement_last_entry_f8acc(list) & 0xffffu) != 0u) {
            count_slots_f88e8(list);
            saved_found = found;
        } else {
            result = clear_failed_entry_f8988(list);
            if ((int16_t)result != 0) {
                count_slots_f88e8(list);
                saved_found = found;
            } else {
                saved_found = 0;
            }
        }
    }

    return found;
}
