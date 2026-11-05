<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Bounded original-vs-patched golden replay (Phase 1, §3.9, partial)

`tools/replay_suite.py` is the first executable slice of the planned replay
oracle. It uses the **C++ Unicorn runner**, not the native64 backend. It runs
each scenario twice **without** a patch by default to establish baseline
repeatability, then once with SHA-pinned in-place EFPATCH1 patches.

The three runs have the same instruction budget, instruction-counted PIT,
scripted input and game/CD source folders. Each gets a **different empty
copy-on-write DOS drive**. The original executable is treated as read-only,
its SHA-256 is checked before and after the suite, and output directories
must not be inside the original game/CD folders. The suite refuses to replace
an existing output directory.

## Invocation

First build an EFPATCH1 using `nix run .#omf-inplace` as described in
[`omf-splicer.md`](omf-splicer.md). It must be based on the supported
`_EF2000.EXE` payload SHA-256; patches with another payload digest are
rejected before the game starts.

~~~sh
nix run .#replay-suite -- \
  --executable ../EF2000/c/EF2000/_EF2000.EXE \
  --patch local/patches/hiscore.efpatch \
  --manifest local/replay-manifest.json \
  --output local/replay-results/run-001
~~~

The Nix app supplies the matching `ef2000-unicorn-cpp` executable. Outside
Nix, call `python3 tools/replay_suite.py` and supply `--runner` explicitly.
`--cd-dir /path/to/data` supplies optional CD files. `--timeout N` defaults
to 300 **host** seconds per run; `--baseline-repeats 1` is only a speed-up
for investigation, not sufficient evidence of deterministic baseline runs.

Example **manifest shape**, deliberately not claiming to be a working
high-score screen replay (the scripted menu path and its patch entry must
first be observed in the actual game):

~~~json
{
  "schema": "ef2000-golden-replay-v1",
  "cases": [
    {
      "name": "highscore-menu",
      "instructions": 30000000,
      "timer_instructions": 23866,
      "events": [
        ["--mouse-event", "150:320:200:1"],
        ["--mouse-event", "152:320:200:0"]
      ],
      "must_hit": ["0x0000432c"]
    }
  ]
}
~~~

An event timestamp is a **PIT tick** (not a guest instruction). Tick values
must fit the case's instruction budget and timer interval. Each case must
name at least one actual patch entry as `must_hit`; only mouse, key and
joystick event arguments are allowed in the manifest. Other runtime options
that could change timing or redirect outputs are fixed by the suite.

## How to read the results

`summary.json`, per-run console logs, and the final PPM screenshots are
written under the new result directory (normally `local/`, ignored by Git).
The patched C++ runner prints one `patch_hit: addr=0x... count=N` for each
in-place replacement, plus a `patch_hits: total=N` line. The suite rejects
missing, duplicate, or inconsistent hit reports and requires each `must_hit`
entry to have a **nonzero** count. This avoids claiming equivalence when an
entirely untouched replacement happens to leave the game working.

The suite additionally checks that:

- Both original runs (when requested) and the patched run have equal stop
  status, guest interrupt count, PIT tick count, IRQ0 count and final
  framebuffer SHA-256.
- Each runner exits successfully, emits exactly one parseable status line,
  and produces a framebuffer.
- A missing patch hit, nondeterministic baseline, guest stop/fault, missing
  screenshot or changed visual hash is a **failure**. All source assets stay
  outside the collected artifact.

Instruction-boundary EIP differences are **reported** but not compared as
equivalence criteria: a compiled replacement can use a different number of
instructions. The current budget still ends at a common guest instruction
count, so even a logically equivalent function with different timing can
produce a framebuffer difference at an arbitrary checkpoint. Investigate
those differences; do not bless a mismatch automatically.

## Not yet a complete §3.9 oracle

This implementation compares the **last** framebuffer in each bounded run,
not palette hashes, selected guest RAM ranges or multiple intermediate
checkpoints. It does not yet record immutable baseline golden hashes for all
6–10 planned scenarios, or perform DOSBox-X cross-checks. The script requires
an already compiled and safely spliced replacement; it does not produce an
OpenWatcom object itself. A nonzero patch entry count proves the replacement
was visited, **not** that its outputs and memory side effects match.

Before changing a function tracker entry to `verified`, we still need:

1. The function-level register/RAM differential (`func_differential.py`).
2. The reproducible original and patched in-game replay above, with a
   baseline repeat and a nonzero patch-hit count.
3. Review of the actual guest call states, control flow and any differences.

The current test coverage is **synthetic/asset-free**. Until an actual
SHA-pinned original executable, Watcom output and functioning patch are run
through these steps, **zero functions have gained `verified` status**.
