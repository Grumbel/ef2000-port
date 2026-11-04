/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Object update loop, unit speed setup and cell trigger, 0x883cc..0x8d030.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0x883cc, 157 bytes. Step one object: run its handler when it is alive and has one. */
void step_object_883cc(ef_addr32 object)
{
    store_runtime_control_word(0x191);
    if ((int16_t)EF_U16(object + 0x40u) <= 0 || EF_U8(object + 0x42u) != 0 || EF_U32(object + 0x3cu) == 0) {
        initialize_ui_record_ring(object);
        return;
    }
    refresh_object_child_slot(object);
    if (EF_U8(0x00127da9u) != 0 && (EF_U8(object + 0x67u) & 1u) == 0)
        guest_fn_55aac(object);
    EF_U32(object + 0x1au) = EF_U32(object);
    EF_U32(object + 0x1eu) = EF_U32(object + 4u);
    EF_U32(object + 0x22u) = EF_U32(object + 8u);
    store_runtime_control_word(0x193);
    EF_U8(0x0020f22fu) = 0;
    if (original_record_selection_check(object) == 1)
        ((void (*)(ef_addr32))EF_U32(object + 0x3cu))(object);
    store_runtime_control_word(0x194);
}

/* 0x8846c, 217 bytes. Step every object of the ring and publish the visible ones. */
void step_objects_8846c(void)
{
    ef_addr32 object;
    ef_addr32 next;

    clear_input_state_block();
    reset_target_search();
    object = EF_U32(EF_U32(0x0020f160u) + 0x34u);
    if (object == EF_U32(0x0020f164u))
        return;
    do {
        next = EF_U32(object + 0x34u);
        if (EF_U8(0x00127da9u) != 0 && EF_U8(0x001a666bu) != EF_U8(object + 0x50u))
            guest_fn_89e08(object);
        else
            step_object_883cc(object);
        guest_fn_88134(object);
        guest_fn_88574(object);
        if (EF_U8(object + 0x58u) != 0) {
            EF_S32(0x0020f16cu) = EF_S32(object) >> 8;
            EF_S32(0x0020f170u) = EF_S32(object + 4u) >> 8;
            EF_S32(0x0020f174u) = EF_S32(object + 8u) >> 8;
            if ((int32_t)guest_fn_237c8(0x0020f16cu) < 0xfa0) {
                EF_U16(object + 0x78u) = EF_U16(object + 0xcu);
                EF_U16(object + 0x7au) = EF_U16(object + 0xeu);
                EF_U16(object + 0x7cu) = EF_U16(object + 0x10u);
                EF_U32(object + 0x6au) = EF_U32(object);
                EF_U32(object + 0x6eu) = EF_U32(object + 4u);
                EF_U32(object + 0x72u) = EF_U32(object + 8u);
                guest_fn_80b14(object + 0x68u);
            }
        }
        object = next;
    } while (object != EF_U32(0x0020f164u));
}

/* 0x8cc40, 171 bytes. Set a unit's base speed from its type and add 2 when the trigger fires. */
void set_unit_speed_8cc40(int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);
    uint8_t speed;

    if (unit == 0)
        return;
    speed = EF_U8(0x00128647u + (uint32_t)EF_U8(unit + 0xe9u) * 0xe5u);
    EF_U8(unit + 0x140u) = speed;
    if (EF_U8(unit + 0x2au) == 0)
        EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca) + 0x4cu) = speed;
    if ((guest_fn_2949c((int32_t)unit_id, 0x23u) & 0xffu) != 0xffu) {
        EF_U8(unit + 0x140u) = (uint8_t)(EF_U8(unit + 0x140u) + 2u);
        if (EF_U8(unit + 0x2au) == 0)
            EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca) + 0x4cu) =
                (uint8_t)(EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca) + 0x4cu) + 2u);
    }
}

/* 0x8d030, 176 bytes. Fire the cell trigger when the player's unit stands on a special terrain value. */
void check_cell_trigger_8d030(void)
{
    int32_t pos[3];
    ef_addr32 unit;
    uint32_t cell;
    uint32_t value;

    if (EF_U16(0x001f88e8u) != 0)
        return;
    unit = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    if (unit == 0)
        return;
    if ((EF_U8(unit + 0x24u) & 4u) != 0 || (EF_U8(unit + 0x24u) & 8u) != 0 || EF_U8(unit + 0x18du) == 0)
        return;
    pos[0] = EF_S32(unit) >> 8;
    pos[1] = EF_S32(unit + 4u) >> 8;
    pos[2] = EF_S32(unit + 8u) >> 8;
    cell = guest_fn_7b2f4_4(0, EF_S32(unit) >> 0x13, EF_S32(unit + 8u) >> 0x13, (uint16_t)pos[2]);
    value = guest_fn_39744((int16_t)cell, (uint16_t)pos[0], (int16_t)((uint32_t)pos[1] >> 0), 0) & 0xffu;
    if (value >= 0x70 && value <= 0x8f)
        guest_fn_8cd78((int32_t)EF_U32(0x001f71c8u) >> 16);
}
