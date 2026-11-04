/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Menu setup, view refresh and widget callback helpers (70..130 bytes). */

/* 0x5f634, 98 bytes. Reset the page state and rebuild the menu. */
void reset_menu_page_5f634(void)
{
    uint32_t index;

    guest_fn_5e658_w();
    index = (uint32_t)(EF_S32(0x0012813eu) >> 16);
    EF_U32(0x001a820cu) = 0;
    EF_U8(0x00128148u) = EF_U8(index * 2u + 0x00128142u);
    EF_U32(0x001a8224u) = 0;
    guest_fn_5e4c4_w(0, 0, 0x1e, 0x41, 3);
    EF_U8(0x0012814cu) = 3;
    EF_U8(0x001a6d1eu) = 0;
    EF_U8(0x001a3637u) = 0;
    guest_fn_5ea64_w();
    guest_fn_86920_w();
}

/* 0x6e390, 77 bytes. Apply the standard step when the request is accepted. */
uint8_t apply_step_6e390(uint8_t a, uint8_t b)
{
    double k;

    if (guest_fn_6e320_w(a, b) == 0)
        return 0;
    k = (EF_U8(EF_U32(0x001f71bcu) + 0x16cu) == 0xf) ? 0.05 : 0.2;
    guest_fn_6e3e0_w(0.2, k);
    return 1;
}

/* 0x81110, 74 bytes. Select the page handler or fall back to the default screens. */
void select_page_handler_81110(void)
{
    EF_U32(0x0015bac8u) = 0x001327d8u;
    if (guest_fn_d6e58_w() != 0) {
        EF_U8(0x0015bac6u) = 0;
        EF_U8(0x0015bac7u) = 0;
        guest_fn_82e40_w();
        return;
    }
    guest_fn_7fe8c_w();
    if (EF_U8(0x0012b104u) == 0) {
        guest_fn_810d0_w(1);
        guest_fn_810f4_w(0);
    }
}

/* 0x83b50, 73 bytes. Advance the page counter by 3 or 1 depending on a flag bit. */
void advance_page_counter_83b50(void)
{
    uint8_t before = EF_U8(0x0015912eu);
    uint16_t step = ((EF_U16(0x001fcbe2u) & 1) == 1) ? 3 : 1;

    EF_U8(0x001fcc48u) = before;
    EF_U16(0x001fcbe0u) = step;
    EF_U8(0x0015912eu) = (uint8_t)(before + step);
    guest_fn_dc334_w();
}

/* 0x84544, 93 bytes. Refresh the view, re-centre if the cursor left it, and redraw the panel. */
uint8_t refresh_panel_84544(void)
{
    center_view_842b4();
    update_ui_composite();
    if ((EF_U16(0x001fcbd0u) != 0 || EF_U16(0x001fcbd2u) != 0)
        && guest_fn_834e8_w(EF_S32(0x001fcc3cu) >> 16, EF_S32(0x001fcc3eu) >> 16) != 0) {
        center_view_842b4();
        update_ui_composite();
    }
    guest_fn_dc832_w(0x001fcbb0u, 0x001fcc2cu, 0xc);
    return 0;
}

/* 0x84c84, 120 bytes. Play the queued message strings, then wait for a key. */
void play_queued_messages_84c84(void)
{
    guest_fn_ac7c0_w();
    guest_fn_ac0e0_w();
    if (EF_U16(0x001fcc54u) == 0) {
        guest_fn_ac408_w(0x00189b5cu);
    } else {
        for (;;) {
            uint16_t n = (uint16_t)(EF_U16(0x001fcc54u) - 1);
            EF_U16(0x001fcc54u) = n;
            if (n == 0xffff)
                break;
            guest_fn_ac408_w(EF_U32(EF_U32(0x001fcc50u)));
            EF_U32(0x001fcc50u) += 4;
        }
    }
    do {
        EF_U32(0x0012b4c8u) = guest_fn_ac5f8_w(1);
    } while (EF_U32(0x0012b4c8u) == 0);
    guest_fn_ac7c0_w();
}

/* 0xa3280, 72 bytes. Request a status string for an index and restore the frame delta. */
void request_status_string_a3280(uint16_t index)
{
    uint16_t saved;

    EF_U16(0x00137160u) = index;
    saved = EF_U16(0x001357d6u);
    copy_string_9d749(0x00137162u, EF_U32(0x001368bcu + (uint32_t)index * 4u));
    update_frame_delta_a4138();
    EF_U16(0x001357d6u) = saved;
    issue_request_a3e50(0x00137158u, 0x1b, 2);
}

/* 0xa334c, 86 bytes. Request the details of an entry when its table flag is set. */
void request_entry_details_a334c(uint16_t index)
{
    uint16_t id = EF_U16(EF_U32(0x0013587cu + ((uint32_t)index << 5)));

    if ((EF_U8(EF_U32(0x001368bcu + (uint32_t)id * 4u) + 0x1cu) & 1) == 1) {
        EF_U16(0x00137174u) = id;
        guest_fn_e9a24_w(0x21, 0x00137176u);
        issue_request_a3e50(0x00137174u, 0x23, 4);
    }
}

/* 0xa5abc, 95 bytes. Run and clear the pending callbacks of every registered widget. */
void run_pending_callbacks_a5abc(void)
{
    uint32_t i;

    for (i = 1; i <= EF_U16(0x0041eed8u); i++) {
        ef_addr32 widget = EF_U32(0x0041edfcu + i * 4u);

        if (EF_U8(widget + 0x3bu) & 0x80) {
            if ((EF_U8(widget + 0x3bu) & 4) && EF_U32(widget + 0xb9u) != 0)
                ((void (*)(ef_addr32))EF_U32(widget + 0xb9u))(widget);
            widget = EF_U32(0x0041edfcu + i * 4u);
            EF_U8(widget + 0x3bu) = (uint8_t)(EF_U8(widget + 0x3bu) & 0x7f);
        }
    }
}

/* 0xa9490, 82 bytes. Step a list cursor n rows forward and latch the result. */
void step_list_cursor_a9490(ef_addr32 list, int16_t n)
{
    ef_addr32 node = EF_U32(list);
    int16_t count;
    int16_t i;

    if (EF_U8(0x001371ffu) != 0)
        EF_U16(list + 0x12u) = (uint16_t)(n + 1);
    count = (int16_t)(n * (int16_t)EF_U16(list + 6u));
    if (count > 0) {
        i = 0;
        do {
            i++;
            node = EF_U32(node + 1u);
        } while (i < count);
    }
    EF_U32(0x004191ecu) = node;
    if (EF_U8(list + 0xcu) != 0) {
        EF_U32(0x004191d4u) = EF_U32(EF_U32(0x004191dcu) + 1u);
        EF_U8(0x0041955fu) = 2;
    }
}
