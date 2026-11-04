/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* File status probes, widget label refresh and camera table setup (95..140 bytes). */

/* 0xa2948, 131 bytes. Probe the data file; returns a status code (8, 9, 0xc or 0). */
uint8_t probe_data_file_a2948(uint8_t key)
{
    uint8_t probe;

    EF_U8(0x0013579au) = 0;
    probe = (uint8_t)guest_fn_e94ec(0x0018cc18u, 0);
    if (probe == 1) {
        EF_U8(0x0013578eu) = 8;
        return 8;
    }
    if (probe == 2) {
        EF_U8(0x0013578eu) = 9;
        return 9;
    }
    if (guest_fn_e93dc_r(0x00136fb4u, key) == 0) {
        release_buffer_e9574();
        return 0xc;
    }
    release_buffer_e9574();
    EF_U8(0x001357bau) = 1;
    if (EF_U16(0x001357b4u) > 8)
        EF_U16(0x001357b4u) = 8;
    return 0;
}

/* 0xabc34, 98 bytes. Re-label the named widget when its owner record changed. */
void relabel_named_widget_abc34(void)
{
    ef_addr32 owner = 0x0041f284u;
    ef_addr32 widget;

    if (EF_U16(0x0041f290u) != 0 && EF_U32(EF_U32(0x0041f294u) + 0x95u) != 0) {
        owner = EF_U32(EF_U32(0x0041f294u) + 0x95u);
    } else if (EF_U16(owner) != 0) {
        owner = EF_U32(EF_U32(owner + 4u) + 0xccu);
    } else {
        owner = 0;
    }
    if (owner == EF_U32(0x0041ed7cu))
        return;
    EF_U32(0x0041ed7cu) = owner;
    widget = find_named_or_abort_a46cc(0x0018da8cu);
    if (widget != 0)
        guest_fn_e1e6c_3(widget, owner, 4);
}

/* 0xabc98, 78 bytes. Draw the named widget's label and activate its page. */
void draw_named_widget_abc98(void)
{
    ef_addr32 widget = find_named_or_abort_a46cc(0x0018da8cu);

    if (widget == 0) {
        EF_U8(0x0041f2e3u) = 0;
        return;
    }
    EF_U8(0x0041f2e3u) = 1;
    guest_fn_e1e6c_3(widget, 0, (uint32_t)((uint8_t)(EF_U8(widget + 0xcbu) << 3) >> 4));
    activate_widget_a4cb4(EF_U8(widget + 0x10u));
}

/* 0xd7b6c, 129 bytes. Initialise the camera table from the mode handle, or copy the default. */
void init_camera_table_d7b6c(void)
{
    if (EF_U16(0x0015bb3eu) == 0xffff) {
        EF_U32(0x0015bb30u) = EF_U32(0x0015bb34u);
        return;
    }
    EF_U32(0x00159080u) = guest_fn_cd7f0(EF_S32(0x0015bb3cu) >> 16);
    EF_U16(0x00420e80u) = 4;
    EF_U16(0x00420e8eu) = 0;
    EF_U16(0x00420e90u) = 0;
    EF_U16(0x00420e8au) = 0xff;
    EF_U16(0x00420e8cu) = 0;
    EF_U16(0x00420e86u) = 0xff;
    EF_U16(0x00420e82u) = 0;
    EF_U16(0x00420e88u) = 9;
    EF_U16(0x00420e84u) = 9;
}

/* 0xd9c30, 129 bytes. Open the stream in either format; 0 on success, 1 on failure. */
uint32_t open_stream_formats_d9c30(void)
{
    uint32_t h;

    EF_U32(0x00420f00u) = 0;
    h = guest_fn_106be8_w(1, 0, 0x4b00);
    EF_U8(0x00420f54u) = guest_fn_107004_w(0x00420f00u, 0x00420f55u, 0x4b00, h);
    if (EF_U8(0x00420f54u) != 0) {
        guest_fn_10703c_w(0x00420f00u);
        return 0;
    }
    h = guest_fn_106be8_w(2, 0x0018fad4u, 0x4b00);
    EF_U8(0x00420f54u) = guest_fn_106e2c_w(0x00420f00u, 0x0018fad4u, h);
    if (EF_U8(0x00420f54u) != 0) {
        guest_fn_10703c_w(0x00420f00u);
        return 0;
    }
    return 1;
}

/* 0xd9dc0, 139 bytes. Read the stream header and latch the heading and position values. */
void read_stream_header_d9dc0(void)
{
    int32_t delta;
    int32_t rem;

    EF_U8(0x00420f54u) = guest_fn_107070_w(0x00420f00u, guest_fn_106be8_w(1, 0, 0));
    if (EF_U8(0x00420f54u) != 0)
        EF_U8(0x00420f54u) = guest_fn_106d34_w(0x00420f00u, guest_fn_106bfc_w(1));
    guest_fn_107060_w(0x00420f00u);
    EF_U32(0x0015bb4cu) = EF_U32(0x00420f18u);
    delta = (int32_t)EF_U32(0x00420f18u) - 0x1554;
    rem = delta % 0xffff;
    if (rem < 0)
        rem = -rem;
    EF_U32(0x0015bb4cu) = (uint32_t)rem;
    EF_U32(0x0015bb50u) = EF_U32(0x00420f1cu);
    EF_U32(0x0015bb54u) = EF_U32(0x00420f20u);
}

/* 0xe5088, 139 bytes. Draw the active unit's marker, then flag it as refreshed. */
void draw_active_marker_e5088(void)
{
    ef_addr32 unit = EF_U32(0x0041eea0u);

    if (unit == 0)
        return;
    if (EF_U32(unit + 0x95u) != 0) {
        guest_fn_e4c9c_w(unit);
    } else if (EF_U8(unit + 0xc4u) & 1) {
        guest_fn_c91c8_w(EF_U8(unit + 0xc2u), EF_U16(0x0041eef6u), EF_U16(0x0041eef8u),
                         (uint16_t)(EF_U16(0x0041eef6u) + EF_U16(0x0041eefau)));
    }
    EF_U16(EF_U32(0x0041eea0u) + 0xdau) = 1;
}
