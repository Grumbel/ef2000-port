/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Slot, bounds and menu-button helpers in the low game code, 0x7dec..0x386f8. */

/* 0x7dec, 85 bytes. */
uint32_t apply_slot_7dec(int16_t unit_id, uint8_t table, uint8_t index)
{
    ef_addr32 unit = guest_fn_7489c(unit_id);
    ef_addr32 entry;

    if (unit == 0)
        return 0;
    entry = EF_U32(0x001972e0u) + (uint32_t)table * 34u;
    return guest_fn_7d20(unit + 0x49u, EF_U32(entry + 0x16u) + (uint32_t)index * 56u);
}

/* 0x7e44, 164 bytes. Place unit UNIT_ID of table TABLE at its roster slot
 * (0x7bc0, 0xff for none): the slot point from 0x7ca8 becomes the x/z position
 * and the slot's wrapped heading (double at +4 of the 25-byte entries at
 * table +0x1e) the heading passed to 0x6d38c, with zero rates, -1.0 and
 * mode 0xc. */
void place_unit_at_slot_7e44(int16_t unit_id, uint8_t table)
{
    double pos[3]; /* x and z from 0x7ca8; 0x6d38c fills in the height */
    ef_addr32 entry;
    uint8_t slot;

    if (guest_fn_7489c(unit_id) == 0)
        return;
    slot = (uint8_t)pick_roster_slot_7bc0((uint32_t)(int32_t)unit_id, table);
    if (slot == 0xff)
        return;
    scaled_entry_point_7ca8((ef_addr32)(uintptr_t)pos, table, slot);
    entry = EF_U32(EF_U32(0x001972e0u) + (uint32_t)table * 34u + 0x1eu) + (uint32_t)slot * 25u;
    guest_fn_6d38c(unit_id, (ef_addr32)(uintptr_t)pos,
                   wrap_angle_b42f0(*(double *)ef_guest_memory_ptr(entry + 4u)), 0.0, 0.0, 0.0, -1.0, 0xc);
}

/* 0x7ee8, 524 bytes. Place unit UNIT_ID in its formation cell of table
 * TABLE: the last of the table's 56-byte cells whose word +0x20 equals the
 * unit's cell word +0x28 (cell 0 in a linked session, none: nothing). The
 * position is the table origin (0x6/0xa through 0xb4230) plus the cell
 * offset (+0x8/+0xc) times the scale at 0x1803f8, plus the unit type's
 * formation offset (the 16-byte double pair at 0x197270 + 16 * type, its
 * first value lengthened by 3/8 of the negated cell depth +0x10), rotated
 * by the cell heading (0x81ec). 0x6d38c then places it with mode 0xa. */
void place_unit_in_formation_7ee8(int16_t unit_id, uint8_t table)
{
    double pos[3]; /* x and z here; 0x6d38c fills in the height */
    double heading, scale, c, s, along;
    ef_addr32 unit = guest_fn_7489c(unit_id);
    ef_addr32 entry, cells, offset;
    int32_t depth;
    uint8_t slot = 0xff;
    uint8_t i;

    if (unit == 0)
        return;
    entry = EF_U32(0x001972e0u) + (uint32_t)table * 34u;
    for (i = 0; i < EF_U8(entry + 0x13u); i++)
        if (EF_U16(EF_U32(entry + 0x16u) + (uint32_t)i * 0x38u + 0x20u) == EF_U16(unit + 0x28u))
            slot = i;
    if (EF_U8(0x00127da8u) != 0)
        slot = 0;
    if (slot == 0xff)
        return;

    heading = sub_entry_value_81ec(table, slot);
    entry = EF_U32(0x001972e0u) + (uint32_t)table * 34u;
    pos[0] = guest_fn_b4230(EF_S32(entry + 0xau) >> 16);
    pos[1] = guest_fn_b4230(EF_S32(entry + 6u) >> 16);
    cells = EF_U32(entry + 0x16u) + (uint32_t)slot * 0x38u;
    scale = *(double *)ef_guest_memory_ptr(0x001803f8u);
    pos[0] = (double)EF_S32(cells + 0xcu) * scale + pos[0];
    pos[1] = (double)EF_S32(cells + 8u) * scale + pos[1];

    depth = -EF_S32(cells + 0x10u) / 2 * 3 / 4;
    offset = 0x00197270u + (uint32_t)EF_U8(unit + 0x2au) * 16u;
    along = (double)depth + *(double *)ef_guest_memory_ptr(offset);
    c = guest_fn_b45a2(heading);
    s = guest_fn_b45ac(heading);
    pos[0] = along * c + -s * *(double *)ef_guest_memory_ptr(offset + 8u) + pos[0];
    pos[1] = along * s + c * *(double *)ef_guest_memory_ptr(offset + 8u) + pos[1];
    guest_fn_6d38c(unit_id, (ef_addr32)(uintptr_t)pos, heading, 0.0, 0.0, 0.0, -1.0, 0xa);
}

