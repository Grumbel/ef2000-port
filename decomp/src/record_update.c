/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x9a624,213 bytes. No inputs, six registers preserved, EAX scratch,
 * plain RET. Sole caller 98bf1 discards EAX. */
void update_records_9a624(void)
{
    uint32_t index = 0u;
    ef_addr32 rec;
    uint32_t classification;
    uint16_t kind;
    uint16_t total;
    uint16_t term;

    while ((int32_t)(int16_t)index < EF_S32(0x001379b0u)) {
        rec = EF_U32(0x001379bcu)
            + (uint32_t)(int32_t)(int16_t)index * 54u;
        if ((EF_S32(rec + 10u) >> 16) == 1) {
            classification = classify_target_flags_9a6fc(rec);
            kind = EF_U16(rec + 16u);
            if (kind == 0u)
                count_ground_kill_9a8b4(rec, (uint32_t)(int32_t)(int16_t)classification);
            else if (kind == 1u || kind == 2u)
                count_air_kill_9a998(rec);
            else if (kind == 3u)
                count_event_9aa78((int16_t)classification);
        }
        index++;
    }
    for (index = 0u; index < 4u; index++) {
        total = EF_U16(0x0040d008u);
        term = EF_U16(0x0040d00eu + index * 2u);
        EF_U16(0x0040d008u) = (uint16_t)(total + term);
        total = EF_U16(0x0040d00au);
        term = EF_U16(0x0040d016u + index * 2u);
        EF_U16(0x0040d00au) = (uint16_t)(total + term);
        total = EF_U16(0x0040d00cu);
        term = EF_U16(0x0040d01eu + index * 2u);
        EF_U16(0x0040d00cu) = (uint16_t)(total + term);
    }
    uncount_side_tallies_9b780();
    guest_fn_9b8a0();
}
