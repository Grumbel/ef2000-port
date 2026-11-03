<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: LicenseRef-EF2000-Derived -->

Recovered C modules belong here, using original source module names where
supported by evidence. `hiscore.c` (`0x432c`), `spatial.c` (`0x263e0`,
`0x263ec`) and `indexed_objects.c` (`0x6a1b4`) contain *drafted* functions with host-model
tests and original-disassembly/caller evidence. OpenWatcom code size, guest
differential and replay verification are still pending for each. No game
functions have yet been verified as replacements.
