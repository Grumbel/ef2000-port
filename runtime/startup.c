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
#else
#define GUEST_BASE 0u
#define NR_EXIT 1
#define NR_WRITE 4
#define NR_LDT 123
#define NR_ALTSTACK 186
#define NR_ACTION 174
#endif
#include "x32.h"
typedef unsigned long uptr;
typedef unsigned short u16;
typedef unsigned char u8;

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
_Static_assert(sizeof(struct context) == 88, "i386 sigcontext size");
_Static_assert(__builtin_offsetof(struct ucontext, registers) == 20, "i386 ucontext layout");

extern void signal_return(void);
static u8 signal_stack[32768];
static u32 service_count;
static struct x32_allocation allocation;

static uptr guest(u32 offset) { return (uptr)GUEST_BASE + offset; }

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
    for (;;) syscall4(NR_EXIT, code, 0, 0, 0);
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
    text(kind, length);
    field("IP", r->ip); field("AX", r->ax); field("BX", r->bx);
    field("CX", r->cx); field("DX", r->dx); field("SI", r->si);
    field("DI", r->di); field("SP", r->sp); field("FLAGS", r->flags);
    field("DS", r->ds); field("SS", r->ss); field("FS", r->fs); field("GS", r->gs);
    field("TRAP", r->trap);
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

static void guest_fault(u32 signal, struct context *r) {
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
    if (++service_count > 100) finish(23);
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
    } else {
        record("STOP ", 5, r);
        finish(20);
    }
    record("RETURN ", 7, r);
    r->ip += 2;
}

#ifdef EF_NATIVE64
static void fault(int signal, void *information, void *context) {
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

void setup(void) {
    /* Linux user_desc flags: seg_32bit=1, contents=expand-down(1), usable=1. */
    descriptor(0, GUEST_BASE, 0x7f, 0x43);
    descriptor(1, 0x02000000, 0xfffff, 0x51); /* PSP, 4 GiB limit as reference. */
    descriptor(2, 0x02001000, 0xfffff, 0x51); /* DOS environment. */
    descriptor(3, 0, 0xfffff, 0x51);          /* Zero-based linear access. */
    descriptor(4, 0x02002000, 0xffff, 0x40); /* 16-bit extender data placeholder. */
#ifdef EF_NATIVE64
    descriptor(5, GUEST_BASE, 0xfffff, 0x55); /* 32-bit readable code segment. */
#endif
    struct stack stack = {(uptr)signal_stack, 0, sizeof(signal_stack)};
    if (syscall4(NR_ALTSTACK, (uptr)&stack, 0, 0, 0) < 0) finish(22);
    struct action action = {(uptr)fault, 0x0c000004, (uptr)signal_return, {0, 0}};
    if (syscall4(NR_ACTION, 11, (uptr)&action, 0, 8) < 0) finish(22);
    volatile u8 *psp = (volatile u8 *)0x02000000;
    psp[0x80] = 4;
    psp[0x81] = ' '; psp[0x82] = 'g'; psp[0x83] = 'u'; psp[0x84] = 'i'; psp[0x85] = 13;
    char const environment[] = "DID=.\0quick_flag=1\0\0\1\0C:\\EF2000\\_EF2000.EXE\0";
    for (u32 i = 0; i < sizeof(environment); ++i)
        *(volatile u8 *)((uptr)0x02001000 + i) = environment[i];
    *(volatile u16 *)guest(0x120cc4) = 0x1f;
    *(volatile u32 *)guest(0x120cc6) = 0u - GUEST_BASE;
    *(volatile u16 *)guest(0x132a24) = 0x27;
    /* The original allocator calculation runs against this declared synthetic
     * 8 MiB model. Keep two pages between initialized heap and stack growth.
     */
    allocation.heap_end = 0x4df000;
    allocation.stack_end = (0x7ff000 - 0x126a) & 0xfffff000u;
    allocation.minimum_stack = allocation.heap_end + 0x2000;
    allocation.free_bytes = allocation.stack_end - allocation.minimum_stack;
    allocation.heap_ceiling = 0x800000;
    /* Prove that ORIGINAL startup clears BSS rather than relying on ELF zeros. */
    for (u32 address = 0x195fb0; address < 0x4de2f0; ++address)
        *(volatile u8 *)guest(address) = 0xa5;
    text("STARTUP synthetic-memory-model\n", 31);
}
