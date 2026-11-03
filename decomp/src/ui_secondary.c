/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

static uint16_t read16(ef_addr32 address)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(address);
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read32(ef_addr32 address)
{
    return (uint32_t)read16(address) | ((uint32_t)read16(address + 2u) << 16);
}

static void write16(ef_addr32 address, uint16_t value)
{
    uint8_t *p = (uint8_t *)ef_guest_memory_ptr(address);
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

/* 0x82da4, 43 bytes. Scan to CR, LF or NUL, then skip any CR/LF run;
 * NUL returns its own address. The guest sometimes returns through a
 * shared epilogue at 0x82c08 after detecting NUL. */
ef_addr32 next_ui_fragment(ef_addr32 string)
{
    uint8_t ch;
    for (;;) {
        ch = *(uint8_t *)ef_guest_memory_ptr(string);
        if (ch == 0 || ch == '\r' || ch == '\n') { break; }
        ++string;
    }
    while ((ch = *(uint8_t *)ef_guest_memory_ptr(string)) == '\r' || ch == '\n') {
        ++string;
    }
    return string;
}

/* 0x83294, 34 bytes. The original subtracts full signed dwords from
 * two global reference coordinates before calling the metric sink. */
uint8_t ui_point_near_reference(int32_t x, int32_t y)
{
    int32_t delta_x = (int32_t)((uint32_t)x - g_ui_reference_x);
    int32_t delta_y = (int32_t)((uint32_t)y - g_ui_reference_y);
    return guest_ui_distance_metric(delta_x, delta_y) <= 0x3b4c00;
}

/* 0x83708, 42 bytes. The preparation callback runs first; only after
 * that do we read both 16-bit coordinates and the two offset globals. */
void compose_ui_relative_vector(ef_addr32 base, uint32_t d,
                                uint32_t b, uint32_t c)
{
    guest_prepare_ui_vector(base, d, b, c);
    write16(base, (uint16_t)(read16(base) - g_ui_composite_offset_x));
    write16(base + 2u, (uint16_t)(read16(base + 2u) - g_ui_composite_offset_y));
}

/* 0x84514, 47 bytes. Set the word latch to one before a twelve-byte
 * guest memory copy, then clear the byte latch. The original returns AL=0. */
uint8_t initialize_ui_component(void)
{
    g_ui_component_enabled = 1;
    guest_copy_ui_component(0x001fcc2c, 0x001fcbb0, 12);
    g_ui_component_latch = 0;
    return 0;
}

/* 0x89924, 60 bytes. Restore +0x5a after the callback even when the
 * earlier event gate is false. The global marker is unconditionally reset. */
void recalculate_object_temporarily(ef_addr32 object)
{
    uint8_t saved = *(uint8_t *)ef_guest_memory_ptr(object + 0x5a);
    *(uint8_t *)ef_guest_memory_ptr(object + 0x5a) = 1;
    g_ui_active_record_scan_latch = 0;
    if (g_tactical_action_enabled) {
        int32_t index = (int32_t)read32(object + 0x2c) >> 16;
        notify_tactical_action(index, 4);
    }
    guest_recalculate_object(object);
    *(uint8_t *)ef_guest_memory_ptr(object + 0x5a) = saved;
}

/* 0x8b650, 50 bytes. Two indirect callbacks bracket a pointer
 * conversion. All three failures return zero; success returns candidate. */
ef_addr32 select_validated_ui_record(ef_addr32 owner)
{
    ef_addr32 source = guest_read_candidate_record();
    ef_addr32 candidate = source ? guest_convert_candidate_record(source) : 0;
    if (candidate != 0 && guest_validate_candidate_record(owner, candidate) == 0) {
        candidate = 0;
    }
    return candidate;
}

/* 0x8be3c, 23 bytes. The second callback receives the original pair,
 * irrespective of how the first callback mutates caller-scratch regs. */
void update_object_remove_link(ef_addr32 object, ef_addr32 link)
{
    guest_object_prepare_unlink(object, link);
    guest_object_apply_unlink(object, link);
}

/* 0x92254, 59 bytes. Both inputs gate the call. Only low 16-bit halves
 * are written back from the bitwise-NOT arithmetic; the first also
 * subtracts 0x400 with wrap. */
void prepare_navigation_component(ef_addr32 object, ef_addr32 predicate)
{
    if (object == 0 || predicate == 0) { return; }
    guest_prepare_navigation_component(0x002143e4, predicate, object);
    write16(0x002143e4, (uint16_t)(~read16(0x002143e4) - 0x400u));
    write16(0x002143e6, (uint16_t)~read16(0x002143e6));
}
