<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 105: first OpenWatcom OMF guest replacements

This batch exercised the first real OpenWatcom objects through the OMF
splicer against the SHA-pinned German TACTCOM executable. It also used the
installation's `ef2000.iso` to restore the missing `EF2000_1.DAT` and
`EF_HD_1.DAT` archives in an ignored disposable data directory. No original
asset or generated patch is committed.

## `to_lower_ascii` (`0xe7835`, 14 bytes)

- The original body is exactly `cmp eax,41h; jl; cmp eax,5ah; jg; add eax,20h;
  ret`. Its two comparisons are signed. The Watcom build of
  `decomp/src/math_string.c` emits the same 14 instructions and same bytes.
- Three direct callers were checked: `0x50d9c` (`call 0xe7835` at `0x50db5`),
  `0xa80b0` (call at `0xa80fe`), and `0xbc1d3` (call at `0xbc1e1`). They load
  a byte into zeroed EAX and consume AL as the converted byte. The callee
  returns EAX, leaves memory untouched, and preserves EBX/ECX/EDX/ESI/EDI/EBP.
- `ef2000-wcc decomp/src/math_string.c` completed with zero warnings. The
  focused OMF object has one 14-byte code public, zero externals and zero
  fixups. `ef2000-omf-inplace` produced a SHA-bound 14-byte patch at `0xe7835`.
- The isolated original-vs-OMF guest differential passed **263/263** inputs:
  all 256 low-byte values plus signed/full-register boundary values. It checked
  ABI outputs and preserved registers; neither side wrote guest memory.
- With the original full-data installation, the 20-million-instruction
  deterministic replay passed **3 original + 2 patched** runs. It compared
  **67,108,850 / 67,108,864** guest bytes (only the 14 replacement bytes
  excluded), and required a nonzero patch hit.
- A longer replay reached the German main menu, quick-fight setup and cockpit
  using two ENTER key events. The screen hashes matched between original and
  patched runs. The replay suite still rejects these checkpoints because
  repeated *unpatched* long runs differ in the interrupt counter and selected
  BSS hash, even with the guest DOS clock, keyboard-only controls and the host
  joystick disabled. This is a harness/game-startup determinism blocker, so
  the tracker remains `spliced`, not `verified`.

## `encode_runtime_segment_offset` (`0x604`, 36 bytes)

- The original loads the unaligned dword at `0x120cc6`, computes
  `delta = EAX - base`, returns `((delta >> 4) & 0xffff) << 16 | (delta & 15)`,
  and saves/restores EBX and EDX. Its direct callers are `0xb04` (call at
  `0xb1f`) and `0xddd48` (call at `0xddd8e`); both provide a guest pointer in
  EAX and consume the returned EAX.
- The Watcom-specific packed dword read compiles to one unaligned x86 load.
  The body is 33 bytes, with three safe padding bytes to the original extent.
  The standalone OMF has no externals or fixups; a SHA-bound in-place patch
  was produced at `0x604`.
- The isolated original-vs-OMF guest differential passed **1,024/1,024**
  varied pointer/base cases. A live captured call also returned with matching
  GPRs and EFLAGS. The isolated call's only memory differences were the
  expected saved-register words below incoming ESP (`ESP=0x3ffeec0`).
- The 20-million-instruction full-RAM replay with this patch does not pass:
  hashes differ in a small high-stack region around `0x3ffecc0..0x3ffed00`.
  The isolated captured call does not explain that final replay difference, so
  no stack exclusion is justified. Keep the state `spliced` and investigate
  the live stack/caller liveness before claiming verification.

## Replay harness finding

`tools/replay_suite.py` now fixes DOS date/time to the deterministic guest
clock, selects keyboard controls and disables the host joystick. These
options are covered by `tests/test_replay_suite.py`. They eliminate host
clock/device inputs, but do not yet stabilize the long original menu replay:
at 700 million instructions, two original runs differed in `interrupts`
(`32678` vs `32628`) and in the selected 64 KiB BSS hash while producing the
same framebuffer. The full menu-to-cockpit result therefore remains
unverified. `nix flake check -L` could not connect to the Nix daemon in this
environment; the installed OpenWatcom and Unicorn tools were used directly.

## Reproduction

The private inputs used here were `_EF2000.EXE` SHA-256
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd` and
payload SHA-256
`3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`.
OpenWatcom 2.0 compiled both production source modules without warnings.
The exact function test specs are `decomp/tests/0x000e7835.json` and
`decomp/tests/0x0000604.json`; their `ow_*.c` focused sources reproduce the
single-function OMF objects.

## Next action

Keep both replacements at `spliced`. First make the menu/cockpit baseline
repeatable (the long replay is not yet a valid oracle); then resolve the
encoder's final stack-region difference. Do not advance other candidates until
these causes are understood or recorded as specific blockers.
