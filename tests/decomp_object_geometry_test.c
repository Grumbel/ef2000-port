/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

static uint8_t guest[0x20000];
ef_addr32 g_world_grid_base = 0x8000;
ef_grid_type_record g_grid_type_records[256];
uint8_t g_object_kind_modes[256 * 0xe5];
uint8_t g_reference_clock[6];
double g_grid_distance_scale = 2.0;
static uint8_t feature_1e;
static uint8_t feature_1d;
static ef_addr32 indexed[32];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(guest));
    return guest + address;
}

uint8_t query_indexed_object_feature(int16_t index, uint8_t code)
{
    assert(index == 5);
    assert(code == 0x1e || code == 0x1d);
    return code == 0x1e ? feature_1e : feature_1d;
}

ef_addr32 lookup_filtered_indexed_object(int16_t index)
{
    return (unsigned)index < 32 ? indexed[index] : 0;
}

ef_addr32 lookup_indexed_object(int16_t index)
{
    return lookup_filtered_indexed_object(index);
}

static void set_clock(uint8_t *destination, int16_t a, int16_t b, int16_t c)
{
    memcpy(destination + 0, &a, 2);
    memcpy(destination + 2, &b, 2);
    memcpy(destination + 4, &c, 2);
}

int main(void)
{
    ef_motion_object *object = (ef_motion_object *)(guest + 0x1000);
    ef_world_grid_cell *row = (ef_world_grid_cell *)(guest + 0x8000);
    double *point = (double *)(guest + 0x9000);
    double *second = (double *)(guest + 0x9100);
    double *p = (double *)(guest + 0x2000 + 0x49);
    double *q = (double *)(guest + 0x2200 + 0x49);
    uint8_t *clock = guest + 0x4000;

    g_object_kind_modes[0x12 * 0xe5] = 2;
    g_object_kind_modes[0x16 * 0xe5] = 2;
    g_object_kind_modes[0x15 * 0xe5] = 2;
    g_object_kind_modes[0x18 * 0xe5] = 3;
    g_object_kind_modes[0x1d * 0xe5] = 4;
    g_object_kind_modes[0x1e * 0xe5] = 4;
    g_object_kind_modes[0x1f * 0xe5] = 4;
    assert(object_kind_is_eligible(0x12) == 1);
    assert(object_kind_is_eligible(0x16) == 1);
    assert(object_kind_is_eligible(0x15) == 0);
    assert(object_kind_is_eligible(0x18) == 0);
    assert(object_kind_is_eligible(0x1d) == 0);
    assert(object_kind_is_eligible(0x1e) == 0);
    assert(object_kind_is_eligible(0x1f) == 1);
    assert(object_kind_is_eligible(0xff) == 1);

    indexed[5] = 0x1000;
    object->index_packed = 5 << 16;
    object->class_code = 3;
    g_grid_type_records[3].limit_a = 10;
    g_grid_type_records[3].limit_b = 3;
    g_grid_type_records[3].limit_multiplier = 5;
    g_grid_type_records[3].limit_extra = 7;
    assert(object_type_limit(0x1000) == 25);
    feature_1e = 0x14;
    assert(object_type_limit(0x1000) == 22);
    feature_1d = 0x14;
    assert(object_type_limit(0x1000) == 10);
    object->current_limit = 20;
    assert(lower_object_limit(0x1000) == 1);
    assert(object->current_limit == 10);
    assert(lower_object_limit(0x1000) == 0);
    object->current_limit = NAN;
    assert(lower_object_limit(0x1000) == 0);
    reset_object_limit(5);
    assert(object->current_limit == 10);
    reset_object_limit(31); /* rejected lookup */

    row->position_a = 3;
    row->position_b = 4;
    row->position_c = 5000;
    point[0] = 0;
    point[1] = 0;
    point[2] = -100;
    assert(grid_distance_to_point(0x9000, 0) == 5);
    assert(scaled_grid_distance_to_point(0x9000, 0) == 10);
    g_grid_distance_scale = -2;
    assert(scaled_grid_distance_to_point(0x9000, 0) == 10);

    set_clock(g_reference_clock, 12, 0, 2);
    set_clock(clock, 12, 0, 0);
    second[0] = 3;
    second[1] = 4;
    assert(object_speed_from_clock(0x9000, 0x9100, 0x4000) == 2.5);
    set_clock(clock, 12, 0, 5);
    assert(object_speed_from_clock(0x9000, 0x9100, 0x4000) == 5);

    indexed[6] = 0x2000;
    indexed[7] = 0x2200;
    p[0] = 0; p[1] = 0; p[2] = 90000;
    q[0] = 3; q[1] = 4; q[2] = -90000;
    assert(indexed_object_distance(6, 7) == 5);
    assert(indexed_object_distance(6, 31) == 0);
    return 0;
}
