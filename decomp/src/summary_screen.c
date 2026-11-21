/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* The summary screens show, per unit or site class, three bars (NATO,
 * neutral, enemy) with the count of each written into its own widget.
 * The counts are words in the summary block at 0x40cf90..0x40d024. */

#define SIDE_COUNT           3
#define BAR_BASE             0x78
#define GROUP_X              5
#define GROUP_STEP           0x37
#define BAR_STEP             15
#define LINE_COLOUR          8

struct summary_group {
    uint32_t x[SIDE_COUNT];      /* bar positions: NATO, neutral, enemy */
    ef_addr32 count[SIDE_COUNT]; /* count words */
    ef_addr32 name[SIDE_COUNT];  /* widget names for the counts */
};

/* Group k of the regular layout: bars 15 apart, counts in consecutive words. */
#define REGULAR_GROUP(k, counts, n0, n1, n2) \
    { { GROUP_X + GROUP_STEP * (k), GROUP_X + GROUP_STEP * (k) + BAR_STEP, \
        GROUP_X + GROUP_STEP * (k) + 2 * BAR_STEP }, \
      { (counts), (counts) + 2u, (counts) + 4u }, { (n0), (n1), (n2) } }

static const uint32_t side_colour[SIDE_COUNT] = { 3, 4, 1 };

static void draw_summary_groups(ef_addr32 screen, const struct summary_group *groups, int count)
{
    /* The original has a 4-byte buffer, enough for counts up to 999. */
    char text[4];
    int g, i;

    for (g = 0; g < count; g++) {
        for (i = 0; i < SIDE_COUNT; i++)
            draw_bar_9bca4(side_colour[i], BAR_BASE, groups[g].x[i],
                           (uint32_t)(int32_t)(int16_t)EF_U16(groups[g].count[i]));
        for (i = 0; i < SIDE_COUNT; i++) {
            signed_to_digits_cbd0f((int16_t)EF_U16(groups[g].count[i]), (ef_addr32)(uintptr_t)text, 10);
            show_widget_a7588(find_widget_checked_a78bc(screen, groups[g].name[i]),
                              (ef_addr32)(uintptr_t)text);
        }
    }
}

/* The base line: two vertical strokes at x = 0x77 and 0x78. */
static void draw_summary_axis(uint32_t first_x, uint32_t second_x, uint32_t y1, uint32_t y2)
{
    draw_scaled_rect_c95ac(LINE_COLOUR, first_x, y1, first_x, y2);
    draw_scaled_rect_c95ac(LINE_COLOUR, second_x, y1, second_x, y2);
}

/* Per-side counts of the air summary: four words each (air, ship,
 * ground-based, static). */
#define AIR_NATO             0x0040d00eu
#define AIR_NEUTRAL          0x0040d016u
#define AIR_ENEMY            0x0040d01eu

/* 0x98eac, 878 bytes. Draw the target class summary: air, ground-based,
 * ship and static targets. */
void draw_class_summary_98eac(ef_addr32 screen)
{
    static const struct summary_group groups[] = {
        { { 0x0a, 0x19, 0x28 }, { AIR_NATO, AIR_NEUTRAL, AIR_ENEMY },
          { 0x0018add8u, 0x0018ade4u, 0x0018adf0u } }, /* "airnb*" */
        { { 0x40, 0x50, 0x5f }, { AIR_NATO + 4u, AIR_NEUTRAL + 4u, AIR_ENEMY + 4u },
          { 0x0018adfcu, 0x0018ae0cu, 0x0018ae1cu } }, /* "gbasednb*" */
        { { 0x78, 0x87, 0x96 }, { AIR_NATO + 2u, AIR_NEUTRAL + 2u, AIR_ENEMY + 2u },
          { 0x0018ae2cu, 0x0018ae38u, 0x0018ae44u } }, /* "shipnb*" */
        { { 0xaf, 0xbe, 0xcd }, { AIR_NATO + 6u, AIR_NEUTRAL + 6u, AIR_ENEMY + 6u },
          { 0x0018ae50u, 0x0018ae60u, 0x0018ae70u } }, /* "staticnb*" */
    };

    draw_summary_groups(screen, groups, 4);
    draw_summary_axis(0x78, 0x77, 0, 0xdc);
}

