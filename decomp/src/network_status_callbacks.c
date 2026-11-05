/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

static uint32_t guest(uint32_t entry, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    return net_ui_guest_call_086(entry, eax, edx, ebx);
}

/* 33ad8, 129 bytes: populate four named widgets from modem settings and
 * a formatted description selected by byte 15f688. The format hook models
 * the original 72-byte stack scratch buffer. */
void net_open_loops_form_33ad8(ef_addr32 widget)
{
    uint32_t choice, result, text;
    result = guest(0xa78bcu, widget, 0x182c70u, 0u);
    guest(0xa7588u, result, 0x15f4fdu, 0u);
    choice = EF_U8(0x15f688u);
    text = net_ui_format_option_086(0x182bc0u,
                                    EF_U32(0x125958u + 4u * choice));
    result = guest(0xa78bcu, widget, 0x182c78u, 0u);
    guest(0xa7588u, result, text, 0u);
    result = guest(0xa78bcu, widget, 0x182c80u, 0u);
    guest(0xa7588u, result, 0x15f53du, 0u);
    result = guest(0xa78bcu, widget, 0x182c8cu, 0u);
    guest(0xa7588u, result, 0x15f57du, 0u);
}

/* 341d8, 62 bytes: walk 0x21-linked widget nodes, copying the name from
 * consecutive 64-byte DirectPlay rows, starting at 4288f0. The iteration
 * limit is signed, exactly as the original JL comparison. */
void net_directplay_game_selected_341d8(ef_addr32 widget)
{
    ef_addr32 child = guest(0xa78bcu, widget, 0x182d94u, 0x4288f0u);
    int32_t i;
    int32_t count = (int32_t)EF_U32(0x429210u);
    for (i = 0; i < count; ++i) {
        guest(0xa7588u, child, 0x4288f0u + (uint32_t)i * 64u, 0u);
        child = EF_U32(child + 0x21u);
    }
}

/* 34480, 29 bytes: reset server socket before the arcade game starts. */
void net_start_arcade_34480(void)
{
    EF_U16(0x1357b8u) = 0u;
    guest(0x34bccu, 0u, 0u, 0u);
    guest(0xa2bdcu, 0u, 0u, 0u);
    guest(0x33630u, 0u, 0u, 0u);
}

/* 344a0, 27 bytes: invert the active network channel flag. */
void net_new_channel_344a0(void)
{
    guest(0xa3da0u, 0u, 0u, 0u);
    guest(0x34ea4u, 0u, 0u, 0u);
    EF_U8(0x1357c8u) ^= 1u;
    guest(0xa2bdcu, 0u, 0u, 0u);
    guest(0x33630u, 0u, 0u, 0u);
}

/* 345d4, 19 bytes: false shares 345d2 RET in the previous function;
 * true tail-jumps to a4d90 with the modem-ending dialog name. */
void net_new_end_string_345d4(void)
{
    if (EF_U8(0x13578cu) != 0u)
        guest(0xa4d90u, 0x182e10u, 0u, 0u);
}

/* 347ac, 13 bytes: AH is discarded, only zero-extended widget ID remains. */
void net_open_loops_347ac(ef_addr32 widget)
{
    guest(0xa4b80u, EF_U8(widget + 0x10u), 0u, 0u);
}

/* 349ec, 23 bytes: retain resolved widget and refresh network loop UI. */
void net_main_loop_349ec(ef_addr32 widget)
{
    EF_U32(0x19f320u) = guest(0xa78bcu, widget, 0x182e60u, 0u);
    guest(0x348e4u, 0u, 0u, 0u);
}

/* 35128, 27 bytes: staged network-initialization transition. */
void net_open_initialise_35128(void)
{
    EF_U8(0x19f59bu) = 0xffu;
    guest(0xa3e04u, 0u, 0u, 0u);
    guest(0x34ea4u, 0u, 0u, 0u);
    guest(0xac7a8u, 0x182f24u, 0u, 0u);
}

/* 35e68 and 35e90 are distinct named callbacks with identical 37-byte
 * bodies. Both connect the status widget, clear its editable state and
 * forward two zero-valued stack arguments to the RX status routine. */
static void open_rx_status(ef_addr32 widget)
{
    guest(0xa75b4u, widget, 0x182fa8u, 0u);
    guest(0xa7434u, widget, 0u, 0u);
    guest(0x35de0u, 0u, 0u, 0u);
}

void net_open_rx_status_35e68(ef_addr32 widget)
{
    open_rx_status(widget);
}

void net_set_nonentry_flag_35e90(ef_addr32 widget)
{
    open_rx_status(widget);
}

/* 35eb8, 75 bytes: the two display options are complementary and tied to
 * the live network-type flag at 13578d, not to a cached UI checkbox. */
void net_new_type_35eb8(ef_addr32 widget)
{
    uint32_t enabled = EF_U8(0x13578du) != 0u;
    guest(0xa73ccu, widget, 0x183114u, enabled);
    guest(0xa73ccu, widget, 0x18311cu, enabled ^ 1u);
}
