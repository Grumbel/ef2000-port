/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x336c0, 45 bytes. No argument is consumed by this callback. */
void net_new_fifo_len_336c0(void)
{
    clear_session_34ea4();
    shutdown_device_a3da0();
    if (EF_U8(0x0013578cu) == 0 || EF_U8(0x0015f098u) != 0)
        EF_U8(0x0015f099u) ^= 1u;
    write_settings_file_a2bdc();
    refresh_network_controls_33630();
}

/* 0x336f0, 47 bytes. The zero case branches to the shared RET at 33618.
 * The enable byte is sampled before shutdown, the option byte afterwards. */
void net_open_ipx_settings_336f0(void)
{
    if (EF_U8(0x0015f099u) == 0)
        return;
    clear_session_34ea4();
    shutdown_device_a3da0();
    EF_U8(0x0015f09au) = (uint8_t)((EF_U8(0x0015f09au) + 1u) & 3u);
    write_settings_file_a2bdc();
    refresh_network_controls_33630();
}

/* 0x3361c, 20 bytes before falling through to the shared refresh. */
void net_new_parity_3361c(void)
{
    clear_session_34ea4();
    shutdown_device_a3da0();
    advance_counter_15f07d();
    write_settings_file_a2bdc();
    refresh_network_controls_33630();
}

/* 0x337c4, 37 bytes. Read the port only after shutting down the device. */
void net_new_port_337c4(void)
{
    shutdown_device_a3da0();
    EF_U8(0x0013579au) = next_enabled_index(EF_U8(0x0013579au));
    clear_session_34ea4();
    write_settings_file_a2bdc();
    refresh_network_controls_33630();
}
