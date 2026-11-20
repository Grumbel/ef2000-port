/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Mission record, 0x1b bytes. */
#define MISSION_SIZE        0x1bu
#define MISSION_ACTIVE      0x09u  /* byte */
#define MISSION_KIND        0x0au  /* byte, template byte 1 */
#define MISSION_SIDE        0x0bu  /* byte, template byte 0 */
#define MISSION_FIELD_0C    0x0cu  /* byte */
#define MISSION_OWNER_WORD  0x0eu  /* word from owner + 9 */
#define MISSION_ID          0x10u  /* word */
#define MISSION_GROUPS      0x12u  /* byte */
#define MISSION_GROUP_ARRAY 0x13u  /* dword */

/* Mission template: byte 4 is the entry count, dword 5 the 10-byte entries. */
#define TEMPLATE_ENTRIES    0x04u
#define TEMPLATE_ENTRY_PTR  0x05u
#define TEMPLATE_ENTRY_SIZE 10u

/* Group record, 0x7c bytes. */
#define GROUP_SIZE          0x7cu
#define GROUP_SHIPS         0x01u  /* byte */
#define GROUP_MODE          0x02u  /* byte */
#define GROUP_FIELD_69      0x69u  /* dword */
#define GROUP_SHIP_IDS      0x78u  /* dword: uint16_t[ships] */

/* Ship node. */
#define SHIP_NEXT           0x04u
#define SHIP_ASSIGNED       0x09u  /* word */
#define SHIP_SIDE           0x0bu  /* byte */
#define SHIP_ID             0x13u  /* word */
#define SHIP_REFCOUNT       0x17u  /* word */
#define SHIP_MISSION        0x19u  /* word */
#define SHIP_GROUP          0x21u  /* word */

#define FORMATION_SLOTS     0x12u  /* byte */
#define GROUP_MODE_FORMATION 2u

/* Give group INDEX up to its ship count of free side-1 ships registered
   for this mission and group; returns how many were attached. */
static int16_t attach_ships(ef_addr32 mission, ef_addr32 group, int16_t index, ef_addr32 ship)
{
    int16_t attached = 0;

    for (; ship != 0; ship = EF_U32(ship + SHIP_NEXT)) {
        if (attached >= (int16_t)EF_U8(group + GROUP_SHIPS))
            break;
        if (EF_U8(ship + SHIP_SIDE) == 1u &&
            EF_U16(ship + SHIP_MISSION) == EF_U16(mission + MISSION_ID) &&
            EF_U16(ship + SHIP_GROUP) == (uint16_t)index) {
            EF_U16(ship + SHIP_REFCOUNT)++;
            EF_U16(EF_U32(group + GROUP_SHIP_IDS) + 2u * (uint32_t)(int32_t)attached) = EF_U16(ship + SHIP_ID);
            attached++;
            EF_U16(ship + SHIP_ASSIGNED) = 1;
        }
    }
    return attached;
}

/* 0xf8e44, 615 bytes. Create a mission from TEMPLATE: allocate the record
   and one group per used template entry, fill each group, attach its
   ships from *SHIPS, step through the formation slots, then set up the
   mission (0xb47e4, 0xbe77c) and insert it into the HEAD/TAIL list.
   Returns ID + 1. */
uint32_t create_mission_f8e44(ef_addr32 template_, uint32_t arg_edx, ef_addr32 owner, uint32_t arg_ecx,
                              ef_addr32 formation, uint32_t id, ef_addr32 head, ef_addr32 tail,
                              uint32_t arg_s4, ef_addr32 ships, uint32_t weights, uint32_t arg_s7,
                              uint32_t arg_s8, uint32_t arg_s9)
{
    ef_addr32 mission;
    ef_addr32 groups;
    ef_addr32 entry;
    ef_addr32 group;
    uint32_t count;
    int16_t slot = 0;
    int16_t index = 0;
    int16_t e;

    mission = guest_fn_cc6c0_x(MISSION_SIZE, 0x00193334u); /* "unable to alloc mission" */
    EF_U8(mission + MISSION_ACTIVE) = 1;
    EF_U8(mission + MISSION_KIND) = EF_U8(template_ + 1u);
    EF_U8(mission + MISSION_FIELD_0C) = 0;
    EF_U8(mission + MISSION_SIDE) = EF_U8(template_);
    EF_U16(mission + MISSION_OWNER_WORD) = EF_U16(owner + 9u);
    EF_U16(mission + MISSION_ID) = (uint16_t)id;
    EF_U8(mission + MISSION_GROUPS) = (uint8_t)count_set_entries_f8e14(template_);

    count = EF_U8(mission + MISSION_GROUPS);
    groups = guest_fn_cc6c0_z(count * GROUP_SIZE, 0x0019334cu, count); /* "unable to alloc %hd groups" */
    EF_U32(mission + MISSION_GROUP_ARRAY) = groups;
    fill_bytes_b3fb0(groups, 0, count * GROUP_SIZE);

    for (e = 0; (int16_t)EF_U8(template_ + TEMPLATE_ENTRIES) > e; e++) {
        entry = EF_U32(template_ + TEMPLATE_ENTRY_PTR) + TEMPLATE_ENTRY_SIZE * (uint32_t)(int32_t)e;
        if (EF_U16(entry) == 0)
            continue;
        group = EF_U32(mission + MISSION_GROUP_ARRAY) + GROUP_SIZE * (uint32_t)(int32_t)index;
        fill_record_f8da8(group, entry, owner, formation, (uint32_t)(int32_t)slot);
        EF_U32(group + GROUP_FIELD_69) = 0;
        EF_U32(group + GROUP_SHIP_IDS) =
            guest_fn_cc6c0_z(2u * EF_U8(group + GROUP_SHIPS), 0x00193368u, EF_U8(group + GROUP_SHIPS));
        if (attach_ships(mission, group, index, EF_U32(ships)) < (int16_t)EF_U8(group + GROUP_SHIPS))
            guest_callptr1_x(EF_U32(0x00132a74u), 0x0019339cu); /* "NB ships [%hd] too low" */
        if (formation != 0 && EF_U8(group + GROUP_MODE) == GROUP_MODE_FORMATION) {
            int32_t next = (int32_t)slot + 1;
            slot = next < (int32_t)EF_U8(formation + FORMATION_SLOTS) ? (int16_t)next : 0;
        }
        EF_U32(group + GROUP_FIELD_69) = 0;
        index++;
    }

    guest_fn_b47e4(mission, 0, arg_s4, weights, arg_s7, (uint32_t)owner, arg_ecx, arg_edx,
                   (uint32_t)(int32_t)(int16_t)arg_s8, arg_s9, head);
    transfer_entry_weights_be77c(mission, weights);
    guest_fn_bc570_x(mission, head, tail);
    return id + 1u;
}

