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

There is no standalone game build yet. The intended workflow is to compile
individual functions with OpenWatcom, splice them into the original guest
image, and compare behavior against the unmodified executable (plan §4).
The host-side emulator remains the validation oracle. See
`LICENSE-NOTE.md` before distributing any recovered source.

Status (2026-11-05): 4,277 Ghidra-inventoried functions plus 10 manually
confirmed additional entry points (4,287 tracked); 3,257 marked `compiles`,
245 additional host-tested `drafted` entry points, and none verified as a
compiled-and-spliced C replacement. The tracker is authoritative. See
[renderer stream findings](../docs/subsystems/renderer-streams.md) (batch 082)
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
