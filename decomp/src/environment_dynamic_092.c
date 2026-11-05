/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_environment_092.h"
#include "ef_mem.h"

#include <stdint.h>
#include <string.h>

/* 0xdac2c: every lightning entry occupies nine bytes: two parameter
 * words, one guest-heap string pointer, and one conditional flag byte.
 * The name-transform at 0xa4421 remains a guest adapter. */
void env_lightning_point_dac2c(void)
{
    ef_environment_fields_090 f;
    uint8_t index = EF_U8(0x425c87u);
    ef_addr32 entry = 0x421a48u + (uint32_t)index * 9u;
    ef_addr32 name;
    unsigned i;
    env_scan_line_090(0xdac2cu, 0x1901e4u, &f);
    EF_U16(entry) = (uint16_t)f.number[0];
    EF_U16(entry + 2u) = (uint16_t)f.number[1];
    EF_U8(entry + 8u) = (f.number[2] == 1u || f.number[2] == 2u)
        ? (uint8_t)f.number[2] : 0u;
    (void)env_engine_call_090(0xa4421u, 0u, 0u, 0u);
    name = env_engine_call_090(0xe29e8u, (uint32_t)strlen(f.text) + 1u, 0u, 0u);
    EF_U32(entry + 4u) = name;
    for (i = 0u; i < sizeof(f.text); ++i) {
        EF_U8(name + i) = (uint8_t)f.text[i];
        if (f.text[i] == '\0')
            break;
    }
    index = (uint8_t)(index + 1u);
    EF_U8(0x425c87u) = index;
    if (index == 0u)
        env_report_090(0x1901f4u);
    (void)env_engine_call_090(0xb3fb0u, 0u, 0u, 0x50u);
}

/* 0xdaef4: each smoke line supplies a filename plus a 16-bit slot index.
 * For each line, the guest looks up the filename once for every primary
 * and secondary crater, writes the resulting 16-bit value at +0x14+2*slot,
 * and sets +0x12 to 0xffff. Both 110-byte arrays then become circular
 * singly linked lists through the pointer at +0x6a. */
void env_smoke_rings_daef4(void)
{
    int32_t lines = EF_S32(0x425ae4u);
    uint32_t first_count = EF_U32(0x425ae0u);
    uint32_t second_count = EF_U32(0x425ae8u);
    uint32_t first = EF_U32(0x425af4u);
    uint32_t second = EF_U32(0x425aecu);
    int32_t line;
    uint32_t i;
    if (lines == -1)
        env_report_090(0x19029cu);
    EF_U32(0x425b88u) = 0u;
    EF_U32(0x425b24u) = 0u;
    for (line = 0; line < lines; ++line) {
        ef_environment_fields_090 f;
        uint32_t slot;
        env_scan_line_090(0xdaef4u, 0x190004u, &f);
        slot = f.number[0];
        for (i = 0u; (int32_t)i < (int32_t)first_count; ++i) {
            uint32_t entry = first + i * 110u;
            uint16_t value = (uint16_t)env_engine_call_090(0x9ddfcu,
                                                            0u, i * 110u, first);
            EF_U16(entry + 0x14u + slot * 2u) = value;
            EF_U16(entry + 0x12u) = 0xffffu;
        }
        for (i = 0u; (int32_t)i < (int32_t)second_count; ++i) {
            uint32_t entry = second + i * 110u;
            uint16_t value = (uint16_t)env_engine_call_090(0x9ddfcu,
                                                            0u, i * 110u, second);
            EF_U16(entry + 0x14u + slot * 2u) = value;
            EF_U16(entry + 0x12u) = 0xffffu;
        }
    }
    /* The original writes the last link even for count=0 (at base-4),
     * reflecting an unchecked count-1 calculation. These host drafts
     * intentionally keep that write; tests allocate nonempty arrays. */
    for (i = 0u; i + 1u < first_count; ++i)
        EF_U32(first + i * 110u + 0x6au) = first + (i + 1u) * 110u;
    EF_U32(first + (first_count - 1u) * 110u + 0x6au) = first;
    EF_U32(0x425b88u) = first;
    for (i = 0u; i + 1u < second_count; ++i)
        EF_U32(second + i * 110u + 0x6au) = second + (i + 1u) * 110u;
    EF_U32(second + (second_count - 1u) * 110u + 0x6au) = second;
    EF_U32(0x425b24u) = second;
}