/* 0x9921c, 1091 bytes. Draw the air summary: fighters, bombers,
 * transports, tankers and AWACS. */
void draw_air_summary_9921c(ef_addr32 screen)
{
    static const struct summary_group groups[] = {
        REGULAR_GROUP(0, 0x0040cfb4u, 0x0018ae80u, 0x0018ae8cu, 0x0018ae98u), /* "fitnb*" */
        REGULAR_GROUP(1, 0x0040cfbau, 0x0018aea4u, 0x0018aeb0u, 0x0018aebcu), /* "bomnb*" */
        REGULAR_GROUP(2, 0x0040cfc0u, 0x0018aec8u, 0x0018aed4u, 0x0018aee0u), /* "tranb*" */
        REGULAR_GROUP(3, 0x0040cfccu, 0x0018aeecu, 0x0018aef8u, 0x0018af04u), /* "refnb*" */
        REGULAR_GROUP(4, 0x0040cfd2u, 0x0018af10u, 0x0018af1cu, 0x0018af28u), /* "awacnb*" */
    };

    draw_summary_groups(screen, groups, 5);
    draw_summary_axis(0x78, 0x77, 5, 0x122);
}

/* 0x99660, 1493 bytes. Draw the ground site summary: early warning radar
 * sites, choke points, political and industrial sites, oil rigs,
 * airfields, ports and dams. */
void draw_site_summary_99660(ef_addr32 screen)
{
    static const struct summary_group groups[] = {
        REGULAR_GROUP(0, 0x0040cfdeu, 0x0018af34u, 0x0018af44u, 0x0018af54u), /* "ewrsitenb*" */
        REGULAR_GROUP(1, 0x0040cfe4u, 0x0018af64u, 0x0018af74u, 0x0018af84u), /* "chokepntnb*" */
        REGULAR_GROUP(2, 0x0040cfeau, 0x0018af94u, 0x0018afa4u, 0x0018afb4u), /* "polindnb*" */
        REGULAR_GROUP(3, 0x0040cff0u, 0x0018afc4u, 0x0018afd4u, 0x0018afe4u), /* "oilrignb*" */
        REGULAR_GROUP(4, 0x0040cff6u, 0x0018aff4u, 0x0018b004u, 0x0018b014u), /* "airfieldnb*" */
        REGULAR_GROUP(5, 0x0040cffcu, 0x0018b024u, 0x0018b030u, 0x0018b03cu), /* "portnb*" */
        REGULAR_GROUP(6, 0x0040d002u, 0x0018b048u, 0x0018b054u, 0x0018b060u), /* "damnb*" */
    };

    draw_summary_groups(screen, groups, 7);
    draw_summary_axis(0x78, 0x77, 0, 0x190);
}

/* 0x99c38, 683 bytes. Draw the defence summary: flak sites, battleships
 * and supply ships. */
void draw_summary_key_99c38(ef_addr32 screen)
{
    static const struct summary_group groups[] = {
        REGULAR_GROUP(0, 0x0040cf90u, 0x0018b06cu, 0x0018b078u, 0x0018b084u), /* "antinb*" */
        REGULAR_GROUP(1, 0x0040cfaeu, 0x0018b090u, 0x0018b09cu, 0x0018b0a8u), /* "battnb*" */
        REGULAR_GROUP(2, 0x0040cfa2u, 0x0018b0b4u, 0x0018b0c4u, 0x0018b0d4u), /* "trashpnb*" */
    };

    draw_summary_groups(screen, groups, 3);
    draw_summary_axis(0x77, 0x78, 5, 0x16d);
}

/* The share screen: a pie of the NATO, neutral and enemy totals with a
 * legend and the percentages. */
