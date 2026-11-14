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
with unambiguous ownership. Near 32-bit indirect calls/jumps read their register
or memory target before any CALL stack push, then use the checked PC dispatcher.
Unknown targets fail explicitly. Direct dependency closure does not establish
indirect-target coverage; selected bodies record `dynamic_transfer_sites`, and
`closure_scope` records that limitation. Far and 16-bit indirect transfers remain
unsupported. The report distinguishes syntax support from the selected set and
records each selected body's direct dependencies and instruction sites.

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

2026-11-13 checked-indirect corpus after reviewed boundary recovery: 1,956
selected bodies (242,587 body bytes), all Watcom warning-free. Of 31,296 probes,
17,600 match, 13,691 are inconclusive and five fail at untranslated dynamic
targets across three bodies (synthetic callbacks into copyright data and an
unowned x87 island). The gate remains nonzero. 720 bodies pass all 16 probes.
Artifacts: `local/asm-c-observed-program-20261113` and
`local/asm-c-observed-diff-20261113`; 83 focused tests pass. Direct closure is
not dynamic target coverage or a native game integration proof.

Counted branches now include LOOP/LOOPE/LOOPNE (with alias spellings),
JECXZ and JCXZ. LOOP decrements ECX with 32-bit wraparound without changing
EFLAGS; its conditional forms use the existing ZF. JCXZ tests only CX and
preserves all of ECX. The separately decoded `addr16 loop` prefix form remains
unsupported. SHLD/SHRD support 16/32-bit register or memory destinations,
register sources, and immediate/CL counts. A bounded concatenated-pair helper
avoids shifts by the C type width and matches the measured Unicorn policy for
undefined OF/AF and oversized 16-bit counts; zero counts preserve flags.
Synthetic tests cover all 256 count values across boundary operand pairs,
with undefined-behavior sanitization, memory destinations and register aliases.

2026-11-13 count/shift corpus: 2,090 bodies, 269,778 body bytes, all Watcom
warning-free. 18,500 probes match, 14,935 are inconclusive and the same five
uncovered-target failures remain. The 134 newly selected bodies contribute
900 matches and no new failure; results for previous bodies are unchanged.
750 bodies pass all 16 probes. Artifacts: `local/asm-c-countshift-program-20261113`
and `local/asm-c-countshift-diff-20261113`. All 85 focused tests pass, with
cross-body count-branch composition additionally rechecked. Full reliability
still needs x87, runtime services, unresolved boundaries/targets and stronger
application evidence; the CLI gate remains nonzero.

The x87 arithmetic dependency can be built separately from a pinned official
SoftFloat 3e archive. Third-party source and built artifacts stay ignored:

```sh
python3 tools/asm_softfloat.py --archive /path/to/SoftFloat-3e.zip \
  --output local/asm-softfloat --watcom /path/to/wcc386
ASM_SOFTFLOAT_BUILD=local/asm-softfloat python3 -m unittest tests.test_asm_softfloat
```

The builder verifies the archive SHA-256 before extraction, rejects unsafe
members, requires a fresh output directory and records source/header/object/
library digests and compiler commands. It builds the non-FAST_INT64 8086
specialization, enables upstream round-to-odd support, and removes only an
unreferenced `roundPack` label after checking for incoming gotos. Upstream
license notices remain in the extracted files. Host compilation uses C99 and
-Werror (upstream does not enable -Wall); Watcom uses -w1 -we. The shared host
link rejects unresolved symbols. No dependency source is vendored or downloaded
by the tool.

The opt-in test requires x86-64, Unicorn and binutils. It compares add/sub/mul/div
on canonical normal/zero operands in all three precision modes and four rounding
modes. Raw 80-bit values are compared with both Unicorn and host x87; exception
flags are compared with host x87. The hardware probe saves/restores the full x87
environment and register stack. `arithmetic-evidence.json` records Unicorn status
disagreements separately. This is an arithmetic prerequisite, not generated x87
instruction support or an architectural-state equivalence claim. Noncanonical
operands, input denormals, tag/stack/environment handling, unmasked exceptions,
transcendentals, and application integration remain pending.

