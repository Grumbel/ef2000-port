<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation plan, revision 5: finish the source, make it readable (2026-11-18)

This revision sets the work order from 2026-11-18 on. It supersedes the order
of work in rev 4, the 2026-11-13 "assembly-focused continuation" note
(instruction matching is no longer a priority), and plan §1 rule 6 and §4
step 11 where they conflict with the rules below. The ground rules in plan §1,
the legal rules and `AGENTS.md` are unchanged. Where rev 2-4 and this file
disagree, this file wins.

## Goal

**Every game function as structured, readable C under `decomp/src`.** The C
must build into a working game without the original code bytes. "Readable"
means: named functions, named globals, struct types for records, and no raw
guest addresses outside a small, documented platform layer.

The hybrid image (original EXE + recovered C) remains the oracle and the
regression harness, but it is not the goal.

## Where we are (2026-11-18 baseline)

- Hybrid: ~2,700 C redirects. The English build reaches Quick Combat flight,
  and the German build creates a campaign. Menu crashes remain in Mitarbeiter
  and Multiplayer. In external view, terrain masks differ from the original by
  2.6 %.
- Tracker (`tools/decomp_status.py summary`): 4,309 functions.
  `compiles` 3,461 (453 KB, 49.5 %); **`blocked` 772 (456 KB, 49.8 %)**;
  `verified` 30; `unknown` 16; `compiles-bad` 10.
- Blocked by reason (bytes): x87 register stack 166 fn / 133 KB; segment
  registers, DOS/BIOS interrupts 136 / 128 KB; implicit-register asm style
  36 / 31 KB; indirect calls 37 / 29 KB; branches on callee flags 37 / 29 KB;
  extender/compiler runtime 48 / 21 KB; untriaged GUI 43 / 21 KB; guest-call
  routing drafts 88 / 11 KB; register thunks 50; smaller classes. Most of these
  were skipped during triage and were never actually attempted.
- Hybrid candidates (compiles or later, not in `hybrid-exclude.tsv`):
  3,003 functions, 384 KB (42.0 %). This is an upper bound; recent link
  reports redirect about 2,600-2,700.
- Readability (`tools/readability_metrics.py`): `literal` 11,893 raw
  global accesses, `computed` 13,854 raw pointer accesses, `unnamed` 4,203
  `guest_fn_*` calls, 568 named globals, 14 struct headers. `misc_drafts1.c`
  alone holds 880 functions and 38 % of the raw accesses.
- The instruction lifter (`tools/asm_c_program.py`, `asm_c_differential.py`,
  `ef_lift_*` output in `local/`) emits one `switch` case per x86
  instruction over emulated registers and flags. **That is an emulator, not a
  decompilation.** Its output can never become readable source and never
  counts toward coverage. From 2026-11-13 to 2026-11-18 most effort went into
  verifying it entry by entry. Over the same period the tracker gained only
  3 verified functions, and the plan log grew by 12,000 lines.

## Order of work

### Stream A: keep the oracle green (small, always first)

1. Script the hybrid regression set as **one command** that runs on private
   Xvfb and prints one PASS/FAIL line per scenario. Extend `hybrid_bisect.py`
   / `replay_suite.py`; do not write a new framework. Scenarios: main menu;
   Quick Fight → flight (native64, 100 s); English Quick Combat; both flight
   paths exercise throttle, rudder, and cockpit/external views. German
   Kampagne erstellen → campaign map with mission selection and overview refresh.
   Owner priority (2026-11-19): campaign interaction and actual flight;
   Mitarbeiter and Multiplayer are optional cases outside the default set.
2. Fix regressions in campaign interaction and flight with `hybrid_bisect.py`.
   A culprit that does not converge within the time-box (plan §1 rule 9) is
   demoted to `compiles-bad` and excluded.
3. Run the regression set and `tools/abi_check.py` after every tranche from
   streams B and C. A tranche that regresses is fixed or reverted before the
   next one starts.

### Stream B: decompile the blocked half (critical path)

Work through the blocked classes in this order. Use 20-50 functions per
tranche and one commit per tranche, with the tracker updated in the same
commit. Each class first gets a short written recipe: a header comment in its
module or one paragraph in `decomp/README.md`. After that, the functions are
mechanical applications of the recipe.

1. **Register thunks and guest-call routing drafts** (about 140 functions):
   rewrite them as direct C calls. These are easy and unblock callers.
