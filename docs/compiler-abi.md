<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# EF2000 compiler and ABI evidence (in progress)

This is a working evidence log, **not** a verified global calling convention.
The image is the SHA-pinned German TACTCOM X-32 payload. Ground signatures
in original disassembly and caller behavior, not in Ghidra's guesses.

## Canonical first-pass compiler flags

`ef2000-wcc` currently uses
`-bt=dos -3r -zp1 -fpi87 -fp3 -ob -ot -s -wx`. These are the chosen
**functional first-pass profile**, not evidence that the original binary
used these switches. Binary matching is not a goal; guest RAM and observable
behavior are. Compare any revised flags against the original with §3.10
differential testing before treating recovered C functions as verified.

- OpenWatcom **2.0** is the selected compiler; 1.9 remains in the shell for
  library fingerprint comparison. The owner has verified the 2.0 OMF check.
- `-3r` selects 386-targeted register argument passing. This is a portable
  conservative baseline, not a claim that the game was built for a 386.
  Calls with stack parameters or x87 returns require explicit treatment.
- `-zp1` matches the confirmed 25-byte packed high-score record with score
  at `+21`. The host C type is also explicitly packed; do not infer all
  unknown game structures have one-byte alignment.
- `-fpi87 -fp3` generates inline 387-compatible x87 instructions. The guest
  execution context supplies the live FP control word; neither switch fixes
  it. Avoid `-on` (numerically unstable), `-op` (forces intermediate spills),
  `-om` (math intrinsic replacement) and aggressive `-ox` until differential
  evidence supports them.
- `-ob -ot` requests branch/time-oriented optimizations without enabling
  numerical rearrangement. `-s` omits runtime stack probes, and `-wx`
  requests maximal warnings. Compiler and patch results must still be
  tested function by function.

The Nixpkgs wrapped compiler supplies `WATCOM`, `INCLUDE` and the host
`binl64`/`binl` search path. The v2 wrapper is explicitly selected even when
the 1.9 compatibility compiler is also in the development shell.

The owner's first native OpenWatcom v2 fixture compiled with the provisional
flags produced a 12-byte function: `push 4; call __CHK; inc eax; ret`. Open
Watcom's documented `-s` switch suppresses stack overflow checks (and their
implicit runtime call), so the wrapper now adds it. This **does not** establish
that EF2000 was compiled with `-s`; it prevents the patch workflow from
introducing an unresolved `__CHK` dependency for otherwise self-contained
functions. The `watcom-omf` Nix check now rejects `__CHK` in its OMF listing.

## Confirmed local instruction observations

| Guest entry | Original instruction evidence | Still unknown |
| --- | --- | --- |
| `0x4050` high-score defaults | Saves EBX, ECX, EDX, ESI; restores them before `ret` on both exits; calls `0x9c960` with EDX zeroed; uses a 25-byte record stride | Caller expectations, return/clobber registers, called runtime ABI |
| `0x41fc` high-score load | Saves EBX, ECX, EDX, ESI; reserves four stack bytes; passes EAX=`0x17f694` and EDX=0 to indirect call `[0x132ab4]` | Target ABI, stack cleanup, outputs |
| `0x432c` score threshold | Loads score in EAX, compares against last record, executes `setg al`, then `ret` | Confirmed: no register/stack inputs; returns AL, stack cleanup zero. Upper EAX bytes are not a return value. |
| `0x433c` high-score insert | Saves EBX, ECX, EDX, ESI, EDI, EBP and reserves eight stack bytes | Input ABI and full return behavior |
| `0x24480` x87 spatial comparison | Inputs from EAX (sign-extended AX) and EDX (pointer to three qwords); reads an eight-byte stack argument; saves/restores EBX, ECX, ESI, EDI; returns with `ret 8`, low-byte result in AL | Exact semantic types, other caller-visible clobbers, x87 control/status requirements |

Evidence: `objdump -b binary -m i386 -M intel -D --start-address=...`
against `local/ghidra/payload.bin`, and `docs/highscores-reconstruction.md`.
Except for the separate `0x432c` row below, these observations are not
complete signatures and remain unverified.