State ABI 2 adds eight physical x87 registers as three explicit 32-bit words
(significand low/high and sign/exponent), plus control/status/tag words and
instruction/data pointers, selectors and opcode. TOP remains in the status
word. No host `double`, `long double`, or packed native 80-bit struct determines
the state layout. ABI 1 manifests are rejected before compilation; regenerate
old translation artifacts. The verifier initializes the oracle and C from the
same captured x87 state and compares every raw register, including empty-slot
payloads, and all eight environment fields. Current full-corpus probes begin
with an empty x87 stack and control word 0x37f; separate seeded tests cover
nonzero payloads, TOP, tags and environment preservation.

FNINIT resets the environment and tags while retaining physical register
contents. A hardware FLDENV/FNINIT/FNSTENV check verifies that it clears a seeded
opcode. Unicorn 2.1.4 leaves that opcode stale; the differential regression
expects and preserves an explicit mismatch for that case. The default zero-
opcode probe does not expose this gap. No environment field is masked to pass
an x87 comparison, and snapshot coverage is not proof of the oracle's hardware
accuracy. Arithmetic and other stack/environment operations remain unsupported.

The ABI 2 rerun (`local/asm-c-x87-state-diff-20261113`) preserves every previous
per-body outcome: 18,500 matches, 14,935 inconclusive and the same five target
coverage failures across 2,090 bodies. Added x87 comparison causes no new
failure under the empty-stack probe profile; it does not prove broader x87
support. The gate remains nonzero.

Control/status lifting includes FNCLEX, FNSTCW, FNSTSW (memory or AX), FLDCW,
and the SAHF/LAHF flag transfer used by x87 comparisons. FNCLEX preserves TOP
and condition codes; stores preserve flags and all unrelated state. FLDCW
normalizes reserved control bits and updates ES/B from pending flags and the
new masks. Pending exception delivery at entry remains an explicit status 8;
FWAIT and broader unmasked-exception handling remain unsupported.

The hardware control test covers all 65,536 control words with zero or IE/PE
flags, plus all 64 flag/mask combinations (135,040 cases). It saves/restores
the full host x87 state and clears newly pending exceptions before restoring.
Unicorn 2.1.4 retains reserved CW bits and changes the instruction pointer on
FNSTSW AX; hardware proves the C normalization and unchanged pointer. Seeded
regressions keep those differences as explicit differential mismatches. They
are not successful original-vs-C comparisons or masked fields. AH/flag tests
cover all 256 AH values against every arithmetic-flag combination.

Raw stack lifting now includes FLD1/FLDZ, FLD ST(i), FXCH, FST/FSTP ST(i),
FFREE, FCHS/FABS and FINCSTP/FDECSTP. `tools/asm_x87.py` supplies shared
physical-register/tag/TOP effects; the renderer separately updates guest
instruction pointer, CS and opcode, retaining the data environment for these
register instructions. Unsupported encodings/prefixes remain rejected.
Stack underflow/overflow sets IE/SF and C1; masked faults produce the hardware
indefinite result. Unmasked invalid operations preserve register/tag/TOP state
and set the deferred exception summary. A later waiting operation stops with
status 8 until exception delivery is implemented. For FLD ST(i), source
underflow takes precedence over destination overflow.

Hardware tests cover every TOP, every occupied-slot mask, all eight register
indices, and masked/unmasked invalid operations: 188,416 stack-effect cases.
Payloads include normal values, both zeros, infinities, quiet/signaling NaNs,
denormals and unnormal encodings, including payloads retained in empty slots.
Another 2,944 generated-body cases compare register/status/tag effects and
instruction/data environment with hardware (host IP/CS relocated to the guest
context); both grids run C with undefined-behavior sanitization. The hardware
probe saves/restores full host x87 state. This proves the tested instruction
families, not broader arithmetic, memory conversion or application equivalence.
Known Unicorn environment/status differences remain explicit; no field is
masked in the bulk verifier to get a pass.

The stack-runtime corpus rerun (`local/asm-c-x87-stack-diff-20261113`) keeps all
2,090 per-body counts unchanged: 18,500 matches, 14,935 inconclusive and the
same five target failures. All 93 tests pass. Syntax support rises to 3,600;
dependency closure and selected byte coverage remain unchanged. Full x87 and
application-level reliability remain pending; the bulk gate stays nonzero.

Extended-memory support includes FLD/FSTP TBYTE with exact ten-byte transfers.
There is no host floating conversion: signaling NaNs, denormals, pseudo-
denormals and unnormal encodings retain their bits. A pending x87 exception
stops before address access; complete operand bounds are checked before stack
fault/status effects or RAM writes. Masked stack faults load/store indefinite;
unmasked invalid leaves stack and memory unchanged while recording the pending
exception. Instruction and data environment fields still update for deferred
stack faults; pending delivery or invalid RAM stops before those updates.

