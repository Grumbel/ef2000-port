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

/* 0x1065c8, 106 bytes. Find the first type-1 0x894-byte entry in the list of an object (array pointer +0x5c, count +0x60); if none is type 1 install the default block 0x194a44 at 0x4dd6b0 and return 0, else trace-call 0x102670 on it and return it. */
uint32_t find_open_entry_1065c8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 obj)
{
    uint32_t p, i = 0, n;
    (void)u0; (void)u1; (void)u2; (void)u3;
    guest_fn_101e70();
    n = EF_U32(obj + 0x60u);
    p = EF_U32(obj + 0x5cu);
    if ((int32_t)n > 0) {
        while (EF_U32(p + 4u) != 1u) {
            i++;
            p += 0x894u;
            if (!((int32_t)i < (int32_t)EF_U32(obj + 0x60u)))
                break;
        }
    }
    if (i == EF_U32(obj + 0x60u)) {
        uint32_t k;
        for (k = 0; k < 23u; k++)
            EF_U8(0x004dd6b0u + k) = EF_U8(0x00194a44u + k);
        guest_fn_101e78();
        return 0u;
    }
    p = EF_U32(obj + 0x5cu) + i * 0x894u;
    trace_call_102670(p);
    guest_fn_101e78();
    return p;
}
