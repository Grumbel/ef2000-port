<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 100: independent guest replay checkpoints and pre-video RAM oracle

This continues Phase 1 step 3.9. **No recovered function is verified.**

## Replay changes

- A manifest's optional ascending `checkpoints` array selects up to eight
  intermediate instruction limits, in addition to its final `instructions`.
  Each point starts from the same original guest entry with the same input
  script; two unpatched and one patched runs compare stop counters, video
  output when present, and selected guest-memory SHA-256s. The complete
  series is bounded at 64 points across cases.
- `summary.json` now retains the per-point baseline, patched results and
  failures. The legacy top-level `baseline` and `patched` entries for each
  case still describe the *final* point. A missing patch-entry hit fails at
  the final point, not necessarily earlier points.
- Explicit `allow_no_framebuffer: true` allows startup checkpoints before
  a VGA mode exists, but only with nonempty `guest_hashes`. Missing video is
  reported as `null`. A missing framebuffer remains an error by default,
  and asymmetrical video presence between original and patched runs always
  fails. This option is **not evidence of visual parity**.
- Asset-free synthetic tests cover checkpoint count/order, independent
  reruns, early-only RAM differences, late patch hits, missing video and
  asymmetrical video. Each run still gets its own empty DOS overlay and
  an exclusive output path.

## Actual pinned executable experiment

The original German TACTCOM `_EF2000.EXE` was read unmodified from an
ignored local directory. Its complete SHA-256 matches `docs/executable.md`.
The independently supplied Unicorn 2.1.4 static archive was linked with
the host C++17 runner; this is a **local diagnostic**, not the Nix build.

An ignored, disposable EFPATCH1 rewrote the eight guest entry bytes at
`0x86` with the *same* original bytes. A manifest requested checkpoints
at **200,000**, **400,000**, **600,000** and **800,000** guest instructions,
selected RAM hash region `0x1970a0:256`, and allowed absent pre-video PPMs.
All 12 runs (2 original plus 1 patched per point) passed, with identical
RAM hashes, interrupt/IRQ0/PIT counts and stop status per point, and one
recorded patch entry hit in each patched run. All game-derived hashes and
the identity patch remain under ignored `local/`; none are committed.

The experiment is valuable for confirming checkpoint orchestration and
entry-hit instrumentation on the real executable, **not** for confirming
any C replacement. The chosen RAM region was unchanged in these early
startup runs. Later replays must select mutated, behaviorally meaningful
RAM and at least one scene framebuffer/palette.

## Isolated x87 failure in the local toolchain

In the same locally linked executable, 868,499 guest instructions complete
with EIP `0x0fcca5`; requesting 868,500 triggers host SIGSEGV (exit 139).
The instruction is `FLD QWORD PTR [0x1936b0]`, the first x87 load in this
startup path. A disposable minimal Unicorn guest with nothing but a
mapped-memory `FLD`/`FSTP` pair also reproduces exit 139. Thus the fault
does **not** require EF2000 game logic, file data or the project's device
hooks. It is a failure of the local supplied Unicorn archive/runtime
combination, not evidence that the original game crashes. The packaged
shared library also requires newer glibc symbols than the host provides;
the exact root cause of the static archive's x87 fault remains unknown.

The disposable smoke test, crash probe and game-derived checkpoint outputs
stay under ignored `local/`. No signal-handler or x87-workaround code was
added to the shipping emulator. On the owner's Nix environment, use the
matching Unicorn/glibc closure, OpenWatcom and the complete installation.

## Verification and next gate

Run commands:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tests.test_replay_suite -q
python3 tools/decomp_status.py check
```

The real identity-patch replay used `python3 tools/replay_suite.py` with
`--runner local/build/ef2000-unicorn-cpp`, the SHA-pinned executable,
`--patch local/build/identity.efpatch`, the ignored manifest and a new ignored
output directory. It reported `1/1 passing` across four checkpoints.

**Still blocked:** `wcc386` and a compatible executable x87-capable Unicorn
toolchain are not installed in this sandbox, and the complete game data is
absent. The next decisive step is compiling a reviewed Watcom C function,
splicing its actual OMF body, comparing captured guest entry states, and
then passing the scripted asset-backed replay. The tracker remains at zero
`verified` functions.