#define SHARE_NATO           0x0040cfd8u /* int16_t totals: NATO, neutral, enemy */
#define SHARE_NEUTRAL        0x0040cfdau
#define SHARE_ENEMY          0x0040cfdcu
#define PIE_CENTRE           100
#define PIE_RADIUS           50
#define DEGREES              (*(double *)ef_guest_memory_ptr(0x0018adbeu)) /* 360.0 */
#define PER_DEGREE           (*(double *)ef_guest_memory_ptr(0x0018adc6u)) /* 1/360 */
#define PERCENT              (*(double *)ef_guest_memory_ptr(0x0018adceu)) /* 100.0 */
#define PERCENT_FORMAT       0x0018ad94u /* "%.2f" */
#define PERCENT_SUFFIX       0x0018ad9cu /* " %" */
#define REST_FORMAT          0x0018adb0u /* "%.2f %%" */
#define NATO_WIDGET          0x0018ada0u /* "nato" */
#define NEUTRAL_WIDGET       0x0018ada8u /* "neutral" */
#define ENEMY_WIDGET         0x0018adb8u /* "enemy" */

static int32_t pie_degrees(double total, double share_of_one)
{
    return (int32_t)truncate_9d72c(total * share_of_one * DEGREES);
}

/* Print the angle as a percentage with two decimals and read the printed
 * value back, so the shares add up to what is shown. */
static double show_percent(ef_addr32 screen, ef_addr32 widget, int16_t degrees, char *text)
{
    double value;

    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, PERCENT_FORMAT, (double)degrees * PER_DEGREE * PERCENT);
    value = guest_fn_9d55d((ef_addr32)(uintptr_t)text, 0);
    strcat_9ca57((ef_addr32)(uintptr_t)text, PERCENT_SUFFIX);
    show_widget_a7588(find_widget_checked_a78bc(screen, widget), (ef_addr32)(uintptr_t)text);
    return value;
}

/* 0x98c1c, 654 bytes. Draw the share pie for SCREEN: NATO (colour 3),
 * neutral (4) and enemy (1) slices in whole degrees, each truncated, with
 * the 16-bit sum as the whole, three framed legend squares, and the NATO
 * and neutral percentages; the enemy one is 100 minus their printed sum. */
void sumgengraph(ef_addr32 screen)
{
    char text[28];
    double one = 1.0 / (double)(int16_t)(EF_U16(SHARE_NATO) + EF_U16(SHARE_NEUTRAL) + EF_U16(SHARE_ENEMY));
    int32_t nato = pie_degrees((double)(int16_t)EF_U16(SHARE_NATO), one);
    int32_t neutral = pie_degrees((double)(int16_t)EF_U16(SHARE_NEUTRAL), one);
    int32_t enemy = pie_degrees((double)(int16_t)EF_U16(SHARE_ENEMY), one);
    int16_t end1 = (int16_t)nato;
    int16_t end2 = (int16_t)(nato + neutral);
    float shown;

    guest_fn_c9734(3, PIE_CENTRE, PIE_CENTRE, 0, end1, PIE_RADIUS, PIE_RADIUS);
    guest_fn_c9734(4, PIE_CENTRE, PIE_CENTRE, end1, end2, PIE_RADIUS, PIE_RADIUS);
    guest_fn_c9734(1, PIE_CENTRE, PIE_CENTRE, end2, (int16_t)(end2 + enemy), PIE_RADIUS, PIE_RADIUS);
    fill_scaled_rect_c99ac(3, 5, 5, 15, 15);
    fill_scaled_rect_c99ac(4, 5, 20, 15, 30);
    fill_scaled_rect_c99ac(1, 5, 35, 15, 45);
    guest_fn_c9a98_x(8, 5, 5, 15, 15);
    guest_fn_c9a98_x(8, 5, 20, 15, 30);
    guest_fn_c9a98_x(8, 5, 35, 15, 45);

    shown = (float)show_percent(screen, NATO_WIDGET, (int16_t)nato, text);
    shown = (float)(show_percent(screen, NEUTRAL_WIDGET, (int16_t)neutral, text) + (double)shown);
    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, REST_FORMAT, PERCENT - (double)shown);
    show_widget_a7588(find_widget_checked_a78bc(screen, ENEMY_WIDGET), (ef_addr32)(uintptr_t)text);
}
