<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 107: verify `to_upper_ascii`, isolate the encoder mismatch

## `to_upper_ascii` (`0xddbc8`, 14 bytes)

This is the second verified OpenWatcom replacement:

- The original body is `cmp eax,61h; jl; cmp eax,7ah; jg; sub eax,20h; ret`.
  OpenWatcom emits the same 14-byte body. Direct caller sequences at
  `0x27fec`, `0xa82f0` and `0xa9130` were reviewed; each passes a byte in EAX
  and consumes AL.
- `ef2000-wcc decomp/tests/ow_to_upper_ascii.c` completed without warnings.
  The SHA-bound OMF splice at `0xddbc8` passed 267 isolated guest cases.
- A 3-original/2-patched 500M-instruction main-menu replay passed, including
  a nonzero patch hit. It compared 67,108,850 of 67,108,864 guest bytes.
- The combined lower/upper main-menu (500M), quick-fight setup (700M), and
  cockpit (1.5B) suite passed. Both entries were hit in all three patched
  cases; status, interrupt counts, PIT ticks, IRQ0 counts, selected BSS hashes
  and framebuffer hashes matched the two original runs.

The tracker row for `to_upper_ascii` is `verified`.

## `encode_runtime_segment_offset` (`0x604`, 36 bytes)

The production C body remains `spliced`, but the stack mismatch is narrowed to
the OpenWatcom code-generation difference. The original body saves EBX and
EDX, computes the low nibble in EBX, combines with `ADD`, then restores both
registers. The current C-generated object saves only EDX and uses `OR`; the
isolated differential's default stack-scratch allowance hides this difference.
The full-memory replay still fails even with deterministic guest DOS and VGA
clocks, with final differences in high stack memory near
`0x3ffecc4..0x3ffecfa`.

A temporary, uncommitted `__declspec(naked)` assembly probe emitted the exact
reviewed 36-byte original body. Its 3-original/2-patched full-RAM startup
replay passed, comparing 67,108,828 bytes (the 36 replacement bytes excluded).
That confirms exact stack stores and instruction effects are sufficient to
pass the gate; it is not a C replacement and does not change tracker state.
Next work is to express the required EBX save and arithmetic effects through
OpenWatcom C code generation, or establish with captured caller evidence that
only the precise dead scratch writes differ. Do not exclude the whole stack.

## Replay tooling

The deterministic VGA retrace fix and `to_lower_ascii` verification are
recorded in batch 106. The long replay still hashes selected BSS rather than
all guest RAM; the 500M full-RAM menu replay supplies the full-memory evidence
for `to_upper_ascii`.

Run outputs and all patch objects remain under ignored `local/` and
`decomp/build/`. No proprietary assets or Ghidra output were added.
