<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Repository instructions

## Commits

- Commit verified work in small, incremental pieces as it becomes ready. Do not
  accumulate hours of unrelated changes before committing.
- Every new commit must include a `Co-authored-by:` trailer, using
  `codex@openai.com` as the attribution address.
- Prefer the exact model identifier from authoritative session metadata or the
  user. Do not infer it from a model family, repository configuration, or another
  agent's model. If unavailable, ask while continuing implementation and checks.
- For ChatGPT Chat sessions that disclose only a reasoning setting (such as
  "High"), the user approved a descriptive fallback. Use
  `Co-authored-by: ChatGPT (High) <codex@openai.com>` for ChatGPT High when the
  exact model cannot be established. Record an actual model identifier instead
  whenever one is available; do not reuse attribution across different sessions.
- Keep commit descriptions focused on the behavior changed and relevant evidence.

## UI verification

- Run all automated UI tests and input injection on a private Xvfb display.
- Never move the mouse, inject keys, change focus, or capture windows on the
  user's desktop display.
- Keep original EF2000 assets unchanged. Use disposable copies or the output
  directory's drive overlay for configuration and generated data.
