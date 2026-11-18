<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Repository instructions

## Commits

- Commit verified work in small, incremental pieces as it becomes ready. Do not
  accumulate hours of unrelated changes before committing.
- Every new commit must include a `Co-authored-by:` trailer naming the AI
  that actually made the changes in that session, with its vendor's
  attribution address. Never copy attribution from earlier commits.
- Use a specific model name when the session metadata or the user states
  one; otherwise use a generic name. Do not guess a version and do not stop
  to ask. Examples:
  `Co-authored-by: Claude Opus 5.5 <noreply@anthropic.com>`,
  `Co-authored-by: Claude <noreply@anthropic.com>`,
  `Co-authored-by: Codex <codex@openai.com>`,
  `Co-authored-by: ChatGPT (High) <codex@openai.com>`.
- The commit author is always the repository owner
  (`Ingo Ruhnke <grumbel@gmail.com>`), never the AI.
- New files use `SPDX-FileCopyrightText: <year> Ingo Ruhnke <grumbel@gmail.com>`;
  never name an AI or its vendor as copyright holder.
- Keep commit descriptions focused on the behavior changed and relevant evidence.

## Delivery

- Agents with direct repository access (Codex, Claude Code) commit
  directly on `master`.
- Web chat sessions without repository access deliver a Git bundle; see the
  handoff section in `TODO.md`.
- Original game data may arrive as a `.zip`. Extract it outside the
  repository (or under the ignored `local/`), verify the `_EF2000.EXE`
  SHA-256 from `docs/executable.md`, and point tools at it with
  `EF2000_EXE` / `--data-dir`. Never commit the archive or its contents.

## Decompilation work

- The goal is readable C for every game function under `decomp/src` that
  builds into a working game without original code bytes.
- At the start of every decompilation session (any model), read
  `docs/decompilation-plan-rev5.md` (current work order; it overrides rev 2-4
  and the handoffs in `docs/handoffs/`), then §1 "Ground rules" and §4
  "Operating loop" of `docs/decompilation-plan.md`, then the last lines of
  `docs/decompilation-progress.md`. Original code remains authoritative.
- Progress is one line per tranche in `docs/decompilation-progress.md`.
  The old log in `docs/decompilation-log-archive.md` is read-only. Do not
  append to `docs/decompilation-plan.md` except to update status markers.
  Do not commit hashes of `local/` files, per-probe narratives, or new
  `decomp-batch-*.md` files.
- Every tranche commit reports `nix run .#status` (tracker, hybrid
  candidates, readability) and the hybrid regression result. Work
  that moves none of these numbers for ten commits is off track: stop and
  move to the next item in rev 5.
- The instruction lifter (`tools/asm_c_program.py`,
  `tools/asm_c_differential.py`, `ef_lift_*`) is frozen. Its output is an
  emulator, not decompiled source, and never counts as coverage.
- Binaries need not match the original; guest RAM state and observable
  behavior must (plan §2.2).
- Never commit Ghidra pseudo-C, original executable bytes, or extracted game
  assets. Recovered game source belongs only under `decomp/`, licensed as
  `LicenseRef-EF2000-Derived`; GPL files use
  `GPL-3.0-or-later WITH GPL-3.0-linking-exception`.
- Never push, publish, or force-push; the owner does that.
- `docs/decompilation-plan.md` sets the work order. `TODO.md` "Next work"
  items are maintenance background unless the plan references them.

## UI verification

- Run all automated UI tests and input injection on a private Xvfb display.
- Never move the mouse, inject keys, change focus, or capture windows on the
  user's desktop display.
- Keep original EF2000 assets unchanged. Use disposable copies or the output
  directory's drive overlay for configuration and generated data.
