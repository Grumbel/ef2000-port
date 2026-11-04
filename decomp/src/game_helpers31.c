/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Mode checks, timers and tables for the cockpit, scattered 0xa00..0x54210. */

/* 0xa00, 95 bytes. Issue request 0x2503 and report whether the returned code byte is 0xcf. */
uint8_t request_returns_cf_a00(uint32_t argument)
{
    uint32_t result;

    EF_U32(0x00196148u) = 0x2503u;
    EF_U32(0x00196150u) = argument;
    guest_fn_2b0(0x00196148u);
    result = EF_U32(0x0019614cu);
    if (result == 0)
        return 1;
    return EF_U8((result >> 12) + (result & 0xffffu) + EF_U32(0x00120cc6u)) == 0xcfu;
}

/* 0x15af0, 140 bytes. Drive the per-owner action chain for a player's slot. */
void run_owner_action_15af0(uint32_t slot, uint8_t arg)
{
    uint32_t extra = arg;

    EF_U32(0x00132ae0u) = 0x0001d470u;
    if (slot == 0xffu)
        return;
    if (EF_U8(0x001a666bu) != 0)
        return;
    guest_fn_15b7c(slot, extra);
    if (EF_U8(EF_U32(0x0019bb58u) + slot * 0xcau + 0x4fu) != 0) {
        if ((guest_fn_169b4(slot) & 0xffffu) != 0)
            guest_fn_16034(slot, extra);
        else if ((guest_fn_16a04(slot) & 0xffffu) != 0)
            guest_fn_161d4(slot, extra);
        else if ((guest_fn_16a3c(slot) & 0xffffu) != 0)
            guest_fn_16588(slot, extra);
    }
    guest_fn_1d420();
}

/* 0x1851c, 96 bytes. Fire the one-shot action when the idle conditions hold. */
void fire_idle_action_1851c(void)
{
    if (EF_U8(0x00132ac2u) != 0 || EF_U8(0x001a363au) != 0 || EF_U8(0x00127d42u) != 0)
        return;
    if (EF_U16(0x00126516u) != 0)
        return;
    if (EF_U8(0x001a362du) != 1 || EF_U8(0x001a3631u) != 1)
        return;
    EF_U8(0x001a3631u) = 0;
    EF_U8(0x001a363du) = 1;
    guest_fn_801e4(4, 1, 1);
}

/* 0x2546c, 118 bytes. Reset or advance the current object's mode after a transition test. */
void update_current_mode_2546c(void)
{
    int32_t index = (int32_t)EF_U32(0x001f71c6u) >> 16;

    if (guest_fn_7489c(index) == 0)
        return;
    if ((guest_fn_6aeb4(index) & 0xffu) == 0) {
        guest_fn_6a1d0(index, 1);
        EF_U16(g_current_object + 0x146u) = 0xff;
        EF_U32(g_current_object + 0x150u) = 0;
        EF_U32(g_current_object + 0x154u) = 0;
        return;
    }
    EF_U8(g_current_object + 0x16cu) = 2;
    set_indexed_object_mode((int16_t)index, 0);
}

/* 0x2a970, 104 bytes. Pop one entry of the 6-byte stack at 0x125262 into the current triple. */
void pop_triple_2a970(void)
{
    uint16_t count = EF_U16(0x00125260u);

    if (count != 0) {
        ef_addr32 entry;

        count--;
        entry = 0x00125262u + (uint32_t)((int32_t)(int16_t)count * 6);
        EF_U16(0x0012dc42u) = EF_U16(entry);
        EF_U16(0x0012dc44u) = EF_U16(entry + 2u);
        EF_U16(0x0012dc46u) = EF_U16(entry + 4u);
    } else {
        EF_U16(0x0012dc42u) = 0;
        EF_U16(0x0012dc44u) = 0;
        EF_U16(0x0012dc46u) = 0;
    }
    EF_U16(0x00125260u) = count;
}

/* 0x3711c, 114 bytes. Reset the list view and load the heading string for the current page. */
void reset_list_view_3711c(void)
{
    if (EF_U8(0x0019f59cu) != 0)
        return;
    switch (EF_U8(0x00125846u)) {
    case 1:
        guest_fn_abe44(0x00183274u);
        break;
    case 2:
        guest_fn_abe44(EF_U16(0x001f8fdeu) != 0 ? 0x00183280u : 0x00183288u);
        break;
    case 3:
        guest_fn_abe44(0x0018326cu);
        break;
    default:
        break;
    }
    EF_U8(0x0041f2e8u) = 1;
    guest_fn_a42f4();
    guest_fn_a4030();
    guest_fn_a4030();
    EF_U16(0x001357b2u) = 0;
}

