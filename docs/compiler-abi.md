<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# EF2000 compiler and ABI evidence (in progress)

This is a working evidence log, **not** a verified global calling convention.
The image is the SHA-pinned German TACTCOM X-32 payload. Ground signatures
in original disassembly and caller behavior, not in Ghidra's guesses.

## Object and world-grid control cluster (2026-11-03)

Six object-control ABIs were checked at their complete entries, exits and
distinct direct call sites. These are **signatures**, not guest-verified C.

| Entry | Inputs / outputs | Independent call sites | Original effects |
| --- | --- | --- | --- |
| `0x238a0` | no input, no consumed return | `0x25aa0`, `0x26000` | calls `0x23800`; if AL true, copies current object word `+0x158` to `+0x146` |
| `0x2541c` | signed AX index, DL mode; void | `0x23a36`, `0x23bb9` | filtered lookup; kind-2 byte `+0x16d`, mode-1/2 helpers `0x2a168`/`0x2a1c4`, or deferred flag `+0x16e` |
| `0x2b884` | signed AX index; AL Boolean | `0x46b82`, `0x46f28` | filtered lookup; object high word `+0x26` chooses grid row; compares unsigned grid word `+0x50` to signed index |
| `0x2b910` | signed AX grid row, DL byte; void | `0x2ca13`, `0x2d572` | dispatches event `0x58+DL` then writes grid byte `+0x90` |
| `0x2b95c` | signed AX grid row; void | `0x2b131`, `0x470bb` | if `+0x49` clear and type gate enabled: calls `0x6a394`, dispatches event 7, sets `+0x49` |
| `0x2ba14` | signed AX grid row; void | `0x46f17`, `0x470c6` | if `+0x4c` set and `+0x4a` clear: calls `0x6a354`, dispatches event 8, sets `+0x4a` |

The grid pointer at `0x19bb58` is loaded as a guest address; records have
stride `0xca`. The 32-bit dword at grid `+0x4e` is arithmetically shifted
right by 16 to obtain a signed object index. The event dispatcher
`0x46168` receives that index in EAX, the event in EDX, zero in EBX and
ECX, and an additional zero stack dword; it ends with `ret 4`. The mode
helpers `0x2a168` and `0x2a1c4` take an object pointer in EAX. Their
callee implementations and complete ABI analyses remain outstanding.

The grid control functions reload the base address and object index after
calling other helpers; a C draft must preserve these reads rather than
cache a value across calls that could change global state. All host-model
tests use synthetic memory. Ghidra pseudo-C and dynamic original execution
were unavailable; neither ABI table nor host success proves a splice.

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
| `0x24480` x87 spatial comparison | AX index, EDX pointer to three qwords, stack qword limit, AL predicate; saves/restores EBX/ECX/ESI/EDI, `ret 8` | Native FP behavioral equivalence, especially status/control word and exceptional values; see confirmed ABI below |

Evidence: `objdump -b binary -m i386 -M intel -D --start-address=...`
against `local/ghidra/payload.bin`, and `docs/highscores-reconstruction.md`.
The `0x432c` and `0x24480` rows now have independently confirmed calling
conventions below; the other rows are still observations, not signatures.

For `0x24480`, two different caller functions show the same argument setup:
`0x2479c` at callsite `0x248b9` and `0x6aeb4` at `0x6b1aa` both store a
computed x87 value as a qword on the stack, put a scalar/index into EAX and
a pointer into EDX, then call `0x24480`. The second caller consumes AL with
`test al,al` at `0x6b1af`. The callee epilogue at `0x2460d` writes AL,
restores registers, and executes `ret 8` at `0x24616`. This establishes
stack ownership and a register+stack calling *pattern*. The storage is now
confirmed; numerical/x87 equivalence remains unverified.

### Confirmed ABI: spatial overlap predicate and shared projection globals

`test_indexed_spatial_overlap` (`0x24480`) receives AX as a signed 16-bit
object index and EDX as the address of **three consecutive qwords**. Its
third argument is one eight-byte floating-point limit passed on the stack,
represented in `signatures.tsv` by the two machine words at `[ESP+4]` and
`[ESP+8]` (little-endian bits of the same double). It returns **AL only**:
zero rejects the object or its spatial comparison, nonzero accepts it.
EBX/ECX/ESI/EDI are saved/restored, EBP is not changed, EAX/EDX are
scratch, and the callee removes exactly eight bytes via `ret 8`.

The whole routine is `0x24480..0x24618`. It resolves the AX index through
`0x7489c`, subtracts object position qwords `+0x49/+0x51/+0x59` from the
EDX vector, rounds each coordinate via `0x9d72c` and compares an integer
derived magnitude to the stack limit. It also calls `0x2461c` and computes
a later x87 bound using per-type data at `0x12861d` and nearby offsets.
Caller `0x2479c` at `0x248b9` supplies EAX from sign-extended DI, EDX
pointing to `[EBX+0x49]`, and a freshly stored double from ST(0). Distinct
caller `0x6aeb4` at `0x6b1aa` uses signed word `[ECX+0x14]`, EDX
`[EBX+0x49]`, and another computed stack double; it immediately checks AL.
These establish the mixed-register/stack ABI, **not** equivalence of the
rounding rules, floating-point exceptions, comparison flags, or global
side effects in a future C replacement.

`update_indexed_projection_globals` (`0x2461c..0x24798`) takes AX as a signed
object index and EDX as three qwords of world position; there is no stack
argument, and callers do not consume any return register. It looks up the
object with `0x6a1b4`, subtracts its stored position and calls the separately
typed `mul_object_matrix_transpose_vector` (`0x6a538`) with the same index
in BX. The resulting vector occupies globals `0x19b290..0x19b2a7`.
Multiple x87 square-root/distance helpers then write qwords at `0x19b2a8`,
`0x19b2b0`, `0x1f70e8`, `0x1f7008`, and `0x1f7018`. An invalid index skips
these writes. EBX/ECX/ESI/EDI/EBP are restored; EAX/EDX are scratch. Both
`0x24480` at `0x24557` and **distinct** `0x2479c` at `0x24982` supply AX/EDX
and use the global results, not an EAX return. X87 stack depth is balanced
throughout the calls, but status/precision and memory write order still
require replay verification.

