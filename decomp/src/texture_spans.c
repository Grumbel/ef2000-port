/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* FS is the guest texture segment, not a flat host pointer. This one-load
 * adapter preserves that original addressing operation. A native port must
 * supply its guest-segment provider instead of using host TLS or DS here. */
#ifdef __WATCOMC__
static uint8_t texture_byte_fs(uint32_t offset);
#pragma aux texture_byte_fs = "mov al,fs:[eax]" parm [eax] value [al] modify [eax]
#else
extern uint8_t texture_byte_fs(uint32_t offset);
#endif

/* Keep IDIV's signed EDX:EAX dividend, quotient overflow and divide-by-zero
 * behavior. A C 64-bit division followed by narrowing would lose the latter. */
#ifdef __WATCOMC__
static uint32_t texture_idiv(uint32_t low, uint32_t high, uint32_t divisor);
#pragma aux texture_idiv = "idiv ebx" parm [eax] [edx] [ebx] value [eax] modify [eax edx]
#else
extern uint32_t texture_idiv(uint32_t low, uint32_t high, uint32_t divisor);
#endif

static uint32_t texture_divide(uint32_t value, uint32_t divisor)
{
    return texture_idiv(value, 0u - (value >> 31), divisor);
}

static uint32_t texture_product_divide(uint32_t a, uint32_t b, uint32_t divisor)
{
    int64_t product = (int64_t)(int32_t)a * (int32_t)b;

    return texture_idiv((uint32_t)product, (uint32_t)((uint64_t)product >> 32), divisor);
}

static uint32_t texture_step(uint32_t difference, int32_t reciprocal)
{
    int64_t product = (int64_t)(int32_t)difference * reciprocal;

    /* IMUL EDX:EAX followed by SHLD EDX,EAX,8: retain bits 24..55. */
    return (uint32_t)((uint64_t)product >> 24);
}

static uint8_t texture_pixel(uint32_t shade_page, uint32_t index)
{
    uint8_t texel = texture_byte_fs(index);

    /* MOV AL replaces the low byte; it does not add it to the page address. */
    return EF_U8((shade_page & 0xffffff00u) | texel);
}

/* 0xd3cb0. Original entry owns 248 bytes; its computed RET continues into
 * the eight unrolled pixel bodies at d3d70..d3def and joins d3df0..d3dff.
 * The width-one endpoint path is d3e00..d3e41. Only EBP is preserved. */
void draw_textured_span_d3cb0(void)
{
    int32_t left = EF_S16(0x00485feau);
    int32_t width = (int32_t)EF_S16(0x00485fd2u) - left;
    ef_addr32 destination;
    uint32_t shade_page;
    uint32_t start_u;
    uint32_t start_v;
    uint32_t index;
    uint32_t step_u;
    uint32_t step_v;
    uint32_t fraction_v;
    uint32_t packed_step;
    uint32_t fraction_step_u;
    uint32_t fraction_step_v;
    uint32_t count;
    int32_t reciprocal;

    if (width <= 0)
        return;
    destination = EF_U32(0x0015b754u) +
        EF_U32(0x00163200u + (uint32_t)(int32_t)EF_S16(0x00485feeu) * 4u) +
        (uint32_t)left;
    shade_page = EF_U32(0x00159084u);
    start_u = EF_U32(0x00485ff0u);
    start_v = EF_U32(0x00485ff4u);
    index = ((start_u >> 16) & 255u) | ((start_v >> 8) & 0xff00u);
    if (width == 1) {
        EF_U8(destination) = texture_pixel(shade_page, index);
        index = ((EF_U32(0x00485fd8u) >> 16) & 255u) |
            ((EF_U32(0x00485fdcu) >> 8) & 0xff00u);
        EF_U8(destination + 1u) = texture_pixel(shade_page, index);
        return;
    }

    reciprocal = EF_S32(0x00159b7cu + (uint32_t)width * 4u);
    EF_U32(0x0015b920u) = (uint32_t)width >> 3;
    step_u = texture_step(EF_U32(0x00485fd8u) - start_u, reciprocal);
    step_v = texture_step(EF_U32(0x00485fdcu) - start_v, reciprocal);
    fraction_step_u = step_u << 16;
    fraction_step_v = step_v << 16;
    fraction_v = (start_v << 16) | (start_v >> 16);
    packed_step = (start_u << 16) | (step_u >> 16);
    packed_step = (packed_step & 0xffff00ffu) | ((step_v >> 8) & 0xff00u);

    /* The computed RET enters the last remainder+1 bodies first, then the
     * full eight-body loop. Preserve its counter writes and inclusive end. */
    count = ((uint32_t)width & 7u) + 1u;
    do {
        do {
            uint8_t pixel = texture_pixel(shade_page, index);
            uint64_t sum = (uint64_t)fraction_v + fraction_step_v;
            uint32_t high;
            uint32_t low;

            fraction_v = (uint32_t)sum;
            high = ((index >> 8) + ((packed_step >> 8) & 255u) +
                    (uint32_t)(sum >> 32)) & 255u;
            sum = (uint64_t)packed_step + fraction_step_u;
            packed_step = (uint32_t)sum;
            low = (index + (packed_step & 255u) + (uint32_t)(sum >> 32)) & 255u;
            index = (high << 8) | low;
            EF_U8(destination++) = pixel;
        } while (--count != 0u);
        EF_U32(0x0015b920u) = EF_U32(0x0015b920u) - 1u;
        count = 8u;
    } while ((int32_t)EF_U32(0x0015b920u) >= 0);
}

