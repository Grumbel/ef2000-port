/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Level wait loop, sound bank setup, grid table and settings file writer, 0x7eccc..0x83424.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0x7eccc, 216 bytes. Wait until the counter at 0x127da4 reaches the target at 0x12583a. */
void wait_target_count_7eccc(void)
{
    uint32_t deadline;

    EF_U8(0x001a5c32u + (uint32_t)EF_U8(0x001a666bu) * 27u) = 1;
    count_external_grid_markers();
    deadline = ((uint32_t (*)(void))EF_U32(0x0015b620u))() + 0x4074u;
    if (EF_U8(0x0012583au) <= EF_U8(0x00127da4u))
        return;
    while (EF_U16(0x00126518u) == 0 && ((uint32_t (*)(void))EF_U32(0x0015b620u))() < deadline) {
        if (EF_U8(0x00127da4u) < EF_U8(0x0012583au))
            send_command_1f();
        guest_fn_58094();
        guest_fn_e2988(0x1e);
        guest_fn_581b0();
        guest_fn_58094();
        guest_fn_7eda4();
        if (EF_U8(0x0041f791u) != 0
            || ((EF_U8(0x0041f7bau) != 0 || EF_U8(0x0041f7c6u) != 0) && EF_U8(0x0041f7a0u) != 0)) {
            EF_U8(0x0012583au) = EF_U8(0x00127da4u);
            return;
        }
        guest_fn_53270();
        if (EF_U8(0x00127da4u) >= EF_U8(0x0012583au))
            break;
    }
}

/* 0x80058, 192 bytes. Load the first sound bank once and register its release hook. */
void init_sound_bank_80058(void)
{
    uint8_t path[0x84];
    ef_addr32 buf = (ef_addr32)(uintptr_t)path;

    if (EF_U32(0x0012b074u) != 0)
        return;
    init_arena_e29a0();
    guest_fn_9c980_sfx(buf, 0x001887e4u);
    guest_fn_9ca57(buf, 0x001887e4u);
    guest_fn_9ca57(buf, 0x001887f0u);
    switch (guest_fn_e94ec(buf, 0) & 0xffu) {
    case 1:
        guest_report_parser_enum_error(0x001887fcu, buf, 0);
        break;
    case 2:
        guest_report_parser_enum_error(0x0018881cu, buf, 0);
        break;
    default:
        break;
    }
    guest_fn_e93dc(0x0012b0d0u, 1);
    release_buffer_e9574();
    if (EF_U32(0x0012b090u) != EF_U32(0x0012b0b4u))
        ui_report_missing_record(0x00188840u);
    guest_fn_d6f90(EF_U32(0x001f9078u), 1, 1);
    EF_U32(0x0012b074u) = 1;
    guest_fn_9c3cf(0x0007fef8u);
}

/* 0x81230, 214 bytes. Load the second sound bank once, create its voice and register its release hook. */
void init_sound_bank_81230(void)
{
    uint8_t path[0x84];
    ef_addr32 buf = (ef_addr32)(uintptr_t)path;

    if (EF_U32(0x0012b108u) != 0)
        return;
    init_arena_e29a0();
    guest_fn_9c980_sfx(buf, 0x00188934u);
    guest_fn_9ca57(buf, 0x00188934u);
    guest_fn_9ca57(buf, 0x00188940u);
    switch (guest_fn_e94ec(buf, 0) & 0xffu) {
    case 1:
        guest_report_parser_enum_error(0x00188950u, buf, 0);
        break;
    case 2:
        guest_report_parser_enum_error(0x00188970u, buf, 0);
        break;
    default:
        break;
    }
    guest_fn_e93dc(0x0012b228u, 1);
    release_buffer_e9574();
    guest_fn_d6f90(EF_U32(0x001fca00u), 1, 0);
    EF_U32(0x0012b108u) = 1;
    EF_U32(0x0012b114u) = guest_fn_d7764_7(0x00188910u, 1, 1, 0x5622u, 0x7fu, 1, 0);
    guest_fn_d7758(EF_U32(0x0012b114u), 0x32000u);
    alloc_object_text_d6fa8(EF_U32(0x0012b114u));
    guest_fn_9c3cf(0x0008115cu);
}

