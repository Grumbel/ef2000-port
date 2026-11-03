/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Status-line, unit-command and parameter-store helpers, 0x10710..0x39203. */

/* 0x1f0c8, 52 bytes. */
void load_resource_1f0c8(void)
{
    EF_U32(0x001237e4u) = 0;
    if (EF_U8(0x00425c6du) != 0)
        EF_U32(0x001237e4u) = guest_fn_c3670(0x00425c6du, 0, 0, 0x200, 0x190);
}

/* 0x2d894, 54 bytes. Store two words in the owner record and refresh it. */
void set_owner_pair_2d894(int16_t owner, uint32_t first, uint32_t second)
{
    ef_addr32 record;

    if ((int32_t)owner == 0xff)
        return;
    record = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)owner * 0xca);
    EF_U32(record + 0xc1u) = first;
    EF_U32(record + 0xc5u) = second;
    guest_fn_2da54((int32_t)owner);
}

/* 0x34a04, 52 bytes. */
void show_status_34a04(void)
{
    ef_addr32 widget = guest_fn_a46cc(0x00182e64u);

    if (widget != 0) {
        guest_fn_e1e6c(0x00130ef6u, 0);
        guest_fn_a4cb4(EF_U8(widget + 0x10u));
        EF_U8(0x0019f59du) = 1;
    }
}

/* 0x34a38, 45 bytes. */
void show_status_34a38(void)
{
    ef_addr32 widget = guest_fn_a46cc(0x00182e64u);

    if (widget != 0) {
        guest_fn_e1e6c(0x00130f14u, 0);
        guest_fn_a4cb4(EF_U8(widget + 0x10u));
    }
}

/* 0x34a68, 45 bytes. */
void show_status_34a68(void)
{
    ef_addr32 widget = guest_fn_a46cc(0x00182e64u);

    if (widget != 0) {
        guest_fn_e1e6c(0x00130f35u, 0);
        guest_fn_a4cb4(EF_U8(widget + 0x10u));
    }
}

/* 0x391d0, 52 bytes. */
void init_list_391d0(void)
{
    EF_U16(0x0040cd72u) = 0xffffu;
    EF_U16(0x0040cd74u) = 0xffffu;
    guest_fn_c36b4(0x001834f8u, 0, 0, 0x23, 0x1e, 0x0040c958u);
}

/* 0x7ae4, 81 bytes. Forward a command for a unit; kind 10 uses 0xff in place of its byte. */
void send_unit_command_7ae4(int16_t unit_id, uint8_t a, uint8_t b, uint8_t kind)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);
    uint32_t extra;

    if (unit == 0)
        return;
    if ((uint32_t)kind == 0xa)
        extra = 0xff;
    else
        extra = EF_U8(unit + 0x20bu);
    guest_fn_740e4((int32_t)unit_id, a, b, kind, extra);
    guest_fn_6a1d0((int32_t)unit_id, 0xb);
}
