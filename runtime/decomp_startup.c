/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Minimal Linux bindings for the first game-owned startup call at 0xe29a0.
 * These are narrowly scoped host services; do not treat them as a complete
 * implementation of the original X-32 heap and Watcom C runtime. */

#include "decomp_startup.h"
#include "decomp_files.h"
#include "decomp_image.h"
#include "ef_abi.h"
#include "ef_mem.h"

#include <iconv.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t next_allocation;
static uint32_t requested_arena;
static uint32_t release_hook;
static uint32_t hook_registrations;
static uint32_t input_cleanup_hook;
static int probing;
static int arena_allocated;
static int stream_allocated;
static uint32_t keyboard_led_bytes;
static int keyboard_ack_pending;
static jmp_buf main_boundary;
static uint32_t main_boundary_address;
static int main_probe_active;
/* 0: normal TACTCOM stop; 1: exit-handler entry; 2: channel-mode prefix. */
static int main_probe_depth;
static int x32_probe_active;
static int x32_virtual_vectors;
static int x32_virtual_traps;
static int x32_virtual_input;
static int x32_virtual_timer;
static int x32_virtual_tactcom;
static int x32_virtual_sound_timer;
static int x32_virtual_ail_init;
static int x32_virtual_ail_irq;
static int x32_virtual_ail_vectors;
static int x32_virtual_ail_callback;
static int x32_virtual_ail_module;
static int x32_virtual_ail_slot;
static int x32_virtual_ail_rate;
static int x32_virtual_ail_start;
static int x32_virtual_ail_finish;
static uint32_t ail_lock_count;
static uint32_t ail_setting_count;
static uint32_t ail_irq_critical_count;
static uint32_t ail_irq_vector_queries;
static uint32_t ail_irq_vector_registrations;
static uint32_t ail_pit_port_writes;
static uint32_t ail_irq_critical_exited;
static uint32_t ail_module_lock_count;
enum { AIL_DEFAULT_LOCK_COUNT = 16u, AIL_IRQ_LOCK_COUNT = 18u };
/* Cold AIL setup locks code and data before trying its first setting.
 * These exact exclusive-end arguments come from the recovered 0x101a90,
 * 0x103d80 and their 0x104908 wrapper, not an assumed extender result. */
static const struct {
    uint32_t first;
    uint32_t last;
} ail_cold_lock_ranges[] = {
    { 0x00101a90u, 0x00103d48u },
    { 0x004dd624u, 0x004dd628u },
    { 0x004dd628u, 0x004dd62cu },
    { 0x004dd620u, 0x004dd624u },
    { 0x004dd62cu, 0x004dd630u },
    { 0x004dd63cu, 0x004dd640u },
    { 0x004dd638u, 0x004dd63cu },
    { 0x004dd640u, 0x004dd644u },
    { 0x004dd630u, 0x004dd634u },
    { 0x00103d80u, 0x001046c4u },
    { 0x004dd670u, 0x004dd6b0u },
    { 0x004dd7ccu, 0x004dd818u },
    { 0x004dd6b0u, 0x004dd7b0u },
    { 0x004dd7b0u, 0x004dd7c8u },
    { 0x0017d2d0u, 0x0017d2d4u },
    { 0x004dd7c8u, 0x004dd7ccu },
    { 0x0017d624u, 0x0017e1d4u },
    { 0x00110dccu, 0x00111575u },
};
/* Two module locks in recovered 0xd6df0, then two timer-service locks in
 * recovered 0xe28e8. These are original 0x104908 exclusive-end requests. */
static const struct {
    uint32_t first;
    uint32_t last;
} ail_module_lock_ranges[] = {
    { 0x000d7108u, 0x000d7150u },
    { 0x001048b4u, 0x00104cb4u },
    { 0x000e28c4u, 0x000e28dcu },
    { 0x0015f73cu, 0x0015f750u },
};
static uint32_t x32_timer_registrations;
static uint32_t x32_input_returns;
static uint32_t x32_request_count;
static struct {
    uint32_t address;
    uint32_t selector;
    uint32_t length;
} x32_requests[11];
static uint32_t x32_trap_count;
static struct {
    uint32_t vector;
    uint32_t handler;
} x32_traps[8];
static void stop_at_guest_boundary(uint32_t address);

/* The native probe uses private guest stack-shaped scratch frames, not an
 * emulated Watcom stack.  Keep it below native64's documented 0x3fff000
 * initial guest ESP, outside the loaded image, BSS and startup arena. */
enum { TACTCOM_PROBE_FRAME = EF_DECOMP_RAM_SIZE - 0x2000u,
       TACTCOM_LOG_FRAME = TACTCOM_PROBE_FRAME + 0x200u,
       TACTCOM_TEXT_FRAME = TACTCOM_PROBE_FRAME + 0x400u,
       TACTCOM_VIEW_FRAME = TACTCOM_PROBE_FRAME + 0x600u,
       TACTCOM_SEQ_FRAME = TACTCOM_PROBE_FRAME + 0x800u,
       TACTCOM_AID_FRAME = TACTCOM_PROBE_FRAME + 0xa00u };

static void fatal_unexpected(const char *service)
{
    fprintf(stderr, "ef2000-decomp: unexpected startup service: %s\n", service);
    abort();
}

/* Original game strings are DOS CP437; diagnostics on Linux are UTF-8.
 * Only the host output is converted; guest RAM retains the original bytes. */
static void report_dos_message(const char *message)
{
    iconv_t converter = iconv_open("UTF-8", "CP437");
    char utf8[1024];
    char *src = (char *)message;
    char *dst = utf8;
    size_t from = strlen(message);
    size_t to = sizeof(utf8) - 1;
    if (converter != (iconv_t)-1) {
        if (iconv(converter, &src, &from, &dst, &to) != (size_t)-1) {
            *dst = 0;
            fprintf(stderr, "TACTCOM diagnostic: %s\n", utf8);
            iconv_close(converter);
            return;
        }
        iconv_close(converter);
    }
    fputs("TACTCOM diagnostic: ", stderr);
    while (*message) {
        unsigned char c = (unsigned char)*message++;
        fputc(c < 0x80 ? c : '?', stderr);
    }
    fputc('\n', stderr);
}

void guest_fatal_arg(ef_addr32 format, uint32_t value)
{
    fprintf(stderr, "ef2000-decomp: guest startup fatal message at 0x%08x (arg 0x%08x)\n",
            format, value);
    abort();
}

/* Original 0xc3f70 is a jump to the extender-backed allocator. During this
 * narrowly bounded probe only the arena and one Views stream record are
 * modeled. The latter lives beyond the initial arena, not inside its
 * accounting structure. This is not a general X-32 heap implementation. */
uint32_t guest_fn_c3f70(uint32_t size)
{
    uint32_t stream_address;
    if (probing && main_probe_active && arena_allocated && !stream_allocated &&
        size == 0x56cu && requested_arena <= UINT32_MAX - next_allocation) {
        stream_address = next_allocation + requested_arena;
        if (!ef_decomp_memory_span(stream_address, size))
            fatal_unexpected("Views stream allocation outside guest RAM");
        stream_allocated = 1;
        return stream_address;
    }
    if (!probing || arena_allocated || size != requested_arena ||
        !ef_decomp_memory_span(next_allocation, size))
        fatal_unexpected("arena allocation (0xc3f70)");
    arena_allocated = 1;
    return next_allocation;
}

/* The original registers 0xe2a50 as a cleanup handler for the arena. Keep
 * the guest offset, not a truncated host function pointer. */
void guest_fn_9c3cf(uint32_t handler)
{
    if (x32_probe_active) {
        if (!probing || !main_probe_active)
            fatal_unexpected("X-32 far-stub cleanup registration");
        if (hook_registrations == 0u && handler == 0xad8u)
            release_hook = handler;
        else if (x32_virtual_timer && hook_registrations == 1u &&
                 release_hook == 0xad8u && x32_timer_registrations == 1u &&
                 handler == 0x000cd554u && input_cleanup_hook == 0u)
            input_cleanup_hook = handler;
        else
            fatal_unexpected("X-32 far-stub cleanup registration");
        ++hook_registrations;
        return;
    }
    if (!probing || handler != 0xe2a50u || hook_registrations)
        fatal_unexpected("cleanup registration (0x9c3cf)");
    release_hook = handler;
    hook_registrations++;
}

/* The original 0xcd414 sends 0xed and then the LED mask to the keyboard
 * controller, waiting for a 0xfa response after each byte. This strictly
 * bounded probe has no real keyboard controller or installed IRQ1 handler. */
void guest_fn_117b0c(uint32_t port, uint32_t value)
{
    /* AIL mode programs channel zero in mode 3 (0x36, low, high).
     * Capture the *requests* only; Linux never receives an out instruction. */
    if (probing && main_probe_active && x32_probe_active &&
        x32_virtual_ail_callback && ail_irq_critical_count == 1u &&
        EF_U32(0x0017d7bau) == 2u && ail_pit_port_writes < 3u &&
        port == (ail_pit_port_writes == 0u ? 0x43u : 0x40u) &&
        value == (ail_pit_port_writes == 0u ? 0x36u : 0u)) {
        ++ail_pit_port_writes;
        return;
    }
    if (probing && main_probe_active && x32_probe_active &&
        x32_virtual_ail_rate && ail_irq_critical_count == 0u &&
        ail_irq_critical_exited == 1u &&
        EF_U32(0x0017d7bau) == 3u &&
        ail_pit_port_writes >= 3u && ail_pit_port_writes < 6u &&
        port == (ail_pit_port_writes == 3u ? 0x43u : 0x40u) &&
        value == (ail_pit_port_writes == 3u ? 0x36u :
                  ail_pit_port_writes == 4u ? 0x3au : 0x5du)) {
        ++ail_pit_port_writes;
        return;
    }
    if (!probing || !main_probe_active || port != 0x60u ||
        keyboard_ack_pending || keyboard_led_bytes >= 2u ||
        value != (keyboard_led_bytes == 0u ? 0xedu : 0u))
        fatal_unexpected("keyboard LED command during TACTCOM init");
    ++keyboard_led_bytes;
    keyboard_ack_pending = 1;
}

uint8_t guest_fn_9c430(uint32_t port)
{
    if (!probing || !main_probe_active ||
        (port != 0x64u && port != 0x60u))
        fatal_unexpected("keyboard controller read during TACTCOM init");
    if (port == 0x64u)
        return keyboard_ack_pending ? 1u : 0u;
    if (!keyboard_ack_pending)
        fatal_unexpected("keyboard acknowledgement without pending byte");
    keyboard_ack_pending = 0;
    return 0xfau;
}

/* The X-32 probe uses synthetic CS/DS selectors. Normal TACTCOM's pool is
 * intentionally left uninitialized; these selectors are not real LDT IDs. */
uint32_t guest_seg_x(uint32_t which)
{
    if (!probing || !main_probe_active || (which != 1u && which != 3u))
        fatal_unexpected("unmodeled protected-mode selector");
    return x32_probe_active ? (which == 1u ? 0x1234u : 0x5678u) : 0u;
}

/* Called only by native GCC's recovered 0x628, *before* its first write.
 * This preserves the normal main probe's pre-CRT halt at 0x628. */
void ef_decomp_x32_request_guard(uint32_t first, uint32_t second, int32_t third)
{
    if (x32_probe_active)
        return;
    if (!probing || !main_probe_active || first != 0x000fe61fu ||
        second != 0u || third != 0xd5 ||
        keyboard_led_bytes != 2u || keyboard_ack_pending ||
        EF_U32(0x00196144u) != 0u)
        fatal_unexpected("TACTCOM first request slot before CRT initialization");
    stop_at_guest_boundary(0x00000628u);
}

/* Start of the real pre-main sequence: X-32's block selector and a six-byte
 * far jump stub are represented by deterministic *guest* addresses. */
uint32_t guest_fn_a60(void)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        EF_U8(0x00132a98u) != 1u)
        fatal_unexpected("X-32 far-stub block selector (0xa60)");
    return 0x80u;
}

uint32_t guest_fn_510(uint32_t size)
{
    if (!probing || !main_probe_active || !x32_probe_active || size != 6u ||
        !ef_decomp_memory_span(0x00400100u, 6u))
        fatal_unexpected("X-32 far-stub allocation (0x510)");
    return 0x00400100u;
}

uint32_t guest_fn_604_x(uint32_t source)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        source != 0x00400100u)
        fatal_unexpected("X-32 far-stub selector (0x604)");
    return 0x00034000u;
}

