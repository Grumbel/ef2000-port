/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Device slot and small arithmetic helpers, 0xd70a0..0xd7bf0. */

/* 0xd70a0, 103 bytes. Claim a device slot for a priority, or evict the lowest-priority one. */
uint32_t claim_device_slot_d70a0(uint8_t priority)
{
    uint8_t best_slot = 0;
    uint8_t best_priority = 0x64;
    uint8_t slot;
    uint8_t probe = 0;

    if (EF_U8(0x0015bac4u) == 0)
        return 0x0eu;

    for (slot = 0; slot < 0x0au; slot++) {
        probe = (uint8_t)guest_fn_d7594(slot);
        if (probe == 0) {
            EF_U8(0x00420e61u + slot) = priority;
            return slot;
        }
    }

    /* ORIGINAL BUG: the scan resumes from the last probe result rather than slot 0. */
    slot = probe;
    do {
        uint8_t current = EF_U8(0x00420e61u + slot);

        if (!(best_priority < current)) {
            best_priority = current;
            best_slot = slot;
        }
        slot++;
    } while (slot < 0x0au);

    if (best_priority >= priority)
        return 0x0eu;
    /* ORIGINAL BUG: the new priority is stored at the loop-exit index, not at best_slot. */
    EF_U8(0x00420e61u + slot) = priority;
    return best_slot;
}

/* 0xd7704, 15 bytes. Add to the word at +0xf and return the new value. */
uint32_t add_word_d7704(ef_addr32 record, uint16_t delta)
{
    uint16_t sum = (uint16_t)(EF_U16(record + 0xfu) + delta);

    EF_U16(record + 0xfu) = sum;
    return sum;
}

/* 0xd77dc, 34 bytes. Scale -(a << 6) by the magnitude of b plus one, biased by 0x40. */
int32_t scale_by_magnitude_d77dc(int16_t a, int16_t b)
{
    int32_t magnitude = (int32_t)abs_a28f2((int32_t)b);
    int32_t numerator = -(int32_t)a;

    numerator <<= 6;
    return numerator / (magnitude + 1) + 0x40;
}

/* 0xd7bf0, 77 bytes. Shift four window words by ten. */
void shift_window_d7bf0(void)
{
    EF_U16(0x00420e84u) = (uint16_t)(EF_U16(0x00420e84u) + 0x0au);
    EF_U16(0x00420e88u) = (uint16_t)(EF_U16(0x00420e88u) + 0x0au);
    EF_U16(0x00420e8cu) = (uint16_t)(EF_U16(0x00420e8cu) + 0x0au);
    EF_U16(0x00420e90u) = (uint16_t)(EF_U16(0x00420e90u) + 0x0au);
}