/* 0xd3e50, 1015 bytes. Perspective-correct transparent span. No GPRs are
 * preserved. The sixteen-pixel block runs before testing the remaining count;
 * its caller supplies a sufficiently wide span. Keep this original order. */
void draw_perspective_transparent_span_d3e50(void)
{
    int32_t left = EF_S16(0x00485feau);
    ef_addr32 destination = EF_U32(0x00163200u +
        (uint32_t)(int32_t)EF_S16(0x00485feeu) * 4u) +
        EF_U32(0x0015b754u) + (uint32_t)left - 1u;
    int32_t width = (int32_t)EF_S16(0x00485fd2u) - left;
    uint32_t count;
    uint32_t difference;
    uint32_t u;
    uint32_t v;
    uint32_t shade_page;
    uint32_t pixel;

    if (width <= 0)
        return;
    count = (uint32_t)width + 1u;
    EF_U32(0x0015b90cu) = texture_idiv(0x7fffffffu, 0u, EF_U32(0x0015b8f4u));
    EF_U32(0x0015b910u) = texture_idiv(0x7fffffffu, 0u, EF_U32(0x0015b8f8u));
    difference = EF_U32(0x0015b910u) - EF_U32(0x0015b90cu);
    /* CDQ precedes SHL EAX,4: the high word is the unshifted sign. */
    EF_U32(0x0015b914u) = texture_idiv(difference << 4, 0u - (difference >> 31), count);
    EF_U32(0x0015b918u) = count;
    u = EF_U32(0x00485ff0u);
    v = EF_U32(0x00485ff4u);
    do {
        uint32_t depth_delta;
        uint32_t depth_span;
        uint32_t target_u;
        uint32_t target_v;

        EF_U32(0x0015b908u) = v;
        EF_U32(0x0015b904u) = u;
        EF_U32(0x0015b90cu) = EF_U32(0x0015b90cu) + EF_U32(0x0015b914u);
        depth_delta = texture_idiv(0x7fffffffu, 0u, EF_U32(0x0015b90cu)) - EF_U32(0x0015b8f4u);
        depth_span = EF_U32(0x0015b8f8u) - EF_U32(0x0015b8f4u);
        target_u = EF_U32(0x00485ff0u) + texture_product_divide(
            EF_U32(0x00485fd8u) - EF_U32(0x00485ff0u), depth_delta, depth_span);
        target_v = EF_U32(0x00485ff4u) + texture_product_divide(
            EF_U32(0x00485fdcu) - EF_U32(0x00485ff4u), depth_delta, depth_span);
        EF_U32(0x0015b900u) = (uint32_t)((int32_t)(target_v - v) >> 4);
        EF_U32(0x0015b8fcu) = (uint32_t)((int32_t)(target_u - u) >> 4);
        shade_page = EF_U32(0x00159084u) & 0xffffff00u;
        for (pixel = 0u; pixel != 16u; ++pixel) {
            uint8_t texel;

            ++destination;
            texel = texture_byte_fs(((u >> 16) & 255u) | ((v >> 8) & 0xff00u));
            if (texel != 0u)
                EF_U8(destination) = EF_U8(shade_page | texel);
            u += EF_U32(0x0015b8fcu);
            v += EF_U32(0x0015b900u);
        }
        count -= 16u;
    } while ((count & 0xfff0u) != 0u);
    /* AND CX,15 retains the high half of ECX. */
    count &= 0xffff000fu;
    if ((uint16_t)count == 0u)
        return;
    EF_U32(0x0015b904u) = u;
    EF_U32(0x0015b8fcu) = texture_divide(EF_U32(0x00485fd8u) - u, count);
    EF_U32(0x0015b900u) = texture_divide(EF_U32(0x00485fdcu) - v, count);
    shade_page = EF_U32(0x00159084u) & 0xffffff00u;
    do {
        uint8_t texel;

        ++destination;
        texel = texture_byte_fs(((u >> 16) & 255u) | ((v >> 8) & 0xff00u));
        if (texel != 0u)
            EF_U8(destination) = EF_U8(shade_page | texel);
        u += EF_U32(0x0015b8fcu);
        v += EF_U32(0x0015b900u);
        count = (count & 0xffff0000u) | (uint16_t)(count - 1u);
    } while ((uint16_t)count != 0u);
}

