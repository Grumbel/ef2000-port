/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_gui_script_094.h"
#include "ef_mem.h"

#include <stdint.h>

enum { TOKEN = 0x485e08, SCRIPT_LINE = 0x161ea8,
       SCRIPT_MODE = 0x485dfc, RGB_TABLE = 0x425c8c };

static uint32_t guest(uint32_t target, uint32_t eax, uint32_t edx)
{
    return gui_script_guest_call_094(target, eax, edx);
}

static void report(uint32_t format, unsigned count)
{
    gui_script_report_094(format, count, TOKEN, EF_U32(SCRIPT_LINE));
}

static unsigned token_length(void)
{
    unsigned i = 0u;
    while (EF_U8(TOKEN + i) != 0u)
        ++i;
    return i;
}

static void copy_token(ef_addr32 destination)
{
    uint32_t i = 0u;
    do {
        EF_U8(destination + i) = EF_U8(TOKEN + i);
    } while (EF_U8(TOKEN + i++) != 0u);
}

/* Both commands require the high half-word of 0x485dfa to equal 12.
 * On empty tokens, the original reports an error but retains the previous
 * filename before constructing a path and invoking the graphics driver. */
static void parse_asset(uint32_t wrong_mode, uint32_t too_long,
                        uint32_t empty_token, uint32_t destination,
                        uint32_t select_driver)
{
    ef_addr32 temp;
    if ((int16_t)EF_U16(SCRIPT_MODE) != 12)
        report(wrong_mode, 2u);
    guest(0xf46ccu, 0u, 0u);
    if (token_length() > 8u)
        report(too_long, 3u);
    if (EF_U8(TOKEN) != 0u)
        copy_token(destination);
    else
        report(empty_token, 2u);

    /* Original path buffer lives in 100 bytes of guest stack. Host adapter
     * supplies a scratch guest address; the path and driver helpers remain
     * unresolved calls, not spoofed stack ABI definitions. */
    temp = gui_script_path_buffer_094();
    guest(0x9c980u, temp, 0x192448u);
    guest(0x9ca57u, temp, 0x192448u);
    guest(0x9ca57u, temp, destination);
    guest(0x9ca57u, temp, 0x192454u);
    guest(select_driver, 0u, 0u);
    guest(0xc34ccu, temp, 0u);
    guest(0xc39acu, 0u, 0u);
}

/* Three unsigned 8-bit channels at each of 256 consecutive RGB entries.
 * Indexing is through signed 16-bit words, just as in the guest. */
static uint32_t rgb_total(int16_t index)
{
    ef_addr32 addr = RGB_TABLE + (uint32_t)(int32_t)index * 3u;
    return (uint32_t)EF_U8(addr) + EF_U8(addr + 1u) + EF_U8(addr + 2u);
}

/* 0xf5214 size 423. The GADGETRY asset selects the first graphics buffer,
 * starts the driver and chooses brightest and darkest RGB palette entries.
 * Starting both searches from 485dfe (not index 0) matters on ties. */
void gui_script_gadgetry_f5214(void)
{
    int16_t brightest = EF_S16(0x485dfeu);
    int16_t darkest = brightest;
    uint16_t index;
    parse_asset(0x1923c0u, 0x1923e8u, 0x192418u,
                0x485f5au, 0xc3974u);
    EF_U16(0x485e02u) = (uint16_t)darkest;
    for (index = 0u; index < 256u; ++index) {
        uint32_t value = rgb_total((int16_t)index);
        if (value > rgb_total(brightest))
            brightest = (int16_t)index;
        if (value < rgb_total(darkest)) {
            darkest = (int16_t)index;
            EF_U16(0x485e02u) = index;
        }
    }
    EF_U16(0x485e00u) = (uint16_t)brightest;
    EF_U16(0x485dfeu) = (uint16_t)brightest;
}

/* 0xf53bc size 200. NAME follows the same path build, but selects the
 * second graphics buffer and does not inspect or alter palette extremes. */
void gui_script_name_f53bc(void)
{
    parse_asset(0x19245cu, 0x192484u, 0x1924b4u,
                0x485f63u, 0xc3980u);
}
