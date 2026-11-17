/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x34480, 29 bytes. Reset the socket word before reinitializing. */
void net_start_arcade_34480(void)
{
    EF_U16(0x001357b8u) = 0;
    initialize_network_dialog_34bcc();
    write_settings_file_a2bdc();
    refresh_network_controls_33630();
}

/* 0x344a0, 27 bytes. Sample and toggle the host byte after shutdown/clear.
 * XOR changes only bit zero, including for noncanonical boolean bytes. */
void net_new_channel_344a0(void)
{
    shutdown_device_a3da0();
    clear_session_34ea4();
    EF_U8(0x001357c8u) ^= 1u;
    write_settings_file_a2bdc();
    refresh_network_controls_33630();
}
