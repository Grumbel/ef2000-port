/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_environment_090.h"
#include "ef_mem.h"

#include <stdint.h>

/* Guest filename helpers: the calls to 0x9d749 and 0x9ca57 are presumed
 * copy/append from their call sites; the host drafts explicitly retain
 * their guest entry points instead of claiming their ABIs are proven. */
static void copy_name(ef_addr32 destination, const char *text)
{
    unsigned i;
    for (i = 0u; i < 80u; ++i) {
        EF_U8(destination + i) = (uint8_t)text[i];
        if (text[i] == '\0')
            return;
    }
}

static void append_guest_name(ef_addr32 destination, ef_addr32 suffix)
{
    unsigned i = 0u;
    unsigned j = 0u;
    while (EF_U8(destination + i) != 0u)
        ++i;
    do {
        uint8_t c = EF_U8(suffix + j++);
        EF_U8(destination + i++) = c;
        if (c == 0u)
            return;
    } while (j < 80u);
}

/* 0xda7fc size 197: original table has five possible palette slots,
 * followed by an unconditional increment (even when beyond slot 4). */
void env_load_missile_palette_da7fc(void)
{
    static const uint32_t palette[][2] = {
        { 0x424b60u, 0x433f30u }, { 0x422460u, 0x437f34u },
        { 0x4257e0u, 0x43bf38u }, { 0x4254e0u, 0x43ff3cu },
        { 0x424e90u, 0x443f40u }
    };
    ef_environment_fields_090 f;
    uint16_t index;
    uint32_t found;
    env_scan_line_090(0xda7fcu, 0x190000u, &f);
    /* The path-building and loader adapters are still unknown guest calls. */
    (void)env_engine_call_090(0xe7cecu, 0x190064u, 0u, 0u);
    found = env_engine_call_090(0x132ab4u, 0u, 0u, 0u);
    if (found == 0u)
        env_report_090(0x1900a8u);
    index = EF_U16(0x425ba0u);
    if (index < 5u)
        (void)env_engine_call_090(0xdbf3cu, palette[index][0],
                                  0u, palette[index][1]);
    EF_U16(0x425ba0u) = (uint16_t)(index + 1u);
}

/* 0xda964 size 112: two scanned words and two 16-bit locals persist. */
void env_world_name_da964(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xda964u, 0x1900dcu, &f);
    EF_U32(0x425b94u) = f.number[0];
    EF_U32(0x425b90u) = f.number[1];
    EF_U16(0x425bc6u) = (uint16_t)f.number[2];
    EF_U16(0x425bccu) = (uint16_t)f.number[3];
}

/* 0xda9d4 size 136: nonempty map name and the guest's suffix append. */
void env_xmap_name_da9d4(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xda9d4u, 0x190000u, &f);
    if (f.text[0] == '\0')
        env_report_090(0x1900e8u);
    copy_name(0x425bf0u, f.text);
    append_guest_name(0x425bf0u, 0x190118u);
}

/* 0xdaa5c size 225: five indexed map names, stride 25. The original
 * error report does not prevent invalid-index writes; host tests use
 * indices within the allocated table. */
void env_mmap_name_daa5c(void)
{
    ef_environment_fields_090 f;
    int32_t index;
    env_scan_line_090(0xdaa5cu, 0x190120u, &f);
    if (f.text[0] == '\0')
        env_report_090(0x190128u);
    index = (int16_t)f.number[0];
    if (index < 0 || index >= 5)
        env_report_090(0x190168u);
    copy_name(0x425bf0u + (uint32_t)index * 25u, f.text);
    append_guest_name(0x425bf0u + (uint32_t)index * 25u, 0x190118u);
}

/* 0xdab40 size 136: reference point filename + map-image extension. */
void env_ref_point_dab40(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xdab40u, 0x190000u, &f);
    if (f.text[0] == '\0')
        env_report_090(0x1901acu);
    copy_name(0x425c6du, f.text);
    append_guest_name(0x425c6du, 0x190118u);
}

/* 0xdabc8 size 97: the original 0xa4421 string transformation is
 * represented as a guest adapter before the copy. */
void env_map_name_dabc8(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xdabc8u, 0x190000u, &f);
    (void)env_engine_call_090(0xa4421u, 0u, 0u, 0u);
    copy_name(0x425b28u, f.text);
}

/* 0xdb0b4 size 93: target 0x80d8c returns its result in DH. The
 * adapter returns a synthetic EDX and the byte is stored at 0x15bb5c. */
void env_load_player_virtual_db0b4(void)
{
    ef_environment_fields_090 f;
    uint32_t edx;
    env_scan_line_090(0xdb0b4u, 0x190000u, &f);
    edx = env_engine_call_090(0x80d8cu, 0u, 0xff00u, 0u);
    EF_U8(0x15bb5cu) = (uint8_t)(edx >> 8);
}

/* 0xdb114, 0xdb16c and 0xdb1c4: three distinct guest callbacks with
 * the same line parser but different original callee addresses. */
void env_load_player_sort_db114(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xdb114u, 0x190000u, &f);
    (void)env_engine_call_090(0x392fcu, 0u, 0u, 0u);
}

void env_load_player_db16c(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xdb16cu, 0x190000u, &f);
    (void)env_engine_call_090(0x39300u, 0u, 0u, 0u);
}

void env_set_flak_count_db1c4(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xdb1c4u, 0x190000u, &f);
    (void)env_engine_call_090(0x3930cu, 0u, 0u, 0u);
}

/* 0xdb21c size 110: parse flak label, warn above 20 (signed), load
 * flak definitions regardless of that warning. */
void env_load_flak_db21c(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xdb21cu, 0x1902d8u, &f);
    EF_U32(0x425afcu) = f.number[0];
    if (EF_S32(0x425ae4u) > 20)
        env_report_090(0x1902dcu);
    (void)env_engine_call_090(0x2a794u, 0u, 0u, 0u);
}

/* 0xdb420 size 179: cloud-fade entries are 20-byte records. The final
 * field is decremented even if it was zero; count is not clamped at 20. */
void env_cloud_fade_db420(void)
{
    ef_environment_fields_090 f;
    uint32_t count = EF_U32(0x15bb60u);
    uint32_t destination = 0x421590u + count * 20u;
    unsigned i;
    if (count == 20u)
        env_report_090(0x190334u);
    env_scan_line_090(0xdb420u, 0x19034cu, &f);
    for (i = 0u; i < 5u; ++i)
        EF_U32(destination + i * 4u) = f.number[i];
    EF_U32(destination + 16u) -= 1u;
    EF_U32(0x15bb60u) = count + 1u;
}
