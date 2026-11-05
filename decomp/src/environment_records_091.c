/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_environment_091.h"
#include "ef_mem.h"

#include <stdint.h>

/* The first two handlers construct a path using unresolved 0xe7cec,
 * then check and load the file through DOS-facing guest callbacks. */
static void darkside_file(uint32_t entry, uint32_t suffix,
                          uint32_t warning, uint32_t destination)
{
    ef_environment_fields_090 f;
    env_scan_line_090(entry, 0x190000u, &f);
    (void)env_engine_call_090(0xe7cecu, 0x190064u, 0u, suffix);
    if (env_engine_call_090(0x132ab4u, 0u, 0u, 0u) == 0u)
        env_report_090(warning);
    (void)env_engine_call_090(0x132ab8u, 0u, destination, 0u);
}

void env_load_darkside_palette_da748(void)
{
    darkside_file(0xda748u, 0x190060u, 0x190070u, 0x421750u);
}

void env_set_darkside_values_da8c4(void)
{
    darkside_file(0xda8c4u, 0x1900a4u, 0x1900a8u, 0x425190u);
}

/* Original crater index is the signed high half of the dword at 425bba.
 * The 16-bit counter at 425bbc is that very same high half. A count of
 * exactly twenty reports an error but does not suppress the write. */
void env_set_crater_count_dad4c(void)
{
    ef_environment_fields_090 f;
    int32_t index = EF_S32(0x425bbau) >> 16;
    uint32_t offset;
    if (index == 20)
        env_report_090(0x190218u);
    env_scan_line_090(0xdad4cu, 0x190250u, &f);
    offset = 0x4218e0u + (uint32_t)index * 18u;
    EF_U32(offset) = f.number[0] << 11;
    EF_U32(offset + 4u) = f.number[1] << 11;
    EF_U32(offset + 8u) = f.number[2] << 11;
    EF_U32(offset + 12u) = f.number[3] << 11;
    EF_U16(offset + 16u) = (uint16_t)f.number[4];
    EF_U16(0x425bbcu) = (uint16_t)(EF_U16(0x425bbcu) + 1u);
}

/* Original allocator size is 110*n, twice: two arrays of 110-byte
 * records. The guest allocation itself remains an adapter. */
void env_allocate_craters_dae40(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xdae40u, 0x18ff74u, &f);
    EF_U32(0x425ae0u) = f.number[0];
    EF_U32(0x425ae4u) = f.number[1];
    EF_U32(0x425ae8u) = f.number[2];
    if (EF_S32(0x425ae4u) > 43)
        env_report_090(0x190264u);
    EF_U32(0x425af4u) = env_engine_call_090(0xe29e8u,
                                             EF_U32(0x425ae0u) * 110u, 0u, 0u);
    EF_U32(0x425aecu) = env_engine_call_090(0xe29e8u,
                                             EF_U32(0x425ae8u) * 110u, 0u, 0u);
}

/* Five logical scanf outputs: field 2 is the signed texture index,
 * fields 0/1 are bytes, fields 3/4 are words. The 16-byte record is
 * inserted at the head of a singly linked guest list. */
void env_sun_texture_db654(void)
{
    ef_environment_fields_090 f;
    ef_addr32 record;
    env_scan_line_090(0xdb654u, 0x190374u, &f);
    record = env_engine_call_090(0xe29e8u, 16u, 0u, 0u);
    EF_U32(record + 12u) = EF_U32(0x425b20u);
    EF_U32(0x425b20u) = record;
    if ((int16_t)f.number[2] >= EF_S32(0x15bb60u))
        env_report_090(0x190388u);
    EF_U8(record + 3u) = (uint8_t)f.number[0];
    EF_U8(record + 2u) = (uint8_t)f.number[1];
    EF_U16(record + 10u) = (uint16_t)f.number[2];
    EF_U16(record + 4u) = (uint16_t)f.number[3];
    EF_U16(record + 8u) = 1u;
    EF_U16(record + 6u) = (uint16_t)f.number[4];
}

/* At most two cannon hits, each occupying eighteen bytes. The original
 * signed comparison also permits a negative index; tests cover 0..2. */
void env_cannon_hit_db730(void)
{
    ef_environment_fields_090 f;
    int32_t index;
    uint32_t record;
    env_scan_line_090(0xdb730u, 0x1903a8u, &f);
    index = EF_S32(0x425b8cu);
    if (index >= 2) {
        env_report_090(0x1903bcu);
        return;
    }
    record = 0x425b64u + (uint32_t)index * 18u;
    EF_U32(record) = f.number[0];
    EF_U32(record + 4u) = f.number[1];
    EF_U32(record + 8u) = f.number[2];
    EF_U32(record + 12u) = f.number[3];
    EF_U8(record + 16u) = (uint8_t)f.number[4];
    EF_U8(record + 17u) = (uint8_t)f.number[5];
    EF_U32(0x425b8cu) = (uint32_t)index + 1u;
}
