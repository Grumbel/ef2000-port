# Mechanical ASM baseline

The recovery sequence is executable → address/byte annotated ASM → conservative
C → readable C. Original instruction matching is the preferred static evidence;
a fingerprint match or successful decode is not proof of equivalence. Preserve
separate behavioral comparison evidence when compiler output differs.

Export all tracker entries from the validated Ghidra payload:

```sh
python3 tools/asm_inventory.py --output local/asm-inventory
```

Use a new output directory for each export. To inspect a cluster:

```sh
python3 tools/asm_inventory.py --address 0x10d80b --address 0x10d94f \
  --output local/asm-heap-growth
```

Each entry has a readable `.asm` listing and JSON instructions with original
addresses, bytes, mnemonic, operands, and direct control-transfer targets where
objdump supplies a numeric operand. The manifest records the payload digest,
selected tracker-row digest, decoder version and tracker recovery metadata.
Every decoded byte and instruction address is checked against the payload;
missing or inconsistent decoding fails the export. Original bytes stay under
the ignored `local/` directory. The CLI rejects other output locations.

These are **unverified linear windows**, using tracker `size`, not exact Ghidra
function bodies. They may include data or adjacent functions and omit detached
blocks. Overlaps are reported among selected entries; undecodable instructions
are listed in each record. No recovery state is promoted by this tool. Do not
use the sum of window sizes as recovered-code coverage. An interrupted export
has no final manifest and must not be treated as complete.

For translation, audit the body boundaries and ABI against callers, compile
with `tools/ef2000_wcc.py`, then inspect `tools/asm_diff.py` and
`tools/asm_match.py`. The latter compares instruction features, not exact
instruction equivalence. Existing isolated differential fixtures provide a
second check without running the game.

Next automation steps are exact body-range import, code/data and indirect-edge
classification, relocation-aware compiled instruction comparison, and dependency
reports for menu/cockpit blockers. Keep unresolved edges and compiler/runtime
ASM exceptions explicit rather than guessing C or treating them as complete.

Schema 2 also records `control_flow.edges`, `control_flow.review_edges`, and
per-entry `review_edge_count`. Calls and conditional branches retain a
fallthrough edge; unconditional jumps, returns, `hlt` and `ud2` do not.
Numeric targets resolve only to decoded instruction starts in the same window.
Indirect transfers, targets outside those starts (including instruction
interiors), window-exiting fallthrough, interrupt/system transitions and bad
instructions remain explicit review items. External calls are review items,
not automatically errors. This mnemonic-based report neither proves reachability
nor resolves jump tables; its input still consists of unverified linear windows.

Ghidra inventory/export now also writes `body-ranges.json`, containing half-open
function ranges and Ghidra instruction locations/lengths, with a SHA-256 of the
actual initialized program memory. Export to a separate directory to preserve
an existing pseudo-C index:

```sh
python3 tools/ghidra_ef2000.py inventory --output local/ghidra/body-inventory
python3 tools/asm_inventory.py --address 0x10d80b \
  --body-ranges local/ghidra/body-inventory/body-ranges.json \
  --output local/asm-body-heap-growth
```

Schema 3 preserves these ranges without decoding the gaps. It checks payload
provenance, span ordering and bounds, entry containment, and instruction byte
continuity separately in every span. A missing body is an error, never a fallback
to a tracker-sized window. The manifest binds the body export by its digest.
`ghidra_instruction_layout_matches` compares every decoded instruction start and
length against Ghidra, exposing embedded data or disagreeing decoders; absent
layout metadata produces `null`, not agreement. These are exact exports of
Ghidra's current ownership hypotheses, not proof that those hypotheses are
complete or correct.

2026-11-13 full check (Ghidra 12.1.4): 4,277 bodies, including 40 with multiple
ranges; no body overlaps; instruction layouts agree for 4,275. Entries 0xfa3d4
and 0x11f6d8 disagree and need review. Thirteen additional tracker entries have
no Ghidra body and are already blocked. The selected full-body export and the
explicit missing-entry tracker are under ignored `local/`; none are promoted.

The conservative ASM-to-C path is `tools/asm_to_c.py`. It emits one draft C
function per supported body, with explicit x86 GPRs, EFLAGS, PC and flat guest
RAM. It does not infer C signatures from Ghidra. Only byte-checked bodies with
matching instruction layouts are eligible; it re-decodes the bytes before
translation. Unsupported instructions or external/unresolved branches reject
the entire body with a reason. No guest-state or recovery tracker is promoted.

