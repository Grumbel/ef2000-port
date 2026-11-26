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

/* XMIDI driver state, 0x1d4 bytes. */
#define MDI_DRIVER        0x000u  /* AIL driver descriptor */
#define MDI_INFO          0x004u  /* driver info returned in DX:AX */
#define MDI_BUFFER        0x008u  /* transfer buffer returned in CX:BX */
#define MDI_TIMER         0x00cu
#define MDI_INTERVAL      0x010u  /* microseconds per service tick */
#define MDI_FIELD_14      0x014u
#define MDI_SEQUENCES     0x018u  /* array of SEQ_SIZE records */
#define MDI_SEQ_COUNT     0x01cu
#define MDI_CHANNEL_TABLES 0x020u /* six tables of 16 dwords, 0x40 bytes apart */
#define MDI_FIELD_1A0     0x1a0u
#define MDI_FIELD_1A4     0x1a4u
#define MDI_PENDING_COUNT 0x1a8u
#define MDI_EVENT_BYTES   0x1acu
#define MDI_MASTER_VOLUME 0x1b0u
#define MDI_STATE_SIZE    0x1d4u

/* Driver info: real-mode pointers to the name of an environment variable
   and to a file name. The transfer buffer receives the variable's value
   (0x80 bytes) and, at +0x80, the file's path. */
#define INFO_ENV_NAME     0x00u
#define INFO_FILE_NAME    0x04u
#define BUFFER_PATH       0x80u

/* Driver header: real-mode pointer to the I/O parameter sets the driver
   suggests, and their number. */
#define HDR_IO_LIST       0x0cu
#define HDR_IO_COUNT      0x10u

#define SEQ_SIZE          0x718u
#define SEQ_STATE         0x000u
#define SEQ_STATUS        0x004u  /* 1 = free */

#define IO_PARAMS_SIZE    0x18u
#define CHANNELS          16u

/* Driver functions. */
#define AIL_DESCRIBE      0x301u
#define AIL_INIT_DEVICE   0x305u
#define AIL_SHUTDOWN      0x306u
#define AIL_INIT_TIMBRES  0x501u

#define g_io_params       0x004dd7b0u  /* I/O parameters handed to the driver */
#define g_xmidi_rate      EF_S32(0x004dd7f8u)  /* service ticks per second */
#define g_xmidi_sequences EF_S32(0x004dd7fcu)
#define g_xmidi_volume    EF_U32(0x004dd800u)
#define g_xmidi_bend_range EF_U32(0x004dd80cu)
#define g_xmidi_scan_io   EF_U32(0x004dd810u)  /* 1 = try the driver's own list */
#define DRIVER_DIRECTORY  0x0017e1d8u
#define XMIDI_SERVICE     0x00112c10u
#define XMIDI_RELEASE     0x00113398u

static ef_addr32 real_to_linear(uint32_t pointer)
{
    return (pointer & 0xffffu) + ((pointer >> 16) << 4) + g_runtime_segment_base;
}

static void copy_io_params(ef_addr32 destination, ef_addr32 source)
{
    uint32_t i;

    for (i = 0; i < IO_PARAMS_SIZE; i += 4u)
        EF_U32(destination + i) = EF_U32(source + i);
}

/* Hands the parameter set to the driver, then lets it probe the hardware. */
static int try_io_params(ef_addr32 state, ef_addr32 params, int publish)
{
    if (publish)
        copy_io_params(g_io_params, params);
    return apply_default_record_1132b8(state, params) != 0;
}

static uint32_t fail_install(ef_addr32 state, ef_addr32 message, int shut_down)
{
    set_ail_error(message);
    if (shut_down) {
        guest_fn_101f88(EF_U32(state + MDI_DRIVER), AIL_SHUTDOWN, 0, 0);
        EF_U32(EF_U32(state + MDI_DRIVER) + DRV_FIELD_18) = 0;
    }
    free_block_1047c4(state, MDI_STATE_SIZE);
    return 0;
}

