<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 116: splice linked-list byte lookup

## `find_node_by_byte_b` (`0xcc26c`, 25 bytes)

The original takes a key in AL and a list-head address in EDX. It preserves
EBX, returns the first node whose byte `+0xb` matches, or zero when no node
matches. Its direct callers are `0xb059c` at `0xb05c3` and `0xbed1c` at
`0xbed3a`.

The Watcom C implementation walks the linked list through volatile pointers.
It compiles to 21 bytes in the function's 25-byte extent. The original loop
uses `TEST/JNE` after each next-pointer load; the C lowering uses an
unconditional back edge and tests at the loop head. Both compare the same
nodes and return the same first match; the safe OMF patch places four NOPs
after `RET` to fill the reviewed extent.

### Evidence and remaining gate

- OpenWatcom 2.0 compiled the production module and focused fixture with zero
  warnings. Their 21-byte target bodies are identical.
- The SHA-bound OMF splice writes one 25-byte range at `0xcc26c`.
- `func_differential.py` passes **8/8** cases: empty list, first match/miss,
  second match, duplicate keys, zero and `0xff` keys, and a three-node miss.
- Original unpatched runner calls were captured with deterministic timing,
  keyboard controls and the standard key-13 menu/setup/cockpit script at
  500M, 700M and 1.5B instructions. None reached `0xcc26c`; no entry snapshot
  was produced. The function has not passed a patched gameplay replay and
  remains `spliced`, not verified.

Next action: find or script an actual caller path for `0xb059c` or `0xbed1c`,
then capture its entry state and run the patched replay.
