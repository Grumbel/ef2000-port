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

### Scene controls and x87 vector conversion batch (16-function wave, second half)

The next eight bodies were checked against the original SHA-pinned image,
with at least two **distinct caller functions** recorded in
`decomp/symbols/signatures.tsv`. Their compact host C models are in
`decomp/src/scene_controls.c` and `decomp/src/vector_math.c`, with boundary
tests and eight additional pending guest differential plans.

Four non-floating routines:

- `0x8f620` takes signed AX, then stores three **words**: AX at `0x198442`,
  50 at `0x19842e` and 1 at `0x198444`.
- `0x8bf00` retains incoming AL in DL while passing the **signed high word**
  of `0x1f71c8` to original `0x7489c`; on success it writes DL at returned
  object offset `+0x1d7`. A missing object causes no store.
- `0x8bf1c` initially sets AL=1. With signed word `0x425bc8` nonzero, it
  compares signed arithmetic `EAX >> 8` against signed high word of
  `0x425bce`. If that target is below signed dword `0x214448`, only values
  **at least** the target pass; otherwise only values **at most** it pass.
  Equality succeeds in both branches. The remaining EAX bits are not a
  32-bit Boolean contract.
- `0x91d9c` initializes original record `0x214404` through guest-only
  `0xd0b68`, then invokes `0xd0ebc`, `0xd0dbc`, `0xd0fb8` in that order,
  passing the unsigned low words of original **EDX, EAX, EBX** respectively
  with DX's high half zeroed. Finally it stores those original low words
  at `0x2144b4`, `0x2144b6`, and `0x2144b0`, respectively. The record
  callbacks are still guest-only dependencies; their effects and register
  preservation need differential confirmation.

Four x87 routines share strict ABI restrictions: they return via memory,
not ST0, do not alter general registers or the stack, and rely on the guest
DS for **possibly unaligned double loads**. All eight have more than one
original caller, but no OpenWatcom object or patched replay has been tested.

- `0x88d30` loads a three-double vector from EBX, subtracts the matching
  values at EDX, and writes to EAX. Load/subtract/store is interleaved for
  each field, so the order matters with aliased buffers.
- `0x88d84` uses `FILD` on signed dword at EAX+`0x26`, multiplies by
  double at `0x18a498`, stores one double to EDX.
- `0x88c54` and `0x8fa58` load signed dwords at EAX+`[8,0,4]`, multiply
  each first by scale A and then scale B, store double at EDX+`[0,8,16]`.
  The first uses scales `0x18a468` and `0x18a470`; the second uses the
  deliberately unaligned doubles `0x18aa0b` and `0x18aa13`.

The host model uses ordinary C `double`; **x87 extended intermediate
precision, rounding modes, FPU exceptions, and aliased source/destination
between individual float operations remain unverified**. Treat these as
drafts, not drop-in replacements, until Watcom compilation and guest oracle
traces demonstrate equivalent RAM and state changes.

### Linked-record resolution and short runtime latches

Ten further functions were inspected directly in the pinned original
executable with two **distinct original caller functions each**, recorded
in `decomp/symbols/signatures.tsv`. Their C drafts in
`link_resolution.c` and `runtime_control.c` have host-only tests and
boundary differential fixtures, but no patched guest confirmation.

- `0x38658` accepts signed low AX as an unguarded index into the pointer
  table at `0x19f5b0`, and EDX as an optional guest candidate record. If
  candidate+`0x15` contains a nonzero dword, that value overrides the
  table. EDX is preserved, as is EBX; the selected guest address is EAX.
- `0x3856c` accepts a guest record in EAX, reads its signed word at
  `+0x16` and old pointer at `+0x18`, calls `0x38658`, then replaces only
  the pointer at `+0x18`. EAX is also left with the new pointer. Both
  record shapes are packed: the candidate field at `+0x15` is unaligned.
  Neither route has an original index bounds check.
- `0x3cd64` stores the full incoming EAX dword at `0x1a35bc`;
  `0x3dcc0` stores the word 5 at `0x1a35ec`; `0x58d24` writes words
  8 and 10 to `0x1a6ce2` and `0x1a6ce8`; `0x5e658` zeros a word at
  `0x12814e`. These have no callback dependencies.
- `0x7a428` copies the dword at `0x12afc4` to `0x1f8e9c`, leaving the
  copied value in EAX. Both callers subsequently replace EAX, but the
  full register result is retained in the signature.
- `0x7eea0`, `0x810d0` and `0x810f4` accept low AL. If their input
  enable byte is zero, they select zero; otherwise they select AL. The
  first uses gate `0x15bac6` and output latch `0x12b070`; the other
  two share gate `0x15bac7`, with separate latches `0x12b104` and
  `0x12b105`. They return the selected value in **full zero-extended EAX**
  and preserve EDX with push/pop.

### Binary command-packet wrappers (eight C drafts)

The next eight original bodies use the same direct-call dependency at
`0xa3f74` with EAX=pointer to a short **stack-local byte packet** and
EDX=length. Every packet wrapper has two distinct direct callers. The
packet dispatch implementation is not yet recovered; the C model provides
an explicitly unresolved `emit_command_packet()` dependency. Host tests
mock this dependency synchronously to observe exact bytes and lengths.

| Original address | Opcode | Packet bytes in order | Length |
| --- | --- | --- | ---: |
| `0x56f0c` | `0x32` | opcode, AX low, AX high, DL | 4 |
| `0x57140` | `0x39` | opcode, AX low, AX high | 3 |
| `0x57198` | `0x3a` | opcode, AX low, AX high | 3 |
| `0x57684` | `0x4a` | opcode, AX low, AX high | 3 |
| `0x576b4` | `0x4b` | opcode, AX low, AX high | 3 |
| `0x576e4` | `0x4c` | opcode, AX low, AX high | 3 |
| `0x545b8` | `0x0c` | opcode, AX low, AX high, byte at `0x1a666b`, DL | 5 |
| `0x546c0` | `0x16` | opcode, AX low, AX high, byte at `0x1a666b` | 4 |

`0x56f0c` and `0x545b8` clobber EDX before dispatch; other wrappers
restore EDX with push/pop. The packet objects are local stack bytes: the
original dependency must consume or copy them during the call, not retain
the pointer for asynchronous use. C `uint8_t` arrays model the **payload**
but not the original stack layout or guest callback side effects.
OpenWatcom register assignment, real `0xa3f74` behavior and live guest
packet ordering must be validated before marking these verified.

## Short clock, tactical-state, grid and UI-list routines (#077)

Sixteen additional draft functions come from direct inspection of the
SHA-pinned guest payload, with two independent caller functions per entry.
The paired host tests run without original data. They **do not** establish
OpenWatcom register assignment, splice safety or guest RAM equivalence.

| Address | C draft | Original ABI/effect | Caller functions (call site) |
| --- | --- | --- | --- |
| `0x40774` | `clock_words_to_seconds` | EAX pointer to three signed words; EAX seconds, wrap at 32 bits | `0xc630` (`0xc63a`), `0x40870` (`0x40880`) |
| `0x40820` | `reference_clock_reaches` | EAX pointer; lexicographic signed-word `reference >= argument`; **AL only** | `0x5437c` (`0x543bb`), `0x855c8` (`0x855f9`) |
| `0x41da4` | `command_code_is_group_1b` | AL code, full EAX 0/1 for `0x1b..0x27` | `0xd290` (`0xd43a`), `0x10a9c` (`0x10b51`) |
| `0x49054` | `clear_tactical_accumulators` | No inputs; clears seven separate typed state fields | `0x3dd30` (`0x3ddc8`), `0x48e7c` (`0x4902a`) |
| `0x4d8f4` | `reset_tactical_slots` | No inputs; sixteen `0xff` stores, conditional byte clear | `0x4c78c` (`0x4c8e0`), `0x4c900` (`0x4ca68`) |
| `0x4dc98` | `advance_first_tactical_status` | No inputs; first matching byte at indices **1..24** only | `0x4c78c` (`0x4c8db`), `0x4c900` (`0x4ca63`) |
| `0x50a1c` | `tactical_lookup_contains` | AL needle, **AL only** result; signed 16-bit count and wrapped 8-bit index | `0x5086c` (`0x508c9`), `0x508fc` (`0x50929`) |
| `0x544c0` | `count_external_grid_markers` | No inputs; count nonzero bytes at +4 in 27-byte rows, store one byte | `0x557e8` (`0x55850`), `0x55860` (`0x5587c`) |
| `0x5cb9c` | `position_within_tactical_bounds` | AX/DX signed XY; **AL only** result | `0x59fa4` (`0x5a062`), `0x5cbc8` (`0x5cd00`) |
| `0x5f1ec` | `sample_active_coordinates` | EAX output guest pointer; optional source +8/+12 dwords scaled by 2048 and biased by 1024 | `0x66fc` (`0x672d`), `0x67d4` (`0x6810`) |
| `0x809bc` | `clear_panel_strip` | AX signed, remainder after `IDIV 16`; clear indices 1..16; EAX 3×remainder | `0x80abc` (`0x80ae0`), `0x878c8` (`0x87963`) |
| `0x85144` | `clock_triplet_is_present` | EAX pointer to three words; **AL only** false for three `-1` words | `0x8516c` (`0x8531e`), `0x857bc` (`0x85858`) |
| `0x85358` | `decrement_grid_activity` | AX signed grid index, byte +0x8e decremented if nonzero | `0x2bcb8` (`0x2bd18`), `0x85380` (`0x853cf`) |
| `0x86920` | `reset_ui_panel_selection` | No inputs; five 1-valued indices and four zeroed words, EAX=5 | `0x17f10` (`0x1800f`), `0x5f634` (`0x5f68d`) |
| `0x87f9c` | `find_ui_linked_tag` | AL tag, DX signed value; EAX node or zero, EDX scratch | `0x55928` (`0x55944`), `0x55b80` (`0x55ba8`) |
| `0x8d0e0` | `mark_ui_links_by_slot` | AX signed slot; set node byte +0x42 on matches; EAX sentinel | `0x8d11c` (`0x8d1f7`), `0x8d330` (`0x8d36e`) |

The `0x40820`, `0x50a1c`, `0x5cb9c` and `0x85144` original routines
write only AL. Higher EAX bytes may remain stale or become scratch values,
so a C `uint8_t` return is **not yet a proven machine-register match**. Their
callers inspect only the low byte at the cited sites. `0x50a1c` has a latent
nontermination if its signed table count exceeds 255 and the entry is absent;
its C loop retains the byte-index wrap. `0x809bc` uses a signed remainder,
including negative-band writes before the nominal table base. Both the
circular UI list's sentinel and its header pointer at +0x34 are respected;
no implicit null termination was invented.

This batch could not run OpenWatcom, Ghidra project export, Unicorn differential,
or patched native64 replay in the current environment. All functions remain
`drafted`; the two-callsite ABI records and per-function guest fixtures are
research inputs for those checks.

### Parser, tactical ring and activity-state batch (eight functions)

Each address below is a bounded contiguous function in the original payload.
Independent direct caller functions were inspected to establish the calling
convention; the packed records in `ef_structs/tactical_records.h` are based on
original byte accesses, not a guessed high-level class hierarchy.

| Address | Draft | Inputs / effects | Two caller functions (call instructions) |
| --- | --- | --- | --- |
| `0x37294` | `mirror_reference_state` | No args; mode byte `1` copies word and two marker-selected bytes; EAX has low-byte result | `0x35590` (`0x35637`), `0x357ec` (`0x35805`) |
| `0x3cc28` | `normalize_script_line_endings` | EAX mutable guest string; three passes remove CR, comments after `;` through LF, repeated LF | `0x3cb2c` (`0x3cb36`), `0x3cc94` (`0x3cc9d`) |
| `0x41f64` | `append_tactical_ring_event` | AX signed X, DX signed Y, BL kind; packed seven-byte entry at signed 16-bit cursor, wrap at 20 | `0x10750` (`0x10787`), `0x110ec` (`0x11136`) |
| `0x50ca4` | `copy_next_text_line` | Global pointer; skip horizontal leading whitespace; copy LF to destination and append NUL; increment byte line counter | `0x50e84` (`0x50e84`), `0x50fcc` (`0x50fe9`) |
| `0x5aa10` | `count_matching_activity_slots` | EAX object pointer; count nonzero signed-strength entries with code equal to signed selected-category slot | `0x59998` (`0x59a97`), `0x5aa64` (`0x5ab05`) |
| `0x62548` | `active_unit_record_contains` | AX signed index; reverse scan of active 23-byte records, **AL-only** Boolean | `0x5d1c4` (`0x5d44c`), `0x65a88` (`0x65b9f`) |
| `0x631bc` | `next_round_robin_record` | No args; advance 16-bit cursor, signed count bound, **AX-only** record/sentinel | `0x65d18` (`0x65d18`), `0x66548` (`0x66785`) |
| `0x79524` | `clear_runtime_transient_counters` | No args; four bytes and six unaligned 32-bit counters reset | `0x3dd30` (`0x3ddcd`), `0x52f38` (`0x53141`) |

