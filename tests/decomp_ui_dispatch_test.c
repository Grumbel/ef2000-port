/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_globals.h"
#include <assert.h>
#include <string.h>

static uint8_t guest[0x220000];
uint8_t g_mode_input_enabled_a, g_mode_output_a;
int32_t g_ui_control_limit;
ef_addr32 g_ui_control_table;
uint8_t g_ui_button_code_table[10];
uint8_t g_ui_control_initialized;
ef_addr32 g_ui_panel_config_ptr;
uint8_t g_ui_alternate_message;

static uint32_t activated[32], deactivated[32], released[48];
static unsigned activates, deactivates, releases, updates, panel_resets, reports, messages;
static struct { uint32_t a, d, b, c; } last_update;
static ef_addr32 last_address;
static int32_t last_invalid;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    assert(addr < sizeof(guest));
    return guest + addr;
}

void ui_control_activate(uint32_t index) { activated[activates++] = index; }
void ui_control_release(uint32_t index) { released[releases++] = index; }
void ui_control_deactivate(uint32_t index) { deactivated[deactivates++] = index; }
void ui_control_update(uint32_t a, uint32_t d, uint32_t b, uint32_t c)
{
    last_update.a = a; last_update.d = d;
    last_update.b = b; last_update.c = c;
    ++updates;
}
void ui_reset_panel_memory(ef_addr32 address) { last_address = address; ++panel_resets; }
void ui_report_out_of_range(int32_t index) { last_invalid = index; ++reports; }
void ui_show_message(ef_addr32 address) { last_address = address; ++messages; }

static void test_array_release(void)
{
    unsigned i;
    g_mode_input_enabled_a = 0;
    ui_initialize_releases();
    ui_release_all_controls();
    assert(!activates && !releases);
    g_mode_input_enabled_a = 1;
    ui_initialize_releases();
    assert(activates == 14 && g_ui_control_initialized == 1);
    for (i = 0; i < 14; ++i) { assert(activated[i] == i); }
    ui_initialize_releases();
    assert(activates == 14);
    ui_release_all_controls();
    assert(releases == 14);
    for (i = 0; i < 14; ++i) { assert(released[i] == i); }
    g_mode_output_a = 0;
    ui_finalize_releases();
    assert(!deactivates && g_ui_control_initialized == 1);
    g_mode_output_a = 1;
    ui_finalize_releases();
    assert(deactivates == 14 && g_ui_control_initialized == 0);
    for (i = 0; i < 14; ++i) { assert(deactivated[i] == i); }
    ui_finalize_releases();
    assert(deactivates == 14);
}

static void test_matching_release(void)
{
    unsigned old = releases;
    memset(g_ui_button_code_table, 0x33, sizeof(g_ui_button_code_table));
    g_ui_button_code_table[2] = 0x41;
    g_ui_button_code_table[8] = 0x41;
    g_mode_output_a = 1;
    ui_release_matching_control(0x41);
    assert(releases == old + 2);
    assert(released[old] == 2 && released[old + 1] == 8);
    ui_release_matching_control(0x0141);
    assert(releases == old + 2);
    g_mode_output_a = 0;
    ui_release_matching_control(0x41);
    assert(releases == old + 2);
}

static void test_dispatch(void)
{
    g_ui_control_limit = 4;
    g_ui_control_table = 0x21000;
    g_mode_output_a = 1;
    guest[g_ui_control_table + 8] = 0xcd;
    guest[g_ui_control_table + 9] = 0xab;
    ui_dispatch_indexed_control(0x55, 2, -4, 27);
    assert(updates == 1);
    assert(last_update.a == 0x55 && last_update.d == 0xfffffffcu);
    assert(last_update.b == 0xabcd && last_update.c == 27);
    ui_dispatch_indexed_control(0x55, 4, 0, 0);
    assert(updates == 1 && reports == 0);
    ui_dispatch_indexed_control(0x55, 5, 0, 0);
    assert(reports == 1 && last_invalid == 5);
    g_mode_output_a = 0;
    ui_dispatch_indexed_control(0x55, 2, 0, 0);
    assert(updates == 1);
    ui_dispatch_indexed_control(0x55, 8, 0, 0);
    assert(reports == 2);
}

static void test_reset_and_text(void)
{
    g_mode_output_a = 0;
    ui_reset_active_panel();
    assert(updates == 1);
    g_mode_output_a = 1;
    g_ui_panel_config_ptr = 0x22000;
    ui_reset_active_panel();
    assert(updates == 2);
    assert(last_update.a == 0 && last_update.d == 1);
    assert(last_update.b == 0x22000 && last_update.c == 0);
    g_mode_input_enabled_a = 1;
    unsigned before = releases;
    ui_reset_panel_and_release();
    assert(releases == before + 14 && panel_resets == 1);
    assert(last_address == 0x1f9020);
    g_ui_alternate_message = 0;
    ui_emit_selected_message();
    assert(messages == 1 && last_address == 0x1843f0);
    g_ui_alternate_message = 9;
    ui_emit_selected_message();
    assert(messages == 2 && last_address == 0x1843f4);
}

int main(void)
{
    test_array_release();
    test_matching_release();
    test_dispatch();
    test_reset_and_text();
    return 0;
}
