/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Freestanding startup harness: native x86-64 compatibility mode or qemu-i386.
 * No libc, no game approximations. Unknown guest interfaces stop explicitly.
 * Linux ABI layouts are from asm/sigcontext.h, asm/ldt.h, and ucontext.h.
 */
#ifdef EF_NATIVE64
#include <ucontext.h>
#define GUEST_BASE 0x10000000u
#define NR_EXIT 60
#define NR_WRITE 1
#define NR_LDT 154
#define NR_ALTSTACK 131
#define NR_ACTION 13
#define NR_SLEEP 230 /* clock_nanosleep */
#define NR_CLOCK 228 /* clock_gettime */
#define NR_MLOCK 149
#define NR_MUNLOCK 150
#define NR_OPENAT 257
#define NR_READ 0
#define NR_CLOSE 3
#define NR_SEEK 8
#define NR_DUP 32
#define NR_FSYNC 74
#define NR_TIMER 38
#else
#define GUEST_BASE 0x10000000u
#define NR_EXIT 1
#define NR_WRITE 4
#define NR_LDT 123
#define NR_ALTSTACK 186
#define NR_ACTION 174
#define NR_SLEEP 407 /* clock_nanosleep_time64 */
#define NR_CLOCK 403 /* clock_gettime64: same 64-bit timespec on both backends */
#define NR_MLOCK 150
#define NR_MUNLOCK 151
#define NR_OPENAT 295
#define NR_READ 3
#define NR_CLOSE 6
#define NR_SEEK 19
#define NR_DUP 41
#define NR_FSYNC 118
#define NR_TIMER 104
#endif
#include "x32.h"
typedef unsigned long uptr;
typedef unsigned short u16;
typedef unsigned char u8;
#define IMAGE_SIZE 0x4000000u
#define INITIAL_SP (IMAGE_SIZE - 0x1000u)
#define DOS_BASE 0x01000000u
#define INPUT_BASE (DOS_BASE + 0xf1400u)
#define JOYSTICK_BASE (DOS_BASE + 0xf1600u)
#define INPUT_MAGIC 0x31504e49u

struct context {
    u32 gs, fs, es, ds, di, si, bp, sp, bx, dx, cx, ax;
    u32 trap, error, ip, cs, flags, signal_sp, ss, fpstate, mask, cr2;
};
struct ucontext {
    u32 flags, link, stack_sp, stack_flags, stack_size;
    struct context registers;
};
struct action { uptr handler, flags, restorer; u32 mask[2]; };
struct descriptor { u32 index, base, limit, flags; };
struct stack { uptr sp; u32 flags; uptr size; };
struct rm_packet {
    u32 magic, entry, ax, bx, cx, dx, si, di;
    u16 ds, es, fs, gs;
    u32 flags, vector;
};
_Static_assert(sizeof(struct rm_packet) == 48, "real-mode bridge packet layout");
_Static_assert(sizeof(struct context) == 88, "i386 sigcontext size");
_Static_assert(__builtin_offsetof(struct ucontext, registers) == 20, "i386 ucontext layout");

extern void signal_return(void);
#ifdef EF_NATIVE64
extern void signal_entry(void);
#endif
static u8 signal_stack[32768];
static struct x32_allocation allocation;
/* Synthetic vector tables. Default protected handlers stop explicitly if used;
 * the original X-32 default handler lives in the extender, outside the payload.
 */
static u32 real_vectors[256];
static struct { u32 offset; u16 selector; } protected_vectors[256];
static struct { u32 offset; u16 selector; } exception_vectors[17];
static u32 conventional_end = 0x10000;
static int game_directory = -1;
static int game_overlay = -1, drive_overlay = -1;
static int real_mode_request = -1, real_mode_response = -1;
static int file_handles[64];
static u8 file_console[64];
static struct { u32 used, base, limit; } user_descriptors[16];
static u32 virtual_interrupts = 1;
static u32 keyboard_ack, keyboard_port61;
static u32 gameport_remaining[4];
static u32 pit_divisor, pit_low, pit_write_high;
static u32 dac_cursor;
static int runtime_ready;
static u32 irq_in_service, irq_return_pending, pit_bios_clocks, timer_interrupts;
static struct context divide_context;
static u32 divide_frame, divide_active, divide_virtual_if;

static uptr guest(u32 offset) { return (u32)(GUEST_BASE + offset); }

static int guest_range(u32 offset, u32 length) {
    if (offset > 0x260 && offset < IMAGE_SIZE && length <= IMAGE_SIZE - offset) return 1;
    uptr address = guest(offset);
    return address >= DOS_BASE + 0x10000 && address < DOS_BASE + conventional_end &&
           length <= DOS_BASE + conventional_end - address;
}

static int syscall4(uptr number, uptr a, uptr b, uptr c, uptr d) {
#ifdef EF_NATIVE64
    long result;
    register uptr fourth __asm__("r10") = d;
    __asm__ volatile("syscall" : "=a"(result)
                     : "0"(number), "D"(a), "S"(b), "d"(c), "r"(fourth)
                     : "rcx", "r11", "memory", "cc");
#else
    int result;
    __asm__ volatile("int $0x80" : "=a"(result)
                     : "0"(number), "b"(a), "c"(b), "d"(c), "S"(d)
                     : "memory", "cc");
#endif
    return result;
}

__attribute__((noreturn)) static void finish(u32 code) {
    if (runtime_ready) {
        int fd = syscall4(NR_OPENAT, (uptr)-100, (uptr)"dos-memory.bin", 0x241, 0600);
        if (fd >= 0) {
            syscall4(NR_WRITE, fd, DOS_BASE, 0x100000, 0);
            syscall4(NR_CLOSE, fd, 0, 0, 0);
        }
    }
    for (;;) syscall4(NR_EXIT, code, 0, 0, 0);
}

static void pipe_transfer(int fd, void *data, u32 size, int write) {
    u8 *cursor = data;
    while (size) {
        int result = syscall4(write ? NR_WRITE : NR_READ, fd, (uptr)cursor, size, 0);
        if (result <= 0) finish(22);
        cursor += result;
        size -= result;
    }
}

static int real_mode_call(struct rm_packet *packet) {
    pipe_transfer(real_mode_request, packet, sizeof(*packet), 1);
    pipe_transfer(real_mode_request, real_vectors, sizeof(real_vectors), 1);
    pipe_transfer(real_mode_request, (void *)DOS_BASE, 0x100000, 1);
    pipe_transfer(real_mode_response, packet, sizeof(*packet), 0);
    if (packet->magic != 0x314d5258) return 0;
    pipe_transfer(real_mode_response, (void *)DOS_BASE, 0x100000, 0);
    for (u32 i = 0; i < 256; ++i) real_vectors[i] = *(u32 *)(uptr)(DOS_BASE + i * 4);
    return 1;
}

