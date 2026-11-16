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
