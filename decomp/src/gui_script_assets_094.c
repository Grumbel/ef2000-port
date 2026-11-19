/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_gui_script_094.h"
#include "ef_gui_script_095.h"
#include "ef_mem.h"

#include <stdint.h>

enum { TOKEN = 0x485e08, SCRIPT_LINE = 0x161ea8,
       SCRIPT_MODE = 0x485dfc, RGB_TABLE = 0x425c8c };

static void report(ef_addr32 format, unsigned arguments)
{
    ef_addr32 callback = EF_U32(0x132a74u);
    uint32_t line = EF_U32(SCRIPT_LINE);
    if (arguments == 3u)
        guest_callptr3_x(callback, format, TOKEN, line);
    else
        guest_callptr2_x(callback, format, line);
}

/* Both commands require the high half-word of 0x485dfa to equal 12.
 * On empty tokens, the original reports an error but retains the previous
 * filename before constructing a path and invoking the graphics driver. */
static void parse_asset(uint32_t wrong_mode, uint32_t too_long,
                        uint32_t empty_token, uint32_t destination,
                        void (*select_buffer)(void))
{
    char path[100];
    ef_addr32 buffer = (ef_addr32)(uintptr_t)path;
    if ((int16_t)EF_U16(SCRIPT_MODE) != 12)
        report(wrong_mode, 2u);
    gui_script_next_token_f46cc();
    if (strlen_ds(TOKEN) > 8u)
        report(too_long, 3u);
    if (EF_U8(TOKEN) != 0u)
        copy_string_9d749(destination, TOKEN);
    else
        report(empty_token, 2u);

    build_default_path_9c980(buffer);
    strcat_9ca57(buffer, 0x192448u);
    strcat_9ca57(buffer, destination);
    strcat_9ca57(buffer, 0x192454u);
    select_buffer();
    start_driver_c34cc(buffer);
    leave_scroll_mode_c39ac();
}

/* Three unsigned 8-bit channels at each of 256 consecutive RGB entries.
 * Indexing is through signed 16-bit words, just as in the guest. */
static uint32_t rgb_total(int16_t index)
{
    ef_addr32 addr = RGB_TABLE + (uint32_t)(int32_t)index * 3u;
    return (uint32_t)EF_U8(addr) + EF_U8(addr + 1u) + EF_U8(addr + 2u);
}

/* 0xf5214 size 423. The GADGETRY asset selects the first graphics buffer,
 * starts the driver and searches for palette extremes starting at entry zero. */
void gui_script_gadgetry_f5214(void)
{
    int16_t brightest = 0;
    uint16_t index;
    parse_asset(0x1923c0u, 0x1923e8u, 0x192418u,
                0x485f5au, select_buffer_146e40);
    EF_U16(0x485e02u) = 0u;
    for (index = 0u; index < 256u; ++index) {
        uint32_t value = rgb_total((int16_t)index);
        if (value > rgb_total(brightest))
            brightest = (int16_t)index;
        if (value < rgb_total(EF_S16(0x485e02u)))
            EF_U16(0x485e02u) = index;
    }
    EF_U16(0x485e00u) = (uint16_t)brightest;
    EF_U16(0x485dfeu) = (uint16_t)brightest;
}

/* 0xf53bc size 200. NAME follows the same path build, but selects the
 * second graphics buffer and does not inspect or alter palette extremes. */
void gui_script_name_f53bc(void)
{
    parse_asset(0x19245cu, 0x192484u, 0x1924b4u,
                0x485f63u, select_buffer_146e44);
}
