/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* Documented segment-read adapter; native bindings supply a guest selector. */
#ifdef __WATCOMC__
static uint32_t release_data_selector(void);
#pragma aux release_data_selector = "xor eax,eax" "mov ax,ds" value [eax] modify [eax]
#else
extern uint32_t release_data_selector(void);
#endif

/* 0x1104b8, 272 bytes. Free a heap block: coalesce with neighbouring free blocks and update the heap's free list and statistics. */
void heap_free_1104b8(uint32_t ptr, uint32_t seg, uint32_t heap)
{
	uint32_t esi, edi, eax, ecx, edx;
	(void)seg;
	if (ptr == 0) {
		return;
	}
	esi = ptr - 4;
	eax = EF_U32(esi);
	if ((eax & 1) == 0) {
		return;
	}
	eax &= ~1u;
	edi = esi + eax;
	if ((EF_U32(edi) & 1) == 0) {
		uint32_t prev, next;
		if (edi == EF_U32(heap + 0xc)) {
			EF_U32(heap + 0xc) = esi;
		}
		eax += EF_U32(edi);
		EF_U32(esi) = eax;
		prev = EF_U32(edi + 4);
		next = EF_U32(edi + 8);
		EF_U32(prev + 8) = next;
		EF_U32(next + 4) = prev;
		EF_U32(heap + 0x1c)--;
		edi = next;
	} else {
		int search = 0;
		EF_U32(esi) = eax;
		edi = EF_U32(heap + 0xc);
		if (esi < edi) {
			if (esi > EF_U32(edi + 4)) {
				goto link;
			}
			edi = EF_U32(heap + 0x28);
			if (esi < edi) {
				goto link;
			}
			search = 1;
		} else {
			edi = EF_U32(edi + 8);
			if (esi < edi) {
				goto link;
			}
			edi = heap + 0x20;
			if (esi > EF_U32(heap + 0x24)) {
				goto link;
			}
			search = 1;
		}
		if (search) {
			ecx = EF_U32(heap + 0x1c) + 1;
			eax = EF_U32(heap + 0x18) / ecx;
			ecx--;
			if (eax < ecx) {
				edx = EF_U32(heap + 0x18) - ecx;
				eax += eax;
				if (edx <= ecx) {
					eax = 0;
				}
				edi = esi + EF_U32(esi);
				do {
					edx = EF_U32(edi);
					if ((edx & 1) == 0) {
						goto link;
					}
					if (edx == 0xffffffffu) {
						break;
					}
					edi += edx & ~1u;
					eax--;
				} while (eax != 0);
			}
			edi = EF_U32(heap + 0xc);
			if (esi < edi) {
				edi = EF_U32(heap + 0x28);
			}
			while (esi >= edi) {
				edi = EF_U32(edi + 8);
			}
		}
	}
link:
	edx = EF_U32(edi + 4);
	ecx = EF_U32(esi);
	{
		uint32_t t = edx;
		edx = edi;
		edi = t;
	}
	if (edi + EF_U32(edi) == esi) {
		ecx += EF_U32(edi);
		EF_U32(edi) = ecx;
		if (esi == EF_U32(heap + 0xc)) {
			EF_U32(heap + 0xc) = edi;
		}
		esi = edi;
	} else {
		EF_U32(heap + 0x1c)++;
		EF_U32(esi + 8) = edx;
		EF_U32(esi + 4) = edi;
		EF_U32(edi + 8) = esi;
		edi = edx;
		EF_U32(edi + 4) = esi;
	}
	EF_U32(heap + 0x18)--;
	if (esi < EF_U32(heap + 0xc) && ecx > EF_U32(heap + 0x10)) {
		EF_U32(heap + 0x10) = ecx;
	}
	if (ecx > EF_U32(heap + 0x14)) {
		EF_U32(heap + 0x14) = ecx;
	}
}

/* 0xfa9d2, 94 bytes. Find the heap that owns the pointer, free it there and update the high-water mark. */
void heap_free_find_fa9d2(uint32_t ptr)
{
	uint32_t ecx = EF_U32(0x16464c);
	for (;;) {
		uint32_t edx = EF_U32(ecx + 8);
		if (edx == 0) {
			break;
		}
		if (!(ecx > ptr) && ptr < edx) {
			break;
		}
		ecx = EF_U32(ecx + 8);
	}
	heap_free_1104b8(ptr, release_data_selector(), ecx);
	if (ecx != EF_U32(0x164650)) {
		if (EF_U32(ecx + 0x14) > EF_U32(0x164654)) {
			EF_U32(0x164654) = EF_U32(ecx + 0x14);
		}
	}
	EF_U8(0x4dd8a1) = 0;
}
