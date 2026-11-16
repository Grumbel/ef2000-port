/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xeac48, 453 bytes. EAX=signed argument count, EDX=pointer vector.
 * Preserve EBX/ECX/ESI/EDI/EBP; return zero in EAX. The optional callback
 * is invoked with EAX=-1 and no stack arguments; its result is unused. */
uint32_t initialize_data_playback_eac48(int32_t count, ef_addr32 arguments)
{
    ef_addr32 option[10];
    int32_t i;
    int32_t interval;

    for (i = 0; i < 10; i++) {
        option[i] = 0x001915a4u; /* original empty string */
        if (i < count) {
            ef_addr32 value = EF_U32(arguments + (uint32_t)i * 4u);
            if (value != 0u)
                option[i] = value;
        }
    }
    build_data_path_eabe8(option[1]);
    open_data_file_eaab0();
    interval = parse_int(option[2]);
    if (interval > 0)
        interval = parse_int(option[2]);
    else
        interval = 0;
    EF_U16(0x00484466u) = (uint16_t)interval;
    EF_U32(0x0048415cu) = get_global_c3824();
    clear_buffer_c38a4();
    /* These helpers use only their stack inputs; their four register
     * parameters in the existing declarations are unused ABI placeholders. */
    if ((uint16_t)compare_remapped_palette_d660c(0u, 0u, 0u, 0u, 0x00484164u) != 0u)
        set_black_palette_d65e8();
    read_data_block_eaaf4();
    EF_U32(0x00484150u) = seek_context_9cee8(0u, EF_U32(0x00160644u));
    guest_fn_f9ee0_x(0x00464580u, 0x00474360u);
    if (EF_U16(0x00484464u) != 0u)
        unpack_planes_c38cc(0u, 0u, 0u, 0u, 0x00464580u);
    else
        decode_planes_10dfec(0u, 0u, 0u, 0u, EF_U32(0x0048415cu), 0x00464580u);

    if (guest_strcmp(option[5], 0x001915a8u) != 0 ||
        (uint16_t)compare_remapped_palette_d660c(0u, 0u, 0u, 0u, 0x00484164u) != 0u) {
        toggle_page_dc250();
        call_e9a24_c386c();
        toggle_page_dc250();
        call_e9a24_c386c();
        toggle_page_dc250();
        call_e9a24_c386c();
        toggle_page_dc250();
    }
    if (guest_strcmp(option[5], 0x001915b8u) == 0 && option[6] != 0u)
        (void)guest_callptr_eax_ret_x(option[6], 0xffffffffu);

    EF_U16(0x0048446au) = 0u;
    memcpy_dc832(0x00425c8cu, 0x00484164u, 0x300u);
    if (guest_strcmp(option[4], 0x001915c8u) != 0) {
        if (guest_strcmp(option[4], 0x001915d4u) == 0) {
            set_black_palette_d65e8();
            fade_palette_in_de580(0x00425c8cu);
            EF_U16(0x0048446au) = 1u;
        } else {
            apply_palette_d663c(0x00425c8cu);
        }
    }
    EF_U16(0x00484468u) = 2u;
    EF_U32(0x00484154u) = field_10dfe0();
    return 0u;
}
