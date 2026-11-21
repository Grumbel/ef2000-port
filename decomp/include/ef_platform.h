/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#ifndef EF_PLATFORM_H
#define EF_PLATFORM_H

#include "ef_types.h"

/* Platform layer: services the game reaches through segment registers,
 * DOS/BIOS interrupts or the X-32 extender. Game code calls only these
 * named wrappers; the hybrid build implements them with the original
 * instruction, a native build supplies its own implementation. */

/* Make SELECTOR (the texture window, dword 0x485fa8) current in FS. The
 * textured span rasterizers around 0xd3a00..0xd5c00 read texels through FS;
 * 0xfc9b0 allocates the selector and 0xfc9f8 moves its base. */
void platform_select_texture_window(uint16_t selector);
#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux platform_select_texture_window = "mov fs,ax" parm [ax] modify exact []
#endif


/* Toggle MASK in EFLAGS and return the bits that actually changed (zero when
 * the CPU does not implement them). The flags stay toggled, as in the
 * original CPU probe 0xf2819: AC (0x40000) separates the 386 from the 486,
 * ID (0x200000) shows that CPUID exists. */
uint32_t platform_toggle_eflags(uint32_t mask);
#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux platform_toggle_eflags = \
	"pushfd" "pop eax" "mov ecx,eax" "xor eax,edx" "push eax" "popfd" \
	"pushfd" "pop eax" "xor eax,ecx" \
	parm [edx] value [eax] modify exact [eax ecx]
#endif

/* CPUID leaf 0: store the 12-byte vendor string (EBX, EDX, ECX) at OUT.
 * CPUID is emitted as bytes because -3r rejects the mnemonic. */
void platform_cpuid_vendor(ef_addr32 out);
#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux platform_cpuid_vendor = \
	"mov eax,0" "db 0fh,0a2h" "mov [esi],ebx" "mov [esi+4],edx" "mov [esi+8],ecx" \
	parm [esi] modify exact [eax ebx ecx edx]
#endif

/* CPUID leaf 1: the processor signature (family, model, stepping in EAX). */
uint32_t platform_cpuid_signature(void);
#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux platform_cpuid_signature = \
	"mov eax,1" "db 0fh,0a2h" \
	value [eax] modify exact [eax ebx ecx edx]
#endif

#endif
