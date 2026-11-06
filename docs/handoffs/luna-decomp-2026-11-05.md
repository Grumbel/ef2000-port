<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# EF2000 decompilation handoff for Codex / GPT-6 Luna (2026-11-05)

**Batch-104 code baseline: `e2c05d77`.** Start from the current `master`,
including the handoff commit after that baseline. This document is the
short operational entry point for a fresh Codex context. It is not a
replacement for the authoritative `AGENTS.md` and
`docs/decompilation-plan.md` rules.

**Batch-105 update (2026-11-06):** OpenWatcom 2.0 objects for
`to_lower_ascii` (`0xe7835`) and `encode_runtime_segment_offset` (`0x604`)
have been compiled, OMF-spliced and executed. Lowercase passes 263 isolated
cases plus a 3-original/2-patched 20M full-RAM startup replay. The encoder
passes 1,024 isolated cases, but its full-RAM replay differs in high-stack
memory. Full installation data is now available locally; a menu→quick-fight→
cockpit run was scripted, but long unpatched runs are not repeatable yet
(interrupt count and BSS hash vary). Both tracker entries are `spliced`, and
none is yet `verified`. See `docs/subsystems/decomp-batch-105.md`; stabilize
the long replay baseline before advancing either entry.

**Batch-106 update (2026-11-06):** The replay mismatch came from VGA retrace
reads sampling host `steady_clock`, which the DOS guest-clock option did not
control. `--vga-clock guest` now advances a synthetic retrace clock, and the
replay suite fixes it alongside DOS time and input. The menu, quick-fight
setup and cockpit suite now passes repeated original and patched runs, with
matching framebuffer/BSS hashes and interrupt counts. `to_lower_ascii` is
`verified`; `encode_runtime_segment_offset` remains `spliced` due its
high-stack full-RAM startup mismatch. See batch 106; gameplay hashes selected
guest ranges, while startup replay compares full RAM.

**Batch-107 update (2026-11-06):** `to_upper_ascii` (`0xddbc8`) is now the
second `verified` OpenWatcom replacement. Its 14-byte body matches the
original, 267 isolated cases pass, and a 3-original/2-patched 500M main-menu
replay matches 67,108,850 guest bytes with a nonzero entry hit. The combined
lower/upper menu→setup→cockpit suite also passes and hits both entries. The
encoder stays `spliced`; an exact 36-byte assembly probe passes full-RAM
replay, while the current C-generated body does not. See batch 107.

**Batch-110 update (2026-11-06):** `encode_runtime_segment_offset` (`0x604`)
is now the third `verified` OpenWatcom replacement. Two flag-neutral inline
nops restore the original instruction count and remove the late replay state
offset. Its 34-byte C body safely fits the 36-byte extent; 1,024 isolated
cases, 3+2 full-RAM startup runs and the repeated menu/setup/cockpit suite all
pass. The current tracker totals are 4,287 entries: 3,254 `compiles`, 247
`drafted`, 111 `typed`/`named`, 669 `unknown`, 3 `library` and 3 `verified`;
none remain `spliced`. See `docs/subsystems/decomp-batch-110.md`.

## Mission and boundaries

Recover EF2000's **original game logic in behavior-faithful C**, with the
original Watcom calling conventions, data layouts and quirks. Use the
original x86 game as an execution oracle. The eventual SDL3/native engine
is a later phase. Do not spend this decompilation session on renderer polish,
GUI redesign, emulation rewrites, or speculative C++ abstractions.

**A large volume of C-like source already exists, but that is not equivalent
to a decompiled working game.** Current tracker (`decomp/symbols/functions.tsv`):

| State | Entries | Original body bytes |
| --- | ---: | ---: |
| `compiles` | 3,254 | 363,699 |
| `drafted` | 247 | 19,916 |
| `typed` / `named` | 111 | 35,109 |
| `unknown` | 669 | 489,650 |
| `library` | 3 | 509 |
| `verified` | 3 | 64 |
| `spliced` | 0 | 0 |
| **Total** | **4,287** | **908,947** |