/* The shared d42b1 affine body, including the nonzero-texel tail d4350.
 * Preserve packed ADD/ADC byte carries and the first OR AL,FS:[BX]. */
static void draw_transparent_affine_span(void)
{
    int32_t left = EF_S16(0x00485feau);
    int32_t width = (int32_t)EF_S16(0x00485fd2u) - left;
    ef_addr32 destination;
    uint32_t step_u;
    uint32_t step_v;
    uint32_t packed_u;
    uint32_t packed_v;
    uint32_t shade_page;
    uint32_t accumulator;
    uint32_t count;

    if (width <= 0)
        return;
    destination = EF_U32(0x0015b754u) +
        EF_U32(0x00163200u + (uint32_t)(int32_t)EF_S16(0x00485feeu) * 4u) + (uint32_t)left;
    step_u = texture_divide(EF_U32(0x00485fd8u) - EF_U32(0x00485ff0u), (uint32_t)width);
    step_v = texture_divide(EF_U32(0x00485fdcu) - EF_U32(0x00485ff4u), (uint32_t)width);
    packed_u = (EF_U32(0x00485ff0u) << 16) | (EF_U32(0x00485ff0u) >> 16);
    packed_u = (packed_u & 0xffff00ffu) | ((EF_U32(0x00485ff4u) >> 8) & 0xff00u);
    packed_v = (EF_U32(0x00485ff4u) << 16) | ((step_u >> 16) & 255u) | ((step_v >> 8) & 0xff00u);
    step_u <<= 16;
    step_v <<= 16;
    shade_page = EF_U32(0x00159084u);
    accumulator = shade_page & 255u;
    shade_page &= 0xffffff00u;
    count = (uint32_t)width + 1u;
    do {
        uint64_t sum;

        accumulator |= texture_byte_fs(packed_u & 0xffffu);
        if (accumulator != 0u) {
            EF_U8(destination) = EF_U8(shade_page | accumulator);
            accumulator = 0u;
        }
        ++destination;
        sum = (uint64_t)packed_u + step_u;
        packed_u = (uint32_t)sum;
        packed_u = (packed_u & 0xffffff00u) |
            ((packed_u + (packed_v & 255u) + (uint32_t)(sum >> 32)) & 255u);
        sum = (uint64_t)packed_v + step_v;
        packed_v = (uint32_t)sum;
        packed_u = (packed_u & 0xffff00ffu) |
            ((((packed_u >> 8) + ((packed_v >> 8) & 255u) + (uint32_t)(sum >> 32)) & 255u) << 8);
    } while (--count != 0u);
}

/* 0xd4250, 250 owned bytes. Wide/deep spans call d3e50; other paths jump
 * into d42b1 using this entry's saved EBP, not an early return. */
void draw_transparent_span_d4250(void)
{
    int32_t a;
    int32_t b;
    uint32_t difference;
    uint32_t magnitude;

    if ((int16_t)(EF_U16(0x00485fd2u) - EF_U16(0x00485feau)) < 16)
        goto affine;
    a = EF_S32(0x00485ffcu) >> 8;
    b = EF_S32(0x00485fe4u) >> 8;
    EF_S32(0x0015b8f4u) = a;
    EF_S32(0x0015b8f8u) = b;
    if (a >= 0x20000 || b >= 0x20000)
        goto affine;
    difference = (uint32_t)a - (uint32_t)b;
    magnitude = (difference ^ (0u - (difference >> 31))) + (difference >> 31);
    if ((int32_t)magnitude > 0x800) {
        draw_perspective_transparent_span_d3e50();
        return;
    }
affine:
    draw_transparent_affine_span();
}
