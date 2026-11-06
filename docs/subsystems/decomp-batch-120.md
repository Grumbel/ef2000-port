<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 120: reset unit counter

## `reset_unit_counter` (`0x5e658`, 12 bytes)

The original sequence is:

```text
push edx
xor edx,edx
mov word [0x12814e],dx
pop edx
ret
```

It clears only the word at `0x12814e`, preserves every GPR, and leaves the
flags produced by `xor edx,edx`. Four direct call sites were inspected:
`0x596c8`, `0x5993c`, `0x5e86c` and `0x5f637`. At those sites the next flag
consumer is preceded by flag-changing work: the first two call helpers that
set flags before branching, `0x5e86c` is followed by `add esp,4`, and `0x5f637`
is followed by `xor edx,edx` before a conditional. The original flags are
therefore not live across this call.

The OpenWatcom branch uses a private `#pragma aux` direct absolute-word store
inside the C function. Its 10-byte production body matches the focused fixture
and fits the 12-byte extent. It changes no GPRs and leaves flags unchanged;
the reviewed callers make that difference unobservable. The patch fills the
remaining two bytes with INT3 padding after RET.

### Differential and replay evidence

- OpenWatcom 2.0 compiled both `decomp/src/runtime_control.c` and
  `decomp/tests/ow_reset_unit_counter.c` with zero warnings/errors.
- The SHA-bound 12-byte OMF patch passes **37/37** isolated guest cases. Each
  case checks all preserved GPRs and the complete modified-memory span; the
  only writes are the target word at `0x12814e`.
- A live cockpit capture from `0x5e658` passes captured-state differential
  (**1/1**).
- The repeated-baseline 500M menu / 700M setup / 1.5B cockpit replay passes
  with the seven already verified OpenWatcom patches plus this candidate.
  The cockpit reaches `0x5e658` **22 times**. Selected 64 KiB BSS and
  framebuffer hashes, final IP, stop status, interrupt, tick and IRQ0 counts
  match their repeated original baselines.

The tracker state is `verified`.

## Replay patch artifact note

This batch rebuilt the encoder patch from the current
`decomp/tests/ow_encode_runtime_segment_offset.c`, including the two
flag-neutral nops required to preserve its verified instruction count. The
older ignored `local/patches/ow-encode.efpatch` predates those nops and is not
the verified encoder artifact; using it makes the cockpit BSS replay fail.
All game images, captures, patch objects and replay outputs remain under
ignored `local/` or `/tmp`.
