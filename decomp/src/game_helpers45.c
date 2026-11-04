/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Network wait loops and session text setup, 0x558b0..0x7e7e0.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0x558b0, 117 bytes. Finish the pending marker round: mark this player ready and update the scene. */
void finish_marker_round_558b0(void)
{
    if (EF_U8(0x0019bc02u) != 0) {
        EF_U8(0x0019bc02u) = 0;
        EF_U8(0x001a5c32u) = 1;
        EF_U8(0x001a5c32u + (uint32_t)EF_U8(0x001a666bu) * 27u) = 1;
        count_external_grid_markers();
        if (EF_U8(0x001f9010u) == 0) {
            if ((int16_t)EF_U16(0x001f71ceu) > (int16_t)EF_U16(0x002119f4u))
                ui_report_missing_record(0x00185c3cu);
            configure_scene_feature((int16_t)((int32_t)EF_U32(0x001f71ccu) >> 16));
        }
    }
    send_marker_packet_1e();
}

/* 0x56bdc, 159 bytes. Announce readiness (command 0x4d) until everyone answers or 150 ticks pass. */
void announce_ready_56bdc(void)
{
    uint8_t packet[1];
    int16_t count = (int16_t)EF_U16(0x001357b0u);
    int16_t i = 0;
    uint32_t deadline;

    packet[0] = 0x4d;
    if (count > 0) {
        do {
            EF_U16(0x001a5dd0u + (uint32_t)((int32_t)i * 2)) = 0;
            i++;
        } while (i < count);
    }
    EF_U16(0x001a5dd0u + (uint32_t)EF_U8(0x001a666bu) * 2u) = 1;
    deadline = ((uint32_t (*)(void))EF_U32(0x0015b620u))() + 0x96u;
    while ((guest_fn_56ba8() & 0xffu) == 0 && EF_U16(0x00126518u) == 0
           && ((uint32_t (*)(void))EF_U32(0x0015b620u))() < deadline) {
        emit_command_packet(packet, 1);
        guest_fn_a3f30();
        guest_fn_e2988(5);
        guest_fn_581b0();
    }
    EF_U16(0x00126518u) = 1;
}

/* 0x57030, 152 bytes. Announce readiness (command 0x36) until everyone answers or 100 ticks pass. */
void announce_ready_57030(void)
{
    uint8_t packet[1];
    int16_t count = (int16_t)EF_U16(0x001357b0u);
    int16_t i = 0;
    uint32_t deadline;

    packet[0] = 0x36;
    if (count > 0) {
        do {
            EF_U16(0x001a5de0u + (uint32_t)((int32_t)i * 2)) = 0;
            i++;
        } while (i < count);
    }
    EF_U16(0x001a5de0u + (uint32_t)EF_U8(0x001a666bu) * 2u) = 1;
    deadline = ((uint32_t (*)(void))EF_U32(0x0015b620u))() + 0x64u;
    while ((guest_fn_56ffc() & 0xffu) == 0 && EF_U16(0x00126518u) == 0
           && ((uint32_t (*)(void))EF_U32(0x0015b620u))() < deadline) {
        emit_command_packet(packet, 1);
        guest_fn_a3f30();
        guest_fn_e2988(2);
        guest_fn_581b0();
    }
}

/* 0x7a4a4, 106 bytes. Allocate the status text buffer, copying the description when enabled. */
void init_status_text_7a4a4(void)
{
    if (EF_U32(0x0012afc8u) != 0) {
        ef_addr32 text = build_status_description(0x00187ba8u);
        uint32_t length = ((uint32_t (*)(ef_addr32))EF_U32(0x00132ab4u))(text);
        ef_addr32 copy;

        if (length == 0)
            guest_report_parser_enum_error(0x00187bacu, text, 0);
        copy = guest_fn_e2a94(length);
        EF_U32(0x001f8e90u) = copy;
        ((void (*)(ef_addr32, ef_addr32))EF_U32(0x00132ab8u))(text, copy);
    } else {
        EF_U32(0x001f8e90u) = guest_fn_e2a94(0x4000u);
    }
    EF_U32(0x001f8e94u) = EF_U32(0x001f8e90u);
}

