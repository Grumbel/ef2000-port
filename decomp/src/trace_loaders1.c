/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Resource loaders over the AIL helpers. */

static uint32_t load_resource_common(uint32_t name, uint32_t arg, uint32_t default_src,
                                     uint32_t (*parse)(uint32_t, uint32_t))
{
    uint32_t handle = guest_fn_105070_x(name, 0u);
    uint32_t size, data, result;
    uint32_t i;
    if (handle == 0u) {
        for (i = 0; i < 23u; i++)
            EF_U8(0x004dd6b0u + i) = EF_U8(default_src + i);
        return 0u;
    }
    size = file_size_by_name_105024(0u, 0u, 0u, 0u, name);
    data = trace_call_10242c(0u, 0u, 0u, 0u, handle, size);
    guest_fn_c3f80(handle);
    if (data == 0u)
        return 0u;
    result = parse(data, arg);
    if (result == 0u)
        trace_call_102488(data);
    return result;
}

/* 0x106464, 115 bytes. Load and parse a resource by name; on a missing file install the 23-byte default block at 0x194a04. */
uint32_t load_resource_106464(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t name, uint32_t arg)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    return load_resource_common(name, arg, 0x00194a04u, guest_fn_105e38_x);
}

/* 0x113a40, 115 bytes. Same with parser 0x1133f8 and default block 0x1954bc. */
uint32_t load_resource_113a40(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t name, uint32_t arg)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    return load_resource_common(name, arg, 0x001954bcu, guest_fn_1133f8_x);
}