static int decimal(char const *string) {
    u32 value = 0;
    if (!*string) finish(22);
    while (*string) {
        if (*string < '0' || *string > '9' || value > 100000) finish(22);
        value = value * 10 + (*string++ - '0');
    }
    return value;
}

static void text(char const *message, u32 length) {
    if (syscall4(NR_WRITE, 1, (uptr)message, length, 0) != (int)length) finish(22);
}

static void field(char const *name, u32 value) {
    char output[32];
    u32 length = 0;
    while (*name) output[length++] = *name++;
    output[length++] = '=';
    for (int shift = 28; shift >= 0; shift -= 4)
        output[length++] = "0123456789abcdef"[(value >> shift) & 15];
    output[length++] = ' ';
    text(output, length);
}

static void record(char const *kind, u32 length, struct context const *r) {
    /* Flight updates a selector several times per frame. Sample paired
     * CALL/RETURN records after the first 128 to keep long runs manageable. */
    static u32 selector_calls;
    if (r->ip == 0x2f0 && (r->ax & 0xffff) == 0x3503) {
        if (length == 5) ++selector_calls;
        if (selector_calls > 128 && (selector_calls & (selector_calls - 1))) return;
    }
    text(kind, length);
    field("IP", r->ip); field("AX", r->ax); field("BX", r->bx);
    field("CX", r->cx); field("DX", r->dx); field("SI", r->si);
    field("DI", r->di); field("SP", r->sp); field("FLAGS", r->flags);
    field("DS", r->ds); field("ES", r->es); field("SS", r->ss); field("FS", r->fs); field("GS", r->gs);
    field("TRAP", r->trap);
    field("ERROR", r->error); field("CR2", r->cr2);
    text("\n", 1);
}

static void descriptor(u32 index, u32 base, u32 limit, u32 flags) {
    struct descriptor d = {index, base, limit, flags};
    if (syscall4(NR_LDT, 1, (uptr)&d, sizeof(d), 0) < 0) finish(22);
}

