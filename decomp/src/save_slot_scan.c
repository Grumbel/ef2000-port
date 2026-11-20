/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/save_slot.h"

#define SAVE_DIR_VARIABLE   0x001822d8u /* environment variable for the save directory */
#define SAVE_NAME_IN_DIR    0x001822dcu /* format: directory, slot */
#define SAVE_NAME_LOCAL     0x001822ecu /* format: slot */
#define SAVE_FIRST_NAMES    0x0012521cu /* string pointers chosen by the first stored index */
#define SAVE_SECOND_NAMES   0x00125238u /* string pointers chosen by the second stored index */
#define SAVE_USE_DIRECTORY  0x0015ee9au /* word: 0 = use the directory variable */

static void read_field(int32_t handle, uint32_t size, ef_addr32 dest)
{
    read_handle_a79d4(0u, 0u, 0u, 0u, (uint32_t)handle, size, dest);
}

static void read_slot(int32_t handle, ef_addr32 slot)
{
    uint32_t index;
    ef_addr32 index_addr = (ef_addr32)(uintptr_t)&index;

    EF_U8(slot + offsetof(ef_save_slot, empty)) = 0u;
    read_field(handle, 0x15u, slot + offsetof(ef_save_slot, text_16));
    read_field(handle, 0x15u, slot + offsetof(ef_save_slot, text_01));
    read_field(handle, 0x15u, slot + offsetof(ef_save_slot, text_2b));
    read_field(handle, 4u, index_addr);
    copy_string_9d749(slot + offsetof(ef_save_slot, first_name), EF_U32(SAVE_FIRST_NAMES + index * 4u));
    read_field(handle, 4u, slot + offsetof(ef_save_slot, value_55));
    read_field(handle, 4u, slot + offsetof(ef_save_slot, value_55) + 4u);
    read_field(handle, 4u, slot + offsetof(ef_save_slot, value_55) + 8u);
    read_field(handle, 4u, index_addr);
    copy_string_9d749(slot + offsetof(ef_save_slot, second_name), EF_U32(SAVE_SECOND_NAMES + index * 4u));
    read_field(handle, 4u, slot + offsetof(ef_save_slot, value_6b));
    read_field(handle, 4u, slot + offsetof(ef_save_slot, value_6f));
}

/* 0x28224, 418 bytes. Read the summaries of the ten save files into the
 * slot table (missing files are marked empty), then ask 0x278c0 to pick
 * a slot for MODE. Returns the slot, or -(slot + 1) when it is empty. */
int16_t scan_save_slots_28224(uint8_t mode)
{
    uint8_t name[0x98];
    ef_addr32 buf = (ef_addr32)(uintptr_t)name;
    int32_t i;
    int32_t slot;

    for (i = 0; i < EF_SAVE_SLOT_COUNT; ++i) {
        ef_addr32 entry = EF_SAVE_SLOT_TABLE + (uint32_t)i * sizeof(ef_save_slot);
        int32_t handle;

        if (EF_U16(SAVE_USE_DIRECTORY) == 0u)
            guest_sprintf_a4342(buf, SAVE_NAME_IN_DIR, getenv_9c858(SAVE_DIR_VARIABLE), (uint32_t)i);
        else
            guest_sprintf_a4342(buf, SAVE_NAME_LOCAL, (uint32_t)i);
        handle = open_slot_a7900(0u, 0u, 0u, 0u, buf);
        if (handle == -1) {
            EF_U8(entry) = 1u;
            continue;
        }
        read_slot(handle, entry);
        close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
    }
    slot = (int16_t)guest_fn_278c0(mode);
    if (EF_U8(EF_SAVE_SLOT_TABLE + (uint32_t)slot * sizeof(ef_save_slot)) != 0u)
        return (int16_t)-(slot + 1);
    return (int16_t)slot;
}
