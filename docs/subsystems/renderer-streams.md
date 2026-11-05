<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Renderer stream entry points (German TACTCOM)

The SHA-pinned `payload.bin` contains seven related renderer entry points at
`0xc49c4..0xc4d43`. All start with `ENTER 0,0`, save ESI/EDI/EBP, take one
pointer from `[EBP+8]`, and end in `RET 4`. The original disassembly—not a
generic Watcom calling-convention assumption—establishes that the pointer is
a **stack argument** and the callee removes it. The recovered sources in
`decomp/src/renderer_streams.c` are **host-tested drafts**, not guest-spliced
replacements.

| Entry | Packed input | Draw target | Direct caller evidence |
| --- | --- | --- | --- |
| `0xc49c4` | count, `count` shade bytes, `count` XY word pairs | `0xd2584` | `0xd7b4d`, `0xd8209` |
| `0xc4a50` | count, `count` UV word pairs, `count` XY word pairs | `0xd2634` | `0x638e4`, `0xd7ca8` |
| `0xc4aeb` | same UV+XY layout, depth base increased by `0x8000` | `0xd2704` | `0x1f3a7`, `0xe4c83` |
| `0xc4b95` | same UV+XY layout | `0xd2c84` | `0x76233`, `0x7a142` |
| `0xc4c30` | same UV+XY layout | `0xd2d7c` | `0x63b5a` |
| `0xc4ccb` | shade-selector word, mode word, count, XY pairs | `0xd2500` | `0x52e18`, `0x52e30`, `0x52e5a` |
| `0xc4d43` | mode word, count, XY pairs | `0xd2500` | `0x20216`, `0x2058d` |

All modes reset the renderer's current state (`0x15918c`, `0x159084`,
`0x15907c`, `0x158ed8`) and save guest ESP to `0x159064`. The colour mode
expands shade bytes to dwords at `0x15977c` (each byte in bits 8–15), builds
48-byte-stride vertex indices at `0x159190`, selects mode 4 during the draw,
then restores mode 0. The textured modes shift each UV word by the low five
bits of `0x158f0c`, store the 16-bit results at `0x15997c`, and repeat the
first UV pair after the final pair. They call `0xc48dc` to load the XY words,
then `0xfc9f8` to update the selector before rendering. The index modes set
`0x159138` to the low byte of the mode word and use the same index sequence.

## Ghidra boundary gap

The current 4,277-entry Ghidra export recognizes `0xc4b95` ending immediately
before `0xc4c30`, then jumps to `0xc4d43`. Two **real** entry points in this
gap were missed:

- `0xc4c30..0xc4cca` (155 bytes): starts `ENTER`, ends `RET 4` at
  `0xc4cc8`, and is called directly from `0x63b5a`.
- `0xc4ccb..0xc4d42` (120 bytes): starts `ENTER`, ends `RET 4` at
  `0xc4d40`, and is called directly from `0x52e18` and several other sites.

The tracker now explicitly includes both addresses, bringing it to 4,279
rows, without changing the Ghidra baseline constant. A future Ghidra rebuild
must add a boundary-repair recipe for these entries and regenerate the
inventory; do not discard these manually reviewed rows during re-seeding.

## Validation and limits

`tests/decomp_renderer_streams_test.c` exercises the seven drafts against
synthetic guest RAM. It checks source offsets, repeated UV closing entry,
16-bit shift truncation, the seven dispatch variants, state resets and
the 48-byte index stride. The complete original payload was disassembled
with `objdump -D -b binary -m i386 -M intel`; no original bytes or pseudo-C
are included in this repository.

OpenWatcom, Ghidra and Unicorn were unavailable in the current environment.
Before moving the tracker to `compiles`/`verified`, this cluster needs:

1. OpenWatcom compilation and a stack-argument/`RET 4` splice shim.
2. An exact original saved-ESP value from inside the guest prologue; the host
   draft currently obtains it from `renderer_stream_saved_esp()`.
3. Explicit FS-segment effects after `0xfc9f8` and a check of all ABI outputs
   and preserved registers. `0xc4c30` has only one identified direct caller.
4. Guest differential cases and complete golden replay checks, including
   real stream records; zero count and overflowing counts need original-state
   evidence because the original loops have no safe zero guard.

No function in this cluster is asserted to be guest verified.
