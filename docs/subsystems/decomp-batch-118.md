<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 118: indexed callback flag

## `sync_flag_12ec8` (`0x12ec8`, 80 bytes)

The original reads the signed high word of dword `0x12287e`, computes the
record address `0x121fb4 + index * 29`, then mirrors that record's first byte
to `0x123790` when mode byte `0x122882` is zero. Otherwise it sets both bytes
to one. The body preserves EBX and EDX and has no stack arguments or external
calls. Its callback-table entry at slot zero is reached by
`reinitialize_runtime_slots` (`0x12160`).

A cockpit call capture records the real callback entry with EAX=0, EDX=0,
mode=1, and slot-zero flag=1. The Watcom branch uses volatile guest pointers;
the 52-byte production body equals the focused fixture exactly. The
SHA-bound in-place OMF patch spans the original 80-byte extent.

### Verification

- OpenWatcom 2.0 compiled `decomp/src/game_state_helpers.c` and
  `decomp/tests/ow_sync_flag_12ec8.c` with zero warnings.
- Five isolated cases cover both modes, indices zero and two, and signed index
  minus one. All five pass, comparing preserved registers and all modified
  bytes.
- The captured live callback passes differential (**1/1**).
- Repeated-baseline `replay_suite.py` passes all three cases: 500M main menu,
  700M quick-fight setup and 1.5B cockpit. The cockpit reaches `0x12ec8` once.
  All selected 64 KiB BSS and framebuffer hashes, final IP, status, interrupt,
  tick and IRQ0 counts match the original baselines.

The tracker state is `verified`.

## Parent `reinitialize_runtime_slots` (`0x12160`, 84 bytes) blocker

The captured cockpit table has 49 nonzero callback pointers among 54 slots.
The original sets EAX to the table's byte offset and EDX to the slot index
before its indirect callback. The current C wrapper passes the callback
address through a separate helper and does not model those live register
inputs. OpenWatcom emits a 124-byte body for the 84-byte extent and leaves
`guest_refresh_runtime_slot_` / `guest_invoke_runtime_slot_` unresolved for the
OMF splicer. Keep this parent function `blocked` until its callback ABI and
lowering are recovered. The reachable slot-zero callback was independently
verified above.
