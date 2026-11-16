/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint16_t x, y, z, pitch, yaw, roll, identifier, page, channel;
} ef_page_point_request;

EF_STATIC_ASSERT(point_request_pitch, offsetof(ef_page_point_request, pitch) == 6);
EF_STATIC_ASSERT(point_request_identifier, offsetof(ef_page_point_request, identifier) == 12);
EF_STATIC_ASSERT(point_request_channel, offsetof(ef_page_point_request, channel) == 16);
EF_STATIC_ASSERT(point_request_size, sizeof(ef_page_point_request) == 18);

/* The callback consumes EAX=request, returns AL, and may trash scratch GPRs. */
#ifdef __WATCOMC__
static uint32_t call_page_point_request(ef_addr32 target, ef_addr32 request);
#pragma aux call_page_point_request parm [eax] [edx] value [eax] \
    modify [eax ebx ecx edx] = \
    "mov ecx,eax" \
    "mov eax,edx" \
    "call ecx"
#else
extern uint32_t call_page_point_request(ef_addr32 target, ef_addr32 request);
#endif

/* 0xa1590, 479 bytes. AX=point, DX=channel, EBX=angles, CX=spread.
 * Call sites9ebea/9ec08 retain ESI, ignore scratch registers and EAX.
 * Original saves ESI/EDI/EBP; the request callback gets one EAX pointer. */
void submit_page_point_a1590(uint32_t point_index, uint32_t channel_index,
    ef_addr32 angles, uint32_t spread_word)
{
    ef_page_point_request storage;
    ef_addr32 request = (ef_addr32)(uintptr_t)&storage;
    ef_addr32 record, point;
    uint16_t x, y, z;
    int32_t spread = (int32_t)(int16_t)spread_word;
    int32_t pitch_jitter = 0, yaw_jitter = 0;
    int32_t half;
    uint32_t flags;

    record = EF_U32(0x00417174u) + 0x0bu
        + (uint32_t)EF_U8(EF_U32(0x0041717cu) + 1u) * 29u
        + (uint32_t)(int32_t)(int16_t)channel_index * 2u;
    if ((EF_U16(record) & 0x8000u) != 0u)
        return;
    if ((int8_t)EF_U8(record + 4u) >= (int8_t)EF_U8(record + 2u))
        return;
    EF_U8(record + 7u) = (uint8_t)((EF_S32(record + 3u) >> 24)
        * (int32_t)EF_S16(EF_U32(0x00132ac8u)) / 100);
    if (spread != 0) {
        /* Signed divide truncates toward zero, including negative spread. */
        pitch_jitter = (int32_t)(uint16_t)step_generator_a45b3() % spread;
        half = spread / 2;
        pitch_jitter -= half;
        yaw_jitter = (int32_t)(uint16_t)step_generator_a45b3() % spread - half;
    }
    point = EF_U32(0x0041717cu) + 0x0au
        + (uint32_t)(int32_t)(int16_t)point_index * 6u;
    EF_U16(0x00417226u) = EF_U16(point + 2u);
    guest_callptr_eaxedx_x(EF_U32(0x00132af4u),
        (uint32_t)(int32_t)EF_S16(point),
        (uint32_t)(EF_S32(point + 2u) >> 16));
    x = (uint16_t)(EF_U16(0x00417224u) + EF_U16(0x00417194u));
    y = (uint16_t)(EF_U16(0x00417226u) + EF_U16(0x00417196u));
    z = (uint16_t)(EF_U16(0x00417228u) + EF_U16(0x00417198u));
    EF_U16(0x00417224u) = x;
    x = (uint16_t)(x - EF_U16(0x00159094u));
    EF_U16(0x00417226u) = y;
    EF_U16(request) = x;
    y = (uint16_t)(y - EF_U16(0x00159098u));
    EF_U16(0x00417228u) = z;
    EF_U16(request + 2u) = y;
    EF_U16(request + 4u) = (uint16_t)(z - EF_U16(0x0015909cu));
    EF_U16(request + 6u) = (uint16_t)(EF_U16(angles) + pitch_jitter);
    yaw_jitter = (int32_t)(int16_t)yaw_jitter + (EF_S32(angles) >> 16);
    flags = EF_U8(EF_U32(0x00417174u) + 0x0au);
    flags = ((flags & 0x7fu) >> 5) << 14;
    EF_U16(request + 8u) = (uint16_t)((uint32_t)yaw_jitter + flags);
    EF_U16(request + 10u) = EF_U16(angles + 4u);
    EF_U16(request + 12u) = EF_U16(record) & 0x03ffu;
    EF_U16(request + 14u) = EF_U16(0x0041722au);
    EF_U16(request + 16u) = (uint16_t)channel_index;
    if ((uint8_t)call_page_point_request(EF_U32(0x00132ad4u), request) != 0u)
        EF_U8(record + 4u) = (uint8_t)(EF_U8(record + 4u) + 1u);
}
