/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

static ef_addr32 read_record(uint32_t size, uint32_t handle)
{
    ef_addr32 record = menu_allocate_a7b98(size);
    guest_fn_9cde8(record, size, handle);
    return record;
}

static void read_extended_record(ef_addr32 owner, uint32_t offset, uint32_t handle)
{
    ef_addr32 record = menu_allocate_a7b98(0x18u);
    EF_U32(owner + offset) = record;
    guest_fn_9cde8(record, 0x18u, handle);
    EF_U32(record + 0x14u) = menu_allocate_a7b98(EF_U16(record + 0x12u));
    guest_fn_9cde8(EF_U32(record + 0x14u), EF_U16(record + 0x12u), handle);
}

static void read_pointer_record(ef_addr32 owner, uint32_t offset, uint32_t handle)
{
    ef_addr32 record = menu_allocate_a7b98(0x14u);
    EF_U32(owner + offset) = record;
    guest_fn_9cde8(record, 0x14u, handle);
    EF_U32(record + 0xeu) = menu_allocate_a7b98((uint32_t)EF_S16(record + 8u));
    EF_U16(record + 0xau) = 0u;
    guest_fn_b3fb0(EF_U32(record + 0xeu), 0u, (uint32_t)EF_S16(record + 8u));
}

/* 0xaa5c4, 1098 bytes. Read list records, groups, and the group's opcode
 * stream. Counters compare signed 16-bit progress against unsigned counts,
 * as in the original; malformed large counts can wrap indefinitely. */
void load_menu_stream_aa5c4(uint32_t handle)
{
    uint16_t list_count, group_count, child_count;
    uint16_t i, j;
    ef_addr32 record, group, node, payload;
    uint32_t size;
    EF_U32(0x4191fcu) = 0u;
    guest_fn_9cde8((ef_addr32)(uintptr_t)&list_count, 2u, handle);
    for (i = 0; (int32_t)(int16_t)i < (int32_t)list_count; ++i) {
        record = read_record(0x1eu, handle);
        EF_U32(record) = EF_U32(record + 4u) = 0u;
        append_node_aa1e4(record);
    }
    EF_U32(0x419200u) = 0u;
    EF_U16(0x4194c2u) = 0u;
    guest_fn_9cde8((ef_addr32)(uintptr_t)&group_count, 2u, handle);
    for (i = 0; (int32_t)(int16_t)i < (int32_t)group_count; ++i) {
        group = read_record(0x19u, handle);
        EF_U32(group) = EF_U32(group + 4u) = 0u;
        EF_U32(0x4191f8u) = group;
        child_count = EF_U16(group + 0xcu);
        EF_U32(group + 8u) = 0u;
        EF_U16(group + 0xcu) = 0u;
        append_node_aa270(group);
        for (j = 0; (int32_t)(int16_t)j < (int32_t)child_count; ++j) {
            node = read_record(0xdu, handle);
            size = 0u;
            switch (EF_U8(node)) {
            case 0: size = 0x17u; break;
            case 1: size = 0x12u; break;
            case 2: size = 9u; break;
            case 3: size = 0x11u; break;
            case 5: case 8: case 13: size = 0x52u; break;
            case 6: size = 0x5bu; break;
            case 9: size = 0xbu; break;
            case 10: size = 9u; break;
            case 11: size = 4u; break;
            case 14: size = 0x2au; break;
            case 15: size = 0xeu; break;
            default: break;
            }
            if (size != 0u) {
                payload = menu_allocate_a7b98(size);
                EF_U32(node + 9u) = payload;
                guest_fn_9cde8(payload, size, handle);
                if (EF_U8(node) == 0 && EF_U8(payload + 0x11u) != 0u)
                    read_extended_record(payload, 0x13u, handle);
                else if (EF_U8(node) == 1 && EF_U8(payload + 0xdu) != 0u)
                    read_extended_record(payload, 0xeu, handle);
                else if (EF_U8(node) == 2)
                    read_extended_record(payload, 5u, handle);
                else if (EF_U8(node) == 14) {
                    read_pointer_record(payload, 0x1eu, handle);
                    if (EF_U32(payload + 0x22u) != 0u)
                        read_pointer_record(payload, 0x22u, handle);
                    if (EF_U32(payload + 0x26u) != 0u)
                        read_pointer_record(payload, 0x26u, handle);
                }
            }
            append_child_aa228(node);
        }
    }
}
