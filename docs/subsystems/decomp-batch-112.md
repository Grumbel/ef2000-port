<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 112: verify runtime probe latch initialization

## `initialize_runtime_probe_latch` (`0x121f0`, 29 bytes)

The original body preserves EDX, reserves eight stack bytes, calls the mouse
motion helper at `0xce02c` with local output pointers, writes word `0x96` to
`0x121ed0`, releases the scratch area and returns. Direct callers are
`0x5f60` at `0x5f66` and `0x5fd8` at `0x5fd9`; the confirmed contract has no
inputs or outputs and preserves EDX.

The Watcom C body uses two 16-bit local outputs, names the existing guest
helper `mouse_read_motion_ce02c`, and uses a small OpenWatcom inline helper
for the absolute word store. The OMF object contains a self-relative external
call framed against FLAT. `omf_inplace.py` now resolves this specific USE32
case from the reviewed guest address and the canonical function address. The
resulting 29-byte EFPATCH1 body is byte-for-byte identical to the original
payload bytes, including the call displacement.

### Evidence

- OpenWatcom 2.0 compiled the production module and focused fixture with zero
  warnings. The production and fixture function bodies have the same
  instructions and relocation placeholder.
- `omf_inplace.py` produced a single 29-byte in-place write at `0x121f0`.
  After resolving the external call, the patch bytes equal all 29 original
  bytes.
- The focused isolated differential passes **8/8** cases, varying preserved
  EDX and the existing latch word/tail bytes. It stubs the mouse helper at
  `0xce02c` with `RET`, so the comparison covers this function while leaving
  real-mode mouse services outside the isolated harness.
- A captured-state differential was attempted from an actual cockpit call.
  It stops at guest address `0x972` (`POP ES`) inside the real-mode mouse
  interrupt path, which the isolated function runner does not model. This is
  a harness limitation; the candidate patch is byte-identical to the
  original.
- The 20M full-RAM startup comparator does not reach this entry and correctly
  rejects the run for zero patch hits. The behaviorally relevant replays do
  reach it: the 500M main menu and 700M quick-fight setup cases pass with two
  repeated baselines, and the 1.5B cockpit case passes with two baselines and
  **one hit at `0x121f0`**. All use the four earlier verified patches too;
  selected 64 KiB BSS hashes, status/ticks/interrupts and frame hashes match.
- `PYTHONPATH=. pytest -q tests/test_omf_splice.py` passes **8 tests**,
  including a synthetic self-relative external call with a FLAT frame.

The function is promoted to `verified`: generated guest code is exactly the
original 29 bytes, its isolated state contract passes, and deterministic
gameplay replay reaches it without changing observed guest state. The
captured-state limitation and the startup-only coverage boundary remain
explicit above.
