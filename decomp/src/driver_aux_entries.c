/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* e0bcc, 114 bytes: reset an active hardware device only when status bit
 * 7 is set, then optionally reopen the second device. The port and timer
 * operations remain explicit host callbacks. */
void audio_maybe_restart_device_e0bcc(void)
{
    uint32_t base;
    uint8_t value;
    if (EF_U8(0x13578cu) == 0u)
        return;
    base = EF_U32(0x15eed0u);
    if (!(audio_port_read_e0bcc(base + 6u) & 0x80u))
        return;
    value = audio_port_read_e0bcc(base + 4u);
    audio_port_write_e0bcc(base + 4u, (uint8_t)(value & ~1u));
    audio_wait_e0bcc(0x46u);
    value = audio_port_read_e0bcc(base + 4u);
    audio_port_write_e0bcc(base + 4u, (uint8_t)(value | 1u));
    if (audio_port_read_e0bcc(base + 6u) & 0x80u) {
        audio_open_e0bcc(0x1905fcu);
        audio_wait_e0bcc(0x46u);
        audio_device_e0bcc(0x15f57du);
    }
}

/* 117a0f, 147 bytes: select an indexed pixel converter based on source
 * layout, plane count, and flags. The original indirect call depends on an
 * extensive video-mode table, supplied via the host callback here. */
void video_dispatch_scanline_117a0f(ef_addr32 record, uint32_t row)
{
    uint32_t opts = EF_U32(record + 0x1cu);
    uint32_t mode = EF_U32(record + 0x18u);
    uint32_t flags = opts & 0x0fu;
    uint32_t source = EF_U32(record + 0x50u);
    uint32_t pitch = EF_U32(record + 0x48u);
    uint32_t dest, extra = record;
    if (mode == 2u || mode == 3u)
        flags |= 0x10u;
    if (mode == 1u || mode == 3u)
        flags |= 0x20u;
    if (EF_U32(record + 0x64u) > 1u)
        flags |= 0x40u;
    EF_U32(0x17e270u) = flags;
    if (opts & 0x20u) {
        uint32_t stride = EF_U32(record + 0x10u);
        uint32_t offset = (stride >> 1) * row;
        flags |= 0x08u;
        EF_U32(0x17e270u) = flags;
        dest = EF_U32(record + 0x2cu) + offset;
        extra = EF_U32(record + 0x2cu) + stride + offset;
    } else {
        dest = EF_U32(record + 0x2cu + row * 4u);
        if (opts & 8u)
            extra = dest + (EF_U32(record + 0x10u) >> 1);
    }
    video_scanline_callback_117a0f(EF_U32(0x1159c8u + flags * 4u),
                                  extra, pitch, source, dest);
}