static int equals(char const *a, char const *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static void dos_result(struct context *r, int result, u32 error) {
    r->ax = result < 0 ? error : (u32)result;
    if (result < 0) r->flags |= 1;
    else r->flags &= ~1u;
}

static int open_game_file(u32 offset, u32 access, int create) {
    char buffer[260];
    char *path = buffer;
    u32 length;
    for (length = 0; length < sizeof(buffer); ++length) {
        if (!guest_range(offset + length, 1)) return -5;
        u8 c = *(u8 *)guest(offset + length);
        if (c == 0) break;
        if (c < 32 || c > 126) return -3;
        path[length] = c == '\\' ? '/' : (c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    if (!length || length == sizeof(buffer)) return -3;
    path[length] = 0;
    text(create ? "FILE CREATE " : "FILE OPEN ", create ? 12 : 10);
    text(path, length); text("\n", 1);
    int root = 0;
    if (length >= 2 && path[1] == ':') {
        if (path[0] != 'C') return -15;
        path += 2; length -= 2;
    }
    if (path[0] == '/') { root = 1; ++path; --length; }
    if (root && length >= 7 && path[0] == 'E' && path[1] == 'F' &&
        path[2] == '2' && path[3] == '0' && path[4] == '0' && path[5] == '0' && path[6] == '/') {
        root = 0; path += 7; length -= 7;
    }
    if (!length) return -3;
    for (u32 i = 0; i < length; ++i)
        if (path[i] == ':' || (path[i] == '.' && path[i + 1] == '.' &&
            (i == 0 || path[i - 1] == '/') &&
            (i + 2 == length || path[i + 2] == '/'))) return -3;
    int console = equals(path, "CON");
    int overlay = root ? drive_overlay : game_overlay;
    int fd;
    if (console) fd = syscall4(NR_DUP, access ? 1 : 0, 0, 0, 0);
    else if (create) {
        if (overlay < 0) return -3;
        fd = syscall4(NR_OPENAT, overlay, (uptr)path, 0x242, 0644);
    } else {
        fd = overlay < 0 ? -2 : syscall4(NR_OPENAT, overlay, (uptr)path, access, 0);
        if (fd == -2 && !root) {
            int original = syscall4(NR_OPENAT, game_directory, (uptr)path, 0, 0);
            if (original < 0 || !access) fd = original;
            else {
                /* Copy on first write. The original descriptor is always read-only. */
                int copy = overlay < 0 ? -3 : syscall4(NR_OPENAT, overlay, (uptr)path, 0xc1, 0644);
                int result = copy;
                if (copy >= 0) {
                    char chunk[4096];
                    while ((result = syscall4(NR_READ, original, (uptr)chunk, sizeof(chunk), 0)) > 0) {
                        int done = 0;
                        while (done < result) {
                            int written = syscall4(NR_WRITE, copy, (uptr)(chunk + done), result - done, 0);
                            if (written <= 0) { result = -5; break; }
                            done += written;
                        }
                        if (result < 0) break;
                    }
                    syscall4(NR_CLOSE, copy, 0, 0, 0);
                }
                syscall4(NR_CLOSE, original, 0, 0, 0);
                fd = result < 0 ? result : syscall4(NR_OPENAT, overlay, (uptr)path, access, 0);
            }
        }
    }
    if (fd < 0) return fd == -2 ? -2 : -5;
    for (u32 handle = 5; handle < 64; ++handle) {
        if (file_handles[handle] < 0) {
            file_handles[handle] = fd;
            file_console[handle] = console;
            return handle;
        }
    }
    syscall4(NR_CLOSE, fd, 0, 0, 0);
    return -4;
}

static void checkpoint(struct context const *r) {
    u32 selector_limit;
    u32 selector = 7;
    __asm__ volatile("lsl %1, %0" : "=r"(selector_limit) : "r"(selector) : "cc");
    int zeroed = 1;
    for (u32 address = 0x195fb0; address < 0x4de2f0; ++address)
        if (*(volatile u8 *)guest(address) != 0) { zeroed = 0; break; }
    int command = equals((char *)guest(*(u32 *)guest(0x132a34)), "gui");
    int program = equals((char *)guest(*(u32 *)guest(0x132a38)), "C:\\EF2000\\_EF2000.EXE");
    text("CHECK ", 6);
    field("DATA_LIMIT", selector_limit);
    field("BSS_ZERO", zeroed);
    field("COMMAND", command);
    field("PROGRAM", program);
    field("PSP", *(u16 *)guest(0x132a3c));
    field("ENV", *(u16 *)guest(0x132a65));
    text("\n", 1);
    if (selector_limit != 0x260 || !zeroed || !command || !program || r->ss != 7)
        finish(24);
}

static void pace_clock_poll(void) {
    /* The original simulation clamps dt to at least one 50 Hz tick per
     * frame. Do not allow its DOS wall-clock polling to outrun that tick.
     * Use monotonic deadlines; the returned DOS time remains real UTC. */
    static struct { unsigned long long seconds, nanoseconds; } previous;
    struct { unsigned long long seconds, nanoseconds; } now, target;
    if (syscall4(NR_CLOCK, 1, (uptr)&now, 0, 0) < 0) finish(22);
    target.seconds = previous.seconds;
    target.nanoseconds = previous.nanoseconds + 20000000;
    if (target.nanoseconds >= 1000000000) {
        target.nanoseconds -= 1000000000;
        ++target.seconds;
    }
    if (now.seconds < target.seconds ||
        (now.seconds == target.seconds && now.nanoseconds < target.nanoseconds)) {
        int result;
        do { result = syscall4(NR_SLEEP, 1, 1, (uptr)&target, 0); } while (result == -4);
        if (result < 0) finish(22);
        if (syscall4(NR_CLOCK, 1, (uptr)&now, 0, 0) < 0) finish(22);
    }
    previous.seconds = now.seconds;
    previous.nanoseconds = now.nanoseconds;
}

static void irq_epilogue(struct context *r) {
    if (!irq_return_pending) return;
    if (r->ip < IMAGE_SIZE + 0x110 && *(u8 *)guest(r->ip) == 0xcf && guest_range(r->sp, 12)) {
        u32 *frame = (u32 *)guest(r->sp);
        if ((frame[1] & 0xffff) != 0x2f) finish(21);
        r->ip = frame[0];
        r->cs = frame[1];
        r->flags = frame[2] & ~0x10000u;
        r->sp += 12;
        virtual_interrupts = !!(r->flags & 0x200);
        irq_return_pending = irq_in_service = 0;
    } else r->flags |= 0x100; /* Trace only the short EOI-to-IRET epilogue. */
}

static int keyboard_pending(void) {
    return *(u32 *)(uptr)INPUT_BASE == INPUT_MAGIC &&
        *(u16 *)(uptr)(INPUT_BASE + 4) != *(u16 *)(uptr)(INPUT_BASE + 6);
}

static u32 keyboard_read(void) {
    if (keyboard_ack) { keyboard_ack = 0; return 0xfa; }
    if (!keyboard_pending()) return 0;
    u16 *head = (u16 *)(uptr)(INPUT_BASE + 4);
    u32 value = *(u8 *)(uptr)(INPUT_BASE + 8 + (*head & 255));
    *head = (*head + 1) & 255;
    return value;
}

static void guest_fault(u32 signal, struct context *r) {
    if (signal == 11 && r->trap == 13 && r->ip == IMAGE_SIZE + 0x120 && divide_active) {
        u32 *context = (u32 *)guest(divide_frame);
        if (context[5] != 0x2f || context[4] < 0x86 || context[4] >= IMAGE_SIZE) finish(21);
        *r = divide_context;
        r->ax = context[0];
        r->ip = context[4];
        r->flags = context[6] & ~0x10000u;
        virtual_interrupts = divide_virtual_if;
        divide_active = 0;
        text("EXCEPTION RETURN ", 17); field("VECTOR", 0); field("IP", r->ip); text("\n", 1);
        return;
    }
    if (signal == 8 && r->trap == 0 && !divide_active && r->cs == 0x2f && r->ss == 7 &&
        exception_vectors[0].offset < IMAGE_SIZE && exception_vectors[0].selector == 0x2f &&
        r->sp >= 0x100 && guest_range(r->sp - 0x100, 0x100)) {
        /* Measured original X-32 exception ABI: IRET frame followed by an
         * SS-relative context pointer; context+0e=2, +10=EIP, +14=CS. */
        divide_context = *r;
        divide_virtual_if = virtual_interrupts;
        divide_frame = r->sp - 0x80;
        divide_active = 1;
        u32 *context = (u32 *)guest(divide_frame);
        for (u32 i = 0; i < 16; ++i) context[i] = 0;
        context[0] = r->ax;
        context[1] = (r->ds & 0xffff) | (r->cs << 16);
        context[2] = (r->es & 0xffff) | (r->fs << 16);
        context[3] = (r->gs & 0xffff) | 0x20000;
        context[4] = r->ip;
        context[5] = r->cs;
        context[6] = r->flags & ~0x10000u;
        context[7] = r->sp;
        context[8] = r->ss;
        r->sp -= 0x100;
        u32 *frame = (u32 *)guest(r->sp);
        frame[0] = IMAGE_SIZE + 0x120;
        frame[1] = r->cs;
        frame[2] = r->flags & ~0x10100u;
        frame[3] = divide_frame;
        r->ip = exception_vectors[0].offset;
        r->flags &= ~0x10100u;
        virtual_interrupts = 0;
        text("EXCEPTION ENTER ", 16); field("VECTOR", 0);
        field("IP", divide_context.ip); field("HANDLER", r->ip); text("\n", 1);
        return;
    }
    if (signal == 5 && r->trap == 1 && irq_return_pending) {
        r->flags &= ~0x10000u;
        irq_epilogue(r);
        return;
    }
    if (signal == 10) { text("TIME_LIMIT reached\n", 19); finish(27); }
    if (signal == 12) { text("WINDOW closed\n", 14); finish(28); }
    if (signal == 14) {
        pit_bios_clocks += pit_divisor;
        while (pit_bios_clocks >= 65536) {
            ++*(u32 *)(uptr)(DOS_BASE + 0x46c);
            pit_bios_clocks -= 65536;
        }
        if (!virtual_interrupts || irq_in_service || r->cs != 0x2f || r->ss != 7 ||
            r->ip < 0x86 || r->ip >= IMAGE_SIZE || r->sp < 12 || !guest_range(r->sp - 12, 12)) return;
        if (real_mode_request >= 0) {
            static u32 last_refresh;
            struct { unsigned long long seconds, nanoseconds; } now;
            if (syscall4(NR_CLOCK, 1, (uptr)&now, 0, 0) < 0) finish(22);
            u32 millis = (u32)now.seconds * 1000 + (u32)now.nanoseconds / 1000000;
            if (millis - last_refresh >= 5) {
                last_refresh = millis;
                /* Host-only framebuffer snapshot; no guest interrupt or registers. */
                struct rm_packet packet = {.magic = 0x314d5258, .vector = 0x100};
                if (!real_mode_call(&packet)) finish(22);
            }
        }
        u32 vector = 8;
        if (keyboard_pending()) {
            if (protected_vectors[9].offset != IMAGE_SIZE + 0x100) vector = 9;
            else {
                /* Host BIOS key queue serves INT16 until a guest IRQ1 handler
                 * is installed; discard its duplicate raw scan bytes. */
                *(u16 *)(uptr)(INPUT_BASE + 4) = *(u16 *)(uptr)(INPUT_BASE + 6);
            }
        }
        u32 *frame = (u32 *)guest(r->sp - 12);
        frame[0] = r->ip; frame[1] = r->cs; frame[2] = r->flags & ~0x10000u;
        r->sp -= 12;
        r->ip = protected_vectors[vector].offset;
        r->flags &= ~0x10100u;
        virtual_interrupts = 0;
        irq_in_service = 1;
        if (++timer_interrupts <= 4) {
            text("TIMER ", 6); field("IP", r->ip); field("COUNT", timer_interrupts); text("\n", 1);
        }
        return;
    }
    if (signal == 7 && r->trap == 17 && (r->flags & 0x40000)) {
        /* DOS runs this CPU probe with alignment checking disabled in CR0.
         * Linux enables CR0.AM and the probe leaves EFLAGS.AC set. Resume the
         * faulting instruction with AC clear; no game instruction is skipped.
         * PUSHF reflection of AC is a declared native CPU-profile limitation.
         */
        text("CPU alignment-check-disabled\n", 29);
        r->flags &= ~0x50000u;
        return;
    }
    int protection_fault = signal == 11 && r->trap == 13;
#ifndef EF_NATIVE64
    /* qemu-user leaves trapno unavailable in these signal frames. Decode only
     * the explicitly supported privileged opcodes at known guest addresses.
     */
    protection_fault |= signal == 11 && r->trap == 0xffffffffu;
#endif
    if (protection_fault && r->ip >= 0x86 && r->ip < 0x195fae &&
        *(u8 *)guest(r->ip) == 0xcd && real_mode_request >= 0) {
        u32 vector = *(u8 *)guest(r->ip + 1);
        if (vector == 0x10 || vector == 0x33 || vector == 0x16 || vector == 0x1a) {
            r->flags &= ~0x10000u;
            record("CALL ", 5, r);
            struct rm_packet packet = {0x314d5258, real_vectors[vector], r->ax, r->bx,
                r->cx, r->dx, r->si, r->di, 0, 0, 0, 0, r->flags, vector};
            if (!real_mode_call(&packet)) { record("STOP ", 5, r); finish(20); }
            r->ax = packet.ax; r->bx = packet.bx; r->cx = packet.cx; r->dx = packet.dx;
            r->si = packet.si; r->di = packet.di;
            r->flags = (r->flags & ~0x8d5u) | (packet.flags & 0x8d5);
            record("RETURN ", 7, r);
            r->ip += 2;
            return;
        }
    }
    if (protection_fault && ((r->ip >= 0x86 && r->ip < 0x195fb0) ||
                            (r->ip >= IMAGE_SIZE + 0x100 && r->ip < IMAGE_SIZE + 0x110))) {
        u8 opcode = *(u8 *)guest(r->ip);
        if (opcode == 0xfa || opcode == 0xfb) {
            virtual_interrupts = opcode == 0xfb;
            if (virtual_interrupts && irq_return_pending) {
                /* STI explicitly re-enables IRQ delivery; no return tracing
                 * is needed for handlers that enable nested interrupts. */
                irq_return_pending = irq_in_service = 0;
                r->flags &= ~0x100u;
            }
            r->flags &= ~0x10000u;
            text("INTERRUPTS ", 11); field("ENABLED", virtual_interrupts); text("\n", 1);
            ++r->ip;
            irq_epilogue(r);
            return;
        }
        u32 port = r->dx & 0xffff;
        if (opcode == 0xe6 || opcode == 0xe4) port = *(u8 *)guest(r->ip + 1);
        if ((opcode == 0xe6 || opcode == 0xee) && port == 0x20 && (r->ax & 255) == 0x20) {
            if (irq_in_service) irq_return_pending = 1;
            r->flags &= ~0x10000u;
            r->ip += opcode == 0xe6 ? 2 : 1;
            irq_epilogue(r);
            return;
        }
        if ((opcode == 0xee || opcode == 0xe6) && (port == 0x3c8 || port == 0x3c9)) {
            if (port == 0x3c8) dac_cursor = (r->ax & 255) * 3;
            else {
                *(u8 *)(uptr)(DOS_BASE + 0xf1000 + dac_cursor) = r->ax & 63;
                dac_cursor = (dac_cursor + 1) % 768;
            }
            r->flags &= ~0x10000u;
            r->ip += opcode == 0xe6 ? 2 : 1;
            return;
        }
        if (opcode == 0xec && port == 0x3da) {
            struct { unsigned long long seconds, nanoseconds; } now;
            if (syscall4(NR_CLOCK, 1, (uptr)&now, 0, 0) < 0) finish(22);
            u32 status = (u32)now.nanoseconds % 16666667 >= 15000000 ? 8 : 0;
            r->ax = (r->ax & ~255u) | status;
            r->flags &= ~0x10000u;
            ++r->ip;
            return;
        }
        if ((opcode == 0xe6 || opcode == 0xee) && (port == 0x43 || port == 0x40)) {
            u32 value = r->ax & 255;
            if (port == 0x43 && (value == 0x36 || value == 0x34)) pit_write_high = 0;
            else if (port == 0x40) {
                if (!pit_write_high) pit_low = value;
                else {
                    pit_divisor = pit_low | (value << 8);
                    if (!pit_divisor) pit_divisor = 65536;
                    u32 usec = (pit_divisor * 838 + 500) / 1000;
                    if (!usec) usec = 1;
                    struct { uptr interval_sec, interval_usec, value_sec, value_usec; } timer = {0, usec, 0, usec};
                    if (syscall4(NR_TIMER, 0, (uptr)&timer, 0, 0) < 0) finish(22);
                }
                pit_write_high ^= 1;
            } else {
                record("FAULT ", 6, r); finish(21);
            }
            r->flags &= ~0x10000u;
            text("PIT ", 4); field("PORT", port); field("VALUE", value);
            field("DIVISOR", pit_divisor); text("\n", 1);
            r->ip += opcode == 0xe6 ? 2 : 1;
            return;
        }
        if ((opcode == 0xec || opcode == 0xe4 || opcode == 0xee || opcode == 0xe6) && port == 0x61) {
            if (opcode == 0xec || opcode == 0xe4) r->ax = (r->ax & ~255u) | keyboard_port61;
            else keyboard_port61 = r->ax & 255;
            r->flags &= ~0x10000u;
            r->ip += (opcode == 0xe4 || opcode == 0xe6) ? 2 : 1;
            return;
        }
        if ((opcode == 0xec || opcode == 0xe4 || opcode == 0xee || opcode == 0xe6) && (port == 0x60 || port == 0x64)) {
            u32 value = r->ax & 255;
            if (opcode == 0xec || opcode == 0xe4) {
                value = port == 0x64 ? ((keyboard_ack || keyboard_pending()) ? 1 : 0) : keyboard_read();
                r->ax = (r->ax & ~255u) | value;
            } else if (port == 0x60) keyboard_ack = 1;
            else {
                record("FAULT ", 6, r);
                finish(21); /* Controller commands need their own state model. */
            }
            r->flags &= ~0x10000u;
            text((opcode == 0xec || opcode == 0xe4) ? "IO IN " : "IO OUT ",
                 (opcode == 0xec || opcode == 0xe4) ? 6 : 7);
            field("IP", r->ip); field("PORT", port); field("VALUE", value); text("\n", 1);
            r->ip += (opcode == 0xe4 || opcode == 0xe6) ? 2 : 1;
            return;
        }
    }
    if (protection_fault && r->ip >= 0x86 && r->ip < 0x195fb0 &&
        (*(u8 *)guest(r->ip) == 0xec || *(u8 *)guest(r->ip) == 0xee) &&
        (r->dx & 0xffff) == 0x201) {
        u32 mask = *(u32 *)(uptr)INPUT_BASE == INPUT_MAGIC ?
            *(u8 *)(uptr)JOYSTICK_BASE & 15 : 0;
        if (*(u8 *)guest(r->ip) == 0xee) {
            static u32 last_axes[4], last_buttons = 0xffffffffu, last_mask = 0xffffffffu;
            u32 buttons = *(u8 *)(uptr)(JOYSTICK_BASE + 1) & 15;
            int changed = buttons != last_buttons || mask != last_mask;
            for (u32 axis = 0; axis < 4; ++axis) {
                gameport_remaining[axis] = *(u16 *)(uptr)(JOYSTICK_BASE + 2 + axis * 2);
                if (gameport_remaining[axis] != last_axes[axis]) changed = 1;
                last_axes[axis] = gameport_remaining[axis];
            }
            if (changed) {
                text("GAMEPORT ", 9); field("IP", r->ip); field("AXES", mask);
                field("X", last_axes[0]); field("Y", last_axes[1]);
                field("Z", last_axes[2]); field("R", last_axes[3]); field("BUTTONS", buttons);
                text("\n", 1);
                last_buttons = buttons; last_mask = mask;
            }
        } else {
            u32 buttons = mask ? *(u8 *)(uptr)(JOYSTICK_BASE + 1) & 15 : 0;
            u32 value = ((~buttons & 15) << 4) | (~mask & 15);
            for (u32 axis = 0; axis < 4; ++axis) {
                if ((mask & (1u << axis)) && gameport_remaining[axis]) {
                    value |= 1u << axis;
                    --gameport_remaining[axis];
                }
            }
            r->ax = (r->ax & ~255u) | value;
        }
        r->flags &= ~0x10000u;
        ++r->ip;
        return;
    }
    if (signal != 11 || r->ip < 0x86 || r->ip > 0x195fae ||
        *(u8 *)guest(r->ip) != 0xcd || *(u8 *)guest(r->ip + 1) != 0x21) {
        record("FAULT ", 6, r);
        finish(21);
    }
#ifdef EF_NATIVE64
    /* #GP interception introduces RF; the original software INT path does not. */
    r->flags &= ~0x10000u;
#endif
    record("CALL ", 5, r);
    if (r->ip == 0x93 && (r->ax & 0xffff) == 0x3505 &&
        (r->bx & 0xffff) == 7 && r->cx == 0x260) {
        /* The original uses an expand-down descriptor, NOT a flat no-op.
         * Implement only the verified first request; later limits need evidence.
         * LDT flags: 32-bit, expand-down data, writable, byte limit, usable.
         */
        descriptor(0, GUEST_BASE, 0x260, 0x43);
        r->cx = 0x260;
        r->flags = (r->flags | 0x40) & ~1u; /* LSL success ZF; service clears CF. */
    } else if (r->ip == 0xa0 && (r->ax & 0xffff) == 0x3005) {
        /* DOSBox-X reference: DOS 5.0; clear BL and CX; preserve their upper bits.
         * Only this startup query is supported, not the complete AH=30 API.
         */
        r->ax = 5;
        r->bx &= ~255u;
        r->cx &= 0xffff0000u;
    } else if (r->ip == 0x19a && (r->ax & 0xffff) == 0x3509) {
        checkpoint(r);
        struct x32_stack_result result;
        if (!x32_stack_reserve(&allocation, r->bx, r->cx, r->flags, &result)) {
            text("ALLOCATION unsupported-or-exhausted\n", 36);
            finish(25);
        }
        r->dx = result.low_limit;
        r->flags = result.flags;
        text("ALLOCATION ", 11);
        field("STACK_END", allocation.stack_end);
        field("FREE", allocation.free_bytes);
        field("STACK_LOW", result.low_limit);
        text("\n", 1);
    } else if (r->ip == 0x245 && (r->ax & 0xffff) == 0x3508) {
        struct x32_heap_result result;
        if (!x32_heap_grow(&allocation, r->cx, r->flags, &result)) finish(25);
        r->ax = result.stack_low;
        r->cx = result.heap_end;
        r->flags = result.flags;
    } else if ((r->ax & 0xffff) == 0x350b) {
        u32 remaining = allocation.heap_ceiling - allocation.heap_end;
        r->ax = remaining < allocation.free_bytes ? remaining : allocation.free_bytes;
        r->flags &= ~1u;
    } else if ((r->ax & 0xffff) == 0x3504 &&
               ((r->bx & 0xffff) == 7 || (r->bx & 0xffff) == protected_vectors[0].selector)) {
        r->cx = GUEST_BASE;
        r->flags &= ~1u;
    } else if ((r->ax & 0xffff) == 0x3501) {
        u32 index;
        for (index = 0; index < 16 && user_descriptors[index].used; ++index) {}
        if (index == 16) {
            r->bx = 0;
            r->flags |= 1;
        } else {
            user_descriptors[index].used = 1;
            user_descriptors[index].base = 0;
            user_descriptors[index].limit = 0xffffffffu;
            descriptor(index + 6, 0, 0xfffff, 0x51);
            r->bx = ((index + 6) << 3) | 7;
            r->flags &= ~1u;
        }
    } else if ((r->ax & 0xffff) >= 0x3502 && (r->ax & 0xffff) <= 0x3505 &&
               (r->bx & 7) == 7 && (r->bx >> 3) >= 6 && (r->bx >> 3) < 22 &&
               user_descriptors[(r->bx >> 3) - 6].used) {
        u32 index = (r->bx >> 3) - 6;
        u32 function = r->ax & 255;
        if (function == 2) {
            descriptor(index + 6, 0, 0, 0x61);
            user_descriptors[index].used = 0;
        } else if (function == 4) {
            r->cx = user_descriptors[index].base;
        } else {
            if (function == 3) user_descriptors[index].base = r->cx;
            else {
                if (r->cx > 0xfffff) r->cx |= 0xfff;
                user_descriptors[index].limit = r->cx;
            }
            u32 limit = user_descriptors[index].limit;
            descriptor(index + 6, user_descriptors[index].base,
                       limit > 0xfffff ? limit >> 12 : limit,
                       limit > 0xfffff ? 0x51 : 0x41);
        }
        r->flags &= ~1u;
    } else if (((r->ax >> 8) & 255) == 0x2a) {
        /* DOS date uses the same UTC clock as AH=2c. */
        struct { unsigned long long seconds, nanoseconds; } now;
        if (syscall4(NR_CLOCK, 0, (uptr)&now, 0, 0) < 0) finish(22);
        u32 days = (u32)now.seconds / 86400;
        u32 weekday = (days + 4) % 7;
        u32 year = 1970, month = 0;
        for (;;) {
            u32 leap = !(year % 4) && ((year % 100) || !(year % 400));
            if (days < 365 + leap) break;
            days -= 365 + leap;
            ++year;
        }
        const u32 lengths[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        for (;;) {
            u32 length = lengths[month] +
                (month == 1 && !(year % 4) && ((year % 100) || !(year % 400)));
            if (days < length) break;
            days -= length;
            ++month;
        }
        r->ax = (r->ax & 0xffffff00u) | weekday;
        r->cx = (r->cx & 0xffff0000u) | year;
        r->dx = (r->dx & 0xffff0000u) | ((month + 1) << 8) | (days + 1);
    } else if (((r->ax >> 8) & 255) == 0x2c) {
        pace_clock_poll();
        /* Measured DOSBox-X ABI: EAX=2c00, low CX/DX replaced, flags and all
         * other registers preserved. Declared UTC wall-clock model; every
         * sample is recorded, rather than freezing time during initialization.
         */
        struct { unsigned long long seconds, nanoseconds; } now;
        if (syscall4(NR_CLOCK, 0, (uptr)&now, 0, 0) < 0) finish(22);
        u32 seconds = (u32)now.seconds % 86400;
        u32 hundredths = (u32)now.nanoseconds / 10000000;
        r->ax = 0x2c00;
        r->cx = (r->cx & 0xffff0000u) | ((seconds / 3600) << 8) |
                ((seconds / 60) % 60);
        r->dx = (r->dx & 0xffff0000u) | ((seconds % 60) << 8) | hundredths;
        text("CLOCK ", 6);
        field("SECONDS", (u32)now.seconds);
        field("NANOSECONDS", (u32)now.nanoseconds);
        text("\n", 1);
    } else if ((r->ax & 0xffff) == 0x250c) {
        /* Original file 0x21b1: legacy PIC bases, master 08h and slave 70h. */
        r->ax = (r->ax & 0xffff0000u) | 0x7008;
        r->bx &= 0xffff0000u;
        r->flags &= ~1u;
    } else if ((r->ax & 0xffff) == 0x2503) {
        r->bx = real_vectors[r->cx & 255];
        r->flags &= ~1u;
    } else if ((r->ax & 0xffff) == 0x2502) {
        r->bx = protected_vectors[r->cx & 255].offset;
        r->es = protected_vectors[r->cx & 255].selector;
        r->flags &= ~1u;
    } else if ((r->ax & 0xffff) == 0x2504) {
        protected_vectors[r->cx & 255].offset = r->dx;
        protected_vectors[r->cx & 255].selector = r->ds;
        r->flags &= ~1u;
    } else if ((r->ax & 0xffff) == 0x2505) {
        real_vectors[r->cx & 255] = r->bx;
        r->flags &= ~1u;
    } else if ((r->ax & 0xffff) == 0x2506) {
        u32 vector = r->cx & 255;
        protected_vectors[vector].offset = r->dx;
        protected_vectors[vector].selector = r->ds;
        /* The extender installs a real-mode callback stub for the protected
         * handler. Keep a separate synthetic thunk address for round trips.
         * Real-mode execution is not supported by this harness yet.
         */
        real_vectors[vector] = 0xf0000000u | (vector << 2);
        r->flags &= ~1u;
    } else if (((r->ax >> 8) & 255) == 0x48 && (r->bx & 0xffff)) {
        u32 size = (r->bx & 0xffff) << 4;
        if (size > 0x90000 - conventional_end) {
            r->ax = 8;
            r->bx = (0x90000 - conventional_end) >> 4;
            r->flags |= 1;
        } else {
            /* X-32 returns both a real-mode segment and an application-relative
             * pointer. The latter wraps when conventional RAM is below DS base.
             */
            r->ax = conventional_end >> 4;
            r->bx = DOS_BASE + conventional_end - GUEST_BASE;
            conventional_end += size;
            r->flags &= ~1u;
        }
    } else if ((r->ax & 0xffff) == 0x252b &&
               ((r->bx & 0xffff) == 0x501 || (r->bx & 0xffff) == 0x601) &&
               (r->es == 7 || r->es == protected_vectors[0].selector) &&
               r->cx < IMAGE_SIZE && r->dx <= IMAGE_SIZE - r->cx) {
        /* X-32 252b, selector-relative lock/unlock (0501/0601). Map the
         * declared image range to Linux page residency, including real failure.
         */
        int result = syscall4((r->bx & 0xffff) == 0x501 ? NR_MLOCK : NR_MUNLOCK,
                              guest(r->cx), r->dx, 0, 0);
        if (result < 0) {
            r->ax = 8;
            r->flags |= 1;
        } else {
            r->ax = 0x252b; /* Upper EAX is an extender implementation temporary. */
            r->flags &= ~1u;
        }
    } else if ((r->ax & 0xffff) == 0x252b &&
               (((r->bx >> 8) & 255) < 5 || ((r->bx >> 8) & 255) > 6 || (r->bx & 255) > 1)) {
        /* Original handler's invalid-subfunction path at file 0x248f. */
        r->ax = 0xa5a5a5a5;
        r->flags |= 1;
    } else if ((r->ax & 0xffff) == 0x2532 && (r->cx & 255) < 0x10) {
        u32 vector = r->cx & 255;
        r->bx = vector < 8 ? protected_vectors[vector].offset : exception_vectors[vector].offset;
        r->es = vector < 8 ? protected_vectors[vector].selector : exception_vectors[vector].selector;
        r->flags &= ~1u;
    } else if ((r->ax & 0xffff) == 0x2533 && (r->cx & 255) <= 0x10 &&
               r->ds == protected_vectors[0].selector && r->dx <= IMAGE_SIZE + 0x100) {
        u32 vector = r->cx & 255;
        exception_vectors[vector].offset = r->dx;
        exception_vectors[vector].selector = r->ds;
        if (vector < 8) {
            protected_vectors[vector].offset = r->dx;
            protected_vectors[vector].selector = r->ds;
        }
        /* Registration is supported; exception delivery still stops explicitly
         * until the extender's exception frame/return ABI is implemented.
         */
        r->flags &= ~1u;
    } else if (((r->ax >> 8) & 255) == 0x3d && (r->ax & 3) < 3 && r->ds == 7) {
        int result = open_game_file(r->dx, r->ax & 3, 0);
        r->dx = 0; /* X-32's filename copy redirects DOS to its transfer buffer. */
        dos_result(r, result, result < 0 ? (u32)-result : 0);
    } else if (((r->ax >> 8) & 255) == 0x3c && r->ds == 7) {
        int result = open_game_file(r->dx, 1, 1);
        r->dx = 0;
        dos_result(r, result, result < 0 ? (u32)-result : 0);
    } else if (((r->ax >> 8) & 255) == 0x68) {
        u32 handle = r->bx & 0xffff;
        if (handle <= 2 || (handle < 64 && file_console[handle] && file_handles[handle] >= 0))
            r->flags &= ~1u;
        else if (handle < 5 || handle >= 64 || file_handles[handle] < 0)
            dos_result(r, -1, 6);
        else if (syscall4(NR_FSYNC, file_handles[handle], 0, 0, 0) < 0)
            dos_result(r, -1, 5);
        else r->flags &= ~1u;
    } else if (((r->ax >> 8) & 255) == 0x3e) {
        u32 handle = r->bx & 0xffff;
        if (handle < 5 || handle >= 64 || file_handles[handle] < 0) dos_result(r, -1, 6);
        else {
            int result = syscall4(NR_CLOSE, file_handles[handle], 0, 0, 0);
            file_handles[handle] = -1;
            dos_result(r, result < 0 ? result : 0x3e01, 6);
        }
    } else if (((r->ax >> 8) & 255) == 0x3f && r->ds == 7 && guest_range(r->dx, r->cx)) {
        u32 handle = r->bx & 0xffff;
        if (handle < 5 || handle >= 64 || file_handles[handle] < 0) dos_result(r, -1, 6);
        else {
            int result = syscall4(NR_READ, file_handles[handle], guest(r->dx), r->cx, 0);
            r->cx = 0;
            r->dx = 0;
            dos_result(r, result, 5);
        }
    } else if ((r->ax & 0xffff) == 0x4400) {
        u32 handle = r->bx & 0xffff;
        if (handle <= 2 || (handle < 64 && file_console[handle] && file_handles[handle] >= 0)) {
            r->dx = (r->dx & 0xffff0000u) | 0x80d3;
            dos_result(r, 0x80d3, 0);
        } else if (handle < 5 || handle >= 64 || file_handles[handle] < 0) dos_result(r, -1, 6);
        else {
            r->dx = (r->dx & 0xffff0000u) | 2; /* Ordinary disk file on synthetic C:. */
            dos_result(r, 2, 0);
        }
    } else if (((r->ax >> 8) & 255) == 0x42 && (r->ax & 255) <= 2) {
        u32 handle = r->bx & 0xffff;
        if (handle < 5 || handle >= 64 || file_handles[handle] < 0) dos_result(r, -1, 6);
        else {
            u32 offset = ((r->cx & 0xffff) << 16) | (r->dx & 0xffff);
            int result = syscall4(NR_SEEK, file_handles[handle], (uptr)(long)(int)offset,
                                  r->ax & 255, 0);
            if (result < 0) dos_result(r, -1, 1);
            else {
                r->dx = (r->dx & 0xffff0000u) | ((u32)result >> 16);
                dos_result(r, result & 0xffff, 0);
            }
        }
    } else if (((r->ax >> 8) & 255) == 0x40 && r->ds == 7 && guest_range(r->dx, r->cx)) {
        u32 handle = r->bx & 0xffff;
        if (handle == 1 || handle == 2 ||
            (handle < 64 && file_console[handle] && file_handles[handle] >= 0)) {
            text("CONSOLE ", 8);
            text((char const *)guest(r->dx), r->cx);
            text("\n", 1);
            dos_result(r, r->cx, 0);
        } else if (handle < 5 || handle >= 64 || file_handles[handle] < 0) dos_result(r, -1, 6);
        else dos_result(r, syscall4(NR_WRITE, file_handles[handle], guest(r->dx), r->cx, 0), 5);
    } else if (((r->ax >> 8) & 255) == 0x4c) {
        text("EXIT ", 5); field("CODE", r->ax & 255); text("\n", 1);
        finish(26);
    } else if ((r->ax & 0xffff) == 0x2511 && real_mode_request >= 0 &&
               r->ds == 7 && guest_range(r->dx, 18)) {
        u8 *frame = (u8 *)guest(r->dx);
        u32 vector = *(u16 *)frame;
        if (vector >= 256) finish(25);
        struct rm_packet packet = {0x314d5258, real_vectors[vector], *(u32 *)(frame + 10), r->bx, r->cx,
                    *(u32 *)(frame + 14), r->si, r->di, *(u16 *)(frame + 2),
                    *(u16 *)(frame + 4), *(u16 *)(frame + 6), *(u16 *)(frame + 8), r->flags, vector};
        if (!real_mode_call(&packet)) { record("STOP ", 5, r); finish(20); }
        r->ax = packet.ax; r->bx = packet.bx; r->cx = packet.cx;
        r->dx = packet.dx; r->si = packet.si; r->di = packet.di;
        *(u16 *)(frame + 2) = packet.ds; *(u16 *)(frame + 4) = packet.es;
        *(u16 *)(frame + 6) = packet.fs; *(u16 *)(frame + 8) = packet.gs;
        *(u32 *)(frame + 14) = packet.dx;
        r->flags = (r->flags & ~0x8d5u) | (packet.flags & 0x8d5);
    } else {
        if ((r->ax & 0xffff) == 0x2511 && r->ds == 7 && guest_range(r->dx, 18)) {
            u8 const *frame = (u8 const *)guest(r->dx);
            u32 vector = *(u16 const *)frame;
            text("REALMODE ", 9);
            field("VECTOR", vector);
            field("ENTRY", vector < 256 ? real_vectors[vector] : 0);
            field("DS", *(u16 const *)(frame + 2));
            field("ES", *(u16 const *)(frame + 4));
            field("FS", *(u16 const *)(frame + 6));
            field("GS", *(u16 const *)(frame + 8));
            field("AX", *(u32 const *)(frame + 10));
            field("DX", *(u32 const *)(frame + 14));
            field("BX", r->bx); field("CX", r->cx); field("SI", r->si); field("DI", r->di);
            text("\n", 1);
            int fd = syscall4(NR_OPENAT, (uptr)-100, (uptr)"conventional.bin", 0x241, 0600);
            if (fd < 0 || syscall4(NR_WRITE, fd, DOS_BASE, 0x100000, 0) != 0x100000) finish(22);
            if (syscall4(NR_CLOSE, fd, 0, 0, 0) < 0) finish(22);
        }
        record("STOP ", 5, r);
        finish(20);
    }
    record("RETURN ", 7, r);
    r->ip += 2;
}

#ifdef EF_NATIVE64
void fault_handler(int signal, void *information, void *context) {
    (void)information;
    ucontext_t *uc = context;
    greg_t *g = uc->uc_mcontext.gregs;
    unsigned long packed = g[REG_CSGSFS];
    u16 ds, es;
    __asm__ volatile("mov %%ds,%0; mov %%es,%1" : "=r"(ds), "=r"(es));
    struct context r = {0};
    /* Linux's 64-bit signal ABI does not report the interrupted FS/GS selectors.
     * Keep the kernel-managed state untouched; mark these log fields unavailable.
     */
    r.gs = 0xffffffffu; r.fs = 0xffffffffu;
    r.ds = ds; r.es = es; r.ss = packed >> 48; r.cs = packed & 0xffff;
    r.ax = g[REG_RAX]; r.bx = g[REG_RBX]; r.cx = g[REG_RCX]; r.dx = g[REG_RDX];
    r.si = g[REG_RSI]; r.di = g[REG_RDI]; r.bp = g[REG_RBP]; r.sp = g[REG_RSP];
    r.ip = g[REG_RIP]; r.flags = g[REG_EFL]; r.trap = g[REG_TRAPNO];
    r.error = g[REG_ERR]; r.cr2 = g[REG_CR2];
    guest_fault(signal, &r);
    g[REG_RAX] = r.ax; g[REG_RBX] = r.bx; g[REG_RCX] = r.cx; g[REG_RDX] = r.dx;
    g[REG_RSI] = r.si; g[REG_RDI] = r.di; g[REG_RBP] = r.bp; g[REG_RSP] = r.sp;
    g[REG_RIP] = r.ip; g[REG_EFL] = r.flags;
    /* Reload the hidden descriptor caches, as the original extender's POP DS/ES
     * does on its return path. Long-mode host code ignores these segment bases.
     */
    __asm__ volatile("mov %0,%%ds; mov %1,%%es" : : "r"((u16)r.ds), "r"((u16)r.es));
}
#else
static void fault(u32 signal, void *information, struct ucontext *uc) {
    (void)information;
    guest_fault(signal, &uc->registers);
}
#endif

void setup(uptr *initial_stack) {
    if (initial_stack[0] != 2 && initial_stack[0] != 4) finish(22);
    if (initial_stack[0] == 4) {
        real_mode_request = decimal((char const *)initial_stack[3]);
        real_mode_response = decimal((char const *)initial_stack[4]);
    }
    game_directory = syscall4(NR_OPENAT, (uptr)-100, initial_stack[2], 0x10000, 0);
    if (game_directory < 0) finish(22);
    game_overlay = syscall4(NR_OPENAT, (uptr)-100, (uptr)"drive/EF2000", 0x10000, 0);
    drive_overlay = syscall4(NR_OPENAT, (uptr)-100, (uptr)"drive", 0x10000, 0);
    for (u32 i = 0; i < 64; ++i) file_handles[i] = -1;
    u16 code_selector;
    code_selector = 0x2f;
    for (u32 i = 0; i < 256; ++i) {
        protected_vectors[i].offset = IMAGE_SIZE + 0x100;
        protected_vectors[i].selector = code_selector;
    }
    for (u32 i = 0; i < 17; ++i) {
        exception_vectors[i].offset = IMAGE_SIZE + 0x100;
        exception_vectors[i].selector = code_selector;
    }
    /* Linux user_desc flags: seg_32bit=1, contents=expand-down(1), usable=1. */
    descriptor(0, GUEST_BASE, 0x7f, 0x43);
    descriptor(1, 0x02000000, 0xfffff, 0x51); /* PSP, 4 GiB limit as reference. */
    descriptor(2, 0x02001000, 0xfffff, 0x51); /* DOS environment. */
    descriptor(3, DOS_BASE, 0xfffff, 0x51);   /* Virtual DOS physical-memory window. */
    descriptor(4, 0x02002000, 0xffff, 0x40); /* 16-bit extender data placeholder. */
    descriptor(5, GUEST_BASE, 0xfffff, 0x55); /* 32-bit readable code segment. */
    struct stack stack = {(uptr)signal_stack, 0, sizeof(signal_stack)};
    if (syscall4(NR_ALTSTACK, (uptr)&stack, 0, 0, 0) < 0) finish(22);
    struct action action = {
#ifdef EF_NATIVE64
        (uptr)signal_entry,
#else
        (uptr)fault,
#endif
        0x0c000004, (uptr)signal_return, {0x2000, 0}};
    u32 signals[] = {4, 5, 7, 8, 10, 11, 12, 14};
    for (u32 i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i)
        if (syscall4(NR_ACTION, signals[i], (uptr)&action, 0, 8) < 0) finish(22);
    volatile u8 *psp = (volatile u8 *)0x02000000;
    psp[0x80] = 4;
    psp[0x81] = ' '; psp[0x82] = 'g'; psp[0x83] = 'u'; psp[0x84] = 'i'; psp[0x85] = 13;
    char const environment[] = "DID=.\0quick_flag=1\0\0\1\0C:\\EF2000\\_EF2000.EXE\0";
    for (u32 i = 0; i < sizeof(environment); ++i)
        *(volatile u8 *)((uptr)0x02001000 + i) = environment[i];
    *(volatile u16 *)guest(0x120cc4) = 0x1f;
    *(volatile u32 *)guest(0x120cc6) = DOS_BASE - GUEST_BASE;
    *(volatile u16 *)guest(0x132a24) = 0x27;
    /* The original allocator calculation runs against this declared synthetic
     * 64 MiB model. Keep two pages between initialized heap and stack growth.
     */
    allocation.heap_end = 0x4df000;
    allocation.stack_end = (INITIAL_SP - 0x126a) & 0xfffff000u;
    allocation.minimum_stack = allocation.heap_end + 0x2000;
    allocation.free_bytes = allocation.stack_end - allocation.minimum_stack;
    allocation.heap_ceiling = IMAGE_SIZE;
    /* Prove that ORIGINAL startup clears BSS rather than relying on ELF zeros. */
    for (u32 address = 0x195fb0; address < 0x4de2f0; ++address)
        *(volatile u8 *)guest(address) = 0xa5;
    text("STARTUP synthetic-memory-model\n", 31);
    *(u8 *)(uptr)(DOS_BASE + 0x449) = 3;
    *(u16 *)(uptr)(DOS_BASE + 0x44a) = 80;
    *(u16 *)(uptr)(DOS_BASE + 0x463) = 0x3d4;
    runtime_ready = 1;
}
