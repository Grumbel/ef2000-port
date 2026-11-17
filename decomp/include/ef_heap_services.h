/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#ifndef EF_HEAP_SERVICES_H
#define EF_HEAP_SERVICES_H

#include "ef_types.h"

/* Explicit semantic boundaries for the allocator drafts, not implementations
 * of DOS/DPMI or declarations of their interrupt register ABI. */
typedef struct ef_heap_dpmi_result {
    ef_addr32 address;          /* returned BX:CX */
    uint16_t handle_low;        /* returned DI */
    uint16_t handle_high;       /* returned SI */
    uint32_t carry;
} ef_heap_dpmi_result;

void ef_heap_allocate_dpmi(uint32_t size, ef_heap_dpmi_result *result);
/* DOS AH48 result after the original RCL/ROR pair: bit 31 is carry,
 * bits 0..30 are returned EAX. Do not substitute an X-32 linear pointer. */
uint32_t ef_heap_allocate_dos(uint32_t paragraphs);
void ef_heap_release_dos(uint32_t selector); /* ES restored afterward */
/* INT31 AX0006: combine returned CX:DX even if service carry is set. */
ef_addr32 ef_heap_selector_base(uint32_t selector);

/* INT21 full EAX=0x80004800, EBX=size; returned EAX is a pointer.
 * Its extender-specific implementation is still unrecovered. */
typedef struct ef_heap_extended_result {
    ef_addr32 address;
    uint32_t carry;
} ef_heap_extended_result;
void ef_heap_allocate_extended(uint32_t size, ef_heap_extended_result *result);

#endif
