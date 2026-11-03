/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Functions with more than four register inputs; extra inputs are on the stack. */

/* 0x10710, 61 bytes. Store six parameters in the 0x197c24 block and mark it valid. */
void store_params_10710(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint16_t stack_e, uint8_t stack_f)
{
    EF_U16(0x00197c28u) = a;
    EF_U16(0x00197c2au) = b;
    EF_U16(0x00197c26u) = stack_e;
    EF_U16(0x00197c2cu) = c;
    EF_U8(0x00121ec6u) = 1;
    EF_U16(0x00197c24u) = d;
    EF_U8(0x00197c2eu) = stack_f;
}

/* 0xa5b1c, 60 bytes. Store four parameters in a widget and redraw it when visible. */
void set_widget_params_a5b1c(ef_addr32 widget, uint32_t a, uint16_t b, uint16_t c, uint16_t stack_d)
{
    if (widget == 0)
        return;
    EF_U32(widget + 0x95u) = a;
    EF_U16(widget + 0x99u) = b;
    EF_U16(widget + 0x9du) = stack_d;
    EF_U16(widget + 0x9bu) = c;
    if ((EF_U8(widget + 0x3bu) & 4u) && EF_U8(0x0013727du) != 0)
        guest_fn_a6038();
}

/* 0xd9ba0, 106 bytes. Mouse-style call 0x33 (entry value ORed with 0x70); results through four pointers. */
uint32_t query_device_d9ba0(uint32_t code, uint32_t arg, ef_addr32 out_a, ef_addr32 out_b, ef_addr32 stack_out_c, ef_addr32 stack_out_d)
{
    EF_U32(0x00420edcu) = code | 0x70u;
    EF_U32(0x00420ed0u) = (arg << 8) | 1u;
    guest_fn_8b8(0x33, 0x00420ec0u);
    EF_U32(out_a) = EF_U32(0x00420ed0u) & 0xffffu;
    EF_U32(out_b) = EF_U32(0x00420ed8u) & 0xffffu;
    EF_U32(stack_out_c) = EF_U32(0x00420ed4u) & 0xffffu;
    EF_U32(stack_out_d) = EF_U32(0x00420edcu) & 0xffu;
    return (uint32_t)((int32_t)(EF_U32(0x00420edcu) & 0xffffu) >> 8);
}