`0x3cc28` has a real source-level hazard: a semicolon comment without LF
keeps advancing through memory even after NUL. `0x50ca4` likewise searches
for LF without checking NUL; the host cases provide valid terminated lines.
`0x37294` does not bound the byte selector and may alias unrelated globals.
`0x62548` and `0x631bc` expose only AL/AX in their confirmed callsites;
the remainder of EAX is not claimed. These are host-tested models with two
guest fixture plans per address, not yet Watcom/splice-verified equivalents.

### Caller-side clock, object-slot and transform batch (eight functions)

This set uses existing drafted helpers `reference_clock_reaches`,
`lookup_filtered_indexed_object` and `clear_panel_strip`. The C host harness
stubs these dependencies and verifies the passed addresses/indices as well as
the outer function's effects. It does **not** prove their integration in the
guest until an original-object splice can be replayed.

| Address | Draft | Original arguments / effects | Two caller functions (call instructions) |
| --- | --- | --- | --- |
| `0x2949c` | `query_indexed_object_feature` | Signed AX object, DL code; 20 activity slots; **AL-only** index/20 | `0x298f0` (`0x29968`), `0x2a168` (`0x2a178`) |
| `0x53308` | `convert_signed_short_scale` | Signed AX loaded by FILD word, float32 multiply, **ST0** output | `0x53350` (`0x533c6`), `0x53618` (`0x53633`) |
| `0x80abc` | `advance_panel_strip` | Global 16-bit cursor, signed `IDIV 16` then clear strip, cursor stored into 12-byte band; **AX-only** result | `0x55928` (`0x55a92`), `0x87db8` (`0x87e50`) |
| `0x855c8` | `grid_clock_reaches_reference` | **Full EAX** row times 0xca; grid cell +0x8a clock base and unsigned countdown +0x8e; **AL-only** | `0x2d488` (`0x2d635`), `0x857bc` (`0x85863`) |
| `0x860c0` | `first_nonignored_activity_slot` | Signed AX object; first nonzero-strength slot whose code is not 0x21/0x22; EAX index/20 | `0x8c49c` (`0x8c8a1`), `0x8cb60` (`0x8cbac`) |
| `0x88100` | `refresh_object_child_slot` | EAX optional parent; invalidate +0x56 to **0x00ff**, then write child +0x16 index only if filtered lookup returns the *same* pointer | `0x87db8` (`0x87e5b`), `0x883cc` (`0x8840d`) |
| `0x88c80` | `transform_child_coordinates` | EAX parent, EDX output; child pointer +0x30, flag when child +4 is 0x80000000; scaled doubles from +8/+0/+4 | `0x89060` (`0x89095`), `0x8935c` (`0x893a4`) |
| `0x88d4c` | `transform_signed_angle_pair` | EAX object, EDX/EBX double destinations; signed word +0xc negated, signed word +0xe retained; double x87 scales | `0x89060` (`0x890a0`), `0x899d8` (`0x89a1d`) |

The `0x2949c` comparison is deliberately asymmetric. The slot code is
`SAR 24` of the 32-bit word at slot offset +0x198, and thus sign-extended;
the input DL is zero-extended. Codes at or above 0x80 cannot match, even if
their underlying eight-bit representations are equal. By contrast,
`0x860c0` compares against positive constants 0x21/0x22.

`0x53308`, `0x88c80`, and `0x88d4c` have x87 semantics; the host model uses
`double` and preserves operation order, but does not establish 80-bit
intermediate rounding. The guest fixture for `0x53308` is recorded but the
current `func_differential.py` validator rejects ST0 signatures. The other
seven fixture schemas validate; all eight remain `drafted` until guest and
Watcom comparisons run.

### UI release and indexed control dispatch (eight-function wave)

| Address | Draft | ABI and side effects | Two independent callers |
| --- | --- | --- | --- |
| `0x42548` | `ui_emit_selected_message` | No args, boolean at `0x1a53dc` selects constant `0x1843f0/0x1843f4`; tailcall `0xa6018` | `0xa5d8@0xa697`, `0xa6fc@0xa867` |
| `0x7fbec` | `ui_dispatch_indexed_control` | AL code, EBX signed table index, EDX/ECX forwarded; signed limit and enable gate; `0xd7158` or error handler | `0x7fcdc@0x7fdf6`, `0x8018c@0x801d9` |
| `0x7fe18` | `ui_initialize_releases` | Global input gate; call `0xd7490` with indices 0..13 only when latch clear | `0x19a80@0x19a81`, `0x3dd30@0x3deba` |
| `0x7fe4c` | `ui_finalize_releases` | Two gates plus initialized latch; `0xd7530` for 0..13, then clear latch | `0x19a80@0x19aa2`, `0x3dd30@0x3decd` |
| `0x7fe8c` | `ui_release_all_controls` | Input gate alone; `0xd74d0` for 0..13 | `0x19a80@0x19ad1`, `0x80120@0x80180` |
| `0x802ac` | `ui_release_matching_control` | AX word compared against ten zero-extended table bytes; multiple matching release callbacks | `0x775f0@0x776c3`, `0x7eec4@0x7eeef` |
| `0x803c4` | `ui_reset_active_panel` | Output gate, call `0xd7158` with EAX=0, EDX=1, EBX=word-address contents at `0x1f9020`, ECX=0 | `0xa8a38@0xa8b25`, `0xa8bd4@0xa8d85` |
| `0x80400` | `ui_reset_panel_and_release` | Call `0x7fe8c` then tailcall `0xd6ffc` with EAX=address `0x1f9020` | `0xa8a38@0xa8bc5`, `0xa8bd4@0xa8f46` |

The `0x7fbec` equality case is deliberately inert, and a negative index is
not checked before pointer-table address calculation. These eight functions
have independent source disassembly, typed state and two original caller
functions each; dispatcher callees remain original guest dependencies with
host sink stubs. The host tests are *not* guest differential or splice proof.

### Command packet, status formatting and tactical display (eight-function wave)

| Address | Draft | Register/stack contract and original effects | Two independent callers |
| --- | --- | --- | --- |
| `0x548a4` | `send_packet_15` | AX/DX words, BL byte, ECX dword, two stack dwords; `ret 8`; emit 18-byte `0x15` packet | `0x46254@0x4641f`, `0x572b4@0x57327` |
| `0x55b44` | `send_packet_41` | DX/BX words, CL byte, one stack dword (low word), grid marker byte; `ret 4`; emit 9-byte `0x41` packet | `0x88664@0x886af`, `0x886d0@0x8871b` |
| `0x56974` | `send_packet_2b` | AX/BX/DX words, word at `0x197c28`, byte at `0x197c2e`; emit 10-byte `0x2b` packet | `0x15b7c@0x15fea`, `0x16e60@0x171eb` |
| `0x82790` | `write_ui_status_flags_a` | EAX destination, two zero-extended status bytes, format `0x188b58` via `0xa4342` | `0x82e40@0x82f99`, `0x830b8@0x8314b` |
| `0x827d0` | `write_ui_status_flags_b` | EAX destination, reversed adjacent status bytes, format `0x188b74` via `0xa4342` | `0x82e40@0x82fdb`, `0x830b8@0x8316c` |
| `0x82bd4` | `write_ui_status_float` | EAX destination; promote `float32` to `double`, pass format `0x188bfc` via `0xa4342` | `0x82e40@0x82f15`, `0x830b8@0x8312a` |
| `0x59484` | `draw_tactical_item_mode` | EAX item; global mode byte exactly 1 selects callback mode 0, all others 1 | `0x5c0cc@0x5c14e`, `0x5e21c@0x5e2d2` |
| `0x5331c` | `refresh_tactical_indicator` | Indirect call through `0x15bab8` *before* copying byte `0x1a5c0b` to `0x127d9d` | `0x1cde8@0x1cf73`, `0x1d318@0x1d370` |

All offsets, packet lengths, register order, caller stack cleanup and callback
ordering were checked against original binary disassembly. `packet_status.c`
uses typed original-sink dependencies; host tests exercise complete packet
bytes, status argument order, floating-point promotion and callback ordering.
The eight guest replay specifications are syntactically validated but no guest
differential results are claimed. In particular, host packet tests do not prove
that the original I/O sink accepts packets in the same runtime state.

### Record and tactical-state transitions (eight-function wave)

| Address | Draft | ABI and original side effects | Two independent callers |
| --- | --- | --- | --- |
| `0x59420` | `draw_tactical_coordinates` | AX/DX low signed coordinates, EBX additional argument; double scale in display mode 1, add signed word offsets and call `0x48bb0` with five stack args | `0x59998@0x59ac5`, `0x59adc@0x59f93` |
| `0x594a4` | `draw_tactical_item_metric` | EAX item pointer; mode 1 sign-extends returned AX of `0x48c34` and shifts by one, other mode returns full EAX | `0x59998@0x59a22`, `0x59adc@0x59f7f` |
| `0x66ac8` | `update_position_bin` | Signed upper word at `0x1a823a`, call `0x663a4` with EDX=0, replace overlapping upper word with result or `0xffff` on negative AX | `0x1b4a4@0x1b4d4`, `0x95e58@0x96a3f` |
| `0x741f0` | `report_tactical_object_action` | EAX object; optional signed high-word index, action `0x26` for null link at +0x12, otherwise `0x12` | `0x172b4@0x1732a`, `0x74584@0x746c2` |
| `0x7a3b0` | `build_status_description` | EAX suffix; copy fixed prefix to `0x1f8ec4`, append constant separator, append suffix; return shared buffer address | `0x7a4a4@0x7a4b5`, `0x7a5bc@0x7a5e1` |
| `0x87970` | `refresh_record_selection` | EAX record; signed high-word index from +0x14 selects six-byte entry; copy entry pointer/word, optionally increment index word at +0x16 | `0x87a20@0x87bcc`, `0x89164@0x89177` |
| `0x879c4` | `rotate_ui_record_ring` | Cursor/anchor/head global pointers; initialize missing cursor through `0x878c8`, rotate one doubly linked node, copy unaligned word +0x54 to +0x51 | `0x55928@0x55958`, `0x87db8@0x87dbd` |
| `0x8f5f4` | `reset_ui_record_pool` | Reset 16-bit count, fill `0x2260` bytes with zero at `0x20f550` through `0xb3fb0`, clear pointer | `0x3cfc0@0x3cff7`, `0x8bd58@0x8bdb2` |

These eight functions each have two independent original callsites and separate
host-model assertions for byte layouts, signed word arithmetic, gate behavior,
caller ordering, list links and the pool clear length. `0x1a823a` and
`0x1a823c` are overlapping views of one four-byte state value; the draft
models this explicitly. The ring rotation assumes a valid original ring; it
must not manufacture recovery behavior for corrupt pointers. Fixture schemas
validate, but no guest differential comparison has yet been performed, and
the original guest callbacks are deliberately represented as typed sinks.

### UI fragments, record templates and tactical cross (eight-function wave)

| Address | Draft | ABI and side effects | Two independent callers |
| --- | --- | --- | --- |
| `0x577ac` | `send_packet_2c` | AL/DL/BL/CL and one stack byte; `ret 4`, send six-byte `0x2c` packet then pass original buffer to `0x57820` | `0x9114@0x9254`, `0x10a9c@0x10ceb` |
| `0x5dbc8` | `draw_tactical_cross` | AX/DX coordinates; signed word radius `(offset+2)<<mode`, two five-argument line draw calls | `0x59fa4@0x5a201`, `0x5a220@0x5a2bf` |
| `0x80d40` | `update_ui_record_from_template` | AX signed lookup index, EDX output record; copy four words, sign-extending three, or write sentinel `0xcc` and clear fields | `0x55928@0x55a8d`, `0x87a20@0x87d10` |
| `0x82dd0` | `compact_ui_fragment` | EAX mutable string; use delimiter helper `0x82da4` to compact suffix in place | `0x82e40@0x82fec`, `0x830b8@0x8315c` |
| `0x82e00` | `find_matching_ui_fragment` | EAX list, EDX search string; length `0xb1b8b`, prefix compare `0xe5c03`, delimiter advance `0x82da4` | `0x82e40@0x82fe7`, `0x830b8@0x83157` |
| `0x84370` | `update_ui_composite` | No args; fixed EAX record and three dwords in EDX/EBX/ECX forwarded to `0x83708` | `0x843b0@0x844a3`, `0x844b0@0x844da` |
| `0x866e0` | `ui_record_current_is_active` | EAX record; linear search of four-byte pointer array at `0x12d7b4` for record+`0x3c`, report missing with `0x18a3b8` | `0x86f7c@0x870d7`, `0x873a4@0x874d0` |
| `0x8be54` | `update_object_with_link` | EAX object, EDX link; original unlink-if-current then install-link guest calls | `0x878c8@0x878f3`, `0x8873c@0x887aa` |

These drafts preserve the original packet and callback ordering, word-sized
arithmetic, in-place string writes, signed record fields and sentinel-based
table traversal. The individual helpers use typed callback dependencies where
native guest routines are not yet recovered. Host tests exercise both branches
and ordering; fixture schemas validate, but the runtime guest differential
tests and OMF splices remain pending.

### Line scanning and callback-driven UI state (eight-function wave)

