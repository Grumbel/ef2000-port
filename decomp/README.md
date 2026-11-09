<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: LicenseRef-EF2000-Derived -->

# Recovered EF2000 source

This directory holds game source code recovered by reverse engineering
EF2000, as described in [the decompilation plan](../docs/decompilation-plan.md).
It is **not** GPL. Everything here, including this README, is under the
[EF2000-Derived Source Notice](../LICENSES/LicenseRef-EF2000-Derived.txt):
the recovered logic is derived from the original program, whose rights
remain with its holders. Contributors' own additions may be reused under
that notice's terms.

The GPL host code (`runtime/`, `tools/`, and later the platform layer)
carries a GPL section 7 linking exception
([LICENSES/GPL-3.0-linking-exception.txt](../LICENSES/GPL-3.0-linking-exception.txt)),
so builds combining both parts can be distributed.

Rules:

- Never add original game data, executable bytes, or Ghidra pseudo-C here.
- Every file needs a `LicenseRef-EF2000-Derived` SPDX header (or is covered
  by the `decomp/**` annotation in `REUSE.toml`).
- Do not move recovered game logic out of this directory into GPL code.

Contents so far:

- `symbols/functions.tsv`: every original function (address, size, name,
  module, recovery state). Maintain with `tools/decomp_status.py`
  (`summary`, `next`, `check`, `seed`).
- `symbols/globals.tsv`: named global data with evidence.
- `symbols/signatures.tsv`: calling conventions confirmed from original code
  and at least two distinct callers (which does not imply C verification).

These files hold facts about the original program, never its code bytes.

Layout:

- `include/`: fixed-width guest types, ABI declarations and recovered layouts.
- `src/`: recovered C, grouped by documented original module when known.
- `compat/`: reproductions of original library behavior where required.
- `tests/`: per-function differential test specifications.
- `build/`: ignored OpenWatcom object files and other generated outputs.

There is no standalone **playable game** build yet. The flake now produces
two executable bootstraps from `standalone/main.c` and recovered
`src/hiscore.c`: `.#ef2000-decomp` (native GCC/Linux) and
`.#ef2000-decomp-watcom` (OpenWatcom/CauseWay DOS, launched via DOSBox-X).
The Linux package also compiles 341 modules into a GCC archive;
`game_helpers125.c`, `runtime_helpers23.c`, and `runtime_helpers42.c` still
contain Watcom-specific or non-portable C. The DOS package retains all OMF
objects for hybrid splicing. The bootstrap uses synthetic high-score data;
it cannot enter a playable game. The game entry's initial arena call has now
been recovered, but TACTCOM, external function bindings, global initialization
and platform services are not yet reconstructed for standalone use.
`nix run .#ef2000-decomp -- --image /path/to/_EF2000.EXE` now validates the
known original executable and maps its initialized X-32 payload and zeroed BSS
into host-owned, bounded 64 MiB guest memory. No original instructions run;
the first-screen startup call graph still needs recovery. The standalone Linux
tests prove that recovered C stream and 8x6 glyph functions can use the
translated guest offsets with synthetic data. This does not make the whole
archive safe to invoke: remaining 32-bit callback pointers and guest function
bindings must be adapted for a 64-bit Linux process.
The optional `--probe-arena` attempts the first recovered game-owned startup
call, `init_arena_e29a0`, and its `reset_buffer_e2a1c` callee, with a bounded
host allocator and the minimum required Watcom/X-32 service bindings. A
synthetic execution test verifies the game globals, 0xe2a50 cleanup hook and
zeroed arena. This is a startup **probe**, not a verified reproduction of the
original executable's entire C-runtime startup; its execution against real
game data passed with the SHA-pinned German executable on 2026-11-09.
`--probe-main` runs the recovered 100-byte game entry at `0x8b274`
(including the native case-insensitive `0x9d524` comparison), invokes the
recovered arena setup, and enters the **partial** TACTCOM `0x8b39c` prefix.
It follows the recovered `0x880` DOS service wrapper via a native, synthetic
X-32 350Bh available-memory response, calls the recovered comma formatter,
prints the memory diagnostic from the original image and flushes the output
stream. The prefix then sets two logging callback globals, uses the recovered
`0x9c980` default-directory path builder and `0x9c858` DOS environment
lookup, appends the game's first resource pathname and dispatches through
recovered `0x9cb20` to the recovered `0xb3b90` resource-size reader. On Linux,
`--probe-main` now maps DOS paths into a **read-only** data directory, by
default the original executable's directory or the explicit `--data-dir DIR`.
The reader recognizes JR and RA header lengths and falls back to seeking to
the actual file end. The original file-open/read/seek/close paths are bound
to five host-owned handles; no data is written or added to the repository.
For the SHA-pinned original executable without a `check.cd`, the normal branch
now copies the `0xea50c` missing-resource message, emits it to host stderr as
UTF-8 (instead of writing the original log file), executes the recovered
`0xeb448` dispatcher, and stops before the `0xeb25a` page switch. With a
synthetic `check.cd` it executes recovered `0x82cec` options initialization,
including the language handler/path selection. A synthetic disconnected
DOS gameport returns `0xff` from port `0x201` during `0xd66c2`, as the
original's no-device branch would, and the probe stops before the `0xe94ec`
text/GUI file loader. These are **bounded probes**: they do not
implement the general variadic logger or connected-joystick input. The memory-shortage
branch stops at `0x8b417`. The remainder of the 402-byte TACTCOM entry and
actual first-screen services are **not yet linked**. The alternate `playvideo`
branch is exercised with synthetic guest argv. This is neither a native
reproduction of the real DOS memory report nor Watcom/guest replay verified.

