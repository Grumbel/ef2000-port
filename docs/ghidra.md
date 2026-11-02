# Headless Ghidra analysis of the X-32 game image

EF2000's `_EF2000.EXE` is a FlashTek X-32 executable, not a conventional
PE/ELF/LE program. Importing the entire DOS executable as 32-bit code puts
addresses in the wrong places. Instead, the existing SHA-pinned extractor
provides the 32-bit initialized payload directly. Its **offset zero** is the
same offset used by the C++ Unicorn guest, startup traces and
`--profile-hot-blocks`.

This is a local reverse-engineering workflow; it is **not** a runtime or
packaging dependency. Original executable bytes, Ghidra projects, and generated
pseudo-C remain in the ignored `local/` directory by default.

## Initialize once

`nix develop` includes Ghidra, sets `GHIDRA_HOME` to the Nix store's
`lib/ghidra` installation, and adds `ef2000-ghidra` to `PATH`.
Run from the EF2000 repository root:

```sh
nix develop
ef2000-ghidra import \
  ../EF2000/c/EF2000/_EF2000.EXE
```

Outside the development shell, `nix run .#ghidra` opens the GUI and
`nix run .#ghidra-headless -- ...` runs the headless analyzer. The full
EF2000 import/recovery/decompilation tool is also available outside the
development shell, without making Ghidra a game-runtime dependency:

```sh
nix run .#ghidra-ef2000 -- import ../EF2000/c/EF2000/_EF2000.EXE
nix run .#ghidra-ef2000 -- inventory
nix run .#ghidra-ef2000 -- audit-flow-gaps
nix run .#ghidra-ef2000 -- repair-boundaries # report only unless --apply
nix run .#ghidra-ef2000 -- export-all --custom-storage-fallback
```

The Nix wrapper writes to **the current directory's `local/ghidra`**, even
though the scripts themselves reside in the read-only Nix store. Override
that default with `EF2000_GHIDRA_WORKSPACE` or per-command `--workspace`;
it is important to use the same workspace across commands. Direct Python
invocations from the repository retain the original source-tree
`local/ghidra` default. With a separate Ghidra installation, set
`GHIDRA_HOME=/path/to/ghidra` or supply
`--analyze-headless /path/to/ghidra/support/analyzeHeadless`.

The command verifies the executable SHA-256,
exports `payload.bin` and `manifest.json`, and imports the binary to the
`local/ghidra/projects/EF2000` project using Ghidra's `BinaryLoader`,
`x86:LE:32:default`, and load base **0**.

The pre-analysis script `tools/ghidra/EF2000Seed.java` creates an uninitialized
non-executable BSS region `[0x195fb0, 0x4de2f0)` and seeds known startup,
Watcom and copy-function addresses, plus selected data labels from
`docs/startup-symbols.md`. `0x8b274` is deliberately named a *candidate* main.

Once imported, open the project through Ghidra's GUI to explore cross-references,
rename functions and apply verified types. The importer refuses to overwrite an
existing project. Reuse the same project with `decompile`; preserve your manual
Ghidra annotations in the local project directory or export them separately.
The asset-free Ghidra CLI tests run under `nix flake check` via the main
package and a separate `ghidra-workflow` check; no original DOS image is
needed by those tests.

## Export the entire discovered-function corpus

The original `decompile` command only exports selected addresses. For a
whole-program reverse-engineering pass, first inventory Ghidra's existing
analysis, then export **every function Ghidra currently recognizes**:

```sh
python3 tools/ghidra_ef2000.py inventory
python3 tools/ghidra_ef2000.py export-all
# Short smoke test before a full run:
python3 tools/ghidra_ef2000.py export-all --limit 20
# Refresh generated C after changing names/types in the Ghidra project:
python3 tools/ghidra_ef2000.py export-all --overwrite
# Optional: retry low-level decompiler failures with a __fastcall prototype
# *evaluation model*. This is not the proven Watcom calling convention.
python3 tools/ghidra_ef2000.py export-all --overwrite --fastcall-fallback
```

Both operations reuse the imported project with `-noanalysis`; they do not
discard manually assigned Ghidra names or types. By default, the output is
`local/ghidra/full-export/`, which is Git-ignored. A completed export contains:

