/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Scene group fixups, UI state reset, video mode selection and record dumps, 0xaa4d4..0xb29a8.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0xaa4d4, 175 bytes. Rewrite the pointer fields of every type 0xe node in each group. */
void patch_group_pointers_aa4d4(void)
{
    ef_addr32 group = EF_U32(0x00419200u);
    int16_t g;
    int16_t n;

    for (g = 0; g < (int16_t)EF_U16(0x004194c2u); g++) {
        ef_addr32 node = EF_U32(group + 8u);
        for (n = 0; n < (int16_t)EF_U16(group + 0xcu); n++) {
            if (EF_U8(node) == 0xe) {
                ef_addr32 rec = EF_U32(node + 9u);
                ef_addr32 p = EF_U32(rec + 0x1eu);
                EF_U32(p) = guest_fn_aa4b8(EF_S32(p + 2u) >> 16, group);
                p = EF_U32(rec + 0x22u);
                if (p != 0)
                    EF_U32(p) = guest_fn_aa4b8(EF_S32(p + 2u) >> 16, group);
                p = EF_U32(rec + 0x26u);
                if (p != 0)
                    EF_U32(p) = guest_fn_aa4b8(EF_S32(p + 2u) >> 16, group);
            }
            node = EF_U32(node + 1u);
        }
        group = EF_U32(group);
    }
}

/* 0xab28c, 149 bytes. Visit the pointer fields of every type 0xe node, then finish the context. */
void visit_group_nodes_ab28c(ef_addr32 context)
{
    ef_addr32 group = EF_U32(0x00419200u);
    int16_t g;
    int16_t n;

    for (g = 0; g < (int16_t)EF_U16(0x004194c2u); g++) {
        ef_addr32 node = EF_U32(group + 8u);
        for (n = 0; n < (int16_t)EF_U16(group + 0xcu); n++) {
            if (EF_U8(node) == 0xe) {
                ef_addr32 rec = EF_U32(node + 9u);
                guest_fn_ab260(context, EF_U32(rec + 0x1eu));
                if (EF_U32(rec + 0x22u) != 0)
                    guest_fn_ab260(context, EF_U32(rec + 0x22u));
                if (EF_U32(rec + 0x26u) != 0)
                    guest_fn_ab260(context, EF_U32(rec + 0x26u));
            }
            node = EF_U32(node + 1u);
        }
        group = EF_U32(group);
    }
    guest_fn_ab174(context);
}

/* 0xaca14, 115 bytes. Pick the default video mode entry from the configured kind. */
void select_default_video_aca14(void)
{
    if (EF_U16(0x0041eedeu) == 0) {
        uint32_t kind = (uint32_t)(EF_S32(0x0041eec0u) >> 24);
        uint32_t name;
        uint32_t idx;

        if (kind == 1)
            name = 0x0018dc64u;
        else if (kind == 2)
            name = 0x0018dc6cu;
        else
            name = 0x0018dc74u;
        EF_U16(0x0041eedcu) = (uint16_t)find_name_index_ca780(name);
        idx = EF_U16(0x0041eedcu);
        EF_U16(0x0041eed4u) = EF_U16(0x00137293u + idx * 25u);
        EF_U16(0x0041eec8u) = EF_U16(0x00137295u + idx * 25u);
    }
}

/* 0xacac0, 166 bytes. Select the video mode named by the argument. */
void select_video_mode_acac0(ef_addr32 name)
{
    uint32_t idx;

    if (guest_fn_9d524(name, 0x0018dc7cu) == 0) {
        EF_U16(0x0041eedau) = 0;
        EF_U16(0x0041eedeu) = 1;
        EF_U16(0x0041eedcu) = (uint16_t)find_name_index_ca780(name);
    } else if (guest_fn_9d524(name, 0x0018dc84u) == 0) {
        EF_U16(0x0041eedau) = 1;
        EF_U16(0x0041eedeu) = 0;
        select_default_video_aca14();
    } else {
        EF_U16(0x0041eedau) = 1;
        EF_U16(0x0041eedeu) = 1;
        EF_U16(0x0041eedcu) = (uint16_t)find_name_index_ca780(name);
    }
    idx = EF_U16(0x0041eedcu);
    EF_U16(0x0041eed4u) = EF_U16(0x00137293u + idx * 25u);
    EF_U16(0x0041eec8u) = EF_U16(0x00137295u + idx * 25u);
}

