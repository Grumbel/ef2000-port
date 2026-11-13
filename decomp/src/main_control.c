/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Original diagnostics are caller-clean indirect calls with one stack arg. */
static void report_main_message(ef_addr32 message)
{
#ifdef __WATCOMC__
    typedef void (__cdecl *Reporter)(ef_addr32);
#else
    typedef void (*Reporter)(ef_addr32);
#endif
    ((Reporter)(uintptr_t)EF_U32(0x00132a74u))(message);
}

/* 0xaaee8, 255 bytes. Consume initial environment assignments, initialize
 * menu services, then dispatch named states until the exit name is selected.
 * The original still reads argv[1] after reporting argc == 1; retain this. */
uint32_t main_control_loop(int32_t argc, ef_addr32 argv)
{
    int32_t i;
    EF_U8(0x0041955cu) = EF_U8(EF_U32(argv));
    while (argc > 1) {
        if (find_assignment_e874b(EF_U32(argv + 4u), 0x3du) == 0u)
            break;
        put_environment_f4347(EF_U32(argv + 4u));
        --argc;
        argv += 4u;
    }
    for (i = 0; i < argc; ++i) {
        if (ascii_stricmp_9d524(EF_U32(argv + (uint32_t)i * 4u),
                              0x0018d954u) == 0)
            EF_U8(0x00137204u) = 1u;
    }
    match_default_key_aae6c();
    if (argc == 1)
        report_main_message(0x0018d960u);
    if ((mouse_button_count_cdf60() & 0xffffu) == 0u)
        report_main_message(0x0018d97cu);
    init_video_mode_cdb20();
    switch_display_mode_d1e50(1u);
    set_black_palette_d65e8();
    initialize_input_services_cd4d4();
    EF_U8(0x001371f0u) = 0u;
    copy_string_9d749(0x00419160u, EF_U32(argv + 4u));
    register_callback_84d94();
    do {
        reset_key_tables_a9bfc();
        if (load_named_state_aad7c(0x00419160u) != 0u)
            dispatch_menu_state_a9e70();
    } while (guest_fn_a4380(0x00419160u, 0x0018d544u) != 0);
    restore_page_cd554();
    restore_video_mode_cdc64();
    return 0u;
}
