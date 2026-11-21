/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Network packet reader state. */
#define NET_READ_DEPTH   0x00127dbcu /* word: nesting of get_net_data_581b0 */
#define NET_SLOT_LIST    0x00136b74u /* dword: pointer to the slot numbers */
#define NET_SLOT_COUNT   0x00136b78u /* dword: pointer to the slot count byte */
#define NET_SLOTS        0x00135878u /* 32-byte slots: +0 header, +4 packet data */
#define NET_SENDER       0x001a6668u /* byte: sender of the packets being read */
#define NET_STOP         0x001a6669u /* byte: stop reading this slot */
#define NET_LOST_MESSAGE 0x001327b7u /* "Multiplay-Verbindung abgebrochen" */

/* Handle the packet at DATA from SLOT and return its length; 0 for an
 * unknown packet type. */
static uint32_t handle_net_packet_581b0(ef_addr32 data, uint8_t slot)
{
    switch (EF_U8(data)) {
    case 0x0b: net_packet_0b_55144(data, slot); return 0x2a;
    case 0x0c: apply_unit_message_54f4c(data); return 5;
    case 0x0d: mark_filtered_object(data); return 4;
    case 0x0e: build_unit_from_record_55928(data, slot); return 0x1e;
    case 0x0f: dispatch_event_triple(data); return 0x11;
    case 0x10: notify_if_scene_pair(data); return 0x10;
    case 0x11: apply_unit_status_packet_5674c(data); return 0x14;
    case 0x12: apply_owner_summary_56af4(data); return 0x1f;
    case 0x14: net_packet_14_554b0(data); return 0x39;
    case 0x15: forward_record_to_46454(data); return 0x12;
    case 0x16: handle_message_54ff4(data); return 4;
    case 0x17: apply_marker_to_grid_row(data); return 4;
    case 0x18: request_name_54e80(data); return 0x101;
    case 0x19: mark_slot_54eb4(data); return 2;
    case 0x1a: set_flag_54efc(); return 1;
    case 0x1b: mark_slot_54f04(data); return 2;
    case 0x1c: commit_marker_selection(data); return 2;
    case 0x1d: update_view_557e8(); return 1;
    case 0x1e: set_activity_record_flag(data); return 2;
    case 0x1f: finish_marker_round_558b0(); return 1;
    case 0x21: copy_payload_550d8(data); return 0x19;
    case 0x22: apply_part_message_54804(data); return 0x0c;
    case 0x23: handle_unit_state_packet_564f4(data); return 0x10;
    case 0x24: clear_pending_55050(data); return 4;
    case 0x25: copy_message_54ba0(data); return 0x10;
    case 0x26: clear_activity_record_flag(data); return 2;
    case 0x27: apply_order_record_56890(data); return 0x0e;
    case 0x28: apply_params_message_566b4(data); return 0x12;
    case 0x29: net_packet_29_56084(data); return 0x2b;
    case 0x2a: forward_marker_matched_record(data); return 6;
    case 0x2b: apply_position_record_569b4(data); return 0x0a;
    case 0x2c: count_and_forward_577e8(data); return 6;
    case 0x2d: tail_jump_56a44(data); return 0x19;
    case 0x2e: forward_filtered_object(data); return 3;
    case 0x2f: set_name_56d18(data); return 0x65;
    case 0x30: clear_flag_bit_1a662e(data); return 3;
    case 0x31: handle_message_56e94(data); return 0x7b;
    case 0x32: forward_slot_message_56f30(data); return 4;
    case 0x33: mark_ready_56efc(data); return 3;
    case 0x34: copy_record_words_12583c(data); return 7;
    case 0x35: flag_marker_zero(); return 1;
    case 0x36: send_player_57_570c8(); return 1;
    case 0x37: mark_ready_570fc(data); return 2;
    case 0x38: copy_byte_57134(data); return 2;
    case 0x39: notify_unit_57160(data); return 3;
    case 0x3a: notify_unit_571b8(data); return 3;
    case 0x3b: forward_command_message_572b4(data); return 6;
    case 0x3d: clear_owner_slot_5736c(data); return 4;
    case 0x3e: handle_owner_message_57218(data); return 5;
    case 0x3f: store_descriptor_entry_57480(data); return 0x1c;
    case 0x40: handle_entry_57620(data); return 5;
    case 0x41: apply_action_55b80(data); return 9;
    case 0x42: net_packet_42_55bf8(data); return 6;
    case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x48:
        /* End of this slot's packets. */
        EF_U8(NET_STOP) = 1;
        return 0x13;
    case 0x4a: bump_counter_19b960(data); return 3;
    case 0x4b: bump_counter_19b96c(data); return 3;
    case 0x4c: bump_counter_19b966(data); return 3;
    case 0x4d: run_mode_one_56c7c(data); return 1;
    case 0x4e: mark_ready_56cbc(data); return 3;
    case 0x4f: release_slot_55784(data); return 5;
    default:
        return 0;
    }
}

/* 0x581b0, 1380 bytes ("GetNetData"): read the received packets of every
 * slot in the slot list (re-reading the count after each slot). A slot whose
 * header word +7 is set is skipped; otherwise its header byte +6 becomes the
 * sender and its packets are handled in order until a type outside
 * 0x0b..0x4f or an end packet. An unknown type ("Duff structure seen") stops
 * the reading of every remaining slot as well: the original's hex dump of
 * the packet reuses the slot counter and leaves it past the list. Packet
 * handlers may call back in; the nesting depth is counted. A lost link
 * (0x13581e) is reported first. The debug log 0xe7a30 the original calls
 * ("To deeeeeeep man", the dump) is a no-op and left out. */
void get_net_data_581b0(void)
{
    uint16_t index;
    uint8_t slot;
    ef_addr32 header;
    ef_addr32 data;
    uint32_t length;

    EF_U16(NET_READ_DEPTH) = (uint16_t)(EF_U16(NET_READ_DEPTH) + 1u);
    if (g_comms_initialize_flag_2 != 0u)
        ui_report_missing_record(NET_LOST_MESSAGE);
    recompute_total_a4030();

    for (index = 0; (int16_t)index < (int16_t)EF_U8(EF_U32(NET_SLOT_COUNT)); index++) {
        slot = EF_U8(EF_U32(NET_SLOT_LIST) + (uint32_t)(int32_t)(int16_t)index);
        header = EF_U32(NET_SLOTS + 32u * slot);
        if (EF_U16(header + 7u) != 0u)
            continue;
        EF_U8(NET_STOP) = 0;
        EF_U8(NET_SENDER) = EF_U8(header + 6u);
        data = EF_U32(NET_SLOTS + 4u + 32u * slot);
        while (EF_U8(data) > 0x0au && EF_U8(data) < 0x50u && EF_U8(NET_STOP) == 0u) {
            length = handle_net_packet_581b0(data, slot);
            if (length == 0) {
                EF_U8(NET_STOP) = 1;
                index = 0x400;
                break;
            }
            data += length;
        }
    }
    EF_U16(NET_READ_DEPTH) = (uint16_t)(EF_U16(NET_READ_DEPTH) - 1u);
}