/* 0x825c, 92 bytes. Find the first free slot of table al (0xfe if all are used, 0xff if none usable). */
uint8_t find_free_slot_825c(uint8_t table)
{
    ef_addr32 entry = EF_U32(0x001972e0u) + (uint32_t)table * 34u;
    uint32_t used = 0;
    uint32_t count = EF_U8(entry + 0x13u);
    uint32_t i;
    uint32_t offset = 0;

    for (i = 0; (int32_t)i < (int32_t)count; i++, offset += 0x38u) {
        ef_addr32 slot = EF_U32(entry + 0x16u) + offset;

        if (EF_U8(slot + 0x27u) != 0) {
            used++;
            continue;
        }
        if (EF_S16(slot + 0x20u) == 0xff)
            return (uint8_t)i;
    }
    if (used == count)
        return 0xfe;
    return 0xff;
}

/* Table slot (0x38 bytes, table entries 0x22 bytes at [0x1972e0]) fields. */
#define TSLOT_UNIT    0x20u /* int16 unit id, 0xff = free */
#define TSLOT_MEMBER  0x22u /* byte, member index 0..6 */
#define TSLOT_MODE    0x25u
#define TSLOT_PHASE   0x26u
#define UNIT_KIND     0x1au
#define UNIT_ACTIVE   0x20u
#define UNIT_MEMBERS  0x50u /* seven int16 object ids */
#define LAST_ORDERED_OBJECT 0x00121e84u

static ef_addr32 table_slot(uint8_t table, uint8_t index)
{
    return EF_U32(EF_U32(0x001972e0u) + (uint32_t)table * 34u + 0x16u) + (uint32_t)index * 56u;
}

/* 0x8cc4, 648 bytes. Advance slot INDEX of table TABLE: mode-1 slots use
   0x8654. Otherwise the unit's current member gets the phase-1 order
   (event 0x4d with its heading when idle) or the phase-2 order (event
   0x4f, once per object); members advance cyclically and the slot frees
   itself when its unit has gone inactive or lost its grid link. */
void step_table_slot_8cc4(uint8_t table, uint8_t index)
{
    ef_addr32 slot = table_slot(table, index);
    int32_t unit_id = EF_S16(slot + TSLOT_UNIT);
    ef_addr32 unit;
    int32_t member;
    ef_addr32 object;

    if (unit_id == 0xff)
        return;
    if (EF_U8(slot + TSLOT_MODE) == 1) {
        step_activity_slot_8654(table, index);
        return;
    }
    unit = EF_U32(0x0019bb58u) + (uint32_t)(unit_id * 0xca);
    if (EF_U8(unit + UNIT_ACTIVE) == 0) {
        if (EF_U8(unit + UNIT_KIND) != 0xb && EF_U8(unit + UNIT_KIND) != 7)
            EF_U16(slot + TSLOT_UNIT) = 0xff;
        return;
    }
    member = EF_S16(unit + UNIT_MEMBERS + 2u * EF_U8(slot + TSLOT_MEMBER));
    object = guest_fn_7489c((int16_t)member);
    slot = table_slot(table, index);

    switch (EF_U8(slot + TSLOT_PHASE)) {
    case 1:
        if (object != 0 && EF_U8(object + 0x16cu) == 0xc) {
            int16_t heading = (int16_t)sub_entry_heading_8424(table, index);

            dispatch_unit_event_46254(0x899, member, 0x4d, (uint32_t)(int32_t)heading, index, 0);
        } else if (object == 0 || EF_U8(object + 0x20au) != 0) {
            EF_U8(slot + TSLOT_MEMBER)++;
        }
        slot = table_slot(table, index);
        if (grid_has_object_mode(EF_S16(slot + TSLOT_UNIT), 0xc) == 0) {
            slot = table_slot(table, index);
            EF_U8(slot + TSLOT_PHASE)++;
            EF_U8(slot + TSLOT_MEMBER) = 0;
        }
        break;
    case 2:
        if (grid_has_object_mode(EF_S16(slot + TSLOT_UNIT), 0xb) != 0)
            break;
        if (object != 0 && EF_U8(object + 0x16cu) == 0xa) {
            if (EF_U16(LAST_ORDERED_OBJECT) != EF_U16(object + 0x16u)) {
                dispatch_unit_event_46254(0x899, EF_S16(object + 0x16u), 0x4f, 0, 0, 0);
                EF_U16(LAST_ORDERED_OBJECT) = EF_U16(object + 0x16u);
            }
        } else {
            EF_U8(table_slot(table, index) + TSLOT_MEMBER)++;
        }
        break;
    }

    slot = table_slot(table, index);
    if (EF_U8(slot + TSLOT_MEMBER) > 6)
        EF_U8(slot + TSLOT_MEMBER) = 0;
    slot = table_slot(table, index);
    if (grid_has_active_link(EF_S16(slot + TSLOT_UNIT)) == 0)
        EF_U16(table_slot(table, index) + TSLOT_UNIT) = 0xff;
}

