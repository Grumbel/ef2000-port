<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 097: OPTIONS input motion and display event loop

Two substantial original TACTCOM routines have source-grounded C **host
drafts**. They are not OpenWatcom-compiled, guest-spliced or differentially
verified replacements. They follow the SHA-pinned disassembly; the local
Ghidra pseudo-C export is unavailable in this environment.

## Cursor and repeat timer `f3d54` (560 bytes)

`gui_cursor_step_f3d54(EAX=x_output, EDX=y_output, EBX=button_output)`
samples elapsed runtime ticks via the existing `f1065c` C implementation,
then polls an original indirect callback at `[15bab8]`. That callback is a
clearly named host adapter, as its ABI and effects are not yet recovered.

The function reads signed dwords at `420dc4` and `420dc8`, divides each by
eight *towards zero*, then narrows to signed 16 bits. Each axis is converted
independently to displacement:

| Absolute quotient | Displacement |
| --- | --- |
| Above 35 | ±5 |
| 21–35 | ±2 |
| 11–20 | ±1 |
| 1–10 | Decrement 32-bit repeat timer by elapsed ticks; ±1 only when the timer becomes negative, resetting it to 30 |
| 0 | No change; timer untouched |

The timer dwords are `161e24` and `161e28`. The resulting steps update
screen X/Y words `425fc8`/`425fc6` with 16-bit arithmetic, then clamp to
`[0,319]` and `[0,199]`, respectively. The original uses the high halves of
two **overlapping dword** reads to test those word values. The output words
are assigned the final positions; a third output word reflects whether
byte `15ba2c` is nonzero.

`tests/decomp_gui_cursor_097_test.c` covers all step boundaries and signs,
small negative fractional inputs, two-stage repeat timeout, backward guest
clock, both coordinate clamps and button state. It links the pre-existing
`advance_runtime_tick` host source, not a fake timer implementation.

## Display loop `f3f84` (497 bytes)

`gui_options_display_f3f84` is the body invoked for the OPTIONS `display`
command. It optionally applies a saved request, presents a status frame,
and converts one **20-byte parameter record** indexed by the high signed
word at `161e1c` into the original `dd9e4` register-and-stack argument
layout. A typed struct passes that layout to a host adapter; no renderer
parameter ABI has been invented.

The loop polls pointer status and optionally its second button source,
performs hit testing until a selection is made, handles animation, and
dispatches a clicked record through `f3aa8` then `f3b78`. After selection,
the original loop keeps running while bit 0 stays pressed and exits on
release. It can also terminate when byte `161e20` becomes nonzero. Screen
refresh uses `4859de`, while `4859dc` selects the fade or no-fade shutdown
path. The cursor snapshot words `1616ee`/`1616f0` gate redraw work.

Hardware polling, hit-testing (`f3714`), screen rendering, recursive OPTIONS
dispatch, `dd9e4` setup and the original `f3d54` stack-output call remain
**explicit host adapters**; these are not counted as newly recovered game
functions. In particular, recovering this loop does *not* mean the hit-test
algorithm, renderer, or joystick ABI is complete.

`tests/decomp_gui_display_097_test.c` covers signed record parameters,
optional startup, early exit, click/selection/release lifecycle, frozen
hover after selection, animation ticks, snapshot refresh, and fade/no-fade
cleanup. Neither test relies on real EF2000 assets.

## Evidence and limitations

- `tests/test_decomp_batch_097_payload.py`: SHA-256-pinned call counts,
  cursor movement thresholds, repeat timers, overlapping word addresses,
  display event dependencies and tracker entries.
- Strict C11 GCC host builds use `-Wall -Wextra -Werror -pedantic`.
- There are currently only one identified direct caller for each new
  routine (`f3f84` calls `f3d54`; `f3b78` calls `f3f84`); guest register
  clobbers and the true indirect polling callback must be verified before
  treating either interface as confirmed.
- The display model uses local host variables for the original stack locals;
  stack-memory equivalence and guest replay remain untested.

**Tracker after 097:** 4,287 functions; 3,257 `compiles`, 247 `drafted`,
669 `unknown`, 83 `named`, 28 `typed`, 3 `library`. No verified guest
replacements.