| File | Meaning |
| --- | --- |
| `report.json` | Function, instruction, callsite and decompilation counts |
| `functions.tsv` | Function entries, names, body lengths, export status and relative filenames |
| `functions/NNNN/0xXXXXXXXX.c` | One decompiled pseudo-C body per successful function, grouped by address |
| `functions/NNNN/0xXXXXXXXX.asm` | Disassembly fallback if Ghidra cannot produce pseudo-C |
| `calls.tsv` | Statically resolved CALL edges and unresolved/indirect callsites |
| `orphan-ranges.tsv` | Contiguous disassembled instruction ranges outside recognized functions |
| `orphan-instructions.asm` | Disassembly of those unassigned instructions |
| `candidate-entries.tsv` | Tentative function entries at unassigned range starts and after `RET` |

## Rank candidate subsystems across the entire decompilation

The complete pseudo-C corpus is a starting point, not a compilable source port.
To move from isolated function-boundary fixes to subsystem-sized work, export
instruction-level Ghidra facts and build a ranked static relationship index:

```sh
ef2000-ghidra export-all --custom-storage-fallback
ef2000-ghidra function-facts
ef2000-ghidra subsystem-map --seed 0x4050 --seed 0x433c \
  --seed 0x4aa0 --seed 0x4b54
```

`function-facts` runs read-only with `-noanalysis`. It writes four files under
`local/ghidra/subsystem-facts/` (or the selected workspace):

| File | Evidence exported |
| --- | --- |
| `function-facts.tsv` | Function body size, instruction/branch/call counts and instruction-level P-code load/store counts |
| `data-references.tsv` | Explicit Ghidra Listing DATA references, including target kind and symbol when known |
| `address-candidates.tsv` | Address or scalar operands in the probable global-address window (`0x110000..0x4de2ef`), with initialized vs runtime classification |
| `cross-function-branches.tsv` | Explicit JUMP references whose target Listing instruction belongs to another function |

Explicit DATA references and plausible literal-address operands are **separate
evidence types**. Ghidra sometimes treats an absolute address embedded in a
`MOV` instruction as a scalar rather than a reference; including it as a
*candidate* recovers useful shared-state leads without asserting a proven xref.
The address window is a heuristic and can miss or include non-global values.
Likewise, an instruction-level `PcodeOp.LOAD` does not establish a verified
C-level object type or argument convention.

`subsystem-map` is a fast, offline Python pass over those TSVs and
`full-export/calls.tsv`: it does **not** launch Ghidra or modify the project.
It produces `subsystem-map/function-affinities.tsv` with the strongest ranked
neighbors, `candidate-groups.tsv` with conservative complete-linkage groups,
`seed-neighborhoods.md`, and `summary.json`. The scoring downweights shared
callees and coarse 64-byte regions, filters common tokens, and keeps direct
CALL and cross-function JUMP edges distinguishable. Each member of a group
must have a qualifying pairwise score with every other member, and groups are
bounded at 16 to avoid spurious giant connected components.

These are **review queues**, not authoritative subsystem labels or inferred
source-level types. Use shared global accesses, control flow, and the original
instructions to validate a candidate before translating routines into C++.
Re-run `function-facts` after Ghidra function-boundary or naming changes, and
`subsystem-map` whenever the cached facts or calls inventory changes. Both
commands support `--workspace` and `--output`.

The exporter resumes from existing `.c` files on repeat runs, and records
per-function decompiler failures without aborting the entire corpus. Use
`--overwrite` when edited signatures, renamed symbols or improved Ghidra
analysis should appear in previously exported code. The `--limit` option is
for diagnostics; `0` (the default) processes all recognized functions.
`--fastcall-fallback` allocates a separate Ghidra decompiler with an alternate
parameter-evaluation model and retries **only** entries that fail the normal
pass; successful alternative output is labeled `written_fastcall_fallback`
in `functions.tsv` and prominently warned in its C header. Neither existing
Ghidra symbols nor calling conventions are mutated. The `report.json` field
`fastcall_fallback_decompilations` counts newly exported non-default C outputs.
This mode is purely exploratory: `__fastcall` is not the Watcom X-32 ABI, and
register/stack parameter recovery needs manual checking.

### Experimental Watcom-style register model

