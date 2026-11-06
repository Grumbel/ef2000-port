<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation plan, revision 2: working build first (2026-11-06)

**This revision supersedes `docs/decompilation-plan.md` §1 rules 5, 6 and 9,
§4 (operating loop) and the "start only when everything is verified"
precondition of §7.** Everything else in the master plan (legal rules,
ABI hypotheses, data formats, Phase 5 goals) stays in force, as do
`AGENTS.md` and the licensing rules.

## Why the change

After batches 1-139 the tracker shows 18 of 4,287 functions `verified`
(0.1 % of bytes) while 3,224 functions (40 %) sit at `compiles`. The
per-function loop (byte-exact splice into the original extent, captured
caller state, full-RAM replay, one batch document and one docs commit per
function) costs 15-30 minutes per function and cannot finish the job.
There is also still no build that runs a single line of recovered code
beyond a few spliced leaf functions.

The goal is now: **a runnable program built from recovered C, imperfect but
working, then made correct by bisection.** Perfection per function is no
longer a gate; a running game is.

## Milestones (in order)

### M1. Hybrid image boots  [ ]

Link every `compiles` function into one image in which every function that
is not yet recovered keeps its original bytes at its original address.

- No extent constraint. A recovered body larger than the original slot is
  fine: place it in a new code segment/area and turn the original entry into
  a jump to it (5-byte `jmp rel32`; where the original is shorter than 5
  bytes, use the trampoline-island approach, or leave it original and record
  why). Callers keep their original addresses.
- Prefer one patch set over many: `tools/omf_splice.py` / `omf_inplace.py`
  should accept a whole list of objects and emit one image. Extend them or
  write `tools/hybrid_link.py`; do not hand-splice per function.
- Calling conventions: functions use the confirmed ABI from
  `signatures.tsv`/`ef_abi.h`. Functions that do not compile cleanly under
  that ABI are left original and listed in `decomp/symbols/hybrid-exclude.tsv`
  with a one-line reason. Do not spend more than 10 minutes on any one.
- Acceptance: the hybrid image, with the full `compiles` set patched in,
  reaches the main menu under the existing replay harness (native64 or
  Unicorn) with no crash. Report: number of functions patched, bytes
  patched, first failure if any.

### M2. Quick-fight reaches the cockpit  [ ]

Same image, menu -> quick-fight setup -> cockpit using the existing
menu/setup/cockpit replay (`--vga-clock guest`). Frame content may differ at
this stage; a crash, hang or divergent control flow is what gets bisected.

### M3. Bisection tooling, then convergence  [ ]

- `tools/hybrid_bisect.py`: given a failing replay, toggle functions between
  recovered and original bytes (binary search over the patch set, ordered by
  address or by call-graph layer) and print the minimal culprit set.
  Automatic and unattended; a run must take minutes.
- Each culprit is fixed (preferred), or demoted to `compiles-bad` in the
  tracker and excluded, with the observed symptom in `notes`.
- Exit: full `compiles` set patched, menu->cockpit replay passes with
  matching framebuffer hash and interrupt/tick counters, or each remaining
  difference is listed with a named culprit.

### M4. Recover the rest  [ ]

Decompile the remaining ~900 functions (`unknown` 669, `drafted` 247,
`named`/`typed`), and keep going in the same hybrid image.

- Order by call-graph layer: leaves first, so callers' draft C is easier.
  Use `decomp_status.py next` ordering.
- **Draft quality is enough.** Write C from disassembly + pseudo-C, make it
  compile with zero warnings, add it to the hybrid image. Do not write a
  per-function differential test, batch document or replay.
- Work in **tranches of 30-100 functions per commit**. After each tranche,
  run the M2 replay; on failure, run the bisector, fix or demote, commit.
- Each tranche must raise `recovered bytes` (see below) by at least a few
  percent. If a tranche does not, stop and report why.

### M5. Verify in bulk, replace the extern bindings  [ ]

Only after M1-M4: promote functions to `verified` in bulk using the
replay-reachability and bulk-differential tooling (`func_differential.py`
driven over all functions in one run, inputs from captured callers). Then
continue with master plan §7 (real data definitions, `wmake` build, DPMI)
and §8.

## Redefined measures

Report these in `decomp/README.md` and the progress log:

- **recovered bytes**: bytes of functions present in the hybrid image as
  compiled recovered C (states `compiles` and above, minus `compiles-bad`).
- **verified bytes**: as before (strict).
- **image status**: highest milestone the current hybrid image passes.

`decomp_status.py summary` should print all three. Add the `compiles-bad`
state if needed.

## Revised ground rules (replace master-plan §1 rules 5, 6, 9; §4)

5'. "Recovered" means: compiles with OpenWatcom with zero warnings and is
    part of the hybrid image. "Verified" is a later promotion (M5).
6'. Commits are per tranche (up to ~100 functions) or per tooling change,
    never per function. No separate "docs: reference batch N" commits;
    keep one running log at `docs/subsystems/decomp-log.md` (append only) and
    update the tracker in the same commit as the code.
9'. Time-box is 10 minutes per function in M1-M4. Over budget -> exclude or
    demote with a note and move on. Do not build new infrastructure for one
    function; do not re-audit already-committed functions.

Unchanged and still mandatory: no Ghidra pseudo-C, original bytes or assets
in commits; recovered source only under `decomp/`; original bugs kept;
behavior over readability; trailers/authorship per `AGENTS.md`; UI tests only
on private Xvfb; never push.

## Existing work: what to keep

- The 18 `verified` functions stay `verified` and go into the hybrid image
  like the rest.
- Open `spliced`/`blocked` entries are re-examined only by M1 rules: if the
  recovered body is merely too big for its slot, the extent constraint no
  longer blocks it.
- Replay baseline work (`--vga-clock guest`, repeated-baseline suite) is the
  M2/M3 test bed. Keep using it.
