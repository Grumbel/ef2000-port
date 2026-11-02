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
