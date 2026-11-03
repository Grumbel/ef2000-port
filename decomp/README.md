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

Layout, build commands and status follow plan §3.1 once Phase 1 begins.
