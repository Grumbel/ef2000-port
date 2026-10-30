<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Repository instructions

## Commits

- Commit verified work in small, incremental pieces as it becomes ready. Do not
  accumulate hours of unrelated changes before committing.
- Every new commit must include a `Co-authored-by:` trailer naming the exact
  model that made the commit, using `codex@openai.com` as the attribution address.
- Obtain the exact model identifier from authoritative session metadata or the
  user. Do not infer it from a model family, repository configuration, or another
  agent's model. If unavailable, ask while continuing implementation and checks.
- For this session, the user confirmed `/model` reports `6.1 Sol`. Use
  `Co-authored-by: 6.1 Sol <codex@openai.com>`. Recheck the model in future sessions.
- Keep commit descriptions focused on the behavior changed and relevant evidence.

## UI verification

- Run all automated UI tests and input injection on a private Xvfb display.
- Never move the mouse, inject keys, change focus, or capture windows on the
  user's desktop display.
- Keep original EF2000 assets unchanged. Use disposable copies or the output
  directory's drive overlay for configuration and generated data.
