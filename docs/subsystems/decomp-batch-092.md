<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 092: palette dispatch, cockpit banks, lightning and smoke rings

Four additional original, previously `named` READCOLS bodies are now
**host-tested drafts**. All four contain real control flow and indexed
state/data operations, not short JMP aliases. This extends batches
[090](decomp-batch-090.md) and [091](decomp-batch-091.md).

| Original entry | Name | Recovered host behavior |
| --- | --- | --- |
| `da5cc` (119 bytes) | `load_pal` | Scan name, traverse fourteen eight-byte `(name,callback)` records at `15bd40`, invoke matching callback; dispatch to fallback on unknown name |
| `da644` (258 bytes) | `load_cock_pal` | First palette loads base bank and copies `0x300` bytes; other indices under 12 choose a source bank of stride `0x4004` and destination bank of stride 768; count increments even on warning |
| `dac2c` (286 bytes) | `lightning_point` | Populate nine-byte indexed record: two words, heap string pointer, conditional flag byte; wrap an 8-bit count with warning on overflow |
| `daef4` (448 bytes) | `load_smoke` | Consume a configured number of filename/index lines; visit both allocated 110-byte record arrays per line, write per-slot values and `0xffff` marker; link each array into a circular list |

## A newly identified palette dispatch fallback

The original `load_pal` loop looks unbounded, but its data table reveals a
special final entry. At load time it stores its **temporary stack-buffer
address at `15bdb0`**, overwriting the last table key. The following
`0xa4380` comparison therefore compares the input string to itself and
selects the last callback (`dbf2c`) after fourteen failed comparisons.
`dbf2c` reports the error at `190480`. The host draft explicitly models
this fallback rather than reading beyond the known fourteen fixed keys.

## Record details

- The cockpit palette counter is a signed 16-bit value at `425bca`, which
  also overlaps the high half of `425bc8`. The initial palette writes two
  globals; later loads update one indexed global from the parsed value.
- Lightning records are packed at nine-byte stride from `421a48`. The
  filename allocator consumes `strlen(name)+1`, and the byte at offset
  `+8` retains the parsed flag only for values 1 or 2. The count at
  `425c87` wraps modulo 256 and emits a warning when it becomes zero.
- The smoke handler reads the count at `425ae4`, which is populated by
  `load_craters` (batch 091). For each input line, it performs lookups in
  every entry of both arrays at `425af4` and `425aec`, writes a 16-bit
  result at offset `+0x14 + 2*slot`, and marks offset `+0x12` with `ffff`.
  Each 110-byte entry's `+0x6a` pointer links to the next record; the last
  points back to the first. Original count-zero handling still writes to
  `base-4`; these host tests deliberately use nonempty allocations.

## Validation and remaining uncertainty

- `tests/decomp_environment_092_test.c` tests all four draft bodies using
  synthetic guest RAM and fake scanf/guest-engine adapters. It covers
  palette match and fallback, first/middle/overflow cockpit branches,
  lightning buffer and byte-count wrap, and two-ring traversal.
- `tests/test_decomp_batch_092_payload.py` checks the SHA-pinned original
  disassembly entry sizes, all four calls to the original scanner at
  `df26f`, relevant global addresses, and the exact original table's
  fallback pointer. No original bytes or proprietary assets enter git.
- The fake `env_scan_fields_090` produces converted outputs; the original
  Watcom scanf parameter widths and guest call ABIs remain unresolved.
  The `a4380` string comparison, `a4421` text transformation, `9ddfc`
  per-record name lookup, allocator, and palette file loading are guest
  adapters. For `load_pal`, the host model does not write a made-up guest
  stack pointer to `15bdb0`. For `lightning_point`, the name-transform
  result is not yet reflected in the copied heap text. Neither is a
  verified replacement for game code.
- The remaining `tmsun_flare` at `db4d4` parses six `%hd` fields into
  partially overlapping local offsets but then reads the upper half of
  several stack dwords and uses them as a sample-count/divisor for
  trigonometric table lookup. Static inspection alone cannot prove the
  original Watcom scanf clobber/layout or local initialization semantics;
  a guest stack/scanf trace is needed before drafting the trigonometric
  function. The unnamed function at `db32c` also calls two still-opaque
  direction helpers (`dc7bc`, `dc7e4`). Both remain untouched in the
  tracker instead of receiving speculative drafts.

As with prior batches, **host tests plus a pinned static-byte audit do not
constitute a differential guest pass, an OpenWatcom object, or a verified
splice**. The tracker now contains 4,287 functions, with 3,257 `compiles`,
226 `drafted`, and 96 still `named`; zero new native replacements were
verified.
