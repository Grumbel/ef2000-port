/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define CONTROLLED_SIDE      0x001f71ceu /* word compared with object +0x16 */
#define LINKED_SESSION       0x00127da8u /* byte */
#define WORLD_GRID_BASE      0x0019bb58u /* pointer to 0xca-byte records */
#define GRID_RECORD_SIZE     0xca

#define OBJ_X                0x00u /* 24.8 fixed point */
#define OBJ_Y                0x04u
#define OBJ_Z                0x08u
#define OBJ_SPEED            0x12u
#define OBJ_SIDE             0x16u /* word */
#define OBJ_TARGET           0x1cu
#define OBJ_PAGE             0x20u /* word */
#define OBJ_FLAGS24          0x24u
#define OBJ_FLAGS26          0x26u
#define OBJ_CELL             0x28u /* word, index into the world grid */
#define OBJ_SLOTS            0x18eu /* 12 bytes */

#define FLAG24_RESTART       0x10u
#define FLAG24_PENDING       0x04u

/* 0x8d11c, 531 bytes. Restart object index. An object of another side only gets its restart
 * flag; for the controlled side the view, pages and object state are reset, the scene is
 * re-entered with two 0xc events and the scene subject is placed at the object. */
void restart_object_8d11c(int16_t index)
{
    ef_addr32 obj = guest_fn_7489c(index);
    ef_addr32 cell;
    int16_t i;

    if (obj == 0)
        return;
    if (EF_U16(obj + OBJ_SIDE) != EF_U16(CONTROLLED_SIDE)) {
        EF_U8(obj + OBJ_FLAGS24) = (uint8_t)((EF_U8(obj + OBJ_FLAGS24) | FLAG24_RESTART) & ~FLAG24_PENDING);
        return;
    }

    EF_U8(0x001a363au) = 0;
    EF_U8(0x001a363bu) = 0;
    EF_U8(0x001a3636u) = 0;
    set_runtime_phase_five();
    reset_working_state_e76c8();
    select_page_9e3f8((uint32_t)(int16_t)EF_U16(obj + OBJ_PAGE));
    run_page_text_9e508((int16_t)EF_U16(obj + OBJ_PAGE), 0);
    open_entry_cd150((int16_t)EF_U16(obj + OBJ_PAGE));
    script_store_step_cd20c(0);
    run_page_text_9e508((int16_t)EF_U16(obj + OBJ_PAGE), 3);
    clear_named_references_8ef38(obj);
    notify_record_action_8c988(obj);

    EF_U32(obj + OBJ_Y) = (uint32_t)-100;
    EF_U32(obj + OBJ_SPEED) = 0;
    for (i = 0; i < 12; i++)
        EF_U8(obj + OBJ_SLOTS + (uint32_t)i) = 0;
    EF_U8(obj + OBJ_FLAGS24) &= 0xe1u;
    EF_U8(obj + OBJ_FLAGS26) &= 0xbfu;

    if (EF_U8(LINKED_SESSION) != 0) {
        mark_ui_links_by_slot(index);
        EF_U16(0x001a823eu) = 0xffffu;
        EF_U16(0x001a8238u) = 0xffffu;
        EF_U32(obj + OBJ_TARGET) = 0;
        EF_U8(0x0012d7aau) = 0;
        reset_menu_7eec4();
        cell = EF_U32(WORLD_GRID_BASE) + (uint32_t)((int32_t)(int16_t)EF_U16(obj + OBJ_CELL) * GRID_RECORD_SIZE);
        guest_fn_7ee8(index, EF_U8(cell + 0x5fu));
        cell = EF_U32(WORLD_GRID_BASE) + (uint32_t)((int32_t)(int16_t)EF_U16(obj + OBJ_CELL) * GRID_RECORD_SIZE);
        /* The original compares the sign-extended side with the zero-extended owner word,
         * so a negative side never matches. Watcom folds the mixed-sign compare into a
         * 16-bit one, hence the explicit sign test. */
        if ((int16_t)EF_U16(obj + OBJ_SIDE) >= 0 && EF_U16(obj + OBJ_SIDE) == EF_U16(cell + 0x50u))
            EF_U8(cell + 0x8eu) = 0;
        reset_object_link_controls();
    }

    refresh_weapon_state_8cb60();
    reinit_screens_5fd8();
    EF_U8(0x001a8246u) = 0;
    refresh_for_mode_65a50();
    EF_U8(0x001a8246u) = 1;
    EF_U16(0x00122c7cu) = 0x32;
    EF_U8(0x00122c53u) = 0;
    EF_U16(0x00122c7au) = 0;
    record_scene_event(0xc);
    record_scene_event(0xc);
    EF_U16(0x00122c98u) = 0xc;
    guest_fn_913dc(0, 4);
    EF_U32(0x0021444cu) = (uint32_t)((int32_t)EF_U32(obj + OBJ_X) >> 8);
    EF_U32(0x00214458u) = obj;
    EF_U32(0x00214448u) = (uint32_t)((int32_t)EF_U32(obj + OBJ_Y) >> 8);
    EF_U32(0x00214450u) = (uint32_t)((int32_t)EF_U32(obj + OBJ_Z) >> 8);
    set_scene_input_triplet((int32_t)(EF_U32(obj + OBJ_X) << 8), (int32_t)(EF_U32(obj + OBJ_Y) << 8),
                            (int32_t)(EF_U32(obj + OBJ_Z) << 8));
}
