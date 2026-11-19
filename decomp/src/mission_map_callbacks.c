/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* For each callback, check the original call sites and use the named
 * declaration that matches the callee's registers and stack arguments.
 * Bind unrecovered targets in ef_abi.h and keep reads that occur after a
 * callback, since the callback can change the selection. */

/* 0x4c610, 37 bytes: 16-bit map selection toggles as 1 - old. */
void mp_cycle_map_4c610(void)
{
    uint16_t mode = EF_U16(0x12656au);

    EF_U8(0x12654cu) = 1u;
    EF_U8(0x1a54f3u) = 1u;
    EF_U16(0x12656au) = (uint16_t)(1u - mode);
}

/* 0x4bc40, 62 bytes: dispatch zoom by the map's one-based mode. */
void mp_zoom_in_map_4bc40(ef_addr32 widget)
{
    /* Each original CMP loads the byte again after a possible callback. */
    if (EF_U8(0x1a53dcu) == 1u)
        update_marker_4bc80(widget);
    if (EF_U8(0x1a53dcu) == 2u)
        check_flags_4be88(widget);
    if (EF_U8(0x1a53dcu) == 4u)
        adjust_route_view_4dcd8(widget);
}

/* 0x4bec0, 45 bytes: dirty or mode-selected map needs redraw. */
void mp_redraw_map_4bec0(ef_addr32 widget)
{
    if (EF_U8(0x1a54f3u) || EF_U8(0x1a559eu)) {
        draw_active_marker_e5088();
        mp_map_select_4bef0();
        EF_U8(0x1a54f3u) = 0u;
    }
    (void)widget; /* mpMapSelect overwrites the incoming EAX. */
}

/* 0x4bef0, 42 bytes: redraw map layers and, for mode 4, height widgets. */
void mp_map_select_4bef0(void)
{
    /* Original EBX/EAX/EDX: 137960, 137980, 137988. */
    draw_map_layers_4bf1c(0x137980u, 0x137988u, 0x137960u);
    if (EF_U8(0x1a53dcu) == 4u)
        draw_route_overlay_4e360();
}

static void increment_three_way_flag(uint32_t address)
{
    uint8_t value = EF_U8(address);

    EF_U8(0x12654cu) = 1u;
    EF_U8(0x1a54f3u) = 1u;
    value = (uint8_t)(value + 1u);
    EF_U8(address) = value;
    if (value == 3u)
        EF_U8(address) = 0u;
}

/* 0x4c638, 50 bytes: EWR overlay cycles 0,1,2 (other bytes wrap). */
void mp_ewr_coverage_4c638(void)
{
    increment_three_way_flag(0x1a559du);
}

/* 0x4c66c, 50 bytes: flags overlay has its own three-way state. */
void mp_toggle_flags_4c66c(void)
{
    increment_three_way_flag(0x1a559eu);
}

/* 0x4dae8, 100 bytes: clear the selected route only if it is unlocked,
 * there is an active object and the selected index is not 0xff. */
void mp_clear_wp_route_4dae8(void)
{
    uint8_t index;
    uint8_t item;
    ef_addr32 node;
    if (EF_U8(0x1a5532u) == 0u) {
        update_dialog_bindings(0x132351u, 0x132373u);
        return;
    }
    if (EF_U32(0x1a54f6u) == 0u)
        return;
    index = EF_U8(0x1a5598u);
    item = EF_U8(0x1a5544u + index);
    if (item == 0xffu)
        return;
    EF_U8(0x1a5540u) = item;
    node = find_named_or_abort_a46cc(0x184b1cu);
    clear_children_dirty(node);
    /* Clearing the children can change the selection. */
    index = EF_U8(0x1a5598u);
    enable_widget_a72d8(EF_U32(0x1a5558u + 4u * index));
}

/* 0x4e768, 79 bytes: reset 24 waypoint heights to 1000; entry zero
 * remains untouched. The original redraw uses EAX=0 and 0x4e7b8. */
void mp_redraw_wp_heights_4e768(void)
{
    uint32_t i;
    EF_U8(0x1a54f3u) = 1u;
    for (i = 1u; i < 25u; ++i) {
        EF_U8(0x1a5672u + i) = 0x11u;
        EF_U32(0x1a560au + i * 4u) = 1000u;
    }
    EF_U8(0x1a559fu) = 1u;
    EF_U8(0x1a5708u) = 0u;
    EF_U8(0x1a5709u) = 0u;
    update_counter_widget_4e7b8(0u);
}

/* 0x4eb78, 19 bytes: this named 'IncSpeed' callback toggles zoom mode. */
uint32_t mp_inc_speed_4eb78(void)
{
    uint8_t mode = EF_U8(0x1a55a1u) != 1u;
    EF_U8(0x1a55a1u) = mode;
    return mode;
}

/* 0x4ec14, 46 bytes: speed adds 60 with 16-bit wrapping first and only
 * then checks against 900. This deliberately retains the wrap behavior. */
void mp_dec_speed_4ec14(void)
{
    uint16_t speed = (uint16_t)(EF_U16(0x1a56f4u) + 60u);
    EF_U16(0x1a56f4u) = speed;
    if (speed > 900u)
        EF_U16(0x1a56f4u) = 900u;
    rebuild_waypoint_speed_4ec70();
}