### Confirmed ABI: projected-position query

`query_projected_object_position` (`0x86f7c..0x872c6`) accepts AX as a
signed selector, EDX as an output pointer to **two signed 16-bit coordinates**,
and BX as a signed 16-bit iteration count. The callee initially writes
`-100` to both output words and sets its eventual result to `-1`. It looks
up the current object index from `0x1f71c8` with `0x7489c`, constructs
fixed-point camera/world coordinates from `0x1a5be8..0x1a5bf0`, invokes
`0xe9b38`/`0x87a20`, and may invoke the function pointer at `0x20f1dc`.
If projection succeeds it calls the confirmed four-stack-argument adapter
`0x15c0c4` to update the output pair. It computes a signed 32-bit distance
or scale in EAX, with `-1` as the no-object/no-result sentinel; do not
restrict the return to AX. ECX/ESI/EDI/EBP are saved/restored, EBX/EDX
are scratch, and there is no stack argument or callee cleanup.

Caller `0x5c7ab` at `0x5c7b9` uses AX=1, EDX pointing to a local pair,
BX=4, stores the returned **AX**, then branches on signed AX. Separate
caller `0x5ca60` at `0x5ca6e` passes AX=9, EDX pointing to its local pair,
BX=9, and branches on full signed EAX before using that EAX as an index.
The callback's effects on global projection state and all integer overflow
edges still need differential testing.

### Confirmed ABIs: zero-argument node initialization and command loop

`initialize_global_node_state` (`0xb0f5c..0xb16bb`) takes no caller-provided
register or stack arguments. Its first call allocates 120 bytes with
`0xcc6c0` using two temporary **outgoing** stack arguments. It zeros or
reinitializes node globals around `0x137958..0x1379c8`, selects separate
paths based on word `0x137992`, and calls the list and table workers in the
`0xb0xxx`, `0xcaxxx` and `0xcbxxx` regions. The early copy at `0xb100e`
uses three `movsd` instructions with **DS source and ES destination** and
assumes the ambient direction flag is clear. All paths converge on the
epilogue at `0xb16ae`: EAX is cleared and all six other GPRs are restored;
the routine uses a plain `ret` and no caller-stack cleanup. Its first
caller is `0xbab4`, followed by a test of global word `0x137998`; its
second is `0xc0f6`, followed by writes to global flags. Neither prepares
parameters or consumes EAX. The initialized records and allocation
semantics are not yet recovered as source.

`process_command_dispatch_loop` (`0xf3b78..0xf3d53`) also accepts no
arguments. It initializes ECX to command buffer `0x48592c`, BH to 1 and
BL to 0, repeatedly reads input via `0xf2d74`, then compares the buffer
against literal command strings at `0x1919f8..0x191a74` with `0xa4380`.
Each match dispatches a command handler or updates bytes `0x4859db` through
`0x4859dd`. Its loop stops when the internal BL flag becomes nonzero.
EBX/ECX/EDX are saved/restored on **every** return path, with no caller
stack cleanup. The other GPRs are not promised by this ABI: the dispatched
helpers may themselves invoke indirect handlers. Caller `0xf4078` at
`0xf40a3` proceeds to test global flags; caller `0xf4178` at `0xf41a0`
restores EBX and returns. Neither passes arguments or consumes an EAX
result. There is no ST(0) return; callback and buffer side effects remain
observable and must survive replacement.

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

### Filtered object lookup and four short state wrappers (C drafts)

The filtered lookup `0x7489c` is distinct from the simple table accessor
`0x6a1b4`. Its callers at `0x23811` (tests EAX) and `0x2b858` (tests EAX before
storing DL to the returned record) confirm signed AX input and EAX 32-bit
pointer/zero output; it preserves EBX/ECX/EDX/ESI/EDI/EBP, with no stack
parameters. Its first operation increments the diagnostic dword at
`0x1a3580` regardless of lookup success. It rejects negative indices,
indices at/above signed count `0x2119f4`, special index `0xff`, the 16-bit
excluded index at `0x1f71d0`, and table entries equal to 32-bit `0xff`.
Null entries return null without a separate test.

**Important noncontiguous body:** original code occupies `0x7489c..0x748f3`
(88 bytes), but rejection paths branch to the five-byte epilogue at
`0x6a1ac..0x6a1b0` shared with another function. Ghidra therefore reports
93 *body bytes*, not 93 writable contiguous bytes at `0x7489c`. An in-place
replacement must use a validated **88-byte** capacity or a separately
reviewed patch plan; do not use the tracker size as an overwrite extent.
The OMF splice planner now refuses tracker rows marked `noncontiguous` in
their notes, even if their state is later advanced to `compiles`.

Two more short wrappers were verified against both callee bodies and their
direct callers: `clear_indexed_object_activation` (`0x6a378`) from
`0x2b9e9`/`0x47394`, and `clear_indexed_object_state` (`0x6a3d0`) from
`0x2b8e5`/`0x46ffd`. Both accept signed AX only, preserve
EBX/ECX/EDX/ESI/EDI/EBP via the lookup helper, and have no register output
observed by callers. The first requires nonzero object byte `+0x140` and
sets `+0x141` to zero; the second unconditionally clears `+0x12a` when a
record exists. Together with confirmed `0x6a354` and `0x6a394`, these form
a four-wrapper state cluster. `0x6a394` selects a `0xe5`-stride flag using
the kind byte at `+0xe9` and modifies bytes `+0x12a`, `+0x13d`, `+0x13e`
only if the flag is nonzero. All four use `0x7489c`, so the diagnostic
lookup count changes even when no record is updated.

`decomp/src/object_lookup.c` and `object_state.c` now model these cases in C
with a host-only guest-pointer adapter. Their calling conventions and source
are *drafts*, not evidence of Watcom compilation or a passing guest replay.

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

Direct original-image audit (SHA-256 `f21a72d8...`, pinned in
`docs/executable.md`): the complete twelve-byte body saves EDX, zeroes EDX,
stores DX at `0x19b376`, restores EDX and returns. The C draft uses a 16-bit
store, with a differential fixture deliberately initializing the adjacent
low word to nonzero values; a 32-bit store would be incorrect.