**At the Batch-110 state:** three routines are strictly verified as
OpenWatcom C replacements: the two ASCII case helpers and the segment
encoder. The 669 unknown functions still hold approximately 54% of
inventoried code *bytes*, so entry counts exaggerate completion.

## Read in this order, and do not reread the full 2,000-line progress log

1. `AGENTS.md` (commits, ownership, licensing, asset restrictions).
2. `docs/decompilation-plan.md` **§1, §2.2, §2.4, §4 and §11** (rules,
   correctness target, verification order and per-function loop).
3. `docs/subsystems/decomp-batch-110.md` (latest actual experiment and exact
   verification evidence); consult batch 104 for its historical startup
   survey and baseline experiments.
4. `docs/decompilation-status-2026-11-05.md` (the quantitative gap; written
   after batch 097, counts remain current at batch 104).
5. `docs/compiler-abi.md`, `docs/function-differential.md`,
   `docs/golden-replay.md` only when those details are needed.

Use `python3 tools/decomp_status.py summary` for current numbers; use `rg`
and address-targeted files to keep context small. Read Ghidra pseudo-C only
from the ignored `local/ghidra/` workspace, but trust original disassembly
over its inferred signatures. Never paste or commit generated pseudo-C.

## What batch 104 actually proved

- Locally linked **static Unicorn 2.1.4** needed `-Wl,--no-relax` to avoid a
  linker-induced recursive x87 memory helper. `tests/unicorn_x87_smoke.cpp`
  now catches it. The CMake/Nix recipes include the guard.
- `--timing instruction --dos-clock guest --sound off` gives repeatable
  pre-CD startup. The guest DOS clock starts at 2000-01-01 12:00 UTC.
- An identity-entry survey hit **121 of 4,287** tracked entries in a
  bounded 20-million-instruction pre-CD run: **43,161 calls** total. Main
  hotspots: `truncate_9d72c` (20,480), `sin_b45ac` (10,240),
  `reduce_angle_b45b6` (10,240), `step_generator_a45b3` (1,682).
- **Three GCC32 diagnostic C replacements** at `0x604`, `0xe7835`, and
  `0x9d749` executed 24 times in two real startup replays; three original
  replays and both patched replays matched in **67,108,783 of 67,108,864**
  guest RAM bytes, excluding exactly 81 replaced instruction bytes. Exit,
  timer and interrupt summaries also matched.
- This is **not** all-register/flag/x87 equivalence, a complete gameplay
  replay, or an OpenWatcom replacement. `copy_string_9d749` specifically
  differs from the original in scratch EDX on **264/512** isolated inputs;
  caller liveness is unresolved. `step_generator_a45b3` clobbers DX and AX
  according to the original; its GCC candidate is 65 bytes for a 49-byte
  original extent and **must not** be inserted as-is.
- Last tests: 388 Python tests passed (81 skipped); 67 CMake/CTest cases
  passed (one intentionally skipped). `nix flake check` could not be run on
  that host; rerun it on NixOS.

## Next assignment: continue the per-function loop

The first three OpenWatcom guest replacements now pass the strict §4 gate.
Continue in bounded 1–5-function clusters, following the plan's work order;
do not resume mass-drafting unknown functions. The next candidate reported
by `python3 tools/decomp_status.py next --module runtime_indexed` is
`read_runtime_slot_flag` at `0x121cc` (33 bytes). Start with its original
disassembly, input/output/clobber analysis and at least two callers. Keep
`copy_string_9d749` blocked pending EDX-liveness evidence and do not splice
`step_generator_a45b3` until its placement/ABI is solved. Promote functions
only after OpenWatcom compilation, safe splice, isolated differential and
appropriate golden replay all pass.

1. Confirm the clean ancestor chain and the private `_EF2000.EXE` SHA in
   `docs/executable.md`. Locate data/CD files on the owner's machine. Keep
   all private inputs, patched binaries, captures, and Ghidra exports out of
   Git (`local/` is ignored). If the CD data is unavailable, record that
   limitation and proceed with isolated function testing; do not fake a
   gameplay golden run.
