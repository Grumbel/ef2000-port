/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Setting descriptors (g_cfg_descriptors), 0x12 bytes each, ended by an
 * empty name: name pointer +0, type +4, target pointer +5, size or maximum
 * length +9, choice table +0xa (8-byte name/value pairs, ended by an empty
 * name), notify routine +0xe. */
#define CFG_ENTRY(index) (g_cfg_descriptors + (uint32_t)(index) * 0x12u)
#define CFG_NAME(index) EF_U32(CFG_ENTRY(index))
#define CFG_TYPE(index) EF_U8(CFG_ENTRY(index) + 4u)
#define CFG_TARGET(index) EF_U32(CFG_ENTRY(index) + 5u)
#define CFG_SIZE(index) EF_U8(CFG_ENTRY(index) + 9u)
#define CFG_CHOICES(index) EF_U32(CFG_ENTRY(index) + 0xau)
#define CFG_NOTIFY(index) EF_U32(CFG_ENTRY(index) + 0xeu)
#define CFG_CHOICE_NAME(index, n) EF_U32(CFG_CHOICES(index) + (uint32_t)(n) * 8u)
#define CFG_CHOICE_VALUE(index, n) EF_U32(CFG_CHOICES(index) + (uint32_t)(n) * 8u + 4u)

enum {
	CFG_NONE = 0,   /* flag without a value: stores 0 nowhere */
	CFG_ENUM = 1,   /* word from the choice table, stored by size */
	CFG_ROUTINE = 2, /* routine name from the choice table into g_cfg_routine */
	CFG_DECIMAL = 3, /* %ld, stored by size */
	CFG_HEX = 4,    /* %lx, stored by size */
	CFG_TEXT = 5,   /* rest of the line up to ';' */
	CFG_WORD = 6,   /* text up to the next blank */
	CFG_LANG_TEXT = 7 /* optional language word, then text for that language */
};

/* Language words, 5-byte records: name pointer, language byte. */
#define CFG_LANGUAGES 0x00160080u
#define CFG_LANGUAGE_NAME(n) EF_U32(CFG_LANGUAGES + (uint32_t)(n) * 5u)
#define CFG_LANGUAGE_VALUE(n) EF_U8(CFG_LANGUAGES + (uint32_t)(n) * 5u + 4u)

#define CFG_LINE_LIMIT 0x84u
#define CFG_CLASS(c) g_character_class_table[(uint8_t)((c) + 1u)]
#define CFG_IDENT_CHAR(c) ((CFG_CLASS(c) & 0xe0u) != 0u || (c) == '_')
#define CFG_BLANK(c) ((CFG_CLASS(c) & 2u) != 0u)
#define CFG_AT(n) EF_U8(g_cfg_cursor + (uint32_t)(n))

typedef void (EF_CDECL *cfg_error_hook)(ef_addr32 format, ...);
#define CFG_ERROR ((cfg_error_hook)(uintptr_t)g_gui_error_hook)

/* Skip blanks, tabs and '=' signs (the latter only inside ROOM). */
static uint16_t cfg_skip_assignment(uint16_t room)
{
	uint16_t n = 0u;

	while (CFG_AT(n) == ' ' || CFG_AT(n) == '\t' ||
	       (CFG_AT(n) == '=' && n < room))
		n++;
	return n;
}

static int cfg_text_char(uint8_t c)
{
	return c != 0u && c != '\r' && c != ';';
}

/* Look up WORD in the choice table of descriptor INDEX; the result
 * indexes the empty terminator when nothing matches. */
static uint16_t cfg_find_choice(uint16_t index, ef_addr32 word)
{
	uint16_t n = 0u;

	while (strlen_ds(CFG_CHOICE_NAME(index, n)) != 0u &&
	       ascii_stricmp_9d524(CFG_CHOICE_NAME(index, n), word) != 0)
		n++;
	return n;
}

/* The original tests g_cfg_text[n] (the terminator) rather than the
 * last character, so this only trims when that byte is a blank. */
static uint16_t cfg_trim_text(uint16_t n)
{
	while (n != 0u && CFG_BLANK(g_cfg_text[n])) {
		g_cfg_text[n] = 0u;
		n--;
	}
	return n;
}

/* 0xe882c, 2990 bytes. Parse one settings line at g_cfg_cursor:
 * `name [=] value`. Looks the name up in g_cfg_descriptors, parses the
 * value by the descriptor type, stores it through the target pointer and
 * calls the notify routine (EAX = descriptor). Lines starting with ';'
 * are comments. Returns 1, also for a language text of another language;
 * on a syntax error returns the result of report_formatted_e8784 (0). */
