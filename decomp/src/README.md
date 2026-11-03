<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: LicenseRef-EF2000-Derived -->

Recovered C modules belong here, using original source module names where
supported by evidence. `hiscore.c` (`0x432c`), `spatial.c` (`0x263e0`,
`0x263ec`) and `indexed_objects.c` (`0x6a1b4`) contain *drafted* functions with host-model
tests and original-disassembly/caller evidence. The `object_lookup.c`
(`0x7489c`) and `object_state.c` (`0x6a354`, `0x6a378`, `0x6a394`,
`0x6a3d0`) cluster adds a filtered pointer lookup and four state mutators.
`world_grid.c` (`0x2b884`, `0x2b910`, `0x2b95c`, `0x2ba14`) recovers
four 202-byte world-grid operations; `object_control.c` (`0x2541c`,
`0x238a0`) adds object mode and relation synchronization wrappers.
OpenWatcom code size, guest
differential and replay verification are still pending for each. No game
functions have yet been verified as replacements.