For `0x24480`, two different caller functions show the same argument setup:
`0x2479c` at callsite `0x248b9` and `0x6aeb4` at `0x6b1aa` both store a
computed x87 value as a qword on the stack, put a scalar/index into EAX and
a pointer into EDX, then call `0x24480`. The second caller consumes AL with
`test al,al` at `0x6b1af`. The callee epilogue at `0x2460d` writes AL,
restores registers, and executes `ret 8` at `0x24616`. This establishes
stack ownership and a register+stack calling *pattern*, but still needs a
fully typed signature and x87 differential evidence.

Packing evidence: the high-score record occupies 25 bytes with a signed
32-bit score at unaligned offset `+21`. The GUI icon `next` pointer at `+0x21`
is another candidate for packed layout. These observations do not prove
every structure in the original program was packed to one byte.

`decomp/include/ef_structs/hiscore.h` now declares the 25-byte record with
explicit packing on native compilers and the Watcom `-zp1` profile.
`decomp/tests/fixture.c` uses C89 compile-time assertions for the record
size and each field offset (0, 20, 21). The Watcom OMF smoke check compiles
these assertions, so it also guards the guest layout. The Ghidra symbol
importer retains one-byte packing for structures marked `EF_PACKED`.

### Confirmed signature: `hiscore_qualifies` (`0x432c`)

- Callee instructions `0x432c..0x433a` read two fixed 32-bit score globals,
  compare them as **signed** values (`setg al`), and return without touching
  the stack, pointers, or any registers other than EAX. No implicit inputs.
- Caller `0x494c`, at `0x49b2`, branches immediately on `test al,al`
  (`0x49b7`) and never examines the upper three bytes of EAX.
- Distinct caller `0x4b54`, at `0x4b64`, likewise branches on `test al,al`
  (`0x4b69`); neither caller places an argument for this function.
- Signature: `uint8_t hiscore_qualifies(void)` with AL as its sole output,
  preserved EBX/ECX/EDX/ESI/EDI/EBP, callee stack cleanup 0, ordinary DS
  for both global loads. EAX bits 8–31 are *not* promised preserved: they
  reflect the loaded score and must not be modeled as a 32-bit Boolean.

This confirms the ABI row, **not** a recovered C implementation. No spliced
function or differential replay is being claimed.

### Confirmed signatures: two register-passing examples

`lookup_indexed_object` (`0x6a1b4`):

- The full callee tests signed AX against zero and the 16-bit count at
  `0x2119f4`. A valid index selects a 32-bit pointer from the table at
  `0x20f550` and returns it in EAX; invalid indices return zero. It changes
  no other general-purpose registers, has no stack arguments and uses DS.
- At `0x24087`, caller `0x24074` sets EAX from its sign-extended input,
  consumes the returned EAX in ECX/EBX, then calls again at `0x24097`.
  At `0x247af`, distinct caller `0x2479c` sign-extends AX, then consumes
  returned EAX in ESI/ECX. These establish signed AX input and 32-bit EAX
  output, not an assumed 32-bit input argument.
- Signature: `ef_addr32 lookup_indexed_object(int16_t index)`; the table's
  record type and logical subsystem remain unidentified (`unk_geometry`).

`list_has_matching_node` (`0xb0208`):

- The callee reads a linked-list head pointer through EAX. It compares
  node bytes `+0x0b` and `+0x0a` to DL and BL respectively and accepts a
  match only when byte `+0x0c` is not 1; follows the 32-bit next pointer
  at `+0x04`. It returns EAX=0 or 1. ECX is saved/restored; EDX may be
  overwritten, while EBX/ESI/EDI/EBP are untouched. No stack args/cleanup.
- At `0xb0258`, caller `0xb0238` sets EAX from its list pointer, EDX from
  its loop index, EBX=15; it stores the returned EAX in ESI. At `0xc149d`,
  caller `0xc1438` sets EAX from a saved list pointer, EDX=0, EBX=15 and
  stores AX in a record. Both caller paths also supply BL/DL as predicates.
