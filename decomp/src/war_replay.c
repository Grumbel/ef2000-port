/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* War replay track: 10-byte points (x, y, hour, minute, ...). */
#define WAR_TRACK         0x001a3650u
#define TRACK_X(i)        EF_S16(WAR_TRACK + 10u * (uint32_t)(i))
#define TRACK_Y(i)        EF_S16(WAR_TRACK + 10u * (uint32_t)(i) + 2u)
#define TRACK_HOUR(i)     EF_S16(WAR_TRACK + 10u * (uint32_t)(i) + 4u)
#define TRACK_MINUTE(i)   EF_S16(WAR_TRACK + 10u * (uint32_t)(i) + 6u)

/* Final markers: 5-byte records (kind, x, y). */
#define WAR_MARKERS       0x001a4aa0u

/* Replay events: 12-byte records (x, y, sprite, hour, minute, ...). */
#define EVENT_X           0x00u
#define EVENT_Y           0x02u
#define EVENT_SPRITE      0x04u
#define EVENT_HOUR        0x06u
#define EVENT_MINUTE      0x08u
#define EVENT_SIZE        12u

/* Map sprite table: 9-byte records with the sheet position (x, y). */
#define MAP_SPRITES       0x001341bdu
#define SPRITE_X(k)       EF_S16(MAP_SPRITES + 9u * (uint32_t)(k))
#define SPRITE_Y(k)       EF_S16(MAP_SPRITES + 9u * (uint32_t)(k) + 2u)
#define SPRITE_WAR_START  142
#define SHEET_PITCH       0x280u

static int reached(int16_t hour, int16_t minute)
{
    return g_war_hour > hour || (g_war_hour == hour && g_war_minute >= minute);
}

static int at_end(void)
{
    return g_war_hour == g_war_end_hour && g_war_minute == g_war_end_minute;
}

static ef_addr32 sprite_source(int32_t sprite)
{
    return g_map_sprite_sheet + (uint32_t)(SPRITE_Y(sprite) * (int32_t)SHEET_PITCH + SPRITE_X(sprite));
}

/* 0x456e8, 625 bytes. Draw one step of the war won/lost replay: the front
   line up to the replay time, the final markers once the end is reached,
   the start sprite and the events that have happened; then finish the
   replay or advance the clock by one minute (wrapping at 24 hours). */
void draw_war_replay_456e8(void)
{
    int16_t i;
    ef_addr32 event;

    g_war_start_hour = TRACK_HOUR(0);
    g_war_start_minute = TRACK_MINUTE(0);
    for (i = 1; i < g_war_track_count; i++) {
        if (reached(TRACK_HOUR(i), TRACK_MINUTE(i)))
            guest_fn_c95ac_x(1, (uint32_t)(int32_t)TRACK_X(i - 1), (uint32_t)(int32_t)TRACK_Y(i - 1),
                             (uint32_t)(int32_t)TRACK_X(i), (uint32_t)(int32_t)TRACK_Y(i));
    }

    if (at_end()) {
        for (i = 0; i < g_war_marker_count; i++) {
            ef_addr32 marker = WAR_MARKERS + 5u * (uint32_t)i;
            guest_fn_4161c(9, i, EF_U8(marker), EF_S16(marker + 1u), EF_S16(marker + 3u));
        }
    }

    guest_fn_ca620((int32_t)sprite_source(SPRITE_WAR_START), (int16_t)(TRACK_X(0) + 2),
                   (int16_t)(TRACK_Y(0) - 10), 0x28u, 0x14u, SHEET_PITCH);
    for (i = 0; i < g_war_event_count; i++) {
        event = g_war_events + EVENT_SIZE * (uint32_t)(int32_t)i;
        if (reached(EF_S16(event + EVENT_HOUR), EF_S16(event + EVENT_MINUTE)))
            guest_fn_ca620((int32_t)sprite_source(EF_S16(event + EVENT_SPRITE)),
                           (int16_t)(EF_S16(event + EVENT_X) - 10), (int16_t)(EF_S16(event + EVENT_Y) - 10),
                           0x14u, 0x14u, SHEET_PITCH);
    }

    if (at_end()) {
        guest_fn_aca94_w();
        return;
    }
    g_war_minute++;
    if (g_war_minute > 59) {
        g_war_hour++;
        g_war_minute = 0;
        if (g_war_hour > 23)
            g_war_hour = 0;
    }
}