void guest_fn_a9c(uint32_t block, uint32_t selector)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        block != 0x80u || selector != 0x00034000u)
        fatal_unexpected("X-32 far-stub selector assignment (0xa9c)");
}

/* Real 0x9c220 dispatches INT 21h, AX=0x252B, EBX=0x501. The default
 * probe halts after the first packed request write. Only the explicitly
 * selected virtual-registry probe acknowledges the remaining eight requests
 * in a private table; no DOS or host vectors are installed. */
static uint32_t read_x32_packed_dword(uint32_t address)
{
    return (uint32_t)EF_U8(address)
         | ((uint32_t)EF_U8(address + 1u) << 8)
         | ((uint32_t)EF_U8(address + 2u) << 16)
         | ((uint32_t)EF_U8(address + 3u) << 24);
}

uint32_t EF_CDECL guest_fn_9c220(uint32_t address, uint32_t selector,
                                 uint32_t length)
{
    static const uint32_t expected[11][3] = {
        { 0x000002b0u, 0x1234u, 0xb70u },
        { 0x0009be0du, 0x1234u, 0x1fu },
        { 0x0009c430u, 0x1234u, 0x1fu },
        { 0x0009c438u, 0x1234u, 0x1fu },
        { 0x0009c446u, 0x1234u, 0x1fu },
        { 0x0009c461u, 0x1234u, 0x1fu },
        { 0x0009c47au, 0x1234u, 0x1ffu },
        { 0x00000222u, 0x1234u, 0xfu },
        { 0x00120cd4u, 0x5678u, 2u },
        { 0x000fe61fu, 0x1234u, 0xd5u },
        { 0x0041f833u, 0x5678u, 0x118u },
    };
    uint32_t index = x32_request_count;
    uint32_t slot;
    if (!probing || !main_probe_active || !x32_probe_active ||
        index >= (x32_virtual_input ? 11u : 9u) ||
        address != expected[index][0] ||
        selector != expected[index][1] || length != expected[index][2])
        fatal_unexpected("X-32 vector request sequence (0x9c220)");
    slot = 15u - index;
    if (EF_U32(0x00196144u) != slot ||
        EF_U8(0x00195fb0u + slot) != slot ||
        read_x32_packed_dword(0x00196022u + slot * 10u) != address ||
        EF_U16(0x00196026u + slot * 10u) != selector ||
        read_x32_packed_dword(0x00196028u + slot * 10u) != length)
        fatal_unexpected("packed X-32 vector record (0x9c220)");
    x32_requests[index].address = address;
    x32_requests[index].selector = selector;
    x32_requests[index].length = length;
    ++x32_request_count;
    if (!x32_virtual_vectors)
        stop_at_guest_boundary(0x0009c220u);
    /* Explicitly opt-in virtual registry: no host IRQ/vector hook exists.
     * This is not evidence that DOS would accept AX=252Bh with these args. */
    return 0u;
}

void ef_decomp_x32_before_traps(void)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_vectors || x32_request_count != 9u)
        fatal_unexpected("unexpected X-32 trap setup entry");
    if (!x32_virtual_traps)
        stop_at_guest_boundary(0x0009c8b4u);
}

/* Original 0x9c8bd..0x9c8d1 copies ESP, SS and DS before switching DS to
 * CS. Native64 cannot obtain that 32-bit protected-mode caller context.
 * Values below are deliberately synthetic and have no host side effects. */
void ef_decomp_x32_trap_context(uint32_t *esp, uint16_t *ss, uint16_t *ds)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_vectors || !x32_virtual_traps ||
        x32_request_count != 9u || x32_trap_count != 0u)
        fatal_unexpected("X-32 trap context outside virtual-only probe");
    *esp = EF_DECOMP_RAM_SIZE - 0x1000u;
    *ss = 0x9abcu;
    *ds = 0x5678u;
}

/* This records the eight original AX=2533h descriptions; it never performs
 * INT 21h, changes the host's interrupt handlers, or reports DOS success. */
void ef_decomp_x32_virtual_trap(uint32_t vector, uint32_t handler)
{
    static const uint8_t expected[8] = { 3u, 6u, 9u, 10u, 11u, 12u, 13u, 14u };
    uint32_t index = x32_trap_count;
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_vectors || !x32_virtual_traps ||
        x32_request_count != 9u || index >= 8u ||
        vector != expected[index] ||
        handler != (index == 0u ? 0x0009c908u : 0x0009c909u) ||
        EF_U32(0x00132a9cu) != 0xd68u ||
        EF_U32(0x00132aa0u) != EF_DECOMP_RAM_SIZE - 0x1000u ||
        EF_U16(0x00132aa4u) != 0x9abcu ||
        EF_U16(0x00132aa6u) != 0x5678u)
        fatal_unexpected("X-32 virtual trap registration (0x9c8b4)");
    x32_traps[index].vector = vector;
    x32_traps[index].handler = handler;
    ++x32_trap_count;
}

void ef_decomp_x32_after_initializer(void)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_vectors || !x32_virtual_traps ||
        x32_request_count != 9u || x32_trap_count != 8u)
        fatal_unexpected("X-32 initializer returned without eight trap requests");
    if (!x32_virtual_input)
        stop_at_guest_boundary(0x00000f43u);
}

/* The strict cold-input continuation stops before the DOS AX=2503h call.
 * A separately opt-in timer probe feeds synthetic return values to the
 * recovered 0x734 source, without emulating an X-32 interrupt. */
uint32_t guest_fn_324(uint32_t source)
{
    if (probing && main_probe_active && x32_probe_active &&
        x32_virtual_input && x32_virtual_traps && x32_request_count == 11u &&
        x32_trap_count == 8u && source == 9u &&
        EF_U32(0x00196144u) == 5u &&
        EF_U32(0x00196140u) == 7u &&
        read_x32_packed_dword(0x001960c4u + 7u * 14u) == 9u &&
        EF_U32(0x0041f828u) == 6u &&
        EF_U32(0x0041f82cu) == 5u &&
        EF_U16(0x000fe624u) == 0x5678u &&
        keyboard_led_bytes == 2u && !keyboard_ack_pending) {
        if (x32_virtual_timer)
            return 0x2468u; /* synthetic AX=2503h result */
        stop_at_guest_boundary(0x00000324u);
    }
    fatal_unexpected("X-32 timer source (0x324)");
    return 0u;
}
uint64_t guest_fn_348(uint32_t source)
{
    if (probing && main_probe_active && x32_probe_active &&
        x32_virtual_timer && x32_virtual_input && source == 9u &&
        read_x32_packed_dword(0x001960c8u + 7u * 14u) == 0x2468u &&
        x32_timer_registrations == 0u)
        return UINT64_C(0xabcd12345678); /* synthetic AX=2502h return */
    fatal_unexpected("X-32 timer source (0x348)");
    return 0u;
}
void guest_fn_484(uint32_t source, uint32_t high,
                  uint32_t flags, uint32_t selector)
{
    if (probing && main_probe_active && x32_probe_active &&
        x32_virtual_timer && x32_virtual_input &&
        x32_timer_registrations == 0u && source == 9u &&
        high == 0xabcdu && flags == 0x000fe61fu && selector == 0x1234u &&
        EF_U32(0x00196140u) == 7u &&
        read_x32_packed_dword(0x001960c4u + 7u * 14u) == 9u &&
        read_x32_packed_dword(0x001960c8u + 7u * 14u) == 0x2468u &&
        read_x32_packed_dword(0x001960ccu + 7u * 14u) == 0x12345678u &&
        EF_U16(0x001960d0u + 7u * 14u) == 0xabcdu) {
        ++x32_timer_registrations;
        return;
    }
    fatal_unexpected("X-32 timer installation (0x484)");
}

/* The real 0xcd4d4 returns at 0xcd550. Check its final guest writes only
 * in the virtual-timer mode; the other probes still stop at their original
 * unsupported guest boundaries. */
void ef_decomp_x32_after_input(void)
{
    if (!x32_probe_active)
        return;
    if (!probing || !main_probe_active || !x32_virtual_timer ||
        x32_timer_registrations != 1u || hook_registrations != 2u ||
        release_hook != 0xad8u || input_cleanup_hook != 0x000cd554u ||
        EF_U8(0x0015b72eu) != 1u || EF_U32(0x0041f824u) != 7u)
        fatal_unexpected("completed X-32 cold input initializer");
    ++x32_input_returns;
    if (!x32_virtual_tactcom)
        stop_at_guest_boundary(0x000cd550u);
}

void guest_dos_int_x(uint32_t interrupt, uint32_t in,
                     uint32_t out, uint32_t sregs)
{
    (void)interrupt; (void)in; (void)out; (void)sregs;
    fatal_unexpected("DOS interrupt services before X-32 reconstruction");
}

/* Bind the original call to the actually recovered C implementation. */
void guest_fn_e2a1c(void)
{
    if (!probing)
        fatal_unexpected("arena reset (0xe2a1c)");
    reset_buffer_e2a1c();
}

void guest_fn_b3fb0(uint32_t destination, uint32_t value, uint32_t count)
{
    void *span;
    if (!probing || !(span = ef_decomp_memory_span(destination, count)))
        fatal_unexpected("bounded guest fill (0xb3fb0)");
    memset(span, (int)(uint8_t)value, count);
}

static void stop_at_guest_boundary(uint32_t address)
{
    if (!main_probe_active)
        fatal_unexpected("game entry boundary outside a main probe");
    main_boundary_address = address;
    longjmp(main_boundary, 1);
}

/* Probe-only X-32 350Bh memory accounting.  This models the remaining
 * bounded host-owned guest RAM after the recovered game arena; it is not
 * claimed to match the original extender's total heap or swap policy. */
void guest_fn_2b0(ef_addr32 request, ef_addr32 result)
{
    uint32_t used = next_allocation + requested_arena;
    if (!probing || !main_probe_active || !arena_allocated ||
        request != 0x00196148u || result != request ||
        EF_U32(request) != 0x350bu || used > EF_DECOMP_RAM_SIZE)
        fatal_unexpected("X-32 350Bh (0x2b0)");
    EF_U32(result) = EF_DECOMP_RAM_SIZE - used;
    EF_U32(0x00196160u) = 0;
}

void ui_report_missing_record(ef_addr32 message)
{
    (void)message;
    fatal_unexpected("X-32 350Bh returned an error");
}

void guest_string_copy(ef_addr32 dest, ef_addr32 src)
{
    if (!probing || !main_probe_active)
        fatal_unexpected("guest string copy outside TACTCOM probe");
    (void)copy_string_9d749(dest, src);
}

void guest_fn_cd12e(ef_addr32 format, uint32_t string_arg, uint32_t unused)
{
    const char *fmt;
    const char *arg;
    const char *end;
    const char *percent;
    if (!probing || !main_probe_active || format != 0x0018a810u || unused != 0)
        fatal_unexpected("TACTCOM memory diagnostic (0xcd12e)");
    fmt = (const char *)ef_decomp_memory_span(format, 128);
    arg = (const char *)ef_decomp_memory_span(string_arg, 40);
    if (!fmt || !arg || !(end = memchr(fmt, 0, 128)) ||
        !(percent = memchr(fmt, '%', (size_t)(end - fmt))) ||
        percent + 1 >= end || percent[1] != 's' ||
        memchr(percent + 2, '%', (size_t)(end - percent - 2)) ||
        !memchr(arg, 0, 40))
        fatal_unexpected("unsupported TACTCOM diagnostic format");
    fwrite(fmt, 1, (size_t)(percent - fmt), stdout);
    fputs(arg, stdout);
    fwrite(percent + 2, 1, (size_t)(end - percent - 2), stdout);
}

uint32_t guest_tail_fa8a0(uint32_t stream)
{
    if (!probing || !main_probe_active || stream != 0x0016043eu)
        fatal_unexpected("TACTCOM stdout flush (0xfa8a0)");
    return fflush(stdout) == 0 ? 0 : UINT32_MAX;
}

void guest_fn_10ddf7_s(void)
{
    fatal_unexpected("TACTCOM unexpected stream reset (0x10ddf7)");
}

void guest_tactcom_low_memory_8b417(ef_addr32 required, ef_addr32 missing)
{
    (void)required;
    (void)missing;
    stop_at_guest_boundary(0x8b417u);
}

uint32_t guest_fn_9c858_arg(uint32_t name)
{
    if (!probing || !main_probe_active || name != 0x0018b154u)
        fatal_unexpected("TACTCOM getenv (0x9c858)");
    return getenv_9c858(name);
}

