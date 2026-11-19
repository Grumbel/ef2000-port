/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_structs/view_keys.h"

/* 0x32c04, 21 bytes plus its orphan loop body 0x32a7c..0x32b3b (no other
 * entry). EAX key name -> EAX. Search the presets cyclically after the
 * current cursor, then the tracks; the track search starts two entries
 * after its cursor (two increments before the first compare). Indices
 * wrap as signed words and are not range-checked before the first
 * reset, exactly as in the original. A match resets the preset cursor to
 * -1 and returns what 0x2fbc4 left in EAX; no match returns 40, the final
 * loop counter. EBX..EDI preserved, EBP unused. Called by the 23 key
 * stubs at 0x32b3c..0x32bfc. */
uint32_t select_key_view_32c04(ef_addr32 key_name)
{
    int16_t index = g_key_view_preset_cursor;
    int16_t tried;
    ef_addr32 record;

    for (tried = 0; tried < EF_KEY_VIEW_PRESET_COUNT; tried++) {
        index++;
        if (index >= EF_KEY_VIEW_PRESET_COUNT)
            index = 0;
        record = (ef_addr32)(uintptr_t)g_key_view_presets
            + (uint32_t)((int32_t)index * (int32_t)sizeof(ef_key_view_preset));
        if (ascii_stricmp_9d524(record + offsetof(ef_key_view_preset, key_name),
                                key_name) == 0) {
            g_key_view_preset_cursor = -1;
            return guest_fn_2fbc4_r(record);
        }
    }

    index = g_key_view_track_cursor;
    index++;
    for (tried = 0; tried < EF_KEY_VIEW_TRACK_COUNT; tried++) {
        index++;
        if (index >= EF_KEY_VIEW_TRACK_COUNT)
            index = 0;
        record = (ef_addr32)(uintptr_t)g_key_view_tracks
            + (uint32_t)((int32_t)index * (int32_t)sizeof(ef_key_view_track));
        if (ascii_stricmp_9d524(record + offsetof(ef_key_view_track, key_name),
                                key_name) == 0) {
            g_key_view_track_selected[0] = 1u;
            g_key_view_track_selected[1] = 1u;
            g_key_view_preset_cursor = -1;
            return guest_fn_2fbc4_r(record);
        }
    }
    return EF_KEY_VIEW_TRACK_COUNT;
}

/* 0x32b90, 12-byte stub; Ghidra also assigns it the shared loop body
 * 0x32a7c..0x32b3b recovered above. Select the view bound to F8.
 * Caller 95e58@960b0. */
uint32_t select_key_view_f8_32b90(void)
{
    return select_key_view_32c04(0x00182af4u);
}
