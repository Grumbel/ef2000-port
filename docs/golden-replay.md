<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Bounded original-vs-patched golden replay (Phase 1, §3.9, partial)

`tools/replay_suite.py` is the first executable slice of the planned replay
oracle. It uses the **C++ Unicorn runner**, not the native64 backend. It runs
each scenario twice **without** a patch by default to establish baseline
repeatability, then once with SHA-pinned in-place EFPATCH1 patches.

At each requested checkpoint, the three runs have the same instruction budget,
instruction-counted PIT, scripted input and game/CD source folders. Each starts
**from guest entry**, with a different empty copy-on-write DOS drive; this is
not a snapshot of one continuous run. The original executable is treated as read-only,
its SHA-256 is checked before and after the suite, and output directories
must not be inside the original game/CD folders. DOS date/time calls use the
deterministic guest clock (2000-01-01 noon UTC plus emulated PIT time), and
the suite uses synthetic VGA retrace advanced by sampled 3DAh reads, so host
wall-clock drift cannot change startup state between runs. It forces keyboard
controls and disables the host joystick so external device state cannot alter
the scripted run. The suite refuses
to replace an existing output directory.

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
      "checkpoints": [10000000, 20000000],
      "timer_instructions": 23866,
      "events": [
        ["--mouse-event", "150:320:200:1"],
        ["--mouse-event", "152:320:200:0"]
      ],
      "must_hit": ["0x0000432c"],
      "guest_hashes": [{"address": "0x1970a0", "length": 256}]
    }
  ]
}
~~~

An event timestamp is a **PIT tick** (not a guest instruction). Tick values
must fit the case's instruction budget and timer interval. Each case must
name at least one actual patch entry as `must_hit`; only mouse, key and
joystick event arguments are allowed in the manifest. Other runtime options
that could change timing or redirect outputs are fixed by the suite.
`guest_hashes` is optional; each range is identified by a guest linear address
and byte length (maximum 16 MiB per range, 32 MiB total). The suite refuses
ranges overlapping any replacement's code bytes, because differences in the
compiled implementation are expected there. Prefer mutable game data and
BSS ranges whose meaning is understood. **The addresses above are only an
illustration, not an established high-score-screen golden checkpoint.**

The optional `checkpoints` list contains up to eight strictly ascending guest
instruction limits smaller than the final `instructions` count. The suite
replays from entry for *each* limit, twice unpatched and once patched, so the
same memory/framebuffer comparisons are made at intermediate points. A
manifest is limited to 64 total points across cases to prevent accidental
explosions in runtime or disk usage. Each point has separate run directories
and a record in the `checkpoints` array of `summary.json`. `must_hit` entries
need to be reached by the **final** point, not necessarily each earlier point.
Early termination before an intermediate checkpoint is a failure.

If the executable has not yet selected a video mode, the C++ runner produces
no PPM. Such pre-video diagnostics may set `"allow_no_framebuffer": true`,
**but only when at least one guest RAM hash is requested**. A missing PPM is
then recorded as `frame_sha256: null`; a missing image in one run and a present
image in another still fails comparison. This exception must not be used to
claim visual parity of a replacement. The default requires a framebuffer.

## How to read the results

`summary.json`, per-run console logs, and any PPM screenshots are
written under the new result directory (normally `local/`, ignored by Git).
The patched C++ runner prints one `patch_hit: addr=0x... count=N` for each
in-place replacement, plus a `patch_hits: total=N` line. The suite rejects
missing, duplicate, or inconsistent hit reports and requires each `must_hit`
entry to have a **nonzero** count. This avoids claiming equivalence when an
entirely untouched replacement happens to leave the game working.

The suite additionally checks that:

- Both original runs (when requested) and the patched run have equal stop
  status, guest interrupt count, PIT tick count, IRQ0 count and final
  framebuffer SHA-256. All requested guest-memory-range SHA-256s must also
  match; missing, duplicate or unexpected hash reports are errors.
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

This implementation compares the framebuffer and selected guest-memory ranges
at independent bounded runs ending at each requested instruction checkpoint,
but does not yet implement in-process snapshot checkpoints, separate palette
hashes or full-RAM comparison. It does not yet record immutable baseline hashes for all
6–10 planned scenarios, or perform DOSBox-X cross-checks. The script requires
an already compiled and safely spliced replacement; it does not produce an
OpenWatcom object itself. A nonzero patch entry count proves the replacement
was visited, **not** that its outputs and memory side effects match.

Before changing a function tracker entry to `verified`, we still need:

1. The function-level register/RAM differential (`func_differential.py`).
2. The reproducible original and patched in-game replay above, with a
   baseline repeat and a nonzero patch-hit count.
3. Review of the actual guest call states, control flow and any differences.

The memory hashing and patch-hit hooks have also been compiled and run in
the native C++ synthetic guest: two calls to a test entry yield
`patch_hit: addr=0x200 count=2`, and four zero guest bytes yield SHA-256
`df3f6198…14b81119`. Batch 105 supplied full game data and ran a scripted
main-menu→quick-fight→cockpit path with an OpenWatcom replacement. The initial
attempts sampled VGA retrace from host `steady_clock`, so repeated originals
diverged. Batch 106 added `--vga-clock guest` and made the replay suite select
it. The three long original/patched cases now repeat with matching status,
interrupt counts, selected BSS hashes and framebuffer hashes. `to_lower_ascii`
also passes its isolated differential and 20-million-instruction full-RAM
startup replay, and is now the first `verified` function. Long gameplay checks
hash selected BSS ranges rather than all guest RAM, so §3.9 remains partial.

Batch 100 added multi-checkpoint comparison and explicit pre-video handling.
In the locally statically linked runner, a disposable identity EFPATCH1 at
guest entry completed four checkpoint budgets (200k, 400k, 600k and 800k),
with two original runs and one patched run per point. All 12 runs agreed on
the selected RAM hash, status and PIT counters, and the patched entry was
observed. This does **not** validate a recompiled function. The same supplied
Unicorn library crashes the host on a minimal standalone 32-bit x87 `FLD` /
`FSTP` test; the actual game enters x87 at guest `0x0fcca5` after approximately
868,499 instructions. The immediate limit is the local Unicorn binary/runtime
combination, not an established game defect. Runs with the owner's native Nix
toolchain and complete game installation remain necessary.