void guest_fn_eb478(ef_addr32 text)
{
    if (!probing || !main_probe_active ||
        (text != TACTCOM_PROBE_FRAME + 0xa0u &&
         text != TACTCOM_VIEW_FRAME && text != TACTCOM_SEQ_FRAME &&
         text != TACTCOM_AID_FRAME))
        fatal_unexpected("TACTCOM pathname lowercase (0xeb478)");
    (void)lowercase_string(text);
}

uint32_t guest_tactcom_file_probe_8b45a(ef_addr32 path)
{
    const char *name;
    uint32_t size;
    if (!probing || !main_probe_active || path != TACTCOM_PROBE_FRAME + 0xa0u ||
        !(name = (const char *)ef_decomp_memory_span(path, 0x50u)) ||
        !memchr(name, 0, 0x50u))
        fatal_unexpected("TACTCOM resource pathname before file probe");
    printf("TACTCOM first resource pathname: %s\n", name);
    if (EF_U32(0x00132ab4u) != 0x0009cb20u)
        fatal_unexpected("unexpected startup file callback at 0x132ab4");
    size = find_in_archives_9cb20(path);
    printf("TACTCOM first resource payload size: %u bytes\n", size);
    return size;
}

void guest_tactcom_missing_resource_8b464(ef_addr32 message)
{
    const char *format;
    const char *end;
    if (message != 0x00132884u || EF_U32(0x00132a74u) != 0x000ea50cu)
        fatal_unexpected("unexpected TACTCOM missing-resource diagnostic");

    /* Original 0xea50c is a variadic formatter backed by Watcom's 32-bit
     * stack. This one call has no variadic arguments; copy the literal into
     * guest RAM and route its would-be file log to stderr (read-only host).
     * Preserve the original reentry latch and 0xeb448 message dispatcher. */
    format = (const char *)ef_decomp_memory_span(message, 256u);
    if (!probing || !main_probe_active || !format ||
        !(end = memchr(format, 0, 256u)) ||
        memchr(format, '%', (size_t)(end - format)) ||
        !ef_decomp_memory_span(TACTCOM_LOG_FRAME, 256u))
        fatal_unexpected("unsupported TACTCOM log format (0xea50c)");
    EF_U8(0x00160638u)++;
    if ((int8_t)EF_U8(0x00160638u) != 1)
        return;
    (void)copy_string_9d749(TACTCOM_LOG_FRAME, message);
    report_dos_message((const char *)ef_decomp_memory_span(TACTCOM_LOG_FRAME, 256u));
    enter_handler_eb448(TACTCOM_LOG_FRAME);
}

void guest_fn_eb25a(uint32_t page)
{
    if (!probing || !main_probe_active || page != UINT32_MAX ||
        EF_U32(0x00160a24u) != TACTCOM_LOG_FRAME)
        fatal_unexpected("TACTCOM diagnostic page switch (0xeb25a)");
    stop_at_guest_boundary(0x000eb25au);
}

void guest_fn_9cfdc_w(uint32_t name)
{
    if (!probing || !main_probe_active)
        fatal_unexpected("options handler selection outside main probe");
    select_handlers_9cfdc(name);
}

void guest_fn_9ca57(ef_addr32 dst, ef_addr32 src)
{
    if (!probing || !main_probe_active || dst != 0x0040d150u ||
        (src != 0x0018b1b4u && src != EF_U32(0x0012ddb0u)))
        fatal_unexpected("options path concatenation (0x9ca57)");
    (void)strcat_9ca57(dst, src);
}

uint32_t guest_fn_d6694(void)
{
    if (!probing || !main_probe_active)
        fatal_unexpected("options input sampling outside main probe");
    return run_steps_d6694();
}

uint32_t guest_fn_d66c2_x(void)
{
    /* Deterministic disconnected-gameport model for the Linux probe. The
     * original 0xd66c2 first samples port 0x201 with IN AL,DX; 0xff clears
     * the four active-low button latches. Its subsequent stability loop
     * samples 0xff twenty times and returns zero (no analog sample).
     * Attached controllers and pulse-timing are not modeled here. */
    if (!probing || !main_probe_active)
        fatal_unexpected("joystick sample outside main probe");
    EF_U8(0x0015ba2cu) = 0;
    EF_U8(0x0015ba2du) = 0;
    EF_U8(0x0015ba2eu) = 0;
    EF_U8(0x0015ba2fu) = 0;
    return 0;
}

uint8_t guest_fn_e94ec(ef_addr32 path, uint32_t flags)
{
    if (!probing || !main_probe_active ||
        path != EF_U32(0x0012b36cu) || flags != 0u ||
        EF_U32(0x00132ab4u) != 0x0009cf74u ||
        EF_U32(0x00132ab8u) != 0x0009cfa4u)
        fatal_unexpected("options text-file loader (0xe94ec)");
    return load_text_file_e94ec(path, flags);
}

/* Bind the game's localized size/read callbacks to recovered game C. The
 * additional frame is a private 0x90-byte guest scratch area replacing the
 * Watcom stack. Neither callback can access an arbitrary host pointer. */
static void validate_localized_name(ef_addr32 name)
{
    const char *filename = (const char *)ef_decomp_memory_span(name, 0x90u);
    const char *prefix = (const char *)ef_decomp_memory_span(0x0040d150u, 0x90u);
    if (!probing || !main_probe_active || !ef_decomp_files_enabled() ||
        !filename || !prefix || !memchr(filename, 0, 0x90u) ||
        !memchr(prefix, 0, 0x90u) ||
        strlen(filename) + strlen(prefix) >= 0x90u ||
        EF_U32(0x0040d1e0u) > strlen(filename) ||
        !ef_decomp_memory_span(TACTCOM_TEXT_FRAME, 0x90u))
        fatal_unexpected("localized file callback path");
}

uint32_t guest_callptr_eax_ret_x(ef_addr32 fn, uint32_t name)
{
    if (fn != 0x0009cf74u || EF_U32(0x00132ab4u) != fn)
        fatal_unexpected("unbound localized file-size callback");
    validate_localized_name(name);
    return size_localized_file_9cf74(name, TACTCOM_TEXT_FRAME);
}

void guest_callptr_eaxedx_x(ef_addr32 fn, uint32_t name, uint32_t dest)
{
    uint32_t result;
    if (fn != 0x0009cfa4u || EF_U32(0x00132ab8u) != fn)
        fatal_unexpected("unbound localized file-read callback");
    validate_localized_name(name);
    result = load_localized_file_9cfa4(name, dest, TACTCOM_TEXT_FRAME);
    if (result != 1u)
        stop_at_guest_boundary(0x000b3c04u);
}

uint32_t guest_fn_b3c04_x(uint32_t name, uint32_t dest)
{
    uint32_t bytes;
    int result;
    if (!probing || !main_probe_active)
        fatal_unexpected("file loader outside startup probe");
    bytes = read_file_payload_size_b3b90(name);
    if (bytes == 0)
        return 0;
    result = ef_decomp_files_read_plain(name, dest, bytes);
    if (result < 0)
        stop_at_guest_boundary(0x000b3c04u);
    return (uint32_t)result;
}

uint32_t guest_fn_eba64_x(ef_addr32 name, ef_addr32 dest, ef_addr32 archive)
{
    (void)name;
    (void)dest;
    (void)archive;
    stop_at_guest_boundary(0x000eba64u);
    return 0;
}

void guest_callptr1_x(ef_addr32 fn, ef_addr32 arg)
{
    (void)fn;
    (void)arg;
    stop_at_guest_boundary(0x000ea50cu);
}

uint8_t guest_fn_e93dc_r(uint32_t source, uint32_t flags)
{
    uint32_t config = EF_U32(0x001600a0u);
    uint32_t allocated;
    const char *contents;
    if (!probing || !main_probe_active || source != 0x0012b370u || flags != 0u ||
        EF_U8(0x001600a4u) != 1u || config < 4u ||
        !(allocated = EF_U32(config - 4u)) ||
        allocated < 0x25u ||
        !(contents = (const char *)ef_decomp_memory_span(config, allocated - 4u)) ||
        !memchr(contents, 0, allocated - 4u))
        fatal_unexpected("TACTCOM text parser input (0xe93dc)");
    return parse_script_text_e93dc(source, flags);
}

/* The original token interpreter at 0xe882c is substantially larger than
 * the recovered scalar and configuration-list subsets. Keep unsupported
 * syntax, device paths and diagnostic calls at exact native boundaries. */
uint8_t guest_fn_e882c_x(void)
{
    uint32_t cur = EF_U32(0x00464384u);
    const char *text;
    size_t length = 0;
    if (!probing || !main_probe_active ||
        !(text = (const char *)ef_decomp_memory_span(cur, 0x85u)))
        fatal_unexpected("TACTCOM token parser cursor (0xe882c)");
    if (text[0] == ';') {
        /* At 0xe88ef the original directly skips a leading ';' after
         * its bounded (0x84-byte) line scan, returning success without
         * interpreting any setting. The larger function stays a boundary. */
        while (length < 0x84u && text[length] != 0 && text[length] != '\r')
            length++;
        if (length < 0x84u)
            return parse_comment_line_e882c();
    }
    if (parse_scalar_setting_e882c())
        return 1u;
    if (parse_text_setting_e882c())
        return 1u;
    stop_at_guest_boundary(0x000e882cu);
    return 0;
}

/* Probe-only adapter for the plain-file branch of 0x9ccb8. The slot
 * allocation, eight-byte read and relative rewind use recovered guest C;
 * only their read-only stream operations are provided by decomp_files.c.
 * Archive lookup and packed JR/RA decoding remain hard boundaries. */
uint32_t guest_fn_9ccb8(ef_addr32 path)
{
    const char *name;
    const char *suffix;
    size_t length, suffix_length;
    int32_t handle;
    uint32_t read_count;
    const ef_addr32 header = path + (path == TACTCOM_VIEW_FRAME ? 0x98u :
                                    path == TACTCOM_AID_FRAME ? 0xa0u : 0x80u);
    const ef_addr32 suffix_addr = path == TACTCOM_VIEW_FRAME ? 0x00182794u :
                                  path == TACTCOM_AID_FRAME ? 0x0017f3c4u :
                                  0x00182714u;
    if (!probing || !main_probe_active ||
        (path != TACTCOM_VIEW_FRAME && path != TACTCOM_SEQ_FRAME &&
         path != TACTCOM_AID_FRAME) ||
        !(name = (const char *)ef_decomp_memory_span(path, 0x94u)) ||
        !(suffix = (const char *)ef_decomp_memory_span(suffix_addr, 0x30u)) ||
        !memchr(name, 0, 0x94u) || !memchr(suffix, 0, 0x30u))
        fatal_unexpected("Views/Aid resource-open path (0x9ccb8)");
    length = strlen(name);
    suffix_length = strlen(suffix);
    if (length < suffix_length ||
        memcmp(name + length - suffix_length, suffix, suffix_length) != 0)
        fatal_unexpected("Views/Aid resource-open suffix (0x9ccb8)");
    printf("TACTCOM sequence resource pathname: %s\n", name);
    if (!ef_decomp_files_enabled())
        stop_at_guest_boundary(0x0009ccb8u);
    if ((EF_U32(0x0040d1f0u) && (EF_U8(0x00132ab0u) & 1u)) ||
        (EF_U32(0x0040d1f4u) && (EF_U8(0x00132ab0u) & 2u)))
        stop_at_guest_boundary(0x000ebb58u);

    handle = open_slot_a7900(0u, 0u, 0u, 0u, path);
    if (handle == -1)
        return UINT32_MAX;
    if (!ef_decomp_memory_span(header, 8u))
        fatal_unexpected("Views.c resource header buffer");
    read_count = read_handle_a79d4(0u, 0u, 0u, 0u, (uint32_t)handle, 8u, header);
    /* The original does not check short header reads and uses stack bytes
     * that may be uninitialized. No defined compressed/plain branch exists
     * for a short file, so stop rather than manufacture one. */
    if (read_count != 8u)
        stop_at_guest_boundary(0x0009ccb8u);
    if (EF_U16(header) == 0x524au || EF_U16(header) == 0x4152u) {
        close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
        stop_at_guest_boundary(0x0009cd53u);
    }
    if (seek_current_a7ac8(0u, 0u, 0u, 0u, (uint32_t)handle, (uint32_t)-8) != 0u)
        fatal_unexpected("Views.c plain-file rewind");
    return (uint32_t)handle;
}

