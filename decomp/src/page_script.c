/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Page script interpreter: runs the compiled word code of a page text
 * against the active page record. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define PAGE_RECORD      g_active_page_record_data
#define PAGE_POINTS      g_active_page_point_data
#define PAGE_FLAGS       EF_U8(PAGE_RECORD + 0xau)
#define PAGE_ENTRY(i)    (PAGE_RECORD + 0xbu + (uint32_t)(int32_t)(i) * 0x1du)
#define ENTRY_FIELD_BASE 0x14u /* eight bytes compared by the entry tests */
#define ENTRY_RESUME     0x12u /* word offset into the point data */

/* Text handler (0xa0b5c for page texts), called with a kind and the
 * script position of the opcode's operands. */
#define TEXT_HANDLER     EF_U32(0x00417178u)
#define TEXT_HANDLER_PAGE 0x000a0b5cu
#define PAGE_HOOK_POINTS EF_U32(0x00132ae4u)
#define PAGE_HOOK_PAIR   EF_U32(0x00132ae8u)
#define PAGE_HOOK_IDLE   EF_U32(0x00132aecu)

/* Scratch state the opcodes leave behind for later inspection. */
#define LAST_ENTRY       EF_U32(0x0041722cu)
#define TEST_VALUE       EF_U16(0x00417230u)
#define TEST_INDEX       EF_U16(0x00417232u)
#define LAST_SLOT        EF_U32(0x00417234u)
#define NEAR_GATE        0x00417238u
#define FAR_GATE         0x00417240u
#define EQUAL_SKIP       EF_U16(0x00417248u)
#define UNEQUAL_SKIP     EF_U16(0x0041724au)
#define TABLE_ENTRIES    0x00417220u
#define RESUME_A         EF_U16(0x00417150u)
#define RESUME_B         EF_U16(0x00417152u)

#define SCRIPT_STOP      EF_U8(0x00132ac2u)
#define SCRIPT_STATE     EF_U8(0x00132ac3u)
#define SAVED_VALUE      EF_U16(0x00132ac4u)
#define SAVED_PAGE       EF_U16(0x00132ad0u)
#define MODE_MASK        EF_U16(0x00159130u)
#define SCREEN_PAIR(i)   EF_U16(0x0015b264u + (uint32_t)(i) * 2u)
#define PAGE_NAMES       0x00411080u

#define MSG_RECOMPILE    0x0018b2d8u /* "ss: '%s' needs recompiling" */
#define MSG_BADOP        0x0018b2f4u /* "ss: '%s' badop (%d)" */
#define LAST_OPCODE      0x80u

static int32_t arg(ef_addr32 pc, uint32_t i)
{
	return (int16_t)EF_U16(pc + i * 2u);
}

/* Relative branch: PC addresses a signed word count. */
static ef_addr32 branch(ef_addr32 pc)
{
	return pc + (uint32_t)(arg(pc, 0u) * 2);
}

static ef_addr32 page_name(void)
{
	return EF_U32(PAGE_NAMES + (uint32_t)EF_U16(PAGE_RECORD) * 4u);
}

static void report_recompile(void)
{
	guest_callptr2_x(g_gui_error_hook, MSG_RECOMPILE, page_name());
}

/* Inline text: the handler sees the operands, then skip the word count
 * at +2 plus its header. */
static ef_addr32 run_text(ef_addr32 pc, uint32_t kind)
{
	guest_callptr_eaxedx_x(TEXT_HANDLER, kind, pc);
	return pc + (uint32_t)(arg(pc, 1u) * 2 + 2);
}

/* Does byte FIELD of the entry named by operand 0 equal operand 1? */
static int entry_byte_equals(ef_addr32 pc, uint32_t field)
{
	ef_addr32 entry = PAGE_ENTRY(arg(pc, 0u));

	LAST_ENTRY = entry;
	return (uint16_t)EF_U8(entry + field) == EF_U16(pc + 2u);
}

