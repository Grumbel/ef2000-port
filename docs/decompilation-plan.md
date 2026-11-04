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