void guest_callptr2_x(ef_addr32 fn, uint32_t format, uint32_t filename)
{
    if (!probing || !main_probe_active || fn != 0x000ea50cu ||
        EF_U32(0x00132a74u) != fn ||
        !((format == 0x001827a8u && filename == TACTCOM_VIEW_FRAME) ||
          (format == 0x00182724u && filename == TACTCOM_SEQ_FRAME) ||
          (format == 0x0017f3d4u && filename == TACTCOM_AID_FRAME)))
        fatal_unexpected("Views/Aid missing-file diagnostic callback");
    stop_at_guest_boundary(0x000ea50cu);
}

uint32_t guest_fn_9d060(uint32_t size)
{
    if (!probing || !main_probe_active || size != 0x56cu)
        fatal_unexpected("Views.c stream allocator request (0x9d060)");
    return alloc_or_abort_9d060(size);
}

uint32_t guest_fn_9cde8(ef_addr32 dest, uint32_t count, uint32_t owner)
{
    const uint32_t stream = next_allocation + requested_arena;
    if (!probing || !main_probe_active || !stream_allocated ||
        dest != stream + 0x451u || count != 0x100u || owner != 100u ||
        !ef_decomp_memory_span(dest, count) ||
        EF_U32(0x001371a4u) == 0u)
        fatal_unexpected("Views.c uncompressed resource read (0x9cde8)");
    if (EF_U32(stream + 0x552u) == 0u &&
        EF_U32(stream + 0x558u) == 0u &&
        guest_fn_a7a64_r(0u, 1u, owner) != 0u)
        fatal_unexpected("Views.c resource was not rewound");
    /* The original dispatches through 0xa79d4 when handle flags 16..18
     * are clear; the recovered handle reader calls the read-only adapter. */
    return read_handle_a79d4(0u, 0u, 0u, 0u, owner, count, dest);
}

static void check_view_parser_context(ef_addr32 stream)
{
    const uint32_t expected = next_allocation + requested_arena;
    const char *line, *token;
    if (!probing || !main_probe_active || !stream_allocated ||
        stream != expected ||
        EF_U32(stream + 0x568u) != 0x0009d084u ||
        EF_U32(stream + 0x560u) != 100u)
        fatal_unexpected("Views.c parser context");
    line = (const char *)ef_decomp_memory_span(stream, 0x401u);
    token = (const char *)ef_decomp_memory_span(stream + 0x400u, 0x51u);
    if (!line || !token || !memchr(line, 0, 0x401u) ||
        !memchr(token, 0, 0x51u))
        fatal_unexpected("Views.c unterminated sequence line or token");
}

void guest_tactcom_view_name_guard_30407(ef_addr32 stream)
{
    const char *name;
    check_view_parser_context(stream);
    name = (const char *)ef_decomp_memory_span(stream + 0x400u, 0x51u);
    /* The original strcpy at 0x30407 is unchecked. Limit this host-only
     * probe to the 0x1e-byte name field, preserving its safe cases. */
    if (!name || strlen(name) >= 0x1eu)
        stop_at_guest_boundary(0x00030407u);
}

void guest_tactcom_view_string_guard_3053d(ef_addr32 stream, uint32_t capacity)
{
    const char *value;
    check_view_parser_context(stream);
    if (capacity != 0x1eu)
        fatal_unexpected("Views.c Name field capacity");
    value = (const char *)ef_decomp_memory_span(stream + 0x400u, 0x51u);
    if (!value || strlen(value) >= capacity)
        stop_at_guest_boundary(0x0003053du);
}

void guest_tactcom_view_string_guard_30577(ef_addr32 stream, uint32_t capacity)
{
    const char *value;
    check_view_parser_context(stream);
    if (capacity != 0x0fu)
        fatal_unexpected("Views.c Key field capacity");
    value = (const char *)ef_decomp_memory_span(stream + 0x400u, 0x51u);
    if (!value || strlen(value) >= capacity)
        stop_at_guest_boundary(0x00030577u);
}

/* Only the ordinary decimal syntax needed by the bounded Views probe is
 * supported. The original 0x9d55d also has uncommon partial/overflow/error
 * semantics; reject those cases at its address until that routine is fully
 * reconstructed and tested against the guest. */
static int view_ordinary_decimal(const char *p)
{
    int digits = 0;
    if (*p == '+' || *p == '-')
        ++p;
    while (*p >= '0' && *p <= '9') {
        ++digits;
        ++p;
    }
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') {
            ++digits;
            ++p;
        }
    }
    if (digits == 0)
        return 0;
    if (*p == 'e' || *p == 'E') {
        int exp_digits = 0;
        ++p;
        if (*p == '+' || *p == '-')
            ++p;
        while (*p >= '0' && *p <= '9') {
            ++exp_digits;
            ++p;
        }
        if (!exp_digits)
            return 0;
    }
    return *p == 0;
}

double guest_tactcom_view_decimal_9d55d(ef_addr32 stream)
{
    const char *token;
    char *end;
    double value;
    check_view_parser_context(stream);
    token = (const char *)ef_decomp_memory_span(stream + 0x400u, 0x51u);
    if (!token || !view_ordinary_decimal(token))
        stop_at_guest_boundary(0x0009d55du);
    value = strtod(token, &end);
    if (*end != 0 || !isfinite(value))
        stop_at_guest_boundary(0x0009d55du);
    return value;
}

void guest_tactcom_view_unknown_option_boundary_310d8(ef_addr32 stream)
{
    check_view_parser_context(stream);
    stop_at_guest_boundary(0x000310d8u);
}

void guest_tactcom_view_count_boundary_31116(ef_addr32 stream)
{
    check_view_parser_context(stream);
    stop_at_guest_boundary(0x00031116u);
}

/* 0x3120b: the original releases the stream via 0x9d088 -> 0xc3f80.
 * This host probe owns only one stream allocation; memory remains readable
 * after free, as in the original allocator, until a subsequent allocation. */
void guest_tactcom_view_release_stream_9d088(ef_addr32 stream)
{
    check_view_parser_context(stream);
    stream_allocated = 0;
}

/* 0x31219 and 0x301ce: unflagged 0x9ce84 dispatches to 0xa7990, clearing
 * the game DOS handle slot. The bounded caller then continues to the next
 * recovered Views step; neither adapter imitates the original X-32 heap. */
void guest_tactcom_view_close_handle_9ce84(uint32_t handle)
{
    if (!probing || !main_probe_active || stream_allocated || handle != 100u ||
        (EF_U32(TACTCOM_VIEW_FRAME + 0x84u) != handle &&
         EF_U32(TACTCOM_SEQ_FRAME + 0x64u) != handle &&
         EF_U32(TACTCOM_AID_FRAME + 0x70u) != handle) ||
        EF_U32(0x001371a4u) == 0u)
        fatal_unexpected("Views.c completed resource close");
    close_handle_a7990(0u, 0u, 0u, 0u, handle);
    if (EF_U32(0x001371a4u) != 0u)
        fatal_unexpected("Views.c handle remains open after close");
}

void guest_tactcom_view_invalid_index_boundary_3120b(ef_addr32 stream)
{
    check_view_parser_context(stream);
    stop_at_guest_boundary(0x0003120bu);
}

void guest_tactcom_sequence_missing_camera_300e6(ef_addr32 stream)
{
    check_view_parser_context(stream);
    stop_at_guest_boundary(0x000300e6u);
}

void guest_tactcom_sequence_entry_boundary_3010e(ef_addr32 stream)
{
    check_view_parser_context(stream);
    stop_at_guest_boundary(0x0003010eu);
}

/* Synthetic fixture safety only: original 0x9d749 uses unbounded strcpy.
 * The third name shares space with the ON flag at +0x4b. */
void guest_tactcom_sequence_string_guard_3001d(ef_addr32 stream,
                                               uint32_t capacity, uint32_t site)
{
    const char *name;
    check_view_parser_context(stream);
    if (!((capacity == 0x1eu && (site == 0x3001du || site == 0x3003cu)) ||
          (capacity == 0x0fu && site == 0x3005bu)))
        fatal_unexpected("Views.c seq2 text field bound");
    name = (const char *)ef_decomp_memory_span(stream + 0x400u, 0x51u);
    if (!name || strlen(name) >= capacity)
        stop_at_guest_boundary(site);
}

void guest_fn_30214_w(void)
{
    const char *source;
    const char *base = NULL;
    uint32_t base_addr;
    if (!probing || !main_probe_active ||
        !ef_decomp_memory_span(TACTCOM_VIEW_FRAME, 0x94u))
        fatal_unexpected("ReadGenSeqs outside TACTCOM main probe");
    /* Keep the original unchecked string append bounded in this host probe. */
    source = (const char *)ef_decomp_memory_span(0x00182794u, 0x30u);
    base_addr = EF_U32(0x00132aa8u);
    if (base_addr != 0u)
        base = (const char *)ef_decomp_memory_span(base_addr, 0x60u);
    if (!source || !memchr(source, 0, 0x30u) ||
        (base_addr != 0u && (!base || !memchr(base, 0, 0x60u))) ||
        strlen(source) + (base ? strlen(base) : 0u) >= 0x94u)
        fatal_unexpected("ReadGenSeqs pathname bounds");
    read_gen_sequences_prefix_30214(TACTCOM_VIEW_FRAME);
}
void guest_fn_2ff44_w(void)
{
    if (!probing || !main_probe_active ||
        !ef_decomp_memory_span(TACTCOM_SEQ_FRAME, 0x84u))
        fatal_unexpected("ReadSeqs outside TACTCOM main probe");
    read_sequence_lists_prefix_2ff44(TACTCOM_SEQ_FRAME);
}
void guest_tactcom_aid_row_guard_f80(ef_addr32 stream, uint32_t site)
{
    check_view_parser_context(stream);
    if (site != 0x00001171u && site != 0x000011cbu &&
        site != 0x0000120eu && site != 0x00001217u &&
        site != 0x00001238u && site != 0x00001244u &&
        site != 0x00001254u && site != 0x000014c9u &&
        site != 0x00001544u && site != 0x0000157cu &&
        site != 0x000015cbu && site != 0x00001633u &&
        site != 0x000016c2u && site != 0x000016ceu &&
        site != 0x000016deu && site != 0x00001953u &&
        site != 0x000019d4u && site != 0x00001a0cu &&
        site != 0x00001a58u)
        fatal_unexpected("Aid.c row-guard caller");
    if (EF_U8(stream) == 0u)
        stop_at_guest_boundary(site);
}

void guest_tactcom_aid_enum_boundary_f80(ef_addr32 stream, uint32_t site)
{
    check_view_parser_context(stream);
    if (site != 0x00001365u && site != 0x000014a7u &&
        site != 0x000017efu && site != 0x00001931u)
        fatal_unexpected("Aid.c relationship error site");
    stop_at_guest_boundary(site);
}

void guest_tactcom_aid_name_guard_f80(ef_addr32 stream, uint32_t site)
{
    const char *name;
    check_view_parser_context(stream);
    if (site != 0x000015a5u && site != 0x00001a32u)
        fatal_unexpected("Aid.c string-copy caller");
    name = (const char *)ef_decomp_memory_span(stream + 0x400u, 0x51u);
    if (!name || strlen(name) >= 0x32u)
        stop_at_guest_boundary(site);
}

void guest_tactcom_aid_count_guard_f80(ef_addr32 stream, uint32_t object_index,
                                      uint32_t row, uint32_t column, uint32_t site)
{
    check_view_parser_context(stream);
    if (site != 0x00001252u && site != 0x000016dcu)
        fatal_unexpected("Aid.c table-count caller");
    if (object_index >= 50u || row >= 5u || column >= 10u)
        stop_at_guest_boundary(site);
}

void guest_tactcom_aid_unknown_special_f80(ef_addr32 stream)
{
    check_view_parser_context(stream);
    stop_at_guest_boundary(0x00001687u);
}

void guest_fn_f80_w(void)
{
    if (!probing || !main_probe_active ||
        !ef_decomp_memory_span(TACTCOM_AID_FRAME, 0xacu))
        fatal_unexpected("ReadAidData outside TACTCOM main probe");
    read_aid_data_prefix_f80(TACTCOM_AID_FRAME);
}

void guest_fn_cd4d4_x(void)
{
    if (!probing || !main_probe_active)
        fatal_unexpected("keyboard input initializer outside TACTCOM");
    initialize_input_services_cd4d4();
}

void guest_tactcom_post_options_boundary_8b477(void)
{
    /* The recovered post-input slice reads the original 0xce08c global and
     * dispatches the conditional page/input handlers in source order. */
    if (!probing || !main_probe_active)
        fatal_unexpected("TACTCOM post-input outside main probe");
    tactcom_after_input_8b47c();
}

