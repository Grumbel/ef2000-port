/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Debug text overlay and version check, 0x283c8..0x404bc.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0x283c8, 289 bytes. Draw the splash buttons and probe the data file for its magic word. */
void check_data_file_283c8(void)
{
    uint8_t name[0x98];
    uint16_t magic = 0;
    ef_addr32 result = guest_fn_a7ec4(0x001822f8u);
    uint32_t handle;
    uint32_t value = 0;

    call_e9a24_c3888();
    call_e9a24_c3834();
    guest_fn_c3974();
    guest_fn_275a4();
    guest_fn_c39ac();
    guest_fn_c386c();
    guest_fn_dc250();
    fill_entries_a8060();
    guest_fn_27fec(0x00182308u, 0x00182300u, 0x78u, 0x50u);
    guest_fn_27fec(0x00182324u, 0x00182314u, 0x96u, 0x50u);
    value = guest_fn_27fec(0x00182338u, 0x00182330u, 0xb4u, 0x50u);
    EF_S32(result) = (int16_t)value;
    if (EF_U16(0x0015ee9au) == 0) {
        uint32_t text = guest_fn_9c858(0x001822d8u);

        format_ui_status_value((ef_addr32)(uintptr_t)name, 0x00182344u, text);
    } else {
        guest_fn_a4342_plain((ef_addr32)(uintptr_t)name, 0x00182350u);
    }
    handle = guest_fn_a7900((ef_addr32)(uintptr_t)name);
    if (handle != 0xffffffffu) {
        guest_fn_a79d4(handle, 2, (uint32_t)(uintptr_t)&magic);
        if ((int16_t)magic == 0x1736)
            EF_U32(result) = 1;
        guest_fn_a7990(handle);
    }
}

/* 0x404bc, 358 bytes. Draw seven debug counters, one per text row. */
void draw_debug_counters_404bc(void)
{
    uint8_t text[0x18];
    ef_addr32 buf = (ef_addr32)(uintptr_t)text;

    latch_values_ce328(0, EF_U32(0x0015b744u) - 1u, 0, EF_U32(0x0015b748u) - 1u);
    guest_fn_cdf48(0, EF_U32(0x0015b744u) - 1u, 0, EF_U32(0x0015b748u) - 1u);
    format_ui_status_value(buf, 0x00184270u, EF_U32(0x00135820u));
    guest_fn_48c90(0x0cu, 0x0au, 0x10u, buf);
    format_ui_status_value(buf, 0x00184278u, EF_U32(0x00135824u));
    guest_fn_48c90(0x0du, 0x0au, 0x20u, buf);
    format_ui_status_value(buf, 0x00184280u, EF_U32(0x00135828u));
    guest_fn_48c90(0x0eu, 0x0au, 0x30u, buf);
    format_ui_status_value(buf, 0x0018428cu, EF_U32(0x0013582cu));
    guest_fn_48c90(0x0fu, 0x0au, 0x40u, buf);
    format_ui_status_value(buf, 0x001842a0u, EF_U32(0x00135830u));
    guest_fn_48c90(0x10u, 0x0au, 0x50u, buf);
    format_ui_status_value(buf, 0x001842b4u, EF_U32(0x00135834u));
    guest_fn_48c90(0x11u, 0x0au, 0x60u, buf);
    format_ui_status_value(buf, 0x001842c4u, EF_U32(0x00135838u));
    guest_fn_48c90(0x12u, 0x0au, 0x70u, buf);
}