The renderer derives default DS/SS from ModRM/SIB base, including no-base SIB
and EBP used as index, and honors encoded segment overrides. Unsupported
address/operand-size prefixes are rejected. Tests compare 81,920 generated-body
cases with hardware across every TOP/occupancy mask, ten payload classes and
masked/unmasked invalid, including instruction/data pointer relocation, opcode,
raw registers/tags/status, RAM and return state. Child-process hardware probes
confirm pending-#MF priority over memory faults and memory faults over stack
faults; C bounds/pending checks also test no partial state or RAM mutation.
This is a checked flat-RAM model, not complete page-fault or exception delivery
emulation. Twenty addressing variants compile with Watcom warnings-as-errors.

Extended-memory corpus: 2,091 bodies, 269,815 bytes, all Watcom warning-free.
All 95 tests pass. The full run has 18,500 matches, 14,937 inconclusive and 19
mismatches. Previous outcomes are unchanged; new helper 0x120a2d contributes
fourteen status/opcode differences and two inconclusive reads. Execution of its
actual original bytes on host x87 confirms C's disputed status/opcode in all
fourteen cases (long-mode probe, limited to those fields). These remain explicit
Unicorn mismatches; the gate is nonzero. Artifacts:
`local/asm-c-x87-memory-diff-20261113` and
`local/asm-x87-original-memory-probe-20261113/evidence.json`.

FLD DWORD/QWORD widens IEEE binary32/binary64 exactly into raw 80-bit state
using integer fields and bounded subnormal normalization. Precision and rounding
control do not round a load. Signed zeros, infinities and NaN payloads/signs
are retained; signaling NaNs are quieted with IE. Source denormals set DE and
produce an exact normalized extended value. A full destination stack takes
priority over source-format exceptions. Unmasked IE suppresses the push;
unmasked DE still commits the normalized load and records pending delivery.
The normal bounds/segment/environment rules also apply. Pending delivery still
stops with status 8; no host floating conversion or original-code fallback.

Hardware tests compare 473,088 generated-body cases: every exponent, both signs,
key mantissa boundaries, all standard precision/rounding modes, each TOP and
occupancy mask, and independent IE/DE masks. They compare raw values, tags/status,
flags, opcode and relocated instruction/data environment. C runs under UBSan;
bounds/pending tests include the last valid operand span. Twenty addressing
variants also compile warning-free with Watcom. Arithmetic and narrowing stores
remain unsupported and need their own rounding/exception validation.

Real-load corpus: 2,096 bodies, 270,162 bytes, all Watcom warning-free. All 96
full-suite tests pass, with focused opcode tests added afterward. Of 33,536
probes, 18,500 match, 14,969 are inconclusive and 67 fail. Previous outcomes
are unchanged; three new bodies add 48 saved-opcode-only mismatches (0x504/
0x505 versus Unicorn zero), confirmed by hardware addressing-form probes.
Two new bodies have only inconclusive probes. The bulk gate remains nonzero;
none of these differences is waived. Artifacts:
`local/asm-c-x87-real-load-diff-20261113`.

FILD WORD/DWORD/QWORD loads signed 16/32/64-bit integers exactly into extended
state. Unsigned magnitude/normalization avoids signed overflow at INT64_MIN
and keeps all input bits regardless of precision/rounding control. Zero loads
positive zero. Stack overflow follows the shared masked/deferred-invalid rules;
no data inexact/denormal exceptions arise from integer widening. Bounds and
pending delivery precede stack changes; environment metadata uses encoded
operand addressing as for real loads.

Hardware tests compare 879,096 generated-body cases under UBSan: all 65,536
16-bit inputs in every standard precision/rounding mode; wider signed extrema
and neighbors of powers of two (including values beyond 2^53); every TOP and
occupancy mask with masked/unmasked IE. Full cases compare retained physical
registers, status/tags, opcode, relocated pointers/selectors, flags and RAM.
Bounds tests cover undersized/wrapping spans and the final valid span; isolated
hardware fault-priority probes cover each width. Thirty addressing variants
compile with Watcom warnings-as-errors. Later stores/arithmetic and dependent
bodies remain unsupported; this does not imply new closed body coverage.

