/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Network packet 0x29: a projectile launched by another player. */

#define NET_PROJECTILE_CURSOR 0x1a6634u /* uint16_t */
#define NET_PROJECTILE_RECORD 0x1a663cu /* 43-byte copy of the last packet */
#define NET_PROJECTILE_SIZE 0x2bu

/* Packet layout (unaligned): +1 new x, +5 new y, +9 new z, +0xd old x,
   +0x11 old y, +0x15 old z, +0x19 cell mode, +0x1b heading,
   +0x1d progress, +0x1f turn, +0x21 cell y, +0x25 target marker
   (-1 = any), +0x27 id. */
void net_packet_29_56084(ef_addr32 data)
{
	uint8_t saved_updates = g_page_value_updates_enabled;
	int16_t target = EF_S16(data + 0x25);
	uint32_t i;

	if (target != -1 && (uint16_t)target != (uint16_t)g_world_grid_marker)
		return;
	if (remember_value_56018(EF_U32(data + 0x27)) != 0)
		return;

	g_projectile_new_x = EF_S32(data + 0x01);
	g_projectile_new_y = EF_S32(data + 0x05);
	g_projectile_new_z = EF_S32(data + 0x09);
	g_projectile_old_x = EF_S32(data + 0x0d);
	g_projectile_old_y = EF_S32(data + 0x11);
	g_projectile_old_z = EF_S32(data + 0x15);
	g_projectile_segment[0] = EF_S16(data + 0x01);
	g_projectile_segment[1] = EF_S16(data + 0x05);
	g_projectile_segment[2] = EF_S16(data + 0x09);
	g_projectile_segment[3] = EF_S16(data + 0x0d);
	g_projectile_segment[4] = EF_S16(data + 0x11);
	g_projectile_segment[5] = EF_S16(data + 0x15);

	if (lookup_plane_cell_7ae88((uint32_t)(int32_t)EF_S16(data + 0x19),
	                            EF_S32(data + 0x0d), EF_U32(data + 0x21),
	                            EF_S32(data + 0x15)) != 0) {
		EF_U16(NET_PROJECTILE_CURSOR) = cursor_word_at_2();
		g_page_value_updates_enabled = 1;
		pack_runtime_position(EF_S16(data + 0x1b), EF_S32(data + 0x0d),
		                      EF_S32(data + 0x15), 0);
		guest_fn_110ec_x((uint32_t)(int32_t)EF_S16(data + 0x1f),
		                 (uint32_t)(int32_t)EF_S16(data + 0x1d));
		add_progress_cd2e8((uint32_t)(int32_t)EF_S16(data + 0x1d));
		for (i = 0; i < NET_PROJECTILE_SIZE; i++)
			EF_U8(NET_PROJECTILE_RECORD + i) = EF_U8(data + i);
		g_page_value_updates_enabled = saved_updates;
	}
	send_dword_marker_packet_2a(EF_U32(data + 0x27));
}