- Signature: `uint32_t list_has_matching_node(ef_addr32 list_head_addr,
  uint8_t key_dl, uint8_t key_bl)` with EAX/DL/BL input storage. The name
  describes the observed structure, not a proven original symbol or a
  completed recovered source function.

Both signatures were determined from the full callee bodies and two distinct
callers each. Their `typed` tracker states assert **ABI only**, never patch or
replay verification.

### Confirmed object-state wrappers and x87 matrix pair

`set_indexed_object_activation` (`0x6a354`) sign-extends input AX and calls
`0x7489c` to resolve an object pointer. If the pointer is nonzero and the byte
at `+0x140` is enabled, it sets bytes `+0x141` to 1 and `+0x142` to 0.
Callers `0x2ba14` at `0x2ba3f` and `0x46454` at `0x473cc` both pass a signed
object index in AX and **ignore** the value left in EAX. The latter uses EDX
afterward; `0x7489c` saves/restores EDX. The wrapper has no stack arguments,
does not pop argument bytes, and preserves EBX/ECX/EDX/ESI/EDI/EBP. Its
recorded signature therefore has a void *contract*, not a claim that the
actual EAX register remains unchanged.

`set_indexed_object_state` (`0x6a394`) likewise resolves the AX index. If a
record is found, its byte `+0xe9` selects an entry at stride `0xe5` in a
table with byte flags at `0x128636`; a nonzero flag causes writes of 1, 3,
and 8 to record offsets `+0x12a`, `+0x13d`, and `+0x13e`. The full routine
pushes/pops EDX, and the lookup preserves EBX/EDX. Distinct callers
`0x2b95c` at `0x2b995` and `0x46454` at `0x46fc1` pass AX and do not consume
EAX. Its recorded output is void, with the same GPR-preservation and zero
stack-cleanup contract as `0x6a354`. Both wrappers have ordinary DS-based
memory accesses. Their lookup callee also increments a fixed diagnostic
counter at `0x1a3580`; do not remove that observable side effect when porting.

The x87 functions at `0x6a4c4` and `0x6a538` share the signed BX object
index and use `0x6a1b4` to obtain the same record. There are **three
register inputs**, all backed by distinct callers, no stack arguments,
and a memory result of three contiguous 64-bit floating-point values:

| Function | Input storage | Memory result | Distinct caller evidence |
| --- | --- | --- | --- |
| `mul_object_matrix_vector` (`0x6a4c4`) | BX index, EDX pointer to 3 doubles | EAX points to the 3-double destination | `0x772d0` at `0x773d2`; `0x725dc` at `0x72761` |
| `mul_object_matrix_transpose_vector` (`0x6a538`) | BX index, EAX pointer to 3 doubles | EDX points to the 3-double destination | `0x74d98` at `0x74de9`; `0x2461c` at `0x24663` |

The source object holds nine unaligned qword coefficients, from offsets
`+0xa1` through `+0xe1` in steps of eight. Their matrix, in row-major
mathematical notation, is `[[a1,a9,b1],[b9,c1,c9],[d1,d9,e1]]` where each
item denotes its hexadecimal byte offset. `0x6a4c4` applies that matrix;
`0x6a538` applies its transpose. Each destination entry is evaluated via
three `fld`/`fmul` and `faddp` operations then `fstp`, preserving the x87
**stack depth**, but not promising unchanged status flags or exception state.
An invalid object index leaves the destination untouched. ECX is saved and
restored, EBX/EDX/ESI/EDI/EBP are not clobbered, and EAX is a scratch
result of the lookup (ignored by callers). There is no live ST(0) return or
callee stack cleanup. These are confirmed call contracts, **not** verified
native vector math: operation order, object packing, precision and x87
exception behavior must still survive differential/replay tests in §3.10.

### Confirmed cross-region scalar, string and parser signatures

`position_delta_metric` (`0x23760`) accepts EAX pointing to three signed
32-bit coordinates at offsets 0, 4, 8. It subtracts three fixed reference
values at `0x1a5bec`, `0x1a5bf0`, `0x1a5be8`, then calls `0xb39ec` with
the resulting components in EAX/EDX/EBX. The callee returns its integer
metric in EAX; `0x23760` restores EBX/ECX/EDX/ESI and leaves EDI/EBP
untouched, with no stack arguments or cleanup. Caller `0x6924` at `0x69de`
stores EAX to `0x197220`; distinct caller `0x92668` at `0x92751` compares
signed EAX to `0x5000`. The exact numerical metric is not named as Euclidean
distance: its `0xb39ec` implementation uses integer arithmetic and still
needs a differential test.

