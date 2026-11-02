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

`nix develop` includes Ghidra and sets `GHIDRA_HOME` to the Nix store's
`lib/ghidra` installation. Run from the EF2000 repository root:

```sh
nix develop
python3 tools/ghidra_ef2000.py import \
  ../EF2000/c/EF2000/_EF2000.EXE
```

Outside the development shell, `nix run .#ghidra` opens the GUI and
`nix run .#ghidra-headless -- ...` runs the headless analyzer. With a separate
Ghidra installation, set `GHIDRA_HOME=/path/to/ghidra`, or supply
`--analyze-headless /path/to/ghidra/support/analyzeHeadless` to the Python tool.
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
