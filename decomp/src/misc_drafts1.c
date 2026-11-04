/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Miscellaneous drafted helpers. */

/* 0x28744, 119 bytes. Sibling of 0x287bc: build the same file name, open it with 0xa7948, run 0xab324 with mode 2 and close the handle. */
void open_named_file_28744(void)
{
    uint8_t name[0x98];
    int32_t handle;
    ef_addr32 buf = (uint32_t)(uintptr_t)name;
    if (EF_U16(0x0015ee9au) == 0u)
        guest_sprintf_a4342(buf, 0x001823c0u, guest_fn_9c858(0x001822d8u), 0x00125208u);
    else
        guest_sprintf_a4342(buf, 0x001823c8u, 0x00125208u);
    handle = open_slot_a7948(0u, 0u, 0u, 0u, buf);
    if (handle != -1) {
        guest_fn_ab324((uint32_t)handle, 2u);
        close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
    }
}

/* 0x36100, 124 bytes, ret 8. Store a request block (0x19f34c.., flag 0x19f5aa) and run 0x35de0; reports errors through the 0x132a74 handler if a request is already pending or dx is above 0x1f6. */
void queue_request_36100(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s0, uint32_t s1)
{
    int32_t v = (int32_t)(int16_t)edx;
    if (EF_U8(0x0019f5aau) != 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00183178u);
    if ((uint32_t)v > 0x1f6u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018319cu);
    EF_U16(0x001357b2u) = (uint16_t)ecx;
    EF_U32(0x0019f34cu) = eax;
    EF_U8(0x0019f5afu) = 2u;
    EF_U32(0x0019f350u) = (uint32_t)v;
    EF_U8(0x0019f5a6u) = (uint8_t)ebx;
    EF_U8(0x0019f5acu) = (uint8_t)s0;
    EF_U8(0x0019f5aau) = 1u;
    EF_U8(0x0019f5adu) = (uint8_t)s1;
    guest_fn_35de0_x(0u, 0u, 0u, 0u, 0u, 0u);
}

/* 0xa8660, 129 bytes, ret 4. Dispatch on the mode byte at [[0x4191d0]+4]: 0 passes the arguments straight on to 0xa8578, 1 mirrors x against 0x13f using a table at 0x418780, 2 offsets x from a table at 0x4187d0. */
void draw_mode_dispatch_a8660(uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t index)
{
    uint8_t mode = EF_U8(EF_U32(0x004191d0u) + 4u);
    uint32_t col = ecx & 0xffu;
    if (mode == 0u) {
        guest_fn_a8578_x((uint32_t)(int32_t)(int16_t)edx, (uint32_t)(int32_t)(int16_t)ebx, col, eax);
    } else if (mode == 1u) {
        uint32_t entry = (index & 0xffff0000u) | EF_U16(0x00418780u + (uint32_t)(int32_t)(int16_t)index * 2u);
        uint32_t x = edx + (0x13fu - entry);
        guest_fn_a8578_x((uint32_t)(int32_t)(int16_t)x, (uint32_t)(int32_t)(int16_t)ebx, col, eax);
    } else if (mode == 2u) {
        uint16_t x = (uint16_t)(edx + EF_U16(0x004187d0u + (uint32_t)(int32_t)(int16_t)index * 2u));
        guest_fn_a8578_x((uint32_t)(int32_t)(int16_t)x, (uint32_t)(int32_t)(int16_t)ebx, col, eax);
    }
}