/* 0xac16c, 211 bytes. Clear the UI state tables and select the video mode. */
void clear_ui_state_ac16c(void)
{
    uint32_t r = guest_fn_ab8c4();

    guest_fn_ca5e4(r, 0);
    EF_U16(0x0041eeb6u) = 0;
    EF_U16(0x0041eed8u) = 0;
    EF_U16(0x0041eed0u) = 0;
    EF_U16(0x0041eeceu) = 0;
    EF_U32(0x0041ecd0u) = 0;
    clear_widget_table_a4e98();
    guest_fn_b3fb0(0x0041e960u, 0, 0x150u);
    EF_U16(0x0041eeccu) = 0;
    if (EF_U8(0x0041f2e1u) != 0)
        guest_query_runtime_pair((uint32_t *)(uintptr_t)0x0041eeb0u, (uint32_t *)(uintptr_t)0x0041eeb0u);
    EF_U8(0x0013725du) = 0;
    EF_U8(0x0013725cu) = 0;
    EF_U8(0x0041f2e5u) = 0;
    select_video_mode_acac0(EF_U8(0x0041f2e1u) != 0 ? 0x0018db9cu : 0x0018dba4u);
    EF_U8(0x0041f2e4u) = 1;
    EF_U32(0x0041ec18u) = 0;
    EF_U8(0x0041f2deu) = 0;
    EF_U8(0x0041f833u) = 0;
    guest_fn_b3fb0(0x0041f284u, 0, 0x59u);
    EF_U8(0x0041f2e6u) = 0;
}

/* 0xac0e0, 139 bytes. Reset the UI after a mode change and centre the cursor. */
void reset_ui_state_ac0e0(void)
{
    if (guest_fn_e4304() != 0)
        guest_fn_e4264();
    guest_fn_e4188();
    init_pool_ab8e0(100000u);
    guest_fn_ab8b0();
    guest_fn_ca200(0x0018db90u);
    EF_U32(0x001378ddu) = EF_U32(0x0041eea8u);
    guest_fn_e17f8(EF_U32(0x0041eea8u), 0);
    EF_U32(0x0041eeacu) = 0;
    EF_U16(0x0013725eu) = (uint16_t)(EF_S32(0x0015b744u) / 2 - 1);
    EF_U16(0x00137260u) = (uint16_t)(EF_S32(0x0015b748u) / 2 - 1);
    EF_U8(0x0041f2e7u) = 1;
    clear_ui_state_ac16c();
    EF_U8(0x0041f833u) = 0;
    EF_U8(0x0041f2e6u) = 0;
}

/* 0xac240, 162 bytes. Run the registered page callbacks, then wait for the mouse buttons to clear. */
void run_page_callbacks_ac240(ef_addr32 arg, uint8_t flag)
{
    uint16_t saved = EF_U16(0x0041eeccu);
    uint16_t c;
    uint16_t i;

    guest_fn_b3fb0(0x0041e8d0u, 0, 0x90u);
    EF_U16(0x0041eec4u) = 0;
    guest_fn_ac2e4(arg, flag);
    if (EF_U16(0x0041eec4u) != 0) {
        i = 0;
        do {
            guest_fn_ac2e4(0x0041e8d0u + (uint32_t)i * 9u, 0);
            i++;
        } while (i < EF_U16(0x0041eec4u));
    }
    for (c = saved; c < EF_U16(0x0041eeccu); c++) {
        uint32_t fn = EF_U32(0x0041e969u + (uint32_t)c * 21u);
        if (fn != 0)
            ((void (*)(uint32_t))fn)(0);
    }
    guest_fn_a5abc();
    while ((uint16_t)mouse_read_buttons_ce064() != 0)
        ;
}

