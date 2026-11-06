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

**Batch-111 update (2026-11-06):** `read_runtime_slot_flag` (`0x121cc`, 33
bytes) is now the fourth strictly verified OpenWatcom replacement. Its
OpenWatcom body fits the original extent; 380 isolated cases and three
captured gameplay calls pass. The repeated-baseline menu/setup/cockpit replay
suite passes, with three hits at `0x121cc` in cockpit and matching selected
BSS/framebuffer hashes. Tracker totals: 3,253 `compiles`, 4 `verified`,
247 `drafted`, 669 `unknown`, and 3 `library`. See
`docs/subsystems/decomp-batch-111.md`.

**Batch-112 update (2026-11-06):** `initialize_runtime_probe_latch`
(`0x121f0`, 29 bytes) is the fifth strictly verified OpenWatcom replacement.
The in-place OMF patch is byte-for-byte identical to the original, including
the resolved call to `mouse_read_motion_ce02c` at `0xce02c`. Eight isolated
cases and repeated-baseline menu/setup/cockpit replays pass; the cockpit run
hits this entry once. The captured-state differential reaches the real-mode
mouse interrupt path and stops at `POP ES` (`0x972`), which the isolated
Unicorn function runner cannot model. Current tracker totals: 3,252
`compiles`, 5 `verified`, 247 `drafted`, 669 `unknown`, and 3 `library`.
See `docs/subsystems/decomp-batch-112.md`.


**Batch-113 update (2026-11-06):** `copy_record9_d0858` (`0xd0858`, 55
bytes) compiles to a 45-byte OpenWatcom C loop preserving the original dword
access order. Eight isolated cases and one captured actual call pass. Its
1.5B cockpit replay reaches the candidate 42 times; framebuffer, status,
interrupts and ticks match, but the selected 64 KiB BSS hash differs, so it
remains `spliced`, not verified. `latch_values_ce328` is now `blocked`: the
straightforward C lowering is 61 bytes for a 44-byte extent. Current tracker:
3,250 `compiles`, 5 `verified`, 1 `spliced`, 1 `blocked`, 247 `drafted`, 669
`unknown`, and 3 `library`. See `docs/subsystems/decomp-batch-113.md`.
**Batch-114 update (2026-11-06):** Original disassembly and three callers
confirm `group_flag_negative_cc1ac` (`0xcc1ac`, 61 bytes): AL selects the
node group, EBX points to the list head, EDX supplies the index, and EAX
returns the sign predicate. The original masks EDX and does not preserve it;
the signature row is corrected. An inline OpenWatcom C lowering compiles
without warnings but is 72 bytes, and the next function starts at `0xcc1ec`,
so it remains `blocked`. Current tracker: 3,249 `compiles`, 5 `verified`, 1
`spliced`, 2 `blocked`, 247 `drafted`, 669 `unknown`, and 3 `library`. See
`docs/subsystems/decomp-batch-114.md`.

**Batch-115 update (2026-11-06):** `init_bounds_d0b68` (`0xd0b68`, 63
bytes) is now the sixth strictly verified OpenWatcom replacement. The
production body and fixture both equal the original 63 bytes. Twelve
isolated cases, three captured caller-state differentials, and the repeated
baseline 1.5B cockpit replay pass; the candidate is reached 155 times with
matching selected BSS/framebuffer hashes and status/tick/interrupt counters.
Current tracker: 3,248 `compiles`, 6 `verified`, 1 `spliced`, 2 `blocked`, 247
`drafted`, 669 `unknown`, and 3 `library`. See
`docs/subsystems/decomp-batch-115.md`.

**Batch-116 update (2026-11-06):** `find_node_by_byte_b` (`0xcc26c`, 25
bytes) has a 21-byte OpenWatcom C body and passes eight isolated linked-list
cases; the safe patch fills the original 25-byte extent. Unpatched entry
captures during 500M main-menu, 700M quick-fight setup and 1.5B cockpit runs
did not reach it. It remains `spliced`, not verified, until an actual caller
path is replayed. Current tracker: 3,247 `compiles`, 6 `verified`, 2
`spliced`, 2 `blocked`, 247 `drafted`, 669 `unknown`, and 3 `library`. See
`docs/subsystems/decomp-batch-116.md`.


**Batch-117 update (2026-11-06):** Re-audited `copy_record9_d0858`
(`0xd0858`). The 49-byte production OpenWatcom body now matches the fixture
and preserves original EAX and EFLAGS; eight isolated cases and one captured
live call pass. A correct full patch-set cockpit replay still differs in BSS
while matching framebuffer, stop point and interrupt/tick counters. The
exact changed field is `g_filtered_object_lookup_calls` at `0x1a3580`
(`0x8efe` original, `0x8ef9` patched); retain `spliced`, not `verified`.
The causal path remains unresolved; see
`docs/subsystems/decomp-batch-113.md`. Current tracker totals are unchanged:
3,247 `compiles`, 6 `verified`, 2 `spliced`, 2 `blocked`, 247 `drafted`,
669 `unknown`, and 3 `library`.


