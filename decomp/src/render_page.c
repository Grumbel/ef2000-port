/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x9e0c4,466 bytes. AX=page ID, EDX/EBX/ECX=coordinates; AL=accepted.
 * Saves ESI/EDI/EBP. Callers4d601/80cd2 and hook wrapper9e0b1 confirm ABI. */
uint8_t render_page_9e0c4(uint32_t page_id, uint32_t x, uint32_t y, uint32_t z)
{
    int32_t page = (int32_t)(int16_t)page_id;
    ef_addr32 record;
    uint32_t mode, index, channel;
    uint16_t dx, dy, dz;
    uint8_t accepted = 0u;

    EF_U16(0x0041722au) = (uint16_t)page_id;
    EF_U32(0x001600bcu) = (uint32_t)page;
    record = lookup_id_b3f58((int16_t)page);
    EF_U32(0x00417174u) = record;
    EF_U32(0x0041717cu) = EF_U32(0x0040d200u + (uint32_t)EF_U16(record) * 4u);
    EF_U16(0x00417194u) = (uint16_t)(x >> 16);
    EF_U16(0x00417196u) = (uint16_t)(y >> 16);
    EF_U16(0x00417198u) = (uint16_t)(z >> 16);
    if ((uint8_t)rotate_view_test_160156(
            EF_U16(EF_U32(0x0041717cu) + 2u), z, y, x, 0x00417154u) != 0u) {
        EF_U32(0x0041716cu) = z;
        dz = (uint16_t)(EF_U16(0x0015909cu) - EF_U16(0x00417198u));
        dy = (uint16_t)(EF_U16(0x00159098u) - EF_U16(0x00417196u));
        EF_U32(0x00417168u) = y;
        dx = (uint16_t)(EF_U16(0x00159094u) - EF_U16(0x00417194u));
        EF_U32(0x00417164u) = x;
        EF_U16(0x00132abcu) = (uint16_t)length_squared_dec90(
            (int16_t)dx, (int16_t)dy, (int16_t)dz);
        record = EF_U32(0x00417174u);
        mode = (EF_U8(record + 0x0au) & 0x7fu) >> 5;
        EF_U32(0x00132af4u) = EF_U32(0x00132af8u + mode * 4u);
        EF_U16(0x0041718eu) = EF_U16(record + 2u);
        EF_U16(0x00417190u) = EF_U16(record + 4u);
        EF_U16(0x00417192u) = EF_U16(record + 6u);
        run_page_text_9e508((uint32_t)page, 1u);
        accepted = 1u;
    }
    if (EF_U8(0x00137a00u) != 0u) {
        if ((EF_U8(EF_U32(0x0041717cu) + 5u) & 1u) != 0u) {
            for (index = 0u; index < EF_U8(EF_U32(0x0041717cu) + 1u); ++index)
                for (channel = 0u; channel < 8u; ++channel)
                    step_page_value_script_a0c04(index, channel);
        } else {
            for (index = 0u; index < EF_U8(EF_U32(0x0041717cu) + 1u); ++index)
                step_page_value_script_a0c04(index, 0u);
        }
    }
    guest_callptr4_x(EF_U32(0x00132ad8u),
        (uint32_t)(EF_S32(0x00417154u) >> 16),
        (uint32_t)(EF_S32(0x00417158u) >> 16),
        (uint32_t)(EF_S32(0x0041715cu) >> 16),
        EF_U8(EF_U32(0x00417174u) + 0x0au) & 0x0fu);
    return accepted;
}