/* Does word operand 0 of the current point's entry equal operand 1? */
static int entry_word_equals(ef_addr32 pc)
{
	ef_addr32 entry;

	TEST_INDEX = EF_U16(pc);
	TEST_VALUE = EF_U16(pc + 2u);
	entry = PAGE_ENTRY(EF_U8(PAGE_POINTS + 1u));
	LAST_SLOT = entry;
	return EF_U16(entry + (uint32_t)((int16_t)TEST_INDEX * 2)) == TEST_VALUE;
}

/* Range gate. Operands: range, point, timer, skip. When value updates
 * are enabled and the page is not frozen, measure the point; if it is
 * inside (or outside) the range and the timer fires, run the following
 * block by replacing the skip with 1. Continue past the skip. */
static ef_addr32 range_gate(ef_addr32 pc, ef_addr32 gate, int inside)
{
	EF_U16(gate + 6u) = EF_U16(pc);
	EF_U16(gate + 4u) = EF_U16(pc + 2u);
	EF_U16(gate + 2u) = EF_U16(pc + 4u);
	EF_U16(gate) = EF_U16(pc + 6u);
	pc += 6u;
	if (g_page_value_updates_enabled != 0u && (PAGE_FLAGS & 0x10u) == 0u) {
		int32_t distance;
		int32_t range;

		measure_page_point_a0aa0((uint32_t)(int32_t)(int16_t)EF_U16(gate + 4u));
		distance = (int16_t)g_page_scene_distance;
		range = (int16_t)EF_U16(gate + 6u);
		if ((inside ? distance < range : distance > range)
		    && step_timer_entry_a1500((uint32_t)(int32_t)(int16_t)EF_U16(gate + 2u)) != 0u)
			EF_U16(gate) = 1u;
	}
	return pc + (uint32_t)((int16_t)EF_U16(gate) * 2);
}

/* Remember where to resume: the entry named by operand INDEX records PC
 * as a word offset into the point data. */
static void mark_resume(ef_addr32 pc, int32_t index)
{
	ef_addr32 entry = PAGE_ENTRY(index);

	LAST_ENTRY = entry;
	EF_U16(entry + ENTRY_RESUME) = (uint16_t)((int32_t)(pc - PAGE_POINTS) >> 1);
}

/* 0x9e7bc, 3752 bytes. Interpret the page script at SCRIPT until opcode
 * 0. Each opcode is a word followed by word operands; branch operands
 * are signed word counts from their own position, and the nested-block
 * opcodes recurse. Obsolete opcodes report that the page needs
 * recompiling; an opcode above 0x80 is reported and retried. */
