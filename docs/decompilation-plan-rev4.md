<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation plan, revision 4: finish the hybrid, then readable source (2026-11-10)

Extends rev 2/3 (milestones M0-M5, hybrid image, bisector). Ground rules,
legal rules and `AGENTS.md` are unchanged.

## Status (2026-11-10)

- Hybrid image: ~1,737 functions redirected; main menu and Quick Fight setup
  pass. **M2 (Accept -> cockpit): reached 2026-11-11** with ~1,707 redirects: the
  hybrid renders the full cockpit. Pixel-exact frame match fails (30 %) only
  because instruction-counted timing shifts game time by ~2 s; compare with a
  tolerance or a state hash, not the raw frame. Root cause of the long hunt:
  recovered C whose stack cleanup (`ret N`) or argument registers differ from
  the original; `hybrid_link` adds no trampoline for stack-argument functions.
  Gate new links with a static `ret`/clobber check against the original
  disassembly instead of bisecting.
- M0 mechanical: 442 `ok` / 578 `manual` of 1,020 (138 KB ok, 405 KB manual).
- Tracker: `compiles` 3,217, `unknown` 659 (480 KB, 53 % of bytes), rest small.
- Side track since 2026-11-09: GCC/Linux "standalone" startup probe
  (`runtime/decomp_startup.c`). It is valuable as the later native-port seed
  but advances ~1 probe step per commit. **Freeze it** until M2 passes.

## Order of work

1. **Stream A (critical path, strong model): M2.** Debug why Accept does not
   reach the cockpit with `hybrid_bisect.py`; fix or demote culprits. Exit:
   menu -> quick fight -> cockpit replay passes.
2. **Stream B (bounded, Haiku-suitable): draft the `unknown` functions.**
   Leaves first (`decomp_status.py next`), 30-60 functions per tranche, draft
   quality, compile clean with `ef2000-wcc`, tracker + `decomp-log.md` update
   in the same commit. No per-function tests or batch docs.
3. **Stream C: M0 rule additions** (`tools/ghidra_to_c.py`) for the commonest
   `manual` classes, each rule reported as an ok-rate delta.
4. **Stream D (after M2): readability pass.** Subsystem by subsystem
   (renderer, flight/sim first): named globals/structs instead of `EF_U32(...)`
   and `guest_fn_*` thunks, commit under `decomp/src`.
5. **Stream E (long term):** replace emulation host with the standalone
   native build, then match-bytes goals.

## Rules for delegated tranches (Stream B)

- After each tranche run the M2 replay (once it passes); if it regresses, run
  `hybrid_bisect.py`, then fix or mark the function `compiles-bad` in the
  tracker.
- Never invent ABI: take register/stack convention from `signatures.tsv` or
  the disassembly; unsure -> leave `drafted`, do not link.
- One commit per tranche, trailer per `AGENTS.md`, never push.

## Gate before linking new functions (2026-11-11)

Run `tools/abi_check.py --objects <decomp-objects> --wdis <wdis>` (about 3
minutes, needs the local Ghidra payload) after every tranche. Exit 1 means a
`ret N` differs from the original: add the address to
`decomp/symbols/hybrid-exclude.tsv` or fix the declaration (`#pragma aux` in
`decomp/include/ef_abi.h`) before replaying. x87 lines are informational.
Open: mixed register/stack functions (0x2d894, 0x106b0, 0x55b44) and
ST0-argument math helpers (cos/sin/tan/atan/atan2) still need conventions.
