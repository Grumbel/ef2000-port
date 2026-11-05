/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
static uint32_t check_addr, flushed, scene_calls, scene_error, scene_zero;
static uint32_t draw_addr, draw_extra, draw_pitch, draw_source, draw_dest;
static uint32_t audio_ports[4], audio_write_ports[2];
static uint8_t audio_written[2];
static uint32_t audio_read_calls, audio_writes, audio_waited, audio_opened, audio_devices;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return &ram[address];
}

void renderer_scene_error_c4db4(ef_addr32 message) { scene_error = message; }
void renderer_scene_zero_c4db4(ef_addr32 location) { scene_zero = location; }
void renderer_scene_record_c4db4(ef_addr32 record)
{
    assert(record == 0x202u || record == 0x204u);
    ++scene_calls;
}

uint8_t renderer_viewport_check_c8ccc(ef_addr32 addr)
{
    check_addr = addr;
    return (uint8_t)(addr == 0x4208e6u ? 0u : 1u);
}
void renderer_viewport_flush_c8ccc(void) { flushed = 1; }
uint8_t renderer_viewport_check_e7a34(ef_addr32 addr)
{
    check_addr = addr;
    return 1u;
}
void renderer_viewport_flush_e7a34(ef_addr32 addr)
{
    assert(addr == 0x15908cu);
    ++flushed;
}

uint8_t audio_port_read_e0bcc(uint32_t port)
{
    assert(port == 0x334u || port == 0x336u);
    return (uint8_t)audio_ports[audio_read_calls++];
}
void audio_port_write_e0bcc(uint32_t port, uint8_t value)
{
    assert(audio_writes < 2u);
    audio_write_ports[audio_writes] = port;
    audio_written[audio_writes++] = value;
}
void audio_wait_e0bcc(uint32_t ticks)
{
    assert(ticks == 0x46u);
    ++audio_waited;
}
void audio_open_e0bcc(ef_addr32 name) { audio_opened = name; }
void audio_device_e0bcc(ef_addr32 name) { audio_devices = name; }

void video_scanline_callback_117a0f(ef_addr32 target, uint32_t ebx,
                                    uint32_t ecx, uint32_t esi, uint32_t edi)
{
    draw_addr = target;
    draw_extra = ebx;
    draw_pitch = ecx;
    draw_source = esi;
    draw_dest = edi;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(audio_ports, 0, sizeof(audio_ports));
    check_addr = flushed = scene_calls = scene_error = scene_zero = 0;
    draw_addr = draw_extra = draw_pitch = draw_source = draw_dest = 0;
    audio_read_calls = audio_writes = audio_waited = audio_opened = audio_devices = 0;
}

static void test_scene_header(void)
{
    reset();
    EF_U16(0x220u) = 0x0400u;
    EF_U16(0x210u) = 0x1234u;
    EF_U16(0x200u) = 0x1122u;
    EF_U16(0x202u) = 0x3400u;
    EF_U16(0x204u) = 0xff7fu;
    renderer_normalize_scene_c4db4(0x200u);
    assert(scene_calls == 2u && scene_zero == 0x230u);
    assert(EF_U16(0x220u) == 4u);
    assert(EF_U32(0x15af8cu) == 0xc53b4u);
    assert(EF_U16(0x210u) == 0x3412u);
    assert(EF_U16(0x200u) == 0x2211u);
    assert(EF_U16(0x202u) == 0x0034u && EF_U16(0x204u) == 0x7fffu);
    reset();
    EF_U16(0x220u) = 0x0300u;
    renderer_normalize_scene_c4db4(0x200u);
    assert(scene_error == 0x158e92u && !scene_calls);
}

static void test_viewports(void)
{
    reset();
    renderer_viewport_five_c8ccc(1, 2, 3, 4, 5);
    assert(check_addr == 0x4208e6u && flushed == 1u);
    assert(EF_U16(0x4208e4u) == 5u && EF_U16(0x4208e6u) == 4u);
    assert(EF_U16(0x4208e8u) == 3u && EF_U16(0x4208eau) == 2u);
    assert(EF_U16(0x4208ecu) == 1u);
    reset();
    renderer_viewport_six_e7a34(1, 2, 3, 4, 5, 6);
    assert(check_addr == 0x15908cu && flushed == 0u);
    assert(EF_U16(0x159088u) == 5u && EF_U16(0x15908au) == 6u);
    assert(EF_U16(0x15908cu) == 4u && EF_U16(0x15908eu) == 3u);
    assert(EF_U16(0x159090u) == 2u && EF_U16(0x159092u) == 1u);
}

static void test_audio(void)
{
    reset();
    EF_U32(0x15eed0u) = 0x330u;
    audio_ports[0] = 0x80u;
    EF_U8(0x13578cu) = 0u;
    audio_maybe_restart_device_e0bcc();
    assert(!audio_read_calls);
    EF_U8(0x13578cu) = 1u;
    audio_ports[1] = 0xabu;
    audio_ports[2] = 0x20u;
    audio_ports[3] = 0x80u;
    audio_maybe_restart_device_e0bcc();
    assert(audio_read_calls == 4u && audio_writes == 2u);
    assert(audio_write_ports[0] == 0x334u && audio_written[0] == 0xaau);
    assert(audio_write_ports[1] == 0x334u && audio_written[1] == 0x21u);
    assert(audio_waited == 2u);
    assert(audio_opened == 0x1905fcu && audio_devices == 0x15f57du);
}

static void test_video_dispatch(void)
{
    reset();
    EF_U32(0x200u + 0x1cu) = 0x28u;
    EF_U32(0x200u + 0x18u) = 3u;
    EF_U32(0x200u + 0x64u) = 2u;
    EF_U32(0x200u + 0x10u) = 24u;
    EF_U32(0x200u + 0x2cu) = 0x800u;
    EF_U32(0x200u + 0x48u) = 120u;
    EF_U32(0x200u + 0x50u) = 0x900u;
    EF_U32(0x1159c8u + 0x78u * 4u) = 0xabcdefu;
    video_dispatch_scanline_117a0f(0x200u, 3u);
    assert(EF_U32(0x17e270u) == 0x78u);
    assert(draw_addr == 0xabcdefu && draw_extra == 0x83cu);
    assert(draw_dest == 0x824u && draw_pitch == 120u && draw_source == 0x900u);
    /* Separate row-pointer mode: no +0x20 layout bit, indexed pointers. */
    EF_U32(0x200u + 0x1cu) = 0;
    EF_U32(0x200u + 0x18u) = 0;
    EF_U32(0x200u + 0x64u) = 0;
    EF_U32(0x200u + 0x34u) = 0x1230u; /* row index 2 */
    video_dispatch_scanline_117a0f(0x200u, 2u);
    assert(EF_U32(0x17e270u) == 0u && draw_dest == 0x1230u);
}

int main(void)
{
    test_scene_header();
    test_viewports();
    test_audio();
    test_video_dispatch();
    return 0;
}
