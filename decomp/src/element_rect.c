/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Window record fields (words). */
#define WINDOW_SCREEN_X     0x41u
#define WINDOW_SCREEN_Y     0x43u
#define WINDOW_WIDTH        0x45u
#define WINDOW_HEIGHT       0x47u
#define WINDOW_SCROLL_X     0x69u
#define WINDOW_SCROLL_Y     0x6bu
#define WINDOW_VIEW_X       0x71u
#define WINDOW_VIEW_Y       0x73u
#define WINDOW_VIEW_WIDTH   0x75u
#define WINDOW_VIEW_HEIGHT  0x77u

/* Element record fields. */
#define ELEMENT_X           0x00u
#define ELEMENT_Y           0x02u
#define ELEMENT_WIDTH       0x04u
#define ELEMENT_HEIGHT      0x06u
#define ELEMENT_KIND        0x8cu    /* low five bits */
#define ELEMENT_KIND_MASK   0x1fu
#define ELEMENT_KIND_FRAME  0u       /* placed in the scrolled view */
#define ELEMENT_KIND_ROW    9u       /* spans the window less a 4-pixel border */

/* Clamp the span [*lo, *hi) to [clip, clip + size). */
static void clip_span(int16_t *lo, int16_t *hi, uint16_t clip, int16_t size)
{
    if ((int32_t)*lo - (int32_t)clip < 0)
        *lo = (int16_t)clip;
    if (!((int32_t)size > (int32_t)*hi - (int32_t)clip))
        *hi = (int16_t)(size + clip);
}

/* 0xe59cc, 562 bytes. EAX window, EDX element. Place the element in the
 * window: frame elements (kind 0) sit in the scrolled view and are clipped
 * to the view rectangle (+0x71..+0x77), others to the whole window. When
 * any part is visible the screen rectangle goes to g_element_rect and its
 * clipped part to g_element_clip; otherwise only the clip width is
 * cleared. EAX and EDX are clobbered. */
void place_window_element_e59cc(ef_addr32 window, ef_addr32 element)
{
    int16_t x0, y0, x1, y1;
    uint16_t clip_x, clip_y;
    int16_t clip_w, clip_h;

    g_element_clip_w = 0;
    if ((EF_U8(element + ELEMENT_KIND) & ELEMENT_KIND_MASK) == ELEMENT_KIND_FRAME) {
        x0 = (int16_t)(EF_U16(window + WINDOW_VIEW_X) - EF_U16(window + WINDOW_SCROLL_X));
        y0 = (int16_t)(EF_U16(window + WINDOW_VIEW_Y) - EF_U16(window + WINDOW_SCROLL_Y));
        clip_w = EF_S16(window + WINDOW_VIEW_WIDTH);
        clip_h = EF_S16(window + WINDOW_VIEW_HEIGHT);
        clip_x = EF_U16(window + WINDOW_VIEW_X);
        clip_y = EF_U16(window + WINDOW_VIEW_Y);
    } else {
        x0 = 0;
        y0 = 0;
        clip_w = EF_S16(window + WINDOW_WIDTH);
        clip_h = EF_S16(window + WINDOW_HEIGHT);
        clip_x = 0;
        clip_y = 0;
    }
    y0 = (int16_t)(y0 + EF_U16(element + ELEMENT_Y));
    x0 = (int16_t)(x0 + EF_U16(element + ELEMENT_X));
    if ((EF_U8(element + ELEMENT_KIND) & ELEMENT_KIND_MASK) == ELEMENT_KIND_ROW)
        x1 = (int16_t)(EF_U16(window + WINDOW_WIDTH) + x0 - 4);
    else
        x1 = (int16_t)(EF_U16(element + ELEMENT_WIDTH) + x0);
    y1 = (int16_t)(EF_U16(element + ELEMENT_HEIGHT) + y0);

    if ((int32_t)x0 - clip_x < 0 && (int32_t)x1 - clip_x < 0)
        return;
    if ((int32_t)y0 - clip_y < 0 && (int32_t)y1 - clip_y < 0)
        return;
    if ((int32_t)x0 - clip_x >= clip_w && (int32_t)x1 - clip_x >= clip_w)
        return;
    if ((int32_t)y0 - clip_y >= clip_h && (int32_t)y1 - clip_y >= clip_h)
        return;

    g_element_rect_x = (int16_t)(EF_U16(window + WINDOW_SCREEN_X) + x0);
    g_element_rect_y = (int16_t)(EF_U16(window + WINDOW_SCREEN_Y) + y0);
    g_element_rect_w = (int16_t)(x1 - x0);
    g_element_rect_h = (int16_t)(y1 - y0);
    clip_span(&x0, &x1, clip_x, clip_w);
    clip_span(&y0, &y1, clip_y, clip_h);
    g_element_clip_w = (int16_t)(x1 - x0);
    g_element_clip_h = (int16_t)(y1 - y0);
    g_element_clip_x = (int16_t)(EF_U16(window + WINDOW_SCREEN_X) + x0);
    g_element_clip_y = (int16_t)(EF_U16(window + WINDOW_SCREEN_Y) + y0);
}