static void reset_channel(ef_addr32 state, uint32_t channel)
{
    uint32_t control = 0xb0u | channel;

    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x72u, 0);  /* patch bank select */
    append_midi_event_111e40(0, 0, 0, 0, state, 0xc0u | channel, 0, 0);
    append_midi_event_111e40(0, 0, 0, 0, state, 0xe0u | channel, 0, 0x40u);
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x70u, 0);  /* voice protect */
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x01u, 0);  /* modulation */
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x07u, g_xmidi_volume);
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x0au, 0x40u); /* pan */
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x0bu, 0x7fu); /* expression */
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x40u, 0);  /* sustain */
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x5bu, 0x28u); /* reverb */
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x5du, 0);  /* chorus */
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x64u, 0);  /* RPN LSB */
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x65u, 0);  /* RPN MSB: pitch bend range */
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x26u, 0);  /* data entry LSB */
    append_midi_event_111e40(0, 0, 0, 0, state, control, 0x06u, g_xmidi_bend_range); /* data entry MSB */
    send_pending_count_message_111df0(0, 0, 0, 0, state);
}

/* 0x1133f8, 1608 bytes. AIL_install_MDI_driver: set up the XMIDI state of
   an installed MIDI driver, find working I/O parameters (the caller's, the
   driver's default, or with g_xmidi_scan_io one of the driver's list),
   initialise the device and its instrument manager, allocate the sequence
   records, start the service timer and reset all 16 channels. Returns the
   state, or 0 with a message in the AIL error buffer. */
