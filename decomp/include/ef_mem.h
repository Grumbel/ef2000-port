/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_MEM_H
#define EF_MEM_H

#include "ef_guest_memory.h"

/* Little-endian, possibly unaligned guest memory lvalues (x86 only). */
#define EF_U8(a)  (*(uint8_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U16(a) (*(uint16_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_U32(a) (*(uint32_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_S16(a) (*(int16_t *)ef_guest_memory_ptr((ef_addr32)(a)))
#define EF_S32(a) (*(int32_t *)ef_guest_memory_ptr((ef_addr32)(a)))

#endif
