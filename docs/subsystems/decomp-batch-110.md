<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 110: verify the runtime segment encoder

## `encode_runtime_segment_offset` (`0x604`, 36 bytes)

The original disassembly reads the unaligned dword at `0x120cc6`, subtracts
it from EAX, packs the low four offset bits and the shifted 16-bit segment,
and finishes with `ADD`. It pushes EBX and EDX before the computation and
pops both before `RET`. Callers `0xb1f` and `0xddd8e` pass the pointer in EAX
and consume the encoded EAX result. The original `ADD` flags are retained by
the C expression using addition for the disjoint fields.

The first 33-byte C object executed 13 guest instructions while the original
executes 15. Its isolated differential passed, but deterministic gameplay
replays eventually sampled a different lookup count at `0x1a3580`; the final
low word was `0x8efe` in the original and `0x8f01` in that candidate. Hashes
at 300M, 600M, 900M, 1.2B and 1.35B instruction checkpoints matched; later
state sampled around the cockpit differed intermittently. The difference
came from the short candidate advancing the instruction-count-timed guest
past the same event boundary at a fixed budget.

Two flag-neutral, one-byte nops are now emitted through a private OpenWatcom
`#pragma aux` inline helper. The C body is 34 bytes, uses 15 guest
instructions, has no external calls or relocations, and safely fits the
reviewed 36-byte extent (the OMF patch pads the two bytes after `RET`). The
function preserves EBX by not modifying it and preserves EDX with the
compiler-generated push/pop. The original's two EBX stack stores are not
recreated; the isolated differential allows exactly eight bytes below the
incoming ESP as callee stack scratch under plan §2.2. The complete-RAM replay
then confirms no persistent difference in guest RAM.

### Evidence

- OpenWatcom 2.0 compiled both `decomp/src/runtime_indexed.c` and the focused
  `decomp/tests/ow_encode_runtime_segment_offset.c` with zero warnings.
- The SHA-bound 36-byte EFPATCH1 passes **1,024/1,024** isolated original-vs-C
  cases, comparing all EFLAGS. The only allowed memory difference is the
  exact 8-byte callee scratch below incoming ESP.
- `compare_guest_replay.py` passes **3 original + 2 patched** 20M startup
  runs. It compares **67,108,828 of 67,108,864** guest bytes; only the 36
  replacement bytes are excluded. Status and allocator summaries match.
- `replay_suite.py` passes all three repeated-baseline cases: main menu
  (500M), quick-fight setup (700M), and cockpit (1.5B). The 64 KiB BSS hashes,
  framebuffer hashes, status, interrupt/tick totals and final IP match in
  every original and patched run. Each patched case reports seven hits at
  `0x604`.
- The output patch built from the focused source and the patch compiled from
  the two-nop probe are byte-identical (`cmp`); the production module body
  has the same instructions at its emitted symbol.

The tracker is promoted from `spliced` to `verified`. The earlier replay
reports for the 13-instruction object remain useful historical evidence of
why instruction-count alignment matters; the current C object passes the full
gate without excluding guest globals or broad stack ranges.
