/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"
#include <stddef.h>

typedef struct MenuInstructionLayout {
    uint8_t opcode, next[4], branch[4], operand[4];
} MenuInstructionLayout;
typedef char menu_instruction_offsets[
    sizeof(MenuInstructionLayout) == 13 &&
    offsetof(MenuInstructionLayout, next) == 1 &&
    offsetof(MenuInstructionLayout, branch) == 5 &&
    offsetof(MenuInstructionLayout, operand) == 9 ? 1 : -1];

/* 0xa9e70, 768 bytes. Execute the current GUI instruction stream.
 * Stream instruction: opcode +0, next +1, branch +5, operand +9.
 * These fields are deliberately unaligned. */
void dispatch_menu_state_a9e70(void)
{
    ef_addr32 instruction;
    menu_call_b3f48();
    menu_seed_a45e4(0x31415926u);
    menu_call_dc250();
    menu_call_dc250();
    menu_call_dcc10();
    EF_U8(0x161e30u) = 0u;
    EF_U32(0x4191ecu) = 0u;
    EF_U32(0x4191dcu) = EF_U32(EF_U32(0x4191f8u) + 8u);
    do {
        EF_U8(0x1371fau) = 0u;
        menu_call_dcc54();
        EF_U32(0x4191d0u) = 0u;
        EF_U8(0x41f833u) = 0u;
        if ((EF_U8(0x1371feu) != 0u && EF_U8(0x1371ffu) == 0u &&
             EF_U8(0x137201u) == 0u &&
             EF_U8(EF_U32(0x4191dcu)) == 11u) ||
            EF_U8(0x137207u) != 0u) {
            if (EF_U8(0x137207u) != 0u) {
                instruction = EF_U32(EF_U32(0x4191dcu) + 5u);
                if (instruction != 0u) {
                    EF_U32(0x4191dcu) = instruction;
                    EF_U8(0x137208u) = 1u;
                }
                EF_U8(0x137207u) = 0u;
            }
            menu_call_a9d48();
        }
        instruction = EF_U32(0x4191dcu);
        EF_U8(0x137201u) = 0u;
        switch (EF_U8(instruction)) {
        case 0u: menu_op_a8bd4(EF_U32(instruction + 9u)); break;
        case 1u: menu_op_a8a38(EF_U32(instruction + 9u)); break;
        case 2u: menu_op_a8958(EF_U32(instruction + 9u)); break;
        case 3u: menu_op_a9130(EF_U32(instruction + 9u)); break;
        case 4u: menu_op_a944c(); break;
        case 5u: menu_op_a9724(EF_U32(instruction + 9u)); break;
        case 6u: menu_op_a9878(EF_U32(instruction + 9u)); break;
        case 7u: menu_op_a9bd0(); break;
        case 8u: menu_op_a9bec(EF_U32(instruction + 9u)); break;
        case 9u: (void)EF_U32(instruction + 9u); break;
        case 10u: menu_op_a983c(EF_U32(instruction + 9u)); break;
        case 11u: menu_op_a905c(EF_U32(instruction + 9u)); break;
        case 12u: menu_op_a9bbc(); break;
        case 13u: menu_op_a99c0(EF_U32(instruction + 9u)); break;
        case 14u: menu_op_a9694(EF_U32(instruction + 9u)); break;
        case 15u: menu_op_a9aa8(EF_U32(instruction + 9u)); break;
        default: break;
        }
        if (EF_U8(0x1371eeu) == 0u)
            menu_call_a9cd0();
        if (EF_U8(0x1371fbu) != 0u) {
            if (EF_U8(0x1371edu) != 0u) menu_call_a992c();
            menu_call_28744();
            if (EF_U8(0x1371edu) != 0u) menu_call_a9990();
            EF_U8(0x1371fbu) = 0u;
        }
        if (EF_U8(0x1371edu) != 0u) menu_call_b3f50();
        else menu_call_b3f48();
        if (EF_U32(0x4191ecu) != 0u) {
            EF_U32(0x4191dcu) = EF_U32(0x4191ecu);
            EF_U32(0x4191ecu) = 0u;
        } else if (EF_U8(0x1371edu) == 2u) {
            EF_U32(0x4191dcu) = EF_U32(EF_U32(0x4191f8u) + 8u);
            EF_U8(0x1371edu) = 1u;
        } else {
            EF_U32(0x4191dcu) = EF_U32(EF_U32(0x4191dcu) + 1u);
        }
        if (EF_U32(0x4191dcu) == 0u && EF_U8(0x1371edu) != 0u) {
            menu_call_b3f48();
            menu_call_a9900();
            EF_U32(0x4191dcu) = EF_U32(EF_U32(0x4191dcu) + 1u);
            EF_U8(0x1371edu) = 0u;
        }
        if (EF_U8(0x1371edu) == 0u && EF_U8(0x41955fu) != 0u) {
            --EF_U8(0x41955fu);
            if (EF_U8(0x41955fu) == 0u)
                EF_U32(0x4191dcu) = EF_U32(0x4191d4u);
        }
        if (EF_U8(0x1371fcu) != 0u) {
            if (EF_U8(0x1371edu) != 0u) {
                menu_call_b3f48();
                menu_call_a9900();
                EF_U8(0x1371edu) = 0u;
            }
            EF_U32(0x4191dcu) = menu_select_a9d0c();
            EF_U8(0x1371fcu) = 0u;
        }
        if (EF_U32(0x4191dcu) == 0u && EF_U8(0x1371ffu) != 0u)
            EF_U8(0x137200u) = 1u;
        if (EF_U8(0x137200u) != 0u) {
            if (EF_U8(0x1371edu) != 0u) {
#ifdef __WATCOMC__
                typedef void (__cdecl *Reporter)(ef_addr32);
#else
                typedef void (*Reporter)(ef_addr32);
#endif
                ((Reporter)(uintptr_t)EF_U32(0x132a74u))(0x18d850u);
            }
            menu_call_a9d48();
        }
    } while (EF_U8(0x137200u) != 0u ||
             (EF_U32(0x4191dcu) != 0u && EF_U8(0x1371f8u) == 0u));
    EF_U8(0x1371f8u) = 0u;
}