/* 0xe28e8 has set its one-time flag and entered original module initializer
 * 0xd6dc4. The latter calls 0x101c54 before installing its module handler.
 * This external sound/runtime service is deliberately not simulated. */
void guest_fn_101c54(void)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_sound_timer || !x32_virtual_tactcom ||
        EF_U8(0x0015f751u) != 1u || EF_U8(0x0015bac5u) != 0u)
        fatal_unexpected("first sound module startup service (0x101c54)");
    if (x32_virtual_ail_init) {
        initialize_ail_trace_101c54();
        if (x32_virtual_ail_callback && ail_irq_critical_exited == 1u)
            return;
        fatal_unexpected("AIL debug initializer returned without AIL settings boundary");
    }
    stop_at_guest_boundary(0x00101c54u);
}

void guest_ail_trace_debug_path_101c87(uint32_t debug_file)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_init || debug_file == 0u)
        fatal_unexpected("AIL debug file branch (0x101c87)");
    stop_at_guest_boundary(0x00101c87u);
}

uint32_t EF_CDECL guest_fn_1041e4_t(uint32_t index, uint32_t value)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_init || index != ail_setting_count || index >= 19u ||
        ail_lock_count != AIL_DEFAULT_LOCK_COUNT ||
        EF_U32(0x0017d2ccu) != 1u || EF_U32(0x0017d2d4u) != 1u ||
        EF_U32(0x004dd624u) != 0u || EF_U32(0x004dd628u) != 0u)
        fatal_unexpected("AIL default-setting service (0x1041e4)");
    ail_setting_count++;
    return exchange_slot_1041e4(index, value);
}

void guest_fn_110fd4_x(void)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_init || ail_setting_count != 19u ||
        EF_U32(0x0017d2ccu) != 1u || EF_U32(0x0017d2d4u) != 1u ||
        EF_U8(0x004dd6b0u) != 0u)
        fatal_unexpected("unimplemented AIL default-settings tail service (0x110fd4)");
    if (x32_virtual_ail_irq) {
        initialize_ail_irq_tables_110fd4();
        if (x32_virtual_ail_callback && ail_irq_critical_exited == 1u)
            return;
        fatal_unexpected("AIL IRQ table prefix returned without interrupt boundary");
    }
    stop_at_guest_boundary(0x00110fd4u);
}

uint16_t guest_ail_irq_enter_110fd9(void)
{
    /* Record the original pushf/CLI/DS->ES transition, without touching
     * host interrupt flags. The test X-32 services already captured DS. */
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_irq || ail_setting_count != 19u ||
        ail_lock_count != AIL_DEFAULT_LOCK_COUNT ||
        EF_U16(0x000fe624u) != 0x5678u || ail_irq_critical_count != 0u)
        fatal_unexpected("AIL IRQ critical-section entry (0x110fd9)");
    ail_irq_critical_count = 1u;
    return EF_U16(0x000fe624u);
}

uint64_t guest_ail_irq8_vector_query_11105b(uint32_t vector)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_irq || ail_irq_critical_count != 1u ||
        ail_lock_count != AIL_IRQ_LOCK_COUNT ||
        EF_U16(0x0017d7beu) != 0x5678u ||
        EF_U32(0x0017d7b6u) != 0u || EF_U32(0x0017d7bau) != 0u ||
        EF_U32(0x0017d7b2u) != UINT32_MAX ||
        EF_U32(0x0017d7c0u) != UINT32_MAX ||
        vector != 8u || ail_irq_vector_queries != 0u)
        fatal_unexpected("AIL IRQ 8 vector query (0x11105b)");
    if (!x32_virtual_ail_vectors)
        stop_at_guest_boundary(0x0011105bu);
    ++ail_irq_vector_queries;
    /* Synthetic protected-mode ES:EBX, not actual host or DOS state. */
    return UINT64_C(0xabcd12345678);
}

uint32_t guest_ail_irq8_realmode_query_111065(uint32_t vector)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_vectors || !x32_virtual_ail_irq ||
        ail_irq_critical_count != 1u || vector != 8u ||
        ail_irq_vector_queries != 1u || ail_irq_vector_registrations != 0u)
        fatal_unexpected("AIL IRQ 8 real-mode query (0x111065)");
    ++ail_irq_vector_queries;
    return 0x2468u; /* Synthetic real-mode EBX. */
}

uint16_t guest_ail_code_selector_111089(void)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_vectors || ail_irq_vector_queries != 2u ||
        EF_U16(0x000fe624u) != 0x5678u)
        fatal_unexpected("AIL protected-mode handler code selector (0x111089)");
    return 0x1234u; /* The existing probe-only synthetic CS. */
}

void guest_ail_irq8_install_111097(uint32_t vector, uint32_t handler,
                                   uint16_t selector)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_vectors || !x32_virtual_ail_irq ||
        ail_irq_vector_queries != 2u || ail_irq_vector_registrations != 0u ||
        vector != 8u || handler != 0x00110de6u || selector != 0x1234u ||
        EF_U32(0x0017d7a4u) != 0x12345678u ||
        EF_U16(0x0017d7a8u) != 0xabcdu ||
        EF_U32(0x0017d7aau) != 0x2468u)
        fatal_unexpected("AIL IRQ 8 protected-mode vector install (0x111097)");
    /* Record only; do not issue INT 21h AX=2506h or install host IRQs. */
    ++ail_irq_vector_registrations;
}

void guest_ail_timer_setup_1110ab(uint32_t slot_offset, uint32_t callback)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_vectors || !x32_virtual_ail_irq ||
        ail_irq_vector_queries != 2u || ail_irq_vector_registrations != 1u ||
        ail_irq_critical_count != 1u || slot_offset != 0x3cu ||
        callback != 0xd68du ||
        EF_U32(0x0017d6a0u) != 2u)
        fatal_unexpected("AIL first timer wrapper call (0x1110ab)");
    if (!x32_virtual_ail_callback)
        stop_at_guest_boundary(0x001110abu);
    /* The already recovered 0x1021f4 -> 0x1114c7 -> 0x110f6f source-C
     * chain performs all timer metadata writes and schedules PIT requests. */
    trace_call_1021f4(slot_offset, callback);
}

uint32_t guest_fn_101e70(void)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_callback ||
        !(ail_irq_critical_count == 1u ||
          (x32_virtual_ail_slot && ail_irq_critical_count == 0u &&
           ail_irq_critical_exited == 1u)) ||
        EF_U32(0x0017d7bau) >= (x32_virtual_ail_rate ? 3u : 2u))
        fatal_unexpected("AIL nested timer critical section (0x101e70)");
    increment_17d7ba();
    return EF_U32(0x0017d7bau);
}

void guest_fn_101e78(void)
{
    if (!probing || !main_probe_active || !x32_probe_active ||
        !x32_virtual_ail_callback ||
        !(ail_irq_critical_count == 1u ||
          (x32_virtual_ail_slot && ail_irq_critical_count == 0u &&
           ail_irq_critical_exited == 1u)) ||
        EF_U32(0x0017d7bau) == 0u ||
        EF_U32(0x0017d7bau) > (x32_virtual_ail_rate ? 3u : 2u))
        fatal_unexpected("AIL nested timer critical exit (0x101e78)");
    decrement_17d7ba();
}

void EF_CDECL guest_fn_1114c7(uint32_t slot, uint32_t value)
{
    if (!probing || !x32_virtual_ail_callback ||
        !((EF_U32(0x0017d7bau) == 0u &&
           slot == 0x3cu && value == 0xd68du) ||
          (x32_virtual_ail_rate && ail_irq_critical_exited == 1u &&
           EF_U32(0x0017d7bau) == 1u &&
           slot == 0u && value == 20000u &&
           EF_U32(0x0017d664u) == 1u)) ||
        EF_U32(0x0017d2c8u) != 0u)
        fatal_unexpected("AIL first timer callback slot (0x1114c7)");
    set_table_slot_value_1114c7(slot, value);
}

void guest_fn_110f6f_x(void)
{
    if (!probing || !x32_virtual_ail_callback ||
        !((EF_U32(0x0017d7bau) == 1u &&
           EF_U32(0x0017d7b2u) == UINT32_MAX) ||
          (x32_virtual_ail_rate && ail_irq_critical_exited == 1u &&
           EF_U32(0x0017d7bau) == 2u &&
           EF_U32(0x0017d7b2u) == 0xd68du &&
           EF_U32(0x0017d6e4u) == 20000u)) ||
        EF_U32(0x0017d720u) != 0xd68du ||
        EF_U32(0x0017d6a0u) != 2u)
        fatal_unexpected("AIL timer reprogram (0x110f6f)");
    timer_reprogram_110f6f();
}

uint32_t guest_fn_110f0c(uint32_t divisor)
{
    if (!probing || !x32_virtual_ail_callback ||
        !((EF_U32(0x0017d7bau) == 2u &&
           EF_U32(0x0017d7b2u) == 0xd68du &&
           divisor == 0u && ail_pit_port_writes == 0u) ||
          (x32_virtual_ail_rate && ail_irq_critical_exited == 1u &&
           EF_U32(0x0017d7bau) == 3u &&
           EF_U32(0x0017d7b2u) == 20000u &&
           divisor == 0x5d3au && ail_pit_port_writes == 3u)))
        fatal_unexpected("AIL timer divisor (0x110f0c)");
    set_timer_divisor_110f0c(divisor);
    return 0u; /* caller discards the original EAX */
}

void guest_ail_irq_exit_1110b3(void)
{
    if (!probing || !x32_virtual_ail_callback ||
        ail_irq_critical_count != 1u || ail_irq_critical_exited != 0u ||
        EF_U32(0x0017d7bau) != 0u ||
        EF_U32(0x0017d7b2u) != 0xd68du ||
        EF_U32(0x0017d720u) != 0xd68du ||
        EF_U32(0x0017d7aeu) != 0u ||
        ail_pit_port_writes != 3u)
        fatal_unexpected("AIL original IRQ flag restoration (0x1110b3)");
    /* Original popf/CLI/(optional STI)/popf; preserve a virtual IRQ state. */
    ail_irq_critical_count = 0u;
    ail_irq_critical_exited = 1u;
}

/* Other references from recovered timer/audio C are deliberately fail-closed.
 * The 0x104840 memory-lock adapter below is the sole exception: its exact
 * inert cold AIL probe sequence is checked explicitly. No sound/IRQ service
 * is assumed to succeed merely because the recovered source links. */
void guest_fn_d6df0(void)
{
    if (probing && x32_probe_active && x32_virtual_ail_callback &&
        ail_irq_critical_exited == 1u && ail_pit_port_writes == 3u &&
        EF_U8(0x0015bac5u) == 1u && EF_U8(0x0015f751u) == 1u) {
        if (!x32_virtual_ail_module)
            stop_at_guest_boundary(0x000d6df0u);
        if (ail_module_lock_count != 0u)
            fatal_unexpected("AIL sound module locks entered twice (0xd6df0)");
        register_stack_ranges_d6df0();
        if (ail_module_lock_count != 2u)
            fatal_unexpected("AIL sound module lock order (0xd6df0)");
        return;
    }
    fatal_unexpected("unimplemented sound module lock setup (0xd6df0)");
}

void guest_fn_104840(uint32_t first, uint32_t last)
{
    /* Only the precisely recovered page-lock sequence of the no-debug
     * cold AIL path is accepted, with no host memory lock installed. */
    if (probing && main_probe_active && x32_probe_active &&
        x32_virtual_ail_init &&
        ail_lock_count < (unsigned)(x32_virtual_ail_irq ?
                                     AIL_IRQ_LOCK_COUNT : AIL_DEFAULT_LOCK_COUNT) &&
        first == ail_cold_lock_ranges[ail_lock_count].first &&
        last == ail_cold_lock_ranges[ail_lock_count].last &&
        first < last && ef_decomp_memory_span(first, last - first)) {
        ail_lock_count++;
        return;
    }
    if (probing && main_probe_active && x32_probe_active &&
        x32_virtual_ail_module && ail_irq_critical_exited == 1u &&
        ail_lock_count == AIL_IRQ_LOCK_COUNT &&
        ail_module_lock_count < sizeof(ail_module_lock_ranges) /
                                 sizeof(ail_module_lock_ranges[0]) &&
        first == ail_module_lock_ranges[ail_module_lock_count].first &&
        last == ail_module_lock_ranges[ail_module_lock_count].last &&
        first < last && ef_decomp_memory_span(first, last - first)) {
        ++ail_module_lock_count;
        return;
    }
    fatal_unexpected("unimplemented DOS memory lock (0x104840)");
}