/* 0x8f4c, 77 bytes. */
void update_table_slots_8f4c(uint8_t table)
{
    ef_addr32 entry;
    uint32_t i;

    if (EF_U8(0x00127da9u) != 0 && EF_U8(0x001a666bu) != 0)
        return;
    entry = EF_U32(0x001972e0u) + (uint32_t)table * 34u;
    for (i = 0; (int32_t)i < (int32_t)EF_U8(entry + 0x13u); i++)
        step_table_slot_8cc4(table, (uint8_t)i);
}

/* 0x8f9c, 69 bytes. Run 46254 for each of the seven word slots of a unit. */
void run_unit_slots_8f9c(int16_t unit_id)
{
    uint32_t offset;

    for (offset = 0; offset != 0xe; offset += 2u) {
        ef_addr32 unit = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)unit_id * 0xca);

        dispatch_unit_event_46254(0x899, EF_S16(unit + offset + 0x50u), 0x4f, 0, 0, 0);
    }
}

/* 0xab2c, 72 bytes. Random value in [a - b, a + b], clamped to 10..0x186. */
uint32_t random_clamped_ab2c(int16_t center, int16_t spread)
{
    int32_t modulus = (int32_t)spread * 2 + 1;
    int32_t random = (int32_t)(half_word_9c960() & 0xffffu);
    int32_t value = random % modulus + (int32_t)center - (int32_t)spread;

    if ((int16_t)value > 0x18f)
        return 0x186;
    if ((int16_t)value < 0)
        return 0xa;
    return (uint32_t)value;
}

/* 0x11770, 100 bytes. Is there an entry with these three words and kind 1? */
uint32_t find_entry_11770(uint32_t a, uint32_t b, uint32_t c)
{
    int16_t i;

    if (EF_U8(0x00132ac2u) != 0)
        return 1;
    for (i = 0; (int32_t)i < EF_S32(0x001379b0u); i++) {
        ef_addr32 entry = EF_U32(0x001379bcu) + (uint32_t)((int32_t)i * 0x36);

        if (EF_U16(entry + 0x16u) == (uint16_t)a && EF_U16(entry + 0x18u) == (uint16_t)b &&
            EF_U16(entry + 0x1au) == (uint16_t)c && EF_S16(entry + 0xcu) == 1)
            return 1;
    }
    return 0;
}

/* 0x17538, 95 bytes. Take one charge from the first of the ten word slots that has one. */
uint32_t take_charge_17538(int16_t index)
{
    ef_addr32 slot = EF_U32(0x001a14f8u) + (uint32_t)((int32_t)index * 69) + 0x28u;
    uint32_t i;

    for (i = 0; i < 10; i++, slot += 2u) {
        uint16_t id = EF_U16(slot);
        ef_addr32 object;
        ef_addr32 state;
        uint16_t charges;

        if (id == 0)
            continue;
        object = guest_lookup_object_8ffe8((int16_t)id);
        state = EF_U32(object + 0x48u);
        charges = EF_U16(state + 0x1cu);
        if (charges != 0) {
            EF_U16(state + 0x1cu) = (uint16_t)(charges - 1u);
            return 1;
        }
    }
    return 0;
}