**Batch-118 update (2026-11-06):** `sync_flag_12ec8` (`0x12ec8`, 80
bytes) is the seventh strictly verified OpenWatcom replacement. Its 52-byte
production body matches the fixture; five isolated cases and one captured
callback differential pass. The repeated-baseline menu/setup/cockpit suite
passes with one cockpit hit and matching selected BSS/framebuffer and
execution counters. `reinitialize_runtime_slots` (`0x12160`) is now blocked:
49 of 54 callback slots are populated in the live capture, its current Watcom
body is 124 bytes for an 84-byte extent, and callback/helper ABI symbols are
unresolved for OMF splicing. See `docs/subsystems/decomp-batch-118.md`.
Current tracker: 3,245 `compiles`, 7 `verified`, 2 `spliced`, 3 `blocked`,
247 `drafted`, 669 `unknown`, and 3 `library`.


**Batch-119 update (2026-11-06):** `max3_signed` (`0x3cd4`, 27 bytes) has
a 19-byte OpenWatcom body matching its fixture and passes 253 isolated guest
cases. The first replay used a stale pre-nop encoder patch and was invalid.
With the current fixture-derived encoder patch, repeated menu/setup/cockpit
replay matches selected BSS/framebuffer and execution counters, but the
candidate entry hit count is zero. It remains `spliced`, not verified. A
separate original-code
audit found `0xa4470` (`mov al,3`) falls through into the x87 checked-square-
root body at `0xa4472`; both entries are blocked pending overlapping-entry
and x87 support. The `0xa4472` C model also omits the original AL=3 x87 stack
imbalance. See `docs/subsystems/decomp-batch-119.md`.
Current tracker: 3,242 `compiles`, 7 `verified`, 3 `spliced`, 5 `blocked`,
247 `drafted`, 669 `unknown`, and 3 `library`.


**Batch-120 update (2026-11-06):** `reset_unit_counter` (`0x5e658`, 12
bytes) is now the eighth verified OpenWatcom replacement. The 10-byte
production body matches the focused fixture and preserves all GPRs; 37
isolated cases and one captured actual call pass. Repeated-baseline
menu/setup/cockpit replay reaches the candidate 22 times in cockpit and
matches selected BSS/framebuffer, status, interrupt and tick state. Current
tracker: 3,241 `compiles`, 8 `verified`, 3 `spliced`, 5 `blocked`, 247
`drafted`, 669 `unknown`, and 3 `library`. See
`docs/subsystems/decomp-batch-120.md`.

**Batch-121 update (2026-11-06):** `set_runtime_phase_five` (`0x3dcc0`, 10
bytes) is now the ninth strictly verified OpenWatcom replacement. Its
production and fixture bodies match the original ten bytes; 37 isolated
cases and one captured cockpit call pass. Repeated-baseline
menu/setup/cockpit replay passes with one candidate hit in cockpit and
matching selected BSS/framebuffer and status, interrupt, tick and IRQ0
counters. See `docs/subsystems/decomp-batch-121.md`. Current tracker:
3,240 `compiles`, 9 `verified`, 3 `spliced`, 5 `blocked`, 247 `drafted`,
669 `unknown`, and 3 `library`.

**Batch-122 update (2026-11-06):** `store_runtime_control_word`
(`0x3cd64`, 6 bytes) is now the tenth strictly verified OpenWatcom
replacement. The OMF body matches the original `mov [0x1a35bc],eax; ret`;
37 isolated cases and two separately captured cockpit calls pass. The
repeated-baseline menu/setup/cockpit suite passes and reaches the candidate
2,435 times in cockpit. See `docs/subsystems/decomp-batch-122.md`. Current
tracker: 3,239 `compiles`, 10 `verified`, 3 `spliced`, 5 `blocked`, 247
`drafted`, 669 `unknown`, and 3 `library`.

**Batch-123 update (2026-11-06):** `initialize_mode_limits` (`0x58d24`, 29
bytes) is now the eleventh strictly verified OpenWatcom replacement. The
body preserves the original two word writes' order and instruction positions
while retaining all registers and flags; 37 isolated cases and one captured
cockpit call pass. The repeated-baseline menu/setup/cockpit suite passes with
one cockpit hit. See `docs/subsystems/decomp-batch-123.md`. Current tracker:
3,238 `compiles`, 11 `verified`, 3 `spliced`, 5 `blocked`, 247 `drafted`,
669 `unknown`, and 3 `library`.

