<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Repository instructions

## Commits

- Commit verified work in small, incremental pieces as it becomes ready. Do not
  accumulate hours of unrelated changes before committing.
- Every new commit must include a `Co-authored-by:` trailer naming the AI
  model that actually made the changes in that commit, with its vendor's
  attribution address. Examples:
  `Co-authored-by: Claude Opus 5.5 <noreply@anthropic.com>`,
  `Co-authored-by: <exact OpenAI model name> <codex@openai.com>`.
- Use the exact model identifier from authoritative session metadata or the
  user. Do not infer it from a model family, repository configuration, earlier
  commits, or another agent's model. If unavailable, ask while continuing
  implementation and checks. Never copy attribution from previous commits;
  each session attributes itself.
- For ChatGPT Chat sessions that disclose only a reasoning setting (such as
  "High"), the user approved a descriptive fallback:
  `Co-authored-by: ChatGPT (High) <codex@openai.com>`. Record an actual model
  identifier instead whenever one is available.
- Keep commit descriptions focused on the behavior changed and relevant evidence.

## Decompilation work

- Follow `docs/decompilation-plan.md`: read its "Ground rules" and
  "Operating loop" sections at the start of every session, and update its
  status markers and progress log in the commits that complete steps.
- Binaries need not match the original; guest RAM state and observable
  behavior must (plan §2.2).
- Never commit Ghidra pseudo-C, original executable bytes, or extracted game
  assets. Recovered game source belongs only under `decomp/`, pending the
  owner's licensing decision.

## UI verification

- Run all automated UI tests and input injection on a private Xvfb display.
- Never move the mouse, inject keys, change focus, or capture windows on the
  user's desktop display.
- Keep original EF2000 assets unchanged. Use disposable copies or the output
  directory's drive overlay for configuration and generated data.