| Address | Draft | ABI and original side effects | Two independent callers |
| --- | --- | --- | --- |
| `0x82da4` | `next_ui_fragment` | EAX guest string; skip to CR/LF/NUL and consume any CR/LF sequence | `0x82dd0@0x82de0`, `0x82e00@0x82e29` |
| `0x83294` | `ui_point_near_reference` | EAX/EDX coordinates minus globals `0x1fcc14/1fcc1c`, call `0xb3988`, return AL=1 on signed result ≤ `0x3b4c00` | `0x834e8@0x835eb`, `0x843b0@0x84479` |
| `0x83708` | `compose_ui_relative_vector` | EAX buffer, EDX/EBX/ECX forwarded to `0x83200`; then subtract word offsets at `0x1fcc44/46` | `0x84370@0x8438a`, `0x84394@0x843a4` |
| `0x84514` | `initialize_ui_component` | No arguments, set status word=1, copy 12 bytes via `0xdc832`, clear byte latch, return AL=0 | `0x84678@0x846a4`, `0x848e0@0x8499b` |
| `0x89924` | `recalculate_object_temporarily` | EAX object, save byte +0x5a, set to 1, clear global marker, conditionally notify action 4, recalculate and restore byte | `0x86968@0x86a79`, `0x89140@0x89158` |
| `0x8b650` | `select_validated_ui_record` | EAX owner; indirect candidate retrieval, conversion through `0xe29e8`, indirect validation, returning validated pointer or zero | `0x8b7e8@0x8b860`, `0x8bad4@0x8bb04` |
| `0x8be3c` | `update_object_remove_link` | EAX/EDX record/link, sequence of guest callbacks `0x8f3f4` then `0x90880` | `0x87a20@0x87d02`, `0x88134@0x881a4` |
| `0x92254` | `prepare_navigation_component` | EAX object and EDX nonzero gate, prepare `0x2143e4` through `0xdc91c`, complement both words and subtract `0x400` from first | `0x9457c@0x945be`, `0x94954@0x94a07` |

Recovering `0x82da4` resolves the original delimiter syntax for the prior
`0x82dd0` and `0x82e00` drafts: these are **CR/LF-separated lines**, not
arbitrary token separators. The corresponding host fixtures now exercise both
line-feed and CR/LF advancement. `0x83708` is also the direct target of the
previous `0x84370` dispatcher, giving a recoverable two-function UI state
path. Host behavior is tested, but the indirect guest callbacks and their
clobbers still require differential replay.

## Command-file parser, token lifetime and packed records (2026-11-03)

Eight original helper bodies in the `0xe710..0xee9c` region were checked
against two independent direct callers each. These are ABI-reviewed host
**drafts**, not equivalent Watcom objects or guest-verified splices.

| Entry | Interface / return | Independent caller instructions | Side effects and boundaries |
| --- | --- | --- | --- |
| `0xe710` | no register inputs or consumed return | `0x4bf9` (`0x4bf8`), `0x4e34` (`0x4e14`) | if `0x1379c4` is nonzero, release optional pointer at `root+0x0a` first, then *re-read* and release the root; zero global |
| `0xe7dc` | no input; return ignored | `0xf739` (`0xf6a8`), `0xff2c` (`0xfecc`) | clear dwords at `0x121eb4`, `0x121eb0`, `0x121eac`; copy byte `0x180c08` to `0x1976b0` and `0x197abe`; clear cursor word `0x121ea8` |
| `0xe89c` | no input, EAX token length | `0xe95c` (`0xe95c`), `0xe980` (`0xe980`) | skip spaces/tabs, optional double quotes, stop at whitespace/comma/NUL or closing quote; write at most 80 bytes, then NUL, and write 16-bit cursor |
| `0xe980` | no input, EAX conversion result | `0xeda3` (`0xed80`), `0xee73` (`0xee5c`) | tokenize, tail jump to `0xcc76d` numeric decoder with guest token address in EAX |
| `0xe95c` | no input, signed EAX or `-1` | `0xedd8` (`0xed80`), `0xee49` (`0xee0c`) | tokenize, check byte length at `0x197abe`, return `-1` for empty token or tail jump to `0x9d4cd` signed numeric decoder |
| `0xe990` | no input, EAX allocated guest pointer | `0xef15` (`0xeefc`), `0xf282` (`0xf260`) | tokenize, indirect allocation via `[0x121ebc]` of token length plus NUL, copy via `0x9d749` |
| `0xe9bc` | no input, EAX 0..10 (0 on failure) | `0xf272` (`0xf260`), `0xf627` (`0xf604`) | compare token to eleven guest pointers starting at `0x132f5c`; on failure call indirect reporter `[0x132a74]` with format `0x180c48`, token, and `0x121eac` context |
| `0xee5c` | EAX destination; returns EAX=1 | `0xf2ba` (`0xf260`), `0xf509` (`0xf314`) | three `0xe980` dword numeric conversions, three `0xe95c` word conversions at destination `+0x0c/+0x0e/+0x10` |

The original token scanner indexes source with `movsx` of the 16-bit
cursor; after 80 copied bytes it leaves the cursor in the middle of an
unconsumed token. The repeated empty token on comma is an original behavior,
not an invented delimiter skip. The apparent decimal and signed integer
decoders at `0xcc76d` and `0x9d4cd` have matching observed ASCII-decimal
instruction patterns, but both remain separate original guest dependencies
until their full contracts are independently audited. The record decoder
depends directly on the new numeric-token wrappers, so the host test covers
their composition as well as individual error and boundary paths.

Live original guest tests, OMF compilation, callback clobber verification,
and whole-game replay remain outstanding. In particular, the indirect
allocator and error reporter are modeled with host stubs; the fixture for
the former supplies a synthetic guest allocator return.

## Seven dictionary enums and two literal families (2026-11-03)

Eight nearby original parsing functions share the `0xe89c` tokenizer
and `0xa4380` zero-on-match string comparison. The following signatures
have two independently identified callers each:

| Entry | Enum source | Distinct caller instructions | Failure format |
| --- | --- | --- | --- |
| `0xea0c` | literal addresses `0x180c84/88/8c/90`, `0x180c94/9c/a4/ac` | `0xf614`, `0xfd02` | `0x180cb4` |
| `0xeaf8` | 30 pointers at `0x132e6c` | `0xee38`, `0xf62f` | `0x180cf0` |
| `0xeb48` | 39 pointers at `0x132d34` | `0xf360`, `0xfd1a` | `0x180d2c` |
| `0xeb9c` | 18 pointers at `0x1333f8` | `0xf37e`, `0xfd24` | `0x180d68` |
| `0xebec` | 27 pointers at `0x133488` | `0xf39c`, `0xfd2e` | `0x180da0` |
| `0xec8c` | 106 pointers at `0x132fb4` | `0xef0e`, `0xf27a` | `0x180e18` |
| `0xece0` | 10 pointers at `0x13336c` | `0xf3c7`, `0xf644` | `0x180e54` |
| `0xed30` | 19 pointers at `0x132c2c` | `0xedbe`, `0xee30` | `0x180e8c` |

All eight return a full EAX zero-based index, with zero also serving
as the failure return after an indirect diagnostic at `[0x132a74]`.
The two-family parser maps both groups to values 0, 1, 2, 3, and does
not report an error when the zero-value guard string matches.
The seven tables use dword byte offsets with strict less-than
comparisons, never a NULL-pointer sentinel. The new C drafts
share a helper but retain the per-entry table bounds and diagnostics.
Host tests check first and last dictionary matches and all not-found
paths; guest fixtures are structural plans only.

## Parser lines, timing and packed runtime coordinates (2026-11-03)

Eight additional original bodies were examined, with two distinct direct
callers per function. These are host-tested **drafts**, not completed splices.

| Entry | Inputs and outputs | Independent caller instructions | Key behavior |
| --- | --- | --- | --- |
| `0xd530` | EAX/EDX/EBX guest output pointers; no consumed return | `0x418dd`, `0x45a79` | copies word globals `0x197696`, `0x19bb5e` and byte `0x121e8c` in that order |
| `0xe810` | no input; AL is first byte of accepted line | `0xed85`, `0xee0f` | token cursor reset; read source via `0xe774`; strip leading spaces/tabs; skip semicolon and blank lines; increment line count `0x121eac` |
| `0xf8f8` | no input; no consumed return | `0xb1b7f`, `0xca7e3` | free optional buffer `0x1a5904` through guest `0xcc768`, then clear the pointer |
| `0xfe4c` | EAX record pointer; EAX=1 | `0xf5d3`, `0xfe36` | read line, skip a token, allocate next token; store pointer `+0x00`, five bytes `+0x05..09`, six signed words `+0x0a..14` |
| `0x10640` | no input; EAX tick sample | `0x1064c`, `0x1065e` | call `[0x15b620]`, store resulting EAX at `0x197c14`; callback clobber contract still open |
| `0x1065c` | no input; signed EAX delta clamped to at least 2 | `0x3ddea`, `0xf3d61` | call `0x10640`, subtract prior tick `0x197c10` with dword wrap, update prior tick |
| `0x106b0` | AX heading, EDX/ECX signed coordinates, one stack dword carrying a byte; `ret 4` | `0x92a6`, `0x15fbc` | arithmetic shift 11, two signed residuals centered at 1024, clear latch, store code; EBX clobbered |
| `0x10f28` | AX unsigned mode 0..3, EDX/EBX pointers; no consumed return | `0x10fb5`, `0x11092` | mode 1 swaps with one negation; mode 2 negates both; mode 3 swaps with other negation; all other modes unchanged |

The original `0xe810` reports input lines of length at least 1024 but
continues writing past that bound afterward: **ORIGINAL BUG**, preserved
in the draft. The `0x106b0` signed right-shift is floor division by 2048,
not C truncation toward zero, and only 16-bit residuals are stored. The
delta at `0x1065c` is compared as signed after wraparound.

The fixture for the parser line supplies a synthetic immediate EOF reader;
the tick fixtures supply an indirect-call stub. Real file callbacks and
timer state need original guest replay before any equivalence claim.

## Indexed callbacks, dialog state and segmented addresses (2026-11-03)

Eight further original functions have two identified direct callers each.
The host drafts remain unverified at the guest/Watcom level.

| Entry | Inputs and output | Caller instructions | Observed effect |
| --- | --- | --- | --- |
| `0x604` | EAX guest pointer to full EAX packed result | `0xb1f`, `0xddd8e` | subtract global pointer base `0x120cc6`, encode 16-bit `(delta>>4)` above 4-bit `(delta&15)` |
| `0x12160` | no inputs; void | `0x5f6b`, `0x5fde` | mark mode active, reset two state fields, visit 54 records at 29-byte stride, invoke callback at `+25` or clear record flag, refresh every slot, clear mode |
| `0x121cc` | signed AX subindex and DX group; AL flag | `0x65a6`, `0x1f8a8` | index overlapping dwords at `0x121ec6 + DX*2`, sign-extend high word, add AX, select byte at `0x121fb4 + 29*index` |
| `0x121f0` | no inputs; void | `0x5f66`, `0x5fd9` | guest query `0xce02c` receives two stack scratch dword pointers; discard outputs then set word `0x121ed0=150` |
| `0x14ef4` | no inputs; void | `0x5fbf`, `0x6008` | invoke `0x14d58` exactly 34 times, EAX 0 through 33 |
| `0x1d5cc` | EAX object pointer, signed DX index; void | `0x1e126`, `0x3953f` | optional `0x55dac` when byte `0x127da9` nonzero, then unconditional `0x1d5f4` |
| `0x4f314` | EAX/EDX widget values; void | `0x4b761`, `0x4cad5` | open named dialog, select two child widgets, bind each value, emit dialog byte `+16`, destroy dialog |
| `0x4f6bc` | no inputs; void | `0x4f48b`, `0x4f53f` | scan signed count `0x1357b0` with a BL byte index, map through `0x1a5510`, clear nonzero flags at `0x1a553b` and call `0x4f314` |

The original `0x4f6bc` uses an eight-bit `BL` counter but compares the
zero-extended 16-bit AX against a signed 16-bit limit. A positive limit
above 255 will repeatedly wrap through the same slots (**ORIGINAL BUG**).
The C draft preserves that behavior. The `0x121cc` packed dword starts at
the same address as the previously identified `0x121ec6` coordinate latch;
the overlap is intentional and must remain in the memory layout.

The dialog callback sequence, and the liveness of registers across
indirect calls, still need actual original-versus-replacement execution.
The table above records direct call sites and a host semantic model only.

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

## Marker packet wrappers and grid-row store (2026-11-03)

Eight further original functions. Each has one direct caller in the Ghidra
call table; indirect callers are not excluded. The host drafts remain
unverified at the guest/Watcom level.

| Entry | Inputs and output | Caller instruction | Observed effect |
| --- | --- | --- | --- |
| `0x545e4` | none; void | `0x7e7b8` | send one-byte packet `1d` through `0xa3f74` |
| `0x54600` | none; void | `0x55855` | send `26, byte 0x1a666b` (two bytes) |
| `0x54624` | none; void | `0x5591e` | send `1e, byte 0x1a666b` (two bytes) |
| `0x54648` | none; void | `0x7ed37` | send one-byte packet `1f` |
| `0x54664` | signed AX, DL byte; void | `0x54f95` | send `24, AXlow, AXhigh, DL` (four bytes) |
| `0x54e5c` | none; void | `0x54ea4` | send `1b, byte 0x1a666b` (two bytes) |
| `0x54e9c` | EAX record pointer; void | `0x583d4` | copy record `+1` to `0x12583b`, call `0x54e5c`, set `0x1a5d1c=1` |
| `0x550b0` | EAX record pointer; void | `0x582dc` | if marker `0x1a666b` equals record `+3`, store it at byte 0 of grid row `movsx(word record+1)*0xca` from `0x19bb58` |

