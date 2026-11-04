/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Mode selection from a stored file record and screen reset, 0x45a4..0x4738.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0x45a4, 401 bytes. Read the default record and derive the chord mode from its two fields. */
void select_mode_from_record_45a4(void)
{
    uint8_t path[0x50];
    int16_t first = 0;
    int16_t third = 0;
    int16_t category = 0;
    int16_t fourth = 0;
    uint16_t group_a = 0;
    uint16_t group_b = 0;
    uint16_t group_c = 0;
    uint16_t sel_d = 0;
    uint16_t sel_e = 0;
    uint16_t sel_f = 0;

    build_default_path_9c980((ef_addr32)(uintptr_t)path);
    guest_fn_9ca57((ef_addr32)(uintptr_t)path, 0x0017f6a4u);
    guest_fn_9ca57((ef_addr32)(uintptr_t)path, 0x00125828u);
    guest_fn_105a4((ef_addr32)(uintptr_t)&first, (ef_addr32)(uintptr_t)&category,
                   (ef_addr32)(uintptr_t)&third, (ef_addr32)(uintptr_t)&fourth,
                   (ef_addr32)(uintptr_t)path);
    if ((category >= 5 && category < 8) || (category > 0x13 && category <= 0x15))
        group_a = 1;
    else if (category >= 8 && category <= 0x13)
        group_b = 1;
    else
        group_c = 1;
    if (fourth == 2)
        sel_d = 1;
    else if (fourth == 1)
        sel_e = 1;
    else
        sel_f = 1;
    if (group_a && sel_d) EF_U8(0x0012583bu) = 9;
    if (group_a && sel_e) EF_U8(0x0012583bu) = 7;
    if (group_a && sel_f) EF_U8(0x0012583bu) = 8;
    if (group_b && sel_d) EF_U8(0x0012583bu) = 3;
    if (group_b && sel_e) EF_U8(0x0012583bu) = 1;
    if (group_b && sel_f) EF_U8(0x0012583bu) = 2;
    if (group_c && sel_d) EF_U8(0x0012583bu) = 6;
    if (group_c && sel_e) EF_U8(0x0012583bu) = 4;
    if (group_c && sel_f) EF_U8(0x0012583bu) = 5;
}

/* 0x4738, 321 bytes. Reset the menu: read the clicked mode widget, load the record and refresh. */
void reset_menu_state_4738(void)
{
    ef_addr32 root;
    int32_t index;

    root = guest_open_named_dialog(0x0017f6acu, 0x0017f6b8u);
    if ((EF_U8(guest_select_dialog_widget(root, 0x0017f6b8u) + 0x1du) & 1u) != 0)
        EF_U16(0x00121e44u) = 1;
    root = guest_open_named_dialog(0x0017f6acu, 0x0017f6c0u);
    if ((EF_U8(guest_select_dialog_widget(root, 0x0017f6c0u) + 0x1du) & 1u) != 0)
        EF_U16(0x00121e44u) = 2;
    root = guest_open_named_dialog(0x0017f6acu, 0x0017f6c4u);
    if ((EF_U8(guest_select_dialog_widget(root, 0x0017f6c4u) + 0x1du) & 1u) != 0)
        EF_U16(0x00121e44u) = 3;
    /* ORIGINAL BUG: the word at 0x121e44 is written but its high word (0x121e46) is read. */
    index = ((int32_t)EF_U32(0x00121e44u) >> 16) - 1;
    copy_string_9d749(0x00125828u, EF_U32(0x00121e40u) + (uint32_t)(index * 0x4c) + 0x3au);
    select_mode_from_record_45a4();
    EF_U32(guest_fn_a7ec4_key(0x0017f6ccu, EF_U8(0x0012583bu))) = EF_U8(0x0012583bu);
    guest_fn_abe44(0x0017f6d4u);
    EF_U8(0x00125846u) = 1;
    EF_U16(0x00125835u) = 0;
    EF_U8(0x00125837u) = 0;
    EF_U32(0x0012656cu) = 0;
    EF_U32(0x00126570u) = 0;
    EF_U8(0x001f6f4fu) = 0;
    EF_U8(0x00125838u) = 0;
    if (EF_U8(0x00125935u) == 1) {
        guest_fn_ac7a8(0x0017f6dcu);
    } else {
        refresh_all_widgets_a4c84();
        guest_fn_aca88();
        guest_fn_a4d90(0x0017f6e4u);
        EF_U8(0x0041f2e8u) = 0x64;
    }
    EF_U32(0x0012656cu) = 0;
    EF_U16(0x001f71d2u) = 0;
}
