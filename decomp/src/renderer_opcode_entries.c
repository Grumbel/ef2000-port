/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Four terminal opcode-handler entries in the c54xx threaded renderer.
 * Their machine-code boundaries are not present in the Ghidra function
 * inventory: each has a distinct entry, explicit RET, and a subsequent
 * contiguous entry. The complex c5551/c5681 interpreters are represented
 * by guest callbacks, not claimed as recovered here. */

static void execute_opcode(uint32_t original, uint32_t index, uint8_t setup)
{
    if (setup)
        renderer_opcode_setup_c54xx(0x0fu);
    renderer_opcode_dispatch_c54xx(original, index);
    if (setup)
        renderer_opcode_install_c54xx();
}

/* c547f, 31 bytes: toggle bit 7; only reconfigure for the nonzero state. */
void renderer_toggle_opcode_a_c547f(uint32_t index)
{
    uint8_t *flag = (uint8_t *)ef_guest_memory_ptr(0x149658u + index);
    *flag ^= 0x80u;
    execute_opcode(0xc5551u, index, (uint8_t)(*flag != 0u));
}

/* c549e, 25 bytes: clear interpreter type and force dispatch reconfigure. */
void renderer_reset_opcode_a_c549e(uint32_t index)
{
    EF_U8(0x148e58u + index) = 0u;
    execute_opcode(0xc5551u, index, 1u);
}

/* c54b7, 31 bytes: second interpreter family, same toggle and branch. */
void renderer_toggle_opcode_b_c54b7(uint32_t index)
{
    uint8_t *flag = (uint8_t *)ef_guest_memory_ptr(0x149658u + index);
    *flag ^= 0x80u;
    execute_opcode(0xc5681u, index, (uint8_t)(*flag != 0u));
}

/* c54d6, 25 bytes: type 4 selects the second interpreter family. */
void renderer_reset_opcode_b_c54d6(uint32_t index)
{
    EF_U8(0x148e58u + index) = 4u;
    execute_opcode(0xc5681u, index, 1u);
}