The adjacent `append_spatial_slot` (`0x263ec`) is a 55-byte leaf with four
direct callers (`0x1f6ef`, `0x2ef50`, `0x2f0fa`, `0x3ac33`). Each supplies
coordinate X in AX, Y in DX, and a one-byte tag in BL; no caller consumes a
return. The callee saves/restores ECX and does not change EBX/ESI/EDI/EBP;
**EDX is clobbered** on a successful append because DX is reused for the
updated count. EAX also changes, so neither belongs in `preserved` or in
`outputs`. The signed upper word of `0x19b374` indexes 29 four-byte records
at `0x19b2e0`: X and Y occupy offsets 0 and 2. Tags are separate bytes at
`0x19b358..0x19b374`; the last tag shares the low word of the dword
containing the high-word count. If signed count is below 29, the callee
stores both coordinates and tag and increments the count; otherwise it
returns without modifying memory. An anomalous negative count would index
before the table (an original-game hazard, not covered by host tests).

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

### Confirmed packed floating-point record and jump-table flag probe

`initialize_packed_vector_record` (`0x11c0b4`) accepts an EAX destination
pointer and EDX source pointer, without stack arguments or cleanup. Its eight
`fld`/`fsub`/`fstp` sequences initialize destination qwords `+0x00..+0x38`
from source qwords `+0x00..+0x38`. The first three subtract each value from
itself (do **not** replace that with integer zero without accounting for NaNs
and x87 exception behavior); the next five subtract a source component.
The routine copies source doubles at `+0x10`, `+0x00`, `+0x08` to destination
`+0x40`, `+0x48`, `+0x50`, then clears several trailing fields up through
the dword at `+0xd1`. An explicit byte zero at `+0x68` is followed by
**unaligned** dword zeros at `+0x69`, `+0x6d`, etc. The surrounding callers
step the output pointer by `0xd5` (213) bytes. These offsets prove a packed
layout *for this record*, but not the names or types of its remaining fields.

Caller `0x10a580` at `0x10a8bb` and distinct caller `0x10c1b0` at
`0x10c4e7` both pass EAX=`[0x4ddee0]` and EDX=`0x433768`; after the call
both reload the destination pointer and add `0xd5` for the next record.
The callee preserves EAX, EDX and all other GPRs (EBX is explicitly saved),
balances its x87 stack and has a void register-result contract. Reproduce
the order and floating-point flags before considering a C replacement.

`test_indexed_object_bit` (`0x68ccc`) is a 13-case **CS-relative jump-table**
switch at `0x68c98`. AX is a signed slot index; zero short-circuits to a
false AL, and DX is an unsigned selector restricted to 0..12. Each case
reads the word at `0x12bc95 + 0x91 * (int16_t)AX`, shifts its selected bit
into the sign position of AX and executes `sar ax,15`. Therefore AL is
`0xff` for a set bit and zero for a clear bit, **not** a normalized 1.
Distinct caller `0x69374` at `0x6945c` sign-extends its two 16-bit inputs
then tests AL; caller `0x68e84` at `0x68ebc` does the same and tests AL.
Only EAX and EDX may be clobbered; EBX/ECX/ESI/EDI/EBP are preserved.
All case exits use plain `ret` and zero callee stack cleanup; no x87 use.
The jump targets live in CS while the data word uses DS. The switch looks
compiler generated; there is not enough evidence to mark it hand-written.

### Confirmed global-input projection and assembler-like return convention

`append_projected_global_record` (`0x15be10`) has **no incoming register or
stack arguments**. It reads globals `0x15bde8/0x15bdec/0x15bdf0`, calls
`0xc474c` to compute fixed-point outputs, and calls `0xc473c` with the
global at `0x15be0c` as a limit. The latter uses a four-word stack call to
`0xceb40`, which cleans up its own 16 bytes. A carry-set result skips the
write and returns EAX=0. On success the routine updates byte/word/dword
arrays keyed by `0x146e52`, calls `0xb3f58` and `0xc41b0`, and returns
EAX=1. The callee's own stack cleanup is zero; it preserves ESI/EDI/EBP,
but not EBX/ECX/EDX. It saves original EBP on entry, **pops it into EDX**
after successful projection, then uses `xchg ebp,edx` to restore EBP while
retaining the projected value for later stores. This unusual contract
justifies provisional `kind=asm`, without asserting the whole function is
necessarily handwritten.

Distinct callers `0x288c8` at `0x28968`, `0x81680` at `0x8177d`, and
`0x817a8` at `0x81836` populate the globals immediately before calling
and do not consume EAX. The explicit zero/one on both callee paths is
nevertheless recorded as a 32-bit EAX result. Register-intermediate and
array semantics still need state-level differential verification.

### Confirmed object-relation mutation contract

`update_indexed_object_relation` (`0x243ac`) takes a signed 16-bit object
index in AX and has a void return contract. It sign-extends AX for lookup
through `0x7489c` and, when the record exists, loads another indexed object
from its signed field `+0x144`, then tests `check_current_object_relation`
(`0x23800`). Depending on that predicate, comparisons of the record's
double at `+0x150`, the fixed double at `0x181c98` and the current object's
double at `+0x15a`, it updates 16-bit relation fields at `+0x146` and
`+0x158` and may zero the current object's double at `+0x15a`.

Caller `0x24074` at `0x240aa` supplies EAX from EBP and immediately tests
DI, not EAX; distinct caller `0x6c710` at `0x6c741` sign-extends the word
at `0x1f71c6` into EAX and then reloads EDX from a global, ignoring EAX.
EBX/ECX/EDX/ESI are pushed/popped across the callee and its lookups,
EDI/EBP are untouched, and there is neither a stack argument nor callee
cleanup. The x87 comparisons balance their stack but use `fnstsw`/`sahf`
to drive branching; a future C translation must preserve comparison and
NaN/exception behavior, not simply rely on optimized host comparisons.

