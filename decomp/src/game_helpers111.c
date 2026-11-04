/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Range check of a tracked target and the text-mode getc. */

/* 0x3df8, 91 bytes. Forget the tracked target (*current = 0) when the new position is missing or farther than limit * 32 from it. */
void drop_target_if_far_3df8(ef_addr32 position, ef_addr32 current, uint32_t limit)
{
    if (position == 0) {
        EF_U32(current) = 0;
        return;
    }
    if (EF_U32(current) != 0) {
        ef_addr32 other = EF_U32(current);
        uint32_t dz = abs_a28f2((int32_t)(EF_U32(position + 8u) - EF_U32(other + 8u)));
        uint32_t dy = abs_a28f2((int32_t)(EF_U32(position + 4u) - EF_U32(other + 4u)));
        uint32_t dx = abs_a28f2((int32_t)(EF_U32(position) - EF_U32(other)));

        if ((uint32_t)vector_length_3c74((int32_t)dx, (int32_t)dy, (int32_t)dz) > (limit << 5))
            EF_U32(current) = 0;
    }
}

/* 0x10f107, 137 bytes. fgetc: read the next byte of a stream (refilling when the count runs out); in text mode skip a CR and treat Ctrl-Z as end of file. */
int32_t stream_fgetc_10f107(ef_addr32 stream)
{
    int32_t ch;

    if ((EF_U8(stream + 0xcu) & 1u) == 0) {
        guest_fn_ebe6d(4);
        EF_U8(stream + 0xcu) = (uint8_t)(EF_U8(stream + 0xcu) | 0x20u);
        ch = -1;
    } else {
        EF_S32(stream + 4u) = EF_S32(stream + 4u) - 1;
        if (EF_S32(stream + 4u) < 0) {
            ch = stream_getc(stream);
        } else {
            ef_addr32 p = EF_U32(stream);
            ch = EF_U8(p);
            EF_U32(stream) = p + 1u;
        }
    }
    if ((EF_U8(stream + 0xcu) & 0x40u) == 0) {
        if (ch == 0xd) {
            EF_S32(stream + 4u) = EF_S32(stream + 4u) - 1;
            if (EF_S32(stream + 4u) < 0) {
                ch = stream_getc(stream);
            } else {
                ef_addr32 p = EF_U32(stream);
                ch = EF_U8(p);
                EF_U32(stream) = p + 1u;
            }
        }
        if (ch == 0x1a) {
            EF_U8(stream + 0xcu) = (uint8_t)(EF_U8(stream + 0xcu) | 0x10u);
            ch = -1;
        }
    }
    return ch;
}