uint32_t EF_CDECL guest_fn_111365_t(uint32_t callback)
{
    if (probing && main_probe_active && x32_probe_active &&
        x32_virtual_ail_module && ail_irq_critical_exited == 1u &&
        ail_module_lock_count == 4u && callback == 0x000e28c4u &&
        EF_U32(0x0040d0b0u) == 1u &&
        EF_U32(0x0040d030u) == 0x000d6da0u &&
        EF_U32(0x00160788u) == 0x0009c398u &&
        EF_U8(0x0015bac5u) == 1u &&
        EF_U8(0x0015f751u) == 1u &&
        EF_U32(0x0017d2c8u) == 0u) {
        if (x32_virtual_ail_slot) {
            if (EF_U32(0x0017d7bau) != 0u ||
                EF_U32(0x0017d664u) != 0u ||
                EF_U32(0x0017d6a0u) != 2u)
                fatal_unexpected("AIL cold timer slot preconditions (0x111365)");
            return claim_table_slot_111365(callback);
        }
        stop_at_guest_boundary(0x00111365u);
    }
    fatal_unexpected("unimplemented audio callback installation (0x111365)");
    return 0;
}

void EF_CDECL guest_fn_1114f7(uint32_t a, uint32_t b)
{
    if (probing && main_probe_active && x32_probe_active &&
        x32_virtual_ail_slot && ail_irq_critical_exited == 1u &&
        ail_module_lock_count == 4u && a == 0u && b == 0x32u &&
        EF_U32(0x0017d664u) == 1u &&
        EF_U32(0x0017d624u) == 0x000e28c4u &&
        EF_U32(0x0015f74cu) == 0u &&
        EF_U32(0x0017d7bau) == 0u &&
        ail_pit_port_writes == 3u) {
        if (x32_virtual_ail_rate) {
            set_rate_1114f7(a, b);
            if (ail_pit_port_writes != 6u ||
                EF_U32(0x0017d7bau) != 0u)
                fatal_unexpected("AIL rate update not balanced (0x1114f7)");
            return;
        }
        stop_at_guest_boundary(0x001114f7u);
    }
    (void)a;
    (void)b;
    fatal_unexpected("unimplemented audio service (0x1114f7)");
}

void EF_CDECL guest_fn_11141f(uint32_t a)
{
    if (probing && main_probe_active && x32_probe_active &&
        x32_virtual_ail_rate && ail_irq_critical_exited == 1u &&
        ail_module_lock_count == 4u && a == 0u &&
        EF_U32(0x0015f74cu) == 0u &&
        EF_U32(0x0015b620u) == 0x000e28e0u &&
        EF_U32(0x0017d664u) == 1u &&
        EF_U32(0x0017d6e4u) == 20000u &&
        EF_U32(0x0017d7b2u) == 20000u &&
        EF_U32(0x0017d7aeu) == 0x5d3au &&
        EF_U32(0x0017d7bau) == 0u &&
        ail_pit_port_writes == 6u) {
        if (x32_virtual_ail_start) {
            table_slot_1_to_2_11141f(a);
            if (EF_U32(0x0017d664u) != 2u ||
                EF_U32(0x0017d7bau) != 0u)
                fatal_unexpected("AIL timer slot did not start (0x11141f)");
            return;
        }
        stop_at_guest_boundary(0x0011141fu);
    }
    (void)a;
    fatal_unexpected("unimplemented audio service (0x11141f)");
}

void guest_fn_bc400(uint32_t stream, uint32_t format, ...)
{
    (void)stream;
    (void)format;
    fatal_unexpected("unimplemented audio stream service (0xbc400)");
}

void guest_tactcom_unbound_service_8b47c(uint32_t address)
{
    /* Do not bypass unmodeled device or timer effects. The recovered
     * 0xe28e8 body immediately returns when its one-time guard is set;
     * entering that no-op arm does not require linking its hardware deps. */
    if (!probing || !main_probe_active)
        fatal_unexpected("TACTCOM post-input service outside main probe");
    if (address == 0x000e28e8u && x32_probe_active &&
        x32_virtual_tactcom && x32_virtual_sound_timer &&
        EF_U8(0x0015f751u) == 0u) {
        start_timer_service_e28e8();
        if (x32_virtual_ail_start) {
            if (EF_U8(0x0015f751u) != 1u ||
                EF_U8(0x0015bac5u) != 1u ||
                EF_U32(0x0015b620u) != 0x000e28e0u ||
                EF_U32(0x0017d664u) != 2u ||
                EF_U32(0x0017d7bau) != 0u ||
                ail_pit_port_writes != 6u)
                fatal_unexpected("AIL first timer service did not return cleanly");
            if (!x32_virtual_ail_finish)
                stop_at_guest_boundary(0x0008b4c9u);
        }
        return;
    }
    if (address == 0x000e28e8u && EF_U8(0x0015f751u) != 0u) {
        return;
    }
    /* Original 0xe9ac0 is side-effect-free for modes 0 and >3. For the
     * other three modes stop before the actual hardware probe call. */
    if (address == 0x000e9ac0u) {
        uint16_t mode = EF_U16(0x00464472u);
        if (mode == 0u || mode > 3u)
            return;
        address = (mode == 1u) ? 0x000d9b30u :
                  (mode == 2u) ? 0x000d9c30u : 0x0010d9e0u;
    }
    if (address != 0x0007feb0u && address != 0x00081110u &&
         address != 0x000982a0u && address != 0x00097f90u &&
         address != 0x00098000u && address != 0x000e28e8u &&
         address != 0x000d9b30u && address != 0x000d9c30u &&
         address != 0x0010d9e0u)
        fatal_unexpected("unsupported TACTCOM post-input call");
    stop_at_guest_boundary(address);
}

uint32_t guest_fn_eb9f8(uint32_t file, uint32_t archive)
{
    (void)file;
    (void)archive;
    fatal_unexpected("archive lookup before archives open (0xeb9f8)");
    return 0;
}

uint32_t guest_fn_b3b90(uint32_t path)
{
    if (!ef_decomp_files_enabled())
        stop_at_guest_boundary(0xb3b90u);
    return read_file_payload_size_b3b90(path);
}

void guest_fn_8b39c(void)
{
    if (!probing || !main_probe_active ||
        !ef_decomp_memory_span(TACTCOM_PROBE_FRAME, 0xf0u))
        fatal_unexpected("TACTCOM prefix outside startup probe");
    tactcom_prefix_8b39c(TACTCOM_PROBE_FRAME);
    if (main_probe_depth >= 1) {
        if (EF_U8(0x0015f751u) != 1u ||
            EF_U16(0x00464472u) != 0u ||
            EF_U32(0x0019840cu) != 0u ||
            EF_U32(0x00198410u) != 0u)
            fatal_unexpected("TACTCOM did not return in no-device warm mode");
        return;
    }
    /* The recovered 402-byte TACTCOM body has returned on a synthetic
     * already-initialized path. Continue into main only after its separate
     * initialization and file/graphics services have been recovered. */
    stop_at_guest_boundary(0x0008b52du);
}
void guest_fn_8b2d8(void) { stop_at_guest_boundary(0x8b2d8u); }

/* These later entry points are deliberately *not* implemented as native
 * services. Reaching them before TACTCOM has been recovered is an error. */
void guest_fn_ea470(uint32_t mode)
{
    if (!probing || !main_probe_active || main_probe_depth < 1 ||
        mode != 1u || EF_U8(0x0015f751u) != 1u)
        fatal_unexpected("exit handler before TACTCOM (0xea470)");
    install_exit_handler_ea470(mode);
    fatal_unexpected("exit handler returned before service boundary (0xea470)");
}

void guest_fn_10dc7f(uint32_t channel, uint32_t mode)
{
    if (!probing || !main_probe_active || main_probe_depth < 1 ||
        channel != 2u || mode != 0x0010dc40u)
        fatal_unexpected("unbound channel-mode service (0x10dc7f)");
    if (main_probe_depth == 2) {
        (void)set_channel_mode_10dc7f((int32_t)channel, (int32_t)mode);
        fatal_unexpected("channel-mode service returned before 0x11d24e");
    }
    stop_at_guest_boundary(0x0010dc7fu);
}

void guest_fn_11d24e_x(void)
{
    if (!probing || !main_probe_active || main_probe_depth != 2)
        fatal_unexpected("runtime-hook tail outside channel-mode probe (0x11d24e)");
    stop_at_guest_boundary(0x0011d24eu);
}

void guest_fn_ebe6d(uint32_t code)
{
    (void)code;
    fatal_unexpected("unbound invalid-channel report (0xebe6d)");
}

void guest_fn_11ce30_x(void) { fatal_unexpected("unbound channel 4 enter (0x11ce30)"); }
void guest_fn_11d01c_x(void) { fatal_unexpected("unbound channel 4 leave (0x11d01c)"); }
void guest_fn_11cfe4_x(void) { fatal_unexpected("unbound channel 7 enter (0x11cfe4)"); }
void guest_fn_11d135_x(void) { fatal_unexpected("unbound channel 7 leave (0x11d135)"); }

void guest_fn_81c(void)
{
    fatal_unexpected("unbound exit service (0x81c)");
}

void guest_fn_ea270(ef_addr32 descriptor)
{
    (void)descriptor;
    fatal_unexpected("resource loader before TACTCOM (0xea270)");
}

uint32_t guest_fn_aaee8(uint32_t argc, ef_addr32 argv)
{
    (void)argc;
    (void)argv;
    fatal_unexpected("main control before TACTCOM (0xaaee8)");
    return 0;
}

static int prepare_arena(uint32_t bss_end, char *reason, size_t reason_capacity)
{
    uint32_t arena_size, base;
    if (bss_end < 0x0015f76cu ||
        !ef_decomp_memory_span(0x001264d4u, sizeof(uint32_t))) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "guest RAM is not initialized");
        return -1;
    }
    arena_size = EF_U32(0x001264d4u);
    base = (bss_end + 0xfffu) & ~0xfffu;
    if (EF_U32(0x0015f75cu) != 0 || arena_size == 0 ||
        arena_size > EF_DECOMP_RAM_SIZE - base) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "arena startup preconditions not met");
        return -1;
    }
    next_allocation = base;
    requested_arena = arena_size;
    hook_registrations = 0;
    release_hook = 0;
    arena_allocated = 0;
    stream_allocated = 0;
    keyboard_led_bytes = 0;
    keyboard_ack_pending = 0;
    probing = 1;
    return 0;
}

static int check_arena(ef_decomp_arena_probe *probe,
                       char *reason, size_t reason_capacity)
{
    uint32_t base = next_allocation;
    uint32_t arena_size = requested_arena;
    uint32_t live_bytes = EF_U32(0x0015f764u);
    /* Startup may allocate the first config text after init_arena. Confirm
     * the recovered allocator's accounting instead of requiring an unused
     * arena at every later first-screen boundary. */
    if (!probe || !arena_allocated || hook_registrations != 1 ||
        release_hook != 0xe2a50u ||
        EF_U32(0x0015f75cu) != base || EF_U32(0x0015f760u) != arena_size ||
        live_bytes > arena_size ||
        EF_U32(0x0015f754u) != base + live_bytes ||
        EF_U32(0x0015f758u) != arena_size - live_bytes ||
        EF_U32(0x0015f768u) != live_bytes) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "recovered arena initialization mismatch");
        return -1;
    }
    probe->base = base;
    probe->size = arena_size;
    probe->release_hook = release_hook;
    probe->hook_registrations = hook_registrations;
    return 0;
}

int ef_decomp_probe_arena(uint32_t bss_end, ef_decomp_arena_probe *probe,
                          char *reason, size_t reason_capacity)
{
    int result;
    if (!probe || prepare_arena(bss_end, reason, reason_capacity) != 0)
        return -1;
    init_arena_e29a0();
    probing = 0;
    result = check_arena(probe, reason, reason_capacity);
    return result;
}