/* 0xb0138, 208 bytes. Allocate and initialise a pool sized from the shape list. */
ef_addr32 alloc_record_pool_b0138(ef_addr32 listp, ef_addr32 countp)
{
    ef_addr32 node = EF_U32(listp);
    ef_addr32 pool;
    int32_t i;

    EF_U32(countp) = 0;
    for (; node != 0; node = EF_U32(node + 4u)) {
        int16_t k;
        for (k = 0; k < (int16_t)EF_U8(node + 0x12u); k++)
            EF_U32(countp) += (uint32_t)EF_U8(EF_U32(node + 0x13u) + (uint32_t)(int32_t)k * 0x7cu + 1u) * 0x14u;
    }
    if (EF_U32(countp) == 0)
        EF_U32(countp) = 0x7d0u;
    pool = guest_fn_cc6c0(EF_U32(countp) * 54u, 0x0018dd7cu, EF_U32(countp) * 54u, EF_U32(countp));
    guest_fn_b3fb0(pool, 0, EF_U32(countp) * 54u);
    for (i = 0; i < (int32_t)EF_U32(countp); i++) {
        EF_U16(pool + (uint32_t)i * 0x36u + 0x30u) = 0xffff;
        EF_U16(pool + (uint32_t)i * 0x36u + 0x32u) = 0xffff;
        EF_U16(pool + (uint32_t)i * 0x36u + 0x34u) = 0xffff;
    }
    EF_U32(0x001379b0u) = 0;
    EF_U16(0x001379b8u) = 0;
    return pool;
}

/* 0xb1a68, 121 bytes. Free a record and all of its sub-tables. */
void free_record_tree_b1a68(ef_addr32 rec)
{
    int16_t c;
    int16_t j;

    if (rec == 0)
        return;
    for (c = 0; EF_S16(rec + 0x1eu) > 0 && c < EF_S16(rec + 0x1eu); c++) {
        ef_addr32 ent = EF_U32(rec + 0x20u) + (uint32_t)(int32_t)c * 10u;
        for (j = 0; j < EF_S16(ent + 4u); j++) {
            ef_addr32 item = EF_U32(ent + 6u) + (uint32_t)(int32_t)j * 6u;
            if (EF_U32(item + 2u) != 0)
                guest_fn_cc768(EF_U32(item + 2u));
        }
        if (EF_U32(ent + 6u) != 0)
            guest_fn_cc768(EF_U32(ent + 6u));
    }
    if (EF_U32(rec + 0x20u) != 0)
        guest_fn_cc768(EF_U32(rec + 0x20u));
    guest_fn_cc768(rec);
}

/* 0xb21ec, 140 bytes. Print one record to a text handle. */
void print_record_b21ec(ef_addr32 rec, uint32_t out)
{
    if (rec == 0) {
        guest_fn_b1bb0(out, 0x0018e040u);
        guest_fn_b1bb0(out, 0x0018e060u);
        guest_fn_b1bb0(out, 0x0018e060u);
        return;
    }
    guest_fn_b1bb0(out, 0x0018e008u, EF_U32(rec), (uint32_t)EF_U8(rec + 5u), (uint32_t)EF_U8(rec + 6u),
                   (uint32_t)EF_U8(rec + 7u), (uint32_t)EF_U8(rec + 8u), (uint32_t)EF_U8(rec + 9u),
                   (uint32_t)(EF_S32(rec + 8u) >> 16), (uint32_t)(EF_S32(rec + 0xau) >> 16),
                   (uint32_t)(EF_S32(rec + 0xcu) >> 16), (uint32_t)(EF_S32(rec + 0xeu) >> 16),
                   (uint32_t)(EF_S32(rec + 0x10u) >> 16), (uint32_t)(EF_S32(rec + 0x12u) >> 16));
}

