/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record copies, dialog binding and directory listing helpers. */

/* 0x88cd0, 53 bytes. Copy three 8-byte fields of an object's detail block into caller buffers. */
void copy_detail_fields_88cd0(ef_addr32 object, ef_addr32 out_a, ef_addr32 out_b, ef_addr32 out_c)
{
    ef_addr32 d = EF_U32(object + 0x30u);

    EF_U32(out_a) = EF_U32(d + 0x69u);
    EF_U32(out_a + 4u) = EF_U32(d + 0x6du);
    EF_U32(out_b) = EF_U32(d + 0x61u);
    EF_U32(out_b + 4u) = EF_U32(d + 0x65u);
    EF_U32(out_c) = EF_U32(d + 0x91u);
    EF_U32(out_c + 4u) = EF_U32(d + 0x95u);
}

/* 0x6e320, 110 bytes. Gather six table dwords for a (kind, slot) pair and forward them. */
void forward_table_entry_6e320(uint8_t slot, uint32_t kind)
{
    uint32_t v[6];
    uint32_t off = (kind & 0xffu) * 279u + (uint32_t)slot * 8u;

    v[0] = EF_U32(0x001252acu + off);
    v[1] = EF_U32(0x001252b0u + off);
    v[2] = EF_U32(0x00125304u + off);
    v[3] = EF_U32(0x00125308u + off);
    v[4] = EF_U32(0x0012535cu + off);
    v[5] = EF_U32(0x00125360u + off);
    guest_fn_6e1c4_x((uint32_t)(uintptr_t)v);
}

/* 0x68efc, 66 bytes. Resolve the name of every populated weapon entry (from index 2) into its halfword id. */
void resolve_weapon_names_68efc(void)
{
    char name[0x14];
    int16_t i = 2;

    for (;;) {
        ef_addr32 e = (uint32_t)(int32_t)i * 0x91u;

        if (EF_U32(e + 0x0012bc68u) == 0)
            return;
        guest_string_copy((uint32_t)(uintptr_t)name, EF_U32(e + 0x0012bc74u));
        i++;
        EF_U16(e + 0x0012bc8au) = (uint16_t)guest_load_halfword((uint32_t)(uintptr_t)name);
    }
}

/* 0x569b4, 96 bytes. Apply a queued position record to its unit and forward the update. */
void apply_position_record_569b4(ef_addr32 rec)
{
    ef_addr32 obj = guest_fn_7489c((int16_t)EF_U16(rec + 1u));

    if (obj == 0)
        return;
    if ((EF_S32(rec + 3u) >> 16) == 1)
        EF_U8(obj + 0x24u) = (uint8_t)(EF_U8(obj + 0x24u) | 0x10u);
    guest_fn_106b0_x((uint32_t)(EF_S32(rec + 5u) >> 16), (uint32_t)(EF_S32(obj) >> 8),
                     (uint32_t)(EF_S32(obj + 4u) >> 8), (uint32_t)(EF_S32(obj + 8u) >> 8), EF_U8(rec + 9u));
    guest_fn_10750_x((int16_t)EF_U16(rec + 1u), EF_S32(rec + 3u) >> 16, EF_S32(rec + 1u) >> 16);
}

/* 0x4f388, 88 bytes. Bind two values into the named dialog's widgets, then emit its flag and close it. */
void bind_dialog_values_4f388(ef_addr32 first, ef_addr32 second)
{
    ef_addr32 dialog = guest_open_named_dialog(0x00184e18u, 0x00184e24u);
    ef_addr32 a = guest_select_dialog_widget(dialog, 0x00184e24u);
    ef_addr32 b = guest_select_dialog_widget(dialog, 0x00184e30u);

    guest_bind_dialog_widget(a, first);
    guest_bind_dialog_widget(b, second);
    guest_emit_dialog_flag(EF_U8(dialog + 0x10u));
    guest_destroy_named_dialog(dialog);
}

/* 0x34a98, 76 bytes. Rebuild the child dialog list with the given text and return the dialog. */
ef_addr32 rebuild_child_dialog_34a98(ef_addr32 text)
{
    ef_addr32 dialog;

    guest_fn_a3e04();
    guest_fn_34ea4_x();
    dialog = guest_open_named_dialog(0x00182e70u, 1);
    set_children_bit6(dialog, 0);
    guest_fn_a73cc(dialog, text, 0);
    guest_fn_a73cc(dialog, 0x00182e7cu, 0);
    guest_fn_a4dd0_x(dialog);
    return dialog;
}

/* 0x32de8, 120 bytes. List the files matching a path and mask into the 13-byte name table; returns the count. */
int32_t list_directory_names_32de8(ef_addr32 path, ef_addr32 mask)
{
    char buf[0x50];
    uint8_t dta[0x2c];
    ef_addr32 table = 0x0019e5f0u;
    int32_t count = 0;

    guest_string_copy((uint32_t)(uintptr_t)buf, path);
    guest_string_append((uint32_t)(uintptr_t)buf, mask);
    if (guest_fn_decde_x((uint32_t)(uintptr_t)buf, 0, (uint32_t)(uintptr_t)dta) == 0) {
        do {
            if (dta[0x1e] != 0x2e) {
                guest_string_copy(table, (uint32_t)(uintptr_t)&dta[0x1e]);
                count++;
                table += 13u;
            }
        } while (guest_fn_decfb_x((uint32_t)(uintptr_t)dta) == 0);
    }
    guest_fn_dedde_x(0x0019e5f0u, (uint32_t)count, 13, 0x00032de0u);
    return count;
}
