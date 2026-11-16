/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint8_t name[9], fade_in, fade_out, interval[2], playback_mode;
    uint8_t duration[2], reserved, has_text, key_modifier, text_record[4];
} ef_menu_animation_prefix;

EF_STATIC_ASSERT(menu_animation_name, offsetof(ef_menu_animation_prefix, name) == 0);
EF_STATIC_ASSERT(menu_animation_fade_in, offsetof(ef_menu_animation_prefix, fade_in) == 9);
EF_STATIC_ASSERT(menu_animation_fade_out, offsetof(ef_menu_animation_prefix, fade_out) == 0x0a);
EF_STATIC_ASSERT(menu_animation_interval, offsetof(ef_menu_animation_prefix, interval) == 0x0b);
EF_STATIC_ASSERT(menu_animation_mode, offsetof(ef_menu_animation_prefix, playback_mode) == 0x0d);
EF_STATIC_ASSERT(menu_animation_duration, offsetof(ef_menu_animation_prefix, duration) == 0x0e);
EF_STATIC_ASSERT(menu_animation_has_text, offsetof(ef_menu_animation_prefix, has_text) == 0x11);
EF_STATIC_ASSERT(menu_animation_key_modifier, offsetof(ef_menu_animation_prefix, key_modifier) == 0x12);
EF_STATIC_ASSERT(menu_animation_text_record, offsetof(ef_menu_animation_prefix, text_record) == 0x13);
EF_STATIC_ASSERT(menu_animation_record_size, sizeof(ef_menu_animation_prefix) == 0x17);

typedef struct {
    uint8_t preceding[0x12];
    uint16_t length;
    ef_addr32 text;
} ef_menu_animation_text_prefix;

EF_STATIC_ASSERT(menu_animation_text_length, offsetof(ef_menu_animation_text_prefix, length) == 0x12);
EF_STATIC_ASSERT(menu_animation_text_pointer, offsetof(ef_menu_animation_text_prefix, text) == 0x14);
EF_STATIC_ASSERT(menu_animation_text_size, sizeof(ef_menu_animation_text_prefix) == 0x18);

typedef struct {
    ef_addr32 text, text_copy, text_record;
    uint8_t waiting, reserved[3], audio, padding[3];
} ef_menu_animation_frame;

EF_STATIC_ASSERT(menu_animation_frame_text, offsetof(ef_menu_animation_frame, text) == 0);
EF_STATIC_ASSERT(menu_animation_frame_text_copy, offsetof(ef_menu_animation_frame, text_copy) == 4);
EF_STATIC_ASSERT(menu_animation_frame_text_record, offsetof(ef_menu_animation_frame, text_record) == 8);
EF_STATIC_ASSERT(menu_animation_frame_waiting, offsetof(ef_menu_animation_frame, waiting) == 0x0c);
EF_STATIC_ASSERT(menu_animation_frame_audio, offsetof(ef_menu_animation_frame, audio) == 0x10);
EF_STATIC_ASSERT(menu_animation_frame_size, sizeof(ef_menu_animation_frame) == 0x14);

/* The original two MOVSDs and MOVSW use DS:ESI -> ES:EDI and the live DF.
 * Do not replace their segment semantics with a flat C copy or insert CLD. */
#ifdef __WATCOMC__
static void menu_animation_copy_effect(ef_addr32 destination);
#pragma aux menu_animation_copy_effect = \
    "mov esi,0x0018d6b8" "movsd" "movsd" "movsw" \
    parm [edi] modify [esi edi]
#else
extern void menu_animation_copy_effect(ef_addr32 destination);
#endif

static void draw_menu_animation_frame(ef_addr32 record)
{
    call_e9a24_c386c();
    if (EF_U8(record + 0x11u) != 0u)
        draw_scrolling_text_a8718();
    else
        EF_U16(0x00425fa6u) = EF_U16(0x004194aeu);
    if (EF_U8(record + 0x11u) != 0u &&
        EF_U8(EF_U32(record + 0x13u) + 0x0fu) == 0u)
        EF_U16(0x00425fa6u) = EF_U16(0x004194aeu);
    toggle_page_dc250();
}

static void fade_menu_animation_out(ef_addr32 record)
{
    uint8_t mode = EF_U8(record + 0x0au);

    if (mode == 1u)
        fade_palette_out_de648(0x00425c8cu);
    else if (mode == 2u)
        fade_palette_out_slow_de6c4(0x00425c8cu);
}

/* 0xa8bd4, 937 bytes. EAX=record, all other GPRs preserved, plain RET.
 * GUI opcode 0 at a9f4c/a9f4f is the sole direct caller; EAX result unused. */
