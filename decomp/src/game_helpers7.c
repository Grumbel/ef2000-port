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

/* 0x8f4c, 77 bytes. */
void update_table_slots_8f4c(uint8_t table)
{
    ef_addr32 entry;
    uint32_t i;

    if (EF_U8(0x00127da9u) != 0 && EF_U8(0x001a666bu) != 0)
        return;
    entry = EF_U32(0x001972e0u) + (uint32_t)table * 34u;
    for (i = 0; (int32_t)i < (int32_t)EF_U8(entry + 0x13u); i++)
        guest_fn_8cc4(table, i);
}

/* 0x8f9c, 69 bytes. Run 46254 for each of the seven word slots of a unit. */
void run_unit_slots_8f9c(int16_t unit_id)
{
    uint32_t offset;

    for (offset = 0; offset != 0xe; offset += 2u) {
        ef_addr32 unit = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)unit_id * 0xca);

        guest_fn_46254(0x899, EF_S16(unit + offset + 0x50u), 0x4f, 0, 0, 0);
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