```sh
python3 tools/asm_to_c.py --inventory local/asm-body-inventory-20261113 \
  --output local/asm-c-draft
python3 tools/asm_c_differential.py --translations local/asm-c-draft \
  --output local/asm-c-evidence --watcom /path/to/wcc386
```

Use fresh output directories. Both CLIs enforce ignored `local/` output. The
verifier requires Python Unicorn and a host C compiler; `--watcom` also compiles
every generated body with OpenWatcom and warnings treated as errors. Source,
runtime, payload and compiled-artifact digests tie the evidence to its inputs.
A final report appears only on completion; `progress.jsonl` records completed
entries while a run is live.

The first supported instruction families are MOV/LEA/extensions, ADD/SUB/CMP,
INC/DEC/NEG/NOT, AND/OR/XOR/TEST, direct internal branches, carry-bit operations,
and 32-bit PUSH/POP/LEAVE/RET. Partial registers preserve their untouched bits.
RAM uses explicit little-endian byte accesses; integer operations use unsigned
arithmetic. Invalid PCs, fuel exhaustion and unmapped RAM return explicit
statuses. Logical operations use the Unicorn oracle's zero value for undefined
AF. This is a static-code, flat-segment intermediate representation, not native
ABI recovery, precise exception emulation or source-only game integration.

The differential runner executes original x86 without guest-service stubs and
compares all eight GPRs, EFLAGS, return PC and all 4 MiB of RAM with the host C
execution. Sixteen deterministic prepared probes per body include scalar edges
and scratch objects. Original faults and original fuel exhaustion are **inconclusive**, never
passes. If the original completes, a C fault, invalid PC or exhausted C budget
is a **mismatch**. `probes-pass` means those probes passed; it does
not establish correctness for all possible inputs, instruction coverage or
compatibility of generated OpenWatcom objects with original callers.

2026-11-13 first corpus: 404 generated drafts, 3,873 unsupported bodies; all
404 compile warning-free with OpenWatcom v2 and the host compiler. The 6,464
probes produce 4,585 passes, 1,879 inconclusive outcomes and zero mismatches;
211 bodies pass all probes, 193 remain inconclusive. Sixty-four focused tests
pass, including 1,568 synthetic program probes and exhaustive ADD/SUB byte pairs
(131,072 comparisons). Main next blockers are calls, shifts, multiplication,
external branches, ambiguous operand widths, sign-extension instructions and
x87. No reliable full-coverage or native game replacement claim is made.

The integer expansion supports SHL/SHR/SAR (counts masked to five bits), all
MUL/IMUL operand forms, ADC/SBB, CBW/CWDE/CWD/CDQ and zero-nesting ENTER. A
memory operand without a printed width can inherit its width from an encoded
register operand; an immediate cannot supply that evidence. Shift helpers use
bounded one-bit steps, preserving zero-count flags and avoiding undefined C
shifts. Multiplication sign-extends into int64_t without implementation-defined
unsigned-to-signed casts; all signed 32-bit products fit that carrier. Undefined
multiply/shift flags follow the measured Unicorn 2.1.4 policy and are compared,
not masked out. Synthetic tests cover every byte input/carry combination,
every byte value/shift count, every byte multiply pair and wider boundaries;
shift/multiply grids also run with undefined-behavior sanitization.

2026-11-13 integer corpus: 662 generated, 3,615 unsupported; all generated bodies
compile with OpenWatcom warnings-as-errors. Original-vs-C probes: 7,302 pass,
3,290 inconclusive, zero mismatches; 315 bodies pass all 16 probes, 347 remain
inconclusive. Artifacts: `local/asm-c-integer-20261113` and
`local/asm-c-integer-diff-20261113`. No native or universal-equivalence claim.

`tools/asm_c_program.py` extends the explicit-state path to direct calls and
cross-body branches without guessing a C ABI:

```sh
python3 tools/asm_c_program.py --inventory local/asm-body-inventory-20261113 \
  --output local/asm-c-program
python3 tools/asm_c_differential.py --translations local/asm-c-program \
  --output local/asm-c-program-evidence --watcom /path/to/wcc386
```

