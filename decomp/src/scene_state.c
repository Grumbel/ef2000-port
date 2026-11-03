/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x0009125c, size 0x3c, 16 distinct caller functions. 0x18ab30 is
 * passed as the *address* of the directory name, never dereferenced here.
 * AX=0 and AX=-1 are explicit missing-key sentinels. */
int16_t scene_lookup_named_word(int32_t key)
{
    ef_addr32 group = find_scene_named_group(0x18ab30, key);
    int16_t low_key = (int16_t)key;
    if (group != 0 && low_key != 0 && low_key != -1) {
        ef_addr32 entry = find_scene_named_entry(low_key, group);
        if (entry != 0) {
            return *(const int16_t *)ef_guest_memory_ptr(entry);
        }
    }
    return 0;
}

/* 0x00091298, size 0x1d, callers include 0x918f0 and seven others.
 * The original calls 0x7b16c with EAX=0, EDX=0x21444c, EBX=0x214450.
 * The subtraction wraps modulo 32 bits, not at a C signed overflow trap. */
int32_t scene_metric_remaining(void)
{
    int32_t value = evaluate_scene_metric(0, g_scene_query_pair[0], g_scene_query_pair[1]);
    return (int32_t)(0xfffffffeu - (uint32_t)value);
}

/* 0x000918f0, size 0x1e, seven independent caller functions. The EAX
 * input is overwritten before 0x91298 and has no effect on the predicate;
 * EDX is preserved in ECX for a signed >= comparison. */
uint8_t scene_metric_reaches_threshold(int32_t ignored, int32_t threshold)
{
    (void)ignored;
    return (uint8_t)(scene_metric_remaining() >= threshold);
}

/* 0x00091910, size 0x49, 13 distinct callers. Three original 32-bit
 * inputs are copied to two places; their signed high words are mirrored
 * to 16-bit slots 4 bytes apart without overwriting the gap bytes. */
void set_scene_input_triplet(int32_t x, int32_t y, int32_t z)
{
    g_scene_input_triplet.x = x;
    g_scene_input_triplet.y = y;
    g_scene_input_triplet.z = z;
    g_scene_input_high.x = (int16_t)((uint32_t)x >> 16);
    g_scene_input_high.y = (int16_t)((uint32_t)y >> 16);
    g_scene_input_high.z = (int16_t)((uint32_t)z >> 16);
    g_scene_runtime_triplet[2] = x;
    g_scene_runtime_triplet[1] = y;
    g_scene_runtime_triplet[0] = z;
}

/* 0x0009195c, size 0x3f, 14 distinct callers. Watcom passes x/y/z
 * in AX/DX/BX; the original calls a stack-argument matrix routine, then
 * copies the low 16 bits of its three incoming values to 0x2144a4..a8. */
void set_scene_angles(int16_t x, int16_t y, int16_t z)
{
    configure_scene_angles(z, y, x);
    g_scene_last_angles[0] = x;
    g_scene_last_angles[1] = y;
    g_scene_last_angles[2] = z;
}

/* 0x00091acc, size 0x2d, 14 distinct callers. Probe 0x913dc with
 * EAX=255, EDX=2, EBX=left, ECX=right; invoke 0x91afc only for
 * returned unsigned AL values 0 or 1. */
void request_scene_pair(int32_t left, int32_t right)
{
    uint8_t status = probe_scene_pair_state(0xff, 2, left, right);
    if (status < 2) {
        perform_scene_pair_request((int16_t)left, (int16_t)right);
    }
}

/* 0x00091c20, size 0x13, caller functions 0x91c48, 0x91c9c,
 * 0x91d30. Both words are zeroed; +0x2144ac is also the high signed
 * half of a dword at +0x2144aa. */
void clear_scene_event_counters(void)
{
    g_scene_event_counters.count = 0;
    g_scene_event_counters.secondary = 0;
}

/* 0x00091c48, size 0x50, 12 distinct caller functions. Control codes
 * 15/16 re-arm the history, then a signed high-word bound <10 controls
 * insertion. There is deliberately no lower-bound test on the counter. */
void record_scene_event(int32_t packed_event)
{
    if (g_scene_control_code == 15 || g_scene_control_code == 16) {
        g_scene_event_armed = 1;
        clear_scene_event_counters();
    }
    if (g_scene_event_counters.count < 10) {
        int16_t slot = g_scene_event_counters.count;
        g_scene_event_history[slot] = (int16_t)packed_event;
        g_scene_event_counters.count = (int16_t)(uint16_t)(slot + 1);
    }
    scene_event_callback();
}
