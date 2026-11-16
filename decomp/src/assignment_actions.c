/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define ASSIGN_U8(a) (*(volatile uint8_t *)ef_guest_memory_ptr(a))
#define ASSIGN_U32(a) (*(volatile uint32_t *)ef_guest_memory_ptr(a))

/* 0x4d9cc,95 bytes. No inputs, six registers preserved, EAX scratch,
 * plain RET. Registry mpPrevAssign actually advances the index. */
void gui_previous_assignment_4d9cc(void)
{
    uint8_t index;
    uint8_t kind;

    if (ASSIGN_U8(0x001a5532u) == 0u) {
        update_dialog_bindings(0x00132351u, 0x00132373u);
        return;
    }
    /* ORIGINAL BUG: no exit if no category three/four entry exists. */
    for (;;) {
        index = (uint8_t)(ASSIGN_U8(0x001a5598u) + 1u);
        ASSIGN_U8(0x001a5598u) = index;
        if (index >= ASSIGN_U8(0x001a5531u))
            ASSIGN_U8(0x001a5598u) = 0u;
        index = ASSIGN_U8(0x001a5598u);
        kind = ASSIGN_U8(0x001a5510u + (uint32_t)index);
        if (kind == 3u || kind == 4u)
            break;
    }
    fill_selection_dialog_4db60();
}

/* 0x4da2c,99 bytes. Same no-input/preserved/RET contract. Registry
 * mpMakeAssign decrements, treating both 254 and 255 as wraparound. */
void gui_make_assignment_4da2c(void)
{
    uint8_t index;
    uint8_t kind;

    if (ASSIGN_U8(0x001a5532u) == 0u) {
        update_dialog_bindings(0x00132351u, 0x00132373u);
        return;
    }
    /* ORIGINAL BUG: retain the unbounded category search. */
    for (;;) {
        index = (uint8_t)(ASSIGN_U8(0x001a5598u) - 1u);
        ASSIGN_U8(0x001a5598u) = index;
        if (index >= 254u) {
            index = (uint8_t)(ASSIGN_U8(0x001a5531u) - 1u);
            ASSIGN_U8(0x001a5598u) = index;
        }
        index = ASSIGN_U8(0x001a5598u);
        kind = ASSIGN_U8(0x001a5510u + (uint32_t)index);
        if (kind == 3u || kind == 4u)
            break;
    }
    fill_selection_dialog_4db60();
}

/* 0x4da90,85 bytes. Same contract. Save byte, reload index, save dword,
 * then advance and redraw again (the advance callback already redraws). */
void gui_view_assignment_4da90(void)
{
    uint32_t index;
    uint8_t byte;
    uint32_t value;

    if (ASSIGN_U8(0x001a5532u) == 0u) {
        update_dialog_bindings(0x00132351u, 0x00132373u);
        return;
    }
    index = ASSIGN_U8(0x001a5598u);
    byte = ASSIGN_U8(0x001a5540u);
    ASSIGN_U8(0x001a5544u + index) = byte;
    index = ASSIGN_U8(0x001a5598u);
    value = ASSIGN_U32(0x001a5554u);
    ASSIGN_U32(0x001a5558u + index * 4u) = value;
    gui_previous_assignment_4d9cc();
    fill_selection_dialog_4db60();
}
