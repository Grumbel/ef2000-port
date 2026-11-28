/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Instantiate a site record with its unit count lists and timetable. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define SITE_SIZE         0x53u
#define TAG_BASE          0x0018eb8cu /* "unable to alloc base" */
#define TAG_MOB           0x0018eba4u /* "unable to alloc mob list" */
#define TAG_ONM           0x0018ebc0u /* "unable to alloc onm list" */
#define TAG_GBB           0x0018ebdcu /* "unable to alloc gbb list" */
#define TAG_GBUSED        0x0018ebf8u /* "unable to alloc gbused list" */
#define TAG_TMS           0x0018ec14u /* "unable to alloc tms list" */
#define START_HOUR        0x0013799eu /* byte: start day; int16 at +2: start hour */

/* Site record fields. */
#define SITE_SIDE         0x0bu /* byte */
#define SITE_TYPE         0x0cu /* byte */
#define SITE_POSITION     0x11u /* 18 bytes from the template */
#define SITE_MOB_SIZE     0x30u /* int16 */
#define SITE_MOB_COUNT    0x32u /* byte, at most 0x31 */
#define SITE_MOB          0x33u /* per-type counts of units of kind 0/1 */
#define SITE_ONM          0x37u
#define SITE_GBB_SIZE     0x3bu /* int16 */
#define SITE_GBB_COUNT    0x3du /* byte */
#define SITE_GBB          0x3eu /* per-type counts of units of kind 2 */
#define SITE_GBUSED       0x42u
#define SITE_READY        0x46u /* int16 */
#define SITE_SLOTS        0x48u /* int16: timetable entries */
#define SITE_TIMETABLE    0x4au /* 9-byte entries */
#define SITE_ACTIVE       0x50u /* int16 */

static ef_addr32 alloc_cleared(int16_t size, ef_addr32 tag)
{
    return guest_fn_cc6c0_x((uint32_t)(int32_t)size, tag);
}

/* Count the units of the list SITE can use: matching type and position,
 * idle and on the site's side. Kinds 0/1 count in the mob list, kind 2 in
 * the gbb list, by the unit's type byte. */
static void count_site_units(ef_addr32 site, ef_addr32 unit, ef_addr32 groups)
{
    for (; unit != 0u && EF_U8(site + SITE_MOB_COUNT) < 0x31u; unit = EF_U32(unit + 4u)) {
        uint32_t kind;

        if ((uint16_t)code_pair_matches_bcec0(EF_U8(unit + 0xbu), EF_U8(site + SITE_TYPE)) == 0u ||
            (uint16_t)records_share_position(site, unit) == 0u || (uint16_t)record_flag_clear_d(unit) == 0u ||
            EF_U16(unit + 9u) != 0u ||
            (uint16_t)group_flag_positive_cc22c(EF_U8(unit + 0xeu), EF_U8(site + SITE_SIDE), groups) == 0u)
            continue;
        kind = EF_U8(unit + 0xbu);
        if (kind <= 1u) {
            EF_U8(EF_U32(site + SITE_MOB) + EF_U8(unit + 0xcu))++;
            EF_U8(site + SITE_MOB_COUNT)++;
        } else if (kind == 2u) {
            EF_U8(EF_U32(site + SITE_GBB) + EF_U8(unit + 0xcu))++;
            EF_U8(site + SITE_GBB_COUNT)++;
        }
    }
}

/* Fill the timetable: SLOTS entries from the start hour, an hour per
 * SLOTS / 8 entries and 480 / SLOTS minutes apart (the minute wraps to 0 at
 * 60), so they span eight hours. */
