/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* AIL driver descriptor, 0x2c bytes. */
#define DRV_SEGMENT      0x00u  /* real-mode segment << 16 */
#define DRV_SELECTOR     0x04u
#define DRV_LINEAR       0x08u  /* low-memory copy of the driver image */
#define DRV_SIZE         0x0cu
#define DRV_HEADER       0x10u
#define DRV_TYPE         0x14u  /* 0 = digital (AIL3DIG), 1 = MIDI (AIL3MDI) */
#define DRV_FIELD_18     0x18u
#define DRV_FIELD_1C     0x1cu
#define DRV_TIMER        0x20u
#define DRV_FIELD_24     0x24u
#define DRV_FIELD_28     0x28u
#define DRV_RECORD_SIZE  0x2cu

/* Driver header fields. */
#define HDR_SERVICE_RATE 0x2cu  /* dword; the high word is the timer rate */
#define HDR_FIELD_30     0x30u
#define HDR_HANDLE       0x32u
#define HDR_ENTRY        0x34u
#define HDR_REAL_MODE    0x36u

#define DRIVER_HANDLES   16u
#define TIMER_CALLBACK   0x00103e0cu
#define AIL_ERROR_TEXT   0x004dd6b0u

static void set_ail_error(ef_addr32 message)
{
    ef_addr32 out = AIL_ERROR_TEXT;
    uint8_t c;
    do {
        c = EF_U8(message++);
        EF_U8(out++) = c;
    } while (c != 0);
}

static uint32_t release_driver(ef_addr32 drv, ef_addr32 message)
{
    set_ail_error(message);
    nop_10483c();
    free_block_1047c4(drv, DRV_RECORD_SIZE);
    return 0;
}

/* 0x104200, 609 bytes. AIL_install_driver: copy a driver image into DOS
   memory, identify it, assign a handle and hook its timer. Returns the
   descriptor, or 0 with a message in the AIL error buffer. */
uint32_t EF_CDECL install_driver_104200(ef_addr32 image, uint32_t size)
{
    ef_addr32 drv;
    ef_addr32 header;
    uint32_t handle;
    uint32_t timer;

    drv = allocate_and_init_1047a4(DRV_RECORD_SIZE);
    if (drv == 0) {
        set_ail_error(0x0019484cu); /* "Insufficient memory for driver descriptor\n" */
        return 0;
    }
    EF_U32(drv + DRV_SIZE) = size;
    if (dos_alloc_block_1047e4(0, 0, 0, 0, (size + 15u) >> 4, drv + DRV_LINEAR,
                               drv + DRV_SEGMENT, drv + DRV_SELECTOR) == 0) {
        set_ail_error(0x00194878u); /* "Insufficient low memory\n" */
        free_block_1047c4(drv, DRV_RECORD_SIZE);
        return 0;
    }
    guest_memmove_109e6f(EF_U32(drv + DRV_LINEAR), image, size);
    header = EF_U32(drv + DRV_LINEAR);
    EF_U32(drv + DRV_HEADER) = header;

    if (compare_nocase_eb374(header, 0x00194894u, 7) == 0)      /* "AIL3DIG" */
        EF_U32(drv + DRV_TYPE) = 0;
    else if (compare_nocase_eb374(header, 0x0019489cu, 7) == 0) /* "AIL3MDI" */
        EF_U32(drv + DRV_TYPE) = 1;
    else
        return release_driver(drv, 0x001948a4u); /* "Invalid driver type\n" */

    for (handle = 0; handle < DRIVER_HANDLES; handle++) {
        if (g_ail_drivers[handle] == 0) {
            g_ail_drivers[handle] = drv;
            EF_U16(header + HDR_HANDLE) = (uint16_t)handle;
            break;
        }
    }
    if (handle == DRIVER_HANDLES)
        return release_driver(drv, 0x001948bcu); /* "Out of driver handles\n" */

    EF_U16(header + HDR_FIELD_30) = 0;
    EF_U32(drv + DRV_FIELD_18) = 0;
    EF_U32(drv + DRV_FIELD_1C) = 0xffffffffu;
    EF_U32(header + HDR_REAL_MODE) = trace_call_101e80(0, 0, 0, 0, 0x66u);
    guest_fn_101ed4_c(0x66u, EF_U16(header + HDR_ENTRY) + EF_U32(drv + DRV_SEGMENT));
    EF_U32(drv + DRV_FIELD_24) = 0;
    EF_U32(drv + DRV_FIELD_28) = 0;
    trace_call_101f88(0, 0, 0, 0, drv, 0x300u, 0, 0);

    if (EF_S16(header + HDR_SERVICE_RATE + 2u) <= 0) {
        EF_U32(drv + DRV_TIMER) = 0xffffffffu;
        return drv;
    }
    timer = trace_call_102144(TIMER_CALLBACK);
    EF_U32(drv + DRV_TIMER) = timer;
    if (timer == 0xffffffffu) {
        set_ail_error(0x001948d4u); /* "Out of timer handles\n" */
        guest_fn_101ed4_c(0x66u, EF_U32(header + HDR_REAL_MODE));
        nop_10483c();
        free_block_1047c4(drv, DRV_RECORD_SIZE);
        return 0;
    }
    trace_call_102198(timer, drv);
    trace_call_102228(EF_U32(drv + DRV_TIMER), (uint32_t)(EF_S32(header + HDR_SERVICE_RATE) >> 16));
    trace_call_1022dc(EF_U32(drv + DRV_TIMER));
    return drv;
}