`check_current_object_relation` (`0x23800`) is a zero-argument predicate
over the record addressed by global `0x1f71bc`. It resolves a 16-bit index
from record offset `+0x156` through `0x7489c`, compares an object field
`+0x146` with a global index at `0x1f71c6`, and compares signed indices
into the `0xca`-byte-stride table at `0x19bb58`. Its final acceptance test
uses flags `0x02`, `0x04`, `0x08` at object `+0x24` and may call `0x7574c`.
Every exit sets AL to 0 or 1; the other bytes of EAX are not a Boolean
result. Distinct callers `0x257d0` at `0x25a97` and `0x25c98` at
`0x25e8e` branch on `test al,al` and pass no arguments. All other GPRs
are preserved; no stack arguments, callee cleanup or segment overrides.

`sync_current_object_slot` (`0x238a0`) calls that predicate. On success it
copies the word at `+0x158` to `+0x146` in the record pointed to by
`0x1f71bc`. Callers `0x257d0` at `0x25aa0` and `0x25c98` at `0x26000`
ignore its EAX scratch result, so its *contract* is void. It saves EDX;
the called predicate preserves the remaining GPRs other than EAX. No
stack/segment special case. These two signatures are ABI-only: the indirect
object and table data layouts still require reconstruction.

`allocate_weighted_node_records` (`0xb0018`) takes EAX pointing to a
linked-list head pointer and EDX pointing to a writable 32-bit record count.
It traverses `next` at node `+0x04`, accumulating half the signed high word
of a dword at `+0x4f`; a zero count is replaced with 2,000. It allocates
storage for records of stride `0x36`, initializes their three terminal
16-bit fields (`+0x30`, `+0x32`, `+0x34`) to `0xffff`, and returns the
allocation in EAX, or zero if there is no positive count. Distinct callers
`0xb1e0` at `0xb28c` and `0xba5c` at `0xbb5e` store EAX as the new table
pointer. EBX/ECX/ESI/EDI/EBP are preserved across the full call chain;
EDX is scratch. The calls to `0xcc6c0` and `0xb3fb0` pass their own
arguments, not arguments inherited from this function's stack.

`allocate_tagged_node_records` (`0xb00d0`) takes the same EAX/EDX
pointer roles, traverses `next` at `+0x04`, and sums byte `+0x12` only
for nodes whose byte `+0x09` is zero or one. It adds 50 records, allocates
another `0x36`-byte-stride table, stores the total at the pointer in EDX,
zeros global `0x1379b4`, and returns the allocation in EAX. Distinct
callers `0xba5c` at `0xbb7c` and `0xbfcc` at `0xc4c8` store returned EAX
to `0x1379c0`. EBX/ECX/ESI/EDI/EBP preserved, EDX not promised;
no stack arguments or callee cleanup. These are **allocation contracts**,
not a proof that the `0x36`-byte record has natural or one-byte alignment.

`strlen_ds` (`0xb1b8b`) implements a NUL-terminated byte scan: it accepts
EAX as the string address and returns the length in EAX, with no stack
arguments. It preserves ECX/EDI, other general registers, and the original
ES segment by pushing it, loading ES from DS for `repnz scasb`, then restoring
ES. **It does not execute `cld`: DF must already be clear.** Distinct
callers `0x82e40` at `0x82eab` (uses returned EAX as an end offset, with
EDX preserved) and `0xe882c` at `0xe896b` (tests the length) confirm the
input and result. Its `kind` stays `unclassified`: matching it to a specific
OpenWatcom library belongs to §3.6, not a premature guess in §3.5.