Integer-load runtime rerun (`local/asm-c-x87-integer-load-diff-20261113`) keeps
all 2,096 per-body outcomes unchanged: 18,500 matches, 14,969 inconclusive and
67 existing mismatches. The host library is byte-identical to the preceding
run because FILD does not yet enter selected bodies. All 98 tests pass. FILD's
hardware coverage is separate from bulk/game coverage; the gate stays nonzero.

The narrowing conversion adapter is built separately from the instruction
renderer:

```sh
python3 tools/asm_softfloat.py --narrow-from local/asm-softfloat \
  --output local/asm-narrow --watcom /path/to/wcc386
ASM_SOFTFLOAT_BUILD=local/asm-softfloat python3 -m unittest \
  tests.test_asm_softfloat tests.test_asm_x87_narrow
```

`tools/asm_x87_narrow.c` consumes three raw extended words and returns binary32/
binary64 bits plus conversion flags and C1. It shares the dependency's platform
header/ABI, handles unsupported encodings and pseudo-denormals, and computes C1
from a second conversion toward zero. Rounding mode, exception flags and
tininess settings are restored. The pinned non-TLS library requires serialized
calls; the existing verifier isolates entries by process. The builder validates
source/header/platform/library digests and Watcom object bindings before making
an adapter output, then records adapter source/object/library hashes. All
third-party material stays under ignored local/.

277,984 masked-conversion cases match hardware values, flags and C1, including
all extended exponents in four rounding directions and boundary/special payloads
across precision controls. Invalid arguments and library-context restoration
are checked. This adapter does not yet implement FST/FSTP memory effects,
exception masks, suppressed stores or pop timing. Store instruction integration
and independent validation of those effects remain required; existing corpus
coverage and its nonzero gate are unchanged.

Generated FST/FSTP DWORD/QWORD now uses the pinned narrowing adapter. IE/OE/UE
that are unmasked suppress the memory write and pop; precision can commit both
and then leave a pending exception. Exact subnormals also raise unmasked UE.
Earlier sticky flags are retained; suppressed OE/UE do not add PE or C1.
Empty-stack invalid uses the indefinite value when masked. Full RAM bounds
and pending delivery checks precede conversion/stack effects.

Only bodies that need narrowing enable the optional header section. It is
outside the base header guard so a later body can enable it in the combined
translation unit. Manifests declare `required_backends` and bind the adapter
source digest. The differential runner requires `--softfloat-build` for those
bodies, validates the pinned profile and objects before output/compilation,
builds a recorded adapter, and links with unresolved symbols rejected. Missing,
undeclared or stale backend dependencies fail explicitly. No original fallback.

458,752 generated-store hardware comparisons cover all 64 exception masks,
four rounding modes, every TOP, empty/nonempty sources, both widths/pop forms
and signed special/boundary values. Compare RAM, retained raw physical registers,
status/tags, flags, opcode and relocated context under UBSan. Bounds, pending
state and isolated hardware fault-priority tests also pass. Forty addressing
variants compile warning-free with Watcom. The conversion and dependency checks
remain separate tests, and full-game/exception-delivery evidence is still needed.

The completed 2,107-body corpus run has 33,712 probes: 18,530 matches,
15,068 inconclusive and 114 mismatches. All preceding 2,096 per-body outcomes
are unchanged. The eleven added bodies contribute 30 matches, 99 inconclusive
and 47 mismatches across four bodies. These remain failures in the report;
no comparison fields are masked. Original helper 0x120a52 independently matches
C on hardware for the disputed status, opcode and eight-byte target spans in
all fourteen completed probes, including masked empty-stack indefinite stores.
This private original-byte probe runs in host long mode (ES ignored, host return
width differs), so it establishes those disputed fields only, not guest ABI
equivalence. Evidence: local/asm-x87-original-store-probe-20261114/evidence.json;
bulk report: local/asm-c-x87-store-diff-20261114/report.json. The gate remains
nonzero; oracle repair and full game integration remain required.

