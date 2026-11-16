/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux guest_callptr1_x parm [eax] [edx] modify [eax ecx edx]
#endif

/* 0xa292c, 26 bytes. No inputs; AL result, six preserved registers,
 * plain RET. Callers a3ebb/a42c4 test AL. */
uint8_t require_device_ready_a292c(void)
{
    if (EF_U8(0x00135798u) == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018cbe8u);
    /* The original reports ready even if the error callback returns. */
    return 1u;
}

/* 0xeb35c, 24 bytes. No inputs; EAX scratch, six preserved registers,
 * plain RET. Callers dc250/dc281 check before page callbacks. */
void check_error_flag_eb35c(void)
{
    if (EF_U8(0x00160795u) != 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x00160796u);
}
