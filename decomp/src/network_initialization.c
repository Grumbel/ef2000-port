/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x34bcc, 215 bytes. No arguments; dispatch initialization's AL status.
 * Status 14 removes every matching entry before compacting and reporting. */
void initialize_network_dialog_34bcc(void)
{
    uint8_t status;
    uint32_t selected;
    uint32_t offset;
    char number[16];
    ef_addr32 dialog;

    if (EF_U8(0x0013579au) == 3u && check_files_343dc() == 0)
        return;
    status = initialize_comms_a3c18();
    if (status == 0) {
        EF_U8(0x0019f5aeu) = 1;
        guest_fn_33630();
    } else if (status == 22u) {
        guest_fn_33630();
    } else if (status == 14u) {
        selected = EF_U32(0x001357bcu);
        for (offset = 0; offset < 16u; offset += 4u) {
            if (EF_U32(0x0013577cu + offset) == selected)
                EF_U32(0x0013577cu + offset) = 0;
        }
        EF_U32(0x001357bcu) = selected;
        compact_table_3458c();
        write_settings_file_a2bdc();
        format_ui_status_value((ef_addr32)(uintptr_t)number, 0x00182e38u,
                               EF_U32(0x001357bcu));
        dialog = apply_a4cb4_a4d90(0x00182e40u);
        apply_to_widget_a75b4(dialog, 0x00182e4cu, (uint32_t)(uintptr_t)number);
    } else {
        dispatch_dialog_state_34b2c();
    }
}
