/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t guest[0x10000];

int16_t g_render_feature_index;
int16_t g_render_feature_limit;
int16_t g_render_feature_enabled;
uint32_t g_grid_secondary_selector;
int16_t g_scene_gate_enabled;
int32_t g_scene_gate_target_packed;
int32_t g_scene_gate_limit;
uint16_t g_scene_angle_saved_first;
uint16_t g_scene_angle_saved_second;
uint16_t g_scene_angle_saved_third;
double g_scene_vector_scale_a;
double g_scene_vector_scale_b;
double g_scene_scalar_scale;
double g_navigation_vector_scale_a;
double g_navigation_vector_scale_b;

static int16_t selected_index;
static ef_addr32 selected_object;
static unsigned lookup_count;
static unsigned callback_count;
static unsigned callback_kind[4];
static ef_addr32 callback_address[4];
static uint16_t callback_value[4];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(guest));
    return guest + address;
}

ef_addr32 lookup_filtered_indexed_object(int16_t index)
{
    ++lookup_count;
    selected_index = index;
    return selected_object;
}

static void record_callback(unsigned kind, ef_addr32 address, uint16_t value)
{
    assert(callback_count < 4);
    callback_kind[callback_count] = kind;
    callback_address[callback_count] = address;
    callback_value[callback_count] = value;
    ++callback_count;
}

void initialize_scene_angle_record(ef_addr32 record)
{
    record_callback(0, record, 0);
}

void update_scene_angle_first(ef_addr32 record, uint16_t value)
{
    record_callback(1, record, value);
}

void update_scene_angle_second(ef_addr32 record, uint16_t value)
{
    record_callback(2, record, value);
}

void update_scene_angle_third(ef_addr32 record, uint16_t value)
{
    record_callback(3, record, value);
}

static void test_scene_controls(void)
{
    configure_scene_feature(-12);
    assert(g_render_feature_index == -12);
    assert(g_render_feature_limit == 50 && g_render_feature_enabled == 1);
    configure_scene_feature(300);
    assert(g_render_feature_index == 300);

    g_grid_secondary_selector = 0xfff90000u; /* signed high word -7 */
    selected_object = 0x200;
    set_secondary_object_byte(0xc5);
    assert(lookup_count == 1 && selected_index == -7);
    assert(guest[0x200 + 0x1d7] == 0xc5);
    selected_object = 0;
    set_secondary_object_byte(0x7f);
    assert(lookup_count == 2 && guest[0x200 + 0x1d7] == 0xc5);

    g_scene_gate_enabled = 0;
    assert(scene_gate_allows(INT32_MIN) == 1);
    g_scene_gate_enabled = 1;
    g_scene_gate_limit = 120;
    g_scene_gate_target_packed = 100 << 16;
    assert(scene_gate_allows(99 * 256) == 0);
    assert(scene_gate_allows(100 * 256) == 1);
    assert(scene_gate_allows(101 * 256) == 1);
    g_scene_gate_target_packed = 120 << 16; /* flips to <= at equality */
    assert(scene_gate_allows(119 * 256) == 1);
    assert(scene_gate_allows(120 * 256) == 1);
    assert(scene_gate_allows(121 * 256) == 0);
    g_scene_gate_target_packed = (int32_t)(uint32_t)0xfffd0000u;
    g_scene_gate_limit = 0;
    assert(scene_gate_allows(-1024) == 0);
    assert(scene_gate_allows(-768) == 1);

    configure_scene_angle_triplet((int16_t)0xfffe, 0x1234, (int16_t)0x8001);
    assert(callback_count == 4);
    assert(callback_kind[0] == 0 && callback_kind[1] == 1);
    assert(callback_kind[2] == 2 && callback_kind[3] == 3);
    for (unsigned i = 0; i < 4; ++i) {
        assert(callback_address[i] == 0x214404);
    }
    assert(callback_value[1] == 0x1234);
    assert(callback_value[2] == 0xfffe);
    assert(callback_value[3] == 0x8001);
    assert(g_scene_angle_saved_first == 0xfffe);
    assert(g_scene_angle_saved_second == 0x1234);
    assert(g_scene_angle_saved_third == 0x8001);
}

static void test_vector_math(void)
{
    static const double a[3] = {2.5, -4.0, 10.0};
    static const double b[3] = {0.5, 3.0, 18.0};
    memcpy(guest + 0x1000, a, sizeof(a));
    memcpy(guest + 0x1100, b, sizeof(b));
    subtract_scene_vectors(0x1200, 0x1000, 0x1100);
    double out[3];
    memcpy(out, guest + 0x1200, sizeof(out));
    assert(out[0] == -2.0 && out[1] == 7.0 && out[2] == 8.0);

    /* In-place subtraction must retain each field's per-field load/store
     * order, and cannot assume the output is a fresh allocation. */
    subtract_scene_vectors(0x1100, 0x1000, 0x1100);
    memcpy(out, guest + 0x1100, sizeof(out));
    assert(out[0] == -2.0 && out[1] == 7.0 && out[2] == 8.0);

    int32_t scalar = -14;
    memcpy(guest + 0x2000 + 0x26, &scalar, sizeof(scalar));
    g_scene_scalar_scale = 0.5;
    convert_scene_scalar(0x2000, 0x2300);
    double converted;
    memcpy(&converted, guest + 0x2300, sizeof(converted));
    assert(converted == -7.0);

    int32_t source[3] = {12, -8, 6};
    memcpy(guest + 0x3000, source, sizeof(source));
    g_scene_vector_scale_a = 0.5;
    g_scene_vector_scale_b = 2.0;
    convert_scene_vector(0x3000, 0x3100);
    memcpy(out, guest + 0x3100, sizeof(out));
    assert(out[0] == 6 && out[1] == 12 && out[2] == -8);

    g_navigation_vector_scale_a = -2.0;
    g_navigation_vector_scale_b = 0.5;
    convert_navigation_vector(0x3000, 0x3200);
    memcpy(out, guest + 0x3200, sizeof(out));
    assert(out[0] == -6 && out[1] == -12 && out[2] == 8);
}

int main(void)
{
    test_scene_controls();
    test_vector_math();
    return 0;
}