`tools/asm_x87_binary.c` now supplies a separate masked raw-80 arithmetic
adapter for add/subtract/multiply/divide. Build it with
`tools/asm_softfloat.py --binary-from local/asm-softfloat --output local/asm-binary`
(optionally `--watcom /path/to/wcc386`). It shares narrowing's validated pinned
dependency builder and records source/library/object hashes. The API accepts
architectural PC (0/2/3) and RC (0..3), rejects reserved PC, and returns raw
extended result plus x87 exception flags and C1. It canonicalizes pseudo-denormal
inputs, rejects unsupported encodings with indefinite, handles denormal-operand
priority, and restores all four SoftFloat settings. Calls must be serialized.

1,064,400 independent native x87 comparisons pass for results and flags/C1:
all supported PC/RC combinations, signed special/unsupported operands, different
NaN payloads, cancellation, precision boundaries, seeded random inputs and an
extended-exponent sweep. Inputs are restored as raw x87 state, avoiding prior
load normalization. Run `ASM_SOFTFLOAT_BUILD=local/asm-softfloat python3 -m unittest
tests.test_asm_x87_binary`. This is masked arithmetic only: guest instruction
rendering, stack effects, unmasked exception effects/delivery and corpus coverage
remain unchanged and unproven by these tests.

Generated register FADD/FMUL/FSUB/FSUBR/FDIV/FDIVR now uses the arithmetic
adapter, including ST(i) destinations and pop forms. Encoding and decoded
mnemonic/operand spelling must agree. Optional arithmetic code uses a separate
header guard and backend declaration/source digest; the verifier validates and
links both arithmetic and narrowing dependencies when required. Missing/stale/
undeclared arithmetic dependencies fail before output/compilation.

The control-aware adapter suppresses the destination for unmasked IE/DE/ZE.
Unmasked OE/UE instead compute the precision-rounded normal result with exponent
wrapped by 24,576; PE can commit the result and pop. Rebased finite operands
retain sticky contributions without host floating-point arithmetic. Exact tiny
results can signal unmasked UE, but rounding to the minimum normal avoids UE.
Earlier sticky flags are preserved; masked stack underflow writes indefinite
and can pop, while unmasked stack invalid suppresses both. Pending exceptions
still stop explicitly at the next waiting instruction (status 8); no #MF
delivery is claimed. Reserved precision control remains unsupported.

5,512,656 raw adapter/hardware comparisons cover masked arithmetic plus all
exception-mask combinations. A separate 1,880,064 generated-body comparison
covers all 144 register encodings, all masks/defined PC/RC, rotated TOP, empty
operands, seeded sticky flags and signed special/range/rounding cases. Compare
all physical raw registers (including popped contents), status/tags, opcode,
flags and relocated environment under UBSan; RAM remains unchanged. Pending
state is checked for no partial mutation. All 144 forms compile warning-free
with Watcom. Tests: `tests.test_asm_x87_binary` and
`tests.test_asm_x87_arithmetic`, with `ASM_SOFTFLOAT_BUILD` set. Memory arithmetic,
the unowned FDIV target at 0x11f7f9, exception delivery and game integration
remain separate work.

Native pointer checks require an uninterrupted measurement on this AMD host.
A hardware-only 10-million-sample probe reproduced four pointer changes, all
across measured thread context switches, and none in 9,999,854 uninterrupted
samples. This is consistent with the AMD pointer-restore limitation described
in [Linux's FPU restore code](https://raw.githubusercontent.com/torvalds/linux/master/arch/x86/kernel/fpu/core.c).
The generated-body test checks per-thread context-switch counters around the
native call and rejects interrupted measurements before inspecting any output
or running C. It retries the same input (bounded), records the rejected count,
and still compares every field. The final 1,880,064-case run rejected 61 such
measurements; it binds adapter/dependency/runtime/generated/test source hashes.
No comparison fields or corpus differences are waived.

All 109 regression tests pass; strengthened context checks pass in a separate
final focused run. The completed 2,117-body corpus has 33,872 probes: 18,530
matches, 15,097 inconclusive and 245 mismatches across 21 bodies. All preceding
2,107 per-body outcomes remain unchanged. The ten new bodies add 29 inconclusive
and 131 mismatches, with differences confined to x87 status/opcode (including
invalid versus divide-by-zero status for 0/0). The gate remains nonzero.
Artifacts: local/asm-x87-register-arithmetic-full-tests-20261114.log,
local/asm-x87-register-arithmetic-tests-20261114.log,
local/asm-x87-binary-unmasked-probe-20261114/context-switch-evidence.json and
local/asm-c-x87-register-arithmetic-diff-20261114/report.json.
