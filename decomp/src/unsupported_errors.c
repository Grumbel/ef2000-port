/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux guest_callptr1_x parm [eax] [edx] modify [eax ecx edx]
#pragma aux guest_callptr2_x parm [eax] [edx] [ebx] modify [eax ecx edx]
#endif

/* Five 17-byte entries: no inputs, AL result, plain RET.
 * Report first, then return zero even if the error callback returns. */
uint8_t unsupported_84650(void)
{
    guest_callptr1_x(EF_U32(0x00132a74u), 0x00188c4cu);
    return 0u;
}

uint8_t unsupported_query_e1730(void)
{
    guest_callptr1_x(EF_U32(0x00132a74u), 0x001906ccu);
    return 0u;
}

uint8_t unsupported_query_e1744(void)
{
    guest_callptr1_x(EF_U32(0x00132a74u), 0x001906f4u);
    return 0u;
}

uint8_t unsupported_query_e1758(void)
{
    guest_callptr1_x(EF_U32(0x00132a74u), 0x0019071cu);
    return 0u;
}

uint8_t unsupported_query_e1780(void)
{
    guest_callptr1_x(EF_U32(0x00132a74u), 0x0019075cu);
    return 0u;
}

/* 0xe1948,16 bytes. EAX argument, plain RET. The format precedes the
 * argument on the cdecl stack; callers e1efb/e1fc8 continue after return. */
void fatal_with_arg_e1948(uint32_t value)
{
    guest_callptr2_x(EF_U32(0x00132a74u), 0x00190810u, value);
}
