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
`world_grid_queries.c` (`0x2beb0`, `0x2bf08`, `0x2dba4`, `0x2e910`)
contains two attached-object scans, a current/external-cell query and an
event-code relation predicate; `world_grid_control.c` (`0x2b558`, `0x2e030`)
connects cell links and resets two retained dwords from pending state.
`world_grid_links.c` (`0x2afcc`, `0x2b5c0`, `0x2b634`, `0x2b690`)
models link cleanup, an external-node back-reference and two ways of filling
the three inherited coordinate doubles. `world_grid_limits.c` (`0x2e08c`)
calculates a per-type bound and visits seven attached object limits, preserving
the filtered-lookup side effects and x87-style selection of unordered values.
The query scans deliberately preserve all filtered object lookups and their
diagnostic counter effects, including visits following a successful match.
OpenWatcom code size, guest
differential and replay verification are still pending for each. No game
functions have yet been verified as replacements.
