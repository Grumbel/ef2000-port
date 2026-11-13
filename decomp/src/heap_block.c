/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"
#include <stddef.h>

typedef struct HeapLayout {
    uint32_t reserved[2], next, cursor, scanned, largest, allocated, freeCount;
    uint32_t sentinelSize, sentinelPrevious, sentinelNext;
} HeapLayout;
typedef char heap_layout_matches[
    sizeof(HeapLayout) == 0x2c && offsetof(HeapLayout, next) == 8 &&
    offsetof(HeapLayout, cursor) == 12 && offsetof(HeapLayout, scanned) == 16 &&
    offsetof(HeapLayout, largest) == 20 && offsetof(HeapLayout, allocated) == 24 &&
    offsetof(HeapLayout, freeCount) == 28 &&
    offsetof(HeapLayout, sentinelSize) == 32 &&
    offsetof(HeapLayout, sentinelNext) == 40 ? 1 : -1];

/* 0x110410, 163 bytes. First-fit allocation from a circular free list heap. */
uint32_t heap_alloc_110410(uint32_t size, uint32_t seg, ef_addr32 heap)
{
	uint32_t n, esi, edi, edx, next, remain;

	/* Flat guest-memory model: seg selects the same linear heap mapping.
	 * Non-flat selectors need a platform binding and are not verified. */
	(void)seg;
	if (size == 0)
		return 0;
	if (size + 7 < size)
		return 0;
	n = (size + 7) & ~3u;
	if (n < 12)
		n = 12;
	if (n > EF_U32(heap + 0x14))
		return 0;
	esi = EF_U32(heap + 0xc);
	edi = EF_U32(heap + 0x10);
	if (n <= edi) {
		esi = EF_U32(heap + 0x28);
		edi = 0;
	}
	for (;;) {
		edx = EF_U32(esi);
		if (n <= edx)
			break;
		if (edi < edx)
			edi = edx;
		esi = EF_U32(esi + 8);
		if (esi == heap + 0x20) {
			EF_U32(heap + 0x14) = edi;
			return 0;
		}
	}
	remain = edx - n;
	EF_U32(heap + 0x10) = edi;
	EF_U32(heap + 0x18)++;
	next = EF_U32(esi + 8);
	if (remain < 12) {
		uint32_t prev;

		EF_U32(heap + 0x1c)--;
		prev = EF_U32(esi + 4);
		EF_U32(prev + 8) = next;
		EF_U32(next + 4) = prev;
		EF_U32(heap + 0xc) = prev;
	} else {
		uint32_t rest = esi + n;
		uint32_t prev = EF_U32(esi + 4);

		EF_U32(heap + 0xc) = rest;
		EF_U32(rest) = remain;
		EF_U32(esi) = n;
		EF_U32(rest + 4) = prev;
		EF_U32(rest + 8) = next;
		EF_U32(prev + 8) = rest;
		EF_U32(next + 4) = rest;
	}
	EF_U32(esi) |= 1;
	return esi + 4;
}
