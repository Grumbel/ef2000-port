/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_environment_091.h"
#include "ef_mem.h"

#include <stdint.h>
#include <string.h>

static void copy_guest_string(ef_addr32 destination, const char *name)
{
    unsigned i;
    for (i = 0u; i < 80u; ++i) {
        EF_U8(destination + i) = (uint8_t)name[i];
        if (name[i] == '\0')
            return;
    }
}

static void append_guest_suffix(ef_addr32 destination, ef_addr32 suffix)
{
    uint32_t i = 0u, j = 0u;
    while (EF_U8(destination + i) != 0u && i < 80u)
        ++i;
    do {
        uint8_t c = EF_U8(suffix + j++);
        EF_U8(destination + i++) = c;
        if (c == 0u)
            return;
    } while (j < 80u);
}

/* Sun flare parses text, two live words and two local values. Its file
 * lookup at 0xde864 returns AX, then one of the locals is squared into
 * 425ba2; the other is saved unchanged in 425ba4. */
void env_sun_flare_db28c(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xdb28cu, 0x190310u, &f);
    EF_U16(0x425bb0u) = 0u;
    EF_U16(0x425bb4u) = (uint16_t)f.number[1];
    EF_U16(0x425bb0u) = (uint16_t)f.number[2];
    /* Original append target is a stack buffer, so text and its suffix
     * cannot be passed as a real guest pointer through this host adapter. */
    (void)env_engine_call_090(0x9ca57u, 0u, 0x190324u, 0u);
    EF_U16(0x425bb8u) = (uint16_t)env_engine_call_090(0xde864u,
                                                       0u, 0u, 0u);
    EF_U16(0x425ba4u) = (uint16_t)f.number[4];
    EF_U16(0x425ba2u) = (uint16_t)(f.number[3] * f.number[3]);
}

/* 425afc controls the number of additional horizon lines consumed.
 * -1 emits a warning and consumes zero lines; callbacks may mutate
 * the guest count between iterations, as in the original. */
void env_fix_horizon_db808(void)
{
    uint32_t i = 0u;
    if (EF_S32(0x425afcu) == -1)
        env_report_090(0x190400u);
    while ((int32_t)i < EF_S32(0x425afcu)) {
        ef_environment_fields_090 f;
        env_scan_line_090(0xdb808u, 0x190004u, &f);
        (void)env_engine_call_090(0x2a7acu, 0u, f.number[0], 0u);
        ++i;
    }
}

/* 40 five-byte records. Original comparison is index >= 40 with no
 * lower-bound guard. Third field is negated and truncated to 16 bits. */
void env_heavenly_body_db8bc(void)
{
    ef_environment_fields_090 f;
    int32_t index;
    uint32_t record;
    env_scan_line_090(0xdb8bcu, 0x19043cu, &f);
    index = (int32_t)f.number[0];
    if (index >= 40)
        return;
    record = 0x422398u + (uint32_t)index * 5u;
    EF_U16(record) = (uint16_t)(0u - f.number[1]);
    EF_U8(record + 2u) = EF_U8(0x425c86u);
    EF_U16(record + 3u) = (uint16_t)env_engine_call_090(0x9ddfcu,
                                                        0u, 0u, 0u);
}

void env_init3d_file_dbff8(void)
{
    ef_environment_fields_090 f;
    size_t length;
    env_scan_line_090(0xdbff8u, 0x190000u, &f);
    length = strlen(f.text);
    if (length == 0u || length > 8u)
        env_report_090(0x1904d4u);
    /* The guest uses 0x9d749/0x9ca57, which are not yet ABI recovered.
     * These bounded copies model the observed destination and suffix. */
    copy_guest_string(0x17d368u, f.text);
    append_guest_suffix(0x17d368u, 0x190504u);
    (void)env_engine_call_090(0x107310u, 0u, 0u, 0u);
}

void env_east_west_palettes_dc094(void)
{
    ef_environment_fields_090 f;
    size_t length;
    env_scan_line_090(0xdc094u, 0x190000u, &f);
    length = strlen(f.text);
    if (length == 0u || length > 12u)
        env_report_090(0x19050cu);
    copy_guest_string(0x15bb64u, f.text);
    (void)env_engine_call_090(0xe4010u, 0x15bb64u, 0u, 0u);
}

/* SETSUN is not a scanf callback: its first token ends at a space (or
 * NUL), and the remaining input is an LF/NUL-terminated second token.
 * Only the second token strips the byte immediately before the delimiter.
 * The path assembly/copy remain guest adapters with unresolved stack
 * buffer arguments; the two destination addresses and call order are real. */
void env_setsun_dc124(void)
{
    ef_addr32 start = EF_U32(0x425af8u);
    ef_addr32 end = start;
    while (EF_U8(end) != 0u && EF_U8(end) != (uint8_t)' ')
        ++end;
    EF_U8(end) = 0u;
    EF_U32(0x425af8u) = end + 1u;
    (void)env_engine_call_090(0xe7cecu, 0x190064u, start, 0x1900a4u);
    (void)env_engine_call_090(0x132ab8u, 0u, 0x420f90u, 0u);

    start = EF_U32(0x425af8u);
    end = start;
    while (EF_U8(end) != 0u && EF_U8(end) != (uint8_t)'\n')
        ++end;
    EF_U8(end) = 0u;
    EF_U8(end - 1u) = 0u;
    EF_U32(0x425af8u) = end + 1u;
    (void)env_engine_call_090(0xe7cecu, 0x190064u, start, 0x1900a4u);
    (void)env_engine_call_090(0x132ab8u, 0u, 0x421290u, 0u);
    EF_U8(0x15bb74u) = 1u;
}
