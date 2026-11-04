/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Handler table setup, flag merging and mode switching. */

/* 0xc448b, 191 bytes. Install the graphics dispatch pointers (including the patched code words). */
void install_graphics_dispatch_c448b(void)
{
    EF_U32(0x0015907cu) = 0;
    EF_U32(0x000c6438u) = 0x0015908au;
    EF_U32(0x000c6497u) = 0x00159138u;
    EF_U32(0x000c64e5u) = 0x00159138u;
    EF_U32(0x0015916cu) = 0x000d2584u;
    EF_U32(0x00159178u) = 0x000d2634u;
    EF_U32(0x00159170u) = 0x000d2584u;
    EF_U32(0x0015917cu) = 0x000d2634u;
    EF_U32(0x00159184u) = 0x000d2a7cu;
    EF_U32(0x00159188u) = 0x000d2d7cu;
    EF_U32(0x00159160u) = 0x000d2500u;
    EF_U32(0x00159164u) = 0x000d2500u;
    EF_U32(0x00159158u) = 0x000c64d0u;
    EF_U32(0x0015915cu) = 0x000c6482u;
    EF_U32(0x0015b434u) = 0x000c660cu;
    EF_U32(0x0015b438u) = 0x000c669cu;
    EF_U32(0x0015b43cu) = 0x000c672cu;
    EF_U32(0x0015b32cu) = 0x000c6d64u;
    EF_U32(0x0017bf70u) = 0x00164870u;
    guest_fn_d1f88_x();
}

/* 0xf75e0, 119 bytes. Merge the pending flag word into the active record, or clear it. */
void merge_pending_flags_f75e0(void)
{
    uint8_t mode = EF_U8(0x0041f2e2u);

    if (mode == 1) {
        EF_U16(EF_U32(0x0041eea0u) + 0xa9u) = (uint16_t)(EF_U16(EF_U32(0x0041eea0u) + 0xa9u) | EF_U16(0x0046438cu));
        return;
    }
    if (mode != 2)
        return;
    if (EF_U32(0x0046438cu) != 0xffffu) {
        uint16_t flags = EF_U16(0x0046438cu);
        ef_addr32 rec = EF_U32(0x0041ecccu);

        EF_U16(0x0041ec30u) = (uint16_t)(EF_U16(0x0041ec30u) | flags);
        EF_U16(rec + 8u) = (uint16_t)(EF_U16(rec + 8u) | flags);
        return;
    }
    EF_U16(0x0041ec30u) = 0;
    EF_U16(EF_U32(0x0041ecccu) + 8u) = 0;
}

static void run_exit_callback_f1c58(ef_addr32 obj)
{
    if (EF_U16(obj + 0xabu) != 0 && EF_U32(obj + 0xadu) != 0) {
        guest_fn_abb00_x(obj);
        ((void (*)(ef_addr32))EF_U32(obj + 0xadu))(obj);
    }
}

/* 0xf1c58, 249 bytes. Run the leave callback of the old object and the refresh steps of the new one. */
uint32_t switch_active_object_f1c58(ef_addr32 state)
{
    ef_addr32 old_obj = EF_U32(state + 8u);
    ef_addr32 obj = EF_U32(state + 4u);
    ef_addr32 extra;
    uint8_t proceed;

    if (old_obj != 0 && obj != old_obj)
        run_exit_callback_f1c58(old_obj);
    obj = EF_U32(state + 4u);
    if (obj == 0)
        return 0;
    extra = EF_U32(state + 0x10u);
    if (extra != 0) {
        proceed = (EF_U16(extra + 0x14u) == 0 && EF_U16(extra + 0x16u) == 0 && EF_U16(extra + 8u) == 0);
        if (!proceed)
            return 0;
    }
    obj = EF_U32(state + 4u);
    if ((EF_U8(obj + 0xa7u) & 0x10u) != 0 && (EF_U8(obj + 0xc4u) & 6u) == 2)
        guest_fn_a4b08_x();
    obj = EF_U32(state + 4u);
    if ((EF_U8(obj + 0xa8u) & 0x10u) != 0 && (EF_U8(obj + 0xc4u) & 6u) == 2)
        refresh_widget_a4b5c(obj);
    run_exit_callback_f1c58(EF_U32(state + 4u));
    return 0;
}

/* 0x10dc7f, 133 bytes. Switch the mode of one of 12 channels, running the per-channel transition. */
uint32_t set_channel_mode_10dc7f(int32_t channel, int32_t mode)
{
    if (channel < 1 || channel > 12) {
        guest_fn_ebe6d(9);
        return 3;
    }
    EF_U32(0x0017e5e0u) = 0x0010dc41u;
    if (channel == 4) {
        if (mode == 2)
            guest_fn_11ce30_x();
        else if (mode != 3)
            guest_fn_11d01c_x();
    } else if (channel == 7) {
        if (mode == 2)
            guest_fn_11cfe4_x();
        else if (mode != 3)
            guest_fn_11d135_x();
    } else if (channel == 2) {
        if (mode != 2 && mode != 3)
            guest_fn_11d24e_x();
    }
    {
        uint32_t old = EF_U32(0x0017d3ccu + (uint32_t)channel * 4u);

        EF_U32(0x0017d3ccu + (uint32_t)channel * 4u) = (uint32_t)mode;
        return old;
    }
}