**Batch-124 update (2026-11-06):** `copy_active_buffer_pointer`
(`0x7a428`, 11 bytes) is now the twelfth strictly verified OpenWatcom
replacement. Its absolute load/store body exactly matches the original;
37 isolated cases and one captured cockpit call pass. The repeated-baseline
menu/setup/cockpit suite passes and reaches the function 320,002 times in
cockpit. See `docs/subsystems/decomp-batch-124.md`. Current tracker:
3,237 `compiles`, 12 `verified`, 3 `spliced`, 5 `blocked`, 247 `drafted`,
669 `unknown`, and 3 `library`.

**Batch-125 update (2026-11-06):** `set_control_mode_a` (`0x7eea0`, 25
bytes) is now the thirteenth strictly verified OpenWatcom replacement. The
OMF body matches the original gate/select/store/return sequence byte for
byte; 37 isolated cases and two captured cockpit calls pass. The
repeated-baseline menu/setup/cockpit suite passes with one cockpit hit. See
`docs/subsystems/decomp-batch-125.md`. Current tracker: 3,236 `compiles`,
13 `verified`, 3 `spliced`, 5 `blocked`, 247 `drafted`, 669 `unknown`, and 3
`library`.

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
| `compiles` | 3,236 | 363,050 |
| `drafted` | 247 | 19,916 |
| `typed` / `named` | 111 | 35,109 |
| `unknown` | 669 | 489,650 |
| `library` | 3 | 509 |
| `verified` | 13 | 362 |
| `spliced` | 3 | 107 |
| `blocked` | 5 | 244 |
| **Total** | **4,287** | **908,947** |

**At the Batch-112 state:** five routines are strictly verified as
OpenWatcom replacements: the two ASCII case helpers, the segment encoder,
runtime slot flag lookup and runtime probe latch. The 669 unknown functions still hold
approximately 54% of inventoried code *bytes*, so entry counts exaggerate
completion.

## Read in this order, and do not reread the full 2,000-line progress log

1. `AGENTS.md` (commits, ownership, licensing, asset restrictions).
2. `docs/decompilation-plan.md` **§1, §2.2, §2.4, §4 and §11** (rules,
   correctness target, verification order and per-function loop).
