# EF2000 decompilation and modernization plan

This is the master plan for turning the current emulation-based port into
(1) recovered C source that compiles with OpenWatcom, (2) documented data
formats, and (3) a modern native build with widescreen and high frame rates.
It is written to be executed step by step by an AI agent. **Read the whole
"Ground rules" and "Operating loop" sections before every work session.**

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
the unpatched `nix run .#unicorn-cpp -L ...` startup reaches its 30M
instruction limit (42.3M guest instructions/s on that run); no real patch
has been executed. See `docs/function-differential.md`.

### 3.9 [ ] Golden replay suite

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