/* 0x1e1c0, 68 bytes. Is the point inside the 16-bit box at box? */
uint32_t point_in_box_1e1c0(ef_addr32 box, int32_t x, int32_t y, int32_t z)
{
    if (x < EF_S16(box) || x > EF_S16(box + 6u))
        return 0;
    if (y < EF_S16(box + 2u) || y > EF_S16(box + 8u))
        return 0;
    if (z < EF_S16(box + 4u) || z > EF_S16(box + 0xau))
        return 0;
    return 1;
}

/* 0x1e684, 107 bytes. Age the twelve timers; those in state 0xe expire back to state 2. */
void age_timers_1e684(void)
{
    uint16_t delta = EF_U16(0x00127d92u);
    int16_t i;

    for (i = 0; i < 12; i++) {
        uint32_t offset = (uint32_t)((int32_t)i * 2);

        if (EF_U8(0x00123764u + (uint32_t)i) == 0xe) {
            EF_U16(0x0019b0f8u + offset) = (uint16_t)(EF_U16(0x0019b0f8u + offset) + delta);
            if (EF_S16(0x0019b0f8u + offset) > 0x438) {
                EF_U16(0x0019b0f8u + offset) = 0;
                EF_U8(0x00123764u + (uint32_t)i) = 2;
            }
        } else {
            EF_U16(0x0019b0f8u + offset) = 0;
        }
    }
    EF_U16(0x00127d92u) = delta;
}

/* 0x1f8c4, 75 bytes. Pop the head of the queue at 0x19b0d0. */
uint32_t queue_pop_1f8c4(void)
{
    int16_t count = EF_S16(0x0019b114u);
    uint32_t head = 0;
    int16_t i;

    if (count == 0) {
        EF_U16(0x0019b114u) = 0;
        return 0;
    }
    head = EF_U32(0x0019b0d0u);
    EF_U32(0x001237f4u) = head;
    for (i = 0; i < count; i++)
        EF_U32(0x0019b0d0u + (uint32_t)((int32_t)i * 4)) = EF_U32(0x0019b0d4u + (uint32_t)((int32_t)i * 4));
    EF_U16(0x0019b114u) = (uint16_t)(count - 1);
    return head;
}

/* 0x237c8, 48 bytes. Translate a vector by the origin at 0x21444c.. and pass it to b39ec. */
void translate_and_call_237c8(ef_addr32 vector)
{
    guest_fn_b39ec(EF_U32(vector) - EF_U32(0x0021444cu), EF_U32(vector + 4u) - EF_U32(0x00214448u),
                   EF_U32(vector + 8u) - EF_U32(0x00214450u));
}

/* 0x266f8, 74 bytes. */
void clear_part_flags_266f8(ef_addr32 object)
{
    uint8_t i;

    guest_fn_26744(object);
    guest_fn_26960(object);
    for (i = 0; i < EF_U8(0x00123819u + (uint32_t)EF_U8(object + 0x3au) * 0xa4u); i++) {
        EF_U8(EF_U32(object + 0x48u) + (uint32_t)i * 0x44u + 0x16u) &= 0xfbu;
        guest_fn_26b60(EF_S16(object + 0x16u));
    }
}

/* 0x2888c, 60 bytes. Largest of three magnitudes (via cf94a). */
uint32_t max_magnitude_2888c(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t pick = (int32_t)guest_fn_cf94a(a) > (int32_t)guest_fn_cf94a(b) ? a : b;
    uint32_t big = guest_fn_cf94a(pick);
    uint32_t last = guest_fn_cf94a(c);

    if ((int32_t)big > (int32_t)last)
        return big;
    return last;
}

/* 0x2b508, 79 bytes. */
uint8_t try_assign_2b508(int16_t unit_id)
{
    ef_addr32 unit = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)unit_id * 0xca);
    int16_t result = (int16_t)guest_fn_2c454(unit + 0x61u, EF_S16(unit + 0x15u), 5, 1);

    if (result == 0xff)
        return 0;
    guest_fn_2b558(unit_id);
    return 1;
}

