/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Keyframe blends, packet buffer, state file header and device start, 0xa1b54..0xa4cb4.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0xa1b54, 157 bytes. Create the state file and write its 12-byte header. */
void create_state_file_a1b54(ef_addr32 name, uint16_t version)
{
    uint16_t header[6];

    if (EF_U32(0x00132b18u) != 0xffffffffu)
        ui_report_missing_record(0x0018b374u);
    EF_U32(0x00132b18u) = guest_fn_a7948(name);
    if (EF_U32(0x00132b18u) == 0xffffffffu)
        ui_report_missing_record(0x0018b388u);
    header[0] = 0xc;
    header[1] = 0xb;
    header[2] = 0x1d;
    header[3] = 0x2b;
    header[4] = 0xd;
    header[5] = version;
    write_checked_a1b28((uint32_t)(uintptr_t)header, 0xc);
    EF_U32(0x00417160u) = 1;
    EF_U8(0x00132b1cu) = 1;
}

/* 0xa1bf4, 189 bytes. Open the state file and validate its 12-byte header. */
void open_state_file_a1bf4(ef_addr32 name, uint16_t version)
{
    uint16_t header[6];

    if (EF_U32(0x00132b18u) != 0xffffffffu)
        ui_report_missing_record(0x0018b374u);
    EF_U32(0x00132b18u) = guest_fn_a7900(name);
    if (EF_U32(0x00132b18u) == 0xffffffffu) {
        EF_U32(0x00417160u) = 0xffffffffu;
        return;
    }
    read_checked_a1afc((uint32_t)(uintptr_t)header, 0xc);
    if (header[0] != 0xc || header[1] != 0xb || header[2] != 0x1d || header[3] != 0x2b
        || header[4] != 0xd || header[5] != version) {
        guest_fn_a1cb4();
        EF_U32(0x00417160u) = 0xffffffffu;
        return;
    }
    read_checked_a1afc(0x00417160u, 4);
    EF_U8(0x00132b1cu) = 2;
}

/* 0xa2188, 186 bytes. Blend two 3-word keys by a 14-bit weight (linear). */
void blend_linear_a2188(ef_addr32 key)
{
    int16_t weight = (int16_t)EF_U16(key + 4u);
    int32_t inverse = (int16_t)(0x4000 - weight);
    int32_t w = weight;

    EF_U16(key + 8u) = (uint16_t)(((int32_t)EF_U32(key + 0xcu) >> 16) * inverse / 0x4000
                                  + ((int32_t)EF_U32(key + 0x12u) >> 16) * w / 0x4000);
    EF_U16(key + 0xau) = (uint16_t)(((int32_t)EF_U32(key + 0xeu) >> 16) * inverse / 0x4000
                                    + ((int32_t)EF_U32(key + 0x14u) >> 16) * w / 0x4000);
    EF_U16(key + 0xcu) = (uint16_t)(((int32_t)EF_U32(key + 0x10u) >> 16) * inverse / 0x4000
                                    + ((int32_t)EF_U32(key + 0x16u) >> 16) * w / 0x4000);
}

/* 0xa2434, 203 bytes. Blend two 3-word keys by a 9-bit weight (quadratic). */
void blend_quadratic_a2434(ef_addr32 key)
{
    int32_t t = (int32_t)EF_U16(key + 4u) / 32;
    int32_t square = t * t;
    int32_t inverse_square = (0x200 - t) * (0x200 - t);

    EF_U16(key + 8u) = (uint16_t)(((int32_t)EF_U32(key + 0xcu) >> 16) * inverse_square / 0x40000
                                  + ((int32_t)EF_U32(key + 0x12u) >> 16) * square / 0x40000);
    EF_U16(key + 0xau) = (uint16_t)(((int32_t)EF_U32(key + 0xeu) >> 16) * inverse_square / 0x40000
                                    + ((int32_t)EF_U32(key + 0x14u) >> 16) * square / 0x40000);
    EF_U16(key + 0xcu) = (uint16_t)(((int32_t)EF_U32(key + 0x10u) >> 16) * inverse_square / 0x40000
                                    + ((int32_t)EF_U32(key + 0x16u) >> 16) * square / 0x40000);
}

