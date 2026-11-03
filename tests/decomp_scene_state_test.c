/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t guest[0x20000];
ef_addr32 g_scene_query_pair[2];
int32_t g_scene_metric_ref_packed;
ef_scene_triplet_packed g_scene_input_triplet;
ef_scene_triplet_high g_scene_input_high;
int32_t g_scene_runtime_triplet[3];
int16_t g_scene_last_angles[3];
ef_scene_event_counter g_scene_event_counters;
int16_t g_scene_event_history[64];
int16_t g_scene_event_armed;
uint8_t g_scene_control_code;

static ef_addr32 named_group;
static ef_addr32 named_entry;
static int group_calls;
static int entry_calls;
static int metric_calls;
static int32_t metric_return;
static int32_t observed_full_key;
static int16_t observed_key;
static ef_addr32 observed_group;
static int32_t observed_metric_args[3];
static int16_t observed_angles[3];
static int angle_calls;
static int probe_calls;
static int request_calls;
static int16_t requested_pair[2];
static int32_t probed_args[4];
static uint8_t probe_status;
static int callback_calls;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(guest));
    return guest + address;
}

ef_addr32 find_scene_named_group(ef_addr32 name, int32_t key)
{
    assert(name == 0x18ab30);
    ++group_calls;
    observed_full_key = key;
    return named_group;
}

ef_addr32 find_scene_named_entry(int16_t key, ef_addr32 group)
{
    ++entry_calls;
    observed_key = key;
    observed_group = group;
    return named_entry;
}

int32_t evaluate_scene_metric(int32_t initial, ef_addr32 right, ef_addr32 left)
{
    ++metric_calls;
    observed_metric_args[0] = initial;
    observed_metric_args[1] = (int32_t)right;
    observed_metric_args[2] = (int32_t)left;
    return metric_return;
}

void configure_scene_angles(int16_t z, int16_t y, int16_t x)
{
    ++angle_calls;
    observed_angles[0] = z;
    observed_angles[1] = y;
    observed_angles[2] = x;
}

uint8_t probe_scene_pair_state(int32_t base, int32_t kind, int32_t left, int32_t right)
{
    ++probe_calls;
    probed_args[0] = base;
    probed_args[1] = kind;
    probed_args[2] = left;
    probed_args[3] = right;
    return probe_status;
}

void perform_scene_pair_request(int16_t left, int16_t right)
{
    ++request_calls;
    requested_pair[0] = left;
    requested_pair[1] = right;
}

void scene_event_callback(void)
{
    ++callback_calls;
}

int main(void)
{
    int16_t entry_value = -149;
    memcpy(guest + 0x100, &entry_value, sizeof(entry_value));
    named_group = 0x200;
    named_entry = 0x100;
    assert(scene_lookup_named_word(42) == -149);
    assert(group_calls == 1 && entry_calls == 1);
    assert(observed_key == 42 && observed_group == 0x200);
    assert(observed_full_key == 42);
    assert(scene_lookup_named_word(0x1234002a) == -149);
    assert(observed_full_key == 0x1234002a && observed_key == 42);
    assert(scene_lookup_named_word(-1) == 0);
    assert(scene_lookup_named_word(0) == 0);
    assert(group_calls == 4 && entry_calls == 2);
    named_group = 0;
    assert(scene_lookup_named_word(3) == 0 && entry_calls == 2);
    named_group = 0x200;
    named_entry = 0;
    assert(scene_lookup_named_word(3) == 0 && entry_calls == 3);

    g_scene_query_pair[0] = 0x7000;
    g_scene_query_pair[1] = 0x9000;
    metric_return = 7;
    assert(scene_metric_remaining() == -9);
    assert(metric_calls == 1 && observed_metric_args[0] == 0);
    assert(observed_metric_args[1] == 0x7000 && observed_metric_args[2] == 0x9000);
    assert(scene_metric_reaches_threshold(0x1234, -9) == 1);
    assert(scene_metric_reaches_threshold(-1, -8) == 0);
    metric_return = -3;
    assert(scene_metric_remaining() == 1);
    assert(scene_metric_reaches_threshold(0, 1) == 1);

    memset(&g_scene_input_triplet, 0x7b, sizeof(g_scene_input_triplet));
    memset(&g_scene_input_high, 0x5a, sizeof(g_scene_input_high));
    set_scene_input_triplet(0x12345678, (int32_t)0xabcddcbaU, -1);
    assert(g_scene_input_triplet.x == 0x12345678);
    assert((uint32_t)g_scene_input_triplet.y == 0xabcddcbaU);
    assert(g_scene_input_triplet.z == -1);
    assert(g_scene_input_triplet.pad_04[0] == 0x7b);
    assert(g_scene_input_triplet.pad_0a[1] == 0x7b);
    assert(g_scene_input_high.x == 0x1234);
    assert((uint16_t)g_scene_input_high.y == 0xabcd);
    assert(g_scene_input_high.z == -1);
    assert(g_scene_input_high.pad_02[0] == 0x5a);
    assert(g_scene_input_high.pad_06[1] == 0x5a);
    assert(g_scene_runtime_triplet[0] == -1);
    assert((uint32_t)g_scene_runtime_triplet[1] == 0xabcddcbaU);
    assert(g_scene_runtime_triplet[2] == 0x12345678);

    set_scene_angles(-320, 40, 1500);
    assert(angle_calls == 1);
    assert(observed_angles[0] == 1500 && observed_angles[1] == 40);
    assert(observed_angles[2] == -320);
    assert(g_scene_last_angles[0] == -320 && g_scene_last_angles[1] == 40);
    assert(g_scene_last_angles[2] == 1500);

    probe_status = 1;
    request_scene_pair(0x12345, 0x98765);
    assert(probe_calls == 1 && request_calls == 1);
    assert(probed_args[0] == 255 && probed_args[1] == 2);
    assert(probed_args[2] == 0x12345 && probed_args[3] == 0x98765);
    assert((uint16_t)requested_pair[0] == 0x2345);
    assert((uint16_t)requested_pair[1] == 0x8765);
    probe_status = 2;
    request_scene_pair(-1, 0);
    assert(probe_calls == 2 && request_calls == 1);
    probe_status = 255;
    request_scene_pair(0, 0);
    assert(request_calls == 1);

    g_scene_event_counters.unknown_aa = 0xabcd;
    g_scene_event_counters.count = 7;
    g_scene_event_counters.secondary = -3;
    clear_scene_event_counters();
    assert(g_scene_event_counters.unknown_aa == 0xabcd);
    assert(g_scene_event_counters.count == 0 && g_scene_event_counters.secondary == 0);

    g_scene_event_counters.count = 9;
    g_scene_control_code = 0;
    record_scene_event(0x12345);
    assert(g_scene_event_history[9] == 0x2345);
    assert(g_scene_event_counters.count == 10 && callback_calls == 1);
    record_scene_event(88);
    assert(g_scene_event_counters.count == 10 && callback_calls == 2);
    g_scene_control_code = 15;
    record_scene_event(0x34567);
    assert(g_scene_event_armed == 1);
    assert(g_scene_event_counters.count == 1 && g_scene_event_counters.secondary == 0);
    assert(g_scene_event_history[0] == 0x4567 && callback_calls == 3);
    g_scene_control_code = 16;
    record_scene_event(-1);
    assert(g_scene_event_history[0] == -1 && g_scene_event_counters.count == 1);
    assert(callback_calls == 4 && g_scene_event_counters.unknown_aa == 0xabcd);
    return 0;
}
