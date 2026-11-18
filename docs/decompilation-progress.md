<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation progress

One line per tranche; rules in `docs/decompilation-plan-rev5.md`. Format:
`YYYY-MM-DD  <stream>  <what>  <tracker delta>  <metrics delta>  <commit>`.
History before rev 5: `docs/decompilation-log-archive.md`.

2026-11-18  baseline  rev 5 starts  compiles=3461 blocked=772 (456 KB, 49.8 %) verified=30 unknown=16 compiles-bad=10 hybrid-candidates=3003 (384 KB)  literal=11893 computed=13854 unnamed=4203 named_globals=568 struct_headers=14  -
2026-11-19  A1  add the six-case private Xvfb hybrid regression command and native bisector oracle  no tracker change  no readability change  :/^tools: add the native hybrid regression suite$
2026-11-19  A2  restore Multiplayer by correcting the decoded-key latch ABI and confirm Mitarbeiter  tracker counts unchanged  unnamed -3  :/^decomp: restore the Multiplayer latch ABI$
