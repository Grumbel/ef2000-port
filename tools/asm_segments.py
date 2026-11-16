# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Descriptor-derived segment access support for explicit-state translations.

The caller supplies a loaded cache descriptor, not a selector-to-table lookup.
Descriptor lookup, loads and privilege transitions remain separate operations.
"""

RUNTIME = r'''
typedef struct {
    uint32_t selector, base, limit, access, flags;
} ef_lift_segment;

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
