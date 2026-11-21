/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_platform.h"

#define CPU_SIGNATURE     0x00161146u /* dword, CPUID leaf 1 EAX */
#define CPU_VENDOR        0x0016114au /* 12 bytes, CPUID leaf 0 */
#define CPU_FAMILY        0x00161156u /* 3 = 386, 4 = 486, else CPUID family */
#define CPU_MODEL         0x00161157u
#define CPU_STEPPING      0x00161158u
#define CPU_HAS_CPUID     0x00161159u
#define CPU_IS_INTEL      0x0016115bu
#define CPU_INTEL_VENDOR  0x0016115du /* "GenuineIntel" */

#define EFLAGS_AC 0x00040000u
#define EFLAGS_ID 0x00200000u

/* 0xf2819, 185 bytes: classify the CPU. A CPU that cannot toggle EFLAGS.AC
 * is a 386; one that cannot toggle EFLAGS.ID is a 486 without CPUID. With
 * CPUID the vendor string, the signature and its family, model and stepping
 * are stored. The vendor test compares only the first byte ("G"), as the
 * original REPZ CMPSB runs with a count of one. */
void detect_cpu_f2819(void)
{
	uint32_t signature;

	EF_U8(CPU_FAMILY) = 3;
	if (platform_toggle_eflags(EFLAGS_AC) == 0)
		return;
	EF_U8(CPU_FAMILY) = 4;
	if (platform_toggle_eflags(EFLAGS_ID) == 0)
		return;
	EF_U8(CPU_HAS_CPUID) = 1;
	platform_cpuid_vendor(CPU_VENDOR);
	if (EF_U8(CPU_VENDOR) == EF_U8(CPU_INTEL_VENDOR))
		EF_U8(CPU_IS_INTEL) = 1;
	signature = platform_cpuid_signature();
	EF_U32(CPU_SIGNATURE) = signature;
	EF_U8(CPU_FAMILY) = (uint8_t)((signature & 0xf00u) >> 8);
	EF_U8(CPU_STEPPING) = (uint8_t)(signature & 0x0fu);
	EF_U8(CPU_MODEL) = (uint8_t)((signature & 0xf0u) >> 4);
}
