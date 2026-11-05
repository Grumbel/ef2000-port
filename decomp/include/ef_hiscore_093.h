/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_HISCORE_093_H
#define EF_HISCORE_093_H

#include "ef_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host-model boundaries for original RNG, DOS file functions and GUI.
 * These are not confirmed OpenWatcom calling conventions and the bodies
 * below are not suitable for an OMF splice until the adapters are replaced. */
void hiscore_seed_093(void);
uint16_t hiscore_random_093(void);
int hiscore_file_exists_093(ef_addr32 guest_filename);
int32_t hiscore_file_open_093(ef_addr32 guest_filename, int writing);
void hiscore_file_read_093(int32_t handle, void *bytes, uint32_t length);
void hiscore_file_write_093(int32_t handle, const void *bytes, uint32_t length);
void hiscore_file_close_093(int32_t handle);

int hiscore_window_exists_093(void);
ef_addr32 hiscore_first_icon_093(unsigned column);
void hiscore_icon_text_093(ef_addr32 icon, const char *text);
void hiscore_invalid_level_093(ef_addr32 guest_message);

void hiscore_generate_defaults_4050(void);
void hiscore_refresh_display_40dc(uint32_t force);
uint8_t hiscore_load_41fc(void);
uint8_t hiscore_save_42a4(void);
int32_t hiscore_insert_433c(ef_addr32 guest_name);

#ifdef __cplusplus
}
#endif

#endif
