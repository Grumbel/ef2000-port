/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Custom-mission ("custom" dialog) slot editing. The five flight slots
 * are kept in parallel arrays: aircraft count, aircraft type, guns-only
 * flag and skill. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0x7de5c, 446 bytes, change callback of a custom-mission slot widget.
 * Find which "slot1"-chained entry of the "custom" dialog is current
 * (the first with flag bit 0 set; with a null WIDGET the scan stays at
 * slot 0). A name of up to two characters is a number: in the "custom"
 * dialog itself it is the aircraft count, elsewhere (for slots after
 * the first) the 1-based aircraft type. The names gunsyes/gunsno and
 * high/medium/low set the guns-only flag and the skill. Finally the
 * slot line "<count> <type> <armament> <skill>" is shown. */
void simSyncSlots(ef_addr32 widget)
{
    char text[0x64];
    ef_addr32 name;
    ef_addr32 entry;
    uint8_t slot;

    g_custom_slot = 0;
    entry = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00187f94u), /* "custom" */
                                      0x00187fc8u);                          /* "slot1" */
    if (widget != 0u) {
        while ((EF_U8(entry + 0x1du) & 1u) == 0u) {
            g_custom_slot++;
            entry = EF_U32(entry + 0x21u);
        }
    }
    name = widget + 0x3fu;
    if (strlen_ds(name) <= 2u) {
        if (guest_strcmp(EF_U32(widget + 0x51u), 0x00187f94u) == 0)     /* "custom" */
            g_custom_slot_count[g_custom_slot] = (uint8_t)parse_int(name);
        else if (g_custom_slot != 0)
            g_custom_slot_type[g_custom_slot] = (int16_t)(parse_int(name) - 1);
    }
    slot = g_custom_slot;
    if (guest_strcmp(name, 0x001880b8u) == 0)                  /* "gunsyes" */
        g_custom_slot_guns_only[slot] = 1u;
    else if (guest_strcmp(name, 0x00188604u) == 0)             /* "gunsno" */
        g_custom_slot_guns_only[slot] = 0u;
    else if (guest_strcmp(name, 0x00187eecu) == 0)             /* "high" */
        g_custom_slot_skill[slot] = 0u;
    else if (guest_strcmp(name, 0x00187fc0u) == 0)             /* "medium" */
        g_custom_slot_skill[slot] = 1u;
    else if (guest_strcmp(name, 0x00187ee8u) == 0)             /* "low" */
        g_custom_slot_skill[slot] = 2u;

    slot = g_custom_slot;
    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, 0x0018860cu,    /* "%d %s %s %s" */
                        (uint32_t)g_custom_slot_count[slot],
                        g_text_strings[g_aircraft_type_text[g_custom_slot_type[slot]]],
                        g_custom_slot_guns_only[slot] != 0u ? 0x0013291au   /* guns only */
                                                            : 0x00131eebu,  /* full armament */
                        g_skill_names[g_custom_slot_skill[slot]]);
    show_widget_a7588(entry, (ef_addr32)(uintptr_t)text);
}