It checks the same byte/decoder provenance, then computes the greatest set of
supported bodies closed over direct calls, jumps and fallthrough dependencies.
Removing an unsupported callee removes all transitive callers, while supported
recursive cycles remain eligible. Targets must be decoded instruction starts
with unambiguous ownership. Indirect calls/jumps and unknown continuations are
unsupported. The report distinguishes syntax support from the selected closed
set and records each selected body's direct dependencies and instruction sites.

CALL writes the original return PC onto the guest stack. The generated
`ef_lift_run` dispatcher selects a body by guest PC and executes exactly one
instruction per unit of a shared budget, including CALL and RET. This avoids
host C recursion and preserves guest stack addresses even when a callee stores
or otherwise observes them. Tail jumps and entry into an owned interior block
use the same dispatcher. Unknown PCs are explicit failures; no untranslated
original-code or emulator fallback exists. The per-body entry points are step
implementation details; use the dispatcher to run a composed program.

The verifier selects the dispatcher using the distinct
`flat-x86-program-explicit-state` model tag. Its source digest is checked along
with every body and runtime header before compilation. OpenWatcom must compile
the dispatcher too. Synthetic tests compare nested frames, same-body recursion,
mutual recursion, tail jumps and saved stack/return addresses against original
x86, check transitive exclusion and ambiguous ownership, and test shared-budget
resumption and dispatcher tamper detection. These are prepared-state comparisons;
full game integration and universal equivalence remain unproven.

2026-11-13 composed corpus: 2,549 bodies are syntactically supported; dependency
closure selects 958 bodies (81,976 body bytes), including 296 with cross-body
dependencies. The other 1,591 syntactically supported bodies remain excluded.
All 958 body objects and the dispatcher compile with OpenWatcom warnings as
errors. The 15,328 probes produce 10,257 passes, 5,071 original-fault/budget
inconclusive outcomes and zero mismatches. 432 bodies pass all probes, including
117 with cross-body dependencies; 526 remain inconclusive. Artifacts:
`local/asm-c-program-20261113` and `local/asm-c-program-diff-20261113`.
Seventy focused tests pass; dispatcher tamper detection was additionally
rechecked with the four program tests. No native ABI or full-coverage promotion.

The next integer expansion includes ROL/ROR/RCL/RCR, DIV/IDIV, SETcc, BSWAP and
XCHG. Rotate helpers use bounded one-bit steps and preserve zero-count flags;
full-width ROL/ROR and effective-zero carry rotates are distinguished. Undefined
multi-bit OF follows the measured Unicorn policy, including immediate-count
forms. Division sign-extends without out-of-range unsigned-to-signed casts and
checks zero divisors, quotient bounds and INT64_MIN/-1 before C division. Status
5 reports a divide exception without modifying GPRs. SETcc reuses branch
conditions; XCHG writes memory before changing a register used by that memory's
address. All effects remain guest state operations, not native ABI guesses.

The verifier now uses one forked worker per entry, reporting whether it is in
original execution, C execution or comparison. Completed probes survive worker
failure. A crash in original execution is explicitly inconclusive; a C crash
after the original completes is a mismatch; remaining unexecuted probes are
inconclusive. Unexpected setup/comparison failures abort the report. No failed
probe is retried or counted as passing. Core dumps are disabled in workers.
This runner requires a Unix Python environment with fork support; generated C
continues to compile independently with the host compiler and OpenWatcom.

Unicorn 2.1.4 SIGFPE was reproduced in a separate process for IDIV ECX with
EDX:EAX = 0x80000000:0 and ECX = 0xffffffff. That oracle case is not claimed as
matching evidence. The C divide-exception guard is checked separately against
the quotient bound under UBSan, and the process-isolation regression executes
the problematic original in a child. Repeated ordinary #DE probes also require
a fresh Unicorn CPU after a fault to avoid retained pending exception state.
The byte/count/carry rotate grids, immediate-count checks, byte division grids,
wide quotient/fault boundaries and existing suites total 73 passing tests.

2026-11-13 rotate/divide corpus: 2,815 syntax-supported bodies; dependency
closure selects 1,178 (117,186 body bytes), including 439 with cross-body
dependencies. All body objects and the dispatcher compile warning-free with
OpenWatcom. The isolated 18,848 probes produce 12,410 passes, 6,438 original-
fault/budget inconclusive outcomes, zero worker failures and zero mismatches.
523 bodies pass all probes (172 with cross-body dependencies); 655 remain
inconclusive. Artifacts: `local/asm-c-rotdiv-program-20261113` and
`local/asm-c-rotdiv-diff-20261113`. Synthetic crash regressions are separate
from these corpus counts. Full coverage and native integration remain unproven.

