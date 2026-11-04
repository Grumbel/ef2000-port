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

Status (2026-11-03): 4,277 inventoried functions, 2946 drafted and none verified
as a compiled-and-spliced C replacement. The tracker is authoritative.
