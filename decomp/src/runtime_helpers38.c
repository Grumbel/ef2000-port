/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Stream flush. */

/* 0xfa8a0, 191 bytes. fflush: write out a dirty write buffer, or seek back over unread input; resets the buffer position and returns 0 or -1. */
int32_t stream_flush_fa8a0(ef_addr32 stream)
{
    int32_t result = 0;

    if ((EF_U8(stream + 0xdu) & 0x10) != 0) {
        EF_U8(stream + 0xdu) &= 0xef;
        if ((EF_U8(stream + 0xcu) & 2) != 0 && EF_U32(stream + 8u) != 0) {
            int32_t written = (int32_t)guest_fn_110393_x(EF_U32(stream + 0x10u), EF_U32(stream + 8u), EF_U32(stream + 4u));
            if (written == -1) {
                EF_U8(stream + 0xcu) |= 0x20;
                result = written;
            } else if (written != (int32_t)EF_U32(stream + 4u)) {
                guest_fn_ebe6d(0xc);
                EF_U8(stream + 0xcu) |= 0x20;
                result = -1;
            }
        }
    } else if (EF_U32(stream + 8u) != 0) {
        EF_U8(stream + 0xcu) &= 0xef;
        if ((EF_U8(stream + 0xdu) & 0x20) == 0) {
            int32_t pos = (int32_t)EF_U32(stream + 4u);
            if (pos != 0)
                pos = (int32_t)guest_fn_fa95f(EF_U32(stream + 0x10u), (uint32_t)(-pos), 1u);
            if (pos == -1) {
                EF_U8(stream + 0xcu) |= 0x20;
                result = pos;
            }
        }
    }
    EF_U32(stream + 4u) = 0;
    EF_U32(stream) = EF_U32(stream + 8u);
    return result;
}