static void fill_timetable(ef_addr32 site)
{
    int16_t slots = EF_S16(site + SITE_SLOTS);
    int16_t minute = 0;
    int16_t i;

    for (i = 0; i < slots; i++) {
        ef_addr32 entry = EF_U32(site + SITE_TIMETABLE) + (uint32_t)((int32_t)i * 9);
        int32_t hour;

        EF_U8(entry) = EF_U8(START_HOUR);
        hour = (int32_t)i / (slots / 8) + EF_S16(START_HOUR + 2u);
        if ((int16_t)hour >= 24)
            hour -= 24;
        EF_U8(entry + 1u) = (uint8_t)hour;
        EF_U8(entry + 2u) = (uint8_t)minute;
        minute = (int16_t)(minute + 0x1e0 / slots);
        if (minute >= 60)
            minute = 0;
        EF_U8(entry + 3u) = 0;
        EF_U8(entry + 4u) = 0;
        EF_U16(entry + 5u) = 0;
        EF_U16(entry + 7u) = 0;
    }
}

/* 0xbcf44, 943 bytes. Allocate a site record from TEMPLATE: copy its side,
 * type, position and fields, allocate cleared mob/onm and gbb/gbused lists
 * sized by the type, count the usable units of UNITS (a list head pointer,
 * GROUPS for the side test) and the template's ready entries. Types 0x31
 * and 0x49 with ready entries get a timetable (0x31 limited to two). */
ef_addr32 create_site_record_bcf44(ef_addr32 template_record, ef_addr32 units, ef_addr32 groups)
{
    ef_addr32 site = guest_fn_cc6c0_x(SITE_SIZE, TAG_BASE);
    uint32_t k;

    EF_U8(site + SITE_SIDE) = EF_U8(template_record + 9u);
    EF_U8(site + SITE_TYPE) = EF_U8(template_record + 0xbu);
    EF_U32(site + 0xdu) = EF_U32(template_record + 0xcu);
    EF_U16(site + 0x23u) = EF_U16(template_record + 0x32u);
    EF_U16(site + 0x25u) = (uint16_t)(EF_S16(template_record + 0x32u) / 2);
    EF_U16(site + SITE_ACTIVE) = EF_U16(template_record + 0x36u);
    for (k = 0; k < 18u; k++)
        EF_U8(site + SITE_POSITION + k) = EF_U8(template_record + 0x16u + k);
    EF_U32(site + 0x27u) = 0;
    EF_U32(site + 0x2bu) = 0;
    EF_U8(site + 0x2fu) = EF_U8(template_record + 0x31u);
    EF_U16(site + 0x4eu) = 2;
    EF_U16(site + SITE_MOB_SIZE) = map_code_bcd78(EF_U8(site + SITE_TYPE));
    EF_U16(site + SITE_GBB_SIZE) = return_esc_cc8e0();

    EF_U32(site + SITE_MOB) = alloc_cleared(EF_S16(site + SITE_MOB_SIZE), TAG_MOB);
    EF_U32(site + SITE_ONM) = alloc_cleared(EF_S16(site + SITE_MOB_SIZE), TAG_ONM);
    fill_bytes_b3fb0(EF_U32(site + SITE_MOB), 0, (uint32_t)(int32_t)EF_S16(site + SITE_MOB_SIZE));
    fill_bytes_b3fb0(EF_U32(site + SITE_ONM), 0, (uint32_t)(int32_t)EF_S16(site + SITE_MOB_SIZE));
    EF_U8(site + SITE_MOB_COUNT) = 0;
    EF_U32(site + SITE_GBB) = alloc_cleared(EF_S16(site + SITE_GBB_SIZE), TAG_GBB);
    EF_U32(site + SITE_GBUSED) = alloc_cleared(EF_S16(site + SITE_GBB_SIZE), TAG_GBUSED);
    fill_bytes_b3fb0(EF_U32(site + SITE_GBB), 0, (uint32_t)(int32_t)EF_S16(site + SITE_GBB_SIZE));
    fill_bytes_b3fb0(EF_U32(site + SITE_GBUSED), 0, (uint32_t)(int32_t)EF_S16(site + SITE_GBB_SIZE));
    EF_U8(site + SITE_GBB_COUNT) = 0;
    count_site_units(site, EF_U32(units), groups);

    EF_U16(site + SITE_SLOTS) = 0;
    EF_U32(site + SITE_TIMETABLE) = 0;
    EF_U16(site + SITE_READY) = count_ready_entries_bcda0(template_record);
    if (EF_U8(site + SITE_TYPE) == 0x31u || EF_U8(site + SITE_TYPE) == 0x49u) {
        if (EF_S16(site + SITE_READY) > 0) {
            int16_t per_slot;

            EF_U16(site + SITE_ACTIVE) = 1;
            if (EF_S16(site + SITE_READY) > 2 && EF_U8(site + SITE_TYPE) == 0x31u)
                EF_U16(site + SITE_READY) = 2;
            per_slot = (int16_t)((int32_t)map_code_bce98(EF_U8(site + SITE_TYPE)) / EF_S16(site + SITE_READY));
            EF_U16(site + SITE_SLOTS) = (uint16_t)((60 / per_slot) << 3);
            EF_U32(site + SITE_TIMETABLE) = alloc_cleared((int16_t)(EF_S16(site + SITE_SLOTS) * 9), TAG_TMS);
            fill_timetable(site);
        } else {
            EF_U16(site + SITE_ACTIVE) = 0;
            EF_U16(site + SITE_SLOTS) = 0;
        }
    }
    EF_U8(site + 0x52u) = 0;
    return site;
}