/* 0xa3f74, 105 bytes. Append a packet to the outgoing buffer, flushing first when it would overflow. */
void append_packet_a3f74(ef_addr32 bytes, uint16_t size)
{
    int32_t limit = (int32_t)EF_U16(0x001357acu) - 1;
    int32_t length = size;

    if (length > limit) {
        guest_fatal_arg3(0x0018cf1cu, (uint32_t)length, (uint32_t)limit, 0);
        return;
    }
    if (length + (int32_t)EF_U16(0x00136bd0u) > limit)
        guest_fn_a3f30();
    guest_fn_e9a24(size, bytes, EF_U32(0x00136dccu) + EF_U16(0x00136bd0u));
    EF_U16(0x00136bd0u) = (uint16_t)(EF_U16(0x00136bd0u) + size);
}

/* 0xa4138, 101 bytes. Update the frame delta (clamped to 100) and clear the frame counters. */
void update_frame_delta_a4138(void)
{
    uint16_t previous = EF_U16(0x001357d6u);
    uint16_t current;
    uint16_t delta;

    EF_U16(0x001357d8u) = previous;
    current = (uint16_t)(((uint32_t (*)(void))EF_U32(0x0015b620u))() - EF_U16(0x00136dd4u));
    EF_U16(0x001357d6u) = current;
    delta = (uint16_t)(current - previous);
    if (delta > 0x64)
        delta = 0x64;
    EF_U8(0x0013581cu) = 1;
    EF_U32(0x00135830u) = 0;
    EF_U32(0x00135834u) = 0;
    EF_U32(0x00135838u) = 0;
    EF_U16(0x001357d8u) = delta;
}

/* 0xa41dc, 169 bytes. Start the device; returns true when it is running. */
uint8_t start_device_a41dc(void)
{
    uint8_t mode;

    if (EF_U8(0x0013578cu) == 0)
        ui_report_missing_record(0x0018cf60u);
    if (EF_U8(0x00135798u) != 0)
        return 1;
    if (EF_U8(0x00136bc8u) == 0) {
        start_device_a42d0();
        EF_U32(0x00136bccu) = ((uint32_t (*)(void))EF_U32(0x0015b620u))() + 10u;
    }
    guest_fn_a4104();
    mode = EF_U8(0x001357c8u);
    if (mode == 0) {
        ef_addr32 flag;

        if ((guest_fn_a2f70() & 0xffu) == 0)
            return 0;
        stop_device_a42f4();
        flag = EF_U32(0x00135794u);
        if (flag != 0)
            EF_U8(flag) = 1;
        EF_U8(0x00135798u) = 1;
        return 1;
    }
    if (mode == 1) {
        ef_addr32 flag = EF_U32(0x00135794u);

        EF_U8(0x00136bc9u) = 1;
        if (flag != 0)
            EF_U8(flag) = 1;
        EF_U8(0x00135798u) = 1;
        return 1;
    }
    return 0;
}

/* 0xa4cb4, 217 bytes. Activate widget slot idx: run its callback and queue it for redraw. */
void activate_widget_a4cb4(uint16_t index)
{
    ef_addr32 widget;

    if (index == 0 || index > EF_U16(0x0041eed8u))
        return;
    if ((EF_U8(EF_U32(0x0041edfcu + (uint32_t)index * 4u) + 0x3bu) & 4u) != 0)
        return;
    guest_fn_a4a94(0);
    widget = EF_U32(0x0041edfcu + (uint32_t)index * 4u);
    if (EF_U32(widget + 0xb9u) != 0)
        ((void (*)(void))EF_U32(widget + 0xb9u))();
    guest_fn_a499c(index);
    if (EF_U8(EF_U32(0x0041edfcu + (uint32_t)index * 4u) + 0xc1u) == 1) {
        uint16_t count;

        EF_U8(0x0041f2e0u) = 1;
        count = EF_U16(0x0041eed6u);
        EF_U16(0x0041eed6u) = (uint16_t)(count + 1u);
        EF_U16(0x0041ed84u + (uint32_t)count * 2u) = index;
        if (EF_U16(0x0041eed6u) > 0x10)
            guest_fatal_arg(0x0018cfc8u, 0x10);
    }
    EF_U8(0x0013725cu) = 1;
    guest_fn_a5ef8(index);
    EF_U8(0x0013725du) = 0;
    EF_U8(0x0013725cu) = 0;
}