/* Request template: byte 5 is the entry count, dword 6 the 10-byte entries. */
#define REQUEST_ENTRIES     0x05u
#define REQUEST_ENTRY_PTR   0x06u

/* Group fields set from the escort source of a type-0xb group. */
#define GROUP_TYPE_ESCORT   0x0bu
#define GROUP_ESCORT_BYTE   0x6du
#define GROUP_ESCORT_WORD   0x6eu
#define GROUP_ESCORT_PHASE  0x70u
#define ESCORT_BYTE         0x0au
#define ESCORT_WORD         0x10u
#define ESCORT_PERIOD       0x12u

/* 0xbe59c, 477 bytes. Create an inactive mission for request TEMPLATE_
   when it has any used entries: one group per used entry built by
   0xbe480, escort groups copying ESCORT's fields with a random phase in
   its period, then set up through 0xb47e4, 0xbe7e4 and 0xbe77c and
   inserted into the HEAD/TAIL list. Returns ID + 1, or ID when the
   template is empty. */
uint16_t create_request_mission_be59c(ef_addr32 template_, uint32_t arg_edx, ef_addr32 escort, ef_addr32 owner,
                                      uint32_t id, uint32_t node_value, uint32_t node_limit, ef_addr32 head,
                                      ef_addr32 tail, uint32_t arg_s5, uint32_t node_head, ef_addr32 weights,
                                      uint32_t arg_s8, uint32_t arg_s9, uint32_t arg_s10)
{
    ef_addr32 mission;
    ef_addr32 groups;
    ef_addr32 entry;
    ef_addr32 group;
    uint32_t count;
    int16_t index = 0;
    int16_t e;

    if ((uint16_t)count_nonzero_entries(template_) == 0)
        return (uint16_t)id;

    mission = guest_fn_cc6c0_x(MISSION_SIZE, 0x0018ec88u); /* "unable to alloc mission" */
    EF_U8(mission + MISSION_ACTIVE) = 0;
    EF_U8(mission + MISSION_KIND) = EF_U8(template_ + 1u);
    EF_U8(mission + MISSION_SIDE) = EF_U8(template_);
    EF_U8(mission + MISSION_FIELD_0C) = 0;
    EF_U8(mission + 0x0du) = EF_U8(template_ + 4u);
    EF_U16(mission + MISSION_OWNER_WORD) = EF_U16(owner + 9u);
    EF_U16(mission + MISSION_ID) = (uint16_t)id;
    EF_U8(mission + MISSION_GROUPS) = (uint8_t)count_nonzero_entries(template_);

    count = EF_U8(mission + MISSION_GROUPS);
    groups = guest_fn_cc6c0_z(count * GROUP_SIZE, 0x0018eca0u, count); /* "unable to alloc %hd groups" */
    EF_U32(mission + MISSION_GROUP_ARRAY) = groups;
    fill_bytes_b3fb0(groups, 0, count * GROUP_SIZE);

    for (e = 0; (int16_t)EF_U8(template_ + REQUEST_ENTRIES) > e; e++) {
        entry = EF_U32(template_ + REQUEST_ENTRY_PTR) + TEMPLATE_ENTRY_SIZE * (uint32_t)(int32_t)e;
        if (EF_U16(entry) == 0)
            continue;
        group = EF_U32(mission + MISSION_GROUP_ARRAY) + GROUP_SIZE * (uint32_t)(int32_t)index;
        build_record_be480(group, entry, owner, arg_edx, head);
        group = EF_U32(mission + MISSION_GROUP_ARRAY) + GROUP_SIZE * (uint32_t)(int32_t)index;
        if (EF_U8(group + GROUP_MODE) == GROUP_TYPE_ESCORT && escort != 0) {
            EF_U8(group + GROUP_ESCORT_BYTE) = EF_U8(escort + ESCORT_BYTE);
            EF_U16(group + GROUP_ESCORT_WORD) = EF_U16(escort + ESCORT_WORD);
            EF_U16(group + GROUP_ESCORT_PHASE) =
                (uint16_t)((int32_t)(uint16_t)half_word_9c960() % (int32_t)EF_U8(escort + ESCORT_PERIOD));
        }
        index++;
    }

    guest_fn_b47e4(mission, (uint32_t)escort, arg_s5, weights, arg_s8, (uint32_t)owner, 0, arg_edx,
                   (uint32_t)(int32_t)(int16_t)arg_s9, arg_s10, head);
    collect_group_nodes_be7e4(mission, node_head, (uint32_t)(int32_t)(int16_t)node_value, (uint8_t)node_limit);
    transfer_entry_weights_be77c(mission, weights);
    list_insert_head(mission, head, tail);
    return (uint16_t)(id + 1u);
}