The [Open Watcom C/C++ User's Guide](https://open-watcom.github.io/open-watcom-v2-wikidocs/cguide.html)
documents the 32-bit `__watcall` convention, including its integer argument
register set and stack cleanup. EF2000's actual X-32 code also uses customized
register-passing sequences. The order of registers within the compiler's
default register set is not necessarily the source order. Thus we cannot
assume a single reconstructed calling convention applies to every function.

For an opt-in decompiler experiment, `tools/ghidra_watcom.py` creates a
**new private copy** of Ghidra, adding the narrowly named model
`__ef2000_aedb` to its x86 Windows compiler specification. It preserves the
original Ghidra installation (including a read-only Nix store) and all
existing projects:

```sh
# In nix develop, GHIDRA_HOME is already set. This copies Ghidra (~hundreds
# of MiB); refuse to overwrite an existing target.
python3 tools/ghidra_watcom.py \
  --destination local/ghidra-watcom-install
# Use a separate export directory so the ordinary corpus is preserved.
python3 tools/ghidra_ef2000.py export-all \
  --workspace local/ghidra-entries-recovery \
  --output local/ghidra-entries-recovery/watcom-export \
  --watcom-fallback \
  --analyze-headless local/ghidra-watcom-install/support/analyzeHeadless
```

The experimental model prioritizes 32-bit arguments in `EAX`, `EDX`, `EBX`,
`ECX` before the stack, and models a possible 64-bit `EDX:EAX` return. This
is a working hypothesis informed by original code, **not an ABI proof**.
It affects only retry decompilations that already failed with Ghidra's
default model; neither Ghidra function signatures nor the binary are edited.
The `functions.tsv` status `written_watcom_fallback`, a C-header warning, and
the `watcom_fallback_decompilations` report field identify those results.
`--watcom-fallback` and `--fastcall-fallback` are mutually exclusive.
If the experimental compiler specification is missing, the tool fails with
an explanatory error rather than silently substituting another model.
On the #044 recovery corpus, this optional mode produced **4,199 pseudo-C
files and four remaining assembly fallbacks** (one more C function than the
generic `__fastcall` retry), with no change to discovered function count.

### Complete pseudo-C coverage with temporary custom storage

Ghidra can fail while inferring overlapping input registers or packed stack
arguments, even when it has successfully disassembled a function. For these
cases, a final **opt-in** retry pins `EAX`, `EDX`, `EBX`, `ECX` as 32-bit
parameter locations using Ghidra's `CUSTOM_STORAGE` signature mode. The
signature changes exist only while the failed function is being retried.
The tool explicitly restores the original parameters and storage mode,
aborts its temporary transaction, and checks that the signature is restored.
The headless invocation also uses **`-readOnly`**, so any project-file
changes are discarded even if a script or Ghidra crashes.
The Python wrapper uses **two passes**: normal decompilation first, then a
second headless process which reuses the unaffected C and retries only the
remaining failures. This prevents speculative function signatures from
changing the pseudo-C of later ordinary functions. Consequently the final
report usually counts ordinary C files as `decompilations_reused`, not
`decompilations_written`. The first pass's `.asm` diagnostics are intentionally
retained beside successfully recovered `.c` files for cross-checking; an
`assembly_fallbacks` count of zero means no recognized function is *only*
available as assembly.
With `--overwrite`, the wrapper builds both passes in a **fresh temporary
export directory** and publishes the complete generated results only after
validation. This prevents a stale custom-storage result from being counted
as reused during a full refresh. Failed entries cannot retain an older C
file in the published `functions/` tree. This does not affect ordinary
resume runs or files outside the generated export tree.

This workaround requires **no modified Ghidra distribution**; it works with
the ordinary Ghidra installation in `nix develop`:

```sh
python3 tools/ghidra_ef2000.py export-all \
  --custom-storage-fallback \
  --output local/ghidra/custom-storage-export
```

Use `--workspace PATH` to select an existing recovered/annotated Ghidra
project. The documented 4,203-function result came from the independently
recovered #044 analysis project, not a fresh unanalyzed import.

With the #044 recovery project in Ghidra 11.4.2 this produced **4,203
pseudo-C files for 4,203 recognized functions**, with eight
`written_storage_fallback` results, zero failed decompilations and zero
assembly-only files. Alternatively, combining `--watcom-fallback` and
`--custom-storage-fallback` (with the private Watcom Ghidra installation
above) yields four Watcom-model retries and four custom-storage retries.
The `custom_storage_fallback_decompilations` report field counts the latter.

These additional files have **unverified parameter declarations**, and
misleading or incomplete call arguments remain possible. The tool labels
every such file in its header and in `functions.tsv`. It does not prove that
every function has been discovered, that the original ABI is understood, or
that the pseudo-C can be compiled to reproduce the original program.

If Ghidra is upgraded, make a copy of your project before opening it with
the newer release, since Ghidra can migrate the project database.

**Inventory is not proof of completeness.** EF2000 contains embedded data,
indirect calls, dynamically populated dispatch tables and X-32 runtime
conventions that generic x86 analysis does not fully understand. In
particular, an unassigned instruction range may be a missed routine, an
inline block, a jump-table landing zone or a false disassembly of data.
`candidate-entries.tsv` is a review queue, not a verified function list.
The exporter does **not** automatically create functions at candidates or
rename them, and nothing in this pipeline produces compilable original C++.

## Conservative recovery of missing functions

The `recover-orphans` command analyzes the unassigned instruction regions.
It considers candidate entries at the beginning of an unassigned range or
immediately following a `RET`, then identifies a *strong candidate* when
both conditions hold:

1. The first instruction resembles a saved-register/stack prologue (`PUSH`
   of a callee-saved register, `SUB ESP`, or `ENTER`).
2. At least one Ghidra reference targets that address (CALL, JMP or data
   pointer). The TSV includes example reference sources to aid validation.

By default **nothing is modified**:

```sh
python3 tools/ghidra_ef2000.py recover-orphans
```

To experiment with function creation, first make a separate copy of your
Ghidra workspace so manually annotated code is preserved. For a new
destination, from the repository root:

```sh
cp -a local/ghidra local/ghidra-recovery
python3 tools/ghidra_ef2000.py recover-orphans \
  --workspace local/ghidra-recovery --apply --max 25
python3 tools/ghidra_ef2000.py recover-orphans \
  --workspace local/ghidra-recovery --apply
python3 tools/ghidra_ef2000.py export-all \
  --workspace local/ghidra-recovery
```

The first applying pass is capped at 25 promotions for inspection; subsequent
passes are allowed to promote all remaining strong candidates. Each created
function gets the explicitly provisional name `candidate_orphan_XXXXXXXX`.
The script checks whether a preceding promotion already covers an address
before creating another function. All promotions use Ghidra's ordinary
function creation and are saved to the selected project. They are **heuristic
recoveries**, not confirmed original function boundaries; review callsites,
control flow and string/data references before assigning semantic names.

The test run using a copied Ghidra 11.4.2 project promoted 235 candidates
without creation failures. Full re-export then found 4,077 functions (previously
3,842), with 4,069 successful pseudo-C files and eight unchanged decompiler
failures. The disassembled bytes outside recognized functions fell from
107,887 to 49,313. These improvements must not be interpreted as proof that
all remaining bytes are executable code or that every recovered boundary is
correct. See [the findings](ghidra-findings.md) for examples.

### Optional aligned callback-table recovery

Some real callback entries start with `PUSH EDX`/`PUSH ECX` instead of a
conventional saved-register prologue. `--tier table` extends the candidate
check only for a four-byte-aligned entry pointed to from an aligned data-table
slot in the higher initialized image. The default **remains `strict`**.

```sh
python3 tools/ghidra_ef2000.py recover-orphans --tier table
# Inspect the generated TSV; on a disposable workspace only:
python3 tools/ghidra_ef2000.py recover-orphans \
  --workspace local/ghidra-recovery --tier table --apply
```

On the existing 4,077-function recovery project, this optional rule recovered
34 more functions and exported pseudo-C for each. The 4,111-function result
still has eight decompiler failures and 47,460 disassembled instruction bytes
outside recognized functions. Promotion does not imply verified function
types, entry boundaries or ABI semantics.

## Recover original callback names from dispatch tables

The game embeds data records consisting of a 32-bit guest handler address
followed by a pointer to its zero-terminated name. Some other tables have the
*same layout*, but contain enum or pilot-name data instead of function
pointers. To avoid mistaking those records for callback evidence, first
generate Ghidra's function inventory and require substantial exact function
entry matches in each candidate table:

```sh
python3 tools/ghidra_ef2000.py inventory
python3 tools/ghidra_ef2000.py callback-tables
python3 tools/ghidra_ef2000.py name-callbacks
```

`callback-tables.tsv` lists each table offset, slot, handler address, string
pointer, callback name and whether Ghidra already recognizes the entry as a
function. `name-callbacks` is **report-only by default**, writing
`callback-names.tsv` with the result for each slot. Both files stay under the
ignored Ghidra workspace. The importer SHA-checks the initialized image;
the naming Ghidra script independently verifies every referenced pointer and
name against that image before applying anything.

Once reviewed, on a **disposable copy** of the project:

```sh
python3 tools/ghidra_ef2000.py name-callbacks \
  --workspace local/ghidra-recovery --apply
# Experimental: also promote only E9 rel32 callback trampolines to verified
# existing function bodies. Requires explicit opt-in to create functions.
python3 tools/ghidra_ef2000.py name-callbacks \
  --workspace local/ghidra-recovery --apply --promote-thunks
# Broader, still opt-in recovery of already-disassembled named entries:
python3 tools/ghidra_ef2000.py name-callbacks \
  --workspace local/ghidra-recovery --apply --promote-entries --max 10
# Review the generated report and the new pseudo-C before continuing:
python3 tools/ghidra_ef2000.py name-callbacks \
  --workspace local/ghidra-recovery --apply --promote-entries
python3 tools/ghidra_ef2000.py export-all \
  --workspace local/ghidra-recovery --overwrite
```

The name script skips handlers shared by different names, names shared by
different handlers, missing function entries, and previously assigned manual
names. It creates **no functions** unless a promotion mode is specified.
That optional tier accepts only an unassigned five-byte `JMP rel32` entry
whose destination already lies inside a recognized function. Neither names
nor exported pseudo-C are original source code: Watcom calling conventions,
aliasing, data types and control flow still require manual verification.

`--promote-entries` is broader: it also accepts aligned guest addresses
pointing to instructions Ghidra has **already disassembled**, provided that
Ghidra has a DATA reference from the *exact* verified callback-table slot.
It refuses to split existing recognized function bodies or disassemble
unknown bytes. `--max N` caps function creation in either promotion mode;
the modes are mutually exclusive and require `--apply`. Review each batch
before an unrestricted pass.

Ghidra 11.4.2 analysis of a copied 4,111-function project identified four
corroborated callback tables with **308 named slots**. Without thunk creation,
188 distinct names were assigned to existing auto-named functions. The
remaining entries included shared aliases and orphan/jump-table targets;
the original manual names were retained. The optional guarded thunk tier
created another 28 functions and assigned 28 further names (216 names in
total). The final re-export found **4,139 functions, 4,131 pseudo-C files**
and the same eight assembly fallbacks. See [the findings](ghidra-findings.md).

An additional independent recovery pass on a copied #043 project promoted
50 already-disassembled, reference-backed callbacks. Ghidra recognized 14
additional functions at their direct-jump destinations. The inventory rose
to **4,203 functions**, without changing game bytes.
The opt-in `--fastcall-fallback` export produced 4,198 pseudo-C files and
five remaining assembly-only bodies; see the findings for caveats.

### Guarded disassembly of named callback targets

The remaining callback audit exposed another class of missing functions:
some verified, named table slots point to **undisassembled bytes**, not just
instructions lacking function boundaries. Ghidra must explicitly decode
those bytes before it can define a function. To avoid treating data as code,
`--promote-undisassembled` is a separate, **applying-only** tier:

```sh
# First make a disposable copy of the current analyzed workspace.
cp -a local/ghidra local/ghidra-undisassembled
python3 tools/ghidra_ef2000.py name-callbacks \
  --workspace local/ghidra-undisassembled \
  --apply --promote-undisassembled --max 3
# Inspect the report before continuing:
python3 tools/ghidra_ef2000.py name-callbacks \
  --workspace local/ghidra-undisassembled \
  --apply --promote-undisassembled
python3 tools/ghidra_ef2000.py export-all \
  --workspace local/ghidra-undisassembled \
  --custom-storage-fallback
```

Each target must have a unique verified callback name and the exact Ghidra
DATA reference from its original name-table slot. The entry must be
four-byte aligned, outside all existing functions/instructions and defined
data, and start with an observed Watcom-style `PUSH`/`MOV` instruction or
an `E9` direct jump. The latter must target an aligned, independently
plausible saved-register entry (or an already decoded instruction). The
disassembler is capped at 1 KiB and stops before the next existing
instruction; Ghidra's function finder may still follow branches beyond
that range. Both the original table pointer/name and initial code bytes
are validated before changing anything. Existing function interiors and
ambiguous aliases are never split or renamed.

On a disposable copy of the recovered #046 project, this tier promoted
**11 named handlers plus six previously undecoded jump destinations**:
4,203 → **4,220 recognized functions**. Five other named entries were
left unmodified because they lie inside existing function bodies. These
are still heuristic code boundaries, not proven original Watcom signatures.
The `--max` cap counts named handler promotions; a verified `E9` handler
may additionally create its previously absent destination function.

### Recover previously undecoded direct CALL destinations

Newly recovered functions can reveal direct `CALL rel32` targets that Ghidra
never disassembled. The `recover-calls` command audits **only calls inside
already recognized functions** and groups unresolved target addresses with
their exact callsites. Its default is read-only; no function boundaries or
instructions change during the audit:

```sh
# Copy an existing #047 recovered workspace, not the original import.
cp -a /path/to/recovered-047-workspace local/ghidra-call-recovery
python3 tools/ghidra_ef2000.py recover-calls \
  --workspace local/ghidra-call-recovery

# Review call-targets.tsv, then only on a disposable workspace:
python3 tools/ghidra_ef2000.py recover-calls \
  --workspace local/ghidra-call-recovery --apply --max 2
python3 tools/ghidra_ef2000.py recover-calls \
  --workspace local/ghidra-call-recovery --apply

# Independently examine the next layer. No automatic recursive promotion:
python3 tools/ghidra_ef2000.py inventory \
  --workspace local/ghidra-call-recovery
python3 tools/ghidra_ef2000.py recover-calls \
  --workspace local/ghidra-call-recovery

# If a target begins directly after a RET byte, audit it separately:
python3 tools/ghidra_ef2000.py recover-calls \
  --workspace local/ghidra-call-recovery --tier adjacent-ret
# Apply only after reviewing this weaker boundary evidence:
python3 tools/ghidra_ef2000.py recover-calls \
  --workspace local/ghidra-call-recovery --tier adjacent-ret --apply

# Once the desired boundary recovery is complete, refresh all pseudo-C:
python3 tools/ghidra_ef2000.py export-all \
  --workspace local/ghidra-call-recovery \
  --overwrite --custom-storage-fallback
```

An eligible target must be four-byte aligned in the executable portion of
the pinned X-32 image, lack an existing function/instruction/typed-data
definition, start with a saved-register `PUSH` opcode, and be referenced by
an exact Ghidra CALL reference whose displacement matches the raw `E8`
instruction bytes. The default `--tier padding` additionally requires one
of two observed Watcom alignment instructions immediately before the entry:
`MOV EAX,EAX` or `LEA EAX,[EAX]`.

An **opt-in** `--tier adjacent-ret` instead requires a `C3` (`RET`) byte
immediately before the aligned target. This can identify adjacent routines
that omit explicit padding, but the preceding byte is not independently
proven to be a correctly decoded return instruction. It is a weaker
boundary hypothesis, so audit its candidates before using `--apply`.
Both tiers use per-target transactions, a 1 KiB decode bound stopping before
existing instructions and typed data, and provisional names of the form
`candidate_direct_call_XXXXXXXX`. No existing function is deliberately split
or overwritten. These bounds can also cause *incomplete* decompilations:
function extents and control flow must still be checked manually.

Each pass snapshots the currently unresolved call graph; re-run the audit
after promotion to reveal calls inside newly decoded functions. The #048
disposable project used both tiers to recover 13 function entries, growing
the recognized set from 4,220 to 4,233 and reducing unresolved direct calls
in **currently decoded code** from six to zero. The remaining orphan bytes,
indirect calls, X-32 register ABI and data types are still open questions.

### Audit truncated function bodies and repair byte-verified overlaps

Ghidra can decompile a function whose stored Listing body is incomplete:
it may decode bytes as alternative instructions while following the
decompiler's control flow. The read-only `audit-flow-gaps` command finds
short gaps between a known function's fall-through address and the next
already-decoded *orphan* instruction. Each row records the two addresses,
the preceding instruction, the first missing byte, an extended window of
orphan instructions, and incoming reference types around that window. The
report also includes the owning function's full existing address ranges,
making internal body holes distinguishable from truncated tails.
**This is a triage list, not a proof that every gap is code.**

The `audit-body-holes` command provides a complementary, read-only inventory
of *every unowned interval between body ranges*, not just short fall-through
gaps. It counts decoded instructions, undefined bytes, defined data, bytes owned
by other functions, references, and the first instructions/data definitions.
Use `audit-body-holes 0x9820` to focus on one function, or omit the argument
to inspect all multi-span functions. A hole containing another function is
**not** an instruction-boundary repair candidate by default.

The separate `repair-boundaries` command has **nine pinned, guarded recipes**:

| Function | Incorrect orphan instructions | True continuation | Verified end |
| --- | --- | --- | --- |
| `0x4050` | `0x4074`–`0x407b` | `CALL 0x9c960` at `0x4072` | `RET` `0x40d9` |
| `0x433c` | `0x4349`–`0x434a` | `MOV EBP,[0x1970a0]` at `0x4345` | `RET` `0x442d` |
| `0x45a4` | `0x45d3`–`0x45d5` | `MOV EDX,0x125828` at `0x45d1` | `RET` `0x4734` |
| `0x4738` | `0x4800`–`0x480d` | `XOR EDX,EDX` at `0x47ff` | `RET` `0x4878` |
| `0x4aa0` | `0x4b32`–`0x4b34` | `CALL 0xa4342` at `0x4b30` | `RET` `0x4bf4` |
| `0x4bf8` | `0x4c35`–`0x4c3a` | `XOR EDX,EDX` at `0x4c34` | `RET` `0x4c4e` |
| `0x4e14` | `0x4f01`–`0x4f0b` | `CALL 0x7c4fc` at `0x4efd` | `RET` `0x4f17` |
| `0x9820` | `0x9c24`–`0x9c2d` and auto-data at `0x9bbc` | `MOV EDX,[0x137958]` at `0x9c1f`; branch at `0x9bbb` | `RET` `0x9e37` |
| `0xfecc` | `0xff00`–`0xff08` | `PUSH 0x181174` at `0xfefe` | `RET` `0xffb2` |

The first `0x4050` repair also disassembles a previously **undefined**
continuation; its final `RET` byte is checked against the original image.
All other verified ends were already decoded. The `0xfecc` repair is an
*internal gap*: the existing function owns two ranges, `0xfecc..fefd` and
`ff0c..ffb2`, while false orphan code blocks only the 14-byte hole between
them. Its recipe requires those exact original ranges before merging them;
it does not erase the already-owned second part.
The `0x9820` recipe verifies three original body ranges and the branch edges
into `0x9bbb` and `0x9bc0`. Ghidra's analysis had treated the four-byte
immediate of `MOV EDX,0x12f6ab` as a `pointer` data definition at `0x9bbc`.
Only that exact auto-analyzed island and two exact false orphan instructions
are cleared, with other function ownership, user symbols, comments and
non-analysis references blocking the repair. The 539 recovered bytes include
two previously disconnected sections of one routine.
The `0x4aa0` recipe is deliberately **non-contiguous**: it attaches the
verified ranges `0x4aa0..0x4b50` and `0x4b7b..0x4bf4`, but preserves the
independent 39-byte `kotsInitKOTS` function at `0x4b54..0x4b7a` and the
three alignment bytes preceding it. The recipe explicitly checks that
neighbor's identity, body size and span, and uses a disassembly address set
that excludes it. It must not claim the entire `0x4aa0..0x4bf4` interval.

```sh
# Start with a disposable copy of a recovered project, never the only copy.
cp -a /path/to/recovered-workspace local/ghidra-boundaries

python3 tools/ghidra_ef2000.py audit-flow-gaps \
  --workspace local/ghidra-boundaries
python3 tools/ghidra_ef2000.py repair-boundaries \
  --workspace local/ghidra-boundaries

# After checking the report, opt in to the pinned repairs:
python3 tools/ghidra_ef2000.py repair-boundaries \
  --workspace local/ghidra-boundaries --apply

# Confirm idempotence and review remaining candidates:
python3 tools/ghidra_ef2000.py repair-boundaries \
  --workspace local/ghidra-boundaries
python3 tools/ghidra_ef2000.py audit-flow-gaps \
  --workspace local/ghidra-boundaries
python3 tools/ghidra_ef2000.py export-all \
  --workspace local/ghidra-boundaries \
  --overwrite --custom-storage-fallback
```

The applying repair verifies each old function body, exact original bytes,
bad instruction lengths, adjacent function entry, absence of other function
bodies or typed data, and no user annotations, default-source DATA pointers,
or flow references at the discarded instructions. Only stale
analysis-generated DATA pointers are tolerated. It clears **only the
enumerated erroneous instruction spans**, disassembles the bounded
continuation, verifies every byte up to the expected `RET`, then updates
the original function's body. Each repair has its own rollback transaction;
failed recipes are reported explicitly. It cannot be used to clear arbitrary
instructions or mass-repair uncertain regions.

On the #048 disposable project, Ghidra 11.4.2 / JDK 21 grew the verified
`0x45a4` function body **45 → 401 bytes** and reduced orphan instruction
bytes **44,763 → 44,409**. On a #049 project copy, the four additional
repairs changed `0x4050` **34 → 138**, `0x4738` **199 → 321**,
`0x4bf8` **60 → 87**, and `0x4e14` **233 → 260**, recovering 280 more
function-body bytes. The generic flow-gap audit reported **17** before
the first repair, **16** after #049, **12** after #050, and **10** after
all seven recipes in #054, **9** after the #055 `0x9820` repair, and
**8** after the #056 `0x4aa0` repair.
The #054 repairs grew `0x433c` **9 → 242** and `0xfecc` **217 → 231**
(247 bytes); the #055 repair grew `0x9820` **1,021 → 1,560**
(539 additional function-body bytes). The #056 `0x4aa0` repair grew its
body **144 → 299**, adding 155 owned code bytes without crossing into
`kotsInitKOTS`.
Other candidates require case-by-case review: for example, `0xbfcc`
has a DEFAULT-source data pointer into disputed code at `0xc302`, which
must not be silently discarded.

## Export selected functions

```sh
python3 tools/ghidra_ef2000.py decompile 0xe9a24 0x8b274 0x9bd20
```

This processes the existing project **without rerunning automatic analysis**.
It generates `local/ghidra/decompiled/0x000e9a24.c` and corresponding files for
the other requested addresses. Each file includes incoming references and a
Ghidra pseudo-C body. If an address is inside a discovered function, that
containing function is exported; an unrecognized address is explicitly
disassembled and treated as a tentative function entry.

For performance investigations:

1. Run the C++ Unicorn backend with `--profile-hot-blocks --profile-copy`.
2. Collect `hot_block: ip=0x...` entries (the samples are **basic blocks**, not
   necessarily function entry points).
3. Decompile those guest addresses and identify the surrounding call graph.
4. Verify candidates against original code and guest traces before replacing
   anything with a C++ fast path. Keep optimizations opt-in until gameplay and
   timing checks pass.

The original memory-copy routine starts at `0xe9a24`. The existing
`--fast-copy` interception and its synthetic self-test make this a useful first
case for checking decompiler output against observed EAX/EDX, RET `0Ch`, and
stack behavior.

To correlate individual copy callers with the C++ backend's
`--profile-copy-callers` output, run:

```sh
python3 tools/ghidra_ef2000.py copy-callers
```

This writes `local/ghidra/copy-callers.tsv`, with each call instruction's
guest offset, its post-CALL **return IP**, and Ghidra's containing function.
Join `copy_caller: ret=0x...` with the `return_ip` column. The map includes
direct calls to `0xe9a24` and *candidate* indirect calls through the graphics
copy-function slot at `0x41f3d0`; the latter are not proven to call memcpy
without matching runtime samples. Ghidra may omit further indirect calls.

## Interpretation limits

- The initialized payload mixes code and data; static function recovery can
  misinterpret tables or jump targets. It is not a complete executable loader.
- The raw image omits runtime selector allocation, DOS memory and extender
  patches. Uninitialized BSS is represented but has no valid startup values.
- Ghidra's generic x86 calling convention is **not** the Watcom X-32 ABI.
  Decompiled parameters, return types and register preservation are hypotheses,
  particularly for register arguments, thunks and hand-written assembly.
- None of the generated `.c` files are drop-in C++ replacements. Do not commit
  copyrighted binary bytes or bulk decompiled game code; retain small
  evidence-backed annotations and behavioral models in the source repository.
- This integration's Python command tests can run without Ghidra or game data.
  A new import or full export still requires both locally.

See [loader.md](loader.md), [startup-symbols.md](startup-symbols.md), and
[execution-probes.md](execution-probes.md) for the address model and evidence.

The first successful end-to-end import and decompilation is documented in
[ghidra-findings.md](ghidra-findings.md), with cross-checked guest offsets,
resource-loader callsites, and ABI caveats.
