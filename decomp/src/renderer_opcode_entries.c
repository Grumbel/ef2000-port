/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Four handlers of the renderer object dispatcher 0xc4270, which calls
 * the table at 0xc3f88 by the object's type byte (0x148e58 + index) with
 * the object index in ESI. Types 0 and 4 are the two stream interpreters
 * 0xc5551 and 0xc5681 themselves; these handlers are types 8, 12, 16 and 20 and
 * either reset the type to an interpreter or toggle the object's hidden
 * bit (0x149658 + index), re-running the interpreter around a renderer
 * setup with code 15 (0xc4412) and a dispatch reinstall (0xc448b). */

#define OBJECT_TYPE        0x00148e58u
#define OBJECT_FLAGS       0x00149658u
#define INTERPRET_A        0x000c5551u
#define INTERPRET_B        0x000c5681u
#define TYPE_A             0u
#define TYPE_B             4u
#define FLAG_HIDDEN        0x80u
#define SETUP_CODE         15u

static void rerun(ef_addr32 interpreter, uint32_t index)
{
    setup_renderer_tables_c4412(SETUP_CODE);
    guest_callptr_esi_x(interpreter, index);
    install_graphics_dispatch_c448b();
}

/* Toggle the hidden bit; a cleared bit just re-runs the interpreter. */
static void toggle(ef_addr32 interpreter, uint32_t index)
{
    EF_U8(OBJECT_FLAGS + index) ^= FLAG_HIDDEN;
    if (EF_U8(OBJECT_FLAGS + index) == 0u)
        guest_callptr_esi_x(interpreter, index);
    else
        rerun(interpreter, index);
}

/* 0xc547f, 31 bytes. */
void renderer_toggle_opcode_a_c547f(uint32_t index)
{
    toggle(INTERPRET_A, index);
}

/* 0xc549e, 25 bytes. */
void renderer_reset_opcode_a_c549e(uint32_t index)
{
    EF_U8(OBJECT_TYPE + index) = TYPE_A;
    rerun(INTERPRET_A, index);
}

/* 0xc54b7, 31 bytes. */
void renderer_toggle_opcode_b_c54b7(uint32_t index)
{
    toggle(INTERPRET_B, index);
}

/* 0xc54d6, 25 bytes. */
void renderer_reset_opcode_b_c54d6(uint32_t index)
{
    EF_U8(OBJECT_TYPE + index) = TYPE_B;
    rerun(INTERPRET_B, index);
}
