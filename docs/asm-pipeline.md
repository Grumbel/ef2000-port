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
