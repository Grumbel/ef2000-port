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

### 2026-11-15 — preserve the tokenizer caller's output pointer

Corrected read_parser_token (0xe89c) to preserve all six non-EAX GPRs.
The original saves EBX/EDX/ESI and never changes ECX/EDI/EBP. Its previous
contract omitted the latter three, allowing compiled token length to leak
through ECX. Original caller 0x105a4 keeps its fourth output pointer in ECX
across the token reader and four integer reads; native mission acceptance
faulted writing through ECX=12 at0x1061a. Existing callers0xe95c/0xe980 also
confirm the reader's no-argument EAX return contract.

With the previous object and previous contract, all three differential cases
fail ECX preservation (11,5,0 instead of the input value). Recompiling with
the corrected contract passes all three, RAM and stack included. Watcom has
zero warnings/errors; all53 focused tests pass; tracker check4293 entries.
Commands: ef2000_wcc.py for parser_records; func_differential.py0xe89c with
explicit spec and --flat-segments; the same eight focused unittest modules
as the keyboard-selector entry above. Evidence: ignored
local/parser-token-preserve-20261116/{old-token.efpatch,token.efpatch,tests.log}.

The private-Xvfb real-input acceptance run now loads MISS3.MIS and enters
flight mode0x13, passing the former0x1061a fault. It then faults separately
at compiled address0x03045a29; flight remains unusable. See that directory's
game.log and output/{calls.log,report.json}. This is not a full replay pass,
so the function remains compiles. The second cockpit-mask bisection batch
also completed: unresolved,487 remaining candidates; its durable report is
local/cockpit-mask-bisect-02-20261115/report.json.

### 2026-11-16 — pass the unit to both weapon refresh paths

Corrected refresh_weapon_state_8cb60's calls to 0x8ca24 and 0x8cae8: both
require the unit pointer in EAX; 0x8ca24 also takes the signed loadout index
in EDX. The prior aliases omitted the pointer, so mission acceptance reached
init_weapon_slots_8ca24 with REC=0 and faulted at compiled 0x03045a29.
Original calls 0x8cb83/0x8cb90 and the callees' prologues confirm the inputs;
0x8d3c7 independently forwards unit EAX and loadout EDX to 0x8ca24.

Replaced the empty refresh probe with three authored differential cases:
loadout refresh, saved-slot refresh, and missing player. Synthetic callees
record both unit pointers and the signed index, while the rest of the
original refresh body exercises its stores. Previous C passes only the
missing-player case; corrected C passes all three, including RAM, stack and
preserved registers. Both alias declarations and their calls are corrected.
The entry remains compiles until full replay fidelity is established.

Watcom compiles game_helpers53 with zero warnings/errors; all 53 focused
unittests pass and decomp_status.py check reports 4293 entries. Commands:
ef2000_wcc.py for game_helpers53; func_differential.py 0x8cb60 with the explicit
spec and --flat-segments; the same eight focused unittest modules used for
the preceding tokenizer correction. Evidence: ignored
local/weapon-refresh-20261116/{refresh.efpatch,tests.log} and
local/weapon-refresh-old-20261116.efpatch.

The real SDL-input run on a private Xvfb display now accepts the mission,
enters flight and completes its100-second test at time_limit: launcher
exit0, guest return27, faults[], startup_checkpoint_verified=true. The
captured frame has terrain and HUD but missing cockpit/sky and distorted
HUD/text; flight fidelity is not established. See this directory's
game.log and output/{report.json,calls.log,framebuffer.png}. The complete
2597-redirect C++ cockpit replay is also running as an independent check.

### 2026-11-16 — full hybrid flight checkpoint after input repairs

At e367efaf, the full 2597-redirect, 342/382-module profile now reaches flight
in both native64 and Unicorn. The native real-input test is recorded above.
The C++ 1.5-billion-instruction replay ends on the mission-loading splash
(640x400, 15% progress), not at a guest fault. Extending the same run setup
to 4 billion instructions reaches 320x200 flight, ending at instruction_limit
with 42485 interrupts, 16000 ticks, 15933 IRQ0 deliveries and IP 0x110de6.
Flight-entry canaries 0x7eea0/0x810d0 each hit once; scaled draw entry 0xc565e
hits 38974 times. Terrain/HUD are visible; cockpit, sky and HUD/text fidelity
remain incorrect. These observations do not establish matching state or
M3 convergence. Updated decomp/README.md's stale 2026-11-14 launcher status.

Evidence: local/weapon-refresh-20261116/{current.json,cockpit-long.log,
cockpit-long.ppm,cockpit-checkpoint.log,cockpit-checkpoint.ppm}. Command:
private-Xvfb replay-long.sh in that directory, using the same four Return
events, guest DOS/VGA clocks and complete patch as replay.sh, with only
the instruction budget and artifact names changed. The third bounded
black-mask bisection batch completed with 284 candidates; the fourth resumes
the union of its persistent exclusions and newly excluded addresses.
Its 285-redirect initial trial reproduces the 320x200 rendering mismatch.
The separate e367efaf Nix package build remains in progress.

### 2026-11-16 — defer signals during native x87 observations

The e367efaf Nix package build failed its sole failing test in the native x87
comparison matrix: retained data pointer/selector were zero instead of the
synthetic 0xdecaf002/0x5678. All 525 tests completed, with 75 optional skips;
the package was not produced. The existing measured context-switch guard
cannot detect signal delivery and return without a context switch.

An authored host probe raises SIGUSR1 between FRSTOR and FCOM, then saves
the observation. On this host, 100/100 calls without measured switches lose
the retained data pointer/selector; 100/100 calls with signal delivery deferred
retain them. This establishes an unguarded host observation failure mode
consistent with the package failure; the failed sample's signal was not
recorded. The oracle now blocks signals immediately before dispatch and
restores the caller's mask after saving the result and restoring host x87
state. The context-switch guard and all numerical/metadata assertions remain.

A deterministic regression raises a real signal during dispatch and checks
that it is delivered afterward, including an unsupported-operation return.
It also checks restoration of a nonempty signal mask. Removing signal
protection makes the regression fail. Nine oracle/comparison tests pass in
the exact Nix Python environment, including all 2,356,224 comparison cases;
the strengthened nonempty-mask regression passes separately. Evidence:
local/x87-signal-probe-20261116/{probe.c,signal-evidence.json,
unprotected-test.log,tests.log,compare-evidence.json} and
local/hybrid-weapon-refresh-20261116-{build,runtime}.log.
Commands: Nix Python -m unittest tests.test_asm_x87_oracle
tests.test_asm_x87_compare with ASM_COMPARE_EVIDENCE set, plus the selected
signal regression. No recovered game code or comparison expectations changed.
The package build must be retried with this observation fix.

### 2026-11-16 — preserve the view-volume entry's registers and carry

Corrected 0xc473c: AX is the margin; ESI/EDI/EBP carry x/y/z. The old C ABI
used EDX/EBX/ECX instead. Original callers 0x15be2e/0x160147/0x16017b also use
CF from the final SHR, not just the EAX return. Watcom rejects EBP as an
ordinary C parameter, so this entry now has a small register/flag adapter;
it forwards the four stack arguments to the existing 0xceb40 guest entry
and returns directly after SHR. The C point-in-view-volume implementation
is unchanged. The GCC value-only model remains available for host probes.
Corrected the signature's preserved set to ESI/EDI/EBP; EBX/ECX/EDX are scratch.

Six authored differential cases record all four stack arguments and compare
CF, EAX, preserved registers, RAM and ESP. Old C fails all six; the actual
one-redirect adapter patch passes all six, including even/odd and high-bit
results. Watcom has zero warnings/errors; disassembly confirms no compiler
frame or flag-changing epilogue; all 53 focused tests pass; tracker check 4293.
Early failed-compilation artifacts with zero redirects are not validation.
Evidence: local/view-volume-20261116/{view-adapter.json,view-adapter.efpatch,
misc.asm,tests.log} and local/view-volume-old-20261116.efpatch.

The ten-redirect diagnostic profile now restores the cockpit and sky. Its
320x200 black mask matches all 64000 original pixels, previously 52.9281%
different. Raw colors are not claimed identical. It ends at instruction_limit,
6000 ticks, 5952 IRQ0 deliveries. See diagnostic-correct.{json,log,ppm,png} in
that directory. The first diagnostic mistakenly replaced the default exclusion
list and enabled unsafe entries; its early unsupported-interface result is
inconclusive. The corrected run retains the union of persistent exclusions
and trial03 exclusions. The sixth bounded bisection ended externally 143
before saving its final report; trial03 had reproduced the symptom with ten
redirects and its incremental exclusions were retained for this control.

The full native input run reaches flight but exposes a separate fault at
compiled 0x03044cf4 in strnlen_far_ed542, passed offset 1 from the formatter.
Full-profile rendering/usability remains pending; see game.log and output/
report.json. Commands: ef2000_wcc.py for misc_drafts1; func_differential.py
0xc473c with explicit spec and --flat-segments; the same eight focused
unittest modules; private-Xvfb diagnostic-correct.sh and native.sh.
The preceding fbebfbaa Nix hybrid build now completes successfully after the
signal observation fix, at local/hybrid-signal-guard-20261116. It predates
this view-volume correction.


### 2026-11-16 — Restore the integer conversion terminator

The 0x10e46f reverse digit buffer omitted the initial zero byte stored by
original 0x10e480. Copying the reversed digits therefore read below the
buffer until incidental stack data contained zero, corrupting formatter
output. Seed digits[0] with zero and start the count at one. The signed
0x10e4b8 wrapper now negates an unsigned magnitude, preserving original
32-bit NEG behavior for INT32_MIN without C signed overflow. Disassembly
and both formatter call sites confirm ECX/ESI/EDI/EBP preserved, with EBX
and EDX scratch; correct the signature rows accordingly.

Seven cases per entry cover zero, decimal, unsigned maximum, binary,
hexadecimal, negative and minimum signed inputs. Their stack is backed by
nonzero payload data at 0x180000 to expose the missing terminator rather
than hiding it in freshly zeroed scratch RAM. Both old entries fail all
seven RAM comparisons; the actual two-redirect repaired patch passes all
14, including destination guards, return pointer, preserved GPRs and ESP.
Watcom reports zero warnings/errors. All 53 focused tests pass and the
tracker check covers 4293 functions. Entries remain compiles until complete
replay equivalence is established. Evidence: local/integer-text-20261116/
{old-poison.log,old-signed.log,old.json,fixed.json,tests.log,compile.log}.
Commands: ef2000_wcc.py for runtime_libc; func_differential.py 0x10e46f and
0x10e4b8 with explicit specs and --flat-segments; the eight focused unittest
modules listed in the previous checkpoint; decomp_status.py check.

The preceding fbebfbaa Nix runtime build is independently confirmed to have
passed 526 tests (75 skips) in 365.436 seconds. Its successful hybrid package
predates the view-volume and integer conversion fixes; see
local/hybrid-signal-guard-20261116-runtime.log. A fresh package build and
interactive verification remain necessary.

The full 2597-redirect native SDL input run now reaches MISS3 flight and
finishes its 100-second limit without a guest fault (launcher exit 0).
Its frame shows the cockpit, HUD, terrain and sky. This clears the previous
strnlen_far_ed542 offset-1 fault in this run; it does not establish pixel
or gameplay equivalence. See local/integer-text-20261116/{native.sh,game.log}
and output/{report.json,framebuffer.png}. All generated data used the drive
overlay and input was confined to a private Xvfb display.


### 2026-11-16 — Pass shadowed text arguments on the caller stack

The fresh 38f929b0 Nix hybrid build succeeds: 526 tests, 75 skips, 359.151
seconds. Its private native SDL run reaches MISS3 flight, but an external
view input exposes a null format pointer in original 0xed133 (fault ed1b6).
The identical unpatched-original input run completes 100 seconds without a
fault. Xvfb's key mapping sends Alt make/break around the requested F4/F1;
these results cover those actual combinations, not plain function keys.

Temporary ignored diagnostic traps at ed133 confirm EDX=0 and identify the
call through original 48c90. The guest_fn_48c90 declaration said cdecl in a
comment but lacked EF_CDECL, so compiled callers supplied four GPR arguments
to an original stack-argument entry. Add EF_CDECL and correct the public
signature's stack cleanup from 16 to 0 (RET at 48d58). Original callers
912fb and 3b78 push the four fixed arguments; 912b8 explicitly removes 16.
The corrected zero-cleanup signature also enables the compiled variadic
48c90 direct entry; no unknown suffix is copied. The full profile now has
2598 redirects and 575712 arena bytes across 342/382 modules.
Rebuild all five direct-caller modules: misc_drafts1, small_wrappers4,
small_wrappers7, state_reset1 and game_helpers43.

Five authored 912b8 cases log the width argument and all four drawing stack
arguments, covering positive/expired/negative message counts, count wrap and
signed-width truncation. The one-redirect old patch passes only the two skip
cases; the repaired one passes all five, including RAM, ESP and preserved
registers. The existing separately compiled status fixture now also executes
this alias and checks caller cleanup. All 53 focused tests pass; all five
Watcom modules compile with zero warnings/errors; tracker check 4293.
Evidence: local/shadow-text-cdecl-20261116/{old.log,old.json,fixed.json,
tests.log,compile-*.log}. Commands: ef2000_wcc.py for those five modules;
func_differential.py 0x912b8 with explicit spec and --flat-segments; the eight
focused unittest modules; decomp_status.py check.

The full 38f929b0 replay also completes four billion instructions without a
fault (16000 ticks, 15933 IRQ0 deliveries), with cockpit and sky restored.
The latest pressed-button frame matches the original across all 640x400
pixels after 550 million instructions. See local/integer-text-20261116/
{cockpit-long.log,cockpit-long.png,pressed.log,pressed.ppm,verification-summary.json}.
The successful package is local/hybrid-integer-text-20261116; full build logs
are local/hybrid-integer-text-build-02-20261116/{build.log,runtime.log,exit-status}.
The first Nix client ended externally 143 mid-suite; its saved log is not a
failed-test verdict. Ordinary detached children did not survive tool cleanup;
the successful retry ran in a private tmux session with a durable exit status.

The repaired full-profile private native run completes its 100-second limit,
launcher exit 0, with no guest fault after the actual Alt+F4/Alt+F1 input.
Its captures show an external aircraft. Both the original and hybrid remain
in external view after the scripted F1 combination; this is not proof of a
return to the cockpit. The hybrid external view has a visible terrain
mismatch (much of its background is gray where the original shows terrain),
which remains to be isolated. See local/shadow-text-cdecl-20261116/
{native.sh,driver.log,game.log,external-window.png,returned-window.png} and
output/report.json. Original controls: local/native-original-camera-20261116/.
Diagnostic-only traps, never committed: local/native-camera-null-format-20261116/
and local/native-camera-null-caller-20261116/. The first packaged screenshot
helper hung after the fault destroyed its window; only that private test
session was terminated. Subsequent captures use a five-second timeout.
The fresh package built above predates this alias fix.

### 2026-11-16 — Allow flight replays through four billion instructions

Raise the replay manifest instruction ceiling from two to four billion:
the current full hybrid needs this budget to reach the external-view terrain
comparison. The existing runner accepts it, but manifest validation rejected
the bisection before execution. A synthetic replay regression runs both
original and patched samples at three- and four-billion instruction budgets,
checks their checkpoint records and frames, and rejects budgets above the
new ceiling. Commands: python -m unittest tests.test_replay_suite
tests.test_hybrid_bisect (23 tests pass); decomp_status.py check (4293).

The 3e374775 Nix hybrid build now succeeds (exit-status 0 in
local/hybrid-shadow-text-build-20261116/). The packaged private-Xvfb test
has loaded that revision with 2598 redirects; its plain F4/F1 check is still
running. The external-terrain bisection is also running, using an ignored
prototype of this validation-limit change. Rendering equivalence remains
unproven; the paired four-billion-instruction captures show about 88% terrain
pixels in the original versus 33% in the hybrid. Evidence lives under
local/external-view-replay-20261116/ and
local/external-terrain-bisect-02-20261116/.

### 2026-11-16 — Verify packaged flight camera controls with raw keys

The 3e374775 packaged native hybrid completes the 100-second private-Xvfb
camera run: launcher exit 0, guest time_limit/return 27, zero faults and
startup_checkpoint_verified=true. The xdotool F4/F1 injection still adds Alt
despite the private xmodmap change, so repeat using direct XTest keycodes
70/67. This repeat also completes successfully; port 0x60 records exactly
3e/be and 3b/bb after the two Return presses, without Alt's 38/b8. Inspected
screenshots show F4's external aircraft and F1's restored cockpit. The
external terrain remains visibly incorrect; this proves camera controls and
fault-free execution for this sequence, not rendering equivalence or full
playability. No game data was modified; generated files use isolated output
directories. Evidence: local/native-raw-camera-20261116/{run.sh,raw-key.py,
driver.log,game.log,verification-summary.json,external-window.png,
returned-window.png}; output/report.json and exit-status. The earlier
xdotool run is local/native-packaged-shadow-text-20261116/.

### 2026-11-16 — Exercise view-angle extraction during terrain diagnosis

Replace empty drafted probes for e9dcc and e9f84 with six authored cases
each: identity matrix, positive/negative direction components, values above
one and below minus one, and negative zero. Initialise the double matrix
and guard the output-angle region. Both isolated compiled entries match the
original in all twelve cases, including GPR/ESP state, the three output
doubles, and in-place clamping when required (24 or 32 compared RAM bytes).
The two-entry diagnostic link confirms two redirects; each function run
applies one. Keep both tracker states at compiles: the full external-view
terrain replay still fails and these cases are not a global fidelity gate.
Commands: hybrid_link.py --only-address 0xe9dcc --only-address 0xe9f84;
func_differential.py for each with its explicit spec and --flat-segments;
decomp_status.py check. Evidence: local/terrain-matrix-probes-20261116/
{view-angles.json,e9dcc.log,e9f84.log}.

The completed first external-terrain bisection retains 649 candidates after
1948 diagnostic exclusions. Its 650-redirect trial still differs in 65.7594%
of terrain-mask pixels. The next bounded pass runs under the committed
four-billion instruction limit and preserves both those exclusions and all
default unsafe-entry exclusions. See local/external-terrain-bisect-02-20261116/
report.json and local/external-terrain-bisect-03-config-20261116/.

### 2026-11-16 — Test projected corners with nonzero viewport inputs

Replace d8528's empty drafted probe with four authored viewport cases:
320x200, 640x400, negative centre/scale values, and maximum positive signed
scale. Initialise the six input words, viewport height and guarded output
region. The one-redirect patch matches the original in all four cases,
including 16 output bytes and GPR/ESP state. Keep the function at compiles
while full terrain fidelity remains unresolved. Commands:
func_differential.py 0xd8528 --spec decomp/tests/0x000d8528.json
--flat-segments with the isolated corners.efpatch; decomp_status.py check.
Evidence: local/terrain-matrix-probes-20261116/{corners.json,corners.log}.

### 2026-11-16 — Restore the shared textured span fallback

Original d3c50 branches at d3c62, d3c85, d3c8d and d3c9b enter
textured renderer body d3cb2; the C translation incorrectly returned.
Call original entry d3cb0 on those paths, which supplies the saved EBP
expected by its body. Declare renderer scratch-register clobbers and
correct d3c50 preservation to EBP only. Keep the neighboring renderer
original while its shared boundaries remain unresolved.

Seven authored cases exercise empty/short/wide spans, both depth limits,
and the separate d3900 dispatch (stubbed to record that call). The original
textured body runs and writes pixels in the fallback cases. The old patch
passes 2/7; the repaired isolated one-redirect patch passes 7/7 RAM and
GPR/ESP comparisons. Full 2598-entry replay is running, so retain compiles
status and do not yet claim terrain fidelity. Commands: ef2000_wcc.py
on game_helpers64.c; hybrid_link.py --only-address 0xd3c50;
func_differential.py 0xd3c50 --spec decomp/tests/0x000d3c50.json
--flat-segments; decomp_status.py check; git diff --check.
Evidence: local/textured-tail-20261116/{before,after}.log and span.json.

### 2026-11-16 — Verify restored terrain in the full translated profile

The d3c50 repair completes the deterministic four-billion-instruction
external-view replay with all 2598 translated entries enabled: exit 0,
status instruction_limit, 16000 ticks and no guest fault. The framebuffer
again shows the aircraft over continuous terrain instead of a large grey
missing-ground region. Against the same original replay, the diagnostic
G>R and G>B terrain-mask disagreement drops from 56.365625% to 2.6078125%;
terrain pixels rise from 21001 to 56332 (original 56347 of 64000). This
establishes restoration of the large missing-ground region, not exact
framebuffer equivalence or full flight fidelity. The current Nix package
build and native flight verification remain pending. Evidence:
local/textured-tail-20261116/{replay.sh,replay.log,replay.ppm,comparison.json}.

### 2026-11-16 — Verify the packaged terrain fix and camera round trip

The 3ce674d0 ef2000-hybrid Nix build completes with exit 0; native package
unittest discovery runs 527 tests in 392.646 seconds. A private Xvfb test
of that built launcher reaches flight, shows the external aircraft over
continuous terrain with raw F4, and restores cockpit/HUD with raw F1.
Port 0x60 confirms unmodified make/break codes 3e/be/3b/bb. At the configured
100-second limit the launcher exits 0, report status time_limit/returncode 27,
faults empty, startup_checkpoint_verified true. Original assets stay read-only;
all generated data use isolated output/overlay directories. This verifies this
flight/camera sequence, not broader playability or exact rendering fidelity.
Evidence: local/hybrid-textured-tail-build-20261116/{build.log,exit-status};
local/native-textured-tail-20261116/{run.sh,verification-summary.json,
external-window.png,returned-window.png,output/report.json}.

### 2026-11-16 — Restore captured ring-3 ASM input context

Add tools/asm_capture.py as the restoration prerequisite for EFCAP002
explicit-state differential replay. Require complete descriptor windows,
present zero-base ring-3 segments and 32-bit code/stack; reject unavailable
context, nonzero bases, VM86 and nested-task inputs. Restore original GDT/LDT
windows and cached LDTR tuple. Direct CS/SS writes do not restore CPL in
Unicorn: establish ring 3 with a one-instruction IRET from disposable ring-0
transition code outside guest RAM. Remove the transition page, restore table
bytes after descriptor loads, and require every captured register and all
physical raw-80/environment fields to round-trip. Segment caches are derived
from captured tables: the format does not record hidden caches, so do not
claim reconstruction of caches whose descriptors changed after loading.

Synthetic tests verify expand-down SS, actual privilege by reloading SS at
ring 3, full RAM/table preservation, nonzero empty x87 payloads, scratch-page
removal and unsupported-profile rejection. With EF_CAPTURE_UNICORN_TEST
pointing at the built capture fixture, tests.test_asm_capture,
tests.test_call_capture and tests.test_func_differential pass 16 tests without
skips (0.331 s). The pinned original 0x604 EFCAP002 also restores all 64 MiB
RAM byte-identically and round-trips registers/raw x87. Evidence:
local/capture-x87-restore-evidence-20261116.json binds capture and restorer
hashes. This completes context restoration for that profile, not execution of
captured inputs through generated C; that integration and segment-access
semantics remain required. Existing four corpus mismatches remain failures.

### 2026-11-16 — Execute captured ASM inputs through both oracles

Connect EFCAP002 restoration to asm_c_differential.compare and its isolated
worker. Captured inputs retain original RAM, GPRs, flags, physical x87 and
selectors; use the captured stack return PC as the stop address without
writing synthetic scratch objects or replacing the return word. Add repeatable
--capture inputs to the bulk CLI, validate source-image binding and translated
entry availability, and record captured results/hashes separately from prepared
probes. A captured mismatch contributes to the CLI's failing exit status.
Keep full RAM/GPR/flags/selector/raw-x87 comparison and original-fault versus
post-original C-failure classification unchanged. Report status 7 accurately
as unsupported segment profile.

The saved original 0x604 capture completes its original execution but the
existing ABI3 generated dispatcher rejects real ring-3 selectors with status 7.
This is a concrete mismatch, not a pass: the next requirement is descriptor-
based segment support in the C model, including access semantics rather than
allow-listing these selector numbers. Existing corpus failures remain recorded.
Evidence: local/capture-x87-generated-replay-20261116.json binds capture,
driver and existing generated library. Synthetic generated-C/fork tests verify
that captured ring-3 selectors are not flattened into the prepared profile and
that an original UD2 remains inconclusive. Captured generated-C integration is
now exercised, but passing captured state and game equivalence remain unproven.

Validation: EF_CAPTURE_UNICORN_TEST=local/codex-build/ef2000-call-capture-unicorn-test
Nix Python -m unittest tests.test_asm_capture tests.test_call_capture
tests.test_func_differential tests.test_asm_c_program: 25 tests pass without
skips in 8.996 seconds. git diff --check passes.

### 2026-11-16 — Implement descriptor-derived segment access checks

Add tools/asm_segments.py with an independent C access helper for a loaded
segment descriptor: decode base/effective inclusive limit/access/flags;
validate present/code/data/writable/readable rights, CPL/RPL/DPL and conforming
code, distinguish instruction/data/stack accesses, and check expand-up/down
bounds, 16/32-bit expand-down maxima and offset overflow. A rejected access
leaves its output address unchanged. Tests cover 30 explicit permission and
boundary cases plus descriptor decoding (including the captured expand-down
0x260 lower bound). GCC -Werror and Watcom -we compile the probe without
warnings. Nix Python unittest tests.test_asm_segments tests.test_asm_capture
passes five tests (0.266 s). Probe artifacts: local/asm-segments-20261116/.

This is the access-check prerequisite only; it is not yet wired into the
explicit-state ABI or generated scalar/stack/string/x87 memory paths. Do not
allow captured selectors based on their numeric values while bypassing those
checks. Existing captured replay status 7 remains a mismatch. Segment identity
must also be retained by the operand parser, which currently drops prefixes.

The earlier terrain bisection finishes naturally at its round limit with
three candidates d3844/d3c50/d4430; its old-object replay still fails. This is
consistent with the independently repaired d3c50 shared tail. Keep its report
as diagnostic evidence; the repaired full-profile/native results above are
the current runtime evidence. Report: local/external-terrain-bisect-04-20261116/.

### 2026-11-16 — Retain scalar memory operand segment identity

The operand parser no longer discards ES/CS/SS/DS/FS/GS prefixes. Store the
segment index as separate memory-operand metadata while retaining the same
offset expression and width. Default EBP/ESP base addressing selects SS;
a scaled EBP index with another base or no base selects DS. LEA's offset
expression remains independent of the segment. Across 56 assembled explicit
and implicit forms, parsed segment metadata agrees with independent ModRM/SIB
and prefix decoding by the x87 memory decoder.

Regenerate the complete current program from the pinned workspace/inventory.
All 4280 entry records, 2528 generated bodies and 2594 source/support/runtime
files are byte-identical to the preceding dispatch-return corpus; both report
hashes are 67670f0a17fb34cef3eca02b72e6f2b721ba33bec7fc0e52d1af3c34036bed7a.
Syntax support remains 4143. This metadata prerequisite does not yet enable
captured descriptors or change executed semantics. ABI and access-path wiring
remain required; no captured mismatch is promoted.

Validation: Nix Python -m unittest tests.test_asm_segments
tests.test_asm_c_program tests.test_asm_capture (14 pass, 8.716 s), then
tests.test_asm_stack_scalar tests.test_asm_stack_all (4 pass, 11.653 s).
Generation: tools/asm_c_program.py --workspace local/ghidra-stack-return-20261114
--inventory local/asm-dispatch-return-body-inventory-20261114 --output
local/asm-c-segment-operands-program-20261116. Cross-corpus byte comparison:
local/asm-segment-operands-evidence-20261116.json. git diff --check passes.

### 2026-11-16 — Wire descriptor-backed scalar execution into ABI 4

Extend explicit state with a segment-profile selector and six decoded cache
records; bump ABI 3 to 4 so old generated artifacts are rejected. Bind the
captured descriptor records without changing selectors. Scalar operand reads
and writes now check segment rights/privilege/limits; RET checks SS; every
instruction case checks full CS fetch length. Mutable immediate reads use
instruction-fetch permission, so execute-only CS remains valid. Reject
nonzero bases, long-mode descriptors and unknown profile values. Preserve and
compare descriptor-profile/cache configuration alongside full existing state.
Captured reports additionally bind restoration/decoder/capture-reader hashes.

Keep descriptor-mode PUSH/POP/PUSHA/POPA/ENTER/LEAVE/CALL, strings, far-pointer
loads, XLAT, WAIT/x87 and segment reloads explicitly unsupported until their
implicit accesses/table lookup are wired. Even a same-selector reload must
consult current table memory rather than assume its existing cache is current.
Legacy prepared-probe semantics stay under profile zero. Restoration now also
permits nonconforming execute-only 32-bit CS, verified by captured instruction
and immediate fetches without data-reading that code segment.

Synthetic captures pass full RAM/GPR/flags/selectors/raw-x87 comparisons for
DS/SS/FS and readable-CS loads, scalar stores/arithmetic, LEA with null FS,
and execute-only CS fetches. A generated cross-limit DS load returns status 7
before changing GPRs/flags/RAM. Unsupported balanced stack use and same-selector
reload remain explicit mismatches; original UD2 remains inconclusive. The
10-module regression (asm_capture, asm_c_segments, asm_inventory, asm_to_c,
asm_c_program, asm_c_x87, asm_wait, asm_stack_all, asm_stack_scalar,
asm_segments) passes 60 tests in 152.785 seconds; the final execute-only
immediate case is rerun successfully. A generated scalar Watcom -we probe
compiles with zero warnings/errors. git diff --check passes.

Regeneration preserves the full selected set: 2528 bodies, syntax support4143;
all2528 body sources change to include fetch/access guards. New runtime hash
8a87f1ccafb8487e6a516d340544e83c5c90e8ca630753617a6a8776db0112ab.
Evidence: local/asm-descriptor-scalar-20261116/{generation-evidence.json,
scalar.c,scalar.obj}; local/asm-c-descriptor-scalar-program-20261116/.
The full Watcom/hardware-oracle corpus plus the saved 0x604 capture is prepared
at local/asm-descriptor-scalar-corpus-config-20261116/command.json. Its result
is a separate gate; no captured-game or full-corpus passing claim is made.

### 2026-11-16 — Reproduce three additional main-menu regressions

Test the current 3ce674d0 game-code package on private Xvfb with fresh isolated
config/state/drive overlays. Mitarbeiter triggers the text-layout allocation
error "Gui SetWindowText ... No memory for text format data", writes
TACTCOM.$$$ in the overlay and later faults during teardown at original31a.
Multiplayer faults at compiled 3064a06, in fatal_exit_1e9 while reading message
pointer1; this is also downstream failure handling, not a proven root cause.
Kampagne opens its choice dialog, but Kampagne erstellen faults at compiled
30438ea in sort_nodes_by_order_e6848 after receiving a null list head. Do not
add null guards or suppress the errors without identifying the earlier defect.
A diagnostic relink using the packaged objects reproduces the exact patch SHA
 ecbe0f843d808c1631a1821427f2a3f759ab93cd7c186192357b1666b39dbbad,
so symbol-to-fault mapping is bound to the tested image.

The same three sequences in the unpatched native original succeed: credits
text is visible, Multiplayer opens network/IPX settings and creates COMMS.CFG
in its overlay, and campaign creation reaches the campaign map. All original
runs finish at their 90-second time limit with no guest faults; all hybrid
runs report unexpected_guest_fault (21). Startup checkpoints are verified for
all six. Two initial concurrent automated attempts raced onto the same private
Xvfb number; discard their navigation results. Valid reruns use distinct fixed
private displays191/192 and originals193/194/195; no desktop input is used.

Evidence: local/menu-regressions-20261116/summary.json;
local/native-menu-mitarbeiter-20261116/;
local/native-menu-{multiplayer,campaign}-20261116-02/;
local/original-menu-{mitarbeiter,multiplayer,campaign}-20261116/;
local/menu-fault-map-20261116/symbols.json. Five authored word-wrap probes
(short, long word, spaces, CR and tabs) pass against original e1e6c with an
actual one-redirect patch, so they do not isolate the credits regression.
Evidence: local/menu-textwrap-probes-20261116/{wrap.json,old.json,old.log}.
The deterministic credits error capture at local/menu-credit-capture-20261116/
is running; the ongoing ASM descriptor corpus remains undisturbed. These
menu regressions are now explicit open runtime work, not covered by the
previous successful flight/camera verification.

### 2026-11-16 — Recover the textured-span renderer in C

Following the owner's request to concentrate on decompiling game code, recover
d3cb0 as draw_textured_span_d3cb0 in decomp/src/texture_spans.c. Read original
ASM and the candidate export; the ASM's computed RET enters eight unrolled
pixel bodies d3d70..d3def, joins d3df0..d3dff and has a separate endpoint path
d3e00..d3e41. Retain original tracked ownership of 248 bytes rather than invent
new Ghidra body coverage. The fresh headless audit command cannot run because
its old 12.1.4 installation was removed; existing pinned inventory still confirms
the 248 owned bytes/layout. The complete shared-tail control flow is reviewed
from pinned disassembly, without changing ownership metadata.

Translate signed 32-bit by signed 32-bit IMUL/SHLD bit selection using a 64-bit product,
packed U/V fractional ADD/ADC carries, byte-wrapped texture indices, the
inclusive last pixel, the remainder-first groups of eight and observable
15b920 counter writes. Preserve MOV AL's address-byte replacement for the
shade page, including an unaligned page input. Width one reads both endpoints
directly. A one-instruction Watcom FS byte-load adapter retains the actual
guest texture segment; native C requires an explicit provider, never host TLS
or silently flat DS. Confirm no register arguments/return value and only EBP
preservation from the entry/tails; callers include the d2634 callback pointer
and d3c50 shared fallback. Name the accessed globals and remove this reviewed
entry's old mechanical exclusion. Keep compiles, without game-verification
promotion.

OpenWatcom builds with zero warnings/errors. Thirty authored fixed-point cases
match original RAM, EBP and ESP, covering negative/zero width, width one,
eight-pixel boundaries and signed/carry-heavy endpoints; the isolated patch
has one actual redirect and uses the existing __I8M runtime helper. The tracked
linker produces a byte-identical tested patch. Full static relink with the
packaged objects succeeds with 2599 redirects. No new UI runs are started.
Commands: tools/ef2000_wcc.py decomp/src/texture_spans.c;
tools/func_differential.py 0xd3cb0 --spec decomp/tests/0x000d3cb0.json
--flat-segments with the isolated patch; tools/hybrid_link.py;
tools/decomp_status.py check; git diff --check. Evidence:
local/texture-span-decomp-20261116/{span-reviewed.json,committed-spec.log,
tracked.json,full.json}. This adds recovered game C, not another validation
framework; the menu regressions and broader bulk goal remain open.

### 2026-11-16 — Recover perspective and transparent span code

Continue decompiling game code as requested, without starting new execution,
UI or replay tests. Add draw_perspective_transparent_span_d3e50 (1015 original
bytes) and draw_transparent_span_d4250 (250 owned bytes) to texture_spans.c,
including the dispatcher's shared affine body d42b1..d4363. Read the original
candidate exports and full pinned disassembly; control flow and arithmetic
are reconstructed from the ASM, not copied pseudo-C. The d429d CALL and
resource-handler pointer at 15b884 establish the renderer entry contracts:
d3e50 preserves no GPRs, d4250 preserves EBP, both consume globals and FS.

Preserve the perspective routine's sixteen-pixel blocks before its remaining-
count check, reciprocal depth writes, signed products and arithmetic shifts,
transparent texel skips, and low-word AND/DEC count operations. In particular,
CDQ precedes SHL EAX,4: use the unshifted sign as the dividend's high word.
An explicit one-instruction IDIV adapter preserves signed EDX:EAX division
and processor divide-by-zero/quotient-overflow behavior; ordinary wide C
quotient narrowing would not. The affine fallback retains packed byte
ADD/ADC carries and initial OR AL,FS:[BX], including the original shade-page
low byte for the first pixel. Preserve fallback entry into the shared tail
rather than return early. No special-case guards are added to alter original
behavior for invalid inputs or narrow direct perspective calls.

OpenWatcom compilation is warning/error-free. Inspect the generated Watcom
listing to confirm actual FS byte loads, IDIV EBX, register clobbers and the
dispatcher's calls. Name the added globals, record confirmed signatures,
remove these two reviewed mechanical exclusions, and retain compiles status
with static-review-only notes. There is no execution-equivalence promotion.
Static full relink succeeds: 2601 redirects, 343/383 modules, 577628 arena bytes.
Tracker remains 4293 functions; 3371 compiles and 863 blocked. Commands:
tools/ef2000_wcc.py decomp/src/texture_spans.c; wdis on its object;
tools/hybrid_link.py; tools/decomp_status.py check; git diff --check.
Artifacts: local/texture-span-decomp-20261116/transparent/{texture_spans.obj,
texture_spans.lst,texture_spans.abi.h} and transparent-full.json beside it.

### 2026-11-16 — Recover opaque perspective and masked affine spans

Add draw_perspective_span_d3900 (839 original bytes) and
 draw_masked_transparent_span_d4370 (173 owned bytes) to texture_spans.c.
Read their candidate exports and complete pinned disassembly, then reconstruct
C using the existing explicit FS/IDIV adapters. d3900 shares the perspective
arithmetic/control structure with d3e50 but writes every texel, including
zero. Preserve its register caching: both increments are cached for a full
sixteen-pixel block; the tail caches V once but reloads U for every pixel.
This differs from the transparent routine's per-pixel global reads and is
kept explicit. Preserve reciprocal and block-start writes, signed product/
division bits and low-word tail count operations.

For d4370 reuse the transparent affine core with an explicit masked mode.
Apply the dynamic 158f24 AND to the entire packed U word before every FS texel
read, including its fractional high word; masking only the texture index
would change later ADD/ADC carries. Retain the initial shade-byte OR behavior,
transparent skips, inclusive endpoint and EBP preservation. The unmasked
selector calls the same core with masked mode disabled. No GPR arguments or
results are invented; d3900 preserves no GPRs, d4370 preserves EBP.

Watcom compiles the 380-line renderer module with zero warnings/errors.
Inspect the generated listing and ABI pragmas: the full-word mask reload is
inside the pixel loop, FS reads remain explicit, and clobbers match original
entry contracts. Full static relink succeeds with 2603 redirects across
343/383 modules and 578370 arena bytes. Tracker check and git diff --check
pass; 3373 functions are compiles and 861 remain blocked. Both new entries
remain compiles/static-review-only, with no execution-verification promotion.
No new test, UI or replay runs are started.

Commands: tools/ef2000_wcc.py decomp/src/texture_spans.c; wdis on the object;
tools/hybrid_link.py; tools/decomp_status.py check; git diff --check.
Evidence: local/texture-span-decomp-20261116/opaque/{texture_spans.obj,
texture_spans.lst,texture_spans.abi.h} and opaque-full.json beside it.

### 2026-11-16 — Recover atlas-textured polygon setup

Recover d2a7c's 518-byte body as build_atlas_textured_object_d2a7c in
textured_objects.c. Read the complete pinned ASM and candidate export.
Correct the old blocker hypothesis: the vertex cursor is ESI, not EDI;
ECX supplies signed count. Return the advanced cursor in ESI and preserve
EAX/EBX/EDX/EDI/EBP exactly. d23d8's prologue confirms that processing the
render object preserves that advanced ESI; c448b installs this handler at
159184. The generated Watcom pragma is parm[esi][ecx], value[esi],
modify[ecx], and its prologue/epilogue visibly save/restore the same five
registers as the original.

Reconstruct direct/atlas coordinate paths with 9/16-bit shifts, modulo 32-bit
atlas bias, alternate span callbacks and the 16388-byte page stride. Retain
signed nonpositive-count handling, unsigned word vertex offsets, full dword
clip-mask merging, two normal-path flag stores around V assignment, and the
final render-object call. Add an asserted vertex-prefix layout without
claiming a full vertex extent; name the accessed globals. Keep compiles and
static-review-only evidence, without a new execution-verification claim.

Watcom compilation reports zero warnings/errors; inspect the generated
listing and ABI header. Full static relink succeeds with 2604 redirects,
344/384 modules and 578782 arena bytes. Tracker remains 4293 functions,
3374 compiles and 860 blocked. Commands: tools/ef2000_wcc.py
 decomp/src/textured_objects.c; wdis object inspection; tools/hybrid_link.py;
tools/decomp_status.py check; git diff --check. No new execution/UI/replay
tests are started. Artifacts: local/texture-span-decomp-20261116/
objects-builder/{textured_objects.obj,textured_objects.lst,
textured_objects.abi.h} and builder-full.json beside it.

### 2026-11-16 — Recover render-object bounds classification

Recover d2130's 416-byte body as classify_render_bounds_d2130 in
render_bounds.c after reading the complete original ASM and candidate export.
Confirm EAX=object with all other GPRs preserved; d23d8 and d299c consume
classification flags rather than an EAX result. Keep the existing bounds
calculators and the explicit four-register callback adapter. Plane predicates
receive EAX=context, EDX=x, EBX=y, ECX=z and return AL; the recorded ce498/ce550
predicate contracts preserve the adapter's nonvolatile registers. Do not
replace dynamic predicates with guessed geometric formulas.

Translate signed 2D overlap and partial-clip comparisons, five short-circuit
3D rejection tests using maximum Z, then five lower-bound tests using minimum
Z. Preserve the different final-plane order between those two groups, current
bounds reads after callbacks, old flag bits, and the separate near-plane flag
in byte+2d. No null-callback fallback or flag reset is added. Add an asserted
prefix view covering every accessed object-field offset and name clip-bound/
predicate globals. Remove the old unreviewed mechanical exclusion and retain
compiles/static-review-only status.

Watcom builds the 85-line module with zero warnings/errors. Listing inspection
confirms the EAX input contract, preservation and callback argument forwarding;
static full relink succeeds with 2605 redirects across 345/385 modules and 579380
arena bytes. Tracker check and git diff --check pass: 3375 compiles, 859 blocked,
4293 total. No new execution/UI/replay tests are started. Commands:
tools/ef2000_wcc.py decomp/src/render_bounds.c; wdis object inspection;
tools/hybrid_link.py; tools/decomp_status.py check; git diff --check.
Artifacts: local/texture-span-decomp-20261116/bounds/{render_bounds.obj,
render_bounds.lst,render_bounds.abi.h} and bounds-full.json beside it.

### 2026-11-16 — Recover the framebuffer-blended texture span

Recover d5a90 and its computed-return/shared pixel tails through d5c83 in
texture_spans.c. The table base at 15b91c is refreshed before width rejection;
lookup indices combine the FS texel as the high byte and the current frame
pixel as the low byte. Retain signed multiply/shift bits, packed ADD/ADC
stepping, remainder-first eight-pixel groups, inclusive end and counter writes.
Preserve the original width-one bug: the second endpoint receives the first
blended color without a second texture/frame/table lookup. Original callbacks
through 101688 and d2a7c require only EBP preservation and no register inputs.

Watcom compiles warning-free; generated listing confirms FS/background/table
read order, per-pixel table reloads and the one-lookup/two-store endpoint path.
Static full link succeeds with 2606 redirects, 345/385 modules, 579932 arena
bytes. Keep compiles/static-review-only status. No execution/UI/replay runs
are started. Evidence: local/texture-span-decomp-20261116/blended/
{texture_spans.obj,texture_spans.lst,texture_spans.abi.h} and blended-full.json.
Commands: ef2000_wcc.py on texture_spans.c; wdis; hybrid_link.py;
decomp_status.py check; git diff --check. Inventory: 3376 compiles, 858 blocked.

### 2026-11-16 — Correct fatal-exit message/code interpretation

The owner's quick-fight control crash again stops at compiled 3064a06 in the
fatal error handler while treating ffff0001 as a string. Read the original
1e9 stack flow: PUSH EDX saves the exit code, PUSH EAX saves the message;
POP EDX after opening CON retrieves the message for scanning/writing, and
POP EAX later retrieves the exit code. The C interpretation was reversed.
Correct fatal_exit_1e9 to EAX=message, EDX=code and carry those names/types
through report_error_11d258, its declarations and signature notes. Original
callers 10e438 and 11d695 independently load literal string addresses into EAX
and 1 into EDX. No error is suppressed and no invalid-pointer guard is added.

Watcom recompiles misc_drafts1.c with zero warnings/errors. Its listing now
captures the message from EAX into EDI and code from EDX into EBP, rather than
reading the code as a pointer. Full static relink with the recovered renderer
modules succeeds: 2606 redirects, 579924 arena bytes. Tracker/diff checks pass.
No new execution/UI/replay runs are started. Evidence:
local/fatal-message-abi-20261116/{objects/misc_drafts1.obj,misc_drafts1.lst,
full.json}; supplied saved run 535d7fe2685b4b00a02656838b2afdf2.

This repairs the secondary fatal-handler fault, not the error that requested
termination; quick-fight input usability remains unresolved. The supplied
Helsinki free-flight run 3fae26aee20d474687ca8f52f767a68f is distinct: its saved
fault at 30008dc is the first byte read in ascii_stricmp_9d524. The original
caller a788c (return PC a78a4) traverses child widgets via +91/+21 and supplies
child+3f. The invalid child is 14000000, producing pointer 1400003f and linear
fault 2400003f. Existing logs narrow this to an invalid UI child link before
name comparison; no source repair is claimed for that corruption. Preserve
both open issues while continuing original-code recovery.

### 2026-11-16 — Identify quick-fight null-code termination

Static inspection of the supplied quick-fight run
535d7fe2685b4b00a02656838b2afdf2 narrows the primary error. Its saved
FAULT_CONTEXT has BP=00120cfc. In the original, address zero jumps to 1de,
which pushes EBP, copies ESP to EBP, loads EAX=120cfc and writes DL=1 before
falling through to fatal_exit_1e9. The compiled handler in that saved run
copies EAX to EBP and EDX to EDI: EBP therefore retains the message address,
while EDI=ffff0001 retains the partially overwritten exit code. This matches
the null-code trap, whose message is "Null code pointer was called".
The upper bits of EDX are intentionally unchanged by MOV DL,1; do not
interpret ffff0001 as a corrupt message supplied by the flight caller.

The last logged keyboard bytes are E0,50 (extended down-arrow make). The
original IRQ handler fe61f queues/translates the key and updates its state;
its ordinary-key path has no indirect function call. This does not identify
the later flight callback that reached zero, nor establish that keyboard
initialization is at fault. The saved run has no guest-stack memory dump to
recover that caller; its nearby stack fields belong to the interrupt bridge
inside fatal_exit_1e9. Keep the flight-control regression open.

Correct the stale EDX message description in the function tracker to match
the already corrected EAX signature. Evidence: original payload disassembly
at 0,1de..220,fe61f..fe6f2; saved payload at 30649b4..3064a80;
existing calls.log tail. Validation: decomp_status.py check; git diff --check.
No new execution, UI or replay runs.

### 2026-11-16 — Recover scrolling-text preparation

Recover a82f0 (607 owned bytes) as prepare_scrolling_text_a82f0 in the new
scrolling_text.c. The former segment-operation blocker is the ten-entry
CS-relative font switch at a82c8; inspect every target and recover the
ordinary C dispatch. Review the original body, existing private pseudo-C
candidate and all six direct callers. EAX is the record input; all other
GPRs are preserved and no caller consumes EAX. Add the confirmed signature,
asserted record-prefix fields and named text-layout globals.

Preserve uppercase conversion, placeholder expansion, caret file dispatch,
80-byte line stride, signed low-word counters with full-register increments,
low-word total/width stores, fixed8/fixed6 and eight variable-width modes,
signed truncation-to-zero centering, clock update and cursor reset. Keep the
original unbounded line writes, read-before-length-check and unconditional
two-byte skip after CR. Invalid font modes retain old width/left-edge data.
No new null guards, capacity checks or line-ending normalization.

Watcom compiles warning-free; wdis confirms register saves, plain RET,
signed counter comparisons and the centering division sequence. Full static
link succeeds with 2607 redirects, 346/386 modules and580630 arena bytes.
Tracker/diff checks pass. Status remains compiles, not verified; no new
execution/UI/replay runs. Inventory:3377 compiles,857 blocked. Evidence:
local/scrolling-text-20261116/{scrolling_text.obj,scrolling_text.lst,
scrolling_text.abi.h,full.json}. Commands: ef2000_wcc.py on scrolling_text.c;
wdis; hybrid_link.py; decomp_status.py check; git diff --check.

### 2026-11-16 — Recover complete and progressive scrolling-text drawing

Recover a8718 (574 owned bytes) as draw_scrolling_text_a8718 in
scrolling_text.c. Review original ASM, the existing private pseudo-C
candidate, callers a8984/a89c0/a8ac5 and helper ABIs. The old implicit-EDI
blocker is spurious: the function saves registers, reads the global record
and initializes its line counters. No register inputs; preserve all six
non-EAX GPRs. Extend asserted record fields for shadow, progressive mode
and color; correct coordinate names to top/left from the dispatcher ABI.

Recover both complete-line drawing and clock-paced character reveal, with
optional one-pixel black shadows, font-height advancement, signed-word
progress/length comparisons, wrapping stores and truncation-to-zero signed
half-tick advancement. Retain the unsigned comparison against the signed
remaining count, unchecked partial-line writes and the final draw even for
an empty prefix. Keep the original local-frame slots +50/+54/+58 explicit,
including their placement after the 80-byte partial buffer. No buffer clear,
capacity guard or malformed-state normalization is introduced.

Watcom compiles warning-free. Wdis confirms six register saves/restores,
plain RET, signed counter comparisons, unsigned partial-copy comparison
and signed division sequence. Full static relink:2608 redirects,
346/386 modules,581208 arena bytes. Tracker/diff checks pass. Status remains
compiles; no new execution/UI/replay runs. Inventory:3378 compiles,856 blocked.
Evidence:local/scrolling-text-draw-20261116/{scrolling_text.obj,
scrolling_text.lst,scrolling_text.abi.h,reviewed.json}. Commands:
ef2000_wcc.py on scrolling_text.c; wdis; hybrid_link.py;
decomp_status.py check; git diff --check.

### 2026-11-16 — Recover LBM picture presentation with optional text

Recover a8a38 (411 owned bytes) as show_menu_picture_a8a38 in the new
menu_picture.c. Inspect original disassembly, the private pseudo-C candidate,
the sole original direct caller a9f5c, and recovered opcode1 dispatch. The
old callee-flags blocker is spurious: every conditional branch has an
explicit local CMP/TEST. Confirm EAX record input, preservation of all other
GPRs, no consumed return and plain RET. Add asserted unaligned record fields
and named path-buffer, palette and state globals.

Preserve filename copy plus .lbm suffix, buffer selection/load/page order,
nonprogressive text before the first page flip, progressive preparation
after fade-in, per-frame progressive redraw, signed duration and completion
comparisons, mandatory first wait iteration, zero-modifier key polling and
post-wait fade-out. Keep repeated record/global reads across callbacks.
The original stack flag stays zero: panel reset/release and duration override
branches are unreachable and no such side effects are added. Clear137205
even when the entry presentation-disabled flag causes an early return.

Remove the old unreviewed-mechanical exclusion for this manually recovered
body. Watcom compiles warning-free; wdis confirms preserved registers,
plain RET, signed duration/completion checks and explicit key-result test.
Full static relink:2609 redirects,347/387 modules,581706 arena bytes.
Tracker/diff checks pass. Status remains compiles; no new execution/UI/replay
runs. Inventory:3379 compiles,855 blocked. Evidence:
local/menu-picture-20261116/{menu_picture.obj,menu_picture.lst,
menu_picture.abi.h,reviewed.json}. Commands:ef2000_wcc.py on menu_picture.c;
wdis; hybrid_link.py; decomp_status.py check; git diff --check.

### 2026-11-16 — Recover data-playback initialization before animation caller

Inspect adjacent menu animation handler a8bd4, then recover its blocked
initializer eac48 (453 owned bytes) as initialize_data_playback_eac48 in
new data_playback.c. Review original ASM, private pseudo-C candidate and
four callers. EAX is signed argument count, EDX the vector, EAX result0;
preserve EBX/ECX/ESI/EDI/EBP. The indirect-call blocker is the option6 hook:
original sets EAX=-1 and pushes no arguments, so use the existing EAX adapter.
Keep empty-string substitution and the original nonzero callback check.

Recover ten-entry option copying, repeated positive interval parsing,
first-block load/decompression, planar/run-list decoder selection, initial
seek position, short-circuited palette comparison and four page toggles,
conditional callback, palette copying/fade choices, pass2 and clock latch.
Preserve low-word palette-result tests and interval stores. Name touched
globals without inventing decompression-buffer capacities. Original helpers
with four unused register placeholders are reviewed before supplying those
placeholders; all meaningful inputs remain the original stack arguments.

Watcom compiles warning-free; wdis confirms signed option bounds, repeated
parse, register callback argument, stack helper cleanup, preserved register
contract and zero return. Full static link:2610 redirects,348/388 modules,
582282 arena bytes. Tracker/diff checks pass. Status remains compiles; no new
execution/UI/replay runs. Inventory:3380 compiles,854 blocked. The larger
a8bd4 caller remains untranslated. Evidence:local/data-playback-20261116/
{data_playback.obj,data_playback.lst,data_playback.abi.h,full.json}.
Commands:ef2000_wcc.py on data_playback.c; wdis; hybrid_link.py;
decomp_status.py check; git diff --check.

### 2026-11-16 — Recover menu animation presentation and cleanup

Recover a8bd4 (937 owned bytes) as show_menu_animation_a8bd4 in new
menu_animation.c, following the recovered eac48 initializer. Review original
ASM, private pseudo-C candidate, sole direct opcode0 caller a9f4f and the
recovered dispatcher contract. EAX=source record; preserve all six other
GPRs; no consumed result; plain RET. Add asserted 23-byte animation and
24-byte text prefixes, plus original local-frame slot offsets. Name option
buffers and the panel gate without changing existing global meanings.

Recover the capability/name gate, mode1 transition, early disabled return,
record/text/data cloning, decimal interval formatting, text setup, palette
fades, audio/panel gates, seven-option initialization, both playback loops,
key waits and cleanup/free order. Preserve signed interval and duration
fields as distinct inputs. Full playback stops only after pump and text
completion; timed playback retains the flag-dependent duration reset/hold
rule. The original full-playback audio branches have identical completion
tests and share that test in C. No allocation guard, terminator byte or
initialization of unwritten local slots is introduced. Retain the mode2
restore on the capability skip, and its bypass on the disabled return.

Resolve the segment-operation blocker with a narrow adapter for the actual
MOVSD/MOVSD/MOVSW copy, preserving live DS/ES/DF rather than flattening the
operation. Watcom compiles warning-free; wdis confirms those instructions
without CLD, six register saves/restores, signed comparisons and plain RET.
Full static relink:2611 redirects,349/389 modules,583404 arena bytes.
Tracker/diff checks pass. Status remains compiles; no new execution/UI/replay
runs. Inventory:3381 compiles,853 blocked. Evidence:
local/menu-animation-20261116/{original.asm,menu_animation.obj,
menu_animation.lst,menu_animation.abi.h,full.json}. Commands:ef2000_wcc.py on
menu_animation.c; wdis; hybrid_link.py; decomp_status.py check; git diff --check.

### 2026-11-16 — Recover script branch execution and interactive prompt

Recover a9130 (794 owned bytes) as execute_script_branch_a9130 in new
script_branch.c. Inspect original ASM, both CS switch tables, private
pseudo-C candidate, sole direct opcode3 caller a9f76 and recovered dispatcher.
Confirm EAX record input, all six other GPRs preserved, no consumed result
and plain RET. Add asserted unaligned record and original 168-byte local
frame fields; name pending/current instruction and unchecked return storage.

Recover unconditional branches, the automatic equal/not-equal/not-greater
helper gates, prompt palette setup, variable/operation/target descriptions,
formatting, repeated strlen uppercase loop, variable-value lookup, IRQ-key
polling and Space/Enter behavior. Resolve the segment blocker with narrow
MOVS-width adapters retaining DS/ES/DF; no flat-copy substitution or CLD.
Keep default-case local bytes unwritten. Direct and prompted subroutine
branches have different count/table store orders; retain both, including
unchecked-overflow aliasing. Do not add bounds checks or normalize inputs.

Watcom compiles warning-free; wdis confirms six register saves/restores,
plain RET, original literal-copy widths, repeated key loads and distinct
return-table store orders. Full static link:2612 redirects,350/390 modules,
584494 arena bytes. Tracker/diff checks pass. Status remains compiles; no
new execution/UI/replay runs. Inventory:3382 compiles,852 blocked. Evidence:
local/script-branch-20261116/{script_branch.obj,script_branch.lst,
script_branch.abi.h,full.json}. Commands:ef2000_wcc.py on script_branch.c;
wdis; hybrid_link.py; decomp_status.py check; git diff --check.

### 2026-11-16 — Recover registered callback cleanup

Recover abec4 (84 owned bytes) in registered_callbacks.c. Original ASM and
callers ac429/ac78c establish no inputs or consumed result, saved
EBX/ECX/EDX and plain RET. Replace the earlier blanket indirect-call blocker
with a narrow register adapter: EAX=0, ECX=record offset, EBX/EDX loop state.
Keep the callback-returned EBX/EDX values, signed index comparison, live
unsigned-word count reload, repeated target read, wrapping increments and
count-clear before the 0x150-byte fill. No bounds guards added.

Watcom compiles without warnings. Wdis confirms callback setup, loop state,
count reload, cleanup parameters and saved registers; no 64-bit helper is
needed. Full static link:2613 redirects,351/391 modules,584624 arena bytes.
Tracker check and git diff --check pass. Status compiles; no execution/UI
runs or equivalence claim. Inventory:3383 compiles,851 blocked. Evidence:
local/registered-callbacks-20261116/{registered_callbacks.obj,
registered_callbacks.lst,registered_callbacks.abi.h,full.json}. Commands:
ef2000_wcc.py on registered_callbacks.c; wdis; hybrid_link.py;
decomp_status.py check; git diff --check.

### 2026-11-16 — Recover parser-state restoration

Recover ab47c (386 owned bytes) in parser_restore.c. Original ASM clears
EDI before use, disproving its earlier implicit-input blocker. Callers
286a9/28819 establish EAX=opened handle, preserved EDX for close, discarded
EAX result and plain RET. Read thirty inline-dword/indirect-record variable
slots, restore the subroutine return table and current instruction through
signed-word group/item indices, restore linked nodes, then read and apply
the optional order target and set presentation state137205.

Retain the asserted 12-byte frame and partial two-byte slot reads, unchecked
list walks, -1 current-position sentinel, low-word loop comparisons and
count sample before each unchecked return-table store (overflow can alias
count). Do not initialize unused local bytes or add guards. Remove ab47c's
old mechanical exclusion only after this manual review.

Watcom compiles without warnings; wdis confirms callee-cleanup stack reads,
word-index walks, pre-store count sample, message store order and plain RET.
Full static link:2614 redirects,352/392 modules,585138 arena bytes. Tracker
check and git diff --check pass. Inventory:3384 compiles,850 blocked. Status
compiles only; no new runtime/UI/replay runs. Evidence:
local/parser-restore-20261116/{original.asm,parser_restore.obj,
parser_restore.lst,parser_restore.abi.h,full.json}. Commands:ef2000_wcc.py
on parser_restore.c; wdis; hybrid_link.py; decomp_status.py check;
git diff --check.

### 2026-11-16 — Recover positioned page-record startup

Recover a04b4 (344 owned bytes) in page_positioned_record.c. Original ASM
and sole direct caller9f521 establish signed AX/DX record/point indices,
five saved registers, EDX scratch and plain RET. Build the page rotation,
transform the selected point, add wrapping world offsets, view-test the
position and initialize script fields before starting160255. Always run
the final positioned-record hook, including failed view tests.

Retain the asserted24-byte frame, overlapping dword coordinate/angle reads,
AL-only view result, repeated repeat-byte read around the rotation-pointer
store, callee-cleaned stack lookup132b14/EAX result1600c0, and final
EAX/EDX/EBX/ECX callback arguments. Reuse established callback adapters;
no new runtime adapters or input guards. Remove the old a04b4 exclusion
after manual review. Name the touched page/script fields and hooks.

Watcom compiles without warnings; wdis confirms signed index strides,
dword shifts, frame layout, lookup-result store and callback adapters.
Full static link:2615 redirects,353/393 modules,585554 arena bytes. Tracker
and git diff --check pass. Inventory:3385 compiles,849 blocked. Status
compiles; no runtime/UI/replay runs or execution-equivalence claim.
Evidence:local/page-positioned-record-20261116/{original.asm,
page_positioned_record.obj,page_positioned_record.lst,
page_positioned_record.abi.h,full.json}. Commands:ef2000_wcc.py on
page_positioned_record.c; wdis; hybrid_link.py; decomp_status.py check;
git diff --check.

### 2026-11-16 — Correct view-test stack argument order

Static review of the next page routine exposed an existing declaration
error at160122/160156 that the a04b4 recovery in165d3508 inherited. Their
original ENTER frames consume margin,Z,Y,X,out at ESP+4..20 and RET20;
the old declaration reversed out and margin and the tracker incorrectly
claimed four register inputs plus four-byte cleanup. Correct both C
signatures/definitions, stack metadata, preserved registers and a04b4's
call. Correct compiler-abi.md's preceding stack-order description too.

Original callers9e140/9fcd7/a057a and9fa11/a07d3 confirm the convention.
Watcom compiles both affected modules without warnings. Wdis confirms
out/X/Y/Z/margin push order in a04b4 and correct stack loads/RET20 in both
view-test bodies. Keep their existing exclusions pending individual
arithmetic/implicit-register fidelity review; update the exclusion reasons
and remove tracker references to obsolete register-only drafted probes.

Full static link:2615 redirects,353/393 modules,585570 arena bytes. Tracker
check and git diff --check pass. No execution/UI/replay runs. Evidence:
local/view-test-stack-abi-20261116/{page_positioned_record.obj,
page_positioned_record.lst,misc_drafts1.obj,misc_drafts1.lst,full.json}.
Commands:ef2000_wcc.py on both affected modules; wdis on both objects;
hybrid_link.py; decomp_status.py check; git diff --check.

### 2026-11-16 — Recover rotated page-record startup

Recover a08c8 (356 owned bytes) in page_positioned_record.c. Original ASM,
pseudo-C and sole direct caller9e8e3 establish signed AX index, six saved
registers and plain RET. Retain visibility gates, hook-transformed position,
corrected stack view test, wrapping adjusted rotation and optional offsets,
script lookup EAX result and original160329 queue-appender call. Inspect
160329's entry to confirm no register inputs; do not misclassify this call
as a C runtime helper. Remove a08c8's old exclusion after manual review.

Keep the asserted24-byte data layout, offset preload before angle stores,
repeat-byte reread between second/third stores and original lookup contract.
Generated listing exposed missing shared callback/view-test clobbers:
ECX held the record pointer across calls that overwrite it. Add matching
shared pragmas for guest_callptr_eaxedx_x,guest_callptr4_x and both view
tests. Rebuilt a08c8 keeps record in ESI across hooks/view test. Recompile
all users:page_positioned_record,misc_drafts1,script_branch,load_planes1,
state_reset1 and guest_callptr1. These are declaration corrections to
existing adapters; no new runtime adapter or guards.

Compilation succeeds; wdis review confirms register saves, signed gates,
stack push order, preserved record pointer and rotation-store sequence.
Full static link:2616 redirects,353/393 modules,586052 arena bytes. Tracker
and git diff --check pass. Inventory:3386 compiles,848 blocked. Status
compiles only; no runtime/UI/replay runs. Evidence:
local/page-rotated-record-20261116/{original.asm,page_positioned_record.obj,
page_positioned_record.lst,full.json} and rebuilt affected objects there.
Commands:ef2000_wcc.py on all six affected modules; wdis on
page_positioned_record.obj; hybrid_link.py; decomp_status.py check;
git diff --check.

### 2026-11-16 — Recover offset page-record startup

Recover a0704 (452 owned bytes) in page_positioned_record.c. Original ASM,
pseudo-C and sole caller9e8cb..9e8d6 establish signed AX/DX indices, four
saved registers, untouched EBP, EDX scratch and plain RET. Store selected
point Y, run the X/Z hook, add wrapping word offsets, view-test integer
coordinates through160122 and initialize optional record rotations before
script startup. Always invoke the final positioned-record hook.

Retain original overlapping dword reads interleaved with coordinate word
stores,12-byte position storage, preloaded optional angle offsets, repeat
byte test before default-one store and reread between second/third angle
stores, stack lookup/EAX result and final register arguments. No new
adapters, bounds guards or execution tests. Remove old a0704 exclusion
only after manual assembly/caller review; name work-coordinate/offset data.

Watcom compiles warning-free; wdis confirms signed strides, hook arguments,
interleaved stores/reads, correct five-argument stack order, saved record
pointer and final callback. Full static link:2617 redirects,353/393 modules,
586660 arena bytes. Tracker/diff checks pass. Inventory:3387 compiles,
847 blocked. Status remains compiles; runtime fidelity unverified.
Evidence:local/page-offset-record-20261116/{original.asm,
page_positioned_record.obj,page_positioned_record.lst,
page_positioned_record.abi.h,full.json}. Commands:ef2000_wcc.py on
page_positioned_record.c; wdis; hybrid_link.py; decomp_status.py check;
git diff --check.

### 2026-11-16 — Recover page-value bytecode interpreter

Recover a0c04 (340 owned bytes) in page_value_script.c. Original ASM,
pseudo-C, CS jump table a0be4 and callers9e023/9e218 establish signed
AX/DX record/channel indices, five saved registers, EDX scratch and plain
RET. The prior segment blocker was the pinned switch table; translate its
eight cases, with5/default stopping. Assert29-byte record layout including
eight cursors/values, rotation index and repeat byte.

Preserve byte/word wraparound, value-store before cursor increment,
entry-only zero check, relative branches, cursor store before potentially
aliasing text operand reads and interpreter continuation after recursive
page-text calls. Do not normalize index signedness or add loop bounds.
Remove old a0c04 exclusion after manual table/body/caller review.

Watcom compiles warning-free. Wdis confirms opcode cases, signed cursor
addressing, post-store text operands, recursive call arguments and RET.
Full static link:2618 redirects,354/394 modules,587044 arena bytes. Tracker
and git diff --check pass. Inventory:3388 compiles,846 blocked. Status
compiles only; no execution/UI/replay runs. Evidence:
local/page-value-script-20261116/{original.asm,page_value_script.obj,
page_value_script.lst,page_value_script.abi.h,full.json}. Commands:
ef2000_wcc.py on page_value_script.c; wdis; hybrid_link.py;
decomp_status.py check; git diff --check.

### Direct-coordinate page record (0xa2728)

Recovered `start_coordinate_page_record_a2728` in
`decomp/src/page_positioned_record.c` (399 original bytes). Sole direct
caller a26fc passes AX=record and DX/BX/CX=word coordinates, retaining
ESI/EDI afterward. Original saves ESI/EDI, leaves EBP untouched, clobbers
EAX/EBX/ECX/EDX and uses plain RET. No second direct caller exists in the
pinned inventory. The four-register legacy guest alias remains compatible.

Preserve signed word callback arguments, word offsets, interleaved stores
and overlapping dword high-word reads, then shift those coordinates left16
for the stack view test160156(margin,Z,Y,X,out). Preserve the uninitialized
world output on a rejecting view path, unconditional final hook, rotation
operand reads before their corresponding stores, and repeat-byte reread
after publishing the rotation pointer. The known view-test exclusions stay
in place; this caller does not establish their arithmetic fidelity.

Watcom compilation is warning-free. Wdis review confirms the four input
registers, preserved ESI/EDI/EBP, left16 shifts, stack push order, record and
rotation pointers retained across callbacks, ordered angle stores, repeat
reread and plain RET. Full static link:2619 redirects,354/394 modules,
587552 arena bytes. Tracker and git diff checks pass. Status compiles only;
no execution, UI or replay runs, and no claim of crash resolution.
Evidence: local/page-coordinate-record-20261116/{original.asm,
page_positioned_record.obj,page_positioned_record.lst,full.json}.
Commands: ef2000_wcc.py on page_positioned_record.c; wdis;
hybrid_link.py; decomp_status.py check; git diff --check.

### Page-point callback request (0xa1590)

Recover `submit_page_point_a1590` (479 original bytes) in
`decomp/src/page_point_request.c`. Call sites9ebea and9ec08 provide AX=point,
DX=channel, EBX=angle pointer, CX=spread (zero at the first site). Both retain
ESI, advance script input afterward and ignore EAX. Original saves
ESI/EDI/EBP; EAX/EBX/ECX/EDX are scratch and return is plain RET.

The previous indirect-call blocker is resolved by the actual instruction
sequence: hook132ad4 consumes EAX pointing to an18-byte request and returns
AL. It does not consume the five parameters suggested by private decompiler
output. Assert request size and pitch/identifier/channel offsets; publish
all nine words in original order. Use a narrow register-call adapter with
all four scratch registers declared; game logic remains C.

Preserve signed byte count comparison, sign-bit gate, signed percent scale,
two generator calls and signed remainder for nonzero spread, word offset
wraparound, ordered coordinate stores and relative-coordinate reads,
post-store angle reads, page yaw flag bits, live request identifiers and
acceptance-only byte increment. No bounds checks or fallback behavior added.
Named the scale-pointer and request-hook globals.

Watcom compilation is warning-free. Wdis confirms signed comparison and
division, two generator calls, word stores, request field offsets, EAX
callback argument, AL test, ESI/EDI/EBP saves and plain RET. Full static link:
2620 redirects,355/395 modules,588124 arena bytes; tracker and diff checks
pass. Inventory3390 compiles,844 blocked. Status compiles only, with no
execution/UI/replay runs or crash-resolution claim. Evidence:
local/page-point-request-20261116/{original.asm,callers.asm,
page_point_request.obj,page_point_request.lst,full.json}. Commands:
ef2000_wcc.py on page_point_request.c; wdis; hybrid_link.py;
decomp_status.py check; git diff --check.

### Chain-positioned page startup (0xa0228)

Recover `start_chain_page_record_a0228` (651 original bytes) in
`decomp/src/page_positioned_record.c`. Sole direct caller9e881 passes AX/DX
script words for record/point, retains ESI and ignores EAX/EDX. Original
saves EBX/ECX/ESI/EDI/EBP and uses plain RET. Confirm the lookup helper's AX/DX
key comparison against the original a0018 body; its AX result is sign
extended before the -1 gate. No second direct caller exists in the pinned
inventory.

Use the signed returned index at78-byte stride, signed24.8 position offsets
and low-word rotation slots. Assert entry size and position/rotation
offsets, name the table base without inventing its capacity. Preserve the
interleaved coordinate stores and overlapping dword reads, 16.16 view-test
inputs, default-angle stores before entry rotation reads, publication of
script globals between angle stores, repeat test before Z store and repeat
reread after optional X/Y stores. Missing-chain return bypasses all hooks;
view rejection still reaches the final world callback with the original
uninitialized local if the view routine leaves it untouched. Keep the
existing view-test arithmetic exclusions.

Watcom compilation is warning-free. Wdis confirms lookup arguments, signed
result gate, callee-preserved locals across callbacks, SAR8 offsets, SHL16
view inputs and stack argument order, ordered angle/global stores, repeat
reread and plain RET. Full static link:2621 redirects,355/395 modules,
588940 arena bytes. Tracker/diff checks pass. Inventory3391 compiles,
843 blocked. Static review only; no execution/UI/replay runs or claim of
crash resolution. Evidence: local/page-chain-record-20261116/{original.asm,
callers.asm,page_positioned_record.obj,page_positioned_record.lst,full.json}.
Commands: ef2000_wcc.py on page_positioned_record.c; wdis;
hybrid_link.py; decomp_status.py check; git diff --check.

### Rotated script queue appender (0x160329)

Recover `append_rotated_script_entry_160329` (200 original bytes) in
`decomp/src/script_queue.c`. Sole direct caller a0a1d uses no register
arguments/results. Original saves ESI/EDI/EBP, clobbers the four scratch
registers and uses plain RET. Remove its obsolete unreviewed-mechanical
exclusion after reviewing the body, caller, vector helper and enqueue entry.

Keep all global reloads and parallel-table writes in their original order,
including loading all three rotation words before any rotation-table store.
Do not clamp the unchecked word slot. Name table bases as single elements
with documented strides rather than inventing capacity bounds.

Inline the arithmetic of implicit-register1600c4 as C: unsigned wrapping
absolute value, add0x8000, arithmetic right16, wrapping square/sum, unsigned
integer_sqrt, and restore the original Z sign to the length. Review e2b60
preservation of EDX to confirm the sign survives. Final key uses wrapping
signed_length+bias plus SAR2(abs(bias)); retain the INT32_MIN case. Avoid
the old pointer-parameter vector draft ABI.

Call original c41b0 through an explicit EAX-key declaration. Its blocked C
model still has an unsigned count comparison where the original uses signed
JGE, and unresolved self-modifying overflow policy; neither that model nor
the implicit-register vector draft is enabled by this commit. Their wider
fidelity review remains pending.

Watcom compiles warning-free. Wdis confirms preserved registers, ordered
loads/stores, SAR16 rounding and wrapping squares, sign restoration,
rotation preloads, SAR2 bias and EAX enqueue argument. Full reviewed static
link:2622 redirects,356/396 modules,589320 arena bytes. Tracker and diff
checks pass; inventory3392 compiles,842 blocked. Status compiles only; no
execution/UI/replay runs or crash-resolution claim. Evidence:
local/script-queue-20261116/{original.asm,vector.asm,callers.asm,
script_queue.obj,script_queue.lst,reviewed.json}. Commands:
ef2000_wcc.py on script_queue.c; wdis; hybrid_link.py;
decomp_status.py check; git diff --check.

### Renderer enqueue draft: signed gate and store order

Review original c41b0..c4259 and caller sites160320/1603e8. Both supply AX
key and consume no returned scratch values. The original increments a word,
reloads it, then uses signed CMP/JGE against0x800. Correct the existing
`renderer_enqueue_c41b0` C model to compare `(int16_t)count`, preserving
negative counts' insertion path and word wraparound. The old unsigned gate
incorrectly routed0x8000..0xffff to overflow.

Restore the live post-increment count reload, original tag-before-key
insertion stores and live count reload for the overflow quicksort argument.
Do not enable this renderer model: its existing blocked status and hybrid
exclusions remain. The original instruction uses XCHG, while the draft
models the exchange as separate loads/stores; self-modified branch fetch,
segment effects, scratch-register behavior and sorting callee ABI still
need recovery. New script appender160329 continues calling originalc41b0.

Watcom compilation of renderer_sort_queue.c is warning-free. Wdis shows a
signed JL insertion gate, count reloads and ordered tag/key stores. Tracker
and git diff checks pass. No new hybrid redirect or runtime verification;
inventory remains3392 compiles,842 blocked. Evidence:
local/renderer-enqueue-review-20261116/{original.asm,renderer_sort_queue.obj,
renderer_sort_queue.lst}. Commands: ef2000_wcc.py on renderer_sort_queue.c;
wdis; decomp_status.py check; git diff --check. No execution/UI/replay runs.

### Page-name and numeric-ID table loader (0x9da80)

Recover `load_page_name_table_9da80` (365 original bytes) in
`decomp/src/page_name_table.c`. No register inputs; all six non-EAX GPRs
preserved. Sole direct caller9d924 ignores EAX but uses retained BL for the
initialization flag store immediately afterward. Original calls9d990 before
saving registers; inspect that callee's preservation too. There is no second
direct caller in the pinned inventory.

Preserve the quoted-string table gate, first loader call, count reset,
path construction, zero-size exit, temporary size+1 allocation, returned
ECX terminator base and returned EBX first-pass cursor. A narrow callback
adapter sets EAX=name and EDX/EBX/ECX=buffer and captures ECX/EBX as two words;
the loader's original9cbc4 target saves those registers and has no implicit
ESI input. Parsing still restarts from the allocation base. Assert the
148-byte frame with144-byte unchecked token storage and signed identifier
at144; leave extra table allocation slots uninitialized. Preserve live
count reloads, wrapping allocation sizes, signed word decimal conversion,
name-table store before identifier-table store and final temporary free.
Do not add token bounds, empty-line filtering or missing-file recovery.

Watcom compiles warning-free. Wdis confirms six saves, gate, callback
registers/capture without64-bit shift helpers, allocations, token and space
loops, signed ID conversion, table stores and RET. Full static link:
2623 redirects,357/397 modules,589818 arena bytes. Tracker/diff checks pass;
inventory3393 compiles,841 blocked. Status compiles only; no execution,
UI or replay runs and no claim of crash resolution. Evidence:
local/page-name-table-20261116/{original.asm,callers.asm,page_name_table.obj,
page_name_table.lst,full.json}. Commands: ef2000_wcc.py on page_name_table.c;
wdis; hybrid_link.py; decomp_status.py check; git diff --check.

### Offset page-record immediate dispatch (0x9f940)

Recover `dispatch_offset_page_record_9f940` (446 original bytes) in
`decomp/src/page_positioned_record.c`. Sole direct caller9e847 passes AX/DX
script words, advances ESI by4 and retains ESI afterward. Original saves
EBX/ECX/ESI/EDI, leaves EBP untouched, treats EAX/EDX as scratch and uses
plain RET. No second direct caller exists in the pinned inventory.

Preserve signed record/point indices,29/6-byte strides, point-Y store before
setup hook, wrapping offsets and interleaved overlapping coordinate reads.
View160122 receives integer coordinates in stack order margin,Z,Y,X,out.
On acceptance publish default rotations with page yaw bias, script globals
in their original order, repeat test before rotation-pointer publication,
optional rotation preloads before X/Y/Z stores, and repeat reread between
Y/Z stores. Call global-input dispatcher16018a through a declaration with
EAX/EBX/ECX/EDX scratch and ESI/EDI/EBP preserved, confirmed from its original
body. Always run the final hook, including a rejecting view path where the
local world buffer can remain uninitialized. Remove obsolete9f940
unreviewed-mechanical exclusion; keep view-test arithmetic exclusions.

This caller does not verify the existing16018a draft or its draw
continuations. Review exposed early rotation preloads in that draft that
still need correction for aliasing; wider callee fidelity remains pending.

Watcom compiles warning-free. Wdis confirms saved registers, signed word
indices, integer view inputs and stack order, retained world pointer across
the scratch-clobbering dispatcher, ordered stores/repeat reread and RET.
Full static link:2624 redirects,357/397 modules,590394 arena bytes; tracker
and diff checks pass. Inventory3394 compiles,840 blocked. Static review
only; no execution/UI/replay runs or crash-resolution claim. Evidence:
local/page-dispatch-record-20261116/{original.asm,callers.asm,
page_positioned_record.obj,page_positioned_record.lst,full.json}. Commands:
ef2000_wcc.py on page_positioned_record.c; wdis; hybrid_link.py;
decomp_status.py check; git diff --check.

### Immediate script dispatcher ordering and ABI (0x16018a)

Correct the existing `append_table_entry_16018a` C draft in
`decomp/src/misc_drafts1.c` against original16018a..160254 and callers9f935
and9fac2. Both callers supply globals and ignore scratch results; original
saves ESI/EDI/EBP and clobbers EAX/EBX/ECX/EDX. Correct the signature record
from its old preserve-all contract; remove its obsolete probe reference
whose register assumptions no longer apply. Status stays compiles.

Read the value byte before the live slot/repeat loads and repeat-table
store. Delay rotation-pointer and all three rotation-word loads until after
coordinate, value-pointer, identifier and absolute-high-Z stores. Preload
all rotation words before writing any rotation table. This preserves
aliasing both when a queue write modifies the script globals and when the
source points into a destination table. Keep the unchecked word slot,
16-bit absolute-value overflow and live slot reload before draw dispatch.

Confirmed existing generated pragmas already send slot in ESI and declare
all GPRs scratch for c552e/c565e. The corrected outer contract now saves only
ESI/EDI/EBP around those callbacks. Recompile misc_drafts1, the only module
with direct named calls to this definition; the newer page caller uses an
explicit matching guest alias. Draw-continuation fidelity remains unproven.

Watcom compiles warning-free. Wdis confirms value/slot/repeat read order,
delayed rotation preloads, word stores, signed high-Z magnitude, ESI slot
reload, matching saves and plain RET. Full static link stays2624 redirects,
357/397 modules; arena590340 bytes. Tracker and diff checks pass. Inventory
unchanged3394 compiles,840 blocked. No execution/UI/replay runs or crash
resolution claim. Evidence: local/script-dispatch-review-20261116/{original.asm,
caller-one.asm,caller-two.asm,misc_drafts1.obj,misc_drafts1.lst,full.json}.
Commands: ef2000_wcc.py on misc_drafts1.c; wdis; hybrid_link.py;
decomp_status.py check; git diff --check.

### Page-yaw-biased record startup (0x9fc00)

Recover `start_biased_page_record_9fc00` (467 original bytes) in
`decomp/src/page_positioned_record.c`. Call sites9e86c,9f1f4 and9f220 pass
signed AX/DX record/point words and retain ESI/EBX interpreter state; EAX
and EDX results are ignored. Original saves EBX/ECX/ESI/EDI, leaves EBP
untouched and uses plain RET. Both script-pair and repeated-start paths
were reviewed against the original assembly.

Preserve29/6-byte record/point strides, signed rotation index, point-Y store
before setup callback, wrapping offsets and interleaved overlapping dword
reads. Shift coordinates left16 before view160156(margin,Z,Y,X,out).
Apply page yaw bias to the default rotation, publish identifier/repeat/
value/world globals in original order, test repeat before publishing its
rotation pointer, preload optional angles before stores and reread repeat
between Y/Z stores. Lookup uses the callee-cleaned stack identifier, then
publish1600c0 and call160255. Always invoke the final world callback,
including the rejecting view path with potentially uninitialized output.
Do not add bounds, rejection cleanup or coordinate normalization.

Remove obsolete9fc00 unreviewed-mechanical exclusion after body/caller
review; keep view-test arithmetic exclusions. Watcom compiles warning-free.
Wdis confirms saved registers, signed indices, SHL16 inputs and stack push
order, bias, ordered globals/angles, repeat reread, lookup argument, final
hook and plain RET. Full static link:2625 redirects,357/397 modules,
590980 arena bytes. Tracker and diff checks pass; inventory3395 compiles,
839 blocked. Static evidence only; no execution/UI/replay runs or crash
resolution claim. Evidence: local/page-biased-record-20261116/{original.asm,
caller-one.asm,caller-two.asm,page_positioned_record.obj,
page_positioned_record.lst,full.json}. Commands: ef2000_wcc.py on
page_positioned_record.c; wdis; hybrid_link.py; decomp_status.py check;
git diff --check.

### Page render entry and hook forwarding (0x9e0c4/0x9e094)

Recover `render_page_9e0c4` (466 original bytes) in
`decomp/src/render_page.c`. Original AX=page ID, EDX/EBX/ECX=coordinates,
AL=acceptance; ESI/EDI/EBP saved, four other GPRs scratch, plain RET.
Review callers4d601,80cd2 and wrapper9e0b1; both zero-coordinate and shifted
world-coordinate paths confirm the input order. Lookup b3f58 preserves
coordinates. Publish signed page parameter and word ID, record/resource
pointers and coordinate high words before view160156(margin,Z,Y,X,out).

On acceptance preserve world-origin publication, signed word relative
scene distance, mode-dependent setup hook and interleaved rotation loads/
stores, then page text1. Run live-bound value-script loops independently of
acceptance, selecting one or eight channels once before iterating. Always
call the final hook with current global world output and page flags, even
on view rejection. Return the saved acceptance byte after all callbacks;
add no bounds, rejection cleanup or stale-state fallback.

Correct existing hook wrapper9e094, whose C draft discarded its three
coordinate inputs. Forward all four register arguments and AL result;
retain stack argument publication, hook restoration and RET4. Original
saves ESI/EDI and leaves EBP untouched, rather than preserving all scratch
registers. Replace its incompatible pragma/signature and retire the old
probe reference. Remove obsolete one-argument page alias. Rebuild all eight
direct page-alias user modules plus small_wrappers6 with the new signature.

Watcom compiles the new routine, wrapper and alias users successfully.
New routine/wrapper compile warning-free. Wdis confirms four register
arguments, five stack view arguments, accepted/rejected paths, live script
bounds, final hook, AL result, wrapper forwarding and RET4. Full static
link:2626 redirects,358/398 modules,591552 arena bytes. Tracker/diff checks
pass; inventory3396 compiles,838 blocked. Status compiles only; no
execution/UI/replay runs and no claim of crash resolution. Evidence:
local/render-page-20261116/{original.asm,wrapper.asm,caller-one.asm,
caller-two.asm,render_page.obj,render_page.lst,small_wrappers6.obj,
small_wrappers6.lst,full.json}. Commands: ef2000_wcc.py on render_page.c,
small_wrappers6.c and eight alias users; wdis; hybrid_link.py;
decomp_status.py check; git diff --check.

### Multi-span device event pump (0xa410c)

Recover `pump_device_events_a410c` in decomp/src/device_event_pump.c.
The existing observed-body inventory already owns all107 bytes:33 at
 a410c..a412c,19 at a39d9..a39eb and55 at a3583..a35b9. Inspect pinned
instructions and inventory call sites across all three spans. The old
boundary blocker is stale: neither ownership nor neighboring boundaries
need changing. The entry branches to the alternate prepass or jumps to
the common earlier loop; both paths push EBX/EDX before its shared epilogue.
Do not include neighboring a4130 or a4138 code.

No inputs; non-EAX GPRs preserved. Sole direct caller a4057 ignores EAX and
retains EBX's running total. Check prepass and event-handler preservation
from original prologues/body as well as signature records. Preserve the
mode-selected two-call prepass, live byte count pointer and ID list reloads,
low-word loop index, byte ID zero extension and optional post-loop scan.
Name the four globals. Rebuild caller module ui_state_helpers against the
confirmed alias contract; no second direct caller exists in the inventory.

Watcom compiles both modules warning-free. Wdis confirms both prepasses,
DX zero extension, live pointer/count loads, handler call and scan gate,
non-EAX preservation and plain RET. Full static link:2627 redirects,
359/399 modules,591682 arena bytes. Tracker/diff checks pass; inventory3397
compiles,837 blocked. Static review only; no execution/UI/replay runs or
claim of crash resolution. Evidence: local/device-event-pump-20261116/{entry.asm,
alternate.asm,tail.asm,caller.asm,device_event_pump.obj,
device_event_pump.lst,full.json}. Commands: ef2000_wcc.py on
device_event_pump.c and ui_state_helpers.c; wdis; hybrid_link.py;
decomp_status.py check; git diff --check.

### Communications initialization (0xa3c18)

Recover `initialize_comms_a3c18` in decomp/src/comms_initialize.c from the
390-byte original. No inputs, AL status, all six non-EAX GPRs preserved,
plain RET. Call sites 346b3/34743 and 34bf1 consume AL; inspect both caller
bodies. The old indirect-call blocker can be narrowed: e1570 installs
ed090/e0984/e11fc or e1568 at 136ba0. Inspect those original initializers;
they take no arguments and return status in AL. Keep the indirect dispatch
and its full scratch-register contract rather than binding a driver in C.

Preserve the pre-initialization fatal callback's caller-cleaned stack
argument, name check, table setup, optional decoded-key early exit, status
publication, and the exceptional status22 continuation. Preserve the live
status reloads, mode read before the count reset, optional entry formatting,
output-buffer byte clear, message-table lookup and ordered reset stores.
The timer call is not merely an EAX scalar call: original EBX retains the
output pointer's high word, its low word is zero, and EAX/ECX/EDX/ESI/EDI
are zero on entry. A narrow inline adapter recreates those inputs, stores
returned EAX at136dd4 first, then captures EBX/EDX as a two-word union.
Keep the returned DX store, clear_tables call, status reload, BX store and
DL store in their original order; do not assume these callback outputs are
zero even though the usual fe560 timer preserves them. Name eleven globals.
No existing C source calls the newly confirmed entry alias.

Watcom compilation: zero warnings/errors. Wdis confirms preservation,
plain returns, both status paths, indirect initializer, clock inputs and
output capture. Full static link:2628 redirects,360/400 modules,592196
arena bytes. Tracker:3398 compiles,836 blocked of4293. Tracker and diff
checks pass. Static review only: no execution, gameplay or differential
proof and no claim that reported crashes are fixed. Evidence:
local/comms-initialize-20261116/{original.asm,caller1.asm,caller2.asm,
comms_initialize.obj,comms_initialize.lst,full.json}. Commands:
ef2000_wcc.py on comms_initialize.c; wdis; hybrid_link.py with pinned payload
and full-objects; decomp_status.py check; git diff --check.

### Base-name helper consumed EBX result (0x9dc88)

Review the page loader9de08 and correct the existing append_base_name_9dc88
ABI and draft. Original9dc88 saves ECX/ESI/EDI, leaves EBP untouched, and
clobbers EAX/EDX/EBX. Its sole direct caller9de22 consumes EBX after the
call: BH/BL are overwritten to compute the record size, retaining upper
EBX for intermediate arithmetic. Allocation and fill calls subsequently
narrow to BX, so the previously retained filename-pointer high bits are
not evidence of an allocation failure. Do not claim this fixes a crash.

Return EBX explicitly using the confirmed signature and generated Watcom
value[ebx] convention. Preserve its exact path-dependent value: the input
name pointer if the first byte is NUL; the last copied signed index if a
later NUL ends the scan; the terminating nonzero byte for dot or the
8-character bound. The original reads the ninth byte before checking the
bound. Keep that load and the byte/lowercase/copy ordering. The subsequent
copy_string and two strcat calls preserve EBX; review their original
instructions too. Stop declaring EBX/EDX preserved. Remove the tracker
reference to the old generic probe, which supplied no string memory or
meaningful EBX-result coverage; retain the JSON as historical evidence.
No existing C callers require rebuilding beyond runtime_helpers7.

Watcom compiles runtime_helpers7 warning-free. Wdis shows the empty-name
input retained, byte and index exit paths, final MOV EBX,ECX, preservation
of ECX/ESI/EDI/EBP and plain RET. Full static hybrid link retains2628
redirects,360/400 modules,592208 arena bytes. Tracker/diff checks pass;
3398 compiles and836 blocked, with no new verification claims. Evidence:
local/page-basename-abi-20261116/{original.asm,caller.asm,runtime_helpers7.obj,
runtime_helpers7.lst,runtime_helpers7.abi.h,full.json}. Commands:
ef2000_wcc.py on runtime_helpers7.c; wdis; hybrid_link.py with pinned
payload and full-objects; decomp_status.py check; git diff --check.
No runtime, UI, replay or differential executions.

### Page resource loading and mode cache (0x9de08)

Recover `load_page_9de08` in decomp/src/page_load.c from the440-byte
original and private export. EAX filename, low EDX mode, AX page ID;
preserve EBX/ECX/ESI/EDI/EBP, no stack arguments, plain RET. Inspect
3cb1b and7a93f: both store only AX. The9ddff zero-mode wrapper passes the
same filename. Confirm the consumed word-result contract in the shared
alias declaration; the original EAX high word is not a consumed ID result.

Keep the asserted168-byte local frame: name0, path80, mode160, reused164,
with unchecked path construction and uninitialized padding. Call the
corrected three-register9dc88 helper, then signed-word resource lookup and
optional registration. Read the resource count/header bytes before the
reuse flag write. All allocation/fill paths narrow the computed size to
BX, including the non-cache path. Preserve the cache-head word at+6,
linked record words at+8, repeated mode-bit reads, requested-mode byte
comparisons, assertion callback and append-link stores. Capture reused
before the final ID lookup, but reload it for the later script gate.
For a new record, fill first, capture mode and flags, write next=-1, clear
mode bits, write the resource ID, then publish the combined saved flags.
Run page text0 only for a new record and reread its flag byte to clear
bit7 afterward. Decrement the live32-bit remaining count with wraparound
and call a1ec8 exactly when the stored result is zero. Name that counter.

Also correct register_resource_9dcf8's falsely preserved EDX metadata:
the original saves the five other non-EAX registers, clears EDX before
table writes and does not restore it. Remove its generic historical probe
reference, retaining the JSON file. Rebuild misc_drafts1 and
runtime_helpers6 for the changed result/clobber declarations; these are
all existing C users of the affected aliases.

Watcom compiles all three modules without warnings/errors. Wdis confirms
the two allocation/cache branches, signed IDs, reuse capture before lookup,
ordered record writes, callback/script calls, counter gate, caller low-word
use and preserved-register/RET contracts. Final full static hybrid link:
2629 redirects,361/401 modules,592846 arena bytes. Tracker:3399 compiles,
835 blocked of4293; tracker/diff checks pass. Static review only, no new
execution, UI/replay or differential proof, and no crash-resolution claim.
Evidence: local/page-load-20261116/{original.asm,caller1.asm,caller2.asm,
page_load.obj,page_load.lst,misc_drafts1.obj,misc_drafts1.lst,
runtime_helpers6.obj,runtime_helpers6.lst,reviewed.json}. Commands:
ef2000_wcc.py on page_load.c,misc_drafts1.c,runtime_helpers6.c; wdis;
hybrid_link.py with pinned payload/full-objects; decomp_status.py check;
git diff --check. The earlier full.json predates the final reuse-load-order
correction; reviewed.json is the authoritative final static link.

### Localized callback guest frames (0x9cf2c/9cf74/9cfa4)

Restore the original guest interfaces in localized_file_handlers.c.
The early Linux probe had replaced the144-byte locals with an extra FRAME
argument, leaving9cf74 and9cfa4 unspliceable. Their guest entries now own
uninitialized unchecked144-byte local arrays. Keep explicit-frame helpers
under distinct non-entry names for the bounded Linux probe and update its
two bindings in runtime/decomp_startup.c. No recovered logic moves into
runtime code. The existing host buffer validation remains unchanged.

9cf74 takes EAX name, returns EAX size and preserves all six non-EAX GPRs.
9cfa4 takes EAX name/EDX destination, returns consumed AL success, preserves
EBX/ECX/ESI/EDI/EBP; EDX can be clobbered by the archive-read path. Inspect
registration9cfdc, which installs them at132ab4/132ab8; these are callbacks,
not direct entries in the old call inventory. Review9d9bb/9d9e9 and
e9517/e9547 indirect call pairs: the size is consumed and ECX is live across
the read. ca225/ca268 also consume the size and only AL of read success.
Review original9cb20,9cbc4 and its eba64 archive path to distinguish preserved
registers from scratch. Confirm both new signatures in the symbol database.
No extra frame enters either guest callback ABI.

The path builder already had its guest signature. Correct its matching
prefix branch to reload40d1e0 after compare, as the original does, rather
than reusing the earlier length. Name the two144-byte prefix buffers and
length. Keep no extra separator, localized-first size lookup, zero-size
fallback to the original name, and the read's independent size test.
The old strcat-dependent differential failure is still unresolved; classify
all three as static-reviewed compiles, not verified. No new differential
or gameplay runs were performed.

Watcom compiles warning-free. Wdis confirms each144-byte local allocation,
EAX-only size argument, EAX/EDX read arguments, plain RET, register saves,
prefix-length reload and both fallback branches. Native cc also compiles
the source and updated runtime binding with -std=c11 -O2 -Wall -Wextra
-Werror -DEF_DECOMP_NATIVE=1 and the normal decomp/runtime include paths.
Final full static link:2632 redirects,361/401 modules,592944 arena bytes.
Tracker:3402 compiles,832 blocked of4293; tracker/diff checks pass. Evidence:
local/localized-callback-abi-20261116/{original.asm,registration.asm,
caller1.asm,caller2.asm,caller3.asm,localized_file_handlers.obj,
localized_file_handlers.lst,localized_file_handlers.host.o,
decomp_startup.host.o,full.json}. Commands: ef2000_wcc.py; wdis; cc -c
on the two affected native files; hybrid_link.py with pinned payload and
full-objects; decomp_status.py check; git diff --check. No claim of runtime
integration verification, bit identity or crash resolution.

### Page history update and rendering (0xa0f84)

Recover `update_page_history_a0f84` in decomp/src/page_history.c from the
1181-byte original and private export. AX source-record index, DX point,
BX history channel, all signed words. Preserve ECX/ESI/EDI/EBP; EAX/EBX/EDX
are scratch, plain RET. Sole direct caller9f0fa reads a three-word script
operand and advances the ESI interpreter PC before calling; no second
direct caller exists in the observed inventory. Assert the36-byte frame's
world vector, rotation/source pointers, channel and counter offsets.

Keep initial state/resource pointers, point-Y publication and the setup
callback's signed X/Z values, ordered coordinate additions and update-flag
read before the final coordinate store. Preserve the enabled oscillator's
word addition, bound read/store order, conditional live negation, signed
count/capacity comparison and descending five-word copies. The shift loop
reloads the live count each iteration. Append lifetime/coordinates/yaw in
original order, then optionally add source yaw and increment the live count.
The aging loop captures its original signed bound, decrements each lifetime
and the live count separately, and invokes the direction helper on every
entry, including expired ones. Do not compact or clamp the queue.

Keep the integer160122 stack order and uninitialized world output on
rejection. Accepted paths publish current rotation/script inputs in their
original interleaving, preload optional rotation offsets before writes,
reread repeat between Y/Z writes, invoke the callee-cleaned lookup and
original160255 dispatcher, then recalculate the history state from live
globals. Draw using a captured signed count, but recalculate state after
every iteration, including the last; each accepted entry gets its own view
call and script dispatch. Preserve the unconditional final world callback.
Name the default history-values byte132b3c without inventing a capacity.

Correct existing add_direction_offset_cee88 ABI metadata after inspecting
all180 original bytes: EAX is unchanged, ESI/EDI saved, EBP untouched,
but EBX/ECX/EDX are scratch. Remove its generic old probe reference; retain
the JSON archive. Rebuild misc_drafts1, the only existing source defining
or using that alias apart from the new history module. Keep the existing
helper's arithmetic; no replacement of the original view/dispatch bodies.

Watcom compiles both modules with zero warnings/errors. Wdis confirms
signed loop tests, live count reloads, word copies and stores, direction
arguments, both five-argument stack view calls, callback publication and
register/RET contracts. Full static link:2633 redirects,362/402 modules,
594522 arena bytes. Tracker:3403 compiles,831 blocked of4293; tracker/diff
checks pass. Static review only, no runtime/UI/replay/differential execution
or crash-resolution claim. Evidence: local/page-history-20261116/{original.asm,
caller.asm,direction.asm,page_history.obj,page_history.lst,
misc_drafts1.obj,misc_drafts1.lst,full.json}. Commands: ef2000_wcc.py on
page_history.c and misc_drafts1.c; wdis; hybrid_link.py with pinned payload
and full-objects; decomp_status.py check; git diff --check.

### Widget frame painter (0xa5ca4)

Recover `draw_widget_frame_a5ca4` in decomp/src/widget_frame.c from the
596-byte original and private export. The stale implicit-EDI blocker is
incorrect: original saves EDI and clears it before use. Only EAX widget
is an input; all six non-EAX GPRs preserved, plain RET. Sole direct caller
a5fd2 retains its widget in EDX; inspect its redraw sequence and the
callee's complete body. No second direct caller exists in the inventory.

Preserve the four initial rectangles, three iterations of two outer-edge
rectangles, two optional upper-edge rectangles, common upper segment and
final right segment: twelve calls when widget word+c5 is zero, fourteen
otherwise. Keep byte colours137947/137945/13794a, fresh geometry reads
between calls, signed word narrowing after wrapped arithmetic, captured
width-minus-one across the optional draws, and the original reversed Y
endpoints. Do not normalize rectangles or resize the button/text regions.
Name the three colour bytes; inferred frame roles are documented in the
global notes. The old mechanical exclusion described a different broken
body that dropped four call arguments. Remove it only after confirming all
five arguments at each of the ten static call sites in this hand recovery.

Review original c94f4: EAX colour, EDX X0, EBX Y0, ECX X1 and a callee-cleaned
stack Y1, RET4. It saves ESI/EDI/EBP and clobbers the four scratch GPRs;
old metadata falsely preserved EBX/ECX/EDX. Correct that contract, add an
explicit shared Watcom pragma, and remove its old generic probe reference
without deleting the historical JSON. Rebuild misc_drafts1, the only existing
source using the affected aliases. Its a5ca4 guest alias now correctly
preserves EDX. The clip entry itself remains original in the hybrid due to
the existing stack-argument gate; the new painter calls it with the original
register/stack interface. Do not claim the clip draft is newly spliced.

Watcom compiles both modules warning-free. Wdis confirms sign extension,
all colour/coordinate loads, one Y1 push per clip call, no caller stack
cleanup, both optional paths, three loop iterations, preserved registers
and plain painter RET. Full static link:2634 redirects,363/403 modules,
595500 arena bytes,1637 exclusions. Tracker:3404 compiles,830 blocked of4293;
tracker/diff checks pass. No runtime/UI/replay/differential execution and
no claim that the menu-highlight bug or crashes are fixed. Evidence:
local/widget-frame-20261116/{original.asm,caller.asm,clip.asm,widget_frame.obj,
widget_frame.lst,misc_drafts1.obj,misc_drafts1.lst,full.json}. Commands:
ef2000_wcc.py on widget_frame.c and misc_drafts1.c; wdis; hybrid_link.py
with pinned payload and full-objects; decomp_status.py check;
git diff --check.

### 2026-11-16: child widget layout (static recovery)

Recover position_widget_child_a6438 (616 original bytes) in widget_layout.c.
Review the original switch table and sole caller a66bc: EAX parent, EDX child,
all six non-EAX GPRs preserved, plain RET; caller keeps ECX/EBX live.
The CS override selects a constant jump table, requiring no segment operation
in C. Preserve all 15 cases, low-word coordinate wrapping, short-circuit
flag reads, original dword coordinate loads and interleaved stores, including
case 9's owner-width copy. Name the 41-entry widget pointer table.
Remove the old mechanical-body exclusion only for this new hand recovery.

Compile widget_layout.c and its sole existing alias caller scene_helpers.c
with ef2000_wcc.py; both warning-free. Inspect the generated body with wdis:
register parameters can be scratch in the compiled body; hybrid_link's
signature-based trampoline restores the original six-register contract.
Full static link against the pinned payload:2635 redirects,364/404 modules,
596158 arena bytes,1636 exclusions. Tracker:3405 compiles,829 blocked.
Run decomp_status.py check and git diff --check. Evidence remains ignored in
local/widget-layout-20261116/{widget_layout.obj,widget_layout.lst,final.json}.
No runtime, UI, replay or differential run; compiles only. This does not
establish that the menu-highlight or reported crashes are fixed.

### 2026-11-16: preserve widget refresh clipping result

Review e5830 (62 bytes), its sole caller a6d44/a6d49, and the original
clipping entry e52a4. The helper returns AL unchanged from e52a4; redraw
branches on that byte (0/1/2) for dirty rectangle save/restore or early exit.
Replace the recovered void interface with uint8_t and explicitly return the
named clipping helper result. Record AL in signatures.tsv and remove the
old generic probe reference (retain its historical JSON). Earlier generated
code may incidentally retain AL; a void C interface did not guarantee it.
No other C source calls this helper. Preserve the pre-call mode capture and
post-call reload of widget owner ID.

Commands: ef2000_wcc.py on runtime_helpers4.c (zero warnings); wdis (AL
survives the epilogue); hybrid_link.py with the pinned payload and complete
object snapshot (2635 redirects,596158 arena bytes,364/404 modules,1636
exclusions); decomp_status.py check; git diff --check. Evidence:
local/widget-refresh-result-20261116/{runtime_helpers4.obj,
runtime_helpers4.lst,full.json}. Static review only, no runtime/UI/replay
or differential execution; no claim that the highlight bug is fixed.

### 2026-11-16: restore missing title frame painter

Recover draw_widget_frame_a713c (199 original bytes) in widget_title_frame.c.
The tracker incorrectly assigned it to runtime_helpers32, whose source has
no definition. Keep its state compiles, correct the module, remove its old
generic probe reference and retain the historical JSON. Review original
ASM and sole direct caller a70be: EAX widget, six other GPRs preserved,
plain RET. Preserve two separate flag reads, signed dword-high coordinate
visibility check, checked title lookup, low-word wrapping and selected/
unselected bitmap branches (unselected width decrements by one).

Move the rectangle helper c92e4's explicit four-register/four-stack,
callee-cleaned pragma from screen_clip1.c to the shared header so the new
caller sees it. Conservatively declare all four scratch GPRs clobbered:
original c92e4 saves only ESI/EDI/EBP around eb174. Preserve its existing
9/9 differential evidence with stubbed eb174; that does not prove the real
callee's scratch-register preservation. Rebuild screen_clip1. The stack
argument gate still excludes c92e4 from hybrid redirection; the new painter
calls original c92e4, forwarding stride, source X/Y and bitmap on the stack.

Commands: ef2000_wcc.py on widget_title_frame.c and screen_clip1.c (zero
warnings); wdis (both branches, four pushes, correct cleanup); hybrid_link.py
(2636 redirects,596384 arena bytes,365/405 modules,1635 exclusions);
decomp_status.py check; git diff --check. Evidence remains ignored under
local/widget-title-frame-20261116/. No new runtime, UI, replay or differential
execution; this is not proof of a menu-highlight or crash fix.

### 2026-11-16: restore missing widget text painter

Recover draw_widget_text_a6c20 (203 original bytes) in widget_text.c.
The old compiles entry assigned runtime_helpers32, but no C definition
exists there. Correct the module, keep compiles and remove the generic
probe reference while retaining the historical JSON. Original ASM and sole
caller a7082 establish EAX widget, six preserved GPRs, plain RET. Preserve
signed dword-high clip calculations, unsigned word text extents, equality
at clip boundaries, unchecked copy into a 132-byte local array, post-copy
coordinate reloads, signed low-word draw coordinates and high-nibble style.
Keep original 9d749 in the hybrid: its C draft remains excluded for known
pair-copy/EDX fidelity differences.

Review e26bc prologue/RET4 and share an explicit pragma for its existing
alias: EAX zero, EDX X, EBX Y, ECX text, one callee-cleaned style stack
argument; four scratch GPRs, ESI/EDI/EBP saved. EAX is overwritten before
use, but preserve the original caller's explicit zero argument. No existing
C source calls this alias. The renderer body itself remains blocked.

Commands: ef2000_wcc.py widget_text.c (zero warnings); wdis (clip branches,
post-copy loads, style push and callee stack cleanup); hybrid_link.py with
pinned payload and complete objects (2637 redirects,596674 arena bytes,
366/406 modules,1634 exclusions); decomp_status.py check; git diff --check.
Evidence: local/widget-text-20261116/{widget_text.obj,widget_text.lst,full.json}.
No runtime/UI/replay/differential execution and no claim of crash or menu
highlight repair. Stack contents/layout equivalence remains unverified.

### 2026-11-16: styled text renderer

Recover draw_styled_text_e26bc (400 bytes) in styled_text.c from the original
ASM and private export. Review sole caller a6cdb: EAX zero (unused), EDX X,
low BX Y, ECX text, low byte of one stack argument selects style; RET4;
ESI/EDI/EBP preserved and four scratch GPRs. The stale implicit-SI blocker
is incorrect: ESI is assigned EDX before its low word is read.

Keep format 1/2 atlas and format 3 packed-glyph loops, initial X+4, signed
low-word destination coordinates, missing-glyph space handling, per-character
style offsets, width reload after height for atlas draws, original glyph
source coordinates, and post-blit text/width/spacing reloads. Name style
fields and first glyph metric table; preserve unchecked style indexing.
Review ca290 prologue/RET20: four register arguments plus width,height,X,Y,
stride on the callee-cleaned stack. Add a narrow shared alias/pragma for
this original blitter. No existing C callers use that new alias.

Commands: ef2000_wcc.py styled_text.c (zero warnings); wdis (five blitter
argument pushes, no caller cleanup, post-call reloads and renderer RET4);
hybrid_link.py (2637 redirects,596994 arena bytes,367/407 modules,1635
exclusions); decomp_status.py check; git diff --check. Evidence:
local/styled-text-20261116/{styled_text.obj,styled_text.lst,full.json}.
The stack-argument gate excludes e26bc from redirects: existing widget text
continues calling the original renderer. Tracker promotion is compiles only
(3406 compiles,828 blocked). No runtime/UI/replay/differential execution;
stack/RAM equivalence and the reported bugs remain unverified.

### 2026-11-16: clipped bitmap dispatcher

Recover blit_bitmap_clipped_ca290 (474 bytes) in clipped_bitmap.c. Original
ASM and callers e276e/e281a/a6ff7 establish EAX bitmap, EDX rectangle,
low BX/CX source X/Y, five stack arguments width,height,X,Y,stride, RET20;
ESI/EDI/EBP preserved, four scratch registers. The stale implicit-SI blocker
is false: ESI is loaded from the first stack argument before use.

Keep signed-word early rejection, original dword-high rectangle reads,
left/top source adjustments, low-word width/height wrapping and repeated
rectangle loads. Preserve the height-only final zero check (do not add a
width check). Stride low word 256 chooses original cd82c with seven
zero-extended stack arguments; otherwise original cd9a6 receives eight
stack arguments, signed coordinates/extents/source offsets and unsigned
stride. Add distinct stack-only aliases and explicit pragmas so the old
register-plus-stack draft interface is not reused. No new globals.

Commands: ef2000_wcc.py clipped_bitmap.c (zero warnings); wdis (signed
comparisons, both seven/eight-push calls, no caller cleanup, RET20);
hybrid_link.py with pinned payload/full objects (2637 redirects,597474 arena
bytes,368/408 modules,1636 exclusions); decomp_status.py check; git diff
--check. Evidence: local/clipped-bitmap-20261116/{clipped_bitmap.obj,
clipped_bitmap.lst,full.json}. Tracker:3407 compiles,827 blocked.
The stack-argument gate still excludes ca290 and its callees; no claim of
new live redirection. No runtime/UI/replay/differential execution; RAM and
stack equivalence and reported crash/highlight repairs remain unverified.

### 2026-11-16: recover missing text width body

Recover text_width_e284c (106 original bytes) in text_width.c; the tracker
previously assigned runtime_helpers35 despite there being no C definition.
Original ASM and sole caller a63c0@a63d4 establish EAX text, DX style,
EAX width, plain RET, EBX/ECX/ESI/EDI/EBP preserved. Correct the false EDX
preservation claim: the original clears EDX and accumulates width there.
Rebuild runtime_helpers10, the sole existing generic-alias caller.

Keep null-text zero, unsigned dword accumulation, unchecked 16-bit style
indexing, glyph width plus spacing for present glyphs, space advance only
for absent space glyphs, and zero advance for other absent glyphs. Retain
the old probe JSON without presenting it as differential evidence; remove
its tracker reference. Existing named globals cover both metric tables.

Commands: ef2000_wcc.py text_width.c and runtime_helpers10.c (zero warnings);
wdis text_width.obj (word style reload, byte metric reads, preserved
registers and plain RET); hybrid_link.py pinned payload/full objects
(2638 redirects,597618 arena bytes,369/409 modules,1635 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/text-width-20261116/{text_width.obj,text_width.lst,full.json}.
Status remains compiles, not verified; counts remain3407 compiles,827
blocked. No runtime/UI/replay/differential execution. Generated stack
layout differs from the original; RAM/stack equivalence and all reported
crash/highlight repairs remain unverified. No packaged runtime rebuild.

### 2026-11-16: screen clip access order

Review clip_rect_to_screen_e58a8 (183 original bytes) against original ASM
and callers a5f9a/a5fcb and a6dc6. No arguments or consumed return; original
preserves EBX/ECX/EDX/ESI/EDI and untouched EBP. Keep signature unchanged.
Correct the C draft's reversed negative-origin stores: capture adjusted
extent, clear origin, then store extent, for each axis. Restore original
signed dword-high reads at 41ec0a/41ec06 and 41ec08/41ec0c for the right and
bottom checks. Preserve signed comparisons, inclusive limit rejection,
word wrap and the absence of empty/negative-size guards. No new globals.
Remove the generic probe tracker reference; retain its JSON as old evidence.
This is an access-order fidelity correction, not a demonstrated highlight
or crash fix; ordinary final clip values were already equivalent.

Commands: ef2000_wcc.py misc_drafts1.c (zero warnings); wdis (both origin
stores before extent stores, four dword loads and arithmetic shifts);
hybrid_link.py pinned payload/full objects (2638 redirects,597634 arena
bytes,369/409 modules,1635 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/screen-clip-order-20261116/{misc_drafts1.obj,misc_drafts1.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence remains
unverified.

### 2026-11-16: recover missing widget text setter

Recover set_record_name_a7510 (119 original bytes) in widget_name.c; tracker
runtime_helpers32 had no definition. ASM and sole caller a758f establish
EAX widget, EDX text, plain RET, EBX/ECX/ESI/EDI/EBP preserved. Correct EDX
preservation metadata: final original 9d749 advances it. Caller overwrites
EDX with one before relayout. Correct unused generic alias to two arguments;
rebuild runtime_helpers10, the only existing named caller.

Keep null-name substitution, low-word length store followed by capacity
reload and capacity+1 allocation, pointer store before failure callback,
unsigned full-length capacity comparison, and original formatted callback
arguments. Preserve the final unchecked original 9d749 copy even after an
error callback returns; no truncation, capacity growth or null guard. Keep
original strlen semantics via b1b8b and zeroing allocator ab840. Name the
error callback pointer and three read-only strings without copying assets.
Remove generic probe tracker reference, retaining its JSON.

Commands: ef2000_wcc.py widget_name.c and runtime_helpers10.c (zero warnings);
wdis widget_name.obj (word store/reload, unsigned jbe, callback argument
order via existing cdecl bridges, final pointer reload/copy and plain RET);
hybrid_link.py pinned payload/full objects (2639 redirects,597842 arena
bytes,370/410 modules,1634 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/widget-name-20261116/{widget_name.obj,widget_name.lst,full.json}.
Status remains compiles; counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild. Full RAM/stack equivalence and all
reported crash/highlight repairs remain unverified.

### 2026-11-16: recover parser key lookups

Recover lookup_key_a7ec4 (158 original bytes) and find_key_checked_a7f64
(173 bytes) in parser_key_lookup.c. Both tracker entries named
runtime_helpers32 without C definitions. ASM and callers 47ec/4c41 and
27ffe establish EAX mutable name, EAX slot address/stored pointer,
respectively, all six other GPRs preserved, plain RET. No ABI changes or
existing-caller rebuild needed; caller 47ec stores preserved EDX through
the scalar result. Review the original lowercase, copy and compare callees.

Preserve in-place lowercasing, unchecked copy to the name slot at signed
count, sentinel-terminated scalar search, signed low-word index wrapping,
21-byte name stride, scalar-kind zero and record-kind one checks, and
separate fallback slot/name addresses. Record search compares the name
before checking count, including at count. Keep the original 80-byte
unchecked formatting buffer and callback argument order; returning error
callbacks still allow the subsequent kind check and result access. Name
count/name table, fallback values and error strings. Remove generic probe
tracker references while retaining their JSON files.

Commands: ef2000_wcc.py parser_key_lookup.c (zero warnings); wdis (signed
index extension, signed count checks after comparison, scalar address vs
record dereference, 80-byte local buffer, cdecl formatting cleanup and
preserved registers); hybrid_link.py pinned payload/full objects (2641
redirects,598262 arena bytes,371/411 modules,1632 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/parser-key-lookup-20261116/{parser_key_lookup.obj,parser_key_lookup.lst,
full.json}. Status remains compiles, counts unchanged. No runtime/UI/replay/
differential execution or packaged runtime rebuild. Generated stack layouts
differ; full RAM/stack equivalence and reported crash/highlight repairs
remain unverified.

### 2026-11-16: recover parser key table reset

Recover reset_key_tables_a9bfc (209 original bytes) in parser_key_reset.c;
the tracker previously assigned runtime_helpers32 without a C definition.
ASM and sole caller aaee8@aafb4 establish no arguments or consumed output,
all six other GPRs preserved, plain RET. Caller retains EDI and ECX. No ABI
change or existing-caller rebuild required.

Keep original byte/dword/word clear widths, ten nonsequential pointer
stores to the option vector, the interleaved variable-count clear, then
latch_b3f48 followed by return_true_3ce2c and its AL store. Preserve all
four separate trailing byte clears. The ten 15-byte option buffers are
fixed storage: neither allocate nor clear them. Name the wider pointer
vector (overlapping the existing first-entry symbol), storage and remaining
reset fields with neutral names where their meaning remains unestablished.
Remove generic probe tracker reference, retaining its JSON.

Commands: ef2000_wcc.py parser_key_reset.c (zero warnings); wdis (all store
widths/order, latch then flag call, preserved registers, plain RET);
hybrid_link.py pinned payload/full objects (2642 redirects,598504 arena
bytes,372/412 modules,1631 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/parser-key-reset-20261116/{parser_key_reset.obj,parser_key_reset.lst,
full.json}. Status remains compiles, counts unchanged. No runtime/UI/replay/
differential execution or packaged runtime rebuild. Full RAM/stack
equivalence and reported crash/highlight repairs remain unverified.

### 2026-11-16: recover path animation step

Recover step_animation_f36a4 (109 original bytes) in path_animation.c;
tracker runtime_helpers35 had no definition. ASM and sole caller f4079
establish no arguments or consumed output, six other GPRs preserved,
plain RET. Caller keeps its hover value in EDI. No ABI changes or existing
caller rebuild required; the C display dispatcher still uses its original
guest-call boundary.

Draw the path selected by the signed high word at 161e18 before reading
stage and delta. Keep wrapped word addition and stage store, negative-stage
reset of delta then stage, and the fresh overlapping dword-high read at
161e14. Only exactly five calls refresh_f35f0 before clearing delta and
setting stage four; values above five do not stop. Name the stage, delta
and path-index fields. Remove generic probe tracker reference, retaining
its JSON. No new timing assumptions or frame guards.

Commands: ef2000_wcc.py path_animation.c (zero warnings); wdis (draw before
metric reloads, word store/sign test, fresh dword read, exact-five branch,
refresh-before-reset order, preserved registers and plain RET);
hybrid_link.py pinned payload/full objects (2643 redirects,598666 arena
bytes,373/413 modules,1630 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/path-animation-20261116/{path_animation.obj,path_animation.lst,
full.json}. Status remains compiles, counts unchanged. No runtime/UI/replay/
differential execution or packaged runtime rebuild. Full RAM/stack
equivalence and reported crash/highlight repairs remain unverified.

### 2026-11-16: recover shape match count and repair wrapper

Recover count_matching_entries_fba6c (105 original bytes) in
shape_match_count.c; runtime_helpers35 had no definition. ASM and callers
bfcb6/fba63 establish EAX/EDX match values, EBX list-head address, EAX count,
ECX/ESI/EDI/EBP preserved, EBX/EDX scratch, plain RET. Correct false
EBX/EDX preservation metadata and unused two-argument generic alias.

Repair call_fba6c at fba54: its draft discarded EAX list-head argument,
omitted EBX at the counting call and declared the returned count void.
Forward all three inputs, load record+1a before record+16, and return the
count. Correct wrapper EAX return/EDX scratch metadata; callers fbb2b/fbc83
sign-extend AX and divide the count by three. Rebuild misc_helpers2 and
game_helpers130, all existing actual users of the corrected interfaces.

Keep list-head dereference, next at +4, fresh byte count at +12 per loop,
signed low-word index and 124-byte entry stride, ordered field +8/+12
matches, node-kind two check, low-AX kind predicate and unsigned total.
No null guard or new globals. Remove generic probe tracker references,
retaining their JSON files.

Commands: ef2000_wcc.py shape_match_count.c, misc_helpers2.c and
game_helpers130.c (zero warnings); wdis counting body and wrapper (word
loop checks, field order, EBX forwarding and EAX return); hybrid_link.py
pinned payload/full objects (2644 redirects,598792 arena bytes,374/414
modules,1629 exclusions); decomp_status.py check (4293 functions); git diff
--check. Evidence: local/shape-match-count-20261116/{shape_match_count.obj,
shape_match_count.lst,misc_helpers2.lst,full.json}. Status remains compiles,
counts unchanged. No runtime/UI/replay/differential execution or packaged
runtime rebuild; full RAM/stack equivalence and reported crash/highlight
repairs remain unverified.

### 2026-11-16: recover vector transform and component forwarding

Recover transform_vector_e99b8 (106 original bytes) in vector_transform.c;
tracker runtime_helpers35 had no definition. ASM and callers 80ba5/83737/
a0526 establish EAX destination, EDX/EBX/ECX components, ESI/EDI/EBP
preserved, three component registers scratch, plain RET. Correct false
preservation metadata. Use unsigned products/addition/doubling for the
original low-dword wrap, without signed-overflow C behavior.

Preserve coefficient load order around output stores, including the
second-row first product before output zero, final-row second/third
products before output one, and final-row first coefficient afterwards.
Destination aliasing the matrix must retain those effects. Name the
36-byte matrix. No coefficient snapshot or widened accumulation.

Repair scale_vector_down_16 at 83734: its draft declared only destination
and omitted EDX/EBX/ECX component arguments when calling the transform.
Restore four inputs in body/header/metadata and mark component registers
scratch. Read all three output dwords before the arithmetic-shift stores.
Original sole caller 83a78 uses the four-register interface. Correct stale
transform alias likewise. Rebuild scene_helpers, misc_drafts1 and
page_positioned_record, all actual C users of changed contracts. Remove
generic probe tracker references, retaining their JSON files.

Commands: ef2000_wcc.py vector_transform.c, scene_helpers.c, misc_drafts1.c
and page_positioned_record.c (zero warnings); wdis transform and wrapper
(low products, coefficient/store order, component forwarding and arithmetic
shifts); hybrid_link.py pinned payload/full objects (2645 redirects,598990
arena bytes,375/415 modules,1628 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/vector-transform-20261116/{vector_transform.obj,vector_transform.lst,
scene_helpers.lst,full.json}. Status remains compiles, counts unchanged.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crash/highlight repairs remain
unverified. Matrix-builder e994c remains missing and is separate work.

### 2026-11-16: recover rotation builder dispatch

Recover build_rotation_e994c (106 original bytes) in rotation_dispatch.c;
tracker runtime_helpers35 had no definition. ASM and callers 80b95/83a66/
a0511 establish EAX flags, EDX/EBX/ECX angles, plain RET. Original preserves
ECX/ESI/EDI/EBP, while EAX/EDX/EBX are scratch. Correct false EBX/EDX
preservation claims here and in e95c0/e977c: both builder prologues save
ECX/ESI/EDI/EBP and their epilogues restore those only.

Forward the three angles in EAX/EDX/EBX to original e95c0 for bit0 set,
e977c otherwise. Keep saved flags across the call. Bit1 performs three
ordered off-diagonal swaps (matrix transposition), not an arithmetic
mirror. Preserve each pair's load/load/store/store order and leave the
matrix otherwise unchanged. Existing named matrix covers all accesses.
Rebuild misc_drafts1 and page_positioned_record, all existing actual C
callers/definitions affected by these contracts. Remove generic probe
tracker references while retaining JSON files.

Commands: ef2000_wcc.py rotation_dispatch.c, misc_drafts1.c and
page_positioned_record.c (zero warnings); wdis (register shuffle, saved
flags, both builder calls and ordered swaps); hybrid_link.py pinned
payload/full objects (2646 redirects,599140 arena bytes,376/416 modules,
1627 exclusions); decomp_status.py check (4293 functions); git diff --check.
Evidence: local/rotation-dispatch-20261116/{rotation_dispatch.obj,
rotation_dispatch.lst,full.json}. Watcom treats argument ECX as scratch
in the body; the hybrid entry trampoline restores original preserved ECX.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crash/highlight repairs remain unverified.

### 2026-11-16: recover stream buffer configuration

Recover set_file_buffer_f22af (103 original bytes) in stream_buffer_config.c;
tracker runtime_helpers35 had no definition. ASM and sole caller 1115af
establish EAX stream, EDX buffer, EBX mode, ECX size, EAX status, plain RET.
Original preserves EBX/EDX/ESI/EDI/EBP and uses ECX as scratch: correct its
false preservation claim. Caller saves its own EBX/ECX and forwards status.
Review original fa7d2 and 10f071 register preservation and state accesses.
Rebuild leaf_helpers2, the only actual C caller.

Keep unsigned size rejection above 7fffffff, accepted modes100/200/400,
and nonnull-buffer/zero-size rejection before touching the stream. Update
flags first, store nonzero size only, capture byte+0d before buffer+8 store,
clear its low three bits, read dword+0c before current-pointer store, then
OR mode into that captured state. Allocate only for null buffer, ignore
allocator result, and return zero. No new globals, guards or buffer frees.
Remove generic probe tracker reference, retaining its JSON.

Commands: ef2000_wcc.py stream_buffer_config.c and leaf_helpers2.c (zero
warnings); wdis (validation branches, call-before-state writes, byte/dword
load/store order and zero status); hybrid_link.py pinned payload/full
objects (2647 redirects,599316 arena bytes,377/417 modules,1626 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/stream-buffer-config-20261116/{stream_buffer_config.obj,
stream_buffer_config.lst,full.json}. Watcom treats parameter EBX as scratch
in its body; hybrid entry restores original preserved EBX. Status remains
compiles, counts unchanged. No runtime/UI/replay/differential execution or
packaged runtime rebuild; full RAM/stack equivalence and reported crashes
remain unverified.

### 2026-11-16: recover small glyph drawing

Recover draw_glyph_f25b0 (107 original bytes) in small_glyph.c; tracker
runtime_helpers35 had no definition. ASM and sole caller f2644 establish
AL colour, EDX X, EBX row, ECX glyph, ESI/EDI/EBP preserved, four scratch
registers, plain RET. Correct false EBX/ECX/EDX preservation metadata.
Caller masks glyph to six bits and advances X by four. Rebuild
game_helpers29, the sole existing C alias user.

Capture stride before framebuffer/row-table reads, preserve unchecked
glyph and row indexing, load each glyph row live before writing bits4/2/1
to pixels0/1/2, and advance by captured stride for all six rows. Preserve
the final stride write even when pixel output aliases that field. No
clipping or transparency writes added. Name stride and the 64-by-six
font table; original table bytes remain private. Remove generic probe
tracker reference, retaining its JSON.

Commands: ef2000_wcc.py small_glyph.c and game_helpers29.c (zero warnings);
wdis (entry stride capture, ordered pixel bit tests/stores, six iterations,
final stride write and plain RET); hybrid_link.py pinned payload/full
objects (2648 redirects,599456 arena bytes,378/418 modules,1625 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/small-glyph-20261116/{small_glyph.obj,small_glyph.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crash/highlight repairs remain unverified.

### 2026-11-16: recover video startup

Recover init_video_d1d40 (102 original bytes) in video_startup.c; the
runtime_helpers35 tracker entry had no definition. Original ASM and callers
d1dde/d1e31 establish EAX width, EDX height, EBX/ECX/EDX/ESI/EDI/EBP
preserved and plain RET. No caller contract changes required.

Preserve height-before-width stores, callback installation before driver
query, and fresh width/height loads after the query. Pass height then width
as the row-table builder's callee-cleaned stack arguments. Preserve the
subsequent drawing setup, table reset, palette upload, buffer release and
allocation calls, then load and invoke the prepare callback with EAX zero,
clear the framebuffer, and freshly load/invoke the present callback with
EAX zero. No callback guards or allocation changes. Name six touched global
fields; no original palette bytes included. Remove generic probe tracker
reference, retaining its JSON.

Commands: ef2000_wcc.py video_startup.c (zero warnings); wdis (ordered
stores, post-query dimension reloads, stack argument order, call sequence
and plain RET); hybrid_link.py pinned payload/full objects (2649 redirects,
599602 arena bytes,379/419 modules,1624 exclusions); decomp_status.py check
(4293 functions); git diff --check. Evidence:
local/video-startup-20261116/{video_startup.obj,video_startup.lst,full.json}.
Watcom treats parameter EDX as scratch; hybrid entry restores original
preserved EDX. Status remains compiles, counts unchanged. No runtime/UI/
replay/differential execution or packaged runtime rebuild; full RAM/stack
equivalence and reported crashes remain unverified.

### 2026-11-16: recover default named-entry selection

Recover select_default_video_aca14 (115 original bytes) in default_video.c;
runtime_helpers33 had no definition. ASM and caller sites ac96a/acab5/acb0a
establish no inputs, EAX scratch, six other registers preserved, plain RET.
Keep the existing function name but do not infer video-mode semantics from
it: the selected names are Plane2, Plane3 and Plane1.

Preserve the override-word early return, high-byte kind dispatch (one/two,
otherwise default), lookup-result narrowing and fresh unsigned word reload.
The original arithmetic high-byte shift and unsigned comparisons choose
the same default for all negative values as this logical extraction. Keep
unchecked 25-byte entry indexing and load both table words before either
output store, preserving alias order. No bounds checks or lookup guards.
Name five state fields with neutral names. Remove the generic probe tracker
reference, retaining its JSON. Existing callers need no ABI rebuild.

Commands: ef2000_wcc.py default_video.c (zero warnings); wdis (override,
three lookup names, result word store/reload, both loads before stores,
register saves and plain RET); hybrid_link.py pinned payload/full objects
(2650 redirects,599764 arena bytes,380/420 modules,1623 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/default-video-20261116/{default_video.obj,default_video.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crash/highlight repairs remain unverified.

### 2026-11-16: recover record-tree cleanup

Recover free_record_tree_b1a68 (121 original bytes) in record_tree_free.c;
runtime_helpers33 had no definition. ASM and sole caller b1af5 establish
EAX record, all six other registers preserved, plain RET. The caller keeps
its next-list pointer in EDX across cleanup. Follow cc768/c3f80 tail jumps
to the existing heap-free boundary; retain the original cc768 call target.
No ABI changes to existing users.

Preserve null-root early return, signed word counts and signed low-word
indices, 32-bit counter increments, unchecked six-/ten-byte indexing,
per-iteration root table/count reloads, and free order: nonnull entry
allocations, nonnull group entry tables, nonnull root group table, root.
Do not clear freed pointers or cache mutable counts across calls. Add
minimum packed record/group/entry layouts with offset/size assertions.
Remove generic probe tracker reference, retaining its JSON.

Commands: ef2000_wcc.py record_tree_free.c (zero warnings); wdis (signed
word comparisons/index extension, live table/count reads, conditional free
order, register saves and plain RET); hybrid_link.py pinned payload/full
objects (2651 redirects,599926 arena bytes,381/421 modules,1622 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/record-tree-free-20261116/{record_tree_free.obj,record_tree_free.lst,
full.json}. Status remains compiles, counts unchanged. No runtime/UI/replay/
differential execution or packaged runtime rebuild; full RAM/stack
equivalence and reported crashes remain unverified.

### 2026-11-16: recover UI reset initialization

Recover reset_ui_state_ac0e0 (139 original bytes) in ui_state_reset.c;
runtime_helpers33 had no definition. ASM and both caller sites 84c96/84cac
establish no inputs, EAX scratch, six other registers preserved, plain RET.
No caller ABI changes required. Inspect nested e17f8/ac16c register saves:
the original zero EDX/DL values survive these calls.

Preserve conditional engine reset, display initialization, 100000-byte pool
initialization, clock latch, record loading, style bitmap copy, glyph rebuild
and state clear order. Centre the cursor using signed division by two
truncated toward zero, subtract one, then narrow to a word, separately for
width and height. Set the reset flag before ac16c, retain both final byte
clears afterward even though ac16c also clears them. Name seven state fields
without guessing meanings for unknown flags. Remove the generic probe
tracker reference, retaining its JSON.

Commands: ef2000_wcc.py ui_state_reset.c (zero warnings); wdis (call order,
style copy, signed-half arithmetic, word stores, final flags, EDX save and
plain RET); hybrid_link.py pinned payload/full objects (2652 redirects,
600120 arena bytes,382/422 modules,1621 exclusions); decomp_status.py check
(4293 functions); git diff --check. Evidence:
local/ui-state-reset-20261116/{ui_state_reset.obj,ui_state_reset.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover nested UI-state clear

Recover clear_ui_state_ac16c (211 original bytes) in ui_state_clear.c;
runtime_helpers33 had no definition. Original ASM and callers ac158/ac791
establish no register inputs, EAX scratch, six other registers preserved,
plain RET. ca5e4 has no inputs and saves EDX; no inferred start-index
parameter added. Existing caller contracts remain unchanged.

Keep ordered pool/record/widget clears, four individual word stores, the
336-byte table fill, device flag read before callback-count clear, and
conditional motion polling with BOTH output pointers equal to 41eeb0.
Reload device flag after polling and before three individual byte clears;
use that captured value for named selection. Preserve final flag/dword
writes and the 89-byte table fill. No merged clears, guards or changes to
selection semantics. Name fifteen touched fields neutrally where meanings
remain unresolved. Remove generic probe tracker reference, retain its JSON.

Commands: ef2000_wcc.py ui_state_clear.c (zero warnings); wdis (ordered
writes/calls, two live device reads, shared motion output address, both fill
lengths and plain RET); hybrid_link.py pinned payload/full objects (2653
redirects,600394 arena bytes,383/423 modules,1620 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/ui-state-clear-20261116/{ui_state_clear.obj,ui_state_clear.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover named-entry selection

Recover select_video_mode_acac0 (166 original bytes) in named_entry_select.c;
runtime_helpers33 had no definition. ASM and caller sites ac1fd/e19ee
establish EAX name, six other registers preserved, plain RET. Keep existing
symbol name without inferring video-mode semantics; selectors operate on
named 25-byte table entries. No existing caller ABI changes.

Preserve ordered case-insensitive null/normal comparisons. Null clears the
enabled word, sets override and still performs the original-name lookup;
normal sets enabled, clears override and invokes the default selector;
explicit names set both flags and perform lookup. Narrow lookup output,
reload selected index unsigned, then retain load/store/load/store table
word order, unlike aca14's load/load/store/store order. No null pointer or
lookup failure guards. Name enabled word. Remove generic probe tracker
reference, retain its JSON.

Commands: ef2000_wcc.py named_entry_select.c (zero warnings); wdis (three
branches, flag-before-lookup order, selected word reload, ordered table
copies and plain RET); hybrid_link.py pinned payload/full objects (2654
redirects,600620 arena bytes,384/424 modules,1619 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/named-entry-select-20261116/{named_entry_select.obj,
named_entry_select.lst,full.json}. Status remains compiles, counts unchanged.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-16: recover reverse unusable-slot cleanup

Recover clear_unusable_slots_bd5f8 (144 original bytes) in unusable_slot.c;
runtime_helpers34 had no definition. ASM and caller sites bdc8d/bdcd7
establish EAX record, full EAX zero/one result, six other registers preserved,
plain RET. Callers test AX: correct the prior AL-only declaration/signature.
No existing C users found, so no caller object rebuild needed.

Preserve kind-two early return and kind-25 signed-low-word count check.
Scan from unsigned byte count minus one downward, skip zero first words,
call the slot predicate with signed DX, test only its low AX, and reload
the table pointer after the predicate while retaining the old byte offset.
Clear only the first unusable occupied slot in reverse order and return
one; return zero otherwise. Full-width loop comparisons are equivalent
here because the byte-derived index remains in -1..254. No extra guards,
clears or rescan. Add minimum packed record/entry layouts and assertions.
Remove generic probe tracker reference, retaining its JSON.

Commands: ef2000_wcc.py unusable_slot.c (zero warnings); wdis (kind gates,
signed count test, reverse loop, predicate AX test, post-call pointer reload,
full EAX result and plain RET); hybrid_link.py pinned payload/full objects
(2655 redirects,600766 arena bytes,385/425 modules,1618 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/unusable-slot-20261116/{unusable_slot.obj,unusable_slot.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover sound-slot selection

Recover select_sound_slot_98050 (141 original bytes) in sound_slot_select.c;
runtime_helpers31 had no definition. ASM and caller sites 3ed7e/98266
establish AL slot, six other registers preserved, plain RET. Existing
caller contract unchanged. Keep original byte enable/mode gates and invalid
slot error callback with format and zero-extended slot on the cdecl stack.
If that callback returns, continue unchecked as the original does.

Preserve current-slot comparison, reset-before-table-base read, 13-byte
record indexing, registration result branch, ordered mode/enable clears,
status-file rewrite, and final slot/latch stores even after failure. Name
four globals; reuse existing enable symbol. Declare the stack-call shim's
ECX clobber locally (matching guest_callptr1.c), so the compiler preserves
the requested slot across the error call. Wdis confirms a saved byte and
reloads, rather than a live CL across the clobber. Remove generic probe
tracker reference, retaining its JSON.

Commands: ef2000_wcc.py sound_slot_select.c (zero warnings); wdis (gates,
callback arguments and saved slot, reset-before-table read, failure writes,
final latch and plain RET); hybrid_link.py pinned payload/full objects
(2656 redirects,600960 arena bytes,386/426 modules,1617 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/sound-slot-select-20261116/{sound_slot_select.obj,
sound_slot_select.lst,full.json}. Status remains compiles, counts unchanged.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-16: recover clock snapshot and tick wait

Recover remember_clock_a7d34 (12 original bytes) and wait_ticks_e1110
(29 bytes) in clock_helpers.c; runtime_helpers30 had no definitions. ASM,
sole snapshot caller a853a and wait callers e0bb3/e0bfa establish no snapshot
inputs and EAX delay respectively, six registers preserved, plain RET.
No existing caller contract changes. The clock callback has no arguments
and returns EAX; use the established register-call function pointer shape.

Snapshot the callback result into 41915c. For wait, call clock before
masking delay to 16 bits, add with uint32 wrap, then poll at least once and
compare unsigned against the fixed deadline. Reload callback pointer on
every call, with no added null check. Preserve the original clock-wrap
comparison behavior explicitly. Name callback and snapshot globals. Remove
both generic probe tracker references, retaining the JSON files.

Commands: ef2000_wcc.py clock_helpers.c (zero warnings); wdis (snapshot
store, initial call before mask/add, fresh callback load inside loop,
unsigned comparison, EDX saves and plain RETs); hybrid_link.py pinned
payload/full objects (2658 redirects,601060 arena bytes,387/427 modules,
1615 exclusions); decomp_status.py check (4293 functions); git diff --check.
Evidence: local/clock-helpers-20261116/{clock_helpers.obj,clock_helpers.lst,
full.json}. Status remains compiles, counts unchanged. No runtime/UI/replay/
differential execution or packaged runtime rebuild; full RAM/stack
equivalence and reported crashes remain unverified.

### 2026-11-16: recover device and stored-error checks

Recover require_device_ready_a292c (26 original bytes) and
check_error_flag_eb35c (24 bytes) in error_gates.c; runtime_helpers30 had no
definitions. ASM and readiness caller sites a3ebb/a42c4 confirm no inputs,
AL result and six preserved registers. Stored-error callers dc250/dc281
consume no result. Both use plain RET. No existing caller ABI changes.

Preserve opposite byte conditions: zero device-ready byte reports fixed
message; nonzero stored-error byte reports the message at 160796. Each
freshly loads the error callback and supplies one cdecl stack argument.
Declare the shim clobbers locally to preserve ECX/EDX. If the callback
returns, readiness still returns AL one; stored-error simply returns,
without clearing the flag or message. Name the readiness/error bytes and
message start (capacity unknown). Remove both generic probe tracker
references, retaining JSON files.

Commands: ef2000_wcc.py error_gates.c (zero warnings); wdis (conditions,
callback arguments, AL one after returning error, saved registers and
plain RETs); hybrid_link.py pinned payload/full objects (2660 redirects,
601176 arena bytes,388/428 modules,1613 exclusions); decomp_status.py check
(4293 functions); git diff --check. Evidence:
local/error-gates-20261116/{error_gates.obj,error_gates.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover pointer-table and node allocation wrappers

Recover alloc_slot_table_39db0 (20 original bytes) and make_node_8204c
(18 bytes) in allocation_wrappers.c; runtime_helpers30 had no definitions.
ASM and sole callers 39dd4/4b0a5 establish AL count with no consumed output,
and EAX value with EAX node output, respectively; six other registers
preserved and plain RET. Inspect allocator targets 984f0/9850c: EAX byte
size only, saved EDX, no flags argument. Existing caller ABI unchanged.

Allocate count narrowed to byte times four and store pointer at 1a1860.
For node, allocate twelve bytes then unconditionally store original value
at offset zero and return allocation. No extra initialization or null check;
allocator supplies its original behavior for remaining bytes. Name callback
and pointer-table globals. Remove both generic probe tracker references,
retaining JSON files.

Commands: ef2000_wcc.py allocation_wrappers.c (zero warnings); wdis (byte
extension and size scaling, indirect EAX size calls, saved node value,
unconditional first-word store, EAX node return and plain RETs);
hybrid_link.py pinned payload/full objects (2662 redirects,601276 arena
bytes,389/429 modules,1611 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/allocation-wrappers-20261116/{allocation_wrappers.obj,
allocation_wrappers.lst,full.json}. Status remains compiles, counts unchanged.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-16: recover optional word callback

Recover run_word_callback_ab820 (21 original bytes) in word_callback.c;
runtime_helpers30 had no definition. Original ASM and caller sites
f1a02/f1a5f establish EAX value, six other registers preserved, plain RET;
callers retain EDX and EBX respectively. No existing C users found and no
caller ABI changes required.

Keep callback presence test followed by a separate live indirect-target
read; volatile accesses preserve both reads. Pass low sixteen value bits
zero-extended in EAX, with no added fallback or second guard. Name callback
slot. Remove generic probe tracker reference, retain JSON.

Commands: ef2000_wcc.py word_callback.c (zero warnings); wdis (two callback
memory accesses, conditional mask, indirect EAX argument call, saved
registers and plain RET); hybrid_link.py pinned payload/full objects
(2663 redirects,601342 arena bytes,390/430 modules,1610 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/word-callback-20261116/{word_callback.obj,word_callback.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover stack-pool block allocation

Recover take_pool_block_b3db0 (150 original bytes) in pool_block.c;
runtime_helpers34 had no definition. ASM and callers e749/bc4a establish
EAX size, full EAX mixed address/handle result, six other registers preserved,
plain RET. Both callers sign-extend AX before resolving a handle. No caller
ABI changes. Original b3fb0 saves/restores EAX, so the return retains the
block address's upper sixteen bits, replacing only AX with count plus bias.

Preserve wrapping remaining subtraction/store before signed threshold
error, then fresh count/base/remaining/table reads. Store block in table,
reload count after that potentially aliasing store, read signed limit before
word count update, then report exhausted handles if necessary. Returning
errors continue. Clear original size bytes at captured block, reload count
and bias afterward, preserve mixed return bits. Declare callback shim
clobbers locally. Name six globals; remove generic probe tracker reference,
retain JSON.

Commands: ef2000_wcc.py pool_block.c (zero warnings); wdis (signed checks,
ordered reads/stores and reloads, callback argument preservation, clear
size, mixed return and plain RET); hybrid_link.py pinned payload/full
objects (2664 redirects,601568 arena bytes,391/431 modules,1609 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/pool-block-20261116/{pool_block.obj,pool_block.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover group-node field visitor

Recover visit_group_nodes_ab28c (149 original bytes) in group_node_visit.c;
runtime_helpers33 had no definition. Original ASM and sole caller ab5a8
establish EAX stream/context, six other registers preserved, plain RET.
Existing parser_restore.c caller needs no contract change.

Preserve signed-low-word indices against live zero-extended word bounds,
initial head capture, per-group first-item read, and next links read after
processing. For type fourteen, capture record once, visit +1e unconditionally,
then freshly load +22 and +26 after preceding calls and visit when nonzero.
Finish with original ab174 on the context, even for zero groups. No null
link guards, cached counts or traversal fixes. Name group count. Original
outer stack counter's uninitialized high word is irrelevant to its low-word
increment and sign-extended comparison; C retains those low-word semantics.
Remove generic probe tracker reference, retain JSON.

Commands: ef2000_wcc.py group_node_visit.c (zero warnings); wdis (signed
indices/live word bounds, type branch, ordered field calls/loads, next-link
reads, final ab174 and plain RET); hybrid_link.py pinned payload/full
objects (2665 redirects,601746 arena bytes,392/432 modules,1608 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/group-node-visit-20261116/{group_node_visit.obj,group_node_visit.lst,
full.json}. Status remains compiles, counts unchanged. No runtime/UI/replay/
differential execution or packaged runtime rebuild; full RAM/stack
equivalence and reported crashes remain unverified.

### 2026-11-16: recover event counter updates

Recover count_event_9aa78 (182 original bytes) in event_count.c;
runtime_helpers31 had no definition. ASM and sole caller 9a683 establish
DX kind only, EAX scratch, six other registers preserved, plain RET. Remove
false EAX record parameter from both declarations and signature; explicit
DX-only pragma preserves register placement. No existing C users found.
Watcom treats parameter EDX as scratch; hybrid entry restores original EDX.

Capture slot word, select rows for kinds 0/13/1d/26/31/49/4f, sign-extend
slot for unchecked indexing, increment selected word with wrap, then write
back captured slot even for unknown kinds. This final store matters if an
unchecked counter index aliases the slot itself. No bounds checks or slot
normalization. Name slot and seven nominal three-word rows. Remove generic
probe tracker reference, retain JSON.

Commands: ef2000_wcc.py event_count.c (zero warnings); wdis (DX input,
signed index extension, seven branches, wrapping word increment and final
slot writeback, plain RET); hybrid_link.py pinned payload/full objects
(2666 redirects,601924 arena bytes,393/433 modules,1607 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/event-count-20261116/{event_count.obj,event_count.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover quoted-string table loading

Recover load_string_table_9d990 (191 original bytes) in string_table_load.c;
runtime_helpers31 had no definition. ASM and sole caller 9da8d establish
no inputs, six other registers preserved, plain RET; caller ignores EAX.
Keep original unchecked 144-byte path buffer, copy/append targets, indirect
size query, returning zero-length error path and exact-length allocation.
Existing caller contract unchanged.

Store storage pointer before file read. A narrow register adapter supplies
EAX path and EDX/EBX/ECX buffer, then captures returned ECX and EBX, matching
the original decimal source and scan cursor rather than assuming either
still equals allocation. Allocate low-word count times four, scan quotes
without NUL/bounds checks, publish pointers before scanning closing quotes,
replace closing quotes with NUL, and publish table pointer last. No extra
terminator, frees or guards. Name storage global. Remove generic probe
tracker reference, retain JSON.

Commands: ef2000_wcc.py string_table_load.c (zero warnings); wdis (144-byte
frame, callback registers and capture, allocation sizes, low-word indexing,
ordered quote scan/stores, final publication and plain RET); hybrid_link.py
pinned payload/full objects (2667 redirects,602166 arena bytes,394/434
modules,1606 exclusions); decomp_status.py check (4293 functions);
git diff --check. Evidence:
local/string-table-load-20261116/{string_table_load.obj,
string_table_load.lst,full.json}. Status remains compiles, counts unchanged.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-16: recover file-open mode parsing

Recover parse_open_mode_bc1d3 (180 original bytes) in open_mode.c;
runtime_helpers34 had no definition. ASM and sole caller bc291 establish
EAX mode, EAX flags, six other registers preserved, plain RET. Caller
retains EDX mode and ECX stream. No caller ABI change.

Lowercase only initial byte via original e7835; reject anything other than
r/w/a by storing error nine via ebe6d and returning zero. Preserve repeated
second-byte reads and ordered optional third-byte reads with volatile byte
accesses. Preserve +b/+t/b+/t+ flag rules, exact default-mode dword comparison
with 0x200, lowercase-only suffix recognition, and final read/write/append
bits. No mode-length checks or standard-library replacement. Name default
translation-mode global. Remove generic probe tracker reference, retain JSON.

Commands: ef2000_wcc.py open_mode.c (zero warnings); wdis (initial lowercase
call, error path, suffix reads and flag branches, default comparison, final
bits, register saves and plain RET); hybrid_link.py pinned payload/full
objects (2668 redirects,602408 arena bytes,395/435 modules,1605 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/open-mode-20261116/{open_mode.obj,open_mode.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover attack-pattern group output

Recover dump_group_list_b29a8 (129 original bytes) in group_list_dump.c;
runtime_helpers33 had no definition. ASM and caller sites b26d2/b26ff
establish EAX list, EDX output, EAX/EDX scratch, five other registers
preserved, plain RET. Correct false EDX preservation. No existing C users
found, so no caller objects need recompiling.

Preserve initial signed count read before unchecked byte-indexed name read,
header output even for zero/negative counts, post-output count reload,
six-byte group indexing with signed low-word index, group count output,
and live table/count reloads before each 17-byte record. Retain signed word
counter comparisons and post-output reloads of outer count. Use original
format addresses and print helpers, no new checks or formatting strings.
Name attack-pattern name table start, extent unknown. Remove generic probe
tracker reference, retain JSON.

Commands: ef2000_wcc.py group_list_dump.c (zero warnings); wdis (header
argument order, signed count/index operations, live table/count reads,
17-byte stride, EDX scratch and plain RET); hybrid_link.py pinned payload/
full objects (2669 redirects,602584 arena bytes,396/436 modules,1604
exclusions); decomp_status.py check (4293 functions); git diff --check.
Evidence: local/group-list-dump-20261116/{group_list_dump.obj,
group_list_dump.lst,full.json}. Status remains compiles, counts unchanged.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-16: recover timeslot point output

Recover print_point_b2d8c (155 original bytes) in point_output.c;
runtime_helpers34 had no definition. ASM and sole caller b2f28 establish
EAX seven-byte record, EDX output, six other registers preserved, plain RET.
No existing C users found; no caller ABI changes. Watcom treats parameter
EDX as scratch; hybrid entry restores original preserved EDX.

Keep action-byte format branch, overlapping dword reads at +5 then +3
with arithmetic high-word extraction, optional mission-name lookup before
action-name lookup, descending byte-field reads and original cdecl argument
order. No narrowed memory reads, enum checks or new format strings. Extend
evidence on the existing 19-/30-name dictionary globals rather than duplicate
them. Remove generic probe tracker reference, retain JSON.

Commands: ef2000_wcc.py point_output.c (zero warnings); wdis (branch,
overlapping dword loads/shifts, lookup and byte-read order, cdecl pushes,
32-/36-byte cleanup and plain RET); hybrid_link.py pinned payload/full
objects (2670 redirects,602826 arena bytes,397/437 modules,1603 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/point-output-20261116/{point_output.obj,point_output.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover record text output

Recover print_record_b21ec (140 original bytes) in record_output.c;
runtime_helpers33 had no definition. ASM and caller sites b21c7/b27d0
establish EAX record, EDX output, six other registers preserved, plain RET.
Existing C users game_helpers130/misc_drafts1 need no ABI change or rebuild.

Preserve null-record branch's three output calls, including repeated final
format. For populated record, read overlapping dwords at +12/+10/+e/+c/
+a/+8 in descending order and arithmetic-shift their high words, then read
bytes +9 through +5 and name pointer +0. Preserve original fourteen cdecl
arguments and 56-byte cleanup. No narrowing memory reads, added field
checks or new formatting text. No new globals. Remove generic probe
tracker reference, retain JSON.

Commands: ef2000_wcc.py record_output.c (zero warnings); wdis (descending
dword/byte reads, signed shifts, argument push order, 56-byte cleanup,
three null-case calls and plain RET); hybrid_link.py pinned payload/full
objects (2671 redirects,603100 arena bytes,398/438 modules,1602 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/record-output-20261116/{record_output.obj,record_output.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover both named-table output variants

Recover dump_table_a_b2810 and dump_table_b_b28b8 (166 original bytes each)
in table_output.c; runtime_helpers33 had no definitions. ASM and sole
caller sites b2cb9/b2cd1 establish EAX record, EDX output, EAX/EDX scratch,
five other registers preserved, plain RET. Correct false EDX preservation
for both. No existing C users found; no caller object rebuild required.

The variants differ only by first name dictionary (133488 vs 1333f8).
Share recovered C logic: ordered third/second/first dictionary reads then
identifier, original header and coordinate calls, overlapping dword high
reads at +1c/+18/+1a, original route header, live signed count and table
reloads for 25-byte records. Pass current signed index in DX and output in
EBX to b2504, increment counter before call, preserve final output even
for empty tables. No bounds checks or new format strings. Remove both
generic probe tracker references, retain JSON files.

Commands: ef2000_wcc.py table_output.c (zero warnings); wdis (both wrapper
dictionary constants, common lookup/read order, cdecl arguments, signed
loop and b2504 register setup, final output and plain RETs); hybrid_link.py
pinned payload/full objects (2673 redirects,603372 arena bytes,399/439
modules,1600 exclusions); decomp_status.py check (4293 functions);
git diff --check. Evidence:
local/table-output-20261116/{table_output.obj,table_output.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover object and timeslot-list output

Recover print_object_b2e98 (176 original bytes) in object_output.c;
runtime_helpers34 had no definition. ASM and sole caller b2e55 establish
EAX record, EDX output, EAX/EDX scratch, five other registers preserved,
plain RET. Correct false EDX preservation and rebuild game_helpers127,
the sole C alias user. Correct point_output.c comment: caller stride is
nine bytes, not seven; previous point-output evidence wording was wrong.

Preserve header field read order, two raw dwords for the original double's
cdecl argument slot (no floating conversion), unchecked dictionary reads,
coordinate call, signed-low-word active count formatting, signed live count
and table reloads, nine-byte indexing, nonzero action-byte filter, final
format even for empty lists. No new guards or formatting text. Existing
dictionary globals reused. Remove generic probe tracker reference, retain
JSON.

Commands: ef2000_wcc.py object_output.c and game_helpers127.c (zero warnings);
wdis (header field order and raw dword pushes, 40-byte cleanup, active count
sign extension, nine-byte loop and plain RET); hybrid_link.py pinned payload/
full objects (2674 redirects,603644 arena bytes,400/440 modules,1599
exclusions); decomp_status.py check (4293 functions); git diff --check.
Evidence: local/object-output-20261116/{object_output.obj,object_output.lst,
game_helpers127.obj,full.json}. Status remains compiles, counts unchanged.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-16: recover record-pool allocation

Recover alloc_record_pool_b0138 (208 original bytes) in record_pool.c;
runtime_helpers33 had no definition. ASM and sole caller 49461 establish
EAX list-pointer address, EDX count-pointer address, EAX allocation result,
EDX scratch, five other registers preserved, plain RET. Correct false EDX
preservation. No existing C users found; no caller rebuild needed.

Capture head before zeroing output count; walk live byte bounds and table
pointers with signed low-word indices, accumulating byte weights times twenty
with wrap. Default zero count to 2000. Preserve four cdecl allocator arguments,
54-byte size wrap, count reload before clear, and signed live count reloads
after each three-word sentinel initialization at +30/+32/+34. Reset final
pool state dword then word; return allocation. No overflow checks, alias
guards or cached loop limits. Name two final state globals. Remove generic
probe tracker reference, retain JSON.

Commands: ef2000_wcc.py record_pool.c (zero warnings); wdis (head-before-
clear, live traversal reads, allocator argument order, post-call count reload,
ordered sentinels/live count, final state writes and plain RET);
hybrid_link.py pinned payload/full objects (2675 redirects,603868 arena
bytes,401/441 modules,1598 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/record-pool-20261116/{record_pool.obj,record_pool.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover matching-entry linkage

Recover link_matching_entries_be320 (210 original bytes) in entry_link.c;
runtime_helpers34 had no definition. ASM and sole caller b15e4 establish
EAX/EDX list-pointer addresses, EAX/EDX scratch, five other registers
preserved, plain RET. Correct false EDX preservation; caller replaces EDX
immediately afterward. No existing C users found; no caller rebuild needed.

Preserve first-list eligibility byte checks, kind exclusions 15/17/16,
fresh second-head read for each eligible node, second-node byte/word gates,
live entry count/table reads with signed low-word index and 124-byte stride.
Keep entry-kind exclusions 6/8/7/9, ordered identity/index word comparisons,
original bb7c0 lookup, +53 word clear before +51 result store, continued
scanning after matches, and next links read after processing. No new null
checks or early exits. No new globals. Remove generic probe tracker
reference, retain JSON.

Commands: ef2000_wcc.py entry_link.c (zero warnings); wdis (gates and
comparison order, live table/count reads, lookup/store order, post-processing
links, saved registers and plain RET); hybrid_link.py pinned payload/full
objects (2676 redirects,604108 arena bytes,402/442 modules,1597 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/entry-link-20261116/{entry_link.obj,entry_link.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16: recover active-record statistics dispatch

Recover update_records_9a624 (213 original bytes) in record_update.c;
runtime_helpers31 had no definition. ASM and sole caller 98bf1 establish
no inputs, six registers preserved, EAX scratch, plain RET. No caller ABI
changes. Air-counter ASM overwrites incoming EDX before use, so retain its
single-record argument; ground/event counters consume signed low-word
classification. Event counter uses the corrected DX-only contract.

Preserve signed low-word index against live dword active count, fresh pool
base each iteration, overlapping dword high-word active-state read, classify
before kind reload, and kind 0/1-or-2/3 dispatch. Then perform four iterations
of ordered load/load/add/store on three word totals without clearing them;
keep word wrap and final 9b780 then 9b8a0 calls. No bounds checks or cached
mutable counts. Name pool pointer, totals and tally rows. Remove generic
probe tracker reference, retain JSON.

Commands: ef2000_wcc.py record_update.c (zero warnings); wdis (live
count/base, signed index, classifier/kind order and argument registers,
ordered wrapping word sums, final calls and plain RET); hybrid_link.py
pinned payload/full objects (2677 redirects,604382 arena bytes,403/443
modules,1596 exclusions); decomp_status.py check (4293 functions);
git diff --check. Evidence:
local/record-update-20261116/{record_update.obj,record_update.lst,full.json}.
Status remains compiles, counts unchanged. No runtime/UI/replay/differential
execution or packaged runtime rebuild; full RAM/stack equivalence and
reported crashes remain unverified.

### 2026-11-16 — unsupported-operation error wrappers

Recover unsupported_84650, unsupported_query_e1730/e1744/e1758/e1780
(17 bytes each) and fatal_with_arg_e1948 (16 bytes) in
unsupported_errors.c. These runtime_helpers30 entries lacked definitions.
Original ASM and callers establish no query inputs, AL-only zero results,
and EAX value input for e1948, with plain RET. Keep each distinct format
address, fresh shared callback load, cdecl caller cleanup and return after
error reporting; e1948 passes format then value. Review callers 34150/3415e,
34ef5/34f03,343a3,342e1,846bf and e1efb/e1fc8. Preserve existing signatures;
no caller rebuild needed. Explicit bridge scratch declarations prevent C
register allocation across the callback. Wdis confirms AL-only clearing,
format/value order and preserved registers. Remove generic probe tracker
references but retain JSON files. Status remains compiles.

Commands: ef2000_wcc.py unsupported_errors.c (zero warnings); wdis;
hybrid_link.py pinned payload/full objects (2683 redirects,604650 arena
bytes,404/444 modules,1590 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/unsupported-errors-20261116/{unsupported_errors.obj,unsupported_errors.lst,full.json}.
No runtime/UI/replay/differential execution or packaged runtime rebuild.
Full guest RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-16 — carried-weapon rating selection

Recover missing pick_best_weapon_16bac (205 original bytes) in
weapon_select.c. Original ASM and callers 1690e/16f07/17025 establish
EAX index, EDX kind output, EBX id output, ST0 result; ECX/ESI/EDI/EBP
preserved, EAX/EDX/EBX scratch, plain RET. Correct the false EAX floating
return and EBX/EDX preservation in signatures, add explicit header pragma,
and rebuild misc_drafts1, the sole existing C caller module.

Preserve seven carried-id reads with fresh world-pool base, 202-byte
record stride, id 0/255 rejection and signed AX filtered lookup. Visit all
20 three-byte weapon entries; test signed positive quantity before signed
byte kind, accept unsigned ranges 1..5 and 17..19, compare signed ratings
with initial -1 and strict improvement. Store kind dword before id word;
leave outputs untouched if no improvement. Keep final FILD dword/FMUL
qword in a small ABI adapter returning ST0 without a double spill, so the
C selection loop does not round the original extended-precision result.
Name first rating field and scale constant. Tracker remains compiles;
remove generic probe reference, retain JSON.

Commands: ef2000_wcc.py weapon_select.c and misc_drafts1.c (zero warnings);
wdis (live pool loads, signed tests, ordered output stores and unspilled
ST0 return); hybrid_link.py pinned payload/full objects (2684 redirects,
604872 arena bytes,405/445 modules,1589 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/weapon-select-20261116/{weapon_select.obj,weapon_select.lst,misc_drafts1.obj,full.json}.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

Also inspected ac240 against its callers and ac2e4: initial CX captures
old registered count before fill, and indirect callbacks carry loop state
in ECX. Leave its missing body pending a confirmed callback register
contract rather than assuming an ordinary C callback.

### 2026-11-16 — matching-record ratio calculation

Recover missing update_match_ratio_c964 (218 original bytes) in
match_ratio.c. ASM and sole caller c27f establish no inputs, six preserved
registers, EAX scratch and plain RET; existing ABI needs no caller rebuild.
Capture signed word group count before dword active-record count, sum
only enabled group totals with dword wrapping, and scan signed low-word
record indices against the captured dword bound. Reload pool per record,
selected word before record word, group field on each inner iteration,
and preserve short-circuit group/enabled/overlapping dword state reads.
Use volatile guest reads to retain the original access order.

The final operation consumes both dword accumulators as signed words.
Keep FILD word/FILD word/FDIVP/FLD scale/FXCH/FMUL/FSUBP/FSTP float in a
small x87 adapter, including zero-denominator behavior and one final
single-precision rounding. Preserve final dword active-count then word
group-count writebacks. Explicit counter stores avoid a compiler-created
MOVSD initializer and its additional direction-flag dependency. Name six
ratio fields/tables/constants without inventing table extents. Remove
generic probe tracker reference, retain JSON; status remains compiles.

Commands: ef2000_wcc.py match_ratio.c (zero warnings); wdis (ordered loads,
signed word iteration, dword accumulation, original x87 opcodes, ordered
writebacks and plain RET); hybrid_link.py pinned payload/full objects
(2685 redirects,605146 arena bytes,406/446 modules,1588 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/match-ratio-20261116/{match_ratio.obj,match_ratio.lst,full.json}.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-16 — ordered dword fill with secondary result

Recover missing fill_dwords_deb87 (108 original bytes) in dword_fill.c.
Original ASM, pseudo-C and callers a3b59/deb67 establish EAX destination,
EDX pattern, ECX count, EAX end-pointer and ECX residual outputs. Correct
signature to include both outputs and preserved EDX. Existing void alias
callers discard results; no alias ABI change or caller rebuild required.

Keep alignment stores until 32-byte boundary or count exhaustion; unsigned
count arithmetic, ordered four/eight-word blocks, read-ahead byte before
the second four stores, final group and one/two/three-word tail. Preserve
ECX equal to one after a three-word tail, zero otherwise. Volatile stores
prevent combining/reordering. Wdis showed Watcom discarding the initial
void-cast volatile read, so use an explicit CMP-byte adapter; use an empty
EAX/ECX result adapter with EDX pattern input to avoid __U8LS and restore
EDX before return. No added guards, alignment restrictions or fault masking.

Commands: ef2000_wcc.py dword_fill.c (zero warnings); wdis (ordered stores,
read-ahead CMP, residual register, end pointer, EDX preservation and RET);
hybrid_link.py pinned payload/full objects (2685 redirects,605322 arena
bytes,407/447 modules,1588 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/dword-fill-20261116/{dword_fill.obj,dword_fill.lst,full.json}.
Status remains compiles, remove generic probe tracker reference but retain
JSON. The linker explicitly excludes deb87: multiple return registers need
a custom trampoline. Thus this recovered body is compiled but NOT redirected
into the game. No runtime/UI/replay/differential execution or packaged
runtime rebuild; full RAM/stack equivalence and reported crashes remain
unverified.

### 2026-11-16 — front-end shared-tail callbacks

Recover gui_open_credits_37260 (registry onopenCredits,83 body bytes) and
gui_new_socket_37254 (registry gnNewSocket,12 bytes) in credits_gate.c.
Remove their host-hook draft definitions from gui_callbacks_089.c. Original
ASM, both pseudo-C exports and callback-table references 12b548/12b588
establish no inputs, EAX scratch, six preserved registers and plain RET.
Add confirmed signature rows and move tracker entries blocked -> compiles.

Resolve shared 3718e..371ad branches directly: mode one tests 125935,
then schedules name 183290 via ac7a8 or stores latch byte 100; other modes
return except mode three, where network flag zero and signed high word of
the overlapping dword at 19769e not minus one tail-call b1e0. Inspect b1e0
entry: incoming EAX is not an input. New-socket stores mode one then runs
the complete shared callback. Keep its mode reload, guest call targets,
byte stores and overlapping dword read; do not retain host hook calls.
Original callback registry names do not establish the meaning of every
branch, nor prove a repair of the reported credits crash.

Run Ghidra 12.1.4 audit-body-holes read-only with temporary config/cache/home
under /tmp. Its one row identifies gap 371ae..3725f as padding plus other
functions, including the socket entry. No boundary repair needed. Earlier
launcher attempts failed before the temporary directories existed; final
audit completed successfully. Remove the two stale unreviewed-mechanical
exclusions now that these entries have ASM-reviewed hand definitions.

Commands: ghidra_ef2000.py audit-body-holes 0x37254 0x37260;
ef2000_wcc.py credits_gate.c and gui_callbacks_089.c (zero warnings); wdis;
hybrid_link.py pinned payload/full objects (2687 redirects,605342 arena
bytes,408/448 modules,1586 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/credits-gate-20261116/{body-holes.tsv,body-holes.log,credits_gate.obj,credits_gate.lst,reviewed.json}.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-16 — assignment navigation and save callbacks

Recover 4d9cc/4da2c/4da90 (95/99/85 original bytes) in assignment_actions.c,
replacing their host-hook gui_callbacks_089 drafts. Keep existing C names
and record original registry names mpPrevAssign/mpMakeAssign/mpViewAssign.
ASM and callback registration/direct 4dad9 call establish no inputs, EAX
scratch, six preserved registers, plain RET. Confirm callees: 4f314 takes
EAX/EDX binding strings, 4db60 has no inputs. Add signatures, move three
tracker rows blocked -> compiles and remove stale mechanical exclusions.

Preserve initial assignment gate and fixed binding arguments; increment
or decrement/store index BEFORE wrap-bound read/adjustment, then reload
index for category lookup. Reverse accepts 254 and 255 as wraparound.
Retain original unbounded search if no category three/four exists. Save
callback loads index then byte, stores byte, freshly reloads index then
dword value, stores dword, invokes advancing callback and redraws again.
Volatile guest accesses preserve these alias-sensitive reads/stores,
including potential overlap between unchecked slot tables and index.
Reuse existing tactical globals, name four newly documented fields.

Commands: ef2000_wcc.py assignment_actions.c and gui_callbacks_089.c (zero
warnings); wdis (ordered stores/reloads, wrap comparisons, call arguments,
EDX preservation and double redraw); hybrid_link.py pinned payload/full
objects (2690 redirects,605412 arena bytes,409/449 modules,1583 exclusions);
decomp_status.py check (4293 functions); git diff --check. Evidence:
local/assignment-actions-20261116/{assignment_actions.obj,assignment_actions.lst,gui_callbacks_089.obj,full.json}.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-16 — warning acceptance and fuel-bar callbacks

Recover 4b1a4/50638 (75/15 original bytes) in frontend_warning.c,
replacing host-hook gui_callbacks_089 drafts. Keep original registry names
cgWarningOkClick/mpDrawFuelBar in tracker evidence. ASM and callback
entries establish no inputs, six preserved registers, plain RET. Warning
uses EAX scratch; fuel-bar returns the EAX pointer from a4db0. Correct its
C declaration from void to ef_addr32; no existing guest C callers.

Preserve network gate, two distinct EAX/EDX binding pairs, signed AX versus
fresh signed word threshold after max_difference_50770, then widget refresh
before fixed pending-name scheduling. Fuel-bar resets view first then
returns apply_a4b80_a4db0(184ec4). Inspect ac9e8/a4db0 original entries to
confirm no hidden input and pointer return. Wdis emits the original
15-byte CALL/MOV/JMP sequence for this body, with relocated call targets.
Reuse documented network/threshold globals. Add confirmed signatures,
move two tracker entries blocked -> compiles and remove stale mechanical
exclusions. Retain legacy host-model tests without executing them; these
are not guest-equivalence evidence for the recovered implementations.

Commands: ef2000_wcc.py frontend_warning.c and gui_callbacks_089.c (zero
warnings); wdis (signed word comparison, argument pairs, call order,
EDX preservation and pointer tail return); hybrid_link.py pinned
payload/full objects (2692 redirects,605384 arena bytes,410/450 modules,
1581 exclusions); decomp_status.py check (4293 functions); git diff --check.
Evidence: local/frontend-warning-20261116/{frontend_warning.obj,frontend_warning.lst,gui_callbacks_089.obj,full.json}.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-17 — type-selection widget callbacks

Recover 5089c/508fc/5095c (96 original bytes each) in type_actions.c,
replacing host-hook gui_callbacks_089 drafts. Original ASM and all three
pseudo-C exports establish EAX widget input, six preserved registers,
EAX scratch and plain RET. Registry names mpType2Click/mpType3Click/
mpType4Click select groups 3/4/1. Add signatures, move three tracker rows
blocked -> compiles and remove stale unreviewed-mechanical exclusions.

Preserve zero-extended first byte minus 48 with wrapping subtraction;
only signed low word equal to one permits the second-byte read. Accept
second digits 48..57 and subtract 38, otherwise retain first result.
Original 50a1c consumes AL group only and preserves EDX; do not invent
an item input. If AL result is nonzero, pass binding strings 1326e7/132704
to 4f314. Otherwise call 50b68 with group and item masked to low byte,
then original 5086c with no inputs. Inspect both callees' ASM and the
5086c register saves; use a local explicit no-input/EAX-scratch pragma
for its existing guest alias. The latter remains original, since its own
shared-tail C recovery is blocked. No added validation or digit parser.

Commands: ef2000_wcc.py type_actions.c and gui_callbacks_089.c (zero
warnings); wdis (conditional second read, group-only lookup, mask,
argument registers, binding branch, final refresh and register saves);
hybrid_link.py pinned payload/full objects (2695 redirects,605406 arena
bytes,411/451 modules,1578 exclusions); decomp_status.py check (4293
functions); git diff --check. Evidence:
local/type-actions-20261116/{type_actions.obj,type_actions.lst,gui_callbacks_089.obj,full.json}.
No runtime/UI/replay/differential execution or packaged runtime rebuild;
full RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-17 — project status command

Added `nix run .#status`, an asset-free report from the flake snapshot's
function tracker. Reports cumulative draft/compilation/integration milestones,
verified evidence, blocked and incorrect work with original body byte shares.
Explicitly distinguishes recorded behavioral verification from untracked
byte-identical recompilation; does not promote tracker states or run game tests.
Validation: `nix run .#status`, `python3 tests/test_decomp_status.py`
(10 tests), `python3 tools/decomp_status.py check`, `git diff --check`.

### 2026-11-17 — remaining batch 089 callbacks

Recovered the original 4b54 (39 bytes), 84ddc (13), and da2b0 (8) into
actual guest C in gui_callbacks_089.c, removing its host-call hook. The first
loads the child pointer at widget+51, closes its byte ID at +10, tests AL
from hiscore_qualifies, and enters select_first_entry_487c or pending-name
ac7a8(17f79c). The summary callback closes its own byte ID. Both preserve
six GPRs, with EAX scratch and no observable return. Registry slots
12b838/12bb68 confirm the widget callbacks; the cloud command is at 15bb84.
Ghidra audit-body-holes found zero holes at all three entries. The external
terminal branches target complete functions, with plain RET and no extra
stack arguments; 487c overwrites EAX before its first call.

The cloud command stores byte one at 425c88 and preserves all seven GPRs.
A void C declaration with modify[] still allowed Watcom to use EAX as scratch;
an explicit EAX identity input/output produces the required preservation.
Wdis shows EDX saved, incoming EAX retained through the store, EAX restored,
EDX popped, RET. Updated the legacy host test call for the new C declaration;
that draft-model test was not executed. Tracker states are compiles, not
verified; removed the two corresponding stale mechanical exclusion rows.

Validation: ef2000_wcc.py gui_callbacks_089.c (zero warnings), wdis
(call arguments, AL test, terminal branches, register preservation),
hybrid_link.py pinned payload/full objects (2698 redirects,605412 arena
bytes,412/451 modules,1576 exclusions), decomp_status.py check (4293
functions), git diff --check. Evidence: local/menu-callbacks-20261117/.
No gameplay, UI, replay or differential runs, and no packaged runtime rebuild.
Reported crashes and full guest RAM/stack equivalence remain unverified.

### 2026-11-17 — type-selection refresh continuation

Recovered refresh_type_widgets_5086c in type_refresh.c (273 original body
bytes): 5086c..5089a plus 50a52..50b34. The body-hole audit reports the
439-byte gap 5089b..50a51 as one padding byte and 438 bytes of other
functions (four callbacks plus tactical_lookup_contains); no boundary repair
is needed. Original entry has no incoming GPR reads, saves EBX/ECX/EDX,
leaves ESI/EDI/EBP, and returns with EAX scratch. Two reviewed callers
5089c@508f4 and 508fc@50954 discard EAX and restore their saved registers.
The existing guest_fn_5086c_w alias remains available to recovered callers.

The C body finds root 184af0, clears child dirty state, then visits slots
1..4 using each fresh byte at 1a553b+slot. The twelve original format
addresses map codes 0..5,18,7,6,16,17,22 to suffixes 0..11. It formats into
one 20-byte stack buffer and enables the corresponding named child. An
unsupported code intentionally skips formatting, retaining the previous
name or the original uninitialized first-iteration buffer. No zeroing,
validation, or fallback has been introduced. Existing g_dialog_slot_flags
covers the touched guest bytes.

Validation: ef2000_wcc.py type_refresh.c (zero warnings), wdis (all twelve
formats, skipped format path, 20-byte buffer, four iterations, cdecl stack
cleanup, register saves/RET), hybrid_link.py pinned payload/full objects
(2699 redirects,605686 arena bytes,413/452 modules,1575 exclusions),
decomp_status.py check (4293 functions), git diff --check. Evidence:
local/type-refresh-20261117/. Tracker is compiles; the stale mechanical
exclusion was removed. No runtime/UI/replay/differential execution or
packaged runtime rebuild. Full RAM/stack equivalence and reported crashes
remain unverified.

### 2026-11-17 — difficulty and map-zoom callbacks

Recovered gui_change_difficulty_4cf4 (86 original bytes) and
gui_zoom_map_a974 (49) into difficulty_zoom.c; removed their host-only
implementations from gui_callbacks_088.c and rebuilt that module. Both take
EAX widget, preserve six other GPRs and return with EAX scratch. Callback
registry slots 12b810 and 12b968 establish the UI entry points; there are
no ordinary direct call sites to these entries. Original callees and their
confirmed register contracts were checked against the call sequences.

Difficulty parses widget+3f, writes only AX to 121e46, looks up 17f7a4,
then reloads the overlapping signed dword at 121e44 and shifts by 16.
Unsigned arithmetic retains the original wrapping (selection-1)*76 plus
pool 121e40 and offset 28. Calls 7c4fc with target EDX; only after it
returns reads the target byte ID and calls a5ef8. Zoom stores the exact
16-bit 1-minus-old value at 121e8e, then selects 132119 for nonzero or
13211e for zero and calls a7588. Non-Boolean values remain distinguishable.
Added three guest-global facts and confirmed signatures, promoted the two
rows to compiles, and removed their stale mechanical exclusions.

Validation: ef2000_wcc.py difficulty_zoom.c and gui_callbacks_088.c (zero
warnings), wdis (store width, post-call reloads, signed shift, wrap stride,
16-bit subtraction, arguments and register saves/RET), hybrid_link.py
pinned payload/full objects (2701 redirects,605690 arena bytes,414/453
modules,1573 exclusions), decomp_status.py check (4293 functions),
git diff --check. Evidence: local/difficulty-zoom-20261117/. No gameplay,
UI, replay or differential execution or packaged runtime rebuild. Runtime
RAM/stack equivalence and reported crashes remain unverified.

### 2026-11-17 — target-view close callback

Recovered gui_close_target_view_b574 in target_close.c, replacing the
host-only draft in gui_callbacks_088.c. Original b574..b5cc is 89 bytes,
EAX widget input, EAX scratch, six other GPRs preserved, plain RET.
Callback registry slot 12b8e0 names gpwCloseTargetView; no ordinary direct
callers. The two strcmp calls both compare widget+3f against 180834;
retained both calls and all three byte stores (2,1,0) at 121e95. The second
comparison is intentionally not simplified. Then lookup root 1805f0 and
redraw its byte ID at +10. Existing confirmed guest callee signatures
cover a4380, a46cc and a5ef8. Added g_target_view_mode and the entry ABI;
tracker promoted to compiles and stale mechanical exclusion removed.

Validation: ef2000_wcc.py target_close.c and gui_callbacks_088.c (zero
warnings), wdis (both calls, branches/stores, final lookup/ID/redraw,
register saves/RET), hybrid_link.py pinned payload/full objects (2702
redirects,605676 arena bytes,415/454 modules,1572 exclusions),
decomp_status.py check (4293 functions), git diff --check. The generated
body is also 89 bytes but uses different registers and instructions;
equal length does not establish byte-identical recompilation. Evidence:
local/target-close-20261117/. No runtime/UI/replay/differential execution
or packaged runtime rebuild. Runtime equivalence and crashes unverified.

### 2026-11-17 — byte-identical recompilation evidence in status

Added an independent byte-comparison report to nix run .#status: exact,
different, not-comparable, not-measured and stale, each with function/body
byte totals. This supplements the functional decompilation objective in
section 2.2; no tracker state is promoted and differing instructions are
not a behavioral failure. tools/recompilation_status.py recompiles a
production module with ef2000_wcc's profile, uses an explicitly reviewed
contiguous original body and compiled extent excluding padding, resolves
external OMF relocations at the original address, and compares all bytes
without masks, trampoline substitution or added padding. Internal/group
relocations, uncertain frames and discontiguous layouts fail closed as
not comparable. No original bytes or objects enter the committed report.

Hash-only records in decomp/symbols/recompilation.json bind the function,
source/header/ABI/global/tool inputs, flags, compiler binary, object and
pinned payload. Status conservatively invalidates changed inputs or flags;
compiler identity is retained as path/SHA-256. Fresh recompilation confirms
121f0 is exact (29 bytes) and b574 differs (89 bytes). Remaining 4291
functions are unmeasured; historical prose alone does not count as fresh
evidence. README documents the record command and layout restrictions.

Validation: production runtime_indexed.c and target_close.c compiled with
zero warnings; python3 -m unittest tests.test_recompilation_status
tests.test_decomp_status -q (14 passing); nix run .#status; tracker check;
git diff --check. Synthetic checks cover external relative relocation,
strict instruction/length differences, unresolved/internal fixups, crossed
public boundaries, and stale-source exclusion. No gameplay/UI/replay or
differential execution. The reliable bulk translation goal remains open.

### 2026-11-17 — Translate descriptor-backed XLAT accesses

XLAT now reads through the cached segment access helper, retaining the
instruction's DS default or explicit override, rather than the legacy flat
read helper. Descriptor profile 1 no longer rejects XLAT unconditionally.
The existing 32-bit address restriction remains; AL changes only after a
successful byte read, with upper EAX and flags preserved. No descriptor
reload or address-size override support is implied.

Synthetic captured-state checks cover DS, FS and CS XLAT execution, plus
an expand-down DS limit failure that must preserve registers, flags and
RAM. Validation: tests.test_asm_capture (4 passing) and
tests.test_asm_to_c.TranslationTests.test_synthetic_programs_match_registers_flags_and_all_ram
(1 passing) using the Nix Python/Unicorn environment; tracker check;
git diff --check. An earlier broader run loaded the fixture before its
expand-down boundary correction and failed that assertion; the corrected
capture suite was rerun successfully. No gameplay or UI runs.
Bulk translation remains incomplete: implicit stack/string accesses,
far-pointer loads, WAIT/x87 and descriptor reloads remain unwired.

### 2026-11-17 — Translate descriptor-backed scalar stack operations

Scalar 16/32-bit PUSH and POP now retain SS for implicit stack accesses
and the decoded segment for explicit memory operands. Stack stores check
the complete descriptor extent before writing bytes; the legacy flat RAM
fault ordering remains unchanged. POP memory destinations still calculate
using post-increment ESP and commit ESP only after the destination stores.
Segment-register PUSH/POP remain explicitly rejected in descriptor mode;
PUSHA/POPA, ENTER/LEAVE and CALL remain separate unwired operations.

Synthetic captured-state checks add register and memory stack operations,
16-bit operands, PUSH ESP / POP ESP, post-increment ESP addressing, and
SS write / DS destination limit rejection without architectural mutation.
Validation: tests.test_asm_capture and the synthetic instruction comparison
matrix in tests.test_asm_to_c; tracker check; git diff --check. No gameplay
or UI runs. This expands descriptor-backed translation support without
promoting any recovered function to behavior-verified status.

### 2026-11-17 — Translate descriptor-backed near CALL transfers

The composed C program renderer now checks the near CALL target against
cached CS and writes the return PC through the SS access helper before
committing ESP. Register/memory indirect targets are still fetched before
the stack write, including a target occupying the return-address slot.
The generic descriptor-mode CALL rejection is removed; standalone rendering
still rejects calls without the composed program renderer. Target registry
checks, direct-dependency closure and the absence of original-code fallback
remain unchanged. No far calls or descriptor reloads are enabled.

Synthetic captured-state comparisons cover direct, register-indirect,
[ESP-4] and [ESP] calls through translated callees and returns. Explicit CS
target-limit and expand-down SS write failures preserve registers, flags,
PC and RAM. Validation: tests.test_asm_c_program and tests.test_asm_capture
in the matching Nix Python/Unicorn environment; tracker check;
git diff --check. No gameplay or UI runs. Reliable bulk translation remains
open, including compound stack, string and descriptor-reload operations.

### 2026-11-17 — Translate descriptor-backed LEAVE frame restoration

LEAVE now reads the saved frame pointer through cached SS, rather than the
legacy flat helper. ESP and EBP commit only after the saved-frame read
succeeds. This enables the existing 32-bit LEAVE translation in descriptor
profile 1 without changing the accepted instruction/operand forms.
ENTER remains rejected in descriptor mode pending allocation-limit handling;
PUSHA/POPA, strings, far loads and descriptor reloads also remain unwired.

Synthetic captured-state comparisons cover LEAVE after a conventional
PUSH EBP / MOV EBP,ESP prologue, with and without local stack allocation.
Explicit saved-frame reads at the expand-down boundary and across the RAM
extent reject without changing registers, flags or RAM. Validation:
tests.test_asm_capture and tests.test_asm_c_program in the matching Nix
Python/Unicorn environment; tracker check; git diff --check. No gameplay
or UI runs; the reliable bulk translation goal remains open.

### 2026-11-17 — Translate descriptor-backed string accesses

MOVS/CMPS/LODS now retain the decoded source segment, including overrides;
MOVS/STOS/CMPS/SCAS use ES for their destination access. Generated memory
reads/writes go through the cached descriptor helpers, removing the blanket
descriptor-mode string/REP rejection. Non-ES destination metadata is rejected.
The accepted 32-bit addressing forms, REP step budgeting, DF direction,
comparison termination, and zero-count no-access behavior remain unchanged.

Synthetic captured-state comparisons cover byte/dword copies, word fills,
loads, REPE comparisons, REPNE scans, backward copies, FS source overrides,
and zero-count copies with otherwise invalid offsets. A first-iteration
source-limit failure preserves registers (including ECX/ESI/EDI), flags and
RAM. This does not claim exception-delivery or later-iteration REP fault
rollback equivalence. Validation: tests.test_asm_capture,
tests.test_asm_c_program and the synthetic instruction comparison matrix in
tests.test_asm_to_c; tracker check; git diff --check. No gameplay or UI runs.
ENTER, PUSHA/POPA, far loads, WAIT/x87 and descriptor reloads remain unwired;
reliable bulk translation remains incomplete.

### 2026-11-17 — Regenerate bulk C and recover captured 0x604 execution

Regenerated the pinned inventory with descriptor-aware XLAT, scalar stack,
near CALL, LEAVE and string translations. Direct closure remains 2528
bodies (4143 syntactically supported); this work changes descriptor-mode
execution coverage, not body inventory. Compared with the November 16 scalar
snapshot, 2172 generated bodies lose 24438 unconditional descriptor-mode
instruction guards. These counts are coverage indicators, not equivalence
claims or recovered-function tracker promotions.

Compiled the complete regenerated program as a warning-free host library
(-O0, C99, -Wall -Wextra -Werror), retaining the existing unchanged narrow
and binary x87 backends. Reused the pinned EFCAP002 input for 0x604 with
its original 64 MiB RAM, selectors, descriptor caches and x87 state.
The full-state comparison now passes with no differences; the earlier
scalar snapshot stopped at entry with C status 7. Original execution used
the independent native x87 oracle (no x87 operations occurred on this path).
This is one captured path, not universal equivalence or gameplay validation.

Evidence (ignored): local/asm-c-segment-stack-string-program-20261117/
contains generated C, provenance report and descriptor-guard delta;
local/asm-c-segment-stack-string-capture-20261117/ contains the reproducible
check.py, host unit/library, compile log, capture/source/library hashes and
comparison report. The translated 0x604 unit also compiles warning-free
with wcc386 -bt=dos -3r -za99 -w1 -we. Tracker check and git diff --check
pass. No synthetic corpus rerun, gameplay or UI session. Remaining segment
reload, compound stack, x87 and fault-path coverage keeps the reliable bulk
translation goal open.

### 2026-11-17 — Enable descriptor-backed x87 register and control paths

Replaced blanket rejection of every x87 instruction with rejection of x87
memory operands whose helpers still use flat reads/writes. Register-only
stack/arithmetic operations and FNINIT/FNCLEX/WAIT can now execute with
captured descriptor caches. FNSTCW/FNSTSW/FLDCW already use decoded segment
helpers and are enabled as well. Unchecked real/integer/extended memory
operations remain rejected before execution; no descriptor lookup or
pending-exception delivery support is added.

Captured-state fixtures compare register-stack operations, sign/absolute
value, physical stack exchange/pop, status output, control-word store/load,
clear exceptions and WAIT against the independent native x87 oracle.
The native oracle is required here: the initial Unicorn-only run disagreed
on x87 IP/opcode bookkeeping. No state fields were masked to pass. A negative
fixture retains rejection of FLD tbyte memory in descriptor mode.
Validation: tests.test_asm_capture and tests.test_asm_c_program in the matching
Nix Python/Unicorn environment; tracker check; git diff --check. No gameplay
or UI runs; reliable bulk translation remains incomplete.

### 2026-11-17 — Translate descriptor-backed x87 memory operands

Generated x87 memory operations now check the full decoded operand extent
and access kind against its cached segment before invoking the existing
floating-point helper. Stores require write permission; SS accesses retain
stack checks. Encoded and decoded segment disagreement is rejected. The
existing pending-x87-exception check precedes the descriptor check. Removed
the remaining blanket x87 memory rejection; the helpers' RAM extent checks,
conversion behavior and x87 environment updates remain unchanged.

Captured-state comparisons against the native x87 oracle cover extended
loads/stores, integer and real loads with extended stores, and an FS source
override. Rejected extended reads and writes preserve GPRs, flags, RAM,
physical x87 registers and x87 environment fields. Validation:
tests.test_asm_capture, tests.test_asm_c_program and tests.test_asm_x87_memory
in the matching Nix Python/Unicorn environment; tracker check;
git diff --check. No gameplay or UI runs. This enables existing supported
x87 instruction forms, not arbitrary x87 instructions or exception delivery.
Descriptor reloads, compound stack operations and broader captured-path
coverage still keep the reliable bulk translation goal open.

### 2026-11-17 — Translate descriptor-backed PUSHA/POPA

The existing 32-bit register-stack helper now validates each actual memory
operand against cached SS before changing registers or RAM. PUSHA retains
the original ESP value in its saved slot; POPA skips that slot without
reading it. Removed their descriptor-mode rejection. Existing flat RAM
fault ordering and partial effects remain unchanged; 16-bit PUSHA/POPA
forms are still rejected by the encoding check.

The captured fixture now saves all registers, changes EAX/EDX and restores
through POPA, comparing full state and RAM with original execution.
Generated-path SS limit failures for both instructions preserve GPRs,
flags and RAM. Validation: tests.test_asm_capture and
tests.test_asm_c_program (13 passing), followed by a focused capture-suite
rerun for the added POPA rejection assertion; tracker check;
git diff --check. No gameplay or UI runs. ENTER and descriptor reloads
remain unwired, and broader captured execution is still required for the
reliable bulk translation goal.

### 2026-11-17 — Preserve RET fault instruction state

Generated near RET now reads its stack operand into a temporary before
committing PC and ESP. A cached-SS rejection previously assigned the failed
read's zero result to PC, losing the faulting instruction address. The
program fixture exercises a RET with a rejecting SS limit and checks that
GPRs, flags, PC and RAM remain unchanged. Existing direct and indirect CALL
execution comparisons remain covered. Validation: tests.test_asm_capture
and tests.test_asm_c_program (13 passing); git diff --check. No gameplay
runs. This closes a fault-state mismatch, not the remaining descriptor
reload or ENTER gaps.

### 2026-11-17 — Validate near RET targets against cached CS

Generated near RET now checks the return offset against the active code
segment before committing PC or ESP, using the same cached-CS validation
as near CALL. Previously a target beyond CS could be accepted as a stop
address or reach the dispatcher after ESP had already advanced. The
program fixture places RET at the last valid code byte with a readable
stack containing an out-of-limit return offset; rejection preserves GPRs,
flags, PC and all RAM. Existing captured successful calls and returns and
SS-fault cases remain covered. Validation: tests.test_asm_capture and
tests.test_asm_c_program (13 passing); git diff --check. No gameplay runs.
Descriptor reloads and ENTER remain pending.

### 2026-11-17 — Rebuild bulk C after x87, register-stack and RET changes

Regenerated `local/asm-c-ret-x87-program-20261117/` from the unchanged
pinned dispatch/return inventory: 4,143 syntactically supported bodies,
2,528 dependency-closed generated bodies and 1,752 unsupported entries.
Compared with the segment/stack/string corpus, 2,444 generated sources
changed; 276 bodies lost 4,922 blanket descriptor rejections. The latter
counts are recorded in its `descriptor-guard-delta.json`; they measure
removed guards, not newly verified functions.

The complete 44,202,684-byte aggregate compiled as host C99 with
`-Wall -Wextra -Werror` and no diagnostics. Replayed the existing pinned
`0x604` EFCAP002 capture against this library: full-state comparison
passed with no differences. That captured path executed no x87 operations;
the focused x87 fixtures remain the evidence for those instructions.
The `ef_lift_00000604.c` entry unit also compiled warning-free with
OpenWatcom; this was not a full Watcom corpus rebuild.

Reproduction and hashes are in
`local/asm-c-ret-x87-capture-20261117/{check.py,report.json}`. Translation
report SHA-256: `475a9bfb18cf336f2a2151631c604825cb59484320588bbedc097a50569e95c1`;
compiled library SHA-256:
`b2459267f86b7ef46f483a7bd54f2620603648d0033deecd7a4a040e5c59a036`.
Generated sources, executable data and captures remain ignored and no
manual function was promoted. No gameplay runs. Broader captured paths,
ENTER and descriptor reloads still stand between this evidence and
reliable bulk translation.

### 2026-11-17 — Compile the complete refreshed corpus with Watcom

Closed the target-compiler coverage gap from the preceding bulk rebuild.
Compiled every generated body and every dispatcher/support C unit from
`local/asm-c-ret-x87-program-20261117/` with OpenWatcom: 2,592 objects
(2,528 bodies plus 64 dispatcher/support units), zero failures, zero
diagnostics and no missing objects. Flags were
`-q -bt=dos -3r -za99 -w1 -we`, with the corpus include directory.
The sequential build took 126 seconds. This proves compilation only;
it does not establish a complete target link, byte identity, or behavioral
coverage of every generated function.

Reproduction and per-source/per-object hashes are in
`local/asm-c-ret-x87-watcom-20261117/{check.py,report.json}`.
Report SHA-256:
`cb757e6b673d6a2bd4e67056bd6a2e11cd0984c71276ca3a0e4a118f87806c10`.
It pins the translation report and compiler executable as well as flags.
All generated sources and objects remain ignored. Validation also included
`git diff --check`; no gameplay or new captures.

Existing EFCAP002 capture discovery identified `0xdcb96` as a useful next
coverage target: its current translation is blocked by unsupported FLDL2E.
Other retained snapshots mostly lack descriptor context, target excluded
interrupt helpers, or contain relocated hybrid addresses. Broader captured
execution, ENTER and descriptor reloads remain open.

### 2026-11-17 — Translate the x87 FLDL2E constant load

Added FLDL2E to the explicit-state stack helper and instruction renderer,
with exact D9 EA encoding validation. The extended log2(e) significand
selects its last bit using rounding control; precision control does not
narrow the constant and no precision exception is introduced. Existing
push, stack-overflow, pending-exception, tag and environment handling is
retained. The native original-execution oracle now admits this instruction;
the generated implementation still executes no original instructions.

Expanded the hardware stack comparison to 192,512 raw cases and 3,008
rendered cases, varying all four rounding modes, all three supported
precision settings, TOP, occupancy and masked/unmasked stack faults.
Added both flat and descriptor-backed full-state constant-store paths
against the native oracle. Validation: tests.test_asm_x87_stack,
tests.test_asm_x87_oracle and tests.test_asm_capture (12 passing); a
standalone generated FLDL2E/RET unit compiled warning-free with OpenWatcom;
git diff --check. No gameplay runs.

Reading the complete `0xdcb96` body also exposed F2XM1 and FSCALE after
FLDL2E. Thus that retained EFCAP002 capture is not unlocked by this change,
and no claim of a newly verified original function is made. The preceding
full-corpus compile evidence applies to its pinned pre-FLDL2E sources.

### 2026-11-17 — Translate the x87 FLDLG2 constant load

Added FLDLG2 using the shared extended-constant push path, with D9 EC
encoding validation. Its log10(2) significand follows rounding control
without precision narrowing or a precision exception. The hardware stack
suite now checks 196,608 raw and 3,072 rendered cases, varying rounding,
precision, TOP, occupancy and stack-fault masking. Added flat and
cached-descriptor constant-store comparisons with full x87 environment
checks through the native original-execution oracle.

Validation: tests.test_asm_x87_stack, tests.test_asm_x87_oracle and
tests.test_asm_capture (12 passing); generated FLDLG2/RET unit compiled
warning-free with OpenWatcom; git diff --check. No gameplay runs.
The two original bodies containing FLDLG2 (`0xfa362`, `0x11d3f0`) also
contain unsupported FYL2X; the former additionally needs FLDLN2. Neither
body is claimed newly translatable or verified. Transcendental arithmetic,
ENTER and descriptor reloads remain open bulk-translation boundaries.

### 2026-11-17 — Complete x87 constant-load translation

Added FLDLN2, FLDPI and FLDL2T to the extended-constant push path and
native comparison oracle. All seven x87 constant loads now have exact
encoding checks. The three new significands follow RC without narrowing
by PC or raising precision exceptions; FLDL2T specifically rounds upward
only for round-toward-positive, while its nearest value equals its lower
neighbor. Stack faults and environment updates use the existing machinery.

Validation: tests.test_asm_x87_stack, tests.test_asm_x87_oracle and
tests.test_asm_capture (12 passing), covering 208,896 raw hardware stack
comparisons and 3,264 rendered comparisons with varying RC/PC, TOP,
occupancy and exception masks, plus full-state flat and descriptor-backed
paths. A generated unit containing all three instructions compiled
warning-free with OpenWatcom; git diff --check. No gameplay runs.
This completes the constant-load instruction family, not transcendental
arithmetic or whole-body coverage: FYL2X, F2XM1, FSCALE and other remaining
instructions still prevent relevant original bodies from translating.
ENTER and descriptor reloads remain open as well.

### 2026-11-17 — Audit ENTER allocation checks before enabling descriptors

The pinned original inventory contains 47 ENTER instructions, all with
nesting zero (allocation sizes include 0, 4, 8, 16, 40, 324 and 2,132).
Investigated the remaining descriptor guard rather than removing it on
successful-path evidence alone. Intel's instruction reference specifies
#SS when the resulting ESP lies outside SS, and a page fault when a write
using the final stack pointer would fault:
<https://cdrdv2-public.intel.com/671200/325462-sdm-vol-1-2abcd-3abcd.pdf>
(ENTER instruction reference).

A restored ring-3 EFCAP002 synthetic fixture exposes an oracle limitation:
Unicorn 2.1.4 executes `enter 64,0` successfully with initial ESP 0x18000
and expand-down SS limit 0x17ff0. It sets ESP to 0x17fbc, below that limit,
and writes the saved EBP at 0x17ffc. It also accepts final ESP equal to
the limit (0x17fbc). These outcomes cannot validate architectural ENTER
allocation checks. The existing generated-path limit rejection tests
must not be described as comparisons against original fault behavior.

Reproduction and the five boundary-case observations are retained in
`local/asm-c-enter-20261117/{probe.py,probe.json}`; they use synthetic
instructions and the existing fixture, without gameplay or original data.
No translator guard was relaxed. Next evidence needed is protected-mode
native ENTER fault capture, including whether the saved-EBP write survives
a final-allocation fault, followed by descriptor and RAM validation that
preserves that ordering. Successful synthetic ENTER comparisons alone are
insufficient. This audit changes the validation path, not translation
coverage; reliable bulk translation remains unfinished.

### 2026-11-17 — Translate descriptor-backed ENTER with native fault ordering

Added an isolated Linux x86-64 compatibility-mode probe using a private
LDT expand-down stack and alternate signal stack. Native ENTER allocation
faults deliver #SS at ENTER with ESP/EBP unchanged, but retain the completed
saved-EBP memory write. Boundary allocations 0, 1, 4, 8, 64 and 2,132 are
compared with generated C after normalizing stack addresses. The probe
also confirms execution under the compatibility-mode code selector.
This supplies the fault-ordering evidence missing from Unicorn.

The zero-nesting 32-bit ENTER renderer now writes saved EBP through SS,
checks the final pointer against SS and modeled RAM, and only then commits
EBP/ESP. Removed its blanket descriptor rejection and added exact encoding
checks. Rejection of the initial push leaves RAM unchanged; rejection of
the final allocation retains the write while preserving registers/PC.
Nonzero effective nesting and other encodings remain unsupported.

Validation: tests.test_asm_enter, tests.test_asm_capture and
 tests.test_asm_c_program initially passed together (14 tests). After adding
successful ENTER/LEAVE capture paths and final-RAM-fault coverage, capture
and synthetic full-register/flags/RAM comparisons passed; the new RAM case
initially used invalid selectors for the flat profile, was corrected, and
the native ENTER suite passed again. A generated ENTER/LEAVE/RET unit
compiled warning-free with OpenWatcom; git diff --check. No gameplay runs.
The native fixture and comparison are committed as tests/native_enter.c
and tests/test_asm_enter.py. Bulk regeneration and broader execution remain
pending, as do descriptor reloads and unsupported x87 arithmetic.

### 2026-11-17 — Translate BSF/BSR and check the original integer conversion

Added 16/32-bit BSF and BSR with register or segmented-memory sources.
Reads complete before any flag/destination effects; zero sources leave
the destination unchanged. ZF is architectural; other undefined flags
follow the existing Unicorn policy (SZP from the source, CF/OF/AF clear).
This is an explicit oracle convention, not a claim that all hardware
implements identical undefined flags.

Validation: tests.test_asm_capture and the synthetic full-register,
flags/all-RAM comparison (5 tests passing), including zero input, partial
register destinations, aliases and memory operands. Independently
revalidated and translated original body `0xeff7f` (uint_to_extended),
then compared nine controlled boundary inputs against original execution
with ring-3 descriptor state: all passed with no differences. These are
synthetic initial states around the original body, not gameplay captures.
The first probe setup omitted table bytes from baseline RAM; aligning
baseline RAM with the restored descriptor tables resolved its table-only
mismatch without excluding any memory from comparison.

Reproduction and results remain under
`local/asm-c-bit-scan-20261117/{check.py,report.json}`. The generated
original-body unit compiled warning-free with OpenWatcom; git diff --check.
No gameplay runs and no manual tracker promotion. This removes the known
BSR instruction blocker; whole-corpus regeneration, descriptor reloads and
unsupported x87 arithmetic remain pending.

### 2026-11-17 — Refresh bulk translation after constants, ENTER and bit scans

Regenerated the independently decoded, pinned original-body inventory with
the current translator. Syntactically supported bodies increased from
4,143 to 4,144; direct-dependency-closed generated bodies increased from
2,528 to 2,532, covering 411,382 original body bytes. Newly generated
bodies are `0xeff6e`, `0xeff7f`, `0xf07cd` and `0xf0810` (602 bytes).
Forty-five generated body sources changed. These counts describe mechanical
translation, not verified equivalence or byte-identical recompilation.

Sources and the translation report remain ignored under
`local/asm-c-enter-bitscan-program-20261117/`; report SHA-256:
`6eac0ed14aa5bd3d59693d82e98ab17e6490ce815a490e0a0216eb0140acb794`.
The refreshed report identifies F2XM1 at `0xdcb96` and FYL2X at
`0xfa362` / `0x11d3f0` as remaining instruction blockers; adding their
preceding constants has not made these whole bodies translatable.

Compiled all 2,532 generated bodies and 65 dispatcher/support C units
with OpenWatcom (`-q -bt=dos -3r -za99 -w1 -we` plus the corpus include
directory): 2,597 objects, zero failures, zero diagnostics and no missing
objects. Per-source/object hashes, compiler hash, flags and the pinned
translation-report hash are retained in
`local/asm-c-enter-bitscan-watcom-20261117/{check.py,report.json}`.
Compilation report SHA-256:
`377a033e15311443ca80e89f8611bcbc509a82710244533e9e0140d238e21132`.
This establishes complete target compilation for this corpus, not a full
target link or behavioral validation of all bodies. No gameplay runs,
new captures or manual tracker promotions; git diff --check passed.
Descriptor reloads and unsupported x87 arithmetic remain open, so reliable
bulk translation is not complete.

### 2026-11-17 — Translate segment PUSH through the loaded stack descriptor

Removed the descriptor-profile blanket rejection for canonical segment
PUSH. Both 16-bit and 32-bit operand forms now store through SS, validate
the actual stack extent and commit ESP only after the store succeeds.
Source selectors, including null selectors, are values rather than memory
accesses or descriptor reloads. Exact opcode/selector/operand-size checks
reject mismatched spellings and unimplemented prefixes. The 32-bit form
retains the existing Unicorn policy of a zero-extended four-byte store;
this is not a claim that all x86 implementations store the upper half.

Validation: tests.test_asm_capture passed (4 tests), with twelve new
full-state cases covering all six segment registers at both operand sizes,
nonzero initial stack bytes and expand-down SS rejection before any RAM,
register or flag changes. The synthetic all-register/flags/RAM comparison
and tests.test_asm_c_program passed in the subsequent 11-test run; only the
new encoding rejection test initially failed because its fixture omitted
the fallthrough address. Correcting that fixture made the focused test
pass. A mixed 16/32-bit generated unit compiled warning-free with Watcom;
git diff --check passed. No gameplay runs.

An ignored native compatibility-mode probe also observed a four-byte,
zero-extended PUSH DS store on this host; source and result remain under
`local/asm-c-segment-push-20261117/{probe.c,probe.json}`. An inventory audit
there records 154 canonical segment-PUSH sites in 78 bodies (all 32-bit),
without promoting their tracker states or claiming whole-body execution.
Segment POP and far-pointer loads still reject descriptor reloads. Bulk
regeneration after this change and the remaining x87 operations are still
needed for reliable bulk translation.

### 2026-11-17 — Check REP comparison flags after a later iteration faults

Investigated the remaining REP flag-rollback question before changing
translation semantics. An isolated native Linux x86-64 AMD compatibility-mode probe executes
REPE CMPSB / SCASB with four requested comparisons and a destination whose
third byte lies on a protected page. Both comparisons complete two
iterations, then fault with count 2, destination advanced by 2 and source
advanced by 2 only for CMPS. Starting flags are 0x202; fault flags are
0x10246, retaining the last successful comparison's arithmetic flags.
RF is supplied by native exception delivery, outside the explicit model.
This host does not supply evidence for rolling arithmetic flags back to
the pre-REP value. No translator behavior was changed on that assumption.

Committed tests/native_rep.c and tests/test_asm_rep_fault.py reproduce this
native observation and compare it against generated C's modeled RAM-extent
fault. The generated execution is split across a budget boundary and
checked for all GPRs, flags (excluding native exception-delivery RF),
restart PC, count and unchanged full RAM. Validation:
`python3 -m unittest tests.test_asm_rep_fault -q` (1 test passed, both
instructions). The initial 32-bit fixture faulted on null DS/ES; explicitly
loading the Linux user data selector corrected the fixture, and the final
probe confirms CS=0x23. git diff --check. Native results remain ignored at
`local/asm-c-rep-flags-20261117/report.json`.

Scope is deliberately limited: the native probe uses 32-bit compatibility
mode on AMD; the generated paths use the existing flat 32-bit state. It does
not establish Intel fault policy, protected-mode descriptor-fault ordering,
or all REP fault cases. The test skips other CPU vendors rather than
claiming their flags are identical. This adds fault-state evidence, not
translation counts or tracker promotions. Descriptor reloads, remaining
x87 instructions and broader target execution still leave reliable bulk
translation incomplete. No gameplay or UI runs.

### 2026-11-17 — Translate AAM and compare the original decimal formatter

Added canonical AAM imm8 translation with exact opcode/immediate checks.
AL is divided by the encoded base, AH receives the quotient and AL the
remainder, preserving upper EAX. SZP come from the resulting AL; undefined
CF/OF/AF follow the existing Unicorn zero policy rather than a universal
hardware claim. A zero base reports divide fault 5 before changing GPRs,
flags or PC. Prefixes and mismatched encoded/spelled bases are rejected.

Validation: the focused scalar grid passed 14,339 cases (every AL value
for bases 1, 2, 3, 10, 16, 24 and 255, eight CF/AF/OF input combinations,
plus three zero-base faults). The encoding-rejection test and
tests.test_asm_c_program passed together (10 tests). git diff --check.

Read the original disassembly and independently revalidated the pinned
106-byte body `0x10e2f5` (u64_to_digits) through read_inventory. Its two
base-10 AAM sites are now translated. Full GPR/flags/RAM comparisons against
the original body passed for 13 integer boundaries, from zero through
UINT64_MAX, each with 1, 4, 8, 16 and 20 requested digits (65 cases).
These use controlled ring-3 initial states, not gameplay captures. The
generated original-body unit compiled warning-free with OpenWatcom.
Reproduction, generated sources and results remain ignored under
`local/asm-c-aam-20261117/{check.py,report.json}`; report SHA-256:
`19f621902a073bac6062516c7081853fff28c401a987ee61fafaede6a91e62f6`.

The apparent DAA and base-24 AAM sites in inventory body `0x10e35f` are
inside an inline numeric table following CALL, not evidence that those
instructions execute. DAA was left unsupported; the table/body boundary
still needs a separate flow audit. No manual tracker promotion or gameplay
runs. Whole-corpus regeneration after this and segment PUSH remains
pending, as do descriptor reloads and unsupported x87 arithmetic; reliable
bulk translation remains incomplete.

### 2026-11-17 — Repair decimal table boundary and retract the empty epilogue stub

Ran audit-body-holes for `0x10e35f` and `0x10e414`; it reported zero
holes, which did not validate the inline bytes as executable code. Reading
the original caller `0x10e250` establishes that EDI supplies CS-relative
table reads after CALL `0x10e35f`. The accessor itself executes exactly
CALL `0x10e414`, POP EDI, RET: the nested CALL's continuation `0x10e364`
is the table pointer, not an instruction continuation. The RET consumes
the outer return address. The previous inventory decoded table values as
DAA, AAM, port I/O and false calls, contaminating flow/signature evidence.

Added `tools/ghidra/EF2000RecoverDecimalTable.java`, gated by 32-bit language,
pinned full-image and region SHA-256, original instruction layout and
reviewed ownership/body extents. On a disposable project copy it sets the
accessor body to its five-byte CALL, marks that CALL as CALL_RETURN, and
replaces the 176 inline bytes `0x10e364..0x10e413` with data while retaining
the helper at `0x10e414`. Apply succeeded (167 to 5 code bytes); a subsequent
read-only report-mode run confirmed the same five-byte body and table.
The original project/payload and existing pinned inventories are unchanged;
future bulk inventory regeneration must use the repaired project.

Corrected functions.tsv size for `0x10e35f`. Retracted the `compiles` /
confirmed claim for the empty `pop_edi_ret_10e414` draft: it is now
`compiles-bad` / unverified, and its source and signature evidence explain
the observable EDI and nonstandard stack effects. It is not a normal void
callee and is excluded by hybrid-link eligibility. No replacement C body
is claimed; its code and all-RAM stack-write effects still need recovery.

Validation: original accessor execution under controlled ring-3 states
with flags 0x202, 0x246 and 0xad7 always traced only `0x10e35f`,
`0x10e414`, `0x10e415`, returning to the outer sentinel with EDI=0x10e364.
Checked all registers/flags and full RAM, including the retained nested
CALL write below entry ESP. Reproduction/results are ignored under
`local/ghidra-inline-table-20261117/{check.py,execution.json,recovery.tsv,recheck.tsv}`;
initial body-hole audit is `local/inline-table-body-holes-20261117.tsv`.
`python3 tools/decomp_status.py check` and git diff --check pass. No
gameplay, UI runs or verified-function promotions.

Recipe invocation, against a disposable copy with the pinned payload:

```sh
analyzeHeadless WORKSPACE/projects EF2000 -scriptPath tools/ghidra \
  -process payload.bin -noanalysis -postScript EF2000RecoverDecimalTable.java \
  /absolute/path/recovery.tsv apply
```

Use `report` with `-readOnly` to roll back the proposed metadata changes.
This repairs a real inventory boundary; descriptor reloads, unsupported
x87 arithmetic and broader execution still leave reliable bulk translation
incomplete.

### 2026-11-17: translate CALL continuations consumed as data

The shared-PC composer now permits an absent CALL continuation only when its
translated direct target is exactly canonical `POP r32; RET`, excluding ESP.
The proof checks decoded operands, bytes, adjacency and instruction ownership;
normal direct dependency closure still requires the helper to translate.
The generated code retains the nested CALL write, POP and RET rather than
folding the accessor to a constant and losing observable stack RAM.

`tests.test_asm_c_program` covers the inline-data continuation, with eight
full-state original/C comparisons from its caller, and rejects POP ESP,
NOP/RET and RET-immediate variants. The helper requires a nested call frame,
so ordinary isolated helper probes are not evidence for that ABI.
`local/asm-c-inline-accessor-20261117/check.py` independently decodes the
repaired original 0x10e35f (5 bytes) and 0x10e414 (2 bytes), verifies three
ring-3 captured states against the original with full GPR/flags/RAM comparison,
and compiles all four generated body/dispatcher units with OpenWatcom without
diagnostics. Generated sources and original bytes remain ignored local data.
No manual tracker promotion, gameplay run or whole-corpus correctness claim;
descriptor reloads and unsupported x87 operations remain open.

### 2026-11-17: refresh bulk C after inline-table recovery

A fresh Ghidra inventory from `local/ghidra-inline-table-20261117` confirms
0x10e35f owns only its five-byte CALL, with no inline table instructions.
`local/ghidra-inline-inventory-20261117/normalize.py` exports the same 4,280
selected entries as the prior bulk snapshot using current tracker metadata
and the newly exported body ranges. Its normalized inventory manifest SHA-256
is `7da71a66a886ef80b90371121cdca019d39579274110912a97f5bbe374c08627`.

Regenerating with `tools/asm_c_program.py` produces 4,146 syntactically
supported bodies and 2,537 bodies closed over direct dependencies, covering
412,102 original body bytes. There are 1,743 unsupported entries. Compared
with the ENTER/bit-scan snapshot, five bodies (720 bytes) enter closure:
0x0ebea3 (65), 0x10e250 (58), 0x10e2f5 (106), 0x10e35f (5), and
0x120560 (486). None leave closure; 34 previously generated body sources
change. The translation report SHA-256 is
`70925408f8fc58276ea2439de6306772e46357379d33b60179cf17b3c71ffdbe`.

`local/asm-c-decimal-count-20261117/check.py` compiles the exact generated
0x10e250/accessor/helper units with a shared dispatcher, then compares 183
ring-3 prepared inputs against original execution. Inputs include zero,
powers of ten and their neighbors through 10^19, and UINT64_MAX-1, with
initial estimates 0, 9 and 18. All full GPR/flags/2-MiB RAM comparisons pass;
the report SHA-256 is
`48df7b0083ea24695d4a6d253dedec4831c4bf87d12a9723863c17f9b5d21127`.
UINT64_MAX itself is not covered by this evidence. These checks exercise
the real nested CALL stack effects in a caller; no gameplay runs are used.
Generated sources remain local, unverified drafts: closure and compilation
do not establish all-input behavior, dynamic-target coverage or game integration.

The completed `local/asm-c-inline-watcom-20261117/check.py` run compiles all
2,602 units (2,537 bodies plus 65 dispatcher/support units) with the pinned
OpenWatcom compiler, `-bt=dos -3r -za99 -w1 -we`: zero failures, zero
diagnostics and zero missing objects in 112.20 seconds. Its report binds
compiler, flags, each source/object and the translation report; SHA-256
`57f1d61d0846c884563f201e1ce5439ee896eaa7a34cffb845e380bfcf4daaa6`.
This replaces the interrupted compile from the paused turn. It is object
compilation evidence, not a full target link or a corpus-wide equivalence proof.
`python3 tools/decomp_status.py check` and `git diff --check` also pass.

### 2026-11-17: preserve DS on legacy indirect jumps

The shared-PC composer now translates objdump's `notrack jmp` alias as a
legacy 32-bit DS override. It accepts only leading `3E FF /4` memory jumps
with a DWORD memory operand, rejects ambiguous explicit segment spellings,
and forces DS even for EBP/ESP addressing. It retains the original bytes
for dispatcher checks and instruction length; control-flow analysis and
dynamic-transfer metadata use the normalized jump. CET enforcement is not
part of this pre-CET execution model. This does not enable other prefix
aliases, far transfers or register NOTRACK forms.

The regression test compares eight prepared executions and a ring-3 capture
with distinct DS/SS LDT entries: DS permits [0x400], while expand-down SS
rejects it. Five malformed/unsupported encodings remain rejected.
`local/asm-c-legacy-jump-20261117/check.py` selects an original jump site from
each of the three previously blocked bodies (0xee22c, 0xee589, 0xeed92),
checks two flags states each against original execution, and compiles five
body/dispatcher units with OpenWatcom without diagnostics. All six comparisons
pass full GPR/flags/RAM checks. These are individual jump-site checks, not
whole-body or dynamic-target coverage proof; no tracker promotion is made.

### 2026-11-17: translate observed data16 aliases

The scalar renderer accepts checked `data16` aliases for `66 8C /r` and
`66 8E /r` segment-register MOVs with WORD memory operands, plus the observed
`66 F3 A4` REP MOVSB form. It validates opcode, ModRM segment selection,
operand direction and width rather than discarding arbitrary operand-size
prefixes. MOVSB remains byte-sized with 32-bit ECX/ESI/EDI; segment stores
remain 16-bit writes. Descriptor-backed segment reloads retain their existing
rejection: this change does not supply current-table lookup/cache updates.

Validation: `tests.test_asm_c_program` and
`tests.test_asm_to_c.TranslationTests.test_data16_aliases_reject_unsupported_encodings`
pass (14 tests). The new cases compare eight prepared states plus a ring-3
captured state for each of ES/DS WORD stores and prefixed REP MOVSB, checking
full registers, flags and RAM. Eight unsupported/mismatched alias spellings
are rejected. `local/asm-c-data16-20261117/check.py` translates and compiles
six actual prefixed instruction sites from 0x109e6f, 0x110fd4, 0x120ad0,
0x120bc0 and 0x120c06 with OpenWatcom without diagnostics. These isolated
site objects are compilation evidence, not original whole-body equivalence
or a new bulk coverage count. No tracker states are promoted.

### 2026-11-17: translate ignored CS/DS prefixes on register MOVs

The renderer now accepts the observed CS/DS-prefixed register-only MOV
aliases: B8..BF imm32 and 89/8B with register ModRM. It checks prefix byte,
register selection, operand direction/width and the encoded immediate before
rendering. These forms have no data-memory access for a segment override to
select; original instruction length and byte validation remain intact.
Memory MOVs and other prefixed opcodes are not admitted by this rule. Their
segment semantics must be handled independently.

`tests.test_asm_c_program` and
`tests.test_asm_to_c.TranslationTests.test_register_segment_prefixes_require_matching_bytes`
pass (14 tests). Three new forms each pass eight prepared full-state probes
and a ring-3 capture (27 comparisons); seven mismatched or unsupported
prefix spellings are rejected. The existing data16/call/indirect-transfer
regressions also pass. `local/asm-c-register-prefix-20261117/check.py`
compiles all 12 observed CS/DS alias sites, from six original bodies, with
OpenWatcom without diagnostics. This is site-level compilation evidence;
no whole-body equivalence, updated bulk count or tracker promotion is claimed.
`python3 tools/decomp_status.py check` and `git diff --check` pass.

### 2026-11-17: translate 16-bit effective addresses

The operand parser now supports the eight encodable BX/BP/SI/DI register
combinations, wrapping the effective offset to 16 bits after adding registers
and displacement. GPR high halves do not participate or change. BP-based
forms default to SS; other forms default to DS, and explicit overrides remain
selected. Mixed-width, scaled 16-bit and illegal register combinations are
rejected. This adds scalar addressing support, not 16-bit REP count/index
semantics or descriptor reload support.

`tests.test_asm_c_program` and
`tests.test_asm_to_c.TranslationTests.test_address16_rejects_invalid_combinations_and_selects_ss_for_bp`
pass (15 tests). Ten new load/store/LEA cases include all eight combinations
and FS overrides, large register high halves and offset wrap, with four
prepared comparisons plus a ring-3 capture each (50 full GPR/flags/RAM
comparisons). Nine invalid address forms remain rejected, and default SS/DS
selection is checked explicitly. `local/asm-c-address16-20261117/check.py`
compiles all 43 observed scalar 16-bit-address sites from six original bodies
(0xd3900, 0xd3e50, 0xd4250, 0xd4370, 0xd53ff, 0xd5a90) with OpenWatcom
without diagnostics. Original FS-based MOV/MOVZX/OR accesses now render;
whole-body behavior and dynamic dependencies remain separate evidence gates.
No tracker promotion or new bulk coverage count is claimed.

### 2026-11-17: make CMPS operand fault order explicit

A native 32-bit compatibility-mode probe on this AMD CPU establishes that
REP CMPSB accesses the ES destination before the source, including each
operand's segment and page checks. With both operands on protected pages,
CR2 selects the destination; null ES beats a source page fault; a destination
page fault beats null DS. These are three distinct first-iteration cases,
with count, SI/DI and comparison flags unchanged (RF is exception delivery).
An initial hypothesis that both segments should be prevalidated was rejected
by the destination-page/null-DS observation before committing production code.

The renderer now performs the destination read and checks its fault before
the source read. Previously it read source then destination before checking
fault, which often produced the same modeled error by overwriting the source
fault. The new ordering removes that reliance and avoids an extra access
after the first architectural fault. No generic sticky-fault helper policy
or MOVS ordering change is inferred from this CMPS observation; Intel ordering
remains unverified.

`tests.test_asm_rep_fault`, `tests.test_asm_capture` and
`tests.test_asm_c_program` pass (20 tests), including native competing-fault
observations and translated full-state fault checks, existing later-iteration
REP flag checks and ordinary string/captured regressions.
`local/asm-c-cmps-priority-20261117/check.py` records the three native cases
and compiles six generated byte/word/dword, repeated/nonrepeated CMPS units
with OpenWatcom without diagnostics. Native competing-fault evidence is
specifically for REP CMPSB; six-unit compilation is not a native fault-order
proof for every form. No new body coverage or tracker promotion is claimed.

### 2026-11-17: refresh bulk coverage after prefix and address support

Regenerated the same pinned 4,280-body inventory with the current renderer
and repaired inline decimal table workspace. The report in
`local/asm-c-refresh-program-20261117/report.json` has SHA-256
`5bc7de990a91d35a62b0ef9e3de6a90cbdf790ec333a99c165f78bd7b0d13e1c`.
Syntactically supported bodies increase from 4,146 to 4,159; closed generated
bodies increase from 2,537 to 2,567, covering 420,008 original body bytes
(previously 412,102). All 30 additions are retained, with no removed body;
one previously generated body changes source. The additions include the
FS-based 16-bit scalar address cluster and newly closed callers, checked
register/operand-size prefix sites and legacy DS indirect jumps. These are
mechanical coverage counts, not recovered native-ABI functions or byte-exact
recompilation results.

The manual tracker remains 810 blocked rows / 457,371 bytes. Of these,
301 rows / 153,742 tracker bytes now have closed generated C. This overlap
identifies candidates for behavior evidence; compilation alone does not
justify promoting their tracker state. The 1,713 remaining generator
failures include 1,592 propagated direct dependencies, 101 unsupported
instruction bodies, 16 fallthrough gaps, two unresolved/external branches,
one unsupported data16 spelling and one unresolved count branch/fallthrough.
Direct instruction blockers include INT (59 bodies), IN (12), OUT (6),
PUSHF (5), CLI (4), LSL (2), STI (1), and x87 transcendental/state operations.
Counts classify the first recorded failure per body, not every instruction.
The remaining data16 spelling is the original combined 66/2E-prefixed
segment load at 0x222; its CS memory override must be preserved when adding
support. Descriptor reloads in the protected segment profile remain a
separate implementation requirement.

`local/asm-c-refresh-watcom-20261117/check.py` compiles all 2,633 translation
units (2,567 bodies plus 66 dispatcher/support units) with OpenWatcom
`-bt=dos -3r -za99 -w1 -we`, with zero failures, diagnostics or missing
objects. Its report records compiler, source and object hashes; report
SHA-256 is `77d64ccb8d1db989fa0638f1b192114ed499c1e3af2e2c182d2d8e1e9f0a849c`.
No full target link, new whole-body equivalence result, gameplay run or
tracker promotion is claimed by this refresh.

### 2026-11-17: retain CS on combined-prefix segment MOV

The original nine-byte helper at 0x222 starts with `66 2E 8E /r`: a
word-sized DS load from CS-relative memory. The renderer now recognizes the
combined operand-size/CS prefix for memory segment MOV, validates the segment
register and direction as before, and requires the explicit CS operand
spelling. It keeps both original prefix bytes for fetch/length validation and
uses CS for the memory access. Missing/contradictory overrides and loading CS
remain rejected. No other combined prefix forms are inferred.

`tests.test_asm_c_program` plus the two focused data16 translation tests pass
(17 tests). The added load fixture passes eight prepared full GPR/flags/RAM
comparisons against original instruction execution in the flat profile;
rendering assertions separately check CS selection and four-byte instruction
length, and three additional malformed forms are rejected.
`local/asm-c-cs-segment-20261117/report.json` records warning-free OpenWatcom
compilation of the original complete 0x222 helper. This does not implement
protected-profile descriptor reloads, prove the original helper across all
selector values, promote a tracker row or refresh whole-corpus counts.

### 2026-11-17: recover the missing loop in body 0xbfcc

The body-hole audit identifies 0xbfff..0xc01a as 28 unowned bytes within
0xbfcc. The back edge at 0xc026 targets 0xbfff, but Ghidra had decoded the
first instruction from 0xc000 after a misleading data reference. Original
bytes decode from 0xbfff into ten instructions ending exactly at 0xc01b;
two conditional branches skip to the final INC. This is executable loop
code, not inline data.

`tools/ghidra/EF2000RecoverBfccLoop.java` pins the language, payload and gap
hashes, accepts only the reviewed old or repaired body, rejects other owners,
and checks all ten instruction lengths and mnemonics after redisassembly.
It expands the body from 1,449 to 1,477 bytes. The tracker size and boundary
note are updated; its blocked/unverified state remains unchanged.
The disposable `local/ghidra-bfcc-loop-20261117` project records successful
apply and repeat report runs. Its fresh inventory exports one contiguous
body with matching instruction layout.

`local/ghidra-bfcc-loop-20261117/check.py` compares the recovered block against
original execution for both branch skips and the memory-writing path, each
with two initial flag values: six full GPR/flags/PC/selector/2 MiB RAM
comparisons pass. The translated block also compiles with OpenWatcom
`-bt=dos -3r -za99 -w1 -we` without diagnostics. These are block proofs, not
whole-function verification. The repaired body renders syntactically, while
untranslated direct dependencies still prevent closed generation. No gameplay
run or tracker promotion is claimed. Future inventory refreshes should use
the repaired disposable project rather than the preceding decimal-only copy.

### 2026-11-17: recover kotsAccept's alternate branches

The branch at 0x52a0 reaches 0x5327 beyond kotsAccept's previously recorded
body. The flow-gap/body-hole audits do not report this case because the
conditional target is outside the body rather than a fallthrough or internal
hole. Inspection of the original bytes and ownership inventory establishes
two alternate paths ending at RETs 0x53d3 and 0x5484, with no other function
owning 0x5327..0x5484. The following alignment instruction is excluded.

`tools/ghidra/EF2000RecoverKotsAccept.java` checks the language, pinned payload,
350-byte region hash, old/repaired body and incoming conditional branch. It
rejects other owners, redisassembles the region, checks complete instruction
coverage (104 instructions) and both terminal RETs, then expands the body
from 251 to 601 bytes. Tracker size and boundary notes are corrected without
promoting its blocked/unverified state. Apply and repeat report runs in
`local/ghidra-kots-branches-20261117` succeed; a fresh inventory exports a
contiguous body whose instruction spans match the original decoder. The
repaired function renders syntactically; untranslated callees still prevent
closed generation. Use this workspace for subsequent inventory refreshes;
it includes the earlier decimal-table and 0xbfcc repairs.

`local/ghidra-kots-branches-20261117/check.py` compares paths starting at
0x533b and 0x53d4 through their status-byte stores, stopping before stack
cleanup. Counts 0, 1 and 3, EBX/ECX zero and two initial flag values produce
twelve full GPR/flags/PC/selector/2 MiB RAM matches. The generated region
compiles with OpenWatcom without diagnostics. This evidence excludes the
initial string comparison, nonzero-object helper calls and whole-function
behavior; it is not a tracker verification or gameplay claim.

### 2026-11-17: translate protected-mode PUSHF in both operand widths

Tracing kotsAccept's remaining callees reaches INT services through
0xa5ef8/0xa5b70 and port I/O through 0xab954/0xde580; these are genuine
service dependencies rather than additional gaps in those two small callees.
No no-op or successful-return approximation is introduced for them.

The instruction renderer now accepts checked `9C` PUSHF and `66 9C` PUSHFW.
It saves the low word or the doubleword image with RF/VM cleared, using the
existing SS-checked stack-store helper, and commits ESP only after successful
writes. The saved-image rule follows Intel SDM volume 2B, PUSHF/PUSHFD
(https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2b-manual.pdf).
Virtual-8086 execution is explicitly unsupported (result 7), since the state
model has no CR4.VME/virtual interrupt substitution semantics. POPF remains
unsupported; its privilege and writable-flag rules are not inferred here.

`tests.test_asm_c_program` and the focused PUSHF encoding, saved-image/fault
and unsupported-input tests pass (19 tests). Both widths have 16 prepared
full-state comparisons plus three ring-3 captured comparisons each (38 total),
including ID and arithmetic/direction flag combinations. Eight direct modeled
checks cover saved RF masking, successful writes, partial/out-of-range stack
writes with unchanged ESP/PC, and rejection of virtual-8086 state. These direct
checks supplement the original comparisons; they are not native RF or
virtual-8086 execution proofs. Four mismatched encoding/spelling cases remain
rejected. `local/asm-c-pushf-20261117/check.py` compiles all eight observed
PUSHF sites from five original bodies with OpenWatcom without diagnostics.
No whole-body verification, tracker promotion or new bulk coverage count is
claimed; subsequent POPF and service operations still need implementation.

### 2026-11-17: translate protected-mode POPF with privilege masks

The renderer accepts exact 9D/66 9D POPF encodings. It reads through SS,
checks the read before any flag or ESP update, and applies the non-reserved
writable-bit mask for the operand width. CPL comes from the existing CS
selector; only CPL 0 can update IOPL, and IF changes only when current CPL
is at most current IOPL. The stack image cannot grant itself privilege.
VM/VIF/VIP and reserved bits remain unchanged, RF clears on success, and
16-bit POPF preserves the upper flag bits apart from RF. Virtual-8086
execution remains explicitly unsupported. These rules follow Intel SDM
volume 2B, POPF/POPFD, operation/table 4-15:
https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2b-manual.pdf

`tests.test_asm_c_program`, the focused POPF encoding and fault/RF tests,
and the unsupported-input test pass (20 tests). Six stack images at both
widths cover requested IOPL/IF, arithmetic/direction/NT/AC/ID, reserved,
RF and virtual-interrupt bits. Four prepared CPL-0 comparisons plus two
ring-3 captures (initial IOPL 0 and 3) per image give 72 full-state
comparisons. TF-setting execution is deliberately absent from this matrix;
debug exception delivery is not established by these comparisons. Six direct
checks additionally cover out-of-range reads with unchanged state, explicit
virtual-8086 rejection, and successful RF clearing. Four encoding/spelling
mismatches remain rejected.

`local/asm-c-popf-20261117/check.py` compiles all five original POPF sites
with OpenWatcom without diagnostics. Re-analysis of the five preceding
PUSHF-blocked bodies now reaches CPUID at 0xf2819 and CLI at 0x110f0c,
0x110fd4, 0x1110c5 and 0x1111e1. No whole-body verification, tracker
promotion or new closed-corpus count is claimed.

### 2026-11-17: refresh bulk C after stack flags and boundary repairs

Re-exported the same 4,280 selected functions from the repaired
`local/ghidra-kots-branches-20261117` workspace using its fresh Ghidra body
ranges. The byte-checked normalized inventory is
`local/asm-refreshed-body-inventory-20261117`; generation output is
`local/asm-c-refreshed-program-20261117`. The translation report SHA-256 is
`97b68ea7f70e89c147a039e386e915fa5663d56d1378d1291464ed22de625fff`.

There are now 4,162 syntactically supported bodies and 2,572 bodies whose
direct dependencies are also translatable, covering 423,515 original body
bytes. Compared with the preceding bulk snapshot, five bodies (3,507 bytes)
are added and none removed: 0x222 (9), 0xee22c (3,350), 0xee3ce (82),
0xee420 (28), and 0xef41f (38). The checked combined CS/data16 segment MOV
in 0x222 unlocks the other four through direct dependencies. Existing closed
bodies have unchanged generated source hashes. The repaired 0xbfcc and
0x522c bodies no longer fail on unresolved external branches, but still need
untranslated callees; PUSHF/POPF bodies now expose CLI/CPUID blockers.

The 1,708 remaining unsupported bodies have first-failure classifications:
1,590 untranslated direct dependencies, 101 unsupported instructions,
16 fallthroughs outside the body, and one overlapping count-branch path.
Instruction failures are INT (59), IN (12), CLI (8), OUT (6), FPATAN (3),
FYL2X and LSL (2 each), and one each of FCOS, FSIN, FPREM, FPTAN, F2XM1,
CPUID, FSINCOS, FNSAVE and STI. These are first failures, not an exhaustive
instruction census. Interrupt and I/O service semantics must be recovered;
no successful-return or no-op substitute is counted as translation.

The manual tracker still has 810 blocked rows (457,749 bytes), including
303 rows (157,174 tracker bytes) in this generated but unverified set.
Historical triage notes do not represent current renderer capability.
No tracker state is promoted: direct-edge closure does not establish dynamic
target coverage, whole-program linkage, or behavioral equivalence.

OpenWatcom evidence is `local/asm-c-refreshed-watcom-20261117/report.json`
(SHA-256 `9cdb3a45badec080dfa8d334ba7d1bf446c89b5889c2cdc8eb53df4e50560e01`).
All 2,638 units have successful, diagnostic-free compilation evidence:
71 newly compiled units (five bodies plus 66 dispatcher/support units), and
2,567 unchanged body results reused from the preceding complete build.
The local check verifies the compiler, source, object and shared state-header
hashes before reuse; ordinary body sources include only that unchanged
state header. Registry consumers are rebuilt. Compiler flags remain
`-q -bt=dos -3r -za99 -w1 -we`. An initial redundant rebuild was interrupted;
its partial output is not used as proof. This refresh required no gameplay
runs. `python3 tools/decomp_status.py check` and `git diff --check` pass.

### 2026-11-17: recover the missing drawing tail in 0x4c120

The flow-gap audit identifies fallthrough from MOV at 0x4c2c3 to 0x4c2c5,
where a false data reference had started an orphan decode at 0x4c2c9.
Reading the original disassembly from the actual fallthrough recovers
91 bytes through 0x4c31f: 28 instructions including calls at 0x4c2c5,
0x4c307 and 0x4c31b. They rejoin the existing node-loop tail at 0x4c320.
The earlier Ghidra pseudo-C also describes the drawing path but is not used
as executable proof. No other function owns the recovered bytes.

`tools/ghidra/EF2000Recover4c120Tail.java` pins the x86 language, complete
payload hash, missing-region hash, exact old/repaired bodies, incoming
fallthrough, and all recovered instruction lengths and mnemonics. It rejects
other owners and applies the repair in a transaction (report mode rolls
back). Applied in `local/ghidra-draw-tail-20261117`, it grows the body from
446 to 537 bytes; a second report run accepts 537 unchanged. This workspace
includes the preceding decimal-table, bfcc-loop and kotsAccept repairs.

Fresh `local/ghidra-draw-tail-inventory-20261117/body-ranges.json` and the
single-body normalized `local/asm-draw-tail-20261117` inventory confirm all
166 instruction boundaries match. The complete generated body compiles with
OpenWatcom `-q -bt=dos -3r -za99 -w1 -we` without diagnostics. All eight
original direct calls target previously closed generated bodies (0xcc22c,
0xcc1ac, 0xcc1ec, 0xc95ac and 0xc9680). No new all-corpus count is claimed.

`local/ghidra-draw-tail-20261117/check.py` / `execution.json` additionally
record 12 original-versus-generated comparisons for the two recovered
argument-setup blocks, 0x4c2ca..0x4c306 and 0x4c30c..0x4c31a. Three stack
patterns and two flag images compare all GPRs, flags, PC, selectors and
2 MiB RAM, including the original stack writes. Calls are outside these
block comparisons; this does not establish whole-body behavior or callee
behavior. The tracker size and boundary note are corrected while its
blocked/unverified state remains. No gameplay run was required.
`python3 tools/decomp_status.py check` and `git diff --check` pass.

### 2026-11-17: recover the campaign handler's missing tail

`mpClickCampaign` at 0x4fe50 had a valid fallthrough at 0x50069 into a
1,392-byte unowned region, followed by its existing epilogue at 0x505d9.
The old orphan decode began at 0x5006a and incorrectly interpreted the
second byte of XOR as an IN instruction. Correct decoding yields 346
instructions spanning 0x50069..0x505d8, including the loop backedge from
0x50136 to 0x50079. This is a boundary repair, not a port-I/O implementation.

`tools/ghidra/EF2000Recover4fe50Tail.java` pins the language, full payload,
region hash, exact old/repaired bodies and preceding fallthrough; rejects
other function owners; clears the stale listing; checks continuous decode
and its instruction count; and transactionally applies or reports the
contiguous body. Region SHA-256:
`b4ba93d6a7f640aad94681ec788beb4f7bb30978440f8cab9e9cff04f32d9aa7`.
The tracker size changes from 546 to 1,938 bytes without state promotion.

The successful apply and repeated report live in
`local/ghidra-campaign-tail-20261117`. This workspace includes the preceding
bfcc, kotsAccept and 4c120 repairs and is the latest repaired project.
`local/ghidra-campaign-tail-inventory-20261117` is its fresh inventory.
`local/asm-campaign-tail-20261117` records matching Ghidra/objdump layouts
for all 533 instructions and warning-free compilation of the complete
mechanical C body with OpenWatcom (`-q -bt=dos -3r -za99 -w1 -we`).

The local `check.py` / `execution.json` compare the recovered campaign-mode
mapping at 0x50386..0x50450 against the original: inputs 0..80, 0x7fff,
0x8000 and 0xffff with two flag images, 168 full GPR/flags/PC/selector/2MiB
RAM comparisons. This does not verify earlier loops or called helpers.
Eight direct callees are absent from the preceding closed generated set:
0x4f314, 0xa3dc, 0xa4c84, 0xb1e0, 0xb3168, 0xc1438, 0xe490 and 0xf918.
Whole-body equivalence and dynamic execution remain unproven. No gameplay
run or updated bulk-closure count is claimed. Tracker and diff checks pass.

### 2026-11-17: recover the software floating-point tail at 0xf0cec

The remaining audited fallthrough at 0xf0e09 starts XOR ESI,0x8000;
the stale orphan listing began four bytes late. Its 183-byte continuation
contains 66 instructions and ends in RET at 0xf0ebf, immediately before
the next function at 0xf0ec0. `EF2000RecoverF0cecTail.java` pins the full
payload and region hash, checks the exact old/repaired body and preceding
PUSH fallthrough, rejects other owners, and validates continuous recovered
instructions before transactionally reporting/applying the new body.
Region SHA-256:
`be41d5347a8994a54d49e6d7d538d2615caff571529a339774e99377d5735b5d`.

`local/ghidra-soft-tail-20261117/proposal.tsv` records rollback-only
285-to-468-byte recovery in the preceding project; `recovery.tsv` records
acceptance of the already repaired 468-byte body. The latest workspace is
`local/ghidra-soft-tail-20261117`, including all preceding boundary repairs.
Its fresh inventory is `local/ghidra-soft-tail-inventory-20261117`.
The normalized body in `local/asm-soft-tail-20261117` matches all 163
Ghidra/objdump instruction boundaries and compiles with OpenWatcom without
diagnostics (`-q -bt=dos -3r -za99 -w1 -we`). Every direct call target is
already in the preceding closed generated set. No new corpus count or
whole-body equivalence is claimed.

The local `check.py` / `execution.json` record six full GPR/flags/PC,
selector and 2MiB RAM comparisons against original execution of
0xf0e09..0xf0e1e, before the first recovered helper call. Three stack
patterns and two flag images exercise the recovered XOR/rotate and argument
loads; called arithmetic, later loops and floating-point results remain
unverified. Tracker size is corrected from 285 to 468 bytes with no state
promotion. Tracker and diff checks pass; no gameplay runs were required.

### 2026-11-17: compare complete recovered 0xf0cec with real callees

`local/asm-soft-complete-20261117/check.py` now generates a dispatcher for
the complete repaired 0xf0cec body and its five transitive callees, 0xee43c,
0xef97a, 0xef9aa, 0xefbd9 and 0xefdae. Callees execute their generated
original instructions; there are no helper stubs or early block stops.
The host shared library builds with warnings as errors. All eight body,
dispatcher and registry units also compile with OpenWatcom without any
diagnostics (`-q -bt=dos -3r -za99 -w1 -we`), recorded in `compile.json`.

There are 74 successful whole-call comparisons against original execution,
including return to the captured caller, all GPRs, flags, selectors and RAM,
using the existing captured protected-mode descriptor profile. Inputs are
zero and explicit-integer-bit extended-precision mantissas 0x8000000000000000,
0xa000000000000000 and 0xffffffffffffffff at biased exponents 0x3ff0,
0x3ffa, 0x3ffe, 0x3fff, 0x4000 and 0x4003, both signs, with incoming flag
images 0x202/0xad7. No x87 transcendental substitution is needed: these
helpers implement their arithmetic in integer instructions. The negative
cases exceeded the initial instruction budget; increasing it to one million
let original and C both complete without altering their code or results.

Report SHA-256:
`cb18166d50f2db7776a279cadc6e61397f3a72375b744bf77e49ab8df666adf1`.
This is finite input evidence, not exhaustive extended-precision equivalence:
NaNs, infinities, unsupported encodings, extreme exponents and faults are
not covered. No runtime splice or golden replay is claimed, and the tracker
remains blocked/unverified. The result strengthens whole-function evidence
beyond the preceding six isolated argument-setup comparisons. Tracker and
diff checks pass; no gameplay/UI run was used.

### 2026-11-17: recover seven omitted terminal returns

Seven remaining first-failure fallthroughs are missing only their terminal
return. `tools/ghidra/EF2000RecoverReturnTails.java` validates the pinned
payload/language, each exact previous/repaired body, tail hash, preceding
fallthrough and absence of other function owners. It transactionally adds
only the checked RET or RET 4. Existing ranges are preserved, including the
separate ranges in 0x5086c; intervening data is not absorbed.

The recovered bodies and sizes are 0x2065c: 1,182 to 1,185; 0x34a98: 76 to
77; 0x363c4: 80 to 81; 0x4d5f0: 100 to 101; 0x4dae8: 100 to 101; 0x5086c:
273 to 274; and 0x68c18: 124 to 125. The total addition is nine bytes.
The apply and repeated report evidence is in
`local/ghidra-return-tails-20261117`. This is the latest repaired workspace,
including all preceding repairs. Its fresh inventory lives in
`local/ghidra-return-tails-inventory-20261117`.

`local/asm-return-tails-20261117` records matching Ghidra/objdump instruction
layouts and diagnostic-free OpenWatcom compilation for all seven complete
mechanical C bodies, 600 instructions total, using
`-q -bt=dos -3r -za99 -w1 -we`. Source/object hashes are in `compile.json`.
This is boundary and compilation evidence, not whole-call differential or
runtime integration proof. Existing tracker states are retained. No updated
bulk closure count is claimed; direct-dependency analysis is still needed.
Tracker and diff checks pass. No gameplay/UI testing was needed.

### 2026-11-17: refresh bulk closure after recovered function tails

Regenerated all 4,280 selected bodies from the latest repaired workspace,
`local/ghidra-return-tails-20261117`, using its fresh body-range inventory.
`local/asm-c-repaired-watcom-20261117/normalize.py` records the selection and
normalization; `local/asm-repaired-body-inventory-20261117` contains the
normalized inventory. The same original payload hash remains pinned.

`python3 tools/asm_c_program.py --inventory
local/asm-repaired-body-inventory-20261117 --workspace
local/ghidra-return-tails-20261117 --output
local/asm-c-repaired-program-20261117` now reports 4,172 syntactically
supported bodies and 2,582 generated-unverified bodies closed over translated
direct dependencies, covering 427,968 original body bytes. This adds ten
bodies and 4,453 bytes to the preceding bulk result, with no removals:
0x2065c, 0x21e04, 0x426a4, 0x4bef0, 0x4bf1c, 0x4c120, 0x68c18,
0x69354, 0x69374 and 0xf0cec. Boundary repairs therefore unlock callers as
well as repaired functions. Indirect-target coverage is not established.
Translation report SHA-256:
`e85ec524fa1b2836786da73d701c88e25ba8e5aead4bc68efc0e8be1f900c471`.

`python3 local/asm-c-repaired-watcom-20261117/check.py` records clean
OpenWatcom evidence for all 2,649 body/dispatcher/support units, using
`-q -bt=dos -3r -za99 -w1 -we`. Of these, 2,567 unchanged units reuse the
complete prior build after checking compiler, source, shared state header
and object hashes; 82 units compile anew. There are zero failures and zero
diagnostics. Compilation report SHA-256:
`36d8514bb347d8691c1190efce6f6ad6f76e111239258658396d5ed0ed431c60`.

The remaining 1,698 excluded bodies have first-failure reasons: 1,590 direct
dependency closure failures, 101 unsupported instructions, six fallthroughs
outside their bodies and one unresolved count branch/fallthrough. The six
remaining fallthrough entries are 0xc6428, 0xc8c34, 0xfa02c, 0xfcc01,
0xfcc6f and 0x1204b6; the overlap/count case is 0xe7d27. These are distinct
from the six historical fallthrough cases in the blocked tracker subset,
which were repaired. Highest immediate unsupported-callee fan-in remains
0xb45ac FSIN (55 callers) and 0xb45a2 FCOS (52 callers); these counts are
not transitive reachability or unique additional-function predictions.

This refresh establishes generation, direct closure and compilation, not
byte-identical output, general behavioral equivalence, runtime splicing or
golden replay. Tracker states are unchanged. No gameplay/UI runs were used.

### 2026-11-17: distinguish unsupported transfers from missing tails

Inspection of the remaining fallthrough failures found that four are
unsupported system transfers, rather than evidence of missing instructions.
`asm_to_c.instruction_c` now rejects far returns, interrupt returns, traps
and other unmodeled system transfers before checking ordinary fallthrough.
It still emits no implementation for these operations. In particular, a
body must not be extended beyond RETF/IRET merely to satisfy the translator.

Against the latest repaired normalized inventory and its complete instruction
start set, 0xfa02c and 0x1204b6 now diagnose unsupported INT3, 0xfcc01 RETF,
and 0xfcc6f IRET. The real missing tails at 0xc6428 and 0xc8c34 still diagnose
fallthrough outside body. No bytes, ownership ranges or tracker states were
changed. The preceding bulk report remains historical evidence; no new
coverage or whole-program build is claimed for this diagnostic-only change.

Validation: three focused TranslationTests pass, including the new
`test_system_transfers_are_not_missing_body_tails`, which checks identical
rejection with and without a decoded successor and retains the missing-tail
diagnostic for a NOP. Tracker and diff checks pass. No gameplay run was used.

### 2026-11-17: recover the shading helper's result store and return

`tools/ghidra/EF2000RecoverC6428Tail.java` validates the pinned language and
payload, exact old/new body, six-byte tail hash, preceding XLAT fallthrough
and absence of other function owners. It transactionally recovers the MOV
and RET at 0xc6437..0xc643c, extending shade_lookup_c6428 from 15 to 21
bytes. No alignment padding or neighboring function is absorbed.

`local/ghidra-shade-tail-20261117` is the latest repaired workspace; the
fresh export is `local/ghidra-shade-tail-inventory-20261117`. Apply records
15 to 21 bytes and repeated report records 21 to 21. The complete six
instruction Ghidra/objdump layouts match. Generated C compiles without
OpenWatcom diagnostics using `-q -bt=dos -3r -za99 -w1 -we`, and the host
shared library builds with warnings as errors.

`local/asm-shade-tail-20261117/check.py` / `report.json` records 96 passing
whole-call comparisons, using the existing captured protected-mode descriptor
profile, against original execution through the return sentinel. They compare
all GPRs, flags, PC, selectors and 2MiB RAM. Word inputs 0, 1, 0x7f, 0x80,
0xff, 0x100, 0x8000 and 0xffff cross offsets 0, 1, 0x7f and 0xff with flag
images 0x202, 0x602 and 0xad7. This covers low-byte carry/wrap, preserved
upper EAX, and both direction-flag settings of the initial LODSW. Faults,
segment boundaries and runtime integration are not covered. Report SHA-256:
`b83accaec26029e825608e9aefd6fcaedbe43822a5f73b7b8a4f2e9be07d515d`.

Tracker size and evidence notes are corrected, with state unchanged. Bulk
closure has not been regenerated after this repair. Tracker and diff checks
pass. No gameplay/UI run was used.

### 2026-11-17: recover the patched-offset loop tail

`tools/ghidra/EF2000RecoverC8c34Tail.java` recovers 32 bytes at
0xc8c55..0xc8c74 after the preceding JO. It pins payload/language and tail
hash, validates exact previous/repaired bodies and ownership, and checks all
ten recovered instruction lengths/mnemonics transactionally. The function
body grows from 60 to 92 bytes. Its separate 27-byte overflow block at
0xc454a..0xc4564 is retained; the intervening bytes are not absorbed.

The latest workspace is `local/ghidra-offset-tail-20261117`, containing the
preceding shading repair as well. Fresh ranges/instructions are exported to
`local/ghidra-offset-tail-inventory-20261117`. Apply reports 60 to 92 bytes;
repeat report confirms 92 to 92. The normalized complete 22-instruction body
has matching Ghidra/objdump layout. In `local/asm-offset-tail-20261117`, its
mechanical C compiles with OpenWatcom without diagnostics using
`-q -bt=dos -3r -za99 -w1 -we`; host compilation also uses warnings as errors.

The local check.py/report.json records 36 whole-call comparisons against
original execution, including GPRs, flags, PC, selectors and complete RAM.
The normal loop path uses counts 1, 2 and 5, source words 0, 1, 0xffffffff
and 0x80000000, and incoming flags 0x202, 0x602 and 0xad7. This exercises
normal backedges and the recovered result stores/return, but not overflow
branches or their callees. Those direct targets are accepted for generation
of this isolated body, not claimed to be translated/closed dependencies.
The report SHA-256 is
`5858bd35355f58808244b86c8a730a204335be329c19b0613a67d7d8afc7aa56`.
Tracker size/notes are corrected without state promotion. No updated bulk
closure, runtime splice or golden replay is claimed. Tracker/diff checks
pass; no gameplay/UI run was used.

### 2026-11-17: preserve branch-targeted TEST/MOV overlapping instructions

The unresolved count branch at 0xe7d27 is intentional x86 overlap: direct
branches enter the immediate of TEST AX at 0xe7d7a and 0xe7daf, executing
MOV EDI,ECX at 0xe7d7c and 0xe7db1. Both paths rejoin immediately after
the four-byte TEST. The primary Ghidra layout is correct and remains intact.

`asm_inventory.overlapping_test_moves` recognizes only exact four-byte
66 A9 89 /register encodings with an explicit jump/count-branch target at
byte two. It independently decodes the two embedded bytes, requiring one
MOV ending at the same PC, and retains both instruction starts. No arbitrary
interior decoding or function-range extension is accepted. Normalization v2
records this change. `asm_to_c.read_inventory` reproduces it from the payload,
checks the original primary layout, and rejects modified decoded operands.
Historical v1 inventories retain their original linear decoding and hashes.

`local/asm-overlap-20261117` records the complete 100-instruction original
body with both added starts, matching primary layout, and diagnostic-free
OpenWatcom compilation (`-q -bt=dos -3r -za99 -w1 -we`). Host compilation
uses warnings as errors. Of 72 whole-call original/C comparisons, 36 empty
or single-character substring cases match full RAM, GPRs, flags, selectors
and return PC. The other 36 multi-character cases stop at the existing
unsupported descriptor-profile segment reload at 0xe7d73; they are recorded
as mismatches, not counted as successful equivalence. This change does not
relax that segment-model restriction. No whole-body equivalence, runtime
integration or bulk coverage increase is claimed; tracker state is retained.

Validation: all 12 InventoryTests pass, including overlap provenance,
operand tampering, negative encodings/targets and historical-v1 reading.
TranslationTests.test_synthetic_programs_match_registers_flags_and_all_ram
passes with an added conditional TEST/MOV overlap fixture and randomized
register/flag inputs. Tracker and diff checks pass. No gameplay run was used.

### 2026-11-17: restore the shading draft's recovered memory effect

The existing handwritten shade_lookup_c6428 draft omitted the store recovered
in the preceding boundary repair. `decomp/src/misc_drafts1.c` now stores the
translated byte to guest 0x15908a before returning it. Its comment and probe
extent now use the complete 21-byte body and explicitly limit this interface
to DF=0, without original register/flag clobber compatibility.

Inspection also invalidates the historical confirmed signature: the original
reads ESI, not EAX, and writes AX, ESI and EBX. signatures.tsv records those
instruction-level observations and the unverified caller/wrapper ABI. The
tracker entry is returned to drafted/unverified; a successfully compiled
pointer-based C draft must not imply a confirmed original register ABI.
The emitted ABI header now omits the old incorrect one-output pragma for this
function; no runtime replacement is installed.

`local/asm-shade-tail-20261117/draft.py` extracts the production C function
and compiles it with the production memory header for a bounded host mapping.
All 64 original/C comparisons pass for return AL, updated ESI and complete
RAM, including the restored result store: eight input words, four shade
index offsets and two incoming flag images, all with DF clear. This is
narrower evidence than the separately generated explicit-state C's 96
full-state comparisons; it does not validate other registers, arithmetic
flags, faults or DF=1 for the handwritten draft.

`python3 tools/ef2000_wcc.py decomp/src/misc_drafts1.c --compiler <pinned
wcc386> --output local/asm-shade-tail-20261117/misc_drafts1.obj` compiles the
entire production module with zero warnings/errors after the ABI correction.
Tracker and diff checks pass. No gameplay run, splice or replay is claimed.

### 2026-11-17: refresh bulk closure after shading, offset and overlap repairs

Fresh normalization of the same 4,280 selected tracker addresses against
`local/ghidra-offset-tail-inventory-20261117/body-ranges.json`, including the
validated TEST/MOV overlap decoding, produces
`local/asm-latest-body-inventory-20261117`. Bulk generation in
`local/asm-c-latest-program-20261117` supports 4,175 bodies syntactically and
closes 2,587 bodies over direct edges, covering 428,570 original body bytes.
Compared with the preceding 2,582-body result, the added bodies are 0xc3364,
0xc6370, 0xc6428, 0xc8c34 and 0xe7d27; none are removed. Dynamic targets
remain outside this closure proof. The translation report SHA-256 is
`a4c013d5bf5a09aa0b7a9c97ce42598617c2eeb56623c27ceb7ce60b946086d4`.

The remaining 1,693 excluded bodies have first-failure diagnostics of 1,588
untranslated direct dependencies and 105 unsupported instructions. No
missing-fallthrough or count-branch diagnostic remains in this inventory.
These categories describe generator diagnostics, not equivalence results.
In particular, e7d27 still lacks descriptor-table segment reload semantics
for its multi-character path, as recorded above.

Intersecting this fresh report with the 810 blocked tracker entries gives
307 generated-unverified, 461 excluded by direct dependencies, 32 rejected
by unsupported instructions and 10 not selected into this inventory.
Thus the tracker label does not mean that all 810 functions currently fail
C generation. No tracker promotion is justified by this compilation report.

`python3 local/asm-latest-watcom-20261117/check.py` verifies 2,654 translation,
dispatcher and registry units against the pinned Watcom compiler: 2,567
unchanged units reuse checked source/compiler/object hashes and the identical
state header; 87 units compile anew. All have zero errors and diagnostics.
Compiler report SHA-256:
`9bfd2c00c2a0d8b75151866918b73bf7ed665eb47df20dbfb00261feadf45395`.
This refresh measures generation and compiler coverage only; it supplies no
new full-state equivalence, gameplay, splice or replay evidence.

### 2026-11-17: implement null data-selector loads in descriptor mode

Descriptor-profile MOV loads of ES, DS, FS and GS now accept null selectors
0..3 without consulting descriptor tables. The selected register retains
its RPL, and its cache becomes unusable, so subsequent data accesses reject
with status 7 without changing registers, flags or RAM. Null SS and non-null
reloads retain the unsupported result; table lookup, accessed-bit updates,
privilege transitions and POP segment loads are not implemented by this step.
This is a bounded part of the segment-model blocker, not its resolution.

The differential harness previously required all caches to remain unchanged.
It now derives the expected unusable cache for a changed null data selector
from the original CPU's final selector, while preserving the unchanged-cache
check for all other cases. It does not use the generated C's selector to
choose the expected cache.

`python3 -m unittest tests.test_asm_capture tests.test_asm_segments` passes
all eight tests with pinned Unicorn. The new regression supplies initially
usable descriptors and compares 16 MOV/return cases (four data registers,
four RPLs) against the original, including full RAM, GPRs, flags and selectors.
It separately checks cache invalidation and rejection of a subsequent read.
The prior same-selector non-null reload still reports unsupported.
The generated `local/asm-null-segments-20261117/lift.c` compiles with pinned
Watcom using `-q -bt=dos -3r -za99 -w1 -we` with zero diagnostics.
The runtime header hash changes; the preceding bulk report remains historical
and is not evidence of compilation against this new header. No tracker
promotion, runtime splice or gameplay/replay result is claimed.

### 2026-11-17: use checked stack reads for segment POP

Segment POP previously rejected every descriptor-profile context and read
through the legacy flat helper. The translator now validates the exact
segment POP encoding, reads its 16-bit selector through the current SS
cache, invokes the segment loader, and advances ESP only on success.
Operand-size 32 advances ESP by four; operand-size 16 advances it by two.
Null data selectors use the preceding verified loader implementation;
non-null descriptor-table loads and null SS remain unsupported.

The capture regression now exercises MOV plus both POP operand sizes for
ES, DS, FS and GS with all four null-selector RPL values: 48 original/C
full-state comparisons. The 32-bit stack values include nonzero upper halves
to check selector truncation. All cases also check cache invalidation and
subsequent access rejection. The 32 POP cases additionally reject an invalid
SS extent without changing registers, selectors, caches, flags or RAM, and
accept a two-byte readable selector at the SS upper limit while advancing
ESP according to operand size. An independent one-instruction Unicorn
probe in `local/asm-null-pop-20261117/oracle_width.py` confirms the 32-bit POP DS
selector read succeeds with only two bytes inside the SS limit.

The generated mixed-operand fixture in `local/asm-null-pop-20261117/lift.c`
compiles with pinned Watcom using `-q -bt=dos -3r -za99 -w1 -we`, with zero
diagnostics. No tracker promotion, runtime splice or gameplay run is claimed.

### 2026-11-17: retain source-segment checks for far-pointer loads

LES/LDS/LFS/LGS/LSS previously read their pointer through flat RAM helpers,
with a blanket rejection of captured descriptor contexts. The translator now
checks the complete offset-plus-selector extent through the operand's source
segment before reading either field, including stack access rights for an
SS source. Both field reads retain that segment. Destination-register writes
remain after successful selector loading, so source faults and unsupported
reloads cannot partially commit the destination. The blanket guard is removed
only after wiring these implicit reads; non-null descriptor-table loads and
null SS still fail through the segment loader.

`AsmCaptureTests.test_null_data_selector_reload` now passes 80 original/C
full-state cases: the previous MOV/POP forms plus 32 far-pointer cases covering
LES/LDS/LFS/LGS, 16/32-bit offsets and four null-selector RPLs. The 16-bit cases
preserve the destination's upper half. Each far-pointer case also checks three
C fault paths (96 checks): a selector truncated by the segment limit, one
truncated by bounded RAM, and a non-null selector requiring an unsupported
table reload. Registers, selectors, caches, flags and RAM remain unchanged
on these faults. Existing cache invalidation/access and POP fault checks pass.

Validation: `python3 -m unittest
 tests.test_asm_capture.AsmCaptureTests.test_null_data_selector_reload
 tests.test_asm_c_segments.SegmentStringTests.test_segment_moves_push_pop_and_far_pointer_loads`
passes, including 48 legacy-profile programs with two differential scenarios
each. The generated eight-form fixture in
`local/asm-null-far-20261117/lift.c` compiles with pinned Watcom using
`-q -bt=dos -3r -za99 -w1 -we`, with zero diagnostics. Tracker and diff checks
pass. No original-game whole-body equivalence, bulk coverage increase, tracker
promotion, runtime splice or gameplay result is claimed.

### 2026-11-17: validate current descriptor bytes before non-null reloads

Non-null segment reloads remain a concrete blocker for e7d27 and other
captured paths. Reusing a previously loaded cache is invalid when table RAM
has changed. Before extending the explicit-state ABI with current GDT/LDT
context, `tools/asm_segments.py` now provides a separate, side-effect-free
lookup primitive. It accepts an already selected table base/limit and current
guest RAM, checks selector width/nullness, table extent, address overflow and
RAM extent, decodes the descriptor, and validates data/stack load rights.
Success returns a candidate cache with accessed rights plus the access-byte
address; installation and the actual accessed-bit RAM write remain the
loader's responsibility. Errors leave outputs and RAM untouched. Null data
selectors continue to use the existing path; LDT entry zero is not null.

Load privilege rules follow Intel's MOV protected-mode operation and exception
conditions (Intel SDM Volume 2B, MOV):
https://cdrdv2-public.intel.com/825757/253667-sdm-vol-2b.pdf
Readable conforming code is distinguished from data/nonconforming code.
This helper does not deliver architectural exceptions, select a table,
install a cache, implement SS interrupt inhibition or enable any new generated
path. In particular, non-null reloads still fail closed. The next integration
must supply live table context, retain guest table writes, commit the accessed
bit with cache installation, and derive differential cache expectations from
original execution rather than the C result.

`python3 -m unittest tests.test_asm_segments` passes 8,192 lookup privilege
combinations (256 access bytes, four CPLs, four RPLs, data/stack), including
unchanged RAM/output rejection checks, plus table/RAM bounds, same-selector
changed-table bytes and descriptor decoding. These are specification-based
checks, not 8,192 original/C comparisons. A standalone lookup/access wrapper
in `local/asm-segment-lookup-20261117/lookup.c` compiles with pinned Watcom
`-q -bt=dos -3r -za99 -w1 -we` with zero diagnostics. The generated runtime
header changes, but its state layout and enabled reload semantics do not.
No tracker promotion, bulk coverage increase or runtime integration is claimed.
The combined segment tests and 80-case captured null-reload regression pass
(four unittest methods). A standalone lookup/access wrapper in
`local/asm-segment-lookup-20261117/lookup.c` compiles with pinned Watcom using
`-q -bt=dos -3r -za99 -w1 -we` without diagnostics. Tracker/diff checks pass.
No tracker promotion, new bulk closure, original-game equivalence, splice or
replay is claimed.

### 2026-11-17: install data-segment caches from opt-in live tables

The descriptor lookup primitive is now connected to the explicit-state data
segment loader. State ABI 5 adds an opt-in table mask and current GDT/LDT
base/limit pairs. A non-null ES/DS/FS/GS reload selects its table from TI,
looks up current bounded guest RAM, rejects unsupported bases/modes, writes
the accessed bit, and installs the candidate cache and visible selector.
All validation precedes these commits. Descriptor snapshots are never reused
as the source of a reload. The ctypes state mirrors the new ABI; old generated
ABI-4 artifacts must be regenerated before differential verification.

SS reloads remain unsupported because interrupt/debug inhibition is absent.
Nonzero bases, long-mode descriptors and conforming-code caches remain
unsupported by this profile. The latter have a distinct access-rule audit
pending despite the lookup primitive understanding their load privileges.
Table windows must be explicitly supplied by the caller; automatic capture
wiring is not enabled yet. It must check that captured table bytes really
live in bounded RAM and compare cache changes against original execution.

`AsmCaptureTests.test_live_table_reload_observes_guest_writes` passes eight
whole-call original/C cases: four data segment registers, each through GDT
and LDT. Guest instructions change descriptor limit/type bytes before the
reload; LDT cases reload the same visible selector, proving the old cache
is not reused. All cases compare complete 256-KiB RAM (including accessed-bit
writes), GPRs, flags, PC and visible selectors, plus the installed cache decoded
from original table RAM. Table configuration remains unchanged. Forty further
C rejection checks cover disabled tables, short table limits, truncated RAM,
nonzero bases and unsupported mode bits without register/cache/RAM changes.

The generated loader fixture in `local/asm-live-reload-20261117/lift.c`
compiles with pinned Watcom using `-q -bt=dos -3r -za99 -w1 -we`, with zero
diagnostics. No original-game whole-body equivalence, tracker promotion,
runtime integration, bulk closure increase or replay is claimed.
`python3 -m unittest tests.test_asm_capture tests.test_asm_segments
 tests.test_asm_c_program` passes all 26 methods, including the preceding
80 null-reload cases and 8,192 lookup privilege combinations. Tracker and
diff checks pass.

### 2026-11-17: gate captured live tables on complete RAM backing

`asm_capture.live_table_windows` identifies GDT/LDT windows only when the
complete captured table fits the bounded guest RAM and its bytes match
exactly. Snapshot mappings outside RAM, truncated tables, mismatched bytes,
invalid extents and 32-bit address overflow remain unavailable. This helper
is preparatory: the differential runner still leaves live reloads disabled
until cache transitions can be checked against original execution evidence.

`python3 -m unittest tests.test_asm_capture.AsmCaptureTests.test_live_table_windows_require_complete_matching_ram`
passes; `git diff --check` passes. No tracker promotion or new bulk equivalence
is claimed.

### 2026-11-17: verify live descriptor reloads in captured whole calls

The captured differential runner now supplies opt-in GDT/LDT windows only
when `live_table_windows` proves complete, matching RAM backing. It checks
that C leaves the table mask/base/limit configuration unchanged. An original
instruction observer recognizes MOV, POP and far-pointer data-selector loads,
saves descriptor bytes before execution, and commits cache expectations at
the following instruction boundary. This handles same-selector reloads and
keeps later table writes from changing earlier cache expectations. Original
faults and budget exhaustion remain inconclusive. SS reloads and other
unsupported loader profiles remain unsupported.

The eight existing live-table original/C cases now also pass through the
public comparison runner, exercising four data registers through GDT and
LDT with guest descriptor writes before reload. A separate original-only
regression reloads the same selector and then changes its descriptor limit;
the observer retains the loaded limit rather than using final table bytes.
The preceding null-load comparisons exercise MOV/POP/far-pointer observation.

Fresh ABI-5 generation of the complete 100-instruction `0xe7d27` body in
`local/asm-live-wholecall-20261117` passes all 72 original/C whole calls,
including all 36 multi-character cases previously rejected at MOV ES,DX.
These compare RAM, GPRs, flags, selectors, modeled caches and x87 state.
Pinned Watcom compilation with `-q -bt=dos -3r -za99 -w1 -we` has no
errors or diagnostics. Source SHA-256:
`39baa984790c25a31c852c679f196f6ee93769f9727d3024081edaf38842fd82`;
object SHA-256:
`8cdae35043346888befe79798bde5a5b96b5c86a5a6d72681f0c9dd373225a0b`;
execution report SHA-256:
`f45288c872df30d114b369c07cbc058fb2228c7ddf10659f42e70edbf8da3b7e`.
No tracker promotion, runtime splice or gameplay replay is claimed.

The combined `tests.test_asm_capture`, `tests.test_asm_segments` and
`tests.test_asm_c_program` run passes all 28 methods. The live-table cases now
also run through the public differential comparison path (eight whole calls).
A separate original-only regression reloads selector 7 and subsequently
changes its descriptor, checking that the observer retains the loaded cache.

### 2026-11-17: extend captured whole-call evidence to six segment-using leaves

Regenerated complete explicit-state bodies for strcat 0x9ca57, strlen
0xb1b8b, memcpy 0xdc832, memcmp 0xed115, far strnlen 0xed542 and memmove
0x109e6f from the normalized latest body inventory. All six compile with
pinned Watcom (`-q -bt=dos -3r -za99 -w1 -we`) without diagnostics.
`local/asm-segment-cluster-20261117/check.py` compares 36 captured whole calls
per body against original execution: 216 pass, zero mismatches or
inconclusive results in the final report. The comparisons include complete
2-MiB RAM, GPRs, flags, selectors, observed modeled caches and x87 state.

The input grid uses counts 0,1,2,3,4,7,8,15,31 and four buffer/flag variants.
Copy/move cases include overlapping buffers in both directions; strcat uses
separate buffers with four alignments. Far strnlen loads selector 7 and
includes termination before its bound. DF is clear on entry; memmove's
backward path executes STD and restores CLD. These are bounded samples,
not universal equivalence or coverage of exceptional inputs. An exploratory
strcat grid with overlapping buffers exhausted the original instruction
budget in 17 cases and was inconclusive; the final grid replaces those with
separate-buffer alignment cases, without treating the earlier runs as passes.

The final report records source/object hashes for all six bodies. SHA-256:
`00e149ad461921becb5a9a9679a05fbbb5513db75cb70f5f54881e6450529207`.
Tracker validation and diff checks pass. This adds evidence for generated
whole bodies, not for the existing handwritten drafts or byte-identical
recompilation. No tracker promotion, runtime integration or replay is claimed.

### 2026-11-17: translate protected-mode LSL against live descriptor RAM

The generator now handles 16/32-bit LSL destinations and register or 16-bit
memory selector sources. The explicit-state helper reads the selected live
GDT/LDT descriptor, checks bounds/type/visibility, expands page-granular
limits, and returns a limit without installing a cache or writing the
accessed bit. Null, out-of-table, invalid-type and privilege-rejected
selectors clear ZF and retain the destination. Missing descriptor context,
truncated RAM or overflowing table addresses remain unsupported status 7,
before flags or destination changes. Descriptor presence is not a load-limit
requirement; all code/data types and the protected-mode LDT/TSS types are
accepted according to Intel's LSL description and descriptor-type table:
https://cdrdv2-public.intel.com/782156/325383-sdm-vol-2abcd.pdf .

`tests.test_asm_capture.AsmCaptureTests.test_lsl_live_descriptors_match_original`
compares 1,024 original/C cases (all 256 access bytes, two destination widths,
register/memory sources), including nonzero descriptor bases and effective
limits with granularity. Another 48 null/out-of-table comparisons exercise
both incoming ZF values. Twelve C context-rejection checks assert unchanged
GPRs, flags, selectors, caches and RAM. The combined capture/segment/program
run passes 29 methods before the final rejection-check addition; the focused
LSL method is repeated afterward.

The complete 78-byte `0x10d63b` body now translates and compiles with pinned
Watcom (`-q -bt=dos -3r -za99 -w1 -we`) with no diagnostics. Twenty-four
captured original/C whole calls pass, including the LSL path, zero-global
path and both final comparison outcomes. Generated artifacts and inputs are
in `local/asm-lsl-20261117`; source SHA-256:
`425a8631ccaf56b725c4833aff2d2ca86c177720932a6e50e93a1fd2d050a98b`;
object SHA-256:
`0ac38f02b15b3f737c4a89c360ec1fd3f234f55fe62393363b359658d619d632`;
report SHA-256:
`9aaffe6b501240bda6e714633a3ee6e87031d3230d7f2fa8381e0f1628dc6a32`.
The second LSL-containing body, `0x10d80b`, still depends on unsupported
extender calls; no new bulk-closure count, runtime integration or tracker
promotion is claimed. Tracker and diff checks pass.

### 2026-11-17: refresh ABI-5 bulk translation and exercise the new LSL chain

Fresh generation against the same 4,280-body normalized inventory produces
`local/asm-c-live-program-20261117`: 4,177 syntactically supported bodies,
2,589 closed over direct edges, 428,767 original body bytes. Compared with
the previous 2,587-body report, 0x10d63b and its 119-byte caller 0x10d94f
are added; no body is removed. The remaining 1,691 bodies have first-failure
diagnostics of 1,588 untranslated dependencies and 103 unsupported
instructions. Dynamic target coverage is not established by direct closure.
Translation report SHA-256:
`75aeffc8435253566b2b6f58a67c077b231a2538c81cf9f28b9d571dbc6c297f`.

Recomputing the intersection with the current 810 blocked tracker entries
gives 310 generated-unverified, 457 dependency-excluded, 33 unsupported and
10 outside this inventory. This corrects the earlier logged intersection
(307/461/32/10); the old report also intersects the current tracker as
310/457/33/10. Neither newly admitted body is a blocked tracker row. No
tracker state is changed based on compilation or generation.

A checked two-body subset in `local/asm-lsl-chain-20261117` generates a
shared dispatcher for 0x10d94f and 0x10d63b. All 96 captured original/C
whole calls pass, comparing complete 2-MiB RAM, GPRs, flags, selectors,
modeled caches and x87 state. Inputs cover requested sizes
0,1,3,4,4095,4096,0xffffff00,0xfffffffe, two minimum allocation sizes,
two extender profiles and three heap-global/header cases. The nested LSL
path executes against live descriptor tables. These samples cover normal
rounding, overflow paths and the return to the translated caller; they do
not validate the remaining heap allocator or extender calls.
Execution report SHA-256:
`13c7b0e3a518114c2d7bd159252c44229a1f412150325b39fd0e353dcf9970e6`.

`local/asm-live-watcom-20261117/check.py` freshly compiles all 2,656
translation, dispatcher and registry units against the ABI-5 runtime with
pinned Watcom. No previous object evidence is reused across the runtime
change. All units have zero errors and diagnostics; all resulting object
hashes are rechecked against the report. Compiler report SHA-256:
`d06f274ddd8b6daec93a3df52234207e8c6435f41b34f9caf27386a64688e992`.
Tracker/diff checks pass. This establishes bulk compiler coverage and bounded
whole-call evidence, not byte-identical recompilation, full-program
equivalence, runtime integration, splice or replay.

### 2026-11-17: expose transitive first-failure blockers

Bulk generation reports now retain direct dependencies for dependency-excluded
bodies and list `root_blockers` for every excluded body. A fixed-point
propagation handles recursive call clusters without dropping failures reachable
through a cycle. These are first observed translation failures, not exhaustive
instruction audits or a promise that fixing one admits every caller. Generated
code, admission rules and tracker states are unchanged.

Reconstructing the graph from the current live-program report identifies
interrupt bodies `0x23b` and `0x10d75c` as reachable from 1058 and 1057 excluded
bodies respectively (including themselves). `0xfa95f` reaches 767;
`0x110393`, 762; `0x1101be`, 740. FSIN `0xb45ac` and FCOS `0xb45a2`
reach 324 and 323. Counts overlap. The ignored evidence is
`local/asm-c-live-program-20261117/blocker-impact.json`. This prioritizes
faithful interrupt/runtime support rather than treating all dependency
exclusions as independent missing translations.

Validation: `python3 -m unittest
 tests.test_asm_c_program.ProgramTests.test_unsupported_callee_removes_all_transitive_callers
 tests.test_asm_c_program.ProgramTests.test_root_blockers_propagate_through_recursive_dependencies`
passes both methods, covering emitted report metadata, a recursive cluster,
multiple roots and an admitted recursive body. No gameplay runs or tracker
promotions.

### 2026-11-17: repair X-32 break-service draft arguments and result

Inspecting the highest-impact interrupt roots against normalized original
instructions and `Machine::interrupt` finds that 0x235/0x23b adjust the X-32
program break, despite historical timer-vector names. The original MOV
ECX,EAX at 0x23f supplies the requested break; MOV AX,0x3508 preserves EAX's
upper half. After the service, 0x24e stores returned ECX, not the request,
at 0x120cd0. The existing C draft supplied zero ECX and stored the request.
Correct both and preserve the original partial AX write in service inputs.
Update tracker/signature explanations without renaming existing linkage.

The C++ runtime already implements this service at interrupt IP 0x245,
including page rounding and allocation accounting. It is not generic DOS
get-vector semantics. The other high-impact root 0x10d75c uses AH=0x48 and
requires a separate allocation path. Generated explicit-state C currently
has no bridge to either service; INT remains unsupported. The scalar draft
still uses the existing external DOS adapter and is not a full-state
replacement. No coverage or verification promotion follows this correction.

Validation: pinned `wcc386 -q -bt=dos -3r -zp1 -za99 -w1 -we
-i=decomp/include -fo=/tmp/ef2000-break-draft.obj decomp/src/misc_drafts1.c`
passes with zero diagnostics; tracker and diff checks pass. An initial
compile without the required `-zp1` failed existing world-grid layout
assertions; the repository flags resolve those errors. No gameplay runs.

### 2026-11-17: add an explicit opt-in INT 21h host-service boundary

The high-impact interrupt roots need access to existing stateful runtime
services, rather than fabricated DOS results in generated instruction code.
Add `--interrupt-boundaries` to bulk generation. Exact CD21 instructions
with known continuations yield dispatcher status 0x121 after advancing PC,
preserving all other guest state. The host must apply the real service
response before resuming; this is not architectural interrupt delivery.
Default translation continues rejecting INT. Reports enumerate service
sites and contract. The ordinary differential driver rejects these reports
because it has no service-aware oracle/bridge yet. Tracker states and bulk
coverage are unchanged; runtime integration remains unfinished.

Validation: three focused program tests pass (interrupt state preservation
and host-CF resume, ordinary dependency exclusion and cyclic root tracking).
The new compiled fixture compares the entire state structure except the
specified PC transition and all guest RAM at the boundary, then observes
host-returned CF through the next ADC and returns through the original
stack. All three generated fixture units compile with pinned Watcom
`-q -bt=dos -3r -za99 -w1 -we`. Tracker and diff checks pass. No gameplay
runs or original-game equivalence claim.

### 2026-11-17: compare translated interrupt callers through the X-32 model

Extend low-level `asm_c_differential.compare` with an explicit service factory
creating independent original/C register-service handlers. Original INT21
hooks and translated yield events use the same register contract, record
request/response traces, and compare them alongside the full existing guest
state/RAM comparison. The translated dispatcher consumes a single instruction
per call so service resumption cannot reset the total instruction budget.
Original service rejection is inconclusive. Memory/selector-changing services
and the general verification CLI are still unsupported.

A synthetic X-32 heap-growth wrapper passes four requested-break cases using
`serve_startup_int21` and independent `Allocation` models; the models' final
state is also checked. An exhausted request is inconclusive. The actual
0x235/0x23b pair in `local/asm-int21-break-20261117` generates and compiles
with pinned Watcom and passes 48 original/C calls: both entries, six request
values and four incoming flag states, including skip and page-rounding paths.
Complete guest state/RAM and service traces match; independent allocation
instances end identically. Report SHA-256:
`9a443dc7f430e12d84e83a1cd7fd6660138414e567ffd80b77b60116ef6a659c`.
This compares original wrapper instructions against translated C under the
shared service model, not the original extender implementation. It does not
verify the handwritten draft, integrate the runtime bridge, admit general
interrupts or promote tracker coverage. All 20 methods in
`tests.test_asm_c_program` pass with Unicorn (18.3 seconds); tracker/diff
checks pass. No gameplay runs.

### 2026-11-17: native generated-service runner and X-32 heap callback

Add `ef_lift_run_services` to the generated registry API, retaining exact
instruction-fuel accounting through native callbacks. Nonzero callback
results and pending faults stop execution; a missing callback retains the
raw external-service yield. Add `runtime/asm_x32_service.{c,h}` to map the
audited IP 0x245 / AX 0x3508 request onto existing `x32_heap_grow`, preserving
all guest fields except returned EAX/ECX/flags. Caller owns initialized
allocation context and reserved guest RAM. Unsupported/exhausted requests
return status 0x122, not a fabricated DOS result. Interactive runtime wiring
and all other service kinds remain pending.

The compiled native-runner fixture checks total fuel across an INT,
zero-fuel preservation, absent callbacks, native page-rounded results and
allocation bookkeeping against the Python model, and rejection without
register corruption. All 21 program tests pass (19.1 seconds); the focused
native test passes again after adding the missing-callback assertion.
`local/asm-int21-break-20261117/check_native.py` compiles the real 0x235/0x23b
pair plus native adapter and allocation implementation with host C and pinned
Watcom (`-q -bt=dos -3r -za99 -w1 -we`). It compares 48 whole calls with
original instructions using Python services versus translated C using native
services: complete guest state/RAM, request/response traces and allocation
fields match. Native evidence report SHA-256:
`ce2a3f7371bc014b72b078d87b0595336619ae9a35775449c7b8e3d3ce608a6c`.
This strengthens translation/bridge evidence but does not independently
execute the original extender or promote bulk/tracker coverage. Tracker/diff
checks pass; no gameplay runs.

### 2026-11-17: restrict bulk service admission to audited interrupt sites

Add repeatable `--interrupt-site` restrictions to experimental boundary
translation, so a host implementing only IP 0x245 does not admit arbitrary
INT21 callers. An empty API site set admits none; supplying sites without
boundary mode is rejected. Reports list only admitted service sites.
A focused regression verifies unrestricted versus restricted admission,
empty restrictions and invalid option combinations. The native runner test
also passes; tracker/diff checks pass.

Fresh generation with `--interrupt-boundaries --interrupt-site 0x245` produces
2591 generated-unverified bodies / 428817 original body bytes, adding only
0x235 (6 bytes) and 0x23b (44 bytes) to the prior default 2589-body result.
The remaining 1689 entries are excluded. This is external-service-dependent
coverage, not standalone coverage or a tracker promotion. In particular,
large caller chains still encounter alternate allocation and other unsupported
services; the previously measured root impact was not a prediction that one
service would admit those callers. The report requires exactly site 0x245.

Artifacts: `local/asm-c-native-heap-program-20261117/report.json`, SHA-256
`7e229388f2718fb06ba521b6d0af059bf97a1b1fb4d6edbd1abe1c060621ff74`.
Pinned Watcom `-q -bt=dos -3r -za99 -w1 -we` compiles the two new bodies,
dispatcher, all 66 registry units, native adapter and allocation implementation
(71 units total) with no diagnostics. Previously unchanged translation bodies
are not recompiled. Earlier whole-wrapper evidence remains relevant to the
audited service path; this run adds no gameplay or universal equivalence claim.

### 2026-11-17: distinguish alternate heap allocation ABIs before bridging

Audit original 0x10d80b/0x10d75c/0x10d689 and startup 0x86. Normal startup
writes mode 3 at 0xbb and 0x132a60=1 at 0xc2. Alternate growth is selected
for mode 1 with its submode byte zero, or mode 9. Mode 1 requires DPMI
allocation and selector-base services, plus DOS allocation/free fallback.
The other alternate path issues full EAX=0x80004800 and consumes returned
EAX as a pointer. The measured ordinary paragraph service returns a segment
in EAX and a linear pointer in EBX, so reusing it would create an ABI error.
The existing C++ AH-only dispatch does not establish the extended contract.
Keep this service unsupported rather than codifying that assumption.

Name the two unnamed allocator rows and replace generic blocked notes with
the exact service sites and dependencies. Document the distinction in
`docs/x32-services.md`; no state promotion or runtime behavior change.
Evidence is original normalized application instructions, compared with the
existing measured conventional-memory service description and runtime
handler. Tracker and diff checks pass. No gameplay runs or new equivalence
claims. The next bridge work requires recovering the extended allocation
contract, or supporting the DPMI/selector path, before admitting those services.

### 2026-11-17: recover the allocator's release prerequisite

Inspect original 0x10d466 and both direct callers 0x10d4c4/0x10d689.
Name it heap_release_empty_10d466 and record its no-argument, five-register
preserving ABI, including the shared 0x10d756 epilogue. It unlinks empty
heaps before releasing their backing allocations through DPMI AX0502 or
ES-based DOS AH49, then continues irrespective of service carry results.
Thus allocation support alone cannot admit the mode-1 allocator chain.
Document the heap emptiness check, handle layout and service sites in
x32-services.md; replace another generic tracker blocker with concrete
requirements. No translation/state promotion or runtime behavior change.
`python3 tools/decomp_status.py check` and `git diff --check` pass.
Evidence is static original instructions and both callers; no gameplay runs.

### 2026-11-17: carry DPMI service boundaries through generated execution

Extend experimental exact-encoding host boundaries from CD21 to CD31.
Status 0x131 carries vector 31h; the native runner dispatches both vectors
without resetting fuel. Per-site IP/vector requests and status mappings use
report model int-service-yield-v2. Existing site restrictions apply to both
vectors; default generation still rejects interrupts. No fabricated interrupt
frame, service return or memory/selector change is introduced.
The differential API's opt-in vector_services handlers receive the vector;
legacy INT21 handlers reject DPMI explicitly. Original/C traces include
vectors when that interface is selected.

All 23 tests in tests.test_asm_c_program pass with pinned Unicorn (19.633s),
including mixed-vector trace equality, callback routing, absent callbacks,
site restriction and shared fuel/resumption. Tracker and diff checks pass.

Fresh generation admits only the nine identified heap sites: 0x245,
0x10d4ab, 0x10d4b6, 0x10d6a3, 0x10d703, 0x10d717, 0x10d728, 0x10d737,
0x10d7ab. It generates 2830 bodies / 507701 original body bytes, leaving
1450 excluded. This adds 239 bodies / 78884 bytes over the prior IP245-only
result, including 0x10d466/0x10d689/0x10d75c/0x10d80b. These bodies require
external services that remain incomplete; admission is not execution coverage.
Pinned Watcom flags -q -bt=dos -3r -za99 -w1 -we compile all 239 new bodies,
dispatcher and 77 registry units (317 units) without diagnostics.
Report: local/asm-c-heap-services-20261117/report.json, SHA-256
d69a7d912efc54dcea07434514bfde3de0d29dfdb9f3c81ca7dc8763ce10a464.

A three-body generated cluster (0x10d427/0x10d466/0x10d689) passes 48
whole-call original/C comparisons for mode-1 allocation's DPMI success path:
six sizes, two allocation addresses, four flag patterns, empty initial heap
list, explicit register-only service responses. Complete guest state/RAM and
service traces match. This does not validate the original extender, nonempty
heap release or allocation-failure fallback. Reproducer and evidence:
local/asm-dpmi-allocate-20261117/check.py and report.json, SHA-256
ca4111cb26d967000e3b0d098c048447f5b3efe4b78f12222fe4361e1d2f3950.
No tracker promotion or gameplay runs. Next work is accurate stateful service
models and memory/selector effects before this larger cluster can be trusted.

### 2026-11-17: recover empty-heap cleanup as C

Add decomp/src/heap_release_empty.c for 0x10d466, keeping DPMI and DOS
selector release as explicit unresolved platform boundaries. Preserve the
original emptiness calculation, saved next pointer, unlink-before-header-read
ordering, handle-word split and continuation after release failure. The
existing heap_release.c block coalescer is separate and unchanged.
Both direct callers and shared epilogue were audited in the preceding ABI
commit. Add the declaration and point the blocked tracker row at the draft;
no verified/compiles state promotion because service/guest ABI work remains.

Pinned Watcom compilation with -q -bt=dos -3r -zp1 -za99 -w1 -we
-i=decomp/include succeeds without diagnostics. A focused original-Unicorn
versus recovered-host-C comparison passes 96 cases: all eight empty/busy
patterns in a three-node heap list, DPMI-only/mixed/DOS-only release types,
success/failure carry responses, and two incoming flag patterns. Compare RAM
apart from original stack scratch 0x3effc0..0x3f0004 and compare release
arguments/order. Original DOS ES restoration is checked using a distinct
valid selector. The host uses the existing recovered unlink helper extracted
from runtime_helpers.c; it does not independently verify that helper's guest
ABI. The checks do not compare final registers/flags or validate real extender
release semantics. These are deliberately narrower than whole-state evidence.
Reproducer: local/heap-release-source-20261117/check.py; report.json SHA-256
c840b27a5f9f0531b136b2ba01355c80a9708463c4a4418547d60196a38415ab.
Tracker/diff checks pass. No gameplay runs.

### 2026-11-17: recover mode-1 allocator C including DOS fallback

Add decomp/src/heap_alloc_mode1.c for original 0x10d689 (211 bytes),
with explicit semantic service boundaries in ef_heap_services.h. Recover
cleanup-before-allocation, DPMI handle stores (including the success/null
address case), backing headers, reservation cap, size limit, two DOS
allocations, unconditional temporary release after the second request, and
selector-base result consumption without testing carry. Encode the original
RCL/ROR DOS result convention explicitly instead of assuming an X-32 pointer.
Record EAX size/result and preserved registers from original pushes/pops,
shared epilogue and the only direct caller found, 10d75c@10d78f.
Name the fallback reservation-size global. Services remain unresolved and
this source does not claim the original guest register/flag ABI is reproduced.
The tracker stays blocked; generated service-dependent admission is unchanged.

Pinned Watcom -q -bt=dos -3r -zp1 -za99 -w1 -we -i=decomp/include
compiles the new source without diagnostics. Focused original-Unicorn versus
recovered-host-C evidence passes 108 cases: DPMI success, failure and success
with address zero; temporary/second DOS allocation failures; DOS success;
sizes 0,1,7,16,65536,65537; reservation globals 0,0xfffff,0xf0000000.
Return addresses, RAM (except original stack scratch) and ordered service
arguments match. Tests also inject ignored DOS release and selector-base
carry failures. Cleanup has an empty list here; its nonempty cases were
checked separately. Full register/flag ABI and actual extender behavior are
outside this evidence. Reproducer: local/heap-mode1-source-20261117/check.py;
report.json SHA-256:
d2efd036867dbb5e8e4635a320796017173a58bbdc4af46b5503211a18d8a980.
Tracker/diff checks pass. No gameplay runs.

### 2026-11-17: recover alternate heap-growth caller C

Add decomp/src/heap_grow_alternate.c for 0x10d75c and its shared epilogue.
Recover enable/break guards, size alignment, mode-1 backing allocation versus
the explicit EAX80004800 extended service, both size deductions, underflow
and minimum-size checks, insertion, allocated-bit marking, heap counters and
release-to-free-list call. Keep the extended service as an unresolved semantic
boundary; its pointer result is not ordinary DOS AH48. Record the single direct
caller 10d80b@10d82f and original preserved registers; name the growth-enable
global. The existing normal-growth source can now reference a recovered caller,
but platform services and exact guest register/flag ABI remain pending.

Pinned Watcom -q -bt=dos -3r -zp1 -za99 -w1 -we -i=decomp/include
compiles without diagnostics. Focused original-Unicorn/source-host comparisons
pass 384 cases: modes 1/9, six sizes (8,12,60,64,68,4096), allocation
success/failure, enabled/disabled/break-sentinel/alignment-failure guards,
and empty/head/tail/middle insertion configurations. Both sides run their real
heap insertion routines; alignment, backing allocation and final free have
explicit matching stubs. Compare return value, RAM except original stack
scratch, and ordered boundary-call arguments. This proves caller behavior
relative to those stubs, not the complete allocator chain, actual alignment,
free logic, extender services or register/flag ABI. The tracker stays blocked.
Reproducer: local/heap-alternate-source-20261117/check.py; report.json SHA-256
d64b9ee12b3455dda355be709cc33de8fd7cadf1f066c35b97ae80fecebf95f8.
Tracker/diff checks pass. No gameplay runs.

### 2026-11-17 — Explain blocked rows using a translation snapshot

Status accepts `--translation-report` to cross-reference blocked tracker rows
with generated-unverified, excluded and absent inventory entries, then ranks
shared root blockers by unique affected rows. Overlapping root groups are
explicit; no tracker promotion or freshness/equivalence claim is made. The
current heap-service snapshot yields 368 generated, 432 excluded and 10 absent
blocked rows. `python3 -m unittest tests.test_decomp_status` passes 11 tests,
including overlapping dependencies, duplicate roots, missing inventory entries
and rejection of duplicate report addresses. Tracker and diff checks pass.
No gameplay runs.

### 2026-11-17 — repair DOS write error translation

Auditing high-impact interrupt roots found a semantic mismatch in the existing
0x110393 C draft: both seek and write errors returned the raw service value
instead of the ffffffff returned through 0x1101e8. Preserve the original RCL/ROR
encoding too: write EAX bit31 is replaced by carry. Correct the ABI record:
EBX and EDX are scratch; ECX/ESI/EDI/EBP are preserved. The original preserves
EDX across its error translation and short-write notification helpers.
The corrected function, extracted with declarations and a compile-only service
stub, compiles without diagnostics with Watcom -bt=dos -3r -zp1 -za99 -w1 -we.
96 original-Unicorn/source-host comparisons pass for return EAX and ordered
semantic boundary/helper calls, covering append/no-append, seek/write carry,
zero/error/exact-count/high-bit returns and three DX patterns. Services and
helpers are explicit matching stubs; full register/flag ABI, RAM and actual DOS
behavior are not validated. No tracker promotion and no gameplay runs.
Reproducer: local/dos-write-source-20261117/check.py; report SHA-256
6ed01c81cd8ea3837043402c90f8a713ed3da73bdb09fff29f5522212f2cbf51.
Tracker validation and git diff --check pass.

### 2026-11-17 — Preserve seek interrupt request registers

Correct the existing 0xfa95f draft to retain the upper half of whence in
EAX while replacing AH with 42, and pass the full offset through EDX alongside
ECX=offset>>16. The previous draft narrowed both unnecessarily. Disassembly
and callers bc4da@bc518/f2183@f21d9 also correct the signature record: EBX/EDX
are scratch, ECX/EDI saved and ESI/EBP untouched. The extracted function
compiles without diagnostics using Watcom -bt=dos -3r -zp1 -za99 -w1 -we.
108 original-Unicorn/source-host comparisons pass for return EAX, exact four
interrupt request registers and error-helper arguments: three offsets, three
whence values including upper bits, both carry values, three returned EAX
patterns and two returned EDX patterns. Explicit service/helper stubs and
intercepted host pointer marshalling; actual DOS, full ABI and RAM are unverified.
Reproducer: local/dos-seek-source-20261117/check.py; report SHA-256
03465964b1f4e2ef173af602269e12f71954f52cc5d8a859fd34aa58a47e3119.
Tracker and diff checks pass. No tracker promotion or gameplay runs.

### 2026-11-17 — Validate the DOS error translator cluster

Rename the misleading translate_key_1101e8 to report_dos_error_1101e8 in
source, declarations, callers and symbol records. It stores the low DOS error
byte at 0x484480 and the mapped runtime errno at 0x484484, then returns -1.
Name those globals, the signed-byte mapping table and version control byte.
No original table contents are copied into committed source. The existing
translator logic needs no semantic correction. Actual recovered store and
address helpers run on both sides of 1040 original-Unicorn/source-host
comparisons: all 256 byte codes plus four extended codes, four version values,
and a synthetic table exercising signed bytes. Return EAX and RAM match except
guest stack scratch; no service/helper behavior is stubbed. Host pointer
access and the existing guest_fn aliases are adapted in the local harness.
Full register/flag ABI and splice/replay remain unverified; tracker unchanged.
Watcom -bt=dos -3r -zp1 -za99 -w1 -we compiles key_helpers1.c cleanly.
Reproducer: local/dos-error-source-20261117/check.py; report SHA-256
1af0188f5437d2fa440e251ef69cfb07bd42648a6a2bc2aa0226e20233565e0e.
Tracker and diff checks pass. No gameplay runs.

### 2026-11-17 — Connect file wrappers to recovered error helpers

Replace unresolved 1101e8 aliases in seek 0xfa95f and write 0x110393 with
report_dos_error_1101e8, and use recovered store_through_ebd30 for the write
short-count errno update. This connects those error paths to recovered C.
The extracted seek/write functions compile cleanly with Watcom -bt=dos -3r
-zp1 -za99 -w1 -we (the marshalling helper has an external declaration).
Connected original-Unicorn/source-host comparisons pass 108 seek cases and
96 write cases, using real recovered error translation, error stores and
address helpers on both sides. Return EAX, RAM excluding guest stack scratch,
and service traces match. Only DOS interrupts and write slot lookup are
stubbed; seek host pointer marshalling is intercepted. This strengthens the
previous wrapper-only evidence but does not validate full ABI, actual services
or splice/replay. No tracker promotion or gameplay runs.
Reproducers: local/dos-seek-cluster-20261117/check.py and
local/dos-write-cluster-20261117/check.py. Report SHA-256 respectively:
ffe865d1413bc96940e0e4d97f5847146da2ba4242596c05e20660deab09ca19,
47a360783be88ad2c48213c8fec4c943725eed0b50f1e35decd1db1e4794fad3.
Tracker and diff checks pass.

### 2026-11-17 — Connect file-slot lookup and device query

Use is_device_handle_1101be directly from lookup_slot_11024e instead of its
unresolved guest alias. The extracted lookup compiles with Watcom -bt=dos -3r
-zp1 -za99 -w1 -we without diagnostics. The connected write cluster now executes
recovered lookup, device query, error translation, stores and address helpers;
only DOS services remain stubbed. 128 original-Unicorn/source-host comparisons
pass for return EAX, RAM excluding stack scratch and semantic service traces.
Cases cover append/no-append, seek/write carry, four result values and four
DX device-word patterns including character-device bit 7. IOCTL comparison
includes operation and handle, excluding ECX/EDX: the C query initializes
those registers whereas the original retains scratch values, and the modeled
service ignores them. Thus this is service-relative evidence, not full register
ABI or actual DOS-service equivalence. No tracker promotion or gameplay runs.
Reproducer: local/dos-file-cluster-20261117/check.py; report SHA-256
806fce07c6c8446dfdc1434c0918b87540e1727f0c5928941b1bf33f60d2c9f3.
Tracker and diff checks pass.

### 2026-11-17 — Extend prepared RAM for generated runtime checks

The standard generated-C differential fixture mapped 4 MiB, excluding the
error globals at 0x484480/0x484484. Add explicit prepared_memory_size (default
unchanged), page aligned from 4 to 256 MiB; reject capture resizing, oversized
payloads and an unmapped return stack. This enables full-state comparisons of
the generated runtime cluster instead of relying only on host-C semantic checks.
All 24 tests in tests.test_asm_c_program pass with pinned Unicorn, including
high-global writes and invalid fixture sizes. Five generated bodies (1101e8,
ebe6d, ebe90, ebd30, ebd36) pass 64 standard differential cases with 5 MiB RAM,
four version settings and sixteen error inputs. No service stubs are needed;
the standard harness compares registers, flags, RAM and x87 state. This remains
prepared-input evidence, not trace coverage or splice/replay verification.
Reproducer: local/asm-dos-error-cluster-20261117/check.py; report SHA-256
6b18ee7a3ad2a3f5ec8a0f0d2746af6be06e11cb494b543307ef65b25475f722.
No tracker promotion or gameplay runs. Tracker and diff checks pass.


### 2026-11-17 — Generated DOS file cluster and remaining blockers

Expanded the restricted service-boundary generation snapshot with the audited
DOS seek/device-query/append-seek/write sites fa973, 1101c7, 1103b7 and 1103e0.
Together with the previous nine heap sites, this generates 2,912 bodies covering
516,233 original body bytes: 82 bodies / 8,532 bytes beyond the heap snapshot.
This is direct-edge closure only; dynamic targets and actual services remain open.
Snapshot: `local/asm-file-services-20261117/generated/report.json`, SHA-256
`49135065831f571ece5317a2fd2c95c574cccc07c631e57c8e8adb1e3a70c33b`.

The eight-body write/slot/device/error/store/address cluster compiles as generated
host C and passes 128 standard full-state prepared-input comparisons with 5 MiB
RAM, including exact service request/response traces. Only the three interrupt
sites use explicit register-only service models; these do not model file I/O,
selector changes or DOS memory effects. This closes the earlier scoped source
checks' register/flags/stack comparison gap for this generated cluster, not for
the handwritten recovered C. No tracker promotion or gameplay claim follows.
Command: pinned Unicorn Python running
`local/asm-dos-file-cluster-20261117/check.py`; report SHA-256
`90fc0a9fc610ec720eaf6f2e53e9e04fa0f671d4064bf08571e053b55409e1f3`.

`tools/decomp_status.py status --translation-report` against this snapshot finds
369 of 810 blocked rows generated but unverified, 431 excluded, 10 outside the
inventory. Largest remaining overlapping roots: interrupt at 2b0 (193 rows),
fcos b45a2 (147), fsin b45ac (146), out feec8 (131), in dc2e0 (130), fpatan
dc1e9 (125), interrupt 11124c (119). Service yields expose boundaries; they do
not implement them. Reliable bulk translation still requires those contracts,
unsupported instruction semantics, dynamic-target coverage and equivalence evidence.


### 2026-11-17 — Native observations for trigonometric blockers

The independent original-execution x87 oracle now accepts exact D9 FE (FSIN),
D9 FF (FCOS) and D9 F3 (FPATAN) register encodings. Its existing hardware
FSAVE/FRSTOR execution, pending-exception rejection, uninterrupted-observation
guard and strict state comparison remain in use. No host libm approximation
or translator helper is involved. Unsupported FPTAN remains rejected.

Validation: pinned Unicorn Python `-m unittest tests.test_asm_x87_oracle`, all
eight tests pass. New raw-state checks cover both signed zeros, all eight TOP
positions, FPATAN result placement/pop/tag effects, and FSIN/FCOS range rejection
at 2**63 with unchanged operand and C2 set. This enables independent observations
for the three large trigonometric dependency roots; it does not implement their
C semantics, establish general transcendental equivalence or promote tracker rows.


### 2026-11-17 — Generated file wrappers use the real overlay drive

Added `runtime/asm_dos_file_service.{hpp,cpp}`, a native dispatcher callback
for the four audited seek/write/device-query interrupt sites. It uses the
existing DosDrive backend and the same register/error/flat-address policy as
the hybrid interpreter, including full ECX buffer counts, signed CX:DX seeks,
upper-DX preservation and numeric DOS errors. Unknown requests reject unchanged.
This is host service plumbing, not recovered original extender semantics.

`python3 -m unittest tests.test_asm_dos_file_service` passes: real temporary
file write/seek/device queries, exact guest-state preservation, error bounds,
rejected requests and overlay/original contents. C++17 compilation uses
-Wall -Wextra -Werror. Pinned Unicorn Python running
`local/asm-dos-file-cluster-20261117/native.py` passes four standard full-state
comparisons of the actual generated eight-body cluster, with independent real
DosDrive instances for original/generated runs (append/ordinary writes and
valid/invalid handles). Service buffers are immutable prepared data in these
probes; no general memory-changing service comparison is claimed. Both runs'
exact service traces and final overlay/original files match. Report SHA-256:
`32a4f79388b20d5c53119c24f2216394c2848abac9ef4efe545fba573e0fd9e8`.
The callback remains opt-in for the native runner, not interactive hybrid wiring;
tracker states and original-service fidelity claims remain unchanged.


### 2026-11-17 — Correct recovered DOS close and connect its service

Disassembly of fa9a0 shows MOV AH,3e retaining the rest of the handle in EAX,
RCL/ROR replacing the returned sign bit with CF, and success using returned
EDX as the slot index. Corrected the existing C draft accordingly; a successful
service return with bit 31 set is no longer mistaken for failure. The failure
path now invokes the recovered errno store (value 4) directly. Call sites
bc4da@bc524 and the 115984 tail entry were inspected. Original EBX/EDX saves
and existing signature remain unchanged; arbitrary service clobbers of other
registers are not covered by a full-ABI claim.

Added fa9a8/AH=3e support to the native DOS file callback using DosDrive.close.
The focused regression `python3 -m unittest tests.test_asm_dos_file_service`
passes, including successful close/overlay persistence and repeated-close CF/6.
The extracted recovered close compiles with wcc386 -bt=dos -3r -zp1 -za99
-w1 -we without diagnostics. Pinned Unicorn Python running
`local/dos-close-source-20261117/check.py` passes 64 original/recovered-source
comparisons across high handle bits, CF, sign-bit service results and changed
returned EDX. Actual recovered slot/errno/address helpers are included; only DOS
register responses are modeled. Compared EAX return, RAM except stack scratch,
and request EAX/EBX/EDX. Report SHA-256:
`8a147649f109d02ebb97bc84bdcf13ef470c427b6974405d9ed8e5f85ba5de43`.
Full ABI, original extender services and integration remain unverified; tracker
states are unchanged.


### 2026-11-17 — Recovered read carry encoding and native RAM effects

Corrected `dos_read_10f0e8`: preserve the incoming handle's non-AH EAX bits
when constructing the DOS request, and clear returned bit 31 on success,
matching the original RCL/ROR carry encoding. Confirmed EAX/EDX/EBX inputs
at callers f1e7d and 10f23c; recorded EBX/EDX scratch and ECX preservation.
The existing recovered error helper remains connected.

Added native AH=3f dispatch at 10f0ef using DosDrive, including actual guest
RAM writes, short EOF results and the hybrid runtime's ECX/EDX result clearing.
The focused real-drive test checks exact state, changed buffer extent, EOF,
overflow rejection and preserved file position. Command:
`python3 -m unittest tests.test_asm_dos_file_service` passes. Pinned Unicorn
Python `local/dos-read-source-20261117/check.py` passes 64 recovered-source
comparisons with register-only DOS models: EAX return, RAM except stack scratch,
and request EAX/EBX/EDX; this does not verify real read memory effects against
original DOS. Report SHA-256:
`9cb3c831b35aa4028981f6fbf4429cd64e05dbf4364ab222f3d72a27d9fb199b`.
Extracted read C compiles without diagnostics with wcc386 -q -bt=dos -3r
-zp1 -za99 -w1 -we -i=decomp/include. Full ABI and integration remain open.

The generation snapshot with fa9a8 additionally allowed is
`local/asm-file-close-services-20261117/report.json`: two additional generated
bodies over the previous file-service snapshot. No tracker promotions follow.


### 2026-11-17 — Compare real read service RAM effects

Extended the standard program differential API with explicit `memory_services`.
Service handlers receive disposable current-RAM snapshots; validated register
responses and fixed-size RAM effects commit together. Service traces bind RAM
before/after hashes in addition to exact register requests/responses, including
transient differences later overwritten by guest instructions. Selector/cache
and out-of-RAM mappings remain outside the service API. Existing register-only
handlers retain their contract.

Pinned Unicorn Python `-m unittest tests.test_asm_c_program`: all 25 tests pass.
The added regression covers matching memory writes, differing writes hidden by
later guest overwrites, and both original/translated RAM resize rejection.
Pinned Unicorn Python `local/asm-dos-read-cluster-20261117/native.py`: six
standard full-state comparisons pass for the generated six-body read/error/
store/address cluster with independent real DosDrive instances and mutable
service RAM. Counts 0/3/20 exercise zero-length, partial and short EOF reads;
valid/invalid handles exercise success/error. Compared exact service traces,
RAM, registers, flags and x87 plus overlay/original files. Report SHA-256:
`f5d762354f5de970e09cef7123d9fcaeee89af4ecb33ded83d8f9d25223cc40f`.
These probes now include actual read buffer effects under the hybrid DOS policy;
they do not establish fidelity to the original extender or full integration.
No tracker promotion.


### 2026-11-17 — Generated open/create uses the overlay drive

Audited fa4a9's DOS sites from disassembly and added native dispatch for open
fa4db/fa5df, create fa5b0, cleanup closes fa518/fa553/fa5c4 and zero-count write
fa541. Open/create filename bounds and result registers follow the existing
hybrid bridge. The configured callback carries joystick/digital-sound/CD-music
options through to DosDrive; the earlier DosDrive-pointer callback remains ABI
compatible with all options false. This exposes the hybrid policy, including
its current zero-count write behavior, not a new original-DOS fidelity claim.

`python3 -m unittest tests.test_asm_dos_file_service` passes with warning-clean
C++17 compilation. Added coverage checks both open sites, create/cleanup/write
sites, exact state, unterminated filename rejection and configuration options.
Pinned Unicorn Python `local/asm-dos-open-cluster-20261117/native.py` passes ten
standard full-state comparisons of nine generated bodies, including actual
lookup/device/error/slot/store/address helpers and independent real drives.
Cases cover existing/missing files, flags 0/1/0x21/0x22/0x421, creation/reopen
and exclusive-open rejection. Compared service register/RAM traces, full final
state and resulting overlay/original files. Report SHA-256:
`c1b73905ec590a5e2d4efae30e8e353d8395d1f61bad4150ed570788ebca3891`.

Expanded generation snapshot `local/asm-file-open-services-20261117/report.json`
contains 2,924 generated bodies / 517,486 original body bytes (12 bodies and
1,253 bytes beyond the earlier file-service snapshot). Report SHA-256:
`2f838328a2e88d68114f648aa8912eaf97240c829d2509e2d74cf48ee0fec1dd`.
Closure remains direct-edge-only; interrupt yields alone implement no services.
Interactive wiring, original extender fidelity and tracker verification remain
open; no tracker promotion.


### 2026-11-17 — Preserve delete-file DOS request bits

Audit the shared interrupt frontier: 0x2b0 is the generic register-block DOS
wrapper, not a single service that can safely receive a fabricated response.
In its neighboring file-service closure, correct handwritten delete_file_fe541:
original 0xfe544 MOV AH,41 preserves AL and upper EAX from the filename address.
The prior C sent constant 0x4100. Preserve those bits and retain full EDX=name;
CF alone selects the actual recovered report_dos_error_1101e8 path. Inspect
callers ba5c@bad2 and 42e8c@42ea1 through tail alias cbc9b.

64 original ASM versus recovered C comparisons pass for four address patterns,
both carry outcomes, four returned EAX patterns and two returned EDX patterns.
Compare return EAX, all RAM except stack scratch, and service request EAX/EDX;
this is a register-only DOS model, not full register ABI, filesystem services,
byte-identical recompilation or gameplay evidence. Actual error/errno/address C
is linked. Report local/dos-delete-source-20261117/report.json SHA-256:
954718069a52878ab11d41d33fe96f8d4eb636599c10df6eb54c7aa7c6ca77ce.

OpenWatcom compiles the extracted body with -w1 -we without diagnostics.
Commands: pinned-Unicorn Python local/dos-delete-source-20261117/check.py;
wcc386 -q -bt=dos -3r -zp1 -za99 -w1 -we -i=decomp/include
-fo=local/dos-delete-source-20261117/delete.obj
local/dos-delete-source-20261117/watcom.c;
python3 tools/decomp_status.py check; git diff --check.
Tracker state remains compiles; no verified promotion or new service admission.


### 2026-11-17 — Recover shared open/create core

Recover open_shared_fa4a9 (453 original body bytes) from original assembly,
including leading-space trimming, access/share bits, exclusive-open cleanup,
zero-write truncation, create attributes, created-handle close and reopen,
original error ordering, device queries and handle-table metadata updates.
Preserve AND AL,3c without clearing the upper three slot bytes, permission XOR
before changing AH, carry encoding and reopen EAX high bits from the close.
Inspect both cdecl callers fa485@fa49e and bc287@bc2fe; record the four stack
arguments, caller cleanup and saved registers. Name creation-permission mask
0x17d5bc; default translation mode 0x16062d was already recorded.

220 scoped original ASM versus actual recovered C cases pass. Link actual
lookup, slot store, device query, error translation and errno/address helpers.
Cases vary flags, file existence, device status, create/close/reopen/write
failure, mode, share, permission mask and preexisting slot bits. Compare EAX,
all RAM except stack scratch, exact service request EAX and service-specific
DOS input operands. This is a register-only DOS model: ignored service inputs,
all registers/flags and the zero-write guest stack pointer are not proven.
No binary identity, real filesystem service or gameplay claim. Report:
local/dos-open-source-20261117/report.json SHA-256
353a8e21dd0890302de1e1d7a9ac518503319d97d1f97dcc21b4010edb41a136.

The complete misc_drafts1 module compiles with zero warnings/errors through
tools/ef2000_wcc.py, as does the extracted body with -w1 -we. Move tracker
blocked -> compiles, with explicit broad-hybrid exclusion until remaining
ABI/service and splice/replay evidence exists. Tracker: 3423 compiles,
809 blocked. Commands: pinned-Unicorn Python
local/dos-open-source-20261117/check.py; tools/ef2000_wcc.py
decomp/src/misc_drafts1.c --compiler <pinned-wcc386>
--output local/dos-open-source-20261117/misc_drafts1.obj;
python3 tools/decomp_status.py check; git diff --check.


### 2026-11-17 — Recover CR/LF translating handle writer

Recover write_handle_10f26a (465 original body bytes), previously an unnamed
blocked runtime entry. Record EAX=handle, EDX=source, EBX=count from the sole
direct inventory caller f1f5a@f1fef. Preserve saved ECX/ESI/EDI/EBP and treat
EBX/EDX as scratch. Implement invalid/nonwritable slot errors, append seek,
binary writes, stack-availability thresholds 0xb0/0x230, 128/512-byte dynamic
stack buffers and CR insertion before each LF. Keep original carry encoding,
short-write errno and arithmetic: add the latest output count to the input
count committed at the preceding complete chunk, including CR-boundary cases.
Do not replace that with a conventional count of successfully written input.
Call the original 0x11d695 exhaustion path rather than inventing a return.
Watcom __alloca avoids adding a second stackavail check or __GRO callback;
host compilation uses __builtin_alloca.

1134 original ASM versus actual recovered source comparisons pass across
seven slot patterns, three stack budgets, nine input streams and six seek /
short-write / write-error modes. Actual lookup, device-query, error and
errno/address source is linked. Compare EAX, nonstack RAM and actual DOS write
byte streams; direct-buffer/seek requests retain exact EAX, text-buffer request
addresses and AX are normalized because stack placement differs. Local host
shim reconstructs nearby host stack pointers solely to observe those bytes.
Stack availability is modeled; low-stack fatal behavior, stack RAM/pointers,
full register/flag ABI, real DOS services, binary identity and gameplay remain
unproven. Evidence: local/dos-text-write-source-20261117/report.json SHA-256
 e39bcc0d32df0d14d8515a9e4a712107d525ef77830abe2121337ffdd44ce47b.

Whole misc_drafts1 compiles with zero OpenWatcom warnings/errors. Mark the
entry compiles with explicit broad-hybrid exclusion, not verified. Tracker:
3424 compiles, 808 blocked. Commands: pinned-Unicorn Python
local/dos-text-write-source-20261117/check.py; tools/ef2000_wcc.py
decomp/src/misc_drafts1.c --compiler <pinned-wcc386>
--output local/dos-open-source-20261117/text-write-module.obj;
python3 tools/decomp_status.py check; git diff --check.


### 2026-11-17 — Translate the generic intdos file-service path

The original 0x2b0 register-block gateway needs more than an INT boundary:
its 0x30e/0x312 stores use operand-size plus FS prefixes (66 64 8c).
Extend checked segment-MOV prefix handling to retain explicit segment overrides,
including FS, rather than dropping the memory access segment. Reject mismatched
textual overrides. Cover both original store forms in synthetic ASM/C comparisons,
including descriptor-backed FS accesses with a valid captured FS cache.

Admit the existing native file-service policies at gateway interrupt site 0x2f0:
create, open, close, read, write, seek and IOCTL 4400 only. Other requests remain
rejected without state or filesystem changes. Do not fabricate success for
extender requests such as 252b/3508. This is the current hybrid DosDrive policy,
not a claim of complete original DOS behavior (zero-byte writes still follow
that policy). Keep the hand-written intdos draft and hybrid exclusion unchanged.

Both original gateway functions (0x2b0, 0x9be0d) now generate C. Forty-two local
original ASM versus generated C comparisons pass: seven admitted requests with
valid/invalid handles and separate/identical/overlapping input-output blocks,
using independent real copy-on-write drives. Compare
registers, flags, supported x87 state, complete prepared RAM including stack
scratch, exact service traces and overlay output; source files stay unchanged.
The output carry word at +0x18 remains untouched because the actual gateway
runs on both sides. Evidence: local/asm-intdos-files-20261117/native-report.json,
SHA-256 27573e6026c64a9836411651f796fa1837d1a10b9a35dd27e21a7b683af091c4.
This proves the scoped gateway translation under that service adapter, not all
193 dependent blocked functions, arbitrary services or gameplay. Tracker states
remain unchanged. All 28 targeted automated checks pass. Commands: pinned-Unicorn Python
local/asm-intdos-files-20261117/native.py;
python3 -m unittest tests.test_asm_dos_file_service;
pinned-Unicorn unittest tests.test_asm_c_program.ProgramTests.test_data16_and_register_segment_prefixes;
python3 -m unittest tests.test_asm_to_c.TranslationTests.test_data16_aliases_reject_unsupported_encodings;
python3 tools/decomp_status.py check; git diff --check.


### 2026-11-17 — Recover chunked copy and its stack ABI

Recover copy_blocks_e9a24 (132 contiguous original bytes) in block_copy.c.
Audit callers a2e90@a2ea7 (33-byte option copy) and d663c@d6662 (768-byte
palette copy): stack arguments count/source/destination, callee RET 12,
EAX advanced destination, preserved EBX/ESI/EDI/EBP and scratch ECX/EDX.
Record the canonical signature and tighten the existing guest alias's clobbers.
Keep the original 16-byte preload/store groups within each 32-byte block,
8-byte groups and byte tail. These differ from memcpy/memmove for overlaps.
Retain unsigned address/count wrapping and signed branch predicates, including
ADD overflow: the tail predicates compare signed remaining against -32/-8.
Do not normalize negative sizes into conventional copy semantics.

The complete module compiles with zero OpenWatcom warnings/errors. In 2220
cases, compare both actual host-compiled recovered C and the emitted Watcom
object against original ASM. The object has no externals or relocations;
execute its actual public code in a separate Unicorn guest. Compare EAX and
complete RAM except code placement and stack scratch; the object additionally
matches original EBX/ESI/EDI/EBP and ESP after RET 12. Cases span counts 0..96,
127/128/129, 255/256/257, 511/512/513, 768, four negative count patterns, and
20 overlapping/separate address offsets, with unaligned source/destination.
Extreme negative counts 0x80000000..0x8000001f follow the recovered arithmetic
but are not executed: they can enter enormous copy loops. Scratch registers,
flags and guest stack scratch are excluded; no binary identity or gameplay
claim. Evidence: local/block-copy-source-20261117/report.json SHA-256
42ebfe52d150545594e194cc8ba8e9d7c6c207370752ac89747b37d49871a995.

Move tracker blocked -> compiles; 3425 compiles, 807 blocked. Update the
existing broad-hybrid exclusion to reflect the manual ABI recovery while
retaining the earlier menu fault history and remaining verification limits.
Commands: tools/ef2000_wcc.py decomp/src/block_copy.c --compiler <pinned-wcc386>
--output local/block-copy-20261117.obj; pinned-Unicorn Python
local/block-copy-source-20261117/check.py; python3 tools/decomp_status.py check;
git diff --check.


### 2026-11-17 — Recover menu name-list updater

Recover refresh_menu_name_list_347bc (136 original bytes), formerly an
unattempted blocked menu entry. The sole direct inventory caller is
345e8@3475b: load EAX=root from [ESI+0x51], ignore returned scratch EAX.
The body saves EBX/ECX/EDX/ESI; audited lookup/format/show callees preserve
EDI/EBP. Record the signature and explicit EAX-only clobber pragma.

Call the existing recovered checked widget lookup, variadic formatter and
text/show helpers with their concrete guest ABIs. Format the identifier at
0x1357c0 into a 16-byte local buffer, then visit up to four following widgets
using optional values at 0x13577c. Zero values produce empty text. Preserve
next-link reads after every show, including the final one, so callback-driven
list changes remain visible. Record both globals and a partial packed widget
layout with compile-time proof of the +0x21 sibling field.

Whole module compiles with zero Watcom warnings/errors. In 336 scoped cases,
run original ASM and the actual relocated Watcom body against identical
explicit lookup/formatter/show ABI models. Vary chain length 1..7, eight zero
patterns, three identifier bit patterns and callback mutation. Match call
traces/text, preserved registers, ESP and complete RAM except code placement
and stack scratch. This proves body ordering under those models, not the
actual widget/formatter callees. Scratch EAX, flags and stack scratch are not
compared; no gameplay, binary identity or splice/replay claim. Evidence:
local/menu-name-list-source-20261117/report.json SHA-256
634a572c7de829962d50e8e118e155eb6bdee4f62805a3b25e1950000fdf48e6.

Move blocked -> compiles, keeping an explicit broad-hybrid exclusion.
Tracker: 3426 compiles, 806 blocked. Commands: tools/ef2000_wcc.py
decomp/src/menu_name_list.c --compiler <pinned-wcc386>
--output local/menu-name-list-20261117.obj; pinned-Unicorn Python
local/menu-name-list-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover session shutdown 0x34d94

Recover the 131-byte menu session shutdown in session_shutdown.c. Inspect
original ASM and both direct callers (34f79, 352c7): no consumed arguments,
return ignored. Preserve ownership-flag clearing before each stack-argument
close and re-read the second flag and handle after the first close. Record
signature and globals. Retain the hybrid exclusion.

Whole-module Watcom compilation has zero warnings/errors. In 64 scoped cases,
original ASM and emitted Watcom code match call traces, nonstack RAM, preserved
registers and ESP under explicit callee ABI models. Cover both device flags,
both ownership flags, and first-close mutations of the second flag/handle.
Models preserve registers and do not execute actual callees. EAX scratch,
flags and stack scratch excluded; no binary identity or splice/replay claim.
Evidence local/session-shutdown-source-20261117/report.json SHA-256
53fc149141a369db03f33370cab3f7411011de1de6fad83966ba65071c26c0a6.
Move blocked -> compiles: 3427 compiles, 805 blocked.
Commands: tools/ef2000_wcc.py decomp/src/session_shutdown.c --compiler
<pinned-wcc386> --output local/session-shutdown-20261117.obj;
pinned-Unicorn Python local/session-shutdown-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover network mission name callback 0x34844

Recover 160-byte callback with EAX widget input; original pointer at 0x12b5c0,
no direct inventory callers. Establish packed widget text field +0x27 with
compile-time offset check. Call the validator even on empty input; read the
widget text pointer again after validation and during the leading-class scan.
Keep byte-wrapped character-table indexing. Preserve the original trailing
scan starting at the NUL rather than silently fixing whitespace handling.

Watcom compilation clean. Sixteen original/emitted-body scoped comparisons
match call traces, nonstack RAM, saved registers and ESP with explicit callee
models. Cases include empty, whitespace, high bytes, length 16/17 and validator
mutation of the widget text pointer. Uses original character-class table.
EAX/ECX, flags, stack scratch and actual callees are outside the proof; no
binary identity or splice/replay claim. Keep explicit hybrid exclusion.
Evidence local/network-mission-name-source-20261117/report.json SHA-256
e2bd097a676179835f9ea90f185fd96ad9a50a4300e3d2a778bc988b66d57142.
Tracker: 3428 compiles, 804 blocked. Commands: tools/ef2000_wcc.py
decomp/src/network_mission_name.c --compiler <pinned-wcc386>
--output local/network-mission-name-20261117.obj; pinned-Unicorn Python
local/network-mission-name-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: refresh bulk frontier after intdos file-service recovery

Regenerate the full current body inventory with the previous file-open
snapshot's interrupt-site set plus 0x2f0 (generic intdos). Current data16 FS
stores now translate the gateway. This produces 3,032 generated-unverified
bodies / 544,308 original body bytes, up 108 bodies / 26,822 bytes from the
previous snapshot. All 108 newly emitted bodies pass host C99 syntax checking
with implicit function declarations treated as errors. This is generation and
compilation evidence only; no new equivalence or tracker promotion.

Snapshot: local/asm-intdos-frontier-20261117/report.json, SHA-256
e1b596044881b4ce896cb31d844476b1a05f5043529f194a1c649e31e25840ad.
Compilation record: same directory/new-body-compile-report.json, SHA-256
0a8eef6047753ab05e1b6bba3a7b3f41d70bff14edb8333ac55fe0bee0b47f42.
Command: tools.asm_c_program.generate(current inventory, fresh output, original
payload, interrupt_boundaries=True, interrupt_sites=previous sites | {0x2f0});
cc -std=c99 -Werror=implicit-function-declaration -fsyntax-only -I<output>
<each newly emitted body>. All generated source remains ignored under local/.

Against the current 804 blocked rows, 401 / 224,556 bytes have generated C,
393 / 232,935 bytes remain excluded and 10 / 451 bytes are outside inventory.
Leading overlapping blockers: FCOS 147, FSIN 146, OUT 129, IN 128, FPATAN 125,
X-32 wrapper 0x11124c 119, delete interrupt 0xfe541 78, 0x11d63f 70,
writer interrupt 0x10f26a 56, F2XM1 47. Removing the intdos translation barrier
alone does not close all its callers' other dependencies. Service yields
remain requests: the runtime gateway accepts only its file-service whitelist;
other requests still reject. Direct-edge closure does not establish indirect
coverage, full runtime services or original behavior. This replaces the older
snapshot for deciding the next shared instruction/service work; tracker states
remain unchanged.

### 2026-11-17: recover serial settings display 0x337ec

Recover the 156-byte EAX-root callback in serial_settings.c. Confirm input
from original ASM and callback table pointer at 0x12b628 (no direct inventory
callers). Locate socket_txt and channel_txt, format socket zero/ffff as ----
and other words as %04x, then format the unsigned channel byte with %hd.
Read the socket after both lookups and the channel after the first show.
Record globals and signature; retain explicit hybrid exclusion.

Watcom compilation clean. Sixty original/emitted-body comparisons match call
traces/text, complete RAM excluding code placement and stack scratch, saved
registers and ESP under explicit lookup/formatter/show models. Cover six socket
values, five channel values including 128/255, and first-show channel mutation.
Actual callees, scratch EAX, flags and stack scratch are outside this proof;
no binary identity or splice/replay claim. Report:
local/serial-settings-source-20261117/report.json SHA-256
f4236ca08af8577933c09c194d39399c46d2bf81d7239e05d927d939f24ad797.
Tracker: 3429 compiles, 803 blocked. Commands: tools/ef2000_wcc.py
decomp/src/serial_settings.c --compiler <pinned-wcc386>
--output local/serial-settings-20261117.obj; pinned-Unicorn Python
local/serial-settings-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover modem settings display 0x33888

Recover the 228-byte EAX-root callback in modem_settings.c. Inspect original
ASM, callback pointer at 0x12b630 and baud helper 0xdf2b0 preservation (no direct
inventory callers). Display port index+1 without byte wrap, rate, FIFO mode
text and selected FIFO length or static '-'. Preserve formatting before each
lookup and re-read FIFO mode/index after the final lookup; do not add index
bounds checks absent from the original. Record globals and signature.

Clean Watcom compile. In 162 original/emitted-body scoped comparisons, match
traces/text, saved registers, ESP and complete RAM except code placement and
stack scratch. Explicit callee models cover three port values including 255,
three FIFO modes, three length indices, rates 0/9600/-1 and lookup mutation
that toggles FIFO enabled state and changes length index. Actual callees,
scratch EAX, flags and stack scratch remain outside the proof; no binary
identity or splice/replay claim. Retain hybrid exclusion. Evidence:
local/modem-settings-source-20261117/report.json SHA-256
7851611c2290d3a7417b7c46a866cb2a1c20b3adaa62e50288e778e5579e8c99.
Tracker: 3430 compiles, 802 blocked. Commands: tools/ef2000_wcc.py
decomp/src/modem_settings.c --compiler <pinned-wcc386>
--output local/modem-settings-20261117.obj; pinned-Unicorn Python
local/modem-settings-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover network service list 0x340a4

Recover the 153-byte EAX-root callback (pointer slot 0x12bb80; no direct
inventory callers). Populate linked rows from 65-byte name slots beginning
0x428cf8. Re-read signed count each iteration and next link after every show,
including the last. Preserve the unchecked list traversal, 16-bit content
height and signed 16-bit variable dialog height; counts >=5 select the fixed
size and enable child controls. Add asserted widget content-height offset
+0x67 and record count global/signature. Keep hybrid exclusion.

Watcom clean. Twenty-one original/emitted-body comparisons under explicit
lookup/show/resize/toggle models match call traces, saved registers, ESP and
all RAM except code placement/stack scratch. Cover counts 0/1/4/5/6 and two
negative patterns, plus first-show changes to count and sibling link. Actual
callees, EAX scratch, flags and stack scratch remain unproved; no binary
identity or splice/replay claim. Report:
local/network-service-list-source-20261117/report.json SHA-256
6c5b09ca755549472cf814d68115e07a64b6b681e6aaec2c64a79e61b1f74068.
Tracker: 3431 compiles, 801 blocked. Commands: tools/ef2000_wcc.py
decomp/src/network_service_list.c --compiler <pinned-wcc386>
--output local/network-service-list-20261117.obj; pinned-Unicorn Python
local/network-service-list-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover provider selection 0x34140

Recover 151-byte EAX selection-record callback (table pointer 0x12bb88; no
direct inventory callers). Assert inline number-text offset +0x3f. Preserve
unchecked parsed index minus one, conditional provider-open call, checked
window/widget lookup, name show, selected flag, fresh index read for name
copy and final dialog action. The shown name is selected before the show;
the stored name uses the index after show. Record signature/globals.

Scoped comparison exposed generated ABI inference putting guest_fn_e1744's
argument on stack despite original call-site EAX setup. Add explicit EAX/AL
pragma with conservative EAX/ECX/EDX clobbers. Inspect original e1744: it is
an unsupported-operation error stub and does not consume EAX. Preserving the
call-site register contract does not implement DirectPlay. Recompile affected
runtime_helpers28.c as well as new module: both clean Watcom.

Twenty-four original/emitted-body comparisons under explicit parser, provider,
error, lookup, show, copy and final-action models match traces, saved EBX/EDX,
ESP and all RAM except code placement/stack scratch. Cover parsed values
0/1/3, both query/open outcomes and show-driven selected-index mutation. Error
model deliberately returns and normalizes helper-register versus original
cdecl message passing; actual fatal behavior and actual callees are not
proved. Other registers, flags and stack scratch excluded. No binary identity
or splice/replay claim; retain exclusion. Report:
local/network-service-open-source-20261117/report.json SHA-256
157f4fb7490e84874a05d2755f59020d8469b6781cfc3fa7a6513135536b6849.
Tracker: 3432 compiles, 800 blocked. Commands: tools/ef2000_wcc.py on
network_service_open.c and runtime_helpers28.c with pinned compiler;
pinned-Unicorn Python local/network-service-open-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover socket change callback 0x33ee8

Recover 208-byte EAX-widget callback (table pointer 0x12b590; no direct
inventory callers). Validate at most four characters using byte-wrapped
character-class indices, reject empty input, then invoke cdecl scanner %x.
Preserve low-byte scanner-result comparison and full-width zero/ffff/current
socket checks before the 16-bit store. Values above ffff remain accepted
and truncate exactly as originally; retain clear-session/shutdown/store/write
ordering and unconditional network refresh. Reuse asserted widget text offset.

Watcom clean. In 252 original/emitted-body scoped comparisons, explicit
parser/session models match traces, saved registers, ESP and RAM except code
placement/stack scratch. Cover seven text patterns (including invalid fifth
character and high byte), six parsed values through ffffffff, scanner results
0/1/101 and two current sockets. Parser results deliberately vary independently
of text to check body control flow; this does not validate the actual scanner.
Actual callees, EAX scratch, flags and stack scratch unproved; no binary
identity or splice/replay claim. Retain exclusion. Report:
local/network-socket-settings-source-20261117/report.json SHA-256
da848a68fd347eee3dbc2942434cc222b4ddd4488e4d8594a6bf933b3ae5364d.
Tracker: 3433 compiles, 799 blocked. Commands: tools/ef2000_wcc.py
decomp/src/network_socket_settings.c --compiler <pinned-wcc386>
--output local/network-socket-settings-20261117.obj; pinned-Unicorn Python
local/network-socket-settings-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: close generated intdos file-flush service gap

Allow DOS AH=68h at the existing 0x2f0 intdos gateway in the generated-state
file adapter. Match runtime/unicorn_cpp.cpp's hybrid policy: low-word BX
selects the handle, console handles 0..2 succeed, successful flush preserves
EAX, invalid handles return error 6 with CF, and other flags/registers/RAM
remain unchanged. Flush uses the existing copy-on-write DosDrive implementation;
original assets remain untouched. No additional interrupt sites are admitted.

The real-drive C++ test checks five handles and both incoming CF states,
full state/RAM preservation, overlay contents before close, preserved file
position and open-handle lifetime, and rejection at a different interrupt site.
This is adapter-policy evidence, not original DOS equivalence or a bulk
function promotion; tracker counts remain unchanged. Commands:
python3 -m unittest tests.test_asm_dos_file_service (PASS);
python3 tools/decomp_status.py check (4293 functions); git diff --check.

### 2026-11-17: expose transitive service requirements in bulk reports

Add reachable_host_service_sites to each generated function report entry.
Compute a conservative fixed point over translated direct dependencies,
including cycles, recording interrupt instruction addresses. Report scope
explicitly excludes unknown dynamic targets/request numbers and does not
claim implemented services or equivalence. Existing generated C is unchanged.

Refresh private snapshot local/asm-service-reachability-20261117/report.json
using the preceding frontier's admitted interrupt sites. SHA-256:
e07411ebc4fc6d6f84a0beb3f4a7877b7e4f983ad433930e588c86494b02d5fa.
Of 3032 generated functions, 443 reach admitted service sites through direct
edges. Of the 399 currently blocked yet generated functions, 90 have these
service dependencies. The other 309 still need equivalence evidence and may
have dynamic/backend dependencies; absence of direct service requirements
is not evidence of completeness. No tracker promotions.

Validation: pinned-Unicorn Python -m unittest tests.test_asm_c_program:
26 PASS, including recursive service propagation, mixed DOS/DPMI vectors,
unsupported-body exclusion and emitted report fields. Full original-inventory
generation succeeds. python3 tools/decomp_status.py check and git diff --check
pass. No game/UI tests or binary-identity claims.

### 2026-11-17: show generated service dependencies in project status

Extend status --translation-report (also forwarded by nix run .#status) with
three counts for generated blocked functions: reachable interrupt sites, no
recorded reachable sites, and unmeasured by older reports. Keep absent evidence
separate from an explicitly empty site list. Exclude nonblocked tracker rows
and explain that direct-edge reachability cannot prove service support.
Document usage in README. The current private reachability snapshot reports
90/309/0 respectively. No tracker changes or equivalence claims.

Validation: python3 -m unittest tests.test_decomp_status (12 PASS), including
old-report compatibility and exclusion of nonblocked functions; status against
local/asm-service-reachability-20261117/report.json; tracker check; diff check.

### 2026-11-17: translate scalar port operations through explicit boundaries

Add opt-in --port-boundaries and repeatable --port-site to the bulk translator.
Validate exact scalar IN/OUT opcode, prefix, operand and length combinations
for AL/AX/EAX with DX or immediate ports. Reject ambiguous/repeated forms.
Yield without changing registers, flags or RAM; PC advances to the checked
continuation. The service runner forwards original IP and an encoded request
(opcode and instruction length), charges one shared fuel unit and propagates
service errors/faults. Document the callback contract in its generated header.
Host code must implement device effects and IN's partial accumulator update;
no device behavior or architectural I/O privilege handling is fabricated.

Record required port requests and scope in host_port_service_boundaries.
The general differential verifier refuses these programs, just as it refuses
unserviced interrupt programs. Default translation continues rejecting ports.
This enables C composition across port instructions but does not establish
that the corresponding game functions can run correctly.

Pinned-Unicorn test suite: 28 PASS. Across twelve scalar encoding forms,
compare explicit modeled port callbacks against original Unicorn execution:
port/width/value traces, GPRs, flags and full RAM match. Also check raw yield
state preservation, callback IP/status, partial accumulator updates, fuel,
site restriction, malformed encodings and verifier refusal. Actual devices,
privilege faults, timing and whole-function equivalence remain unproved.

Private full-inventory snapshot local/asm-port-frontier-20261117/report.json
SHA-256 305bae590910f8bc03b37f0aa91f6cb974612e27f95f9a866ea56b856600b145:
3219 generated bodies / 571821 original body bytes, with 57 port requests;
187 more bodies than the preceding 3032-body frontier. All 187 new bodies pass
cc -std=c99 -Werror=implicit-function-declaration -fsyntax-only. Compile report
local/asm-port-frontier-20261117/new-body-compile-report.json SHA-256
cb2b1d9d099469c19a96ad3046736a35338a47c9730c2e4a12a2fbd35dc745a5.
Snapshot uses preceding admitted interrupt sites and all scalar port sites;
this is syntactic coverage with explicit service obligations, not verification.
Tracker unchanged. Commands: pinned-Unicorn Python -m unittest
tests.test_asm_c_program; full inventory generate(..., port_boundaries=True);
new-body syntax checks; python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: share VGA DAC policy with generated port execution

Extract the hybrid's existing 3c8/3c9 DAC write behavior into runtime/vga_dac.hpp
and use it from Machine::port_out. Preserve AL index selection, six-bit data
masking, 768-component wrap and advance-after-successful-store ordering.
Add an explicit generated-state port service adapter that validates the request
encoding, continuation, port and palette bounds before changing anything.
Accept only scalar OUT to 3c8/3c9; reject IN, other ports, malformed requests and
invalid spans with 0x122 unchanged. The caller supplies the mapped palette
address: hybrid DOS palette storage is outside payload RAM, so do not silently
substitute a payload address. Generated callers must wire this adapter explicitly.

Pinned-Unicorn Python -m unittest tests.test_asm_vga_port_service: 2 PASS.
Native C++ checks cover 780 writes and all accumulator widths, mask/wrap,
register/flag preservation, high DX/AX bits, invalid spans/continuations/requests
and failed-store cursor preservation. An emitted-C 780-component loop using the
actual adapter matches original instruction execution under the prior hybrid
DAC policy in all GPRs, flags and full RAM. This is shared-policy evidence,
not real VGA hardware fidelity, replay verification or function promotion.

Also pass tests.test_asm_dos_file_service, tracker check and diff check.
Hybrid runtime passes c++ -std=c++17 -Wall -Wextra -Werror -fsyntax-only with
installed Unicorn/OpenSSL includes (non-SDL configuration). No desktop/UI tests.
Tracker unchanged; palette adapter does not implement retrace/keyboard/gameport.

### 2026-11-17: share sampled retrace clock with generated execution

Extract the hybrid's sampled/cached 3da retrace policy into VgaRetrace and use
it in Machine::port_in, preserving per-slice first-read sampling, profiling
clock counts, host-clock sampling and the replay clock's 100000-ns increments.
Add ef_asm_vga_retrace_service for scalar IN from 3da. Preserve high EAX bits
for AL/AX inputs, all other guest state and RAM. Caller selects sampling interval
and clock, and controls slice resets explicitly. Reject unknown requests and
zero intervals unchanged. Extract scalar request validation into a common
header used by DAC and retrace adapters, keeping DAC behavior unchanged.

Native tests cover 4800 reads across intervals 1/4/4096, every accumulator
width, slice reset, host-clock caching, the 15000000 threshold and 16666667
period, and rejection without clock advancement. Existing generated palette
loop still passes. Commands: pinned-Unicorn Python -m unittest
tests.test_asm_vga_port_service (2 PASS); non-SDL hybrid c++ syntax check with
-Wall -Wextra -Werror; tracker check; diff check.

Private original-body comparison additionally generates the actual 49-byte
polling routine at dc2e0 from validated inventory and runs its emitted C with
the actual native retrace adapter. In 36 cases (intervals 1/4/64, six initial
sample phases, two flag sets), original Unicorn execution under the preceding
hybrid clock policy matches all GPRs, flags, PC, full RAM including stack, and
input traces. No scratch exclusions. Report:
local/asm-retrace-original-20261117/report.json SHA-256
a6e7cacc09603e3b591de8b9b7fb24e5ff6561502d7b46df776d5b03c81b316b.
Command: pinned-Unicorn Python local/asm-retrace-original-20261117/check.py.
This establishes the sampled hybrid policy for this body, not real-device
fidelity, all initial states, replay/splice evidence or byte identity. Tracker
unchanged. Keyboard/gameport and other unsupported ports still reject.

### 2026-11-17: reusable service-aware port differential comparisons

Extend asm_c_differential.compare with explicit port_factory, constructing
independent original/translated device instances. Forward normalized port,
direction, width and instruction IP; compare full request/response traces in
addition to existing GPR/flags/segment/x87/RAM checks. Support disposable RAM
snapshots and before/after hashes when memory_services is selected. Validate
that IN only changes its addressed accumulator bits and OUT changes no GPRs or
flags; reject invalid/resizing responses before committing RAM. Original
service failures are inconclusive, translated failures after an original return
are mismatches. Stop rejected Unicorn IN callbacks explicitly with an integer
return to avoid ctypes swallowing an exception as an undefined port value.

Pinned-Unicorn Python -m unittest tests.test_asm_c_program: 29 PASS. New tests
exercise all twelve scalar forms with/without RAM effects, trace equality,
divergent per-side responses and illegal unrelated-register mutations. Existing
interrupt, dispatch, fuel, x87 and memory-service checks still pass. Tracker
and diff checks pass. This is explicit-model comparison evidence; no default
hardware model is supplied and the general verifier still refuses service
programs without an appropriate driver. No tracker promotions or byte-identity
claims. Reusable compare API replaces the need for bespoke port tracing in
future original-body checks.

### 2026-11-17: correct outp ABI and check actual palette source

Audit original 117b0c: save EBX, copy EAX to EBX, replace AL with DL,
replace EDX with EBX, OUT DX,AL, restore EBX, RET. Previous signature wrongly
listed EDX preserved. Remove it and explicitly declare EAX/EDX clobbers for
both canonical outp_117b0c and guest_fn_117b0c alias. Preserve ECX and all other
GPRs. Direct callers 117bc4 and 117d2a do not consume scratch outputs; they
preserve outer EDX on their own stacks. This avoids generated call contracts
assuming a value survives in EDX when the original overwrites it.

Compile game_helpers125.c and misc_drafts1.c with pinned Watcom: zero warnings.
Compare actual Watcom upload_vga_palette_feec8 body to the original 94-byte
body, executing the actual original 117b0c helper on the compiled side, with
port callbacks implementing prior hybrid DAC policy. Six scenarios cover
identical/all-changed/first-only/last-only/alternating/gapped palette changes.
Port/width/value traces, saved GPRs, ESP and all RAM except code placement and
stack scratch match. EAX scratch, flags and stack scratch are excluded; no
splice/replay, universal match or hardware-fidelity claim. Source remains
compiles; annotate tracker evidence without promotion. Both before and after
ABI correction pass. Private report:
local/palette-watcom-source-20261117/report.json SHA-256
25db57b6ea31cf30354051b18b94d89af3c4c7651626b61d16c2115aa6d68690.
Commands: tools/ef2000_wcc.py on both modules with pinned compiler;
pinned-Unicorn Python local/palette-watcom-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.


### 2026-11-17 — transitive scalar port service requirements

Generated entries now record reachable_host_port_service_sites separately from
interrupt requirements, using the same fixed-point direct-edge closure through
recursive call cycles. Unsupported bodies have no requirement certificate;
dynamic targets and runtime DX port values remain unknown. Empty interrupt
requirements alone must not be interpreted as absence of host services.

All 30 tests in tests.test_asm_c_program pass with pinned Unicorn, including
recursive port dependency propagation, isolated service-free entries and
site restrictions excluding unsupported callees. Tracker validation and
 git diff --check pass. No tracker promotion or device-equivalence claim.

Private full-inventory report local/asm-port-reachability-20261117/report.json
SHA-256 47f0fa3a4a672f68932e729b5c7908f2e2a574d20272e3a20b756822089ed756:
3599 generated entries / 656290 body bytes, 248 with reachable port sites.
This run admits ALL INT 21h/31h sites as external service boundaries; its
larger count is not a gain over the prior restricted-service frontier and
does not mean those services are implemented. Generated bodies remain unverified.
Commands: tools/asm_c_program.py --inventory local/asm-latest-body-inventory-20261117
--workspace local/ghidra-offset-tail-20261117 --output local/asm-port-reachability-20261117
--interrupt-boundaries --port-boundaries; pinned-Unicorn Python -m unittest
tests.test_asm_c_program; python3 tools/decomp_status.py check; git diff --check.


### 2026-11-17 — recover network channel callback 0x344bc

Recover 206-byte EAX-widget callback change_network_channel_344bc, registered
at table slot 0x12b5a0 with no direct inventory callers. Original ASM and
private pseudo-C read; original is authoritative. Saves EBX/ECX/EDX/ESI,
normal callees preserve EDI/EBP, EAX scratch. Reuse asserted widget text offset.
Validate at most three characters via byte-wrapped class index and mask 0x22;
reject empty input, test scanner AL for success, require signed parsed value
1..255 and difference from current channel. Preserve clear-session, shutdown,
byte store, settings write order and unconditional network refresh.

Pinned Watcom compile: zero warnings. 432 original/emitted-body comparisons
with explicit parser/session models match call traces, six saved GPRs, ESP,
and RAM excluding code placement and stack scratch. Cover empty/valid/invalid
and high-byte text, ignored fourth character, signed range extremes, scanner
results 0/1/101 and current channel 1/255. Actual scanner and device callees,
EAX scratch, flags and stack scratch remain unproved; no byte identity,
splice/replay or universal equivalence claim. Keep hybrid exclusion. Tracker:
3434 compiles, 798 blocked. Private report
local/network-channel-settings-source-20261117/report.json SHA-256
72ff9dc06ce22175d579d1b438eeabbe04accb3b31e62b0a250355606d04d321.
Commands: tools/ef2000_wcc.py decomp/src/network_channel_settings.c --compiler
<pinned-wcc386> --output local/network-channel-settings-20261117.obj;
pinned-Unicorn Python local/network-channel-settings-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.


### 2026-11-17 — recover serial status refresh 0x33b5c

Recover 244-byte refresh_serial_status_33b5c after reading original ASM and
private pseudo-C. EAX root confirmed by two direct call sites 3508f/350c4,
both passing selected widget and ignoring return, plus callback slot 12b668.
Original saves EBX/ECX/EDX/ESI; normal callees preserve EDI/EBP; EAX scratch.
Preserve stage-change formatting into a 64-byte local, pending command/reply
updates and pending flags retained when pointer is null. Reply scan ignores
NUL, stopping at CR/ff or 31 bytes; retain original behavior, pointer store
before lookup, reload after show and final unconditional pointer store.

Pinned Watcom compile: zero warnings. 144 scoped original/emitted-body
comparisons with formatter/find/show models pass: changed/unchanged stage,
clear/pending/null command and reply, CR/ff/embedded NUL/overlength text,
and callbacks mutating the reply pointer. Compare normalized traces (local
formatted text by content), six saved GPRs, ESP and nonstack RAM excluding
code placement. EAX scratch, flags, stack scratch and actual callees remain
unproved; no binary identity, splice/replay or universal-equivalence claim.
Keep hybrid exclusion; tracker 3435 compiles / 797 blocked. Private report
local/serial-status-window-source-20261117/report.json SHA-256
c677a157a7cd326aed8ebf6e64e6e1fe6532e1d9197f550c8712ac58311eb33e.
Commands: tools/ef2000_wcc.py decomp/src/serial_status_window.c --compiler
<pinned-wcc386> --output local/serial-status-window-20261117.obj;
pinned-Unicorn Python local/serial-status-window-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.


### 2026-11-17 — recover network initialization dispatcher 0x34bcc

Recover 215-byte initialize_network_dialog_34bcc after reading original ASM
and private pseudo-C. No inputs: EAX overwritten before use; saves EBX/ECX/EDX,
normal callees preserve ESI/EDI/EBP. Sole direct inventory caller 3448c and
callback table pointer 12b5d8; return ignored. Mode 3 checks files first;
initialize-comms AL 0 sets session flag and refreshes; 22 refreshes; 14 removes
all matching four-entry values, compacts, writes settings and displays loop
error; remaining statuses dispatch dialog state. Reload selected value after
settings before formatting. Error dialog lookup returns EAX root and preserves
EDX name / EBX local-text address for apply_to_widget; express all arguments
explicitly with confirmed helper signatures.

Pinned Watcom compile: zero warnings. 2048 scoped original/emitted-body
comparisons pass with explicit callee models: all 256 status bytes, mode 2/3,
file-check success/failure, selected zero/nonzero with duplicate table entries.
Models mutate selection during initialization, compaction and settings write
to check reload timing. Compare call traces (local formatted text by content),
six saved GPRs, ESP and RAM except code placement/stack scratch. Actual callees,
EAX scratch, flags and stack scratch remain unproved. No binary identity,
splice/replay or universal equivalence claim; retain hybrid exclusion.
Tracker: 3436 compiles / 796 blocked. Private report
local/network-initialization-source-20261117/report.json SHA-256
ae47ea07fc951e9e27f7905f30310b2a07007bb2d0597da10594d691e4b57fe0.
Commands: tools/ef2000_wcc.py decomp/src/network_initialization.c --compiler
<pinned-wcc386> --output local/network-initialization-20261117.obj;
pinned-Unicorn Python local/network-initialization-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.


### 2026-11-17 — recover player-window initializer 0x33c64

Recover 346-byte initialize_players_window_33c64 after reading original ASM,
private pseudo-C and five dword jump-table entries at 33c50. EAX root feeds
three widget lookups; all six other GPRs pushed/popped; plain RET. Callback
slot 12b4f0, no direct inventory callers. Reset five related controls before
reading enable flag; zero clears server/name/type. Modes 0..4 select original
control operations and labels, including optional control in mode 2. Higher
unsigned modes skip type display but still update name and server status.
Keep flag reads after preceding UI calls; do not cache them at entry.

Pinned Watcom compile: zero warnings. 128 scoped original/emitted-body
comparisons with explicit find/apply/show models pass for modes 0..5/127/255,
enable/gate/host flags, and callbacks mutating flags. Relocate compiled
internal jump-table entries and pointer as well as external calls. Compare
traces, six saved GPRs, ESP and RAM except code placement/stack scratch.
Actual callees, EAX scratch, flags and stack scratch unproved; no byte
identity, splice/replay or universal-equivalence claim. Retain hybrid
exclusion; tracker 3437 compiles / 795 blocked. Private report
local/network-players-window-source-20261117/report.json SHA-256
046ce7d3f8fd747426feecbacda66201dda01197185f5f937a656c4e32314be4.
Commands: tools/ef2000_wcc.py decomp/src/network_players_window.c --compiler
<pinned-wcc386> --output local/network-players-window-20261117.obj;
pinned-Unicorn Python local/network-players-window-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.


### 2026-11-17 — recover loop-number change handler 0x345e8

Recover 450-byte change_network_loop_345e8 after reading original ASM and
private pseudo-C. EAX widget; six other GPRs pushed/popped, plain RET;
callback slot 12b5a8 and no direct inventory callers. Assert number-label +3f
and parent +51 in the existing partial widget layout without changing other
offsets. Parse decimal slot, report invalid range through cdecl function
pointer 132a74, validate at most eight hex characters, permit empty text,
and test full scanner EAX rather than AL. Preserve shutdown/store/compact/
settings/wait-window ordering, status-14 removal of every matching value,
modal error and initialization retry, then network and parent-name refresh.
Pass wait-window result explicitly to c9fc4; preceding aca88 preserves EAX.

Pinned Watcom compile: zero warnings. 751 scoped original/emitted-body
comparisons with parser/session/dialog models pass: slots -1/0/1/4/5,
scanner returns 0/1/101, empty/valid/ninth-invalid/invalid/high-byte text,
initialization statuses 0/14/22, retry success/failure and inactive early
return. Error callback is allowed to return in the model; scanner models
always write output, so original uninitialized slot after a nonwriting
failed scan followed by returning error is not verified. Compare normalized
call traces (local formatted text by content), six saved GPRs, ESP and RAM
except code placement/stack scratch. Actual callees, EAX scratch, flags and
stack scratch remain unproved; no byte identity, splice/replay or universal
equivalence claim. Retain hybrid exclusion. Tracker 3438 compiles / 794
blocked. Private report local/network-loop-settings-source-20261117/report.json
SHA-256 49ebfea4df71d42c2b4b2f3d4f79ef576b1b6d9f8a5f07aa792b2fbace3c6d8b.
Commands: tools/ef2000_wcc.py decomp/src/network_loop_settings.c --compiler
<pinned-wcc386> --output local/network-loop-settings-20261117.obj;
pinned-Unicorn Python local/network-loop-settings-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17 — Translate privilege-authorized CLI

Add exact-encoding CLI support to the explicit-state translator: clear only
IF when protected-mode CPL <= IOPL. VM86 and privilege failures return
unsupported (7), preserving state rather than inventing CR4.PVI/VME, VIF or
#GP delivery. STI remains unsupported because its interrupt shadow is not
modeled. No asynchronous interrupt delivery or host service is added.

New synthetic checks cover IF initially clear/set, IOPL 0..3, CPL 0/3,
VM86 rejection, zero fuel, instruction fuel/PC, unchanged RAM/registers and
restartable rejected contexts. Four authorized ring-zero cases also compare
against Unicorn. These checks establish only the modeled CLI scope, not
hardware interrupt delivery or whole-function equivalence.

Regeneration with all INT21/31 and scalar ports as external boundaries
produces 3602 generated-unverified entries (previously 3599): 101f70,
1111e1 and 11f2d3 newly enter the closure. Other CLI callers retain additional
blockers. Tracker states stay unchanged. Private report:
local/asm-cli-reachability-20261117/report.json, SHA-256
1fb3ff12460a306442a193383099a6dcca62001e15ba97fd75cda1d9993f4ff7.
Commands: python3 -m unittest tests.test_asm_cli;
pinned-Unicorn Python -m unittest tests.test_asm_cli;
tools/asm_c_program.py --inventory local/asm-latest-body-inventory-20261117
--workspace local/ghidra-offset-tail-20261117
--output local/asm-cli-reachability-20261117 --interrupt-boundaries
--port-boundaries; python3 tools/decomp_status.py check; git diff --check.
Regression validation: pinned-Unicorn Python -m unittest
tests.test_asm_to_c tests.test_asm_c_program: 58/58 PASS; focused CLI
suite: 3/3 PASS. No original-function equivalence promotion.

### 2026-11-17 — Recover network service window initialization

Recover 3396c (361 bytes) as network_service_window.c. EAX supplies the root;
all six other general registers are saved/restored. Callback pointer 12bb78
is the only payload pointer occurrence and no direct inventory callers were
found. The disassembly establishes three root/name lookups, a separate named
dialog lookup, selected-provider display, host-title creation and widget
position updates. Preserve sign-extension of AX on each classification scan
iteration (including backward wrap at 32768), byte-wrapped classification
index, uppercase/append call order, and flag reads after preceding callbacks.
The final inactive nonhost branch clears the title with the original word
store. Existing widget struct assertion covers text pointer offset 27.

Watcom compilation is warning-free. Private checker executes actual emitted
Watcom code versus original ASM in 129 cases: provider/host/session/gate and
callback-mutation combinations, empty/ordinary/leading-space/high-byte names,
and a 32768-space signed-index wrap case. Both sides execute original strcpy
9d749, uppercase a4421 and strcat 9ca57; only widget operations and the literal
formatter are modeled. Compare normalized traces, six saved GPRs, ESP and
whole 5 MiB RAM excluding code placement and stack scratch. Scratch EAX,
flags, stack contents, malformed overflowing titles, real UI integration and
splice/replay remain unproved. State compiles, retain hybrid exclusion.
Tracker: 3439 compiles / 793 blocked (455224 blocked body bytes).
Private report local/network-service-window-source-20261117/report.json,
SHA-256 80922ae3301de9fa8951cea4e7d0444db1926be2c3d3ae20f9fa06d73f5981a8.
Commands: tools/ef2000_wcc.py decomp/src/network_service_window.c --compiler
<pinned-wcc386> --output local/network-service-window-20261117.obj;
pinned-Unicorn Python local/network-service-window-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17 — Recover credits resource loader

Recover 371b0 (161 bytes) in credits_window.c. Disassembly establishes EAX
widget, saved EBX/ECX/EDX and callee-preserved ESI/EDI/EBP. No direct inventory
callers; callback pointer 12b550. Build a 144-byte local path, query size via
132ab4, report zero via cdecl error hook 132a74, allocate size+16 modulo 2^32
only if global 19f36c is null, load via 132ab8 and test only AL, then redraw
and wrap using a fresh global-pointer read. Error callbacks may return:
default eb2c0 saves/restores EDX, with ECX preserved by its callees; do not
turn reports into early exits. Record the three newly identified globals.

Warning-free Watcom compilation. Private actual emitted-body versus ASM
checker passes 256 cases: eight sizes (including zero, arena boundary and
wraparound), existing/absent buffer, four full-EAX load results, optional
query/redraw pointer mutation and empty/nonempty path prefix. Execute the
original path builder and its string/lowercase helpers, original arena
allocator/zeroing helper, and original default error handler with counter
initialized to 5 (its output branch stays disabled). File size/load and
redraw/wrap calls are modeled. Compare normalized call traces, six saved
GPRs, ESP and 5 MiB RAM except code placement and stack scratch. Scratch EAX,
flags, stack, error formatting/output, real file/UI services and splice/replay
remain unproved; this is not a Mitarbeiter crash-fix claim. Retain hybrid
exclusion, mark compiles. Tracker: 3440 compiles / 792 blocked / 455063
blocked body bytes. Private report:
local/credits-window-source-20261117/report.json, SHA-256
1bae50b3f598f8bb1d166cefeb06e0e36bed523b36ec43dfc53821aacad30449.
Commands: tools/ef2000_wcc.py decomp/src/credits_window.c --compiler
<pinned-wcc386> --output local/credits-window-20261117.obj;
pinned-Unicorn Python local/credits-window-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17 — Recover shared network control refresh

Recover 33630 as network_controls.c, including its separate 33dbe..33e78
range (328 total body bytes). The old boundary-split blocker is resolved
against the existing independently decoded two-range inventory. No register
inputs; save/restore EBX/ECX/EDX, with other GPRs preserved by callees. Audit
34480@34496 and 33e78@33e8e callers (the latter retains ECX dialog); inventory
has 17 direct call/tail sites. All selection flags are computed before
refresh_all_widgets_a4c84 and apply/status calls. Preserve signed upper-half
1357ae interpretation (int16 at 1357b0), exact positive/negative threshold
branches and original callback ordering. Reuse existing named globals.

Warning-free Watcom compile. Scoped actual emitted-body versus original
noncontiguous ASM comparison: 384 PASS across four binary globals, six
signed counts (-32768,-1,0,1,2,32767), two irrelevant low-half patterns and
callbacks optionally changing every input global. A first comparison exposed
an inverted primary-control condition in the draft; corrected to match the
authoritative branches before retaining evidence. Compare traces, six saved
GPRs, ESP and 5 MiB RAM excluding code placement and stack scratch. Widget
refresh/apply/status callees are modeled; scratch EAX, flags, stack and real
integration/splice/replay are unproved. Keep hybrid excluded, state compiles.
Tracker: 3441 compiles / 791 blocked / 454735 blocked body bytes.
Private report local/network-controls-source-20261117/report.json, SHA-256
a64cce7587f62e757bf5b34d40ea79f5a3315f473b1f2dceb2282e006f0fda7a.
Commands: tools/ef2000_wcc.py decomp/src/network_controls.c --compiler
<pinned-wcc386> --output local/network-controls-20261117.obj;
pinned-Unicorn Python local/network-controls-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: replace three modem text callback probe hooks

Recovered 33720/33738/337ac (21 bytes each) into network_text_callbacks.c.
Use real 9d749 string-copy and a2bdc settings-write calls instead of the
host-only probe. Confirmed EAX widget input, text +0x27, preserved six GPRs;
callback slots 12b648/12b650/12b660, no direct callers. Watcom zero warnings.
48 scoped actual compiled-C/original-ASM comparisons PASS: empty/ASCII/long/
high-bit strings, source equal to destination, and settings callback mutation.
Execute original strcpy; model settings writer. Compare call traces, six saved
GPRs, ESP and 5 MiB RAM excluding code placement and stack scratch. EAX,
flags, stack and integration/splice/replay remain unproved. Keep hybrid excluded.
Tracker: 3444 compiles / 788 blocked / 454672 blocked body bytes.
Private report local/network-text-source-20261117/report.json, SHA-256
a82a049f5250642bca6d091becc6050ea308ad68349700ab5e2ba2992cd06e1e.
Commands: tools/ef2000_wcc.py decomp/src/network_text_callbacks.c --compiler
<pinned-wcc386> --output local/network-text-20261117.obj;
pinned-Unicorn Python local/network-text-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover FIFO option callbacks

Recovered 336c0 (45 bytes) and 336f0 (47 bytes), replacing host-only probes
with real clear-session, shutdown-device, settings-write and shared-refresh
calls. Neither callback consumes an argument. Callback slots 12b618/12b620;
no direct callers. Authoritative ASM corrects the old draft's inverted guard:
FIFO toggles when initialization is zero OR the guard is nonzero. Preserve
IPX enable sampling before calls and option sampling after shutdown; zero
returns through 33618, other exits tail-jump 33630.
Watcom clean; 288 scoped compiled-C/original-ASM comparisons PASS across
binary initialization/guard, enable 0/1/255, six option values including byte
wrap, and optional mutations by every modeled callee. Compare call traces,
six saved GPRs, ESP and 5 MiB RAM excluding code placement and stack scratch.
Four callees modeled; scratch EAX, flags, stack and integration unproved.
Keep hybrid excluded, state compiles. Tracker 3446 compiles / 786 blocked /
454580 blocked body bytes. Private report
local/network-options-source-20261117/report.json, SHA-256
0663df70201cd2ecd312e0e85dcae3f8de295481e246668c0f8225eaf2aa88e0.
Commands: tools/ef2000_wcc.py decomp/src/network_option_callbacks.c --compiler
<pinned-wcc386> --output local/network-options-20261117.obj;
pinned-Unicorn Python local/network-options-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover initialization string selector

Recovered 33760 (74 bytes) into network_init_string.c, replacing formatter
and guest-call probes with real cdecl a4342 and Watcom a7588/a2bdc calls.
Confirm EAX widget, preserved six GPRs, 16-byte local, callback slot 12b658,
no direct callers. Preserve unchecked byte XOR table indexing and unbounded
%s formatting, including load-before-choice-store order. Watcom clean.
56 scoped compiled-C/original-ASM comparisons PASS: seven index bytes,
four fitting strings, and optional callback mutations. Three callees modeled;
compare normalized calls, six saved GPRs, ESP and 5 MiB RAM excluding code
placement/stack scratch. EAX, flags, stack, overflow and integration unproved.
Keep hybrid excluded; state compiles. Tracker 3447 compiles / 785 blocked /
454506 blocked body bytes. Private report
local/network-init-source-20261117/report.json, SHA-256
fdea187645105b8c2db51644ef191cb7e510b57caa72873e66d286fbb94d6347.
Commands: tools/ef2000_wcc.py decomp/src/network_init_string.c --compiler
<pinned-wcc386> --output local/network-init-20261117.obj;
pinned-Unicorn Python local/network-init-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover parity and port callbacks

Recovered 3361c (20 bytes) and 337c4 (37 bytes) into
network_option_callbacks.c with real guest calls. Neither consumes an input;
callback slots 12b600/12b5f0, no direct callers. Preserve parity fallthrough
and port tail jump to shared refresh, shutdown before sampling the port,
and selector AL store before clearing the session. Watcom zero warnings.
240 scoped compiled-C/original-ASM comparisons PASS, executing original
parity counter df2e4 and enabled-index selector e1540, modeling the other
four callees with optional state mutation. Cover parity wrap, five port
values including 254/255 and two complementary enabled tables. Compare six
saved GPRs, ESP, traces and 5 MiB RAM excluding code placement/stack scratch.
EAX, flags, stack, empty enabled table and integration remain unproved.
Keep hybrid excluded, state compiles. Tracker 3449 compiles / 783 blocked /
454449 blocked body bytes. Private report
local/network-port-parity-source-20261117/report.json, SHA-256
49d70cec7fef0936215ef541bd33fd53f7e374d74d69a32a1b8e7db3b3433e5d.
Commands: tools/ef2000_wcc.py decomp/src/network_option_callbacks.c --compiler
<pinned-wcc386> --output local/network-port-parity-20261117.obj;
pinned-Unicorn Python local/network-port-parity-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover baud selection callback and retire settings probes

Recovered 335c0 (89 bytes), replacing all remaining probe hooks in
network_settings_callbacks.c with real guest calls. Confirm EAX widget,
six preserved GPRs, cdecl df26f scanner, text read after clear/shutdown,
exactly one assignment in signed range [1,4], and rejection stored as ff.
Callback slot 12b5f8, no direct callers. Watcom zero warnings.
30 scoped compiled-C/original-ASM comparisons PASS executing the original
scanner and its callees. Cover empty/invalid input, signed values, range
boundaries, whitespace, trailing text and integer overflow text, plus optional
mutations by four modeled setup/settings/refresh callees. Compare six saved
GPRs, ESP, normalized call traces and 5 MiB RAM excluding code placement and
stack scratch. Scratch EAX, flags, stack and integration remain unproved.
Keep hybrid excluded, state compiles. Tracker 3450 compiles / 782 blocked /
454360 blocked body bytes. Private report
local/network-baud-source-20261117/report.json, SHA-256
23753a7d384401f3a138cc8952c2de0747a551c4e8ec53290aa1d743fb812488.
Commands: tools/ef2000_wcc.py decomp/src/network_settings_callbacks.c --compiler
<pinned-wcc386> --output local/network-baud-20261117.obj;
pinned-Unicorn Python local/network-baud-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover arcade start and channel callbacks

Recovered 34480 (29 bytes) and 344a0 (27 bytes) into
network_session_callbacks.c with real guest calls. Confirm no arguments,
six preserved GPRs and callback slots 12b580/12b598; no direct callers.
Preserve socket word store before initialization, host-byte XOR after clear,
and channel tail jump to shared refresh. Watcom zero warnings.
192 scoped compiled-C/original-ASM comparisons PASS: four host-byte values
including fe/ff, three socket words, adjacent-byte sentinels and optional
mutations by five modeled callees. Compare call traces, six saved GPRs,
ESP and 5 MiB RAM excluding code placement/stack scratch. Scratch EAX,
flags, stack and actual callee integration remain unproved. Keep hybrid
excluded, state compiles. Tracker 3452 compiles / 780 blocked / 454304
blocked body bytes. Private report
local/network-session-callbacks-source-20261117/report.json, SHA-256
ea5780f39707f10f425500d0413d8909dba501ae00b110c1d1f5fc7838d04fe1.
Commands: tools/ef2000_wcc.py decomp/src/network_session_callbacks.c --compiler
<pinned-wcc386> --output local/network-session-callbacks-20261117.obj;
pinned-Unicorn Python local/network-session-callbacks-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover modem form initializer

Recovered 33ad8 (129 bytes) into network_init_string.c with real guest calls.
Confirm EAX root, preserved six GPRs, four ordered find/show pairs, 72-byte
local and choice sampled after showing the device. Preserve unchecked choice
and unbounded %s formatting. Callback slot 12b638, no direct callers.
Watcom zero warnings. 56 scoped compiled-C/original-ASM comparisons PASS:
seven choice bytes, four fitting strings including 71 bytes, optional choice
mutations in every modeled callee, and distinct pointers for all 256 table
indices to check the post-callback load. Three callees modeled. Compare six
saved GPRs, ESP, normalized call traces and 5 MiB RAM excluding code placement
and stack scratch. Scratch EAX, flags, stack, overflow and integration remain
unproved. Keep hybrid excluded, state compiles. Tracker 3453 compiles /
779 blocked / 454175 blocked body bytes. Private report
local/network-loops-form-source-20261117/report.json, SHA-256
9d541aa478878c876eba8e36eb33865cfc11efa4769c67ab9e48217f31365f7c.
Commands: tools/ef2000_wcc.py decomp/src/network_init_string.c --compiler
<pinned-wcc386> --output local/network-loops-form-20261117.obj;
pinned-Unicorn Python local/network-loops-form-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover DirectPlay row display callback

Recovered 341d8 (62 bytes) into network_directplay_list.c, replacing lookup
and display probes with real guest calls. Confirm EAX root, six preserved
GPRs, callback slot 12bb90, no direct callers; widget link offset +21 has an
existing struct assertion. Correct the old draft's cached count: original
reloads signed count at every iteration, then reads the next link after show.
Use unsigned index increment and row address arithmetic to preserve wrap.
Watcom zero warnings. 144 scoped compiled-C/original-ASM comparisons PASS:
six initial signed counts, lookup mutations, display mutations that stop/grow
or negate count, and next-link replacement. Compare traces, six saved GPRs,
ESP and 5 MiB RAM excluding code placement/stack scratch. Two callees modeled;
scratch EAX, flags, stack, huge traversal and integration remain unproved.
Keep hybrid excluded, state compiles. Tracker 3454 compiles / 778 blocked /
454113 blocked body bytes. Private report
local/network-directplay-list-source-20261117/report.json, SHA-256
6cf0f60638e1ac4fdfc83700b7140835ef74d1829af688c0444456c75602c76b.
Commands: tools/ef2000_wcc.py decomp/src/network_directplay_list.c --compiler
<pinned-wcc386> --output local/network-directplay-list-20261117.obj;
pinned-Unicorn Python local/network-directplay-list-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover network type display callback

Recovered 35eb8 (75 bytes) into network_type_callback.c with real a73cc
calls. Confirm EAX root, six preserved GPRs, type byte normalized once before
either call and complementary EBX flags for names 183114/18311c. Callback
slot 12b5e8, no direct callers. Watcom zero warnings.
128 scoped compiled-C/original-ASM comparisons PASS executing original
a73cc flag setter; lookup and refresh modeled. Cover four type bytes,
four existing widget flag patterns, suppressed/active refresh, missing widgets,
and type mutation by callbacks. Compare traces, six saved GPRs, ESP and
5 MiB RAM excluding code placement/stack scratch. Scratch EAX, flags, stack
and actual integration remain unproved. Keep hybrid excluded, state compiles.
Tracker 3455 compiles / 777 blocked / 454038 blocked body bytes.
Private report local/network-type-source-20261117/report.json, SHA-256
5f24a5ec44430278f0fd456acc7c0f9ff461964f572de3e4bc2b8ef1e80e5424.
Commands: tools/ef2000_wcc.py decomp/src/network_type_callback.c --compiler
<pinned-wcc386> --output local/network-type-20261117.obj;
pinned-Unicorn Python local/network-type-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover receive-status callbacks

Recovered 35e68/35e90 (37 bytes each) into network_receive_callbacks.c.
Confirm EAX widget, six preserved GPRs and real apply/clear/rescale calls.
The two zero stack words are the 0.0 scale read by 35de0, which uses RET8.
Callback slots 12b560/12b568, no direct callers. Watcom zero warnings.
64 scoped compiled-C/original-ASM comparisons PASS with three callees modeled:
check EAX/EDX/EBX apply arguments, original widget passed to clear, both zero
stack words, callee cleanup, six saved GPRs, ESP, call traces and 5 MiB RAM
excluding code placement/stack scratch. Vary widget addresses, saved-register
values, initial global bytes and callback mutations. Scratch EAX, flags,
stack contents and actual integration remain unproved. Keep hybrid excluded,
state compiles. Tracker 3457 compiles / 775 blocked / 453964 blocked bytes.
Private report local/network-receive-source-20261117/report.json, SHA-256
66b14ccd5e25d77c45287dcb0841303786d8f6cf540add222a417c3186a8473f.
Commands: tools/ef2000_wcc.py decomp/src/network_receive_callbacks.c --compiler
<pinned-wcc386> --output local/network-receive-20261117.obj;
pinned-Unicorn Python local/network-receive-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.

### 2026-11-17: recover modem dialog callbacks

Recovered 345d4 (19 bytes) and 347ac (13 bytes) into
network_dialog_callbacks.c, replacing host guest-call probes with real calls.
Confirm callback slots 12b640/12b5b0, conditional shared RET and zero-extended
widget ID at +10; add layout assertion. Watcom zero warnings.
128 scoped compiled-C/original-ASM comparisons PASS, with apply/close callees
modeled; vary initialization 0/1/128/255, widget address, ID 0/255, saved
registers and callback mutation. Compare six saved GPRs, ESP, traces and
5 MiB RAM excluding code placement and stack scratch. EAX, flags, stack and
integration remain unproved; keep compiles and hybrid excluded.
Tracker 3459 compiles / 773 blocked / 453932 blocked bytes.
Private report local/network-dialog-source-20261117/report.json SHA-256
37081bccf50509826849e86af0751a6cf7702dfcce1145eba37710ce2f50065a.
Commands: tools/ef2000_wcc.py decomp/src/network_dialog_callbacks.c --compiler
<pinned-wcc386> --output local/network-dialog-20261117.obj;
pinned-Unicorn Python local/network-dialog-source-20261117/check.py;
python3 tools/decomp_status.py check; git diff --check.
