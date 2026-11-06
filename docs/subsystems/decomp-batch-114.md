<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 114: negative group-flag lookup boundary

## `group_flag_negative_cc1ac` (`0xcc1ac`, 61 bytes)

The original disassembly walks the linked list whose head pointer is at EBX,
comparing each node's byte `+0xb` with AL. On a match, EDX is masked to its
low byte; indices 9 and 10 return zero, otherwise the signed byte at
`*(node+0xc) + index` determines the EAX result. A missing group returns zero.
The three direct calls are `0xd68c` at `0xdb4b`, `0xe069`, and `0xe393`; each
sets EBX to the head, AL to the group, and EDX to the index, then tests AX.
The original pushes/restores ECX but modifies EDX on a matching path, so EDX
must not be listed as preserved in the confirmed signature.

A focused inline C implementation of the full linked walk compiles with zero
OpenWatcom warnings but occupies 72 bytes, 11 bytes beyond the original body.
The next function begins at `0xcc1ec`. The in-place patch planner routes the
oversized body to an executable arena, which the project has not yet safely
reserved, and refuses to create a patch. The function remains `blocked`.

Evidence: `objdump -b binary -m i386 -M intel -D` on the SHA-pinned
`local/initial-image/payload.bin`; OpenWatcom 2.0 fixture compile; OMF planner
rejection after temporarily validating the tracker state. No function
differential or replay is claimed for this blocked candidate.