Registers: the packet wrappers push and pop EDX (or, for `0x54664`, take DL)
and only clobber EAX; the packet helper `0xa3f74` is a guest dependency.

## More packet wrappers and scene latches (2026-11-03)

Eight further original functions, each with a single direct caller. The host
drafts remain unverified at the guest/Watcom level.

| Entry | Inputs | Caller | Observed effect |
| --- | --- | --- | --- |
| `0x55760` | AX, DX words | `0x8d7ae` | send `4f, AX, DX` (five bytes) |
| `0x55860` | EAX record | `0x58437` | clear byte at `0x1a5c32 + 23*record[1]` (unsigned), then call `0x544c0` |
| `0x55bd0` | AX, DX words, BL | `0x851e7` | send `42, AX, DX, BL` (six bytes); BH is overwritten by the opcode |
| `0x56004` | none | `0x58042` | clear words `0x1a6630` and `0x127dae` |
| `0x561bc` | EAX dword | `0x561ab` | send `2a, EAX, marker` (six bytes) |
| `0x561e4` | EAX record | `0x58455` | if `dword[+1] & 15 == marker`, call `0x562d8` with the dword and byte `+5` |
| `0x56954` | EAX record | `0x58359` | call `0x87f9c` with byte `+0xd` and signed high word of `+0xc`; nonzero result calls `0x878c8` |
| `0x56d40` | none | `0x56d1b` | send `30, marker, 0` (three bytes) |

## Further packets, bounded counters and mode limits (2026-11-03)

Ten more original functions, each with one or two direct callers; host drafts
only, unverified at the guest/Watcom level.

| Entry | Inputs | Observed effect |
| --- | --- | --- |
| `0x571f0` | AL, DX word, BL | send `3e, AL, DX, BL` (five bytes) |
| `0x5728c` | AX, DX words, BL | send `3b, AX, DX, BL` (six bytes); BH overwritten |
| `0x57348` | AX word, DL | send `3d, AX, DL` (four bytes) |
| `0x575fc` | AX, DX words | send `40, AX, DX` (five bytes) |
| `0x57650` | AX word | send `2e, AX` (three bytes) |
| `0x576a4`, `0x576d4`, `0x57704` | EAX record | if signed word `+1` < 4, increment byte at `0x19b960`/`0x19b96c`/`0x19b966` plus that index; there is no lower bound (**ORIGINAL BUG** kept) |
| `0x58d04` | none | words `0x1a6ce2=4`, `0x1a6ce8=5` |
| `0x58d44` | none | words `0x1a6ce2=8`, `0x1a6ce8=10` |

## Runtime word, record lookup and counter helpers (2026-11-03)

Eight more original functions; host drafts only, unverified at the guest/Watcom level.

| Entry | Inputs | Observed effect |
| --- | --- | --- |
| `0x6454c` | none | call `0xd0b68`, then `0xd0ebc` on object `0x1a7c30` with zero-extended word `0x127d3e` (only direct caller is the function at `0x624f4`) |
| `0x64524` | none | same, with the word negated modulo 16 bits |
| `0x68bf4` | none | words `0x1f6f38`, `0x1f6f42` from `0xde864` on `0x186268`, `0x186270` |
| `0x69080` | none | word `0x1282dc` from low word of `0x9ddfc(0x186278)` |
| `0x6a0f8` | AL | tail call `0x9ddfc` with dword at `0x128564 + 0xe5*AL` |
| `0x6b420` | AL | AL = 1 iff byte `0x128568 + 0xe5*AL` is 4 (2, 3 and the rest return 0) |
| `0x77afc` | none | `0xa45b3` returns AX; EAX = dword `0x12a760[AX/0x230]` |
| `0x7772c` | none | words `0x1f891e=0xffff`, `0x1f891c=0` |

## Status value formatters and small state helpers (2026-11-03)

Ten more original functions; host drafts only, unverified at the guest/Watcom level.
The seven formatters follow the existing `0xa4342` cdecl convention (destination
in EAX, format string address, one value).

| Entry | Observed effect |
| --- | --- |
| `0x82720`, `0x8273c`, `0x82774`, `0x82bf0` | format byte `0x1fcb30` / `0x1a5c0d` / `0x15b7e4` / `0x15bdd0` with `0x188b24` / `0x188b34` / `0x188b4c` / `0x188c10` |
| `0x82758` | format signed high word of dword `0x12b364` with `0x188b40` |
| `0x827b4` | format the EAX result of `0x810ec(dest)` with `0x188b68` |
| `0x827f4` | format dword `0x122dc4` with `0x188b84` |
| `0x845a4` | word `0x12b4a6=2`, byte `0x1fcc43=1`, AL=0 (AH=1 left set) |
| `0x81794` | word `0x1fcaec` from `0xde864(0x1889d4)` |
| `0x7a434` | release pointer `0x12afc4` through `0xc3f80` when nonzero, then clear it |

## misc_state (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x85400` | AX:int16_t | none | 857bc@85954,857bc@8599a | row[0x8e]=row[0x8f]-1 for grid row AX |
| `0x8828c` | none | none | 8846c@8846f | clear bytes 0x12d7aa..ac and word 0x20f196 |
| `0x89140` | EAX:ef_addr32 | none | 86968@86a6d | record+0x26 = trunc(dword + double 0x18a4e0) then 0x89924 |
| `0x8ba78` | none | none | 8bd58@8bdb7 | dword 0x12db5c=0 |
| `0x8bdd4` | none | none | 3ea0c@3eec5 | dword 0x132ad4=dword 0x20f420 |
| `0x8f5d8` | EAX:ef_addr32,EDX:uint32_t | none | 8be54@8be5a | if 0x8c09c(record,value) and value==record+0x1c then clear it |
| `0x90924` | EAX:ef_addr32,EDX:uint32_t | none | 8be54@8be63 | if 0x90050(record,value) and value==record+0x1c then clear it |
| `0x91238` | none | none | 92c28@92ceb,92c28@92daf | byte 0x12db9e=0x35, word 0x21449c=1, 0xd0b68(0x12dbec) |
| `0x919b8` | none | none | none direct | if byte 0x12db9e==0x36 call 0xdc334(1,1) |
| `0x91c00` | AX:int16_t,DX:int16_t,BX:int16_t | EAX:uint32_t | 95e58@97009 | 0xe2b60(x*x+y*y+z*z) |
| `0x91c34` | none | AL:uint8_t | 1c780@1cb47,1c780@1cc61 | word 0x2144ae != word 0x2144ac |
| `0x94948` | EAX:ef_addr32 | EAX:uint32_t | 95e58@96552 | 0x9457c(record, dword record+0x30) |
| `0x94ce8` | none | none | 95e58@96cc3 | clear words 0x12dba2/a4/a6 then 0x94a74 |
| `0x94ffc` | none | none | 95e58@96320 | filtered lookup of high word 0x21449e; if found store 0x214458 and 0x94d08; falls into 0x95018 |
| `0x9783c` | EAX:ef_addr32,EDX:uint32_t | none | 9775c@9782c,97860@978bd | word+6=0 word+8=EDX-1 tail 0x975f0 |
| `0x97dc0` | none | EAX:uint32_t | 97dcc@97edb,97dcc@97f27 | 0x10640 then return dword 0x197c14 |
| `0x97f70` | none | EAX:uint32_t | 3ea0c@3ed91,3ea0c@3ed9f | 0x9c858 result or 0x18acb0 |
| `0x98000` | none | none | 8b39c@8b4bf | if byte 0x420a72 tail 0xd5f0c(dword 0x12dd98) |
| `0x98558` | none | none | 49228@494ef | dword 0x12dd9c=0x9850c, 0x408580=0x1f4000 |

## low_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf60` | EAX:int32_t | EAX:int32_t | 1ef8@1f58,1f64@1f87,1f64@20a0 | zero divisor returns 0 else low-word random (0x9c960) modulo divisor |
| `0x3cd4` | EAX:int32_t,EDX:int32_t,EBX:int32_t | EAX:int32_t | 2e04@304c,2e04@30d9,2e04@31a3 | signed maximum of three |
| `0x6bf4` | none | none | 5f60@5fb0,5fd8@6003 | clear words 0x197244/46/48 and bytes 0x197254, 0x121e4f |
| `0x73d4` | EDX:ef_addr32 | AL:uint8_t | 759c@7753 | !(double record+0x71 > double 0x1803c4) |
| `0x1064c` | none | none | 52f38@52f94 | 0x10640 then dword 0x197c10 = dword 0x197c14 |
| `0x10688` | none | EAX:uint32_t | 53270@5327c | 0x10640; return 0x197c14 - 0x197c10 and latch |
| `0x11d20` | AX:int16_t,DL:uint8_t | none | 12210@12259,12210@12297 | byte 0x121fb4+29*AX = DL then 0x11e7c |
| `0x12140` | none | none | 11d44@11dd1 | 0x11e7c for 0..53 |
| `0x14d3c` | AX:int16_t | AL:uint8_t | 589c0@58c62 | byte 0x1225d4+19*AX |
| `0x151a0` | none | none | 58d64@58d69 | bytes 0x12289e=1 0x12289f=0 0x1228a0=0 and 0x197c50=byte 0x181730 |
| `0x17388` | AX:int16_t | EAX:uint32_t | 16588@167ed | 0x8ffe8 lookup; or 3 into +0x30; tail 0x3739c(high word +0x36) |
| `0x173a4` | EAX:int32_t | none | 168d8@1696a | row[0x8e]=row[0x8f]-1 with 32-bit row index |
| `0x27538` | EAX:uint32_t,EDX:uint32_t | none | 275a4@275c9 | dword 0x19b430=EDX 0x19b434=EAX |
| `0x28a70` | AX:int16_t | AL:uint8_t | 28ed8@28f8a | filtered lookup; dword +0x1c nonzero |
| `0x28a8c` | AX:int16_t | AL:uint8_t | 291fc@292e2,291fc@29347 | 0x8ffe8 lookup; dword +0x1c nonzero |
| `0x2a4a0` | EAX:int32_t,EDX:int32_t,EBX:int32_t,ECX:int32_t | AL:uint8_t | 2a4c8@2a57a | signed range check against 0x1a5bec and 0x1a5be8 |
| `0x2a794` | none | none | db21c@db27b | clear dwords 0x19bb38/3c/40 |
| `0x32b3c` | none | EAX:uint32_t | none direct | tail 0x32c04 with EAX=0x182ad8 |
| `0x32b48` | none | EAX:uint32_t | none direct | tail 0x32c04 with EAX=0x182adc |
| `0x32b54` | none | EAX:uint32_t | none direct | tail 0x32c04 with EAX=0x182ae0 |
| `0x32b60` | none | EAX:uint32_t | none direct | tail 0x32c04 with EAX=0x182ae4 |
| `0x32b6c` | none | EAX:uint32_t | none direct | tail 0x32c04 with EAX=0x182ae8 |
| `0x32b9c` | none | EAX:uint32_t | 95e58@95ff3 | tail 0x32c04 with EAX=0x182af8 |
| `0x32ba8` | none | EAX:uint32_t | 4d668@4d6e7 | tail 0x32c04 with EAX=0x182afc |
| `0x33750` | none | none | none direct | tail 0xa41c8 when byte 0x1357c8 nonzero |
| `0x3918c` | none | none | 3ea0c@3ee50 | dword 0x1a1780=0x53ecc and byte 0x1a363a=0 |
| `0x3bd28` | AX:int16_t | AL:uint8_t | 3a3a8@3b443 | 0 when row byte +0x1a in 0x0a..0x0c else 1 |
| `0x3c434` | none | EAX:uint32_t | 7ab00@7abbf | tail 0xe2af4(dword 0x1a2e78) |
| `0x3c768` | AX:int16_t | none | 3cb2c@3cbf8 | copy byte +0 and word +2 of 0x3c730 entry into record 0x1264c8 at +0x0b/+0x2a |
| `0x3cd24` | none | EAX:uint32_t | 7ab00@7ac44 | 0x3ccec object then 0x3cc94 and 0xe2af4 |
| `0x3cd40` | none | EAX:uint32_t | ad440@ad475 | 0x3ccec object then 0x3cb2c, 0xe2af4; returns dword 0x1a2e98 |
| `0x3ce04` | none | none | 3cfc0@3d2f0 | 258 dwords at 0x1a3150.. random(0x1ff)-0x100 |
| `0x3dcb0` | none | none | 3dd30@3e015 | 0x394e0, 0x8846c, tail 0x888cc |
| `0x410d0` | none | none | 43020@430ef | 0x53270, byte 0x125847=0, 0x8bd58(0x1a58b4,0x1a5a1c), 0x49228 |
| `0x41df4` | AL:uint8_t | EAX:uint32_t | 9a998@9aa20 | AL is 0x0b or 0x0c |
| `0x41e0c` | AL:uint8_t | EAX:uint32_t | 41b98@41c78 | AL is 0x26 or 0x27 |
| `0x41e84` | AL:uint8_t | EAX:uint32_t | 9a998@9aa5c | AL is 0, 0x16 or 2 |

