/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Fill the open slots of every owner with group requests built from the
 * unclaimed nodes that share the owner's key. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Owner fields. */
#define OWNER_NEXT        0x04u
#define OWNER_KIND        0x09u /* selects the node type rotation */
#define OWNER_CLASS       0x0bu /* 'I' and 'O' owners do not reuse reserved slots */
#define OWNER_KEY         0x16u /* 12 bytes: two key dwords and a third copied along */
#define OWNER_SLOT_COUNT  0x2au /* int16 */
#define OWNER_SLOTS       0x2cu /* pointer to 21-byte slot entries */

/* Slot entry fields. */
#define SLOT_STATE        0x00u /* 0xff free, 0xfe reserved */
#define SLOT_WORDS        0x07u /* three words copied into the request key */

/* Node fields. */
#define NODE_NEXT         0x04u
#define NODE_CLAIMED      0x09u /* word */
#define NODE_CLASS        0x0bu
#define NODE_TYPE         0x0cu
#define NODE_MODE         0x0du /* 1 and 2 are never grouped */
#define NODE_KEY          0x3bu /* two dwords */

#define KEY_SIZE          0x12u /* request key: 3 dwords + 3 words */
#define TYPE_COUNT        27
#define GROUP_LIMIT       8

/* Node type for slot I (0..3) of an owner of kind 0, 2 or 5 (air types)
 * or of kind 1, 3 or 4. */
static uint8_t slot_type(uint8_t kind, uint16_t slot, uint8_t previous)
{
    switch (kind) {
    case 0:
    case 2:
    case 5:
        switch (slot) {
        case 0: return 0x11u;
        case 1: return 0x08u;
        case 2: return 0x13u;
        case 3: return 0x0eu;
        }
        break;
    default:
        switch (slot) {
        case 0: return 0x1au;
        case 1: return 0x16u;
        case 2:
        case 3: return 0x17u;
        }
        break;
    }
    return previous;
}

/* 0xcc90c, 1199 bytes. For every owner in the list at *OWNERS that has
 * open slots (count_open_entries_cc840), collect the key of each open slot
 * (owner position plus the slot's three words) into a temporary table and
 * count, per node type, the unclaimed groupable nodes of *NODES whose key
 * matches the owner's. The slots then take node types in a rotation that
 * depends on the owner's kind; for each slot a request is built
 * (build_group_request_ccdbc) for up to eight nodes of that type that are
 * not yet requested, so that a type with more than eight spare nodes gets a
 * request of eight and the rest of a type all in one. NEXT_ID numbers the
 * requests and the next free number is returned. */
uint16_t request_slot_groups_cc90c(int16_t next_id, uint32_t arg1, uint32_t head, ef_addr32 nodes,
                                   ef_addr32 owners, uint32_t arg4)
{
    ef_addr32 owner;

    for (owner = EF_U32(owners); owner != 0u; owner = EF_U32(owner + OWNER_NEXT)) {
        int16_t open = (int16_t)count_open_entries_cc840(owner);
        int16_t available[TYPE_COUNT];
        int16_t requested[TYPE_COUNT];
        ef_addr32 keys, node;
        int16_t total, i, j, k;
        uint16_t slot;
        uint8_t type = 0;

        if (open == 0)
            continue;
        keys = guest_fn_cc6c0_z((uint32_t)((int32_t)open * KEY_SIZE), 0x0018f430u, (uint32_t)(int32_t)open);
        k = 0;
        for (j = 0; j < (int16_t)EF_U16(owner + OWNER_SLOT_COUNT); j++) {
            ef_addr32 entry = EF_U32(owner + OWNER_SLOTS) + (uint32_t)((int32_t)j * 0x15);
            uint32_t state = EF_U8(entry + SLOT_STATE);

            if (state == 0xffu || (state == 0xfeu && EF_U8(owner + OWNER_CLASS) != 0x49u &&
                                   EF_U8(owner + OWNER_CLASS) != 0x4fu)) {
                ef_addr32 key = keys + (uint32_t)((int32_t)k * KEY_SIZE);

                EF_U32(key) = EF_U32(owner + OWNER_KEY);
                EF_U32(key + 4u) = EF_U32(owner + OWNER_KEY + 4u);
                EF_U32(key + 8u) = EF_U32(owner + OWNER_KEY + 8u);
                EF_U16(key + 0xcu) = EF_U16(EF_U32(owner + OWNER_SLOTS) + (uint32_t)((int32_t)j * 0x15) + SLOT_WORDS);
                EF_U16(key + 0xeu) = EF_U16(EF_U32(owner + OWNER_SLOTS) + (uint32_t)((int32_t)j * 0x15) + SLOT_WORDS + 2u);
                EF_U16(key + 0x10u) = EF_U16(EF_U32(owner + OWNER_SLOTS) + (uint32_t)((int32_t)j * 0x15) + SLOT_WORDS + 4u);
                k++;
            }
        }

        fill_bytes_b3fb0((ef_addr32)(uintptr_t)available, 0u, sizeof available);
        fill_bytes_b3fb0((ef_addr32)(uintptr_t)requested, 0u, sizeof requested);
        total = 0;
        for (node = EF_U32(nodes); node != 0u; node = EF_U32(node + NODE_NEXT)) {
            if (EF_U32(node + NODE_KEY) != EF_U32(owner + OWNER_KEY) ||
                EF_U32(node + NODE_KEY + 4u) != EF_U32(owner + OWNER_KEY + 4u))
                continue;
            if ((uint16_t)pair_matches_cc7e8(EF_U8(node + NODE_CLASS), EF_U8(node + NODE_TYPE)) == 0u &&
                (uint16_t)pair_matches_cc81c(EF_U8(node + NODE_CLASS), EF_U8(node + NODE_TYPE)) == 0u)
                continue;
            if (EF_U8(node + NODE_MODE) == 1u || EF_U8(node + NODE_MODE) == 2u ||
                EF_U16(node + NODE_CLAIMED) != 0u)
                continue;
            total++;
            available[EF_U8(node + NODE_TYPE)]++;
        }

        if (total != 0) {
            slot = 0;
            for (i = 0; i < open; i++) {
                int32_t spare;

                slot++;
                if ((int16_t)slot > 3)
                    slot = 0;
                if (EF_U8(owner + OWNER_KIND) > 5u)
                    continue;
                type = slot_type(EF_U8(owner + OWNER_KIND), slot, type);
                spare = (int32_t)available[type] - (int32_t)requested[type];
                if (spare > GROUP_LIMIT) {
                    next_id = (int16_t)build_group_request_ccdbc((uint32_t)(int32_t)next_id, type, GROUP_LIMIT,
                                                                 keys + (uint32_t)((int32_t)i * KEY_SIZE),
                                                                 owner, arg1, head, nodes, arg4);
                    requested[type] = (int16_t)(requested[type] + GROUP_LIMIT);
                } else if (spare > 0) {
                    next_id = (int16_t)build_group_request_ccdbc((uint32_t)(int32_t)next_id, type,
                                                                 (uint8_t)(available[type] - requested[type]),
                                                                 keys + (uint32_t)((int32_t)i * KEY_SIZE),
                                                                 owner, arg1, head, nodes, arg4);
                    requested[type] = (int16_t)(requested[type] + available[type]);
                }
            }
        }
        tail_jump_cc768(keys);
    }
    return (uint16_t)next_id;
}
