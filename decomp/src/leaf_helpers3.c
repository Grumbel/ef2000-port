/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Call sequences and small helpers (45..75 bytes). */

/* 0xa60, 57 bytes. First key code with a pending request, else the default 0x3f. */
uint32_t find_pending_key_a60(void)
{
    uint32_t code;

    for (code = 0x80; code < 0xd0; code++)
        if (request_returns_cf_a00(code) != 0)
            return code;
    for (code = 0x60; code < 0x67; code++)
        if (request_returns_cf_a00(code) != 0)
            return code;
    return 0x3f;
}

/* 0x1f960, 50 bytes. Reset the scroll state. */
void reset_scroll_state_1f960(void)
{
    guest_fn_22e98_w(2, 0);
    EF_U32(0x001237f0u) = 0;
    EF_U32(0x001237f4u) = 0;
    EF_U16(0x001237eeu) = 0;
    EF_U16(0x001237ecu) = 0;
    EF_U16(0x0019b114u) = 0;
}

/* 0x7bc00, 61 bytes. Toggle a flag word, then run 0x7bc40 on a named widget. */
void toggle_named_widget_7bc00(void)
{
    EF_U16(0x001f8fd6u) = (uint16_t)(1 - EF_U16(0x001f8fd6u));
    guest_fn_c768_w(EF_S32(0x001f8fd4u) >> 16);
    guest_fn_7bc40_w(find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00187c90u), 0x00187c98u));
}

/* 0x844b0, 51 bytes. Refresh the view when a cursor offset is set, else reset it. */
uint8_t refresh_view_844b0(void)
{
    if (EF_U16(0x001fcbd0u) != 0 || EF_U16(0x001fcbd2u) != 0) {
        EF_U16(0x0012b4a6u) = 0;
        return guest_fn_843b0_w();
    }
    center_view_842b4();
    guest_fn_84370_w();
    return 0;
}

/* 0x9202c, 65 bytes. Recompute the two scene extents. */
void recompute_scene_extents_9202c(void)
{
    guest_fn_d099c(0x0021439cu, 0x0012dbecu);
    EF_U16(0x0012db98u) = (uint16_t)guest_fn_d13ac_w(0x0021439cu);
    EF_U16(0x0012db9au) = (uint16_t)guest_fn_d13dc_w(0x0021439cu, 0x0021439cu);
    guest_fn_d099c(0x0012dbecu, 0x0021439cu);
}

/* 0x9ab30, 57 bytes. Run the sound bank update chain on the object from 0x9a544. */
void update_sound_chain_9ab30(void)
{
    uint32_t obj = guest_fn_9a544_w();

    guest_fn_9ab6c_w(obj);
    guest_fn_9ac84_w(obj);
    guest_fn_9ae44_w(obj);
    guest_fn_9b04c_w(obj);
    guest_fn_ca5c_w(obj);
    guest_fn_9b2a4_w(obj);
    guest_fn_bc421_w(obj);
}

/* 0xa3f30, 67 bytes. Flush the pending byte buffer through 0xa3eac. */
void flush_pending_bytes_a3f30(void)
{
    uint16_t count = EF_U16(0x00136bd0u);

    if (count == 0)
        return;
    EF_U8(EF_U32(0x00136dccu) + count) = EF_U8(0x00135799u);
    issue_request_ready_a3eac(EF_U32(0x00136dccu), (uint16_t)(count + 1));
    EF_U16(0x00136bd0u) = 0;
    EF_U8(0x00136bd2u) = 0;
}

/* 0xabce8, 66 bytes. Refresh the named widget's page and draw its label. */
void refresh_named_widget_abce8(void)
{
    ef_addr32 widget = find_named_or_abort_a46cc(0x0018da8cu);

    if (widget == 0)
        return;
    EF_U8(0x0041f2e3u) = 0;
    guest_fn_a4b80(EF_U8(widget + 0x10u));
    guest_fn_e1e6c_3(widget, 0, (uint32_t)((uint8_t)(EF_U8(widget + 0xcbu) << 3) >> 4));
}

/* 0xdeb50, 49 bytes. Fill a block with a repeating four-byte pattern. */
void fill_pattern_deb50(ef_addr32 dst, uint32_t pattern, uint32_t count)
{
    uint32_t dword_count;

    if (count == 0)
        return;

    while ((dst & 3u) != 0) {
        EF_U8(dst) = (uint8_t)pattern;
        ++dst;
        pattern = (pattern >> 8) | (pattern << 24);
        if (--count == 0)
            return;
    }

    dword_count = count >> 2;
    guest_fn_deb87(dst, pattern, dword_count);
    dst += dword_count << 2;
    count &= 3u;

    if (count > 0)
        EF_U8(dst) = (uint8_t)pattern;
    if (count > 1)
        EF_U8(dst + 1u) = (uint8_t)(pattern >> 8);
    if (count > 2)
        EF_U8(dst + 2u) = (uint8_t)pattern;
}

/* 0xe474c, 56 bytes. Store two arguments and run the shutdown call chain. */
void store_args_and_shutdown_e474c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_a,
                                   uint32_t stack_b)
{
    (void)u0;
    (void)u1;
    (void)u2;
    (void)u3;
    EF_U32(0x004330d0u) = stack_a;
    EF_U32(0x004330d4u) = stack_b;
    guest_fn_e6990_w();
    guest_fn_109440_w();
    guest_fn_1098f0_w();
    guest_fn_109a80_w();
    guest_fn_109b20_w();
    guest_fn_d6090_w();
    guest_fn_e4340_w();
}
