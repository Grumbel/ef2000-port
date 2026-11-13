/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* ASM exception: read the actual guest DS limit. Native code must supply
 * the guest limit; host segment limits are not interchangeable. */
#ifdef __WATCOMC__
static uint32_t heap_segment_end(void);
#pragma aux heap_segment_end = "xor eax,eax" "mov ax,ds" "lsl eax,eax" "inc eax" value [eax] modify [eax]
#else
extern uint32_t heap_segment_end(void);
#endif

/* 0x10d63b, 78 bytes. Size of the last free block when it reaches the
 * current segment end, else zero. Refresh the end for extender modes 2..8. */
uint32_t guest_fn_10d63b(void)
{
    ef_addr32 heap = EF_U32(0x16464cu);
    ef_addr32 last;
    uint32_t end;
    uint8_t mode;
    if (heap == 0u) return 0u;
    last = EF_U32(heap + 0x24u);
    end = last + EF_U32(last) + 4u;
    mode = EF_U8(0x132a5eu);
    if (mode >= 2u && mode <= 8u && EF_U8(0x132a60u) == 0u)
        EF_U32(0x132a30u) = heap_segment_end();
    if (end == EF_U32(0x132a30u)) return EF_U32(last);
    return 0u;
}