Use `.#ef2000-hybrid` to combine OpenWatcom OMF replacements with the
original executable through the native64 harness until the source is complete.
The original-code runtime remains the validation oracle. See
`LICENSE-NOTE.md` before distributing any recovered source.

Status (2026-11-07): 4,277 Ghidra-inventoried functions plus 10 manually
confirmed additional entry points (4,287 tracked); 3,223 marked `compiles`,
247 additional host-tested `drafted` entry points, 11 `spliced`, 5 `blocked`,
and 20 `verified`. The verified OpenWatcom OMF replacements are
`to_lower_ascii`, `to_upper_ascii`, `encode_runtime_segment_offset`,
`read_runtime_slot_flag`, `initialize_runtime_probe_latch`, `init_bounds_d0b68`,
`sync_flag_12ec8`, `reset_unit_counter`, `set_runtime_phase_five`,
`store_runtime_control_word`, `initialize_mode_limits`,
`copy_active_buffer_pointer`, `set_control_mode_a`, `set_control_mode_b`,
`set_control_mode_c`, `resolve_link_pointer`, `refresh_link_pointer`,
`exchange_slot_1041e4`, `strip_cr_inplace` and `find_last_char`.
Their isolated differentials and deterministic guest replays pass. The encoder uses two flag-neutral nops to
retain the original instruction count; its only isolated memory difference
is the 8-byte callee stack scratch below incoming ESP. Replay determinism
requires the synthetic VGA retrace clock. See
[batch 110](../docs/subsystems/decomp-batch-110.md),
[batch 111](../docs/subsystems/decomp-batch-111.md) and
[batch 112](../docs/subsystems/decomp-batch-112.md), plus
[batch 121](../docs/subsystems/decomp-batch-121.md) and
[batch 122](../docs/subsystems/decomp-batch-122.md),
[batch 123](../docs/subsystems/decomp-batch-123.md) and
[batch 124](../docs/subsystems/decomp-batch-124.md) for the latest verified
replacements; batch [125](../docs/subsystems/decomp-batch-125.md) verifies the
first control-mode gate, [batch 126](../docs/subsystems/decomp-batch-126.md)
verifies the paired mode B/C setters, and [batch 127](../docs/subsystems/decomp-batch-127.md)
verifies the signed link-pointer lookup and its unaligned candidate override;
[batch 128](../docs/subsystems/decomp-batch-128.md) verifies its record updater.
M0 mechanically compiles 418/1,020 available unrecovered Ghidra exports
(41.0%, 127,079/543,917 body bytes = 23.4%) with OpenWatcom v2; 589 exports
remain `manual` and 13 have compile errors. Hand-written recovered C covers
364,031 body bytes (40.0%); together with mechanical `ok` exports, potential
hybrid coverage is 491,110/908,947 tracked body bytes (54.0%). A focused
154-function image passes repeated-baseline main-menu and quick-fight-setup
replays. The broad image was relinked with 2,453 functions (2,043
hand-written, 410 mechanical), 12,265 entry bytes, a 587,348-byte arena and
599,613 total patch bytes, with 2,287 exclusions across 704/766 modules. Its
500M main-menu replay still faults at guest IP `0x190` on tick 561 with an
unmapped read at `0x358be389`; M1/M2 remain unpassed. See the latest replay
and compiler evidence in
[`decomp-log.md`](../docs/subsystems/decomp-log.md).
See also
[the quantitative status and remaining-work report](../docs/decompilation-status-2026-11-05.md),
as well as [renderer stream findings](../docs/subsystems/renderer-streams.md) (batch 082)
and [renderer/driver batch 083](../docs/subsystems/decomp-batch-083.md) for
boundary repairs, and [mission callback batch 084](../docs/subsystems/decomp-batch-084.md)
for its twenty named GUI entries, validation and pending guest work.
See also [campaign and weapons-panel batch 085](../docs/subsystems/decomp-batch-085.md),
which adds twenty more named callbacks with host-level tests only; and
[multiplayer/network callbacks batch 086](../docs/subsystems/decomp-batch-086.md),
which reconstructs twenty network configuration and status handlers, and
[environment parameter batch 087](../docs/subsystems/decomp-batch-087.md),
which covers fifteen damage/fade line parsers and five mode flag handlers;
and [environment and GUI batch 088](../docs/subsystems/decomp-batch-088.md),
which covers eight additional environment handlers and twelve GUI callbacks.
The [63-entry GUI dispatch and mission batch 089](../docs/subsystems/decomp-batch-089.md)
separately reports 48 five-byte jump aliases, two small quit wrappers, and
13 substantive callback/state bodies; jump aliases must not be mistaken
for completed native game logic.
The [READCOLS environment batch 090](../docs/subsystems/decomp-batch-090.md)
adds twenty nontrivial line parsers and global-state handlers as host drafts,
with explicit guest scanf and engine-call boundaries. They require guest
differential replay and OpenWatcom ABI verification before replacement.
The [palette, crater, and sky batch 091](../docs/subsystems/decomp-batch-091.md)
adds twelve more nontrivial READCOLS host models, including two record layouts,
linked texture allocation, signed bounds, and the two-stage SETSUN scanner;
guest ABI and engine operations are still unresolved.
The [READCOLS tail batch 092](../docs/subsystems/decomp-batch-092.md)
adds palette dispatch, cockpit palette selection, lightning-point allocation,
and the two smoke record rings; those four remain host-only drafts, with
specific guest call boundaries and a documented unresolved flare parser.
The [high-score C batch 093](../docs/subsystems/decomp-batch-093.md) adds five
host-tested C bodies for existing high-score routines, checked against the
previously guest-tested C++ model; they remain unspliced drafts.
The [GUI script parser batch 094](../docs/subsystems/decomp-batch-094.md)
adds seven source-grounded host C handlers for layout properties, string
commands, resource paths and palette extrema; tokenizer and graphics-driver
guest boundaries still require differential replay.
The [GUI script control batch 095](../docs/subsystems/decomp-batch-095.md)
adds a native guest-RAM lexer, the six-number FROM/SIZE/HOTSPOT reader and
the 14-command OPTIONS dispatcher. Its remaining helpers use host adapters.
The [OPTIONS label-table continuation](../docs/subsystems/decomp-batch-095-labels.md)
adds two more source-grounded host models for option assignment lookup,
replacement, deletion and allocation.
The [OPTIONS source reader batch 096](../docs/subsystems/decomp-batch-096.md)
connects the dispatcher to the original line and token routines and models
the indirect formatting wrapper, with pinned-byte audits for both entries.
The [OPTIONS display batch 097](../docs/subsystems/decomp-batch-097.md)
models the original cursor repeat timers, pointer selection loop and
fade/no-fade cleanup; rendering, hit testing and device inputs remain adapters.