## mid_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x41ea0` | AL:uint8_t | EAX:uint32_t | 9a998@9a9fe | AL in {0,1,5,6,7} |
| `0x41f00` | AL:uint8_t | EAX:uint32_t | 9a998@9a9e0 | AL in {2,3,0x18,0x19} |
| `0x47ee4` | none | none | 47f48@47f93 | byte 0x126654=0, dword 0x126688=0x12665c |
| `0x47f9c` | none | none | 47fbc@48015,77c18@77c22 | 0x46070; clear byte 0x126655 when high word of 0x12668e is 0xff |
| `0x50d9c` | none | none | 50e84@50ebe | map bytes of 0x1a5968 through 0xe7835 until zero |
| `0x55084` | EAX:ef_addr32 | none | 581b0@582b8 | filtered object word+0x18 = marker when record+3 equals marker |
| `0x5511c` | EAX:ef_addr32 | none | 581b0@58336 | marshal record fields to 0x46454 (4 regs + 2 stack dwords) |
| `0x55884` | EAX:ef_addr32 | none | 581b0@58428 | byte 0x1a5c32+23*record[1]=1 unless byte 0x1f9012; always 0x544c0 |
| `0x56928` | EAX:ef_addr32 | none | 581b0@58368 | copy 3 dwords, 0x1d5f4(triple, high word +0xd) |
| `0x56c90` | none | none | 56c7c@56c82 | packet 4e marker word then 0xa3f30 |
| `0x56ed0` | none | none | 56e94@56ec7 | packet 33 marker word then 0xa3f30 |
| `0x56d68` | EAX:ef_addr32 | none | 581b0@584af | when byte 0x1357c8 clear bit record[1] of word 0x1a662e |
| `0x56fa8` | EAX:ef_addr32 | none | 581b0@584eb | words +1,+3,+5 to 0x12583c/3e/40 |
| `0x56fe8` | none | none | 581b0@584fa | byte 0x1264e1=1 when marker is 0 |
| `0x57670` | EAX:ef_addr32 | EAX:uint32_t | 581b0@58491 | filtered lookup word+1; tail 0x741f0 |
| `0x5f8a8` | none | none | 5f60@5f89 | 0x9e508(high word obj+0x1e,0x24) for object from 0x1f71c8; clear byte 0x1a6d18 |
| `0x65498` | EAX:ef_addr32 | EAX:uint32_t | 646d4@64d88 | dword +0x16 or dword 0x1a7c54 when null |
| `0x70418` | none | EAX:uint32_t | 6c488@6c5cf | 0x6e390(byte obj+0x18c +2, 3) with obj dword 0x1f71bc |
| `0x757b0` | EAX:uint32_t | EAX:uint32_t | 759b8@75bbf | zero becomes 1 |
| `0x77950` | DX:uint16_t | EAX:uint32_t | 8748@8823,8748@8855,8748@88ec | 0xa1a90(sx DX, 0xa1a40()) unless null, 0 or 0xffff |
| `0x80d20` | DX:int16_t | EAX:uint32_t | 80d40@80d4b | 0xa1a90(sx DX, 0xa1a40()) or 0 |
| `0x77b20` | none | EAX:uint32_t | 779f4@77a1c | dword table 0x12a934[selector word / 0x666] |
| `0x77b44` | none | EAX:uint32_t | 779f4@77a51 | dword table 0x12a9d4[selector word / 0x333] |
| `0x77b68` | none | EAX:uint32_t | 779f4@77a60 | dword table 0x12ab14[selector word / 0x1c3] |
| `0x77b8c` | none | EAX:uint32_t | 779f4@77a43 | dword table 0x12ad58[selector word / 0x1745] |
| `0x77bb0` | none | EAX:uint32_t | 779f4@77a2a | dword table 0x12ad84[selector word / 0x3fff] |
| `0x77c18` | none | none | 77c90@77ce7,77c90@77d02,77c90@77d2b | dword 0x12adf8=dword 0x126694 then 0x47f9c |
| `0x7a298` | none | EAX:uint32_t | 3dd30@3e877 | tail 0x79ff0 with EAX=0x12af60 |
| `0x7a454` | none | none | 3ea0c@3eeca | 0x7a9bc if dword 0x12afd0; clear 0x1f8ea0,0x12afd0; 0x12afc0=-1 |
| `0x7a510` | none | EAX:uint32_t | 7a738@7a832 | tail 0xe2af4(dword 0x1f8e90) |
| `0x7ccd0` | none | EAX:uint32_t | none direct | tail 0xb1a10 with EAX=0x1379bc |
| `0x7e89c` | none | AL:uint8_t | 7e8c0@7e953,7e8c0@7eb1d | slot=high word 0x19bb5c; valid unless 0xff and 0x2c358 nonzero |
| `0x7f6ac` | EAX:ef_addr32 | EAX:uint32_t | 7f6d8@7f744 | 0 when high word +0x16 > 7 or word +0x18 equals marker |
| `0x803e8` | none | none | a8bd4@a8e5c | poll 0xd7594(0) until nonzero low byte when byte 0x15bac6 |

## scene_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x815f4` | AL:uint8_t | EAX:uint32_t | 81448@815a2 | tail 0xd72b4(AL) when byte 0x12b104 set else 0 |
| `0x83734` | EAX:ef_addr32 | none | 83a40@83a78 | 0xe99b8 then three dwords >>= 16 (arithmetic) |
| `0x84394` | EAX:ef_addr32 | none | 845c0@845df,845c0@84611 | 0x83708(0x1fcc3e, dwords 0/4/8) |
| `0x844e4` | none | AL:uint8_t | 84678@8469d | word 0x12b4a6=1, 0xdc832, byte 0x1fcc43=0, AL=0 |
| `0x88548` | none | none | 8846c@88474 | dwords 0x20f14c=0x7fffffff 0x20f154=0 0x20f150=filtered lookup |
| `0x88d08` | EAX:ef_addr32,EDX:ef_addr32 | none | 8a1dc@8a2e9,8a1dc@8a2f7 | copy six dwords EDX to EAX |
| `0x89960` | EAX:ef_addr32 | none | none direct | byte 0x12d7b3=0; if byte 0x125847 0x9e508(high word +0x2c,4); 0x8a318 |
| `0x8b530` | none | none | 8bb50@8bb53 | copy 0x8e dwords of filtered object to 0x2117b0 |
| `0x8cd48` | none | none | 49228@4970e | word 0x12db60=1; 0x8c0f0(0); store signed word at 0x1f71d0 and obj+0x16 unless -1 |
| `0x8fe30` | none | none | 8fe58@8fe5e | 0xb3fb0(0x211ac8,0,4) and (0x211a00,0,0xc8) |
| `0x94194` | none | none | 95e58@961d5,95e58@96217 | clear words 0x12dba0/a6; 0x91df4 0x9202c 0x93a8c; scene refresh |
| `0x941bc` | none | none | 95e58@96240 | identical body to 0x94194 |
| `0x978e8` | EAX:ef_addr32 | none | 97464@974a0 | if word +0xa: high word +6 <= 2 -> 0x979c8,0x97914; then 0x97950 |
| `0x97d90` | AX:int16_t,DX:int16_t,EBX:uint32_t | none | 97dcc@97f1d | 0xe30e0(0,x+1,y+1,EBX) and (6,x,y,EBX) |
| `0x984b4` | none | none | 19a80@19ad6,3ea0c@3eed9 | 0xde4b0 if byte 0x420a72==2; dword 0x12dcfc=-1; byte 0x12dd00=1 |
| `0x982d4` | none | none | 3dd30@3dfc7 | tail 0xde3b4(high word 0x12dcf6) when sound flags set |
| `0xa0e98` | AX:int16_t | AL:uint8_t | 3d950@3d97d,3da24@3da52 | bit 4 of byte +0xa of 0xb3f58 record |
| `0xa0ea8` | AX:int16_t,DX:uint16_t,BX:uint16_t,CX:uint16_t | none | 1d474@1d4fe,2f740@2f7a8,777f4@778a7 | words +2 +4 +6 of 0xb3f58 record |
| `0xa1858` | AX:int16_t | AL:uint8_t | 10f70@10faf,11014@11038,1e214@1e22b | bits 5-6 of byte +0xa |
| `0xa1a10` | AX:int16_t | EAX:ef_addr32 | cd150@cd193,e8484@e84e6 | table 0x40d200[word record] entry+0xa+6*byte +9 |
| `0xa2e4c` | AX:uint16_t | none | a2e6c@a2e7b | 0xb3fb0(dword 0x1368bc[AX],0,0x21) |
| `0xa2e6c` | none | none | a3c18@a3cc5,a3da0@a3de4 | 0xa2e4c for 0..7 |
| `0xa2e90` | AX:uint16_t | none | a2f70@a303a,a3c18@a3ce4 | 0xe9a24(0x21,0x135840,dword 0x1368bc[AX]) |
| `0xa3108` | AX:uint16_t,DX:uint16_t | EAX:uint32_t | a312c@a3186 | tail 0xec0ec when byte 0x13579a clear else 0 |
| `0xa38c8` | none | none | a3c18@a3d81 | clear 8 words 0x1357d8, 8 words 0x13580a, 8 dwords 0x1357e8 |
| `0xa3a10` | EAX:ef_addr32 | AL:uint8_t | 34844@34858 | nonempty string with 0xb1b8b result <= 0x10 |
| `0xa3b48` | none | none | a3c18@a3c5a | 0xdeb87(0x136dd8,-1,8) |
| `0xa3bfc` | none | none | a3c18@a3c5f | dwords 0x1368bc[i]=0x136dfc+0x21*i for i<8 |
| `0xa4318` | none | none | 3dd30@3de2d | clear dwords 0x135820 0x135824 0x135828 |
| `0xa4421` | EAX:ef_addr32 | EAX:ef_addr32 | 3b58@3be9,3b58@3c3e,2aa34@2aab1 | uppercase a..z in place, return pointer |
| `0xa4b5c` | EAX:ef_addr32 | none | f1c58@f1d1f | when byte +0x11 clear: 0xa4a94 0xa4a20 on byte +0x10, 0xa6038 |
| `0xa4c84` | none | none | 4738@4847,494c@4a16,494c@4a52 | 0xa4b80(i) for i=1..word 0x41eed8 |
| `0xa4e98` | none | none | ac16c@ac1a5 | clear dwords 0x41edfc..0x41ee9c (41 entries) |
| `0xa4ec0` | EAX:ef_addr32 | none | a59d8@a5a01 | words +0x5f=4 +0x63=2 +0x61=4 +0x5d=2 or 0x14 with flag |
| `0xa51dc` | EAX:ef_addr32 | none | a520c@a530a,a5484@a54e6,a59d8@a5a4e | child word +4 = word +0x45 (-0x44 with flag 0x40) |
| `0xa54f0` | EAX:ef_addr32 | none | a59d8@a5a1d | 0xa5484(widget, high words +0x3f,+0x41) |
| `0xa56f8` | EAX:ef_addr32 | none | ba5c@bba1,bfcc@c4a0,a570c@a575a | words +0x69 +0x6b = 0; tail 0xf13ac |
| `0xa5980` | none | none | ac2e4@ac3d7 | 0xa5764(dword 0x41edfc[i]) for i=1..word 0x41eed8 |
| `0xa59ac` | EAX:ef_addr32 | none | a59d8@a5a47 | clear child words +0xe +0x10 +0x12 unless flag 2 |
| `0xa66a0` | EAX:ef_addr32 | none | a520c@a5318,a59d8@a5a2b,f13ac@f13b6 | children with low five bits of +0x8c: 0xa6438 |
| `0xa6a64` | EAX:ef_addr32 | none | a59d8@a5a39 | children without those bits: 0xa69d0 |
| `0xa7380` | EAX:ef_addr32 | none | bfcc@c2c8,bfcc@c2f2,bfcc@c31c | clear bit 0 of +0x1d and 0xa6d00 |
| `0xa739c` | EAX:ef_addr32,DL:uint8_t | none | 4f18@4fdb,4f18@4fe4,4f18@4fed | bit 6 of +0x1d = DL&1; 0xa5ef8(byte +0x50) unless byte 0x41f2e5 |
| `0xa7408` | EAX:ef_addr32,DL:uint8_t | none | 34a98@34abb | apply 0xa739c to children without the low five bits |
| `0xa7434` | EAX:ef_addr32 | none | 35e68@35e7b,35e90@35ea3,362c4@36313 | 0xa7380 on children with bit 0 of +0x1d |
| `0xa7b80` | none | EAX:uint32_t | a99c0@a99e2,a99c0@a99ec,aad7c@aadb4 | tail 0x9c858 with EAX=0x18d57c |
| `0xa7b8c` | none | none | a9bfc@a9c9d | dword 0x4191c8 = 0xb3f48() |

