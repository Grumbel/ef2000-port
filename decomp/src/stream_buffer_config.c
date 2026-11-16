/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xf22af, 103 bytes. EAX stream, EDX buffer, EBX mode, ECX size;
 * EAX status, ECX scratch, EBX/EDX/ESI/EDI/EBP preserved, plain RET.
 * Sole caller 1115af forwards EAX and restores its own EBX/ECX. */
int32_t set_file_buffer_f22af(ef_addr32 file, ef_addr32 buffer,
                            uint32_t mode, uint32_t size)
{
    uint8_t flags;
    uint32_t state;

    if (size > 0x7fffffffu)
        return -1;
    if (mode != 0x100u && mode != 0x200u && mode != 0x400u)
        return -1;
    if (buffer != 0u && size == 0u)
        return -1;
    update_flags_fa7d2(file);
    if (size != 0u)
        EF_U32(file + 0x14u) = size;
    flags = EF_U8(file + 0xdu);
    EF_U32(file + 8u) = buffer;
    EF_U8(file + 0xdu) = (uint8_t)(flags & 0xf8u);
    state = EF_U32(file + 0xcu);
    EF_U32(file) = buffer;
    EF_U32(file + 0xcu) = state | mode;
    if (buffer == 0u)
        (void)alloc_stream_buffer_10f071(file);
    return 0;
}