void run_page_script_9e7bc(ef_addr32 script)
{
	ef_addr32 pc = script;

	for (;;) {
		uint32_t op = EF_U16(pc);

		pc += 2u;
		if (op > LAST_OPCODE) {
			pc -= 2u;
			guest_callptr3_x(g_gui_error_hook, MSG_BADOP, page_name(),
			                 (uint32_t)arg(pc, 0u));
			continue;
		}
		switch (op) {
		case 0x00:
			return;
		case 0x01:
			pc = branch(pc);
			break;
		case 0x02:
			run_page_script_9e7bc(branch(pc));
			pc += 2u;
			break;
		case 0x03:
			clear_page_slot_9f7d0((int16_t)arg(pc, 0u), EF_U16(pc + 2u));
			pc += 4u;
			break;
		case 0x04:
			queue_record_entry_9f850((uint32_t)arg(pc, 0u));
			pc += 2u;
			break;
		case 0x05:
			set_page_slot_9f808((int16_t)arg(pc, 0u), EF_U16(pc + 2u),
			                    EF_U8(pc + 4u), pc + 6u);
			pc += 0xcu;
			break;
		case 0x06:
			dispatch_offset_page_record_9f940((uint32_t)arg(pc, 0u), (uint32_t)arg(pc, 1u));
			pc += 4u;
			break;
		case 0x07:
			start_script_record_9fb00(arg(pc, 0u));
			pc += 2u;
			break;
		case 0x08:
			start_biased_page_record_9fc00((uint32_t)arg(pc, 0u), (uint32_t)arg(pc, 1u));
			pc += 4u;
			break;
		case 0x09: case 0x14:
			pc = run_text(pc, 0u);
			break;
		case 0x0a:
			copy_page_words_a0d58((int16_t)arg(pc, 0u), (uint16_t)arg(pc, 1u), pc + 4u);
			pc += (uint32_t)(arg(pc, 1u) * 2 + 4);
			break;
		case 0x0b:
			copy_page_words_a0d58((int16_t)arg(pc, 0u), 1u, pc + 2u);
			pc += 4u;
			break;
		case 0x0c: case 0x0d: case 0x58:
			pc += 0xcu;
			break;
		case 0x0e: case 0x1f: case 0x40:
			pc += 0x10u;
			break;
		case 0x0f:
			PAGE_FLAGS = (uint8_t)((PAGE_FLAGS & 0xf0u) | (EF_U8(pc) & 0x0fu) | 0x80u);
			pc += 2u;
			break;
		case 0x10:
			PAGE_FLAGS |= 0x90u;
			break;
		case 0x11: case 0x12: case 0x49: case 0x6c:
			pc += 2u;
			break;
		case 0x13:
			PAGE_FLAGS = (uint8_t)((PAGE_FLAGS & 0xefu) | 0x80u);
			break;
		case 0x15: case 0x16: case 0x17: case 0x18:
		case 0x19: case 0x1a: case 0x1b:
			pc = run_text(pc, op - 0x14u);
			break;
		case 0x1c: case 0x1d: case 0x23: case 0x52:
			pc += 0xeu;
			break;
		case 0x21:
			SCRIPT_STOP = 1u;
			break;
		case 0x25:
			pc = entry_word_equals(pc) ? branch(pc + 4u) : pc + 6u;
			break;
		case 0x3a:
			pc = entry_word_equals(pc) ? pc + 6u : branch(pc + 4u);
			break;
		case 0x26: case 0x27: case 0x28: case 0x29:
		case 0x2a: case 0x2b: case 0x2c: case 0x2d:
			pc = entry_byte_equals(pc, ENTRY_FIELD_BASE + op - 0x26u)
			     ? branch(pc + 4u) : pc + 6u;
			break;
		case 0x6f: case 0x70: case 0x71: case 0x72:
		case 0x73: case 0x74: case 0x75: case 0x76:
			if (entry_byte_equals(pc, ENTRY_FIELD_BASE + op - 0x6fu))
				run_page_script_9e7bc(branch(pc + 4u));
			pc += 6u;
			break;
		case 0x2e:
			start_offset_page_record_a0704((uint32_t)arg(pc, 0u), (uint32_t)arg(pc, 1u));
			pc += 4u;
			break;
		case 0x36:
			start_script_record_a060c(arg(pc, 0u));
			pc += 2u;
			break;
		case 0x37:
			fill_page_cell_a0ebc(pc);
			pc += 0x12u;
			break;
		case 0x41: case 0x50: case 0x51:
			pc += 0x12u;
			break;
		case 0x38:
			clear_page_slot_a0f4c((int16_t)arg(pc, 0u));
			pc += 2u;
			break;
		case 0x39:
			update_page_history_a0f84((uint32_t)arg(pc, 0u), (uint32_t)arg(pc, 1u),
			                          (uint32_t)arg(pc, 2u));
			pc += 6u;
			break;
		case 0x3b:
			SAVED_VALUE = EF_U16(pc);
			SAVED_PAGE = (uint16_t)g_active_page_identifier;
			pc += 2u;
			break;
		case 0x3c:
			start_rotated_page_record_a08c8((uint32_t)arg(pc, 0u));
			pc += 2u;
			break;
		case 0x3d:
			TEST_VALUE = EF_U16(pc);
			pc += 2u;
			pc = ((uint32_t)MODE_MASK & (uint32_t)(int32_t)(int16_t)TEST_VALUE) == 0u
			     ? branch(pc) : pc + 2u;
			break;
		case 0x3e:
			EF_U16(page_slot_a0da8((int16_t)arg(pc, 0u))) = 1u;
			pc += 2u;
			break;
		case 0x3f:
			step_page_slot_a0a2c((uint32_t)arg(pc, 1u));
			pc += 4u;
			break;
		case 0x42:
			pc = (int16_t)g_page_scene_distance > arg(pc, 0u) ? pc + 4u : branch(pc + 2u);
			break;
		case 0x43:
			pc = (int16_t)g_page_scene_distance <= arg(pc, 0u) ? pc + 4u : branch(pc + 2u);
			break;
		case 0x44:
			if (!((int16_t)g_page_scene_distance > arg(pc, 0u)))
				run_page_script_9e7bc(branch(pc + 2u));
			pc += 4u;
			break;
		case 0x45:
			if ((int16_t)g_page_scene_distance > arg(pc, 0u))
				run_page_script_9e7bc(branch(pc + 2u));
			pc += 4u;
			break;
		case 0x46:
			measure_page_point_a0aa0((uint32_t)arg(pc, 0u));
			pc += 2u;
			break;
		case 0x47:
			flag_page_slot_a14c8((int16_t)arg(pc, 0u));
			pc += 2u;
			break;
		case 0x4a:
			start_biased_page_record_9fc00((uint32_t)arg(pc, 0u), (uint32_t)arg(pc, 1u));
			LAST_ENTRY = PAGE_ENTRY(arg(pc, 0u));
			if (EF_U8(LAST_ENTRY + 0x1bu) != 0u)
				start_biased_page_record_9fc00((uint32_t)arg(pc, 2u), (uint32_t)arg(pc, 1u));
			pc += 6u;
			break;
		case 0x4e:
			mark_resume(pc + 2u, arg(pc, 0u));
			pc += 8u;
			break;
		case 0x4f:
			transform_and_relate_9f710(arg(pc, 0u), (uint32_t)arg(pc, 1u),
			                           arg(pc, 2u), arg(pc, 3u));
			pc += 8u;
			break;
		case 0x53: {
			ef_addr32 slot = page_slot_a0da8((int16_t)arg(pc, 0u));

			LAST_SLOT = slot;
			EF_U16(slot) = (uint16_t)(EF_U16(slot) - 1u);
			pc += 2u;
			break;
		}
		case 0x54: case 0x55: case 0x56:
			update_bearing_9f664(arg(pc, 0u));
			mark_resume(pc + 4u, arg(pc, 1u));
			if (op != 0x55)
				EF_U16(pc + 4u) = RESUME_A;
			if (op != 0x54)
				EF_U16(pc + 6u) = RESUME_B;
			pc += 0xau;
			break;
		case 0x5a:
			SCREEN_PAIR(0) = EF_U16(pc);
			SCREEN_PAIR(1) = EF_U16(pc + 2u);
			pc += 4u;
			break;
		case 0x5b:
			guest_callptr_eaxedx_x(PAGE_HOOK_POINTS, (uint32_t)arg(pc, 0u),
			                       0x00417194u /* g_page_point_word_offsets */);
			pc += 2u;
			break;
		case 0x5c:
			submit_page_point_a1590((uint32_t)arg(pc, 0u), (uint32_t)arg(pc, 1u), pc + 4u, 0u);
			pc += 0xau;
			break;
		case 0x6e:
			submit_page_point_a1590((uint32_t)arg(pc, 0u), (uint32_t)arg(pc, 1u), pc + 4u,
			                        (uint32_t)arg(pc, 5u));
			pc += 0xcu;
			break;
		case 0x5d:
			pc = range_gate(pc, NEAR_GATE, 1);
			break;
		case 0x69:
			pc = range_gate(pc, FAR_GATE, 0);
			break;
		case 0x5e: case 0x5f: case 0x60: case 0x61:
		case 0x62: case 0x63: case 0x64: case 0x65:
			copy_page_byte_a0b98((int16_t)(op - 0x5eu), (int16_t)arg(pc, 0u), (int16_t)arg(pc, 1u));
			pc += 4u;
			break;
		case 0x66:
			set_page_entry_a2634((int16_t)arg(pc, 0u), (int16_t)arg(pc, 1u),
			                     pc + (uint32_t)(arg(pc, 2u) * 2 + 4));
			pc += 6u;
			break;
		case 0x67:
			apply_page_cell_a2698((int16_t)arg(pc, 0u), (int16_t)arg(pc, 1u));
			pc += 4u;
			break;
		case 0x68:
			/* Format check: anything but version 0x20 is re-read as an opcode. */
			if (arg(pc, 0u) == 0x20)
				pc += 2u;
			else
				report_recompile();
			break;
		case 0x6a:
			if (TEXT_HANDLER == TEXT_HANDLER_PAGE)
				guest_callptr_eaxedx_x(PAGE_HOOK_PAIR, (uint32_t)arg(pc, 0u), (uint32_t)arg(pc, 1u));
			pc += 4u;
			break;
		case 0x6b:
			start_positioned_page_record_a04b4((uint32_t)arg(pc, 0u), (uint32_t)arg(pc, 1u));
			pc += 4u;
			break;
		case 0x6d:
			stamp_page_cell_a1424((int16_t)arg(pc, 0u), (int16_t)arg(pc, 1u),
			                      (uint32_t)arg(pc, 2u), (uint8_t)arg(pc, 3u));
			pc += 8u;
			break;
		case 0x77:
			TEST_VALUE = EF_U16(pc);
			EQUAL_SKIP = EF_U16(pc + 2u);
			pc += 2u;
			if ((uint16_t)SCRIPT_STATE == TEST_VALUE)
				EQUAL_SKIP = 1u;
			pc += (uint32_t)((int16_t)EQUAL_SKIP * 2);
			break;
		case 0x78:
			TEST_VALUE = EF_U16(pc);
			UNEQUAL_SKIP = EF_U16(pc + 2u);
			pc += 2u;
			if ((uint16_t)SCRIPT_STATE != TEST_VALUE)
				UNEQUAL_SKIP = 1u;
			pc += (uint32_t)((int16_t)UNEQUAL_SKIP * 2);
			break;
		case 0x79:
			guest_callptr0_x(PAGE_HOOK_IDLE);
			break;
		case 0x7a:
			while (arg(pc, 0u) != -1) {
				(void)store_table_entry_9dde0(TABLE_ENTRIES, EF_U16(pc));
				pc += 2u;
			}
			pc += 2u;
			break;
		case 0x7b: {
			ef_addr32 slot = page_slot_a0da8((int16_t)arg(pc, 0u));

			EF_U16(slot) = (uint16_t)(EF_U16(slot) + EF_U16(pc + 2u));
			LAST_SLOT = slot;
			pc += 4u;
			break;
		}
		case 0x7c:
			copy_page_words_a0d58((int16_t)arg(pc, 0u), 2u, pc + 2u);
			pc += 6u;
			break;
		case 0x7d:
			store_offset_cd2a8(pc);
			pc += 0xcu;
			break;
		case 0x7e:
			start_chain_page_record_a0228((uint32_t)arg(pc, 0u), (uint32_t)arg(pc, 1u));
			pc += 4u;
			break;
		case 0x7f:
			init_record_slot_9fea8((uint32_t)arg(pc, 0u), arg(pc, 1u), (uint32_t)arg(pc, 2u));
			pc += 6u;
			break;
		case 0x80:
			convert_entry_vector_9ffc8((int16_t)arg(pc, 0u), (int16_t)arg(pc, 1u));
			pc += 4u;
			break;
		default:
			/* 0x1e 0x20 0x22 0x24 0x2f-0x35 0x48 0x4b-0x4d 0x57 0x59 */
			report_recompile();
			break;
		}
	}
}