/* 0x7c4fc, 119 bytes. Build a path from a name, remember it and hand a copy to the loader. */
void set_session_text_7c4fc(ef_addr32 name, uint32_t target)
{
    uint8_t path[0x90];
    ef_addr32 buf = (ef_addr32)(uintptr_t)path;
    uint32_t length;
    ef_addr32 copy = 0x001301bau;

    guest_fn_9c980_sfx(buf, 0x00187ce4u);
    guest_fn_9ca57(buf, 0x00187ce4u);
    guest_fn_9ca57(buf, name);
    copy_string_9d749(0x001971b4u, buf);
    length = ((uint32_t (*)(ef_addr32))EF_U32(0x00132ab4u))(buf);
    if (length != 0) {
        copy = arena_take_e29e8(length + 1u);
        ((void (*)(ef_addr32, ef_addr32))EF_U32(0x00132ab8u))(buf, copy);
        EF_U8(copy + length) = 0;
    }
    guest_fn_e1e6c_3(target, copy, 0);
}

/* 0x7e5a0, 205 bytes. Mark unowned players as ours and resend until all players answer or time runs out. */
uint8_t claim_players_7e5a0(void)
{
    uint32_t deadline = ((uint32_t (*)(void))EF_U32(0x0015b620u))() + 0x4074u;
    int16_t pending;

    do {
        int16_t count = (int16_t)EF_U16(0x0019bb5cu);
        int16_t i = 0;

        guest_fn_e2988(0x1e);
        pending = 0;
        if (count > 0) {
            do {
                ef_addr32 player = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)i * 0xca);

                if (EF_U8(player) == 0xfe)
                    EF_U8(player) = EF_U8(0x001a666bu);
                if (EF_U8(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)i * 0xca)) != EF_U8(0x001a666bu)) {
                    send_grid_marker_short_packet(i);
                    pending++;
                }
                i++;
            } while (i < (int16_t)EF_U16(0x0019bb5cu));
        }
        guest_fn_581b0();
        guest_fn_58094();
    } while (pending != 0 && EF_U16(0x00126518u) == 0
             && ((uint32_t (*)(void))EF_U32(0x0015b620u))() < deadline);
    if (pending != 0 && EF_U16(0x00126518u) == 0)
        guest_report_parser_enum_error(0x00188640u, (uint32_t)(int32_t)pending, 0);
    return 1;
}

/* 0x7e758, 135 bytes. Claim the players, then send command 0x1d until the pending counter drains. */
uint8_t sync_players_7e758(void)
{
    uint8_t pending;
    uint32_t deadline;

    if ((claim_players_7e5a0() & 0xffu) == 0)
        return 0;
    if ((guest_fn_7e670() & 0xffu) == 0)
        return 0;
    EF_U8(0x001a5c32u + (uint32_t)EF_U8(0x001a666bu) * 27u) = 0;
    deadline = ((uint32_t (*)(void))EF_U32(0x0015b620u))() + 0x4074u;
    pending = EF_U8(0x00127da4u);
    if (pending != 0) {
        while (EF_U16(0x00126518u) == 0 && ((uint32_t (*)(void))EF_U32(0x0015b620u))() < deadline) {
            send_command_1d();
            guest_fn_58094();
            guest_fn_e2988(0x1e);
            guest_fn_581b0();
            if (EF_U8(0x00127da4u) == 0)
                break;
        }
    }
    return 1;
}

/* 0x7e7e0, 187 bytes. Wait for the pending round to clear, then mark this player ready. */
void wait_round_clear_7e7e0(void)
{
    uint32_t deadline = ((uint32_t (*)(void))EF_U32(0x0015b620u))() + 0x4074u;

    if (EF_U8(0x0019bc02u) != 0) {
        while (EF_U16(0x00126518u) == 0 && ((uint32_t (*)(void))EF_U32(0x0015b620u))() < deadline) {
            guest_fn_e2988(0x1e);
            guest_fn_581b0();
            guest_fn_58094();
            if (EF_U8(0x0019bc02u) == 0)
                break;
        }
    }
    if (((uint32_t (*)(void))EF_U32(0x0015b620u))() >= deadline) {
        EF_U8(0x001a5c32u) = 1;
        EF_U8(0x0019bc02u) = 0;
        EF_U8(0x001a5c32u + (uint32_t)EF_U8(0x001a666bu) * 27u) = 1;
        count_external_grid_markers();
        if (EF_U8(0x001f9010u) == 0) {
            if ((int16_t)EF_U16(0x001f71ceu) > (int16_t)EF_U16(0x002119f4u))
                ui_report_missing_record(0x00188698u);
            configure_scene_feature((int16_t)((int32_t)EF_U32(0x001f71ccu) >> 16));
        }
    }
}
