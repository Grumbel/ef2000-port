<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Handoff for Codex / GPT-6 Luna: mechanical bulk first (2026-11-06)

Read `AGENTS.md`, `docs/decompilation-plan-rev2.md` and
`docs/decompilation-plan-rev3.md`. They supersede the batch-by-batch loop.

## Message to paste to Luna

> Stop writing C function by function. Ghidra already decompiled all
> functions; your job is to make a script do the bulk and to fix what the
> script and compiler reject.
>
> 1. Run `python3 tools/ghidra_to_c.py --skip-recovered --compile` inside
>    the Nix dev shell (OpenWatcom `wcc386`; pass `--compiler` if needed).
>    The compile path is untested; fix the script first. Output goes to
>    `local/mechanical/` and must never be committed.
> 2. Read `local/mechanical/report.tsv`. Group failures by first error and
>    by `manual` cause. Add one script rule at a time, re-run, and note the
>    ok-rate (functions and bytes) in `docs/subsystems/decomp-log.md`.
>    Priorities: return registers, `extraout_*`, `CONCATn`, `_N_M_`
>    sub-fields, `DAT_x` widths from `globals.tsv`.
> 3. Then build the hybrid image (rev 2 M1) from hand-written `compiles`
>    bodies plus mechanical `ok` bodies (hand-written wins), everything else
>    original bytes, no size limit per body, one tool (`tools/hybrid_link.py`),
>    not per-function splices. Reach the menu, then the cockpit (M2). Use the
>    bisector (M3, `tools/hybrid_bisect.py`) on any failure.
> 4. Hand-edit only what scripts cannot do: bisector culprits first, then
>    renderer and flight/sim subsystems, committed under `decomp/src`.
>
> Rules: commit per rule or tranche, not per function; no batch documents
> or "docs: reference batch" commits; 10 minutes max per function; no Ghidra
> output, original bytes or assets in commits; never push; Xvfb only.
> Report after each step: ok-rate, bytes patched, highest milestone, first
> failure. If something structural blocks M1 (linker, segments, X-32),
> stop and write down exactly what.

## State

- `tools/ghidra_to_c.py` exists and generates (first run, unrecovered only):
  287 of 1,022 functions ok to generate, 735 `manual`. Never compiled yet.
- Tracker: 4,287 functions; 3,224 `compiles`, 18 `verified`, 669 `unknown`.
- Test bed: menu/setup/cockpit replay with `--vga-clock guest`.
- The working tree may hold uncommitted edits from the previous Luna session
  (`decomp/src/runtime_helpers.c`, `functions.tsv`, test JSON,
  `docs/subsystems/decomp-batch-140.md`); review, commit or discard first.
