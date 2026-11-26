/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Point list handed to 0xc8c78: (x, y) word pairs. */
#define CURVE_POINTS      0x0015957cu
#define CURVE_STEPS_BASE  0x00019220  /* (this << shift) >> 16 = steps - 1 */

#define POINT_X(p)  ((int32_t)(int16_t)(uint16_t)(p))
#define POINT_Y(p)  ((int32_t)(int16_t)(uint16_t)((p) >> 16))

/* The original shifts by CL, which uses only the low five bits of the count. */
static int32_t sar(int32_t value, uint32_t count)
{
    return value >> (count & 31u);
}

/* One axis of the curve in 16.16 fixed point: the emitted coordinate is
   base + offset; offset starts at control - end and delta is its step. */
typedef struct CurveAxis {
    int32_t offset;
    int32_t delta;
    int32_t base;
} CurveAxis;

static void setup_axis(CurveAxis *axis, int32_t control, int32_t end, int32_t start, uint32_t shift)
{
    int32_t step;
    uint32_t k1 = (uint8_t)(shift + 1u);
    uint32_t k3 = (uint8_t)(shift * 2u + 3u);
    uint32_t k4 = (uint8_t)(shift * 2u + 4u);

    axis->offset = (control - end) << 16;
    axis->delta = (control - start) << 16;
    axis->base = (start << 16) - axis->offset + 0x8000;
    /* subtract d/2^k3 + d/2^(k3+k4) + d/2^(2*k3+k4), then add offset/2^k1 */
    step = sar(axis->delta, k3);
    axis->delta -= step;
    step = sar(step, k4);
    axis->delta -= step;
    axis->delta -= sar(step, k3);
    axis->delta += sar(axis->offset, k1);
}

static void step_axis(CurveAxis *axis, uint32_t shift)
{
    axis->delta -= sar(axis->offset, shift);
    axis->offset += sar(axis->delta, shift);
}

/* 0xfd164, 335 bytes. Draw a quadratic curve from START to END bent
   towards CONTROL (each an (x, y) word pair) by forward differencing in 16.16
   fixed point: emit ((0x19220 << SHIFT) >> 16) + 1 points from START
   towards END, then END itself, into the list at 0x15957c, and draw it
   as a polyline through 0xc8c78. Callee pops its four arguments. */
void draw_quad_curve_fd164(uint32_t shift, uint32_t control, uint32_t end, uint32_t start)
{
    CurveAxis x;
    CurveAxis y;
    ef_addr32 out = CURVE_POINTS;
    int32_t steps;

    setup_axis(&x, POINT_X(control), POINT_X(end), POINT_X(start), shift);
    setup_axis(&y, POINT_Y(control), POINT_Y(end), POINT_Y(start), shift);
    steps = sar((int32_t)((uint32_t)CURVE_STEPS_BASE << (shift & 31u)), 16);
    for (; steps >= 0; steps--) {
        EF_U16(out) = (uint16_t)sar(x.base + x.offset, 16);
        EF_U16(out + 2u) = (uint16_t)sar(y.base + y.offset, 16);
        out += 4u;
        step_axis(&x, (uint8_t)shift);
        step_axis(&y, (uint8_t)shift);
    }
    EF_U16(out) = (uint16_t)end;
    EF_U16(out + 2u) = (uint16_t)(end >> 16);
    out += 4u;
    guest_fn_c8c78_si(CURVE_POINTS, (out - CURVE_POINTS) >> 2);
}