#define VIRTUAL_SITES     0x00135424u /* nine saved 83-byte site records */
#define VIRTUAL_COUNT     9
#define TAG_VIRTUAL       0x001933b4u /* "Unable to alloc base in AddVirtPort" */
#define TAG_V_MOB         0x001933d8u
#define TAG_V_ONM         0x001933f4u
#define TAG_V_GBB         0x00193410u
#define TAG_V_GBUSED      0x0019342cu
#define FMT_UNKNOWN_CLASS 0x00193448u /* "CG Unknown mobile class [%hd] inbuildbasefromTA" */
#define TAG_V_TMS         0x00193478u /* "Unable to alloc [%hd] timeslots" */

/* Count the units of the list SITE can use (matching type and position,
 * idle); kinds other than 0..2 are reported through the error hook. */
static void count_virtual_site_units(ef_addr32 site, ef_addr32 unit)
{
    for (; unit != 0u; unit = EF_U32(unit + 4u)) {
        uint32_t kind;

        if ((uint16_t)code_pair_matches_bcec0(EF_U8(unit + 0xbu), EF_U8(site + SITE_TYPE)) == 0u ||
            (uint16_t)records_share_position(site, unit) == 0u || (uint16_t)record_flag_clear_d(unit) == 0u ||
            EF_U16(unit + 9u) != 0u)
            continue;
        kind = EF_U8(unit + 0xbu);
        if (kind <= 1u) {
            EF_U8(EF_U32(site + SITE_MOB) + EF_U8(unit + 0xcu))++;
            EF_U8(site + SITE_MOB_COUNT)++;
        } else if (kind == 2u) {
            EF_U8(EF_U32(site + SITE_GBB) + EF_U8(unit + 0xcu))++;
            EF_U8(site + SITE_GBB_COUNT)++;
        } else {
            guest_callptr2_x(g_gui_error_hook, FMT_UNKNOWN_CLASS, kind);
        }
    }
}

/* 0xf90ac, 921 bytes. Build the nine virtual port sites numbered FIRST..
 * FIRST+8 from VIRTUAL_SITES: each is allocated and cleared, takes the
 * saved fields, gets fresh mob/onm and gbb/gbused lists of the saved sizes
 * counted from UNITS and a timetable of the saved slot count, is marked virtual (+0x52) and inserted at the head of
 * HEAD/TAIL. Returns FIRST + 9. */
