/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint8_t identifier[2], cursors[16], rotation_index[2], values[8], repeat;
} ef_page_value_record;

EF_STATIC_ASSERT(page_value_cursors, offsetof(ef_page_value_record, cursors) == 2);
EF_STATIC_ASSERT(page_value_rotation, offsetof(ef_page_value_record, rotation_index) == 18);
EF_STATIC_ASSERT(page_value_values, offsetof(ef_page_value_record, values) == 20);
EF_STATIC_ASSERT(page_value_repeat, offsetof(ef_page_value_record, repeat) == 28);
EF_STATIC_ASSERT(page_value_record_size, sizeof(ef_page_value_record) == 29);

/* 0xa0c04, 340 bytes. AX=record, DX=value channel; EDX scratch.
 * Callers9e023/9e218 pass signed words and retain EBX/ECX loop state.
 * CS table a0be4 selects cases0..7; case5 shares the default stop path. */
void step_page_value_script_a0c04(uint32_t record_index, uint32_t channel_index)
{
    ef_addr32 record = EF_U32(0x00417174u) + 0x0bu
        + (uint32_t)(int32_t)(int16_t)record_index * 29u;
    uint32_t channel = (uint32_t)(int32_t)(int16_t)channel_index;
    ef_addr32 cursor = record + channel * 2u + 2u;
    ef_addr32 value = record + channel + 0x14u;
    ef_addr32 code;
    uint16_t instruction, displacement;

    if (EF_U16(cursor) == 0u)
        return;
    for (;;) {
        code = EF_U32(0x0041717cu)
            + (uint32_t)(EF_S32(cursor - 2u) >> 16) * 2u;
        instruction = EF_U16(code);
        switch (instruction & 0xffu) {
        case 0:
            EF_U8(value) = (uint8_t)(instruction >> 8);
            EF_U16(cursor) = (uint16_t)(EF_U16(cursor) + 1u);
            return;
        case 1:
            EF_U8(value) = (uint8_t)(EF_U8(value) + 1u);
            EF_U16(cursor) = (uint16_t)(EF_U16(cursor) + 1u);
            return;
        case 2:
            EF_U8(value) = (uint8_t)(EF_U8(value) - 1u);
            EF_U16(cursor) = (uint16_t)(EF_U16(cursor) + 1u);
            return;
        case 3:
            if (EF_U8(value) == (instruction >> 8)) {
                EF_U16(cursor) = (uint16_t)(EF_U16(cursor) + 2u);
                break;
            }
            /* Fall through to the relative branch on inequality. */
        case 4:
            displacement = EF_U16(code + 2u);
            EF_U16(cursor) = (uint16_t)(EF_U16(cursor) + displacement + 1u);
            break;
        case 6:
            EF_U16(cursor) = (uint16_t)(EF_U16(cursor) + 2u);
            /* Read the operand after the cursor store, including aliasing. */
            run_page_text_9e508(EF_S32(0x00417228u) >> 16,
                (uint32_t)(EF_S32(code) >> 16));
            break;
        case 7:
            EF_U16(cursor) = (uint16_t)(EF_U16(cursor) + 2u);
            code += 2u + (uint32_t)EF_U16(code + 2u) * 2u;
            run_text_on_page_9e494((uint32_t)(EF_S32(0x00417228u) >> 16), code);
            break;
        default:
            EF_U16(cursor) = 0u;
            return;
        }
        /* Only entry tests for cursor zero; branches continue at index zero. */
    }
}
