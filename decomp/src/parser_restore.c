/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint8_t order[4], group[4], item[4];
} ef_parser_restore_frame;

EF_STATIC_ASSERT(parser_restore_group, offsetof(ef_parser_restore_frame, group) == 4);
EF_STATIC_ASSERT(parser_restore_item, offsetof(ef_parser_restore_frame, item) == 8);
EF_STATIC_ASSERT(parser_restore_size, sizeof(ef_parser_restore_frame) == 12);

static ef_addr32 resolve_parser_position(ef_addr32 frame)
{
    ef_addr32 group = EF_U32(0x00419200u);
    ef_addr32 item;
    uint32_t i;
    int32_t group_index = EF_S16(frame + 4u);
    int32_t item_index;

    for (i = 0u; (int16_t)i < group_index; ++i)
        group = EF_U32(group);
    item_index = EF_S16(frame + 8u);
    item = EF_U32(group + 8u);
    for (i = 0u; (int16_t)i < item_index; ++i)
        item = EF_U32(item + 1u);
    return item;
}

/* 0xab47c, 386 bytes. EAX=handle; preserve all other GPRs, plain RET.
 * Callers 286a9 and 28819 pass the opened handle and discard EAX. */
void restore_parser_state_ab47c(uint32_t handle)
{
    ef_parser_restore_frame storage;
    ef_addr32 frame = (ef_addr32)(uintptr_t)&storage;
    uint32_t i;
    int16_t bound;

    /* Only two bytes of each position slot are read. Do not initialize the
     * remaining bytes or add validation absent from the original. */
    for (i = 0u; i < 30u; ++i) {
        ef_addr32 slot = 0x00418708u + i * 4u;
        if (EF_U8(0x0041948eu + i) == 0u)
            guest_fn_a79d4(handle, 4u, slot);
        else
            guest_fn_a79d4(handle, 0x15u, EF_U32(slot));
    }
    guest_fn_a79d4(handle, 2u, 0x004194acu);
    i = 0u;
    if (EF_S16(0x004194acu) > 0) {
        do {
            ef_addr32 item;
            guest_fn_a79d4(handle, 2u, frame + 4u);
            guest_fn_a79d4(handle, 2u, frame + 8u);
            item = resolve_parser_position(frame);
            /* Sample before the unchecked table write: it can alias count. */
            bound = EF_S16(0x004194acu);
            EF_U32(0x00418e38u + (uint32_t)(int32_t)(int16_t)i * 4u) = item;
            ++i;
        } while ((int16_t)i < bound);
    }
    guest_fn_a79d4(handle, 2u, frame + 4u);
    guest_fn_a79d4(handle, 2u, frame + 8u);
    if (EF_S16(frame + 4u) != -1)
        EF_U32(0x004191dcu) = resolve_parser_position(frame);

    visit_group_nodes_ab28c(handle);
    EF_U16(0x00137202u) = 0xffffu;
    guest_fn_a79d4(handle, 2u, 0x00137202u);
    if (EF_S16(0x00137202u) != -1) {
        EF_U8(frame) = 0u;
        EF_U16(frame + 2u) = EF_U16(0x00137202u);
        EF_U8(frame + 1u) = 0u;
        latch_order_target_a905c(frame);
    }
    EF_U8(0x00137205u) = 1u;
}
