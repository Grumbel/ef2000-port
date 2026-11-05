<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# EF2000 decompilation status — 2026-11-05 (after batch 097)

This is a **recovery** progress report, not a claim that the original game
has already been rebuilt from the recovered C. It uses the current
`decomp/symbols/functions.tsv` as its numerical source.

| Tracker state | Functions | Body bytes | Share of 908,947 inventoried bytes |
| --- | ---: | ---: | ---: |
| `compiles` | 3,257 | 363,763 | 40.0% |
| `drafted` | 247 | 19,916 | 2.2% |
| `typed` | 28 | 7,517 | 0.8% |
| `named` | 83 | 27,592 | 3.0% |
| `unknown` | 669 | 489,650 | 53.9% |
| `library` | 3 | 509 | 0.1% |
| **Total** | **4,287** | **908,947** | **100%** |

The 669 `unknown` functions are only 15.6% of function *entries* but 53.9%
of body *bytes*: they average roughly 732 bytes per entry, compared with
roughly 112 bytes for `compiles`. Counting functions therefore hides the
harder work. `compiles` means preliminary C syntax has passed host-level
compilation checks, not that all behavior, ABI or game replay is correct.

**Verified, spliced C game-function replacements: 0.** The project's strict
definition of `verified` requires an OpenWatcom build, a real guest splice,
function-level differential tests and golden replay. Even the 3,504 entries
in `compiles`/`drafted` have not met that complete standard.

## What is working

- Original game runs through the native64 backend on Linux and the portable
  Unicorn C++ backend; the emulator remains the behavior reference.
- Ghidra inventory, symbols, ABI samples, REUSE licensing and OpenWatcom/Nix
  integration exist; host regression tests and many pinned-byte audits run.
- Resource formats, GUI resources, a high-score C++ behavioral reference,
  portions of GUI parsing, READCOLS configuration, world grid, network, and
  OPTIONS menu behavior have been documented and reconstructed in drafts.

## What remains

1. **Finish the proof-of-replacement pipeline**: reliable OMF splicing,
   guest register/memory differential checks and a reproducible full golden
   replay suite. The first seven high-score functions must pass this before
   Phase 1 exits. The plan currently marks the OMF splicer, harness patch
   support, library identification and differential tester in progress;
   the golden suite and exit criteria remain unchecked.
2. **Recover the large functions and call chains**: 669 unknown functions,
   totaling about 490,000 bytes (478 KiB) of original code, plus 111 `named`/`typed` and
   247 drafts still short of compilation/behavioral proof.
3. **Verify the existing reconstructed code**: 3,257 `compiles` entries
   require systematic guest ABI checks and differential/replay coverage.
   Bulk automation and replay coverage are essential to make this tractable.
4. **Complete the subsystem waves** from startup and file loading to
   rendering, input, audio, GUI, flight physics, AI, missions and multiplayer.
   None has yet reached the plan's ≥95% *verified bytes* exit threshold.
5. **Build the complete original-target OpenWatcom executable**, including
   initialized data and X-32/DPMI adaptation; then implement the fully
   native SDL3 platform layer and higher-resolution/widescreen renderer.
   These phases are explicitly future work in the plan.

## Confidence and timing

It would be misleading to call this project "80% done" because 3,504 of
4,287 functions have C source or host drafts. The majority of original code
**by byte size** remains unknown, and the entirely separate guest-verification
and native-modernization stages are unfinished. A completion date cannot be
derived from function counts; several complex original subsystems and the
first successful whole-game splice are still ahead.

Batch sizes of 50–100 can be appropriate for mechanically similar aliases,
table accessors or independently tested short helpers. Large stateful menu,
renderer and flight functions need smaller reviewed clusters. The throughput
priority should be to **finish and automate verified replacements**, rather
than continuing to increase the drafted count without any guest verification.

The immediate recommended milestone is the high-score Phase-1 exit: get the
existing C high-score functions through OpenWatcom, splice them, and pass a
deterministic replay; this unlocks a meaningful completion metric for every
subsequent subsystem.
