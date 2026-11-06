<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 113: camera-record copy and placement findings

## `copy_record9_d0858` (`0xd0858`, 55 bytes)

The original copies nine dwords in this order: offsets `0, 12, 24, 4, 16,
28, 8, 20, 32`. The three direct callers are `0x91c98` at `0x91d77`,
`0x91df4` at `0x91e81`, and `0x91df4` at `0x91ee6`. They pass destination
in EAX and source in EDX. The observed caller ranges are non-overlapping; the
isolated tests additionally cover same-buffer and overlapping ranges. The
original returns with EAX still holding destination and EDX containing the
last source dword. Neither value is consumed after these calls: the first
caller’s outer function discards EAX, while the other two overwrite EAX
immediately; EDX is restored by the caller or overwritten before use.

The production Watcom branch expresses the unusual order as two nested loops
and uses volatile guest pointers to preserve each read/write order. Its object
body is 45 bytes and matches the focused fixture exactly. The safe splicer
pads the remaining ten bytes of the 55-byte original extent after the return.

### Evidence and unresolved replay result

- The SHA-pinned executable is
  `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
- OpenWatcom 2.0 compiled the production module and fixture with zero warnings.
- `omf_inplace.py` produced one in-place 55-byte patch at `0xd0858`.
- The isolated differential passes **8/8** cases, including overlap offsets
  `-32, -8, -4, 0, 4, 8, 32` and separate buffers. A captured live call from
  the 1.5B cockpit replay passes **1/1**, comparing all 36 copied bytes and
  the ABI-preserved registers.
- The repeated-baseline cockpit replay uses 1.5B instructions, PIT interval
  250,000, and key 13 down/up events at ticks 2100/2102 and 3100/3102. With
  all five previously verified patches plus this candidate, it reaches
  `0xd0858` **42 times**. Baselines repeat exactly; patched framebuffer,
  stop status, interrupts, PIT ticks and IRQ0 count match. The selected BSS
  range `0x195fb0..0x1a5faf` does **not** match: baseline SHA-256
  `93aec90671d6ae27204af5dbba82a1da835a7a4c20d27aa1fb38b4698df32ab7`,
  patched `cffc584b1b5b60a78b5bc2453fd4dc84bd943d55fc4e6dfa5926a56f52875cf3`.
  This is an unresolved behavior/replay failure; do not mark the function
  verified. The candidate adds loop instructions relative to the original,
  but timing has not been proven to be the cause.

The tracker state is `spliced` while the replay divergence remains unresolved.

## `latch_values_ce328` (`0xce328`, 44 bytes)

Original disassembly confirms the simple six-store/read body and three direct
call sites, but a straightforward OpenWatcom C fixture compiles to 61 bytes
and saves ESI. The next function begins at `0xce354`; the in-place splicer
correctly rejects the oversized object. The tracker is `blocked` pending a C
lowering that fits safely or an independently reviewed placement strategy.


## Batch 117 re-audit (2026-11-06)

The production C now explicitly preserves the original EAX destination return
and the incoming arithmetic flags. Two small OpenWatcom helpers emit
`PUSHFD`/`POPFD`; the nested C loops preserve the unusual dword order. The
production object body is 49 bytes (the next public begins after three
alignment bytes), and its first 49 bytes equal the focused fixture exactly.
The SHA-bound patch spans the 55-byte original extent. The ABI record now
includes EAX as an output, and the test spec checks EFLAGS mask `0x8d5`.

Eight synthetic cases and the saved live call pass, comparing EAX, preserved
registers, selected EFLAGS, and all 36 copied bytes. With all six verified
patches applied, the 1.5B cockpit replay reaches this function 42 times;
framebuffer SHA, final IP, stop status, interrupts, ticks, and IRQ0 count match
the repeated original baseline. The selected BSS hash still differs:
`93aec90671d6ae27204af5dbba82a1da835a7a4c20d27aa1fb38b4698df32ab7`
original versus
`d8521a5f3d08cf9cdcb9dc1768b4e1b3919d201f321d3da5a8ac7b7ed8e37903`
patched.

Address-sliced replay identified the changed region around `0x1a34b0`. A
follow-up hash of the exact four-byte `g_filtered_object_lookup_calls` global
at `0x1a3580` confirms its value changes from `0x8efe` to `0x8ef9`; this
lookup counter is incremented by `0x7489c` and is read for display at
`0x3d4b0`. This is a concrete guest-state difference, so it cannot be excluded as
incidental. It is consistent with
a timing-driven shift in later lookup calls, but the causal path is not yet
proven. Keep the candidate `spliced`, not `verified`.