int16_t add_virtual_ports_f90ac(ef_addr32 head, ef_addr32 tail, ef_addr32 units, int16_t first)
{
    int16_t number;

    for (number = first; (int32_t)number < (int32_t)first + VIRTUAL_COUNT; number++) {
        ef_addr32 saved = VIRTUAL_SITES + (uint32_t)(((int32_t)number - first) * (int32_t)SITE_SIZE);
        ef_addr32 site = guest_fn_cc6c0_x(SITE_SIZE, TAG_VIRTUAL);
        uint32_t k;

        fill_bytes_b3fb0(site, 0, SITE_SIZE);
        EF_U16(site + 9u) = (uint16_t)number;
        EF_U8(site + SITE_SIDE) = EF_U8(saved + SITE_SIDE);
        EF_U8(site + SITE_TYPE) = EF_U8(saved + SITE_TYPE);
        EF_U32(site + 0xdu) = EF_U32(saved + 0xdu);
        for (k = 0; k < 18u; k++)
            EF_U8(site + SITE_POSITION + k) = EF_U8(saved + SITE_POSITION + k);
        EF_U16(site + 0x23u) = EF_U16(saved + 0x23u);
        EF_U32(site + 0x27u) = EF_U32(saved + 0x27u);
        EF_U32(site + 0x2bu) = EF_U32(saved + 0x2bu);
        EF_U8(site + 0x2fu) = EF_U8(saved + 0x2fu);
        EF_U16(site + SITE_MOB_SIZE) = EF_U16(saved + SITE_MOB_SIZE);
        EF_U32(site + SITE_MOB) = alloc_cleared(EF_S16(site + SITE_MOB_SIZE), TAG_V_MOB);
        EF_U32(site + SITE_ONM) = alloc_cleared(EF_S16(site + SITE_MOB_SIZE), TAG_V_ONM);
        fill_bytes_b3fb0(EF_U32(site + SITE_MOB), 0, (uint32_t)(int32_t)EF_S16(site + SITE_MOB_SIZE));
        fill_bytes_b3fb0(EF_U32(site + SITE_ONM), 0, (uint32_t)(int32_t)EF_S16(site + SITE_MOB_SIZE));
        EF_U8(site + SITE_MOB_COUNT) = 0;
        EF_U16(site + SITE_GBB_SIZE) = EF_U16(saved + SITE_GBB_SIZE);
        EF_U32(site + SITE_GBB) = alloc_cleared(EF_S16(site + SITE_GBB_SIZE), TAG_V_GBB);
        EF_U32(site + SITE_GBUSED) = alloc_cleared(EF_S16(site + SITE_GBB_SIZE), TAG_V_GBUSED);
        fill_bytes_b3fb0(EF_U32(site + SITE_GBB), 0, (uint32_t)(int32_t)EF_S16(site + SITE_GBB_SIZE));
        fill_bytes_b3fb0(EF_U32(site + SITE_GBUSED), 0, (uint32_t)(int32_t)EF_S16(site + SITE_GBB_SIZE));
        EF_U8(site + SITE_GBB_COUNT) = 0;
        count_virtual_site_units(site, EF_U32(units));

        saved = VIRTUAL_SITES + (uint32_t)(((int32_t)number - first) * (int32_t)SITE_SIZE);
        EF_U16(site + SITE_READY) = EF_U16(saved + SITE_READY);
        EF_U16(site + SITE_SLOTS) = EF_U16(saved + SITE_SLOTS);
        EF_U32(site + SITE_TIMETABLE) = guest_fn_cc6c0_z((uint32_t)((int32_t)EF_S16(site + SITE_SLOTS) * 9),
                                                         TAG_V_TMS, (uint32_t)(int32_t)EF_S16(site + SITE_SLOTS));
        fill_timetable(site);

        saved = VIRTUAL_SITES + (uint32_t)(((int32_t)number - first) * (int32_t)SITE_SIZE);
        EF_U16(site + 0x4eu) = EF_U16(saved + 0x4eu);
        EF_U16(site + SITE_ACTIVE) = EF_U16(saved + SITE_ACTIVE);
        EF_U8(site + 0x52u) = 1;
        list_insert_head(site, head, tail);
    }
    return (int16_t)(first + VIRTUAL_COUNT);
}
