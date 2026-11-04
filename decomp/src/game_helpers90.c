/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Wireframe box drawing through the 3D line routine e81b0. */

/* 0xe83d8, 169 bytes. Draw the four edges of the rectangle spanned by two 3-word points in the plane of the first point's y. */
void draw_rect_outline_e83d8(ef_addr32 p, ef_addr32 q)
{
    int16_t a[3];
    int16_t b[3];
    ef_addr32 pa = (ef_addr32)(uintptr_t)a;
    ef_addr32 pb = (ef_addr32)(uintptr_t)b;

    a[1] = EF_S16(p + 2u);
    b[1] = EF_S16(p + 2u);
    a[0] = EF_S16(p);
    a[2] = EF_S16(p + 4u);
    b[0] = EF_S16(q);
    b[2] = EF_S16(p + 4u);
    guest_fn_e81b0(pa, pb);
    a[2] = EF_S16(q + 4u);
    b[2] = EF_S16(q + 4u);
    guest_fn_e81b0(pa, pb);
    a[0] = EF_S16(p);
    a[2] = EF_S16(p + 4u);
    b[0] = EF_S16(p);
    b[2] = EF_S16(q + 4u);
    guest_fn_e81b0(pa, pb);
    a[0] = EF_S16(q);
    b[0] = EF_S16(q);
    guest_fn_e81b0(pa, pb);
}

/* 0xe853c, 167 bytes. Draw a wireframe box: both rectangle outlines plus the four vertical edges. */
void draw_box_outline_e853c(ef_addr32 p, ef_addr32 q)
{
    int16_t a[3];
    int16_t b[3];
    ef_addr32 pa = (ef_addr32)(uintptr_t)a;
    ef_addr32 pb = (ef_addr32)(uintptr_t)b;

    draw_rect_outline_e83d8(p, q);
    draw_rect_outline_e83d8(q, p);
    a[1] = EF_S16(p + 2u);
    b[1] = EF_S16(q + 2u);
    a[2] = EF_S16(p + 4u);
    b[2] = EF_S16(p + 4u);
    a[0] = EF_S16(p);
    b[0] = EF_S16(p);
    guest_fn_e81b0(pa, pb);
    a[0] = EF_S16(q);
    b[0] = EF_S16(q);
    guest_fn_e81b0(pa, pb);
    a[2] = EF_S16(q + 4u);
    b[2] = EF_S16(q + 4u);
    a[0] = EF_S16(p);
    b[0] = EF_S16(p);
    guest_fn_e81b0(pa, pb);
    a[0] = EF_S16(q);
    b[0] = EF_S16(q);
    guest_fn_e81b0(pa, pb);
}
