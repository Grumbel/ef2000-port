<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 106: deterministic VGA retrace and first verified OMF function

## Replay nondeterminism cause

The long replay's status/interrupt and BSS differences came from VGA input
status port `0x3da`. The C++ runner sampled host `steady_clock` to synthesize
vertical retrace, even with `--dos-clock guest`; a replay therefore still had
a host-time input. Two traced 500M-instruction originals first diverged at the
same DOS INT 21h call site after their VGA status polling had diverged.

`runtime/unicorn_cpp.cpp` now supports `--vga-clock guest`. It advances a
synthetic retrace phase by 100 microseconds for each sampled `0x3da` status
read. Host time remains the default for interactive runs. The replay suite
fixes guest DOS time, guest VGA retrace, keyboard controls and joystick off.
The fake-runner test checks all four options.

## `to_lower_ascii` (`0xe7835`, 14 bytes)

This is the first function promoted to `verified`:

- Original disassembly and all three direct callers were reviewed; the
  OpenWatcom function body matches the original 14 bytes exactly.
- `ef2000-wcc` compiled the production module without warnings. The
  SHA-bound OMF splice passed 263 isolated original-versus-replacement cases.
- `compare_guest_replay.py` passed three original and two patched 20M
  instruction startup runs, comparing 67,108,850 of 67,108,864 guest bytes.
- `replay_suite.py` passed three long cases: main menu at 500M instructions,
  quick-fight setup at 700M, and cockpit at 1.5B. For every case, two
  unpatched runs and one patched run matched stop state, software interrupt
  count, PIT ticks, IRQ0 count, selected 64 KiB BSS hash, and framebuffer
  hash. Patch hit counts were 460, 686 and 5,637 respectively.
- The long gameplay cases use a deterministic synthetic VGA retrace clock and
  hash selected BSS, not all guest RAM. The separate 20M startup replay is the
  full-RAM evidence; §3.9 remains a partial oracle overall.

The tracker row for `to_lower_ascii` is now `verified`.

## `encode_runtime_segment_offset` (`0x604`, 36 bytes)

No new encoder result: the 1,024-case isolated differential still passes, but
the 20M full-RAM startup replay still differs in high-stack RAM. It remains
`spliced`; do not add broad stack exclusions. Next inspect the captured caller
state and live stack writes around `0x3ffecc0..0x3ffed00`.

## Reproduction and remaining work

The long replay used the private SHA-pinned executable and payload listed in
batch 105, plus the ignored installation data under `local/ow-gamedata`.
Results are in `local/ow-lowercase-menu-cockpit-replay-004/summary.json`.
Assets, patches and run output remain ignored and uncommitted.

`to_lower_ascii` is now the only verified recovered game function. Continue
with the encoder stack mismatch, then choose the next leaf routine from the
plan's work order. Do not treat the synthetic replay clock as a model of
physical VGA timing.
