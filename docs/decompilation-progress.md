<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation progress

One line per tranche; rules in `docs/decompilation-plan-rev5.md`. Format:
`YYYY-MM-DD  <stream>  <what>  <tracker delta>  <metrics delta>  <commit>`.
History before rev 5: `docs/decompilation-log-archive.md`.

2026-11-18  baseline  rev 5 starts  compiles=3461 blocked=772 (456 KB, 49.8 %) verified=30 unknown=16 compiles-bad=10 hybrid-candidates=3003 (384 KB)  literal=11893 computed=13854 unnamed=4203 named_globals=568 struct_headers=14  -
2026-11-19  A1  add the six-case private Xvfb hybrid regression command and native bisector oracle  no tracker change  no readability change  :/^tools: add the native hybrid regression suite$
2026-11-19  A2  restore Multiplayer by correcting the decoded-key latch ABI and confirm Mitarbeiter  tracker counts unchanged  unnamed -3  :/^decomp: restore the Multiplayer latch ABI$
2026-11-19  B1  recover twenty GUI callback jumps as direct C calls  blocked -20 (100 bytes), compiles +20, hybrid-candidates +20  no readability change  :/^decomp: recover twenty GUI callback thunks$
2026-11-19  B1  recover seventeen GUI jumps, two quit callbacks and the debrief redraw route  blocked -20 (163 bytes), compiles +20, hybrid-candidates +20  literal -4, named globals +3  :/^decomp: recover more GUI callback routes$
2026-11-19  B1  finish the tracked GUI thunks and route nine mission callbacks  blocked -20 (376 bytes), compiles +20, hybrid-candidates +20  no readability change  :/^decomp: finish tracked GUI thunks and route mission callbacks$
2026-11-19  B1  recover twenty mission, GUI and environment callback routes  blocked -20 (1,088 bytes), compiles +20, hybrid-candidates +20  literal +2, computed -2  :/^decomp: replace the mission and GUI callback adapters$
2026-11-19  B1  recover twenty campaign, weapons and GUI script routes  blocked -20 (3,132 bytes), compiles +20, hybrid-candidates +20  literal +2, computed -7  :/^decomp: replace campaign, weapon and GUI script adapters$
2026-11-19  B1  convert twenty environment scanner callbacks from host adapters to direct C and game calls  blocked -20 (2,042 bytes), compiles +20, hybrid-candidates +20  literal -12, computed +4, unnamed +16  private Xvfb regression PASS 6/6; abi_check 0 return mismatches
2026-11-19  B1  replace the OPTIONS dispatcher guest-call adapter with direct recovered calls and link it into the hybrid  compiles +1 blocked -1 (476 bytes) hybrid-candidates +1  no readability change  private Xvfb regression PASS 6/6; abi_check 0 return mismatches; link 2,825 redirected
2026-11-19  B1  route the OPTIONS display draft through 19 confirmed recovered calls; retain a link exclusion for three unresolved pointer/cursor adapters  blocked -1 (497 bytes), compiles +1; hybrid-candidates unchanged by exclusion  no readability change  private Xvfb regression PASS 6/6 (2,825 redirected); gui_display module omitted for unresolved pointer adapter; abi_check 0 return mismatches
2026-11-19  B1  replace the elapsed-centiseconds tail thunk with a direct call to its recovered target  blocked -1 (5 bytes), compiles +1, hybrid-candidates +1  no readability change  private Xvfb regression PASS 6/6; abi_check 0 return mismatches; link 2,826 redirected
2026-11-19  B2  recover four renderer word helpers with EBX cursor/AX result ABI  blocked -4 (64 bytes), compiles +4, hybrid-candidates +4  no readability change  private Xvfb German regression PASS 5/5 (main menu, 100 s Quick Fight flight, campaign map, Mitarbeiter, Multiplayer); English Quick Combat unavailable (English data absent); module and repo-wide abi_check 0 ret mismatches; link 2,830 redirected, 1,515 excluded
