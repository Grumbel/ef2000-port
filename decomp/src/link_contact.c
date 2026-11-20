/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))
#define ADDR(p)   ((ef_addr32)(uintptr_t)(p))

#define LINK_ROW_SIZE      25
#define LINK_POINT_TABLE   0x0012a6cdu  /* 73-byte rows of three 24-byte points */
#define LINK_POINT_ROW     73
#define LINK_POINT_SIZE    24
#define MODEL_ORIGIN_TABLE 0x001285f5u  /* 229-byte rows */
#define MODEL_ORIGIN_ROW   229

#define RANGE_SCALE        EF_F64(0x00186d7cu)  /* 20.0 */
#define RANGE_OTHER_SIDE   EF_F64(0x00186d84u)  /* 2.0 */
#define MAX_HEIGHT_ABOVE   EF_F64(0x00186d8cu)  /* 25.0 */

#define UNIT_SIDE          0x18   /* int16 */
#define UNIT_FLAGS         0x25
#define UNIT_LINKABLE      0x04
#define UNIT_POSITION      0x49   /* double[3] */
#define UNIT_HEIGHT        0x91   /* double */
#define UNIT_LINK_DONE     0x18a
#define UNIT_LINK_SLOT     0x18b
#define UNIT_LINK_ROW      0x187
#define UNIT_MODEL_ROW     0xe9

#define SIDE_NONE          0xff
#define STATE_LINKED       14

enum { EVENT_LINK_SLOT2 = 0x47, EVENT_LINK_SLOT0 = 0x48, EVENT_LINK_SLOT1 = 0x49, EVENT_LINKED = 0x4a };

/* Rotate the model-space point at MODEL by UNIT's orientation into OUT and
 * add UNIT's position. */
static void world_point(double out[3], ef_addr32 model, int16_t unit, ef_addr32 rec)
{
    int i;

    mul_object_matrix_vector(ADDR(out), model, unit);
    for (i = 0; i < 3; i++)
        out[i] += EF_F64(rec + UNIT_POSITION + 8u * i);
}

/* 0x772d0, 610 bytes. Check whether UNIT has reached the link point of
 * OBJECT (its link slot's model point against UNIT's model origin, both in
 * world space). The watched unit must be within 20 * g_link_range_scale
 * (twice that for another side), at most 25 above OBJECT and flagged
 * linkable; any other unit within 1.0. On contact, a linked session with
 * OBJECT owned by another valid side sends the link packet and moves UNIT
 * to the linked state; otherwise the slot event (0x48/0x49/0x47 for slots
 * 0/1/2) and the linked event are dispatched. */
void check_link_contact_772d0(int16_t object, int16_t unit)
{
    ef_addr32 obj = lookup_filtered_indexed_object(object);
    ef_addr32 rec;
    double range, link_point[3], origin[3], d[3];
    uint8_t row;
    int i;

    if (obj == 0)
        return;
    rec = lookup_filtered_indexed_object(unit);
    if (rec == 0)
        return;
    if (unit == g_watched_unit) {
        range = g_link_range_scale * RANGE_SCALE;
        if (EF_U16(rec + UNIT_SIDE) != EF_U16(obj + UNIT_SIDE))
            range *= RANGE_OTHER_SIDE;
        if (EF_F64(rec + UNIT_HEIGHT) - EF_F64(obj + UNIT_HEIGHT) > MAX_HEIGHT_ABOVE)
            return;
        if ((EF_U8(rec + UNIT_FLAGS) & UNIT_LINKABLE) == 0)
            return;
    } else {
        range = 1.0;
    }

    row = EF_U8(g_object_link_record_base + (uint32_t)EF_U8(obj + UNIT_LINK_ROW) * LINK_ROW_SIZE);
    world_point(link_point, LINK_POINT_TABLE + (uint32_t)row * LINK_POINT_ROW
                + (uint32_t)EF_U8(rec + UNIT_LINK_SLOT) * LINK_POINT_SIZE, object, obj);
    world_point(origin, MODEL_ORIGIN_TABLE + (uint32_t)EF_U8(rec + UNIT_MODEL_ROW) * MODEL_ORIGIN_ROW, unit, rec);
    for (i = 0; i < 3; i++)
        d[i] = link_point[i] - origin[i];
    if (vector_length_b4364(ADDR(d)) >= range)
        return;

    if (g_world_grid_mode != 0 && EF_U16(obj + UNIT_SIDE) != (uint16_t)g_world_grid_marker
        && EF_S16(obj + UNIT_SIDE) != SIDE_NONE) {
        send_two_word_byte_packet_3b((uint16_t)object, (uint16_t)unit, EF_U8(rec + UNIT_LINK_SLOT));
        transition_indexed_object_state(unit, STATE_LINKED);
        EF_U8(rec + UNIT_LINK_DONE) = 1;
        return;
    }
    switch (EF_U8(rec + UNIT_LINK_SLOT)) {
    case 0: dispatch_unit_event_46254((uint32_t)(int32_t)object, unit, EVENT_LINK_SLOT0, 0, 0, 0); break;
    case 1: dispatch_unit_event_46254((uint32_t)(int32_t)object, unit, EVENT_LINK_SLOT1, 0, 0, 0); break;
    case 2: dispatch_unit_event_46254((uint32_t)(int32_t)object, unit, EVENT_LINK_SLOT2, 0, 0, 0); break;
    }
    dispatch_unit_event_46254((uint32_t)(int32_t)object, unit, EVENT_LINKED, 0, 0, 0);
}
