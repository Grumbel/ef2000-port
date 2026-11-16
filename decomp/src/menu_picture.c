/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Byte arrays describe the unaligned word and pointer without host padding. */
typedef struct {
    uint8_t name[9];
    uint8_t fade_in, fade_out;
    uint8_t duration[2];
    uint8_t has_text;
    uint8_t text_record[4];
} ef_menu_picture_prefix;

EF_STATIC_ASSERT(menu_picture_name, offsetof(ef_menu_picture_prefix, name) == 0);
EF_STATIC_ASSERT(menu_picture_fade_in, offsetof(ef_menu_picture_prefix, fade_in) == 9);
EF_STATIC_ASSERT(menu_picture_fade_out, offsetof(ef_menu_picture_prefix, fade_out) == 0x0a);
EF_STATIC_ASSERT(menu_picture_duration, offsetof(ef_menu_picture_prefix, duration) == 0x0b);
EF_STATIC_ASSERT(menu_picture_has_text, offsetof(ef_menu_picture_prefix, has_text) == 0x0d);
EF_STATIC_ASSERT(menu_picture_text_record, offsetof(ef_menu_picture_prefix, text_record) == 0x0e);

/* 0xa8a38, 411 bytes. EAX=picture record; all other GPRs preserved.
 * The sole direct caller is GUI opcode 1 at a9f59/a9f5c. No EAX result.
 * The original local byte is zero throughout: its panel reset/release
 * branches never execute. Do not introduce those side effects here. */
void show_menu_picture_a8a38(ef_addr32 record)
{
    uint32_t remaining;
    uint8_t mode;

    EF_U8(0x00137205u) = 0u;
    if (EF_U8(0x001371eeu) != 0u)
        return;
    if (EF_U8(record + 0x0du) != 0u &&
        EF_U8(EF_U32(record + 0x0eu) + 0x0fu) == 0u)
        prepare_scrolling_text_a82f0(EF_U32(record + 0x0eu));

    select_buffer_146e40();
    copy_string_9d749(EF_U32(0x00417254u), record);
    strcat_9ca57(EF_U32(0x00417254u), 0x0018d6a8u); /* ".lbm" */
    (void)start_driver_c34cc(EF_U32(0x00417254u));
    leave_scroll_mode_c39ac();
    call_e9a24_c386c();
    if (EF_U8(record + 0x0du) != 0u &&
        EF_U8(EF_U32(record + 0x0eu) + 0x0fu) == 0u)
        draw_scrolling_text_a8718();
    if (EF_U8(record + 9u) != 0u)
        set_black_palette_d65e8();
    toggle_page_dc250();
    mode = EF_U8(record + 9u);
    if (mode == 1u)
        fade_palette_in_de580(0x00425c8cu);
    else if (mode == 2u)
        fade_palette_in_slow_de5f4(0x00425c8cu);
    else
        apply_palette_d663c(0x00425c8cu);

    if (EF_U8(record + 0x0du) != 0u &&
        EF_U8(EF_U32(record + 0x0eu) + 0x0fu) != 0u)
        prepare_scrolling_text_a82f0(EF_U32(record + 0x0eu));
    remaining = (uint32_t)(EF_S32(record + 9u) >> 16);
    do {
        if (EF_U8(record + 0x0du) != 0u &&
            EF_U8(EF_U32(record + 0x0eu) + 0x0fu) != 0u) {
            call_e9a24_c386c();
            draw_scrolling_text_a8718();
            toggle_page_dc250();
        }
        wait_vsync_dc2e0();
        if (EF_U8(record + 0x0du) == 0u) {
            remaining--;
        } else if (EF_U8(EF_U32(record + 0x0eu) + 0x0fu) != 0u &&
                   EF_S16(0x00425fa6u) >= EF_S16(0x004194aeu)) {
            remaining--;
        } else if (EF_U8(EF_U32(record + 0x0eu) + 0x0fu) == 0u) {
            remaining--;
        }
        if ((int32_t)remaining <= 0)
            break;
    } while (handle_key_a7be8(0u) == 0u);

    wait_for_key_a89f0();
    mode = EF_U8(record + 0x0au);
    if (mode == 1u)
        fade_palette_out_de648(0x00425c8cu);
    else if (mode == 2u)
        fade_palette_out_slow_de6c4(0x00425c8cu);
}