uint32_t install_xmidi_driver_1133f8(ef_addr32 driver, ef_addr32 io_params)
{
    uint16_t regs[4];             /* AX, BX, CX, DX */
    ef_addr32 state;
    ef_addr32 info;
    ef_addr32 buffer;
    ef_addr32 name;
    ef_addr32 found = 0;
    ef_addr32 defaults;
    ef_addr32 header;
    uint32_t i;

    register_ranges_111c90();
    state = allocate_and_init_1047a4(MDI_STATE_SIZE);
    if (state == 0) {
        set_ail_error(0x001953f0u); /* "Could not allocate memory for driver\n" */
        return 0;
    }
    EF_U32(state + MDI_DRIVER) = driver;
    if (EF_U32(driver + DRV_TYPE) != 1)
        return fail_install(state, 0x00195418u, 0); /* ".MDI driver required\n" */

    guest_fn_101f88(driver, AIL_DESCRIBE, 0, (uint32_t)(uintptr_t)regs);
    info = ((uint32_t)regs[3] << 4) + regs[0] + g_runtime_segment_base;
    buffer = ((uint32_t)regs[2] << 4) + regs[1] + g_runtime_segment_base;
    EF_U32(state + MDI_INFO) = info;
    EF_U32(state + MDI_BUFFER) = buffer;

    name = real_to_linear(EF_U32(info + INFO_ENV_NAME));
    if (name != 0 && EF_U8(name) != 0) {
        name = getenv_9c858(name);
        if (name != 0 && EF_U8(name) != 0)
            copy_padded(buffer, name, BUFFER_PATH);
    }
    name = real_to_linear(EF_U32(EF_U32(state + MDI_INFO) + INFO_FILE_NAME));
    if (name != 0 && EF_U8(name) != 0) {
        copy_string_9d749(EF_U32(state + MDI_BUFFER) + BUFFER_PATH, DRIVER_DIRECTORY);
        strcat_9ca57(EF_U32(state + MDI_BUFFER) + BUFFER_PATH, name);
    } else {
        EF_U8(EF_U32(state + MDI_BUFFER) + BUFFER_PATH) = 0;
    }

    fill_bytes_b3fb0(g_io_params, 0xffu, IO_PARAMS_SIZE);
    if (io_params != 0 && try_io_params(state, io_params, 1))
        found = io_params;
    if (found == 0) {
        defaults = trace_call_1023d8(0, 0, 0, 0, EF_U32(state + MDI_DRIVER));
        if (defaults != 0 && try_io_params(state, defaults, 1))
            found = defaults;
    }
    if (found == 0 && g_xmidi_scan_io == 1) {
        for (i = 0;; i++) {
            header = EF_U32(EF_U32(state + MDI_DRIVER) + DRV_HEADER);
            if ((int32_t)EF_U16(header + HDR_IO_COUNT) <= (int32_t)i)
                break;
            defaults = real_to_linear(EF_U32(header + HDR_IO_LIST)) + i * IO_PARAMS_SIZE;
            /* only the first set is handed over before the probe */
            if (try_io_params(state, defaults, i == 0)) {
                found = defaults;
                break;
            }
        }
    }
    if (found == 0)
        return fail_install(state, 0x00195430u, 0); /* "XMIDI sound hardware not found\n" */

    copy_io_params(g_io_params, found);
    guest_fn_101f88(EF_U32(state + MDI_DRIVER), AIL_INIT_DEVICE, 0, 0);
    EF_U32(EF_U32(state + MDI_DRIVER) + DRV_FIELD_18) = 1;
    guest_fn_101f88(EF_U32(state + MDI_DRIVER), AIL_INIT_TIMBRES, 0, (uint32_t)(uintptr_t)regs);
    if (regs[0] == 0)
        return fail_install(state, 0x00195450u, 1); /* "Could not initialize instrument manager\n" */

    EF_S32(state + MDI_SEQ_COUNT) = g_xmidi_sequences;
    EF_U32(state + MDI_SEQUENCES) = allocate_and_init_1047a4((uint32_t)g_xmidi_sequences * SEQ_SIZE);
    if (EF_U32(state + MDI_SEQUENCES) == 0)
        return fail_install(state, 0x0019547cu, 1); /* "Could not allocate SEQUENCE structures\n" */
    for (i = 0; (int32_t)i < EF_S32(state + MDI_SEQ_COUNT); i++) {
        EF_U32(EF_U32(state + MDI_SEQUENCES) + i * SEQ_SIZE + SEQ_STATUS) = 1;
        EF_U32(EF_U32(state + MDI_SEQUENCES) + i * SEQ_SIZE + SEQ_STATE) = state;
    }

    EF_S32(state + MDI_INTERVAL) = 1000000 / g_xmidi_rate;
    EF_U32(state + MDI_FIELD_1A0) = 0;
    EF_U32(state + MDI_FIELD_1A4) = 0;
    EF_U32(state + MDI_PENDING_COUNT) = 0;
    EF_U32(state + MDI_EVENT_BYTES) = 0;
    EF_U32(state + MDI_FIELD_14) = 0;
    EF_U32(state + MDI_MASTER_VOLUME) = 0x7fu;
    for (i = 0; i < CHANNELS * 6u; i++)
        EF_U32(state + MDI_CHANNEL_TABLES + i * 4u) = 0;

    EF_U32(state + MDI_TIMER) = trace_call_102144(XMIDI_SERVICE);
    if (EF_U32(state + MDI_TIMER) == 0xffffffffu) {
        set_ail_error(0x001954a4u); /* "Out of timer handles\n" */
        guest_fn_101f88(EF_U32(state + MDI_DRIVER), AIL_SHUTDOWN, 0, 0);
        EF_U32(EF_U32(state + MDI_DRIVER) + DRV_FIELD_18) = 0;
        free_block_1047c4(EF_U32(state + MDI_SEQUENCES), EF_U32(state + MDI_SEQ_COUNT) * SEQ_SIZE);
        free_block_1047c4(state, MDI_STATE_SIZE);
        return 0;
    }
    trace_call_102198(EF_U32(state + MDI_TIMER), state);
    EF_U32(EF_U32(state + MDI_DRIVER) + DRV_FIELD_24) = XMIDI_RELEASE;
    EF_U32(EF_U32(state + MDI_DRIVER) + DRV_FIELD_28) = state;

    for (i = 0; i < CHANNELS; i++) {
        reset_channel(state, i);
        if ((i & 3u) == 0)
            trace_call_101ff4(3);
    }
    trace_call_102228(EF_U32(state + MDI_TIMER), (uint32_t)g_xmidi_rate);
    trace_call_1022dc(EF_U32(state + MDI_TIMER));
    return state;
}
