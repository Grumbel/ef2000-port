/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

typedef void (EF_CDECL *ef_loop_error)(ef_addr32, ef_addr32);

/* 0x345e8, 450 bytes. EAX=widget. Decimal slot and hexadecimal loop value.
 * Full EAX scanner results are tested; empty value text is permitted. */
void change_network_loop_345e8(ef_addr32 widget)
{
    int32_t slot;
    uint32_t value;
    ef_addr32 text;
    ef_addr32 window;
    uint32_t offset;
    uint8_t character;
    uint8_t classification;
    uint8_t valid = 1;
    uint8_t status;
    char number[16];

    if (EF_U8(0x0013578cu) == 0)
        return;
    if (guest_fn_df26f_x(widget + offsetof(ef_widget_link, number_label),
                        0x00182bbcu, (ef_addr32)(uintptr_t)&slot) != 1 ||
        slot < 1 || slot > 4) {
        ((ef_loop_error)(uintptr_t)EF_U32(0x00132a74u))(
            0x00182e18u, widget + offsetof(ef_widget_link, number_label));
    }
    text = EF_U32(widget + offsetof(ef_widget_link, text));
    for (offset = 0; offset < 8u; ++offset) {
        character = EF_U8(text + offset);
        if (character == 0)
            break;
        classification = EF_U8(0x00195af0u + (uint8_t)(character + 1u));
        if ((classification & 0x12u) == 0)
            valid = 0;
    }
    if (valid != 0) {
        if (guest_fn_df26f_x(EF_U32(widget + offsetof(ef_widget_link, text)),
                            0x00182e2cu, (ef_addr32)(uintptr_t)&value) != 1)
            value = 0;
        clear_session_34ea4();
        shutdown_device_a3da0();
        EF_U32(0x00135778u + (uint32_t)slot * 4u) = value;
        compact_table_3458c();
        write_settings_file_a2bdc();
        window = apply_a4cb4_a4d90(0x00182e30u);
        clear_word_41eeda();
        guest_tail_c9fc4(window);
        status = initialize_comms_a3c18();
        set_word_41eeda();
        apply_a4b80_a4db0(0x00182e30u);
        if (status != 0) {
            if (status == 14u) {
                for (offset = 0; offset < 16u; offset += 4u) {
                    if (EF_U32(0x0013577cu + offset) == value)
                        EF_U32(0x0013577cu + offset) = 0;
                }
                compact_table_3458c();
                write_settings_file_a2bdc();
                format_ui_status_value((ef_addr32)(uintptr_t)number, 0x00182e38u, value);
                window = find_named_or_abort_a46cc(0x00182e40u);
                apply_to_widget_a75b4(window, 0x00182e4cu, (uint32_t)(uintptr_t)number);
                run_widget_a4dd0(window);
                shutdown_device_a3da0();
                status = initialize_comms_a3c18();
            }
            if (status != 0)
                dispatch_dialog_state_34b2c();
        }
    }
    guest_fn_33630();
    refresh_menu_name_list_347bc(EF_U32(widget + offsetof(ef_widget_link, parent)));
}
