/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define GRID_RECORD_SIZE   0xca
#define ENTRY_SIZE         25
#define SIDE_NONE          0xff

/* Grid record fields */
#define ROW_STATE          0x1a
#define ROW_LINK           0x24   /* int16 */
#define ROW_WATCHED_UNIT   0x50   /* uint16 */
#define ROW_UNIT_MODE      0x5e
#define ROW_POSITION       0x61   /* double[2] */
#define ROW_TARGET_DONE    0x89
#define ROW_ENTRIES        0x8a   /* pointer to 25-byte entries */
#define ROW_ENTRY_INDEX    0x8e
#define ROW_PLAYER_FLAG    0x20

/* Entry fields */
#define ENTRY_CLOCK        0x10
#define ENTRY_KIND         0x18

enum {
    KIND_MOVE = 1, KIND_ATTACK = 2, KIND_WAIT = 3, KIND_LAND = 4, KIND_LAUNCH = 5,
    KIND_PATROL = 6, KIND_ASSIGN = 7
};

static ef_addr32 row_record(int16_t row)
{
    return g_world_grid_base + (uint32_t)((int32_t)row * GRID_RECORD_SIZE);
}

static ef_addr32 current_entry(ef_addr32 rec)
{
    return EF_U32(rec + ROW_ENTRIES) + (uint32_t)EF_U8(rec + ROW_ENTRY_INDEX) * ENTRY_SIZE;
}

/* Row states 2, 3, 4, 8, 9, 11 and 12 keep a patrol entry as it is. */
static int patrol_state_is_busy(uint8_t state)
{
    return state == 8 || state == 9 || state == 11 || state == 2 || state == 3 || state == 4 || state == 12;
}

/* 0x857bc, 615 bytes. Refresh grid row ROW for its current entry. The
 * position doubles are converted (0xb4250) and the results dropped, as in
 * the original. Wait entries finish when their clock is present and
 * reached (else 0x85380 runs), land and launch entries change the row
 * state; the player's own row with the watched unit only steps on (0x8516c).
 * Other kinds: move sets the unit mode unless it is 1, attack decrements
 * the link unless the owner has weight and reaches, or assigns, patrol in
 * an idle state without a link sets state 8, assign tries 0x2b508; most
 * paths end in 0x8516c. */
void refresh_row_target_857bc(int16_t row)
{
    ef_addr32 rec = row_record(row);
    ef_addr32 entry;

    (void)double_to_scaled_b4250(EF_F64(rec + ROW_POSITION));
    (void)double_to_scaled_b4250(EF_F64(rec + ROW_POSITION + 8));
    rec = row_record(row);
    entry = current_entry(rec);
    switch (EF_U8(entry + ENTRY_KIND)) {
    case KIND_LAND:
        guest_fn_2b1f4_x(row, 5);
        guest_fn_8516c_x(row);
        return;
    case KIND_LAUNCH:
        set_record_state_2b0b4((uint32_t)(int32_t)row, 1);
        guest_fn_8516c_x(row);
        return;
    case KIND_WAIT:
        if (clock_triplet_is_present(entry + ENTRY_CLOCK) && grid_clock_reaches_reference(row)) {
            EF_U8(row_record(row) + ROW_TARGET_DONE) = 0;
            guest_fn_8516c_x(row);
        } else {
            drain_kind_two_chain_85380((uint32_t)(int32_t)row);
        }
        return;
    }

    if (row == g_active_grid_index) {
        ef_addr32 own = row_record(g_active_grid_index);

        if ((int32_t)EF_U16(own + ROW_WATCHED_UNIT) == (int32_t)g_watched_unit && EF_U8(own + ROW_PLAYER_FLAG) == 1) {
            guest_fn_8516c_x(g_active_grid_index);
            return;
        }
    }

    rec = row_record(row);
    switch (EF_U8(current_entry(rec) + ENTRY_KIND)) {
    case KIND_MOVE:
        if (EF_U8(rec + ROW_UNIT_MODE) != 1)
            set_unit_mode_488f8(row, 1);
        break;
    case KIND_ATTACK:
        if ((uint8_t)owner_has_weight_8557c((uint32_t)(int32_t)row) == 0) { /* AL result */
            decrement_grid_row_link(row);
        } else if (row_within_reach_85420(row)) {
            EF_U8(row_record(row) + ROW_TARGET_DONE) = 0;
            guest_fn_8516c_x(row);
            guest_fn_2b95c_x(row);
        } else if (!try_assign_2b508(row)) {
            decrement_grid_row_link(row);
        }
        return;
    case KIND_PATROL:
        if (!patrol_state_is_busy(EF_U8(rec + ROW_STATE)) && EF_S16(rec + ROW_LINK) == SIDE_NONE)
            set_record_state_2b0b4((uint32_t)(int32_t)row, 8);
        return;
    case KIND_ASSIGN:
        try_assign_2b508(row);
        break;
    }
    guest_fn_8516c_x(row);
}
