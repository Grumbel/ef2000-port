/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef union {
    uint64_t pair;
    uint32_t words[2];
} ef_comms_clock_registers;

/* The installed initializers ed090/e0984/e11fc/e1568 take no inputs.
 * Only their AL result is consumed. */
#ifdef __WATCOMC__
static uint8_t initialize_comms_driver(void);
#pragma aux initialize_comms_driver = "call dword ptr ds:[0x136ba0]" \
    value [al] modify [eax ebx ecx edx esi edi]

/* Preserve the original zero inputs and EBX's uncleared high word. The
 * clock's EAX is stored first; its returned BX and DX/DL are still used
 * after clear_tables. Do not replace these with assumed zero constants. */
static uint64_t initialize_comms_clock(uint32_t upper_bx);
#pragma aux initialize_comms_clock parm [ebx] value [edx eax] \
    modify [eax ebx ecx edx esi edi] = \
    "xor eax,eax" \
    "xor edx,edx" \
    "xor ecx,ecx" \
    "xor esi,esi" \
    "xor edi,edi" \
    "call dword ptr ds:[0x15b620]" \
    "mov dword ptr ds:[0x136dd4],eax" \
    "mov eax,ebx"
#else
extern uint8_t initialize_comms_driver(void);
extern uint64_t initialize_comms_clock(uint32_t upper_bx);
#endif

/* 0xa3c18,390 bytes. No inputs, AL status, six non-EAX GPRs preserved.
 * Callers 346b3/34743 and 34bf1 inspect only AL. */
uint8_t initialize_comms_a3c18(void)
{
    uint8_t status, mode;
    ef_addr32 output;
    ef_comms_clock_registers clock;

    if (EF_U8(0x0013578cu) == 1u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018cee8u);
    EF_U8(0x0013581du) = 0u;
    EF_U8(0x0013581eu) = 0u;
    check_name_a39ec();
    EF_U8(0x0013578du) = 0u;
    EF_U32(0x001357c0u) = 0u;
    fill_block_a3b48();
    init_entry_table_a3bfc();
    if (EF_U8(0x001357bau) == 0u) {
        latch_decoded_key_a29cc(1u);
        status = EF_U8(0x0013578eu);
        if (status != 0u)
            return status;
    } else {
        EF_U8(0x0013578eu) = 0u;
    }
    install_handlers_a3acc();
    install_render_table_e1570();
    status = initialize_comms_driver();
    EF_U8(0x0013578eu) = status;
    if (status != 0u && status != 0x16u) {
        EF_U32(0x00135790u) = EF_U32(0x00135710u + (uint32_t)status * 4u);
        return status;
    }
    clear_all_entries_a2e6c();
    mode = EF_U8(0x001357c8u);
    EF_U16(0x001357b0u) = 0u;
    if (mode != 0u) {
        format_entry_a2e90(0u);
        EF_U16(0x001357b0u) = 1u;
    }
    if (EF_U8(0x0013578du) == 0u)
        EF_U8(0x0013578cu) = 1u;
    output = EF_U32(0x00135794u);
    if (output != 0u)
        EF_U8(output) = 0u;
    status = EF_U8(0x0013578eu);
    EF_U8(0x00135798u) = 0u;
    EF_U16(0x001357aeu) = 0u;
    EF_U16(0x00136bd0u) = 0u;
    EF_U16(0x001357b2u) = 0u;
    output &= 0xffff0000u;
    {
        uint32_t message = EF_U32(0x00135710u + (uint32_t)status * 4u);
        EF_U8(0x001357c9u) = 0u;
        EF_U8(0x00136bc9u) = 0u;
        EF_U8(0x0013583cu) = 0u;
        EF_U8(0x00136bd2u) = 0u;
        EF_U32(0x00135790u) = message;
    }
    EF_U8(0x00136dd0u) = 0u;
    EF_U16(0x001357d4u) = 0u;
    clock.pair = initialize_comms_clock(output);
    EF_U16(0x001357d6u) = (uint16_t)clock.words[1];
    clear_tables_a38c8();
    status = EF_U8(0x0013578eu);
    EF_U16(0x001357d8u) = (uint16_t)clock.words[0];
    EF_U8(0x0013581cu) = (uint8_t)clock.words[1];
    return status;
}
