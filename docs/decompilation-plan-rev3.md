<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation plan, revision 3: Ghidra does the bulk, the LLM does the cleanup

Extends `docs/decompilation-plan-rev2.md` (milestones M1-M5 stay) with a new
first step **M0** and a changed M4. Everything in rev 2 about the hybrid image,
the bisector, commit granularity and time-boxes still applies.

## Why

Ghidra already produced pseudo-C for all 4,277 functions. Rev 2 still had the
LLM write C for every function by hand. That is a slow, expensive decompiler.
Better: a deterministic script turns the Ghidra export into draft C, the
compiler and replay find the failures, and the LLM works only on those.

## Legal constraint (plan rule 2, unchanged)

Mechanically generated C is derived from Ghidra output. It lives only under
`local/mechanical/` (git-ignored) and is built at build time, like extracted
data tables. Only reviewed or rewritten code is committed under `decomp/src`.
Hand-written bodies in `decomp/src` always win over generated ones.

## M0. Mechanical bulk generation  [~]

Tool: `tools/ghidra_to_c.py` (committed; it contains no game code).

    python3 tools/ghidra_to_c.py --skip-recovered            # generate
    python3 tools/ghidra_to_c.py --skip-recovered --compile  # + wcc386, report

It writes `local/mechanical/src/ef_fn_<addr>.c`, a shared
`ef_gen_abi.h` (prototypes + `#pragma aux`), and `report.tsv`
(`generated` / `ok` / `compile-error` / `manual`, with the first error).

What it does: Ghidra types -> fixed-width types, `FUN_x` -> `ef_fn_x`,
`DAT_x` -> `EF_U32(0x..)`, string/pointer refs -> guest-memory pointers,
`in_EAX`/`in_AL`/... -> register parameters (from `signatures.tsv` when
confirmed, otherwise **inferred** from `in_*` uses and marked `/* inferred */`).

Known first-run numbers (unrecovered functions only, no compile yet):
287 of 1,022 generate cleanly (67 KB); 735 are `manual` (477 KB). The main
`manual` causes in the whole export are `CONCATn` (1,331 functions),
`extraout_*` (605+), `unaff_*` (315+), `swi` and `x.\_N_M_` sub-field
access. These are the register-pair / return-in-register cases that Ghidra
cannot express and that need the LLM (or better ABI knowledge).

Not yet done (in priority order):
1. Run with `--compile` in the Nix dev shell (OpenWatcom is not on the
   plain PATH; pass `--compiler` or set `WCC386`). The compile path is
   untested. Fix systematic errors in the script, not by hand.
2. Return registers: `value [..]` is only emitted for confirmed signatures.
   Infer from callers (what they read after the call) for the rest.
3. `extraout_*` -> use callee output registers once the callee's `value`
   pragma is known; `CONCATn(a, b)` -> `((a << k) | b)` patterns; `_N_M_`
   sub-field access -> shifts/masks. Each rule removes a whole `manual` class.
4. `DAT_x` width: take type/size from `globals.tsv`, default `EF_U32`.
5. Stack-argument functions: Ghidra's own parameters are used as-is.
   Verify against `signatures.tsv` `stack_bytes`; mismatches go to the
   exclude list.

Exit criterion: `report.tsv` has a stable ok-rate; each rule added raises
the number of `ok` functions. Report the rate after every rule.

## M1-M3 (rev 2, with one change)

M1 links the union of hand-written `compiles` bodies and M0 `ok` bodies
(hand-written wins). Everything else keeps its original bytes. Same
acceptance: boots to the menu, then cockpit (M2), then bisect (M3).

## M4. LLM cleanup, only where it pays

Replace "decompile the remaining ~900" with, in this order:
1. Functions that are `manual` because of a mechanical pattern: first try to
   add a script rule; only fall back to hand editing when it is a one-off.
2. Functions the bisector names as culprits.
3. Subsystems the port needs readable: renderer (wave 5), flight/sim
   (waves 2, 9). Rename, add structs, remove `EF_U32(...)` in favor of
   named globals, commit the cleaned result under `decomp/src`.
Everything else can stay mechanical (uncommitted, regenerated) for now.

## Measures

Add to the rev 2 measures: **mechanical ok-rate** (functions and bytes,
from `report.tsv`) and **hybrid coverage** (recovered + mechanical ok bytes
as a share of all game bytes).

## Rules unchanged

No Ghidra output, original bytes or assets in commits; never push; Xvfb
only; per-tranche commits; no per-function batch documents.