/* 0xb2810, 166 bytes. Dump a table entry and its sub-records (variant A names). */
void dump_table_a_b2810(ef_addr32 rec, uint32_t out)
{
    int16_t c;

    guest_fn_b1bb0(out, 0x0018e260u, (uint32_t)EF_U8(rec + 1u), EF_U32(0x00133488u + (uint32_t)EF_U8(rec) * 4u),
                   EF_U32(0x00133304u + (uint32_t)EF_U8(rec + 2u) * 4u),
                   EF_U32(0x0013336cu + (uint32_t)EF_U8(rec + 3u) * 4u));
    guest_fn_b2a2c(rec + 8u, out);
    guest_fn_b1bb0(out, 0x0018e1acu, (uint32_t)(EF_S32(rec + 0x1au) >> 16), (uint32_t)(EF_S32(rec + 0x18u) >> 16),
                   (uint32_t)(EF_S32(rec + 0x1cu) >> 16));
    if (EF_S16(rec + 0x1cu) > 0) {
        c = 0;
        do {
            guest_fn_b2504(EF_U32(rec + 0x20u) + (uint32_t)(int32_t)c * 25u, (uint32_t)(int32_t)c, out);
            c++;
        } while (c < EF_S16(rec + 0x1cu));
    }
    guest_fn_b1bb0(out, 0x0018e1c4u);
}

/* 0xb28b8, 166 bytes. Dump a table entry and its sub-records (variant B names). */
void dump_table_b_b28b8(ef_addr32 rec, uint32_t out)
{
    int16_t c;

    guest_fn_b1bb0(out, 0x0018e260u, (uint32_t)EF_U8(rec + 1u), EF_U32(0x001333f8u + (uint32_t)EF_U8(rec) * 4u),
                   EF_U32(0x00133304u + (uint32_t)EF_U8(rec + 2u) * 4u),
                   EF_U32(0x0013336cu + (uint32_t)EF_U8(rec + 3u) * 4u));
    guest_fn_b2a2c(rec + 8u, out);
    guest_fn_b1bb0(out, 0x0018e1acu, (uint32_t)(EF_S32(rec + 0x1au) >> 16), (uint32_t)(EF_S32(rec + 0x18u) >> 16),
                   (uint32_t)(EF_S32(rec + 0x1cu) >> 16));
    if (EF_S16(rec + 0x1cu) > 0) {
        c = 0;
        do {
            guest_fn_b2504(EF_U32(rec + 0x20u) + (uint32_t)(int32_t)c * 25u, (uint32_t)(int32_t)c, out);
            c++;
        } while (c < EF_S16(rec + 0x1cu));
    }
    guest_fn_b1bb0(out, 0x0018e1c4u);
}

/* 0xb29a8, 129 bytes. Dump a list of sub-lists. */
void dump_group_list_b29a8(ef_addr32 list, uint32_t out)
{
    int16_t i;
    int16_t j;

    guest_fn_b1bb0(out, 0x0018e2a4u, EF_U32(0x00132cd0u + (uint32_t)EF_U8(list) * 4u),
                   (uint32_t)(int32_t)EF_S16(list + 1u));
    if (EF_S16(list + 1u) <= 0)
        return;
    i = 0;
    do {
        ef_addr32 sub = EF_U32(list + 3u) + (uint32_t)(int32_t)i * 6u;
        guest_fn_b1bb0(out, 0x0018e2bcu, (uint32_t)(int32_t)EF_S16(sub));
        for (j = 0; j < EF_S16(sub); j++)
            guest_fn_b2960(EF_U32(sub + 2u) + (uint32_t)(int32_t)j * 17u, out);
        i++;
    } while (i < EF_S16(list + 1u));
}
