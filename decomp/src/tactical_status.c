/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_guest_memory.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define VIEW_INDEX          0x001a6d1bu /* byte, row of the 15-byte view table */
#define VIEW_TABLE          0x00128170u /* +0 x, +2 right edge, +6 y (words) */
#define PLAYER_OBJECT_ID    0x001f71cau
#define G_LOAD              0x001a3040u /* double */
#define ECM_ACTIVE          0x001a81e5u
#define LABELS_HIDDEN       0x001a81e1u
#define TARGET_PRIMARY      0x001a823eu
#define TARGET_SECONDARY    0x001a8238u
#define TACTICAL_MODE       0x001282c5u
#define SHOW_GUIDED_STORES  0x00122886u

#define STORE_RECORD_SIZE   0x91u
#define STORE_NAME          0x0012bc6cu /* name pointer */
#define STORE_GUIDANCE      0x0012bc97u

/* Player object fields. */
#define OBJECT_STATION      0x1d7u /* signed byte */
#define OBJECT_STATIONS     0x198u /* 3-byte entries, store at +3 */
#define STATION_GUN         0x13
#define STATION_NONE        0x14

/* The original frame: the label buffer sits directly below the two
   coordinate words, so a label longer than 11 characters overwrites them
   before they are read. */
struct status_frame {
    char text[12];
    int16_t x;
    int16_t pad_x;
    int16_t y;
    int16_t pad_y;
};

static int32_t station_store(ef_addr32 object, int32_t station)
{
    return (int8_t)EF_U8(object + OBJECT_STATIONS + 3u * (uint32_t)station + 3u);
}

/* 0x5aa64, 651 bytes. Draw the tactical status block of the player
   object: the selected store with its round count, the G load, the ECM
   tag, then (unless a guided store is hidden) the range marker when
   MARKER is not 0xff, the target readout and the mode-specific labels. */
void draw_tactical_status_5aa64(uint8_t marker)
{
    struct status_frame f;
    uint32_t row = 15u * EF_U8(VIEW_INDEX);
    ef_addr32 object;
    int32_t station;
    int32_t store;
    ef_addr32 name;
    int16_t count;
    uint8_t draw_marker;

    f.x = (int16_t)EF_U16(VIEW_TABLE + row);
    object = guest_fn_7489c(EF_S16(PLAYER_OBJECT_ID));
    if (object == 0)
        return;
    station = (int8_t)EF_U8(object + OBJECT_STATION);
    if (station == STATION_NONE)
        return;
    f.y = (int16_t)(3 * EF_S16(VIEW_TABLE + row + 6u) / 6);
    draw_marker = (uint8_t)(marker + 1u);
    if (station == STATION_GUN) {
        draw_marker = 0;
        count = (int16_t)EF_U16(object + 0x19cu + 3u * (uint32_t)station);
    } else {
        count = (int16_t)count_matching_activity_slots(object);
    }

    name = EF_U32(STORE_NAME + STORE_RECORD_SIZE * (uint32_t)station_store(object, station));
    if (name != 0) {
        if (count != 0)
            guest_sprintf_a4342((ef_addr32)(uintptr_t)f.text, 0x00185e8cu, name, (int32_t)count);
        else
            guest_sprintf_a4342((ef_addr32)(uintptr_t)f.text, 0x00185ed8u, name);
    } else {
        f.text[0] = (char)EF_U8(0x00185edcu);
    }
    draw_tactical_coordinates(f.x, f.y, (uint32_t)(uintptr_t)f.text);
    guest_sprintf_a4342((ef_addr32)(uintptr_t)f.text, 0x00185ee0u, EF_F64(G_LOAD));
    draw_tactical_coordinates(-0x3c, -0x20, (uint32_t)(uintptr_t)f.text);

    if (EF_U8(ECM_ACTIVE) != 0) {
        int16_t y;
        int32_t width;

        guest_sprintf_a4342((ef_addr32)(uintptr_t)f.text, 0x00185e88u);
        y = (int16_t)(f.y - 0x18);
        width = draw_tactical_item_metric((ef_addr32)(uintptr_t)f.text);
        draw_tactical_coordinates((int16_t)(EF_U16(VIEW_TABLE + row + 2u) - width), y,
                                  (uint32_t)(uintptr_t)f.text);
    }

    store = station_store(object, (int8_t)EF_U8(object + OBJECT_STATION));
    if (store != -1 && EF_U8(STORE_GUIDANCE + STORE_RECORD_SIZE * (uint32_t)store) == 2
        && EF_U8(SHOW_GUIDED_STORES) == 0)
        return;
    if (draw_marker != 0)
        draw_range_marker_5df08();
    if (EF_S16(TARGET_PRIMARY) != -1 || EF_S16(TARGET_SECONDARY) != -1)
        draw_target_readout_5acf0();
    switch (EF_U8(TACTICAL_MODE)) {
    case 0:
    case 1:
        if (EF_U8(LABELS_HIDDEN) == 0)
            draw_contact_labels_5d1c4();
        refresh_selected_label_5c354();
        break;
    case 6:
        if (EF_U8(LABELS_HIDDEN) == 0)
            draw_contact_labels_5d1c4();
        draw_tactical_marker_5c518();
        break;
    }
}