The program dispatcher now checks each instruction's current guest bytes against
its payload-bound encoding before execution. Status 6 rejects changed code,
even when its effects happen to match the old instruction. The immutable byte
table is generated only under ignored local/; it is not committed. This enforces
the static-code precondition at runtime rather than assuming it from a decode.
The standalone per-body lifter still requires static code from its caller.

Host compilation now combines the hash-checked bodies and dispatcher into one
translation unit, sharing the stateless runtime helpers. Its SHA-256 is recorded
in the evidence. OpenWatcom continues compiling each body and dispatcher as
separate objects. Six focused build/program tests pass, including code mutation
with otherwise identical outputs. The guarded 1,178-body corpus reproduces
12,410 passes, 6,438 inconclusive outcomes and zero mismatches, with every Watcom
object warning-free. Artifacts: `local/asm-c-guard-program-20261113` and
`local/asm-c-guard-diff-20261113`. Self-modifying code remains unsupported and
is now detected, not silently executed using an obsolete translation.

State ABI 1 adds all six visible segment selectors. The original side binds a
zero-base GDT (code 8, data 16 and 24), and C uses the same selector profile;
null data selector aliases 0..3 are included. Each probe now checks selectors
alongside GPRs, EFLAGS, PC and RAM. Unknown selector bindings return status 7;
this is not a general nonflat GDT implementation. Generated reports carry the
state ABI and incompatible older outputs must be regenerated before rerunning.

Supported segment operations include MOV to/from selectors, 32-bit segment
PUSH/POP and LES/LDS/LFS/LGS/LSS with 16- or 32-bit offsets. The PUSH policy is
measured against Unicorn (full dword writes), including prefilled stack slots.
Far-pointer fields are read before a destination register changes their address.
Flat segment overrides preserve the bound address interpretation; nonflat
segments and full architectural protection/exception emulation remain outside
the model.

MOVS/STOS/LODS/CMPS/SCAS support byte, word and dword elements, both DF directions,
REP and the comparison repeat conditions. Copies execute element by element,
preserving overlap effects. Each repeated element consumes one budget step;
a comparison mismatch advances immediately, while a continuing repeat performs
the oracle's final zero-count check. Step-by-step resumption tests verify this
budget behavior, including zero counts, flags and all RAM.

The byte guard exposed intentional operand patching in the original decompressor.
Verified immediate suffixes are now read from guest RAM as data for MOV register-
immediate (B0..BF), register ALU groups 80/81/83, and register shift/rotate groups
C0/C1. The encoded opcode, operation group, destination register, width and prefix
must match the decoded instruction; signed 83 immediates are explicitly extended.
The guard continues checking every non-parameterized byte. This preserves fixed
instruction semantics while allowing its data parameter to change; it is not a
runtime decoder or original-code execution fallback. Modified opcodes, register
bits, addressing and all unsupported patch forms still fail status 6. Tests cover
8/16/32-bit MOV patches, wide/signed-byte/word arithmetic patches, shift-count
patches and rejection of destination-register opcode changes.

2026-11-13 selector/string corpus: 2,942 syntax-supported bodies; direct closure
selects 1,358 (139,340 body bytes), including 585 with cross-body dependencies.
All body objects and the dispatcher compile warning-free with OpenWatcom v2.
The 21,728 isolated probes produce 13,912 matches, 7,816 original-fault/budget
inconclusive outcomes, no worker failures and zero mismatches. 572 bodies pass
all probes (214 with cross-body dependencies); 786 remain inconclusive. All
79 focused tests pass. Artifacts: `local/asm-c-segstr-final-program-20261113` and
`local/asm-c-segstr-final-diff-20261113`.

The initial run correctly stopped on 15 bodies: twelve callers of a decompressor
that patches immediate parameters, and three helpers using null selector aliases
1..3. These were fixed in generic encoding/selector rules, with dedicated tests;
no mismatches were masked, ignored or promoted. A complete 15-body failure
closure passed before the full rerun. Indirect control flow, nonflat bindings,
DOS/runtime services, x87 and native integration remain open; none of these
results promotes the recovery tracker or establishes a working source-only game.
