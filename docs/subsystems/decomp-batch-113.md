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