uint8_t parse_config_line_e882c(void)
{
	ef_addr32 key = (ef_addr32)(uintptr_t)g_cfg_key;
	ef_addr32 text = (ef_addr32)(uintptr_t)g_cfg_text;
	char word[0x24];
	ef_addr32 word_addr = (ef_addr32)(uintptr_t)word;
	uint16_t room = CFG_LINE_LIMIT;
	uint16_t index;
	uint16_t n;
	uint8_t c;
	uint8_t type;
	ef_addr32 target;

	g_cfg_value = 0u;
	g_cfg_text[0] = 0u;
	g_cfg_text_length = 0u;

	for (n = 0u; CFG_AT(n) != 0u && CFG_AT(n) != '\r' && n < CFG_LINE_LIMIT; n++)
		;
	if (n >= room) {
		CFG_AT(0x1f) = 0u;
		return (uint8_t)report_formatted_e8784(0x00190cdcu, g_cfg_cursor); /* Line too long */
	}

	for (n = 0u; CFG_IDENT_CHAR(CFG_AT(n)) && n < room; n++)
		g_cfg_key[n] = CFG_AT(n);
	if (n == 0u && CFG_AT(0) == ';')
		return 1u;
	if (n == 0u || n >= room) {
		CFG_AT(0x1e) = 0u;
		return (uint8_t)report_formatted_e8784(0x00190cf0u, g_cfg_cursor); /* Rubbish at start of line */
	}
	g_cfg_key[n] = 0u;
	g_cfg_cursor += n;
	room -= n;

	for (index = 0u; strlen_ds(CFG_NAME(index)) != 0u &&
	                 ascii_stricmp_9d524(CFG_NAME(index), key) != 0; index++)
		;
	if (strlen_ds(CFG_NAME(index)) == 0u)
		return (uint8_t)report_formatted_e8784(0x00190d24u, key); /* not recognised */

	n = cfg_skip_assignment(room);
	if (n >= room) {
		CFG_AT(0x1f) = 0u;
		return (uint8_t)report_formatted_e8784(0x00190d50u, key, g_cfg_cursor);
	}
	room -= n;
	g_cfg_cursor += n;

	type = CFG_TYPE(index);
	if (type != CFG_NONE && type != CFG_TEXT && type != CFG_LANG_TEXT) {
		c = CFG_AT(0);
		if (!CFG_IDENT_CHAR(c) && c != '-' && c != '+') {
			CFG_AT(0x1f) = 0u;
			return (uint8_t)report_formatted_e8784(0x00190d7cu, key, g_cfg_cursor); /* bad/no Parameter */
		}
	}

	switch (CFG_TYPE(index)) {
	case CFG_NONE:
		g_cfg_value = 0u;
		break;
	case CFG_ENUM:
		if (CFG_CHOICES(index) == 0u)
			CFG_ERROR(0x00190da4u, g_cfg_file_name, (uint32_t)g_cfg_line_number, key);
		for (n = 0u; CFG_IDENT_CHAR(CFG_AT(n)); n++)
			word[n] = (char)CFG_AT(n);
		word[n] = 0;
		n = cfg_find_choice(index, word_addr);
		if (strlen_ds(CFG_CHOICE_NAME(index, n)) == 0u)
			return (uint8_t)report_formatted_e8784(0x00190df8u, word_addr, key); /* not valid */
		g_cfg_value = CFG_CHOICE_VALUE(index, n);
		break;
	case CFG_ROUTINE:
		if (CFG_CHOICES(index) == 0u)
			CFG_ERROR(0x00190e24u, g_cfg_file_name, (uint32_t)g_cfg_line_number, key);
		for (n = 0u; (CFG_CLASS(CFG_AT(n)) & 8u) != 0u; n++)
			word[n] = (char)CFG_AT(n);
		word[n] = 0;
		n = cfg_find_choice(index, word_addr);
		if (strlen_ds(CFG_CHOICE_NAME(index, n)) == 0u)
			CFG_ERROR(0x00190e7cu, g_cfg_file_name, (uint32_t)g_cfg_line_number, key, word_addr);
		g_cfg_routine = CFG_CHOICE_VALUE(index, n);
		break;
	case CFG_DECIMAL:
		if (guest_fn_df26f_x(g_cfg_cursor, 0x00190ed8u /* %ld */,
		                     (ef_addr32)(uintptr_t)&g_cfg_value) != 1)
			CFG_ERROR(0x00190edcu, g_cfg_file_name, (uint32_t)g_cfg_line_number, key, g_cfg_cursor);
		break;
	case CFG_HEX:
		if (guest_fn_df26f_x(g_cfg_cursor, 0x00190f30u /* %lx */,
		                     (ef_addr32)(uintptr_t)&g_cfg_value) != 1)
			CFG_ERROR(0x00190f34u, g_cfg_file_name, (uint32_t)g_cfg_line_number, key, g_cfg_cursor);
		break;
	case CFG_TEXT:
		if (CFG_SIZE(index) > 0x80u)
			CFG_ERROR(0x00190f84u, g_cfg_file_name, (uint32_t)g_cfg_line_number, key);
		for (n = 0u; n < room && cfg_text_char(CFG_AT(n)); n++)
			g_cfg_text[n] = CFG_AT(n);
		g_cfg_text[n] = 0u;
		if (CFG_SIZE(index) > room)
			CFG_ERROR(0x00190fecu, g_cfg_file_name, (uint32_t)g_cfg_line_number, key);
		n = cfg_trim_text(n);
		if (n > CFG_SIZE(index))
			return (uint8_t)report_formatted_e8784(0x00191054u, text, key); /* too long */
		break;
	case CFG_WORD:
		if (CFG_SIZE(index) > 0x80u)
			CFG_ERROR(0x001911a8u, g_cfg_file_name, (uint32_t)g_cfg_line_number, key);
		if (CFG_SIZE(index) > room)
			CFG_ERROR(0x00191214u, g_cfg_file_name, (uint32_t)g_cfg_line_number, key);
		for (n = 0u; !CFG_BLANK(CFG_AT(n)); n++)
			g_cfg_text[n] = CFG_AT(n);
		g_cfg_text[n] = 0u;
		if (n > CFG_SIZE(index))
			return (uint8_t)report_formatted_e8784(0x00191270u, text, key); /* too long */
		break;
	case CFG_LANG_TEXT:
		if (CFG_SIZE(index) > 0x80u)
			CFG_ERROR(0x0019108cu, g_cfg_file_name, (uint32_t)g_cfg_line_number, key,
			          (uint32_t)CFG_SIZE(index), 0x80u);
		for (n = 0u; CFG_IDENT_CHAR(CFG_AT(n)); n++)
			word[n] = (char)CFG_AT(n);
		g_cfg_language = 0u;
		word[n] = 0;
		while (strlen_ds(CFG_LANGUAGE_NAME(g_cfg_language)) != 0u &&
		       ascii_stricmp_9d524(CFG_LANGUAGE_NAME(g_cfg_language), word_addr) != 0)
			g_cfg_language++;
		if (strlen_ds(CFG_LANGUAGE_NAME(g_cfg_language)) == 0u) {
			g_cfg_language = g_cfg_current_language;
		} else {
			g_cfg_cursor += n;
			room -= n;
			g_cfg_language = CFG_LANGUAGE_VALUE(g_cfg_language);
		}
		if (g_cfg_language != g_cfg_current_language)
			return 1u;
		n = cfg_skip_assignment(room);
		if (n >= room) {
			CFG_AT(0x1f) = 0u;
			return (uint8_t)report_formatted_e8784(0x00191120u, key, g_cfg_cursor);
		}
		g_cfg_cursor += n;
		room -= n;
		for (n = 0u; n < room && cfg_text_char(CFG_AT(n)); n++)
			g_cfg_text[n] = CFG_AT(n);
		if (n == 0u)
			return (uint8_t)report_formatted_e8784(0x0019114cu, key); /* String missing */
		g_cfg_text[n] = 0u;
		n = cfg_trim_text(n);
		if (n >= CFG_SIZE(index))
			return (uint8_t)report_formatted_e8784(0x00191170u, key); /* too long */
		break;
	default:
		return (uint8_t)report_formatted_e8784(0x001912a4u, key); /* Duff command type */
	}

	target = CFG_TARGET(index);
	if (target != 0u) {
		switch (CFG_TYPE(index)) {
		case CFG_ENUM:
		case CFG_DECIMAL:
		case CFG_HEX:
			switch (CFG_SIZE(index)) {
			case 1u:
				EF_U8(target) = (uint8_t)g_cfg_value;
				break;
			case 2u:
				EF_U16(target) = (uint16_t)g_cfg_value;
				break;
			case 4u:
				EF_U32(target) = g_cfg_value;
				break;
			}
			break;
		case CFG_TEXT:
		case CFG_WORD:
		case CFG_LANG_TEXT:
			copy_string_9d749(target, text);
			break;
		}
	}
	type = CFG_TYPE(index);
	if (type >= CFG_TEXT && type <= CFG_LANG_TEXT)
		g_cfg_text_length = (uint16_t)strlen_ds(text);
	if (CFG_NOTIFY(index) != 0u)
		guest_callptr_eax_x(CFG_NOTIFY(index), CFG_ENTRY(index));
	return 1u;
}