/* 0x3a2d0, 117 bytes. Build a 36-entry table of scaled sine/cosine pairs (radius 60). */
void build_ring_table_3a2d0(void)
{
    int16_t i;

    EF_U8(0x0012635eu) = 1;
    for (i = 0; i < 0x24; i++) {
        uint32_t angle = (uint32_t)(((int32_t)i * 0xffff) / 0x24) & 0xffffu;
        uint32_t index = angle >> 3;
        int32_t sine = (int16_t)EF_U16(0x00141e08u + index * 2u);
        int32_t cosine = (int16_t)EF_U16(0x00142e08u + index * 2u);

        EF_U16(0x001a1870u + (uint32_t)i * 4u) = (uint16_t)((sine * 0x3c) >> 15);
        EF_U16(0x001a1872u + (uint32_t)i * 4u) = (uint16_t)((-(cosine * 0x3c)) >> 15);
    }
}

/* 0x3cd9c, 101 bytes. Convert the 11-bit fixed-point cursor and probe the cell under it. */
void probe_cursor_cell_3cd9c(void)
{
    int32_t y = (int32_t)EF_U32(0x001a58f4u) / 2048;
    int32_t x = (int32_t)EF_U32(0x001a58f0u) / 2048;

    EF_U8(0x001a361eu) = 0;
    if (EF_U8(0x001a5a9du) != 0) {
        int16_t found = (int16_t)guest_fn_7b2f4(0, (int32_t)(int16_t)y, (int32_t)(int16_t)x);

        if (found == -1)
            return;
        if ((int16_t)guest_fn_a17e8() != 0x32)
            return;
    }
    EF_U16(0x001a35eeu) = 0xffffu;
}

/* 0x47fbc, 97 bytes. Decrease the second countdown word unless device 0xb is in state 4. */
void tick_countdown_47fbc(void)
{
    if ((device_state_is_4(0x0b) & 0xffu) != 0)
        return;
    if (EF_U8(0x001a3647u) == 0)
        EF_U16(0x00126658u) = (uint16_t)(EF_U16(0x00126658u) - EF_U16(0x00127d92u));
    else
        EF_U16(0x00126658u) = (uint16_t)(EF_U16(0x00126658u) - (uint16_t)((int32_t)EF_U16(0x00127d92u) / 3));
    if ((int16_t)EF_U16(0x00126658u) < 0)
        guest_fn_47f9c();
}

/* 0x54210, 98 bytes. Cycle a frame counter every other tick and pass the frame to 0x8f4c. */
void cycle_frame_54210(void)
{
    uint8_t frame;

    if (EF_U16(0x001972e6u) == 0)
        return;
    EF_U8(0x00127da1u) = (uint8_t)(EF_U8(0x00127da1u) + EF_U8(0x00127d92u));
    if (EF_U8(0x00127da1u) <= 1)
        return;
    EF_U8(0x00127da1u) = (uint8_t)(EF_U8(0x00127da1u) - 1u);
    frame = (uint8_t)(EF_U8(0x001a5c20u) + 1u);
    EF_U8(0x001a5c20u) = frame;
    if (!((int16_t)frame < (int16_t)EF_U16(0x001972e6u)))
        EF_U8(0x001a5c20u) = 0;
    guest_fn_8f4c(EF_U8(0x001a5c20u));
}

/* 0x5437c, 134 bytes. Update every record of the 171-byte list: arm ready ones and refresh active ones. */
void update_records_5437c(void)
{
    int16_t i;

    if (EF_U16(0x0019af82u) == 0)
        return;
    for (i = 0; i < (int16_t)EF_U16(0x0019af82u); i++) {
        ef_addr32 record = EF_U32(0x0019af7cu) + (uint32_t)((int32_t)i * 0xab);

        if (EF_U8(record + 0x47u) == 0) {
            if ((guest_fn_40820(EF_U32(record + 0xa1u) + 0x10u) & 0xffu) != 0) {
                EF_U8(record + 0x48u) = 1;
                guest_fn_1d700((int32_t)i);
            }
        }
        if (EF_U8(EF_U32(0x0019af7cu) + (uint32_t)((int32_t)i * 0xab) + 0x47u) != 0)
            guest_fn_1e130();
    }
}

/* 0xd628, 99 bytes. Emit the pending message and, when idle, open the dialog and advance the page. */
void emit_and_advance_d628(void)
{
    EF_U8(0x00121e8cu) = 0;
    ui_emit_selected_message();
    if (EF_U16(0x0019769au) != 0)
        return;
    if ((int16_t)max_difference_50770() > (int16_t)EF_U16(0x001357b0u)) {
        refresh_widget_507dc();
        return;
    }
    if (EF_U16(0x00137998u) != 0 || EF_U16(0x0013799au) != 0)
        return;
    guest_fn_a78bc_name(guest_fn_a46cc(0x00180880u), 0x0018088cu);
    EF_U8(0x001a364bu) = (uint8_t)(EF_U8(0x001a364bu) + 0x0au);
    guest_fn_43020();
}