`read_next_command_token` (`0xf46cc`) has no explicit argument registers.
It scans a NUL-terminated global command string at `0x4859e0`, starting at
the signed 16-bit cursor at `0x161eac`. It skips spaces/tabs, handles a
leading double quote or a comma-delimited unquoted token, copies up to 80
characters to the buffer at `0x485e08` with a NUL terminator, and updates
the 16-bit cursor. EAX holds the 0..80 byte count on return; EBX/EDX/ESI
are saved/restored and other GPRs are untouched. Callers `0xf4a88` at
`0xf4b1c` and `0xf4bdc` at `0xf4bf8` both inspect the global token buffer
instead of supplying register arguments, and neither requires the count.
Its EAX result is recorded because the original callee consistently leaves
the count there, not because a caller necessarily depends on it. No stack
arguments or cleanup; ordinary DS for its global accesses.

`elapsed_centiseconds` (`0xf8248`) is a five-byte **tail-jump thunk** to
`0x1100b1`, not an empty body. The target calls `0x11004b`, which requests
DOS time through `0x11f287` (function `0x2c`), combines hours/minutes/seconds/
hundredths into centiseconds, and subtracts a startup baseline at `0x4dd984`
with day wrap (`0x83d600` centiseconds). Distinct callers `0xac5f8` at
`0xac681` store EAX to a timing global, `0xe3120` at `0xe3124` converts
EAX to floating point, and `0x106c3c` at `0x106c3f` subtracts EAX from a
prior tick. Neither thunk nor target accepts register/stack arguments;
EBX/ECX/EDX/ESI/EDI/EBP are saved by the runtime call path or never
changed. EAX is the 32-bit elapsed centisecond count; no stack cleanup.
Keep `kind=thunk` here, without preempting §3.6's classification of the
underlying DOS runtime code.

### Confirmed void ABIs and a non-GPR side effect

`reset_spatial_slot_count` (`0x263e0`) has no arguments or register return.
It zeros a 16-bit word at `0x19b376` (the upper half of the dword at
`0x19b374`), saving/restoring EDX while leaving all other general registers
untouched. Callers `0x1f0fc` at `0x1f624` and `0x3a3a8` at `0x3ab19`
neither set arguments nor consume any return. Its name is descriptive and
provisional; the surrounding spatial counter structure needs type recovery.

`x87_unmask_exceptions` (`0x120800`) is a small x87 control-word helper.
It saves the current CW to a four-byte temporary stack slot, clears bits
0, 2 and 3 with `AND 0xfff2` (unmasking invalid-operation, divide-by-zero,
and overflow exceptions), loads the modified CW and restores ESP. It
preserves every general register; **modifying x87 state is its result**.
Caller `0x120485` invokes it at `0x1204ae`, after installing the FPU handler;
caller `0x120746` invokes it at `0x120798`, after restoring DS from the
handler. Neither supplies arguments or consumes GPR results. Marked
`kind=asm` based on its explicit FPU control instructions; §3.6 may later
identify it as part of a linked runtime library.

The annotation importer maps empty outputs in `signatures.tsv` to `void`
instead of preserving Ghidra's guessed integer return. Both void signatures
still need original guest-state replay evidence before any C replacement.

### Floating-point and segment exceptions under review