`count_filtered_node_matches` (`0xb0850`) counts linked-list records. The
inputs are AL (mode 0 or 1), EBX (address of a list-head pointer) and EDX
(address of a second, filter list-head pointer). The function walks the
first list through each node's `+0x04` next pointer. For each node it calls
`0xcc1ac` with its mode in AL, the node key from `+0x0b` in DL, and EBX
pointing to the filter list head. After that predicate accepts a node, the
signed high word of its dword at `+0x07` is tested against one of two
mode-specific sets (mode 0: `1..6,8,9,11..14`; mode 1:
`17..22,24..26`). Unsupported modes yield zero. The full 32-bit count is
returned in EAX; EBX and EDX are scratch, while ECX/ESI/EDI are saved and
restored and EBP is untouched. No stack arguments or callee cleanup.
Caller `0xb0c34` at `0xb0c46` supplies AL/EBX/EDX and stores EAX as a
32-bit count; distinct caller `0xb0c9c` at `0xb0cae` supplies the same
roles and stores the resulting AX. This is a *counting contract*, not a
fully recovered data schema for the node's odd field offsets.

`count_positive_filtered_node_matches` (`0xb0954`) is a companion to
`0xb0850` with the **same AL/EBX/EDX inputs and full-EAX count output**.
It calls `0xcc22c` instead of `0xcc1ac`, testing for a positive rather than
negative signed byte in the filter node's per-key payload. Both mode 0 and
mode 1 count their usual signed high-word type sets, additionally admitting
certain type IDs when word `+0x09` in the source node is nonzero. The
function preserves ECX/ESI/EDI/EBP and has no stack arguments/cleanup;
EBX/EDX are scratch. Caller `0xb0c9c` at `0xb0cbe` passes the same argument
roles after counting negative matches; distinct caller `0xcb6fc` at
`0xcb78f` supplies AL=0, EBX from EDI, EDX from ECX and consumes the
returned low AX after `cwde` to compare it with seven. Both positive and
negative variants operate on packed linked records and need memory
equivalence tests before C replacement.

`transition_indexed_object_state` (`0x6a1d0`) accepts a signed object index
in AX and a requested state byte in DL. The full callee saves EBX/ECX/ESI/
EDI, looks up AX through `0x7489c`, and exits without side effects if no
object exists. Otherwise it clears flag bits of record byte `+0x25`, then
dispatches on DL values `2`, `5`, `6`, `9`, `0x0e`, `0x10`, and a general
fallback. Case 2 can invoke the already-typed `set_indexed_object_state`
(`0x6a394`) and a relation predicate; cases 5 and 6 invoke `0x752d0` and
`0x75344`; case 9 invokes the record-event helper `0x46168` with its own
temporary stack argument. All paths update object state bytes (notably
`+0x16c`, `+0x25`, `+0x26`) or deliberately leave them untouched. There
is no caller-visible register result; EAX/EDX are scratch. EBX/ECX/ESI/
EDI/EBP are preserved, and the outer callee has no stack parameters or
cleanup (the inner `0x46168` call handles its own argument).

Distinct caller `0x46454` at `0x466b5` passes AX from DI and DL from a
stack byte; caller `0x72944` at `0x72cc4` sign-extends AX from the global
at `0x1f71c8` and sets DL to `0x0b`. Both continue using memory state,
not EAX. This confirms a **state-transition ABI** but does not assert a
complete enum meaning for the requested state values or verify gameplay
equivalence.

### Floating-point, stack, and segment calling conventions

