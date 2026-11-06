<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Handoff for Codex / GPT-6 Luna: working build first (2026-11-06)

Read `AGENTS.md` and `docs/decompilation-plan-rev2.md` first. Rev 2
**supersedes** the batch-by-batch verification loop from
`docs/handoffs/luna-decomp-2026-11-05.md` and master-plan §1 rules 5/6/9 and
§4. The old handoff is background only.

## Message to paste to Luna

> Change of plan. Stop the per-function verification loop; it verified 18 of
> 4,287 functions (0.1 % of bytes) and cannot finish the job. Read
> `docs/decompilation-plan-rev2.md` and
> `docs/handoffs/luna-hybrid-2026-11-06.md` and follow them.
>
> Goal: a runnable program built from recovered C. Imperfect is fine; we
> bisect later.
>
> 1. M1 first: build one hybrid image with *all* ~3,200 `compiles`
>    functions patched in (everything else stays original bytes), with no
>    extent constraint (use jump-to-new-code for bodies larger than their
>    slot). Write a single tool for this; do not splice per function. Get
>    it to the main menu under the replay harness. Exclude functions that
>    do not link cleanly (list in `hybrid-exclude.tsv`, 10 minutes max
>    each).
> 2. M2: menu -> quick-fight -> cockpit.
> 3. M3: write the automatic bisector (`tools/hybrid_bisect.py`) and use it
>    to find and fix or demote culprits.
> 4. M4: only then decompile the ~900 remaining functions, leaves first, at
>    draft quality, 30-100 functions per commit. No per-function tests, batch
>    docs or "docs: reference batch" commits; append to
>    `docs/subsystems/decomp-log.md`.
>
> Report after each milestone: functions/bytes patched, highest milestone
> passed, first failure. Do not re-audit existing verified work. Keep the
> legal rules from AGENTS.md (no Ghidra output, original bytes or assets in
> commits; never push; Xvfb only).
> If M1 turns out to be blocked by something structural (linker, segment
> layout, X-32 specifics), stop and write down exactly what blocks it
> instead of working around it function by function.

## State at handoff

- Tracker: 4,287 functions; `compiles` 3,224, `verified` 18, `spliced` 10,
  `blocked` 5, `drafted` 247, `unknown` 669.
- Test bed: menu/setup/cockpit replay with `--vga-clock guest`
  (batch 106); selected-BSS/framebuffer hashes and interrupt/tick counters
  are the comparison.
- Known suspects from earlier batches: `copy_record9_d0858` (BSS counter
  `g_filtered_object_lookup_calls` differs), `reinitialize_runtime_slots`
  (callback ABI unresolved), `latch_values_ce328`/`group_flag_negative_cc1ac`
  (too large for slot, no longer an issue under rev 2).
- Uncommitted working-tree edits from the previous Luna session may exist
  (`decomp/symbols/signatures.tsv`, `docs/compiler-abi.md`,
  `decomp/tests/0x0010d1da.json`, `decomp/tests/ow_find_last_char.c`);
  review and commit or discard them first.