3. `docs/subsystems/decomp-batch-125.md` (latest actual experiment and exact
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

Five OpenWatcom guest replacements now pass the strict §4 gate. Continue in
bounded 1–5-function clusters, following the plan's work order; do not resume
mass-drafting unknown functions. Run
`python3 tools/decomp_status.py next --module runtime_indexed`; if it reports
no eligible leaf because of unresolved dependencies, select a nearby small
function and resolve its guest-call boundary before compiling. Keep
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

**Batch-126 update (2026-11-06):** Both `set_control_mode_b` (`0x810d0`)
and C (`0x810f4`, 25 bytes each) are verified: 37 isolated cases each,
captured original calls pass, and repeated-baseline replays reach both with
matching selected BSS/frame/counters. The default `--sound off` menu/setup/
cockpit suite passes 3/3; a targeted `--sound sb16-dma` cockpit replay with
only the B/C candidates passes and hits B four times and C three times.
With the full patch set, `sb16-dma` services 40 additional DMA blocks and
differs in selected state, so that combination is not considered verified.
The replay suite now accepts `--sound` (default remains `off`) so gated
hardware-config paths can be exercised. Tracker totals: 3,234 `compiles`,
15 `verified`, 3 `spliced`, 5 `blocked`, 247 `drafted`, 669 `unknown`, and 3
`library`. See `docs/subsystems/decomp-batch-126.md`.

**Batch-127 update (2026-11-06):** `resolve_link_pointer` (`0x38658`, 26
bytes) is verified: the 23-byte OpenWatcom body fits in place; 10 isolated
cases, one captured live cockpit call, and the repeated-baseline menu/setup/
cockpit suite pass. It is hit 16 times in cockpit with matching selected
BSS/frame/counters. The OMF splicer now handles leading-underscore Watcom
data externals and absolute offsets framed against the zero-base FLAT group,
with synthetic regression tests. Tracker totals: 3,233 `compiles`, 16
`verified`, 3 `spliced`, 5 `blocked`, 247 `drafted`, 669 `unknown`, and 3
`library`. See `docs/subsystems/decomp-batch-127.md`.

**Batch-128 update (2026-11-06):** `refresh_link_pointer` (`0x3856c`, 23
bytes) is verified: its OpenWatcom body fits in place, two isolated cases and
one captured caller call pass, and repeated-baseline menu/setup/cockpit replay
passes with eight hits (the helper `resolve_link_pointer` is hit 16 times).
Selected BSS, framebuffer and execution counters match. Tracker totals:
3,232 `compiles`, 17 `verified`, 3 `spliced`, 5 `blocked`, 247 `drafted`, 669
`unknown`, and 3 `library`. See `docs/subsystems/decomp-batch-128.md`.

**Batch-129 update (2026-11-06):** `field_10dfe0` (`0x10dfe0`, 12 bytes)
compiles cleanly with OpenWatcom and splices in place. Five isolated cases
pass. It remains `spliced`: the captured live caller state sets
`g_runtime_segment_base` to `0x05000000`, outside the runner's 64 MiB map, so
the original function faults at `0x10dfe5` in the captured differential and
cannot yet pass replay verification. See `docs/subsystems/decomp-batch-129.md`.
Current tracker: 3,231 `compiles`, 17 `verified`, 4 `spliced`, 5 `blocked`,
247 `drafted`, 669 `unknown`, and 3 `library`.

**Batch-130 update (2026-11-06):** `get_state_106708` (`0x106708`, 13
bytes) was corrected to its original caller-clean cdecl stack ABI. Its
OpenWatcom body matches the original bytes; four isolated cases pass, and
the repeated menu/setup/cockpit suite passes 3/3 with matching checkpoints.
It remains `spliced` because none of those replays reaches the entry, and
500M startup / 1.5B cockpit capture probes found no call. The differential
harness now separates stack argument size from callee cleanup, and OMF
matching handles cdecl leading-underscore code publics. See
`docs/subsystems/decomp-batch-130.md`. Current tracker: 3,230 `compiles`, 17
`verified`, 5 `spliced`, 5 `blocked`, 247 `drafted`, 669 `unknown`, and 3
`library`.

**Batch-131 update (2026-11-06):** `set_state_1_106634` (`0x106634`, 16
bytes) now has a caller-clean cdecl signature and exact OpenWatcom in-place
body. A C behavior model uses an inline `#pragma aux` sequence because the
ordinary C lowering was 17 bytes. Four isolated cases pass; the repeated
menu/setup/cockpit suite passes 3/3, but the target is not hit and 500M
startup / 1.5B cockpit capture probes found no call. Keep it `spliced`.
See `docs/subsystems/decomp-batch-131.md`. Current tracker: 3,229
`compiles`, 17 `verified`, 6 `spliced`, 5 `blocked`, 247 `drafted`, 669
`unknown`, and 3 `library`.

**Batch-132 update (2026-11-06):** `set_field_10677c` (`0x10677c`, 16
bytes) and its trace wrapper `0x102824` were corrected to caller-clean stack
ABIs. OpenWatcom emits the exact original setter body; four isolated cases
and one captured sound-DMA call from `0x102824@0x102853` pass. Keep the
candidate `spliced`: the sound-DMA repeated original baselines differ by two
interrupts and the patched checkpoint RAM hash also differs; SB16-probe
stops at unsupported real-mode IN port `0xa1` before reaching it. See
`docs/subsystems/decomp-batch-132.md`. Current tracker: 3,228 `compiles`, 17
`verified`, 7 `spliced`, 5 `blocked`, 247 `drafted`, 669 `unknown`, and 3
`library`.

**Batch-133 update (2026-11-06):** `set_field_10682c` (`0x10682c`, 16
bytes) and trace wrapper `0x1028c0` were corrected to caller-clean stack
ABIs. The exact OpenWatcom body passes four isolated cases and one captured
sound-DMA call from `0x1028c0@0x1028ef`. The full sound-DMA replay reaches
`0x10682c` four times but cannot pass its gate: repeated original baselines
differ by 42 interrupts (42,314 vs. 42,272), and patched selected BSS differs.
Keep it `spliced`. See `docs/subsystems/decomp-batch-133.md`. Current tracker:
3,227 `compiles`, 17 `verified`, 8 `spliced`, 5 `blocked`, 247 `drafted`,
669 `unknown`, and 3 `library`.


**Batch-134 update (2026-11-06):** `times_100` (`0x106be8`, 18 bytes) now has a modulo-2^32 C model and OpenWatcom in-place patch. The emitted `imul eax,100; ret` preserves EDX; eight isolated cases pass. Repeated menu/setup/cockpit replay matches existing checkpoints but has zero calls to the candidate, whose callers are behind the virtual-I/O selector. Keep it `spliced` until a caller path is exercised. See `docs/subsystems/decomp-batch-134.md`. Current tracker: 3,226 `compiles`, 17 `verified`, 9 `spliced`, 5 `blocked`, 247 `drafted`, 669 `unknown`, and 3 `library`.