## list_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa7b98` | EAX:uint32_t | none | aa5c4@aa5f2,aa5c4@aa65c,aa5c4@aa6af | Dword 0x4191c8 += EAX. |
| `0xa7bb0` | none | none | a98d0@a98d0 | Dword 0x4191c4 = 0xb3f50(). |
| `0xa7bbc` | EAX:uint32_t | none | aaa50@aaada,aaa50@aab28,aaa50@aab59 | Dword 0x4191c4 += EAX. |
| `0xa7bd4` | none | none | 19a80@19aff,f2ac0@f2b49 | Byte 0x1371fc = 1, byte 0x1371ee = 0. |
| `0xa7df4` | none | none | a9d48@a9da1 | Copy 100 dwords from 0x418e38 to 0x418fc8. |
| `0xa7e20` | none | none | a9d48@a9dff | Copy 100 dwords from 0x418fc8 back to 0x418e38. |
| `0xa86e4` | none | none | a8718@a87b8 | When byte 0x41f833 is 0x39 and signed word 0x425fa6 is * below signed word 0x4194ae, copy the latter over it and clear t |
| `0xa944c` | none | none | a9e70@a9f80 | Decrement word 0x4194ac; dword 0x4191dc = table * 0x418e38[signed high word of dword 0x4194aa]. |
| `0xa9468` | AX:int16_t | EAX:int32_t | a94e4@a95cf | Random 16-bit value times signed SI, divided by 65536 * rounding toward zero (the SBB sequence). |
| `0xa98d0` | none | none | a99c0@a9a2b | Latch several words and dwords after 0xa7bb0. |
| `0xa9900` | none | none | a9e70@aa095,a9e70@aa0ee | Reverse of 0xa98d0 (without the 0xa7bb0 call). |
| `0xa9990` | none | EAX:uint32_t | a9e70@aa01a | Load the live words from a second saved set, then * tail-call 0xb3f50. |
| `0xa9bbc` | none | none | a9e70@a9fd2 | When byte 0x1371ff is zero clear byte 0x1371ee. |
| `0xa9bd0` | none | none | a9e70@a9fcb | Byte 0x137200 = 1 when 0x1371ff is set, else byte 0x1371f8 = 1. |
| `0xa9bec` | none | none | a9e70@a9f97 | Clear byte 0x1371ee, then call 0xf41dc. |
| `0xaa198` | EAX:ef_addr32 | none | aa350@aa39d | Follow the singly linked list from dword 0x4191fc as * many steps as the dword at +1 says (at least none) and store the  |
| `0xaa1c0` | EAX:ef_addr32 | none | aa350@aa3ac | As 0xaa198 for the dword at +0. |
| `0xaa4b8` | AX:int16_t,EDX:ef_addr32 | EAX:ef_addr32 | aa4d4@aa50b,aa4d4@aa524,aa4d4@aa53d | Step AX (signed word) times along the chain starting at * dword +8 of EDX (next pointer at +1); returns the node reached |
| `0xab0b0` | EAX:ef_addr32,EDX:ef_addr32 | none | ab0dc@ab12a,ab0dc@ab138,ab0dc@ab148 | Two 0xa7a1c(EAX, ...) stack calls with record fields. |
| `0xab260` | EAX:ef_addr32,EDX:ef_addr32 | none | ab28c@ab2da,ab28c@ab2e8,ab28c@ab2f8 | As 0xab0b0 through 0xa79d4. |
| `0xab8b0` | none | none | ac0e0@ac0fe | Dword 0x41ec14 = 0xc382c(). |
| `0xab8c4` | none | none | ab8e0@ab8ff,ac16c@ac170 | Dword 0x137270 = dword 0x13726c; 0xb3fb0(that, 0, dword 0x41ecd4). |
| `0xab8e0` | EAX:uint32_t | none | ac0e0@ac0f9 | Allocate EAX bytes via 0xe29e8; record base, cursor, * size and end; then clear the pool. |
| `0xabe24` | none | none | abe44@abe4b,abf8c@abfe4,ac408@ac491 | Clear 0x150 bytes at 0x41eab0 and word 0x41eeca. |
| `0xabf24` | EAX:uint32_t | none | abf8c@abfb4 | When non-null store EAX into eight dwords spaced 0x12 apart from 0x1621be. |
| `0xac980` | AX:uint16_t,DX:uint16_t,BX:uint16_t,CX:uint16_t | none | ac9a0@ac9ca,ac9e8@aca01 | Words 0x137950..56 = AX, DX, BX, CX; tail-call 0xac7f0. |
| `0xac9e8` | none | none | 34f94@350f1,4f2f8@4f30f,4f36c@4f383 | 0xac980(0, 0, high word 0x15b742, high word 0x15b746); dword 0x41ec18 = 0. |
| `0xaca88` | none | none | 4738@484c,4bf8@4c48,b1e0@b296 | Word 0x41eeda = 0. |
| `0xaca94` | none | none | bfcc@c4af,bfcc@c4d2,345e8@346bc | Word 0x41eeda = 1; when word 0x41eedc is zero, clear * word 0x41eede and run 0xaca14. |
| `0xb03e8` | none | none | b0f5c@b1078 | Clear three words 0x13799e, 0x1379a0 and 0x1379a2. |
| `0xb1814` | EAX:ef_addr32,EDX:ef_addr32 | none | b1b10@b1b1b | Free each node of the list at *head via 0xb17d4, then clear *head and *tail. |
| `0xb188c` | EAX:ef_addr32,EDX:ef_addr32 | none | b1b10@b1b66 | As 0xb1814 through 0xb1840. |
| `0xb1974` | EAX:ef_addr32,EDX:ef_addr32 | none | b0f5c@b13b4,b1b10@b1b2a | As 0xb1814 through 0xb1918. |
| `0xb19e4` | EAX:ef_addr32,EDX:ef_addr32 | none | b1b10@b1b48 | As 0xb1814 through 0xb19a0. |
| `0xb1ae4` | EAX:ef_addr32,EDX:ef_addr32 | none | b1b10@b1b57 | As 0xb1814 through 0xb1a68. |
| `0xb1a10` | EAX:ef_addr32 | none | 494c@4a46,494c@4a66,494c@4a83 | Release *EAX via 0xcc768 when set, clear it and dwords 0x1379a8/0x1379b0. |
| `0xb1a3c` | EAX:ef_addr32 | none | ba5c@bb72,bfcc@c4be,b1b10@b1b7a | As 0xb1a10 with dwords 0x1379ac and 0x1379b4. |
| `0xb2e64` | EAX:ef_addr32 | EAX:uint32_t | b2e98@b2ef5 | Count the 9-byte entries (pointer +0x4a, count word * +0x48) whose byte +3 is nonzero. |
| `0xb33ac` | none | EAX:uint32_t | b3374@b337d | Dword 0x41f354 = 0xa4574(0); tail-call 0xf9ecd(0x41f354). |
| `0xb4e48` | AX:int16_t,EDX:ef_addr32 | EAX:int32_t | b914c@b91c1 | First index >= AX with a zero byte +3 in the 9-byte * entry table of EDX, or -1. |
| `0xb549c` | AX:int16_t | EAX:int32_t | b54d0@b54e1,b56d0@b56dc,b5fe8@b64e8 | q = AX / 8 truncated; 60 when q <= 0 else 60 / q. |
| `0xb5580` | EAX:ef_addr32 | EAX:int32_t | b7d4c@b7d75,b7d4c@b7d83,b8510@b858d | Random 16-bit value modulo the signed high word of dword +0x1c. |
| `0xb43f4` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 74d98@74e03,89fbc@89fe2,89fbc@8a005 | Cross product EDX x EBX into EAX (three doubles each). |
| `0xb44d0` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 11c350@11c395,11c584@11c623 | out = a + b for three doubles (EDX + EBX into EAX). |
| `0xb44ec` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 8e940@8e9f6,e6990@e69a9 | out = a - b for three doubles (EDX - EBX into EAX). |

