# EF2000 decompilation and modernization plan

This is the master plan for turning the current emulation-based port into
(1) recovered C source that compiles with OpenWatcom, (2) documented data
formats, and (3) a modern native build with widescreen and high frame rates.
It is written to be executed step by step by an AI agent. **Read the whole
"Ground rules" and "Operating loop" sections before every work session.**

> **Revision 2 (2026-11-06): see `docs/decompilation-plan-rev2.md`.** It
> supersedes §1 rules 5/6/9, §4 and the §7 precondition: working hybrid build
> first, bisection and bulk verification later. Revision 3
> (`docs/decompilation-plan-rev3.md`) adds M0, mechanical Ghidra-to-C generation.

Status markers used below: `[ ]` not started, `[~]` in progress, `[x]` done.
Update them in this file as part of the commit that completes a step.

---

## 0. Where the project stands (baseline, 2026-11-03)

Do not re-derive these facts; they are established and documented.

| Area | State | Where documented |
| --- | --- | --- |
| Target binary | German TACTCOM `_EF2000.EXE`, SHA-256 `f21a72d8…afd`, FlashTek X-32, Watcom C/C++32 runtime ("1989, 1994"), game build string "Revision 3.23.2.35 by Derek on Nov 18 1996" | `docs/executable.md`, `docs/loader.md` |
| Image layout | Guest offset 0 = payload start; BSS `[0x195fb0, 0x4de2f0)`; entry `0x86`; C entry `0x9bd20`; candidate `main` `0x8b274` | `docs/loader.md`, `docs/startup-symbols.md` |
| Execution | **native64**: original code runs natively in an x86-64 Linux process (LDT, compatibility mode, base `0x10000000`), menu + flight work, full speed. **Unicorn C++/Python**: portable, slower, reaches flight. | `docs/startup-runtime.md`, `README.md` |
| Devices | DOS file/memory, PIT, PIC, keyboard, mouse, gameport, VGA/VBE, SB16 DMA, MSCDEX CD audio, save states | `TODO.md`, `docs/*.md` |
| Ghidra | One-command rebuild (`ef2000-ghidra rebuild`, ~4 min); with Ghidra 12.1.4: **4,277 functions, 4,277 with pseudo-C**, ~35 KB of instructions outside functions; call graph, data refs, subsystem affinity groups (11.4.2 had 4,233) | `docs/ghidra.md`, `docs/ghidra-findings.md` #062 |
| Typed reconstruction | One module: high scores (`runtime/ef2000_highscores.*`), verified by **differential execution** (original code in Unicorn vs. native C++) | `docs/highscores-reconstruction.md` |
| Data formats | DID archives (hash index, RA compression), ILBM/PBM/PCX/BMP, `.gui` scripts, sprites, fonts, `MISSION.CFG`/`NAVIG.NAV` text, mission guest struct layouts, terrain grid probe | `docs/gui-assets.md`, `docs/mission-*.md` |
| 3D shapes/world | Understood in the sibling `../tfx3view` (libTFX: `.3` shape bytecode, supershapes, target lists, EF2000 world) — **not yet integrated here** | `../tfx3view/libTFX/` |
| Embedded source names | `Gui.c`, `Views.c`, `Aid.c`, `MAPDRAW.C`, `avionics.c`, `guimain.c`, `parser.c`, `tokenise.c`, `cfg_read.c`, `parsecfg.c`, `loadtarg.c`, `ipx.c`, `Serialc.c`, … (assert/error strings) | `strings _EF2000.EXE` |
| OpenWatcom | Available in nixpkgs as `open-watcom-v2` (2026-04 snapshot) and `open-watcom-bin` (1.9) — **not yet in the flake** | — |

The strategic gap: everything so far either *runs the original bytes* or
*re-describes* them in isolated C++ models. Nothing yet replaces original code
inside the running game, and nothing is compiled with Watcom. This plan
closes that gap with a **hybrid, function-by-function replacement** pipeline.

---

## 1. Ground rules (read every session)

These extend `AGENTS.md`; on conflict, `AGENTS.md` wins.

1. **Original assets are read-only and never committed.** This includes the
   EXE, DID archives, extracted payload, Ghidra projects, Ghidra pseudo-C,
   framebuffers/screenshots of game content and audio captures. They live in
   `local/` (git-ignored). Tests that need them are opt-in via `EF2000_EXE` /
   `EF2000_DATA_DIR` and must skip cleanly when absent.
2. **Recovered game source is a derived work, not GPL.** It goes only under
   `decomp/` (see §3.1), which has its own licensing note. Never move
   recovered game logic into `runtime/` or `tools/` (GPL code) and never paste
   Ghidra output verbatim into any committed file.
3. **Behavior preservation beats readability.** Keep original bugs (example:
   the high-score checksum loop that never increments its pointer), integer
   wraparound, signedness, x87 precision, evaluation order with side effects,
   and uninitialized-padding behavior. If you think something is a bug, write
   a comment `/* ORIGINAL BUG: ... */` and keep it. Fixes go behind a
   `EF_FIX_*` macro, default off, later.
4. **Ghidra signatures are hypotheses.** The ABI is Watcom register calling
   (`EAX, EDX, EBX, ECX`, then stack, callee cleanup) *with exceptions*.
   Confirm every function's inputs/outputs/clobbers from disassembly and its
   callers before writing C. Record the confirmed signature in the symbol DB.
5. **Nothing counts as done without evidence.** "Done" for a function means:
   it compiles with OpenWatcom, it is spliced into the running game, and it
   passes the differential test and the golden replay suite (§2.4, §4).
6. **Small commits.** One function or one tightly coupled cluster (≤ ~10
   functions) per commit. Commit message: imperative subject with component
   prefix (`decomp:`, `formats:`, `tools:`, `harness:`, `port:`), body with
   addresses, evidence, and the exact test commands run. Attribution trailer
   per `AGENTS.md`.
7. **UI testing only on private Xvfb displays.** Never touch the user's
   desktop session.
8. **Never edit a fixed address/offset table for a different executable.**
   Everything in this plan is for the SHA-pinned German TACTCOM build.
9. **Time-box.** If a single function takes more than ~2 hours of effort
   without converging, mark it `blocked` in the tracker with notes on what
   failed and move on. Do not rewrite surrounding infrastructure to get one
   function through without first writing down why.
10. **Do not delete or rewrite existing working emulator features.** The
    emulator is the oracle. Until §7 is complete it must keep working.

---

## 2. Target architecture

### 2.1 Three build products from one source tree

```
                         decomp/src/*.c   (recovered game code, C89/C99-ish)
                                 |
        +------------------------+-------------------------+
        |                        |                         |
  (A) HYBRID SPLICE        (B) FULL DOS BUILD         (C) MODERN BUILD
  OpenWatcom wcc386        OpenWatcom wcc386+wlink    GCC/Clang (i686 first,
  -> OMF .obj              -> LE exe, DOS/32A or       x86-64 later) + SDL3
  -> tools/omf_splice.py   DOS4GW stub                 platform layer
  -> patched guest image   -> runs in DOSBox-X         -> widescreen, high FPS,
  -> native64 / Unicorn       and native64 harness        GPU renderer
```

- **(A) Hybrid splice** is the *workhorse* for the whole decompilation. Each
  recovered function is compiled by OpenWatcom with the original calling
  convention, linked against original global/function addresses, and patched
  into the guest image loaded by the native64 harness. The rest of the game
  keeps running original bytes. This gives continuous, whole-game validation
  from the very first function.
- **(B) Full DOS build** happens once all game code (not the Watcom C runtime
  or third-party libraries) is recovered. It proves the source is complete.
- **(C) Modern build** reuses the same C sources with a platform abstraction
  replacing DOS/VGA/SB16/gameport access. Modern features live only here.

### 2.2 Decision: functional decompilation with matching RAM state

**Decided by the owner (2026-11-03):** byte-identical ("matching") binaries
are **not** a goal. Do not spend time coaxing OpenWatcom into reproducing the
original instruction sequences. The original compiler was Watcom 10.x and
OpenWatcom descends from 11.0, so its code generation will differ anyway.

What **must** match is observable state:

- **Guest RAM contents**: all initialized data, BSS and heap that game code
  writes, at every replay checkpoint and after every differential-tested
  call. This includes struct padding, unused bytes the original leaves
  untouched (do not zero them), and order of writes where another routine or
  interrupt handler can observe them.
- **Outputs at the ABI boundary**: return registers and any register the
  callers actually read (e.g. the memcpy routine's `EAX`/`EDX` postconditions).
- **Hardware-visible effects**: framebuffer, palette, port I/O sequences,
  audio and file bytes.

Allowed to differ: instruction sequences, code size, scratch registers that
no caller reads, stack-frame contents below `ESP` after return, and the
function's own stack locals. When a replay diverges *only* in stack memory or
another region that cannot affect behavior, document that region in the
replay suite's exclusion list with the evidence; never exclude a region just
to make a test pass.

`tools/asm_diff.py` (§3.5) is a **debugging aid** for finding where a
recompiled function's logic diverges, not a progress metric.

### 2.3 Library code is not decompiled

Identify and label, but do not hand-decompile:

- Watcom C/C++32 runtime (clib, math, startup, x87 emulation hooks).
- FlashTek X-32 extender interface code.
- Miles/AIL sound library (if statically linked; the `.DIG`/`.MDI` drivers are
  external files).
- Any UniVBE/VESA helper library.

For (B) these are replaced by OpenWatcom's own libraries + DOS/32A; for (C) by
the platform layer and the host C library. Where a game function depends on a
library quirk (e.g. `rand()` sequence, `sprintf` formatting), reproduce the
quirk explicitly in `decomp/compat/`.

### 2.4 Validation oracles (strongest first)

1. **Golden replay suite**: deterministic, instruction-counted native64 or
   Unicorn runs with scripted input (`--timing instruction`, `--key-event`,
   `--mouse-event`, `--joystick-event`), recording framebuffer hashes and
   *state hashes* (selected guest memory regions) at fixed checkpoints. The
   spliced build must produce identical hashes.
2. **Per-function differential test**: run the original function and the
   recompiled function on identical guest memory/register states (generalize
   `tools/highscores_differential.py`), compare registers the ABI says are
   outputs, flags where relevant, and all memory writes.
3. **Static checks**: OpenWatcom compiles without warnings at `-w4` for the
   file; the splicer resolves all symbols.

---

## 3. Phase 1 — Decompilation infrastructure

Goal: after this phase, recovering a function is a mechanical loop (§4).
Do the steps in order. Each step is one or a few commits.

### 3.0 [x] Rebuild the Ghidra workspace reproducibly (do this first)

`local/` is git-ignored, so a fresh checkout has **no Ghidra project**. The
4,233-function state described in `docs/ghidra-findings.md` was reached
through many separate passes (bundles #041–#056), several of them run with
`--apply` in different disposable workspaces (`local/ghidra-recovery`,
`local/ghidra-entries-recovery`, `local/ghidra-undisassembled`,
`local/ghidra-call-recovery`). There is no single command that reproduces it.

1. Read `docs/ghidra.md` and the #041–#056 sections of
   `docs/ghidra-findings.md`; list every `--apply` pass, its flags, its order
   and the function count it reported.
2. Add `ef2000-ghidra rebuild [--workspace DIR]`: import the SHA-pinned EXE,
   then run all applying passes in that order in **one** workspace
   (`local/ghidra` by default), then `inventory`, `export-all
   --custom-storage-fallback` and `function-facts`. Refuse to run on an
   existing project unless `--force` is given.
3. Acceptance: the rebuilt project reports the recognized-function count
   recorded for the flake's Ghidra release (**4,277** with Ghidra 12.1.4;
   4,233 with 11.4.2), the same number of pseudo-C exports, and the nine
   `repair-boundaries` recipes report as applied. If counts differ, record
   the difference and its cause in `docs/ghidra-findings.md` before
   continuing; do not paper over it.
4. Add an asset-free test for the pass ordering (like the existing
   `tests/test_ghidra_ef2000.py`).
5. Later, step 3.3 (`apply-symbols`) becomes the last stage of `rebuild`.

Done 2026-11-03: `ef2000-ghidra rebuild EXE` (see `docs/ghidra.md`) runs the
full sequence in about four minutes; results in `docs/ghidra-findings.md`
#062. It never overwrites an existing project; use `--workspace` for a new
one. Re-run it after a Ghidra upgrade and update the expected count
(`DOCUMENTED_FUNCTIONS` in `tools/ghidra_ef2000.py`) with evidence.

### 3.1 [x] Repository layout and licensing scaffolding

Create:

```
decomp/
  README.md            # what this is, legal note, how to build, status summary
  LICENSE-NOTE.md      # derived from proprietary EF2000; not GPL; owner decides distribution
  symbols/
    functions.tsv      # THE tracker (see 3.2)
    globals.tsv        # data symbols: address, size, type, name, evidence
    signatures.tsv     # confirmed ABI per function (regs in/out/clobbered, stack bytes)
  include/
    ef_types.h         # fixed-width types, packed-struct macros, Watcom pragmas
    ef_abi.h           # #pragma aux definitions for non-standard conventions
    ef_globals.h       # extern declarations for all named globals
    ef_structs/*.h     # one header per recovered struct family
  src/                 # recovered C, one file per original module where known
                       # (gui.c, views.c, aid.c, mapdraw.c, avionics.c, parser.c, ...)
  compat/              # reproductions of library quirks (rand, printf variants)
  tests/               # differential test specs (YAML/JSON) per function
  build/               # (ignored) OpenWatcom outputs
```

- Licensing is in place (2026-11-03): `decomp/**` is annotated in
  `REUSE.toml` as `LicenseRef-EF2000-Derived`
  (`LICENSES/LicenseRef-EF2000-Derived.txt`), and all GPL code carries
  `GPL-3.0-or-later WITH GPL-3.0-linking-exception`, which permits combining
  it with `decomp/`. Give every new file in `decomp/` the
  `LicenseRef-EF2000-Derived` header and every new host-side file the GPL
  expression with the exception. Do not publish or push `decomp/` content
  anywhere on your own initiative; pushing is the owner's decision.
- Module naming: use original file names from assert/error strings when the
  function's evidence links it to that string. Otherwise use
  `src/unk_<subsystem>.c` and rename later.

### 3.2 [x] Function tracker (`decomp/symbols/functions.tsv`)

Columns (tab-separated, header row, sorted by address):

```
addr  size  name  module  state  kind  sig_status  test  notes
```

- `state`: `unknown` → `named` → `typed` → `drafted` → `compiles` →
  `spliced` → `verified`. Also `blocked`, `library`,
  `data` (misidentified), `dead` (unreferenced and unreachable, proven).
- `kind`: `unclassified` (default until step 3.6 runs), `game`, `clib`,
  `x32`, `ail`, `vbe`, `thunk`, `asm` (hand-written assembly in original).
- `sig_status`: `unverified` or `confirmed`. `typed`, `compiles`, `spliced`
  and `verified` require `confirmed` (with the row in `signatures.tsv`).
- Seed it from the Ghidra inventory (`ef2000-ghidra inventory` →
  `functions.tsv`). Addresses and sizes are facts and may be committed;
  pseudo-C is not.
- Write `tools/decomp_status.py`:
  - `summary` — counts per state/kind/module, bytes covered, % verified.
  - `next [--module M] [--leaf-first]` — proposes the next function: all
    callees already `verified` or `library`, smallest first, within the
    chosen subsystem.
  - `check` — validates the TSV (sorted, no overlaps, states legal, every
    `verified` row has a test file, every `compiles`+ row has a source).
  - Add `check` to `nix flake check` (asset-free).

  Done 2026-11-03: `tools/decomp_status.py` (`seed`, `summary`, `next`,
  `check`) with `tests/test_decomp_status.py`; the committed tracker is
  validated by that test under `nix flake check`. Seeded with all 4,277
  functions; 294 carry evidence-backed names (original callback names and
  the documented high-score/startup routines, module `hiscore` for the
  former). `decomp/symbols/globals.tsv` holds the 11 documented globals;
  `signatures.tsv` is empty because no ABI has been confirmed yet.
  Re-seed after every future Ghidra boundary change (recurring maintenance,
  not an incomplete acceptance criterion). Re-seeded against the independent
  12.1.4 rebuild on 2026-11-03: 0 added, 0 changed.

### 3.3 [x] Reproducible Ghidra annotations

Right now names/types live only in the local Ghidra project. Make them
reproducible from git:

- `tools/ghidra/EF2000ApplySymbols.java`: reads `functions.tsv`,
  `globals.tsv`, `signatures.tsv` and parses `decomp/include/**/*.h` with
  Ghidra's `CParser`; applies names, struct types, global types and custom
  storage signatures. Idempotent.
- `tools/ghidra/EF2000ExportSymbols.java`: dumps names the human/agent created
  in the GUI back into the TSVs (for merge review, never auto-commit).
- Wire as `ef2000-ghidra apply-symbols` / `export-symbols`. Re-run
  `export-all --overwrite` after applying, so pseudo-C uses recovered names.
- Test: synthetic-binary test like existing `tests/test_ghidra_ef2000.py`.

Done 2026-11-03: `apply-symbols` reads the three reviewed TSVs and parses
struct headers with Ghidra `CParser`; applies names, typed globals and
explicit register-storage signatures. `export-symbols` writes review-only
TSVs, marking GUI-derived ABI candidates as unverified. `rebuild` applies
symbols before exporting pseudo-C. Asset-free CLI tests plus a live synthetic
Ghidra round trip verify that names, custom storage, global types and parsed
structs persist; applying twice is idempotent. On the real 12.1.4 project,
the second import made zero changes and full re-export completed with 4,277
functions and zero decompiler failures. Known unconfirmed layouts remain raw
bytes; conventions outside the current register/zero-cleanup schema are
rejected pending §3.5.

### 3.4 [x] OpenWatcom in the flake

- Add `open-watcom-v2` (and `open-watcom-bin` 1.9 for comparison) to the dev
  shell. Export `WATCOM`, `INCLUDE`, `PATH` correctly (OpenWatcom needs
  `WATCOM` set and `binl64`/`binl` on PATH).
- Add `ef2000-wcc` wrapper that compiles one `decomp/src/*.c` file with the
  project's canonical flags into `decomp/build/<name>.obj`.
- Smoke check in `nix flake check`: compile a trivial `decomp/tests/fixture.c`
  and inspect the OMF with `wdis`.

Implementation prepared 2026-11-03: the flake includes wrapped OpenWatcom v2
and 1.9, an `ef2000-wcc` compiler wrapper, and a `watcom-omf` check that runs
`wcc386` on `decomp/tests/fixture.c` and disassembles the object with `wdis`.
Bootstrap flags are provisional until §3.5. Python wrapper tests and C89
fixture syntax checks pass locally. The owner verified on NixOS that
`NIXPKGS_ALLOW_UNFREE=1 nix build --impure .#checks.x86_64-linux.watcom-omf -L`
passes and `ef2000-wcc decomp/tests/fixture.c` emits a 12-byte OMF function
disassembled successfully by `wdis` (2026-11-03). The emitted `__CHK` call
is a compiler runtime dependency to address under §3.5; it does not invalidate
the toolchain smoke check.

### 3.5 [x] Determine the ABI conventions and canonical compiler flags

Binaries do not need to match (§2.2), so this step is about **ABI and data
layout**, not code generation. Do it before writing real game code.

1. Collect ~40 functions of different shapes (leaf, 1–3 calls, x87 code,
   `switch` jump tables, struct accesses, string loops) from different
   address regions.
2. Determine for each: calling convention (register vs. stack, callee
   cleanup, which registers are preserved), return-value registers, x87
   argument/return handling, and segment-register assumptions.
3. Determine struct packing from field offsets used by code. The high-score
   record is packed at 25 bytes and a GUI icon `next` pointer sits at the odd
   offset `+0x21`, so `-zp1` (or `#pragma pack(1)`) is likely for at least
   those structs. Struct layout must reproduce RAM byte for byte.
4. Choose one OpenWatcom version (prefer OW 2.0; keep 1.9 only if 2.0 fails
   on something) and a canonical flag set: register calling (`-5r` or
   `-3r`), `-zp1` unless evidence says otherwise, an x87 mode matching the
   game's FP control word, optimizations enabled but `-fp`/`-o` options that
   reorder floating point disabled if they change results.
5. Write `tools/asm_diff.py`: disassemble original bytes and compiled OMF
   code with relocated operands shown symbolically, side by side. Debugging
   aid only (§2.2).
6. Record the ABI rules, pragmas and flags in `docs/compiler-abi.md`,
   `decomp/include/ef_abi.h` and the `ef2000-wcc` wrapper.
7. Watch for functions that were clearly hand-written assembly (unusual
   register use, `rep movsd` blocks, self-modifying code, `in/out`). Mark
   `kind=asm`. Prefer rewriting them in C when the observable behavior
   (§2.2) can be reproduced; otherwise keep them as `wasm` assembly.

In progress 2026-11-03: the 40-candidate cross-region worksheet and
two-caller instruction audit live in `decomp/symbols/abi-samples.tsv` and
`abi-audit.tsv`; reviewed evidence and canonical OpenWatcom 2.0 flags are
recorded in `docs/compiler-abi.md`. The `-3r -zp1 -fpi87 -fp3 -ob -ot -s`
first-pass profile and its OMF fixture are in the flake. The Ghidra 12.1.4
symbol importer supports custom register/stack storage, 80-bit x87 ST(0)
storage, and callee `ret N` purge. **35 of 40** sampled ABIs are confirmed (but not C-recovered or
replay-verified), including the large indexed projection/global update
(`0x2461c`), signed projected-position query (`0x86f7c`), node initializer
(`0xb0f5c`), command-dispatch loop (`0xf3b78`) and register-plus-qword-stack
x87 overlap check (`0x24480`). The x87 ST(0)-return helper (`0xf8250`) and
ES-specific non-popping ST(0) store (`0x120a52`) now have explicit 80-bit
register contracts. The other five samples have been reviewed as exceptions
or partial ABIs: indirect callback/formatted-output clobbers (`0x12d90`,
`0xabec4`, `0xf6124`) and DOS segment/interrupt bridges (`0xee43c`,
`0x120451`). Resolve them before full native replacement or explicitly
isolate them as assembly/platform shims.

ABI sampling closed for this phase: **all 40 sampled functions were
reviewed**; 35 have imported custom-storage signatures and five have
bounded exception reports instead of a guessed ABI. Their explicit
indirect/segment dependencies make them ineligible for automatic C
replacement until platform shims or runtime evidence resolve the missing
contracts. This completes the representative ABI survey, not the
function-by-function ABI validation required by §1(4).

### 3.6 [~] Library identification

1. Build OW 1.9 and OW 2.0 `clib3r`/`clib3s`/`math387r` libraries' function
   bytes into signature files (first N bytes with relocations masked, plus
   length). Also check whether the Watcom 10.x runtime differs.
2. Write `tools/flirt_match.py` (or use Ghidra Function ID with a generated
   FID database) to match signatures against the image.
3. Mark matches `kind=clib` with the library function name. Expect several
   hundred: `strcpy` (`0x9d749`), `stricmp`-like (`0x9d524`), `memcpy`
   (`0xe9a24`, note: non-standard EAX/EDX postconditions — maybe game asm),
   `strlen` (`0xb1b8b`), `printf` family, file I/O, `malloc`, x87 helpers.
4. Identify the X-32 interface stubs (around startup `0x86`, extender calls
   documented in `docs/x32-services.md`) → `kind=x32`.
5. Identify AIL/Miles API functions by their documented export names in
   strings and by call patterns into the loaded `.DIG` driver → `kind=ail`.
6. Commit only names/addresses/kinds; never commit library bytes.

Expected outcome: 15–30 % of functions labeled as library code before any
game code is written.

In progress 2026-11-03: `tools/flirt_match.py` reads extracted OMF objects,
tracks LEDATA/FIXUPP relocations and emits masked code-prefix fingerprints.
It matches only inventoried function entry addresses and writes a review-only
JSON proposal, never editing the canonical tracker. Three synthetic OMF
unit tests cover 32-bit relocation masking, validation and the CLI; the
workflows and limitations are in `docs/library-identification.md`. Actual
OW 1.9/2.0 library extraction, empirical matches and independent 10.x
verification remain to be done. No OMF-derived library classifications are
claimed yet.
Three startup/runtime entries (`0x86`, `0x9bd20`, `0x9bd70`) have since been
classified from independent Watcom startup-source and call-path evidence,
with their uncertainty recorded in `docs/library-identification.md`; these
are **not** OMF fingerprint matches. Automated library identification remains
unverified against real extracted library objects.

### 3.7 [~] OMF splicer (`tools/omf_splice.py`)

Turns OpenWatcom object files into patches against the guest image.

- Parse 32-bit OMF records: `THEADR`, `LNAMES`, `SEGDEF`, `GRPDEF`, `PUBDEF`,
  `EXTDEF`, `LEDATA`, `LIDATA`, `FIXUPP` (thread + explicit fixups, self- and
  segment-relative), `COMDAT` (OW may emit these for `-zm`).
- Resolve `EXTDEF` symbols from `functions.tsv` + `globals.tsv`
  (name → guest offset). Unknown externals are a hard error.
- For each `PUBDEF` function that matches a tracker row:
  - If the new code fits in the original size: write it at the original
    address, pad with `int3` (0xCC).
  - Otherwise: place it in a **patch arena** and write `jmp rel32` at the
    original entry, pad the rest of the old body with `int3`.
- Static data defined in the object (string literals, constant tables) goes
  into the patch arena's data section. **Recovered functions must reference
  original global variables at their original addresses** (via externs), not
  fresh copies, until the full build in Phase 4.
- Output: a patch file (`.efpatch`, simple documented binary or JSON+bin:
  list of `{offset, bytes}` plus arena image + arena base) and a report.
- Refuse to patch functions marked with cross-function jumps or body holes
  (`audit-flow-gaps`, `audit-body-holes` results) unless the cluster that
  shares the code is replaced together.
- Asset-free tests with synthetic images and synthetic OMF created by
  compiling fixtures with `wcc386` in the flake check.

First stage (2026-11-03): `tools/omf_splice.py` inspects 32-bit OMF and
can produce a **non-runnable offline `.efpatch` plan** for a narrow, explicitly
reviewed, single-function object with 32-bit relocations. It computes
self-relative and segment-relative fixups and lays out inline or redirected
code and private data, but deliberately refuses unsupported relocations,
unconfirmed ABIs and functions without a reviewed body/edge boundary.
Seven asset-free synthetic tests cover the parser, relocation resolution,
patch plans, rejection rules and CLI. See `docs/omf-splicer.md` for scope;
real compiler-object coverage, multi-object linking, arena verification and
emulator integration are still pending. The current output is **not** ready
to apply to a guest.

In-place-only packaging stage (2026-11-03): `tools/omf_inplace.py` takes a
compiled, explicitly reviewed single-function OMF plus the SHA-pinned
original executable, resolves the object and emits `EFPATCH1` directly.
It refuses all arena/redirect/data writes, ambiguous externals, unconfirmed
ABI or a tracker below `compiles`. Synthetic full-chain tests cover relocation
through binary patch decoding, and optional Python Unicorn guest comparison.
The draft `0x432c` still needs actual OpenWatcom object and game evidence.

### 3.8 [~] Harness support for patches

- Native64 (`tools/run_startup.py` / `runtime/startup.c`) and the C++ Unicorn
  runner get `--patch FILE` (repeatable). Apply after loading and relocating
  the image, before entry.
- Patch arena: reserve a region inside the 64 MiB guest image model that the
  original game provably never uses (verify heap high-water mark from runs:
  log the maximum `brk`/allocation end; pick a region above it with a safety
  margin, or extend the reservation to e.g. 96 MiB and place the arena at
  64 MiB). Document the chosen range in `docs/startup-runtime.md`.
- Log every applied patch (function name, address, size) to the run directory.
- Save states must record the applied patch set and refuse to load into a
  run with a different set.

First stage (2026-11-03): `tools/efpatch.py` turns reviewed *in-place only*
OMF JSON plans into SHA-bound binary `EFPATCH1` writes. Native64/QEMU and C++
Unicorn accept repeatable `--patch` and validate the complete patch set
before altering a private guest image; arena/redirected plans are refused
until memory high-water measurements make a reservation defensible. Native
save-state identity now incorporates the ordered patch set, preserving prior
unpatched identities. Four Python and one C++ asset-free tests cover format
validation, bounds, overlap and digest rejection. See `docs/omf-splicer.md`.
No game-function differential or end-to-end patched run has yet been made.

Allocation-watermark instrumentation (2026-11-03): C++ Unicorn reports
`x32_memory` at exit; native64/QEMU writes `MEMORY` to `calls.log` and
`memory_watermark` to the launcher report. Both track `3508h` heap growth,
`3509h` stack reserve and call counts, without changing the 64 MiB layout.
They are observations, not yet proof of a safe arena. The owner confirmed
the unpatched `nix run .#ef2000-unicorn -L ...` startup reaches its 30M
instruction limit (42.3M guest instructions/s on that run); no real patch
has been executed. See `docs/function-differential.md`.

### 3.9 [~] Golden replay suite

The first bounded original-vs-patched framebuffer/selected-memory replay checker
and patch entry-hit instrumentation are implemented (`tools/replay_suite.py`;
see `docs/golden-replay.md`). Batch 100 added independent reruns ending at
multiple instruction checkpoints, including a RAM-hash-only allowance for
early startup before VGA mode setup. The full suite still needs in-process
state checkpoints, palette hashes and an asset-backed, real-function patch
replay; the current identity-patch experiment is only a plumbing check.
The full scope below remains outstanding.

- Define 6–10 deterministic scenarios as files in `tests/replays/*.json`
  (input script + instruction limit + checkpoints). Minimum set:
  1. Boot to main menu (640×400) — framebuffer + palette hash.
  2. Navigate options menus and high-score screen.
  3. Quick Combat mission 01 to cockpit, then 30 s of scripted flight with
     roll/pitch/throttle input.
  4. External view cycling (F-keys), map screen, HUD modes.
  5. Weapon release and a kill in a quick-combat mission.
  6. Campaign start, briefing, TACTCOM map interactions.
  7. Save → load roundtrip (native64 `--sound off`).
  8. Sound-enabled run with PCM capture hash (`--wav-capture`).
- Each checkpoint records: instruction count, framebuffer SHA-256, palette
  SHA-256, and hashes of selected memory regions (initially the full
  initialized-data + BSS range minus regions proven to contain timing noise;
  record the excluded ranges explicitly).
- `tools/replay_suite.py record|verify [--patch ...]`. Golden hashes go in
  `local/` (they derive from copyrighted data); the scenario definitions are
  committed.
- Determinism first: verify that two unpatched runs produce identical hashes.
  Any nondeterminism (host time, wall clock pacing, uninitialized host data)
  must be eliminated or documented before this suite becomes a gate.

### 3.10 [~] Generalized differential function tester

Generalize `tools/highscores_differential.py` into
`tools/func_differential.py`:

- Input: function address, signature (from `signatures.tsv`), a test spec in
  `decomp/tests/<addr>.json` describing input generators (register value
  ranges, memory regions to randomize, pointers to construct, callee stubs).
- Runs the original function in a fresh Unicorn guest built from the pinned
  image, then the same with the patch applied, both from identical states.
- Callees: either run the original callee code (default), or stub them with
  recorded behavior when they reach unmodeled services (file I/O, video).
- Compares: output registers, `EFLAGS` bits the ABI cares about (usually
  none), every written memory byte, x87 stack/status if FP is involved,
  call sequence to stubs.
- Also supports **trace-captured inputs**: run a golden replay, snapshot the
  guest state at the first N calls of the function (`--capture-calls ADDR[:N]
  --capture-dir DIR` in the C++ harness), replay those states through both implementations.
  This is the most valuable input source; use it whenever the function is
  reached in a replay.
- Reports coverage: which basic blocks of the original function were hit
  (Unicorn block hook). Target ≥ 90 % block coverage or document why not.

First isolated-call stage (2026-11-03): `tools/func_differential.py` reads
the confirmed ABI tracker and SHA-pinned payload, runs spec-defined calls
with an in-place EFPATCH1 replacement on identical 32-bit Unicorn machines,
and checks ABI outputs, preserved registers, stack cleanup and the union
of runtime-written guest RAM (excluding dead callee scratch). It records
original function dynamic block entries, not a speculative static coverage
percentage. A synthetic code-pair differential test is wired into the
dedicated Nix check; on the minimal local host it skips execution because
Python Unicorn is absent. x87, guest hardware services, stubbed callees,
real compiled functions and golden replays remain missing. Live-call capture
and its isolated replay were added in the stage documented below.
See `docs/function-differential.md`.

Live-call capture stage (2026-11-03): the C++ Unicorn runner accepts
`--capture-calls ADDR[:N] --capture-dir DIR` and writes at most four
`EFCAP001` original guest-entry snapshots per run. They retain 64 MiB
guest RAM, original-image SHA and 13 register values (proprietary, local
only), with fail-closed bounds and no replacement code active while recording.
`tools/func_differential.py --captured FILE` restores the memory/register
context and overlays only the exact patch writes in its second run.
Synthetic C++/Python wire-format tests pass; actual capture/replay against
the pinned executable, x87/segment descriptors, hardware side effects,
callee stubs and static block coverage remain outstanding.

First C candidate (2026-11-03): `decomp/src/hiscore.c` implements the
already-confirmed `hiscore_qualifies` at `0x432c` with signed 32-bit score
comparison and a byte return. `decomp/tests/0x000432c.json` supplies ten
boundary states; the host-model C test and spec validation pass. The tracker
marks this function `drafted`, **not** `compiles` or `verified`: OpenWatcom
object inspection, a safe splice and live guest differential/replay are still
required. The `watcom-omf` check will compile the candidate when next run.

Original-binary audit and second C batch (2026-11-03): restored the owner's
SHA-pinned `_EF2000.EXE` to an ignored `local/` path, extracted its code, and
checked the complete `0x432c`, `0x263e0`, `0x263ec` and `0x6a1b4` bodies
against the ABI notes. The spatial reset has callers `0x1f624`/`0x3ab19`;
the adjacent 29-entry spatial append has **four** distinct callers, a newly
confirmed AX/DX/BL ABI and matching record layout. The indexed-object
lookup has callers `0x24087`/`0x247af`. The three additional C drafts live
in `decomp/src/spatial.c` and `decomp/src/indexed_objects.c`. Each has native
C tests and multiple guest differential input states covering memory
adjacency, capacity limits, signed AX bounds and junk high EAX bits. All four
remain `drafted`; a local host build or inspected disassembly is **not** an OpenWatcom replacement
differential or full-game replay. The `watcom-omf` Nix check now compiles and
disassembles the three additional candidates in the owner's environment.

Filtered object-state batch (2026-11-03): five more C drafts cover the
`0x7489c` guarded pointer lookup and four byte-flag wrappers (`0x6a354`,
`0x6a378`, `0x6a394`, `0x6a3d0`), each with at least two confirmed direct
callers and ABI signatures. Recovered separate record flags at `0x128636`
with a `0xe5` stride, an excluded index at `0x1f71d0`, and the diagnostic
counter at `0x1a3580`. Host-model tests and isolated guest specs exercise
sentinels, signed bounds, disabled records and counter wraparound. The lookup
original crosses into a **shared epilogue at `0x6a1ac`**: its Ghidra body
size is noncontiguous and must not be treated as an in-place overwrite length.
All five remain `drafted`; the matching Watcom objects and guest verification
are still outstanding. The current workspace has no
`local/ghidra/full-export/calls.tsv`, so `decomp_status.py next` could not
rank these candidates; selection and caller checks used direct disassembly
of the pinned executable. Reacquire the Ghidra pseudo-C review before any
splice approval.
The existing in-place OMF converter also accepts only one code public per
object; this cluster's multi-function `object_state.c` requires function-wise
object extraction or isolated compilation before actual splice testing.

World-grid/object-control batch (2026-11-03): six more drafted functions
(`0x2b884`, `0x2b910`, `0x2b95c`, `0x2ba14`, `0x2541c`, `0x238a0`) connect
the previous object-state wrappers to 0xca-byte world-grid rows and guest
event dispatch. All six have original instruction and at least two caller
checks, per-function planned guest states and an asset-free combined C test.
The signed high word at grid row offset `+0x4e` is an object index; event
dispatch at `0x46168` takes four register arguments plus one outgoing stack
word and returns with `ret 4`. Unrecovered event/mode/predicate callees are
kept as explicit dependencies (stubbed only in the host test). Neither
OpenWatcom compilation, safe splicing nor original-game replay was possible
in this chat sandbox, so all six remain **drafted**. The missing Ghidra
pseudo-C export must be restored before splice approval.

Second world-grid batch (2026-11-03): another six drafted functions
(`0x2b558`, `0x2beb0`, `0x2bf08`, `0x2dba4`, `0x2e030`, `0x2e910`)
recover forward/reverse cell links, full seven-slot attached-object scans,
mode-filtered variable-count scans, current/external selection, two-dword
state reset and an AL/DL event-code predicate. Original call sites come
from at least two distinct caller functions per routine. The adjacent
grid-row fields +0x13/+0x19/+0x20/+0x22/+0x24/+0x26 and +0x91..+0xa0
have provisional layout names and static offset assertions. A new synthetic
host C test covers all 65,536 predicate inputs and verifies that successful
object matches do **not** skip later filtered lookup/counter side effects.
The five delegated grid helpers remain explicitly unresolved; OpenWatcom
OMF, live guest differential and full replay are still pending. Six new
guest fixtures and flake smoke-check hooks are test **plans** only, so the
new functions remain `drafted`, never `compiles` or `verified`.

World-grid dependency batch (2026-11-03): five more drafted functions
(`0x2afcc`, `0x2b5c0`, `0x2b634`, `0x2b690`, `0x2e08c`) recover a 171-byte
link-cleanup state machine, 69-byte external-node linkage, coordinate copying,
integer-to-double scaling, and the original seven-object limit scan. Each
has two independently identified caller functions; the external predicate
`0x2db64` has only one direct caller and was **not** promoted to a confirmed
signature. The packed world grid now names positions at +0x27..+0x3e and
+0x61..+0x78; external-node offsets +0x01/+0x07/+0x0b/+0x0f/+0x44, type
limit doubles and scaling globals have static layout checks/explicit addresses.
Synthetic host tests cover link cleanup/back-reference, coordinate transfers,
seven filtered lookups even after matches, and unordered floating comparison.
Real x87 extended precision, original dispatcher `0x2b1f4`, OpenWatcom OMF,
safe splice and golden replay remain pending. All five are `drafted` only.

World-grid neighborhood/category batch (2026-11-03): seven more drafted
functions (`0x2a934`, `0x2bb3c`, `0x2bbd0`, `0x2bc3c`, `0x2c0ec`,
`0x2da54`, `0x2dce0`) cover linked-node recycling, 17-byte adjacency
lists, two-dimensional distance, cross-object relationships, a 20-feature
priority classifier and externally gated visibility. All seven have original
disassembly and at least two distinct caller functions, explicit ABI rows,
synthetic host assertions and per-function planned guest input states.
`0x2dce0` still calls two unrecovered distance predicates; `0x2bb3c` and
`0x2bc3c` have x87 ST0 outputs whose precision is untested. Missing Ghidra
pseudo-C, OpenWatcom compilation and guest splicing prevent promotion beyond
`drafted`; the original emulator is unchanged.

### 3.11 [ ] Phase-1 exit criteria

- `nix flake check` passes with all new asset-free tests.
- The high-score routines (`0x4050`, `0x41fc`, `0x42a4`, `0x432c`, `0x433c`,
  `0x40dc`, `0x4c50`) are rewritten as C in `decomp/src/hiscore.c`, compiled
  with OpenWatcom, spliced into the running game, and pass both the
  differential tester and the full golden replay suite. (The existing C++
  model in `runtime/ef2000_highscores.*` remains as an independent check.)
- `tools/decomp_status.py summary` shows those as `verified`.

---

## 4. The operating loop (per function or cluster)

Follow these steps exactly. Each loop ends in a commit.

1. **Pick.** `python3 tools/decomp_status.py next --module <current>`.
   Prefer functions whose callees are all verified/library. If the proposal
   is blocked by a boundary problem, run `ef2000-ghidra audit-flow-gaps` /
   `audit-body-holes` for that address and resolve the boundary first (that
   is its own commit, updating `functions.tsv`).
2. **Read the original.** Get Ghidra pseudo-C (`local/ghidra/full-export/...`)
   *and* the disassembly. The disassembly is authoritative.
3. **Determine the signature.** Which registers are read before written
   (inputs)? Which are written and later read by callers (outputs)? Which are
   preserved (push/pop)? Stack arguments and `ret N`? Check at least two
   callers. Record in `signatures.tsv`. If it is not plain `__watcall`, add a
   `#pragma aux` in `ef_abi.h`.
4. **Name and type.** Name the function (verb-noun, or the original name if
   recovered from a callback table/string). Name any newly touched globals in
   `globals.tsv` and create/extend struct headers in `include/ef_structs/`.
   Add `static_assert`/`offsetof` checks for every struct field offset you
   rely on (use a compile-time negative-array trick for Watcom C89).
5. **Write C.** In the right `decomp/src/<module>.c`. Rules:
   - No Ghidra artifacts (`undefined4`, `CONCAT`, `in_EAX`, `unaff_`).
   - Use fixed-width types from `ef_types.h`.
   - Keep the original control flow shape where it affects behavior
     (short-circuit evaluation, loop exit conditions, signed vs. unsigned
     compares).
   - Comment the original address above each function:
     `/* 0x000433c  size 0xc0  verified: replays+diff */`.
6. **Compile.** `ef2000-wcc decomp/src/<module>.c`. Zero warnings.
7. **Diff assembly** (optional, only when hunting a divergence):
   `python3 tools/asm_diff.py <addr> decomp/build/<module>.obj`.
8. **Differential test.** Write `decomp/tests/<addr>.json`. Run
   `tools/func_differential.py <addr>` with random and trace-captured inputs.
   Fix until identical. Never "fix" the test to accept a difference without
   proving the difference is unobservable (e.g. a dead scratch register).
9. **Splice and replay.** Build the patch set of *all* verified functions
   (not just this one) and run `tools/replay_suite.py verify --patch ...`.
   All checkpoints must match. If a mismatch appears, bisect by patch subset
   (`--patch-only ADDR,...`).
10. **Update tracker** (`state=verified`, test path) and commit.
11. Every ~20 functions: run the full suite with sound enabled, update
    `decomp/README.md` status numbers, and update §9 progress log below.

When stuck (step 3 or 8 does not converge within the time-box):
set `state=blocked`, write the specific observation in `notes`, commit the
tracker change alone, and pick the next function.

---

## 5. Phase 2 — Data formats (parallel track)

Data-format work can proceed alongside Phase 1/3 and directly feeds struct
recovery. Each format gets: `docs/formats/<name>.md` (with the evidence rules
from `docs/research.md`), a read-only Python reader in `tools/formats/`,
asset-free synthetic tests, an opt-in test against the real installation,
**and a link to the original game routine that reads it** (address + name in
`functions.tsv`). The game's reader is the ground truth; a format is only
`complete` when every field the reader touches is named and typed.

Status table (update as you go):

| Format | Files | Known from | Status | Next action |
| --- | --- | --- | --- | --- |
| DID archive index + RA compression | `DID.DAT`, `DID1ST.DAT` | `tools/did_archive.py` | [x] reader | Link to original loader `0xea270` (clump/serclump); document hash function evidence |
| Clump lists | `CLUMP_3.TXT`, `CLUMP_SS.TXT` | ghidra-findings | [ ] | Trace `0xea270` descriptors, document clump semantics (preload groups?) |
| Images ILBM/PBM/PCX/BMP | `IFF/`, archive members | `tools/*_image.py` | [x] | — |
| Sprites `.spr`, fonts | archive | `tools/spr_image.py`, menu_font | [~] | Find glyph-metrics struct in code |
| GUI scripts `.gui` | archive | `docs/gui-assets.md` | [~] | Map parser (`parser.c`/`tokenise.c`/`guimain.c`) functions; full grammar |
| **3D shapes `.3`** | archive (`3/*.3`) | `../tfx3view/libTFX/ADF Shape*`, `EF2000 Shape.hpp` | [ ] here | Port knowledge: write `docs/formats/shape3.md` + Python bytecode disassembler; locate the game's shape interpreter in the EXE (search for a dispatch table whose opcode count matches libTFX `executeBytecode`) |
| Supershapes / LOD lists | archive | `EF2000 SupershapeList.*` | [ ] | Same; link to loader |
| Target lists | archive | `EF2000 TargetList.*`, `loadtarg.c` string | [ ] | Find `loadtarg.c` functions via assert string xrefs |
| World / terrain grids | archive | `EF2000 World.*`, `tools/probe_did_grids.py` | [~] | Merge tfx3view world knowledge with the 40K grid audit; find terrain renderer |
| Palettes / shading tables | archive | tfx3view `COL.hpp` | [ ] | Document palette + light/fog lookup tables used by renderer |
| Missions | `MISSION.CFG`, `MISSIONS/` | `tools/mission_records.py`, `docs/mission-*.md` | [~] | Enum meanings, units, cross-file refs |
| Navigation | `NAVIG.NAV` | same | [~] | Units of coordinates (same space as world?) |
| Campaign | `CAMPAIGN.CG`, `CONFLICT.CG`, `CAMPBRF.BRF` | — | [ ] | Find readers (`cfg_read.c`/`parsecfg.c` family) |
| Briefings / text | `SUM*.TXT`, `.BRF` | — | [ ] | — |
| Config | `EF2000.CFG`, `MDI.INI`, `DIG.INI` | `docs/configuration-research.md` | [~] | — |
| Save games | `TFXSAV.99` | — | [ ] | Find save writer; diff saves made at controlled points |
| High scores | `HISCORES.DAT` | `docs/highscores-reconstruction.md` | [x] | — |
| Sound effects / samples | archive | `docs/sound-research.md` | [ ] | Locate sample table; document format/rate |
| Music | `.OPL`, `.AD`, XMI? | — | [ ] | Low priority |
| Video / animations | archive / CD | `playvideo` branch | [ ] | Low priority |
| Cockpit / HUD art and layout | archive | — | [ ] | Needed for widescreen HUD layout (§8.3) |

Method for finding readers: (1) run a golden replay with the existing archive
request trace (`tools/resolve_archive_trace.py`) and DOS file-open logging,
recording the guest call stack (return addresses) at each open/read; (2) the
innermost game (non-clib) frame is the reader; (3) mark it in
`functions.tsv` with `module` = format name; (4) decompile it through §4 —
the recovered C *is* the format specification.

Also add a `tools/formats/dump_all.py` that, given an installation, extracts
every format into an output dir (JSON/PNG/OBJ/glTF) — useful for modders and
for checking coverage (every archive member classified).

3D model export: once `shape3` is documented, emit glTF 2.0 with palette
colors/textures. Validate visually against `../tfx3view` and in-game renders.

---

## 6. Phase 3 — Subsystem decompilation waves

Order chosen to unlock the end goals as early as possible. Within a wave use
the `subsystem-map` affinity groups (`ef2000-ghidra subsystem-map --seed`)
to find related functions. Do not jump waves unless blocked.

| Wave | Subsystem | Why now | Seeds / entry points |
| --- | --- | --- | --- |
| 1 | High scores (Phase-1 exit), string/utility helpers that are *game* code | Proves the pipeline | `0x4050`…`0x4c50` |
| 2 | Startup, `main`, init sequence, main loop, timing and frame pacing | Needed to decouple simulation from rendering (high FPS) | `0x9bd20`, `0x8b274`, `0x8b39c`, `0xaaee8`, PIT/IRQ0 handler |
| 3 | Resource loading: archive, clumps, decompression, config parsing | Ground truth for data formats | `0xea270`, archive hash/RA routines, `cfg_read.c`/`parsecfg.c`/`parser.c`/`tokenise.c` |
| 4 | Video output layer: mode set, palette, page flip, VGA/VBE blits | Becomes the platform layer boundary | `0xc3886`, `0xc3dc0`, `0xc3f38`, dispatch slot `[0x41f3d0]` |
| 5 | 3D renderer: camera/projection, transform, clipping, shape bytecode interpreter, rasterizers (flat/gouraud/textured), terrain, sky, sorting | Required for widescreen and GPU backend | Divide-overflow recovery around `0xc5a7c`, shape interpreter (from §5), hot blocks from `--profile-hot-blocks` in flight |
| 6 | Input: keyboard map, joystick calibration, mouse | Platform layer | gameport/keyboard handlers |
| 7 | Sound: AIL API usage, sample triggers, CD audio control | Platform layer | AIL calls, MSCDEX calls |
| 8 | GUI system: `.gui` interpreter, windows, icons, callbacks (`gpw*`), TACTCOM | Large, self-contained | `Gui.c`, `guimain.c`, callback tables |
| 9 | Flight model, weapons, damage | Core sim | flight hot blocks; joystick→control surface data flow |
| 10 | AI, avionics, radar, `Aid.c`, `Views.c` | Core sim | assert strings |
| 11 | Missions, campaign, `MAPDRAW.C`, briefings | Ties to §5 data | mission readers (`docs/mission-readers.md`) |
| 12 | Multiplayer: `ipx.c`, `Serialc.c` | Last; may be stubbed in modern build | — |
| 13 | Remaining orphans, dead code, data-as-code cleanups | Completeness for Phase 4 | `orphan-ranges.tsv` |

Per wave:
- Start by profiling: run the relevant golden replay with
  `--profile-hot-blocks` and the call-capture tool to list which functions in
  this subsystem actually execute, in what order, how often.
- Document the subsystem in `docs/subsystems/<name>.md`: data structures,
  call graph, global state, open questions. Update as functions land.
- Finish the wave when ≥ 95 % of the wave's bytes are `verified` and the rest
  is `blocked` with notes.

Renderer-specific notes (wave 5) — collect these facts explicitly, they drive
§8:
- Where screen width/height, projection center and focal length/FOV live
  (constants in code vs. globals). List every place that assumes 320 or 640.
- The framebuffer layout(s) and whether flight supports 640×400.
- Fixed-point formats of matrices/vertices, the depth sort method,
  span/edge tables, texture formats and palette-shading tables.
- The boundary where world-space geometry becomes screen-space polygons
  (this is the future GPU backend hook point).

Flight/sim-specific notes (wave 2 + 9):
- The simulation step: the README notes a minimum 50 Hz physics step. Find
  the accumulator logic and whether sim steps and render frames are already
  separate loops. Document precisely; §8.2 depends on it.
- Random number generator(s): addresses, seeding; required for deterministic
  replay and for differential testing.

---

## 7. Phase 4 — Complete OpenWatcom build (B)

Start only when all `kind=game` and `kind=asm` rows are `verified` (or
`dead` with proof).

1. [ ] Replace the remaining extern-to-original-address bindings with real
   definitions: every global in `globals.tsv` gets a definition in
   `decomp/src/data_*.c` with its original initializer. Initializers come
   from the original image at build time via a generator
   (`tools/gen_data.py` reads the user's EXE and emits `local/` C files), so
   copyrighted data tables do not have to be committed. Mark which tables are
   hand-authored-from-understanding (committable) vs. extracted (generated).
2. [ ] Write `decomp/build.wlk` / Makefile (`wmake`) linking with OpenWatcom
   clib and the chosen extender: **DOS/32A** (open source, LE format) as the
   primary target; optional `system x32r` only if FlashTek X-32 libraries are
   available legally (do not assume).
3. [ ] Replace X-32-specific calls (`docs/x32-services.md`) with DPMI
   equivalents behind `decomp/src/plat_dos_x32.c` vs. `plat_dos_dpmi.c`.
4. [ ] Run the built EXE in DOSBox-X (from the dev shell) on a disposable
   copy of the installation; reach menu and flight. Then run it under the
   native64 harness (which then needs an LE loader, or run the LE image via
   DOS/32A inside the harness — decide after measuring effort; DOSBox-X is
   sufficient for acceptance).
5. [ ] Golden replays in DOSBox-X are not instruction-identical; instead
   compare framebuffer sequences at scripted points with a tolerance-free
   compare where possible and record known differences.

---

## 8. Phase 5 — Modern native build (C) and modern features

### 8.1 [ ] Platform layer

Can start as soon as waves 2, 4, 6, 7 are verified (it does not need the
whole game; unrecovered code can't run natively though — see the note below).

- Define `decomp/include/plat.h`: video (set mode, present indexed frame +
  palette), timer (monotonic ticks, sim tick source), input (key events,
  mouse, analog axes, buttons), audio (play sample, stream, CD track
  control), files (read-only asset fs + writable user dir mapping, reuse the
  overlay/XDG path rules already implemented), memory.
- Backends: `plat_dos.c` (original hardware semantics, for build B) and
  `plat_sdl3.c` (for build C). Reuse logic from `runtime/` (SDL3 frontend,
  gamepad profile, overlay windows, CD audio, path handling) by moving
  GPL-licensed host code into a library that `plat_sdl3.c` calls.
- Build C first as **i686** (`-m32`, GCC or Clang, `-fno-strict-aliasing`,
  `-fwrapv`, `-mfpmath=387`, `-ffloat-store` where needed, packed structs via
  `#pragma pack(1)` matching `-zp1`). This keeps pointer sizes and x87
  behavior identical. Make it 64-bit clean later (8.6).
- Compiler-portability header: map Watcom-isms (`#pragma aux`, `__far`,
  `_disable/_enable`, `inp/outp`, `int386`) to platform calls. Any remaining
  direct port I/O in game code is a bug to be moved into the platform layer.

Note: build C needs *all* game code recovered to run without the emulator.
If the modern build is wanted earlier, an intermediate "reverse hybrid" is
possible: native C code as the host program calling into the still-original
code through the native64 compatibility-mode bridge. Evaluate only if Phase 3
is slow; it is complex and not required by this plan.

### 8.2 [ ] High frame rate

1. Keep the original simulation tick exact (fixed step as discovered in
   wave 2/9). Never change the physics timestep — the flight model and AI
   must stay bit-compatible with original behavior in "original mode".
2. Separate the loop: `while (accumulator >= step) { sim_step(); }` then
   `render(alpha)` with `alpha = accumulator / step`.
3. Interpolation: for each renderable object and the camera, keep previous
   and current transforms (position: lerp; orientation: slerp or matrix
   normalized lerp). Store these in a renderer-side snapshot, not in sim
   structs, so sim memory layout and determinism are unchanged.
4. Cockpit instruments / HUD update with sim state but draw every frame.
5. Option `--original-timing` reproduces the original coupled loop for
   regression comparisons against the emulator.
6. Acceptance: 144 Hz render with stable sim, replay hashes of *sim state*
   identical to original-timing mode.

### 8.3 [ ] Resolution independence and widescreen

1. Software renderer first: parameterize framebuffer width/height and all
   constants catalogued in wave 5. Run at arbitrary resolution (e.g.
   1920×1080 indexed) with **Hor+ FOV**: keep vertical FOV of the original
   4:3 view, widen horizontal.
2. Account for the original non-square pixels (320×200 and 640×400 displayed
   at 4:3). The existing `--aspect-ratio` logic encodes this; the renderer's
   projection must use the same pixel aspect.
3. 2D layers (cockpit art, HUD, MFDs, menus): render at original resolution
   into separate layers and composite centered with pillarboxing, or anchor
   HUD elements to screen edges where the layout allows. Menus stay 4:3.
4. Check culling/clipping: a wider frustum shows objects the original culled;
   expand cull planes accordingly; check terrain LOD/draw distance.
5. Acceptance: at 4:3 + original resolution the output is identical to the
   original renderer; at 16:9/21:9 no stretched geometry, no popping at
   screen edges, HUD readable.

### 8.4 [ ] GPU renderer (optional, after 8.3)

- Hook at the world→screen boundary identified in wave 5. Emit triangles with
  palette-index → RGB lookup (and the original shading tables as 1D/2D
  textures to keep the look) into SDL3 GPU.
- Keep the software renderer as the reference; add a screenshot-diff tool
  that compares them at original resolution.
- Use the GFX+ (3Dfx) research in `docs/gfxplus-rendering.md` only as a
  reference for what DID changed for acceleration.

### 8.5 [ ] Other modern features (after 8.1–8.3)

Configurable keybindings and gamepad (reuse existing profile), mouse-look
views, window/fullscreen switching, save anywhere (based on save-state
infrastructure only if the sim state is fully serializable), volume
controls, CD-music from files (OGG/FLAC) instead of BIN/CUE, localization
switching (German/English text resources), optional bug fixes via
`EF_FIX_*` macros.

### 8.6 [ ] 64-bit cleanliness

Many structs hold 32-bit pointers in packed layouts loaded from files or
shared with assembly. Convert these to 32-bit handles/offsets or explicit
fix-up on load. Only after the i686 build is fully working.

---

## 9. Progress log

Append one line per completed step or every ~20 verified functions:
`YYYY-MM-DD  <step id>  <one-line result>  <commit>`.

```
2026-11-03  plan  Plan written; baseline in §0.
2026-11-03  2.2   Owner decision: functional decompilation; RAM state must match, binaries need not.
2026-11-03  3.1   Licensing in place: LicenseRef-EF2000-Derived for decomp/, GPL linking exception elsewhere.
2026-11-03  3.0   Added: no Ghidra workspace exists on the handoff machine; rebuild step required.
2026-11-03  3.0   Done: ef2000-ghidra rebuild; Ghidra 12.1.4 gives 4,277 functions, 4,277 pseudo-C, 0 failures.
2026-11-03  3.2   Tracker tool and seeded functions.tsv (4,277 rows, 294 named); globals.tsv seeded.
2026-11-03  3.1   Done: derived-source legal note, C headers, module/test directories, ignored build output.
2026-11-03  3.2   Done: re-seed from fresh 12.1.4 Ghidra inventory is identical (4,277 rows); tracker check and tests pass.
2026-11-03  3.3   Done: reproducible apply/export symbols; live synthetic persistence/idempotence test and 4,277-function refresh pass.
2026-11-03  3.4   Done: owner ran Nix watcom-omf check and ef2000-wcc fixture; wdis decoded OMF (12-byte code, __CHK dependency noted).
2026-11-03  3.5   In progress: initial high-score ABI instruction audit and OMF disassembly helper.
2026-11-03  3.5   Sampled 40 candidates across five code regions for manual calling-convention review.
2026-11-03  3.5   Collected 40 disassembly summaries and two distinct callers each; examined mixed EAX/EDX/stack ABI at 0x24480.
2026-11-03  3.5   Owner's OMF fixture exposed __CHK stack probe; add -s and forbid that new unresolved dependency in flake smoke check.
2026-11-03  3.5   Confirmed hiscore_qualifies (0x432c) signature from full callee and both callers, recording AL-only output.
2026-11-03  3.5   Confirmed ABI at 0x6a1b4 (signed AX to EAX pointer) and 0xb0208 (EAX,DL,BL inputs; EAX Boolean).
2026-11-03  3.5   Confirmed void ABIs 0x263e0/0x120800; documented x87 precision source 0x17d500 and three exceptional conventions.
2026-11-03  3.5   Chose OW 2.0 first-pass flags (-3r/-zp1/-fpi87/-fp3/-ob/-ot/-s); high-score packed record and Ghidra packing checks added.
2026-11-03  3.5   Confirmed four object helpers: AX-only flag mutations at 0x6a354/0x6a394 and BX-indexed x87 matrix/transpose vector writes at 0x6a4c4/0x6a538.
2026-11-03  3.5   Audited six object/grid control ABIs and 0xca-byte grid layout; drafted host-tested C for rows/state/mode/relation, no original-game verification.
2026-11-03  3.5   Confirmed EAX-pointer metric 0x23760, DS-to-ES strlen 0xb1b8b (requires DF clear), and global token parser 0xf46cc; reviewed two segment/interrupt exceptions.
2026-11-03  3.5   Confirmed AL-only global object relation predicate 0x23800 and void slot-sync wrapper 0x238a0, with two distinct callers apiece.
2026-11-03  3.5   Confirmed 0xb0018/0xb00d0 EAX/EDX pointer-to-list/count allocators and 0x36-byte table stride; audited nonstandard ret 12 formatter 0x1204dc and EBX/ECX/ESI/EDI projection helper 0x15c071.
2026-11-03  3.5   Confirmed elapsed-centisecond thunk at 0xf8248; recorded partial ABI of indirect output 0xf6124 and callback dispatch 0x12d90.
2026-11-03  3.5   Confirmed 0x11c0b4 EAX/EDX-to-packed-213-byte x87 record, 0x68ccc CS jump-table AL bit probe, and 0x15be10 global-input fixed-point writer (19/40 ABI samples).
2026-11-03  3.5   Confirmed 0x243ac AX-indexed x87 relation update (20/40); reviewed 0xf9ee0 stack-based LZ-style REP decoder with ret 8, pending Ghidra stack storage support.
2026-11-03  3.5   Confirmed 0xb0850 AL/EBX/EDX linked-list counter (21/40); reviewed 0xabec4 callback table with unresolved indirect-callee clobbers.
2026-11-03  3.5   Confirmed 0x6a1d0 signed AX/DL object-state dispatch with 24 callers (22/40 ABI samples).
2026-11-03  3.5   Confirmed 0xb0954 positive filtered-node count as companion to 0xb0850 (23/40 ABI samples).
2026-11-03  3.5   Ghidra 12.1.4 reviewed-signature importer now supports [ESP+N] stack parameters and ret N purge; asset-free synthetic save/reload test passes.
2026-11-03  3.5   Confirmed five nonstandard stack/register ABIs: LZ/RA decompressors, ret 12 nonfinite formatter, ret 16 projection adapter and its register helper (28/40 samples).
2026-11-03  3.5   Confirmed five remaining long/mixed ABIs: x87 ret 8 overlap, indexed global projection, callback-backed projection query, global-node initialization and command dispatch (33/40 samples; seven reviewed exceptions remain).
2026-11-03  3.5   Added 80-bit ST0 storage to the Ghidra symbol importer; confirmed ret 8 stack-double/ST0 return at 0xf8250 and ES:[EDI] non-popping ST0 store at 0x120a52 (35/40 samples; five indirect/segment exceptions).
2026-11-03  3.5   Representative ABI audit closed at 35 confirmed and five explicitly bounded, ineligible-for-automatic-splice callback/segment exceptions (40/40 reviewed).
2026-11-03  3.6   Began library identification: relocation-masked OMF public-function fingerprint tool and proposal-only matcher, with three synthetic tests; real Watcom library evidence pending.
2026-11-03  3.6   Classified three source-matched Watcom/X-32 startup routines as library code; not fingerprint-derived and not counted as verified function replacements.
2026-11-03  3.7   Added a strict offline OMF splice planner, 32-bit relocation/layout proof cases, and seven synthetic tests; patch loading and real compiler-object validation remain outstanding.
2026-11-03  3.8   Implemented SHA-bound in-place EFPATCH1 loading in native64/QEMU and C++ Unicorn, with save-state patch identity and asset-free format tests; guest arena mapping remains blocked on high-water evidence.
2026-11-03  3.8   Added X-32 allocator process watermarks in both runners and native report parsing; highwater evidence from sustained gameplay and mapping audit still required before patch arena.
2026-11-03  3.10  Began ABI-aware isolated original-versus-patched Unicorn function differential tests, with strict fixture validation and dynamic original block counts; no real recovered replacement verified yet.
2026-11-03  3.10  Added bounded original call-entry RAM/register capture to C++ Unicorn, and SHA-bound captured-state replay in the isolated differential tool; asset-free capture format tests pass, but live-game proof is pending.
2026-11-03  3.5   Drafted first recovered game C function (0x432c signed high-score qualification), with packed-layout asserts, native host test and ten differential boundary fixtures; guest verification pending.
2026-11-03  3.7   Added SHA-pinned direct OMF-to-EFPATCH1 path for reviewed in-place-only functions and synthetic full-chain relocation tests; first real OpenWatcom object still pending.
2026-11-03  4     SHA-pinned original image audited against C drafts at 0x432c/0x263e0/0x6a1b4; spatial append 0x263ec recovered from four direct callers, three new drafted functions with signed/packed guest specs.
2026-11-03  4     Drafted 0x7489c filtered object lookup plus four 0x6a3xx state wrappers; five new ABI-reviewed C functions and differential plans, with shared-epilogue splice hazard documented.
2026-11-03  4     Drafted six more world-grid link/query/control functions (21 total) with ABI/caller evidence, row-offset asserts, 65,536-case host predicate check and six pending guest plans.
2026-11-03  4     Drafted five more world-grid link/position/limit helpers (26 total); two independent callers each, seven-filtered-lookup host model, five guest plans, Watcom hooks pending execution.
2026-11-03  4     Drafted seven grid adjacency/relation/category/visibility/pool helpers (33 total); 859 original bytes mapped in the new batch, host tests and seven guest plans; x87/Watcom and differential proof pending.
2026-11-03  4     Drafted eight 2D geometry, object limit and motion helpers (41 total) with ABI evidence, packed offsets, two-dimensional-only distance quirks and eight guest plans; host tests pass, Watcom/guest checks pending.
2026-11-03  4     Drafted seven scene-node list, child flag and event helpers (48 total) with two distinct callers each; retained 0x907f0 shared-epilogue splice warning and seven pending guest plans. Host tests pass; original-object dispatch and x87 still unverified.
2026-11-03  4     Drafted eight more scene-state/event helpers (56 total): packed triple mirrors, full-EAX named lookup, 0xfffffffe metric offset, angle dispatch, pair probe and signed event-count history. Host tests and per-address guest plans; no Watcom/splice differential yet.
2026-11-03  4     Drafted eight indexed-scene/object helpers (64 total): 58-caller scene node lookup, percentage grading, secondary-grid membership, record reset/child slots, navigation marker, grid-link queries and paired-object references. Host tests, byte-offset asserts and 8 guest plans; no Watcom/splice differential yet.
2026-11-03  4     Drafted eight buffer/planar/object-table helpers (72 total): cursor bound diagnostics, signed word I/O, 12-byte plane-source selection, two-byte planar fetch, canonical object-pointer test, unguarded table slot and scene refresh latch. C host assertions and 8 guest plans; original-game differential pending.
2026-11-03  4     Drafted eight scene control and x87 vector helpers (80 total): feature word stores, secondary-grid object byte, signed direction-switching scene gate, four-callback angle triplet, vector subtraction and three scaled conversions. 8 new guest fixtures, host tests and two-callsite ABI evidence; x87/Watcom/replay verification outstanding.
2026-11-03  4     Drafted ten linked-record and runtime-control helpers (90 total): preferred candidate pointer at +0x15 versus signed indexed-table fallback, in-place reference refresh, four state word setters, pointer copy, and three gated byte latches. Original instructions and two independent callsites each, ten guest fixture plans and host tests; Watcom/guest evidence pending.
2026-11-03  4     Drafted eight command packet wrappers (98 total): 3/4/5-byte stack commands with opcodes 0x0c/0x16/0x32/0x39/0x3a/0x4a/0x4b/0x4c, preserved AX byte order, runtime marker and DL; eight guest plans, a byte-exact host sink and two-callsite ABI records. Original packet dispatcher 0xa3f74 and Watcom/splice replay remain unresolved.
2026-11-03  4     Drafted sixteen clock, tactical-state, grid and UI-list helpers (114 total): signed three-word comparisons and time conversion, tactical byte counters/lookup, bounded XY query, packed coordinate samples, grid countdown, UI sentinel traversal and signed strip clearing. Sixteen per-address guest fixtures, two original caller functions each and two standalone host-test executables; Ghidra/Watcom, live guest differential and splice replay still pending.
2026-11-03  4     Drafted eight script-line, tactical event-ring, activity-record and rotating-selector helpers (122 total). Packed 7/3/23-byte records, global offsets, two original callsites each, eight guest fixtures, and a standalone passing host model. Watcom, guest differential, and replay remain outstanding.
2026-11-03  4     Drafted eight callers/coordinate helpers (130 total): filtered-object activity lookup, child pointer reconciliation, per-grid indexed clocks, panel strip rotation, signed-short x87 conversion and three-double pair/triple transforms. Original two-callsite evidence, eight guest plans (the ST0 signature still unsupported by the validator) and a second passing host executable. Original guest differential and spliced replay not run.
2026-11-03  4     Drafted eight UI dispatch and release controls (138 total): gates, initial-state latch, fourteen-item callback loops, ten-entry button-code matching, signed indexed control dispatch, conditional panel reset, and message selection. Eight fixture plans, two original callsites per function, and a standalone passing host test; original guest dispatcher tests still pending.
2026-11-03  5     Drafted eight packet/status/tactical wrapper functions (146 total): exact 18/9/10-byte outbound packets, two status-flag formatters, float32-to-double formatting, tactical display-mode inversion, and callback-first byte latch. Two original callsites each, eight fixture plans and a passing host test; guest differential still pending.
2026-11-03  6     Drafted eight record/tactical transitions (154 total): doubled screen coordinates, signed metrics, overlapping position-bin word, conditional action reports, string assembly, six-byte selection lookup, doubly linked ring rotation and pool reset. Two callsites each, eight fixture plans, standalone host test passes; guest differential still pending.
2026-11-03  7     Drafted eight UI/tactical helpers (162 total): packet 2c and post-send callback, tactical cross drawing, indexed template loading, UI fragment compaction and search, composite dispatch, record-table membership, and unlink-then-install. Eight replay fixtures validate, two original callsites per helper, host tests pass; guest verification pending.
2026-11-03  8     Drafted eight UI line and state helpers (170 total): CR/LF line advance, signed point-range metric, UI vector preparation and relative offsets, component initialization, temporary object recalculation, candidate validation, link removal, and navigation word transforms. Corrected preceding line-fragment host fixtures to use actual CR/LF semantics; all eight new fixture plans and both UI host suites pass, guest replay pending.
2026-11-03  9     Drafted eight command-file parser helpers (178 total): buffer ownership/initialization, quoted or unquoted tokenization with 80-byte truncation and signed-16 cursor, two numeric conversions, indirect text allocation, 11-name enum lookup, and 3-dword/3-word record decoding. Reused parsed-token implementations across dependent recovered functions. Original independent callsites, eight guest plans and a passing dedicated host test; original execution still unverified.
2026-11-03  10    Drafted eight more enum parsers (186 total): one two-family literal matcher and seven independent 10/18/19/27/30/39/106-entry pointer-array matchers. Shared their original linear-search semantics in a common typed helper, with first/last/failure host tests, eight guest fixtures and two direct caller locations for each. No original differential replay yet.
2026-11-03  11    Drafted eight parser-line/runtime helpers (194 total): semicolon/empty-line skipping, buffer release, text record with 5 byte and 6 word fields, three packed input-copy fields, callback-backed time samples and signed delta clamping, 11-bit packed coordinates and four-way signed pair transforms. Two original caller sites each, 8 guest plans, two passing host suites; OpenWatcom and original guest verification remain unavailable.
2026-11-03  12    Drafted eight indexed runtime and event/dialog helpers (202 total): pointer segment/offset encoding, 54-entry callback reset, overlapping packed-word slot indexing, latch initialization, 34-event scan, optional object dispatch, two-widget bind and dialog slot flush. Two independent callsites each, eight pending guest plans, two standalone host suites; original guest differential still unavailable.
2026-11-03  13    Drafted eight marker-packet and grid-row helpers (210 total): one- and two-byte packets 1d/1e/1f/26/1b with grid marker, a four-byte AX/DL packet 24, a record-to-latch commit and a conditional marker store into the 0xca-byte grid row. One direct caller each; host test passes; guest differential and replay unavailable.
2026-11-03  14    Drafted eight more packet and scene-latch helpers (218 total): packets 4f/42/2a/30, activity-record flag clear, two latch words, marker-matched forwarding and a scene-pair notify. One direct caller each; host test passes; guest verification unavailable.
2026-11-03  15    Drafted ten more packet, counter and mode-limit helpers (228 total): packets 3e/3b/3d/40/2e, three upper-bounded byte counters (no lower bound kept) and two limit setters. Host test passes; guest verification unavailable.
2026-11-03  16    Drafted eight runtime-word and record-lookup helpers (236 total): paired acquire/set on object 0x1a7c30, halfword loads, 0xe5-byte record lookup and mode test, quotient table lookup and a pair reset. Host test passes; guest verification unavailable.
2026-11-03  17    Drafted ten status-formatter and state helpers (246 total): seven 0xa4342 value formatters, a pending-mode setter, a halfword load and a pointer release. Host test passes; guest verification unavailable.
2026-11-03  18    Built the OpenWatcom wrapper from the flake (needs NIXPKGS_ALLOW_UNFREE=1 --impure) and compiled every decomp/src module: 13 modules failed under C89 (loop-scoped declarations, mixed declarations, non-constant initializers); added -za99 to the canonical flags and rewrote two aggregate initializers. All 246 drafted functions now compile with 0 warnings, 0 errors and are marked compiles; splice and differential evidence still pending.
2026-11-03  19  Drafted and Watcom-compiled 19 more functions in misc_state (265 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  20  Drafted and Watcom-compiled 37 more functions in low_helpers (302 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  21  Drafted and Watcom-compiled 34 more functions in mid_helpers (336 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  22  Drafted and Watcom-compiled 47 more functions in scene_helpers (383 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  23  Drafted and Watcom-compiled 45 more functions in list_helpers (428 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  24  Drafted and Watcom-compiled 43 more functions in list_lookup (471 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  25  Drafted and Watcom-compiled 35 more functions in code_predicates (506 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  26  Drafted and Watcom-compiled 41 more functions in math_string (547 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  27  Drafted and Watcom-compiled 56 more functions in misc_helpers2 (603 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  28  Drafted and Watcom-compiled 42 more functions in runtime_helpers (645 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  29  Drafted and Watcom-compiled 24 more functions in runtime_libc (669 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  30  Drafted and Watcom-compiled 21 more functions in game_state_helpers (690 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  31  Drafted and Watcom-compiled 24 more functions in game_state_helpers2 (714 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  32  Drafted and Watcom-compiled 22 more functions in game_state_helpers3 (736 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  33  Drafted and Watcom-compiled 18 more functions in unit_flags (754 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  34  Drafted and Watcom-compiled 11 more functions in camera_helpers (765 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  35  Drafted and Watcom-compiled 8 more functions in line_of_sight (773 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  36  Drafted and Watcom-compiled 15 more functions in entity_helpers (788 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  37  Drafted and Watcom-compiled 14 more functions in entity_helpers2 (802 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  38  Drafted and Watcom-compiled 18 more functions in engine_helpers (820 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  39  Drafted and Watcom-compiled 16 more functions in runtime_helpers2 (836 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  40  Drafted and Watcom-compiled 18 more functions in runtime_helpers3 (854 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  41  Drafted and Watcom-compiled 21 more functions in ui_widget_helpers (875 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  42  Drafted and Watcom-compiled 14 more functions in list_helpers (889 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  43  Drafted and Watcom-compiled 22 more functions in engine_helpers3 (911 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  44  Drafted and Watcom-compiled 11 more functions in runtime_helpers4 (922 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  45  Drafted and Watcom-compiled 33 more functions in tiny_helpers (955 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  46  Drafted and Watcom-compiled 4 more functions in runtime_helpers5 (959 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  47  Drafted and Watcom-compiled 5 more functions in ui_state_helpers (964 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  48  Drafted and Watcom-compiled 14 more functions in game_helpers4 (978 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  49  Drafted and Watcom-compiled 38 more functions in game_helpers5 (1016 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  50  Drafted and Watcom-compiled 24 more functions in game_helpers6 (1040 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  51  Drafted and Watcom-compiled 22 more functions in game_helpers7 (1062 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  52  Drafted and Watcom-compiled 11 more functions in game_helpers8 (1073 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  53  Drafted and Watcom-compiled 14 more functions in game_helpers9 (1087 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  54  Drafted and Watcom-compiled 7 more functions in game_helpers10 (1094 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  55  Drafted and Watcom-compiled 7 more functions in game_helpers11 (1101 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  56  Drafted and Watcom-compiled 11 more functions in game_helpers12 (1112 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  57  Drafted and Watcom-compiled 9 more functions in runtime_helpers6 (1121 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  58  Drafted and Watcom-compiled 9 more functions in runtime_helpers7 (1130 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  59  Drafted and Watcom-compiled 7 more functions in runtime_helpers8 (1137 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  60  Drafted and Watcom-compiled 3 more functions in runtime_helpers9 (1140 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  61  Drafted and Watcom-compiled 4 more functions in game_helpers13 (1144 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  62  Drafted and Watcom-compiled 4 more functions in game_helpers14 (1148 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  63  Drafted and Watcom-compiled 4 more functions in game_helpers15 (1152 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  64  Drafted and Watcom-compiled 4 more functions in game_helpers16 (1156 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  65  Drafted and Watcom-compiled 6 more functions in game_helpers17 (1162 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  66  Drafted and Watcom-compiled 6 more functions in game_helpers18 (1168 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  67  Drafted and Watcom-compiled 3 more functions in game_helpers19 (1171 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  68  Drafted and Watcom-compiled 5 more functions in game_helpers20 (1176 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  69  Drafted and Watcom-compiled 7 more functions in game_helpers21 (1183 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  70  Drafted and Watcom-compiled 4 more functions in game_helpers22 (1187 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  71  Drafted and Watcom-compiled 5 more functions in game_helpers23 (1192 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  72  Drafted and Watcom-compiled 55 more functions in tiny_helpers2 (1247 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  73  Drafted and Watcom-compiled 7 more functions in runtime_helpers10 (1254 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  74  Drafted and Watcom-compiled 6 more functions in runtime_helpers11 (1260 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  76  Drafted and Watcom-compiled 11 more functions in runtime_helpers12 (1271 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  77  Drafted and Watcom-compiled 6 more functions in runtime_helpers13 (1277 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  78  Drafted and Watcom-compiled 5 more functions in runtime_helpers14 (1282 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  79  Drafted and Watcom-compiled 5 more functions in runtime_helpers15 (1287 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  80  Drafted and Watcom-compiled 4 more functions in runtime_helpers16 (1291 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  81  Drafted and Watcom-compiled 9 more functions in game_helpers24 (1300 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  82  Drafted and Watcom-compiled 5 more functions in game_helpers25 (1305 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  83  Drafted and Watcom-compiled 6 more functions in runtime_helpers17 (1311 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  84  Drafted and Watcom-compiled 3 more functions in game_helpers26 (1314 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  85  Drafted and Watcom-compiled 2 more functions in runtime_helpers18 (1316 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  86  Drafted and Watcom-compiled 6 more functions in game_helpers27 (1322 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  87  Drafted and Watcom-compiled 20 more functions in game_helpers28 (1342 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  88  Drafted and Watcom-compiled 15 more functions in game_helpers29 (1357 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  89  Drafted and Watcom-compiled 14 more functions in game_helpers30 (1371 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  90  Drafted and Watcom-compiled 12 more functions in game_helpers31 (1383 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  91  Drafted and Watcom-compiled 10 more functions in game_helpers32 (1393 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  92  Drafted and Watcom-compiled 5 more functions in game_helpers33 (1398 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  93  Drafted and Watcom-compiled 7 more functions in game_helpers34 (1405 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  94  Drafted and Watcom-compiled 7 more functions in game_helpers35 (1412 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  95  Drafted and Watcom-compiled 5 more functions in game_helpers36 (1417 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  96  Drafted and Watcom-compiled 7 more functions in game_helpers37 (1424 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  97  Drafted and Watcom-compiled 8 more functions in game_helpers38 (1432 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  98  Drafted and Watcom-compiled 6 more functions in game_helpers39 (1438 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  99  Drafted and Watcom-compiled 8 more functions in tiny_helpers3 (1446 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  100  Drafted and Watcom-compiled 5 more functions in game_helpers40 (1451 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  101  Drafted and Watcom-compiled 3 more functions in game_helpers41 (1454 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  102  Drafted and Watcom-compiled 2 more functions in game_helpers42 (1456 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  103  Drafted and Watcom-compiled 2 more functions in game_helpers43 (1458 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  104  Drafted and Watcom-compiled 12 more functions in runtime_helpers19 (1470 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  105  Drafted and Watcom-compiled 15 more functions in runtime_helpers20 (1485 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  106  Drafted and Watcom-compiled 20 more functions in runtime_helpers21 (1505 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  107  Drafted and Watcom-compiled 13 more functions in runtime_helpers22 (1518 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  108  Drafted and Watcom-compiled 4 more functions in runtime_helpers23 (1522 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  109  Drafted and Watcom-compiled 19 more functions in runtime_helpers24 (1541 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  110  Drafted and Watcom-compiled 17 more functions in runtime_helpers25 (1558 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  111  Drafted and Watcom-compiled 4 more functions in runtime_helpers26 (1562 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  112  Drafted and Watcom-compiled 11 more functions in runtime_helpers27 (1573 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  113  Drafted and Watcom-compiled 10 more functions in runtime_helpers28 (1583 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  114  Drafted and Watcom-compiled 3 more functions in runtime_helpers29 (1586 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  115  Drafted and Watcom-compiled 5 more functions in game_helpers44 (1591 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  116  Drafted and Watcom-compiled 8 more functions in game_helpers45 (1599 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  117  Drafted and Watcom-compiled 6 more functions in game_helpers46 (1605 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  118  Drafted and Watcom-compiled 6 more functions in game_helpers47 (1611 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  119  Drafted and Watcom-compiled 4 more functions in game_helpers48 (1615 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  120  Drafted and Watcom-compiled 5 more functions in game_helpers49 (1620 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  121  Drafted and Watcom-compiled 5 more functions in game_helpers50 (1625 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  122  Drafted and Watcom-compiled 17 more functions in runtime_helpers30 (1642 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  123  Drafted and Watcom-compiled 5 more functions in runtime_helpers31 (1647 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  124  Drafted and Watcom-compiled 8 more functions in game_helpers51 (1655 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  125  Drafted and Watcom-compiled 7 more functions in runtime_helpers32 (1662 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  126  Drafted and Watcom-compiled 13 more functions in runtime_helpers33 (1675 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  127  Drafted and Watcom-compiled 7 more functions in runtime_helpers34 (1682 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  128  Drafted and Watcom-compiled 9 more functions in runtime_helpers35 (1691 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  129  Drafted and Watcom-compiled 11 more functions in float_helpers1 (1702 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  130  Drafted and Watcom-compiled 14 more functions in float_helpers2 (1716 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  131  Drafted and Watcom-compiled 12 more functions in float_helpers3 (1728 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  132  Drafted and Watcom-compiled 6 more functions in float_helpers4 (1734 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  133  Drafted and Watcom-compiled 11 more functions in float_helpers5 (1745 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  134  Drafted and Watcom-compiled 11 more functions in float_helpers6 (1756 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  135  Drafted and Watcom-compiled 47 more functions in leaf_helpers1 (1803 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  136  Drafted and Watcom-compiled 15 more functions in leaf_helpers2 (1818 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  137  Drafted and Watcom-compiled 10 more functions in leaf_helpers3 (1828 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  138  Drafted and Watcom-compiled 10 more functions in leaf_helpers4 (1838 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  139  Drafted and Watcom-compiled 7 more functions in leaf_helpers5 (1845 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  140  Drafted and Watcom-compiled 6 more functions in leaf_helpers6 (1851 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  141  Drafted and Watcom-compiled 5 more functions in game_helpers53 (1856 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  142  Drafted and Watcom-compiled 5 more functions in game_helpers54 (1861 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  143  Drafted and Watcom-compiled 1 more functions in game_helpers55 (1862 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  144  Drafted and Watcom-compiled 5 more functions in game_helpers56 (1867 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  145  Drafted and Watcom-compiled 4 more functions in game_helpers57 (1871 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  146  Drafted and Watcom-compiled 1 more functions in game_helpers58 (1872 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  147  Drafted and Watcom-compiled 2 more functions in float_helpers7 (1874 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  148  Drafted and Watcom-compiled 4 more functions in float_helpers8 (1878 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  149  Drafted and Watcom-compiled 2 more functions in float_helpers9 (1880 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  150  Drafted and Watcom-compiled 4 more functions in game_helpers59 (1884 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  151  Drafted and Watcom-compiled 1 more functions in game_helpers60 (1885 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  152  Drafted and Watcom-compiled 3 more functions in game_helpers61 (1888 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  153  Drafted and Watcom-compiled 1 more functions in game_helpers62 (1889 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  154  Drafted and Watcom-compiled 4 more functions in game_helpers63 (1893 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  155  Drafted and Watcom-compiled 2 more functions in game_helpers64 (1895 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  156  Drafted and Watcom-compiled 2 more functions in game_helpers65 (1897 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  157  Drafted and Watcom-compiled 3 more functions in game_helpers66 (1900 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  158  Drafted and Watcom-compiled 2 more functions in game_helpers67 (1902 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  159  Drafted and Watcom-compiled 2 more functions in game_helpers68 (1904 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  160  Drafted and Watcom-compiled 2 more functions in matrix_fixed (1906 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  161  Drafted and Watcom-compiled 2 more functions in game_helpers69 (1908 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  162  Drafted and Watcom-compiled 1 more functions in game_helpers70 (1909 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  163  Drafted and Watcom-compiled 2 more functions in game_helpers71 (1911 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  164  Drafted and Watcom-compiled 2 more functions in game_helpers72 (1913 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  165  Drafted and Watcom-compiled 2 more functions in game_helpers73 (1915 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  166  Drafted and Watcom-compiled 3 more functions in game_helpers74 (1918 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  167  Drafted and Watcom-compiled 4 more functions in game_helpers75 (1922 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  168  Drafted and Watcom-compiled 2 more functions in game_helpers76 (1924 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  169  Drafted and Watcom-compiled 1 more functions in game_helpers77 (1925 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  170  Drafted and Watcom-compiled 6 more functions in float_helpers10 (1931 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  171  Drafted and Watcom-compiled 2 more functions in float_helpers11 (1933 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  172  Drafted and Watcom-compiled 2 more functions in float_helpers7 (1934 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  173  Drafted and Watcom-compiled 24 more functions in leaf_helpers1 (1958 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  174  Drafted and Watcom-compiled 11 more functions in leaf_helpers7 (1969 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  175  Drafted and Watcom-compiled 6 more functions in game_helpers78 (1975 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  176  Drafted and Watcom-compiled 1 more functions in game_helpers79 (1976 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  177  Drafted and Watcom-compiled 7 more functions in game_helpers80 (1983 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  178  Drafted and Watcom-compiled 5 more functions in game_helpers81 (1988 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  179  Drafted and Watcom-compiled 3 more functions in game_helpers82 (1991 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  180  Drafted and Watcom-compiled 2 more functions in game_helpers83 (1993 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  181  Drafted and Watcom-compiled 2 more functions in game_helpers84 (1995 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  182  Drafted and Watcom-compiled 3 more functions in game_helpers85 (1998 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  183  Drafted and Watcom-compiled 4 more functions in game_helpers86 (2002 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  184  Drafted and Watcom-compiled 6 more functions in game_helpers87 (2008 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  185  Drafted and Watcom-compiled 4 more functions in game_helpers88 (2012 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  186  Drafted and Watcom-compiled 5 more functions in game_helpers89 (2017 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  187  Drafted and Watcom-compiled 2 more functions in game_helpers90 (2019 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  188  Drafted and Watcom-compiled 3 more functions in runtime_helpers30 (2022 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  189  Drafted and Watcom-compiled 4 more functions in game_helpers91 (2026 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  190  Drafted and Watcom-compiled 2 more functions in game_helpers92 (2028 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  191  Drafted and Watcom-compiled 3 more functions in game_helpers93 (2031 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  192  Drafted and Watcom-compiled 1 more functions in game_helpers94 (2032 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  193  Drafted and Watcom-compiled 1 more functions in game_helpers95 (2033 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  194  Drafted and Watcom-compiled 4 more functions in runtime_helpers31 (2037 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  195  Drafted and Watcom-compiled 6 more functions in runtime_helpers32 (2043 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  196  Drafted and Watcom-compiled 3 more functions in runtime_helpers33 (2046 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  197  Drafted and Watcom-compiled 3 more functions in game_helpers96 (2049 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  198  Drafted and Watcom-compiled 1 more functions in game_helpers97 (2050 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  199  Drafted and Watcom-compiled 1 more functions in game_helpers98 (2051 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  200  Drafted and Watcom-compiled 5 more functions in game_helpers99 (2056 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  201  Drafted and Watcom-compiled 3 more functions in game_helpers100 (2059 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  202  Drafted and Watcom-compiled 2 more functions in game_helpers101 (2061 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  203  Drafted and Watcom-compiled 2 more functions in game_helpers102 (2063 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  204  Drafted and Watcom-compiled 2 more functions in game_helpers103 (2065 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  205  Drafted and Watcom-compiled 1 more functions in game_helpers104 (2066 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  207  Drafted and Watcom-compiled 3 more functions in game_helpers106 (2069 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  208  Drafted and Watcom-compiled 3 more functions in game_helpers107 (2072 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  209  Drafted and Watcom-compiled 2 more functions in game_helpers108 (2074 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  210  Drafted and Watcom-compiled 1 more functions in game_helpers109 (2075 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  211  Drafted and Watcom-compiled 2 more functions in game_helpers110 (2077 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  212  Drafted and Watcom-compiled 2 more functions in game_helpers111 (2079 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  213  Drafted and Watcom-compiled 2 more functions in game_helpers112 (2081 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  214  Drafted and Watcom-compiled 1 more functions in game_helpers113 (2082 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  215  Drafted and Watcom-compiled 1 more functions in game_helpers114 (2083 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  216  Drafted and Watcom-compiled 1 more functions in game_helpers115 (2084 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  217  Drafted and Watcom-compiled 2 more functions in game_helpers116 (2086 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  218  Drafted and Watcom-compiled 1 more functions in game_helpers117 (2087 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  220  Drafted and Watcom-compiled 3 more functions in runtime_helpers35 (2090 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  221  Drafted and Watcom-compiled 0 more functions in game_helpers118 (2090 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  221  Drafted and Watcom-compiled 1 more functions in game_helpers118 (2091 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  222  Drafted and Watcom-compiled 2 more functions in runtime_helpers34 and game_helpers105 (2093 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  222  Drafted and Watcom-compiled 3 more functions in game_helpers119 (2096 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  223  Drafted and Watcom-compiled 5 more functions in game_helpers120 (2101 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  224  Drafted and Watcom-compiled 3 more functions in game_helpers121 (2104 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  225  Drafted and Watcom-compiled 2 more functions in runtime_helpers36 (2106 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  226  Drafted and Watcom-compiled 2 more functions in game_helpers122 (2108 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  227  Drafted and Watcom-compiled 2 more functions in game_helpers123 (2110 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  228  Drafted and Watcom-compiled 2 more functions in game_helpers124 (2112 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  229  Drafted and Watcom-compiled 4 more functions in game_helpers125 (2116 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  230  Drafted and Watcom-compiled 3 more functions in runtime_helpers37 (2119 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  231  Drafted and Watcom-compiled 1 more functions in runtime_helpers38 (2120 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  232  Drafted and Watcom-compiled 1 more functions in game_helpers126 (2121 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  233  Drafted and Watcom-compiled 4 more functions in game_helpers127 (2125 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  234  Drafted and Watcom-compiled 2 more functions in game_helpers128 (2127 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  235  Drafted and Watcom-compiled 5 more functions in game_helpers129 (2132 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  236  Drafted and Watcom-compiled 2 more functions in game_helpers130 (2134 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  237  Drafted and Watcom-compiled 2 more functions in runtime_helpers39 (2136 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  238  Drafted and Watcom-compiled 1 more functions in runtime_helpers39 (2137 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  239  Drafted and Watcom-compiled 4 more functions in runtime_helpers40 (2141 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  240  Drafted and Watcom-compiled 1 more functions in runtime_helpers41 (2142 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  241  Drafted and Watcom-compiled 5 more functions in runtime_helpers42 (2147 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  242  Drafted and Watcom-compiled 2 more functions in game_helpers125 (2149 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  243  Drafted and Watcom-compiled 4 more functions in game_helpers131 (2153 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  244  Drafted and Watcom-compiled 3 more functions in game_helpers131 (2156 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  245  Drafted and Watcom-compiled 1 more functions in runtime_helpers40 (2157 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  246  Drafted and Watcom-compiled 2 more functions in game_helpers131 (2159 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  247  Drafted and Watcom-compiled 4 more functions in game_helpers132 (2163 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  248  Drafted and Watcom-compiled 26 more functions in trace_wrappers1 (2189 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  249  Drafted and Watcom-compiled 7 more functions in hotspot_draw (2196 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  250  Drafted and Watcom-compiled 4 more functions in box_rows (2200 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  251  Drafted and Watcom-compiled 3 more functions in hotspot_draw (2203 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  252  Drafted and Watcom-compiled 3 more functions in colour_tables (2206 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  253  Drafted and Watcom-compiled 15 more functions in colour_tables2 (2221 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  254  Drafted and Watcom-compiled 4 more functions in code_maps1 (2225 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  255  Drafted and Watcom-compiled 1 more functions in small_wrappers1 (2226 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  256  Drafted and Watcom-compiled 3 more functions in small_wrappers2 (2229 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  257  Drafted and Watcom-compiled 2 more functions in small_wrappers3 (2231 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  258  Drafted and Watcom-compiled 3 more functions in small_wrappers4 (2234 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  259  Drafted and Watcom-compiled 2 more functions in small_wrappers5 (2236 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  260  Drafted and Watcom-compiled 2 more functions in script_helpers1 (2238 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  261  Drafted and Watcom-compiled 1 more functions in fill_rects1 (2239 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  262  Drafted and Watcom-compiled 1 more functions in key_helpers1 (2240 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  263  Drafted and Watcom-compiled 2 more functions in table_switches1 (2242 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  264  Drafted and Watcom-compiled 1 more functions in dos_slots1 (2243 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  264  Drafted and Watcom-compiled 1 more functions in dos_slots1 (2244 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  265  Drafted and Watcom-compiled 1 more functions in colour_tables3 (2245 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  266  Drafted and Watcom-compiled 1 more functions in draw_loops1 (2246 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  267  Drafted and Watcom-compiled 2 more functions in unit_events1 (2248 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  268  Drafted and Watcom-compiled 1 more functions in init_tables1 (2249 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  268  Drafted and Watcom-compiled 1 more functions in init_tables1 (2250 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  269  Drafted and Watcom-compiled 2 more functions in fill_rects2 (2252 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  270  Drafted and Watcom-compiled 1 more functions in page_rotate1 (2253 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  270  Drafted and Watcom-compiled 1 more functions in page_rotate1 (2254 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  271  Drafted and Watcom-compiled 2 more functions in list_ops1 (2256 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  271  Drafted and Watcom-compiled 1 more functions in list_ops1 (2257 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  271  Drafted and Watcom-compiled 1 more functions in list_ops1 (2258 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  271  Drafted and Watcom-compiled 2 more functions in list_ops1 (2260 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  272  Drafted and Watcom-compiled 2 more functions in probe_helpers1 (2262 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  272  Drafted and Watcom-compiled 1 more functions in probe_helpers1 (2263 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  273  Drafted and Watcom-compiled 3 more functions in screen_clip1 (2266 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  273  Drafted and Watcom-compiled 1 more functions in screen_clip1 (2267 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  274  Drafted and Watcom-compiled 2 more functions in state_reset1 (2269 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  266  Drafted and Watcom-compiled 1 more functions in draw_loops1 (2270 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  275  Drafted and Watcom-compiled 2 more functions in load_planes1 (2272 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  274  Drafted and Watcom-compiled 2 more functions in state_reset1 (2274 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  274  Drafted and Watcom-compiled 1 more functions in state_reset1 (2275 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  276  Drafted and Watcom-compiled 1 more functions in tactical_marks1 (2276 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  276  Drafted and Watcom-compiled 1 more functions in tactical_marks1 (2277 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  276  Drafted and Watcom-compiled 1 more functions in tactical_marks1 (2278 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  276  Drafted and Watcom-compiled 1 more functions in tactical_marks1 (2279 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  276  Drafted and Watcom-compiled 1 more functions in tactical_marks1 (2280 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  276  Drafted and Watcom-compiled 1 more functions in tactical_marks1 (2281 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  276  Drafted and Watcom-compiled 1 more functions in tactical_marks1 (2282 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  273  Drafted and Watcom-compiled 1 more functions in screen_clip1 (2283 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  275  Drafted and Watcom-compiled 1 more functions in load_planes1 (2284 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  274  Drafted and Watcom-compiled 1 more functions in state_reset1 (2285 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  273  Drafted and Watcom-compiled 1 more functions in screen_clip1 (2286 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  271  Drafted and Watcom-compiled 1 more functions in list_ops1 (2287 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  275  Drafted and Watcom-compiled 1 more functions in load_planes1 (2288 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  276  Drafted and Watcom-compiled 1 more functions in tactical_marks1 (2289 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  266  Drafted and Watcom-compiled 1 more functions in draw_loops1 (2290 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  276  Drafted and Watcom-compiled 1 more functions in tactical_marks1 (2291 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  276  Drafted and Watcom-compiled 1 more functions in tactical_marks1 (2292 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  277  Drafted and Watcom-compiled 2 more functions in small_wrappers6 (2294 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  277  Drafted and Watcom-compiled 4 more functions in small_wrappers6 (2298 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  277  Drafted and Watcom-compiled 3 more functions in small_wrappers6 (2301 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  278  Drafted and Watcom-compiled 4 more functions in small_wrappers7 (2305 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  278  Drafted and Watcom-compiled 2 more functions in small_wrappers7 (2307 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  278  Drafted and Watcom-compiled 2 more functions in small_wrappers7 (2309 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  278  Drafted and Watcom-compiled 3 more functions in small_wrappers7 (2312 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  278  Drafted and Watcom-compiled 2 more functions in small_wrappers7 (2314 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  278  Drafted and Watcom-compiled 1 more functions in small_wrappers7 (2315 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  279  Drafted and Watcom-compiled 3 more functions in copy_rects1 (2318 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  280  Drafted and Watcom-compiled 2 more functions in script_helpers2 (2320 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  280  Drafted and Watcom-compiled 2 more functions in script_helpers2 (2322 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  280  Drafted and Watcom-compiled 2 more functions in script_helpers2 (2324 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  281  Drafted and Watcom-compiled 2 more functions in page_lookup1 (2326 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  279  Drafted and Watcom-compiled 2 more functions in copy_rects1 (2328 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  282  Drafted and Watcom-compiled 2 more functions in trace_loaders1 (2330 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  283  Drafted and Watcom-compiled 2 more functions in text_metrics1 (2332 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  266  Drafted and Watcom-compiled 2 more functions in draw_loops1 (2334 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  274  Drafted and Watcom-compiled 1 more functions in state_reset1 (2335 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  273  Drafted and Watcom-compiled 1 more functions in screen_clip1 (2336 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  284  Drafted and Watcom-compiled 2 more functions in search_helpers1 (2338 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  284  Drafted and Watcom-compiled 2 more functions in search_helpers1 (2340 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  254  Drafted and Watcom-compiled 1 more functions in code_maps1 (2341 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  278  Drafted and Watcom-compiled 1 more functions in small_wrappers7 (2342 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  278  Drafted and Watcom-compiled 1 more functions in small_wrappers7 (2343 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  282  Drafted and Watcom-compiled 1 more functions in trace_loaders1 (2344 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  284  Drafted and Watcom-compiled 1 more functions in search_helpers1 (2345 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  284  Drafted and Watcom-compiled 1 more functions in search_helpers1 (2346 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  285  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2347 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  286  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2349 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  287  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2351 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  288  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2353 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  289  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2355 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  290  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2356 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  291  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2358 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  292  Drafted and Watcom-compiled 2 more functions in float_helpers9 (2360 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  293  Drafted and Watcom-compiled 7 more functions in misc_drafts1 (2367 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  294  Drafted and Watcom-compiled 5 more functions in misc_drafts1 (2372 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  295  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2375 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  296  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (2379 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  297  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (2383 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  298  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2384 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  299  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2387 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  300  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (2391 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  301  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2394 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  302  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2394 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  303  Drafted and Watcom-compiled 5 more functions in misc_drafts1 (2399 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  304  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2401 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  305  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (2405 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  306  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2407 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  307  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2408 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  308  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2410 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  309  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2413 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  310  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2415 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  311  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2418 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  312  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2420 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  313  Drafted and Watcom-compiled 6 more functions in float_helpers9 (2426 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  314  Drafted and Watcom-compiled 4 more functions in float_helpers9 (2430 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  315  Drafted and Watcom-compiled 5 more functions in float_helpers9 (2435 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  316  Drafted and Watcom-compiled 0 more functions in float_helpers9 (2435 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  317  Drafted and Watcom-compiled 3 more functions in float_helpers9 (2438 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  318  Drafted and Watcom-compiled 2 more functions in float_helpers9 (2440 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  319  Drafted and Watcom-compiled 1 more functions in float_helpers9 (2441 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  320  Drafted and Watcom-compiled 2 more functions in float_helpers9 (2443 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  321  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2446 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  322  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2448 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  323  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2449 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  324  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2451 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  325  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2453 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  326  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2453 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  327  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2455 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  329  Drafted and Watcom-compiled 5 more functions in misc_drafts1 (2460 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  330  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2461 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  331  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2463 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  332  Drafted and Watcom-compiled 5 more functions in misc_drafts1 (2468 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  333  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2470 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  334  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (2474 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  335  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (2478 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  336  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2481 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  337  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2484 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  338  Drafted and Watcom-compiled 3 more functions in script_helpers2 (2487 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  339  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2489 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  340  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2492 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  341  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2495 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  342  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2498 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  343  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2498 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  344  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2501 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  345  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2504 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  346  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (2508 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  347  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2509 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  348  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2512 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  349  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2514 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  350  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2515 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  351  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2516 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  352  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2517 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  353  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2519 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  354  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2521 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  355  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2523 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  356  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2526 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  357  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2527 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  358  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (2531 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  359  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2531 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  360  Drafted and Watcom-compiled 6 more functions in misc_drafts1 (2537 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  361  Drafted and Watcom-compiled 5 more functions in misc_drafts1 (2542 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  362  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2543 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  363  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2546 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  364  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2548 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  365  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2549 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  366  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2550 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  367  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2552 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  368  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2553 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  369  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2554 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  370  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2555 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  371  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2556 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  372  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2557 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  373  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2560 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  374  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2561 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  375  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2562 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  376  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2563 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  377  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2564 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  378  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2565 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  379  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2566 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  380  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2567 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  381  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2568 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  382  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2569 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  383  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2570 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  384  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2571 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  385  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2572 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  386  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2573 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  387  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2574 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  388  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2575 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  389  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2576 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  390  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2577 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  391  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2578 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  392  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2579 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  393  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2581 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  394  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2582 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  395  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2583 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  396  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2584 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  397  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2585 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  398  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2586 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  399  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2587 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  400  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2588 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  401  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2589 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  402  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2590 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  403  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2591 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  404  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2592 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  405  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2593 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  406  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2594 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  407  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2595 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  408  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2596 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  409  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2597 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  410  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2598 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  411  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2599 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  412  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2600 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  413  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2601 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  414  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2602 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  415  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2603 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  416  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2604 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  417  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2605 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  418  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2606 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  419  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2607 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  420  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2608 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  421  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2609 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  422  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2610 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  423  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2611 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  424  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2612 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  425  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2613 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  426  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2614 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  427  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2615 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  428  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2615 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  429  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2616 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  430  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2617 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  431  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2618 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  432  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2619 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  433  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2620 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  434  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2621 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  435  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2622 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  437  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2623 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  438  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2624 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  439  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2625 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  440  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2625 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  441  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2626 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  442  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2627 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  443  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2629 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  444  Drafted and Watcom-compiled 5 more functions in misc_drafts1 (2634 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  445  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2637 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  446  Drafted and Watcom-compiled 7 more functions in misc_drafts1 (2644 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  447  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2645 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  448  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2647 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  449  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2649 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  450  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2651 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  451  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2652 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  452  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2653 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  453  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2654 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  454  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2655 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  455  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2656 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  456  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2657 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  457  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2658 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  458  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2659 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  459  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2660 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  460  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2661 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  461  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2662 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  462  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2664 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  463  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2665 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  464  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2665 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  465  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2666 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  466  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2667 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  467  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2669 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  468  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2670 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  469  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2671 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  470  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2672 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  471  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2673 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  473  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2674 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  474  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2675 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  475  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2676 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  476  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2677 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  477  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2678 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  478  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2679 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  479  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2680 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  481  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2681 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  482  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2682 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  483  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2683 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  484  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2684 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  485  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2685 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  486  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2686 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  487  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2687 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  488  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2688 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  489  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2689 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  490  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2690 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  491  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2691 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  492  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2692 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  493  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2693 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  494  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2694 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  495  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2695 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  496  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2696 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  497  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2697 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  498  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2698 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  499  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2699 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  500  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2700 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  501  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2701 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  502  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2702 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  503  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2703 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  504  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2704 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  505  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2705 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  506  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2706 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  507  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2707 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  508  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2708 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  509  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2710 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  510  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2711 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  511  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2712 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  512  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2713 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  513  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2714 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  514  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2715 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  515  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2716 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  516  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2717 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  517  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2718 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  518  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2719 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  519  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2720 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  520  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2721 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  521  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2723 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  522  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2724 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  523  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2725 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  524  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2727 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  525  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2729 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  526  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2732 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  527  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2733 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  528  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2735 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  529  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2736 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  530  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2737 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  531  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2738 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  532  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2739 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  533  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2740 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  534  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2741 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  535  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2742 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  536  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2743 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  537  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2744 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  538  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2745 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  539  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2746 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  540  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2747 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  541  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2748 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  542  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2750 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  543  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2752 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  544  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2753 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  545  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2754 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  546  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2755 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  547  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2756 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  548  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2757 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  549  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2758 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  550  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2759 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  551  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2760 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  552  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2761 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  553  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2762 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  554  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2764 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  555  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2765 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  556  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2767 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  557  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2768 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  558  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2769 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  559  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2771 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  560  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2772 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  561  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2773 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  562  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2774 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  563  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2775 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  564  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2776 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  565  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2777 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  566  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2778 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  567  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2779 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  568  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2780 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  569  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2782 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  570  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2783 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  571  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2784 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  572  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2785 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  573  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2786 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  574  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2787 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  575  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2788 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  576  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2789 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  577  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2790 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  578  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2791 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  579  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2792 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  580  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2793 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  581  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2794 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  582  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2795 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  583  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2796 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  584  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2797 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  585  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2798 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  586  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2799 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  587  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2800 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  588  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2801 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  589  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2802 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  590  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2803 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  591  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2804 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  592  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2805 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  593  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2806 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  594  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2807 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  595  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2808 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  596  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2811 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  597  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2813 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  598  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2813 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  599  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2814 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  600  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2815 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  601  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2816 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  602  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2817 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  603  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2818 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  604  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2819 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  605  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2820 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  606  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2821 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  607  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2822 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  608  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2823 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  609  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2825 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  610  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2827 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  611  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2828 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  612  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2830 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  613  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2832 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  614  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2833 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  615  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2834 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  616  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2835 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  617  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2836 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  618  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2837 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  619  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2838 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  620  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2838 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  621  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2839 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  622  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2839 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  623  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2840 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  624  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2841 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  625  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2842 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  626  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2843 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  627  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2844 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  628  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2844 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  629  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2845 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  630  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2846 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  631  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2848 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  632  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2849 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  633  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2850 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  634  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2851 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  635  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2852 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  636  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2853 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  637  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2854 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  638  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2855 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  639  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2856 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  640  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2857 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  641  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2858 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  642  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2859 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  643  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2859 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  644  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2860 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  645  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2861 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  646  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2862 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  647  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2863 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  648  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2864 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  649  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2865 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  650  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2866 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  651  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2867 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  652  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2868 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  653  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2869 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  654  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2870 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  655  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2871 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  656  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2872 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  657  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2873 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  658  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2874 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  659  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2875 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  660  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2876 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  661  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2877 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  662  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2877 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  663  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2878 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  664  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2879 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  665  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2880 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  666  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2881 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  667  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2882 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  668  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2883 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  669  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2884 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  670  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2885 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  671  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2886 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  672  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2887 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  673  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2888 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  674  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2889 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  675  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2890 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  676  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2890 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  677  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2891 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  678  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2892 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  679  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2893 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  680  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2894 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  681  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2895 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  682  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2896 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  683  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2897 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  684  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2898 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  685  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2899 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  686  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2900 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  687  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2901 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  688  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2902 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  688  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2902 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  689  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2902 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  690  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2903 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  691  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2904 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  692  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2905 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  692  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2905 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  693  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2906 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  694  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2907 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  695  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2909 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  696  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2910 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  697  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2911 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  698  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2912 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  699  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2913 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  700  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2914 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  701  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2915 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  702  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2916 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  703  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2917 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  704  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2918 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  705  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2919 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  705  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2919 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  706  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2920 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  707  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2921 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  708  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2922 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  709  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2923 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  710  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2924 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  711  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2925 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  712  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2926 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  713  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2927 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  714  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2928 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  715  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2929 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  716  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2930 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  717  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2931 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  718  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2932 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  719  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2933 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  720  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2934 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  721  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2935 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  722  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2936 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  722  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2936 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  723  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2937 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  724  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2938 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  725  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2939 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  726  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2940 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  727  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2941 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  728  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2942 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  729  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2943 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  730  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2944 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  731  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2945 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  732  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2946 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  733  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2947 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  734  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2948 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  735  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2949 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  736  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2950 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  736  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2950 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  737  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2951 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  738  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2952 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  739  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2953 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  740  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2954 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  741  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2955 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  742  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2956 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  743  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2957 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  744  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2958 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  744  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2958 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  745  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2959 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  745  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2959 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  746  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2960 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  747  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2961 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  748  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2962 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  749  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2963 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  750  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2964 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  751  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2965 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  752  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2966 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  753  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2967 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  754  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2968 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  755  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2969 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  755  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2969 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  756  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2970 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  757  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2971 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  758  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2972 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  758  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (2972 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  759  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2973 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  760  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2974 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  761  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2975 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  762  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2976 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  763  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2977 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  764  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2978 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  765  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2979 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  766  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2980 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  767  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2981 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  768  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2982 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  769  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2983 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  770  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2984 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  771  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2985 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  772  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2986 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  773  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2988 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  774  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2990 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  775  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (2993 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  776  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (2995 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  777  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2996 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  778  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2997 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  779  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2998 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  780  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (2999 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  781  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3000 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  782  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3001 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  783  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3002 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  784  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3003 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  785  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3004 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  786  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3005 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  787  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3006 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  788  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3007 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  789  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3008 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  790  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3009 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  791  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3010 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  792  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3011 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  793  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3012 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  794  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3013 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  795  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3014 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  796  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3015 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  797  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3016 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  797  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (3016 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  798  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3018 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  799  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (3018 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  800  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3019 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  801  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3020 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  802  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3021 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  803  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3022 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  804  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3023 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  805  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3024 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  806  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (3024 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  807  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3025 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  808  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3026 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  809  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3027 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  809  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (3027 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  810  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3028 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  810  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (3028 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  811  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3029 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  812  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3030 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  813  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3031 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  814  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3032 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  815  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3033 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  816  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3034 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  817  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3035 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  818  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3036 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  819  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3037 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  820  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3038 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  821  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3039 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  822  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3040 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  823  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3041 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  824  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3042 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  825  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3043 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  826  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3044 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  827  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3045 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  828  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3046 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  829  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3047 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  829  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (3047 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  830  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3048 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  831  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3049 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  832  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3051 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  833  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3052 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  834  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3053 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  835  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3054 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  836  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3055 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  837  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3056 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  838  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3057 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  839  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3058 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  840  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3059 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  841  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3060 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  842  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3061 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  843  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3062 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  844  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3063 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  845  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3064 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  846  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (3068 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  847  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3069 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  847  Drafted and Watcom-compiled 0 more functions in misc_drafts1 (3069 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  848  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (3072 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  849  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3074 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  850  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3075 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  851  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3076 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  852  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3078 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  853  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3079 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  854  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3081 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  855  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3083 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  856  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3084 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  857  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3086 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  858  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (3089 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  859  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3090 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  860  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3091 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  861  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3092 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  862  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3094 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  863  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3095 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  864  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3096 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  865  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3097 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  866  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3098 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  867  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3100 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  868  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3101 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  869  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3102 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  870  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3104 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  871  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3105 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  872  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3107 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  873  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3108 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  874  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3110 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  875  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (3113 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  876  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3114 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  877  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3115 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  878  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3116 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  879  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3117 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  880  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (3120 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  881  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3122 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  882  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3123 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  883  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3124 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  884  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (3128 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  885  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3129 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  886  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3130 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  887  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3132 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  888  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (3135 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  889  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3137 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  890  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3139 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  891  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (3142 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  892  Drafted and Watcom-compiled 5 more functions in misc_drafts1 (3147 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  893  Drafted and Watcom-compiled 5 more functions in misc_drafts1 (3152 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  894  Drafted and Watcom-compiled 6 more functions in misc_drafts1 (3158 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  895  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3160 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  896  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3162 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  897  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3163 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  898  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3164 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  899  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3165 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  900  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (3168 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  901  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3170 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  902  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3171 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  903  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3173 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  904  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3174 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  905  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3175 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  906  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3176 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  907  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3177 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  908  Drafted and Watcom-compiled 3 more functions in misc_drafts1 (3180 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  909  Drafted and Watcom-compiled 4 more functions in misc_drafts1 (3184 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  910  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3185 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  911  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3186 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  912  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3187 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  913  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3189 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  914  Drafted and Watcom-compiled 5 more functions in misc_drafts1 (3194 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  915  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3195 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  916  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3196 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  917  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3197 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  918  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3199 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  919  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3200 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  920  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3201 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  921  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3202 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  922  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3203 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  923  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3204 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  924  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3206 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  925  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3207 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  926  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3208 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  927  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3209 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  928  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3210 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  929  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3211 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  930  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3212 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  931  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3213 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  932  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3214 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  933  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3215 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  934  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3216 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  935  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3217 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  936  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3218 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  937  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3219 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  938  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3220 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  939  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3221 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  940  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3222 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  941  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3223 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  942  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3224 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  943  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3225 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  944  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3226 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  945  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3227 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  946  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3228 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  947  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3229 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  948  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3230 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  949  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3231 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  950  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3232 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  951  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3234 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  952  Drafted and Watcom-compiled 2 more functions in misc_drafts1 (3236 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  953  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3237 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  954  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3238 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  955  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3239 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  956  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3240 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  957  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3241 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  958  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3242 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  959  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3243 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  960  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3244 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  961  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3245 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  962  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3246 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  962  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3247 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  963  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3248 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  964  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3249 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  965  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3250 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  966  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3251 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  967  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3252 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  968  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3253 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  969  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3254 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  970  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3255 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  971  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3256 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-03  971  Drafted and Watcom-compiled 1 more functions in misc_drafts1 (3257 total drafted/compiling); callers and effects in compiler-abi.md. Host and guest differential tests not yet run (bulk drafting mode).
2026-11-05  972  Recovered seven related renderer stream entry points as host-tested drafts (addresses 0xc49c4..0xc4d43), and identified two previously omitted Ghidra functions at 0xc4c30/0xc4ccb by original payload disassembly and direct caller evidence. Tracker: 4279 entries, 3257 compiles, 7 drafted; OpenWatcom compilation, stack ABI, Ghidra boundary repair and guest replay still pending. See docs/subsystems/renderer-streams.md.
2026-11-05  973  Drafted and host-tested 20 additional entries in one batch: 4 renderer queue/sort, 4 endian/index helpers, 2 bitstream decoders, 3 scene/viewport adapters, 2 audio/video adapters, 4 threaded opcode handlers, and 1 DOS regblock bridge. Repaired the Ghidra boundary c5372/c5384 and extended c41b0 from 131 to 170 bytes to cover its self-modified alternate overflow branch. Eight omitted entry boundaries added: 4287 tracked, 3257 compiles, 27 drafts. Guest splice/replay pending. See docs/subsystems/decomp-batch-083.md.
2026-11-05  974  Drafted and host-tested 20 already-inventoried, named mission-planning callback bodies, 4bc40..50c40: map/overlay cycling, waypoints, speed bounds and menu transitions. Tracker: 4287 tracked, 3257 compiles, 47 drafts; signatures and guest splice/replay remain unverified. See docs/subsystems/decomp-batch-084.md.
2026-11-05  975  Drafted and host-tested 20 already-inventoried callback functions: eight campaign-menu bodies 40e50..40f9c and twelve aircraft/weapons-panel bodies 68f40..699d4. Tracker: 4287 tracked, 3257 compiles, 67 drafts; no new ABI confirmations or spliced replays. See docs/subsystems/decomp-batch-085.md.
2026-11-05  976  Drafted and host-tested 20 already-inventoried multiplayer GUI callback bodies, 335c0..35eb8, including baud/parity/serial/IPX controls, network launch, DirectPlay list rows and receive-status widgets. Explicitly identified shared jump/return tails at 33618/33630/345d2; no guest ABI evidence claimed. Tracker: 4287 tracked, 3257 compiles, 87 drafts. See docs/subsystems/decomp-batch-086.md.
2026-11-05  977  Reconstructed 15 original 74-byte damage/environment line-parser entry points db95c..dbd84 and five associated horizon/environment flag handlers db8a8..dbe54 as host-tested drafts. Captured the shared cursor mutation, including the unconditional byte-before-line-end overwrite, and verified all destinations against the SHA-pinned original disassembly. Tracker: 4287 entries, 3257 compiles, 107 drafted; Watcom and guest tests pending. See docs/subsystems/decomp-batch-087.md.
2026-11-05  978  Drafted and host-tested 20 further named original functions: 8 environment/configuration handlers dbdd0 and dbe5c..dbef8, and 12 mission/GUI callbacks 4318..ca40. Verified original entry patterns with an opt-in SHA-pinned disassembly audit. Preserved the overlap of difficulty fields 121e44/121e46, the repeated comparison at b574, and the four pilot row selectors. Tracker: 4287 entries, 3257 compiles, 127 drafted; guest and Watcom tests pending. See docs/subsystems/decomp-batch-088.md.
2026-11-05  979  Drafted and host-tested 63 named GUI/configuration entries in one cumulative batch: 48 byte-verified five-byte JMP aliases, two 15-byte Quit CALL/JMP wrappers, and 13 substantive mission/GUI and cloud-state bodies. Distinguishes dispatch-only entries from algorithm recovery; preserved original mission digit decoding, assignment-loop wraparound and gnNewSocket->onopenCredits fallthrough. Tracker: 4287 entries, 3257 compiles, 190 drafted; no guest splices or new Watcom-compiles. See docs/subsystems/decomp-batch-089.md.
2026-11-05  980  Reconstructed host drafts for 20 already-named READCOLS environment/configuration bodies, including destructive line scanning, numeric sign conversions, five-way missile palette choice, map-name variants, flak/player bridges and 20-byte cloud records. SHA-pinned binary audit and synthetic guest-RAM host tests added; scanner/engine ABI remain explicit unverified adapters. Tracker: 4287 entries, 3257 compiles, 210 drafted; zero new Watcom compiles or guest splices. See docs/subsystems/decomp-batch-090.md.
2026-11-05  981  Drafted and host-tested 12 additional READCOLS palette/crater/sky callbacks with original-byte audit: crater stride 18 and overlapping high-word counter, 110-byte crater allocations, sun texture linked nodes, cannon-hit stride 18, sky objects stride 5, original dual SETSUN scanner. Tracker: 4287 entries, 3257 compiles, 222 drafted; engine/scanf guest ABI and Watcom splice/replay unverified. See docs/subsystems/decomp-batch-091.md.
2026-11-05  982  Drafted four further READCOLS routines: 14-key palette dispatch plus overwritten final fallback pointer, cockpit palette bank selection, 9-byte lightning records, and 110-byte smoke record rings. Host tests and SHA-pinned static audits pass. Original tmsun_flare's overlapping 16-bit scanf locals remain unresolved and undrafted. Tracker: 4287 entries, 3257 compiles, 226 drafted; guest ABI and splices pending. See docs/subsystems/decomp-batch-092.md.
2026-11-05  983  Ported five high-score C routines 4050/40dc/41fc/42a4/433c. Host C++ reference parity across 2,000 insertion cases, file byte tests, linked GUI list tests, and pinned original-byte audit; guest ABI, Watcom build and splice pending. Tracker: 4287 entries, 3257 compiles, 231 drafted. See docs/subsystems/decomp-batch-093.md.
2026-11-05  984  Drafted seven named GUI script handlers f4db8..f53bc: SIZE/FROM/BACKDROP mode-specific fields, ACTION/POINTER duplicate-flag quirk, GADGETRY/NAME asset paths and 256-RGB palette extrema. Host tests and SHA-pinned disassembly audits pass; graphics-driver ABI and guest splice still pending. Tracker: 4287 entries, 3257 compiles, 238 drafted. See docs/subsystems/decomp-batch-094.md.
2026-11-05  985  Drafted three original GUI-script and OPTIONS routines f3b78, f46cc, f5484. Lexer now handles 80-byte tokens, quoted strings and comma cursor semantics in guest RAM; rectangle parser handles optional FROM/SIZE/HOTSPOT and callback state; OPTIONS dispatcher covers 14 keywords. Strict host tests and pinned-byte audits pass, guest splice pending. Tracker: 4287 entries, 3257 compiles, 241 drafted. See docs/subsystems/decomp-batch-095.md.
2026-11-05  986  Drafted two more OPTIONS label table routines f4347 and f44b6: ASCII-folded name match through '=', append/replace/remove, pointer/flag array relocation and conditional allocation release. Tested synthetic resize-move, missing/empty labels, deletion of first/last entry and allocation failure; guest heap ABI remains unverified. Tracker: 4287 entries, 3257 compiles, 243 drafted. See docs/subsystems/decomp-batch-095-labels.md.
2026-11-05  987  Drafted original OPTIONS reader f2d74 and formatter f6124. The reader reuses native f2c78/f2cd0/f2d54 and makes the previous 14-way f3b78 dispatcher test run from real script text, including skipped comments and labels; preserved f2da4 fallthrough into ASCII lowering. Formatter remains callback-adapted. Pinned original-byte audit and strict C host test; guest splice pending. Tracker: 4287 entries, 3257 compiles, 245 drafted. See docs/subsystems/decomp-batch-096.md.
2026-11-05  988  Drafted original OPTIONS cursor timing/stepping f3d54 and display-event loop f3f84. Includes signed divide-by-eight, three velocity thresholds, tick repeat countdown, 320x200 clamping, 20-byte display record forwarding, pointer selection/release and fade/no-fade cleanup. Source-grounded GCC C11 tests and pinned original-byte audit; renderer, hit testing, polling and guest calling conventions remain adapters, with no Watcom splice. Tracker: 4287 entries, 3257 compiles, 247 drafted, 669 unknown. See docs/subsystems/decomp-batch-097.md.
2026-11-05  3.9  Multi-checkpoint replays compare original and SHA-pinned identity-patched entry at four instruction budgets (12 runs total); early startup uses explicit RAM-only mode until a framebuffer exists. Synthetic x87 test isolates the local Unicorn host crash. No recovered C function verified. See docs/subsystems/decomp-batch-100.md.
2026-11-05  3.8  First compiled-C diagnostic guest replacement: GCC -m32 produces a 15-byte ELF32 body for hiscore_qualifies 0x432c, matching the reviewed original extent. Strict ELF32/R_386_32-to-EFPATCH1 conversion and 10 isolated original-vs-C Unicorn guest cases now pass (signed boundaries, complete GPRs, RET and write-watch). This is not an OpenWatcom/OMF replacement, a full-game replay, or verified game logic; tracker states unchanged. See docs/subsystems/decomp-batch-101.md.
2026-11-05  3.8  Second substantive compiled-C guest comparison: high-score insertion 0x433c accepts EAX as name pointer and preserves six GPRs. GCC -m32 -mregparm=1 -fcall-saved-ecx -fcall-saved-edx emits a 226-byte body, padded to the original 242-byte extent solely for isolated testing. Original-vs-C Unicorn x86 matches 100 full 250-byte table/ABI cases (shifts, NUL padding, ties, signed overflow extrema, random); outside-table guest write-watch clean. Opt-in multi-section ELF patch selection and explicit reviewed-draft short-body flags are tested. Still no Watcom/OMF or whole-game modified-code replay; state remains drafted and zero functions verified. See docs/subsystems/decomp-batch-102.md.
2026-11-05  3.10  Extended original-vs-recompiled GCC32 guest differentials to 17 recovered functions in three existing modules: 10,464/10,464 isolated cases pass, with guarded memory, register and entrypoint checks. Corrected 0xcc81c's originally misrecorded EDX preservation (original clobbers EDX); tracked GCC scratch EDX and EAX differences explicitly. Diagnostic ELF selection now supports up to 256 section headers (needed by 73-section low_helpers object). Python tracker and tests pass; no OpenWatcom OMF or patched-game replay, zero new verified functions. See docs/subsystems/decomp-batch-103.md.
2026-11-05  3.10  Batch 104: fixed the local static Unicorn x87 guest crash via linker --no-relax; 5/5 x87 and deterministic PIT date/time tests pass. Identity-entry survey reaches 121 of 4,287 tracked function entries in a bounded pre-CD startup (43,161 calls). Original-guest ABI checks cover 1,024 segment:offset, 512 lowercase, 512 string-copy and 512 generator cases, correcting EAX/EDX signature errors. Three GCC32 C replacements actually run 24 times during the real startup, and 3 original + 2 patched deterministic runs agree on seven hashes covering 67,108,783 of 67,108,864 guest bytes (the 81 changed instructions excluded). Missing proprietary CD data prevents gameplay; no production Watcom splice or verified functions. See docs/subsystems/decomp-batch-104.md.
2026-11-06  3.10  Batch 105: OpenWatcom 2.0 compiled and OMF-spliced `to_lower_ascii` (exact 14-byte instruction match) and `encode_runtime_segment_offset` (33-byte body in 36-byte extent). Original-vs-OMF isolated tests pass 263 and 1,024 cases respectively. Lowercase passes a 3-original/2-patched 20M full-64MiB startup replay; the encoder's full-memory replay differs in high-stack RAM. Extracted private CD data enabled a menu→quick-fight→cockpit path, but long unpatched menu runs are not deterministic (interrupt counter and BSS hash vary) even with fixed DOS time and keyboard-only input. Both remain `spliced`; zero `verified`. Replay-suite tests cover deterministic clock/input flags. `nix flake check -L` is blocked by daemon socket access. See docs/subsystems/decomp-batch-105.md.
2026-11-06  3.9  Batch 106: traced long-replay nondeterminism to host `steady_clock` sampling for VGA retrace at port 0x3DA. Added an explicit synthetic guest retrace clock and fixed it in replay-suite runs. Main-menu, quick-fight setup and cockpit cases now pass repeated-original/patched comparisons for status, interrupts, ticks, BSS and framebuffer. `to_lower_ascii` passes 263 isolated cases, 3+2 full-64MiB startup replays and the three long gameplay cases; promoted it to the first `verified` function. Encoder remains `spliced` with unexplained high-stack full-RAM mismatch. Long cases hash selected state, so §3.9 is still partial. See docs/subsystems/decomp-batch-106.md.
2026-11-06  3.9  Batch 107: verified `to_upper_ascii` (`0xddbc8`): exact 14-byte OpenWatcom body, 267 isolated cases, 3+2 500M-instruction full-RAM main-menu runs (67,108,850 guest bytes), plus combined lower/upper menu/setup/cockpit replays with both patch entries hit. Encoder mismatch now traces to C codegen omitting the original EBX save/ADD effects; a temporary exact-body assembly probe passes 3+2 full-RAM startup runs but is not a C replacement. Keep encoder `spliced` and investigate preserving these effects in C. See docs/subsystems/decomp-batch-107.md.
2026-11-06  3.9  Batch 109: changed the encoder C expression to produce the original ADD result/flags. The 33-byte OpenWatcom body passes 1,024 isolated cases with full EFLAGS checking and an 8-byte below-ESP scratch allowance. A 500M menu replay matches status, selected RAM and framebuffer with seven hits. The 20M full-RAM replay still differs in active high-stack state; the encoder remains `spliced`. The longer menu/setup/cockpit run was interrupted after menu and setup passed, so cockpit is unverified for this entry. See docs/subsystems/decomp-batch-109.md.
2026-11-06  3.10  Batch 110: retained the original 15-instruction count by adding two flag-neutral OpenWatcom inline nops to the C encoder. The 34-byte body fits the 36-byte extent. Its 1,024-case/full-EFLAGS differential passes with only 8 bytes below incoming ESP treated as callee scratch; 3+2 20M full-RAM startups and repeated menu/setup/cockpit runs pass, including all 64 KiB BSS and framebuffer hashes. Promoted `encode_runtime_segment_offset` to `verified`; this is the third verified OpenWatcom replacement. See docs/subsystems/decomp-batch-110.md.
2026-11-06  3.10  Batch 111: recovered `read_runtime_slot_flag` (`0x121cc`, 33 bytes) as direct guest loads and signed index arithmetic. Its 26-byte OpenWatcom code body fits the original extent; the focused OMF splice passes 380 boundary/seeded cases and 3 captured gameplay calls. All three repeated-baseline menu/setup/cockpit cases pass, with the candidate hit 3 times in cockpit. Promoted it to `verified`; four OpenWatcom replacements are now verified. See docs/subsystems/decomp-batch-111.md (189ee9f1).
2026-11-06  3.10  Batch 112: verified `initialize_runtime_probe_latch` (`0x121f0`, 29 bytes). OpenWatcom resolves its FLAT-framed self-relative call to `mouse_read_motion_ce02c`; the resulting OMF patch is byte-for-byte identical to the original. Eight isolated cases and repeated-baseline menu/setup/cockpit replays pass, with one cockpit hit. The captured differential cannot model the helper's real-mode interrupt; the exact byte match and live replay cover that boundary. See docs/subsystems/decomp-batch-112.md. Commit: `e5305802`.
2026-11-06  3.11  `copy_record9_d0858` (`0xd0858`, 55 bytes) now has an OpenWatcom C lowering that fits its extent and preserves the original column-wise dword access order. Eight synthetic overlap/order cases and one captured live call pass differential; the cockpit replay reaches it 42 times but differs in the selected 64 KiB BSS hash (framebuffer, status, ticks and interrupts match). Keep it `spliced`, not verified, while the checkpoint divergence is unresolved. `latch_values_ce328` remains blocked: the straightforward C object is 61 bytes for a 44-byte extent. See docs/subsystems/decomp-batch-113.md. Commit: `5d861e4a`.
2026-11-06  3.12  Confirmed the direct caller ABI and original EDX clobber for `group_flag_negative_cc1ac` (`0xcc1ac`). A focused OpenWatcom C lowering compiles cleanly but is 72 bytes for a 61-byte body; the unreserved executable arena prevents safe placement. Corrected the signature preservation row and marked the candidate blocked. See docs/subsystems/decomp-batch-114.md. Commit: `ef1fa19b`.
2026-11-06  3.13  Verified `init_bounds_d0b68` (`0xd0b68`, 63 bytes): the production OpenWatcom body and focused fixture equal all original bytes. Twelve isolated cases and three captured caller states pass; the repeated-baseline 1.5B cockpit replay reaches the function 155 times with matching selected BSS/framebuffer and execution counters. See docs/subsystems/decomp-batch-115.md. Commit: `8ad16c15`.
2026-11-06  3.14  `find_node_by_byte_b` (`0xcc26c`, 25 bytes) compiles to a 21-byte OpenWatcom C body and passes eight isolated list cases. The safe OMF patch fits. Original call-capture runs at menu (500M), quick-fight setup (700M), and cockpit (1.5B) did not reach this entry; keep it `spliced` until an actual caller path is replayed. See docs/subsystems/decomp-batch-116.md. Commit: `57e267d7`.
2026-11-06  3.15  Re-audited `copy_record9_d0858` (`0xd0858`): the 49-byte OpenWatcom C body matches its focused fixture, preserves EAX and EFLAGS, and passes eight isolated cases plus one captured call. The repeated 1.5B cockpit replay still differs in BSS despite matching frame/status/interrupt counters; the exact changed field is `g_filtered_object_lookup_calls` (`0x1a3580`, `0x8efe` original vs `0x8ef9` patched). Keep `spliced`, not verified, while resolving the timing-driven lookup-count difference. See docs/subsystems/decomp-batch-113.md. Commit: `52701340`.
2026-11-06  3.16  Verified `sync_flag_12ec8` (`0x12ec8`, 80 bytes): the 52-byte OpenWatcom body matches its fixture; five isolated cases, one captured indirect callback, and repeated-baseline menu/setup/cockpit replays pass with one cockpit hit and matching BSS/frame/counters. Blocked parent `reinitialize_runtime_slots` after a 124-byte Watcom body exceeded its 84-byte extent and 49 of 54 captured callbacks exposed unresolved calling-convention requirements. See docs/subsystems/decomp-batch-118.md.
2026-11-06  3.17  `max3_signed` (`0x3cd4`, 27 bytes) has a 19-byte OpenWatcom body that matches its fixture and passes 253 original-vs-patched isolated cases. Correction: the first cockpit replay used a stale pre-nop encoder patch; with the current fixture-derived encoder patch, repeated menu/setup/cockpit replay matches BSS/frame/status/counters, but `0x3cd4` receives no entry hit. Retain `spliced`, not verified, until a caller path is replayed. Audited overlapping x87 entries `0xa4470`/`0xa4472`: the two-byte `mov al,3` entry falls through into the checked-square-root body, whose ST0 contract and AL=3 stack imbalance are not represented by the current C model. Both entries are blocked pending a suitable x87/overlap differential. See docs/subsystems/decomp-batch-119.md.
2026-11-06  3.18  Verified `reset_unit_counter` (`0x5e658`, 12 bytes): a 10-byte OpenWatcom direct-store body preserves all GPRs, passes 37 isolated cases and one captured actual call, and reaches 22 times in cockpit. Repeated-baseline menu/setup/cockpit replay matches selected BSS, framebuffer, status, interrupt and tick counts. The four callers overwrite the original XOR flags before conditional use. See docs/subsystems/decomp-batch-120.md.
2026-11-06  3.19  Verified `set_runtime_phase_five` (`0x3dcc0`, 10 bytes): the OpenWatcom direct store matches the original body; 37 isolated cases and one captured cockpit call pass. Repeated-baseline menu/setup/cockpit replay passes with one cockpit hit and matching selected BSS, framebuffer, status, interrupt, tick and IRQ0 state. See docs/subsystems/decomp-batch-121.md.
2026-11-06  3.20  Verified `store_runtime_control_word` (`0x3cd64`, 6 bytes): the OpenWatcom C wrapper emits the original `mov [0x1a35bc],eax; ret`; 37 isolated cases and two captured actual calls pass. The repeated-baseline menu/setup/cockpit suite reaches the entry 2,435 times in cockpit and matches selected BSS, framebuffer and execution counters. See docs/subsystems/decomp-batch-122.md.
2026-11-06  3.21  Verified `initialize_mode_limits` (`0x58d24`, 29 bytes): the OpenWatcom C body writes the two words at their original instruction positions, preserves all registers/flags and retains the original nine-instruction cadence. Thirty-seven isolated cases and one captured cockpit call pass; repeated-baseline menu/setup/cockpit replay reaches it once in cockpit and matches selected BSS, framebuffer and execution counters. See docs/subsystems/decomp-batch-123.md.
2026-11-06  3.22  Verified `copy_active_buffer_pointer` (`0x7a428`, 11 bytes): the OpenWatcom body exactly matches the original absolute load/store/RET sequence; 37 isolated cases and one captured cockpit call pass. Repeated-baseline menu/setup/cockpit replay reaches it 320,002 times in cockpit and matches selected BSS, framebuffer and execution counters. See docs/subsystems/decomp-batch-124.md.
2026-11-06  3.23  Verified `set_control_mode_a` (`0x7eea0`, 25 bytes): the OpenWatcom C wrapper reproduces the original gate/select/store/return sequence byte for byte. Thirty-seven isolated cases and two captured cockpit states pass; repeated-baseline menu/setup/cockpit replay reaches the entry once in cockpit and matches selected BSS, framebuffer and execution counters. See docs/subsystems/decomp-batch-125.md.
2026-11-06  3.24  Verified paired setters `set_control_mode_b/c` (`0x810d0`/`0x810f4`): 37 isolated cases each; B passes two captured calls and C one sound-DMA captured call; repeated-baseline cockpit replays hit B and C and match selected BSS/frame/counters. The three-case `--sound off` suite passes 3/3. See docs/subsystems/decomp-batch-126.md.
2026-11-06  3.25  Verified `resolve_link_pointer` (`0x38658`): 10 isolated cases and one captured cockpit call pass; repeated-baseline menu/setup/cockpit replay hits the replacement 16 times and matches selected BSS, framebuffer and execution counters. Added bounded OMF resolution for Watcom's leading-underscore data externals and proven-zero-base FLAT absolute offsets. See docs/subsystems/decomp-batch-127.md.
2026-11-06  3.26  Verified `refresh_link_pointer` (`0x3856c`): two isolated cases and one captured call from `0x381a0` pass; repeated-baseline menu/setup/cockpit replay reaches it eight times, reaches its verified resolver 16 times, and matches selected BSS/frame/counters. See docs/subsystems/decomp-batch-128.md.
2026-11-06  3.27  `field_10dfe0` (`0x10dfe0`, 12 bytes) compiles warning-free and splices; five isolated cases pass. Keep it `spliced`: its live capture points to `0x05000000`, outside the runner's 64 MiB guest mapping, and captured differential faults on the original read at `0x10dfe5`. See docs/subsystems/decomp-batch-129.md.
2026-11-06  3.28  Corrected `get_state_106708` (`0x106708`) from EAX input to one caller-clean cdecl stack argument; OpenWatcom emits the original 13 bytes and four isolated cases pass. The repeated 3-case replay passes but has zero target hits; leave `spliced`. Differential runner now separates stack argument size from callee cleanup, and OMF matching covers cdecl leading-underscore code publics. See docs/subsystems/decomp-batch-130.md.
2026-11-06  3.29  `set_state_1_106634` (`0x106634`, 16 bytes) uses a C behavior model with inline OpenWatcom sequence because ordinary C emits 17 bytes. The 16-byte patch matches the original and four isolated cases pass; repeated menu/setup/cockpit replay passes 3/3 but reaches the entry zero times, so retain `spliced`. See docs/subsystems/decomp-batch-131.md.
2026-11-06  3.30  Corrected `set_field_10677c` (`0x10677c`, 16 bytes) and trace wrapper `0x102824` to caller-clean stack ABIs. Exact OpenWatcom body passes four isolated cases and one captured live call (`0x102853`). SB16-DMA golden replay is nondeterministic (baseline interrupts differ by two; patched state also differs), and SB16-probe stops on unsupported IN 0xa1; keep `spliced`. See docs/subsystems/decomp-batch-132.md.
2026-11-06  3.31  Corrected `set_field_10682c` (`0x10682c`, 16 bytes) and trace wrapper `0x1028c0` to caller-clean stack ABIs. The exact OpenWatcom body passes four isolated cases and one captured live call (`0x1028ef`). Sound-DMA replay reaches it four times, but repeated baselines differ by 42 interrupts (42,314 vs. 42,272) and patched selected BSS differs; keep `spliced`. See docs/subsystems/decomp-batch-133.md.
2026-11-06  3.32  `times_100` (`0x106be8`, 18 bytes) compiles and splices with OpenWatcom; modulo-2^32 multiply and EDX preservation pass 8 isolated cases. Repeated menu/setup/cockpit checkpoints match, but none reaches the virtual-I/O caller path, so the required-hit gate fails and the candidate stays `spliced`. See docs/subsystems/decomp-batch-134.md.
2026-11-06  3.33  Corrected `pause_object_10687c` (`0x10687c`, 22 bytes) and wrapper `0x1027a0` to caller-clean cdecl. The exact OpenWatcom body and five isolated cases pass; SB16-DMA replay reaches neither the target nor a stable baseline (42,279 vs. 42,324 interrupts, with patched RAM differing), so keep it `spliced`. See docs/subsystems/decomp-batch-135.md.
2026-11-06  3.34  `exchange_slot_1041e4` (`0x1041e4`, 25 bytes) is verified. The exact OpenWatcom body preserves the original cdecl stack ABI and EBX/EDX clobbers; four isolated and four captured startup calls pass. Repeated menu/setup/cockpit replay passes 3/3 with 19 hits per scenario. Corrected wrapper `0x101e14` and its two-argument call sites. See docs/subsystems/decomp-batch-136.md.
2026-11-06  3.35  `find_last_char` (`0x10d1da`, 26 bytes) is verified with an exact OpenWatcom body, six isolated cases and four captured startup calls. Repeated menu/setup/cockpit replay passes 3/3 with 20,889, 20,933 and 278,507 hits; matching state/counters. Corrected the signature to record EDX clobber. See docs/subsystems/decomp-batch-137.md.
2026-11-06  3.36  Corrected `exchange_callback_106938` (`0x106938`) and wrapper `0x102dbc` to caller-clean cdecl. Exact OpenWatcom body passes five isolated and four captured sound-channel calls. Sound-off paths do not hit it; SB16-DMA reaches six times but remains nondeterministic (42,342 vs. 42,372 interrupts, selected BSS differs), so keep `spliced`. See docs/subsystems/decomp-batch-138.md.
2026-11-06  3.37  `strip_cr_inplace` (`0x1091e0`, 28 bytes) is verified with an exact OpenWatcom body, six isolated cases and one captured startup call (348 guest RAM bytes compared). Repeated menu/setup/cockpit replay passes 3/3 with 1/1/2 hits and matching state/counters. See docs/subsystems/decomp-batch-139.md.
2026-11-06  3.38  `times_100_div_10` (`0x106bfc`, 32 bytes) now models modulo-2^32 multiplication followed by signed division; OpenWatcom patch and 12 isolated cases pass. The repeated menu/setup/cockpit replay preserves checkpoints but has zero candidate hits because callers are in virtual-I/O/serial-tracker paths; retain `spliced`. See docs/subsystems/decomp-batch-140.md.
2026-11-09  native-startup  Recovered the strict decimal type-3 route of configuration interpreter 0xe882c and 16-byte CONTROL callback 0x82710. Synthetic width/callback tests and opt-in SHA-pinned real-image startup pass. MUSIC (type 5), diagnostics and first-screen 0x30214 remain explicit boundaries; not Watcom-spliced/verified. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Extended 0xe882c through verified type-5 text copying and original MUSIC/SOUNDFX callbacks (0x8289c/0x82814) for strict two-short-number settings. Shipped prefix now stops at JOYSTICK; an all-supported sound configuration reaches 0x30214. Original diagnostic/error routes and non-sound callbacks remain unsupported. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Added JOYSTICK's disconnected-0xd6960 callback and SPEECH/VRHELMET cases, completing the unchanged shipped EF2000.CFG in GCC/Linux startup. The probe now stops at 0x30214 after config arena release. Connected joystick, parser diagnostics, original guest replay and Watcom splice remain unsupported. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Recovered the `ReadGenSeqs` 0x30214 prelude (40 sequence records; 0x9c980 default directory and 0x9ca57 Views resource suffix) and advanced the Linux GCC probe to the real resource opener at 0x9ccb8. All forty per-slot memory writes and guard bytes are covered synthetically; the SHA-pinned original startup confirms the path. The external archive opener and rest of the 4,119-byte Views.c parser remain unimplemented. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Advanced the bounded Views opener 0x9ccb8 across the read-only plain-file branch: missing files now reach original diagnostic callback 0xea50c; synthetic plain data rewinds the header and initializes the already recovered 0x9d060/0x9d114 stream, stopping at line parser 0x9d1e8. Packed JR/RA and archive entry points remain explicit hard boundaries, and the 4,119-byte sequence parser is still unrecovered. Synthetic and SHA-pinned GCC probes, no original data committed. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Recovered the 0x9d1e8 Views line reader in its assigned source module and connected the existing 0x9d170 character and 0x9d278 token helpers to the bounded plain-file stream. A '#' header initializes the first sequence record through 0x303f9, where the probe stops before the name/options decoder; non-headers stop at 0x31201. Synthetic tests check every initialized field, CRLF/comment/blank handling across a 256-byte refill, and overlong-line rejection; opt-in SHA-pinned startup uses invented resource text. Packed/archive branches, repeat records, first screen, guest differential and Watcom splice are still pending. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Advanced the first Views '#' record from 0x303f9 through its tokenized name and six original camera-mode comparisons at 0x30423..0x304fb. The bounded GCC probe stops at 0x304fc before the larger options decoder and at 0x30407 for oversized names, leaving unknown modes unchanged. Synthetic tests cover all labels, case folding, invalid values and a record spanning the stream refill; opt-in SHA-pinned startup also passes with invented resource text. Neither additional records nor a Watcom splice are claimed. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Recovered the bounded first Views record's Name/Key/Location line-option loop at 0x304fc..0x305e0, using original string labels and token helpers; host guards stop unchecked copies at 0x3053d/0x30577 for oversized values. The GCC probe now stops at 0x305e1 before numeric PosDamping parsing. Synthetic case-folding, quoted-string, refill and overflow tests plus SHA-pinned opt-in startup pass; further records, original guest differential and Watcom splice remain pending. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Extended the bounded first Views record through original scalar PosDamping/AngDamping/MinAlt and six RollLock values, plus three vector options OffSet/RandomOffset/InitialOffSet (0x305e1..0x30aae). Ordinary finite decimals use a narrow host-only adapter for the still-unrecovered 0x9d55d; malformed tokens stop there. Synthetic scalar, enum, vector and invalid-input tests plus SHA-pinned opt-in GCC startup pass. The next hard boundary is LookAtObj at 0x30aaf. No full numeric converter, guest differential, Watcom splice or later records claimed. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Advanced the first Views record from 0x30aaf through 0x30f58: LookAtObj's embedded SOURCE selector, LookAt/Dist scaled signed-word vectors, XClamp/YClamp enable flags and 182-scaled bounds, plus unscaled HUDCenter/HUDHeight. The GCC probe now stops at AInsts (0x30f59). Synthetic low-word/truncation/selector tests and the opt-in SHA-pinned original-image resource probe pass. The 0x9d55d decimal adapter is host-only; later options, additional records, packed resources, guest differential and Watcom splice remain pending. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Recovered the first Views record's Insts layout and Cockpit selector at 0x30f59..0x310c7, including all six instrument layouts (byte +0x9b), unknown-value retention, and ON/other cockpit flag (byte +0xa0). Original pointer 0x182910 spells Insts, not the preceding AInsts substring; this corrects the previous handoff's label. The bounded GCC parser now stops at END/unknown dispatch 0x310c8 before end-of-record validation and derived geometry. Tests exercise original label offsets, all enum states, case-folding, unknowns and SHA-pinned source image with synthetic resource text. Remaining record finalization, subsequent records, original numeric converter, guest differential and Watcom splice are unimplemented. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Recovered Views END dispatch at 0x310c8 and validation of InitialOffSet coordinate count (0 or 3), with distinct bounded stops for unknown options at 0x310d8, invalid counts at 0x31116, and mode-two geometry at 0x3119b. Non-geometry records now increment the slot index and resume scanning successive '#' records while skipping nonmarkers, stopping at EOF cleanup 0x3120b. Synthetic two-record tests and SHA-pinned source-image GCC probe pass; no geometry computation, file-close cleanup, guest differential or Watcom splice claimed. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Recovered Views camera-mode-two END geometry at 0x31147..0x311f9: signed LookAt word expansion, OffSet dword vectors, two angle words (+0x83/+0x85) and low-word 3D distance (+0x87), using the already recovered math functions. Corrected native-stack-pointer aliasing in 0xdc8d8 and 0xdc91c and connected 0xb3b1c directly to the recovered 0xb3ae0 lookup. The synthetic GCC probe now also executes the stream release 0x9d088 and unflagged resource close 0x9ce84, stopping at the original return 0x3122a with the game file slot cleared. Axis-aligned, 16-bit truncation and large-offset cases plus SHA-pinned original-image GCC startup pass; guest-state differential, exceptional numeric input, packed resources, native outer-caller continuation and Watcom splice remain pending. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Continued after the 0x3122a ReadGenSeqs return through apply_options_82cec into the next 720-byte Views reader at 0x2ff44. It initializes twenty 0x9c-stride list records, opens the separate \\views\\seq2.dat, copies named fields, resolves camera names against the forty generic 0xa1-stride records, records ON flags plus 50/32-scaled numeric fields, handles END sentinels and EOF cleanup, then reaches the following unbound 0xf80 call. Synthetic name/index/scale/default-state tests and opt-in original-image GCC probes pass; packed resources, exceptional input, guest-state differential, outer TACTCOM continuation and Watcom splice remain pending. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Entered Aid.c ReadAidData at 0xf80, initialized five sets of guest tables and parsed 27+8 integer mappings from independent \\views\\aid2.dat. Synthetic and SHA-pinned original-image GCC startup reach the nested association boundary 0x1202; no guest differential, general file support or hybrid claim. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Recovered one nested Aid.c association object through 0x15cb, with original enum labels, float scales, signed truncation, object name and group-1 grid reference. Synthetic first-record variants pass and an original-image mapping-only probe stops at the next heading guard 0x120e; subsequent records and both grid loops remain pending. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Recovered full normal plain-text Aid.c association loops and stream/file cleanup, covering seven ordinary grids, two special-name grids, global 50-record index, and original 0x1ad8 selection reset. Synthetic full-grid and original-image integration probes advance beyond 0xf80 to the next TACTCOM boundary 0xcd4d4; malformed paths are bounded and packed resources, guest differential and hybrid splice remain pending. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Connected TACTCOM 0x8b477 to recovered input init 0xcd4d4: virtual keyboard LED port acknowledgements, real C key-table reset and selector write, stopping before the missing pre-main X-32 0x628 request pool. The already-initialized input branch instead reaches 0xce08c; original source disassembly and synthetic/EXE-backed startup tests cover both. No IRQ registration, full DOS runtime, Watcom splice, differential or screen output is claimed. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Recovered the 292-byte pre-main X-32 initializer at 0xe20, with the original sixteen request free-list entries, eight timer entries, nine vector requests and environment-controlled trap setup. Added a second isolated composition test linking the recovered 0xe20, 0xb04, 0x628, 0xcd4d4, 0x734 and 0x6b8 C bodies. With only external hardware/CRT calls modeled, it exercises 11 request registrations, two teardown hooks, one timer registration, and request release/reuse, checking packed guest-RAM records and warm initialization. Removed undefined C alignment from the 10- and 14-byte pool accesses; the composition passes UBSan. This is not a native CRT integration or guest differential; the native cold path still stops at 0x628 pending faithful extender services. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Advanced the warm TACTCOM 0x8b47c..0x8b4c4 prefix: the recovered 0xce08c global read, two conditional page hooks 0x7feb0/0x81110, and optional device hooks 0x982a0/0x97f90/0x98000 are now selected in their original order. An isolated C test checks eight flag/device combinations; synthetic and SHA-pinned original-image GCC startup reach the first active unsupported hook or the original 0xe28e8 timer-service boundary with flags clear. The native cold path still stops at extender request 0x628. No handlers or timers have been simulated as no-ops; caller continuation after 0x8b4c4, guest differential and OpenWatcom splice are still missing. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Reconstructed the rest of TACTCOM 0x8b4c4..0x8b52d from original disassembly. The timer 0xe28e8 initialized guard and mode 0/>3 no-probe case in 0xe9ac0 allow warm startup to proceed without hardware calls; mode 1..3 stop at 0xd9b30/0xd9c30/0x10d9e0. The original jump table maps mode 1/2/3 to 0x30/0x46/0x47, which aliases the high word passed into real recovered selector 0x1d3c4 and scaler 0x1c60c. Using the preexisting disconnected-gameport model, a mode-zero warm GCC probe reaches TACTCOM's actual return at 0x8b52d. Source-linked host mode tests and synthetic/original-image startup probes cover this path. The original cold startup still stops before X-32 request 0x628; first-time timer setup, connected hardware, guest differential, Watcom splice and main's following calls remain unimplemented. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Added an opt-in GCC cold X-32 startup probe using original source-C 0xe20, 0xb04, and 0x628 rather than a preseeded request count. The probe models only selector values and six-byte far-stub allocation, verifies the original 16/8 free-list initializations and packed first request at slot 15, and halts at 0x9c220, the first real INT 21h AX=252Bh vector-registration boundary. The 0x628/0x6b8/0x734 recovered pool source was isolated for native linking; the ordinary main probe retains its guarded 0x628 stop, with no cross-mode leakage. Synthetic GCC and opt-in SHA-pinned-original startup tests pass. No actual DOS/X-32 vectors, timer IRQs, Watcom startup or guest differential is claimed. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Added a second, explicitly virtual GCC cold X-32 registration mode after the strict 0x9c220 boundary. The host records all nine 0x252B requests in an inert table, checking sequence, selector, length, free-list count and packed memory slot on each recovered 0x628 call. With no DOS interrupt or IRQ installed, the recovered 0xe20 reaches its original getenv branch and stops before 0x9c8b4 would install eight trap handlers through INT 21h AX=2533h. The strict 0x9c220 and ordinary TACTCOM 0x628 boundaries remain unchanged; original-image and synthetic GCC tests exercise the new mode. Live X-32 services, trap context, timers, guest differential and Watcom splice remain pending. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Inspected original 0x9c8b4 through 0x9c908: the trap installer saves callback/entry ESP/SS/DS then uses INT 21h AX=2533h on exactly eight vectors (3, 6, 9..14), with handler offsets 0x9c908/0x9c909. New opt-in --probe-x32-traps executes recovered source-C for 0xe20 and 0x9c8b4, checks the saved synthetic context in guest RAM, and collects eight original-order trap descriptions in a host-only registry before stopping at the original 0xf43 return. The earlier strict and vector-only probes retain their stops; no real DOS calls, interrupts, actual CPU register capture, complete CRT or guest differential are implemented. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Opt-in --probe-x32-input now continues recovered source-C from 0xe20 through cold keyboard setup 0xcd4d4, including 0xcd414/0xcd3c0 LED acknowledgement, 0xcd430 key table reset, 0xfe610 synthetic DS capture, two more 0x628 request allocations and the first 0x734 timer slot write. It checks 11 total packed request records, eight inert trap descriptions and slot 7's timer source 9, stopping before the real 0x324 INT 21h AX=2503h timer read. No timer is installed or DOS return code assumed; original-image and synthetic GCC tests verify the boundary, leaving the main probe and earlier opt-in stops unchanged. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  New separately opt-in --probe-x32-timer completes recovered source-C 0xcd4d4 through the 0xcd550 return. Original disassembly confirms timer calls 0x324/0x348/0x484 use INT 21h AX=2503h/2502h/2506h. The test-only host supplies explicit synthetic values 0x2468 and 0xabcd:12345678, validates timer slot 7, and records exactly one inert callback request; original C then writes the timer handle, registers cleanup 0xcd554 and sets the input flag. Prior strict probe stops remain. No DOS timer installed, extender response verified, guest differential or OpenWatcom splice; see docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Added --probe-x32-tactcom, which composes the original recovered cold X-32 and keyboard/timer source through the TACTCOM 0x8b47c post-input C body. The original zero page/device flags cause the subsequent first-time 0xe28e8 timer/sound service to remain a strict unimplemented boundary (the warm timer guard is not preseeded). Opt-in original-image and synthetic GCC tests verify the stop, cold input flag, second cleanup callback and staged timer handle; prior strict probes remain. This does not cover TACTCOM's preceding file/options phase, the real extender, live timers or guest differential; see docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Extended the separately opt-in cold TACTCOM path with --probe-x32-tactcom-timer: original recovered 0xe28e8 sets its one-time guard, enters recovered 0xd6dc4 and stops strictly at the external sound service 0x101c54 before module-ready is marked. Synthetic guest-memory and SHA-pinned original-image GCC checks pass; prior stop sites are unchanged. Sound callbacks/memory locks remain hard failures, with no live audio, DOS timer, guest differential or Watcom splice. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Added the no-AIL_DEBUG original branch of AIL trace init 0x101c54 in source-C. A further opt-in --probe-x32-tactcom-ail composes the cold TACTCOM path through recovered 0x101a90 and 0x103d80 lock initialization and enters reset_settings_104580. Sixteen precisely checked inert DOS memory-lock requests precede nineteen default-setting exchanges via previously verified 0x1041e4, followed by clearing the additional AIL globals. The strict stop is now interrupt-sensitive 0x110fd4; trace state and both lock-init flags match, while the sound module remains uninitialized. Synthetic and SHA-pinned GCC tests pass; debug logfile path, real memory locking, live audio/IRQ, guest differential and Watcom splice remain unsupported. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Recovered the source-C prefix of AIL IRQ initialization 0x110fd4 through the first DOS INT 21h AX=2502h at 0x11105b. A new strictly opt-in --probe-x32-tactcom-ail-irq records a virtual-only interrupt-disabled entry and the original two further memory-lock range requests (18 total), saves the synthetic data selector and proves four metadata dwords and four 16-dword state tables are overwritten as in the binary. Earlier cold AIL mode still stops at 0x110fd4. No real IRQ, PIT, vector query or audio setup is performed; full function, guest differential and native binary equivalence remain pending. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Extended 0x110fd4 source-C through the X-32 IRQ8 protected/real-mode vector capture and inert installation request: synthetic AX=2502h/2503h return values populate the original ES:EBX/EBX save slots 0x17d7a4/0x17d7a8/0x17d7aa, and the guarded AX=2506h request references handler 0x110de6 before setting 0x17d6a0 to 2. The new --probe-x32-tactcom-ail-vectors stops at the 0x1110ab timer callback registration; the old 0x110fd4/0x11105b stops persist. Synthetic host memory checks and SHA-pinned original-executable loading pass, but the adapter does not call DOS or install an IRQ/PIT timer. Remaining timer callback, flag restoration, original guest differential and Watcom splice are pending. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Completed the source-C cold AIL IRQ initializer through return 0x1110c4 in new strict --probe-x32-tactcom-ail-callback mode. The original callback period d68d in slot 15 passes through existing recovered C 0x1021f4/0x1114c7/0x110f6f/0x110f3c/0x110f0c, exercising two nested lock/unlock pairs and producing three inert PIT port requests (0x43:0x36; 0x40:0; 0x40:0). Cached period and callback table entry become d68d, divisor becomes zero, and virtual IRQ flags are restored without touching Linux interrupts. Original caller sets module state and stops at next unsupported sound lock 0xd6df0. All earlier probe boundaries persist. Synthetic and SHA-pinned tests pass, but there is no real IRQ, timer, audio, guest x86 differential or Watcom native splice. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Followed the next original source-C cold AIL sound-module step from 0xd6df0 through recovered 0xd6df0 and 0xe28e8 memory-lock wrappers: four precisely checked inert code/data locks, recovered 0x9c3cf cleanup registration of 0xd6da0 into guest handler stack 0x40d030 (count 1). New strict --probe-x32-tactcom-ail-module preserves all earlier opt-in stop sites and stops at 0x111365 before timer slot allocation through wrapper 0x102144. Synthetic/Pinned-original GCC tests compare lock count, cleanup record, and unchanged timer handle; no host page locks, IRQ/PIT, audio driver, native splice or original guest differential is implemented. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Extended the opt-in cold AIL path through existing recovered source-C claim_table_slot_111365 (first free slot offset 0), with the original nested software critical-depth increment/decrement allowed only after virtual IRQ restoration in new --probe-x32-tactcom-ail-slot mode. The callback table records state 1 at 0x17d664 and handler 0xe28c4 at 0x17d624, and the caller stores handle 0 at 0x15f74c. The old IRQ slot 15 and its d68d period, four inert locks, cleanup stack and all three inert PIT writes persist. Strict stop moves to 0x1114f7 before timer interval programming; older modes retain their earlier stops. Pinned-original GCC and synthetic RAM tests pass; no native timer, DOS IRQ/PIT IO, Watcom splice or guest x86 differential has run. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Continued the recovered source-C timer rate chain at 0x1114f7 (nested software critical sections, 0x1021f4 -> 0x1114c7 -> 0x110f6f -> 0x110f3c -> 0x110f0c) in separate --probe-x32-tactcom-ail-rate mode. For slot offset 0 and rate 0x32, the stored period is 20000, replacing the previous minimum d68d; exact 32-bit scaler gives PIT reload 0x5d3a. The three further inert port writes are (0x43,0x36), (0x40,0x3a), (0x40,0x5d), for six total; the original caller writes callback e28e0 to 0x15b620 and stops at 0x11141f before starting the slot. Older strict probe boundaries remain. GCC synthetic and SHA-pinned original tests pass; there is no live timer/audio, Watcom splice, or guest instruction differential. See docs/subsystems/decomp-log.md.
2026-11-09  native-startup  Added separate --probe-x32-tactcom-ail-start continuation through the already recovered source-C table_slot_1_to_2_11141f, promoting callback slot 0 from state 1 to 2 under the software critical-section bracket. The containing first TACTCOM timer-service initializer returns, and a strict stop at 0x8b4c9 precedes the following hardware dispatch. Cold synthetic and pinned-original checks cover the state 2 transition, preserved callback, rate/reload, module handler, zero critical depth and six inert PIT requests; old probe stops stay available. No timer ticks, hardware IRQ/OUT, guest instruction differential or Watcom splice.
2026-11-09  native-startup  Added opt-in --probe-x32-tactcom-ail-finish continuation after the cold timer service returns: the bounded no-device mode-zero arm of 0xe9ac0, original TACTCOM jump-table mapping, source-C select_tables_1d3c4 and scale_words_1c60c complete to the true 0x8b52d return. Strict synthetic and pinned-original probes check unchanged running slot 0 and slot 15, six inert PIT requests, 20000/0x5d3a timing, cleared mode-zero selectors and disconnected-gameport state; earlier stops remain. Hardware modes 1..3 still stop at 0xd9b30/0xd9c30/0x10d9e0, and source-C cold tail omits earlier file/options setup. No live IRQ/audio, full main or guest x86 differential/Watcom splice.
2026-11-09  native-startup  Added separately opted-in synthetic-warm --probe-main-exit after full read-only TACTCOM file/options processing and a preinitialized no-device timer/joystick path: real recovered candidate_game_main now calls source-C install_exit_handler_ea470(1) after TACTCOM returns, and stops before its first unbound channel-mode call 0x10dc7f. Sentinels protect original channel-2 slot and mode-change global; invalid hardware mode cannot pass. Existing --probe-main and independent cold X-32 paths retain their earlier boundaries. No channel switch, exit registration, following clump file load, Watcom splice, or guest x86 differential.
2026-11-09  native-startup  Added opt-in --probe-main-channel continuation from the same synthetic warmed game main through existing recovered source-C set_channel_mode_10dc7f(2,0x10dc40). The function writes 0x10dc41 to mode-change callback/global 0x17e5e0, then reaches channel-2 runtime-hook tail 0x11d24e; strict stop before the hook ensures the 0x17d3d4 channel slot sentinel remains unchanged. Hardware mode 1 fails before the continuation and the previous --probe-main-exit still stops before 0x10dc7f effects. No actual channel switch, runtime hook, exit-handler installation, guest x86 differential, or Watcom splice.
2026-11-09  native-build  Corrected native flake startup link by selecting the 50 recovered C modules used by the passing GCC standalone via one shared runtime/decomp_host_sources.txt manifest; no longer pulls the entire archive with conflicting guest_callptr1/guest_io Watcom/DOS thunks. Preserved full GCC libef2000-decomp.a for inspection. Nix startup C integration test now includes game_helpers75 for recovered channel service. Verified locally with separately compiled selected GCC objects linked to host adapter sources, plus --self-test, standalone and asset-free Python suites. Nix command unavailable in this workspace; 32-bit pointer-cast and Watcom-only scene-state warnings in the unrelated archive do not imply host-call safety.
```

---

## 10. Risks and how to react

| Risk | Signal | Reaction |
| --- | --- | --- |
| Golden replays are nondeterministic | Two unpatched runs differ | Stop decompilation; fix determinism in the harness first (time sources, wall-clock pacing, uninitialized host memory) |
| OpenWatcom can't express an original calling convention | Signature needs registers/flags OW can't model with `#pragma aux` | Write that function in `wasm` assembly (`kind=asm`) and move on |
| Function boundaries wrong | Splice breaks unrelated code; replay diverges | Re-run `audit-flow-gaps`/`audit-body-holes`; fix boundary in tracker + Ghidra; replace shared-code clusters as a unit |
| Self-modifying or runtime-patched code | Original bytes change at runtime (the harness already invalidates changed-code caches in the real-mode CPU) | Detect via write-watch on code pages during replays; list such functions; recover their generator logic |
| x87 differences | FP results differ in the last bits | Check precision control word set by the game; use `-fp5`/`-fpi87` correctly; in build C use `-mfpmath=387` and the same control word |
| Library quirk dependence | Differential test diverges inside clib calls | Reproduce quirk in `decomp/compat/`, do not "fix" |
| Patch arena collision with heap | Random crashes late in replays | Verify arena range against max heap usage; add a guard page |
| Scope creep in tooling | Days spent on tools without verified functions | Each Phase-1 step has an exit test; after Phase 1, tooling work must be justified by a blocked function |
| Legal | Recovered code redistributed | Owner decides; agent never publishes `decomp/` anywhere |

---

## 11. Quick reference for a work session

Session start checklist:

1. `git status` is clean and `git log -3` matches what you expect.
2. Read §1 and §4 of this file, plus `AGENTS.md`.
3. Confirm game data is present: `../EF2000/c/EF2000/_EF2000.EXE` with the
   SHA-256 in `docs/executable.md`. In web chat sessions the data arrives as
   a `.zip`: extract it outside the repository (or under `local/`), verify
   the SHA-256 and pass its path via `EF2000_EXE` / `--data-dir`. Without the
   data only asset-free work is possible; say so instead of guessing.
   Web chat sessions deliver their commits as a Git bundle (`TODO.md`).
4. Confirm `local/ghidra` exists; if not, run step 3.0 first.
5. Find the first unchecked step in §3 (or the current wave in §6) and
   continue there. Do not start a later phase early.

### Regenerating `local/`

`local/` is git-ignored and never delivered. Nothing in the repository
needs an existing `local/`; every input that matters is regenerated:

| What | Needed for | How to recreate |
| --- | --- | --- |
| `local/ghidra` (project, pseudo-C, inventory, facts) | All decompilation work | `python3 tools/ghidra_ef2000.py rebuild --analyze-headless $GHIDRA_HOME EXE` (Nix: `ef2000-ghidra rebuild EXE`); ~4 min on a desktop |
| GUI skin/font/symbol extracts | Optional SDL3 overlay skin | `python3 tools/ef2000_gui_assets.py skin DATA_DIR --out local/gui-skin` |
| Golden replay hashes (§3.9) | Splice validation | `tools/replay_suite.py record` once written |
| Run directories (`startup-*`, `cd-*`, `*-reference-*`, …) | Nothing; historical captures | Not needed. Findings are already in `docs/` and `docs/*-reference.json` |

Web chat sessions (no Nix): the Ghidra tools are plain Python 3 standard
library plus a Ghidra installation. Point `--analyze-headless` (or
`GHIDRA_HOME`) at it. **Expected function counts depend on the Ghidra
release**: 4,233 with 11.4.2, 4,277 with 12.1.4 (`docs/ghidra-findings.md`
#062). Report the release used in every commit that cites counts. A project
created by a newer Ghidra cannot be opened by an older one, so do not reuse
a `local/ghidra` copied from another machine unless the versions match.
Steps that need Nix-only tools (OpenWatcom wrapper, native64 harness with
`modify_ldt`, Xvfb) may be unavailable in a sandbox; say so in the session
report and pick asset-free or Python-only work instead of faking results.

```sh
nix develop                                      # tools: Ghidra, OpenWatcom, Unicorn, DOSBox-X
python3 tools/decomp_status.py summary           # where are we
python3 tools/decomp_status.py next --module M   # what next
ef2000-ghidra apply-symbols && ef2000-ghidra export-all --overwrite
ef2000-wcc decomp/src/<module>.c                 # compile
python3 tools/asm_diff.py <addr> decomp/build/<module>.obj
python3 tools/func_differential.py <addr>        # per-function oracle
python3 tools/omf_splice.py decomp/build/*.obj -o local/patches/current.efpatch
python3 tools/replay_suite.py verify --patch local/patches/current.efpatch
nix flake check                                  # asset-free checks before commit
```

Before committing: tracker updated, tests listed in the commit body, no
files from `local/`, no pseudo-C, no game assets, attribution trailer.

### 2026-11-13 assembly-focused continuation

Current user direction prioritizes matching generated assembly over game runs, superseding the earlier instruction in section 2.2 to avoid instruction matching. Full C coverage remains primary; pure-C menu and cockpit are secondary milestones. Corrected the EAX/AH contract of campaign callback 0x40ec4; compilation and static assembly review recorded in docs/subsystems/decomp-log.md. This entry remains unspliced and is not counted as verified.

2026-11-13 continuation: campaign flag callbacks 0x40ebc/0x40ec4 now independently arena-link and pass five isolated differential cases each; source-only menu remains incomplete.

2026-11-13 startup audit: corrected unconditional x87 helper dispatch in 0x120485; stubbed-callee differential passes all three hook-state cases. The native source-only startup boundary at 0x11d24e remains; neither this isolated check nor hybrid linkage establishes a source-only menu.

2026-11-13 resource-loader continuation: drafted original startup clump loader 0xea270 (378 bytes), with exact descriptor/index layouts and missing-file differential evidence. Populated paths remain unverified; source-only main menu remains incomplete.

2026-11-13 populated resource validation: clump loader 0xea270 passes ten isolated synthetic-service comparisons, including both index-building loops and diagnostic dispatch. Marked compiles; real loading and pure-C main menu remain incomplete.

2026-11-13 main-loop continuation: recovered 0xaaee8 main control loop; seven isolated synthetic-service differentials pass, including consumed environment argument and repeated GUI dispatch. GUI dispatcher 0xa9e70, real services and native startup boundaries remain unresolved; pure-C main menu is not achieved.

2026-11-13 dispatcher continuation: hand-recovered 0xa9e70 passes 21 isolated synthetic-service cases, including every opcode and three stream transitions. Retain broad exclusion until mode/exit tails and real GUI integration are checked. Source-only main menu remains incomplete.

2026-11-13 composed menu-control validation: dispatcher now passes 27 synthetic-service cases including mode/exit tails; combined recovered main loop plus dispatcher passes one original-vs-C comparison. Real menu services and source-only startup remain incomplete.

2026-11-13 GUI state-loader continuation: corrected 0xaad7c frame layout and isolated source module; six synthetic-service differentials pass. Actual GUI parser integration and source-only startup remain incomplete.

2026-11-13 GUI binary-parser continuation: recovered 0xaa5c4 with 22 passing synthetic-service comparisons, including all payload opcodes and optional nested records. Real resource I/O, list-link integration and source-only startup/menu remain incomplete.

2026-11-13 parser/link integration: recovered parser now compares successfully with all three real C append helpers, including multi-list/group/child input. Synthetic read/allocation remain; source-only startup and rendered menu are incomplete.

2026-11-13 GUI arena integration: corrected and recovered allocator 0xa7b98 (old-cursor EAX return), five isolated cases pass; composed parser, allocator and three linking helpers pass four cases. Resource reads, byte fill, startup and source-only main menu remain incomplete.

2026-11-13 resource-read integration: 0x9cde8 isolated and ABI corrected; nine routing/padding comparisons and four composed parser/read cases pass. Real archive/handle I/O, copy/fill integration and source-only main menu remain incomplete.

2026-11-13 copy integration: valid flat segment descriptors permit unchanged original ES saves/restores; 27 copy cases and four parser/read/arena/list/copy cases pass. Synthetic fill and real archive/handle I/O remain, as do source-only startup and menu integration.

2026-11-13 memory-parser composition: self-contained C fill removes the final service stub from the eight-function RAM-backed parser fixture; four comparisons pass. This is synthetic input and prepared state, not actual menu resources or cold startup. Pure-C working main menu remains incomplete.

2026-11-13 actual GUI-resource evidence: eight-function recovered parser chain matches original for gui.par from DID.DAT and DID1ST.DAT, with no service stubs and prepared RAM-backed input. Reproducer tools/menu_resource_differential.py stores all proprietary artifacts in ignored local/. Archive I/O, fixups, startup and working main menu remain incomplete.

2026-11-13 pointer-fixup continuation: recovered 0xaa4b8/0xaa4d4, passing eight synthetic comparisons and both real gui.par parse-plus-fixup comparisons. Remaining post-load passes and source-only startup/menu are incomplete.

2026-11-13 scene-resolution continuation: recovered 0xaa350, six targeted synthetic-service comparisons pass, and two real-resource sequential parse/fixup/scene checks pass. Resolver helper replacements, names/resources and source-only startup/menu remain incomplete.

2026-11-13 scene-helper composition: recovered missing 0xaa198/0xaa1c0, eight isolated cases pass; scene pass with four real C helpers passes three composed cases. Post-load resource/name setup and source-only startup/menu remain incomplete.

2026-11-13 list-name pass: isolated/audited 0xaa3dc, ten targeted comparisons pass, and native build/bootstrap checks pass. Heap/string-copy composition, actual post-load setup and source-only startup/menu remain incomplete.

2026-11-13 name/copy integration: isolated pair-loop strcpy 0x9d749 passes 18 comparisons; composed name/copy passes ten cases. General heap 0xfc795 and actual post-load/startup/menu integration remain incomplete.

2026-11-13 general-heap continuation: recovered outer allocator 0xfc795 with documented DS-read ASM adapter; nine instrumented-helper comparisons pass. Per-heap allocation, extender growth, native selector binding and source-only startup/menu remain incomplete.

2026-11-13 per-heap allocation: 0x110410 isolated/audited, eleven flat-selector comparisons pass; outer/block composition passes nine cases. Real extender growth, heap lifecycle, native selector binding and source-only startup/menu remain incomplete.

2026-11-13 full post-load comparison: twenty recovered C entries match original parser/name/fixup/scene sequence on both real gui.par resources with prepared heap/RAM. Ten name/real-heap cases pass; 0x1105c3 remains original on failure due to three-byte entry extent. Cold startup, real archive I/O and working main menu remain incomplete.

2026-11-13 heap-initialization continuation: isolated descriptor insertion 0x10d5c7 passes five original comparisons. Growth 0x10d80b still requires DOS/selector-limit bindings and initial-block release; source-only startup/menu remains incomplete.

2026-11-13 heap lifecycle: corrected DS handling in release, six inner and five composed release cases pass. Descriptor initialization -> initial-block release -> allocation passes three no-stub cases from provisioned raw RAM. Extender provisioning, native selector bindings and working main menu remain incomplete.

2026-11-13 growth sizing: recovered 0x10d63b with documented selector-limit adapter (eight comparisons pass); normalizer 0x10d94f with corrected preserved registers passes 25 composed cases. Growth entry, native selector bindings and working main menu remain incomplete.
2026-11-13  asm-pipeline  [x] Added reproducible per-entry linear ASM inventory (tools/asm_inventory.py; docs/asm-pipeline.md), with original address/bytes, direct targets, payload/tracker digests, decoder provenance, overlap and undecodable-instruction records. [~] Exact function-body recovery and relocation-aware instruction equivalence remain pending; inventory does not promote recovery states. Full first export: 4,290 entries under ignored local/; five focused tool tests pass. Existing heap-growth draft remains under validation.
2026-11-13  asm-comparison  Fixed asm_diff.py to use supported wdis -e/-p options instead of rejected -r. Actual OpenWatcom listing comparison now succeeds for heap growth; five focused tests pass. This is a diagnostic listing, not an equivalence verdict.
2026-11-13  heap-growth  Recovered 0x10d80b control flow in heap_grow.c; OpenWatcom clean, 359-byte C body versus original 324. Fifteen isolated original-vs-C cases and four cases composed with C size/tail/insertion/release helpers PASS. DS/LSL/+1 is a documented ASM exception; DOS break provision and alternate growth remain external, native provider unbound, entry stays drafted. No game runs or source-only menu claim.

2026-11-13 script-runner follow-up: completed review of unfinished 0x1093d8 attempt. OpenWatcom emits zero warnings/errors. Original ASM pushes/pops ECX but changes EBX/EDX; caller 0xe40cf consumes neither, so corrected their erroneous preserved-register contract. Replaced the invalid draft probe with five synthetic cases; callback records the first line byte instead of its stack address. Empty/comment-only pass; three selector cases fail because original selector stores the temporary line address in the caller table sentinel (0x1e0108), exposing different stack layout. Retained volatile cursor: removing it exceeds the 180000-instruction budget in copy_line; retain blocked state and normal hybrid exclusion. No golden replay or working-menu claim. Commands: tools/ef2000_wcc.py decomp/src/script_helpers1.c; tools/hybrid_link.py --image local/startup-nosound-02/payload.bin --hand-objects local/script-finish-objects --exclude-list /dev/null --only-address 0x1093d8 -o local/script-finish-final.patch; tools/func_differential.py 0x1093d8 --payload local/startup-nosound-02/payload.bin --patch local/script-finish-final.patch --spec decomp/tests/0x001093d8.json.

2026-11-13 ASM control-flow inventory: schema 2 records direct/indirect call and
jump edges, fallthrough, undecodable instructions and system-transition review
items without promoting recovery states. Full export succeeds for 4,290 entries;
heap growth 0x10d80b has five review edges and normalizer 0x10d94f has one.
Seven focused tests pass (`python3 -m unittest tests.test_asm_inventory
tests.test_asm_diff`). Reproducer: `python3 tools/asm_inventory.py --output
local/asm-flow-inventory-20261113`. Exact body-range import, indirect-edge
resolution and instruction equivalence remain pending; linear-window reports
are not evidence of complete functions or source-only execution.

2026-11-13 ASM body-range import: Ghidra exports actual half-open body spans and
instruction locations/lengths bound to initialized-memory SHA-256. Inventory
schema 3 validates provenance and decodes each span separately; missing bodies
fail closed. Ghidra 12.1.4 full inventory: 4,277 bodies (40 multi-span), no
ownership overlaps, 4,275 matching decoder layouts; 0xfa3d4/0x11f6d8 disagree.
Thirteen tracker-only entries remain blocked and have no imported body.
Commands: `python3 -m unittest tests.test_asm_inventory tests.test_asm_diff
tests.test_ghidra_ef2000` (56 pass); `python3 tools/ghidra_ef2000.py inventory
--output local/ghidra/body-layout-20261113`; `python3 tools/asm_inventory.py
--tracker local/ghidra/body-tracker-20261113.tsv --body-ranges
local/ghidra/body-layout-20261113/body-ranges.json --output
local/asm-body-inventory-20261113` (4,277 exported). Translation, ABI and semantic
equivalence remain separate gates; reliable bulk C translation is not yet achieved.

2026-11-13 conservative ASM-to-C translation: added tools/asm_to_c.py and
asm_c_differential.py. Explicit-state C preserves partial GPRs, flags, flat RAM
and control flow for a bounded instruction subset; unsupported bodies fail
closed. Re-decodes inventory bytes, binds generated source/runtime digests and
keeps all derived output under ignored local/. Full 4,277-body pass generates
404 drafts and reports 3,873 unsupported; OpenWatcom v2 -we and host compiler
accept all 404. Sixteen probes each: 4,585 matches, 1,879 inconclusive, zero
mismatches; 211 bodies pass all probes, 193 remain inconclusive. Comparison checks
all GPRs, EFLAGS, PC and all 4 MiB RAM with original x86, without service stubs.
Original faults/budget exits do not pass. Focused suite: 64 tests, including
131,072 exhaustive byte ADD/SUB comparisons and 1,568 synthetic program probes.
Commands: tools/asm_to_c.py --inventory local/asm-body-inventory-20261113 --output
local/asm-c-checked-20261113; tools/asm_c_differential.py --translations
local/asm-c-checked-20261113 --output local/asm-c-diff-checked-20261113 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386;
python3 -m unittest tests.test_asm_to_c tests.test_asm_inventory tests.test_asm_diff
tests.test_ghidra_ef2000 (Python 3.13.12 / Unicorn 2.1.4). Existing Ghidra-to-C
baseline: 357 unrecovered compile-ok, 499 manual (no semantic claim).
Reliable bulk coverage remains active: calls, shifts, multiplication, unresolved
branches, ambiguous widths, x87, native ABI/integration and stronger state
coverage remain. Generated explicit-state functions are not hybrid replacements
and do not promote tracker states or establish a source-only menu.

2026-11-13 ASM integer expansion: SHL/SHR/SAR, all MUL/IMUL forms, ADC/SBB,
CBW/CWDE/CWD/CDQ, zero-nesting ENTER and encoded-register memory-width inference.
Unsigned arithmetic and bounded shift steps avoid C overflow/shift UB; undefined
flags retain the Unicorn oracle policy and remain checked. Tightened differential
classification: any C failure after the original completes is a mismatch.
Ten oracle/tool tests pass, including exhaustive byte arithmetic/carry, shift
count and multiplication grids, wider boundaries and UBSan shift/multiply grids;
56 inventory/Ghidra checks pass. Rechecked the stricter C-failure regression.
Full Ghidra 12.1.4 body corpus: 662 generated (49,199 body bytes), 3,615 unsupported;
662 OpenWatcom v2 -we objects; 10,592 probes: 7,302 passes, 3,290 original-fault/
budget inconclusive, zero mismatches. 315 bodies pass all 16 probes; 347 remain
inconclusive. No ABI/tracker promotion. Commands: python3 -m unittest
tests.test_asm_to_c; python3 -m unittest tests.test_asm_inventory
tests.test_asm_diff tests.test_ghidra_ef2000; tools/asm_to_c.py --inventory
local/asm-body-inventory-20261113 --output local/asm-c-integer-20261113;
tools/asm_c_differential.py --translations local/asm-c-integer-20261113 --output
local/asm-c-integer-diff-20261113 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386
(Python 3.13.12/Unicorn 2.1.4 for oracle commands). Direct calls remain the main
blocker; a read-only graph check finds 2,549 syntactically translatable direct-
call bodies and 958 closed over their direct dependencies, before implementation.
Reliable bulk coverage, indirect control flow, x87 and native integration remain
incomplete.

2026-11-13 direct-call ASM composition: tools/asm_c_program.py reuses validated
inventory decoding, selects the greatest dependency-closed set of supported
bodies, and emits a PC dispatcher with one shared guest instruction budget.
Direct CALL/RET use original guest stack addresses/return PCs; tail jumps and
owned interior targets are supported without host recursion or ABI inference.
Unsupported callees exclude transitive callers; indirect calls and unknown
continuations remain unsupported. Generated source/dispatcher/runtime hashes
are checked by the differential runner. Ghidra 12.1.4 corpus: 2,549 syntax-
supported bodies; 958 selected, 81,976 body bytes, 296 cross-body-dependency
bodies; 1,591 syntax-supported bodies remain excluded by closure. All 958 body
objects plus dispatcher compile with OpenWatcom v2 -we. 15,328 original-vs-C
probes: 10,257 passes, 5,071 original-fault/budget inconclusive, zero mismatches.
432 bodies pass all 16 probes, including 117 with cross-body dependencies;
526 remain inconclusive. No original-code fallback, native ABI recovery,
tracker promotion or game-integration claim. Seventy focused tests pass,
including nested frames, recursion, mutual recursion, tail jumps, guest-stack
observability, dependency exclusion and shared-budget resumption; four program
tests rechecked after dispatcher-tamper regression was added. Commands:
python3 -m unittest tests.test_asm_c_program tests.test_asm_to_c
tests.test_asm_inventory tests.test_asm_diff tests.test_ghidra_ef2000;
tools/asm_c_program.py --inventory local/asm-body-inventory-20261113 --output
local/asm-c-program-20261113; tools/asm_c_differential.py --translations
local/asm-c-program-20261113 --output local/asm-c-program-diff-20261113 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386
(Python 3.13.12/Unicorn 2.1.4). Dependency-frontier audit points to segment/string
helpers (0xed133: 283 dependent bodies), rotations (0xdeb50: 216), heap-selector
operations (0xfc795: 181), indirect calls and DOS/runtime boundaries. Full
reliable bulk coverage, x87, remaining ownership/boundary cases and native
integration remain active work.

2026-11-13 rotate/divide and oracle isolation: added ROL/ROR/RCL/RCR, DIV/IDIV,
SETcc, BSWAP and XCHG with register-dependent memory addresses preserved.
Division checks zero, quotient bounds and INT64_MIN/-1 before C arithmetic;
rotate flags include the measured undefined-OF policy. Byte/count/carry grids,
immediate rotations, quotient/remainder/fault boundaries and existing tests:
73 pass, with UBSan scalar grids. Stress testing reproduced a Unicorn 2.1.4
host SIGFPE for IDIV ECX at EDX:EAX=0x80000000:0, ECX=0xffffffff. That original
case is isolated and explicitly inconclusive, not matching evidence; the C
exception guard is separately checked against the quotient bound. The runner
now forks per entry and reports original/C/comparison phase: original crashes
are inconclusive, C crashes after original completion are mismatches, completed
probes survive, and unexecuted probes are not counted as passes. Core dumps are
disabled in children. A fresh CPU is used after ordinary #DE scalar probes to
avoid Unicorn pending-exception state. Ghidra 12.1.4 corpus: 2,815 syntax-
supported; 1,178 closed bodies (117,186 body bytes), 439 with cross-body
dependencies; 1,637 syntax-supported remain excluded. All 1,178 bodies plus
dispatcher compile under OpenWatcom v2 -we. Isolated corpus 18,848 probes:
12,410 passes, 6,438 original-fault/budget inconclusive, zero worker failures,
zero mismatches. 523 bodies pass all probes, including 172 with cross-body
dependencies; 655 remain inconclusive. No tracker or native ABI promotion.
Commands: python3 -m unittest tests.test_asm_to_c tests.test_asm_c_program
tests.test_asm_inventory tests.test_asm_diff tests.test_ghidra_ef2000;
tools/asm_c_program.py --inventory local/asm-body-inventory-20261113 --output
local/asm-c-rotdiv-program-20261113; tools/asm_c_differential.py --translations
local/asm-c-rotdiv-program-20261113 --output local/asm-c-rotdiv-diff-20261113
--watcom /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386
(Python 3.13.12/Unicorn 2.1.4). Current dependency frontiers include segment/
formatter 0xed133 (330 dependent bodies), DOS service 0xfa95f (194), heap segment
state 0xfc795 (192), and indirect callbacks. Segment/string state, service
bindings, indirect transfers, x87, runtime enforcement of static-code assumptions,
remaining body-boundary issues and native integration remain active work before
reliable full bulk coverage can be claimed.

2026-11-13 static-code enforcement: program dispatcher compares current guest
instruction bytes against payload-bound encodings before each step; status 6
rejects changed code, including mutations with otherwise identical observable
outputs. Original bytes remain only in ignored generated artifacts. Host build
uses one fingerprinted translation unit for stateless helper sharing; Watcom
still builds separate body/dispatcher objects. Six focused program/build tests
pass. Full Ghidra 12.1.4 selected corpus unchanged at 1,178 bodies: all Watcom
objects plus dispatcher warning-free; 18,848 isolated probes reproduce 12,410
passes, 6,438 original-fault/budget inconclusive, zero mismatches; 523 bodies pass
all probes. Commands: python3 -m unittest tests.test_asm_c_program
tests.test_asm_to_c.TranslationTests.test_generated_artifact_provenance_and_bulk_verifier;
tools/asm_c_program.py --inventory local/asm-body-inventory-20261113 --output
local/asm-c-guard-program-20261113; tools/asm_c_differential.py --translations
local/asm-c-guard-program-20261113 --output local/asm-c-guard-diff-20261113 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386
(Python 3.13.12/Unicorn 2.1.4 for oracle tests). No tracker/native promotion.
Segment/string state, indirect transfers, runtime bindings, x87 and body-boundary
cases still prevent reliable full coverage.

2026-11-13 selector/string state and instruction data parameters: State ABI 1
adds ES/CS/SS/DS/FS/GS and pins a zero-base GDT profile (null data aliases 0..3,
code 8, data 16/24). The verifier checks every visible selector and rejects
incompatible generated state ABIs. Supported MOV/PUSH/POP selector operations,
LES/LDS/LFS/LGS/LSS, segment overrides, CLD/STD and byte/word/dword
MOVS/STOS/LODS/CMPS/SCAS including REP/repeat conditions preserve guest state.
REP element and terminal-check budget/resumption match measured Unicorn steps;
copy overlap is preserved element by element. Unknown bindings return status 7.
The first guarded corpus reported 15 mismatching bodies. Twelve callers reached
intentional decompressor operand patches at 0xcf7f8/0xcf806/0xcf82b; three helpers
used null selector alias 1. Added generic, encoding-proven mutable immediate
suffixes for MOV register-immediate, register ALU 80/81/83 (with signed-byte
extension), and register shift/rotate C0/C1. Generated C reads those parameters
from guest RAM while operation/register/prefix bytes remain guarded; arbitrary
opcode/addressing changes still fail status 6. No function-address exception,
original-code fallback or ignored mismatch. Complete 15-body failure closure
passes; full rerun is clean. All 79 focused tests pass, including selector state,
stack slot widths, far-pointer aliases, DF/count/overlap/repeat exits, exact
step budgets, mutable data fields and unchanged opcode guard regressions.
Full Ghidra 12.1.4 corpus: 2,942 syntax-supported, 1,358 selected closed bodies
(139,340 body bytes), 585 with cross-body dependencies; 1,584 syntax-supported
remain excluded. All 1,358 Watcom v2 -we body objects plus dispatcher compile
warning-free. 21,728 isolated probes: 13,912 matches, 7,816 original-fault/budget
inconclusive, zero worker failures and zero mismatches. 572 bodies pass all
probes, including 214 with cross-body dependencies; 786 remain inconclusive.
Commands: python3 -m unittest tests.test_asm_c_segments tests.test_asm_c_program
tests.test_asm_to_c tests.test_asm_inventory tests.test_asm_diff
tests.test_ghidra_ef2000; tools/asm_c_program.py --inventory
local/asm-body-inventory-20261113 --output local/asm-c-segstr-final-program-20261113;
tools/asm_c_differential.py --translations local/asm-c-segstr-final-program-20261113
--output local/asm-c-segstr-final-diff-20261113 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386
(Python 3.13.12/Unicorn 2.1.4). Failed iterations remain under ignored local/.
No tracker promotion or native/source-only game claim. Checked indirect calls/
jumps, nonflat selector bindings, runtime services, x87, remaining body boundaries
and stronger application-level evidence remain before full reliable coverage.


**2026-11-13 — Reviewed boundaries exposed by indirect translation.**
Checked indirect execution exposed two missing formatter callbacks at 0xa432f
and 0xe300f. Reviewed disassembly, containing caller MOV ECX literals at
0xa4352/0xe3026, neighboring function boundaries and terminating RET establish
19-byte leaves. The 0xc64d0 XLAT leaf was truncated at 20 bytes: its trailing
store and RET extend the body to 26 bytes (exclusive end 0xc64ea). The only
cleared false datum is the reviewed four-byte address operand at 0xc64e5.
Added payload-bound, transactional report/apply recovery; unknown ownership,
data conflicts, unexpected instruction classes or boundaries abort the repair.
Headless Ghidra audit, apply and repeat audit succeed; repeat reports all three
already recovered. Fresh inventory contains 4,279 functions and 943,490
instruction bytes. Tracker adds the two named callbacks and corrects only the
XLAT leaf size; no verification or ABI promotion. Evidence under ignored local/:
observed-leaves-{audit,apply,repeat}-20261113.tsv and
observed-body-inventory-20261113. Commands: tools/ghidra_ef2000.py
recover-observed-leaves [--apply] --output <report>; tools/ghidra_ef2000.py
inventory --output local/ghidra/observed-body-inventory-20261113.


**2026-11-13 — Checked indirect ASM dispatch and XLAT.**
Added near 32-bit register/memory CALL and JMP to the explicit-state dispatcher.
Targets are read before CALL pushes its return address, preserving CALL ESP,
[ESP] and [ESP-4] aliasing. Dynamic targets must be translated instruction
starts; unknown PCs return status 2 without an original-code fallback.
Reports expose dynamic transfer sites and state that dependency closure covers
only direct edges. Far/16-bit transfers remain unsupported. Added byte XLAT
with wrapped EBX+AL addressing, partial AL write and unchanged flags.
All 83 focused tests pass (Python 3.13.12/Unicorn 2.1.4), including synthetic
indirect register/memory calls, stack aliases, tail jumps, jump tables and
explicit failure for an untranslated CPUID target. Command: python3 -m unittest
tests.test_asm_c_segments tests.test_asm_c_program tests.test_asm_to_c
tests.test_asm_inventory tests.test_asm_diff tests.test_ghidra_ef2000.
Repaired inventory selects 1,956 of 3,560 syntax-supported bodies, 242,587 body
bytes, including 313 bodies with dynamic transfers. All 1,956 body objects and
dispatcher compile with Watcom warnings treated as errors. Earlier unrepaired
1,953-body comparison exposed 29 bodies with missing dynamic targets, leading
to the reviewed boundary repairs above. Remaining target coverage and original
fault/budget probes are not passes; no full coverage, native ABI or game
integration claim. Artifacts: local/asm-c-observed-program-20261113 and
local/asm-c-observed-full-tests-20261113.log.

Final repaired-corpus comparison: 31,296 probes, 17,600 matches, 13,691
original-fault/budget inconclusive outcomes and five failures across three
bodies. 720 bodies pass all 16 prepared probes; 1,233 are inconclusive. The
remaining failures are explicit unknown-PC stops: formatter entries 0xded86
and 0xed133 each receive synthetic callback values 0x7f/0x80 (copyright text,
not reviewed code); 0x11f7ef reaches unowned x87 FDIV at 0x11f7f9. No arithmetic,
RAM or register mismatch appears in this run; these target-coverage failures
still keep the CLI gate nonzero. The 26 other formerly failing bodies no longer
fail after callback/XLAT recovery. Both callbacks have two matching and fourteen
inconclusive probes each; XLAT leaf has fourteen matching and two inconclusive.
Command: tools/asm_c_differential.py --translations
local/asm-c-observed-program-20261113 --output local/asm-c-observed-diff-20261113
--watcom /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386
(Python 3.13.12/Unicorn 2.1.4); final report exists, exit status 1 as expected.
Reliable bulk translation remains active: x87 state/precision, runtime services,
remaining integer patterns and boundaries, dynamic target coverage, and stronger
application-level evidence remain unresolved.


**2026-11-13 — Count branches and double shifts.**
Added LOOP/LOOPE/LOOPNE, JCXZ/JECXZ and SHLD/SHRD to the explicit-state
translator. Counter wraparound and flags preservation are compared at zero,
one, high-word and signed boundaries; address-size-prefixed LOOP stays rejected.
Double shifts use bounded one-bit concatenated-pair steps; 100,352 synthetic
original-vs-C cases cover both widths, directions, all 256 count values and
boundary source/destination pairs under undefined-behavior sanitization. Flags
are compared including measured undefined OF/AF and count>16 word behavior.
The full 85-test ASM/Ghidra suite passes; additional cross-body count-branch
composition checks use the same dispatcher and dependency analysis as calls.
New corpus: 3,598 syntax-supported, 2,090 selected bodies (269,778 body bytes),
134 more selected bodies and 27,191 more body bytes than the previous corpus.
All 2,090 bodies and dispatcher compile with Watcom warnings-as-errors.
Commands: python3 -m unittest tests.test_asm_c_segments tests.test_asm_c_program
tests.test_asm_to_c tests.test_asm_inventory tests.test_asm_diff
tests.test_ghidra_ef2000; tools/asm_c_program.py --inventory
local/asm-observed-body-inventory-20261113 --output
local/asm-c-countshift-program-20261113. Unit evidence:
local/asm-c-countshift-full-tests-20261113.log (Python 3.13.12/Unicorn 2.1.4).
No tracker promotion or application integration claim.

Final count/shift corpus: 33,440 probes, 18,500 matches, 14,935 original-fault/
budget inconclusive and the same five missing-target failures across three
bodies as before (0xded86, 0xed133, 0x11f7ef). All 1,956 previously selected
bodies have unchanged probe counts; the 134 newly selected bodies contribute
900 matches, 1,244 inconclusive, zero failures; 30 pass every probe and 104
remain inconclusive. Overall 750 bodies pass all probes, 1,337 inconclusive,
three fail. The command exits 1 and does not waive target gaps. Command:
tools/asm_c_differential.py --translations local/asm-c-countshift-program-20261113
--output local/asm-c-countshift-diff-20261113 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386.
Additional program composition tests pass (8 tests, including cross-body LOOP
and JECXZ). No reliable full-bulk or native/game integration claim.

x87 backend feasibility only: official Berkeley SoftFloat 3e archive
(https://www.jhauser.us/arithmetic/SoftFloat-3e.zip), SHA-256
21130ce885d35c1fe73fc1e1bf2244178167e05c6747cad5f450cc991714c746,
kept under local/softfloat-probe-20261113. Upstream documents extFloat80 and
precision control (https://www.jhauser.us/arithmetic/SoftFloat-3/doc/SoftFloat.html).
The non-FAST_INT64 8086 specialization compiles 232/237 library sources with
Watcom -we; five fail only on unreferenced labels (s_addExtF80M,
s_roundPackMToF128M, s_roundPackToF16/F32/F64). The raw arithmetic/conversion
entry points compile. Exact diagnostics are in watcom-probe.json. This is
not an integrated backend or precision/state equivalence proof; no third-party
source, oracle dependency or floating-point approximation was committed.


**2026-11-13 — Reproducible independent 80-bit arithmetic prerequisite.**
Added tools/asm_softfloat.py: explicitly supplied official SoftFloat 3e archive,
SHA-256 pinned, safe extraction and fresh ignored output directory, no network
or vendored dependency source. Builds 237 sources in the non-FAST_INT64 8086
specialization. Enable upstream SOFTFLOAT_ROUND_ODD so four conditional labels
remain used; remove only the unreferenced roundPack label from s_addExtF80M
with a no-incoming-goto check. License notices and statements remain intact.
All 237 host objects (C99 -Werror, upstream warning profile) and all 237 Watcom
objects (-w1 -we) compile; every compiler log is empty. Host shared-library link
rejects unresolved symbols. Manifest binds archive/source/header/platform,
commands, repaired source and compiled artifacts by hashes.
Four focused tests pass, including digest/path/symlink/repair rejection and an
opt-in synthetic arithmetic test. All 3,072 add/sub/mul/div cases match raw
80-bit host x87 results and exception flags across three precision and four
rounding modes. The independent hardware oracle executes synthetic opcodes,
saving/restoring the entire x87 environment/register stack. Unicorn 2.1.4 raw
results also match all 3,072; its observable FNSTSW flags disagree in 1,592
cases (inexact/overflow/underflow flags absent, some invalid cases differ).
This is a demonstrated oracle limitation, not a waived generated-C mismatch.
No x87 instruction lifting, architectural-state ABI, stack/tags/environment,
noncanonical/input-denormal handling, unmasked exceptions or transcendental
coverage is claimed. Existing bulk gate remains nonzero. Integration must
retain these gaps explicitly and use a suitable state oracle.
Commands: tools/asm_softfloat.py --archive
local/softfloat-probe-20261113/SoftFloat-3e.zip --output
local/asm-softfloat-verified-build-20261113 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386;
ASM_SOFTFLOAT_BUILD=local/asm-softfloat-verified-build-20261113 python3 -m unittest
tests.test_asm_softfloat (Python 3.13.12/Unicorn 2.1.4). Build report and
arithmetic-evidence.json are under that ignored directory; test log:
local/asm-softfloat-verified-tests-20261113.log. No tracker promotion.


**2026-11-13 — Explicit x87 state and comparison ABI 2.**
Added eight physical raw-80 registers as low/high significand and sign/exponent
words, plus control/status/tag, instruction/data pointers, selectors and opcode.
The oracle and C receive identical captured initial state; every raw register
(including empty-slot payloads) and every environment field is compared. Old
ABI 1 manifests are rejected before any compilation. No host floating-point
type determines the state representation. Added FNINIT environment/tag reset,
retaining physical contents. No arithmetic or broader stack support yet.
Seeded synthetic tests exercise nonzero TOP, payloads/tags, selectors/pointers,
control/status/opcode preservation; negative tests mutate every field and an
empty register and require mismatch. Hardware FLDENV/FNINIT/FNSTENV proves
FNINIT clears the saved opcode; Unicorn 2.1.4 leaves a seeded nonzero opcode
stale. The verifier continues to reject that difference explicitly; no field
masking or false original-vs-C pass. Default empty-stack/zero-opcode probes do
not expose it. Architectural-state completeness of the oracle remains unproven.
89-test ASM/Ghidra suite passes; all five current x87 tests pass separately
(including the subsequently added ABI rejection test). Synthetic FNINIT also
compiles warning-free with Watcom; all 2,090 corpus bodies and dispatcher do too.
Commands: python3 -m unittest tests.test_asm_c_x87 tests.test_asm_c_segments
tests.test_asm_c_program tests.test_asm_to_c tests.test_asm_inventory
tests.test_asm_diff tests.test_ghidra_ef2000; python3 -m unittest
tests.test_asm_c_x87; tools/asm_c_program.py --inventory
local/asm-observed-body-inventory-20261113 --output
local/asm-c-x87-state-program-20261113 (ABI 2, same 2,090 selected bodies,
269,778 body bytes). Evidence: local/asm-c-x87-state-{full-tests,tests}-20261113.log,
local/asm-c-x87-fninit-watcom-20261113/compile.log. Python 3.13.12/Unicorn 2.1.4;
Watcom v2 -bt=dos -3r -za99 -w1 -we. No tracker promotion or full-bulk claim.

Final ABI 2 corpus comparison completes: all 2,090 per-body probe counts are
unchanged from the count/shift corpus. 33,440 probes: 18,500 matches, 14,935
original-fault/budget inconclusive, five uncovered-PC failures across the same
three bodies (0xded86, 0xed133, 0x11f7ef); 750 bodies pass all probes, 1,337
inconclusive, three fail. Added x87 comparisons introduce no new failure for
the default empty-stack initial profile. This does not establish x87 arithmetic
or nonempty application-state equivalence. Exit 1 remains the correct gate.
Command: tools/asm_c_differential.py --translations
local/asm-c-x87-state-program-20261113 --output
local/asm-c-x87-state-diff-20261113 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386
(Python 3.13.12/Unicorn 2.1.4). Final report binds ABI 2 and explicitly records
x87 oracle limitations. Full reliable translation remains active.


**2026-11-13 — x87 control/status and AH flag transfer.**
Added FNCLEX, FNSTCW, FNSTSW (memory/AX), FLDCW and SAHF/LAHF. Control/status
stores and flag transfers preserve unrelated registers, x87 state and EFLAGS
bits; FNCLEX retains TOP/condition codes. FLDCW normalizes reserved CW bits and
updates pending exception ES/B from the new masks. Entry with a pending #MF
returns explicit status 8 without state mutation; exception delivery/FWAIT and
broader x87 stack/arithmetic lifting remain incomplete.
Hardware tests execute all 65,536 control words with zero or IE/PE flags plus
all 64 pending-flag/mask combinations: 135,040 C-vs-hardware cases pass. AH flag
transfers compare all 256 AH values with 64 arithmetic-flag combinations in
both directions (32,768 original-vs-C cases, UBSan enabled). Seeded state tests
cover nonzero tags/TOP/payload/environment and all FNCLEX clear bits. Hardware
FLDENV/FNSTSW AX/FNSTENV proves status-store leaves the environment unchanged;
Unicorn 2.1.4 instead changes FIP. It also retains reserved CW bits that hardware
normalizes. Negative differential regressions preserve those as mismatches;
no fields are masked or reported as successful original-vs-C comparisons.
All 92 ASM/Ghidra tests pass (128 s); the six latest x87 tests pass again after
expanding the hardware grid/negative cases. A synthetic body containing every
new operation compiles warning-free with Watcom -bt=dos -3r -za99 -w1 -we.
Regeneration selects the same 2,090 bodies/269,778 bytes: every generated C file,
header and complete translation manifest is byte-identical to the ABI 2 corpus.
Verified original evidence/report/library hashes, retaining its 18,500 matches,
14,935 inconclusive and same five target failures. No unnecessary rerun or
new coverage claim. Identity proof:
local/asm-c-x87-control-corpus-identity-20261113.json. This tranche establishes
control primitives; integer dependencies and broader x87 instructions still
prevent additional bodies from closing. Reliable full bulk translation is not
complete; no tracker promotion.
Commands: python3 -m unittest tests.test_asm_c_x87 tests.test_asm_c_segments
tests.test_asm_c_program tests.test_asm_to_c tests.test_asm_inventory
tests.test_asm_diff tests.test_ghidra_ef2000; python3 -m unittest
tests.test_asm_c_x87; tools/asm_c_program.py --inventory
local/asm-observed-body-inventory-20261113 --output
local/asm-c-x87-control-program-20261113. Logs: local/asm-c-x87-control-full-tests-20261113.log,
local/asm-c-x87-control-current-tests-20261113.log,
local/asm-c-x87-control-watcom-20261113/compile.log (Python 3.13.12/Unicorn 2.1.4).


**2026-11-13 — Raw x87 stack instructions and hardware state grid.**
Added shared primitives in tools/asm_x87.py, integrated in the generated runtime
and renderer: FLD1/FLDZ, register FLD/FXCH/FST/FSTP/FFREE, FCHS/FABS and
FINCSTP/FDECSTP. Model physical registers, tag classes, TOP and C1, masked
underflow/overflow indefinite results, and deferred unmasked invalid exceptions.
Pending delivery remains status 8 without mutation. Renderer records guest IP,
CS and opcode independently; register operations preserve the data environment.
No original-code fallback, native floating type or guessed function ABI.
Hardware probes establish source-underflow precedence over destination overflow
for FLD ST(i), including the case where both conditions apply. All 188,416
stack-effect cases match hardware across eight TOPs, all occupancy masks,
register indices and masked/unmasked IE. Payload classes: normal, both zeros,
quiet/signaling NaN, infinity, denormal and unnormal, even in empty slots.
2,944 generated-body comparisons also pass, including normalized instruction
IP/CS and directly compared opcode/data environment, flags and return stack.
C grids run with UBSan; native probes save/restore full host x87 state.
All 93 ASM/Ghidra tests pass; all 46 rendered variants compile warning-free
with Watcom. Corpus: 3,600 syntax-supported (+2), still 2,090 selected bodies,
269,778 body bytes; all selected bodies plus dispatcher compile with Watcom.
The newly syntax-supported 0x120928/0x1209b5 remain excluded by memory x87 and
comparison dependencies (0x120a07/0x120a2d/0x120a52). This is not extra recovered
bulk coverage or a complete x87 claim. Memory loads/stores, conversion,
arithmetic/comparisons, pending delivery and known oracle gaps remain active.
Commands: python3 -m unittest tests.test_asm_x87_stack tests.test_asm_c_x87
tests.test_asm_c_segments tests.test_asm_c_program tests.test_asm_to_c
tests.test_asm_inventory tests.test_asm_diff tests.test_ghidra_ef2000;
python3 -m unittest tests.test_asm_x87_stack;
tools/asm_c_program.py --inventory local/asm-observed-body-inventory-20261113
--output local/asm-c-x87-stack-program-20261113. Evidence:
local/asm-c-x87-stack-full-tests-20261113.log,
local/asm-x87-stack-{special,final}-tests-20261113.log,
local/asm-x87-stack-watcom-20261113/compile.log. Python 3.13.12/Unicorn 2.1.4;
Watcom v2 -bt=dos -3r -za99 -w1 -we. No tracker promotion.

Final expanded-runtime corpus run completes with every per-body outcome unchanged
from ABI 2: 18,500 matches, 14,935 original-fault/budget inconclusive and the
same five target failures across 0xded86, 0xed133, 0x11f7ef. 750 bodies pass all
16 probes; 1,337 inconclusive; three fail. 33,440 probes total, CLI exit 1.
This validates the previously selected integer corpus with the new shared
runtime, not x87 application coverage. New stack instructions remain covered
by the independent hardware/generated-body tests; the corpus dependency
closure still excludes them. Command: tools/asm_c_differential.py --translations
local/asm-c-x87-stack-program-20261113 --output local/asm-c-x87-stack-diff-20261113
--watcom /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386.
Final report exists. Full reliable translation remains active.


**2026-11-13 — Raw extended-memory x87 transfers.**
Added FLD/FSTP TBYTE with exact ten-byte little-endian access, stack/tag/status
handling and complete instruction/data environment updates. No host floating
type or conversion is used: hardware preserves signaling NaNs, denormals,
pseudo-denormals and unnormal payload encodings for this operand size.
Pending #MF stops before RAM access; operand bounds are checked before stack
fault effects. Masked stack faults transfer indefinite; unmasked IE preserves
stack/RAM and defers delivery while recording the faulting environment.
Data segment selection is decoded from ModRM/SIB, distinguishing EBP-as-index,
no-base SIB and SS bases, plus segment overrides. Unknown prefix forms remain
unsupported. This remains checked flat RAM, not complete page/segment fault or
exception delivery emulation.
81,920 generated-body cases match hardware across TOP/occupancy, masked and
unmasked IE, and ten payload classes as both loads and actual store sources.
Compare raw registers, tags/status, opcode, relocated instruction/data context,
RAM and return state. Sixteen child-process hardware fault probes (cores disabled)
establish pending #MF over memory fault and memory fault over stack fault.
C tests cover partial-span rejection, RAM smaller than the operand, wraparound,
last valid unaligned span and no state/RAM mutation on failure. Twenty address
variants compile warning-free with Watcom -bt=dos -3r -za99 -w1 -we.
All 95 ASM/Ghidra tests pass (132 s); latest two memory tests pass separately
after broadening store sources and boundary cases. Corpus: 3,601 syntax-
supported, 2,091 selected bodies, 269,815 bytes (+one 37-byte body at 0x120a2d).
All selected bodies and dispatcher compile warning-free with Watcom. No tracker
promotion or reliable full-bulk claim; narrower-format conversion, arithmetic,
comparisons, exception delivery and oracle accuracy remain unresolved.
Commands: python3 -m unittest tests.test_asm_x87_memory tests.test_asm_x87_stack
tests.test_asm_c_x87 tests.test_asm_c_segments tests.test_asm_c_program
tests.test_asm_to_c tests.test_asm_inventory tests.test_asm_diff
tests.test_ghidra_ef2000; python3 -m unittest tests.test_asm_x87_memory;
tools/asm_c_program.py --inventory local/asm-observed-body-inventory-20261113
--output local/asm-c-x87-memory-program-20261113. Logs:
local/asm-c-x87-memory-full-tests-20261113.log,
local/asm-x87-memory-final-tests-20261113.log,
local/asm-x87-memory-watcom-20261113/compile.log (Python 3.13.12/Unicorn 2.1.4
for full suite; standalone hardware tests also run on Python 3.13.17).

Final 2,091-body comparison: 33,456 probes, 18,500 matches, 14,937 original-fault/
budget inconclusive and 19 mismatches across four bodies. Every previous
per-body outcome is unchanged. New 0x120a2d has fourteen mismatches and two
original-unmapped-read inconclusive probes. All fourteen differ only in x87
status (C IE/SF=0x41 versus Unicorn zero) and saved opcode (C 0x32e or 0x1f6
versus Unicorn zero); GPRs, RAM and all other compared state match. The original
helper bytes, extracted only under ignored local/, also execute on host x87
with the same fourteen prepared inputs: hardware confirms the disputed C
status/opcode in every case. This long-mode probe has balanced 64-bit push/pop
versus original 32-bit push/pop and proves only those disputed x87 fields,
not full guest ABI/RAM equivalence. Evidence binds original payload/body,
comparison report and native probe library hashes:
local/asm-x87-original-memory-probe-20261113/evidence.json.
The existing five target failures persist (0xded86/0xed133/0x11f7ef). No oracle
difference is masked, reclassified as a pass, or waived: the gate exits 1;
750 bodies pass all prepared probes, 1,337 inconclusive, four fail. The emulator
itself is unchanged. Suitable independent x87 verification remains required.
Command: tools/asm_c_differential.py --translations
local/asm-c-x87-memory-program-20261113 --output local/asm-c-x87-memory-diff-20261113
--watcom /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386.
Final report exists; full reliable bulk translation remains active.


**2026-11-13 — Exact binary32/binary64 x87 memory loads.**
Added FLD DWORD/QWORD via integer IEEE-field widening and bounded denormal
normalization, without native floating types or rounding. Preserve signs,
zeros, infinities, NaN payloads; quiet sNaN and set IE. Input subnormals set DE
and normalize exactly. Hardware proves the nonuniform unmasked behavior:
IE suppresses the push, while DE commits it and records deferred delivery.
Full-stack fault precedes source-format exceptions; normal RAM/pending priority
and guest instruction/data environment remain intact. Share the encoded
ModRM/SIB/segment decoder with extended loads. Narrowing stores/arithmetic and
pending delivery remain unsupported.
473,088 generated-body hardware comparisons pass under UBSan: every exponent,
both signs, mantissa boundaries, all three standard precision controls/four
round modes, TOP/occupancy and independent IE/DE masks. Compare raw registers,
tags/status, opcode, relocated context, RAM and flags. Boundary checks cover
invalid/short/wrapping spans and the last valid operand. Twenty real-load
address variants compile warning-free with Watcom -bt=dos -3r -za99 -w1 -we.
All 96 ASM/Ghidra tests pass (144 s); focused latest load tests pass separately.
Regeneration: 3,606 syntax-supported, 2,096 selected bodies, 270,162 body bytes
(+5 selected, +347 bytes): 0x45b60, 0x84f60, 0x11ac3c, 0x11b3c0, 0x120a60.
All selected bodies and dispatcher compile warning-free with Watcom. No tracker
promotion or full-bulk/game integration claim; oracle gaps stay explicit.
Commands: python3 -m unittest tests.test_asm_x87_load tests.test_asm_x87_memory
tests.test_asm_x87_stack tests.test_asm_c_x87 tests.test_asm_c_segments
tests.test_asm_c_program tests.test_asm_to_c tests.test_asm_inventory
tests.test_asm_diff tests.test_ghidra_ef2000;
python3 -m unittest tests.test_asm_x87_load;
tools/asm_c_program.py --inventory local/asm-observed-body-inventory-20261113
--output local/asm-c-x87-real-load-program-20261113. Evidence:
local/asm-c-x87-real-load-full-tests-20261113.log,
local/asm-x87-real-load-final-tests-20261113.log,
local/asm-x87-real-load-watcom-20261113/compile.log. Full suite Python 3.13.12/
Unicorn 2.1.4; standalone hardware grid also Python 3.13.17.

Final 2,096-body comparison completes: 33,536 probes, 18,500 matches, 14,969
original-fault/budget inconclusive and 67 mismatches across seven bodies.
All prior 2,091 per-body outcomes are unchanged. New 0x45b60/0x11ac3c each have
16 inconclusive probes. New 0x84f60/0x11b3c0/0x120a60 each have 16 mismatches,
all exclusively saved x87 opcode (C 0x504/0x505 versus Unicorn zero). Raw FP
values, tags/status, GPRs, flags, RAM and other environment fields match in
those completed probes. Added focused hardware SIB/displacement opcode tests
independently confirm 0x504/0x505; host RIP-relative form shares ModRM opcode
encoding with guest absolute form, without asserting identical addressing.
Latest two load tests pass; the opcode method passes again after replacing its
raw encoding with equivalent assembler syntax. Logs:
local/asm-x87-real-load-final-tests-20261113.log and
local/asm-x87-real-load-opcode-tests-20261113.log. No original bytes committed.
The remaining 19 failures persist: five uncovered-target probes and fourteen
previously hardware-confirmed status/opcode disagreements at 0x120a2d. No field
masking, waived mismatch or oracle rewrite: bulk CLI exits 1. 750 bodies pass
all probes, 1,339 inconclusive, seven fail. Full reliability remains unproven;
independent x87 oracle coverage and remaining instruction/runtime families stay
active. Command: tools/asm_c_differential.py --translations
local/asm-c-x87-real-load-program-20261113 --output
local/asm-c-x87-real-load-diff-20261113 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386.
Final report exists and binds the current runtime.


**2026-11-13 — Exact signed integer x87 memory loads.**
Added FILD WORD/DWORD/QWORD with unsigned magnitude and bounded normalization.
Handle signed minima without UB and retain all 64-bit integer precision in every
PC/RC setting. Preserve flags and unrelated raw registers/state; use existing
bounds, pending delivery, stack-fault and encoded environment rules. Integer
widening introduces no data inexact/denormal exception. No native floating type,
original-code fallback or guessed guest ABI.
879,096 generated-body hardware comparisons pass under UBSan: exhaustive signed
16-bit values across twelve precision/rounding modes, 32/64-bit extrema and
power-of-two boundaries, TOP/occupancy and masked/unmasked IE. Compare full raw
FP/status/tag state in the stack grid, plus opcode, relocated context, flags,
return stack and input RAM. Bounds tests include INT64_MIN at the final valid
span. Thirty encoded address variants compile warning-free with Watcom.
All 98 ASM/Ghidra tests pass (154 s). Latest integer-load tests additionally
exercise child-process hardware fault priority for all three widths, with cores
disabled. Corpus remains 3,606 syntax-supported, 2,096 selected bodies and
270,162 bytes: integer-load bodies still meet later unsupported stores,
arithmetic or dependencies. All corpus bodies and dispatcher compile warning-
free with Watcom. No inflated coverage, tracker promotion or complete-bulk claim.
Commands: python3 -m unittest tests.test_asm_x87_integer_load tests.test_asm_x87_load
tests.test_asm_x87_memory tests.test_asm_x87_stack tests.test_asm_c_x87
tests.test_asm_c_segments tests.test_asm_c_program tests.test_asm_to_c
tests.test_asm_inventory tests.test_asm_diff tests.test_ghidra_ef2000;
python3 -m unittest tests.test_asm_x87_integer_load;
tools/asm_c_program.py --inventory local/asm-observed-body-inventory-20261113
--output local/asm-c-x87-integer-load-program-20261113. Evidence:
local/asm-c-x87-integer-load-full-tests-20261113.log,
local/asm-x87-integer-load-final-tests-20261113.log,
local/asm-x87-integer-load-watcom-20261113/compile.log. Full suite Python 3.13.12/
Unicorn 2.1.4; standalone hardware test also Python 3.13.17. Watcom v2 -bt=dos
-3r -za99 -w1 -we. Narrowing/rounding stores and arithmetic remain pending.

Final expanded-runtime comparison completes: every 2,096 per-body probe count
is unchanged from the real-load corpus. 33,536 probes: 18,500 matches, 14,969
original-fault/budget inconclusive, 67 existing mismatches across seven bodies;
750 bodies pass all probes, 1,339 inconclusive, seven fail. Host libraries are
also byte-identical, confirming the unselected integer helper adds no executable
behavior to this corpus. Hardware/synthetic coverage proves FILD separately;
there is no added closed-body coverage claim. Known target gaps and x87 oracle
differences stay explicit; CLI exits 1 and no mismatch is waived.
Command: tools/asm_c_differential.py --translations
local/asm-c-x87-integer-load-program-20261113 --output
local/asm-c-x87-integer-load-diff-20261113 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386.
Final report exists. Full reliable bulk translation remains active.


**2026-11-14 — Pinned narrowing conversion adapter prerequisite.**
Added tools/asm_x87_narrow.c around SoftFloat 3e's 8086 extended-to-real
conversions, with explicit raw-word/bit-result ABI. Include the same platform
header as the dependency; no native floating type or guest memory side effect.
Detect unsupported extended encodings (IE/indefinite), normalize pseudo-
denormals, retain NaN signs/payloads, map flags and derive C1 via a second
conversion toward zero. Restore all changed library settings; require serialized
calls for the non-TLS dependency. No original-code fallback or game code.
Added asm_softfloat.py --narrow-from, requiring fresh ignored output and a
validated 237-source profile, source/header/platform/library hashes, safe paths,
unique bindings and (when requested) Watcom object hashes. Record adapter source,
commands, dependency report/library and resulting artifacts. Source/header/path/
profile/duplicate/missing-Watcom corruption fails before compilation/output.
Host and Watcom -w1 -we compile the adapter without diagnostics.
277,984 hardware masked-conversion comparisons pass: all 32,768 extended
exponents in four rounding modes for both widths, plus signed boundary/special
significands across all standard PC/RC settings. Compare output bits, exception
flags and C1; context restoration and invalid arguments are checked. Six
SoftFloat/adapter tests pass with all opt-in dependencies enabled; existing
3,072 arithmetic hardware cases are rerun too. This is a conversion prerequisite,
not completed FST/FSTP or unmasked-store/pop semantics. No translation/runtime
code or selected corpus changed; existing bulk gate remains nonzero.
Commands: tools/asm_softfloat.py --narrow-from
local/asm-softfloat-verified-build-20261113 --output
local/asm-x87-narrow-build-20261114 --watcom
/nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386;
ASM_SOFTFLOAT_BUILD=local/asm-softfloat-verified-build-20261113 python3 -m unittest
tests.test_asm_softfloat tests.test_asm_x87_narrow. Evidence:
local/asm-x87-narrow-final-tests-20261114.log,
local/asm-x87-narrow-evidence-20261114.json and
local/asm-x87-narrow-build-20261114/report.json (Python 3.13.12/Unicorn 2.1.4).
Store-mask commit/pop behavior, remaining arithmetic/runtime and oracle accuracy
stay active; no tracker promotion or full-bulk claim.

Next store integration must retain experimentally observed commit distinctions:
unmasked IE/OE/UE suppress the memory write/pop; unmasked PE can still write and
pop. Unmasked UE also traps an exact subnormal result that raises no UE when
masked; therefore masked conversion flags alone are insufficient to decide
store effects. The native probes are under local/asm-x87-narrow-probe-20261114
(state.c/state.so), currently binary32 exploratory cases only. These observations
are implementation input, not a completed instruction conformance test.


**2026-11-14 — Generated narrowing stores and explicit backend binding.**
Integrated FST/FSTP DWORD/QWORD with the pinned conversion adapter. Model all
mask-dependent effects: unmasked IE/OE/UE suppress writes/pop and new PE/C1;
unmasked PE may commit, then defer delivery. Exact subnormals also signal
unmasked UE. Preserve old sticky flags, raw source payloads and tags; empty
source uses masked indefinite or deferred unmasked invalid. Bounds and pending
checks precede effects. Update encoded instruction/data environment separately.
Optional header section supports late inclusion in the combined translation
unit. Both generators declare required backend and adapter-source hash. The
verifier requires --softfloat-build, validates source/profile/header/library/
Watcom objects before creating output, builds a recorded adapter, and rejects
unresolved symbols. Missing/stale/undeclared dependencies fail explicitly.
458,752 generated-body hardware comparisons pass under UBSan: all 64 masks,
four RC modes, all TOPs, empty/nonempty sources, signed boundary/special values,
both widths and pop forms. Compare RAM/canaries, every retained physical FP
value, status/tags, flags, opcode and relocated context. Additional isolated
fault probes cover invalid data, overflow, underflow/precision, empty stack and
pending #MF; bounds/pending checks verify no partial C state/RAM mutation.
Backend declaration/source tampering fails before compilation/output. Forty
address variants and the adapter compile warning-free with Watcom.
All 106 ASM/Ghidra/dependency tests pass with optional dependencies enabled
(169 s). Regeneration: 3,640 syntax-supported, 2,107 selected bodies, 272,045
bytes (+11 selected, +1,883 bytes). All body objects and dispatcher compile with
Watcom; the full corpus comparison is a separate gate. No tracker promotion or
full game/native ABI claim; remaining arithmetic, delivery and oracle accuracy
stay active.
Commands: ASM_SOFTFLOAT_BUILD=local/asm-softfloat-verified-build-20261113 python3
-m unittest tests.test_asm_x87_store tests.test_asm_x87_narrow tests.test_asm_softfloat
tests.test_asm_x87_integer_load tests.test_asm_x87_load tests.test_asm_x87_memory
tests.test_asm_x87_stack tests.test_asm_c_x87 tests.test_asm_c_segments
tests.test_asm_c_program tests.test_asm_to_c tests.test_asm_inventory
tests.test_asm_diff tests.test_ghidra_ef2000;
tools/asm_c_program.py --inventory local/asm-observed-body-inventory-20261113
--output local/asm-c-x87-store-program-20261114. Evidence:
local/asm-c-x87-store-full-tests-20261114.log,
local/asm-x87-store-final-tests-20261114.log,
local/asm-x87-store-watcom-20261114/compile.log. Full suite Python 3.13.12/
Unicorn 2.1.4; native-only tests also Python 3.13.17. Watcom -bt=dos -3r -za99
-w1 -we. Original assets remain unchanged and all generated material stays local/.

Completed corpus comparison: 33,712 probes, 18,530 matching, 15,068 inconclusive,
114 mismatching. All prior 2,096 per-body outcomes are unchanged. Eleven added
bodies contribute 30 matches, 99 inconclusive and 47 mismatches across four
bodies (0x2b634, 0x3519c, 0xea884, 0x120a52). No differences are waived.
Actual original helper 0x120a52 independently agrees with C on hardware for
status/opcode and eight-byte target spans in all fourteen disputed completed
probes, including masked empty-stack indefinite stores. The host-long-mode
probe ignores ES and uses host return width: this confirms those fields only,
not full guest context or ABI equivalence. Its evidence binds original payload,
body, bulk report and probe library hashes. Evidence:
local/asm-x87-original-store-probe-20261114/evidence.json and
local/asm-c-x87-store-diff-20261114/report.json. Run with --softfloat-build
local/asm-softfloat-verified-build-20261113 and --watcom as above. Corpus gate
exits nonzero; oracle accuracy, checked-target failures, arithmetic and game
integration remain active. No tracker promotion.

**2026-11-14 — Independently validated masked extended arithmetic adapter.**
Added generic asm_x87_binary.c for raw add/subtract/multiply/divide, returning
extended bits and x87 flags/C1 at every defined precision/rounding setting.
Normalize pseudo-denormals, handle unsupported encodings and denormal-operand
priority, and preserve SoftFloat mode/flags/tininess/precision settings. Invalid
API controls leave output/settings unchanged. Shared validated adapter builder
now supports --binary-from and records dependency/source/library/Watcom hashes.
1,064,400 hardware cases agree on raw results and flags/C1; cover signed
zeros/infinities, multiple signaling/quiet NaN payloads, unsupported encodings,
subnormals/pseudo-denormals, cancellation/rounding boundaries, seeded random
canonical/arbitrary inputs and all extended exponents with PC 0/2/3 and RC 0..3.
Hardware inputs use FRSTOR, so denormal/unsupported raw operands are not silently
normalized by a preceding load. Native evidence exposed DE suppression by
invalid/NaN or divide-by-zero; adapter now models it. Watcom build warning-free.
Nine focused arithmetic/narrowing/dependency/store tests pass with no skips.
Commands: ASM_SOFTFLOAT_BUILD=local/asm-softfloat-verified-build-20261113 python3
-m unittest tests.test_asm_x87_binary tests.test_asm_x87_narrow
tests.test_asm_softfloat tests.test_asm_x87_store (Python 3.13.12/Unicorn 2.1.4);
tools/asm_softfloat.py --binary-from local/asm-softfloat-verified-build-20261113
--output local/asm-x87-binary-build-20261114 --watcom <wcc386>.
Evidence: local/asm-x87-binary-regression-20261114.log,
local/asm-x87-binary-evidence-20261114.json,
local/asm-x87-binary-build-20261114/report.json and watcom.log.
This adapter proves masked raw arithmetic only; unmasked architectural effects,
stack/rendering integration, pending exception delivery, full corpus and game
integration still require work. Existing selected-body count and nonzero gate
are unchanged. No tracker promotion; all dependency/generated artifacts local/.

**2026-11-14 — Register arithmetic, exception masks and exponent wrapping.**
Integrated FADD/FMUL/FSUB/FSUBR/FDIV/FDIVR register forms and pop variants.
Check opcode/operand spelling before rendering; preserve raw popped payloads,
all other registers/tags and prior sticky flags. Model masked/unmasked stack
invalid, early IE/DE/ZE suppression, and late OE/UE/PE results/pop. New controlled
adapter rebases finite inputs, preserving tiny sticky contributions, to obtain
precision-rounded exponent-wrapped OE/UE results (24,576 bias). Exact subnormal
results can raise unmasked UE; a rounded minimum-normal result avoids it.
Retain explicit pending-delivery status 8; reserved PC remains unsupported.
Both generators bind a separate arithmetic backend/source hash; verifier
validates dependencies before creating output, builds recorded adapters and
links both backends with unresolved symbols rejected. No original fallback.
5,512,656 raw hardware cases pass across all exception masks, PC/RC and
special/range/rounding inputs. Another 1,880,064 generated-body hardware cases
pass under UBSan: all 144 encoded register variants, every mask/defined PC/RC,
rotated TOP, empty operands and seeded sticky flags; compare all physical raw
registers, tags/status, opcode, flags and relocated context, including retained
popped payloads. Pending-state checks show no mutation; RAM unchanged. All 144
forms compile warning-free with Watcom. Regeneration selects 2,117 bodies,
273,524 bytes (+10 bodies, +1,479 bytes); syntax support 3,665. All 2,117 body
objects and dispatcher compile with Watcom -we. Full regression/corpus gates
are recorded separately when complete. The unowned 0x11f7f9 FDIV target still
needs boundary review; memory arithmetic also remains unsupported. No tracker promotion or game/ABI
claim. Evidence:
local/asm-x87-binary-control-tests-20261114.log,
local/asm-x87-binary-control-evidence-20261114.json,
local/asm-x87-register-arithmetic-tests-20261114.log,
local/asm-x87-register-arithmetic-evidence-20261114.json,
local/asm-x87-register-arithmetic-watcom-20261114/compile.log,
local/asm-c-x87-register-arithmetic-program-20261114/report.json.

Final regression: 109 tests pass without skips (250 s, Python 3.13.12/Unicorn
2.1.4); final strengthened arithmetic context checks also pass (62 s, Python
3.13.17). Native-only stress isolated a host observation problem: four pointer
changes in ten million samples, all with measured thread context switches;
9,999,854 uninterrupted samples preserve pointers. Consistent with the AMD
pointer restore limitation documented in Linux arch/x86/kernel/fpu/core.c.
The generated-body test now brackets each native call with per-thread context
switch counts and rejects interrupted measurements before examining results
or running C; bounded retries repeat the exact input. Final run rejects 61
interrupted samples and compares all fields for 1,880,064 accepted cases. No
diff-based retry or field masking. Evidence binds current adapter, dependency,
runtime, generated and test sources. Host-only evidence:
local/asm-x87-binary-unmasked-probe-20261114/context-switch-evidence.json.
Full comparison: 33,872 probes, 18,530 matches, 15,097 inconclusive and 245
mismatches across 21 bodies; 751 bodies pass all probes and 1,345 are
inconclusive. All prior 2,107 per-body outcomes unchanged. Ten new bodies
contribute 29 inconclusive and 131 mismatches, confined to x87 status/opcode
(including 0/0 invalid versus divide-by-zero). All discrepancies remain failures;
gate exits 1. Report: local/asm-c-x87-register-arithmetic-diff-20261114/report.json.
Commands: ASM_SOFTFLOAT_BUILD=local/asm-softfloat-verified-build-20261113 python3
-m unittest tests.test_asm_x87_arithmetic tests.test_asm_x87_binary plus the
preceding 106-test list; tools/asm_c_differential.py --translations
local/asm-c-x87-register-arithmetic-program-20261114 --output
local/asm-c-x87-register-arithmetic-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386>.
Logs: local/asm-x87-register-arithmetic-full-tests-20261114.log and
local/asm-x87-register-arithmetic-tests-20261114.log. Oracle accuracy, reviewed
dynamic target ownership, remaining x87 operations and game integration remain
active. No tracker promotion and no full equivalence claim.

**2026-11-14 — Complete division-dispatch boundary and invalid draft withdrawal.**
Reviewed missing target 0x11f7f9 from the computed jump at 0x11f7f2. It belongs
to the complete 64-way division dispatch 0x11f7ef..0x11fc1a, not a separate
ordinary leaf. Full table/linear decoding establishes 1,068 bytes and 376
instructions: prefix plus 64 contiguous cases, 48 RET/16 INT 6 exits and 42
calls to 0x11f6d8; next helper starts at 0x11fc1b. New reviewed Ghidra recovery
checks exact image/body/table provenance, prior body, ownership/data conflicts,
case boundaries, permitted flow, sole callee and stack cleanup. Adds all 64
computed-jump references and the complete body atomically. No original bytes
or pseudo-C committed. Read-only flow-gap/body-hole audits preceded recovery;
the ten-byte prefix alone had no internal holes for those audits to expose.
Proposal rollback, apply (10 -> 1068), repeat (already recovered), and wrong
provenance/no-output guard run on a disposable project. Regenerated 4,279-body
inventory: 4,277 instruction-layout agreements, including all 376 root
instructions. Pseudo-C still warns about the table and remains nonauthoritative.
Both caller sites 0x1189a6/0x1189df set EAX=15 and continue using x87 results.
Existing C only returns a table target: mark compiles-bad, withdraw confirmed
signature, correct extent and invalid test index, and label the test template
as missing raw x87 caller input. No function is promoted to verified.
Complete root now correctly fails generation at unsupported INT; helper closure
and memory arithmetic also need work. Syntax support 3,664; selected 2,116,
273,514 bytes. All remaining body sources byte-identical to preceding corpus.
This is a recovery correction, not proof that the previously failing dispatch
now executes correctly. Full corpus evidence is recorded separately below.
Commands: tools/ghidra_ef2000.py audit-flow-gaps / audit-body-holes 0x11f7ef;
recover-divide-dispatch --workspace local/ghidra-x87-dispatch-20261114 (report,
--apply, repeat); inventory --output <workspace>/body-inventory; asm_inventory
export from those ranges; tools/asm_c_program.py --inventory
local/asm-x87-dispatch-body-inventory-20261114 --output
local/asm-c-x87-dispatch-program-20261114. Ghidra 12.1.4/OpenJDK 21.0.12.1,
private XDG settings/cache under /tmp; base project and assets unchanged.
Evidence: local/asm-x87-dispatch-{flow-gaps,body-holes,proposal,apply,repeat,
bad-provenance,inventory,decompile}-20261114 logs/TSVs;
local/ghidra-x87-dispatch-20261114/body-inventory/body-ranges.json;
local/asm-x87-dispatch-body-inventory-20261114/0011f7ef.json.

Focused regression: 80 Ghidra/inventory/program/renderer tests pass without
skips (109 s); tracker check passes for 4,292 rows. Command: python3 -m unittest
tests.test_ghidra_ef2000 tests.test_asm_inventory tests.test_asm_c_program
tests.test_asm_to_c (Python 3.13.12 with Unicorn 2.1.4); python3
tools/decomp_status.py check. Log: local/asm-x87-dispatch-regression-20261114.log.
All 2,116 remaining body objects and rebuilt dispatcher compile warning-free
with Watcom -bt=dos -3r -za99 -w1 -we.

Completed repaired-corpus comparison: 33,856 probes, 18,530 matching, 15,082
inconclusive and 244 mismatching across 20 bodies. All remaining per-probe
records are identical to the preceding 2,117-body run (not just totals).
Removed prefix had one mismatch and fifteen inconclusive probes; none is
reclassified as a pass. 751 bodies pass all probes; 1,345 remain inconclusive.
CLI exits 1 and complete-dispatch translation remains unsupported. No narrowed
equivalence claim. Command: tools/asm_c_differential.py --workspace
local/ghidra-x87-dispatch-20261114 --translations
local/asm-c-x87-dispatch-program-20261114 --output
local/asm-c-x87-dispatch-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386>, Python 3.13.12/
Unicorn 2.1.4. Evidence: local/asm-c-x87-dispatch-diff-20261114/report.json,
whose manifest digest is independently checked. Original assets remain intact;
all original/generated exports and Ghidra projects stay ignored under local/.

**2026-11-14 — Independent x87 oracle work rationale.**
Current strict corpus has 244 mismatching probes across 20 bodies; 240 include
x87 opcode differences and 137 include status differences. Independent native
ISA matrices and original-helper probes already establish several Unicorn
defects, including missing status/opcode, 0/0 priority and empty-stack stores.
A separate opt-in native x87 coprocessor is warranted for the corpus, rather
than weakening comparisons or special-casing a single function. Preserve the
default unmodified Unicorn mode and every compared state/RAM field. New oracle
must execute only a fixed generic ISA allowlist, use no translator/SoftFloat
helpers, reject unknown forms and pending delivery, record provenance/coverage,
and qualify memory/context relocation and host interruption handling.

**2026-11-14 — Opt-in native x87 coprocessor for original execution.**
Added independent asm_x87_oracle.py and --x87-oracle hardware; default Unicorn
behavior stays unchanged. Fixed generic ISA allowlist covers currently generated
x87 register/stack/arithmetic, real/integer memory transfers and control forms.
No translator/runtime helpers, SoftFloat or arbitrary guest bytes in the native
executor. Raw FSAVE physical state and all metadata enter/leave a saved/restored
host x87 context. Independent ModRM/SIB decoding, full operand reads/private
buffers, pointer/selector/FOP relocation and AX preservation join the native
operation to the flat Unicorn integer/memory oracle. Unsupported/prefixed forms
and pending delivery are inconclusive; FWAIT without pending status is a no-op.
Reject interrupted native measurements using thread context-switch counters
before output inspection/C execution; bounded identical-input retries only,
never crash/diff retries. Native operand-size/opcode checks precede execution.
Record native source/tool/library/command provenance and per-probe operation/
interruption evidence. Keep every state/RAM comparison and original-fault/fuel
policy. No full hardware guest ABI, protected descriptor or #MF delivery claim.
Six new tests pass: 3,156 ModRM/SIB/default/override-address checks against
independent Unicorn accesses; native controls/raw-memory integration; corruption
of all eight physical payloads, all x87 fields and RAM/GPR/flags/PC detected;
unsupported forms/delivery/fuel explicitly inconclusive; mode/report provenance
checked. Focused regression passes 99 tests without skips (127 s), retaining all
old Unicorn mismatch expectations. Previously failing 20-body focused rerun:
240 x87 mismatch probes now match all fields; four target-coverage failures
remain, with original inconclusive outcomes retained. Full corpus is separate.
Commands: python3 -m unittest tests.test_asm_x87_oracle tests.test_asm_c_x87
tests.test_asm_c_segments tests.test_asm_c_program tests.test_asm_to_c
tests.test_asm_inventory tests.test_asm_diff tests.test_ghidra_ef2000;
ASM differential command as before plus --x87-oracle hardware. Python 3.13.12/
Unicorn 2.1.4. Evidence: local/asm-x87-oracle-tests-20261114.log,
local/asm-x87-oracle-regression-20261114.log,
local/asm-x87-oracle-focused-evidence-20261114.json. C translator and generated
bodies unchanged; native execution is restricted to the original oracle.

Full hardware-mode corpus completes: 33,856 probes, 18,770 matches, 15,082
inconclusive and four mismatches across two bodies. Transition audit against
the unmodified Unicorn run: 18,530 pass -> pass, 15,082 inconclusive ->
inconclusive, 240 mismatch -> pass, four mismatch -> mismatch; no completed
probe downgraded. 758 all-pass bodies, 1,356 inconclusive, two mismatching.
Remaining failures are 0xded86/0xed133 reaching unowned callback values 0x7f/
0x80, still C status 2 after original completion. CLI exits 1. The separate
default-Unicorn report retains 244 mismatches; no report/field is rewritten.
Native coverage: 1,584 operations, 22 forms, 309 probes; zero interrupted
samples. Eight previously inconclusive probes stop at unsupported native forms
(three register, five memory); unsupported observations remain explicit gaps.
All 2,116 C body objects and dispatcher compile warning-free with Watcom -we;
host original/coprocessor and portable C/SoftFloat execution remain separate.
Current oracle source digest and unchanged translation manifest verified against
the final reports. Evidence:
local/asm-c-x87-hardware-oracle-diff-20261114/report.json,
local/asm-c-x87-hardware-oracle-diff-20261114/original-x87/report.json.
Command: tools/asm_c_differential.py --workspace
local/ghidra-x87-dispatch-20261114 --translations
local/asm-c-x87-dispatch-program-20261114 --output
local/asm-c-x87-hardware-oracle-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386>
--x87-oracle hardware (Python 3.13.12/Unicorn 2.1.4, Linux x86-64).
No tracker promotion or full native ABI/game claim. Target coverage, unsupported
x87/trap/runtime services, captured inputs and game integration remain active.

**2026-11-14 — Exact real-memory arithmetic with operand exception priority.**
Added all twelve D8/DC real-memory add/multiply/subtract/divide/reverse forms.
Reuse exact IEEE widening with FLD; preserve signaling NaNs for arithmetic,
then let the validated adapter determine priority. Explicitly carry DE for
normalized narrow subnormals into controlled arithmetic; invalid/NaN or ZE
suppresses DE, while unmasked DE suppresses the arithmetic destination before
late range/precision effects. Existing register API delegates with no external
DE. Preserve original bounds/pending ordering, empty-stack invalid behavior,
sticky flags, raw nondestination payloads and tags; no source writes or pop.
Independent native oracle adds these memory forms and validates measured host
CS/DS before relocating metadata. No arithmetic helper is shared with it.
847,872 generated-body/native cases pass under UBSan across all forms, masks,
defined precision/rounding, rotated TOP, empty sources and signed special/
denormal/range/rounding operands. Compare all raw physical payloads, tags/status/
control, opcode, flags/GPRs, source/canaries and relocated context. Null-memory
fork probes confirm #PF before new stack invalid, pending #MF before #PF;
bounds/pending C checks verify no partial state/RAM mutation. Another 120
composed addressing/segment cases pass and compile with Watcom -we, including
ModRM/SIB/absolute and SS/ES/FS/GS=24. Refactored FLD and native-oracle focused
regressions pass (nine tests, 15 s). Regeneration: 3,803 syntax-supported,
2,178 selected bodies, 282,897 bytes (+62 bodies, +9,383 bytes). Full suites and
corpus outcomes are separate evidence. No tracker promotion/game equivalence.
Commands: ASM_SOFTFLOAT_BUILD=local/asm-softfloat-verified-build-20261113 python3
-m unittest tests.test_asm_x87_memory_arithmetic; set
ASM_MEMORY_ARITHMETIC_WATCOM=<wcc386> for all 120 addressing-object checks.
tools/asm_c_program.py --inventory local/asm-x87-dispatch-body-inventory-20261114
--output local/asm-c-x87-memory-arithmetic-program-20261114. Evidence:
local/asm-x87-memory-arithmetic-tests-20261114.log,
local/asm-x87-memory-arithmetic-evidence-20261114.json,
local/asm-x87-memory-addressing-tests-20261114.log,
local/asm-c-x87-memory-arithmetic-program-20261114/report.json.

Full regression: 118 tests pass without skips (296 s), including all previous
raw arithmetic/register/store/real-load hardware suites, Ghidra/inventory,
segments, dependency integrity and oracle checks. The strengthened memory suite
then passes again (two tests, 41 s), adding denormal operands combined with
overflow/underflow so the final matrix reaches 847,872 cases. All 2,178 body
objects and dispatcher compile warning-free with Watcom -we. Evidence:
local/asm-x87-memory-arithmetic-full-tests-20261114.log and
local/asm-x87-memory-arithmetic-final-tests-20261114.log. Full test command is
the preceding 116-test list plus tests.test_asm_x87_memory_arithmetic, with
ASM_SOFTFLOAT_BUILD and ASM_MEMORY_ARITHMETIC_WATCOM enabled; Python 3.13.12/
Unicorn 2.1.4. Final matrix artifact binds current adapter/runtime/test/native
source digests. Independent layout inspection also identifies the two excluded
bodies (0xfa3d4, 0x11f6d8): decoder disagreements concern FWAIT-prefixed grouping,
not yet normalized or accepted. Division closure remains blocked; resolve this
in its own recovery/decoder change. No scope or comparison field is waived.

Completed hardware-oracle corpus: 34,848 probes, 19,408 matches, 15,436
inconclusive and four existing mismatches across 0xded86/0xed133 (targets 0x7f/
0x80). Added 62 bodies contribute 638 matches and 354 inconclusive, no new
mismatches. Every preceding probe's pass/inconclusive/mismatch state remains
unchanged: 18,770/15,082/four. 779 bodies pass all probes, 1,397 remain
inconclusive and two mismatch. CLI exits 1. Native reference executes 8,731
operations in 39 forms; seven unsupported register observations remain
inconclusive. No missing observation is called a pass. Current arithmetic
adapter and native-oracle source digests verified in final report; final memory
matrix also binds current adapter/runtime/test/native source digests. It rejects
39 interrupted measurements before any result inspection or C execution.
Command: tools/asm_c_differential.py --workspace
local/ghidra-x87-dispatch-20261114 --translations
local/asm-c-x87-memory-arithmetic-program-20261114 --output
local/asm-c-x87-memory-arithmetic-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386>
--x87-oracle hardware (Python 3.13.12/Unicorn 2.1.4). Evidence:
local/asm-c-x87-memory-arithmetic-diff-20261114/report.json. No tracker promotion;
decoder grouping, target coverage, remaining x87/trap/runtime services, captured
inputs and full-game integration remain active. Assets unchanged; all generated
code, original exports and dependency artifacts remain ignored under local/.

**2026-11-14 — Separate WAIT execution and strict normalized layout provenance.**
Resolved both remaining decoder disagreements through architectural WAIT
normalization, not changed Ghidra body extents or waived layout checks. Intel's
FSTCW/FNSTCW reference documents separate WAIT and nonwaiting execution.
Normalize leading WAIT chains only when standalone or grouped with an x87
operation; retain every physical PC and byte and re-decode the residual opcode.
Raw Ghidra spans remain available beside normalized spans. Schema 4 copies and
hash-binds the raw body-range evidence, and translation rechecks its payload/
document digest, normalization profile, raw/normalized spans and agreement flag.
Reject false agreement, missing spans, bogus merged non-x87 spans and tampered
evidence. Legacy schema 3 remains accepted only with independent decode
agreement; fused old inventories require regeneration.
All 4,279 recovered bodies now agree. 0xfa3d4's repeated WAIT/FNSTCW grouping
and 0x11f6d8's WAIT/FLDCW grouping are explained without changing ownership,
ranges or payload. WAIT advances one byte/one fuel unit and otherwise preserves
state; pending delivery returns 8 before the following store. Separate residual
FNSTCW entry remains nonwaiting even with pending state. Tests cover full-state
native comparison, separate entry/fuel, no partial mutation, native #MF-before-
store and negative normalization/provenance cases. No #MF delivery implemented.
Fourteen focused WAIT/inventory tests and 104 regression tests pass without
skips (0.7 s / 128 s; Python 3.13.12/Unicorn 2.1.4). All 2,180 body objects and
dispatcher compile with Watcom -bt=dos -3r -za99 -w1 -we. Syntax support 3,806;
selected 2,180 / 283,197 bytes (+two bodies/+300 bytes). New 0x11f6d8 and
0x120800 each match all 16 probes. 0xfa3d4 remains untranslated at FCOMP.
Every preceding body source byte-identical and every preceding probe outcome
unchanged. Full 34,880-probe hardware-oracle corpus: 19,440 matches, 15,436
inconclusive, four remaining target mismatches across 0xded86/0xed133. 781
all-pass bodies, 1,397 inconclusive and two mismatching; CLI still exits 1.
No tracker promotion, handwritten-signature or game-equivalence claim.
Commands: python3 -m unittest tests.test_asm_wait tests.test_asm_inventory
tests.test_asm_to_c tests.test_asm_c_program tests.test_asm_c_x87
tests.test_asm_c_segments tests.test_asm_x87_oracle tests.test_asm_diff
tests.test_ghidra_ef2000; export inventory from
local/ghidra-x87-dispatch-20261114/body-inventory/body-ranges.json to
local/asm-wait-body-inventory-20261114; tools/asm_c_program.py --inventory
local/asm-wait-body-inventory-20261114 --output local/asm-c-wait-program-20261114;
tools/asm_c_differential.py --workspace local/ghidra-x87-dispatch-20261114
--translations local/asm-c-wait-program-20261114 --output
local/asm-c-wait-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386> --x87-oracle hardware.
Evidence: local/asm-wait-tests-20261114.log, local/asm-wait-regression-20261114.log,
local/asm-wait-body-inventory-20261114/manifest.json (including copied raw body
evidence), local/asm-c-wait-diff-20261114/report.json. Full remaining target,
x87/trap/runtime-service coverage, captured inputs and game integration stay
active. Original assets unchanged; all exports and generated source stay local/.

**2026-11-14 — Exact x87 comparisons and explicit unmasked condition-code profile.**
FCOMP is first unsupported in 213 bodies. Added all 39 canonical register,
memory, quiet-NaN, pop and FTST forms. Integer raw-80 ordering normalizes only
local significands/exponents, handles signed zero/infinity and detects NaN/
unsupported encodings without host floating-point arithmetic. Preserve physical
payloads and other tags; masked IE/stack invalid produces unordered and requested
pops, unmasked IE/DE suppresses pops. Prior sticky flags, SF and unaffected
status/flags remain visible. Memory forms widen exactly with existing helper,
carry narrow DE, and apply bounds/pending checks before effects.
Native probes exposed a CPU-specific observable: this host computes condition
codes on unmasked IE/DE even while suppressing pops. Do not hide those fields
or silently impose a host behavior. State ABI 3 appends x87_compare_profile:
bit 0/1 selects computed versus preserved C0/C2/C3 on unmasked IE/DE. Two fixed
independent probes measure the reference profile once before guest execution;
record measurements and chosen flags. Native and Unicorn select 3 here. C
configuration is initialized to that fixed profile and checked for mutation;
no guest result fitting or field masking. Policy 0 preserves codes; all four
policies are tested and unknown bits rejected. ABI 1/2 artifacts fail before
compilation; regeneration required. No old state layout is silently reused.
2,356,224 final generated-body/native cases pass under UBSan: all 39 forms,
all masks/defined PC/RC, rotated TOP, old condition/sticky bits, empty operands,
signed zeros/NaNs/infinities, unsupported encodings, denormals/pseudo-denormals
and negative-versus-negative ordering. Check all raw physical values, tags/
status/control, opcode, flags, memory/source spans and relocated metadata.
125 regression tests pass without skips (403 s); final strengthened two-test
comparison suite passes (106 s), including preserving/updating policies and
invalid-profile rejection. Old ABI rejection and CPU-config corruption tests
pass. All 39 forms compile warning-free with Watcom -we. Regeneration: 3,994
syntax-supported, 2,249 selected bodies, 299,929 bytes (+69/+16,732). Full corpus
outcomes are separate evidence. No tracker promotion or game-equivalence claim.
Commands: python3 -m unittest tests.test_asm_x87_compare plus preceding complete
123-test module list, with ASM_SOFTFLOAT_BUILD and ASM_MEMORY_ARITHMETIC_WATCOM
enabled (actual total 125 tests); final python3 -m unittest
tests.test_asm_x87_compare with ASM_COMPARE_EVIDENCE; tools/asm_c_program.py
--inventory local/asm-wait-body-inventory-20261114 --output
local/asm-c-x87-compare-program-20261114. Python 3.13.12/Unicorn 2.1.4. Evidence:
local/asm-x87-compare-full-tests-20261114.log,
local/asm-x87-compare-final-tests-20261114.log,
local/asm-x87-compare-final-evidence-20261114.json,
local/asm-x87-compare-watcom-20261114/compile.log and
local/asm-c-x87-compare-program-20261114/report.json. Native profile measurement
is original-oracle-only and shares no comparison implementation with C.

Final ABI 3 hardware-oracle run: 35,984 probes, 20,201 matching, 15,779
inconclusive and four existing target mismatches. Added 69 bodies contribute
761 matches, 343 inconclusive and no mismatches. All old probe states unchanged:
19,440 pass -> pass, 15,436 inconclusive -> inconclusive, four mismatch ->
mismatch. 813 bodies pass all probes; 1,434 are inconclusive and two mismatch
(0xded86/0xed133 targets 0x7f/0x80). CLI exits 1. All 2,249 body objects and
dispatcher compile warning-free with Watcom -we. Native reference executes
17,269 operations in 57 forms and rejects five interrupted measurements before
result inspection/C execution; seven unknown register forms remain inconclusive.
Final 2,356,224-case matrix rejects 73 interrupted samples and binds current
test/runtime/native sources. Profile 3 and its two independent measurements
are present in the final report; current original-oracle source and translation
manifest digests independently checked. No field masking, guest-result fitting,
state downgrade or tracker promotion.
Command: tools/asm_c_differential.py --workspace
local/ghidra-x87-dispatch-20261114 --translations
local/asm-c-x87-compare-program-20261114 --output
local/asm-c-x87-compare-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386> --x87-oracle hardware.
Evidence: local/asm-c-x87-compare-diff-20261114/report.json. Remaining integer
stores, other ISA/services/traps, reviewed dynamic targets, captured callers
and full-game integration remain active. No full game/ABI equivalence claim.

**2026-11-14 — Exact x87 integer stores; closure remains blocked.**
Add five FIST/FISTP memory encodings: 16/32-bit nonpop/pop and 64-bit pop.
Raw extended-to-integer conversion uses unsigned integer operations, handles
all rounding modes and signed range boundaries without signed overflow or
shift UB, and ignores PC. Preserve raw source payloads after pops. Masked
invalid/empty values store integer indefinite; unmasked invalid suppresses
store/pop, whereas unmasked precision commits both. Extended denormals do
not raise DE during integer conversion. Invalid suppresses precision and C1;
valid inexact conversions set PE and magnitude-increment C1. Full memory
bounds and pending delivery checks precede effects; FISTTP stays unsupported.
Independent fixed-opcode native oracle gains only these five allowlisted
forms. 307,200 generated/native cases pass under UBSan with all masks, defined
PC/RC, rotated TOP, empty operands, both signs, halfway/range cases and raw
special/unsupported encodings. Compare physical registers, tags/status/control,
opcode, relocated context, store canaries and integer state. Test pending-before-
bounds and rejected wrap/short spans. All five forms compile with Watcom -we,
zero warnings. Command: ASM_INTEGER_STORE_WCC=<wcc386>
ASM_INTEGER_STORE_EVIDENCE=local/asm-x87-integer-store-evidence-20261114.json
python3 -m unittest tests.test_asm_x87_integer_store.

Regenerate with tools/asm_c_program.py --workspace
local/ghidra-x87-dispatch-20261114 --inventory
local/asm-wait-body-inventory-20261114 --output
local/asm-c-x87-integer-store-program-20261114. Syntax support grows 3,994 ->
4,128 (+134), but direct closure remains 2,249 bodies/299,929 bytes. All
selected sources and dispatcher are byte-identical to the preceding program.
No full-corpus rerun or new probe total claimed; previous four callback-target
mismatches remain unresolved. The next high-fanout instruction is FRNDINT in
0x9d72c (45 immediate callers); interrupts/services and audited boundary gaps
also remain. Evidence: local/asm-x87-integer-store-tests-20261114.log,
local/asm-x87-integer-store-evidence-20261114.json,
local/asm-x87-integer-store-closure-evidence-20261114.json and generated report.
No function is promoted to game-verified.

The complete 19-module ASM inventory/translation/closure/segment/x87 regression
command passes: 77 tests, no skips, 423.983 s, Python 3.13.12/Unicorn 2.1.4,
with the validated SoftFloat build and both Watcom options enabled. Command
arguments/environment: local/asm-x87-integer-store-full-command-20261114.json;
output: local/asm-x87-integer-store-full-tests-20261114.log. The focused matrix
rejects 36 interrupted native measurements before result inspection/C execution.

**2026-11-14 — FRNDINT and 123 additional closed bodies.**
Implement the exact D9 FC FRNDINT form using unsigned integer operations on raw
extended precision. PC has no effect; even values beyond signed-integer range
retain integral payloads. Handle all RC, signed zeros, infinities, NaN quieting,
unsupported encodings, masked empty-stack indefinite and exception priority.
Nonzero exponent-zero operands include DE; unmasked IE/DE suppresses the write,
while unmasked PE commits. C1 records magnitude increment; preserve other
condition/sticky flags, TOP, data-pointer context and other physical registers.
Unknown encodings/prefixes/spelling and pending delivery fail explicitly. Native
oracle gains only the independent fixed-opcode FRNDINT form, no shared rounding
implementation. 498,688 generated/native cases pass under UBSan: 236,544 all-mask/
PC/RC, rotated TOP, both signs, empty/special/boundary/tie cases plus 262,144
all-exponent/both-sign/all-RC cases. Compare full raw stack/environment, complete
RAM, GPRs/flags and configuration; test pending, fuel and malformed encoding.
Watcom -we compiles without warnings. Final two-test suite passes in 22.205 s;
19 interrupted native samples rejected before results or C execution. An initial
run matched all native cases then failed an incorrect expected zero-fuel return
code in the test (expected 5; existing documented execution result is 1); the
corrected assertion and full matrix pass without changing runtime fuel behavior.
Command: ASM_ROUND_WCC=<wcc386>
ASM_ROUND_EVIDENCE=local/asm-x87-round-evidence-20261114.json
python3 -m unittest tests.test_asm_x87_round.

Regeneration: tools/asm_c_program.py --workspace
local/ghidra-x87-dispatch-20261114 --inventory
local/asm-wait-body-inventory-20261114 --output
local/asm-c-x87-round-program-20261114. Syntax support 4,129; direct closure
2,372 bodies/342,254 bytes (+123/+42,325 from integer-store stage). Helper
0x9d72c and its callers no longer stop at FRNDINT; FIST/P bodies from the last
step now join closure through it. Generated state only; no tracker promotion.
Full-corpus differential and game integration remain separate evidence gates.
Evidence: local/asm-x87-round-final-tests-20261114.log,
local/asm-x87-round-evidence-20261114.json and generated report.

The 20-module regression command passes 79 tests without skips (450.155 s),
Python 3.13.12/Unicorn 2.1.4, validated SoftFloat and all three Watcom test
options enabled. Exact command arguments/environment:
local/asm-x87-round-full-command-20261114.json; output:
local/asm-x87-round-full-tests-20261114.log.

Completed hardware-oracle corpus: 37,952 probes, 21,057 matches, 16,891
inconclusive and four existing callback-target mismatches. Added 123 bodies
contribute 856 matches and 1,112 inconclusive, with no new mismatches. All old
probe states are unchanged: 20,201 pass -> pass, 15,779 inconclusive ->
inconclusive, four mismatch -> mismatch. 845 bodies pass all probes, 1,525
are inconclusive and two mismatch (0xded86/0xed133 targets 0x7f/0x80). CLI exits
1; the full gate remains failing. Helper 0x9d72c passes all 16 probes. All 2,372
body objects and dispatcher compile warning-free with Watcom -we. The native
reference executes 95,870 operations in 65 forms, rejecting eight interrupted
measurements before inspecting results/C execution. No field masking, failure
reclassification or tracker promotion. The 2,249 preceding body sources remain
byte-identical; newly closed bodies contain 156 FISTP instructions.
Command: tools/asm_c_differential.py --workspace
local/ghidra-x87-dispatch-20261114 --translations
local/asm-c-x87-round-program-20261114 --output
local/asm-c-x87-round-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386> --x87-oracle hardware.
Evidence: local/asm-c-x87-round-diff-20261114/report.json,
local/asm-x87-round-corpus-summary-20261114.json and
local/asm-x87-round-closure-evidence-20261114.json. Current native source and
translation-manifest digests checked independently. Remaining instructions,
services/traps, audited boundaries, reviewed dynamic targets, captured inputs
and game integration remain required; reliable bulk translation is active.

**2026-11-14 — Exact raw-80 square root and 124 additional closed bodies.**
Dependency-only impact analysis selects FSQRT: one directly blocked body, 124
optimistic closure additions versus 119 for PUSHA/102 for INT. Estimates do not
assume that later instructions in a blocked body are supported. Implement D9 FA
using extF80M_sqrt from the validated, pinned SoftFloat build, with explicit
raw payloads, PC/RC, exception priority and mask effects. Preserve binary adapter
interfaces; extend only its hash-bound source and the original oracle's fixed
FSQRT allowlist. Save/restore rounding, precision, flags and tininess settings.
Reject reserved PC. Handle NaN quieting, unsupported/empty indefinite, signed
zero, infinities and DE; invalid suppresses DE, unmasked IE/DE suppresses writes,
unmasked PE commits. C1 records magnitude increment; other status, TOP, raw
physical values and DP context remain observable. Square root cannot produce
an extended range exception for a finite positive input; unexpected backend
range flags fail explicitly. No host-FP or original-code fallback.

634,368 generated/native cases pass with generated-code UBSan: 236,544 all-mask/
PC/RC, both-sign, rotated TOP, empty/raw-special cases; 393,216 all-exponent/
PC/RC cases; 4,608 exact-square and squared-rounding-midpoint neighbors. Compare
all raw values, tags/control/status/opcode, relocated context, GPRs/flags,
complete RAM and configuration; assert every SoftFloat global is restored after
each operation. Pending delivery, fuel, reserved PC and encoding rejection pass.
A preceding integer-only body tests late backend enabling in one TU. Adapter
and generated bodies compile warning-free with Watcom -we. Final two-test suite
passes in 30.082 s, rejecting 53 interrupted native samples before inspecting
results/C execution. Command: ASM_SOFTFLOAT_BUILD=<validated-build>
ASM_SQRT_WCC=<wcc386>
ASM_SQRT_EVIDENCE=local/asm-x87-sqrt-final-evidence-20261114.json
python3 -m unittest tests.test_asm_x87_sqrt.

Regeneration with tools/asm_c_program.py --workspace
local/ghidra-x87-dispatch-20261114 --inventory
local/asm-wait-body-inventory-20261114 --output
local/asm-c-x87-sqrt-program-20261114 yields 4,130 syntax-supported and 2,496
closed bodies/402,146 bytes (+124/+59,892), including 0xa4472 and its dependency
chains. All preceding 2,372 body sources are byte-identical. These remain
unverified at game level; full corpus results are a separate gate.
Evidence: local/asm-next-blocker-impact-20261114.json,
local/asm-x87-sqrt-final-tests-20261114.log,
local/asm-x87-sqrt-final-evidence-20261114.json,
local/asm-x87-sqrt-closure-evidence-20261114.json and generated report.

Full 21-module ASM regression: 81 tests pass without skips (493.593 s),
Python 3.13.12/Unicorn 2.1.4, validated SoftFloat and all four Watcom test
options enabled. Exact arguments/environment:
local/asm-x87-sqrt-full-command-20261114.json; output:
local/asm-x87-sqrt-full-tests-20261114.log.

Generic 0xa4472 probes (16) remain inconclusive: empty initial FP stack causes
original memory faults in its error path before FSQRT. Add a separate valid-ST0
experiment with the existing compare(initial_x87=...) API and full dispatcher,
without altering tools or corpus states. All 768 prepared cases match: all TOP/
PC/RC, zeros including -0, positive exact/irrational roots, true/pseudo-denormal
and largest finite input. Each original executes exactly one FSQRT. Compare
complete GPR/FP state, environment and 4 MiB RAM; no field masking or outcome
reclassification. This is additional prepared-input evidence, not captured
caller/game integration proof. Command: PYTHONPATH=<repo>:<Unicorn-site-packages>
<Python-3.13.12> local/check-sqrt-wrapper-20261114.py. Evidence:
local/asm-x87-sqrt-wrapper-20261114.log and
local/asm-x87-sqrt-wrapper-evidence-20261114.json; script/library hashes recorded.

Completed hardware-oracle corpus: 39,936 probes, 22,104 matches, 17,828
inconclusive and four unchanged callback-target mismatches. Added 124 bodies
contribute 1,047 matches and 937 inconclusive, no new mismatches. Every old
probe state is unchanged: 21,057 pass -> pass, 16,891 inconclusive ->
inconclusive, four mismatch -> mismatch. 884 bodies pass all probes, 1,610
are inconclusive and two mismatch (0xded86/0xed133 targets 0x7f/0x80). CLI exits
1; the full gate still fails. All 2,496 bodies and dispatcher compile warning-
free with Watcom -we. Native reference executes 117,161 operations in 68 forms,
including 453 FSQRT operations through callers; 18 interrupted samples rejected
before inspecting results or C execution. Generic wrapper probes remain 16
inconclusive; the separate 768 valid-ST0 wrapper cases pass and bind the same
compiled library. No masked fields, outcome reclassification or tracker promotion.
Command: tools/asm_c_differential.py --workspace
local/ghidra-x87-dispatch-20261114 --translations
local/asm-c-x87-sqrt-program-20261114 --output
local/asm-c-x87-sqrt-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386> --x87-oracle hardware.
Evidence: local/asm-c-x87-sqrt-diff-20261114/report.json and
local/asm-x87-sqrt-corpus-summary-20261114.json. Native tool, translation manifest
and supplementary wrapper script/library digests checked independently. Stack
operations, interrupt/services, remaining x87 operations, audited flow gaps,
reviewed dynamic targets, captured callers and full-game integration remain;
reliable bulk translation remains active.

**2026-11-14 — PUSHA/POPA and newly exposed trap boundaries.**
Add exact unprefixed 32-bit 60/61 register saves/restores from the corpus.
Preserve original ESP in the frame, skip/discard its POPA slot without reading,
leave flags/FP state unchanged and commit ESP only on completion. Match the
flat Unicorn integer oracle's partial faults: ascending-address/byte writes for
PUSHA and earlier restored registers for POPA. Native fault ordering and trap
delivery are not proven by this emulator observation; original faults remain
inconclusive. Other widths/prefixes/encoding mismatches remain unsupported.
1,272 generated/Unicorn 2.1.4 cases pass under UBSan: 1,122 complete, 150 faulting.
Cover randomized registers/RAM/flags, unaligned stacks, both RAM edges and wrap;
compare full RAM, GPRs/flags/PC plus every remaining state byte. Read hooks prove
POPA does not read the discarded ESP slot. Both forms compile with Watcom -we,
zero warnings. Command: PYTHONPATH=<Unicorn-site-packages>
ASM_STACK_ALL_WCC=<wcc386>
ASM_STACK_ALL_EVIDENCE=local/asm-stack-all-evidence-20261114.json
<Python-3.13.12> -m unittest tests.test_asm_stack_all.

Regenerate: tools/asm_c_program.py --workspace
local/ghidra-x87-dispatch-20261114 --inventory
local/asm-wait-body-inventory-20261114 --output
local/asm-c-stack-all-program-20261114. Actual syntax support 4,134, selected
2,499 bodies/402,520 bytes (+3/+374): 0xc46bc, 0xc5c24 and 0xc5cd3. All preceding
2,496 body sources remain byte-identical. The 119-body dependency-only estimate
for PUSHA was optimistic, not a support proof. Six newly exposed roots stop at
INT, 0xc4167 depends on 0xc409c, and 0xfa02c reports apparent post-INT3 gaps.
Perform read-only audit-body-holes 0xfa02c and audit-flow-gaps using the private
headless wrapper on the disposable project. One undefined six-byte hole, no
references, no ordinary flow-gap candidate; both missing next-PCs follow INT3
at 0xfa0b2/0xfa167. Do not enlarge the function into those bytes without trap/
continuation evidence. No project or tracker changes. Evidence:
local/asm-stack-all-tests-20261114.log,
local/asm-stack-all-evidence-20261114.json,
local/asm-stack-all-closure-evidence-20261114.json,
local/asm-stack-all-fa02c-holes-20261114.tsv,
local/asm-stack-all-flow-gaps-20261114.tsv and generated program report.
Full corpus outcomes remain separate evidence; no game-verification promotion.

Affected inventory/translation/program/segment/x87-state/WAIT/stack regression
passes: 51 tests without skips (132.424 s), Python 3.13.12/Unicorn 2.1.4. Numeric
x87 algorithms/backends are unchanged; their preceding matrix evidence remains
separate. Exact command/environment:
local/asm-stack-all-regression-command-20261114.json; output:
local/asm-stack-all-regression-20261114.log.

Static interrupt frontier now covers 56 first-blocked bodies: 99 INT 21h,
18 INT 31h, three INT 10h, three INT 2Fh and 16 INT 6 instructions. Counts are
instructions within those bodies, not service-ID proofs. Keep DOS/DPMI/BIOS
services separate from deliberate trap semantics; no blanket INT translation or
service stubs were added. Evidence:
local/asm-stack-all-interrupt-frontier-20261114.json.

Next stack frontier: 0xc409c (dependency of newly unblocked PUSHA body 0xc4167)
uses 16-bit memory PUSH/POP at 0xc40c7/0xc40f6. Those scalar forms remain
unsupported; do not infer that all PUSH/POP variants were completed here.

Completed hardware-oracle corpus: 39,984 probes, 22,152 matches, 17,828
inconclusive and four unchanged callback-target mismatches. All three new
bodies pass all 16 probes (48 new matches). Every preceding outcome is unchanged:
22,104 pass -> pass, 17,828 inconclusive -> inconclusive, four mismatch ->
mismatch. 887 bodies pass all probes, 1,610 are inconclusive and two mismatch
(0xded86/0xed133 targets 0x7f/0x80). CLI exits 1; the full gate remains failing.
All 2,499 bodies and dispatcher compile warning-free with Watcom -we. Native
x87 reference coverage remains 117,161 operations/68 forms; seven interrupted
native samples rejected before result inspection/C execution. No relaxed fields,
original-fault promotion, game-verification claims or tracker changes.
Command: tools/asm_c_differential.py --workspace
local/ghidra-x87-dispatch-20261114 --translations
local/asm-c-stack-all-program-20261114 --output
local/asm-c-stack-all-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386> --x87-oracle hardware.
Evidence: local/asm-c-stack-all-diff-20261114/report.json and
local/asm-stack-all-corpus-summary-20261114.json. Current native-tool/translation-
manifest digests independently checked. Scalar memory stack forms, services/traps,
remaining ISA, audited boundaries, reviewed targets, captured callers and game
integration remain required; reliable bulk translation remains active.

**2026-11-14 — Scalar word/dword stack operands and 28 closed bodies.**
Add 16/32-bit register and memory PUSH/POP, signed/full immediate PUSH including
PUSHW, strict operand-size/form and encoded immediate checks. Keep 32-bit address
parsing; validate short-register/ModRM/SIB/displacement lengths and reject unknown
prefix/width/group/register/immediate mismatches. Preserve original-ESP source
EA on PUSH and post-increment-ESP destination EA on memory POP without committing
ESP on a destination fault. POP SP merges into post-increment ESP, preserving
upper-half carry. Cache the popped/source value before overlapping stores.
Use checked byte stores for partial flat-Unicorn writes, including existing
32-bit PUSH. Preserve GPR/flags/FP state except explicit stack/destination effects.
Native fault delivery is not proven; original-fault corpus probes stay inconclusive.

12,649 generated/Unicorn 2.1.4 cases pass under UBSan, 91 forms: 10,040 complete,
2,609 faulting. Include all GPR/SP/ESP aliases, both widths, memory/SIB/FS/absolute/
ESP operands, overlapping reads/writes, signed/full immediates and alternate
encodings. Exercise RAM boundaries, unaligned accesses, 32-bit wrap and 64 KiB
carry. Compare complete RAM/GPR/flags/PC and every remaining state byte. Both
normal and rejected encodings tested; all forms compile warning-free with
Watcom -we. Focused two-test suite: 11.341 s. Command:
PYTHONPATH=<Unicorn-site-packages> ASM_SCALAR_STACK_WCC=<wcc386>
ASM_SCALAR_STACK_EVIDENCE=local/asm-scalar-stack-evidence-20261114.json
<Python-3.13.12> -m unittest tests.test_asm_stack_scalar.

Regenerate tools/asm_c_program.py --workspace
local/ghidra-x87-dispatch-20261114 --inventory
local/asm-wait-body-inventory-20261114 --output
local/asm-c-scalar-stack-program-20261114: 4,142 syntax-supported and 2,527
closed bodies/410,779 bytes (+28/+8,259). 0xc409c joins closure and no first-
blocker 'unsupported stack operand' remains. Scalar PUSH changes 2,024 preceding
body sources; require actual state comparison, not a source-identity inference.
Full corpus results are separate. Evidence:
local/asm-scalar-stack-tests-20261114.log,
local/asm-scalar-stack-evidence-20261114.json,
local/asm-scalar-stack-closure-evidence-20261114.json and generated report.
No tracker or game-verification promotion.

Scalar-stack full-program gate exposed a bulk-size limit before differential
execution: all 2,527 body objects compile, but Watcom E1145 fails the monolithic
dispatcher at line 156720 within its constant byte initializer (128,988 PC
entries; 410,779 original-code bytes). This is not an instruction mismatch or
permission blocker. Split registry data into bounded translation units, preserving
all sorted PC entries, byte guards, target rejection and shared fuel/state. Bind
new source/header artifacts into the manifest and require their hashes before
compilation. This is necessary to keep the expanding bulk program reviewable and
compilable; do not suppress Watcom or shrink the selected set to pass the gate.
Evidence: local/asm-c-scalar-stack-diff-20261114/objects/ef_lift_program.log.

Registry remedy implemented: pc-shards-v1 splits the same 128,988 locations into
63 separate table modules (maximum 2,048 entries), plus a hash-bound common header
and compact dispatcher. Two-level binary lookup preserves exact PC membership,
per-instruction guarded bytes, stop PC, shared fuel and all selected functions.
Body sources/runtime are byte-identical to the failed monolithic scalar-stack
output. All 2,527 bodies, dispatcher and 63 tables now compile with Watcom -we.
Manifest support inventory/layout/size and every header/table digest are checked
before output/compilation; object hashes are recorded. Existing monolithic
artifacts remain supported. Two-entry-table tests cover boundary calls/return/
resume, unknown PCs on both sides and between tables, changed opcode bytes,
corrupt/missing/duplicate/malformed support and invalid shard sizes. The final
boundary/configuration test passes. Initial old rejection assertions for PUSH AX
and memory POP were updated to unsupported PUSHF/POPF; scalar semantics did not
change to make them pass. Evidence:
local/asm-scalar-stack-shards-focused-20261114.log (19 tests, 116.459 s),
local/asm-scalar-stack-shards-final-boundary-20261114.log,
local/asm-scalar-stack-shards-generation-evidence-20261114.json and
local/asm-c-scalar-stack-shards-program-20261114/report.json. Final regression and
full corpus are separate gates, still required; no game-verification promotion.

Final affected eight-module regression passes 53 tests without skips
(150.562 s), plus the final boundary/configuration test with invalid-size cases.
Command modules/environment: local/asm-scalar-stack-regression-command-20261114.json;
final output: local/asm-scalar-stack-shards-regression-20261114.log. Numeric x87
algorithms/backends are unchanged; their instruction matrices remain separate
prior evidence. Current full-corpus execution still requires its own result.

Independent cross-artifact comparison also confirms all 128,988 PC/function/
offset/guard-length entries and all 410,779 guarded code bytes are unchanged
between the failed monolithic output and the partitioned registry (offsets
rebased per table). Every table descriptor matches its first PC, count and
symbols. This checks the complete registry beyond sampled execution coverage.
Evidence: local/check-registry-partition-20261114.py and
local/asm-scalar-stack-registry-equivalence-20261114.json, binding both manifests
and the checking script. Original bytes remain only in ignored local artifacts.

Completed partitioned hardware-oracle corpus: 40,432 probes, 22,290 matches,
18,136 inconclusive and six mismatches. Added 28 bodies contribute 138 matches,
308 inconclusive and two uncovered-target failures in 0xc6b80. All preceding
outcomes are unchanged: 22,152 pass -> pass, 17,828 inconclusive -> inconclusive,
four mismatch -> mismatch. 893 bodies pass all probes, 1,631 are inconclusive
and three mismatch. 0xc409c passes all 16 probes. Native x87 coverage remains
117,161 operations/68 forms; five interrupted samples rejected before inspecting
results/C execution. All 2,527 body objects, dispatcher and 63 table modules
compile warning-free with Watcom -we; all table object digests are bound.

The two new failures stop at 0xc5e65 after original completion. Current inventory
has no owner there; independent disassembly shows a one-byte RET, and dispatch
table 0x15b270 points to it from the indexed jump at 0xc6cdf. This is a concrete
observed code target, unlike the four previous copyright-header targets 0x7f/0x80.
Keep both failures, do not downgrade them or infer target coverage from direct
closure. Recover/audit this legitimate leaf boundary as its own change before
claiming that the expanded program passes. The full gate exits 1. No masked
fields, original-fault promotion or game-verification claims.
Command: tools/asm_c_differential.py --workspace
local/ghidra-x87-dispatch-20261114 --translations
local/asm-c-scalar-stack-shards-program-20261114 --output
local/asm-c-scalar-stack-shards-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386> --x87-oracle hardware.
Evidence: local/asm-c-scalar-stack-shards-diff-20261114/report.json and
local/asm-scalar-stack-shards-corpus-summary-20261114.json. Current native-tool,
translation-manifest and all support-source/object digests independently checked.
Remaining services/traps, reviewed targets, captured callers and game integration
keep reliable bulk translation active.

**2026-11-14 — Recover the observed RET-only dispatch target.**
The scalar-stack run's two added failures in 0xc6b80 stop at 0xc5e65. Review
independent disassembly and the table at 0x15b270: slot zero points to the
single-byte RET, immediately after INT3 at 0xc5e64. Extend the existing reviewed-
leaf recovery with only [0xc5e65,0xc5e66). Guard pinned payload/language, indexed
JMP at 0xc6cdf owned by 0xc6b80, EAX/table-literal operands, slot-zero pointer,
preceding one-byte INT3, no conflicting owner/data, and exact RET extent. No
padding or header data is promoted; the trap remains outside compiled coverage.

Copy the disposable project to local/ghidra-stack-return-20261114. Run
recover-observed-leaves in report mode (proposed 0 -> 1 byte), apply, then report
again (1 -> 1 already_recovered), using the private headless wrapper. Other
three reviewed leaves remain unchanged. Fresh inventory has exactly 4,280
functions: all 4,279 previous raw ranges/instruction spans are identical; only
the singleton RET body is added. Add a tracker row as unknown/asm/unverified,
without native ABI or game-verification claims. Export normalized ASM for all
4,280 owned bodies; 13 tracker-only entries remain excluded as before.

47 tooling tests pass (0.270 s), including rejection of a fresh old three-leaf
report that omits the new target. Regeneration yields 4,143 syntax-supported,
2,528 selected bodies/410,780 bytes. All 2,527 preceding entries/body sources
and runtime are identical; one RET entry is added. Full Watcom/native-oracle
corpus is a separate pending gate. Commands: tools/ghidra_ef2000.py
recover-observed-leaves --workspace local/ghidra-stack-return-20261114
--analyze-headless local/ghidra-headless-private-20261114 [--apply] --output
<fresh-report>; inventory to local/asm-dispatch-return-ghidra-20261114;
tools/asm_c_program.py --workspace local/ghidra-stack-return-20261114
--inventory local/asm-dispatch-return-body-inventory-20261114 --output
local/asm-c-dispatch-return-program-20261114.
Evidence: local/asm-dispatch-return-{audit,apply,idempotent}-20261114.tsv,
local/asm-dispatch-return-final-tool-tests-20261114.log,
local/asm-dispatch-return-inventory-evidence-20261114.json and
local/asm-dispatch-return-program-evidence-20261114.json. Recovery/audit state
is not a function-verification promotion.

Final recovered-target hardware-oracle corpus: 40,448 probes, 22,308 matches,
18,136 inconclusive and four original callback-target mismatches. The new RET
leaf passes all 16 probes. Both previous 0xc6b80 target failures now match full
state/RAM; its 14 original-fault probes remain inconclusive. All other preceding
outcomes are unchanged: 22,290 pass -> pass, 18,136 inconclusive -> inconclusive,
two mismatch -> pass, four mismatch -> mismatch. 894 bodies pass every probe,
1,632 are inconclusive and two mismatch (0xded86/0xed133 targets 0x7f/0x80).
CLI exits 1; the full gate remains failing. All 2,528 bodies, dispatcher and 63
table units compile warning-free with Watcom -we. Native x87 reference coverage
is 117,161 operations/68 forms; 15 interrupted samples rejected before inspecting
results/C execution. No failures were downgraded or fields masked.
Command: tools/asm_c_differential.py --workspace
local/ghidra-stack-return-20261114 --translations
local/asm-c-dispatch-return-program-20261114 --output
local/asm-c-dispatch-return-diff-20261114 --softfloat-build
local/asm-softfloat-verified-build-20261113 --watcom <wcc386> --x87-oracle hardware.
Evidence: local/asm-c-dispatch-return-diff-20261114/report.json and
local/asm-dispatch-return-corpus-summary-20261114.json. Native-tool, translation-
manifest and support object digests independently checked. This is prepared-
state boundary proof, not native ABI/captured-game/replay verification. The new
tracker row remains unknown/asm/unverified. Services/traps, other ISA, reviewed
dynamic targets, captured callers and full-game integration remain active.

**2026-11-14 — Capture the context needed for real ASM-to-C inputs.**
Bulk prepared-state verification remains 2,528 selected bodies with 22,308
matching, 18,136 inconclusive and four uncovered callback-header failures.
Existing EFCAP001 snapshots omit x87 and CS/FS/GS, so they cannot safely seed
that model. Add opt-in --capture-x87/EFCAP002 while retaining default EFCAP001
output and legacy decoding. Capture all physical raw-80 slots (empty payloads
included), eight environment fields, every selector, producer version, GDTR/LDTR,
selected raw descriptors and bounded addressable table windows. Missing table
entries/windows are explicit; table memory is not a hidden-cache snapshot.
Serialize explicit little-endian fields, validate sizes/widths/descriptor-table
consistency, preserve source-image binding and refuse overwrites. The old ABI
oracle rejects EFCAP002 rather than silently dropping context. Captured replay
into the ASM model still needs segment/profile restoration; do not claim it done.

Synthetic native tests verify physical FP0..FP7 versus logical ST0 at TOP=5,
retain nonzero empty payloads, preserve accessed descriptor bits and distinguish
cached selectors from table entries made unavailable by a reduced limit.
Cross-language fixture checks the Python decoder and malformed context rejection.
Capture format assertions now run in Release builds as explicit checks.
Warning-free build: cmake --build local/codex-build --target
 ef2000-call-capture-test ef2000-call-capture-unicorn-test ef2000-unicorn-cpp -j2.
14 Python tests pass without skips (0.344 s): tests.test_call_capture and
tests.test_func_differential with EF_CAPTURE_UNICORN_TEST pointing to the built
fixture and Python 3.13.12/Unicorn 2.1.4. Six native CTests pass (0.32 s): the two
capture tests plus patch-hits, guest-hash, BDA and clock runner regressions.

Real original-run capture at 0x604 under private Xvfb, guest DOS/VGA clocks,
sound off and a disposable drive overlay: EFCAP002 records 64 MiB RAM, selectors
CS=0x2f and DS/ES/SS=7, and 48/256-byte GDT/LDT windows. A separate default-v1
run produces identical 13 registers and byte-identical RAM, and both terminate
at the same five-million-instruction state (55 interrupts, 20 ticks, IP=0xfef56,
AX=0xf62f). No original assets modified. The first candidate executable was a
previous modified local fixture; SHA checking rejected it before capture. The
actual runs use the pinned original ../EF2000/c/EF2000/_EF2000.EXE.
Evidence: local/capture-x87-{commit-tests,commit-ctest,pinned}-20261114.log,
local/capture-legacy-pinned-20261114.log and
local/capture-x87-game-evidence-20261114.json. Proprietary snapshots remain ignored
under local/. This supplies richer input evidence, not completed capture replay,
ABI validation or full-game verification.

### 2026-11-14 — unblock hybrid packaging inventory checks

The requested ef2000-hybrid Nix run fails before launch: 57 inventory subtests
for batches 090–097 require the historical drafted state, although the tracker
correctly demoted host-adapter bodies to blocked. Keep sizes, names, modules,
kinds and signature assertions; permit tracked progress states and require a
concrete Blocked: note for blocked entries. Do not promote these bodies to make
the build pass. Python unittest discovery restricted to test_decomp_batch_09*_payload.py
passes all eight asset-free inventory tests; eight original-byte suites skip
without EF2000_PAYLOAD. Runtime repair and complete Nix validation remain pending.

### 2026-11-14 — enforce hybrid ABI exclusions

Hybrid repair found that hybrid_link recorded unsupported signatures in
unwrapped_abi, but retained them in eligible_entries: both entry jumps and
external references still selected the unwrapped C body. Remove skipped entries
from resolution and redirects, and include the reason in exclusions. Synthetic
two-module tests check missing signatures, callee-clean stack arguments, multiple
return registers and unsupported preserved registers: the callee entry remains
original and a supported compiled caller's relocated CALL targets that original
entry. 22 hybrid/app/OMF tests pass (one compiler-dependent skip).

A pinned full-object relink drops 200 unsupported redirects (2800 to 2600),
with 342/382 modules retained. This is an ABI correctness fix, not a working-game
claim: the bounded private-Xvfb replay still faults at 0x10d901 in heap growth.
The Nix package now builds after the inventory-test fix; its native64 launcher
reproduces the same fault. Startup bisection continues separately.
Evidence: local/hybrid-abi-filter-tests-20261114.log,
local/hybrid-abi-filter-fix-20261114/ and local/hybrid-launch-repair-02-20261114/.

### 2026-11-14 — index ABI aliases during hybrid linking

Pre-index ABI alias spellings once rather than rescanning and normalizing every
alias for every relocation. Full 382-object linking drops from roughly 15–20 s
to under one second. The complete patch is byte-identical before/after indexing
(SHA-256 19900f34dee7f35151f54eb0107673a5421d1ff02e2651ca9f33784b94c8f866),
using the same startup exclusions and pinned image. Hybrid alias/exclusion tests
pass. This makes the launcher and successive startup bisections practical;
no generated-code or behavior change is intended. Evidence stays under
local/hybrid-interrupt-fix-20261114/{current,indexed}.efpatch.

### 2026-11-14 — correct archive prefix matching

Startup bisection isolates 0xeb520 as sufficient for a false missing-dataset
exit. Its prefix loop used break-on-mismatch; original 0xeb5d6/0xeb5d8 continues
on mismatch and stops on a match. Correct the branch. Four original captured
COLTAB calls plus two input variants replacing only the pathname with CHECK.CD
match return values and preserved registers under the flat-segment ABI oracle.
The old compiled draft fails the CHECK.CD variant: original EAX=0x4e0e04, C=0.
This checks ABI outputs, not all RAM (compared_ram_bytes=0). Capture/patch data
remain local. The module compiles with two existing unrelated unused-variable
warnings (e4 and p); no new warning.

Captured-call replay decoded arena patches a second time without the 64 MiB
address limit already used when applying them, rejecting valid hybrid patches.
Use the same limit for both decodes. 14 capture/differential tests pass with
Unicorn (one optional C++ fixture skip); real captured hybrid replay now runs.
Evidence: local/hybrid-archive-corrected-20261114/{diff-flat,diff-check,diff-old-check}.log.
The full hybrid now passes the data-set check but still has a configuration
callback fault, separately bisected to scanf integer conversion at 0x108ddb.
No menu/cockpit completion claim yet.

### 2026-11-14 — native port I/O from the hybrid arena

The native backend faults at relocated guest_inp_x (0x302fdff, IN AL,DX with
DX=0x64), although Unicorn passes that point: native I/O address guards accept
only original code. Extend existing port and CLI/STI handling to the explicitly
reserved arena. Keep the supported devices/opcodes unchanged. A synthetic guest
regression copies a helper into the arena, checks port 0x61 write/read, keyboard
status, disconnected gameport, CLI/STI and a zero DOS exit. The same helper in
mapped RAM outside that arena must fault. Native test passes without game data.
The actual native run now reaches video mode 0x13, then reproduces the later
Unicorn GUI fault; that failure is separately bisected to 0xc3364.

Read diagnostic transcripts with UTF-8 replacement for raw DOS console bytes,
so a German error message cannot prevent report.json from being written.
Evidence: local/hybrid-native-port-fix-20261114/; validation:
xvfb-run -a python3 -m unittest tests.test_native_hybrid_io -v and the two
asset-free tests in tests.test_startup_runtime.TranscriptTests.

### 2026-11-14 — preserve unsafe startup and callback fragments

Keep five entries original in the broad hybrid: 0x222 is code-inspected by
X-32 GETDS validation; redirecting it fails immediately. Entry 0x235 feeds
comparison flags into already-excluded tail 0x23b; its C wrapper cannot supply
that interface. Fragment 0x9c4b2 restores registers/segments and transfers into
an interrupt stub; the draft only returns the stub address. Preserving it plus
the ABI exclusions removes the heap fault and exposes archive lookup.

After correcting archive prefix matching, startup bisection isolates 0x108ddb
as sufficient for an invalid callback fetch during configuration parsing, then
0xc3364 as sufficient for stack corruption during GUI asset loading. Demote
these two and 0x9c4b2 to compiles-bad and record exclusions with evidence.
The image retains 2597 compiled redirects across 342/382 modules and reaches
the 640x400 DID splash; a later stop remains, so neither M1 nor M2 is complete.
The 200M-instruction bisect-06 did not reach that later stop and reduced timing
differences instead; its candidate set is not a culprit or exclusion list.
A 300M-instruction abnormal-stop bisect is running instead.
Evidence: local/hybrid-bisect-startup-20261114/bisect-{03,04,05}/report.json,
local/hybrid-gui-fix-20261114/ and local/hybrid-native-port-fix-20261114/.

### 2026-11-14 — restore the hand-written hybrid main menu

The 300M-instruction startup bisector isolates pattern loader 0xe4010 as
sufficient for a jump into pathname bytes at 0x9cfd8; exclude and demote it.
The next bisector isolates bitstream refill 0xfa332 as sufficient for an
invalid write through 0xffffffff. Disassembly shows register outputs ESI/EDX
and carry, not the C draft's pointer inputs. Correct its signature and those
of its coupled decode fragments 0xfa1d4/0xfa200: the latter return their
symbols in ECX/EBX while updating AL/ESI/EDX. Their pointer-state C drafts
are not valid guest-entry replacements. Mark the three compiles-bad and
preserve this fragment cluster pending register/flag adapters.

The full hand-written profile retains 2593 C redirects, 12965 entry bytes,
a 576198-byte arena and 342/382 linked modules. A 500M-instruction Unicorn
run reaches the main menu without a guest fault (2000 ticks, 1937 IRQ0s,
198 lowercase replacement hits). Native64 also reaches the same visible
menu and stops normally at its 15-second diagnostic limit, with no faults.
Both framebuffers were inspected. Evidence: local/hybrid-bitstream-cluster-fix-20261114/
and local/hybrid-native-menu-fix-20261114/. This is the hand-written launcher
profile, not proof of the combined mechanical milestone or full-game equivalence.

Quick Fight remains broken: Return at tick 2100 stops at 0x17f7ca/tick 2118
with unsupported port OUT c88d. Its bounded replay is recorded in
local/hybrid-quickfight-fix-20261114.log. The follow-up bisect-09 was stopped
at the owner's request to finish and commit; it has no established culprit.
Do not claim the launcher is fully usable or M2 complete. The preceding Nix
build (708ac765) passes its checks but reproduces the pre-pattern-fix fault;
the final source revision has not yet been rebuilt through Nix.

### 2026-11-14 — repair default-path call arity and Quick Fight setup

Four callers used guest_fn_9c980_sfx(buffer, suffix), although original 0x9c980
reads only EAX and preserves EDX. The generated one-register ABI pragma spills
the extra argument to the stack; the original callee does not remove it.
The old 0x7c4fc object contains PUSH suffix followed by CALL with no cleanup.
Replace all four uses with build_default_path_9c980(buffer) and remove the
misleading alias declaration. Keep explicit suffix concatenation; add the
missing strcat of 0x18a86c in load_effect_list_8bad4, matching 0x8bae9/0x8baeb.
Original callers at 0x80058 and 0x81230 likewise concatenate their suffix after
the path call. Correct 0x7c4fc's preservation record: EDX is scratch, not saved;
callers 4aa0@4aee and 4cf4@4d38 discard it. Three affected modules compile with
zero warnings. Sound-bank/effect-list paths are assembly-audited, not dynamically
verified here.

Full hand-written relink retains all 2593 redirects across 342/382 modules.
Private-Xvfb replay with Return at ticks 2100/2102 now reaches Quick Fight setup
and runs to 700M instructions without a guest fault: 2800 ticks, 2735 IRQ0s,
214 lowercase replacement hits. Repeated after the preservation correction,
with the same stop state/counters; framebuffer inspected. This corrects the
previous bad jump at 0x17f7ca without disabling further C replacements.
The obsolete pre-fix bisect-10 was stopped after the concrete call-site repair;
its partial reductions are not an established culprit list.

25 compiler-wrapper/hybrid/app/OMF tests pass without skips. Evidence:
local/hybrid-path-arity-fix-20261114/{compile45,compile46,compile-misc,
compile45-corrected-abi,tests,run,run-corrected-abi}.log.
The committed e11001c3 Nix build passes 504 Python tests (66 expected skips)
and reaches the native menu with no faults at its 15-second limit; the new
path-call correction has not yet been rebuilt through Nix.

A longer two-Return run reaches the simulation-loading screen but does not
reach the cockpit: at 1.5B instructions IP=0, AX=1, and the four flight-entry
canaries (0x58d24/0x7a428/0x7eea0/0x810d0) have zero hits. No M2 or full-game
equivalence claim. Evidence: local/hybrid-path-cockpit-20261114.log and its
inspected framebuffer. Next investigate this loading control-flow divergence.

### 2026-11-14 — reject false instruction-limit passes at EIP zero

The simulation-loading stall exposed an oracle bug: uc_emu_start uses end=0.
After a guest return/jump reaches zero, Unicorn returns UC_ERR_OK without
running further instructions, but the runner repeatedly charges whole slices
and reports instruction_limit. Detect this unexpected end-address stop in both
instruction-counted and real-time modes before delivering further timer ticks.
Report a bisectable guest-execution error with SP, stack words, tick/IRQ counters
and replacement hits. A requested --capture-calls 0 now records that first stop
through the existing snapshot writer. Do not treat budget exhaustion as progress
when guest execution has stopped. Synthetic zero-return self-tests require this
error in both timing modes; existing clock, retrace, BDA, patch-hit and hash
regressions remain passing (eight native CTests in the selected group).
All 19 replay tests pass, including a parser regression checking that this
failure keeps canary counts. Commands: xvfb-run -a ctest --test-dir
local/codex-build --output-on-failure -R
'unicorn-cpp-(zero-return|patch-hits|guest-hash|bda|clock|retrace)';
python3 -m unittest tests.test_replay_suite -v.

Actual two-Return replay with the corrected path-call image now stops at the
first zero transfer: ticks=3221, IRQ0=3154, SP=0x3ffe6e4, AX=1; top stack words
are 0x93,0,0x92,0,0x91,0,0x90,0. A complete guest-RAM snapshot is retained under
local/hybrid-null-stop-20261114/calls/. This supersedes the earlier misleading
1.5B-instruction stop at tick 6000. Next locate the preceding control transfer;
the current stack already looks overwritten, so a short block-history diagnostic
may be needed. Evidence: local/hybrid-null-stop-20261114/run.log and
local/hybrid-null-stop-{selftest-build,ctest}-20261114.log.
The first diagnostic launch did not start because the redirected output
directory was absent; after creating it, the actual replay completed with the
reported failure and snapshot. No game assets changed.

### 2026-11-14 — bounded basic-block history identifies loading corruption

Add opt-in --trace-block-tail: keep only the last 64 basic-block addresses
and append them to an unexpected zero-address stop. This avoids an unbounded
instruction log while retaining the code immediately before a corrupt return.
Synthetic 80-block jump chains verify ring wrap, chronological order and exact
capacity in instruction-counted and real-time modes. Four zero-stop CTests and
19 Python replay tests pass. Default execution installs no history hook.

The same two-Return hybrid replay now identifies build_ramp_table_e7ea4 as the
last returning body, repeatedly calling original truncate_9d72c and __CHP.
The captured register/RAM snapshot is byte-identical with and without tracing
(SHA-256 0ce2e0456cb8502c780d4d743365398f36ef7f9f36dc9d101fc7c06041594679).
Both stop at tick 3221 with the same corrupted stack, confirming that the
recorder did not change the guest state in this case. Snapshot decoding was
validated against the reconstructed, SHA-bound patched input image.

The actual caller pushes an eight-byte double each iteration, but original
0x9d72c consumes/returns ST0 and performs plain RET. The canonical declaration
missing an x87 pragma and its EAX:double signature are incorrect (the alternate
guest_fn_9d72c_x alias already has an x87 pragma). Next correct that ABI, keep
the unverified ceil/floor replacement excluded, and replay the full image.
Evidence: local/hybrid-block-tail-20261114/{run,annotated}.log, its validated
stopped-ram.bin, and local/hybrid-block-tail-{build,ctest,parser-tests}-20261114.log.
Proprietary state and diagnostic code dumps remain ignored.

### 2026-11-14 — correct canonical x87 truncation call ABI

Declare truncate_9d72c with Watcom parm/value [8087] and preserved GPRs;
correct its original signature from EAX:double to ST0 input/output. Disassembly
at 0x9d72c saves/restores the x87 control word around FRNDINT and uses plain RET.
Both e7ea4@e7f24 and f80@14eb pass ST0 and pop the result themselves. The
alternate alias already had an x87 pragma; canonical callers did not. Keep
the ceil/floor C implementation explicitly excluded pending raw x87 precision
and exception checks. Recompile both canonical-caller modules with zero warnings.
Correct the ramp builder's preservation record: EDX is scratch, not saved.

Replace its dummy spec with seven real cases: full/interior/single-interval
ramps, equal/reversed endpoints, and invalid bounds. Isolated original-vs-C
comparisons pass all seven, comparing 256 ramp bytes for each valid case,
return value, preserved registers and stack cleanup. Synthetic A5 guards detect
out-of-range writes. The old call ABI faults on the full-range negative control
(invalid return fetch at 0xe898000a), so these cases detect the regression.
Commands: tools/hybrid_link.py --only-address 0xe7ea4 with the recompiled
objects; tools/func_differential.py 0xe7ea4 --payload
local/ghidra-stack-return-20261114/payload.bin --patch
local/hybrid-truncate-abi-fix-20261114/only-ramp.efpatch --spec
decomp/tests/0x000e7ea4.json. Evidence: local/hybrid-truncate-abi-fix-20261114/
{diff-ramp,diff-ramp-old-control,compile-misc,compile-float,tests}.log.

Full 2593-redirect replay no longer returns to zero from the ramp builder. It
now exposes a later invalid callback fetch at 0xfab33ce9, tick 3221, through
release_ref_108ffa and guest_callptr_eax_ret_x. Cockpit-entry canaries remain
unreached. Investigate the callback declaration/definition clobber contract
next; no M2 completion or full raw-x87 equivalence claim.

### 2026-11-14 — expose EAX callback-helper clobbers to callers

Move the public Watcom pragmas for guest_callptr_eax_ret_x and
guest_callptr_eax_x from guest_callptr1.c to ef_abi.h. Both shims put the
callback address in ECX; separately compiled callers previously assumed ECX
survived. An asset-free three-register caller kept its third argument in ECX
across the helper and returned 12306 instead of 41. The same fixture, compiled
with the shared declaration and executed with the actual helper object, returns
41 with balanced stack. The old object fails this test as a negative control.

Validation command: EF2000_WCC386=<pinned-wcc386> PYTHONPATH=<unicorn-site-packages>
python3 -m unittest tests.test_callback_abi tests.test_ef2000_wcc
 tests.test_hybrid_link tests.test_func_differential tests.test_replay_suite
(34 pass, no skips). Recompile all seven affected modules with
 tools/ef2000_wcc.py; zero warnings. Link the broad profile with
 tools/hybrid_link.py: 2593 replacements retained. Runtime evidence resides in
local/hybrid-callback-abi-fix-20261114/. Full replay passes the former
scanner callback fault at tick 3221 and stops later at tick 3839, IP 0x110e23,
with unsupported_interface (detail: unsupported real-mode BIOS INT 13).
Cockpit canaries remain unreached. Investigate that stop next; this does not
complete M2 or establish a usable flight session.

### 2026-11-14 — correct scroll dimensions and row-table call ABI

The next stop was a general-protection exception (vector 13 decimal), not an
unimplemented disk BIOS call. Exception-entry RAM shows the original row-table
builder at 0xfc39c running with zero COUNT/STEP, wrapping LOOP and overwriting
the saved AIL data selector. Its caller at c39f0 consumes WIDTH in EDX and HEIGHT
in EBX, then pushes them for the callee. Recovered callers passed only EAX.
An unpatched control reaches all four cockpit canaries through 1.5B instructions.

Correct c39f0's canonical and alias declarations and all nine recovered call
sites to pass 256x192, as their original callers do. draw_label_entries_150d4
also now passes the loaded resource returned by cd7f0, retaining the label
pointer separately for text drawing. Correct both scroll helpers' row-table
calls to COUNT/STEP on the stack; remove the two old bisect exclusions after
four isolated cases each pass. Correct the row builder's own declaration and
body to two stack parameters, and preserve the original zero-count LOOP wrap.
Its ECX is scratch; the leaving-scroll wrapper also clobbers ECX/EDX/EBX.
Five direct-body row-table cases pass, including a wrapping step and zero step.
The ordinary linker still excludes that stack callee pending a stack-aware
trampoline; the direct test uses its compiled ABI without a saving wrapper.

Commands: tools/func_differential.py {0xc39ac,0xc39f0,0xfc39c} --payload
local/ghidra-stack-return-20261114/payload.bin --spec decomp/tests/<addr>.json
--patch <isolated patch>; local/hybrid-scroll-abi-fix-20261114/test-row-body.py
constructs the direct row-body patch from the linked arena. Four affected
modules compile warning-free (remove two unused local declarations exposed
when rebuilding game_helpers130). The 34 focused Python tests pass with
OpenWatcom and Unicorn enabled. Evidence: local/hybrid-scroll-abi-fix-20261114/
{diff-enter-v3,diff-leave-v3,diff-row,tests}.log. Temporary runtime write-watch
and exception snapshot instrumentation has been removed from source and the
normal runner rebuilt. Full 2595-redirect replay passes the former tick-3839
timer failure and reaches cockpit initialization 0x58d24 once. It then faults
at tick 3923 in string_copy.obj:copy_string_9d749+9, reading invalid source
0x26c22fef. Remaining cockpit canaries are unreached; investigate that source
pointer/caller next. No full cockpit usability or M2 completion claim.

### 2026-11-14 — preserve the directory cache while reading it

The tick-3923 string-copy fault comes through load_directory_list_7a66c and
pool_append_7a51c. A local exception snapshot confirms cached mode
([0x12afc8]=1), a corrupted filename buffer, and duplicated/truncated names
in the pool. Original disassembly branches over buffer_write_word at 7a684
and over pool_append at 7a6da when reading the cache. The recovered C called
both unconditionally, advancing and rewriting the input stream while reading
it. Restore the two else-block boundaries; preserve the original unchecked
filename copy and all observable data formats.

Replace the dummy test with three synthetic cached-list cases: empty, two
filenames, and three filenames in section two with a nonzero starting slot.
Compare guest RAM, preserved GPRs and stack cleanup. All three fail against
the old C (even the empty case incorrectly advances the cursor by four bytes
instead of two); all three pass after the fix. Command:
PYTHONPATH=<unicorn-site-packages> python3 tools/func_differential.py 0x7a66c
--payload local/ghidra-stack-return-20261114/payload.bin --patch
local/hybrid-directory-cache-fix-20261114/only-directory.efpatch --spec
decomp/tests/0x0007a66c.json --flat-segments. Flat descriptors are needed by
the original strlen callee's ES load. Evidence: local/hybrid-directory-cache-fix-20261114/
{diff-old-flat,diff-fixed,tests}.log. The affected module compiles warning-free.
Temporary exception instrumentation was removed and the normal runner rebuilt.
All 34 focused Python tests pass. The 2595-redirect replay through 1B
instructions reaches its budget normally at tick 4000, after the former
3923 fault; 0x58d24 is hit once and remaining cockpit canaries are unreached.
Evidence: billion.log and billion-frame.ppm in the same local directory.
Two 1.5B attempts ended with SIGTERM and no guest result, so they provide no
pass/fail evidence. A new full run reports process status periodically in a
private Xvfb display to separate process lifetime from guest behavior. Full
cockpit usability and M2 remain unproven.

### 2026-11-14 — retain block history for memory faults

Extend --trace-block-tail to ordinary Unicorn execution errors in both timing
modes, retaining the opt-in/default-off behavior and the same 64-entry ring.
Factor history formatting out of the zero-address path. Add asset-free invalid
read self-tests with and without tracing in both modes. An 80-block chain
checks exact capacity and chronological wrap; untraced failures contain no
history. Private-Xvfb CTest command:
ctest --test-dir local/codex-build --output-on-failure
-R 'unicorn-cpp-(memory-fault|zero-return)' (8 pass). The 19 replay-parser tests
also pass. Wire the new checks into the Nix package check phase.

The post-directory-fix replay now has an authoritative later failure at tick
5074, executing stack memory after a bad transfer. The ordinary full-history
probe was too slow to finish before SIGTERM. A temporary diagnostic build
installed the recorder at tick 4900; its final failure record exactly matches
the untraced record before the appended 64 addresses. Those addresses show a
long march through stack data and do not yet identify the initial transfer.
The temporary activation change was removed and the normal source rebuilt.
Evidence: local/hybrid-directory-cache-fix-20261114/{monitored,tail,
fault-tail-tests-private,fault-tail-replay-tests}.log.

A full Nix Watcom rebuild exposed one stale incremental module, misc_helpers2,
from the earlier scroll-clobber change. Normalized OMF comparison shows every
other module identical. Use the complete fresh 382-object set from
/nix/store/w1gssyp2zvnbxdlxmzfcfbsc7w94v3sv-ef2000-decomp-watcom-0.1.0-dev.1730+g46659df/lib/ef2000/decomp-objects
for subsequent integration probes. Its 2595-redirect replay still fails at
tick 5074 in stack memory, so that artifact correction does not resolve M2.
Fresh evidence: local/hybrid-fresh-objects-20261114/monitored.log.

NIXPKGS_ALLOW_UNFREE=1 nix build -L --impure .#ef2000-hybrid currently fails
one of 506 Python checks (67 skips): the FLD hardware-oracle test observes
native FIP zero rather than the instruction address. A direct isolated rerun
passes; investigate the oracle/host-state interaction before changing any
expectation. Include the existing (width, bits, control word, TOP, tags) vector
in the native-FIP assertion message so a future failure identifies its input.
The unchanged check passes directly with python3 -m unittest
 tests.test_asm_x87_load.RealLoadTests.test_widening_modes_masks_stack_faults_and_all_exponents
(7.75 seconds). The package and playable cockpit remain unverified.

### 2026-11-15 — supply the active-widget fill's missing bottom edge

Correct draw_active_marker_e5088 to pass all five arguments to c91c8, including
(uint16_t)(y + height) from words 0x41eef8 and 0x41ef00. The original pushes
that bottom edge at e50d0 and c91c8 returns with RET 4; the four-argument alias
pushed nothing, corrupting the caller's stack on the solid-background path.
Remove that alias and use the existing canonical five-argument declaration.
The rectangle renderer itself remains original under its existing exclusion.

The old isolated C faults at invalid fetch 0x65768798 on filled_region. The
corrected body passes five cases: no active widget, no background flag,
a 24x12 fill, an out-of-bounds fill, and a synthetic custom-draw callback.
The fill compares all 288 affected pixels plus the refresh marker; stack
cleanup and preserved GPRs match. Command: tools/func_differential.py 0xe5088
--payload local/ghidra-stack-return-20261114/payload.bin --patch
local/active-marker-height-20261115/fixed.efpatch --spec
decomp/tests/0x000e5088.json --flat-segments. Module compiles warning-free;
34 focused Python tests pass. Full 2595-redirect loading still reaches the
separate tick-5074 stack-execution fault. Evidence: local/active-marker-height-20261115/
{diff-old,diff-fixed,tests,monitored}.log.

The reported main-menu highlight defect is reproduced by pressing at guest
(160,110), in the gray region below the first label: only the part through row
102 changes, while original rows 103..115 change too. This cutoff is exactly
the 64-KiB VGA bank boundary at a 640-byte pitch. The active-widget argument
fix does not repair it (pressed framebuffers identical before/after); inspect
the banked update path separately. Images/logs stay in local/menu-highlight-20261115/.

The complete 27-test x87 tranche passes in the failed derivation's exact
Nix environment (10 expected skips). A fresh NIXPKGS_ALLOW_UNFREE=1 nix build
-L --impure .#ef2000-hybrid then completes: 506 Python checks pass (67 skips)
and local/nix-hybrid-20261115-3c8c220d links the built launcher. Strict native
FIP assertions remain unchanged. One earlier full-suite native-FIP failure
remains an observed intermittent check; no x87 expectation was relaxed.

### 2026-11-15: bisect small framebuffer defects

Add --frame-diff-limit to hybrid_bisect (default 0.2, range 0..1), allowing
--match frame --frame-diff-limit 0 to isolate the pressed main-menu button's
missing rows. Exact failure matching also includes the stopped instruction
pointer, which changes with unrelated demotions; frame matching isolates the
visual symptom independently. Record the selected threshold in report.json
and print four decimal percentage places so small mismatches stay visible.
A synthetic 100-pixel frame with three changed channels in one pixel verifies
that the oracle counts pixels and detects a 1% defect below the default limit.
Validation: python3 -m unittest tests.test_hybrid_bisect tests.test_replay_suite
(20 passing tests). Asset-backed bisection runs on private Xvfb; artifacts
remain in local/menu-highlight-bisect-20261115/.

### 2026-11-15: restore the whole pressed main-menu button region

[x] Isolate the missing button highlight rows and repair the callback ABI.
Private-Xvfb pixel bisection reduces the defect to 0x101688 alone (20 trials,
local/menu-highlight-bisect-20261115/report.json). Translating only this entry
reproduces the exact missing rows; demoting only this entry in the full hybrid
produces a byte-identical original pressed-button PPM at 550M instructions.

The function is a scanline draw/advance loop, not a hardware delay. Its original
stack slot protects the count from callback clobbers. A captured original call
has count 15, draw callback 0xd5a90 and advance callback 0x101040. The draw body
clobbers EAX/EBX/ECX/EDX/ESI/EDI. guest_callptr0_x declared its narrower clobbers
only in the implementation, so the old caller kept the count in EDX; merely
exposing those clobbers moves the count into EBX and still fails. Declare the
full renderer clobber set in both the public helper contract and its inline
call primitive. The compiler now protects the count across the draw calls.

The complete 2595-redirect hybrid, including the corrected C loop, produces a
byte-identical original pressed-button PPM (all 640x400 pixels), restoring the
entire region below the label. The 64-KiB cutoff was the symptom, not evidence
that the VGA bank emulation needed changing. No replacement is disabled.
All three affected modules compile without warnings. Six synthetic row counts
(0,1,2,3,12,13), with draw-register-clobbering callbacks and counter RAM, match
original ABI/RAM. A separately compiled asset-free caller protects a live value
from the actual helper and a callback clobbering EBX/ECX/EDX/ESI/EDI; the focused
36-test Python tranche passes. Evidence: local/scanline-callback-20261115/,
all-clobbers{,-diff}.log, all-clobbers.ppm and tests.log. The separate cockpit
loading fault persists after the intermediate narrower clobber correction;
full launcher/gameplay validation remains pending.

### 2026-11-15: reject interrupted native x87 observations

The launcher rebuild at 312a1cfd runs 508 Python tests but fails the native
extended-memory test: saved FIP is zero instead of the instruction address.
This is the same class of intermittent observation as the earlier FLD failure.
A local diagnostic records an involuntary context switch with the zero FIP.
A disposable probe then pins itself and a busy child to one CPU and inserts
sched_yield between the native x87 instruction and FNSAVE: it reproduces FIP=0
with the involuntary-switch count changing from 34 to 35 inside the measured
native-call interval. Evidence remains in local/nix-x87-memory-20261115/
{probe,probe-immediate,probe-yield}.log. Do not infer a translator mismatch from
this interrupted hardware observation.

Add a test helper that samples thread voluntary/involuntary context-switch
counters immediately around the native call. Discard only samples with a
measured switch, at most four attempts; repeated interruption raises an error.
Use it for the two native load/memory tests that have exhibited the failure.
All existing pointer, opcode, numeric, exception and guest-state assertions
remain strict. No generated-code failure is retried. Three synthetic tests
prove that an uninterrupted result (including zero) is returned unchanged,
only measured switches cause another observation, and repeated interruption
fails closed. Seven helper/load/memory tests pass in the failed derivation's
exact Nix environment (12.870 seconds); fixed-tests.log records the result.

### 2026-11-15: guard the remaining direct native x87 probes

A same-source Nix retry passes the native comparison test but fails the raw
stack probe on FXCH ST(0): retained FDP/FDS are zero instead of 0x234567/16.
That probe still executed hardware without measuring interruption. Apply the
same bounded observation guard to direct stack, integer-load and narrowing-
store probes. Arithmetic and the native oracle already measure context
switches. Preserve all expected state and assertion strictness. Intentionally
faulting child-process calls remain direct.

Validation in the exact Nix environment: helper/stack/integer/store tranche
runs seven tests in 18.728 seconds (two optional store skips); rerun those two
store tests with ASM_SOFTFLOAT_BUILD pointing at the existing verified local
build and both pass in 11.488 seconds. Evidence: local/nix-x87-memory-20261115/
{remaining-fixed-tests,store-fixed-tests}.log.

The preceding full build also observed zero retained FDP/FDS in the native
comparison oracle despite its existing switch guard. Its diagnostic isolated
run passes all 2,356,224 cases unchanged in 116.401 seconds, and that comparison
passes in the full retry. This remaining intermittent hardware-oracle symptom
is not claimed resolved by the direct-probe guard. Logs remain in
local/nix-hybrid-20261115-current{,-retry}.log and
local/nix-x87-memory-20261115/probe-compare.log. No translator assertion is
waived and no known numerical divergence is accepted.

### 2026-11-15: restore the parser allocation callback ABI

[x] Repair 0xe764, the allocation hook selected at [0x121ebc] while loading
header lists. Original pushes size and the cfg_read failure diagnostic as two
cdecl arguments to 0xcc6c0, cleans eight bytes, and returns the allocated
pointer in EAX. The C draft called a register-argument alias and declared no
return. Use the existing explicit cdecl two-argument alias, return ef_addr32,
remove the unused incorrect alias, and correct the confirmed signature.

An earlier-start execution guard catches the first invalid transfer at
0x3f86d5a immediately after the default IRQ trampoline 0x4000100. A write watch
then catches the first corruption at 0xe301a (formatter output), tick 5074:
compiled ABI-trampoline bytes are interpreted as a failure format and copied
through the stack buffer into 0x4000100. The allocator's original failure path
at 0xcc702 formats the bogus stack argument; this is downstream of the wrong
0xe764 call convention, not a VGA/IRQ emulation issue. Diagnostic source and
runners are local only; runtime source and normal runner are restored. Evidence:
local/first-invalid-transfer-20261115/{probe,watch}.log and watch-captures/.

Three synthetic allocator adapters record the two stack arguments and return
a distinct pointer for sizes 0,14,132. Old C fails all three, fixed C passes
all three with preserved registers and stack cleanup. Module compiles with
zero warnings; 36 focused Python checks pass. The full 2595-redirect replay
passes the previous failure and grows the guest heap further, then reaches a
separate tick-5095 GP at original LES [EBP+8] (0x9c231), currently reported as
unsupported BIOS interrupt 13. This later far-pointer/call ABI remains to
investigate. Evidence: local/allocator-callback-20261115/{old,fixed,tests,replay}.log.

The prior committed profile's complete Nix build passes 511 tests (68 skips),
and the exact requested nix run launcher reaches the menu without faults
under private Xvfb. Its fresh Nix object patch is byte-identical to the
verified whole-button fix. Logs: local/nix-hybrid-20261115-final.log and
local/native-final-20261115.log. Full gameplay remains incomplete.

### 2026-11-15: honor explicit cdecl declarations in ABI generation

[x] Correct the release-call ABI reaching the next loading failure. Interrupt
trace at tick 5095 shows 0x9c231 LES [EBP+8] receiving offset=0x2f,
selector=0xd5 and length=0x19002f. The caller is compiled finish_request_6b8:
its OMF passes the intended first argument in EAX, only selector/length on
the stack, and expects callee cleanup. The public guest_fn_9c23d declaration
already says EF_CDECL, but generated confirmed pragmas overwrite it using
incorrect EAX-only metadata for the shared-tail entry.

Do not generate a register pragma over an explicit EF_CDECL/__cdecl
annotation. Correct original 0x9c23d's signature to three caller-cleaned stack
arguments; it sets AX=0x601 and jumps to the same stack prologue as 0x9c220.
Its already-excluded C placeholder remains excluded. The only currently
emitted pragma conflicting with an explicit EF_CDECL is guest_fn_9c23d.
A synthetic compiler-wrapper fixture verifies both stack annotations survive
while ordinary register inference still works.

Three original-versus-C finish_request cases cover active aligned and
unaligned slots and an already-released slot. The old caller faults fetching
0x32435465; the corrected caller passes all three, including observed stack
arguments, free-list/count updates, preserved registers and cleanup. Watcom
compiles warning-free and 37 focused Python tests pass. The full unchanged
2595-redirect profile passes the LES fault and reaches a separate tick-5116
GP at original POP FS (0x31a). Evidence: local/x32-release-abi-20261115/
{old,fixed,tests,replay}.log and x32_pool.asm.

The allocator-fix pressed-button regression, with the same 0x101688 tracepoint
configuration as its original baseline, is byte-identical to the original.
An earlier diagnostic without that tracepoint ran unexpectedly long and was
explicitly terminated (exit 143), not counted as a guest pass/failure. Host
ptrace inspection was denied by the kernel. Artifacts remain under
local/allocator-callback-20261115/pressed{,-traced}*.

### 2026-11-15: retain point-list parser call order

[x] Move the count read in read_point_list_fb10 after read_parser_line and
read_parser_token. Original fb18/fb1d/fb22 perform those three calls in that
order; the C initializer called the integer parser on the previous line.
Three isolated adapters record call phase and allocation size for counts
0,1,3 and populate coordinates so only the middle point is clamped. Old C
fails all three, fixed C passes all three with ABI/RAM comparison (17 observed
original blocks). The module compiles warning-free; 37 focused Python tests
pass. Use isolated-fixed.efpatch for these stubbed-callee tests: the full
patch calls compiled parser bodies and intentionally bypasses original-entry
stubs. Evidence: local/point-list-order-20261115/{old,isolated-fixed,tests}.log.

The full 2595-redirect profile still reports the same mission-file line-30
waypoint error. A capture before enter_handler_eb448 teardown shows the actual
message and parser line "WP_ROUTE 2 0 1", cursor 14. Its caller chain is
parse_component_record_f314 -> read_record_ed80 -> read_parser_enum_19_names,
not the corrected point-list function. Investigate that component's earlier
line consumption next. Evidence: local/point-list-order-20261115/replay.log
and error-captures/; all assets/snapshots remain local.

### 2026-11-15: consume all twenty component-prefix records

[x] Correct parse_component_record_f314's family-zero prefix loop. Original
f3db initializes the cursor to REC, increments it at f3e0, and writes offsets
+0x55..+0x68, consuming twenty parse_enum_eea0 records. C started at REC+1,
consuming only nineteen and leaving the following position/waypoint lines
out of sync. Restore the initial cursor; other families retain their skip.

The focused fixture also exposes incorrect EDX preservation metadata.
Original f314 saves EBX/ECX/ESI/EDI, uses DL as parent kind and leaves EDX as
a scratch point offset; f5f9 does not restore it. Correct the signature and
recompile both the body and its runtime_helpers27 caller. With that proven
ABI, old family zero fails RAM at REC+0x55 while families one/two pass; all
three fixed adapters pass with original RAM/GPR/stack comparison (49 observed
original blocks). Both modules compile warning-free; 37 focused tests pass.
Evidence: local/component-prefix-count-20261115/{old-correct-abi,abi-fixed,tests}.log.

Full-profile loading attempts with this change were externally terminated
(exit 143) without guest results, including a process-monitored run. They do
not prove success or failure. The shorter 1.3B-instruction checkpoint subsequently completed normally
(status=instruction_limit, tick 5200, 57.17 seconds), passing the previous
1.279B-instruction parser stop. Traced entries 0x58d24, 0x7eea0 and 0x810d0
each ran once; 0x7a428 had not run. This establishes progress beyond the
previous failure, not a completed cockpit replay or native flight usability.
Evidence: local/component-prefix-count-20261115/short-checkpoint.log.
The full 2595-redirect profile remains enabled. Checkpoint artifacts stay in
local/component-prefix-count-20261115/.


### 2026-11-15: skip six-field record labels and preserve caller registers

[x] Restore read_parser_six_field_record's initial read_parser_token call
(original ee5f). Without it the label becomes the first coordinate and the
last field remains unread. Original ee5c saves EDX and its tokenizer/number
parser dependencies preserve the other non-result GPR; correct the signature
accordingly. The old body also demonstrably clobbered EBX and ECX. Rebuild
parser_records and both callers, misc_drafts1 and runtime_helpers27.

Expand decomp/tests/0x000ee5c.json with signed bounds, low-word truncation,
quoted labels and the existing numeric-label/missing-final-field case.
Using original parsing dependencies, old C fails all four cases; fixed C
passes all four RAM/GPR/stack comparisons. All three modules compile with
zero warnings; 37 focused Python tests pass. Commands: tools/func_differential.py
0xee5c --payload <payload> --patch <isolated patch> --spec
 decomp/tests/0x000ee5c.json --flat-segments; unittest modules callback_abi,
ef2000_wcc, hybrid_link, func_differential, replay_suite and hybrid_bisect.
Evidence: local/six-field-label-20261115/{old,fixed,tests}.log.

The 1.4B-instruction replay with all 2595 redirects fails later at tick 5207
with an invalid fetch (0xbff20800); the preceding full-profile build fails
at the same tick (0xbff70800). The prior line-30 parser error is passed,
but flight usability remains unverified. Do not treat the parser adapter
passes as proof that loading or the cockpit works. Logs:
local/{component-prefix-count,six-field-label}-20261115/extended-checkpoint.log.


### 2026-11-15: call original trigonometric leaves through ST0

[x] The full-profile block tail locates the tick-5207 invalid return immediately
after the sixth trigonometric call in unit_rotation_matrix_7215c. Compiled
callers pushed an eight-byte argument for cos_b45a2/sin_b45ac, but original
b45a2/b45ac consume and replace ST0 and return without popping arguments.
Six calls left 48 bytes on the stack. Original b45b6 saves EAX/EBP and touches
no other GPR; both leaves preserve all GPR. Correct their public pragmas and
signature rows to ST0 input/output, zero stack arguments, all GPR preserved.
Keep both existing replacement exclusions: this repairs callers to original
entries, not the C replacements' unverified trigonometric equivalence.

Recompile misc_drafts1 (the only other source using these canonical names):
zero warnings. Its rotation-matrix caller now directly CALLs each leaf with
ST0 live, without the old SUB ESP,8 / FSTP [ESP] sequence. The asset-free
trig_contract_fixture invokes both public declarations with synthetic
FCOS/FSIN;RET callees and verifies outputs, guards, return value and ESP for
0, -0.75 and 2.25. The old header faults on all three angles; the corrected
contract passes. All 38 focused tests pass using EF2000_WCC386 and Unicorn:
python3 -m unittest tests.test_callback_abi tests.test_ef2000_wcc
 tests.test_hybrid_link tests.test_func_differential tests.test_replay_suite
 tests.test_hybrid_bisect. Evidence under local/trig-call-abi-20261115/.

The full 2595-redirect replay passes that invalid return but still fails in
the same tick at another control transfer: execution reaches heap data
0xcc50c3 and faults reading 0xe5610000. This is a new diagnostic point, not
a successful flight replay. The previous block-tail evidence is in
local/six-field-label-20261115/exp-entry-capture.log; corrected integration
result: local/trig-call-abi-20261115/extended-checkpoint.log.


### 2026-11-15: suppress compiler frames around assembly-owned helper stacks

[x] The next block tail ends in IF@DSQRT, then heap data at 0xcc50bf.
Disassembly of watcom_rt_helpers shows PUSH EBX/ECX/EDX/ESI/EDI before FSQRT;
the handwritten RET executes before the generated POPs and jumps to saved
EDI. Seventeen recently added private pragmas incorrectly said MODIFY [].
Like the existing integer helpers, declare all GPR modified to suppress
compiler saves around these assembly-owned stacks. Each body still preserves
its required registers explicitly. This private implementation declaration
does not weaken recovered callers' public contracts.

Add asset-free tests for all 17 affected helpers: eight ST0 math operations,
ceil/floor/trunc with three inputs each, five cdecl diagnostic thunks and the
four-register/two-stack callback thunk. Check return addresses, exact stack
cleanup, arguments, preserved GPR, returned math values, x87 control word and
empty tags after consuming the result. The old object faults in all 23
subcases; the corrected object passes. All 42 focused tests pass:
EF2000_WCC386=<wcc386> PYTHONPATH=<Unicorn> python3 -m unittest
 tests.test_watcom_rt_helpers tests.test_callback_abi tests.test_ef2000_wcc
 tests.test_hybrid_link tests.test_func_differential tests.test_replay_suite
 tests.test_hybrid_bisect. The runtime module compiles warning-free.
Evidence: local/runtime-helper-frames-20261115/{old-tests,tests}.log.

The full 2595-redirect replay passes the tick-5207 invalid helper return,
then faults at tick 5214: fetch 0x180c7e5a, ESP 0x3ffeca0. Flight usability
remains incomplete. Original-transfer evidence:
local/trig-call-abi-20261115/transfer-trace.log. Corrected integration log:
local/runtime-helper-frames-20261115/extended-checkpoint.log.


### 2026-11-15: keep the self-modifying rotation entry original

[x] The next full-profile block tail ends at c4755 before fetch 0x180c7e5a.
Original c4755 starts SUB EAX,imm32; camera-origin writes target its immediate
at c4756, plus the following EBX/ECX immediates c475c/c4762. A five-byte JMP
redirect occupies the same bytes, so normal origin updates corrupt its
relative displacement. The draft C interface additionally uses an output
pointer instead of returning coordinates through ESI/EDI/EBP. Demote only
this proven unsafe entry to compiles-bad/unverified and explicitly exclude
it until self-modifying operands and the multi-register ABI are represented.

The linked patch now has no write overlapping c4755..c4765; all other 2594
redirects remain enabled. Full replay passes the former invalid fetch,
then stops at address zero later in tick 5214. Captures at c4755, its caller
continuation 160171 and c473c show intact original SUB instructions with
all three origin immediates patched to zero, and successful coordinate
return to the original wrapper. These captures prove the retained path runs;
they do not prove flight usability. Evidence:
local/rotation-smc-20261115/{extended-checkpoint,transfer-trace}.log and
transfer-trace-error-captures/. The later tail ends at compiled 0x302fb43
after original record_word_16010e returns. All 42 focused tests pass;
python3 tools/decomp_status.py check reports tracker ok (4293 functions).


### 2026-11-15: preserve script callback cleanup and return values

[x] The zero-address block tail ends at guest_callptr1_x after the original
record_word_16010e returns. Original script starters 9fb00@9fbe8 and
a060c@a06eb push one argument, call [132b14] without caller cleanup, and
store EAX at 1600c0 before calling 160255. C used the cdecl helper (double
cleanup after RET 4) and discarded the result. Add a distinct one-stack-arg,
callee-cleaned, EAX-returning callback helper; publish its six-GPR scratch
contract. Correct the shared script-start body to store the return value.
The original cdecl helper retains its caller-cleanup contract.

The asset-free separately compiled caller test checks a live value across
six-register callback clobbers, the argument, return value and exact ESP.
Both script entry fixtures cover zero, relative and negative indices, the
relative-position flag, extra-word wrapping, callback arguments/results and
all original RAM/GPR/ESP comparisons. Old C faults in the first case of each;
fixed C passes all six. Both affected modules compile warning-free; all 43
focused tests pass, including tests.test_watcom_rt_helpers and
 tests.test_callback_abi plus ef2000_wcc, hybrid_link, func_differential,
replay_suite and hybrid_bisect. Differential command: tools/func_differential.py
<9fb00|a060c> --payload <payload> --patch <isolated patch> --spec
 decomp/tests/0x000<address>.json --flat-segments.
Evidence: local/script-callback-abi-20261115/{old-*,fixed-*,tests}.log.

The full 2594-redirect replay passes the previous tick-5214 zero stop and
reaches tick 5273, where enter_handler_eb448 captures the primary error
"emm: bad getadr (0)". The later GP at 0x31a / BIOS-13 diagnostic occurs
during fatal teardown, as in the earlier parser failure; it is secondary.
Do not call this a flight pass. Integration log and full error snapshot:
local/script-callback-abi-20261115/extended-checkpoint.log and
extended-checkpoint-error-captures/0x00eb448-0000.efcap.


### 2026-11-15: isolate generated ABI headers per object

[x] Parallel wrapper calls shared destination.parent/ef_confirmed_abi.h.
Each write_text truncates that file while another WCC process may read it;
a concurrent compile emitted only 8213 included lines versus 12388 when
recompiled sequentially. Give every destination object its own .abi.h file.
No callers outside the wrapper require the former shared filename.

The deterministic asset-free regression synchronizes two fake compiler jobs
after both headers are published, then checks each compiler's header against
its job. The old wrapper fails (first reads second's pragmas); object-specific
headers pass. All 44 focused tests pass, including the real OpenWatcom/Unicorn
callback and runtime-helper tests. Evidence:
local/renderer-handler-abi-20261115/{old-header-race,tests-with-race-fix}.log.
All three modules for the current renderer investigation were recompiled
sequentially before linking, so its integration result does not rely on the
racing builds. This fixes publication ownership; it is not a claim that
other translated bodies or cached objects are ABI-correct.


### 2026-11-15: pass renderer handler indices in ESI

[x] The "emm: bad getadr (0)" capture's live caller stack identifies
run_handler_stack_c425a -> guest_callptr0_x -> original c5551 -> b3f58.
Original c4270 loads the handler index into ESI, c4273 loads its byte-selected
dispatch offset into EBX, and c427a calls the selected handler. C loaded the
index only into a local and called through a zero-argument helper, exposing
stale ESI to the handler. Add a distinct public EBX/ESI callback helper with
all seven GPR scratch. Pass both values explicitly. Original c4281/c4289
returns EBX=147e56; correct the loop's public signature to return that value
and stop claiming callback-clobbered GPR are preserved. Recompile the helper,
misc_drafts1 and its renderer_sort_queue caller sequentially, zero warnings.

The four isolated original-versus-C cases cover empty, single, reverse-order
and repeated entries; handlers log EBX/ESI and clobber all seven GPR. Fixed
C passes RAM/EBX/ESP comparisons in all four. Old empty case fails EBX;
old single case faults. Keep tracker state compiles: these tests do not
verify arbitrary renderer handlers or claim static coverage of the shared
tail. Command: tools/func_differential.py 0xc425a --payload <payload>
 --patch <isolated patch> --spec decomp/tests/0x000c425a.json --flat-segments.
All 44 focused tests pass after the independent compile-wrapper race fix;
tracker validates (4293 functions). Evidence:
local/renderer-handler-abi-20261115/{old,fixed,tests-with-race-fix}.log.

The full 2594-redirect replay now follows the actual handler path and faults
inside formatting at ed1b6 reading ffffffbf, tick 5235. A diagnostic control
restoring only c425a's original entry (2593 redirects) reaches the exact same
fault with identical registers, stack words, IRQ count and canary counts.
This establishes parity at this later failure, not flight usability. The
original loop is only a diagnostic control, not a committed exclusion.
Logs: local/renderer-handler-abi-20261115/{extended-checkpoint,original-control}.log.


### 2026-11-15: use the tactical text sink's five-stack-argument ABI

[x] The formatter fault's block tail identifies draw_tactical_coordinates
calling original 48bb0, then E3022/ED133 with format pointer ffffffbf. Its
nonvariadic sink alias lacked EF_CDECL, so WCC passed four arguments in
registers and only AUX on the stack. Original 59477 pushes all five arguments
and cleans twenty bytes; original 48c32 is a plain RET. Mark the sink
EF_CDECL, correct 48bb0 cleanup from twenty to zero, and correct 59420's
preserved registers to ECX/ESI/EDI/EBP (EAX/EDX/EBX are scratch).
Recompile record_transitions and its game_helpers63/game_helpers130/
misc_drafts1 callers, all warning-free, using object-specific ABI headers.

Expand 59420's isolated test with a five-stack-argument sink and a wrapped
coordinate case. Old C faults returning through AUX; all three fixed cases
pass RAM/GPR/ESP comparisons. Correcting 48bb0's cleanup also removes its
previous automatic stack-aware gate. Do not silently enable that replacement:
eight new differential cases show literal text passes in all four mode
variants, but numeric text fails in all four. Original "n=123" becomes
"n=1702266776" (initial EDI 0x65768798): hybrid_link's register-saving
trampoline forwards only the five fixed arguments, not the unknown variadic
suffix. Explicitly retain the original entry and mark the replacement
compiles-bad until its variadic forwarding is represented. No claim that the
C body itself is the source of that trampoline mismatch.

Commands: tools/func_differential.py <59420|48bb0> --payload <payload>
 --patch <isolated patch> --spec decomp/tests/0x000<address>.json
 --flat-segments. Evidence: local/tactical-text-abi-20261115/{old-59420,
fixed-59420,fixed-48bb0,tests}.log. All 44 focused tests pass; tracker validates.
The record-transitions host test passes when compiled directly with assertions
enabled. Its existing Release CMake target fails on unused assert-only
variables/parameters under NDEBUG; do not report that target as passed.

With 48bb0 retained original, the full 2594-redirect replay passes the invalid
format-pointer read, but reports unsupported port OUT 0000 at IP 0x32/tick
5235. This is another unresolved failure, not flight usability. The temporary
2595-redirect diagnostic with C text enabled also ends with the port failure
and is not a valid text replacement pass. Authoritative retained-original
integration log: local/tactical-text-abi-20261115/original-text.log.


### 2026-11-15: retain complete argument frames at native C variadic entries

[x] Fix the generic linker's variadic stack loss. Its register-saving wrapper
re-pushes only the fixed signature arguments, so a variadic C body reads saved
GPR where the unknown suffix belongs (48bb0's 123 became EDI=65768798).
Discover explicit variadic declarations in the ABI header, ignoring comments.
For handwritten native C variadic definitions with stack-only inputs, caller
cleanup, native EAX/ST0 results and no EAX-preservation requirement, enter the
body directly so its own C ABI preserves non-result registers and the caller's
entire argument frame remains in place. Unsupported shapes stay original;
fixed-argument entries retain their existing wrappers. Report direct entries.

Asset-free Unicorn regression exercises a synthetic C-ABI callee with zero,
one, three and six variable words, checking sums, all preserved GPR, return
address, ESP and unchanged caller argument bytes. Additional negative cases
reject register-input, EAX-preserving and nonnative-result variadic contracts;
prototype parsing distinguishes comments containing ellipses. All 46 focused
tests pass with OpenWatcom and Unicorn. Command: python3 -m unittest
 tests.test_watcom_rt_helpers tests.test_callback_abi tests.test_ef2000_wcc
 tests.test_hybrid_link tests.test_func_differential tests.test_replay_suite
 tests.test_hybrid_bisect.

Re-enable 48bb0 only after nine original-versus-C RAM/GPR/ESP cases pass:
literal/numeric text in four mode variants, plus mixed signed integer/hex/
string-pointer arguments. With the old wrapper numeric cases mismatch and
the mixed case faults; direct entry passes all nine. The floating-format
probe faults in the original oracle, so remains inconclusive under ignored
local/variadic-tail-20261115/double-probe.json. Do not count it as a pass or
as a replacement-specific defect. Keep tracker state compiles, not verified.
Differential command: tools/func_differential.py 0x48bb0 --payload <payload>
 --patch <isolated patch> --spec decomp/tests/0x00048bb0.json --flat-segments.
Evidence: local/variadic-tail-20261115/{old-differential,differential,tests}.log.

Full link: 2595 redirects, 342/382 modules, 575548 arena bytes; report lists
48bb0 as the only direct variadic entry. The replay still reports unsupported
port OUT 0000 at tick 5235. Flight usability remains incomplete; no completion
claim follows from the function tests. Integration:
local/variadic-tail-20261115/extended-checkpoint.log.


### 2026-11-15: remove ghost stack arguments from odometer calls

[x] A late block-state trace from original 59420 reaches show_scaled_value's
compiled epilogue, then returns to 0x2c and executes banner text (OUTSB at
0x31 explains the port-0 report). The 5e21c alias declared four parameters
although its confirmed ABI consumes only EAX/EDX/EBX. WCC therefore pushed
the fourth dummy zero and assumed the callee would remove it; original
5e4c0 is a plain RET. Both show_scaled_value_5c2d8 and draw_cell_text_5e1b0
passed that unsupported fourth argument. Correct the declaration and both
calls. The rebuilt callers no longer PUSH zero before CALLing 5e21c.

Also correct 5e1b0's preservation row: its original prologue saves only
ESI/EDI/EBP; EAX/EDX/EBX are scratch through the odometer and ECX is used
for shift/mode setup. Recompile misc_drafts1 with that complete ABI, zero
warnings. Three scaled-value cases exercise positive/negative/zero values,
float-rounded formatter arguments, sign-extended ids and the three-register
odometer call using authored callee stubs. Two cell-text cases exercise
normal/doubled coordinates, the text-width result and final odometer
arguments. Fixed C passes all five RAM/GPR/ESP comparisons; old C faults
in the first case of each function. All 46 focused tests pass; tracker
validates (4293 functions). Differential commands:
 tools/func_differential.py <5c2d8|5e1b0> --payload <payload>
 --patch <isolated patch> --spec decomp/tests/0x000<address>.json
 --flat-segments. Evidence: local/odometer-call-abi-20261115/{old-*,fixed-*,tests}.log.

The successful diagnostic trace and low-memory snapshot are under
local/variadic-tail-20261115/coordinate-state-*. Earlier body-entry probes
hit the trace limit during mission setup or rejected an arena snapshot;
they are not full replay passes. The complete 2595-redirect integration
passes the low-memory port stop but reaches another fetch fault at
0x42a3e857/tick 5235 with instruction-looking words on the stack. This is
unresolved; do not infer that the cockpit or native flight is usable.
Authoritative current patch/log:
local/odometer-call-abi-20261115/abi-complete.{efpatch,log}.


### 2026-11-15: restore caller-cleaned status and text-width sinks

[x] The post-odometer fetch fault ends after original 48c34 scans a buffer
full of instruction bytes through strlen, then returns through overwritten
code-looking words. The tactical metric sink alias still used register
arguments although original 594a4 pushes mode/item and original 48c8e is a
plain RET. Mark draw_tactical_mode_item EF_CDECL. Correct the same missing
contract on the four adjacent sprintf aliases (pair, double, value and no
varargs). Original a4371 is also a plain RET. Correct 48c34/a4342 cleanup
metadata from 8 to 0; their native C variadic bodies now use the verified
full-frame direct-entry path. Correct 594a4 preservation to all non-result
GPR: its EDX save plus the width callee's other preservation.

Recompile all six affected caller modules and both native C variadic callee
modules using private ABI headers; all eight compile warning-free. An
asset-free separate caller tests all five public alias shapes, including the
double's two stack words, exact caller cleanup and width return value. The
old header causes an invalid fetch; the corrected header passes. All 47
focused tests pass without skips. Three metric-wrapper differential cases
check signed-half/full width and arguments (old C fails all three), eight
real-formatter width cases cover mode variants and integer/literal formats,
and three real-formatter sprintf cases cover literals and signed/mixed
string/hex/unsigned arguments. All fourteen pass original RAM/GPR/ESP and
return-value comparison. Commands: tools/func_differential.py
<594a4|48c34|a4342> --payload <payload> --patch <isolated patch>
 --spec decomp/tests/0x000<address>.json --flat-segments; focused unittest
modules watcom_rt_helpers, callback_abi, ef2000_wcc, hybrid_link,
func_differential, replay_suite and hybrid_bisect. Tracker validates.
Evidence: local/status-cdecl-20261115/{old-contract,contract,old-594a4,
fixed-*,tests}.log. Earlier arity audit lists 63 possible declarations under
local/odometer-call-abi-20261115/remaining-arity-audit.tsv; they are candidates,
not confirmed defects or authorization to remove arguments blindly.

Full profile now has 2597 redirects, 342/382 modules, 575584 arena bytes and
three native variadic entries (48bb0, 48c34, a4342). Both 1.4B and 1.5B
instruction replays complete normally at their instruction limits, reaching
ticks 5600 and 6000 without the prior stack/port faults. The 1.5B frame still
differs substantially from original control: HUD renders over mostly black
terrain/cockpit areas and labels differ. This is not faithful rendering or
verified native flight usability. Original-entry canaries alone do not count
compiled direct calls, so do not infer absence of an entire subsystem from
an original-entry hit count of zero. Logs and frames:
local/status-cdecl-20261115/{extended-checkpoint,cockpit-checkpoint}.{log,ppm,png}.
Next work remains rendering/control-flow convergence and the current Nix
launcher, not declaring the bulk translation goal complete.

### 2026-11-15 — Keep rendering bisection on the current variadic ABI

The bisector's two linker calls omitted `variadic_names`, silently omitting
native variadic entries and rebuilding their old argument-frame failure into
rendering trials. Both initial selection and each trial now parse the selected
ABI header and pass its variadic declarations to the linker. A synthetic
command-level regression verifies both calls; all seven linker/bisector tests
pass with Unicorn enabled (`python3 -m unittest tests.test_hybrid_bisect
 tests.test_hybrid_link`). The first real cockpit trial is byte-identical to
local/status-cdecl-20261115/current.efpatch (SHA-256 c7135c66708013cd4fdc2084e84d96f66326a8cff4d3365565f8d3fa91be1760).
Rendering bisection is running under private Xvfb in
local/render-bisect-20261115; no rendering fix or completed bisection is claimed.

### 2026-11-15 — Distinguish cockpit blanking from loading-screen stalls

Rendering trials now include framebuffer geometry in the frame-failure
signature. A 640x400 loading stall is no longer interchangeable with the
320x200 cockpit failure; regression coverage also rejects missing frames.
All eight linker/bisector tests pass with Unicorn enabled. Original and full
hybrid 1.5B replays reproduce their earlier raw frames exactly. The full
hybrid differs in 90.2812% of pixels, with 95.7297% black pixels versus
9.14375% in original. Diagnostic complementary redirects restore visible
terrain/cockpit but change palette, so raw pixel difference alone cannot
isolate this blanking defect. The superseded broad bisections were stopped
intentionally; they do not establish a culprit. A black-pixel-mask bisection
is running under private Xvfb in local/render-black-mask-bisect-20261115;
its ignored wrapper retains unmodified frames as frame.raw.ppm. No subset
is promoted as the production profile.

Nix output local/nix-hybrid-current-20261115 resolves to the completed
39198eaa launcher build. Its runtime build log records 522 tests passing
(75 skipped), followed by installation/fixup. The outer build process exited
143 after producing the output; the installed launcher is being independently
smoke-tested. This is not a verified native cockpit playthrough.

### 2026-11-15 — Preserve variadic frames in the native hybrid launcher

The completed Nix launcher build exited ff before the menu: the console
reported `Unable to open file '\.par'`. Fresh and cumulative object sets
both failed under its patch builder. `decomp_app.build_patch` omitted the
new variadic declarations, generating three obsolete fixed-argument frames
(99 extra arena bytes) and losing resource-format arguments. It now loads
variadic names from the same ABI header used for aliases. Its fresh-object
patch is byte-identical to the CLI linker patch (2597 redirects, 575600 arena
bytes). Add a builder regression using an independently selected ABI header;
all 12 launcher/linker/bisector tests pass with Unicorn enabled:
`python3 -m unittest tests.test_decomp_app tests.test_hybrid_bisect
 tests.test_hybrid_link`.

With the corrected builder, the freshly built native runtime and complete
Nix object set reach video mode 100 (menu), finish the 20-second time limit
and return zero under private Xvfb. The old builder failed with the same
runtime/objects. Evidence: local/native-current-20261115.log,
local/native-cumulative-control-20261115.log, and
local/native-variadic-fixed-20261115{.log,/report.json,/calls.log}.
The packaged command still needs rebuilding with this fix; native flight
usability and cockpit rendering remain unverified.

### 2026-11-15 — Continue scaled renderer entries into drawing

Original c552e/c565e are 35-byte scaling entries that continue into shared
rendering bodies c5551/c5681; the C drafts returned after scaling, and even
returned immediately for scale +/-1. Restore those draw calls with ESI=index
using the existing register callback adapter. Reconstruct the scaling step
locally so it does not call the excluded AL/ESI division/multiplication
fragments using their incorrect ordinary-C declarations. Preserve signed
division, byte negation (-128 stays -128), atomic three-coordinate updates,
and draw suppression on any signed multiplication overflow. Correct both
signature inputs from EAX to ESI and remove the false preserved-register
claim through the full draw callback. Queue table slots 20/24 and immediate
dispatch 16018a@16024f confirm the ESI input.

Eight independent differential cases per entry cover +/-1, signed division,
multiplication, -128 and overflow in each coordinate. Authored sinks log ESI,
count draw calls and clobber all seven GPR including EBP. The old bodies pass
only 3/8 (overflow cases) per entry; corrected bodies pass 16/16 RAM/ESP and
contract checks. Original helper arithmetic executes in the oracle; no game
bytes are stored in the fixtures. WCC reports zero warnings/errors; all 53
focused tests pass with WCC and Unicorn enabled, tracker check passes.

The full profile retains 2597 redirects, 342/382 modules, 575816 arena bytes.
Its 1.5B replay reaches instruction_limit/tick6000 without crashing; c565e
receives 2821 original-entry hits. Terrain is now visible: black-pixel fraction
falls from 95.7297% to 46.1031%. Cockpit panels remain absent and HUD differs;
this is not complete rendering fidelity or verified flight usability. Both
functions remain compiles. The 550M pressed-menu replay remains byte-identical
to the original whole-button highlight. Evidence and objects:
local/renderer-scale-tail-20261115/{old-552e,old-565e,fixed-c552e,fixed-c565e,
compile,tests,cockpit-checkpoint,button}.log and corresponding ignored frames.
Commands: ef2000_wcc.py for misc_drafts1, func_differential.py for both entries
with explicit specs and --flat-segments, and the seven previous focused
unittest modules plus tests.test_decomp_app.

The exact packaged `NIXPKGS_ALLOW_UNFREE=1 nix run --impure .#ef2000-hybrid --
../EF2000/c/EF2000/` prefix at a899f96a now passes a private-Xvfb headless
20-second menu smoke: time_limit, launcher exit0, guest return27, faults[],
startup_checkpoint_verified=true. Its complete Nix build includes the launcher
variadic fix, predating this renderer fix. Evidence:
local/native-packaged-fixed-20261115{.log,/report.json,/calls.log}.
The separate fresh-object C++ menu observation ended externally with143 and
no log; it is inconclusive, not a replay failure or pass. The black-mask
bisection of the prior profile remains live as an independent diagnostic.

### 2026-11-15 — preserve the keyboard handler's live data selector

Corrected store_selector_fe610: hybrid Watcom now reads DS through
guest_seg_x(3), as the original MOV AX,DS does, instead of writing zero to
0xfe624. That word is the immediate loaded by the IRQ1 handler at 0xfe622.
Real SDL Return input exposed a native general-protection fault at 0xfe639
with DS=0; flat-memory menu replays had not exercised this protection check.
The nonzero protected-mode differential fails the previous C (expected 0x10,
actual 0) and passes the replacement, including EAX, preserved registers,
stack and observed RAM writes. The authored surrounding instruction canaries
also detect widened writes. The entry remains compiles: full replay fidelity
is still pending.

Evidence: local/keyboard-selector-20261115/{selector.efpatch,game.log,
output/report.json,output/calls.log}. A private Xvfb SDL run receives two
Return make/break pairs at guest port 0x60, ends at its 40-second time limit,
launcher exit 0, guest return 27, faults[], startup_checkpoint_verified=true.
This proves removal of the first-key crash, not interactive flight readiness.
Watcom compilation has zero warnings/errors; all 53 focused tests pass;
decomp_status.py check reports 4293 entries. Commands: ef2000_wcc.py for
misc_drafts1; func_differential.py 0xfe610 with the explicit spec and
--flat-segments; unittest modules test_watcom_rt_helpers, test_callback_abi,
test_ef2000_wcc, test_hybrid_link, test_func_differential, test_replay_suite,
test_hybrid_bisect and test_decomp_app.

The bounded cockpit black-mask bisection completed with outcome unresolved,
649 exclusions and 1947 remaining candidates (not a minimal culprit set).
Its report is local/cockpit-mask-bisect-01-20261115/report.json. The next
batch can resume those exclusions; it must continue distinguishing the
640x400 loading stall from the 320x200 cockpit rendering mismatch.
