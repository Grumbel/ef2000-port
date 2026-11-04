/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Table-entry list operations. */

/* 0xa4a94, 114 bytes. If entry id has flag 8 set, remove it and every entry whose owner byte (+0x11) is id. */
void remove_entry_and_children_a4a94(uint32_t id)
{
    uint16_t i;
    id &= 0xffffu;
    if (id == 0u || id > EF_U16(0x0041eed8u))
        return;
    {
        uint32_t rec = EF_U32(0x0041edfcu + id * 4u);
        if (rec == 0u || (EF_U8(rec + 0x3bu) & 8u) == 0u)
            return;
    }
    remove_id_a4718((uint16_t)id);
    for (i = 1; i <= EF_U16(0x0041eed8u); i++) {
        if (EF_U8(EF_U32(0x0041edfcu + (uint32_t)i * 4u) + 0x11u) == id)
            remove_id_a4718(i);
    }
}

/* 0xf3aa8, 114 bytes. For an 22-byte record at 0x485400 + index * 22, run the 0xf3904 call (unless 0x4859db is set), then format its text at +8. */
void draw_record_text_f3aa8(uint32_t index)
{
    uint32_t rec = 0x00485400u + (uint32_t)((int32_t)(int16_t)index * 22);
    if (EF_U8(0x004859dbu) == 0u)
        guest_fn_f3904_x((int32_t)EF_S16(rec), (int32_t)EF_S16(rec + 2u), (int32_t)EF_S16(rec + 4u), (int32_t)EF_S16(rec + 6u));
    guest_fn_f3a1c(rec + 8u);
}

/* 0x287bc, 114 bytes. Build a file name from a format (one of two, chosen by 0x15ee9a), open it, and if that succeeds run 0xab47c and close the handle. */
void open_named_file_287bc(void)
{
    uint8_t name[0x98];
    int32_t handle;
    ef_addr32 buf = (uint32_t)(uintptr_t)name;
    if (EF_U16(0x0015ee9au) == 0u)
        guest_sprintf_a4342(buf, 0x001823c0u, guest_fn_9c858(0x001822d8u), 0x00125208u);
    else
        guest_sprintf_a4342(buf, 0x001823c8u, 0x00125208u);
    handle = open_slot_a7900(0u, 0u, 0u, 0u, buf);
    if (handle != -1) {
        guest_fn_ab47c((uint32_t)handle);
        close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
    }
}

/* 0xa0a2c, 114 bytes. Step a page slot: when its counter (+0) is zero invoke the handler at 0x132adc with the slot's words and the current 0x417194 triple, otherwise decrement the counter; then copy the triple into the slot. */
void step_page_slot_a0a2c(uint32_t index)
{
    ef_addr32 slot = page_slot_a0da8((int16_t)index);
    uint16_t counter = EF_U16(slot);
    if (counter == 0u) {
        guest_callptr4s2_x(EF_U32(0x00132adcu),
                           EF_S16(0x00417194u), EF_S16(0x00417196u), EF_S16(0x00417198u),
                           EF_S16(slot + 2u), EF_S16(slot + 4u), EF_S16(slot + 6u));
    } else {
        EF_U16(slot) = (uint16_t)(counter - 1u);
    }
    EF_U16(slot + 2u) = EF_U16(0x00417194u);
    EF_U16(slot + 4u) = EF_U16(0x00417196u);
    EF_U16(slot + 6u) = EF_U16(0x00417198u);
}

/* 0xa4a20, 116 bytes. Variant of 0xa4a94: if entry id has flag 8, push it to the front and rewrite every entry whose owner byte is id via 0xa48ac. */
void promote_entry_and_children_a4a20(uint32_t id)
{
    uint16_t i;
    id &= 0xffffu;
    if (id == 0u || id > EF_U16(0x0041eed8u))
        return;
    {
        uint32_t rec = EF_U32(0x0041edfcu + id * 4u);
        if (rec == 0u || (EF_U8(rec + 0x3bu) & 8u) == 0u)
            return;
    }
    push_front_id_a4820((uint16_t)id);
    for (i = 1; i <= EF_U16(0x0041eed8u); i++) {
        if (EF_U8(EF_U32(0x0041edfcu + (uint32_t)i * 4u) + 0x11u) == id)
            guest_fn_a48ac_x(i);
    }
}

/* 0x9e494, 116 bytes. Run the 0x9e7bc text routine with the current-page globals (0x417174/0x417178/0x41717c/0x41722a) temporarily pointed at page a, restoring them afterwards. */
void run_text_on_page_9e494(uint32_t page, ef_addr32 text)
{
    uint16_t saved_page = EF_U16(0x0041722au);
    uint32_t saved_cur = EF_U32(0x00417174u);
    uint32_t saved_tab = EF_U32(0x0041717cu);
    uint32_t saved_fn = EF_U32(0x00417178u);
    uint32_t cur;
    EF_U16(0x0041722au) = (uint16_t)page;
    cur = (uint32_t)guest_fn_b3f58((int32_t)(int16_t)page);
    EF_U32(0x00417174u) = cur;
    EF_U32(0x0041717cu) = EF_U32(0x0040d200u + (uint32_t)EF_U16(cur) * 4u);
    EF_U32(0x00417178u) = 0x000a0b5cu;
    guest_fn_9e7bc_x(text);
    EF_U16(0x0041722au) = saved_page;
    EF_U32(0x00417174u) = saved_cur;
    EF_U32(0x0041717cu) = saved_tab;
    EF_U32(0x00417178u) = saved_fn;
}