2. On NixOS, run `nix flake check` once. Build/use the existing Unicorn C++
   runner and OpenWatcom wrappers. Prefer a narrow test after each function,
   the full check after a coherent cluster.
3. Start with `to_lower_ascii` at **`0xe7835`**, 14 bytes, already covered
   by 512 isolated GCC guest cases and 12 pre-CD live calls. Confirm its
   **original** inputs, return-register semantics and two real callers.
   Compile the existing C body via OpenWatcom, inspect the OMF symbol/reloc
   and ABI, and produce a SHA-bound patch with the existing OMF tooling.
   Do not assume a GCC diagnostic ELF patch can stand in for this step.
4. Run original-vs-patched **isolated guest comparisons**, including caller
   inputs, complete observed memory writes, output registers and preserved
   registers. Then run two unpatched deterministic startup baselines and
   the OMF-spliced variant. If data permits, build a **menu-to-cockpit
   deterministic replay** with inputs and checkpoints and compare those too.
5. Move to `encode_runtime_segment_offset` (`0x604`, 36 bytes, one live
   startup call) after the first replacement. Leave `copy_string_9d749`
   blocked on EDX-liveness proof; leave `step_generator_a45b3` unspliced
   until code placement/ABI is solved.
6. Only mark `state=verified` when the strict plan §4 gate passes: Watcom
   compilation, safe splice, function differential and appropriate golden
   replay. If the CD data is missing and full replay is impossible, record
   the partial result and **keep the original tracker state**.

**Success criterion:** at least one original C routine proven as a real
OpenWatcom guest replacement with reproducible evidence, or a tightly
documented blocker with a synthetic regression test and the next candidate
identified. Do not claim verified merely because C compiles or a final
memory digest matches.

## Reproducible commands and environment

```sh
git status --short
python3 tools/decomp_status.py summary
nix flake check -L
nix develop

# Private original DOS executable path supplied by owner:
export EF2000_EXE=/path/to/original/_EF2000.EXE

# Study exact 20-million-instruction pre-CD survey and 3+2 replay commands:
less docs/subsystems/decomp-batch-104.md
```

For game-data-backed runner commands, consult `README.md`,
`docs/golden-replay.md`, and `nix run .#unicorn-cpp -- --help`. Run GUI tests
under **private Xvfb**, never on the user's desktop display. If Ghidra
exports are missing, rebuild according to `docs/ghidra.md`; do not invent
decompilation evidence.

## Batch discipline and delivery

- Luna should work on *bounded* 1–5-function clusters with meaningful input
  variation, not 100 shallow C skeletons. Time-box a hard function to ~2 h;
  record specific blockers and move on.
- Favor source and ABI evidence over prose. Summarize per function: guest
  address/extent, callers, register interface, memory reads/writes, tests,
  guest replay evidence, precise remaining uncertainty.
- Commit small, verified units on `master`, author
  **Ingo Ruhnke <grumbel@gmail.com>**, with
  `Co-authored-by: Codex (GPT-6 Luna) <codex@openai.com>`.
- Never publish, force-push, rewrite history, or add proprietary assets.
  With local repository access, commit directly. In an isolated chat
  environment, deliver **only one cumulative numbered Git bundle**, built
  from base `0ea7b71392e2f24ba7bca4f749dceda997b16047` and containing
  the previous history. Bundle 104 ends at `e2c05d77`; the handoff is
  bundle 105. Number subsequent bundles monotonically and verify each with
  `git bundle verify`.
- At each batch end, update `docs/decompilation-plan.md` progress and any
  actual subsystem findings; report concrete test commands and skipped
  conditions. Do not rewrite the long historical progress log or repeatedly
  regenerate Ghidra if the existing project is usable.

The project-local `.codex/config.toml` selects `gpt-6-luna` with **medium**
reasoning and **low** verbosity. This is a default, not a guarantee of the
model offered by an account or Codex client. For unusual ABI, x87, indirect
control-flow or linker analysis, raise effort to **high** for the targeted
session; return to medium after. Sandbox and approval controls intentionally
remain user-owned.
