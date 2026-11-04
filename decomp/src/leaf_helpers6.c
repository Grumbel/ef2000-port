/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Settings load, page widgets and shutdown waits (140..190 bytes). */

/* 0x39254, 168 bytes. Load the numeric settings from their key names. */
void load_settings_39254(void)
{
    EF_U32(lookup_key_a7ec4(0x00183510u)) = 0;
    EF_U16(0x001a3600u) = (uint16_t)guest_fn_9ddfc_n(EF_U32(0x001a58f8u));
    if (EF_U8(0x001a5a9du) != 0)
        EF_U32(0x001a5bf0u) = 0xfffffffdu;
    EF_U8(0x0012afbdu) = 0;
    EF_U8(0x0012afbcu) = 0;
    EF_U16(0x001a35f8u) = (uint16_t)guest_fn_9ddfc_n(0x00183518u);
    EF_U16(0x001a17aeu) = (uint16_t)guest_fn_9ddfc_n(0x0018351cu);
    EF_U16(0x001a17acu) = (uint16_t)guest_fn_9ddfc_n(0x00183524u);
    EF_U16(0x001a17b6u) = (uint16_t)guest_fn_9ddfc_n(0x0018352cu);
    EF_U16(0x0019f30au) = (uint16_t)guest_fn_9ddfc_n(0x00183534u);
    guest_fn_68a28_w();
    if (EF_U8(0x001a364du) == 0)
        guest_fn_39204_w();
}

/* 0x42f7c, 163 bytes. Point the page widgets at the current selection. */
void bind_page_widgets_42f7c(void)
{
    ef_addr32 widget = find_named_or_abort_a46cc(0x00184558u);
    ef_addr32 child;
    uint32_t childName = 0;

    EF_U32(widget + 0x95u) = EF_U32(0x0041eeacu);
    child = find_widget_checked_a78bc(widget, 0x00184560u);
    EF_U32(child + 0x9au) = EF_U32(0x0041eeacu);
    guest_fn_a5ef8(EF_U8(widget + 0x10u));
    if (EF_U16(0x00137998u) != 0) {
        widget = find_named_or_abort_a46cc(0x00184540u);
        childName = 0x00184568u;
    } else if (EF_U16(0x0013799au) != 0) {
        widget = find_named_or_abort_a46cc(0x00184548u);
        childName = 0x00184570u;
    }
    if (childName != 0) {
        EF_U32(widget + 0x95u) = EF_U32(0x0041eeacu);
        child = find_widget_checked_a78bc(widget, childName);
        EF_U32(child + 0x9au) = EF_U32(0x0041eeacu);
        guest_fn_a5ef8(EF_U8(widget + 0x10u));
    }
}

/* 0x5cff0, 141 bytes. Redraw the panel frame, then the selection or the help text. */
void redraw_panel_5cff0(void)
{
    guest_fn_ce328_w(EF_S32(0x001a6cf0u) >> 16, EF_S32(0x001a6cf6u) >> 16, EF_S32(0x001a6ceeu) >> 16,
                     EF_S32(0x001a6cf4u) >> 16);
    guest_fn_cdf48(EF_S32(0x001a6cf0u) >> 16, EF_S32(0x001a6cf6u) >> 16, EF_S32(0x001a6ceeu) >> 16,
                   EF_S32(0x001a6cf4u) >> 16);
    if (EF_U8(0x001a6d19u) != 0)
        guest_fn_5d48c_w(EF_S32(0x001a6cd6u) >> 16, EF_S32(0x001a6cd4u) >> 16);
    else if (EF_U8(0x001a6d1au) != 0)
        guest_fn_5d080_w(0x001a6b0cu);
}

/* 0x82cec, 183 bytes. Apply the saved options and refresh the display flags. */
void apply_options_82cec(void)
{
    guest_fn_9cfdc_w(EF_U32(0x0012ddb0u));
    EF_U8(0x001371efu) = EF_U8(0x0012ddb4u);
    guest_fn_d6694();
    EF_S32(0x001fcba8u) = EF_S32(0x00420d9au) >> 16;
    EF_S32(0x001fcba4u) = EF_S32(0x00420d98u) >> 16;
    if ((uint8_t)guest_fn_e94ec(EF_U32(0x0012b36cu), 0) == 0) {
        guest_fn_e93dc_r(0x0012b370u, 0);
        release_buffer_e9574();
    }
    EF_U32(0x0015bab0u) = 0x3000;
    if (EF_U8(0x00198466u) & 2)
        EF_U32(0x0015bab0u) = 0xb000;
    if (EF_U8(0x00198466u) & 0x10)
        EF_U8(0x0015bab1u) |= 0x80;
    if (EF_U8(0x00198466u) & 4)
        EF_U8(0x0015bab1u) |= 0x40;
    guest_fn_30214_w();
    guest_fn_2ff44_w();
    guest_fn_f80_w();
}

/* 0xa8958, 149 bytes. Run the animation loop of a list record until a key press. */
void run_animation_loop_a8958(ef_addr32 record)
{
    ef_addr32 owner;
    int32_t count;

    if (EF_U8(0x001371eeu) != 0)
        return;
    guest_fn_f2660_w(0x0018d698u);
    owner = EF_U32(record + 5u);
    guest_fn_a82f0_w(owner);
    guest_fn_dc2c0();
    guest_fn_a8718_w(owner);
    guest_fn_dc250();
    guest_fn_de580(EF_U8(record + 2u) == 1 ? 0x00160e44u : 0x00464060u);
    count = EF_S32(record + 1u) >> 16;
    for (;;) {
        if (EF_U8(EF_U32(record + 5u) + 0xfu) != 0) {
            guest_fn_dc2c0();
            guest_fn_a8718_w(EF_U32(record + 5u));
            guest_fn_dc250();
        }
        count--;
        guest_fn_dc2e0();
        if (count == -1)
            break;
        if (guest_fn_a7be8_w(0) != 0)
            break;
    }
    guest_fn_de648_w(0x00464060u);
}

/* 0xcd554, 146 bytes. Drain the pending key table, release the buffers and restore the page. */
void restore_page_cd554(void)
{
    uint32_t spin;
    uint32_t i;
    uint32_t pending;

    if (EF_U8(0x0015b72eu) == 0 || EF_U8(0x0015b72eu) == 2)
        return;
    for (spin = 0; spin < 100000u; spin++) {
        pending = 0;
        for (i = 1; i < 0x80; i++)
            if (EF_U8(0x0041f790u + i) != 0)
                pending++;
        if (pending == 0)
            break;
    }
    guest_fn_6b8_w(EF_U32(0x0041f828u));
    guest_fn_6b8_w(EF_U32(0x0041f82cu));
    guest_fn_7b4_w(EF_U32(0x0041f824u));
    guest_fn_cd414_w((EF_U8(EF_U32(0x00120cc6u) + 0x417u) >> 4) & 7);
    EF_U8(0x0015b72eu) = 2;
}