The initialized image contains the 16-bit word `0x127f` at `0x17d500`.
`0xedbe1` loads it with `fldcw`, while `0xf1072` reads it before a call to
`0x10e810` (which later restores the caller's CW). Its precision field
(bits 9:8 = `10`) selects **53-bit significand precision**, and rounding
bits 11:10 = `00` select round-to-nearest. This is a runtime *path*, not a
global claim: other functions temporarily change the control word, and we
have not captured the live CW at gameplay checkpoints. The compiler flag
`-fpi87` generates x87 instructions but does not establish the run-time CW.

`scale_double_to_st0` (`0xf8250`): the full callee reads one 8-byte floating
stack argument from `[esp+4]`, multiplies it by fixed doubles at `0x1932a4`
and `0x1932ac` (in that order) and leaves the result at ST(0), returning
with `ret 8`. Distinct callers `0xf8278` (`0xf8285`) and `0xf8298`
(`0xf82a5`) each push two 32-bit words, call it, and pass the resulting
x87 value directly into `0xb45ac` or `0xb45a2`, respectively. All GPRs
are untouched; x87 status can change and stack depth **increases by one**.
The confirmed custom-storage signature models the input as two 32-bit
stack words and the output as `ST0:float10` (the full 80-bit x87 register),
with eight-byte callee cleanup. It does **not** claim that the original
input is two independent integer arguments or that an OpenWatcom C `double`
return already reproduces the intermediate precision.

`store_x87_st0_via_es` (`0x120a52`) reads BL bit 2 to choose between
`fst qword ptr ES:[EDI]` and `fst dword ptr ES:[EDI]`, without popping
ST(0). Callers `0x120928` at `0x12097d` and `0x1209b5` at `0x1209dc`
share the same BL/EDI/ST(0) environment; both may subsequently pop ST(0)
based on a separate flag in BL. Every GPR is unchanged; the x87 stack
register is *live-in and live-out*, represented by `ST0:float10` in both
input and output storage, not by a newly computed floating return. There
is no stack argument or callee cleanup. This is a confirmed **assembly
boundary** (`kind=asm`), not a plain C function: the destination uses
**ES**, which cannot silently be replaced with an ordinary DS pointer,
and `fst` rounds to 32 or 64 bits only for the memory store while keeping
the original ST(0) value and depth. FP exceptions/status and segment mapping
still need state-level replay evidence.

`project_fixedpoint_pair_from_stack` (`0x15c0c4`) starts with `enter 0,0`, reads three
32-bit stack arguments from `[ebp+8,+12,+16]`, invokes `0xc474c`, then
loads a fourth argument from `[ebp+20]` and calls `0x15c071`. It finishes
with `ret 16`, so the **callee cleans up four stack arguments**. Distinct
callers `0x5f220` at `0x5f3bb` and `0x3d4b0` at `0x3d5f1` each push four
values, call it and test EAX for zero. The sibling `0x15c0ea` uses the
same stack-argument wrapper pattern. The first three dwords are signed
fixed-point inputs; the fourth is a writable pointer to two 16-bit results.
The wrapper preserves ESI/EDI/EBP and returns the Boolean from `0x15c071`
in EAX; EBX/ECX/EDX are scratch. The Ghidra importer now supports the
four `[ESP+N]` locations and 16-byte purge. `kind=asm` describes its
unusual EBP intermediate/restore sequence, not a verified C replacement.

`decompress_bitstream_backrefs` (`0xf9ee0`) is an assembler-like
bitstream decoder with a two-pointer **stack convention**. The original
begins with `enter 0,0`, saves ESI/EDI/EBX/EBP, executes `cld`, takes the
destination pointer from `[ebp+8]` and the source pointer from `[ebp+12]`,
then starts reading the stream eight bytes after the source address. Its
bit-carry state machine emits literals with `movsb`, repeated words with
`rep movsw`, and overlapping LZ-style back-references with `rep movsb`.
Source is read through DS:ESI, destination written through **ES:EDI**;
there is no incoming EAX/EDX register argument or register return. EAX/ECX
are scratch; EBX/EDX/ESI/EDI/EBP are preserved; `cld` leaves DF clear.
All normal exits run `leave; ret 8` at `0xfa027`, so the callee removes
eight bytes of **caller arguments**. Distinct caller `0xb3c04` at
`0xb3c8e` pushes EBP as source then EDI as destination; caller `0xeab44`
at `0xeab7c` pushes `0x474360` as source then `0x464580` as destination.
They do not use a return value. This is ABI evidence, not yet a guessed
compression-format specification or recovered decompressor. The confirmed
custom-storage signature records the two stack pointers and eight-byte
callee cleanup; this is **ABI-only** typing.

`decompress_ra_stream` (`0xfa02c`) is a different `ret 8` bitstream decoder.
It uses `pushad`/`popad` to preserve all GPRs except its EAX return, clears
DF, takes the **source** from `[ebp+8]` and **destination** from `[ebp+12]`,
then invokes internal helpers at `0xfa31b`, `0xfa251`, `0xfa1d4`, and others.
It writes literal bytes with `movsb` and back-references with `rep movsb`
through DS:ES. Success returns EAX=1; failure executes **INT3** *before*
its zero-EAX return if execution resumes. The trap is observable and must
not be silently removed in a C replacement. Caller `0xb3c04` at `0xb3cc2`
pushes EDI (destination) followed by ESI (source); distinct caller
`0xeba64` at `0xebadf` pushes a destination then a compressed-source
pointer. Neither performs caller stack cleanup or consumes the status.
This is an assembler-like compression ABI, not yet a recovered RA codec.

`clear_registered_callbacks` (`0xabec4`) walks a table of `0x15`-byte
records anchored at `0x41e971`, bounded by the word count at `0x41eecc`.
For a nonzero callback pointer in each entry it sets EAX=0, sets ECX to
the record's byte displacement (not an absolute pointer), then invokes the
callback indirectly. It clears the count and calls `0xb3fb0` on the base
`0x41e960` with EBX=`0x150`. Caller `0xac408` at `0xac429` and distinct
caller `0xac780` at `0xac78c` provide no register/stack arguments and ignore
EAX. EBX/ECX/EDX are saved by this wrapper, but the callback's possible
ESI/EDI/EBP side effects and GPR return contract remain unknown. Marked
`exception_reviewed`, not `confirmed`; labeling it a safe void register
contract before resolving callback targets would conceal that uncertainty.

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

`format_nonfinite_double` (`0x1204dc`) uses a **three-dword stack-argument** contract with `ret 12`,
not Watcom register passing. With its prologue saves and eight-byte local
area included, the source 64-bit floating bit-pattern is at `[esp+0x14]`
and `[esp+0x18]`, and a destination pointer is at `[esp+0x1c]`. The
routine tests a double exponent for the all-one pattern, writes a selected
fixed message (optionally prefixed by `-`) to that destination, and returns
EAX=1 if handled, EAX=0 otherwise. Callers `0x11d3f0` at `0x11d40c` and
`0x120560` at `0x12058c` push precisely three dwords and test EAX for
success without further stack adjustment. EBX/EDX are explicitly saved;
the remaining GPRs are untouched. The 64-bit source is passed as **two
stack dwords**, not as ST(0), so the x87-candidate label was only a
sampling heuristic. The custom-storage signature now records both stack
words, the output pointer, the EAX Boolean and the 12-byte purge. A
Watcom `#pragma aux` declaration remains to be supplied with the future
recovered C caller, not presumed by ABI annotation alone.

`write_projected_fixedpoint_pair` (`0x15c071`) receives an output pointer in EBX, an integer denominator from
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
EAX/ECX/EDX clobbered, EBX/ESI/EDI/EBP preserved. Its unusual four-register
contract is now recorded as a confirmed custom-storage signature; matching
Watcom `#pragma aux` is still needed if recovered C directly calls it.

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

## Second world-grid link/query/control cluster (drafted)

The following six routines have original instruction-level checks and at
least **two distinct caller functions** each. Their C representations and
synthetic host tests do not establish an OpenWatcom binary replacement or
guest-differential equivalence. All use zero outgoing stack bytes and the
existing 202-byte (`0xca`) grid-record stride.

| Original | Recovered routine | Observed ABI and independent caller evidence |
| --- | --- | --- |
| `0x2b558` | `link_grid_cell` | Signed AX cell, signed DX neighbor, no consumed return; caller `0x2b508` at `0x2b546` and `0x46454` at `0x46ea8`; saves EBX/ECX, EDX scratch. Mutates +0x24 and +0x26, optionally neighbor +0x22, then invokes unrecovered `0x2b1f4` and `0x2b634`. |
| `0x2beb0` | `grid_has_active_link` | Signed AX cell, AL Boolean; callers `0x8cc4` at `0x8f2a` and `0x2c778` at `0x2ca1d`. Saves EBX/ECX/EDX/ESI. Checks all seven attached object indices, including after a match. |
| `0x2bf08` | `grid_has_object_mode` | Signed AX cell, DL mode, AL Boolean; separate callers near `0x8e1e` and `0x2cb52`. Saves EBX/ECX/ESI; EDX scratch. Iterates a byte count at grid row +0x4f, ignoring objects with bit `0x08` at object +0x24, checking mode at +0x16c. |
| `0x2dba4` | `grid_is_current_or_external` | Signed AX cell, AL Boolean; callers `0x90c0` at `0x90c5` and one near `0x17475`. Saves EBX/ECX/EDX. Rejects `0xff`, compares global active index, then conditionally scans an external-index list using unrecovered `0x2db64`. |
| `0x2e030` | `reset_grid_cell_state` | Signed AX cell, no consumed return; caller `0x2e740` at `0x2e87c` and `0x7e8c0` at `0x7ea0d`. Saves EBX/ECX/EDX. Skips `0xff`, clears row +0x20, calls `0x2e08c`, reloads grid base, copies dwords +0x99/+0x9d into +0x91/+0x95, conditionally calls `0x54c10`. |
| `0x2e910` | `grid_event_matches` | AL event code, DL mode, AL Boolean; callers `0x270d0` at `0x27206` and one at `0x65da1`. AH/EDX may be modified. Cases are `(code=0, mode=1)`, `(code=1, mode=0)`, `(code=2, mode!=2)`, otherwise false. |

Both attached-object scans invoke `lookup_filtered_indexed_object` once per
entry, even when an earlier entry already satisfied the predicate. Its
global lookup counter is observable and must match the original. The source
files preserve unresolved grid callbacks as typed external dependencies.
Guest fixtures are explicitly pending execution, including paths that need
real callback captures; the host test uses stubs only for those dependencies.

## World-grid dependency follow-up (five C drafts)

The original direct-call scan was checked against function boundaries, not
just raw call-site counts: the five functions below each have at least two
**different** caller functions. Their C models remain drafted; none has yet
passed the full guest differential or OpenWatcom splice.

| Target | Inputs / observed return | Callee preservation and independent callers |
| --- | --- | --- |
| `0x2afcc` `detach_grid_link_on_mode_change` | AX signed cell, DL next mode, no return consumed | EBX/ECX saved; EDX scratch. Called by `0x2b0b4` at `0x2b0c8` and `0x2b1f4` at `0x2b20b`. |
| `0x2b5c0` `link_external_grid_cell` | AX signed cell, DX signed node, no return consumed | EBX/ECX saved; EDX scratch. Called by `0x46454` at `0x46f69` and `0x47764` at `0x479cb`. |
| `0x2b634` `copy_grid_link_position` | AX signed cell, no return consumed | EBX/ECX/EDX saved. Called by `0x71fc` at `0x72fd` and `0x2b558` at `0x2b5b5`. |
| `0x2b690` `refresh_external_grid_position` | AX signed cell, no return consumed | EBX/ECX/EDX saved. Called by `0x2b5c0` at `0x2b62c` and `0x46454` at `0x468b0`. |
| `0x2e08c` `recompute_grid_limit` | AX signed cell, no return consumed | EBX/ECX/EDX/ESI saved. Called by `0x2ce18` at `0x2ce6c` and `0x2e030` at `0x2e050`, also by `0x85420`. |

At `0x2afcc`, only the literal `0x00ff` is an absent link. For mode 3 with
an unset link state, a two-way linked pair is broken when the requested new
mode differs from 3; mode 6 sets grid +0xbb only when its detail byte is 6/7
and the cell is not active. At `0x2b5c0`, the neighboring index refers to a
**different 69-byte array** via global pointer `0x1a14f8`: the external
back-reference stores AX's low byte; mode + marker determine whether it is
written. It invokes unresolved state dispatcher `0x2b1f4` before refreshing
the position. `0x2b634` inherits three 64-bit positions from the indexed
world-grid row; `0x2b690` instead multiplies signed external integers at
+0x0f/+0x07/+0x0b by the double at `0x182512`.

At `0x2e08c`, a threshold starts with three doubles from a `0xe5`-byte type
record at `0x1285d5` (offsets 0/8/16), the last multiplied by the double at
`0x182605`. All seven filtered object lookups execute, even after finding a
lower limit; an object double is read at +0x177. The original x87 `fcomp`
and `jae` update the threshold for strictly smaller **or unordered** object
values. The C model retains that decision (`!(value >= threshold)`) and uses
`long double` for the initial calculation, but 80-bit precision, NaN payload
canonicalization, status flags and exception behavior require original guest
comparison before considering the replacement verified.

`0x2db64` (external-grid membership predicate) was intentionally omitted:
although readable and only 63 bytes long, its single known direct caller is
`0x2dba4`. An ABI signature would violate the two-distinct-caller rule.

## Grid neighbor, relation and category follow-up (seven C drafts)

Seven more original functions were read from the SHA-pinned extracted payload
and checked at two or more distinct **function** callers. The caller sites
consume register results as documented, even when the original callee writes
additional scratch registers; all remain source drafts without an OpenWatcom
object or a guest differential comparison.

| Original entry | ABI and two independently inspected original callers |
| --- | --- |
| `0x2a934` `recycle_oldest_grid_node` | No inputs/consumed return; saves EBX/EDX; callers `0x2a7f8:0x2a899` and `0x2a8ac:0x2a8b9`. |
| `0x2bb3c` `grid_cell_distance` | Signed AX/DX row indices, ST0 distance; saves EBX; callers `0x71fc:0x724f` and `0x7308:0x7378` compare ST0 with x87 (`fcomp`); four more call sites. |
| `0x2bbd0` `find_grid_neighbor_with_detail` | Signed AX cell, signed AX neighbor or `0x00ff`; saves EBX/ECX/EDX/ESI/EDI; callers `0x2bc3c:0x2bc79`, `0x2bcb8:0x2bcc5` and `0x2c6dc:0x2c71d` sign-extend AX. |
| `0x2bc3c` `grid_neighbor_distance` | Signed AX cell, ST0 double; saves EBX/ECX/EDX; callers `0x2ced4:0x2d212` and `0x6e59c:0x6e6cf` both immediately compare ST0. |
| `0x2c0ec` `grid_has_cross_object_relation` | Signed AX cell, Boolean AL; saves EBX/ECX/EDX/ESI; callers `0x2c6dc:0x2c710` and `0x77c90:0x7878a` both test AL. |
| `0x2da54` `refresh_grid_category` | Signed AX cell, void; saves EBX/ECX/EDX/ESI/EDI; callers `0x161d4:0x164e8` and `0x16588:0x16769` use the written +0xbc category byte, plus caller `0x2d894`. |
| `0x2dce0` `grid_is_visible_from_reference` | Signed AX cell, Boolean AL; saves EBX/ECX/EDX; callers `0x2e740:0x2e88e` and `0x7e8c0:0x7ec55` test AL. |

`0x2bbd0` treats each adjacency list as 17 bytes (one unused byte and eight
signed neighbor words) from `0x1fcb40`, indexed by the signed word at grid
row +0x1c. It seeks the first non-self, non-0xff neighbor whose +0x1b
detail mode is one. `0x2bc3c` returns zero unless the active row's +0x1c
group equals the argument row, the helper finds a neighbor, and that
neighbor's +0x1a current mode is nonzero. Then `0x2bb3c` passes the
two rows' pairs of +0x61/+0x69 doubles to library routine `0xb415c`, which
computes the two-dimensional Euclidean distance and leaves it in ST0.
Exact x87 intermediate stores and square-root rounding still need tests.

`0x2c0ec` iterates seven embedded object indices at row +0x50. Every
successful `0x7489c` lookup is followed by a second filtered lookup using
the signed high word at object +0x144; the objects' signed high words at
+0x26 identify grid rows. The first mismatch between those rows' +0x15
locator high words returns AL=1. Other paths keep scanning, preserving the
original filtered-lookup counter increments.

`0x2da54` reads one of two embedded object indices at grid +0x50/+0x52,
chosen by comparison with the high signed half of `0x1f71c8`. The selected
object has up to 20 three-byte features at +0x19b; only a nonzero following
word is considered, with a gate in the 0x91-byte type records at `0x12bc94`.
Eligible codes 6/7/8/14/28 have priority one, 12/13 priority two, and
9/15 priority three; only a strictly lower priority replaces the selected
category written at grid +0xbc. The original checks all 20 entries even
after finding a priority-one match. `0x2a934` moves the oldest node from
the active 0x19bb3c/0x19bb38 queue to the 0x19bb40 free list, linking via
node +0x12; no empty-queue guard exists. `0x2dce0` performs a mandatory
reference-distance predicate at `0x2dc84`, then every external predicate
at `0x2dbf8` when enabled, and finally suppresses the result if row +0x1a
is zero. Those two predicate callees are explicitly unrecovered because
their independently confirmed caller/ABI evidence is incomplete.

## Object geometry and indexed scene nodes — 15-function batch

The original image has eight additional, independently called object/point
helpers. `0x2bab0` and `0x2bae0` take an EAX pointer to two or three doubles
plus a **signed DX grid index** and return a double in x87 ST0. The former
subtracts the two unscaled coordinates and passes two stack doubles to the
runtime hypot helper `0xb4620`, whose epilogue is `ret 16`. The latter first
multiplies *each* of the three input and three grid components by the double
at `0x18251a`; its callee `0xb415c` consumes the first two components only.
Both have five or more independent original caller functions.

`0x6a5f0` accepts two signed indexed-object numbers in AX and DX, resolves
both using `0x6a1b4`, returns zero if either lookup fails, and otherwise
subtracts all three position doubles at +0x49/+0x51/+0x59. Again only the
first two differences enter `0xb415c`, so a large Z difference must not
affect the distance. `0x6b3d0` takes the byte in AL and reads a code in a
0xe5-stride table at `0x128568`: code 2 accepts only kinds 0x12/0x16;
code 3 rejects all; code 4 rejects kinds 0x1d/0x1e; other codes accept.

`0x6b4ac` takes an object guest pointer in EAX, reads its type at +0xe9,
and forms an x87 limit using the four unaligned doubles in the corresponding
0xe5-byte record at `0x1285d5`. It conditionally adds term 2 if the object's
feature-0x1e lookup is not sentinel 0x14, and terms 3 and 4 if its
feature-0x1d lookup is not 0x14. `0x6b54c` takes that same EAX pointer and
replaces the double at +0x177 only when the old limit is **strictly greater**;
it returns the update flag in AL. `0x6b58c` takes a signed AX index, performs
one filtered object lookup and unconditionally stores the computed limit if
found. `0x6b5a8` accepts two double-array pointers in EAX/EDX and a packed
three-word time pointer in EBX; `0x40774` converts the clock to seconds,
`max(1, reference_time - old_time)` divides the 2D displacement, and the
result remains in ST0. The host C models use 64/80-bit intermediates as an
approximation only; runtime hypot, NaN behavior, and exact x87 rounding still
need guest differential proof. All eight have two or more distinct callers.

Seven more functions operate on the scene-node list headed at `0x211ad4`.
The node's +0x14 high word is an index in `0x8ffe8`, and +0x50 is the next
pointer. `0x90050` tests whether a nonzero node resolves to itself;
`0x90074` sets +0x30 bits 0 and 1, clears bit 2 of each 0x44-byte child
slot's +0x16 byte (count at `0x123819 + kind*0xa4`), and notifies the
owner. `0x900c0` dispatches according to global alternate-mode and the
node's +0x18 marker; only the direct branch writes the +0x22 word to five.
`0x901ec` walks the list and dispatches by kind below/above 0x1b, re-reading
the next link *after* each callback. `0x9065c` returns head for null input,
otherwise the successor. `0x906b4` looks up the scene node by AX and returns
AL=1 if any child slot's +0x40 signed-high-word index resolves via the
filtered indexed-object lookup. `0x907f0` finds the next such node, starting
at the successor of an EAX pointer or the global head for EAX=0. It has an
**out-of-body branch into the `0x8ffe8` shared epilogue at `0x9000a`**.
This candidate must not be independently spliced without resolving that
tail. These seven also have at least two distinct original caller functions;
the related original dispatch/lookup callees are typed **dependencies**, not
additional recovered implementations.

All fifteen functions are **drafted host models**, not ABI-safe replacement
objects. The two host suites exercise signed modes, disabled feature branches,
strict limit comparisons, time clamping, ignored Z, scene child flags, event
dispatch and callback mutation of a next pointer. Per-function guest fixtures
remain plans; no native guest differential or Watcom recompilation succeeded
in this environment.

### Scene state, query, and event helpers (0x9125c–0x91c48)

Eight adjacent helpers have at least two distinct original callers each.
`0x9125c` takes full EAX and forwards it to `0xa1a40` as EDX with a fixed
name pointer at `0x18ab30`; the later sentinel checks use DX only and the
entry lookup takes a signed 16-bit key. The C return models AX only, not the
unobserved high half of EAX on successful lookup. `0x91298` calls `0x7b16c`
with EAX=0 and the two query globals as EDX/EBX, and returns a wrapping
`0xfffffffe - query` value. `0x918f0` compares that result against a signed
EDX threshold. It loads the high signed word of `0x1a35f8` before calling
`0x91298`, but that callee immediately clears EAX; the load has no visible
effect on its Boolean result.

`0x91910` copies EAX/EDX/EBX to three dwords at `0xc4756`, `0xc475c`,
`0xc4762` (two-byte gaps preserved), the signed high words to the three
four-byte-spaced slots at `0x159094`, and the original dwords to
`0x2143f4`, `0x2143f0`, `0x2143ec` in reverse memory order. `0x9195c`
calls `0xc4654` with three sign-extended stack words in BX, DX, AX order,
then stores original AX/DX/BX at `0x2144a4`/`a6`/`a8`. `0x91acc` invokes
`0x913dc` with EAX=255, EDX=2, EBX/ECX as the original inputs; only AL=0/1
triggers `0x91afc` with signed AX/DX values.

`0x91c20` zeros the two 16-bit counters at `0x2144ac` and `0x2144ae`.
`0x91c48` checks control code 15/16 at `0x12db9e`, arms and clears the
history if selected, then appends the input low word at `0x214428 + 2*count`
when the signed count is less than ten. The count is **the high word of a
32-bit load at `0x2144aa`**; the C layout retains the overlapping unknown
low word. There is no original lower-bound check on negative counts. The
original callback `0x86920` always fires. All side-effect callees remain
unrecovered dependencies, with host mocks only; no compiled guest
replacement has been spliced or verified.

### Indexed scene and object-link helpers (0x68c18–0x8ffe8)

The scene-node lookup `0x8ffe8` has **58 distinct original callers**. It
subtracts `0x2001` from EAX, tests only the resulting signed AX against zero
and the signed count at `0x211ae0`, and returns the pointer stored in the
four-byte array pointed to by `0x211ad0`. Its shared epilogue at `0x9000a`
is also an explicit *cross-function* branch target of `0x907f0`. Either
function's individual replacement requires special care to preserve both
entry paths; normal in-place splicing cannot assume the body is isolated.

`0x75620` grades unsigned AL as 0→6, 1–49→4, 50–74→3, 75–99→2 and
100–255→1, writing a full EAX category. `0x7574c` requires the filtered
index lookup to succeed, then recognizes the signed high word of the
selector at `0x1f71c8` or (when alternate grid mode is active) one of the
`0x1b`-byte external probes beginning at `0x1a5c2e`. `0x75c84` clears a
dword at `0x128550` and the adjacent words `0x1f88ee`/`f0`.

`0x76b30` uses the byte at parent object +`0x187` to select a 25-byte
link-table row pointed to by `0x1f8918`, writes BX into word slot
`3 + 2*DL`, then, if BX resolves to a valid filtered child, stores DL to
child +`0x18c`, zeros +`0x18a`, and copies the parent word +`0x16` to
child +`0x188`. It has no slot bounds check. `0x78aa8` copies a source
record to guest address `0x1f8d00` via original callback `0x9d749`, then
sets the word at +`0x56` to `0x118`.

`0x6ae30` uses the signed high word at object +`0x26` as a `0xca`-byte
grid-row index. When row +`0x1b` is `0xb`, it returns whether the **full
AX** returned by `0x6aa70` is nonzero. Otherwise the high signed word at
row +`0xb7` selects a linked index, with exactly `0xff` as a rejection
sentinel; on success it marks row +`0xbb` before calling `0x6abac`.
`0x68c18` clears the 3-byte marks of an object and its paired index,
the latter taken from the *high word* at `0x1282e2 + 2*index`, i.e.
`g_object_paired_index_words[index+1]`. It calls original `0xe7c50` twice
with a root high word in EAX, the current index in EBX, and EDX=-1. The
second call reloads both global values after the first callback, which the
host test explicitly exercises. Negative or out-of-range indexes are not
checked by the original. Each of these eight helpers has at least two
distinct original callers; guest-only dependencies remain mocked and
verification is still pending.

### Buffer cursor, planar scene and object identity helpers

Eight more leaf-sized functions have multiple distinct original callers.
`0x7a480` computes a **signed, wrapping** 32-bit difference of the pointers
at `0x1f8e94` and `0x1f8e90`, reporting only when greater than `0x4000`
through the original indirect diagnostic at `0x132a74`. `0x7a584`
stores signed AX and advances by two *before* that capacity check;
`0x7a5a0` reads and sign-extends the word, advances by two and checks.
These routines lack a pre-access bounds check; host tests preserve that
ordering, and the diagnostic remains a guest-only dependency.

`0x7aac0` selects a planar data source from twelve-byte records at
`0x1f8e00`, keyed by full EAX and cached at `0x12afc0`. On a mode change,
it copies the record's +8/+4/+0 pointers to `0x1f8ea4`/`0x1f8eb8`/
`0x1f8eb4` respectively, leaving EAX equal to the last source pointer;
on the unchanged branch EAX remains the mode. `0x7b2f4` then reads a byte
from each selected plane at `(row*400 + 399 - column)` and combines them
as high and low bytes of a word. Its original instructions clear **AH
only** after reading the low byte, so an out-of-range column may leak the
high word of `399-column` into EAX; the drafted model deliberately keeps it.

`0x8c09c` tests whether a guest object pointer equals the indexed object
lookup for the signed high word at object +`0x14`. `0x8c0bc` sign-extends
AX and performs an **unguarded** four-byte pointer-table fetch from
`0x20f550`; unlike the earlier guarded `0x6a1b4`, it never checks count or
zero. `0x95018` simply sets the byte latch at `0x12dbc4` to one. All
eight are synthetic host models with pending original-game checks.

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
