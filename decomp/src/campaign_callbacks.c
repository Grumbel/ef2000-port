/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

static void copy_campaign_key(uint32_t source)
{
    unsigned i;
    /* Three MOVSD instructions with DF clear; original source is 12 bytes. */
    for (i = 0; i < 3u; ++i)
        EF_U32(0x41f30cu + i * 4u) = EF_U32(source + i * 4u);
}

/* Both difficulty callbacks enable the overview and detail widgets. */
static void select_campaign_dialog(void)
{
    ef_addr32 root = find_named_or_abort_a46cc(0x184374u);
    ef_addr32 child = find_widget_checked_a78bc(root, 0x184384u);
    ef_addr32 detail;

    enable_widget_a72d8(child);
    detail = find_widget_checked_a78bc(root, 0x184388u);
    clear_widget_dirty(detail);
    enable_widget_a72d8(detail);
    EF_U8(0x12584au) = 6u;
}

/* 0x40e50, 107 bytes: start new campaign and enter difficulty selector. */
void gc_set_difficulty_40e50(void)
{
    EF_U8(0x1a364bu) = 0x32u;
    EF_U8(0x1a364cu) = 1u;
    refresh_all_widgets_a4c84();
    apply_a4cb4_a4d90(0x18436cu);
    apply_a4cb4_a4d90(0x184374u);
    select_campaign_dialog();
}

/* 0x40ed0, 19 bytes: parse the widget string at +0x3f and mirror AL. */
int32_t gc_performance_no_40ed0(ef_addr32 widget)
{
    int32_t value = parse_int(widget + 0x3fu);
    EF_U8(0x1264e2u) = (uint8_t)value;
    EF_U8(0x12584au) = (uint8_t)value;
    return value;
}

/* 0x40ee4, 52 bytes: reset old-campaign window, default selector, and
 * copy the three original dwords to the 0x41f30c campaign key. */
void gc_load_old_campaign_40ee4(void)
{
    refresh_all_widgets_a4c84();
    apply_a4cb4_a4d90(0x18438cu);
    refresh_campaign_view_c9fc4();
    EF_U16(0x137992u) = 1u;
    copy_campaign_key(0x184358u);
}

/* 0x40f18, 73 bytes: accept existing campaign. */
void gc_loadfile_accepted_40f18(void)
{
    apply_a4b80_a4db0(0x18436cu);
    EF_U16(0x137992u) = 0u;
    EF_U16(0x137994u) = 1u;
    copy_campaign_key(0x18439cu);
    set_pending_name_ac7a8(0x184364u);
    clear_word_41eeda();
    refresh_campaign_view_c9fc4();
}

/* 0x40f64, 53 bytes: initialize menu with the current campaign key. */
void gc_init_main_menu_40f64(void)
{
    EF_U16(0x137992u) = 0u;
    EF_U16(0x137994u) = 1u;
    copy_campaign_key(0x18439cu);
    set_pending_name_ac7a8(0x184364u);
}

/* 0x40f9c, 140 bytes: result of the indirect availability callback
 * selects the new-campaign path or a two-widget fallback. */
void gc_set_campaign_difficulty_40f9c(void)
{
    uint32_t decision;
    refresh_all_widgets_a4c84();
    decision = guest_callptr_eax_ret_x(EF_U32(0x132ab4u), 0x18439cu);
    if (decision != 0u) {
        apply_a4cb4_a4d90(0x18436cu);
        apply_a4cb4_a4d90(0x1843a8u);
    } else {
        EF_U8(0x1a364bu) = 0x32u;
        EF_U8(0x1a364cu) = 1u;
        apply_a4cb4_a4d90(0x184374u);
        select_campaign_dialog();
        refresh_campaign_view_c9fc4();
    }
}
