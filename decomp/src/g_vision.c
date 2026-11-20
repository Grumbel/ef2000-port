/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

#define LOAD_FLOOR        (-4.0f)  /* g_load_factor is clamped to -4..11 */
#define LOAD_CEILING      11
#define LOAD_NEGATIVE_OK  (-2)     /* no stress between -2 and 9 */
#define LOAD_POSITIVE_OK  9
#define STRESS_RATE       0.2f
#define STRESS_SCALE      54.0f
#define RECOVER_RED       0.25f    /* per tick step, negative stress */
#define RECOVER_BLACK     0.5f     /* per tick step, positive stress */
#define STRESS_LIMIT      50.0f
#define BLACK_LEVEL_TOP   49.0f
#define RED_LEVEL_MIN     15

#define FADE_BLACK        0        /* g_vision_fade indices */
#define FADE_RED          1

enum { VISION_CLEAR = 0, VISION_RED = 1, VISION_BLACK = 2 };

static float load_stress(double load)
{
    float g = (float)load;

    if (load < (double)LOAD_FLOOR)
        g = LOAD_FLOOR;
    if (g > (float)LOAD_CEILING)
        g = (float)LOAD_CEILING;
    if ((float)LOAD_NEGATIVE_OK > g)
        return (float)(((double)g - (double)LOAD_NEGATIVE_OK) * (double)STRESS_RATE * (double)STRESS_SCALE);
    if (g > (float)LOAD_POSITIVE_OK)
        return (float)(((double)g - (double)LOAD_POSITIVE_OK) * (double)STRESS_RATE * (double)STRESS_SCALE);
    return 0.0f;
}

/* 0x75ca0, 608 bytes. Update the G-load vision effect: loads below -2 or
 * above 9 (clamped to -4..11) add stress, which recovers toward zero by
 * the tick step (0.25 per step while negative, 0.5 while positive) and is
 * held within -50..50. Negative stress is a red-out (level stress + 50,
 * at least 15), positive stress a black-out (level 49 - stress, at least
 * 0); g_vision_fade_mode says which, and g_vision_tint_changed is set
 * when either level changes. */
void update_g_vision_75ca0(void)
{
    int16_t old_red = g_vision_fade[FADE_RED];
    int16_t old_black = g_vision_fade[FADE_BLACK];
    double step = (double)(int32_t)(uint32_t)g_simulation_tick_step;
    float load = load_stress(g_load_factor);
    double sum;

    if (g_g_stress < 0.0f) {
        g_g_stress = (float)(step * (double)RECOVER_RED + (double)g_g_stress);
        if (!(g_g_stress <= 0.0f))
            g_g_stress = 0.0f;
    } else if (g_g_stress != 0.0f) {
        g_g_stress = (float)((double)g_g_stress - step * (double)RECOVER_BLACK);
        if (g_g_stress < 0.0f)
            g_g_stress = 0.0f;
    }
    sum = (double)g_g_stress + (double)load;
    g_g_stress = (float)sum;
    if (!(sum >= -(double)STRESS_LIMIT))
        g_g_stress = -STRESS_LIMIT;
    if (g_g_stress > STRESS_LIMIT)
        g_g_stress = STRESS_LIMIT;

    if (g_g_stress < 0.0f) {
        g_vision_fade[FADE_RED] = (int16_t)(int32_t)truncate_9d72c((double)g_g_stress + (double)STRESS_LIMIT);
        if (g_vision_fade[FADE_RED] < RED_LEVEL_MIN)
            g_vision_fade[FADE_RED] = RED_LEVEL_MIN;
        g_vision_fade[FADE_BLACK] = 0;
        g_vision_fade_mode = VISION_RED;
    } else if (g_g_stress != 0.0f) {
        g_vision_fade[FADE_BLACK] = (int16_t)(int32_t)truncate_9d72c((double)BLACK_LEVEL_TOP - (double)g_g_stress);
        if (g_vision_fade[FADE_BLACK] < 0)
            g_vision_fade[FADE_BLACK] = 0;
        g_vision_fade[FADE_RED] = 0;
        g_vision_fade_mode = VISION_BLACK;
    } else {
        g_vision_fade_mode = VISION_CLEAR;
    }
    if (old_red != g_vision_fade[FADE_RED] || old_black != g_vision_fade[FADE_BLACK])
        g_vision_tint_changed = 1;
}