Batch 113 adds `copy_record9_d0858` as a spliced, unverified OpenWatcom candidate: function differential passes, but the deterministic cockpit replay has a selected BSS hash mismatch. `latch_values_ce328` is blocked because its current C lowering exceeds the original extent. See [batch 113](../docs/subsystems/decomp-batch-113.md).
Batch 114 corrects the `group_flag_negative_cc1ac` ABI record: original disassembly masks EDX, so it is not preserved. Its 72-byte OpenWatcom C lowering cannot fit the 61-byte body; see [batch 114](../docs/subsystems/decomp-batch-114.md).
Batch 115 verifies `init_bounds_d0b68`, an exact 63-byte OpenWatcom replacement with 12 isolated cases, three captured caller states and a passing cockpit replay (155 hits). See [batch 115](../docs/subsystems/decomp-batch-115.md).
Batch 116 adds `find_node_by_byte_b` as a spliced OpenWatcom candidate. Its eight isolated cases pass, but available deterministic replay paths did not reach it; see [batch 116](../docs/subsystems/decomp-batch-116.md).

Batch 118 verifies `sync_flag_12ec8` from a live indirect callback and blocks its parent `reinitialize_runtime_slots` on callback ABI and placement evidence; see [batch 118](../docs/subsystems/decomp-batch-118.md).
Batch 119 records an OpenWatcom `max3_signed` candidate that passes 253 isolated cases and the repeated menu/setup/cockpit replay when paired with the current fixture-derived encoder patch. The candidate gets no entry hit, so it remains spliced. It also blocks the overlapping x87 entries `0xa4470`/`0xa4472`; see [batch 119](../docs/subsystems/decomp-batch-119.md).
