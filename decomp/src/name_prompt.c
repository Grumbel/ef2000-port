/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define SCAN_ESCAPE        0x01u
#define SCAN_BACKSPACE     0x0eu
#define SCAN_ENTER         0x1cu
#define SCAN_CTRL          0x1du
#define SCAN_LEFT_SHIFT    0x2au
#define SCAN_RIGHT_SHIFT   0x36u
#define SCAN_ALT           0x38u

#define PROMPT_MAX_CHARS   11
#define PROMPT_BUFFER      0x14     /* bytes; the last one is forced to 0 */
#define PROMPT_CENTRE_X    0xa0
#define PROMPT_BACKGROUND  0x0eu
#define PROMPT_TEXT        0xd7u
#define EMPTY_STRING       0x001820fcu

/* The keyboard interrupt stores each scancode in g_key_scancode. */
static uint8_t wait_for_scancode(void)
{
    g_key_scancode = 0;
    while (*(volatile uint8_t *)&g_key_scancode == 0)
        ;
    return *(volatile uint8_t *)&g_key_scancode;
}

static int is_modifier(uint8_t scan)
{
    return scan == SCAN_CTRL || scan == SCAN_ALT || scan == SCAN_LEFT_SHIFT || scan == SCAN_RIGHT_SHIFT;
}

static int is_name_char(uint8_t c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ';
}

static void draw_centred(int16_t y, ef_addr32 text)
{
    int32_t half = (int16_t)text_width_raw_dd650(text) / 2;

    draw_text_small_dd6a0(PROMPT_TEXT, (uint32_t)(int32_t)(int16_t)(PROMPT_CENTRE_X - half),
                          (uint32_t)(int32_t)y, text);
}

/* 0x27fec, 567 bytes. EAX key name, EDX default text, BX row, CX half
 * width; returns AX. Edit up to eleven upper-case letters, digits and
 * spaces on a box centred at column 160 of row Y. Enter accepts (an empty
 * entry takes the default), Esc restores the default, Backspace deletes.
 * The result is copied to the value of key NAME (0xa7f64) and drawn once
 * more; the return value is 1 when byte 0x41f7ad is set. */
uint16_t prompt_name_27fec(ef_addr32 key, ef_addr32 fallback, int32_t row, int32_t half_width)
{
    int16_t y = (int16_t)row;
    int16_t w = (int16_t)half_width;
    ef_addr32 target = find_key_checked_a7f64(key);
    uint8_t buffer[PROMPT_BUFFER + 4];
    ef_addr32 text = (ef_addr32)(uintptr_t)buffer;
    uint8_t editing = 1;
    int16_t length = 0;

    call_e9a24_c386c();
    fill_rect_ceccc(PROMPT_BACKGROUND, PROMPT_CENTRE_X - w, y - 2, PROMPT_CENTRE_X + w, y + 7);
    toggle_page_dc250();
    buffer[0] = EF_U8(EMPTY_STRING);
    do {
        uint8_t scan = wait_for_scancode();

        if (scan == SCAN_ENTER) {
            editing = 0;
            if (length == 0)
                copy_string_9d749(text, fallback);
        } else if (scan == SCAN_BACKSPACE) {
            if (length != 0)
                buffer[--length] = 0;
        } else if (scan == SCAN_ESCAPE) {
            copy_string_9d749(text, fallback);
            editing = 0;
        } else if (!is_modifier(scan) && length < PROMPT_MAX_CHARS) {
            uint8_t c = (uint8_t)to_upper_ascii(g_scancode_ascii[scan]);

            if (is_name_char(c)) {
                buffer[length++] = c;
                buffer[length] = 0;
            }
        }
        call_e9a24_c386c();
        fill_rect_ceccc(PROMPT_BACKGROUND, PROMPT_CENTRE_X - w, y - 2, PROMPT_CENTRE_X + w, y + 7);
        draw_centred(y, text);
        toggle_page_dc250();
    } while (editing);

    buffer[PROMPT_BUFFER] = 0;
    copy_string_9d749(target, text);
    select_buffer_146e40();
    draw_centred(y, text);
    leave_scroll_mode_c39ac();
    return EF_U8(0x0041f7adu) != 0;
}
