# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Descriptor-derived segment access support for explicit-state translations.

The caller supplies a loaded cache descriptor, not a selector-to-table lookup.
The lookup helper validates a selected table window but does not install the
cache or set the descriptor's accessed bit. Those commits belong to the loader.
"""

RUNTIME = r'''
typedef struct {
    uint32_t selector, base, limit, access, flags;
} ef_lift_segment;

/* Look up a non-null selector in its already selected GDT/LDT window.
 * The caller supplies the current table base/limit and guest RAM, not a
 * saved descriptor. No output or RAM changes on rejection. The returned
 * access-byte address lets the loader commit the accessed bit together
 * with cache installation. Fault delivery and table selection are separate.
 */
static unsigned ef_segment_lookup(const uint8_t *memory, uint32_t memory_size,
                                  uint32_t table_base, uint32_t table_limit,
                                  uint32_t selector, uint32_t cpl, unsigned stack,
                                  ef_lift_segment *output, uint32_t *access_address) {
    uint32_t offset = selector & 0xfff8u, address, low, high;
    uint32_t type, dpl, rpl = selector & 3u;
    ef_lift_segment descriptor;
    if (selector > 0xffffu || !(selector & 0xfffcu) || cpl > 3 || stack > 1 ||
        offset > table_limit || table_limit - offset < 7u ||
        table_base > UINT32_MAX - offset) return 7;
    address = table_base + offset;
    if (memory_size < 8u || address > memory_size - 8u) return 7;
    if (selector > 0xffffu || (selector & 0xfffcu) == 0 || cpl > 3 || stack > 1 ||
        offset > table_limit || table_limit - offset < 7u ||
        table_base > UINT32_MAX - offset) return 7;
    address = table_base + offset;
    if (memory_size < 8 || address > memory_size - 8u) return 7;
    low = (uint32_t)memory[address] | (uint32_t)memory[address+1] << 8 |
          (uint32_t)memory[address+2] << 16 | (uint32_t)memory[address+3] << 24;
    high = (uint32_t)memory[address+4] | (uint32_t)memory[address+5] << 8 |
           (uint32_t)memory[address+6] << 16 | (uint32_t)memory[address+7] << 24;
    descriptor.selector = selector;
    descriptor.base = (low >> 16) | (high & 255u) << 16 | (high & 0xff000000u);
    descriptor.limit = (low & 0xffffu) | (high & 0xf0000u);
    descriptor.access = (high >> 8) & 255u;
    descriptor.flags = (high >> 20) & 15u;
    if (descriptor.flags & 8u) descriptor.limit = (descriptor.limit << 12) | 4095u;
    type = descriptor.access & 15u;
    dpl = (descriptor.access >> 5) & 3u;
    if ((descriptor.access & 0x90u) != 0x90u) return 7;
    if (stack) {
        if ((type & 8u) || !(type & 2u) || rpl != cpl || dpl != cpl) return 7;
    } else {
        if ((type & 8u) && !(type & 2u)) return 7;
        /* Readable conforming code has no DPL/RPL load restriction. */
        if (!(type & 8u) || !(type & 4u)) {
            if (dpl < cpl || dpl < rpl) return 7;
        }
    }
    descriptor.access |= 1u;
    *output = descriptor;
    *access_address = address + 5u;
    return 0;
}

/* access: descriptor access byte; flags: descriptor bits 52..55.
 * Return zero for a successful access. No RAM or CPU state is modified on
 * rejection. Bounds use inclusive effective limits and reject offset wrap.
 * kind: 0=data read, 1=data write, 2=instruction fetch, 3=stack read,
 * 4=stack write. Privilege checks are those for a loaded segment at CPL.
 */
static unsigned ef_segment_linear(const ef_lift_segment *seg, uint32_t cpl,
                                  uint32_t offset, uint32_t bytes,
                                  unsigned kind, uint32_t *linear) {
    uint32_t access = seg->access, type = access & 15u;
    uint32_t dpl = (access >> 5) & 3u, rpl = seg->selector & 3u;
    uint32_t last, maximum;
    unsigned code = (type & 8u) != 0, stack = kind == 3 || kind == 4;
    unsigned write = kind == 1 || kind == 4;
    if (kind > 4 || cpl > 3 || !bytes || (seg->selector & 0xfffcu) == 0 ||
        seg->selector > 0xffffu || access > 255u || seg->flags > 15u ||
        !(access & 0x80u) || !(access & 0x10u)) return 7;
    if (stack) {
        if (code || !(type & 2u) || dpl != cpl || rpl != cpl) return 7;
    } else if (kind == 2) {
        if (!code || rpl != cpl || ((type & 4u) ? dpl > cpl : dpl != cpl)) return 7;
    } else {
        if ((code && (write || !(type & 2u))) || (!code && write && !(type & 2u))) return 7;
        if (code && (type & 4u)) {
            if (dpl > cpl) return 7;
        } else if (dpl < cpl || dpl < rpl) return 7;
    }
    if (bytes - 1u > UINT32_MAX - offset) return 7;
    last = offset + bytes - 1u;
    if (!code && (type & 4u)) {
        maximum = (seg->flags & 4u) ? UINT32_MAX : 0xffffu;
        if (offset <= seg->limit || last > maximum) return 7;
    } else if (last > seg->limit) return 7;
    /* The 32-bit linear address wraps after adding the cached segment base. */
    *linear = seg->base + offset;
    return 0;
}
'''


def descriptor(selector, raw):
    """Decode a raw x86 descriptor into effective cached access fields."""
    if not 0 <= selector <= 0xffff or not 0 <= raw <= 0xffffffffffffffff:
        raise ValueError('invalid selector or descriptor width')
    base = ((raw >> 16) & 0xffff) | ((raw >> 32) & 255) << 16 | ((raw >> 56) & 255) << 24
    limit = (raw & 0xffff) | ((raw >> 48) & 15) << 16
    flags = (raw >> 52) & 15
    if flags & 8:
        limit = (limit << 12) | 4095
    return selector, base, limit, (raw >> 40) & 255, flags
