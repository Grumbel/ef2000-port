/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small DOS call wrappers, widget lookups and call sequences (25..44 bytes). */

/* 0x324, 35 bytes. DOS call 0x2503 with one argument; returns the result word. */
uint32_t dos_call_2503_324(uint32_t a)
{
    EF_U32(0x00196148u) = 0x2503;
    EF_U32(0x00196150u) = a;
    guest_fn_2b0(0x00196148u, 0x00196148u);
    return EF_U32(0x0019614cu);
}

/* 0x3d0, 37 bytes. DOS call 0x2505 with two arguments. */
void dos_call_2505_3d0(uint32_t a, uint32_t b)
{
    EF_U32(0x0019614cu) = b;
    EF_U32(0x00196150u) = a;
    EF_U32(0x00196148u) = 0x2505;
    guest_fn_2b0(0x00196148u, 0x00196148u);
}

/* 0xc24, 38 bytes. DOS call 0x3503 with a signed word and a second argument. */
void dos_call_3503_c24(int16_t a, uint32_t b)
{
    EF_U32(0x00196150u) = b;
    EF_U32(0x0019614cu) = (uint32_t)(int32_t)a;
    EF_U32(0x00196148u) = 0x3503;
    guest_fn_2b0(0x00196148u, 0x00196148u);
}

/* 0xa5bc, 28 bytes. Find a named widget and hand it to 0xc868. */
void run_named_widget_a5bc(void)
{
    guest_fn_c868_w(find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00180728u), 0x00180580u));
}

/* 0x7cc38, 35 bytes. Reset via 0xc768, then run 0x7bc40 on a named widget. */
void run_named_widget_7cc38(void)
{
    guest_fn_c768_w(0);
    guest_fn_7bc40_w(find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00187c90u), 0x00187c98u));
}

/* 0x84d94, 37 bytes. Register the 0x84dbc callback through 0xab838 after 0xabf8c. */
void register_callback_84d94(void)
{
    guest_fn_abf8c_w(EF_U8(0x0012ddacu), 0x0012b4ccu, 0x000a7ec4u);
    guest_fn_ab838_w(0x00084dbcu);
}

/* 0xa29cc, 41 bytes. Latch a decoded key into the state table. */
void latch_decoded_key_a29cc(uint8_t key)
{
    uint8_t decoded;

    guest_fn_a29f8_w(key);
    decoded = guest_fn_a2948_w(key, 0);
    EF_U8(0x0013578eu) = decoded;
    EF_U32(0x00135790u) = EF_U32(0x00135710u + (uint32_t)decoded * 4u);
}

/* 0xa32c8, 30 bytes. Issue a request for one byte of state. */
void issue_state_request_a32c8(uint8_t value)
{
    EF_U8(0x0013719au) = value;
    issue_request_a3e50(0x0013719au, 1, 0xb);
}

/* 0xc469b, 33 bytes. Copy three constant dwords from the code area into the work area. */
void copy_constants_c469b(void)
{
    EF_U32(0x001590b4u) = EF_U32(0x000c4756u);
    EF_U32(0x001590b8u) = EF_U32(0x000c475cu);
    EF_U32(0x001590bcu) = EF_U32(0x000c4762u);
}

/* 0xcdc64, 40 bytes. Re-apply the stored video mode if it differs from the current one. */
void restore_video_mode_cdc64(void)
{
    guest_fn_cdc38_w();
    if ((uint16_t)guest_fn_fe700_w() != EF_U16(0x0015b75cu))
        guest_fn_fe710_s(EF_S32(0x0015b75au) >> 16);
}

/* 0xea3ec, 40 bytes. Rebuild the cached chunk when the descriptor has one; true if present. */
uint8_t refresh_cached_chunk_ea3ec(ef_addr32 descriptor, ef_addr32 text)
{
    EF_U32(0x004644d0u) = 0;
    if (EF_U32(descriptor + 0xcu) != 0)
        EF_U32(0x004644d0u) = guest_fn_ea20c_w(descriptor, text);
    return EF_U32(0x004644d0u) != 0;
}

/* 0x11159a, 29 bytes. Set a file's buffer: fully buffered for a given buffer, else line buffered. */
int32_t set_default_file_buffer_11159a(ef_addr32 file, ef_addr32 buffer)
{
    return set_file_buffer_f22af(file, buffer, buffer != 0 ? 0x100u : 0x400u, 0x1000u);
}

/* 0x118520, 29 bytes. True when the record has no entry or its entry holds zero. */
uint32_t entry_empty_118520(ef_addr32 record)
{
    ef_addr32 entry;

    if (EF_U32(record + 0x1cu) == 0)
        return 1;
    entry = EF_U32(record + 0xau);
    return EF_U16(entry + 4u) == 0;
}

/* 0x1196a0, 40 bytes. Run the seven table initialisers, then 0x11c20c. */
void run_table_initialisers_1196a0(void)
{
    guest_fn_1196c8_w();
    guest_fn_119844_w();
    guest_fn_1198ec_w();
    guest_fn_119938_w();
    guest_fn_1199c8_w();
    guest_fn_119a54_w();
    guest_fn_119acc_w();
    guest_fn_11c20c_w();
}

/* 0x11f538, 28 bytes. Store two derived values for a number. */
void store_derived_pair_11f538(ef_addr32 out, uint32_t value)
{
    EF_U32(out) = guest_fn_11f4d8_w(value);
    EF_U32(out + 4u) = guest_fn_11f4d8_w(value + 0x6487u);
}
