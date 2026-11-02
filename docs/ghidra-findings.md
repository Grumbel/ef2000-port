# First Ghidra decompilation findings

The SHA-pinned original `_EF2000.EXE` (1,684,561 bytes, SHA-256
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`)
was imported and analyzed successfully with **Ghidra 11.1.2** and a Java 17
JDK. The import used the repository's X-32 payload extractor, the **0-based**
`x86:LE:32:default` image, and the BSS/entry-point annotations from
`EF2000Seed.java`. All addresses in this note are *guest image offsets*.

The resulting Ghidra project and exported pseudo-C are local research data:
`local/ghidra/projects/EF2000` and `local/ghidra/decompiled/`, respectively.
Neither original executable bytes nor bulk decompiler output belongs in Git.

## Cross-checked functions

| Entry | Observation | Evidence and confidence |
| --- | --- | --- |
| `0x86` | FlashTek/Watcom X-32 startup; clears BSS and calls runtime initialization | Ghidra flow matches the previously disassembled startup; strong |
| `0x9bd70` | Watcom initializer dispatcher | Traverses six-byte initializer records and calls function pointers; strong |
| `0x9bd20` | Watcom C entry, calls `0x8b274` | Direct call and runtime setup in Ghidra output; strong |
| `0x8b274` | Candidate game `main`; handles `playvideo` specially | Both Ghidra decompilation and the original instruction sequence; strong for branch, provisional function identity |
| `0x9d524` | ASCII case-insensitive string comparison | Both inputs are folded from `A`–`Z` to lowercase; returns first differing byte subtraction; strong |
| `0x8b2d8` | Separate video-mode initialization | Only called on the `playvideo` branch of `0x8b274` in the recovered entry path; probable |
| `0x8b39c` | TACTCOM initialization | Contains the `TACTCOM found ... K bytes of memory` diagnostic and a substantial setup call sequence; strong for initialization |
| `0xea270` | Resource/clump loader | Processes file/entry descriptors; its error path mentions `serclump: no file`; strong for loading, structure layout provisional |
| `0xaaee8` | Main control path | Sets up command/parser and mouse-related services, then enters repeated processing; strong for flow, purpose of individual calls provisional |
| `0xe9a24` | Guest memory-copy routine | Ghidra output agrees with disassembly and prior Unicorn hotspot sampling; strong |

### The `playvideo` branch

At `0x8b274`, EAX holds an argument count and EDX points to an argument-vector
array. The function checks for exactly two arguments, compares the second one
with the zero-terminated string **`playvideo`** at `0x18a7e8` via `0x9d524`,
and calls `0x8b2d8` if they match. It then returns zero without entering the
normal game initialization. Other invocations follow this call sequence:

1. `0xe29a0`: early runtime setup/allocation;
2. `0x8b39c`: TACTCOM/game initialization;
3. `0xea470`: floating-point exception setup;
4. `0xea270` twice, with descriptor pointers `0x1603f4` and `0x16040c`;
5. `0x9c3cf`: registers a callback;
6. `0xaaee8`: the main control path.

The two descriptor objects begin with pointers to the strings
**`clump_3.txt`** (`0x1914ac`) and **`clump_ss.txt`** (`0x1914c0`). Those
addresses and strings were checked directly against the extracted image,
not inferred solely from Ghidra's C-like output. The descriptors and their
resource lifetimes are good future targets for dynamic tracing and profiling.

### Memory-copy ABI matters

`0xe9a24` implements a forward copy in 32-byte unrolled blocks (`0xe9a3a`),
then 8-byte and byte tails. Direct disassembly verifies:

- Near-call stack arguments: `[EBP+8]` = byte count, `[EBP+12]` = source,
  `[EBP+16]` = destination.
- EAX and EDX advance with the destination and source, respectively; the
  routine restores the other saved general-purpose registers and exits via
  `RET 0Ch`.
- Ghidra's generic x86 signature describes a `void` function and does **not**
  model those EAX/EDX postconditions. Its signed-loop conditions are also
  awkwardly reconstructed. Check assembly and live traces before using its
  pseudo-C as an optimization specification.

Ghidra found **23 direct call references** to `0xe9a24` in this static image;
the list is incomplete for indirect calls and is *not* a frequency profile.
The copy-caller map additionally finds two callsites using the writable
graphics dispatch slot at `0x41f3d0`: `0xc3dc0` and `0xc3f38`. A live guest
profile verifies that both dispatched to the memcpy routine in the observed
startup run; their respective return IPs are `0xc3dc6` and `0xc3f3e`.
The earlier Unicorn sampling put 23.3% of sampled blocks at `0xe9a3a` during
one startup run. This supports studying the callers' byte counts and buffers,
but does not establish a sustained FPS improvement from `--fast-copy`.

### Real-game per-caller measurements

The C++ headless backend was built against Unicorn 2.1.3 and run for 12 wall
seconds with the supplied original game installation, SB16 DMA enabled,
`--profile-copy-callers`, and `--profile-hot-blocks`. Without either copy
acceleration option, it counted **154,811** memcpy calls moving **816,738,410**
bytes. The three leading callsites accounted for **99.35%** of those bytes:

| Guest return IP | Calls | Bytes | Interpretation |
| --- | ---: | ---: | --- |
| `0xc3dc6` | 6,072 | 388,614,144 | Indirect chunked transfer into the banked VGA memory window |
| `0xc3f3e` | 3,304 | 211,456,000 | Indirect transfer of a 64,000-byte frame into VGA memory |
| `0xc3886` | 3,299 | 211,328,000 | Direct copy of the main graphics buffer |

The first two are invisible to a search for direct calls to `0xe9a24`.
They call through `[0x41f3d0]`, as verified by disassembly. The previously
implemented `--fast-copy` handles the third site, but its conservative data
range excludes destination addresses in DOS/VGA RAM.

This led to a separate, **opt-in** `--fast-copy-vga` extension. It permits a
host copy only when the entire destination lies inside the mapped VGA
`[DOS_BASE+0xa0000, DOS_BASE+0xb0000)` bank and all other existing fast-copy
guards (source range, minimum size, flat DS, non-overlap, return address)
hold. It implies `--fast-copy`; the existing flag alone keeps its former
eligibility rules. There is no memory-mapped I/O write callback for that bank:
the VBE implementation captures its contents via `uc_mem_read`.

In a second 12-second run with `--fast-copy-vga`, **460,911** VGA-window
copies totaling **29,498,305,536** bytes took the new path, while **344,325**
ordinary large copies also took the previous path. The workload subsequently
spent many more iterations drawing; raw interrupt and byte totals are thus
**not comparable as FPS measurements**. Both the original `--fast-copy` and
new VGA mode produced byte-identical **640×400** menu-frame captures in
separate eight-second SB16 runs (PPM SHA-256
`fef5340cabcbe7356bb52225fa4508e9cd12d0f49f480d6fd646e6b78d4f9d73`).
That verifies one rendered menu state, not game-wide correctness or frame pacing.

## Reproduce and extend

From `nix develop`, with a local copy of the matching proprietary executable:

```sh
python3 tools/ghidra_ef2000.py import /path/to/_EF2000.EXE
python3 tools/ghidra_ef2000.py decompile \
  0xe9a24 0x8b274 0x9d524 0x8b39c 0xea270 0xaaee8
```

Next useful investigations are (1) checking the VGA fast path in actual
windowed gameplay and measuring FPS/frame pacing, (2) tracing which clump
files and records feed the cockpit/loading path, and (3) matching `0xaaee8`'s
repeating calls with menu and in-flight traces. Treat every unverified Ghidra
function signature as a hypothesis until checked against the Watcom register ABI.

See [ghidra.md](ghidra.md), [startup-symbols.md](startup-symbols.md), and
[execution-probes.md](execution-probes.md) for workflow and earlier evidence.

## Whole-program inventory and decompilation (bundle #041)

To move beyond hand-picked hotspots, the new `inventory` and `export-all`
commands walk the complete Ghidra function manager and create a reproducible
index, call-edge map, per-function pseudo-C and explicit disassembly fallbacks.
An independent copy of the imported project was analyzed with **Ghidra
11.4.2 / JDK 21**, preserving the original Ghidra 11.1.2 project. The export
finished on the SHA-pinned original payload with the following results:

| Static-analysis measurement | Count |
| --- | ---: |
| Ghidra-recognized functions | 3,842 |
| Functions with exported pseudo-C | 3,834 |
| Decompiler failures with assembly fallback | 8 |
| Disassembled instructions | 281,521 |
| Instruction bytes | 933,013 |
| Disassembled bytes outside recognized functions | 107,887 |
| Separate unassigned instruction ranges | 646 |
| Provisional candidate entries (range starts/after `RET`) | 1,080 |
| CALL instruction sites | 19,079 |
| Resolved in-image CALL edges | 18,527 |
| Unresolved direct in-image CALL edges | 1 |
| Indirect or externally targeted CALL sites | 555 |

The eight high-level decompiler errors report overlapping or unadjustable
input varnodes (not missing bytes). Their guest entries are `0x2a8ac`,
`0x440f4`, `0x87db8`, `0xb3084`, `0xb3168`, `0xb59a8`, `0xb8510`, and
`0xca620`. Each has a full Ghidra disassembly fallback under the corresponding
`functions/` subdirectory. Some other pseudo-C may still have incorrect ABI
assumptions despite decompiling successfully.

The unassigned-instruction report is the most important next lead for a more
complete reconstruction. For example, the region beginning at `0x5630` has
a plausible saved-register function prologue, and an orphan thunk at
`0x84bd4` jumps there. Likewise `0x7d2f8` starts with a saved-register
prologue and is the destination of a thunk at `0x84b94`. They are **good
investigation candidates**, not automatically promoted symbols: many other
orphan ranges clearly include stray data interpreted as instructions.

Thus we now have a high-level decompilation of **99.8% of discovered
functions**, but not a full semantic decompilation of EF2000. The next phase
is systematic function-boundary recovery in the 646 orphan ranges, then
annotation of dispatch tables, Watcom register calling conventions and data
structures. This is independent of Unicorn performance optimization.

## Reference-backed recovery of orphan routines (bundle #042)

A second Ghidra pass classified the 1,080 unassigned candidate entries by
their first instruction and references. **235** had both a plausible register
save/stack prologue and at least one direct jump, call or data reference.
Of those, 211 had data references, 24 jump references and one call reference
(some candidates had more than one kind). Data-referenced routines appear in
tables the initial auto-analysis did not elevate to functions.

The recovery script leaves the project untouched unless explicitly invoked
with `--apply`. On a **disposable copy** of the analyzed project, a capped
first pass promoted 25 candidates, and a second pass promoted the remaining
210. No creation failed. A fresh full-export yielded:

| Measurement | Before | After |
| --- | ---: | ---: |
| Recognized functions | 3,842 | **4,077** |
| Functions with pseudo-C | 3,834 | **4,069** |
| Assembly-only decompiler failures | 8 | 8 |
| Disassembled bytes outside functions | 107,887 | **49,313** |
| Separate unassigned ranges | 646 | **437** |
| Candidate entries in remaining ranges | 1,080 | **785** |
| Unresolved direct in-image CALL edges | 1 | **0** |

The new functions account for 58,574 bytes of previously unassigned
instructions, a reduction of about 54% of that particular uncertainty. The
total number of disassembled instructions remains unchanged: this operation
creates function boundaries around existing code; it does not discover more
machine-code bytes.

Two recovered functions illustrate why reference evidence matters:

- `0x5630` is reached by an unconditional jump at `0x84bd4`. Its recovered
  pseudo-C writes a `custom.cfg` file with mission and aircraft information,
  using embedded error and format strings as independent evidence.
- `0xdc0` is reached through a data pointer at `0x114d44`. Its pseudo-C
  invokes an X-32 fault-information formatter and an error-reporting callback.

The names remain provisional (`candidate_orphan_XXXXXXXX`), and incorrect
parameter/return types are still common under the generic x86 calling
convention. The next round should examine the remaining 437 orphan ranges,
recover additional indirect dispatch tables, and start applying verified
Watcom calling conventions and meaningful subsystem names.

## Callback-table recovery and original names (bundle #043)

The remaining orphan candidates included functions with volatile-register
entry sequences, such as `PUSH EDX`, referenced through aligned tables in
initialized memory. A second, opt-in `recover-orphans --tier table` Ghidra pass
promoted 34 additional functions on a disposable recovery-project copy. All
34 decompiled into pseudo-C. The default saved-register tier was left intact.

Further byte-level examination revealed a dense GUI callback table starting
at guest offset `0x12b4c8`. Each eight-byte record holds a handler address
and a pointer to its name in the payload's string section. For example:

| Table slot | Handler | Embedded name |
| --- | --- | --- |
| `0x12b4d0` | `0x84afc` | `guiClosedownHelp` |
| `0x12b4d8` | `0x84b04` | `UpdateMagnify` |
| `0x12b900` | `0xbe48` | `gpwClickPilot2` |
| `0x12b918` | `0xbf24` | `gpwSaveConflict` |

The same eight-byte pattern occurs in unrelated tables, including pilot
callsigns and enum strings; **most matching byte patterns are not callback
tables**. The new extractor corroborates the contents against Ghidra's
existing exact function entries. Four tables pass this check, together
containing 308 name slots. Of those, 188 distinct names could be safely
applied to already discovered auto-named functions. Repeated names, handlers
used by multiple names, and unrecognized function entries are reported and
left unchanged by the initial pass.

Of the remaining handler addresses, some are unassigned five-byte `JMP rel32`
stubs that transfer control to recognized functions. An **explicit optional**
`name-callbacks --apply --promote-thunks` pass promoted 28 of these in the
same disposable Ghidra project and assigned their names, yielding 216 distinct
original callback names in total. For example, `0x84bd4` is now identified as
the `InitCampaign` callback thunk into the previously recovered `0x5630`
routine. Its decompilation shows the underlying campaign-configuration
logic. Targets inside existing bodies are never promoted merely because
they appear in the table; the script requires a decoded standalone direct
jump and an existing destination function.

The final full decompilation in Ghidra 11.4.2 / JDK 21 yielded:

| Measurement | After #042 | After #043 |
| --- | ---: | ---: |
| Recognized functions | 4,077 | **4,139** |
| Exported pseudo-C functions | 4,069 | **4,131** |
| Assembly-only fallback functions | 8 | **8** |
| Disassembled instruction bytes outside functions | 49,313 | **47,320** |
| Separate unassigned ranges | 437 | **378** |
| Candidate entries in remaining ranges | 785 | **713** |

The new boundaries represent 1,993 bytes of previously unassigned
instructions. Named thunks can share underlying decompiled logic with their
targets and must not be counted as independent recovered source routines.
**This is still not a complete or verified original-source reconstruction**:
the unresolved ranges, eight assembly fallbacks, indirect calls, data types,
and Watcom register ABI need further work. The local generated C and Ghidra
database are ignored; only the tools, tests and analysis documentation are
included in the cumulative git bundle.

## Disassembled callback entries and decompiler limitations (bundle #044)

The four corroborated callback tables from #043 still contained unrecognized
unique-name handler slots. A Ghidra audit revealed that many were *already
disassembled* and directly referenced by a table slot; their function entries
were simply not created because they began with a register move, comparison,
arithmetic operation, or a jump into a separately unassigned routine, rather
than a conventional function prologue.

An additional opt-in `name-callbacks --apply --promote-entries` pass requires
**all** of the following before creating a function:

1. The named table entry is unambiguous, and its slot and string agree with
   the pinned imported image.
2. The guest handler offset is four-byte aligned, with a Ghidra DATA reference
   from precisely that table slot.
3. Ghidra has already disassembled an instruction starting at that offset.
4. No existing recognized function contains that address.

The script never disassembles arbitrary bytes or splits an existing function.
A capped trial created ten functions; all ten decompiled into valid Ghidra
pseudo-C. For example, `gnNewChannel` at `0x344a0` toggles a network-setting
flag, and `onopenCredits` at `0x37260` dispatches GUI state changes.
On the same disposable Ghidra 11.4.2 project, an unrestricted second pass
promoted another 40 named callbacks. Ghidra additionally created 14 ordinary
functions at previously unrecognized jump destinations, so the final gain
was **64** recognized functions, **50** newly verified original callback names,
and **2,436** fewer instruction bytes outside recognized functions.

| Static-analysis measurement | After #043 | After #044 |
| --- | ---: | ---: |
| Recognized functions | 4,139 | **4,203** |
| Exported pseudo-C functions | 4,131 | **4,195** |
| Assembly-only fallback functions | 8 | **8** |
| Disassembled instruction bytes outside functions | 47,320 | **44,884** |
| Separate unassigned instruction ranges | 378 | **320** |
| Candidate entries in remaining ranges | 713 | **645** |

Across the four embedded tables, **266 distinct original callback names**
are now applied in the recovery project. Sixteen unique-name entries still
do not identify a separate function: some are not disassembled at all,
while others fall inside a recognized function body or overlap existing
control flow. Shared aliases remain ambiguous. These require case-by-case
examination, not automatic function creation.

The same eight decompilation failures persist at `0x2a8ac`, `0x440f4`,
`0x87db8`, `0xb3084`, `0xb3168`, `0xb59a8`, `0xb8510`, and `0xca620`.
Ghidra's `decompile`, `normalize`, and `paramid` analysis styles all fail
on their input-variable overlaps; `register` and `firstpass` process the
functions but yield no high-level C. For example, the assembly at `0x2a8ac`
reads a dword from `[ESP+2]` and also manipulates the high and low halves of
registers, a pattern incompatible with generic x86 argument assumptions.
These are **not** fixed by simply relabeling entry points, and the assembly
fallbacks are preserved. Correctly modeling Watcom X-32's register and
stack conventions is a prerequisite for reliable source reconstruction.

An additional **non-mutating, opt-in** decompiler retry using Ghidra's
`__fastcall` *parameter-evaluation* model did recover pseudo-C for three of
those eight entries: `0x2a8ac`, `0x440f4`, and `0x87db8`. This is not evidence
that the original game uses Microsoft `__fastcall`; it is an experimental
way to avoid some incompatible register/stack input overlaps. Ghidra's
`__cdecl`, `__stdcall`, `__thiscall`, and combined prototype models did not
recover additional unique entries in this trial. The generated C carries a
warning and the `functions.tsv` status `written_fastcall_fallback`, making
these three distinguishable from ordinary decompilations.

With `export-all --overwrite --fastcall-fallback` the final result is
**4,198 pseudo-C files** and **five current assembly fallbacks** out of 4,203
recognized functions. The five unresolved entries are `0xb3084`, `0xb3168`,
`0xb59a8`, `0xb8510`, and `0xca620`. Their instruction-level disassembly
remains available for manual recovery.

## Experimental Watcom register model (bundle #045)

The official [Open Watcom C/C++ User's Guide](https://open-watcom.github.io/open-watcom-v2-wikidocs/cguide.html)
describes the 32-bit `__watcall` register set as `EAX`, `EBX`, `ECX`, `EDX`,
with register sets determining *priority* rather than a guaranteed register
selection order, caller-preserved state dependent on the call, stack cleanup,
and possible `EDX:EAX` 64-bit returns. X-32 game code has its own low-level
register/stack patterns, so this does **not** uniquely determine the ABI of
every recovered function.

To explore those assumptions without modifying the installed Ghidra or any
existing projects, a standalone tool now creates a disposable Ghidra copy
with a named experimental calling-convention model `__ef2000_aedb` in its
x86 Windows compiler spec. The model tries integer arguments in `EAX`,
`EDX`, `EBX`, `ECX` order, followed by the stack, with an `EDX:EAX` return
option. The original Ghidra models remain unchanged, and this one is used
**only to retry already-failing decompilations**, not to rewrite thousands
of existing inferred function signatures.

Two register orders were tested on a disposable copy of the #044 project
in Ghidra 11.4.2 / JDK 21. The `EAX, EBX, ECX, EDX` prioritization yielded
the same three new C functions as the earlier generic `__fastcall` fallback.
The `EAX, EDX, EBX, ECX` prioritization also decompiled **`0xca620`**,
previously an assembly-only function. Its C output suggests GUI coordinate
clipping and redraw logic, but it still contains suspicious inferred
`in_stack_00000002` arguments; no source-level correctness is claimed.

| Corpus measure | Default decompiler | `__fastcall` fallback | Experimental Watcom-style fallback |
| --- | ---: | ---: | ---: |
| Recognized functions | 4,203 | 4,203 | 4,203 |
| Pseudo-C exports | 4,195 | 4,198 | **4,199** |
| Assembly-only fallbacks | 8 | 5 | **4** |
| Non-default pseudo-C outputs | 0 | 3 | **4** |

The remaining four low-level failures are `0xb3084`, `0xb3168`, `0xb59a8`,
and `0xb8510`. Their decompiler errors are unchanged under both register
priorities. The custom spec is a **reproducible experiment**, not the complete
Watcom ABI, and should remain opt-in until its register and stack semantics
are corroborated against many concrete callers and return sites.

## Full recognized-function pseudo-C coverage (bundle #046)

The final four decompiler failures were input-storage reconciliation errors,
not missing machine instructions:

| Entry | Original Ghidra error | Tentative role from instruction stream |
| --- | --- | --- |
| `0xb3084` | Cannot properly adjust input varnodes | Formatting and displaying world status |
| `0xb3168` | Cannot properly adjust input varnodes | Related status/layout update |
| `0xb59a8` | Overlapping input varnodes | Floating-point coordinate calculations and clamping |
| `0xb8510` | Overlapping input varnodes | Large UI/layout data construction routine |

A disposable project was used to test Ghidra's
`FunctionUpdateType.CUSTOM_STORAGE` with explicit 32-bit `EAX`, `EDX`, `EBX`,
`ECX` inputs. This allows Ghidra 11.4.2 to decompile all four functions,
but the results still contain inferred stack arguments and uncertain
interprocedural register state. Two independent full-corpus passes yielded:

| Export mode | Pseudo-C | Decompiler failures | Last-resort custom-storage outputs |
| --- | ---: | ---: | ---: |
| Plain Ghidra + `--custom-storage-fallback` | **4,203 / 4,203** | **0** | 8 |
| Watcom-like model + `--custom-storage-fallback` | **4,203 / 4,203** | **0** | 4 |

The custom-storage retry is independent of the modified Ghidra compiler
specification. It applies temporary function signature annotations in an
explicit transaction, extracts the generated C, and rolls back that
transaction after explicitly restoring the original signature. The headless
analyzer also runs `-readOnly`, protecting the saved Ghidra project. The
wrapper uses separate ordinary and fallback passes because otherwise a
temporary annotation can influence C exported later in the same session.
The exporter labels its files `written_storage_fallback` and emits prominent
warnings. These are **readable reverse-engineering aids**, not proven C
implementations. The status `4,203/4,203` measures only functions already
recognized by this particular analysis project; the 44,884 bytes of
disassembled instructions outside recognized functions remain unclassified.

## Newly decoded, table-backed functions (bundle #047)

The remaining named callback slots from #044 were inspected directly in
Ghidra 11.4.2. Of sixteen unique-name handlers without an independent
function entry, **five** point inside existing functions (four at `RET`
instructions and one at a shared interior `MOV` instruction). These were
deliberately left alone. Eleven others have neither a decoded instruction
nor an enclosing function, despite valid pointers from the independently
verified in-game callback tables.

An additional opt-in `name-callbacks --apply --promote-undisassembled`
tier now requires the exact name-table DATA reference, aligned handler
addresses, no preexisting code/defined data, and a small allowlist of
observed Watcom-style entry instructions. For an `E9 rel32` thunk, the
destination must be an aligned, already decoded address or an untouched
address beginning with a saved-register `PUSH`. Both the thunk and a
previously undecoded destination are disassembled under tight per-seed
limits before Ghidra creates their function boundaries. The destination
gets a provisional name, not the callback's original name. The handler
thunk inherits that provisional name until explicitly renamed from its
verified table slot.

The eleven recovered original callback names are:

| Handler | Original callback name | Observed role |
| --- | --- | --- |
| `0x84bac` → `0x4738` | `gpwContinueArcade` | Arcade continuation thunk |
| `0x84bb4` → `0x4bf8` | `InitArcade2` | Arcade initialization thunk |
| `0x4d4c` | `gpwAcceptHiscore` | High-score acceptance and file selection |
| `0x84bc4` → `0x4e14` | `arcNextDifficulty` | Difficulty selection thunk |
| `0x4a90` | `arcEndgameAccept` | Endgame acceptance and score handling |
| `0x84bcc` → `0x4f18` | `kotsClickTeam` | Team-selection thunk |
| `0x522c` | `kotsAccept` | Team acceptance and slot state |
| `0x84bdc` → `0xbfcc` | `gpwLoadSaveCampaign` | Campaign load/save thunk |
| `0x84c04` → `0xd550` | `gpwCloseDebrief` | Debrief close thunk |
| `0x4dae8` | `mpClearWPRoute` | Clears a planned route |
| `0x4fe50` | `mpClickCampaign` | Campaign map interaction |

These semantic roles are provisional interpretations of the new pseudo-C;
the function boundaries and call signatures still require checking against
actual game execution. A capped trial created three named entries, followed
by a clean, uncapped run on another disposable copy of the #046 project.
The latter recovered **eleven named handlers and six additional target
routines**, all producing pseudo-C with the optional two-pass storage
fallback. Ghidra's original project was not modified.

| Static-analysis measurement | After #046 | After #047 |
| --- | ---: | ---: |
| Recognized functions | 4,203 | **4,220** |
| Pseudo-C function exports | 4,203 | **4,220** |
| Named distinct callbacks applied | 266 | **277** |
| Disassembled instruction bytes | 933,013 | **935,854** |
| Disassembled instruction bytes outside functions | 44,884 | **44,763** |
| Orphan instruction ranges | 320 | **319** |
| Unresolved direct in-image call edges | 0 | **6** |

The six newly exposed unresolved calls are not regressions in previously
known functions; they originate in the recovered bodies at `0x4738`,
`0x4bf8` and `0xbfcc`. Their five distinct target addresses are `0x45a4`,
`0x9ab30`, `0xb0404`, `0x45694` and `0xc964`. They form a strong, bounded
next recovery queue. The fact that newly decoded instructions can reveal
yet more callees is precisely why complete *recognized-function* pseudo-C
coverage is not the same as full original-program decompilation.

## Chasing newly exposed direct calls (bundle #048)

The six calls uncovered in #047 led to five previously undecoded routines.
Every target was four-byte aligned, started with a saved-register `PUSH`,
and followed an explicit Watcom-style alignment instruction. The new
`recover-calls` command independently verifies the original `E8 rel32`
displacement and Ghidra CALL reference from an already recognized caller,
and refuses to disassemble arbitrary unreachable bytes. A read-only audit
found exactly these five guarded targets; a capped trial promoted two,
followed by the remaining three on the same disposable project.

One of the new functions, `0x9ab30`, is a seven-call dispatcher. Its previously
unknown callees exposed a second recovery wave:

| Recovery tier | Newly recognized entries | Evidence |
| --- | --- | --- |
| Default alignment-padding tier | `0x45a4`, `0xc964`, `0x45694`, `0x9ab30`, `0xb0404` | Five direct-call targets from #047 |
| Default alignment-padding tier | `0xca5c`, `0x9ab6c`, `0x9b04c` | Three calls exposed inside `0x9ab30` |
| Opt-in adjacent-`RET` tier | `0x9a544`, `0x9ac84`, `0x9ae44`, `0x9b2a4` | Four aligned PUSH prologues immediately after `C3` bytes |
| Default alignment-padding tier | `0xc1ba8` | Call exposed by new function `0x9b2a4` |

The second tier was intentionally kept opt-in. A preceding `C3` is an
additional byte-level clue, not proof of a source-level function boundary;
it must be combined with an exact call reference and a plausible prologue.
For each promotion Ghidra disassembled at most 1 KiB from the seed, avoiding
known instructions/defined data and rolling back on failure. All thirteen
creations succeeded on the disposable copy of the #047 project; the original
project was preserved.

| Static-analysis measurement | After #047 | After #048 |
| --- | ---: | ---: |
| Recognized functions | 4,220 | **4,233** |
| Exportable pseudo-C functions | 4,220 | **4,233** |
| Disassembled instruction bytes | 935,854 | **939,953** |
| Disassembled bytes outside recognized functions | 44,763 | **44,763** |
| Unresolved direct in-image calls in decoded code | 6 | **0** |
| Indirect or externally targeted CALL sites | 555 | **556** |

The newly discovered chain passes through arcade/menu state handling,
configuration/UI dispatch, and small linked-structure helper routines;
those semantic descriptions are hypotheses from pseudo-C, not validated
game-runtime behavior. `0x9ab30` itself now decompiles as an ordered sequence
of eight calls, seven of which were unknown before the pass. The Ghidra
pseudo-C corpus was regenerated to incorporate updated callee names and
signatures, rather than leaving stale cached output.

**Zero unresolved direct calls applies only to currently disassembled
instructions.** There are still 44,763 already decoded bytes outside
recognized functions, large areas of undecoded image bytes, and more than
five hundred indirect/external CALL sites. Some function bodies may be
truncated by preexisting mistaken instruction boundaries; for example the
new `0x45a4` boundary stops near an orphan instruction at `0x45d3` despite
plausible continuing code. We therefore have better coverage and a cleaner
static call graph, **not a complete or verified recompilable decompile**.
The next priorities are auditing instruction conflicts at the new entries
and resolving indirect dispatch tables before trusting the high-level C.

## Repairing an overlapping Ghidra decode (bundle #049)

The newly recovered `0x45a4` routine exposed a discrepancy: Ghidra
decompiled high-level arcade state logic far beyond the **45-byte** body
recorded in its function manager. Manual `objdump -b binary -m i386`
inspection of the pinned original image showed that the direct call at
`0x45cc` returns to `0x45d1`, where the bytes are:

```asm
0x45cc  CALL 0x9ca57
0x45d1  MOV EDX,0x125828
0x45d6  MOV EAX,ESP
0x45d8  LEA ECX,[ESP+0x6c]
0x45dc  CALL 0x9ca57
```

But the old Ghidra listing started its orphan region with `POP EAX` at
`0x45d3` and `ADC AL,[EAX]` at `0x45d4`. Both are false instructions
inside the five-byte `MOV` immediate. The coherent continuation ends in
`RET` at `0x4734`, then three bytes of Watcom alignment padding before the
already recognized `0x4738` routine. A Ghidra auto-analysis DATA reference
from `0xd8f00` happens to point at the mistaken `0x45d3` decode; it is
not independent evidence of a callable function entry.

The new `repair-boundaries` script has **one explicit, pinned recipe** for
this case. It is read-only by default and requires `--apply` before any
changes. It checks the existing function and following entry, exact bytes
and old instruction lengths, absence of typed data or other function
bodies, and absence of user comments, user labels or incoming flow
references at the bad instructions. The stale analysis-generated data
reference is permitted. Under a transaction, the script clears only those
two incorrect code units, decodes the bounded continuation, and requires
the whole linear span through `RET` to exist before extending the original
function body. Any failure rolls back; repeat audits report
`already_repaired` without modifying the project.

| Ghidra 11.4.2 measurement | Before | After |
| --- | ---: | ---: |
| `0x45a4` recognized body bytes | 45 | **401** |
| Total recognized functions | 4,233 | 4,233 |
| Disassembled bytes | 939,953 | **939,955** |
| Instruction bytes outside functions | 44,763 | **44,409** |
| Orphan instruction ranges | 319 | **318** |
| Unresolved direct calls in disassembled code | 0 | 0 |

A second new command, `audit-flow-gaps`, performs an entirely read-only
search for short gaps between fall-through addresses inside recognized
functions and the next orphan instruction. On the #048 project it found
**17 possible gaps**, including the repaired one. A repeated scan of the
corrected #049 project reported **16 remaining leads**. These are *not*
auto-repaired: several resemble the same mistaken-decoding pattern in
callbacks recovered during #047 (`0x4738`, `0x4bf8`, `0xbfcc` among them),
but some may be legitimate undecoded
data or broken flow. Each requires byte-level confirmation before any
clearing of existing Ghidra instructions. The corrected project was
re-exported in full with the optional storage fallback; the existing
Watcom argument uncertainties remain. The two-pass exporter now uses a
**fresh temporary corpus** under `--overwrite` and publishes it after both
passes complete, ensuring all eight marked storage-fallback files are
newly regenerated rather than silently reused from an earlier analysis.

## #050: Four more reconstructed function tails

Continuing from the corrected #049 project, the expanded read-only
`audit-flow-gaps` report now includes nearby orphan instruction boundaries
and the source/type of inbound references. Manual disassembly of the
original pinned image, plus comparison with the existing Ghidra Listing,
confirmed four additional incorrectly truncated routines. Each verified
guest continuation was guarded with a **separate fixed repair recipe**
in `EF2000RepairBoundaries.java`, including exact original bytes,
instruction lengths, existing function body and next entry, expected
re-synchronization point, verified terminating `RET`, and a check for
user-defined, default-data or flow references at the cleared addresses.

| Function | Incorrect decoding / missing listing | Body before | Body after |
| --- | --- | ---: | ---: |
| `0x4050` | A bogus `RET 0x30be` at `0x4079` masked the remaining setup loop | 34 | **138** |
| `0x4738` | False `ROR`, `ADD`, `XOR` sequence at `0x4800`–`0x480d` | 199 | **321** |
| `0x4bf8` | False `SAR` at `0x4c35` inside the `XOR EDX,EDX` / `MOV EAX,...` sequence | 60 | **87** |
| `0x4e14` | False `ADD`, `ROR`, `OR` sequence at `0x4f01`–`0x4f0b` | 233 | **260** |

The `0x4050` case was the most consequential: after an actual `CALL` at
`0x4072` the Listing had no instructions at all beyond the spurious
`RET 0x30be` to the next function. The original bytes instead decode
into a coherent loop, with the final `RET` at `0x40d9`, Watcom padding
at `0x40da` and a separate prologue at `0x40dc`. Its end byte was
verified directly because Ghidra had not previously disassembled it.

The `0x4738`, `0x4bf8` and `0x4e14` cases each reconnect to existing,
coherently decoded orphan instructions. All four transactions were
successfully applied on the disposable #050 project and extended the
corresponding existing functions by a combined **280 bytes**. The fifth
recipe, `0x45a4` from #049, was already repaired and left unchanged.
The `0xbfcc` gap remains deliberately **unrepaired**: the disputed
`0xc302` decode has a DEFAULT-source DATA reference from `0x128148`,
so the automatic repair safety policy correctly excludes it. The
remaining gap list is for individual investigation, not bulk clearing.

| Ghidra 11.4.2 measurement | #049 | #050 |
| --- | ---: | ---: |
| Recognized functions | 4,233 | 4,233 |
| Summed function body bytes | 895,546 | **895,826** |
| Disassembled instructions | 283,570 | **283,602** |
| Disassembled instruction bytes | 939,955 | **940,057** |
| Instruction bytes outside functions | 44,409 | **44,231** |
| Orphan instruction ranges | 318 | **314** |
| Short flow-gap audit candidates | 16 | **12** |
| Direct-call edges | 18,785 | **18,790** |
| Unresolved direct-call edges | 0 | **0** |

The full #050 export after repair still produced **4,233/4,233**
pseudo-C functions with **eight freshly regenerated custom-storage
fallbacks**, no failed decompilations, and no unresolved direct calls.
All five recipes were re-audited as `already_repaired` (idempotent).
This improves Listing ownership and control-flow recovery; it does not
make the Ghidra pseudo-C a source-equivalent, compilable reimplementation
of the game, nor does it resolve uncertain Watcom calling conventions.

## #054: High-score insertion and an internal function-body hole

The short flow-gap audit now includes the owner's **actual body spans**.
This distinction matters: a function can be truncated at its tail, or it
can already own disconnected blocks past a misdecoded instruction. The
second case must preserve those existing blocks. A new pinned `0x433c`
recipe and a separate two-span `0xfecc` recipe have been checked against
the original X-32 bytes and applied to a disposable #050 project copy.

| Function | Original Listing ranges | Restored body | Correction |
| --- | --- | --- | --- |
| `0x433c` | `0x433c..0x4344` (9 bytes) | `0x433c..0x442d` (242 bytes) | Wrong `SBB` at `0x4349` overlapped the immediate of `MOV EBP,[0x1970a0]` at `0x4345` |
| `0xfecc` | `0xfecc..0xfefd`, `0xff0c..0xffb2` (217 bytes) | `0xfecc..0xffb2` (231 bytes) | Wrong `ADC`, `ADD`, `ADC` at `0xff00..0xff08` split a real `PUSH 0x181174` and an indirect `CALL` |

The `0xfecc` recipe verifies every byte of the **two previously owned
ranges**, the 14-byte hole, the original `RET` at `0xffb2`, and the
independent prologue at `0xffb4`. It only clears the enumerated orphan
instructions; the existing second half of the function is retained and
rejoined transactionally. Both new recipes and the five previous ones
then reported `already_repaired` in a repeated audit.

### Recovered high-score data layout

The fully decoded `0x433c` routine is an apparent high-score insertion
function. It compares a current score against ten existing records,
shifts lower-scoring records down by one 25-byte slot, inserts a new
name/type/score record, and returns an insertion slot or a no-slot
sentinel. The separately repaired `0x4050` routine initializes this same
table from fixed-width seed names and starting values. The record
boundaries are corroborated by literal 20-byte-spaced, NUL-terminated
names in the initialized image at `0x121292` (not copied into the repo).

| Guest address | Offset | Size | Inferred field |
| --- | ---: | ---: | --- |
| `0x1970a4 + 25*i` | `+0x00` | 20 | NUL-terminated pilot/name storage |
| `0x1970b8 + 25*i` | `+0x14` | 1 | Mode/category byte (initial values 1–3; exact meaning unknown) |
| `0x1970b9 + 25*i` | `+0x15` | 4 | Little-endian score value |
| `0x1970a0` | outside record | 4 | Current score used in insertion comparison |

This is a **provisional structural inference**, not a validated C++ ABI.
In particular, the guest's packed 25-byte stride, register arguments,
and its string-copy behavior need explicit modeling before implementing
a native replacement.

| Ghidra 11.4.2 measurement | #050 | #054 |
| --- | ---: | ---: |
| Recognized functions | 4,233 | 4,233 |
| Summed function body bytes | 895,826 | **896,073** |
| Disassembled instruction bytes | 940,057 | **940,063** |
| Instruction bytes outside functions | 44,231 | **43,990** |
| Orphan instruction ranges | 314 | **312** |
| Short flow-gap audit candidates | 12 | **10** |
| Unresolved direct-call edges | 0 | **0** |

The refreshed #054 corpus still has **4,233/4,233 pseudo-C exports**, eight
newly generated storage-fallback files and zero export failures. The
remaining 10 gaps include multi-span functions with nontrivial, larger
disputed regions; none was bulk-repaired. For example, `0x9820` already
owns three disconnected code ranges, and the gap near `0xbfcc` has an
external DEFAULT-source data reference into its suspect decode.

## #055: Rejoining a three-span routine without losing its branch target

The `0x9820` function initially owned three disconnected Listing ranges:
`0x9820..0x9bba`, `0x9bc7..0x9c1e`, and `0x9e2e..0x9e37`.
The first missing interval (`0x9bbb..0x9bc6`) was a **real conditional
branch path**, not unrelated data. Its first instruction is
`MOV EDX,0x12f6ab` at `0x9bbb`, reached by the default conditional jump
at `0x9ba9`; the jump at `0x9bb9` reaches `0x9bc0`. Ghidra had defined
the four immediate bytes at `0x9bbc..0x9bbf` as an automatic `pointer`
datum, preventing the branch path from being disassembled.

The second hole (`0x9c1f..0x9e2d`) began with the real six-byte
`MOV EDX,[0x137958]` at `0x9c1f`. Orphan decodes `ADD` at `0x9c24`
and `CMP` at `0x9c2a` consumed bytes from the true `MOV`, `MOV`, `XOR`,
`CMP` sequence. The correct stream resynchronizes at the previously
decoded `JLE` at `0x9c2e`, and terminates at the already-owned
`RET` at `0x9e37`, immediately before the `0x9e38` function prologue.

The eighth pinned repair requires all three **exact old body ranges**,
both original branch references, the byte pattern and type of the sole
false pointer datum, all wrong instruction lengths, the resynchronization
instruction, the final `RET`, and the following prologue. It rejects other
function ownership, user symbols/comments and non-analysis references
in the cleared spans. It disassembles both holes and extends only the
original function's body in a transaction; a repeat reports
`already_repaired`. The body is now one linear **1,560-byte** interval,
up from 1,021 bytes. This restores **539 code bytes** to their owner.

The recovered path participates in a routine that traverses linked
records and 124-byte (`0x7c`) subrecords, calls formatting/output helpers,
and handles several status flags. It is not yet a validated named native
routine: the refreshed pseudo-C for `0x9820` is byte-for-byte identical
to the previous export despite the improved Listing ownership.

### General multi-span audit

The new **read-only** `audit-body-holes` tool reports all intervals between
disconnected function body ranges, including instruction windows,
undefined/defined data counts, references, and ownership by other functions.
On the repaired #055 project it found **45 holes** across the remaining
multi-span functions. **18** contain bytes owned by other functions and
**4** contain defined data (all four also overlap other functions); **27**
have neither of those two obstructions. This is a triage aid, not license
to join the 27 remaining intervals: actual control flow, references and
instruction boundaries must still be checked individually.

| Ghidra 11.4.2 measurement | #054 | #055 |
| --- | ---: | ---: |
| Recognized functions | 4,233 | 4,233 |
| Summed function body bytes | 896,073 | **896,612** |
| Disassembled instruction bytes | 940,063 | **940,080** |
| Instruction bytes outside functions | 43,990 | **43,468** |
| Orphan instruction ranges | 312 | **311** |
| Short flow-gap audit candidates | 10 | **9** |
| Unresolved direct-call edges | 0 | **0** |

The two-pass #055 export still produced **4,233/4,233 pseudo-C files**, with
eight temporary-storage fallbacks and zero failures. No executable, loaded
image, Ghidra project, or generated pseudo-C is committed to this repo.

## #056: Recovering a caller around a separate KOTS callback

The next short fall-through gap was at `0x4b30`, in `FUN_00004aa0`.
The true instruction is `CALL 0xa4342` (`e8 0d f8 09 00`), interrupted by
false orphan decodes `CLC` at `0x4b32` and `OR` at `0x4b33`.
The Listing resynchronizes at `MOV AX,[0x121e44]` at `0x4b35`. The next
branch ladder tests values 1, 2, and 3 and chooses one of three arcade
text-file paths (`arcroo.txt`, `arcpro.txt`, `arctop.txt`), confirmed by
the original initialized image and the static copy-source addresses.

It would be wrong to extend `FUN_00004aa0` into one contiguous function:
the distinct, previously named `kotsInitKOTS` function occupies
`0x4b54..0x4b7a`, preceded by three bytes of alignment at `0x4b51`.
The corrected `FUN_00004aa0` body instead consists of **two ranges**:

| Ownership | Address range | Size |
| --- | --- | ---: |
| `FUN_00004aa0` | `0x4aa0..0x4b50` | 177 |
| Alignment (not claimed) | `0x4b51..0x4b53` | 3 |
| `kotsInitKOTS` (preserved) | `0x4b54..0x4b7a` | 39 |
| `FUN_00004aa0` | `0x4b7b..0x4bf4` | 122 |

The new pinned recipe verifies the original 144-byte prefix, the exact
false orphan instructions and true continuation, the neighbor's name and
39-byte body, both new body ranges, the `RET` at `0x4bf4`, following
three-byte alignment and the independent prologue at `0x4bf8`. Ghidra
11.4.2 applied it to an isolated #055 project copy and repeated auditing
returned `already_repaired`. The parent body grew **144 → 299 bytes**
without overlapping the callback.

This also strengthens the earlier high-score reverse engineering:
the independently identified `kotsInitKOTS` callback reaches the
`FUN_0000433c` high-score insertion routine through a conditional jump into
shared code at `0x487c`, which calls `0x433c`. The decompiler incorporates
that shared control flow into the callback's pseudo-C, but **the callback
does not directly CALL `0x433c`**. That path checks the returned slot for
failure, uses the record's 25-byte stride, and invokes save/update helpers.
The layout at `0x1970a4` is therefore used by a real original-game callback,
not just an isolated inferred table.

| Ghidra 11.4.2 measurement | #055 | #056 |
| --- | ---: | ---: |
| Recognized functions | 4,233 | 4,233 |
| Summed function body bytes | 896,612 | **896,767** |
| Disassembled instruction bytes | 940,080 | **940,082** |
| Instruction bytes outside functions | 43,468 | **43,315** |
| Orphan instruction ranges | 311 | **309** |
| Short flow-gap audit candidates | 9 | **8** |
| Unresolved direct-call edges | 0 | **0** |

The regenerated #056 pseudo-C file for `0x4aa0` is byte-for-byte identical
to #055: the decompiler had already followed this control flow, although
the Listing had not assigned those blocks to their correct function.
This is an ownership and instruction-boundary recovery, not a new native
replacement. The entire **4,233/4,233** corpus exported again with eight
temporary-storage fallbacks and zero failures.
The general body-hole audit now reports **46** intervals rather than 45:
the newly recognized split body of `0x4aa0` correctly exposes the
intervening `kotsInitKOTS` function as a hole instead of hiding it.

## #057: Whole-program static reference and subsystem-affinity inventory

The boundary-by-boundary recovery approach has diminishing returns for a
**complete source reconstruction**. This pass adds an independent, read-only
`function-facts` Ghidra script and an offline `subsystem-map` pass to build a
whole-program investigation queue without rerunning the decompiler for every
question. The source remains 4,233 recognized functions and 4,233 exported
pseudo-C bodies from #056; this pass does not claim a native replacement.

On the recovered #056 Ghidra 11.4.2 project, the new fact export measured:

| Static inventory | Observed |
| --- | ---: |
| Recognized functions | **4,233** |
| Explicit Listing DATA references to guest memory | **39,642** |
| Operand address candidates in the global-address window | **6,038** |
| Function-owned cross-boundary JUMP instructions | **159** |
| Distinct direct-CALL function pairs from `calls.tsv` | **11,370** |
| Distinct cross-boundary JUMP function pairs | **133** |
| Functions with at least one explicit data reference | **3,163** |
| Functions with at least one address candidate | **1,311** |
| Complete-linkage candidate groups (threshold 8, max 16 functions) | **562** |

Literal operands and Ghidra's established DATA references have different
provenance and are not merged into an asserted xref. Coarse shared address
regions and shared callees receive lower weights than exact addresses; common
tokens are discarded. Candidate groups require a qualifying affinity between
**every pair** of members, specifically to avoid a spurious giant component
joined through weak chains. Neither groups nor scores establish program
semantics, pointer types or Watcom X-32 parameter conventions.

The first four seed neighborhoods yielded actionable high-score/arcade leads:

| Seed | Closely related original routines | Independent evidence |
| --- | --- | --- |
| `0x4050` (score lookup) | `0x40dc`, `0x433c` (score insertion) | Shared exact references to `0x1970a4`, `0x1970b8`, `0x1970b9` |
| `0x433c` (insertion) | `0x487c`, `0x40dc`, `0x432c` | Exact score-table globals and a direct CALL from `0x487c` |
| `0x4aa0` (arcade/save path) | `gpwAcceptHiscore` (`0x4d4c`), `gpwMissionSelected` (`0x7c328`), `gpwChangeDifficulty` (`0x4cf4`) | Shared UI/save-state globals, including `0x121e44` and `0x1f8fc8` |
| `kotsInitKOTS` (`0x4b54`) | `InitArcade` (`0x4c50`), `0x487c` | Shared callees and an actual conditional JUMP at `0x4b6b` into `0x487c` |

The cross-function control-flow inventory resolves an ambiguity in the #056
decompiler output: `kotsInitKOTS` branches to `0x487c` at `0x4b6b`; code at
`0x487c` calls `0x433c`. The pseudo-C follows this shared path, but the
original `kotsInitKOTS` instruction body contains **no direct CALL** to
`0x433c`. Both edges are now represented explicitly without conflating them.

Regenerate the local reports with `ef2000-ghidra function-facts` followed by
`ef2000-ghidra subsystem-map --seed 0x4aa0` (after a complete `export-all`
inventory). See [the workflow](ghidra.md) for file formats, scoring limitations
and command options. All four fact TSVs, group reports, original executable,
Ghidra project and pseudo-C remain local ignored research data.
