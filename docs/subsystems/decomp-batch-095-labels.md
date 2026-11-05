<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 095 continuation: OPTIONS label table

Two original routines at `f4347` (367 bytes) and `f44b6` (224 bytes) manage
an OPTIONS file's label/value assignments. Their C models read and write
guest RAM; allocation and deallocation remain explicit host adapters.

## Storage layout and matching

- `[160a18]` is a 32-bit pointer to a null-terminated array of assignment
  string pointers. The string bytes remain at their original addresses.
- `[160a1c]` points to a contiguous array of byte ownership flags, one per
  assignment. The pointer table and flags share one allocation, with a
  variable gap between the null pointer and flag bytes.
- Labels are compared case-insensitively through their first `=`. The
  original `ddbc8` helper uppercases ASCII `a..z` only, and matching stops
  at the delimiter without inspecting the value.
- A matching search returns the **one-based** slot; a miss returns the
  **negative entry count**. Deleting a match returns zero.

`store_option_assignment` rejects null inputs and missing `=` with `-1`.
An empty value (`name=`) asks the lookup function to remove the label and
returns zero even for a missing label. A nonempty value replaces the
matching pointer or appends it. Initial allocation is nine bytes; for N
existing entries a growing allocation asks for `5*N+9` bytes. The new
ownership flag is zero.

The original deletion loop moves pointer entries including the terminating
null, conditionally calls `fa9d2` on an owned string, moves the old flag
array into the last pointer slot's former location and then shifts flags
left. It copies an extra trailing byte rather than explicitly zeroing the
last flag; the C draft retains this quirk. An earlier host model put the
flags one pointer slot too early; the x86 loop was rechecked and an
end-deletion regression test added before committing.

## Evidence and limitations

- `tests/decomp_gui_options_095_test.c` exercises creation, casefolded
  search, append, replace, deletion at either end, pointer/flag layout,
  optional owner release, absent flag storage and allocator failures.
- `tests/test_decomp_batch_095_payload.py` checks both new function
  boundaries, the `=` test, the original allocator/move/free callees and
  both table-global references against SHA-pinned original bytes.
- Original `fc795`, `e8651` and `fa9d2` are not reimplemented here; an
  injected guest allocator models the requested sizes and contents.
  A moving `e8651` realloc can invalidate the original old ownership-array
  pointer before the following guest `memmove`; memory lifetime/ABI needs
  guest differential replay before these bodies can be spliced.
- No executable bytes or Ghidra pseudo-C are included. These remain
  `state=drafted`, not verified guest replacements.

**Tracker after continuation:** 4,287 total, 3,257 `compiles`, 243
`drafted`, 84 `named`, no new guest-verified replacements. Batch 095
therefore contains five distinct original-function C drafts across two
commits.
