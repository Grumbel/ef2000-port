/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x4eb8c, 135 bytes. mpWPZoomToggle: derive a waypoint slot index from the
 * signed word at 0x41eef4 ((y + 10) / 20) and a value from the signed word
 * at 0x41eef2 (120 - x, scaled by 500 or by 10 depending on byte 0x1a55a1,
 * clamped to >= 0). For a nonzero slot whose type byte at 0x1a5672 is not
 * 0x11, 5 or 8, store the value in the dword table at 0x1a560a, record the
 * slot in byte 0x1a559f and refresh the two widgets. */
void mpWPZoomToggle(void)
{
    int32_t scale = EF_U8(0x001a55a1u) == 0u ? 500 : 10;
    int32_t span = 120 - (int32_t)(int16_t)EF_U16(0x0041eef2u);
    int32_t slot = ((int32_t)(int16_t)EF_U16(0x0041eef4u) + 10) / 20;
    int32_t value = scale * span;
    if (value < 0)
        value = 0;
    if (slot != 0) {
        uint32_t type = EF_U8(0x001a5672u + (uint32_t)slot);
        if (type != 0x11u && type != 5u && type != 8u) {
            EF_U32(0x001a560au + (uint32_t)slot * 4u) = (uint32_t)value;
            EF_U8(0x001a559fu) = (uint8_t)slot;
            update_counter_widget_4e7b8(1u);
            guest_fn_4f0d4();
        }
    }
}