void show_menu_animation_a8bd4(ef_addr32 source)
{
    ef_menu_animation_frame frame;
    ef_addr32 local = (ef_addr32)(uintptr_t)&frame;
    ef_addr32 record;
    uint8_t mode;

    EF_U8(local + 0x10u) = 0u;
    if (guest_strcmp(source, 0x0018d6b0u) != 0 && (int32_t)result_f2800() < 5)
        goto restore_mode;
    switch_display_mode_d1e50(1u);
    EF_U8(0x00137205u) = 0u;
    /* Original early return deliberately bypasses the mode-2 restore. */
    if (EF_U8(0x001371eeu) != 0u)
        return;

    record = tail_jump_c3f70(0x17u);
    memcpy_dc832(record, source, 0x17u);
    if (EF_U8(source + 0x11u) != 0u) {
        ef_addr32 text_record = tail_jump_c3f70(0x18u);
        ef_addr32 text;
        ef_addr32 original_text;

        EF_U32(local + 8u) = text_record;
        memcpy_dc832(text_record, EF_U32(source + 0x13u), 0x18u);
        text = tail_jump_c3f70(EF_U16(text_record + 0x12u));
        EF_U32(local + 4u) = text;
        EF_U32(local) = text;
        original_text = EF_U32(source + 0x13u);
        memcpy_dc832(text, EF_U32(original_text + 0x14u), EF_U16(original_text + 0x12u));
        EF_U32(record + 0x13u) = text_record;
        EF_U32(text_record + 0x14u) = EF_U32(local + 4u);
        if (EF_U8(0x0015bac6u) != 0u)
            EF_U8(local + 0x10u) = 1u;
    }
    if (EF_U8(record + 0x11u) != 0u &&
        EF_U8(EF_U32(record + 0x13u) + 0x0fu) == 0u)
        prepare_scrolling_text_a82f0(EF_U32(record + 0x13u));
    copy_string_9d749(EF_U32(0x00417254u), record);
    menu_animation_copy_effect(EF_U32(0x00417260u));
    guest_sprintf_a4342(EF_U32(0x00417258u), 0x0018d6c4u, EF_S32(record + 9u) >> 16);
    if (EF_U8(record + 9u) != 0u)
        set_black_palette_d65e8();
    if (EF_U8(local + 0x10u) != 0u)
        init_sound_entries_80374(0x0018d6c8u);
    if (EF_U8(0x0012b0f4u) == 0u && EF_U8(local + 0x10u) != 0u)
        EF_U8(local + 0x10u) = 0u;

    if (initialize_data_playback_eac48(7, 0x00417250u) == 0u) {
        if (EF_U8(record + 0x11u) != 0u &&
            EF_U8(EF_U32(record + 0x13u) + 0x0fu) == 0u) {
            draw_scrolling_text_a8718();
            toggle_page_dc250();
        }
        mode = EF_U8(record + 9u);
        if (mode == 1u)
            fade_palette_in_de580(0x00425c8cu);
        else if (mode == 2u)
            fade_palette_in_slow_de5f4(0x00425c8cu);
        else
            apply_palette_d663c(0x00425c8cu);
        if (EF_U8(local + 0x10u) != 0u)
            ui_reset_active_panel();
        if (EF_U8(record + 0x11u) != 0u &&
            EF_U8(EF_U32(record + 0x13u) + 0x0fu) != 0u)
            prepare_scrolling_text_a82f0(EF_U32(record + 0x13u));

        EF_U8(local + 0x0cu) = 1u;
        if (EF_U8(record + 0x0du) == 1u) {
            uint32_t pending = 1u;

            do {
                if (pending != 0u)
                    pending = pump_records_eab44();
                draw_menu_animation_frame(record);
                /* Both original audio branches have this same test. */
                if (pending == 0u && EF_S16(0x00425fa6u) >= EF_S16(0x004194aeu))
                    EF_U8(local + 0x0cu) = 0u;
                if (EF_U8(local + 0x0cu) == 0u)
                    break;
            } while (handle_key_a7be8(EF_U8(record + 0x12u)) == 0u);
            wait_for_key_a89f0();
            wait_for_event_803e8();
            release_pending_handle_eae10();
            fade_menu_animation_out(record);
        } else {
            uint32_t remaining = (uint32_t)(EF_S32(record + 0x0cu) >> 16);

            do {
                (void)pump_records_eab44();
                draw_menu_animation_frame(record);
                remaining--;
                if (EF_U8(local + 0x10u) != 0u) {
                    if (EF_S16(0x00425fa6u) >= EF_S16(0x004194aeu))
                        remaining = 0u;
                    else if (remaining == 0u)
                        remaining = 1u;
                }
                if ((int32_t)remaining <= 0)
                    break;
            } while (handle_key_a7be8(EF_U8(record + 0x12u)) == 0u);
            wait_for_key_a89f0();
            release_pending_handle_eae10();
            fade_menu_animation_out(record);
        }
    }
    if (EF_U8(local + 0x10u) != 0u)
        ui_reset_panel_and_release();
    if (EF_U8(record + 0x11u) != 0u) {
        tail_jump_c3f80(EF_U32(local));
        tail_jump_c3f80(EF_U32(local + 8u));
    }
    tail_jump_c3f80(record);
restore_mode:
    switch_display_mode_d1e50(2u);
}