static int run_x32_probe(uint32_t bss_end, ef_decomp_x32_probe *probe,
                         int virtual_vectors, int virtual_traps, int virtual_input,
                         int virtual_timer, int virtual_tactcom,
                         int virtual_sound_timer, int virtual_ail_init,
                         int virtual_ail_irq, int virtual_ail_vectors,
                         int virtual_ail_callback, int virtual_ail_module,
                         int virtual_ail_slot, int virtual_ail_rate,
                         int virtual_ail_start, int virtual_ail_finish,
                         char *reason, size_t reason_capacity)
{
    uint32_t stopped;
    unsigned i;
    if (!probe || bss_end <= 0x00400106u ||
        !ef_decomp_memory_span(0x00400100u, 6u)) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "guest RAM is not initialized for X-32");
        return -1;
    }
    memset(probe, 0, sizeof(*probe));
    release_hook = 0u;
    hook_registrations = 0u;
    input_cleanup_hook = 0u;
    x32_timer_registrations = 0u;
    ail_lock_count = 0u;
    ail_setting_count = 0u;
    ail_irq_critical_count = 0u;
    ail_irq_vector_queries = 0u;
    ail_irq_vector_registrations = 0u;
    ail_pit_port_writes = 0u;
    ail_irq_critical_exited = 0u;
    ail_module_lock_count = 0u;
    x32_input_returns = 0u;
    x32_request_count = 0u;
    memset(x32_requests, 0, sizeof(x32_requests));
    x32_trap_count = 0u;
    memset(x32_traps, 0, sizeof(x32_traps));
    probing = 1;
    main_probe_active = 1;
    x32_probe_active = 1;
    x32_virtual_vectors = virtual_vectors;
    x32_virtual_traps = virtual_traps;
    x32_virtual_input = virtual_input;
    x32_virtual_timer = virtual_timer;
    x32_virtual_tactcom = virtual_tactcom;
    x32_virtual_sound_timer = virtual_sound_timer;
    x32_virtual_ail_init = virtual_ail_init;
    x32_virtual_ail_irq = virtual_ail_irq;
    x32_virtual_ail_vectors = virtual_ail_vectors;
    x32_virtual_ail_callback = virtual_ail_callback;
    x32_virtual_ail_module = virtual_ail_module;
    x32_virtual_ail_slot = virtual_ail_slot;
    x32_virtual_ail_rate = virtual_ail_rate;
    x32_virtual_ail_start = virtual_ail_start;
    x32_virtual_ail_finish = virtual_ail_finish;
    keyboard_led_bytes = 0u;
    keyboard_ack_pending = 0;
    main_boundary_address = 0u;
    if (setjmp(main_boundary) == 0) {
        initialize_x32_services_e20();
        if (virtual_input)
            initialize_input_services_cd4d4();
        if (virtual_tactcom) {
            if (x32_input_returns != 1u)
                fatal_unexpected("TACTCOM resumed before cold input returned");
            guest_tactcom_post_options_boundary_8b477();
            if (virtual_ail_finish)
                stop_at_guest_boundary(0x0008b52du);
        }
    }
    stopped = main_boundary_address;
    x32_probe_active = 0;
    x32_virtual_vectors = 0;
    x32_virtual_traps = 0;
    x32_virtual_input = 0;
    x32_virtual_timer = 0;
    x32_virtual_tactcom = 0;
    x32_virtual_sound_timer = 0;
    x32_virtual_ail_init = 0;
    x32_virtual_ail_irq = 0;
    x32_virtual_ail_vectors = 0;
    x32_virtual_ail_callback = 0;
    x32_virtual_ail_module = 0;
    x32_virtual_ail_slot = 0;
    x32_virtual_ail_rate = 0;
    x32_virtual_ail_start = 0;
    x32_virtual_ail_finish = 0;
    main_probe_active = 0;
    probing = 0;

    /* Preserve the first real 0x628's side effects. An INT 21h request
     * registration must not be silently assumed successful. */
    for (i = 0; i < 16u; ++i) {
        if (EF_U8(0x00195fb0u + i) != i)
            break;
    }
    if (stopped != (virtual_ail_finish ? 0x0008b52du :
                    virtual_ail_start ? 0x0008b4c9u :
                    virtual_ail_rate ? 0x0011141fu :
                    virtual_ail_slot ? 0x001114f7u :
                    virtual_ail_module ? 0x00111365u :
                    virtual_ail_callback ? 0x000d6df0u :
                    virtual_ail_vectors ? 0x001110abu :
                    virtual_ail_irq ? 0x0011105bu :
                    virtual_ail_init ? 0x00110fd4u :
                    virtual_sound_timer ? 0x00101c54u :
                    virtual_tactcom ? 0x000e28e8u :
                    virtual_timer ? 0x000cd550u :
                    virtual_input ? 0x00000324u :
                    virtual_traps ? 0x00000f43u :
                    virtual_vectors ? 0x0009c8b4u : 0x0009c220u) ||
        x32_request_count != (virtual_input ? 11u : virtual_vectors ? 9u : 1u) ||
        i != 16u ||
        x32_trap_count != (virtual_traps ? 8u : 0u) ||
        EF_U8(0x00132a98u) != 1u ||
        EF_U32(0x00120d24u) != 0x80u ||
        EF_U32(0x0019613cu) != 0x00400100u ||
        EF_U8(0x00400100u) != 0x9au ||
        EF_U8(0x00400105u) != 0xcfu ||
        EF_U32(0x00196144u) != (virtual_input ? 5u : virtual_vectors ? 7u : 15u) ||
        EF_U32(0x00196140u) != (virtual_input ? 7u : 8u) ||
        keyboard_led_bytes != (virtual_input ? 2u : 0u) || keyboard_ack_pending ||
        hook_registrations != (virtual_timer ? 2u : 1u) ||
        release_hook != 0xad8u ||
        x32_timer_registrations != (virtual_timer ? 1u : 0u) ||
        input_cleanup_hook != (virtual_timer ? 0x000cd554u : 0u) ||
        x32_input_returns != (virtual_timer ? 1u : 0u)) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "recovered X-32 pre-main state mismatch");
        return -1;
    }
    for (i = 0; i < 8u; ++i) {
        if (EF_U8(0x00196134u + i) != i) {
            if (reason && reason_capacity)
                snprintf(reason, reason_capacity, "recovered X-32 timer free list mismatch");
            return -1;
        }
    }
    probe->stop_address = stopped;
    probe->request_free = EF_U32(0x00196144u);
    probe->timer_free = EF_U32(0x00196140u);
    probe->first_address = EF_U32(0x00196022u + 15u * 10u);
    probe->first_selector = EF_U16(0x00196026u + 15u * 10u);
    probe->first_length = EF_U32(0x00196028u + 15u * 10u);
    probe->far_stub_address = EF_U32(0x0019613cu);
    probe->cleanup_hook = release_hook;
    probe->registered_requests = x32_request_count;
    probe->last_address = x32_requests[x32_request_count - 1u].address;
    probe->last_selector = x32_requests[x32_request_count - 1u].selector;
    probe->last_length = x32_requests[x32_request_count - 1u].length;
    probe->keyboard_led_bytes = keyboard_led_bytes;
    probe->registered_traps = x32_trap_count;
    if (virtual_traps) {
        probe->saved_trap_callback = EF_U32(0x00132a9cu);
        probe->saved_trap_esp = EF_U32(0x00132aa0u);
        probe->saved_trap_ss = EF_U16(0x00132aa4u);
        probe->saved_trap_ds = EF_U16(0x00132aa6u);
        probe->first_trap_vector = x32_traps[0].vector;
        probe->first_trap_handler = x32_traps[0].handler;
        probe->last_trap_vector = x32_traps[7].vector;
        probe->last_trap_handler = x32_traps[7].handler;
    }
    if (virtual_input) {
        probe->keyboard_code_slot = EF_U32(0x0041f828u);
        probe->keyboard_data_slot = EF_U32(0x0041f82cu);
        probe->pending_timer_source = read_x32_packed_dword(0x001960c4u + 7u * 14u);
        probe->stored_data_selector = EF_U16(0x000fe624u);
    }
    if (virtual_timer) {
        probe->timer_2503_result = read_x32_packed_dword(0x001960c8u + 7u * 14u);
        probe->timer_2502_result_low = read_x32_packed_dword(0x001960ccu + 7u * 14u);
        probe->timer_2502_result_high = EF_U16(0x001960d0u + 7u * 14u);
        probe->timer_registrations = x32_timer_registrations;
        probe->input_cleanup_hook = input_cleanup_hook;
        probe->input_initialized = EF_U8(0x0015b72eu);
        if (probe->timer_2503_result != 0x2468u ||
            probe->timer_2502_result_low != 0x12345678u ||
            probe->timer_2502_result_high != 0xabcdu ||
            probe->input_initialized != 1u || EF_U32(0x0041f824u) != 7u) {
            if (reason && reason_capacity)
                snprintf(reason, reason_capacity, "recovered cold timer/input state mismatch");
            return -1;
        }
    }
    if (virtual_tactcom) {
        /* Zero-flag/no-device startup goes through the recovered 0x8b47c
         * dispatch without taking any page/device handler branches.
         * 0xe28e8 is the first required timer/sound subsystem call. */
        if (EF_U8(0x0015bac6u) != 0u ||
            EF_U8(0x0015bac7u) != 0u ||
            EF_U8(0x00420a72u) != 0u ||
            EF_U8(0x0015f751u) != (virtual_sound_timer ? 1u : 0u) ||
            EF_U8(0x0015bac5u) != (virtual_ail_callback ? 1u : 0u) ||
            EF_U32(0x0041f824u) != 7u) {
            if (reason && reason_capacity)
                snprintf(reason, reason_capacity, "recovered cold TACTCOM tail state mismatch");
            return -1;
        }
        probe->timer_service_initialized = EF_U8(0x0015f751u);
        probe->sound_module_initialized = EF_U8(0x0015bac5u);
        if (virtual_ail_finish &&
            (EF_U16(0x00464472u) != 0u ||
             EF_U32(0x0019840cu) != 0u ||
             EF_U32(0x00198410u) != 0u ||
             EF_U8(0x0015ba2cu) != 0u ||
             EF_U8(0x0015ba2fu) != 0u)) {
            if (reason && reason_capacity)
                snprintf(reason, reason_capacity,
                         "recovered cold TACTCOM mode-zero return mismatch");
            return -1;
        }
        if (virtual_ail_init) {
            if (ail_lock_count != (unsigned)(virtual_ail_irq ?
                                            AIL_IRQ_LOCK_COUNT : AIL_DEFAULT_LOCK_COUNT) ||
                ail_setting_count != 19u ||
                EF_U32(0x0017d2ccu) != 1u || EF_U32(0x0017d2d4u) != 1u ||
                EF_U32(0x004dd624u) != 0u || EF_U32(0x004dd628u) != 0u ||
                EF_U32(0x004dd620u) != 0u) {
                if (reason && reason_capacity)
                    snprintf(reason, reason_capacity, "recovered cold AIL debug state mismatch");
                return -1;
            }
            probe->ail_locked_ranges = ail_lock_count;
            probe->ail_settings_written = ail_setting_count;
            probe->ail_trace_ranges_initialized = EF_U32(0x0017d2ccu);
            probe->ail_settings_ranges_initialized = EF_U32(0x0017d2d4u);
            if (virtual_ail_irq) {
                if (ail_irq_critical_count != (virtual_ail_callback ? 0u : 1u) ||
                    EF_U16(0x0017d7beu) != 0x5678u ||
                    EF_U32(0x0017d7b2u) != (virtual_ail_rate ? 20000u :
                                             virtual_ail_callback ? 0xd68du : UINT32_MAX) ||
                    EF_U32(0x0017d7c0u) != UINT32_MAX) {
                    if (reason && reason_capacity)
                        snprintf(reason, reason_capacity, "recovered cold AIL IRQ table state mismatch");
                    return -1;
                }
                probe->ail_irq_critical_entered = 1u;
                probe->ail_saved_ds_selector = EF_U16(0x0017d7beu);
                if (virtual_ail_vectors) {
                    if (ail_irq_vector_queries != 2u ||
                        ail_irq_vector_registrations != 1u ||
                        EF_U32(0x0017d7a4u) != 0x12345678u ||
                        EF_U16(0x0017d7a8u) != 0xabcdu ||
                        EF_U32(0x0017d7aau) != 0x2468u ||
                        EF_U32(0x0017d6a0u) != 2u) {
                        if (reason && reason_capacity)
                            snprintf(reason, reason_capacity,
                                     "recovered cold AIL saved IRQ vectors mismatch");
                        return -1;
                    }
                    probe->ail_irq_vector_queries = ail_irq_vector_queries;
                    probe->ail_irq_vector_registrations = ail_irq_vector_registrations;
                    probe->ail_irq_original_pm_offset = EF_U32(0x0017d7a4u);
                    probe->ail_irq_original_pm_selector = EF_U16(0x0017d7a8u);
                    probe->ail_irq_original_rm_offset = EF_U32(0x0017d7aau);
                    if (virtual_ail_callback) {
                        if (ail_irq_critical_exited != 1u ||
                            ail_pit_port_writes != (virtual_ail_rate ? 6u : 3u) ||
                            EF_U32(0x0017d720u) != 0xd68du ||
                            EF_U32(0x0017d6e0u) != 0u ||
                            EF_U32(0x0017d7aeu) != (virtual_ail_rate ? 0x5d3au : 0u) ||
                            EF_U32(0x0017d7bau) != 0u) {
                            if (reason && reason_capacity)
                                snprintf(reason, reason_capacity,
                                         "recovered cold AIL timer callback mismatch");
                            return -1;
                        }
                        probe->ail_timer_slot_value = EF_U32(0x0017d720u);
                        probe->ail_timer_reload = EF_U32(0x0017d7aeu);
                        probe->ail_pit_port_writes = ail_pit_port_writes;
                        probe->ail_irq_critical_exited = ail_irq_critical_exited;
                        if (virtual_ail_module) {
                            if (ail_module_lock_count != 4u ||
                                EF_U32(0x0040d0b0u) != 1u ||
                                EF_U32(0x0040d030u) != 0x000d6da0u ||
                                EF_U32(0x00160788u) != 0x0009c398u) {
                                if (reason && reason_capacity)
                                    snprintf(reason, reason_capacity,
                                             "recovered AIL sound module lock state mismatch");
                                return -1;
                            }
                            probe->ail_module_locked_ranges = ail_module_lock_count;
                            probe->ail_module_cleanup_handler = EF_U32(0x0040d030u);
                            probe->ail_module_cleanup_count = EF_U32(0x0040d0b0u);
                            if (virtual_ail_slot) {
                                if (EF_U32(0x0017d664u) !=
                                    (virtual_ail_start ? 2u : 1u) ||
                                    EF_U32(0x0017d624u) != 0x000e28c4u ||
                                    EF_U32(0x0015f74cu) != 0u ||
                                    EF_U32(0x0017d7bau) != 0u ||
                                    EF_U32(0x0017d6a0u) != 2u ||
                                    (virtual_ail_rate &&
                                     (EF_U32(0x0017d6e4u) != 20000u ||
                                      EF_U32(0x0017d7b2u) != 20000u ||
                                      EF_U32(0x0017d7aeu) != 0x5d3au ||
                                      EF_U32(0x0015b620u) != 0x000e28e0u ||
                                      ail_pit_port_writes != 6u))) {
                                    if (reason && reason_capacity)
                                        snprintf(reason, reason_capacity,
                                                 "recovered AIL timer slot state mismatch");
                                    return -1;
                                }
                                probe->ail_module_timer_slot = EF_U32(0x0015f74cu);
                                probe->ail_module_timer_handler = EF_U32(0x0017d624u);
                                probe->ail_module_timer_slot_state = EF_U32(0x0017d664u);
                                if (virtual_ail_rate) {
                                    probe->ail_module_timer_period = EF_U32(0x0017d6e4u);
                                    probe->ail_module_timer_reload = EF_U32(0x0017d7aeu);
                                    probe->ail_module_timer_callback_ptr = EF_U32(0x0015b620u);
                                    probe->ail_module_timer_port_writes = ail_pit_port_writes;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

int ef_decomp_probe_x32(uint32_t bss_end, ef_decomp_x32_probe *probe,
                        char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, reason, reason_capacity);
}

int ef_decomp_probe_x32_vectors(uint32_t bss_end, ef_decomp_x32_probe *probe,
                                char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, reason, reason_capacity);
}

int ef_decomp_probe_x32_traps(uint32_t bss_end, ef_decomp_x32_probe *probe,
                              char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, reason, reason_capacity);
}

int ef_decomp_probe_x32_input(uint32_t bss_end, ef_decomp_x32_probe *probe,
                              char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, reason, reason_capacity);
}

int ef_decomp_probe_x32_timer(uint32_t bss_end, ef_decomp_x32_probe *probe,
                              char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom(uint32_t bss_end, ef_decomp_x32_probe *probe,
                                char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom_timer(uint32_t bss_end, ef_decomp_x32_probe *probe,
                                      char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom_ail(uint32_t bss_end, ef_decomp_x32_probe *probe,
                                     char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom_ail_irq(uint32_t bss_end, ef_decomp_x32_probe *probe,
                                         char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom_ail_vectors(uint32_t bss_end,
                                             ef_decomp_x32_probe *probe,
                                             char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
                         reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom_ail_callback(uint32_t bss_end,
                                             ef_decomp_x32_probe *probe,
                                             char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
                         reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom_ail_module(uint32_t bss_end,
                                           ef_decomp_x32_probe *probe,
                                           char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
                         reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom_ail_slot(uint32_t bss_end,
                                         ef_decomp_x32_probe *probe,
                                         char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
                         reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom_ail_rate(uint32_t bss_end,
                                         ef_decomp_x32_probe *probe,
                                         char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
                         reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom_ail_start(uint32_t bss_end,
                                          ef_decomp_x32_probe *probe,
                                          char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
                         reason, reason_capacity);
}

int ef_decomp_probe_x32_tactcom_ail_finish(uint32_t bss_end,
                                           ef_decomp_x32_probe *probe,
                                           char *reason, size_t reason_capacity)
{
    return run_x32_probe(bss_end, probe, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                         reason, reason_capacity);
}

static int run_main_probe(uint32_t bss_end, uint32_t argc, uint32_t argv,
                          const char *data_dir,
                          ef_decomp_arena_probe *probe, uint32_t *stop_address,
                          char *reason, size_t reason_capacity,
                          int continuation_depth)
{
    uint32_t stopped;
    int result;
    if (!probe || !stop_address || prepare_arena(bss_end, reason, reason_capacity) != 0)
        return -1;
    if (ef_decomp_files_begin(data_dir) != 0) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "cannot open read-only data directory");
        probing = 0;
        return -1;
    }
    main_probe_active = 1;
    main_probe_depth = continuation_depth;
    main_boundary_address = 0;
    if (setjmp(main_boundary) == 0)
        (void)candidate_game_main(argc, argv);
    stopped = main_boundary_address;
    main_probe_depth = 0;
    main_probe_active = 0;
    probing = 0;
    ef_decomp_files_end();
    if ((continuation_depth == 1 && stopped != 0x0010dc7fu) ||
        (continuation_depth == 2 && stopped != 0x0011d24eu)) {
        if (reason && reason_capacity) {
            if (continuation_depth == 2)
                snprintf(reason, reason_capacity,
                         "warm game main stopped before the runtime-hook tail (0x11d24e)");
            else
                snprintf(reason, reason_capacity,
                         "warm game main stopped before the exit-handler service (0x10dc7f)");
        }
        return -1;
    }
    if (stopped != 0xb3b90u && stopped != 0xeb25au && stopped != 0xe882cu &&
        stopped != 0x9ccb8u && stopped != 0x9cd53u &&
        stopped != 0x30407u && stopped != 0x3053du && stopped != 0x30577u &&
        stopped != 0x310d8u && stopped != 0x31116u &&
        stopped != 0x300e6u && stopped != 0x3010eu &&
        stopped != 0x3001du && stopped != 0x3003cu && stopped != 0x3005bu &&
        stopped != 0x9d55du && stopped != 0x3120bu &&
        stopped != 0xebb58u && stopped != 0xf80u &&
        stopped != 0x1171u && stopped != 0x11cbu &&
        stopped != 0x120eu && stopped != 0x1217u &&
        stopped != 0x1238u && stopped != 0x1244u && stopped != 0x1254u &&
        stopped != 0x1365u && stopped != 0x14a7u &&
        stopped != 0x14c9u && stopped != 0x1544u &&
        stopped != 0x157cu && stopped != 0x15a5u && stopped != 0x15cbu &&
        stopped != 0x1252u && stopped != 0x16dcu && stopped != 0x1687u &&
        stopped != 0x1633u && stopped != 0x16c2u && stopped != 0x16ceu &&
        stopped != 0x16deu && stopped != 0x17efu && stopped != 0x1931u &&
        stopped != 0x1953u && stopped != 0x19d4u && stopped != 0x1a0cu &&
        stopped != 0x1a32u && stopped != 0x1a58u &&
        stopped != 0xb3c04u &&
        stopped != 0xeba64u && stopped != 0xea50cu && stopped != 0x628u &&
        stopped != 0x7feb0u && stopped != 0x81110u &&
        stopped != 0x982a0u && stopped != 0x97f90u &&
        stopped != 0x98000u && stopped != 0xe28e8u &&
        stopped != 0xd9b30u && stopped != 0xd9c30u &&
        stopped != 0x10d9e0u && stopped != 0x8b52du &&
        stopped != 0x10dc7fu && stopped != 0x11d24eu &&
        stopped != 0x8b417u && stopped != 0x8b2d8u) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "game main did not reach an expected boundary");
        return -1;
    }
    *stop_address = stopped;
    if (stopped == 0x8b2d8u) {
        /* The original playvideo branch skips arena initialization. */
        if (arena_allocated || hook_registrations)
            return -1;
        memset(probe, 0, sizeof(*probe));
        return 0;
    }
    result = check_arena(probe, reason, reason_capacity);
    if (result != 0)
        return -1;
    return 0;
}

int ef_decomp_probe_main(uint32_t bss_end, uint32_t argc, uint32_t argv,
                         const char *data_dir,
                         ef_decomp_arena_probe *probe, uint32_t *stop_address,
                         char *reason, size_t reason_capacity)
{
    return run_main_probe(bss_end, argc, argv, data_dir, probe, stop_address,
                          reason, reason_capacity, 0);
}

int ef_decomp_probe_main_exit_handler(uint32_t bss_end, uint32_t argc,
                                      uint32_t argv, const char *data_dir,
                                      ef_decomp_arena_probe *probe,
                                      uint32_t *stop_address,
                                      char *reason, size_t reason_capacity)
{
    return run_main_probe(bss_end, argc, argv, data_dir, probe, stop_address,
                          reason, reason_capacity, 1);
}

int ef_decomp_probe_main_channel(uint32_t bss_end, uint32_t argc,
                                 uint32_t argv, const char *data_dir,
                                 ef_decomp_arena_probe *probe,
                                 uint32_t *stop_address,
                                 char *reason, size_t reason_capacity)
{
    return run_main_probe(bss_end, argc, argv, data_dir, probe, stop_address,
                          reason, reason_capacity, 2);
}

/* Recovered modules that the standalone host build links but whose callees
 * are original guest code (or modules deliberately kept out of the host
 * build) are not available here; reaching one is a probe failure. */
ef_addr32 break_down_time_10ec62(uint32_t days, uint32_t secs, int32_t offset, ef_addr32 tm)
{
    (void)days; (void)secs; (void)offset; (void)tm;
    fatal_unexpected("break_down_time (0x10ec62)");
    return 0u;
}

void guest_fn_10e97f_x(ef_addr32 tm)
{
    (void)tm;
    fatal_unexpected("guest time service (0x10e97f)");
}

void guest_fn_10eda1_x(void)
{
    fatal_unexpected("guest time service (0x10eda1)");
}

uint32_t guest_fn_1100d2(uint32_t value, ef_addr32 buffer)
{
    (void)value; (void)buffer;
    fatal_unexpected("guest number formatter (0x1100d2)");
    return 0u;
}

uint32_t guest_tail_110dcc(uint32_t a)
{
    (void)a;
    fatal_unexpected("guest AIL tail (0x110dcc)");
    return 0u;
}

uint32_t guest_tail_110dd9(uint32_t a)
{
    (void)a;
    fatal_unexpected("guest AIL tail (0x110dd9)");
    return 0u;
}

uint32_t run_if_available_bc36a(ef_addr32 name, ef_addr32 mode)
{
    (void)name; (void)mode;
    fatal_unexpected("run_if_available (0xbc36a)");
    return 0u;
}

int32_t set_default_file_buffer_11159a(ef_addr32 file, ef_addr32 buffer)
{
    (void)file; (void)buffer;
    fatal_unexpected("set_default_file_buffer (0x11159a)");
    return 0;
}

int32_t stream_printf_bc400(ef_addr32 stream, ef_addr32 fmt, ...)
{
    (void)stream; (void)fmt;
    fatal_unexpected("stream_printf (0xbc400)");
    return 0;
}
