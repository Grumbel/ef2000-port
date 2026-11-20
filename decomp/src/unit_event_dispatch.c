/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EVENT_KIND_LAST       0x85
#define EVENT_KIND_SIZE       7u       /* records at 0x126a60 */
#define EVENT_KIND_NOTIFY     0x00126a61u /* byte +1: 1 = tell the player */
#define EVENT_KIND_ALWAYS_A   0x4d     /* still shown while the message filter is set */
#define EVENT_KIND_ALWAYS_B   0x4f

#define UNIT_NONE             0xff
#define UNIT_SPECIAL_A        0x899
#define UNIT_SPECIAL_B        0x89a

#define OBJECT_FLAGS          0x24u
#define OBJECT_FLAG_GONE      0x08u
#define OBJECT_OWNER          0x18u    /* int16: player slot, or 0xff when unowned */
#define GRID_CELL_SIZE        0xcau
#define GRID_CELL_PLAYER      0x20u    /* byte: 1 when the cell is the player's own */

static int kind_notifies_player(int32_t kind)
{
    return EF_U8(EVENT_KIND_NOTIFY + (uint32_t)kind * EVENT_KIND_SIZE) == 1;
}

static int message_allowed(int32_t kind)
{
    return g_message_filter == 0 || kind == EVENT_KIND_ALWAYS_A || kind == EVENT_KIND_ALWAYS_B;
}

/* Objects owned by another player belong to a remote machine. */
static int owned_remotely(ef_addr32 obj)
{
    return EF_U16(obj + OBJECT_OWNER) != (uint16_t)g_world_grid_marker &&
           EF_S16(obj + OBJECT_OWNER) != UNIT_NONE;
}

/* 0x46254, 509 bytes. AX source unit, DX target unit, BX event kind
 * (0..0x85), ECX argument, then two stack dwords; callee cleans eight
 * bytes. Both units must exist (the source may be one of the special ids
 * 0xff, 0x899 and 0x89a, the target 0x899) and neither may be gone; a
 * source owned by another player drops the event. Events involving the
 * watched unit 0x1f71ca raise the alert flag 0x1f9011 or queue a player
 * message (0x47764) when the event kind asks for it. A target owned by
 * another player in a network game gets the event as packet 15 (0x548a4);
 * anything else is applied here by 0x46454. */
void dispatch_unit_event_46254(uint32_t source_in, int32_t target_in, uint32_t kind_in, uint32_t arg,
                               uint32_t stack_a, uint32_t stack_b)
{
    int16_t source = (int16_t)source_in;
    int16_t target = (int16_t)target_in;
    int16_t kind = (int16_t)kind_in;
    ef_addr32 target_obj;
    ef_addr32 source_obj;

    if (kind < 0 || kind > EVENT_KIND_LAST)
        return;
    target_obj = lookup_filtered_indexed_object(target);
    source_obj = lookup_filtered_indexed_object(source);
    if (source_obj == 0 && source != UNIT_SPECIAL_A && source != UNIT_NONE && source != UNIT_SPECIAL_B)
        return;
    if (target_obj == 0 && target != UNIT_SPECIAL_A)
        return;
    if (source_obj != 0) {
        if (EF_U8(source_obj + OBJECT_FLAGS) & OBJECT_FLAG_GONE)
            return;
        if (owned_remotely(source_obj))
            return;
    }

    if ((uint16_t)object_uses_secondary_grid(source) == (uint16_t)g_watched_unit &&
        kind_notifies_player(kind) && message_allowed(kind))
        g_watched_unit_alert = 1;
    if (source == g_watched_unit &&
        (EF_U8(g_world_grid_base + (uint32_t)((int32_t)g_active_grid_index * (int32_t)GRID_CELL_SIZE) + GRID_CELL_PLAYER) == 1 ||
         kind_notifies_player(kind)) &&
        message_allowed(kind))
        queue_message_47764((uint32_t)(int32_t)source, (uint32_t)(int32_t)kind, arg, stack_a, stack_b);

    if (target_obj != 0) {
        if (EF_U8(target_obj + OBJECT_FLAGS) & OBJECT_FLAG_GONE)
            return;
        if (owned_remotely(target_obj) && g_world_grid_mode != 0) {
            send_packet_15(source, target, (uint8_t)kind, arg, stack_a, stack_b);
            return;
        }
    }
    guest_fn_46454(source, target, (uint32_t)(int32_t)kind, arg, stack_a, stack_b);
}
