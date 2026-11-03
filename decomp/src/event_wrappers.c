/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_guest_memory.h"

static uint8_t read8(ef_addr32 address)
{
    return *(const uint8_t *)ef_guest_memory_ptr(address);
}

static int16_t read_signed16(ef_addr32 address)
{
    return (int16_t)((uint16_t)read8(address) |
                     ((uint16_t)read8(address + 1u) << 8));
}

static void write8(ef_addr32 address, uint8_t value)
{
    *(uint8_t *)ef_guest_memory_ptr(address) = value;
}

/* 0x14ef4, 29 bytes: walk indices zero through 33 inclusive, with
 * each call receiving the current signed index in EAX. */
void refresh_all_event_slots(void)
{
    for (int16_t i = 0; i < 34; ++i) {
        guest_refresh_event_slot(i);
    }
}

/* 0x1d5cc, 38 bytes: the optional first dispatch is controlled by
 * a guest mode byte, but the second dispatch always runs. */
void dispatch_two_object_events(ef_addr32 object, int16_t index)
{
    if (read8(0x00127da9) != 0) {
        guest_dispatch_object_event_optional(object, index);
    }
    guest_dispatch_object_event_required(object, index);
}

/* 0x4f314, 88 bytes: open, bind two named child widgets, forward the
 * active dialog byte at +0x10, then close the containing guest widget. */
void update_dialog_bindings(ef_addr32 first, ef_addr32 second)
{
    ef_addr32 dialog = guest_open_named_dialog(0x00184b80, 0x00184e00);
    ef_addr32 widget_a = guest_select_dialog_widget(dialog, 0x00184e00);
    ef_addr32 widget_b = guest_select_dialog_widget(dialog, 0x00184e0c);
    guest_bind_dialog_widget(widget_a, first);
    guest_bind_dialog_widget(widget_b, second);
    guest_emit_dialog_flag(read8(dialog + 0x10u));
    guest_destroy_named_dialog(dialog);
}

/* 0x4f6bc, 74 bytes. ORIGINAL BUG: the loop index is BL and wraps
 * at 255, so a signed positive count greater than 255 never terminates. */
void flush_active_dialog_slots(void)
{
    uint8_t index = 0;
    int16_t limit = read_signed16(0x001357b0);
    while ((int16_t)index < limit) {
        uint8_t slot = read8(0x001a5510u + index);
        if (read8(0x001a553bu + slot) != 0) {
            write8(0x001a553bu + slot, 0);
            update_dialog_bindings(0x001323da, 0x001323f6);
        }
        ++index;
    }
}