2. **Branches on callee flags, implicit-register asm style, carry chains:**
   the callee returns the flag or the extra register as an explicit value
   (struct return or out-parameter in C). Use the existing `#pragma aux`
   mechanism in `ef_abi.h` so hybrid callers stay compatible.
3. **Indirect calls and indirect jumps:** recover the table as a typed
   array of function pointers (or a `switch`), named in `globals.tsv`.
4. **x87 register-stack code** (largest class): write ordinary `double` /
   `long double` C expressions in original evaluation order. OpenWatcom
   `-fpi87` keeps the x87 precision. Check each function with one
   `func_differential.py` spec of random inputs (`decomp/tests/<addr>.json`).
   Bit-exact results under the game's control word are the bar. Rule 3
   (behavior over readability) applies.
5. **Segment registers, DOS/BIOS interrupts, extender runtime:** these
   become the platform layer. Wrap each service in a small named C function
   (`dos_open`, `vga_set_mode`, `x32_alloc_selector`, ...) declared in one
   header under `decomp/include/`, with the hybrid implementation calling the
   original service. Game code calls only those wrappers. The native build
   later supplies its own implementation of the same header.
6. **Untriaged GUI and large functions:** ordinary decompilation per plan
   §4.

Ghidra pseudo-C, the disassembly and the lifter output may all be used as
**reading aids** (all stay in `local/`). Committed code is hand-written C
in the style of plan §4 step 5.

Exit: `blocked` + `unknown` < 2 % of bytes, and every `compiles` function is
linked in the hybrid or listed with a reason in `hybrid-exclude.tsv`.

### Stream C: readability (runs in parallel; takes over once B is done)

One module or subsystem per tranche, starting with the worst entries of
`readability_metrics.py --by-module`: `misc_drafts1`, `game_helpers130`,
then the renderer and flight/sim subsystems (see
`docs/subsystems/code-overview-2026-11-12.md`).

- Split catch-all files (`misc_drafts1.c`, `game_helpersNN.c`,
  `runtime_helpersNN.c`, ...) into subsystem modules. Moving code does not
  change behavior; keep moves and renames in separate commits.
- Every touched global gets a name and type in `globals.tsv` and an
  `extern` in a header. Replace `EF_U32(0x...)` with the name.
- Every repeated base+offset pattern becomes a struct in
  `decomp/include/ef_structs/` with offset asserts.
- Every touched `guest_fn_<addr>` call gets the callee's recovered name.
- Behavior gate: the module's existing `decomp/tests` specs and the Stream A
  regression set. Readability changes must not change compiled behavior.

Exit: `literal` and `unnamed` are zero outside the platform layer, and
`computed` falls below 10 % of its baseline.

### Stream D (later): native build without original code

When B exits, link all recovered C plus the platform layer natively (the
frozen GCC/Linux standalone startup probe, `runtime/decomp_startup.c`, is the
seed). The milestones are main menu, then flight, then a full mission, all
with no original code bytes executed.

## Frozen

- **The instruction lifter and its sweeps.** Do not add fixtures, sweeps,
  probes or documentation for it. Keep the tools and their tests working. A
  lifter fix is allowed only when a Stream B function needs the lifter as an
  oracle, and that fix is part of the Stream B tranche.
- Byte-matching and instruction-matching goals (rev 4 Stream E).
- The plan's progress log. The history is in
  `docs/decompilation-log-archive.md` and is read-only.

## Reporting rules

- Progress goes into `docs/decompilation-progress.md`, **one line per
  tranche**:
  `YYYY-MM-DD  <stream>  <what>  <tracker delta>  <metrics delta>  <commit>`.
  Do not commit hashes of files under `local/`. Do not write new
  `decomp-batch-*.md` documents or per-probe narratives. Record a design
  decision, such as a class recipe or a platform-layer interface, once in the
  relevant README or header.
- One commit per tranche or per fix. Do not create separate `docs:` commits
  that record evidence for an earlier commit; put the evidence in that
  commit's message.
- Every tranche commit body lists the milestone table, the hybrid
  candidate row and the readability line from `nix run .#status`
  (`tools/decomp_status.py status`), plus the Stream A regression result.
  Pass `--link-report` with the `hybrid_link.py --report` output when a
  tranche relinks the hybrid, so that the exact redirect count is recorded
  as well.
- If ten consecutive commits move none of these numbers, stop. Write one
  progress line that says why, and switch to the next item in the order
  above instead of building new infrastructure.
