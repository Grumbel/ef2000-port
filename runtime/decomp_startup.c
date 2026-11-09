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
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t next_allocation;
static uint32_t requested_arena;
static uint32_t release_hook;
static uint32_t hook_registrations;
static int probing;
static int arena_allocated;
static int stream_allocated;
static jmp_buf main_boundary;
static uint32_t main_boundary_address;
static int main_probe_active;

/* The native probe uses private guest stack-shaped scratch frames, not an
 * emulated Watcom stack.  Keep it below native64's documented 0x3fff000
 * initial guest ESP, outside the loaded image, BSS and startup arena. */
enum { TACTCOM_PROBE_FRAME = EF_DECOMP_RAM_SIZE - 0x2000u,
       TACTCOM_LOG_FRAME = TACTCOM_PROBE_FRAME + 0x200u,
       TACTCOM_TEXT_FRAME = TACTCOM_PROBE_FRAME + 0x400u,
       TACTCOM_VIEW_FRAME = TACTCOM_PROBE_FRAME + 0x600u };

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
    if (!probing || handler != 0xe2a50u || hook_registrations)
        fatal_unexpected("cleanup registration (0x9c3cf)");
    release_hook = handler;
    hook_registrations++;
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
         text != TACTCOM_VIEW_FRAME))
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

uint32_t guest_fn_e94ec(ef_addr32 path, uint32_t flags)
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
    const ef_addr32 header = TACTCOM_VIEW_FRAME + 0x98u;
    if (!probing || !main_probe_active || path != TACTCOM_VIEW_FRAME ||
        !(name = (const char *)ef_decomp_memory_span(path, 0x94u)) ||
        !(suffix = (const char *)ef_decomp_memory_span(0x00182794u, 0x30u)) ||
        !memchr(name, 0, 0x94u) || !memchr(suffix, 0, 0x30u))
        fatal_unexpected("Views.c resource-open path (0x9ccb8)");
    length = strlen(name);
    suffix_length = strlen(suffix);
    if (length < suffix_length ||
        memcmp(name + length - suffix_length, suffix, suffix_length) != 0)
        fatal_unexpected("Views.c resource-open suffix (0x9ccb8)");
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
        EF_U32(0x00132a74u) != fn || format != 0x001827a8u ||
        filename != TACTCOM_VIEW_FRAME)
        fatal_unexpected("Views.c missing-file diagnostic callback");
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

void guest_tactcom_view_numeric_boundary_305e1(ef_addr32 stream)
{
    check_view_parser_context(stream);
    stop_at_guest_boundary(0x000305e1u);
}

void guest_tactcom_view_nonrecord_boundary_31201(ef_addr32 stream)
{
    check_view_parser_context(stream);
    stop_at_guest_boundary(0x00031201u);
}

void guest_tactcom_view_invalid_index_boundary_3120b(ef_addr32 stream)
{
    check_view_parser_context(stream);
    stop_at_guest_boundary(0x0003120bu);
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
    fatal_unexpected("ReadGenSeqs returned from unbound resource parser");
}
void guest_fn_2ff44_w(void) { stop_at_guest_boundary(0x0002ff44u); }
void guest_fn_f80_w(void) { stop_at_guest_boundary(0x00000f80u); }

void guest_tactcom_post_options_boundary_8b477(void)
{
    stop_at_guest_boundary(0x000cd4d4u);
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
    fatal_unexpected("unbounded return from TACTCOM prefix");
}
void guest_fn_8b2d8(void) { stop_at_guest_boundary(0x8b2d8u); }

/* These later entry points are deliberately *not* implemented as native
 * services. Reaching them before TACTCOM has been recovered is an error. */
void guest_fn_ea470(uint32_t mode)
{
    (void)mode;
    fatal_unexpected("exit handler before TACTCOM (0xea470)");
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

int ef_decomp_probe_main(uint32_t bss_end, uint32_t argc, uint32_t argv,
                         const char *data_dir,
                         ef_decomp_arena_probe *probe, uint32_t *stop_address,
                         char *reason, size_t reason_capacity)
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
    main_boundary_address = 0;
    if (setjmp(main_boundary) == 0)
        (void)candidate_game_main(argc, argv);
    stopped = main_boundary_address;
    main_probe_active = 0;
    probing = 0;
    ef_decomp_files_end();
    if (stopped != 0xb3b90u && stopped != 0xeb25au && stopped != 0xe882cu &&
        stopped != 0x9ccb8u && stopped != 0x9cd53u &&
        stopped != 0x30407u && stopped != 0x3053du && stopped != 0x30577u &&
        stopped != 0x305e1u && stopped != 0x31201u &&
        stopped != 0x3120bu && stopped != 0xebb58u &&
        stopped != 0xb3c04u &&
        stopped != 0xeba64u && stopped != 0xea50cu && stopped != 0xcd4d4u &&
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