## list_lookup (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb55c8` | EAX:ef_addr32,EDX:ef_addr32,BX:uint16_t,CL:uint8_t | EAX:ef_addr32 | b7d4c@b7d6b,b80ac@b80cc,b8510@b857a | Walk the list at *head (next at +4) for a node whose * dwords +9/+0xd equal the record's +0x11/+0x15, word +0x1c equals  |
| `0xba3bc` | EAX:ef_addr32 | none | bb324@bb47d | Fill bytes 0..9 with 4. |
| `0xba3e4` | EAX:ef_addr32 | none | bb324@bb47d | Fill bytes 0..9 with 0x0d. |
| `0xbae34` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Bytes 0 and 1 = 0x12 when DL is 1, else 0x11. |
| `0xbae4c` | EAX:ef_addr32 | none | bb324@bb47d | Bytes 0..3 = 2, 2, 3, 3. |
| `0xbc34f` | EAX:ef_addr32 | none | bc36a@bc36d | When 0xfa6d3(0) returns nonzero, run 0xbc287(EAX). |
| `0xbc36a` | EAX:ef_addr32 | none | 9ea0@9fa4,a3dc@a3f7,357ec@3589f | Run 0xbc34f with EBX preserved as zero. |
| `0xbc465` | EAX:int32_t | EAX:int32_t | bc471@bc491,bc471@bc4ac,bc471@bc4b9 | Digit character for a value: +0x30, then +0x27 past '9'. |
| `0xbc570` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | f6a8@f810,f6a8@f86b,fecc@ff7f | Insert EAX at the head of the doubly linked list whose * head pointer is EBX and tail pointer EDX (previous at +0, next  |
| `0xbc5d0` | EAX:ef_addr32 | EAX:uint32_t | 49228@494f9,49228@49509,b0f5c@b1505 | Count nodes of the list at *EAX (next at +4). |
| `0xbc5e8` | EAX:ef_addr32,EDX:uint32_t | EAX:ef_addr32 | a3dc@a418,ab74@abc9,ab74@af46 | Node number EDX of the list at *EAX, or 0. |
| `0xbcb08` | EAX:ef_addr32 | EAX:uint32_t | bcc2c@bcc85,bcc2c@bccd4,bcc2c@bcd44 | Byte +5 at least 0x32 (signed) and byte +6 nonzero. |
| `0xbcb24` | EAX:ef_addr32 | EAX:uint32_t | cac4c@cacc2,faafc@fab29 | Byte +6 at least 0x55 and byte +0 not 0xff or 0xfe. |
| `0xbcc04` | AL:uint8_t | AL:uint8_t | bcc2c@bcd1b,bcda0@bce49 | Map code: 0x31 -> 0x37, 0x49 -> 0x4b, 0x0b/0x56 -> 0x0c, else 0. |
| `0xbcd78` | AL:uint8_t | AL:uint8_t | bcf44@bcfe7 | Map code: 0x31 -> 0x27, 0x49 -> 0x12, 0x0b/0x56 -> 0x1b, else 0. |
| `0xbce98` | AL:uint8_t | AL:uint8_t | bcf44@bd1a6 | Map code: 0x31 -> 2, 0x49 -> 0x3c, 0x0b/0x56 -> 0x0c, else 0. |
| `0xbcf18` | EAX:ef_addr32 | EAX:uint32_t | bcf44@bd0e4,bfb2c@bfb52,f90ac@f9252 | Byte +0xd is zero. |
| `0xbcf28` | EAX:ef_addr32,EDX:ef_addr32 | EAX:uint32_t | bcf44@bd0d8,be3f4@be413,f90ac@f9246 | Dwords +0x11/+0x15 equal the other record's +0x3b/+0x3f. |
| `0xbd4e0` | AL:uint8_t | EAX:uint32_t | f88a0@f88bb,f89e0@f8a08,f8a30@f8a4e | AL is 2, 3, 4, 9 or 0x0c. |
| `0xbd728` | EAX:ef_addr32,DL:uint8_t | EAX:uint32_t | bdbf8@bdc65,f8b14@f8bb0 | DL (as a signed word) is below the signed word +0x23. |
| `0xbdbc8` | EAX:ef_addr32 | EAX:uint32_t | bd5f8@bd618,be59c@be5af,be59c@be605 | Count the 10-byte entries (pointer +6, count byte +5) * whose first word is nonzero. |
| `0xbecf8` | EAX:ef_addr32 | EAX:uint32_t | bed1c@bed72 | 1 when 0xbc7ec(record) is zero and 0xbecc0(record) is nonzero. |
| `0xbef90` | AX:int16_t,DX:int16_t,EBX:ef_addr32 | EAX:ef_addr32 | befb8@bf1f2 | Find the node in the list at *EBX with dword +0x16 == * signed AX and dword +0x1a == signed DX. |
| `0xbfa28` | EAX:ef_addr32 | EAX:uint32_t | bf81c@bf8b6 | Byte +0x22 of the records at dwords +0x33 and +0x37 * differ and the first is nonzero. |
| `0xbfcd8` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | b0b00@b0b3d,b0b00@b0bcd,c0b04@c0f1d | Node in the list at *EDX with dwords +0x16/+0x1a equal * to the record's +0x11/+0x15. |
| `0xbfcfc` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | 4ce4c@4cf86,50770@507ae,b47e4@b4970 | Node in the list at *EAX with dwords +0x11/+0x15 equal * to the record's +0x16/+0x1a. |
| `0xc1b8c` | AX:uint16_t,EDX:ef_addr32 | EAX:ef_addr32 | 9a8b4@9a8c4,9a998@9a9a7,9b2a4@9b391 | Node in the list at *EDX whose word +0x13 equals AX. |
| `0xcb5f8` | AX:uint16_t,EDX:ef_addr32 | EAX:ef_addr32 | cb800@cb876,cb800@cb897 | Identical body to 0xc1b8c. |
| `0xc1ba8` | AX:uint16_t,EDX:ef_addr32 | EAX:ef_addr32 | 9b2a4@9b4ae | Node number AX of the list at *EDX (16-bit compare), or 0. |
| `0xc3834` | none | none | 283c8@283f1,f3f84@f4137 | 0xe9a24(dword 0x164600, dword 0x15b754, dword 0x146e40). |
| `0xc3850` | none | none | 3dd30@3e278 | 0xe9a24(dword 0x164600, dword 0x15b754, dword 0x146e44). |
| `0xc386c` | none | none | 278c0@27932,27fec@2800c,27fec@2815d | 0xe9a24(dword 0x164600, dword 0x146e40, dword 0x15b754). |
| `0xc3888` | none | none | 27d18@27d8c,283c8@283ec,284ec@2850b | 0xe9a24(dword 0x164600, dword 0x146e44, dword 0x15b754). |
| `0xc38a4` | none | none | eac48@eacb2 | Tail-call 0xb3fb0(dword 0x146e40, 0, dword 0x164600). |
| `0xc3974` | none | none | ab74@ae67,278c0@27900,27fec@281db | Dword 0x15b754 = dword 0x146e40. |
| `0xc3980` | none | none | 284ec@284f7,f53bc@f546c | Dword 0x15b754 = dword 0x146e44. |
| `0xc3990` | none | none | 278c0@27b21,27d18@27e9d,39204@39216 | Dword 0x15b754 = dword 0x15b758. |
| `0xc399c` | none | none | cdb20@cdc1e,d1d40@d1d90,dc250@dc277 | Dwords 0x15b754 and 0x146e4c = dword 0x15b758. |
| `0xc3a28` | none | none | c3f1c@c3f47 | When dword 0x146e40 is set, replace it with dword * 0x15b758 and store the old value at 0x15b758. |
| `0xc3a48` | none | none | c3ae8@c3c11 | When dword 0x15b758 is set and byte 0x146e4a clear, * release it through 0xc3f80 and clear it. |
| `0xc3f50` | EAX:uint32_t,EDX:uint32_t | AL:uint8_t | afbc@b0b2,afbc@b0c1,afbc@b0d4 | Byte at dword 0x15b754 + EAX + dword 0x163200[EDX]. |
| `0xc4389` | none | none | c5261@c5283 | Run 0xe3c20. |
| `0xca9fc` | EAX:int32_t,EDX:int32_t | EAX:int32_t | b0554@b0564,b8510@b8534,caa28@caace | EAX / 40 + 10 * (EDX / 40), signed. |

## code_predicates (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcbef4` | AL:uint8_t | AL:uint8_t | cabb0@cabd0 | AL if it is 1..4, else 0. |
| `0xcbf00` | none | none | b0f5c@b1684,b1b10@b1b84 | Release dword 0x1379c8 through 0xcc768 when set, then clear it. |
| `0xcc164` | AL:uint8_t,DL:uint8_t,EBX:ef_addr32 | none | c0b04@c0eac,c0b04@c0ee1,cb6fc@cb7c2 | In list EBX node number AL, set byte [dword +0xc + DL] * to 0x9c and byte [dword +0x10 + DL] to 0x7f. |
| `0xcc188` | AL:uint8_t,DL:uint8_t,EBX:ef_addr32 | none | c0b04@c0d0a,c0b04@c0d1d,c0b04@c0d52 | As 0xcc164 with the value 0x64. |
| `0xcc26c` | AL:uint8_t,EDX:ef_addr32 | EAX:ef_addr32 | b059c@b05c3,bed1c@bed3a | Node in the list at *EDX whose byte +0xb equals AL. |
| `0xcc5fc` | AL:uint8_t | EAX:uint32_t | d68c@df63,d68c@e0a8,d68c@e1b3 | AL is 0x10..0x15. |
| `0xcc628` | AL:uint8_t | EAX:uint32_t | d68c@df8c,d68c@e0d4,d68c@e1df | AL is 0x18, 0x19, 0x1a, 0x1b, 0x1d or 0x1e. |
| `0xcc654` | AL:uint8_t | EAX:uint32_t | d68c@dfb5,d68c@e100,d68c@e20a | AL is 0x18 or 0x1c..0x1f. |
| `0xcc67c` | AL:uint8_t | EAX:uint32_t | d68c@dfe0,d68c@e12c,d68c@e236 | AL is 0x20 or 0x21. |
| `0xcc694` | AL:uint8_t | EAX:uint32_t | d68c@e00b,d68c@e157,d68c@e25f | AL is 0x23 or 0x24. |
| `0xcc6ac` | AL:uint8_t | EAX:uint32_t | d68c@e035,d68c@e288,9a8b4@9a97d | AL is 0x22. |
| `0xcc7e8` | AL:uint8_t,DL:uint8_t | EAX:uint32_t | bdeec@be17c,cc90c@cca5f,ccdbc@cce56 | DL in {0x16, 0x17, 0x0e, 0x11, 0x13} and AL is 2. |
| `0xcc81c` | AL:uint8_t,DL:uint8_t | EAX:uint32_t | f918@f946,f918@f9a7,b31f0@b321a | DL is 8 or 0x1a and AL is 2. |
| `0xcd24c` | none | AX:uint16_t | 1df90@1e04d,56084@5614a,69374@69437 | Word at +2 of the structure at dword 0x15b628. |
| `0xcd258` | none | AX:uint16_t | 1e28c@1e33d,1e3c4@1e43b,5674c@56797 | Word table 0x41f700 indexed by the signed high word of * the cursor's dword +2. |
| `0xcd270` | none | EAX:int32_t | d0470@d0671 | Dword table 0x41f700 selects a signed high word; the * result is twice that plus dword 0x41f704. |
| `0xcd390` | none | EAX:uint32_t | 1e214@1e27a,d0470@d0768 | Advance dword 0x15b628 by twice its signed word at +0; * return whether the word at the new position is nonzero. |
| `0xcd6fc` | none | none | e4188@e422a | When byte 0x15b734 is set clear it and run 0x9c3cf(0xcd6cc); * dword 0x15b730 = 0. |
| `0xcdf48` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 11e7c@11f78,11e7c@12059,1e7c0@1e8e4 | Dwords 0x17cf9c..a8 = EAX, EDX, EBX, ECX. |
| `0xcdc38` | none | none | cdc64@cdc67,d1e50@d1ea8 | Run 0xc37ec, then release dword 0x15b758 as 0xc3a48 does. |
| `0xce824` | EAX:ef_addr32,EDX:uint32_t | none | ce830@ce8c6,ce830@ce8dc,ce830@ce8f8 | Append EDX to the counted array at EAX (count dword at +0). |
| `0xcff64` | EAX:ef_addr32 | EAX:int32_t | cff80@cffc4,cff80@cffce,cff80@d0057 | 0xb3830(signed word +0, high words of +2 and +6), as signed 16. |
| `0xd13dc` | EAX:ef_addr32 | EAX:uint32_t | 31b40@31ccb,31d7c@31e17,53c80@53e1f | 0xb3b1c(dword +8, dword +0x20). |
| `0xd13ac` | EAX:ef_addr32 | EAX:int32_t | 31b40@31cbe,31d7c@31e0a,53c80@53e0f | -0xb3b1c(-dword +0x14, low word of 0xe2b60(dword+8 squared + dword+0x20 squared)). |
| `0xd13ec` | EAX:ef_addr32 | EAX:int32_t | 31b40@31c78,31b40@31cfc,53c80@53e2f | -0xb3b1c(dword +0xc, (dword+0 * dword+0x20 - dword+8 * dword+0x18) >> 15). |
| `0xd17bc` | none | none | d1c54@d1c92 | 0xa7990(dword 0x420944), callee pops 4. |
| `0xd1f5c` | none | none | none direct | Copy dwords 0x15b88c..98 to 0x15b87c..88. |
| `0xd2110` | EAX:ef_addr32 | none | d23d8@d2437,d299c@d2a3f | 0xfc8cc(dword [p], p+4) for p = dword +0x4c; set bit 0 of byte +0x2c. |
| `0xd5fa8` | EAX:uint32_t,EDX:uint32_t | none | 98198@981d0 | When byte 0x15b968 is set call cdecl 0x103778(dword 0x420a64, EAX, EDX). |
| `0xd6410` | EAX:ef_addr32 | EAX:uint32_t | d6478@d64e0,d6478@d6532 | Sum of the first three bytes. |
| `0xd6d5c` | EAX:ef_addr32 | AL:uint8_t | 1c780@1cb3a,3d37c@3d43b | When bit 1 of byte 0x15bac0 is set store dword 0x420da0 at EAX; return whether it was. |
| `0xd6f90` | EAX:ef_addr32 | none | 80058@800fa,80374@803bb,81230@812b3 | For each pointer in the array at EAX whose string is nonempty run 0xd6fa8. |
| `0xd7594` | AL:uint8_t | AL:uint8_t | 77c90@77dbe,801e4@80248,80314@8032e | With byte 0x15bac4 set, 0x1028f4(dword 0x420dd0[AL]) equals 2. |
| `0xd75fc` | AL:uint8_t | AL:uint8_t | 47fbc@47fc3,77c90@77ddb,81308@81318 | As 0xd7594, comparing with 4. |
| `0xd7724` | EAX:ef_addr32,DX:uint16_t | EAX:int32_t | 7ef14@7f33d | Word +0x11 += DX; return the signed high word of dword +0xf. |

## math_string (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdc2c0` | none | none | 1e480@1e51c,1e480@1e58f,1e480@1e636 | 0xb3fb0(dword 0x15b754, 0, dword 0x164600). |
| `0xdc314` | none | none | 3dd30@3e044,3dd30@3e054,63d5c@64057 | Unless both dwords 0x15bdd4 and 0x15bdd8 equal 0x100, * run 0xdc3d5 and 0xce354. |
| `0xdc860` | EAX:ef_addr32,EDX:ef_addr32 | none | dc8d8@dc906 | Halve the dwords at EAX and EDX until both fit in a signed 16-bit range. |
| `0xddbc8` | EAX:int32_t | EAX:int32_t | 27fec@28121,a82f0@a8374,a9130@a9355 | Signed 'a'..'z' to uppercase. |
| `0xe7835` | EAX:int32_t | EAX:int32_t | 50d9c@50db5,a80b0@a80fe,bc1d3@bc1e1 | Signed 'A'..'Z' to lowercase. |
| `0xdec00` | AL:uint8_t,EDX:uint32_t,EBX:uint32_t | none | none direct | Byte at dword 0x15b754 + EDX + dword 0x163200[EBX] = AL. |
| `0xdec18` | AL:uint8_t,EDX:int32_t,EBX:int32_t | none | 39a0c@39a69,39a0c@39b47,3a3a8@3b84b | As 0xdec00 but only inside the signed clip rectangle * 0x15bdc0..c4 (x) and 0x15bdc8..cc (y). |
| `0xdec90` | AX:int16_t,DX:int16_t,BX:int16_t | EAX:uint32_t | 9e0c4@9e195,a0aa0@a0b49,a2500@a2580 | 0xe2b60 of the sum of squares of three signed words. |
| `0xdecb0` | EAX:int32_t,EDX:int32_t | EAX:uint32_t | 2f740@2f786,dc91c@dc99b,dcaa4@dcb36 | Low word of 0xe2b60(EAX^2 + EDX^2). |
| `0xdecc4` | EAX:int32_t,EDX:int32_t,EBX:int32_t | EAX:uint32_t | 30214@311ed,31514@31588,83a40@83adb | Low word of 0xe2b60(EAX^2 + EDX^2 + EBX^2). |
| `0xe2b60` | EAX:uint32_t | EAX:uint32_t | 39cb4@39d4b,91c00@91c17,d13ac@d13c1 | Bitwise unsigned integer square root. |
| `0xe2fdb` | none | EAX:uint32_t | 3ce04@3ce07 | Linear congruential generator on the seed at the * pointer returned by 0xe2fd5: seed = seed*0x41c64e6d + 0x3039, result  |
| `0xdf2e4` | none | none | 3361c@33626 | Byte 0x15f07d = (byte + 1) & 7. |
| `0xe10e0` | none | none | e0b88@e0b8b,e1194@e11a8 | Byte 0x15f685 = 0, fill 0x80 bytes at 0x15f5fd with 0xff, * byte 0x15f684 = 0, byte 0x15f685 = 1. |
| `0xe1540` | AL:uint8_t | AL:uint8_t | 337c4@337d0 | Next index after AL (mod 5) whose byte at 0x15f708+index is nonzero. |
| `0xe17a0` | none | none | abf8c@abfcd | Clear 0x5000 bytes at 0x4198d0. |
| `0xe192c` | AL:uint8_t | AL:uint8_t | a63c0@a63f0 | Byte at 0x1378d8 + 10*AL. |
| `0xe2988` | EAX:int32_t | none | 357ec@35aa8,56bdc@56c5f,57030@570bc | Store EAX in dword 0x15f748 and, if positive, wait for * an interrupt handler to bring it to zero or below. |
| `0xe2a1c` | none | none | e29a0@e29e1,e4264@e42a4 | Clear dwords 0x15f764/68, dword 0x15f758 = dword * 0x15f760, dword 0x15f754 = dword 0x15f75c, then clear that buffer. |
| `0xe4304` | none | AL:uint8_t | ac0e0@ac0e1 | Byte 0x15f930 equals 1. |
| `0xe599c` | EAX:ef_addr32 | none | a5ef8@a5fc6,e5870@e5873 | Copy words +0x41..+0x47 to 0x41ec08..0e. |
| `0xe5870` | EAX:ef_addr32 | none | a5ef8@a5f6e | 0xe599c(record), then 0xe52a4(byte +0x10, 1). |
| `0xe588c` | EAX:ef_addr32 | none | a60a8@a60bd | As 0xe5870 through 0xe5960. |
| `0xe6800` | EAX:ef_addr32,DX:int16_t,BX:int16_t,CX:int16_t | EAX:ef_addr32 | e6818@e6834 | 0xe67a0(EAX, signed DX, signed BX+DX, signed CX); returns EAX. |
| `0xe6818` | EAX:ef_addr32 | none | 49228@493ab,b0f5c@b1034,b0f5c@b1234 | For each node of the list at *EAX call 0xe6800 on its * dword +0x2c with (0, word +0x2a minus 1, 1). |
| `0xe7fa4` | EAX:uint32_t | none | 7a38c@7a397 | Dword 0x464360 = EAX, dword 0x15b268 = 0xe7f60. |
| `0xe8348` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | e8368@e83c1 | 0xe81b0 on (A,B), (B,C), (C,A) for EAX, EDX, EBX. |
| `0xe85e4` | AL:uint8_t,DL:uint8_t | none | 7b3fc@7b499,7b3fc@7b4b8 | Byte 0x46437e = AL, byte 0x46437d = DL. |
| `0xe8733` | EAX:uint32_t,EDX:uint32_t | EAX:ef_addr32 | 106f10@106f64,117ed4@117ee9,117f14@117f23 | Allocate EAX*EDX bytes through 0xfc795 and clear them. |
| `0xe874b` | EAX:ef_addr32,EDX:uint32_t | EAX:ef_addr32 | aaee8@aaf06,e7fc0@e7fec,e7fc0@e803a | strchr: first occurrence of byte EDX in the string at EAX * (including the terminator), or 0. |
| `0xea130` | EAX:ef_addr32 | EAX:uint32_t | ea270@ea2d5 | Remove carriage returns in place; returns the number removed. |
| `0xea154` | EAX:ef_addr32 | EAX:ef_addr32 | ea270@ea2fb | Advance to the next newline, replace it with NUL and * return the pointer after it (or the end of string). |
| `0xea1e8` | EAX:ef_addr32,EDX:ef_addr32 | EAX:int32_t | none direct | Signed compare of the dwords at EAX and EDX: -1, 0 or 1. |
| `0xea240` | EAX:ef_addr32 | none | ea270@ea281 | If dword +0xc is set, release dwords +0x14 and +0x10 * through 0xc3f80 and clear +0xc, +0x10 and +0x14. |
| `0xea84c` | EAX:ef_addr32 | none | 9fea8@9ffb4,9fea8@9ffbc | Clear three dwords. |
| `0xea870` | EAX:ef_addr32,EDX:ef_addr32 | none | 97914@97929,97950@97968,97950@9798c | Copy three dwords from EDX to EAX. |
| `0xea958` | EAX:ef_addr32,EDX:ef_addr32 | none | 97914@97944,97950@97983,97950@979a0 | Add three dwords of EDX to those at EAX. |
| `0xea9a0` | EAX:ef_addr32,DX:int16_t,BX:int16_t,CX:int16_t | none | 9ffc8@9fffe | Dwords at EAX = sign-extended DX, BX, CX. |
| `0xea9b8` | EAX:ef_addr32,EDX:ef_addr32 | EAX:int32_t | 97534@975a2 | Fixed-point (16.16) dot product of two dword triples. |
| `0xeb478` | EAX:ef_addr32 | EAX:ef_addr32 | 9c980@9c9b6,9c9c0@9ca0d,a7ec4@a7ec9 | Lowercase ASCII 'A'..'Z' in place; returns the pointer. |
| `0xeb4c0` | EAX:ef_addr32 | none | ebcd8@ebceb | Release the pointer stored at EAX through 0xc3f80 and clear it. |

## misc_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xeba4c` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 9cb70@9cb8a,9cb70@9cbaa | Zero when EDX is zero, else dword +8 of 0xeb520's result (or 0). |
| `0xebcd8` | EAX:uint32_t,EDX:ef_addr32 | none | 9ce84@9cea5,9ce84@9ceb6 | Release the object's pointer at +0x154 for flag 0x80000, else defer to 0xa7990. |
| `0xebcfc` | EAX:uint32_t,EDX:uint32_t,EBX:ef_addr32 | EAX:uint32_t | 9cee8@9cf0a,9cee8@9cf1a | Advance the cursor at +0x158 by EAX and return the distance from +0x154. |
| `0xebe6d` | EAX:uint32_t | none | bc1d3@bc1fa,ebe79@ebe7e,f1d90@f1da5 | Store EAX through the pointer returned by 0xebd30. |
| `0xebe90` | EAX:uint32_t | none | 1101e8@1101f2 | As 0xebe6d with 0xebd36. |
| `0xec0ec` | AX:uint16_t,DX:uint16_t | AL:uint8_t | a3108@a311e | Compare 6 bytes at dword 0x1368bc[AX]+0x15 with those at dword 0x135874[DX*32]+0x16. |
| `0xece90` | none | none | a29f8@a2a3a | Clear 0x10 bytes at 0x13577c and byte 0x484665. |
| `0xed115` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | EAX:int32_t | a2f70@a2fed,d660c@d662c,ec0ec@ec113 | memcmp of EBX bytes at EAX and EDX: 0, -1 or 1 (unsigned). |
| `0xedb15` | EAX:ef_addr32 | none | ed6cc@ed99a,ed6cc@eda19 | Uppercase the string at EAX in place. |
| `0xf17d8` | EAX:ef_addr32 | none | f18a4@f18fa,f18a4@f19a7,f1a70@f1b6f | Call 0xa4cb4 on up to five words starting at +0x59. |
| `0xf180c` | EAX:ef_addr32 | none | f18a4@f18e2,f18a4@f197f,f1a70@f1b4d | As 0xf17d8 with words at +0x63 and 0xa4b80. |
| `0xf227f` | EAX:ef_addr32 | EAX:int32_t | a7a64@a7aa9,bc4da@bc507 | Position from 0x10f4df(dword +0x10) combined with dword +4. |
| `0xf23ac` | AL:uint8_t,EDX:int32_t,EBX:int32_t,ECX:ef_addr32 | none | a8578@a85b5,a9130@a938a,a9130@a93bd | For each byte of the string at ECX call 0xf2320(x, y, byte & 0x3f), x += 8. |
| `0xf2800` | none | EAX:uint32_t | a8bd4@a8bf5 | 0xf2819(), then return byte 0x161156. |
| `0xf2d54` | none | AL:uint8_t | f2d74@f2d99 | The string at 0x48592c ends with ':'. |
| `0xf2dcc` | EAX:ef_addr32 | none | f2dfc@f2e15 | Truncate the string at the last '.'. |
| `0xf2e54` | none | none | f3b78@f3bb3 | 0xf2cd0(), 0x9d749(0x485908, 0x48592c), byte 0x4859de = 1. |
| `0xf3480` | none | none | f34cc@f35a8 |  |
| `0xf34a0` | none | none | f3f84@f40b1 |  |
| `0xf35d8` | none | none | f3b78@f3c2b | Word 0x161e1c = 0x9d4cd(0x48592c) after 0xf2cd0(). |
| `0xf35f0` | none | none | f36a4@f36f2 |  |
| `0xf3b1c` | none | none | f3b78@f3cd5 |  |
| `0xf3b40` | none | none | f3b78@f3cf6 | As 0xf35d8, storing word 0x161e1e. |
| `0xf3b58` | none | none | f3b78@f3d43 |  |
| `0xf4178` | none | none | f41dc@f42af |  |
| `0xf9ed2` | EAX:uint32_t | none | 101c54@101ce6 |  |
| `0xf9d98` | EAX:int32_t,EDX:ef_addr32,EBX:int32_t | none | f9dcb@f9e23,f9dcb@f9e40,f9dcb@f9e53 | Store the two decimal digits of EAX at EBX+EDX. |
| `0xfa7d2` | EAX:ef_addr32 | none | bc287@bc345,f22af@f22e5,10f071@10f075 |  |
| `0xfba54` | EAX:uint32_t,EDX:ef_addr32 | none | fbad8@fbb2b,fbad8@fbc83 | 0xfba6c(dword +0x16, dword +0x1a) with EBX = EAX. |
| `0xfddd2` | EAX:uint32_t | EAX:uint32_t | c4310@c433a | Map 0xfddb3 to 0xfddb7 and 0xfddc5 to 0xfddcc, else keep. |
| `0xfef26` | none | none | d1d40@d1d75,d1e50@d1eb2 | Fill 192 dwords at 0x17cfc0 with -1. |
| `0xfc9f8` | EAX:int32_t | none | c4a50@c4acd,c4aeb@c4b68,c4b95@c4c12 |  |
| `0x101f44` | EAX:uint32_t | none | 104464@10449b,105e38@106237,105e38@1062cb | Trace wrapper for 0x1111ca. |
| `0x101ff4` | EAX:uint32_t | none | 105da0@105dd7,112808@11287d,1133f8@113a06 | Trace wrapper for 0x1112ed. |
| `0x1022dc` | EAX:uint32_t | none | e28e8@e297c,104200@104453,105e38@106424 | Trace wrapper for 0x11141f. |
| `0x102330` | EAX:uint32_t | none | 1114a3@1114b1 | Trace wrapper for 0x111473. |
| `0x102384` | EAX:uint32_t | none | 104464@104484,105da0@105de3,1113fb@111409 | Trace wrapper for 0x1113d4. |
| `0x102488` | EAX:uint32_t | none | 104694@1046a5,106464@1064ca,113a40@113aa6 | Trace wrapper for 0x104464. |
| `0x102644` | EAX:uint32_t | none | 1115fc@11161e | Trace wrapper for 0x106634. |
| `0x102670` | EAX:uint32_t | none | d6e58@d6f6b,d72b4@d7323,1065c8@106620 | Trace wrapper for 0x106644. |
| `0x102774` | EAX:uint32_t | none | d7158@d72a5,d72b4@d7398 | Trace wrapper for 0x10683c. |
| `0x1027a0` | EAX:uint32_t | none | d7490@d74c1 | Trace wrapper for 0x10687c. |
| `0x1027cc` | EAX:uint32_t | none | d7530@d7561 | Trace wrapper for 0x106894. |
| `0x1027f8` | EAX:uint32_t | none | d74d0@d7501 | Trace wrapper for 0x1068b8. |
| `0x10311c` | EAX:uint32_t | none | d5e0c@d5ea5 | Trace wrapper for 0x11402c. |
| `0x103148` | EAX:uint32_t | none | 112c10@112e38,112c10@11308a,113cf4@113cfe | Trace wrapper for 0x114058. |
| `0x1031a0` | EAX:uint32_t | none | d5ec0@d5ed6,113398@1133b1 | Trace wrapper for 0x114170. |
| `0x101ed4` | EAX:uint32_t,EDX:uint32_t | none | 104200@104393,104200@104402,104464@1044be | Trace wrapper for 0x111142. |
| `0x1021f4` | EAX:uint32_t,EDX:uint32_t | none | 110fd4@1110ab,1114f7@111515,111527@111557 | Trace wrapper for 0x1114c7. |
| `0x102228` | EAX:uint32_t,EDX:uint32_t | none | e28e8@e2962,104200@104447,105e38@106418 | Trace wrapper for 0x1114f7. |
| `0x10225c` | EAX:uint32_t,EDX:uint32_t | none | none direct | Trace wrapper for 0x111527. |
| `0x102824` | EAX:uint32_t,EDX:uint32_t | none | d7158@d721e,d7158@d725f,d72b4@d734c | Trace wrapper for 0x10677c. |
| `0x102858` | EAX:uint32_t,EDX:uint32_t | none | d7158@d7248,d72b4@d735e,d73b8@d73e4 | Trace wrapper for 0x10679c. |
| `0x10288c` | EAX:uint32_t,EDX:uint32_t | none | none direct | Trace wrapper for 0x1067cc. |
| `0x1028c0` | EAX:uint32_t,EDX:uint32_t | none | d7158@d7273 | Trace wrapper for 0x10682c. |
| `0x102a98` | EAX:uint32_t,EDX:uint32_t | none | d6e58@d6f32 | Trace wrapper for 0x106988. |