/* 0x2bb64, 108 bytes. Find the unit linked to unit_id whose kind is 0xb, or 0xff. */
uint32_t find_linked_unit_2bb64(int16_t unit_id)
{
    ef_addr32 units = EF_U32(0x0019bb58u);
    ef_addr32 table = EF_U32(0x001fcb40u);
    int16_t i;

    for (i = 0; i < 8; i++) {
        int16_t other = (int16_t)EF_U16(table +
            (uint32_t)((int32_t)EF_S16(units + (uint32_t)((int32_t)unit_id * 0xca) + 0x1cu) * 0x11) +
            (uint32_t)((int32_t)i * 2) + 1u);

        if (other == 0xff || other == unit_id)
            continue;
        if (EF_U8(units + (uint32_t)((int32_t)other * 0xca) + 0x1bu) == 0xb)
            return (uint16_t)other;
    }
    return 0xff;
}

/* 0x33e78, 109 bytes. */
void update_buttons_33e78(void)
{
    ef_addr32 button = guest_fn_a46cc(0x00182d6cu);
    uint8_t flag = 1;

    guest_fn_33630();
    if (EF_U8(0x0013578eu) == 4u)
        flag = 0;
    guest_fn_a73cc(button, 0x00182d78u, flag);
    guest_fn_a73cc(button, 0x00182d80u, (uint32_t)(flag ^ 1u));
    guest_fn_a4cb4(EF_U8(button + 0x10u));
    apply_a4cb4_a4d90(0x00182d88u);
}

/* Shared body of the four button-pair helpers below. */
static void set_button_pair(ef_addr32 key, uint32_t mode, uint32_t flag)
{
    ef_addr32 button = guest_fn_a46cc(key);

    if (button == 0)
        return;
    guest_fn_a520c(button, EF_S16(button + 0x45u), mode, 0);
    guest_fn_a73cc(button, 0x0018310cu, flag);
    guest_fn_a73cc(button, 0x00183104u, flag);
}

/* 0x35fbc, 71 bytes. */
void set_buttons_35fbc(void)
{
    set_button_pair(0x001830f8u, 0x86, 0);
}

/* 0x36004, 77 bytes. */
void set_buttons_36004(void)
{
    set_button_pair(0x001830f8u, 0x72, 1);
}

/* 0x36964, 71 bytes. */
void set_buttons_36964(void)
{
    set_button_pair(0x00182fb0u, 0x86, 0);
}

/* 0x369ac, 77 bytes. */
void set_buttons_369ac(void)
{
    set_button_pair(0x00182fb0u, 0x72, 1);
}

/* 0x38674, 97 bytes. Number of live, non-flagged objects in the ten word slots of entry index. */
int32_t count_live_38674(int16_t index)
{
    ef_addr32 slot;
    uint32_t count = 0;
    uint32_t i;

    if (index >= (int16_t)EF_U16(0x001a1506u))
        return 0;
    slot = EF_U32(0x001a14f8u) + (uint32_t)((int32_t)index * 69) + 0x28u;
    for (i = 0; i < 10; i++, slot += 2u) {
        uint16_t id = EF_U16(slot);
        ef_addr32 object;

        if (id == 0)
            continue;
        object = guest_lookup_object_8ffe8((int16_t)id);
        if (object != 0 && (EF_U8(object + 0x30u) & 2u) == 0)
            count++;
    }
    return (int16_t)count;
}

/* 0x386f8, 80 bytes. Position of the entry's node in its list. */
uint32_t node_position_386f8(int16_t index)
{
    ef_addr32 entry = EF_U32(0x001a14f8u) + (uint32_t)((int32_t)index * 69);
    ef_addr32 node = EF_U32(0x0019f5b0u + (uint32_t)((int32_t)EF_S16(entry + 0x16u) * 4));
    uint32_t position = 0;

    while (node != EF_U32(entry + 0x18u)) {
        position++;
        node = EF_U32(node + 0x15u);
    }
    return position;
}

/* 0x23760, 48 bytes. Approximate 3D distance (b39ec) from the position record to the reference at 0x1a5bec/0x1a5bf0/0x1a5be8. */
int32_t position_delta_metric(ef_addr32 position)
{
    return guest_fn_b39ec_q((int32_t)(EF_U32(position) - EF_U32(0x001a5becu)),
                            (int32_t)(EF_U32(position + 4u) - EF_U32(0x001a5bf0u)),
                            (int32_t)(EF_U32(position + 8u) - EF_U32(0x001a5be8u)));
}