The initialized image contains the 16-bit word `0x127f` at `0x17d500`.
`0xedbe1` loads it with `fldcw`, while `0xf1072` reads it before a call to
`0x10e810` (which later restores the caller's CW). Its precision field
(bits 9:8 = `10`) selects **53-bit significand precision**, and rounding
bits 11:10 = `00` select round-to-nearest. This is a runtime *path*, not a
global claim: other functions temporarily change the control word, and we
have not captured the live CW at gameplay checkpoints. The compiler flag
`-fpi87` generates x87 instructions but does not establish the run-time CW.

`0xf8250`: the full callee reads one 8-byte floating stack argument from
`[esp+4]`, multiplies it by two fixed double globals and leaves the result
at ST(0), returning with `ret 8`. Distinct callers `0xf8278` (`0xf8285`)
and `0xf8298` (`0xf82a5`) each push two 32-bit words, call it, and pass
the resulting x87 value to a following FP routine. This establishes a
**stack-argument / ST(0)-return convention**, not a standard GPR return.
It is intentionally not marked `sig_status=confirmed` until the Ghidra
symbol importer supports this storage and the wider FP call contract.

`0x120a52`: reads BL bit 2 to choose between `fst qword ptr ES:[EDI]`
and `fst dword ptr ES:[EDI]`, without popping ST(0). Callers `0x120928`
at `0x12097d` and `0x1209b5` at `0x1209dc` share the same BL/EDI/ST(0)
environment. This routine depends on **ES**, which cannot silently be
replaced with the usual DS-based flat-memory pointer when ported. Its
unusual x87/segment contract is documented but not yet represented as a
Ghidra custom storage signature; likely assembler or FP runtime code.

`0x15c0c4`: an argument adapter starts with `enter 0,0`, reads three
32-bit stack arguments from `[ebp+8,+12,+16]`, invokes `0xc474c`, then
loads a fourth argument from `[ebp+20]` and calls `0x15c071`. It finishes
with `ret 16`, so the **callee cleans up four stack arguments**. Distinct
callers `0x5f220` at `0x5f3bb` and `0x3d4b0` at `0x3d5f1` each push four
values, call it and test EAX for zero. The sibling `0x15c0ea` uses the
same stack-argument wrapper pattern. Detailed type and clobber recovery is
still pending; a default Watcom-register signature would be wrong here.

`0xee43c` is a likely hand-written segment-sensitive callback bridge. Its
caller `0xefbd9` at `0xefbfc` and distinct caller `0xeffb4` at `0xeffeb`
both set AL=1, AH=0x81 before the call. The routine uses AL to modify a
byte at DS:[EBP+4], conditionally copies 108 bytes from DS:EBP to SS:ESP
with `rep movsd`, invokes an indirect external callback, then copies the
result back under carefully reassigned DS/ES segments. It explicitly clears
DF with `cld`; segment/flag behavior is therefore part of the contract.
Its indirect callback's register effects are not established, so this is an
**exception review**, not a confirmed GPR signature.

`0x120451` wraps DOS `int 21h` and uses ES as a result. Callers `0x11d01c`
at `0x11d0e6` and `0x11d135` at `0x11d1ff` pass an interrupt-vector number
in EAX, then save the returned EAX and DX selector separately. The wrapper
restores its original EBX/ECX and ES, puts ES into EDX before returning,
and reloads EAX from its preserved input. Interrupt-side clobbers and
descriptor semantics still need explicit verification before assigning a
full signature or porting it to host C.

`0x1204dc` uses a **three-dword stack-argument** contract with `ret 12`,
not Watcom register passing. With its prologue saves and eight-byte local
area included, the source 64-bit floating bit-pattern is at `[esp+0x14]
and `[esp+0x18]`, and a destination pointer is at `[esp+0x1c]`. The
routine tests a double exponent for the all-one pattern, writes a selected
fixed message (optionally prefixed by `-`) to that destination, and returns
EAX=1 if handled, EAX=0 otherwise. Callers `0x11d3f0` at `0x11d40c` and
`0x120560` at `0x12058c` push precisely three dwords and test EAX for
success without further stack adjustment. EBX/EDX are explicitly saved;
the remaining GPRs are untouched. The 64-bit source is passed as **two
stack dwords**, not as ST(0), so the x87-candidate label was only a
sampling heuristic. Pending stack-location support in the annotation
importer and a justified Watcom `#pragma aux` declaration.

`0x15c071` receives an output pointer in EBX, an integer denominator from
the **signed upper word** of ECX, and fixed-point coordinate values from
ESI and EDI. It writes two 16-bit coordinates to `[EBX]` and `[EBX+2]`
and returns EAX=1 only if the denominator is between 16 and 20,000;
otherwise it writes `0xffffffff` and returns EAX=0. Its two distinct
callers are the stack adapters `0x15c0c4` at `0x15c0df` and `0x15c0ea`
at `0x15c105`. Both adapters first invoke arithmetic at `0xc474c` or
`0xc4755`, which supplies EBP/ESI/EDI intermediate results; they move
EBP into ECX before restoring their frame pointer and set EBX from the
fourth stack argument. Therefore **ECX is not simply the original first
stack argument**, and treating this routine as ordinary `__watcall` would
lose a nonstandard return convention. No inner stack arguments or cleanup;
EAX/ECX/EDX clobbered, EBX/ESI/EDI/EBP preserved. Reviewed as an ABI
exception pending an explicit compiler declaration and adapter model.

`0x12d90` is an indirect callback dispatcher with register input AX and DX.
It derives a signed 16-bit record index from AX plus a table selected by
signed DX, checks record flags, invokes direct record handlers at `0x11e7c`
and `0x12210`, and optionally calls the 32-bit function pointer at record
offset `+0xcd`. Callers `0x6460` at `0x6575` (EAX=7, EDX=0) and `0x13950`
at `0x1396a` (EAX=3, EDX=1) demonstrate the input register halves; neither
inspects return registers. The indirect handler's register and segment
effects have not been bounded, so preservation and return storage remain
unconfirmed. The record stride visible in the routine is `0x1d` bytes.

`0xf6124` wraps an indirect formatted-output call through `[0x132a74]`.
It pushes four dwords onto the stack (format address `0x192c4c`, global
`0x464390`, a zero-extended 16-bit global at `0x16007c`, and address
`0x464396`), then reclaims 16 bytes and restores EDX. Distinct callers
`0xf7518` at `0xf7527` and `0xf79b0` at `0xf79bf` make no argument setup
and do not use EAX afterward. The call's actual return/clobber convention
depends on the unresolved indirect target, so only the four temporary
**outgoing** stack arguments are established, not inherited caller arguments.

## Comparing assembly

```sh
ef2000-wcc decomp/src/hiscore.c
python3 tools/asm_diff.py 0x4050 decomp/build/hiscore.obj
```

The helper displays an original linear disassembly window beside `wdis -r`
output with OMF relocation annotations. Ghidra function bodies may be
noncontiguous: the window is a diagnostic aid, not an exact body export.

## Remaining work

1. Review the 40-entry worksheet at `decomp/symbols/abi-samples.tsv`,
   deterministically generated from Ghidra function facts and the original
   image by `tools/abi_samples.py`. It covers five address regions, with
   leaf, direct-call, indirect-call, branch-heavy, memory-heavy, large,
   x87-*candidate* and REP-*candidate* selections. One region has no REP
   heuristic match, so its row is explicitly labeled fallback. The byte
   heuristics can match data/immediates: confirm instructions with disassembly
   before classifying actual x87, strings, switches, or assembly stubs.
   `decomp/symbols/abi-audit.tsv` now records disassembly summaries and two
   callsite addresses for each sample. All 40 have at least two **distinct**
   direct callers; nine show actual x87 instructions and four show REP
   instructions in the inspected linear windows. Six show immediate `ret N`.
   These mechanical facts are reproducible with `tools/abi_audit.py`; neither
   file contains original instruction bytes or decompiler text.
   The worksheet status field is manually reviewed: `confirmed` means an
   address also has a signature in `signatures.tsv`, `partial_abi` is
   incomplete contract analysis, and `exception_reviewed` has a documented
   unusual convention that the Ghidra importer cannot yet express.
   Regenerating with `tools/abi_samples.py --output` retains existing review
   statuses for matching entry/shape rows; `--reset-status` clears them.
2. Confirm inputs, outputs, preserved registers, stack cleanup, x87 state
   and segment assumptions from disassembly and at least two callers each.
   Sixteen of the forty candidates now have confirmed ABI rows (four object
   routines, two void helpers, a pointer lookup, a linked-list search,
   a scalar metric wrapper, a string scan, a command token parser, plus
   the current-object relation/slot pair, two linked-list allocators and a
   timing thunk);
   exceptions and partial reviews remain separate from complete signatures.
3. Record confirmed signatures in `decomp/symbols/signatures.tsv`, adding
   `#pragma aux` declarations only when justified by evidence.
4. When differential replay becomes available, evaluate `-5r` and stronger
   optimizations on equivalent RAM traces. The selected 2.0 flags above are
   suitable for the first C/OMF experiments, not final proof of correctness.