/* 0x82060, 125 bytes. Allocate the grid table of n rows of 17 bytes and mark every cell empty (0xff). */
void alloc_grid_table_82060(int16_t rows)
{
    int32_t bytes = (int32_t)rows * 17;
    ef_addr32 table;
    int16_t i = 0;

    table = EF_U32(0x001fcb40u);
    table = ((uint32_t (*)(uint32_t, uint32_t))EF_U32(0x0012dd9cu))((uint32_t)bytes, (uint32_t)bytes);
    EF_U32(0x001fcb40u) = table;
    guest_fn_b3fb0(table, 0, (uint32_t)bytes);
    table = EF_U32(0x001fcb40u);
    if (rows > 0) {
        do {
            int16_t j = 0;

            do {
                EF_U16(table + (uint32_t)((int32_t)i * 17) + (uint32_t)((int32_t)j * 2) + 1u) = 0xff;
                j++;
            } while (j < 8);
            i++;
        } while (i < rows);
    }
    EF_U32(0x001fcb40u) = table;
}

/* 0x830b8, 219 bytes. Rewrite the settings file with the status text lines appended. */
void rewrite_settings_830b8(void)
{
    uint8_t text[0x400];
    ef_addr32 buf = (ef_addr32)(uintptr_t)text;
    ef_addr32 name = EF_U32(0x0012b36cu);
    uint32_t length = guest_fn_b3b90(name);
    uint32_t i;

    if (length == 0 || length > 0x400u)
        return;
    guest_fn_b3c04(name, buf);
    text[length] = 0;
    for (i = 0; (int16_t)i < (int32_t)length; i++) {
        if (text[(int16_t)i] == 0x1a)
            text[(int16_t)i] = 0;
    }
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188ae0u));
    write_ui_status_float(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188ab4u));
    write_ui_status_flags_a(buf + guest_string_length(buf));
    compact_ui_fragment(find_matching_ui_fragment(buf, 0x00188abcu));
    write_ui_status_flags_b(buf + guest_string_length(buf));
    write_whole_file_b3d00(EF_U32(0x0012b36cu), buf, guest_string_length(buf));
}

/* 0x83424, 194 bytes. Project a point through the view matrix; false when it is behind the viewer. */
uint8_t project_point_83424(int16_t x, int16_t y, uint32_t scale, ef_addr32 out_x, ef_addr32 out_y)
{
    int32_t yy = y;
    int32_t xx = x;
    int32_t shift = (int32_t)(0x800 >> (4 - ((int32_t)EF_U32(0x001fcbdeu) >> 16)));
    int32_t zz = (int16_t)shift;
    int32_t sum_x = (int32_t)EF_U32(0x0015ffdau) * xx + (int32_t)EF_U32(0x0015ffe6u) * yy
                    + (int32_t)EF_U32(0x0015fff2u) * zz;
    int32_t sum_z = (int32_t)EF_U32(0x0015fff6u) * zz + ((int32_t)EF_U32(0x0015ffeau) * yy
                    + (int32_t)EF_U32(0x0015ffdeu) * xx);
    int32_t sum_y = (int32_t)EF_U32(0x0015fffau) * zz + ((int32_t)EF_U32(0x0015ffeeu) * yy
                    + xx * (int32_t)EF_U32(0x0015ffe2u));

    if (sum_z <= 0)
        return 0;
    EF_S32(out_x) = muldiv_saturate_833a0((int32_t)scale, sum_x, sum_z);
    EF_S32(out_y) = muldiv_saturate_833a0((int32_t)scale, sum_y, sum_z);
    return 1;
}
