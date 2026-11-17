/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* 0x33720, 21 bytes. EAX=widget; preserve the incoming EDX. */
void net_new_number_33720(ef_addr32 widget)
{
    copy_string_9d749(0x0015f57du, EF_U32(widget + 0x27u));
    write_settings_file_a2bdc();
}

/* 0x33738, 21 bytes. Store the modem prefix before writing settings. */
void net_new_pt_33738(ef_addr32 widget)
{
    copy_string_9d749(0x0015f53du, EF_U32(widget + 0x27u));
    write_settings_file_a2bdc();
}

/* 0x337ac, 21 bytes. Store the modem device before writing settings. */
void net_open_modem_status_337ac(ef_addr32 widget)
{
    copy_string_9d749(0x0015f4fdu, EF_U32(widget + 0x27u));
    write_settings_file_a2bdc();
}
