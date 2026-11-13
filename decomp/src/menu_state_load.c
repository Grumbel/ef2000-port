/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

static void report_missing_state(ef_addr32 path)
{
#ifdef __WATCOMC__
    typedef void (__cdecl *Reporter)(ef_addr32, ef_addr32);
#else
    typedef void (*Reporter)(ef_addr32, ef_addr32);
#endif
    ((Reporter)(uintptr_t)EF_U32(0x132a74u))(0x18d930u, path);
}

/* 0xaad7c, 238 bytes. Load the named saved state EAX: the name is cut at its first '.' (in place), formatted with 0x18d7e8 into a lookup buffer and the data file name is built from the default path (0xa7b70), the optional 0xa7b80 prefix and the name (formats 0x18d7cc and 0x18d7dc). The lookup name is opened with 0xa7900, falling back to the data file name (0x9ccb8, error format 0x18d930). The file is read (0xaa5c4) and closed (0x9ce84), the follow-up steps 0xaa3dc, 0xaa4d4 and 0xaa350 run, dword 0x419200 is copied to 0x4191f8 and byte 0x18d544 to 0x419160. Returns 1. */
uint32_t load_named_state_aad7c(ef_addr32 name)
{
	struct { char data[0xb4]; char look[0x64]; } frame;
	typedef char frame_size_matches[sizeof(frame) == 0x118 ? 1 : -1];
	(void)sizeof(frame_size_matches);
	uint32_t i = 0u, h;

	while (EF_U8(name + i) != 0x2eu && EF_U8(name + i) != 0u)
		i++;
	EF_U8(name + i) = 0u;
	guest_sprintf_a4342((ef_addr32)(uintptr_t)frame.look, 0x0018d7e8u, name);
	if (call_9c858_18d57c() != 0u) {
		uint32_t r80 = call_9c858_18d57c();
		uint32_t r70 = guest_fn_a7b70_x();
		guest_sprintf_a4342((ef_addr32)(uintptr_t)frame.data, 0x0018d7ccu, r70, r80, name);
	} else {
		guest_sprintf_a4342((ef_addr32)(uintptr_t)frame.data, 0x0018d7dcu, guest_fn_a7b70_x(), name);
	}
	h = guest_fn_a7900((ef_addr32)(uintptr_t)frame.look);
	if (h == 0xffffffffu) {
		h = open_resource_9ccb8((uint32_t)(uintptr_t)frame.data);
		if (h == 0xffffffffu)
			report_missing_state((ef_addr32)(uintptr_t)frame.data);
	}
	guest_fn_aa5c4_x(h);
	guest_fn_9ce84(h);
	guest_fn_aa3dc_x();
	guest_fn_aa4d4_x();
	guest_fn_aa350_x();
	EF_U32(0x004191f8u) = EF_U32(0x00419200u);
	EF_U8(0x00419160u) = EF_U8(0x0018d544u);
	return 1u;
}
