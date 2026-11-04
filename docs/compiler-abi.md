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

## runtime_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x103244` | EAX:uint32_t,EDX:uint32_t | none | d5e0c@d5e96 | Trace wrapper for 0x1141a0. |
| `0x1033c8` | EAX:uint32_t,EDX:uint32_t | none | d5fc8@d5fe5 | Trace wrapper for 0x1142cc. |
| `0x103504` | EAX:uint32_t | none | d5d18@d5d31 | Trace wrapper for 0x113c24. |
| `0x1037e0` | EAX:uint32_t,EDX:uint32_t | none | 1123a4@1126c1 | Trace wrapper for 0x11458c. |
| `0x103b5c` | EAX:uint32_t,EDX:uint32_t | none | 1123a4@112761,114058@1140ea | Trace wrapper for 0x114878. |
| `0x103b90` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | 1123a4@112725,1123a4@11276c | Trace wrapper for 0x114994. |
| `0x103d48` | none | none | none direct | If dword 0x17d2cc is set, run 0x1048b4(0x101a90, 0x103d48) and clear it. |
| `0x1041e4` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 101e14@101e40 | Exchange dword 0x4dd7cc[index] with value; return the old one. |
| `0x104694` | none | none | 101ddc@101df9 | Call 0x102488 on each non-null dword 0x4dd670[0..15], then 0x1023b0 and 0x1110c5. |
| `0x1047a4` | EAX:uint32_t | EAX:uint32_t | 104200@104209,105e38@105e48,105e38@106209 | Allocate with 0xc3f70(size) and, if successful, run 0x104908(block, size). |
| `0x1047c4` | EAX:uint32_t,EDX:uint32_t | none | 104200@104267,104200@1042e4,104200@10434c | For a non-null block run 0x104920(block, size) then free it with 0xc3f80. |
| `0x104908` | EAX:uint32_t,EDX:uint32_t | none | d5e0c@d5e5e,d6df0@d6e03,d6df0@d6e15 | 0x104840(base, base + length). |
| `0x104920` | EAX:uint32_t,EDX:uint32_t | none | d5ec0@d5ee7,d6ffc@d701d,d710c@d7123 | 0x1048b4(base, base + length). |
| `0x105334` | EAX:ef_addr32 | none | 105648@105803,10585c@1058aa,105da0@105dae | If dword +0x54 is set, send message 0x402 through 0x101f88 and clear it. |
| `0x106634` | EAX:ef_addr32 | none | 102644@102667 | Dword +4 = 1. |
| `0x106708` | EAX:ef_addr32 | EAX:uint32_t | 1028f4@102919 | Dword +4, or 0 for null. |
| `0x106718` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t | none | 1026fc@10272c | Reset +0xc and +0x14, set +8 and +0x10. |
| `0x106740` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t | none | 102738@102768 | Update +0x34 / +0x38 and notify through 0x10535c when they change. |
| `0x10677c` | EAX:ef_addr32,EDX:uint32_t | none | 102824@10284e | Dword +0x3c = value. |
| `0x10679c` | EAX:ef_addr32,EDX:uint32_t | none | 102858@102882 | Update +0x40 and notify when it changes. |
| `0x1067cc` | EAX:ef_addr32,EDX:uint32_t | none | d710c@d712e,10288c@1028b6 | As 0x10679c for +0x44. |
| `0x10682c` | EAX:ef_addr32,EDX:uint32_t | none | 1028c0@1028ea | Dword +0x30 = value. |
| `0x10683c` | EAX:ef_addr32 | none | 102774@102797 |  |
| `0x10687c` | EAX:ef_addr32 | none | 1027a0@1027c3 | State 4 becomes 8. |
| `0x106894` | EAX:ef_addr32 | none | 1027cc@1027ef | State 8 becomes 4 and 0x1052f0 runs. |
| `0x106938` | EAX:ef_addr32,EDX:uint32_t | EAX:uint32_t | 102dbc@102de8 | Exchange dword +0x850; null object returns 0. |
| `0x106988` | EAX:ef_addr32,EDX:uint32_t | none | 102a98@102ac2 | Store dword +0x68 and notify each of the +0x60 entries (0x894 bytes each) whose state is not 1. |
| `0x106be8` | EAX:int32_t | EAX:int32_t | d9c30@d9c45,d9c30@d9c7e,d9cb4@d9cd5 | value * 100. |
| `0x106bfc` | EAX:int32_t | EAX:int32_t | d9cb4@d9cf4,d9dc0@d9de6,106d34@106d9c | value * 100 / 10. |
| `0x106c3c` | EAX:int32_t,EDX:int32_t | AL:uint8_t | 106cb0@106cec,106d34@106da5,106d34@106db2 | 0xf8248() - EAX >= EDX (signed). |
| `0x10703c` | EAX:ef_addr32 | none | d9c30@d9c69,d9c30@d9c9d,106f10@106f22 | Release the object stored at *slot. |
| `0x107060` | EAX:ef_addr32 | none | d9cb4@d9d1a,d9dc0@d9e01 | 0x118428(*EAX, 0x53). |
| `0x1089d4` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | 108a17@108a3e | Build a 256-bit set at EDX from the bytes of the string at EAX up to ']'. |
| `0x108fd3` | EAX:int32_t | EAX:int32_t | 108ddb@108ef6,108ddb@108f32 | Hex digit value of a character, or 0x10. |
| `0x1091e0` | EAX:ef_addr32 | EAX:ef_addr32 | 1093d8@1093ea | Remove carriage returns in place; returns the end pointer. |
| `0x1091fc` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | 1093d8@109405 | Copy one line from EDX to EAX, consuming the newline; returns the source after it. |
| `0x109284` | EAX:ef_addr32 | none | e3e50@e3e88,1092b0@1092bf,1092b0@109314 | Advance *pointer over characters whose class-table entry (0x195af0[c+1]) has bit 2. |
| `0x10d1b5` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | EAX:ef_addr32 | e7fc0@e8009,104504@104542,1133f8@113507 | strncpy with NUL padding; returns the destination. |
| `0x10d1da` | EAX:ef_addr32,DL:uint8_t | EAX:ef_addr32 | e7fc0@e8028,e7fc0@e8075,eb520@eb60a | strrchr: last occurrence of the byte in the string including the terminator, or 0. |
| `0x10d427` | EAX:ef_addr32 | none | 10d466@10d48f,10d4c4@10d4fc | Unlink a node from the doubly linked list with heads at dwords 0x164650 and 0x16464c. |
| `0x10ddfc` | EAX:uint32_t | EAX:int32_t | 10f1bf@10f1e2 | Count list entries (list at dword 0x485f9f) whose flags in +0xc match the mask; flush those with bit 0x10 in +0xd via 0x |
| `0x10dfe0` | none | EAX:uint32_t | eab44@eab47,eac48@eadfa | Dword +0x46c of the object pointed to by dword 0x120cc6. |

## runtime_libc (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10e438` | none | none | ed6c5@ed6c5,108afd@108d4e | 0x11d258(0x195168, 1). |
| `0x10e46f` | EAX:uint32_t,EDX:ef_addr32,EBX:uint32_t | EAX:ef_addr32 | ed6cc@ed989,10e4b8@10e4ca | Convert EAX to text in radix EBX at EDX using the digit table at 0x10e44a; * returns the destination start. |
| `0x10e4b8` | EAX:int32_t,EDX:ef_addr32,EBX:uint32_t | EAX:ef_addr32 | ed6cc@ed855,ed6cc@ed88f | Signed conversion: radix 10 negatives get a leading '-'. |
| `0x10e837` | EAX:int32_t | EAX:uint32_t | f118d@f11e6,10e872@10e88d,10e97f@10e9d6 | Gregorian leap year. |
| `0x10e957` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | 10e97f@10e9a8 | 0x10e872(a, c) > 0x10e872(b, c). |
| `0x10ec32` | EAX:ef_addr32,EDX:ef_addr32 | EAX:uint32_t | 10e97f@10eb13,10e97f@10ec0e | Three-dword signed lexicographic less-than (most significant dword at +8). |
| `0x10edb4` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | 10edda@10ee89,10edda@10ee9e,10edda@10eeb0 | Parse decimal digits at EAX into *EDX; returns the pointer after them. |
| `0x10f190` | EAX:ef_addr32 | EAX:int32_t | 10f107@10f13a,10f107@10f165 | Read one byte from a counted buffer stream; -1 at the end. |
| `0x10f4df` | EAX:uint32_t | EAX:uint32_t | f2183@f2227,f227f@f2287 | 0xfa95f(EAX, 0, 1). |
| `0x1100b1` | none | EAX:uint32_t | none direct | Elapsed value of 0x11004b relative to dword 0x4dd984, wrapping at 0x83d600. |
| `0x110123` | EAX:uint32_t | EAX:uint32_t | f9ecd@f9ecd,101c54@101ce1 | 0x1100d2(EAX, 0x4dd988). |
| `0x1102a3` | EAX:uint32_t,EDX:uint32_t | none | fa4a9@fa65e,fa9a0@fa9b6 | Store value / 0x4000 in dword table 0x17d614[index]. |
| `0x11024e` | EAX:int32_t | EAX:uint32_t | fa4a9@fa5fa,10f26a@10f276,110393@1103a1 | Look up dword table 0x17d614[index], marking the first six slots as seen. |
| `0x110dcc` | none | none | 101ba8@101bb1 |  |
| `0x110dd9` | none | none | 101ba8@101bd1 |  |
| `0x110f3c` | EAX:uint32_t | EAX:uint32_t | 110f6f@110fb1 | 0x110f0c(value < 0xd68d ? value * 10000 / 0x20bc : 0). |
| `0x111359` | none | EAX:uint32_t | none direct |  |
| `0x111569` | none | EAX:uint32_t | none direct |  |
| `0x1113ac` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 102198@1021c4 | Exchange dword 0x17d764[index] under the lock 0x101e70/0x101e78. |
| `0x1113fb` | none | none | 1023b0@1023d1 | Call trace_call_102384 for the slots 0x38 down to 0. |
| `0x11144f` | none | none | none direct | As 0x1113fb for 0x1022dc. |
| `0x1114a3` | none | none | none direct | As 0x1113fb for 0x102330. |
| `0x1114f7` | EAX:uint32_t,EDX:uint32_t | none | 101c54@101d8c,102228@102252 | 0x1021f4(a, 1000000 / b) under the lock. |
| `0x111527` | EAX:uint32_t,EDX:uint32_t | none | 10225c@102286 | 0x1021f4(a, b == 0 ? 0xd68d : 10000 * b / 0x2e9c) under the lock. |

## game_state_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb04` | none | none | e20@e2a |  |
| `0xd08` | EAX:int32_t | EAX:ef_addr32 | d68@e00 | Select a record address by kind. |
| `0x1ef8` | none | none | 1cd8@1ec6 | Every second call pick a byte from tables 0x1961f8 (stride 51) or 0x196191 (stride 52) * and store 0xf60 of it in byte 0 |
| `0x3cf0` | none | none | 1b70@1c5d,1cd8@1eea,3ea8@3f64 | Reset the selection state from the record at dword 0x197061. |
| `0x4bf8` | none | none | none direct |  |
| `0x5f60` | none | none | 3dd30@3de06 | Initialisation sequence. |
| `0x6acc` | EAX:ef_addr32 | AL:uint8_t | 6924@6996,625d8@62b2a,65c1c@65c7d | Category of an object from the byte at +0xe9: 1 (<=0xf), 2 (0x10..0x15), 3 (0x20..0x21), 4 (0x22..0x24). |
| `0x6e18` | none | none | 49228@495bc |  |
| `0x90c0` | AX:int16_t | none | 2d488@2d645 |  |
| `0xe774` | none | AL:uint8_t | e810@e81e,e810@e835,e810@e866 | Read one character from the 0x100-byte buffer at 0x197b0f; 0x1a becomes 0. |
| `0xee0c` | EAX:ef_addr32 | EAX:uint32_t | f260@f2f6 | Read a record through the 0xe95c byte/word readers. |
| `0xeefc` | EAX:ef_addr32 | EAX:uint32_t | f314@f540 |  |
| `0xefe8` | EAX:ef_addr32 | EAX:uint32_t | f038@f078 |  |
| `0xf13c` | EAX:ef_addr32 | none | f198@f247 |  |
| `0x117d4` | none | AL:uint8_t | 7bc40@7bd94 | Is there an entry (0x2c bytes each, count word +4, array dword +0xa) with byte 0 clear * and byte +0x2b not 100? |
| `0x12ec8` | none | none | 12160@12188 | Mirror byte 0x123790 with the flag at 0x121fb4 + 29*index. |
| `0x169b4` | EAX:int32_t | EAX:uint32_t | 15af0@15b33,16034@16055 |  |
| `0x16ab4` | EAX:uint32_t | EAX:uint32_t | 15b7c@15c5d | Active record whose kind byte +0x1c is one of a fixed set. |
| `0x17480` | EAX:uint32_t,EDX:int32_t | EAX:uint32_t | 15b7c@15c87 |  |
| `0x1cd90` | none | none | 1cde8@1cfd0 | Adjust word 0x12b366 by the key flags and clamp it to 0x5a..0x7f. |
| `0x1d3c4` | AX:uint16_t | none | 8b39c@8b519 |  |

## game_state_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1d700` | AX:int16_t | none | 5437c@543d0 | Dispatch on the kind byte +0x41 of the 0x171-byte record number AX. |
| `0x1f878` | EAX:uint32_t | none | 14c5c@14d33,1a3cc@1a448,1a3cc@1a466 | Push an entry onto the ten-entry stack at 0x19b0d0 and request an event. |
| `0x27858` | none | AL:uint8_t | 278c0@27adb | Show two panels and wait for key 0x15 or 0x31; true for 0x15. |
| `0x28830` | none | none | 3cfc0@3d1bb | Reset the ten 14-byte entries at 0x19b8d0 and chain them. |
| `0x2a168` | EAX:ef_addr32 | none | 2541c@25450 |  |
| `0x2a720` | none | none | 3cfc0@3d1c5 | Reset counters and link the twenty 22-byte entries at 0x19b980. |
| `0x2a7ac` | EAX:ef_addr32,EDX:uint32_t | none | db808@db882 | As 0x2a720 but for string EAX into the word at +8+2*EDX. |
| `0x316b8` | none | none | 2fbc4@2ff14,3122c@3129c | Run 0x32414 and add the three deltas at 0x19e560 to the position at 0x19e5c1. |
| `0x31aec` | none | none | 3122c@312e7 | Identical to 0x316b8. |
| `0x31d10` | none | none | 3122c@3128e,3122c@312fd | Update the angle words from mode byte 0x19e55f. |
| `0x32260` | none | none | 3122c@3131d | Save the current position and angles. |
| `0x34390` | none | none | none direct |  |
| `0x343dc` | none | AL:uint8_t | 34bcc@34be4 | Check both data files; show an error dialog and return 0 on failure. |
| `0x3458c` | none | none | 345e8@34695,345e8@346fc,34bcc@34c43 | Compact the four-entry dword table at 0x13577c: an empty entry shifts the later ones down. |
| `0x34f30` | none | none | 34f94@34ff6 |  |
| `0x35144` | none | none | none direct |  |
| `0x35ab8` | none | none | none direct |  |
| `0x35bb8` | none | none | 35cfc@35d1e |  |
| `0x363c4` | none | none | 36438@364f6,36438@3652a,36438@365e9 | Walk the list from dword 0x19f320 (next at +0x21), toggling each node by the flag bytes at 0x19f318. |
| `0x37d3c` | AX:int16_t | AL:uint8_t | 37eac@37f7b | True when 0x37cdc, or 0x37c64 for any subitem, accepts the index. |
| `0x37e60` | AX:int16_t | AL:uint8_t | 37eac@37f52 | True when 0x37e00 and 0x37d88 for every subitem accept the index. |
| `0x39204` | none | none | 39254@392f4 |  |
| `0x394e0` | none | none | 3dcb0@3dcb0 | Consume the pending request at 0x1a3608 and apply it through 0x1d5cc. |
| `0x39dc4` | none | none | 49228@495b7 | Reset the ten-entry table at dword 0x1a1860. |

## game_state_helpers3 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3cf08` | none | none | 3dd30@3e36c | Fire due timed events from the 11-byte table at 0x1a57d0. |
| `0x3db9c` | none | none | 3dd30@3dfa9 | Fire due events from the 9-byte table at 0x1a5a28. |
| `0x41e24` | EAX:uint32_t | EAX:uint32_t | 9a998@9a9c2 |  |
| `0x488f8` | AX:int16_t,DL:uint8_t | none | 857bc@85933 | Store a mode byte in unit AX and send the matching request. |
| `0x4bc80` | EAX:ef_addr32 | none | 4bc40@4bc51 |  |
| `0x4ca90` | none | none | 4c900@4ca72 | Clear the pending (==1) entries of the list at 0x1a5510 and refresh if any. |
| `0x4d114` | none | none | 4bf1c@4c112 | Draw the sprite described at 0x1a54f6. |
| `0x4d180` | none | none | 4bf1c@4c117 | As 0x4d114 for the sprite at 0x1a54fa. |
| `0x4d5f0` | none | none | 4d2fc@4d505 |  |
| `0x54328` | none | none | 54110@5413c |  |
| `0x54ba0` | EAX:ef_addr32 | none | 581b0@582fa | Copy three dwords and a byte from a message into record +7/+0xb/+0xf/+0x13. |
| `0x54ff4` | EAX:ef_addr32 | none | 581b0@58309 |  |
| `0x557e8` | none | none | 581b0@5840e |  |
| `0x56018` | EAX:uint32_t | AL:uint8_t | 56084@560b9 | Record a value in the history of twenty dwords at 0x1a5d70; returns 1 if already present. |
| `0x5c07c` | none | none | 601c@60af | Move word 0x1281f6 towards 0 or 100 by word 0x127d92. |
| `0x5f8d4` | AL:uint8_t | none | 5f60@5fa5,5fd8@5ff4,601c@60c5 | Switch indicator 0x1a6d18 on (mode 1) or off. |
| `0x64fa4` | none | none | 5f60@5f70 | Build the doubly linked free list of 100 entries (0x1c bytes) at 0x1a6d60. |
| `0x65360` | AL:uint8_t,DX:uint16_t | none | 654b8@6554b,73924@73948 | Find the active entry with type AL and id DX, unlink it and return it to the free list. |
| `0x65748` | EAX:ef_addr32 | none | 654b8@654f4 | Flag 4 or 1 in byte 0x1a81de when the object is within range of the viewpoint (positions scaled by 256). |
| `0x657c4` | EAX:ef_addr32 | none | 654b8@654fd | As 0x65748 without the second shift, flags in byte 0x1a81dd. |
| `0x6a090` | none | AL:uint8_t | 68a60@68a76,68a60@68ad3 | True when any of the twenty slots refers to a record whose type byte is 10 or 11. |
| `0x6b444` | AX:int16_t | none | 242b8@2439d |  |

## unit_flags (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x6c3d0` | none | none | 6b6c8@6bbbd |  |
| `0x6c8e0` | none | none | 6c488@6c520 |  |
| `0x7104c` | none | none | 6c488@6c500,6c488@6c5b3 | Advance the state byte +0x16e of the player record. |
| `0x7baa4` | none | none | 3dd30@3e255 |  |
| `0x7fb9c` | EAX:ef_addr32 | AL:uint8_t | 7ef14@7f0f5,7ef14@7f122,7ef14@7f1b0 | Character code for an object. |
| `0x80120` | none | none | 3dd30@3e881,3ea0c@3eed4,7feb0@7feee |  |
| `0x81308` | none | EAX:uint32_t | 77c90@77e39 | Prepare the 0x28-byte block at 0x1fc980; returns its descriptor or 0. |
| `0x8160c` | AX:uint16_t | none | 29b14@29bc5,29dd0@29e26,589c0@58a9b |  |
| `0x895bc` | EAX:ef_addr32 | none | none direct |  |
| `0x8960c` | EAX:ef_addr32 | none | none direct |  |
| `0x898d4` | EAX:ef_addr32 | none | 8935c@8936e,8935c@89410,8935c@89451 |  |
| `0x8b9e8` | AX:int16_t,DX:int16_t | EAX:uint32_t | 3ce70@3cea4 | Find the list node (list dword 0x12db5c, next at +0x1d) whose coordinates match; returns its word +0x1b or -1. |
| `0x8daa0` | AX:int16_t | none | 8d59c@8da8a | Copy flag bit 2 of word +0x26 into bit 3 and announce it. |
| `0x8db20` | AX:int16_t | none | 8d59c@8d9e1 | Copy flag bit 6 of word +0x24 into bit 7. |
| `0x8db80` | AX:int16_t | none | 8d59c@8da04 | Copy flag bit 10 of word +0x24 into bit 11. |
| `0x8dbe0` | AX:int16_t | none | 8d59c@8da27 | Copy flag bit 8 of word +0x24 into bit 9. |
| `0x8dc40` | AX:int16_t | none | 8d59c@8d9be | Copy flag bit 0 of word +0x24 into bit 5. |
| `0x8dc94` | AX:int16_t | none | 8d59c@8da49 | Copy flag bit 12 of word +0x24 into bit 14. |

## camera_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8dcf4` | AX:int16_t | none | 8d59c@8da67 | Copy flag bit 15 of word +0x24 into bit 0 of word +0x26. |
| `0x90828` | none | none | 8bb50@8bb5d |  |
| `0x90a70` | none | none | 3cfc0@3d2d3 | Initialise the forty 0x30-byte entries at 0x211b00. |
| `0x92f98` | none | none | 95e58@96cf0 |  |
| `0x92ffc` | none | none | 93060@93098 | Step byte 0x12dc34 through the list of words at +0x50 of the current record, skipping the player. |
| `0x94484` | none | none | 95e58@9619b |  |
| `0x947c4` | AX:int16_t | none | 95e58@9693f |  |
| `0x94830` | AX:int16_t | none | 95e58@968fa |  |
| `0x94a1c` | AX:int16_t | none | 95e58@969df | Unit 0xff means the record at dword 0x1a821c. |
| `0x985f4` | AL:uint8_t,EDX:int32_t | none | 49228@49753,7a844@7a89a,7e8c0@7eac1 | Percentage of progress (value - dword 0x40c120) * 100 / dword 0x40c118, reported on change. |
| `0x9a4d8` | none | none | 98bec@98bec |  |

## line_of_sight (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1c60c` | none | none | 8b39c@8b51e | Scale six signed words by the percentage in dword 0x122dc4. |
| `0x238c0` | EAX:int32_t | AL:uint8_t | 242b8@24376 | Can unit AX attack the target of its own target? |
| `0x327e0` | none | none | 3122c@31246 | Latch the position and target vectors. |
| `0x41028` | none | none | none direct |  |
| `0x5a8d0` | none | none | none direct |  |
| `0x66bf4` | EAX:ef_addr32 | AL:uint8_t | 65a88@65bcd,65d24@65de3,65e10@65e93 | Line-of-sight request from the viewpoint at 0x1a5bec to the object. |
| `0x66cd4` | AX:int16_t,EDX:ef_addr32 | AL:uint8_t | 291fc@29220,298f0@29942 | Line-of-sight request from `object` to unit AX. |
| `0x66d80` | EAX:ef_addr32 | AL:uint8_t | 62edc@63099 | As 0x66bf4 from the point at 0x21444c. |

## entity_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9cb20` | EAX:uint32_t | EAX:uint32_t | 41fc@420d,6b50@6bb9,f6a8@f700 | Try the two open archives, then fall back to 0xb3b90. |
| `0x9cb70` | EAX:uint32_t | EAX:uint32_t | 9cc28@9cc81,ea270@ea30d | As 0x9cb20 with 0xeba4c and 0xa7b3c; -1 becomes 0. |
| `0x9ce84` | EAX:uint32_t | none | f80@1ac1,f6a8@f8e4,fecc@ffa1 | Dispatch a close request by flag bits 0x10000/0x20000/0x40000. |
| `0x9d4cd` | EAX:ef_addr32 | EAX:int32_t | f80@1196,f80@11e0,4cf4@4cf9 | atoi: skip class-2 characters, optional sign, class-0x20 digits. |
| `0xa4380` | EAX:ef_addr32,EDX:ef_addr32 | EAX:int32_t | 20ec@2d06,4d4c@4d5b,4d4c@4d83 | strcmp: 0, -1 or 1. |
| `0xa5000` | EAX:ef_addr32 | none | a59d8@a5a0f | Clamp two word fields so that they fit inside the extents at +0x43 / +0x45. |
| `0xa59d8` | none | none | f819c@f8239 | Run the per-object initialisation steps once for every entity (1..word 0x41eed8). |
| `0xa5a68` | none | none | a499c@a4a13 |  |
| `0xa6940` | EAX:ef_addr32 | none | a59d8@a5a40 | Walk the child list at +0x91 (next at +0x21) and default the timers at +0x14 / +0x16. |
| `0xa7328` | EAX:ef_addr32 | none | acfb8@ad385,acfb8@ad3ce | Toggle flag bit 0 at +0x1d and propagate it. |
| `0xa89f0` | none | none | a8a38@a8b95,a8bd4@a8e57,a8bd4@a8f10 | Wait up to 600 polls for key 0x19. |
| `0xa8f80` | EAX:ef_addr32 | none | a9130@a918c | Branch when the variable equals the operand. |
| `0xa8fcc` | EAX:ef_addr32 | none | a9130@a91b3 | Branch when the variable differs. |
| `0xa9014` | EAX:ef_addr32 | none | a9130@a91da | Branch when the variable is not greater than the operand. |
| `0xacb68` | EAX:ef_addr32 | none | ac5f8@ac67c | Latch "previous" copies of the fields and shift flag bits 0..2 to 3..5 of byte +0x1e. |

## entity_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb038c` | none | none | b0f5c@b107d | Random orientation words at 0x41f32e (mod 3), 0x41f32c (mod 360) and 0x41f32a (mod 46). |
| `0xb0448` | none | none | ca7e0@ca82f | As 0xb038c but offset by the previous values (high words of the neighbouring dwords). |
| `0xb1740` | EAX:ef_addr32 | none | b17d4@b17e9,cb9f4@cbbe7 | Free the sub-allocations of an object. |
| `0xb1b10` | none | none | c594@c608 |  |
| `0xb3988` | EAX:uint32_t,EDX:uint32_t | EAX:int32_t | 67d4@687c,21654@21955,21654@219f0 | Approximate length from two components: max - max/32 - max/128 + 3/8 min + 3/128 min... |
| `0xb3b1c` | EAX:int32_t,EDX:int32_t | EAX:int32_t | 66fc@67c0,26744@268ab,26744@268bc | Quadrant-aware wrapper around 0xb3ae0 (angle of the vector EAX/EDX, 16-bit angle units). |
| `0xbcb50` | EAX:uint32_t | EAX:uint32_t | faafc@fab3e,fabf0@fac23 |  |
| `0xbcec0` | AL:uint8_t,DL:uint8_t | EAX:uint32_t | bcf44@bd0c6,f90ac@f9234 |  |
| `0xbd490` | AL:uint8_t,DL:uint8_t | EAX:uint32_t | bd508@bd52b,bd554@bd581,bd5a8@bd5ca |  |
| `0xbd5a8` | EAX:ef_addr32,DX:int16_t | EAX:uint32_t | bd5f8@bd655 |  |
| `0xbd688` | EAX:ef_addr32 | none | bdbf8@bdc0c,bf444@bf718 | Mark all slots (10 bytes each) present and total their counts. |
| `0xbd6d8` | EAX:ef_addr32 | none | bdbf8@bdc84,bdbf8@bdc99,bdbf8@bdcce | Count the present slots and total their counts. |
| `0xbf980` | EAX:ef_addr32 | EAX:uint32_t | bf81c@bf889 | Do the records at +0x33 and +0x37 differ in the bytes at +0x20 or +0x21? |
| `0xbf9d4` | EAX:ef_addr32 | EAX:uint32_t | bf81c@bf8a2 | As 0xbf980 for the bytes at +0x23 and +0x24. |

## engine_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc4105` | none | none | b69c@b6c0,39744@3979d,3dd30@3e01f |  |
| `0xca17c` | none | none | ac5f8@ac621,ac780@ac7a0 |  |
| `0xca7e0` | none | none | ba5c@baaf,bfcc@c0c1 |  |
| `0xcc584` | AL:uint8_t | EAX:uint32_t | d68c@df3a,d68c@e07c,d68c@e187 |  |
| `0xcc76d` | EAX:ef_addr32 | EAX:int32_t | e980@e98a,f3b58@f3b6f | Same as 0x9d4cd. |
| `0xcc894` | AL:uint8_t | EAX:uint32_t | afc5c@afca9 |  |
| `0xcd150` | AX:int16_t | AL:uint8_t | 1e214@1e220,1e28c@1e2c6,1e3c4@1e3f3 |  |
| `0xcd430` | none | none | cd4d4@cd4ea | Clear the key state tables and set the default scan codes. |
| `0xd1f10` | none | none | 69fac@69fbd | Install the four handler pairs. |
| `0xd37f4` | EAX:ef_addr32 | none | none direct | Count a hit when the cell under the object equals dword 0x159144 + 0xe0. |
| `0xd7c40` | none | none | none direct |  |
| `0xd7e94` | none | EAX:uint32_t | d86e0@d8993,d86e0@d89cb,d86e0@d8cd2 |  |
| `0xd8358` | AX:int16_t | none | 95e58@97033 | Advance the wrapping position at word 0x15bb3c by delta/4. |
| `0xdc3d5` | none | none | dc314@dc329 | Reset the scale factors and the patched shift counts to 8. |
| `0xddc88` | none | EAX:uint32_t | ddbe0@ddc69 |  |
| `0xde12c` | none | none | ddce8@ddce8 |  |
| `0xde2d8` | none | EAX:uint32_t | a9cd0@a9ce4 |  |
| `0xde4b0` | none | none | 984b4@984c1,a9aa8@a9b91,a9d48@a9daf |  |

## runtime_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe0870` | none | none | e1194@e1194 | Fill the channel record selected by byte 0x15f07f. |
| `0xe1194` | none | AL:uint8_t | none direct |  |
| `0xe1260` | none | AL:uint8_t | none direct | Is any of the six devices present? |
| `0xea4c0` | none | none | ea50c@ea56b | Rewrite a "$...:...$" template in place. |
| `0xeb9f8` | EAX:uint32_t,EDX:ef_addr32 | EAX:uint32_t | 9cb20@9cb3a,9cb20@9cb5a |  |
| `0xed4e7` | EAX:ef_addr32,EDX:ef_addr32 | none | ed3b4@ed3c3 | Parse printf flag characters into the spec at EDX (+0x14 flags, +0x16 fill). |
| `0xf2c78` | none | none | f2d74@f2d74,f3a1c@f3a36,f3a1c@f3a62 | Read the next script line (after leading blanks) from dword 0x485924 into 0x485956. |
| `0xf2cd0` | none | none | f2d74@f2d94,f2dfc@f2e02,f2e54@f2e55 | Copy the next token of the line at 0x485956 to 0x48592c. |
| `0xf45d8` | none | AL:uint8_t | f4640@f464e,f4640@f4665,f4640@f4696 | Read one character from the buffer at 0x485e59 (see 0xe774). |
| `0xf57d4` | none | EAX:ef_addr32 | f4a88@f4a8d | Take a node from the free list at 0x485df8 (or allocate one) and push it on the list at 0x485df4. |
| `0xf88e8` | EAX:ef_addr32 | none | f8b14@f8bcf,f8b14@f8be4,f8b14@f8c19 | As 0xbd6d8 for the layout with count byte +4 and array +5. |
| `0xf8938` | EAX:ef_addr32 | none | f8b14@f8b69 | As 0xbd688 for the layout with count byte +4 and array +5. |
| `0xf8a78` | EAX:ef_addr32 | EAX:uint32_t | f8acc@f8af2 | Decrement byte +3 while it exceeds 2 (modes 0 and 2) or 3 (other modes). |
| `0xfc08c` | AL:uint8_t,DX:uint16_t | EAX:uint32_t | fc0ec@fc167 | Map a mode code (and a flag word) to a table index. |
| `0xfc478` | EAX:uint32_t | AL:uint8_t | fc4e4@fc502,fc600@fc605 | VESA call 0x4f01 for mode ECX; true when the BIOS answers 0x4f. |
| `0xfc4e4` | none | EAX:uint32_t | fc534@fc5da | Find the first VESA mode in the list whose info has bit 7 set; returns dword +0x28 of its info. |

## runtime_helpers3 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10d5c7` | EAX:ef_addr32 | EAX:ef_addr32 | 10d4c4@10d568,10d75c@10d7db,10d80b@10d91c | Insert a heap block into the address-sorted list at 0x16464c and reset it. |
| `0x10f071` | EAX:ef_addr32 | EAX:ef_addr32 | f1d90@f1dd0,f1f5a@f1f95,f22af@f230d | Allocate the buffer of a stream record. |
| `0x111d98` | EAX:uint32_t | EAX:uint32_t | 111e40@111e4b,112c10@113021 | Number of data bytes of a MIDI status byte (cdecl). |
| `0x111f50` | EAX:uint32_t | EAX:uint32_t | 111f84@111fc4,111f84@112031,113d10@113d60 | Byte-swap a dword (cdecl). |
| `0x113c24` | EAX:ef_addr32 | none | 103504@103527 | Copy a path to 0x17e1d8 and cut the extension (cdecl). |
| `0x114994` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t | none | 103b90@103bc0 | Record slot b of owner a as value c-1 (cdecl). |
| `0x117ea8` | EAX:ef_addr32 | none | 118290@118384,118290@1183a5 | Free the buffer of a record and clear the record. |
| `0x117f14` | EAX:ef_addr32 | EAX:uint32_t | 117f68@11800a | Allocate the 0x1a-byte state block of a serial record. |
| `0x1184c4` | EAX:ef_addr32 | EAX:int32_t | 106cb0@106cd7,106d34@106d6b,106dc4@106de0 | Pop one byte from the ring buffer of a serial record, -1 if empty. |
| `0x1185ec` | EAX:ef_addr32 | none | 106d34@106d46,107070@10708e,107070@107119 | Reset both ring buffers of a serial record and quiesce the port. |
| `0x118664` | EAX:ef_addr32,EDX:uint32_t | none | 106f10@106fee | Set or clear the hardware-flow flag of a serial record. |
| `0x118694` | EAX:ef_addr32,EDX:uint32_t | none | 106f10@106fd9,117f68@11827f | Set or clear bit 0 of the modem control register. |
| `0x1186cc` | EAX:ef_addr32,EDX:uint32_t | none | 106f10@106fe5,117f68@118273 | Set or clear bit 1 of the modem control register. |
| `0x11c020` | EAX:uint32_t | none | 10a580@10a8ac,10c1b0@10c4d8 | Allocate and clear n records of 0xd5 bytes at 0x4ddee0. |
| `0x11c06c` | none | none | 11c020@11c02d | Free the five records at 0x4ddee0 and their 0x69 sub-buffers. |
| `0x11f308` | EAX:int32_t | EAX:int32_t | 11f368@11f3c0,11f368@11f3d9,11f368@11f3f7 | Interpolated table lookup (6-bit fraction, 0x17ea2c table), result >> 14. |
| `0x11f460` | EAX:int32_t | EAX:int32_t | 11f4d8@11f4f6,11f4d8@11f50d,11f4d8@11f52c | Table lookup of x * 2^14 / 0x6487 (0x17e628 table), result >> 14. |
| `0x11f4d8` | EAX:int32_t | EAX:int32_t | 11f538@11f53d,11f538@11f54a | Full-circle (0x1921f units) wrapper around trig_lookup_11f460. |

## ui_widget_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9c3cf` | EAX:uint32_t | EAX:int32_t | 81c@875,b04@b44,80058@8010a | Push a handler on the 32-entry stack at 0x40d030; -1 if full. |
| `0x9d749` | EAX:ef_addr32,EDX:ef_addr32 | none | f80@15a5,f80@1a32,20ec@2646 | strcpy. |
| `0x9d7ac` | EAX:uint32_t,EDX:uint32_t | none | none direct |  |
| `0x9d80c` | EAX:uint32_t,EDX:uint32_t | none | none direct |  |
| `0x9d938` | none | none | e4264@e429f |  |
| `0x9d964` | EAX:ef_addr32 | EAX:uint32_t | 9d990@9d9f1,9da80@9db84 | Parse the decimal digits at text. |
| `0x9da50` | EAX:ef_addr32 | EAX:ef_addr32 | 9da80@9db09,9da80@9dbc8 | Skip to the end of the current line and past the line break. |
| `0xa0da8` | AX:int16_t | EAX:ef_addr32 | 9e7bc@9f124,9e7bc@9f36d,9e7bc@9f5df | Address of 16-bit slot index in the current page record. |
| `0xa0de0` | AX:int16_t,DX:int16_t | EAX:ef_addr32 | 8c930@8c957,cd150@cd187,e3c6c@e3c9c | Select page record and return the address of slot index. |
| `0xa0f4c` | AX:int16_t | none | 9e7bc@9f0dc | Clear slot index of the current page. |
| `0xa14c8` | AX:int16_t | none | 9e7bc@9eabd | Set the high flag of slot index of the current page. |
| `0xa17e8` | AX:int16_t | EAX:int32_t | 3cd9c@3cde9,53c80@53cf8 | Value of the type byte (bits 2..7) if bit 1 is set, else -1. |
| `0xa1824` | EAX:ef_addr32,DX:int16_t | none | d0470@d04d3 | Copy the header fields of record id into dest. |
| `0xa6998` | EAX:ef_addr32 | none | a59d8@a5a32 | Refresh every child of a widget that is not of kind 9. |
| `0xa7104` | EAX:ef_addr32 | none | a6d00@a70f3 | Redraw the kind 1..3 children of a widget, stopping at the first kind 0. |
| `0xa7260` | EAX:ef_addr32 | none | a5ef8@a5fbf | Redraw all children of a widget (both original branches are identical). |
| `0xa729c` | EAX:ef_addr32,EDX:ef_addr32 | none | a72d8@a72fd,a7328@a7364 | Call a7380 on enabled siblings of the same group as other. |
| `0xa72d8` | EAX:ef_addr32 | none | 487c@490d,4e14@4eb6,4e14@4ecf | Enable a widget, notifying its group. |
| `0xa7e4c` | none | none | a9d48@a9d9c | Copy table 0x418708 to 0x418690 for unlocked entries. |
| `0xa7e88` | none | none | a9d48@a9dfa | Copy table 0x418690 back to 0x418708 for unlocked entries. |
| `0xa983c` | EAX:ef_addr32 | none | a9e70@a9fba | Apply a set/add/subtract operation to a guest value. |

## list_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xaa1e4` | EAX:ef_addr32 | none | aa5c4@aa612 |  |
| `0xaa228` | EAX:ef_addr32 | none | aa5c4@aa9c9,aaa50@aad3e | Append a child (links at +1/+5) to the list of the current owner. |
| `0xaa270` | EAX:ef_addr32 | none | aa5c4@aa696,aaa50@aab14 |  |
| `0xaa2b4` | EAX:ef_addr32 | none | aa350@aa395 | Resolve the (parent index, child index) pair of node to the child pointer. |
| `0xb0404` | none | none | bfcc@c0c6 | Advance the 16-bit counters at 0x13799c..0x1379a2. |
| `0xb16c0` | EAX:ef_addr32 | none | bed1c@bee8f,bf444@bf7f7,bf81c@bf963 | Free a record with 10-byte entries (count at +5, array at +6). |
| `0xb1700` | EAX:ef_addr32 | none | f9448@f956f | As above with count at +4 and array at +5. |
| `0xb17d4` | EAX:ef_addr32 | none | b1814@b1825,cb9f4@cba6f,cb9f4@cbb8b | Free a record with 0x7c-byte objects (count at +0x12, array at +0x13). |
| `0xb3d74` | none | none | e4264@e42a9 |  |
| `0xba40c` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill an 8-byte table depending on mode (3 selects the flat variant). |
| `0xbc594` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | ca838@ca85c,cb9f4@cba68,cb9f4@cbb84 | Unlink node from a list whose head slot is *head_a (edx) / *head_b (ebx). |
| `0xbcbc0` | EAX:uint32_t | EAX:uint32_t | be438@be462,fba6c@fbaae | Is the kind one of 8, 0xe, 0xf, 0x11, 0x13, 0x16, 0x17, 0x1a? |
| `0xbd344` | EAX:ef_addr32,EDX:ef_addr32 | EAX:uint32_t | bd39c@bd438 | Is there a kind-0x10 entry in the list matching the id at ref+9? |
| `0xbec84` | EAX:ef_addr32 | EAX:uint32_t | becc0@becde,faafc@fab1a,fabf0@fac0e |  |

## engine_helpers3 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9c980` | EAX:ef_addr32 | none | f80@1120,45a4@45ae,2ff44@2ff7d | Build the default path string in buffer. |
| `0xc3328` | none | none | c34cc@c3501,c36b4@c3709,c3714@c375c | Release the buffer at 0x41f3bc according to its mode byte. |
| `0xc37ec` | none | none | cdc38@cdc3a,d1d40@d1d84 | Free the two buffers at 0x146e40/0x146e44. |
| `0xca5e4` | none | none | ac16c@ac177 | Clear the flag bytes of records from the start index upward. |
| `0xcc1ac` | AL:uint8_t,EDX:uint32_t,EBX:ef_addr32 | EAX:uint32_t | d68c@db4b,d68c@e069,d68c@e393 | Is the flag byte for index negative? |
| `0xcc1ec` | AL:uint8_t,EDX:uint32_t,EBX:ef_addr32 | EAX:uint32_t | d68c@dd04,433fc@434b0,4357c@4361d | Is the flag byte for index zero? |
| `0xcc22c` | AL:uint8_t,EDX:uint32_t,EBX:ef_addr32 | EAX:uint32_t | d68c@d993,d68c@df24,d68c@e333 | Is the flag byte for index positive? |
| `0xcd20c` | EAX:uint32_t | EAX:uint32_t | 57480@575f3,8d11c@8d18c,8d330@8d39f | Store value into the output table and step the script cursor. |
| `0xcd5f4` | none | EAX:int32_t | 48e7c@48fe3 | Pop the first byte of the 20-byte queue at 0x41f810, -1 if empty. |
| `0xcdf60` | none | EAX:uint32_t | 3ea0c@3edf6,aaee8@aaf5f,abf8c@abfb9 | Query the mouse driver once and cache the button count. |
| `0xce02c` | EAX:ef_addr32,EDX:ef_addr32 | none | 121f0@121fa,ac16c@ac1ce,ac854@ac87e | Mouse function 0xb: read motion counters into two outputs. |
| `0xce064` | none | EAX:uint32_t | 278c0@27b32,278c0@27bc8,27d18@27eb6 | Mouse function 3: button state. |
| `0xce328` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 1e480@1e4a7,1e7c0@1e8d5,1f0fc@1f1de | Latch four values and the current pair at 0x15b744/0x15b748. |
| `0xce944` | EAX:ef_addr32 | EAX:ef_addr32 | d23d8@d2411 | Apply pending event bits of an object through the 5-entry table at 0x15b798. |
| `0xcf78c` | none | none | 14d58@14e3c,14d58@14e74,14d58@14ea5 | Forward the five words of the record at 0x4208e4 to cf3a4. |
| `0xd0858` | EAX:ef_addr32,EDX:ef_addr32 | none | 91c98@91d77,91df4@91e81,91df4@91ee6 | Copy a 9-dword record. |
| `0xd0b68` | EAX:ef_addr32 | none | 1b4a4@1b5e3,26b60@26f51,27348@27355 | Reset a 9-dword bounds record (0x7fff limits). |
| `0xd65a8` | EAX:ef_addr32,EDX:ef_addr32 | none | d660c@d6620,d663c@d664b | Remap a 256-entry RGB palette through three 64-byte tables. |
| `0xd6dc4` | none | none | d5d18@d5d39,d6e58@d6e9e,e28e8@e28ff | One-time module init; registers the exit handler at 0xd6da0. |
| `0xd7040` | EAX:ef_addr32,EDX:ef_addr32 | none | d7070@d7078 | Build "<default path><first><second>" with a fixed separator. |
| `0xdcc10` | none | none | a9e70@a9e8d | Reset the 320x200 clip state. |
| `0xdcc54` | none | none | a9e70@a9eb7 | Same as dcc10 with the stores in a different order. |

## runtime_helpers4 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe0b88` | EAX:ef_addr32 | EAX:uint32_t | e0b6c@e0b71,e0bcc@e0c37,e1194@e11c6 | Open the device selected by value and report success. |
| `0xe5830` | EAX:ef_addr32 | none | a6d00@a6d44 |  |
| `0xe5960` | EAX:ef_addr32 | none | a5ef8@a5f95,e588c@e588f | Compute the rectangle of a record into 0x41ec08..0x41ec0e. |
| `0xe5c03` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | EAX:int32_t | 81354@81369,82e00@82e17,c3430@c3495 | strncmp. |
| `0xeb448` | EAX:uint32_t | none | ea50c@ea5e7,eb2c0@eb315 |  |
| `0xeff7f` | EAX:uint32_t,EDX:ef_addr32 | none | ee22c@eed2d | Convert an unsigned 32-bit integer to an 80-bit extended value at dest. |
| `0xf79b0` | none | none | none direct | OR the mask at 0x46438c into the flag words. |
| `0xf88a0` | EAX:ef_addr32,DX:int16_t | EAX:uint32_t | f8988@f89b3 |  |
| `0xfa77b` | EAX:ef_addr32 | none | bc287@bc311,bc450@bc45c | Move the node owning object from the list at 0x485f9c to the free list. |
| `0xfc050` | AL:uint8_t | AL:uint8_t | fc0ec@fc119,fc1e8@fc21a | Classify a kind into 1, 2 or 3. |
| `0xfc6c8` | EAX:uint32_t | none | d1d40@d1d58 | Query the video driver and clamp the width field. |

## tiny_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9c024` | EAX:uint32_t | EAX:uint32_t | none direct | Real-mode segment:offset style conversion of a dword (cdecl). |
| `0x9c1a1` | EAX:uint32_t | EAX:uint32_t | none direct | High word of a dword (cdecl). |
| `0x9c1b1` | EAX:uint32_t | EAX:uint32_t | none direct | Identity (cdecl). |
| `0x9c960` | none | EAX:uint32_t | f60@f6a,2e04@334f,2e04@337c |  |
| `0x9d780` | EAX:ef_addr32,EDX:ef_addr32 | none | a00a4@a00c5,a08c8@a0931 | Copy the first and third dword of a record and clear the second. |
| `0x9d794` | EAX:uint32_t,EDX:uint32_t | none | 9f664@9f697,9f710@9f720,9f850@9f885 |  |
| `0x9dde0` | EAX:ef_addr32,EDX:uint32_t | AX:uint16_t | 9e7bc@9f5d5,9f7d0@9f7f7,9f808@9f83d | Store the converted table entry index into the 16-bit slot at destination. |
| `0xa41c8` | none | none | 33750@33759,34e18@34e20 |  |
| `0xa41d0` | none | none | 34ca4@34d34 |  |
| `0xa455b` | none | none | 9bdbb@9bdfd | Run edd49 with the byte at 0x132a60 cleared, then restore the dword. |
| `0xa4d90` | EAX:ef_addr32 | EAX:ef_addr32 | 4738@4856,487c@48d7,487c@48e1 |  |
| `0xa4db0` | EAX:ef_addr32 | EAX:ef_addr32 | 487c@4917,487c@4921,494c@49e8 |  |
| `0xa6018` | EAX:ef_addr32 | EAX:ef_addr32 | 5110@51c6,ba5c@bbab,bfcc@c4aa |  |
| `0xa7318` | EAX:ef_addr32 | EAX:ef_addr32 | 362c4@362f4,362c4@36300,362c4@3630c |  |
| `0xa75fc` | EAX:ef_addr32 | EAX:uint32_t | 4c50@4c66 |  |
| `0xa7b70` | none | EAX:ef_addr32 | a81e4@a8243,a9878@a9882,a99c0@a99f2 |  |
| `0xb3d68` | none | EAX:uint32_t | 3d4b0@3d729 |  |
| `0xbaf24` | EAX:ef_addr32 | none | bb324@bb47d |  |
| `0xbc450` | EAX:ef_addr32 | EAX:uint32_t | bc421@bc448 |  |
| `0xc8e38` | none | none | ac408@ac438 |  |
| `0xc8fdc` | none | none | ac408@ac42e |  |
| `0xc90c0` | none | none | ac408@ac433 |  |
| `0xcbd0f` | EAX:int32_t,EDX:ef_addr32,EBX:uint32_t | EAX:ef_addr32 | c768@c79e,c768@c7db,4d668@4d78e | Signed decimal prefix, then the digit conversion. |
| `0xcd29c` | none | EAX:ef_addr32 | 10f70@10fba,11014@11042,1e214@1e263 |  |
| `0xd16fc` | EAX:uint32_t | none | d170c@d1750,d1774@d179e,d1774@d17b2 |  |
| `0xd6f88` | none | EAX:uint32_t | 80120@80177 |  |
| `0xd7070` | EAX:ef_addr32 | EAX:ef_addr32 | d6fa8@d6fb4 |  |
| `0xd7738` | EAX:ef_addr32 | EAX:uint32_t | 7fcdc@7fdca,8018c@801a7,801e4@8027a |  |
| `0xd7748` | EAX:ef_addr32 | EAX:uint32_t | 801e4@8022c |  |
| `0xe7cd0` | EAX:ef_addr32,EDX:ef_addr32 | none | e7cec@e7d1f | Append a fixed string and then second to first. |
| `0xea414` | none | EAX:uint32_t | 9dcf8@9dd0f,de864@de985 |  |
| `0xea864` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 97534@97578,97534@9758a,975f0@97674 | Store three dwords. |
| `0xf13ac` | EAX:ef_addr32 | none | a56f8@a5704 |  |

## runtime_helpers5 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd17d0` | none | none | d1c54@d1c7c | Emit the fixed command sequence of the 0x18f6e4 display list. |
| `0xe4264` | none | none | 3ea0c@3edc9,3ea0c@3ee1d,3ea0c@3ef59 | Reset the engine state unless already in state 0 or 2. |
| `0xf8010` | none | none | none direct | Merge the two 2-bit fields of the mask at 0x46438c into the object flag bytes. |
| `0xfc600` | EAX:uint32_t | EAX:uint32_t | d1da8@d1db2,d1da8@d1dbd,d1de8@d1e16 | Set a VESA mode, with the linear-framebuffer bit first if supported. |

## ui_state_helpers (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa4030` | none | none | 34e18@34e53,34f94@34ff1,357a4@357ba | Recompute the total size at 0x135838 from the two index lists. |
| `0xa7be8` | AL:uint8_t | EAX:uint32_t | a8958@a89d7,a8a38@a8b8c,a8bd4@a8e4a | Handle the current key code at 0x41f833; returns 1 if consumed. |
| `0xa9d48` | none | none | a9e70@a9f23,a9e70@aa13e | Toggle between the live view state and the saved copy at 0x4195b0.. |
| `0xe65d8` | EAX:ef_addr32,EDX:ef_addr32 | none | e6684@e66ea,e6684@e6746,e6684@e6782 | Exchange the fields of two 0x15-byte records. |
| `0xfaa30` | AL:uint8_t | AL:uint8_t | fbad8@fbc5b | Map a scan code to a key class, 0 if unmapped. |

## game_helpers4 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x5fd8` | none | none | 8d11c@8d275 | Reinitialise the game screens. |
| `0x6b1c` | none | none | 5f60@5fb5 |  |
| `0xa9a8` | none | EAX:int32_t | ab74@ad1f,ab74@adfd,ab74@af5d | Smaller of (table byte + 1) and (random % 3 + 2). |
| `0xb544` | none | none | none direct |  |
| `0xef5c` | EAX:ef_addr32 | EAX:uint32_t | f6a8@f8cb | Fill four dwords with 21-bit values scaled by 2^11. |
| `0x13950` | none | none | 848e0@849a0 |  |
| `0x16984` | EAX:uint32_t | EAX:uint32_t | 169b4@169c6,16a04@16a07,16a3c@16a4e | Is the unit alive and not of kind 0xc? |
| `0x16a04` | EAX:uint32_t | EAX:uint32_t | 15af0@15b4a |  |
| `0x16a3c` | EAX:uint32_t | EAX:uint32_t | 15af0@15b61 |  |
| `0x16a74` | EAX:uint32_t | EAX:uint32_t | 15b7c@15bf3 |  |
| `0x17434` | AX:int16_t | EAX:uint32_t | 15b7c@15ded | Search the 20 three-byte slots of a unit for kind 6 or 0x15. |
| `0x174c8` | EAX:uint32_t,EDX:int32_t | EAX:uint32_t | 15b7c@15cb4 | Does the unit's target differ from value and the unit's kind allow it? |
| `0x174f8` | EAX:uint32_t | EAX:uint32_t | 16034@16069,16034@16158,16034@16170 | Is the unit's kind not one of 0, 7, 9, 0xa, 0xb, 0xc? |
| `0x1b52c` | none | EAX:uint32_t | 1b4a4@1b4ca,1b4a4@1b4d9 |  |

## game_helpers5 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe744` | EAX:uint32_t | EAX:ef_addr32 | e990@e9a7,f038@f05b,f08c@f0b9 | Allocate-and-clear: allocate count bytes via b3db0/b3f58 and zero them. |
| `0x1d778` | EAX:ef_addr32 | none | 1d700@1d74d,1d700@1d768 |  |
| `0x1d8a8` | EAX:ef_addr32 | EAX:uint32_t | 1d700@1d73f,1d700@1d75d | Advance the stage counter for some stage kinds; returns 1 if advanced by 0x1d8e8. |
| `0x1e130` | AX:int16_t | none | 5437c@543eb | Forget the cached record pointer if it equals record index. |
| `0x28a34` | none | none | 288c8@28988 | Move the head of the used list to the free list. |
| `0x2af20` | none | none | 2af64@2af67 | Set three flag byte ranges at 0x19bb60.. to 1. |
| `0x2af64` | none | none | 49228@49606 |  |
| `0x2db64` | AX:int16_t,EDX:uint32_t | EAX:uint32_t | 2dba4@2dbd4 | Does the unit named by table entry slot carry id? |
| `0x35528` | none | none | 36ad8@36b47,36ad8@36bb4 |  |
| `0x357a4` | none | none | none direct |  |
| `0x35cc8` | none | none | 36438@36472,36438@3657e,36438@3669d |  |
| `0x3cd6c` | none | EAX:ef_addr32 | 3dd30@3e393 | Find the node with tag 0x2117b0 in the ring starting after the head. |
| `0x3ce30` | none | none | 3dd30@3e37a | Count down the timer at 0x1a35ac, firing 151c4 when it runs out. |
| `0x41ec8` | EAX:uint32_t | EAX:uint32_t | 9a998@9aa3e | Is the kind in 3..10? |
| `0x41f24` | none | none | 43020@430ea | Clear the 20 seven-byte slots and rebuild the screen buffer. |
| `0x46038` | none | none | 49228@49610,7e8c0@7ea1f |  |
| `0x48020` | none | none | 77c90@77dc7 | Advance the tick counter at 0x126658 by the frame time (a third in slow mode). |
| `0x4be88` | EAX:ef_addr32 | none | 4bc40@4bc64 |  |
| `0x50c70` | none | none | 510d0@51175 |  |
| `0x50dc8` | none | EAX:uint32_t | 50e84@50eb1 | Does the text at 0x1a5968 end in ':'? |
| `0x52128` | none | none | 50f54@50fae |  |
| `0x54eb4` | EAX:ef_addr32 | none | 581b0@583e3 | Mark slot b[ptr+1] in table 0x1a5d10 if the sender matches. |
| `0x54f04` | EAX:ef_addr32 | none | 581b0@583ff | As above for table 0x1a5d08 / counter 0x1a5d1e. |
| `0x55050` | EAX:ef_addr32 | none | 581b0@582c4 |  |
| `0x550d8` | EAX:ef_addr32 | none | 581b0@58318 | Copy the 0x19-byte payload of a message into object message[1]. |
| `0x56d18` | EAX:uint32_t | none | 581b0@584a0 |  |
| `0x56e94` | EAX:ef_addr32 | none | 581b0@584be |  |
| `0x57160` | EAX:ef_addr32 | none | 581b0@58532 |  |
| `0x571b8` | EAX:ef_addr32 | none | 581b0@58541 |  |
| `0x57620` | EAX:ef_addr32 | none | 581b0@5858c |  |
| `0x5ea64` | none | none | 5f634@5f688 |  |
| `0x62504` | none | none | 62454@624eb | Process every entry of the 0x17-byte table at 0x1a7d32. |
| `0x65d18` | none | none | 65a50@65a6d |  |
| `0x689f0` | none | none | 69fac@6a086,84b64@84b64 | Copy the 20 three-byte entries of table 0x1f6eac to 0x1f6ef4. |
| `0x68a2c` | none | none | 68a60@68b69 |  |
| `0x73d1c` | AX:int16_t | none | 6c8e0@6c920,72944@72c4d | Clear a unit flag one time in five. |
| `0x7597c` | none | none | 3dd30@3dd65 |  |
| `0x77be0` | none | none | 3dd30@3ddd2 |  |

## game_helpers6 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x624f4` | none | none | 601c@607d |  |
| `0x7556c` | AX:int16_t | EAX:uint32_t | 77c90@77e79 | Return the pointer or value associated with the unit's slot. |
| `0x7632c` | none | none | 767f4@76871 | Copy the 3-byte payloads of the 4-byte entries at 0x421750 until 0xff. |
| `0x7a51c` | EAX:ef_addr32 | none | 7a66c@7a6ef | Append a string to the string pool at 0x1f8e94. |
| `0x7a54c` | none | EAX:ef_addr32 | 7a66c@7a6cc | Copy the pool string into 0x1f8f14 and advance the pool. |
| `0x7a5bc` | none | none | 7a738@7a816 |  |
| `0x7b3c8` | none | none | 3ea0c@3ee55 | Allocate the two 0x27100-byte buffers once. |
| `0x7eec4` | none | none | 17f10@17f85,8d11c@8d218 |  |
| `0x802e0` | AX:int16_t | none | 7ef14@7f445 |  |
| `0x80314` | none | none | 87a20@87d40 |  |
| `0x815b4` | EAX:ef_addr32 | EAX:ef_addr32 | 8748@878a | Find the 8-byte entry whose two words match those at pointer. |
| `0x8461c` | none | AL:uint8_t | 84678@846b2 |  |
| `0x84a80` | AX:int16_t | AL:uint8_t | 848e0@848ef | Does the unit own a slot of kind 0x21 or 0x22? |
| `0x85b70` | none | none | 49228@4960b |  |
| `0x880c8` | none | none | 3dd30@3e7f4 | Run 878c8 for every ring node of kind 1. |
| `0x8965c` | EAX:ef_addr32 | none | none direct |  |
| `0x89990` | EAX:ef_addr32 | none | 8935c@89428,8935c@89469 |  |
| `0x8c064` | EAX:uint32_t | AL:uint8_t | 9596c@95a0d | Is value present in the 0x898-entry table at 0x20f550? |
| `0x8d554` | AX:int16_t | none | 8dd48@8de76 |  |
| `0x94880` | EAX:ef_addr32 | none | 95e58@96887,95e58@9689f |  |
| `0x98280` | none | none | 3ea0c@3eede |  |
| `0x982a0` | none | none | 8b39c@8b4b5 |  |
| `0x984f0` | EAX:uint32_t | EAX:ef_addr32 | 6df0@6e03,7780@7794,2aef8@2af0b | Allocate size bytes and clear them. |
| `0x9bb64` | EAX:ef_addr32 | none | 9b8a0@9b8c0,9b8a0@9b8e0,9b8a0@9b900 | Shift the three 20-entry word tables of a record down by one. |

## game_helpers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x7dec` | AX:int16_t,DL:uint8_t,BL:uint8_t | EAX:uint32_t | 2c778@2cae2,70a58@70b7e |  |
| `0x825c` | AL:uint8_t | AL:uint8_t | 84b8@8546,8fe4@8ff6,2ab54@2acfb | Find the first free slot of table al (0xfe if all are used, 0xff if none usable). |
| `0x8f4c` | AL:uint8_t | none | c630@c6f9,54210@5426a,7e8c0@7ebb6 |  |
| `0x8f9c` | AX:int16_t | none | 90c0@90fa | Run 46254 for each of the seven word slots of a unit. |
| `0xab2c` | AX:int16_t,DX:int16_t | EAX:uint32_t | ab74@acf2,ab74@ad08,ab74@add0 | Random value in [a - b, a + b], clamped to 10..0x186. |
| `0x11770` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | 494c@4990 | Is there an entry with these three words and kind 1? |
| `0x17538` | AX:int16_t | EAX:uint32_t | 15b7c@15ff6 | Take one charge from the first of the ten word slots that has one. |
| `0x1e1c0` | EAX:ef_addr32,EDX:int32_t,EBX:int32_t,ECX:int32_t | EAX:uint32_t | 1e214@1e268 | Is the point inside the 16-bit box at box? |
| `0x1e684` | none | none | 3dd30@3dfb8 | Age the twelve timers; those in state 0xe expire back to state 2. |
| `0x1f8c4` | none | EAX:uint32_t | 1f994@1fd73 | Pop the head of the queue at 0x19b0d0. |
| `0x237c8` | EAX:ef_addr32 | none | 8846c@884f1 | Translate a vector by the origin at 0x21444c.. and pass it to b39ec. |
| `0x266f8` | EAX:ef_addr32 | none | 26670@266ee |  |
| `0x2888c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | 288c8@28917 | Largest of three magnitudes (via cf94a). |
| `0x2b508` | AX:int16_t | AL:uint8_t | 857bc@8598f,857bc@859a8 |  |
| `0x2bb64` | AX:int16_t | EAX:uint32_t | 73964@73a44,73964@73ba6 | Find the unit linked to unit_id whose kind is 0xb, or 0xff. |
| `0x33e78` | none | none | 34b2c@34b49 |  |
| `0x35fbc` | none | none | 36054@36076 |  |
| `0x36004` | none | none | 36438@367a1 |  |
| `0x36964` | none | none | 369fc@36a17 |  |
| `0x369ac` | none | none | 36ad8@36ce1 |  |
| `0x38674` | AX:int16_t | EAX:int32_t | 15b7c@15c97,15b7c@15cc4,15b7c@15da6 | Number of live, non-flagged objects in the ten word slots of entry index. |
| `0x386f8` | AX:int16_t | EAX:uint32_t | d290@d4ca | Position of the entry's node in its list. |

## game_helpers8 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3c558` | none | none | 3c5b8@3c5bd,3c5b8@3c5f9,3c5b8@3c611 | Copy the next CR-terminated line from the input buffer to 0x1a2df0. |
| `0x3c94c` | AX:int16_t,DX:int16_t,BX:int16_t | none | 3cc94@3ccd9 | Store a 16-bit value split over two byte planes at (row, 0x18f - column). |
| `0x3d904` | none | none | 3dd30@3e458 | Draw the 16x16 grid of 10x10 cells. |
| `0x3da24` | none | AL:uint8_t | none direct | Are all entries of the table at 0x12b268 satisfied? |
| `0x45694` | none | none | bfcc@c0cd |  |
| `0x45ba0` | none | none | 3dd30@3ddd7,46038@46039 | Reset the 10-entry id table and the four 0x190-byte pages. |
| `0x46128` | EAX:ef_addr32 | none | 8d59c@8d708 | Retire the entries of the 9-slot table that belong to object. |
| `0x50770` | none | EAX:int32_t | ba5c@bbe1,bfcc@c4ea,d628@d640 | Largest (second - first) byte difference over the list at 0x137988. |
| `0x55784` | EAX:ef_addr32 | none | 581b0@58602 | Release a unit's slot if it still refers to the unit's target. |
| `0x55b80` | EAX:ef_addr32 | none | 581b0@5859b |  |
| `0x562d8` | EAX:uint32_t,DX:int16_t | none | 561e4@56200 | Bump the per-slot counter of the first record whose key equals value. |

## game_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x57218` | EAX:ef_addr32 | none | 581b0@58550 |  |
| `0x5736c` | EAX:ef_addr32 | none | 581b0@5856e | Clear slot message[3] of the owner's table. |
| `0x58028` | none | none | 3ea0c@3ee41 | Rebuild the bit mask of the slots 1..count-1. |
| `0x653d8` | EAX:uint32_t,EDX:uint32_t | EAX:ef_addr32 | 6563c@65681 | Find the node with this kind and id in the list at 0x1a7c54. |
| `0x685ac` | none | none | 3cfc0@3d1ac,3cfc0@3d1b6 | Fill 300 entries of three words with sums of two random values. |
| `0x6900c` | none | none | 69fac@69fcf |  |
| `0x699f4` | EAX:ef_addr32 | EAX:uint32_t | 69bdc@69bfd | Index of the 0x91-byte record whose name equals text, or 0. |
| `0x6a148` | EAX:uint32_t | EAX:uint32_t | 73500@7355a | Next unit after start that belongs to a record of kind 1, or 0xff. |
| `0x752d0` | AX:int16_t | none | 6a1d0@6a2c9,6a1d0@6a2d5 | Copy the waypoint selected by the unit's index into the unit. |
| `0x755c8` | AX:int16_t,DL:uint8_t | EAX:uint32_t | 2af78@2afb0 | Sum the weights of the unit's slots whose class equals filter. |
| `0x77088` | EAX:ef_addr32,DL:uint8_t | none | 770d4@772a7 |  |
| `0x7ae0c` | EAX:int32_t,EDX:int32_t | EAX:uint32_t | 7bb00@7bb2c | Read the 16-bit value at wrapped (a, b) of the 0x190x0x190 two-plane map. |
| `0x7f7dc` | EAX:ef_addr32 | EAX:uint32_t | 7ef14@7f103,7ef14@7f1bc | Squared distance between a vector (>>8, 16-bit) and the origin at 0x21444c. |
| `0x8018c` | AX:int16_t,EDX:uint32_t | none | 3122c@314fe,94d08@94f51 |  |

## game_helpers10 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1f0c8` | none | none | 5f60@5f84 |  |
| `0x2d894` | AX:int16_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 2b1f4@2b2fe,2b1f4@2b358 | Store two words in the owner record and refresh it. |
| `0x34a04` | none | none | 33630@33e6c |  |
| `0x34a38` | none | none | 35590@35605 |  |
| `0x34a68` | none | none | 35cfc@35d19 |  |
| `0x391d0` | none | none | 39204@39249 |  |
| `0x7ae4` | AX:int16_t,DL:uint8_t,BL:uint8_t,CL:uint8_t | none | 46454@469fe,7156c@71a1c | Forward a command for a unit; kind 10 uses 0xff in place of its byte. |

## game_helpers11 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3cc94` | EAX:ef_addr32 | none | 3cd24@3cd30 | Plot a pixel for each step of the string walker. |
| `0x413b0` | none | none | bfcc@c110,41f24@41f5b |  |
| `0x507dc` | none | none | ba5c@bbef,bfcc@c4f8,d628@d64e |  |
| `0x50e84` | none | EAX:uint32_t | 52164@522bb | Wait for a key other than 0x3b with a long enough name, then handle it. |
| `0x540d0` | none | none | 3dd30@3de01 |  |
| `0x54e80` | EAX:uint32_t | none | 581b0@583c2 |  |
| `0x77034` | AX:int16_t,DX:int16_t,BL:uint8_t | none | 77088@770c0 | Send command 0x44/0x45/0x43 for kind 0/1/2. |

## game_helpers12 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x54688` | AX:int16_t,DL:uint8_t | none | 54f4c@54fe9 | Mark the unit as pending (0xfe) and send command 0x0d. |
| `0x546e8` | AX:int16_t,DL:uint8_t | none | 54ff4@55047 | Mark the owner record as pending (0xfe) and send command 0x17. |
| `0x54720` | AX:int16_t | none | 37934@37b1d,38284@3855c | Send the position block of object id (command 0x21). |
| `0x54784` | AX:int16_t,DX:int16_t | none | 26b60@270bc | Send one 0x44-byte sub-record of the object (command 0x22). |
| `0x55dac` | EAX:ef_addr32,DX:uint16_t | none | 1d5cc@1d5e0 | Send a position vector and a word (command 0x0f). |
| `0x55dec` | EAX:ef_addr32 | none | 888cc@88c33 | Send the word at +0x54 of the record (command 0x10). |
| `0x55e1c` | AX:int16_t,DL:uint8_t,BX:uint16_t | none | 900c0@900fb | Send the parameter block for an object (command 0x28). |
| `0x55e94` | AX:int16_t,DL:uint8_t | none | 748f4@7493f | Send the parameter block for a unit (command 0x23). |
| `0x55f0c` | AX:int16_t,DX:uint16_t,BX:uint16_t,CX:uint16_t | none | 90124@901e0 | Send an object's parameters with four caller words (command 0x27). |
| `0x56f6c` | none | none | 3dd30@3e2e7 | Send the three words at 0x12583c (command 0x34). |
| `0x570c8` | none | none | 581b0@58507 |  |

## runtime_helpers6 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9ddfc` | EAX:ef_addr32 | EAX:uint32_t | 9820@9ace,2a720@2a76e,2a7ac@2a7d1 | Call 0x9de08 with a zero second argument. |
| `0xa28f2` | EAX:int32_t | EAX:uint32_t | 20ec@2128,20ec@2142,20ec@215a | Absolute value. |
| `0xab838` | EAX:uint32_t | none | 84d94@84db1 |  |
| `0xb3d60` | none | EAX:uint32_t | 3d4b0@3d72f |  |
| `0xb3f48` | none | EAX:uint32_t | a7b8c@a7b8c,a992c@a9988,a9e70@a9e74 |  |
| `0xb3f50` | none | EAX:uint32_t | a7bb0@a7bb0,a9990@a99ba,a9e70@aa02d |  |
| `0xc3824` | none | EAX:uint32_t | eac48@eaca8 |  |
| `0xc382c` | none | EAX:uint32_t | ab8b0@ab8b3,c9fc4@ca07c,c9fc4@ca142 |  |
| `0xb55a4` | EAX:ef_addr32 | EAX:int32_t | b7d4c@b7da2,b7d4c@b7dbf,b8510@b85c5 | Random number modulo the signed high word at +0x2 of the record. |

## runtime_helpers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9c9c0` | EAX:ef_addr32,EDX:ef_addr32 | none | 371b0@371c2,abaf0@abaf6,c8e58@c8e75 | Build a search path: default directory + separator + name. |
| `0x9d7d4` | EAX:int32_t,EDX:int32_t | none | none direct | Reset the clip words; the first stores are overwritten in the original. |
| `0x9dc88` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 9de08@9de22 | Lower-case the name part (at most 8 chars) of a file name and append it to a path. |
| `0x9f7d0` | AX:int16_t,EDX:uint32_t | none | 9e7bc@9e80d | Clear the flag byte of a page slot and mark the page dirty. |
| `0x9f808` | AX:int16_t,EDX:uint32_t,BL:uint8_t,ECX:uint32_t | none | 9e7bc@9e832 | Set flags and a relative word offset in a page slot and mark the page dirty. |
| `0xa0d58` | AX:int16_t,DX:uint16_t,EBX:ef_addr32 | none | 9e7bc@9e916,9e7bc@9e937,9e7bc@9e94c | Copy count words into the current page from source. |
| `0xa0018` | AX:uint16_t,DX:uint16_t | EAX:int32_t | a0228@a0240 | Find the entry in the 0x4e-byte chain whose two keys match, or -1. |
| `0xa1a90` | AX:int16_t,EDX:int32_t | EAX:uint32_t | 2fbc4@2fc8b,3c730@3c744,74228@74267 | Look up key in the (key, offset) pair table of a page; return the address or 0. |
| `0xa2634` | AX:int16_t,DX:int16_t,EBX:uint32_t | none | 9e7bc@9f498 | Fill one entry of the current page and refresh the page slot. |

## runtime_helpers8 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa3acc` | none | none | a3c18@a3c90 | Install the table of ten handler addresses. |
| `0xa33a4` | EAX:uint32_t | none | a33f0@a3404 | Dispatch on the low nibble of the entry's kind byte. |
| `0xa33f0` | none | none | a410c@a4119 | Dispatch every entry of the list at 0x136b84. |
| `0xa4678` | EAX:ef_addr32 | EAX:ef_addr32 | a46cc@a46d0 | Find the registered entry whose name equals key, or 0. |
| `0xa4dd0` | EAX:ef_addr32 | EAX:ef_addr32 | 345e8@34739,34a98@34ad9 | Run a widget modally until its flag 4 is cleared. |
| `0xa5484` | EAX:ef_addr32,DX:int16_t,BX:int16_t | none | a520c@a52cf,a520c@a5303,a54f0@a54fe | Move a widget, clamped to the screen unless flag 0x10 is set. |
| `0xa570c` | EAX:ef_addr32,DL:uint8_t | none | 340a4@34132,34298@34319,34298@34343 | Toggle the child controls of kinds 5, 6, 0xd, 0xe and 0xf. |

## runtime_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10710` | AX:uint16_t,DX:uint16_t,BX:uint16_t,CX:uint16_t,[ESP+4]:uint16_t,[ESP+8]:uint8_t | none | 882b0@8833e | Store six parameters in the 0x197c24 block and mark it valid. |
| `0xa5b1c` | EAX:ef_addr32,EDX:uint32_t,BX:uint16_t,CX:uint16_t,[ESP+4]:uint16_t | none | 41314@413a5,4b1f0@4b2ce,69bdc@69c7c | Store four parameters in a widget and redraw it when visible. |
| `0xd9ba0` | EAX:uint32_t,EDX:uint32_t,EBX:ef_addr32,ECX:ef_addr32,[ESP+4]:ef_addr32,[ESP+8]:ef_addr32 | EAX:uint32_t | 93e58@93e89,94000@94031 | Mouse-style call 0x33 (entry value ORed with 0x70); results through four pointers. |

## game_helpers13 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1ad8` | none | none | f80@1ac6 | Reset the selection state block at 0x196fe8. |
| `0x2d78` | AL:uint8_t,DL:uint8_t,BX:int16_t,ECX:uint32_t | AL:uint8_t | 20ec@24d9,20ec@24fd,20ec@2521 | True when both owner bytes of the 62-byte record match and the time is in range. |
| `0x3d74` | none | AL:uint8_t | 1cd8@1e92 | Refresh the selected state byte from the owner and report a change. |
| `0x7b38` | AL:uint8_t,DL:uint8_t,BX:int16_t | AL:uint8_t | 7bc0@7c00,7bc0@7c4c | True when the unit's slot entry matches its type and refers to it (or to anyone). |

## game_helpers14 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x946c` | none | none | 94fc@97ab | Bubble-sort the (key, value) word pairs at 0x197370 by key. |
| `0x9e38` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32 | AL:uint8_t | 9ea0@a228,357ec@3599c | Find the 25-byte entry of kind 8 (the last one) and copy its fields out. |
| `0xed80` | EAX:ef_addr32 | EAX:uint32_t | f314@f483,fb10@fb76 | Read one record from the input stream into the 0x1c-byte record at dest. |
| `0x159a0` | EAX:ef_addr32 | EAX:uint32_t | 48c34@48c6b,48d5c@48d89,48da0@48dcd | Pixel width of a string: 3 for a space, glyph width + 1 otherwise. |

## game_helpers15 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x173c4` | AX:int16_t | EAX:uint32_t | 16a74@16a9d | Scan the owner's linked units for a slot code of 6 or 0x15. |
| `0x1d318` | none | none | 3ea0c@3ee4b | Reset the game-loop state and run ten warm-up ticks. |
| `0x1fd88` | none | none | 2065c@2067b | Build the 0x19b030/0x19b080 circle tables (full turn is 0xc000 steps). |
| `0x1fe4c` | none | none | 1ff0c@1ff6b | Build the 0x19afe0/0x19af90 circle tables from the two radius bytes. |

## game_helpers16 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x277d8` | AX:int16_t | none | 278c0@27aca | Show the confirm dialog; Enter (0x15) marks the slot record. |
| `0x2b3b0` | AX:int16_t,DL:uint8_t | none | 2b0b4@2b125,2b0b4@2b14a | Send a command to each of the owner's seven linked units that accept it. |
| `0x2b444` | AX:int16_t,DL:uint8_t | none | 2b1f4@2b253,2b1f4@2b2ce,2b1f4@2b319 | Send a command to a unit, then pair it with each accepting unit of its owner. |
| `0x2c6dc` | AX:int16_t | AL:uint8_t | 2c778@2cbd7 | Nonzero (1) when the owner fails the check selected by its record type. |

## game_helpers17 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x2f740` | none | none | 3dd30@3e1e4 | Draw every marker of the 16-byte table that is in range. |
| `0x37b44` | AX:int16_t,DX:int16_t | EAX:uint32_t | 8748@87dc | Index of the last 0x45-byte record whose position cell is (x, y), or 0xff. |
| `0x38750` | AX:uint16_t,DX:uint16_t,BX:uint16_t,CL:uint8_t,[ESP+4]:uint8_t,[ESP+8]:uint8_t,[ESP+12]:uint8_t | none | 22f28@22f8d | Store a scaled set of 18 percentages, or mark the slot unused. |
| `0x39960` | none | none | 39a0c@39a1f | Build the 0x1a17c0/0x1a1810 ellipse tables (radii 10 and 8). |
| `0x40428` | none | none | 3dd30@3e435 | Clear the 320x200 view and refresh the unit selected by the current key. |
| `0x48ae8` | AX:int16_t,DX:int16_t | EAX:uint32_t | 77c90@77dff | Classify an entry: 0/1 for ordinary, 2 for kind 2, logs unknown kinds. |

## game_helpers18 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x50d04` | none | none | 50e84@50ea4,50fcc@51006,50fcc@5104f | Copy the next token of the command line (0x1a5986) to 0x1a5968. |
| `0x544f8` | none | none | 7e8c0@7ebed | Copy six owner words into each 0x1b-byte entry of the table at 0x1a5c2e. |
| `0x54804` | EAX:ef_addr32 | none | 581b0@58327 | Apply a part-update message to the object. |
| `0x54f4c` | EAX:ef_addr32 | none | 581b0@582d0 | Apply a unit-state message when it concerns the local player or is open. |
| `0x55aac` | EAX:ef_addr32 | none | 87db8@87e71,883cc@88423 | Send the full state of a record (command 0x0e). |
| `0x56444` | AX:int16_t,DX:uint16_t,BX:uint16_t | none | 172b4@17344,74970@74d7b,74fb4@75296 | Send the unit's twelve option bytes and flag bits (command 0x11). |

## game_helpers19 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x566b4` | EAX:ef_addr32 | none | 581b0@583a4 | Load the parameter block from a message and apply it for the local player. |
| `0x572b4` | EAX:ef_addr32 | none | 581b0@5855f | Forward a command message as packet 0x48/0x49/0x47 plus a closing 0x4a. |
| `0x573d0` | AX:int16_t,DL:uint8_t | none | 90ac4@90c0e,90c6c@90f36 | Send entry idx of the 48-byte table at 0x211af0 (command 0x3f). |

## game_helpers20 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x81354` | EAX:ef_addr32 | EAX:ef_addr32 | 8c49c@8c50b,8fcec@8fd50 | Find the 8-byte table entry whose name equals key; return its payload address or 0. |
| `0x821f8` | AX:uint16_t,DX:int16_t | AL:uint8_t | 426a4@42758 | Index of the last 16-bit slot of row b (17 bytes per row) equal to a. |
| `0x831a4` | EAX:ef_addr32,EDX:int32_t,EBX:int32_t,ECX:int32_t | none | 83a40@83aac | Store a vector, halved until every component fits in 16 bits. |
| `0x85c90` | AX:int16_t | EAX:uint32_t | 2a1fc@2a27b,86110@86188,86968@86a06 | Address of row `index` (6 bytes) in the selected unit's page table, or the default table. |
| `0x887d0` | none | none | 888cc@889e3 | Flag the first kind-0x1f/0x20 record of the ring at 0x20f160 that passes the hit test. |

## game_helpers21 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x89bf0` | none | AL:uint8_t | 89ac0@89af4 | True when a random value modulo the configured period is within the tick budget. |
| `0x8b988` | EAX:int32_t,EDX:int32_t | none | 8bb18@8bb44 | Draw every effect of the ring at 0x12db5c that is close to (x, y). |
| `0x8ba30` | EAX:ef_addr32 | none | 8ba98@8babf | Allocate the sprite for an effect and link it at the head of the ring. |
| `0x8bb18` | none | none | 3dd30@3e1da | Draw the effects around the camera position. |
| `0x8c930` | AX:int16_t | EAX:uint32_t | 8c49c@8c919 | Select the page slot for a page id, or 0 when the page table lacks it. |
| `0x8e5f0` | EAX:ef_addr32 | none | 8e664@8e909 | Draw a record: low records use the primary routine, deeper ones the offset one. |
| `0x8f9e8` | none | none | 3ea0c@3ee32 | Install the table of nine handler addresses at 0x158f4c. |

## game_helpers22 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8fa84` | none | none | 3dd30@3de15 | Clear ten 24-byte menu entries and load their string handles. |
| `0x8fd70` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32 | none | 8fe58@8fe71 | Build a path from the default directory and three parts. |
| `0x97464` | none | none | 3dd30@3e14f | Walk the 0x4e-byte chain of records from 0x12dc74 and draw those of depth >= 2. |
| `0x97a4c` | none | none | 3dd30@3de0b | Load two string handles and reset the chain head. |

## game_helpers23 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x2a1c4` | EAX:ef_addr32 | none | 2541c@25457 | Re-evaluate a record's state byte and refresh the object. |
| `0x2af78` | AX:int16_t,DL:uint8_t | EAX:uint32_t | 8557c@855ad | Sum the weights of the six linked units of an owner. |
| `0x3bc60` | EAX:uint32_t,EDX:int32_t,EBX:int32_t | none | 3a3a8@3b038 | Draw a small cross of arm length 3 at (x, y). |
| `0x3bc90` | AX:int16_t,EDX:int32_t,EBX:int32_t | none | 3bcc0@3bd18 | Draw a small box around (x, y); half-size 2 in mode 3, otherwise 1. |
| `0x4d1ec` | EAX:int32_t,EDX:int32_t,EBX:uint32_t | none | 441fc@44368 | Blit the cursor image for the current screen (when the page matches key). |

## tiny_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x106a8` | none | none | none direct | Empty stub (single ret). |
| `0x151c4` | none | none | 3ce30@3ce4f,3d950@3d9c3,3db9c@3dbd4 | Empty stub (single ret). |
| `0x392fc` | none | none | db114@db157 | Empty stub (single ret). |
| `0x3930c` | none | none | db1c4@db207 | Empty stub (single ret). |
| `0x54b9c` | none | none | 37eac@37fd4 | Empty stub (single ret). |
| `0x68a28` | none | none | 39254@392e6 | Empty stub (single ret). |
| `0x76944` | none | none | 76704@767e6 | Empty stub (single ret). |
| `0x80d88` | none | none | 3cfc0@3d1ca | Empty stub (single ret). |
| `0x81864` | none | none | 3dd30@3e38e | Empty stub (single ret). |
| `0x866d4` | none | none | 87fe4@88038 | Empty stub (single ret). |
| `0x9be0c` | none | none | ee43c@ee463 | Empty stub (single ret). |
| `0xe7a30` | none | none | 581b0@581d1,581b0@5862c,581b0@58651 | Empty stub (single ret). |
| `0xeb259` | none | none | eb25a@eb25d,eb25a@eb263,eb272@eb275 | Empty stub (single ret). |
| `0xfc8c0` | none | none | c4167@c41a7 | Empty stub (single ret). |
| `0xfd162` | none | none | none direct | Empty stub (single ret). |
| `0x10483c` | none | none | 104200@1042d9,104200@104341,104200@104416 | Empty stub (single ret). |
| `0xd7760` | none | none | 81448@8147e,81448@81565,81448@8158a | Empty stub (single ret). |
| `0x9d770` | none | none | 9e0c4@9e285,9f940@9faf0,9fc00@9fdc5 | Empty stub (single ret). |
| `0x54008` | none | none | 3ea0c@3eecf | Tail jump to the empty stub 0x106a8. |
| `0x9d774` | [ESP+4]:uint32_t,[ESP+8]:uint32_t | none | a0a2c@a0a6d | Empty stub that pops two stack arguments (ret 8). |
| `0x151c8` | none | none | none direct |  |
| `0x17474` | AX:int16_t | EAX:uint32_t | 15b7c@15d6e,161d4@1626e,16588@165ed |  |
| `0x39310` | AX:int16_t | none | 3dd30@3e17c,3dd30@3e188 |  |
| `0x3ce2c` | none | AL:uint8_t | a9bfc@a9ca2 |  |
| `0x3efa0` | none | AL:uint8_t | f3f84@f40bd |  |
| `0x4904c` | none | AL:uint8_t | 1cde8@1ce2f,1cde8@1cef7 |  |
| `0x4b188` | none | AL:uint8_t | 4a210@4a990 |  |
| `0x54efc` | none | none | 581b0@583f2 |  |
| `0x57134` | EAX:ef_addr32 | none | 581b0@58523 | Copy the byte at +1 of the record to 0x1a3649. |
| `0x7eebc` | none | EAX:uint32_t | 3dd30@3dd88 |  |
| `0x810ec` | none | EAX:uint32_t | 827b4@827b7 |  |
| `0x85cf8` | none | none | 3ea0c@3ee3c |  |
| `0x8bde0` | EAX:uint32_t | none | 888cc@889eb,888cc@88a70 |  |
| `0x8f3c0` | none | EAX:uint32_t | 3f7fc@3f91c |  |
| `0xcc8e0` | none | AL:uint8_t | bcf44@bcff2 | Returns the ESC scan code. |
| `0xce08c` | none | EAX:uint32_t | 8b39c@8b481,ac854@ac8aa |  |
| `0xcf94a` | EAX:int32_t | EAX:uint32_t | 15b7c@15c23,15b7c@15c48,15b7c@15cf2 | Absolute value. |
| `0xd76f4` | EAX:ef_addr32,DX:uint16_t | none | 7eec4@7eee5,7ef14@7f42e,7ef14@7f48e |  |
| `0xd76fc` | EAX:ef_addr32 | AX:uint16_t | 7ef14@7f2c3,7ef14@7f369,7ef14@7f416 |  |
| `0xd7714` | EAX:ef_addr32,DX:uint16_t | none | 7ef14@7f3fd,7ef14@7f46e,7f850@7f901 |  |
| `0xd771c` | EAX:ef_addr32 | EAX:int32_t | 7ef14@7f264,7ef14@7f282,7ef14@7f3e8 | High word of the dword at +0xf, sign-extended. |
| `0xd7730` | EAX:ef_addr32 | EAX:uint32_t | 7fc34@7fc48,7fc34@7fc6f,7fc34@7fc92 |  |
| `0xd7754` | EAX:ef_addr32 | EAX:uint32_t | 81394@813e0,81448@8152f,81448@8154c |  |
| `0xd7758` | EAX:ef_addr32,EDX:uint32_t | none | 81230@812e3,81448@814c8 |  |
| `0xd775c` | EAX:ef_addr32 | EAX:uint32_t | 81448@814dc,81448@81512 |  |
| `0xd7800` | EAX:ef_addr32,EDX:uint32_t | none | 7fcdc@7fe05 |  |
| `0xe2b54` | none | EAX:uint32_t | c32dc@c32e0 |  |
| `0xe2fd5` | none | EAX:uint32_t | e2fdb@e2fdc |  |
| `0xebd30` | none | EAX:uint32_t | ebe6d@ebe70,f4db8@f4ddf,f4db8@f4df6 |  |
| `0xebd36` | none | EAX:uint32_t | ebe90@ebe93 |  |
| `0xfa89a` | none | EAX:uint32_t | bc471@bc479 |  |
| `0xfddb0` | EAX:int32_t,EDX:int32_t,EBX:int32_t | EAX:int32_t | ce5f8@ce74e,ce5f8@ce766,ce5f8@ce77e | (a * b) / c with a 64-bit intermediate; EDX is preserved. |
| `0x109e6c` | none | EAX:uint32_t | none direct |  |
| `0x10d9e0` | none | EAX:uint32_t | e9ac0@e9b0d |  |
| `0x10d1f4` | EAX:ef_addr32 | EAX:uint32_t | e8651@e867d |  |

## runtime_helpers10 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa63c0` | EAX:ef_addr32 | none | a66cc@a66e3,e1958@e1b58 | Update a widget's position words from the colour nibble. |
| `0xa73cc` | EAX:ef_addr32,EDX:uint32_t,BL:uint8_t | EAX:ef_addr32 | 33e78@33eb0,33e78@33ec5,34a98@34ac4 | Set bit 6 of the located widget's flags and refresh it. |
| `0xa74bc` | EAX:ef_addr32,DX:int16_t,BX:int16_t | none | c768@c7b7,c768@c7ee,c768@c829 | Move a widget (negative coordinates clamp to 0) and relayout it. |
| `0xa7588` | EAX:ef_addr32 | none | 40dc@4115,40dc@4163,40dc@4171 | Show a widget. |
| `0xa788c` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | a59ac@a59b8,a78bc@a78d9 | Find the child control of a widget whose name equals key. |
| `0xa8060` | none | none | 283c8@2840f | Write the fill byte to every live entry of the table, clear dead ones. |
| `0xa9d0c` | none | EAX:uint32_t | a9e70@aa0f9 | Select the list node at position count (1-based) and return its payload. |

## runtime_helpers11 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xac9a0` | EAX:ef_addr32 | none | 34298@3437a,34390@343d5,343dc@34408 | Redraw the rectangle of a widget and remember it as the active one. |
| `0xb0554` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | EAX:uint32_t | b059c@b0637 | True when the list holds a node with the same two keys and the hash of b. |
| `0xb1840` | EAX:ef_addr32 | none | b188c@b189d | Free the four blocks of a record, then the record. |
| `0xb19a0` | EAX:ef_addr32 | none | b19e4@b19f5,ca838@ca863 | Free a record with an owned block, a nested block and its own storage. |
| `0xb4e74` | AX:int16_t,DL:uint8_t,BL:uint8_t,CX:uint16_t,[ESP+4]:uint32_t,[ESP+8]:ef_addr32 | none | b5fe8@b6833,b5fe8@b685b,b69e4@b7ce8 | Fill the 9-byte entry idx of the table at +0x4a of an object. |
| `0xb55f8` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,CL:uint8_t | EAX:ef_addr32 | b97a4@b97c0,b9aa4@b9abc,f8b14@f8b8f | Find the list node matching two key pairs and a kind byte. |

## runtime_helpers12 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa992c` | none | EAX:uint32_t | a9e70@aa008 | Rotate the saved list state into place; returns the global at 0x146e18. |
| `0xac7f0` | none | none | ac854@ac96f,ac980@ac99b | Clamp the cursor (0x13725e, 0x137260) into the window rectangle. |
| `0xb0238` | EAX:ef_addr32,DX:int16_t | none | b0f5c@b15f3 | Copy per-column values into every matching node of the list. |
| `0xb18b8` | EAX:ef_addr32,EDX:ef_addr32 | none | b1b10@b1b39 | Free every node of the list (and their three owned blocks), then clear both heads. |
| `0xb1918` | EAX:ef_addr32 | none | b1974@b1985 | Free the owned blocks of a record, then the record. |
| `0xb4c20` | EAX:ef_addr32,DL:uint8_t,EBX:ef_addr32,ECX:uint32_t | EAX:ef_addr32 | c0b04@c0c99,c0b04@c0cf5,c0b04@c0d8c | Find a kind-0x31 child (following the first link) that passes the check. |
| `0xb4c7c` | EAX:ef_addr32,DL:uint8_t,EBX:ef_addr32,ECX:uint32_t | EAX:ef_addr32 | c0b04@c0cbd,c0b04@c0d38,c0b04@c0db3 | Find a kind-0x31 sibling (following the second link) that passes the check. |
| `0xb56d0` | AX:int16_t,DX:int16_t,EBX:ef_addr32 | EAX:int32_t | b5800@b5831 | Frame index scaled by the per-row count: (column * 60 + row) / divisor. |
| `0xbd508` | EAX:ef_addr32 | EAX:uint32_t | bd5a8@bd5d6 | Count the 10-byte entries of the record that are special and non-empty. |
| `0xbd554` | EAX:ef_addr32 | EAX:uint32_t | bd5a8@bd5df | Count the special entries of a kind-0x15 record, 1 for other kinds. |
| `0xbe438` | EAX:uint32_t,EDX:uint32_t,EBX:ef_addr32 | EAX:ef_addr32 | be480@be526,c1438@c1781 | Find the kind-2 node of the list whose target matches the two keys and is valid. |

## runtime_helpers13 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb4cdc` | EAX:ef_addr32,DL:uint8_t,EBX:ef_addr32,ECX:uint32_t | EAX:ef_addr32 | 94fc@971f,c1210@c13b5,c1210@c13d7 | As find_child_b4c20 with the 0xcc22c check. |
| `0xb4d38` | EAX:ef_addr32,DL:uint8_t,EBX:ef_addr32,ECX:uint32_t | EAX:ef_addr32 | c1210@c13fb,c1210@c141d | As find_sibling_b4c7c with the 0xcc22c check. |
| `0xbd73c` | EAX:ef_addr32,DX:int16_t | EAX:uint32_t | bdbf8@bdcaf,f8b14@f8bfa | True when some window of (width) consecutive 9-byte entries are all unused. |
| `0xbd86c` | EAX:ef_addr32 | EAX:uint32_t | bdbf8@bdc78,bdbf8@bdcc2,bdbf8@bdd11 | True when a non-kind-2 record has a filled entry (scanned from the end) that passes the check. |
| `0xbd8c8` | AL:uint8_t,DX:int16_t,BL:uint8_t,ECX:ef_addr32,[ESP+4]:ef_addr32 | EAX:uint32_t | bd92c@bda20 | True when the list's index-th node matching the criteria exists. |
| `0xbecc0` | EAX:ef_addr32 | EAX:uint32_t | becf8@bed07,fac78@facb1,fac78@facd1 | Count the 21-byte entries of the object that pass the check. |

## runtime_helpers14 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xca1c8` | EAX:ef_addr32,EDX:uint32_t,BX:uint16_t,CX:uint16_t,[ESP+4]:uint32_t,[ESP+8]:uint8_t,[ESP+12]:uint8_t | none | bc38@bc8f | Store position words and optionally the two link pointers in a record. |
| `0xca838` | EAX:ef_addr32,EDX:ef_addr32 | none | b0f5c@b115d,b0f5c@b1516 | Unlink and free every kind-2 node of the list. |
| `0xcabb0` | EAX:ef_addr32,EDX:ef_addr32 | none | 49228@494d5 | Translate 400 bytes through the lookup routine at 0xcbef4. |
| `0xcabe8` | EAX:ef_addr32,EDX:ef_addr32 | none | b0f5c@b15d5 | Copy fields from the first node of list A into every matching node of list B. |
| `0xcb688` | EAX:ef_addr32 | none | b0f5c@b136a | Count down the per-slot timers of every node and reload expired ones. |

## runtime_helpers15 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcbc54` | EAX:ef_addr32,EDX:ef_addr32 | none | 49228@49417,b0df4@b0e82,b0f5c@b1149 | Look up each node's tile byte in a 40-wide grid from its position. |
| `0xcc468` | AL:uint8_t,EDX:ef_addr32 | EAX:uint32_t | befb8@bf2cc,c0b04@c0b86,ccdbc@cd0e0 | True when the first node of this kind has a negative flag byte. |
| `0xcc840` | EAX:ef_addr32 | EAX:uint32_t | afc5c@afc90,cc90c@cc944 | Count the 21-byte entries that are free (0xff) or reserved (0xfe) for other kinds. |
| `0xcd7a0` | none | none | e4264@e42ae | Free the blocks in table slots 101..n and reset the count. |
| `0xcd81c` | [ESP+4]:int16_t | EAX:uint32_t | none direct | Forward the stack word to 0xcd7f0. |

## runtime_helpers16 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd70a0` | AL:uint8_t | EAX:uint32_t | 7fcdc@7fdd4,8018c@801b1,801e4@80284 | Claim a device slot for a priority, or evict the lowest-priority one. |
| `0xd7704` | EAX:ef_addr32,DX:uint16_t | EAX:uint32_t | 7ef14@7f5fe,7ef14@7f674 | Add to the word at +0xf and return the new value. |
| `0xd77dc` | AX:int16_t,DX:int16_t | EAX:int32_t | 7fcdc@7fdbb | Scale -(a << 6) by the magnitude of b plus one, biased by 0x40. |
| `0xd7bf0` | none | none | d7c40@d7cad,d7cb8@d7e87 | Shift four window words by ten. |

## game_helpers24 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdc21c` | [ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t | none | 1e480@1e4d8,3cfc0@3d26b,63d5c@63ec5 | Store two 16.16 coordinates (with a half-unit bias) from stack arguments. |
| `0xdc7e4` | EAX:ef_addr32,DX:int16_t,BX:int16_t | none | 1f0fc@1f2c3,1f0fc@1f2df,1f0fc@1f302 | Rotate (b, a) by the sine/cosine pair at 0x425f90 and store two words. |
| `0xdc890` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | dc91c@dc98f,dcaa4@dcb2a | Halve three dwords until all fit in a signed 16-bit range. |
| `0xdced0` | EAX:ef_addr32 | EAX:uint32_t | a82f0@a84d1 | Pixel width of a string in the font with 4-pixel blanks. |
| `0xdd10c` | EAX:ef_addr32 | EAX:uint32_t | a82f0@a84df | Pixel width of a string in the font with 5-pixel blanks. |
| `0xdd210` | EAX:ef_addr32 | EAX:uint32_t | a82f0@a84d8 | Pixel width of a string in the font with 6-pixel blanks and 2-pixel gaps. |
| `0xdd650` | EAX:ef_addr32 | EAX:uint32_t | 27d18@27d9a,27d18@27de7,27fec@28192 | Pixel width of a string; blanks are tested before masking. |
| `0xdd988` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32,[ESP+4]:ef_addr32,[ESP+8]:ef_addr32,[ESP+12]:ef_addr32,[ESP+16]:ef_addr32 | none | f2ac0@f2b0e | Copy the saved view state at 0x15ee80..0x15ee90 to eight output pointers. |
| `0xdec4c` | EAX:int32_t,EDX:int32_t | EAX:uint32_t | 75f00@75fbf,75f00@75fe3,75f00@76005 | Read a byte from the clipped 8-bit plane, or -1 outside the window. |

## game_helpers25 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdf21b` | EAX:ef_addr32 | EAX:int32_t | none direct | Read the next byte from a stream, or -1 (setting flag 2) at its end. |
| `0xe17b8` | EAX:uint32_t,EDX:int32_t,EBX:int32_t | AL:uint8_t | e17f8@e1891,e17f8@e18ab | True when a column of 256-byte rows is clear for the given count. |
| `0xe3040` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | e30e0@e3108 | Draw a 8x6 glyph, each set bit writes the colour byte. |
| `0xe30e0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:ef_addr32 | none | 3d308@3d34b,3d308@3d35b,3dd30@3e73d | Draw a string with 9-pixel glyph spacing. |
| `0xe3908` | EAX:uint32_t | EAX:uint32_t | e3ad0@e3afd | Search a word table backwards for a value, using slot 0 as the sentinel. |

## runtime_helpers17 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe3c6c` | [ESP+4]:uint32_t | none | none direct | Resolve a page slot from the 0x190a44 chain and store it in 0x159134. |
| `0xe6120` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 109070@109072 | Store five scalar settings. |
| `0xe7c50` | AX:int16_t,DX:uint16_t,BX:int16_t | none | 68a2c@68a58,68a60@68ace,68a60@68b3a | Store a word into the 0x190c3c page table entry. |
| `0xe7c90` | AX:int16_t,DX:int16_t | AX:uint16_t | 68e84@68e9e | Read a word from the 0x190c3c page table entry, or 0. |
| `0xeb374` | EAX:ef_addr32,EDX:ef_addr32,EBX:int32_t | EAX:int32_t | 9c858@9c88a,103f04@104091,103f04@1040b9 | Case-insensitive compare of up to n bytes. |
| `0xeb4dc` | EAX:ef_addr32,EDX:ef_addr32,EBX:int32_t | EAX:ef_addr32 | eb520@eb684 | Binary search 12-byte entries sorted by their first signed dword. |

## game_helpers26 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x68e84` | EAX:uint32_t | EAX:uint32_t | 69740@69889,69bdc@69cfc | Assign an object bit test to each of 20 page words; returns the number set. |
| `0x7a5f0` | EAX:uint32_t | none | 7a738@7a7a9 | Append a word to the recorder table from the stream or a 73-byte record. |
| `0xaae6c` | none | none | aaee8@aaf47 | Set flag 0x137206 when the default key letter matches a slot in mode 5. |

## runtime_helpers18 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xaafe8` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | EAX:uint32_t | ab060@ab07e,ab324@ab395,ab324@ab418 | Find a node in the nested group lists; report its index and group index. |
| `0xab840` | EAX:uint32_t | EAX:uint32_t | 371b0@371f2,a7510@a7536,e1e6c@e1eee | Bump-allocate a zeroed block from the arena; 0 when it does not fit. |

## game_helpers27 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x65410` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,CL:uint8_t,[ESP+4]:uint16_t,[ESP+8]:uint8_t,[ESP+12]:uint8_t | none | 65030@65276,65030@652a9,65030@652f4 | Move a node from the free list to the head of the active list and fill it. |
| `0x6583c` | none | none | 654b8@6555f | Advance the vertical countdown; step to the next frame when it expires. |
| `0x658cc` | none | none | 654b8@6555a | Advance the horizontal countdown; step to the next frame when it expires. |
| `0x7aa3c` | none | none | 7ab00@7abc4 | Pick two random table values for the current stream slot. |
| `0x82168` | AX:int16_t,DX:int16_t,BL:uint8_t | none | 4a210@4a818,4aa4c@4ae37 | Claim the first free (0xff) slot in a row of eight and link both tables. |
| `0x833a0` | EAX:int32_t,EDX:int32_t,EBX:int32_t | EAX:int32_t | 83424@834c3,83424@834d0,834e8@83527 | Compute a*b/c, saturating to +/-0x7fffffff-ish when the product overflows. |

## game_helpers28 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x56ba8` | none | AL:uint8_t | 56bdc@56c2c | True when every ready word in table 0x1a5dd0 is nonzero. |
| `0x56cbc` | EAX:ef_addr32 | none | 581b0@585f3 | Mark the ready word of the unit in a message at +1 (table 0x1a5dd0). |
| `0x56efc` | EAX:ef_addr32 | none | 581b0@584cd | Mark the ready word of the unit in a message at +1 (table 0x1a5dc0). |
| `0x56f30` | EAX:ef_addr32 | none | 581b0@584dc | Forward a message to 0x8f22c when its unit has a nonzero slot word. |
| `0x56ffc` | none | AL:uint8_t | 57030@5707d | True when every ready word in table 0x1a5de0 is nonzero. |
| `0x570fc` | EAX:ef_addr32 | none | 581b0@58514 | Mark the ready word indexed by the message byte at +1 (table 0x1a5de0). |
| `0x577e8` | EAX:ef_addr32 | none | 581b0@58473 | Count a local-owner message, then hand the message on. |
| `0x6319c` | AX:uint16_t | none | 62edc@6317c | Append a word to the 100-entry history ring. |
| `0x631fc` | AX:uint16_t | AL:uint8_t | 66548@6673b | True when a value is present in the history ring. |
| `0x77748` | none | none | 8c49c@8c56d | Count a pending frame while the marker word is -1. |
| `0x789d0` | AL:uint8_t,EDX:ef_addr32 | none | 56e94@56ec2 | Store a 122-byte name entry for an owner slot, skipping the local owner. |
| `0x8b5f0` | EAX:ef_addr32 | none | 8ba98@8bac8 | Latch a word pointer (+1) and clamp its mode to 1 or 2. |
| `0xa45b3` | none | none | 6c1c@6cf3,6c1c@6d15,6c1c@6d2f | Step a 32-bit generator held in two words; reseed when it reaches zero. |
| `0xa4640` | AX:uint16_t | EAX:uint32_t | e52a4@e533f | Find a word in the 0x41eda8 id table; returns its index, or a cleared low byte. |
| `0xa4e6c` | none | none | ac2e4@ac3dc,ac408@ac43d | Clear the 42-entry id table. |
| `0xa9cd0` | none | none | a9e70@a9ff3 | Track a mouse request and refresh the cursor when its value changed. |
| `0xac7a8` | EAX:ef_addr32 | none | 4738@4840,4b54@4b76,4c50@4cea | Set the pending flag and copy a name into the message buffer. |
| `0xb3ae0` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | b3b1c@b3b4a,b3b1c@b3b55,b3b1c@b3b67 | Table-driven ratio of two unsigned values (a/b or complement). |
| `0xc58cc` | [ESP+4]:uint32_t,[ESP+8]:uint32_t | none | none direct | Reset three clip bounds; both stack arguments are ignored. |
| `0xdc7bc` | EAX:uint32_t | none | 1f0fc@1f2a2,1f0fc@1f609,2a4c8@2a5e4 | Load a sine/cosine word pair for an angle (8 angle units per table step). |

## game_helpers29 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x92f0` | none | none | bfcc@c275,c630@c757,c868@c879 | Clear five 8-entry word tables. |
| `0x56ccc` | none | none | 601c@6035 | Send the stored chat line as packet 0x2f when chat is allowed. |
| `0x73924` | AX:int16_t,DX:int16_t | none | 46454@46761 | Release a unit's current entry and clear its pending slot id if it matches. |
| `0x81868` | none | none | 3cfc0@3d2eb | Fill 50 word pairs with generator output. |
| `0x90c2c` | AX:int16_t | none | 90c6c@90e9b | Raise a 48-byte record's gauge by 0x41a, capped at 0x1400. |
| `0xa0b98` | AX:int16_t,DX:int16_t,BX:int16_t | none | 9e7bc@9f3c4,9e7bc@9f3de,9e7bc@9f3f6 | Copy one byte between two page rows (29-byte stride, offset 0x14). |
| `0xa4b08` | EAX:ef_addr32 | none | b5d0@b66f,b5d0@b68c,bfcc@c20f | Refresh the list for an object that is flagged and not owned. |
| `0xdf2b0` | none | EAX:int32_t | 33888@338be | Divide 115200 by the table entry for the current rate index. |
| `0xe3b58` | none | none | 97dcc@97f4b | Release the saved handle, if any. |
| `0xf2141` | EAX:int32_t,EDX:ef_addr32 | EAX:uint32_t | f2183@f2206,f2183@f2239 | Advance a window cursor by count; wrap and report 1 when out of range. |
| `0xf261c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:ef_addr32 | none | a8578@a85c9 | Draw a string with 4-pixel glyph spacing. |
| `0xf29ec` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:ef_addr32 | none | a9130@a9374 | Draw a string with 6-pixel spacing, glyphs offset from the blank. |
| `0xf41a8` | none | none | f41dc@f41f7 | With the flag set, remember the request value and refresh the cursor. |
| `0xf41c4` | none | none | a9d48@a9e20,f3f84@f3fa2 | With the flag set, apply the saved request value. |
| `0xf5e6c` | AX:uint16_t,DX:uint16_t,BX:uint16_t,CX:uint16_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | f5484@f5688 | Latch six words and install the 0xf5eac handler. |

## game_helpers30 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x6330` | EAX:ef_addr32 | none | 601c@62e0 | Count down a timer for an active object; call 0x56a4c and rewind on underflow. |
| `0x47f48` | none | none | 48064@4807c | Decrease the countdown word, by a third of the frame time when flagged. |
| `0x49cd4` | EAX:ef_addr32 | none | 49bdc@49c4c | Copy the row words selected by an object's count into the 0x1a5280 list. |
| `0x8ccec` | AX:int16_t | none | 8c49c@8c800 | Reset a unit's link slots to 0xff. |
| `0x9d114` | EAX:uint32_t | EAX:ef_addr32 | f80@114f,2ff44@2ffb8,30214@30291 | Allocate and initialise a 0x56c-byte stream record bound to an owner. |
| `0xa4e14` | none | none | ac2e4@ac3e1 | Rebuild the id table from the objects that are flagged and not kind 1. |
| `0xa7204` | EAX:ef_addr32 | none | a60a8@a618c | Walk a chain of children and tidy each one that is not a basic kind. |
| `0xc3a74` | none | none | cdb20@cdbd0 | Choose the drawing handlers from two mode flags. |
| `0xf8988` | EAX:ef_addr32 | EAX:uint32_t | f8b14@f8bd8,f8b14@f8c22,f8b14@f8c71 | Find the last entry that is set and fails its check, clear it; report if found. |
| `0xf89e0` | EAX:ef_addr32 | EAX:uint32_t | f88a0@f88d0 | Count matching entries of a kind-0x15 list; 1 for other kinds. |
| `0xf8a30` | EAX:ef_addr32 | EAX:uint32_t | f88a0@f88c7 | Count the filled entries whose code passes the 0xbd4e0 test. |
| `0xf8acc` | EAX:ef_addr32 | EAX:uint32_t | f8b14@f8bc3,f8b14@f8c0d,f8b14@f8c5c | Decrement the last set entry that accepts it; report if one did. |
| `0xf8e14` | EAX:ef_addr32 | EAX:uint32_t | f8e44@f8e9b | Count the entries whose first word is nonzero. |
| `0xfabf0` | EAX:ef_addr32 | EAX:int32_t | fbad8@fbcd1 | 1 when the object has at least one active entry whose code passes the set test. |

## game_helpers31 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa00` | EAX:uint32_t | AL:uint8_t | a60@a68,a60@a81 | Issue request 0x2503 and report whether the returned code byte is 0xcf. |
| `0x15af0` | EAX:uint32_t,DL:uint8_t | none | 2ced4@2d42e,7e8c0@7e994 | Drive the per-owner action chain for a player's slot. |
| `0x1851c` | none | none | 72944@72ce2,72944@72d3f | Fire the one-shot action when the idle conditions hold. |
| `0x2546c` | none | none | 239ac@23a6b,255f0@256ac,255f0@256e9 | Reset or advance the current object's mode after a transition test. |
| `0x2a970` | none | none | 3dd30@3e250 | Pop one entry of the 6-byte stack at 0x125262 into the current triple. |
| `0x3711c` | none | none | none direct | Reset the list view and load the heading string for the current page. |
| `0x3a2d0` | none | none | 3a3a8@3a499 | Build a 36-entry table of scaled sine/cosine pairs (radius 60). |
| `0x3cd9c` | none | none | 3cfc0@3d2e6 | Convert the 11-bit fixed-point cursor and probe the cell under it. |
| `0x47fbc` | none | none | 48064@4808a | Decrease the second countdown word unless device 0xb is in state 4. |
| `0x54210` | none | none | 54110@54132 | Cycle a frame counter every other tick and pass the frame to 0x8f4c. |
| `0x5437c` | none | none | 54110@54141 | Update every record of the 171-byte list: arm ready ones and refresh active ones. |
| `0xd628` | none | none | none direct | Emit the pending message and, when idle, open the dialog and advance the page. |

## game_helpers32 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x56208` | none | none | 58028@58047 | Clear ten 55-byte record counters and the 80x8 ready-word grid. |
| `0x5b288` | AL:uint8_t,DX:uint16_t,BX:uint16_t,CL:uint8_t | none | 5fc80@5fd1e | Run 0x5b2f0 with temporary values for four globals, then restore them. |
| `0x5b9a8` | none | none | 5ba10@5ba28 | Build a 10-entry table of scaled sine/cosine pairs (radius 12). |
| `0x64498` | none | none | 646d4@6474b | Build a 36-entry table of scaled sine/cosine pairs (radius 50, truncating). |
| `0x748f4` | AX:int16_t,EDX:uint32_t,EBX:uint32_t,CL:uint8_t | none | 777f4@77901,777f4@7792f,8cd78@8cee9 | Send unit parameters when a remote unit is selected, otherwise defer to 0x74970. |
| `0x8cae8` | EAX:ef_addr32 | none | 8cb60@8cb83 | Fill an object's 20 slot entries from the defaults table and reset its cursor. |
| `0x9075c` | EAX:uint32_t | EAX:uint32_t | 6aa70@6aaa2,6aa70@6aaba | Find the next free object id after a start id, wrapping at the table end. |
| `0x948c4` | AX:int16_t | none | 95e58@96874,95e58@96af7 | Pair the current unit with a target unit, or reset the target state. |
| `0x97a94` | AX:int16_t | none | 97b18@97b51 | Apply a 78-byte camera record through 0xa0ea8 and 0x9e0c4. |
| `0x97ca8` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32,[ESP+4]:ef_addr32,[ESP+8]:ef_addr32 | none | 97064@970f1,97064@971c2 | Copy the current 78-byte camera record to six output pointers. |

## game_helpers33 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x57714` | none | none | 3dd30@3de10 | Clear the eight name slots, then store the pending name (upper-cased) in its slot. |
| `0x62454` | none | none | 601c@6076 | Refresh the flight timers and run the per-frame entry updates. |
| `0x7775c` | none | none | 49228@49b48 | Collect the level-0x20 objects into a new table when the marker word is -1. |
| `0x97f90` | none | none | 8b39c@8b4ba | Pick the table for the detected input device and install the handlers. |
| `0x9d170` | EAX:ef_addr32 | AL:uint8_t | 9d1e8@9d1fe,9d1e8@9d217,9d1e8@9d245 | Read the next byte of a buffered stream, skipping CRs; 0 at end of data. |

## game_helpers34 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9fe1c` | AX:int16_t | none | 9fdd4@9fe10 | Unlink an id from the singly linked list of 78-byte records. |
| `0xa0ebc` | EAX:ef_addr32 | none | 9e7bc@9f0cc | Fill a page row cell from a 9-word descriptor and mark it present. |
| `0xa2698` | AX:int16_t,DX:int16_t | none | 9e7bc@9f4b0 | Apply a page cell to a slot unless the slot is locked, and refresh it. |
| `0xa36b0` | none | none | a410c@a39de | Accumulate clamped timing offsets of the listed entries. |
| `0xa4718` | AX:uint16_t | none | a4a94@a4abe,a4a94@a4aef | Remove an object id from the id table and clear its flag 4. |
| `0xa479c` | AX:uint16_t | none | a499c@a49cb | Add a flagged object id to the first free slot of the id table. |
| `0xa4820` | AX:uint16_t | none | a4a20@a4a4b | Push a flagged object id to the front of the id table. |

## game_helpers35 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x2c17c` | AX:int16_t | AL:uint8_t | 2c6dc@2c73b | True when an owner has a linked unit whose own link is on another team. |
| `0x4e6bc` | none | none | 4dcd8@4decc | Drop the oldest queued entry by shifting the 24-entry queue down. |
| `0x56a4c` | none | none | 6330@6371,9114@91f3,9114@9244 | Send packet 0x12 with the local owner's name and counters. |
| `0x79128` | none | none | 3dd30@3e71c | Per-frame HUD pass: latch the screen rectangle and run the overlay updates. |
| `0x80a18` | none | none | 80d8c@80de7 | Reset the 256 name entries, register them in the page table and clear 16 slots. |
| `0x878c8` | EAX:ef_addr32 | none | 56954@5696c,879c4@879d4,880c8@880e9 | Remove an object from the active list and put it on the free list. |
| `0x90880` | EAX:ef_addr32,EDX:ef_addr32 | none | 8be3c@8be4b | Keep the nearest candidate in an object's +0x1c link (Chebyshev-like distance). |

## game_helpers36 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x92070` | none | none | 942cc@942f4,94370@94398,94484@944a9 | Classify the signed angle word into a -2..1 step (kept when out of range). |
| `0x9dfc0` | AX:int16_t,DX:uint16_t,BX:uint16_t,CX:uint16_t | none | 161d4@164d8,1e28c@1e391,95e58@96311 | Select a page and its layout; refresh each cell when the layout is active. |
| `0xa1424` | AX:int16_t,DX:int16_t,EBX:uint32_t,CL:uint8_t | none | 9e7bc@9eab0 | Stamp a page cell with a scaled duration and a command code. |
| `0xa905c` | EAX:ef_addr32 | none | a9e70@a9fc4,ab47c@ab5e9 | Latch the target of an incoming order message. |
| `0xab0dc` | EAX:uint32_t | none | ab324@ab45e | Visit every kind-0xe node of every group and notify its three links. |

## game_helpers37 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdccd8` | AL:uint8_t,EDX:int32_t,BX:uint16_t,ECX:ef_addr32 | none | a8578@a85dd | Draw a string in the 4-pixel-blank font, advancing by each glyph width. |
| `0xdd1b0` | AL:uint8_t,EDX:int32_t,BX:uint16_t,ECX:ef_addr32 | none | a8578@a8605 | Draw a string in the 5-pixel-blank font, advancing by each glyph width. |
| `0xdd460` | EAX:ef_addr32 | EAX:uint32_t | a82f0@a84ed | Width of a string in the 17-pixel-blank font minus the trailing gap. |
| `0xdd55c` | EAX:ef_addr32 | EAX:uint32_t | a82f0@a84f4 | Width of a string in the 5-pixel-blank font minus the trailing gap. |
| `0xdd728` | EAX:ef_addr32 | EAX:uint32_t | a82f0@a8458 | Width of a string in the 6-pixel-blank font minus the trailing gap. |
| `0xe7850` | none | none | e78b4@e7935 | Store the double constants used by the time conversion. |
| `0xe85f0` | EAX:ef_addr32,EDX:ef_addr32 | none | 7b7d8@7b943 | Interpolate a point back along a slope defined by the line at 0x420904. |

## game_helpers38 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x46070` | none | none | 47f9c@47f9c | Shift the nine 20-byte history records down and reset the newest slots. |
| `0x48064` | none | none | 3dd30@3e712 | Advance the countdowns and age the four 140-entry timer tables. |
| `0x48798` | AX:int16_t,DX:int16_t,BL:uint8_t | none | 46454@46daa,46454@46dc4 | Set the target mode and share the link with an enemy-owned linked unit. |
| `0x48848` | AX:int16_t,DX:int16_t,BL:uint8_t | none | 46454@46de1,46454@46dfb | Like 0x48798 with modes 6 and 7; the flag test comes first. |
| `0x54010` | none | none | 3ea0c@3edba | Reset the flight-control state words and floats. |
| `0x54410` | none | none | 49228@49b5b | Reset every unit's team word, the owner flags and the 8 counters. |
| `0x56af4` | EAX:ef_addr32 | none | 581b0@583b3 | Store an owner summary message: name, counters and flags. |
| `0x5f588` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | AL:uint8_t | 5f220@5f467 | Turn direction of three points: 1, -1 (0xff) or 0, with a collinear tiebreak. |

## game_helpers39 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x6456c` | EAX:ef_addr32 | none | 62b84@62c7c,62edc@62fa7,646d4@64ec7 | Rotate a vector by the matrix at 0x1a7c30. |
| `0x64618` | EAX:ef_addr32 | none | 625d8@62856,62b84@62c8c | Rotate a vector by the matrix at 0x1a5bc0. |
| `0x663a4` | EAX:uint32_t,DL:uint8_t | EAX:uint32_t | 65a88@65aed,65a88@65b09,65c1c@65c59 | Find the next unit after an index that matches a team mode; return its id or -1. |
| `0x69680` | EAX:ef_addr32,EDX:ef_addr32,BX:int16_t,CX:int16_t | none | 69740@69859 | Transform (x, y) by the 3x3 matrix at 0x15ffda and add the translation pair. |
| `0x758c0` | AX:uint16_t,EDX:ef_addr32,EBX:ef_addr32,CL:uint8_t,[ESP+4]:uint8_t,[ESP+8]:uint8_t | none | 76370@7642a,76370@764f2 | Blend a 256-entry RGB palette towards a target colour by factor/50. |
| `0x77534` | EAX:uint32_t | none | 46454@46795 | Trigger the linked-unit event for a unit and notify the matching display slots. |

## tiny_helpers3 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1d420` | none | none | 15af0@15b74,3cfc0@3d1c0,3dd30@3dd7c | Reset the 0x132ae0 handler pointer. |
| `0x768fc` | none | none | 58d64@58d78 | Set the 4000 limit word. |
| `0x77c28` | none | none | 77c90@77d94 | Set the 0x118 timer word. |
| `0x9854c` | none | none | 49228@49288,49228@494e8,69094@69133 | Install the 0x984f0 handler. |
| `0xa45a8` | none | none | 3dd30@3dd60,3dd30@3de1f,7a844@7a876 | Reseed the generator. |
| `0xe3c60` | none | none | c8c34@c4550,e4188@e423e | Set the 0x159134 pointer to its default. |
| `0xeb24e` | none | none | 9bd20@9bd50 | Set the 0x17d400 limit to 0x8000. |
| `0xfd371` | none | none | none direct | Set the 0x17bfea pointer to its default. |

## game_helpers40 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x6322c` | none | none | 62454@624e6 | Step the damage level down when the weakest live part dropped far enough. |
| `0x65960` | none | none | 5f60@5f75,5fd8@5fe3 | Reset the target-tracking state. |
| `0x6a82c` | AX:int16_t,DX:int16_t | AL:uint8_t | 6a8fc@6a95b,6aeb4@6af2b,6aeb4@6afa6 | True when the second unit is a valid hostile target for the first. |
| `0x76948` | none | none | 76370@765d8 | Fade the 256-entry palette towards white by the fade counter. |
| `0x76f58` | EAX:uint32_t,EDX:ef_addr32 | none | 77088@770c9 | Report the formation members and leader to the roster function 0x76b30. |

## game_helpers41 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x5488` | none | none | 5630@566e | Pick the mode value from which row/column button pair is pressed. |
| `0x16d60` | EAX:ef_addr32,DL:uint8_t,BX:int16_t | none | 15b7c@15eab,16588@168a2,16e60@16f72 | Count down the 20 three-byte slots of the given kind by the given amount. |
| `0x1c3d0` | none | none | 1cde8@1ce50 | Latch the raw input state, clearing it when a menu or pause is active. |

## game_helpers42 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x45a4` | none | none | 4738@47da,4bf8@4c2f | Read the default record and derive the chord mode from its two fields. |
| `0x4738` | none | none | none direct | Reset the menu: read the clicked mode widget, load the record and refresh. |

## game_helpers43 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x283c8` | none | none | 284ec@28553,284ec@28578 | Draw the splash buttons and probe the data file for its magic word. |
| `0x404bc` | none | none | 3dd30@3e466 | Draw seven debug counters, one per text row. |

## runtime_helpers19 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x880` | none | EAX:uint32_t | 8b39c@8b3a6 | Issue DOS service 0x350b and return the result register. |
| `0xa9c` | EAX:uint32_t,EDX:uint32_t | none | b04@b2e | Issue DOS service 0x2505 with two register values. |
| `0xc74` | AX:int16_t | none | none direct | Issue DOS service 0x3502 with a sign-extended 16-bit argument. |
| `0xcac` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | fc534@fc5e8 | Open a file through 0x9c2d3 and abort when it fails. |
| `0x6df0` | AL:uint8_t | none | 6e18@6e35 | Allocate and zero five bytes per unit. |
| `0x7780` | EAX:int32_t,EDX:int32_t | EAX:uint32_t | 77a8@77d1,8748@8841,8748@8904 | Allocate and zero a block of the product of both sizes. |
| `0xec3c` | none | EAX:uint32_t | f314@f3bf | Parse a token and look it up in the 13-entry table at 0x133304. |
| `0xeea0` | none | EAX:uint32_t | f314@f3e1 | Skip three tokens and look the fourth up in the 37-entry table at 0x1335a0. |
| `0xef98` | none | EAX:uint32_t | efe8@f028 | Parse a token and look it up in the 3-entry table at 0x132cc4. |
| `0xf0ec` | none | EAX:uint32_t | f13c@f14b | Parse a token and look it up in the 6-entry table at 0x132d1c. |
| `0xf038` | EAX:ef_addr32 | EAX:uint32_t | f08c@f0d7 | Read a count and that many 17-byte parser records. |
| `0xf08c` | EAX:ef_addr32 | EAX:uint32_t | f314@f588 | Read a count and that many 6-byte groups of 17-byte parser records. |

## runtime_helpers20 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x27544` | EAX:ef_addr32,DX:int16_t,EBX:int32_t | none | 275a4@275d2 | Draw a null-terminated list of 6-byte text entries, centered at x=160. |
| `0x2aef8` | AX:int16_t | none | 49228@495f9 | Allocate and zero the unit table for count+1 records of 0xca bytes. |
| `0x34d50` | none | none | 34d94@34db2 | Show the 60-step progress sequence, waiting five clock ticks per step. |
| `0x35748` | AL:uint8_t,DX:uint16_t,BL:uint8_t | none | 35590@3561e | Arm the timer record unless one is already active. |
| `0x372d4` | AX:int16_t | none | 49228@49601 | Allocate and zero a table of count+1 records of 0x45 bytes. |
| `0x38584` | none | EAX:uint32_t | 4aa4c@4ac7f | Take the next free slot index of the 2000-slot table. |
| `0x3c410` | none | none | 7ab00@7ab0e | Allocate the 0x9202-byte work area. |
| `0x3c6c0` | EAX:ef_addr32 | none | 7aa2c@7aa36 | Copy a string into a fresh buffer and register it. |
| `0x3c730` | EAX:ef_addr32,DX:int16_t | EAX:ef_addr32 | 3c768@3c772,3c790@3c7a1 | Find a named entry in the scene or abort. |
| `0x3ccec` | EAX:ef_addr32 | EAX:ef_addr32 | 3cd24@3cd25,3cd40@3cd41 | Duplicate a string into freshly allocated memory. |
| `0x76b10` | AL:uint8_t | none | 76cc4@76cd7 | Allocate a table of count 25-byte records. |
| `0x7a3e8` | none | none | 7ab00@7ab09 | Allocate (once) and clear the 0xc000-byte buffer. |
| `0x82000` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint8_t | EAX:ef_addr32 | 4ae5c@4afbe | Build a 17-byte record from the arguments. |
| `0x82024` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:ef_addr32 | 4ae5c@4afed | Build a 16-byte record from the arguments. |
| `0x8c0c8` | AX:int16_t,EDX:uint32_t | none | 8c0f0@8c148 | Store a value in the 0x898-entry table, aborting on a bad index. |

## runtime_helpers21 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8fdf0` | AX:int16_t | EAX:ef_addr32 | 8ba30@8ba3a | Find the entry with the given id in the 50-entry table or abort. |
| `0x8fee0` | none | none | 49228@4962e | Reset the 0x5dc0-byte buffer state and allocate the buffer. |
| `0x8ff20` | AL:uint8_t | EAX:ef_addr32 | 37498@37780 | Allocate and zero a table of count 68-byte records. |
| `0x980e8` | AL:uint8_t | none | 98050@98094,98198@9825b,98280@9828a | Reset a 13-byte slot when the sound path is on; abort on a bad slot. |
| `0x985b0` | AL:uint8_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 49228@49737,7a844@7a88c,7e8c0@7ea77 | Store a range and start the transfer; abort when the range is negative. |
| `0x9d060` | EAX:uint32_t | EAX:ef_addr32 | 9d114@9d11c | Allocate a block or abort with the requested size. |
| `0xa00a4` | EAX:ef_addr32 | EAX:uint32_t | a00e8@a013d | Count a record down; true while it is still live. |
| `0xa1afc` | EAX:uint32_t,EDX:uint32_t | none | a1bf4@a1c36,a1bf4@a1c9e,a1ec8@a1edf | Read from the stream and abort if the count is short. |
| `0xa1b28` | EAX:uint32_t,EDX:uint32_t | none | a1b54@a1bd6,a1cb4@a1cdf,a1d00@a1d36 | Write to the stream and abort if the count is short. |
| `0xa2900` | AX:uint16_t | AL:uint8_t | a3e50@a3e67,a3eac@a3ecc | Check a count against the limit at 0x1357ac, aborting if above. |
| `0xa39ec` | none | none | a3c18@a3c47 | Abort unless the stored name matches the expected one. |
| `0xa3bdc` | none | none | 34f94@350a3 | Query the current slot through the callback and cache its table entry. |
| `0xa3da0` | none | none | 335c0@335ce,3361c@33621,336c0@336c5 | Shut the device down and clear its state flags. |
| `0xa3e04` | none | none | 34a98@34a9e,34e18@34e95,35128@3512f | Close the device through its callback when it is armed but not running. |
| `0xa3e50` | EAX:uint32_t,DX:uint16_t,BL:uint8_t | none | a2ef4@a2f4b,a2f70@a30ee,a3280@a32c0 | Issue a request through the callback and account for it. |
| `0xa3eac` | EAX:uint32_t,DX:uint16_t | none | 34d50@34d6a,35220@3526a,3617c@3623f | Issue a request with a zero flag if the device is ready. |
| `0xa3fe0` | none | none | a3da0@a3dda,a3da0@a3ddf,a4030@a4052 | Poll the device and accumulate its two status bytes. |
| `0xa42d0` | none | none | a41dc@a420c | Start the device through its callback if it is not yet started. |
| `0xa42f4` | none | none | 357a4@357b5,3711c@37175,a3da0@a3db4 | Stop the device through its callback when running. |
| `0xa46cc` | EAX:ef_addr32 | EAX:ef_addr32 | 40dc@40eb,40dc@4133,40dc@41ae | Look up a named object or abort. |

## runtime_helpers22 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa75cc` | EAX:ef_addr32,EDX:ef_addr32 | none | 69b70@69bc2 | Copy a name of at most 16 characters into an object. |
| `0xa78bc` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | 40dc@40fd,40dc@4138,40dc@41b3 | Find a widget by name in a root or abort. |
| `0xa7990` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 41fc@425f,42a4@4306,5630@5f2e | Close a handle (100..105) and clear its slot. |
| `0xa79d4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | EAX:uint32_t | 41fc@4249,41fc@4259,28224@282be | Read from a handle (100..105). |
| `0xa7a1c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | EAX:uint32_t | 42a4@42f0,42a4@4300,286c0@2872d | Write to a handle (100..105). |
| `0xa7a64` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | a7ab4@a7abe,a7ac8@a7ad5,a7ae0@a7aed | Seek a handle (100..105) and flush it. |
| `0xa7d40` | none | EAX:int32_t | a8718@a87bd | Clock ticks since the last call, clamped to 1..50. |
| `0xabde4` | none | none | ac5f8@ac722,ca17c@ca1b8,e1958@e1b03 | Run the callback of every registered 21-byte entry. |
| `0xac7c8` | EAX:uint32_t | none | 84aec@84af6,ac5f8@ac773 | Print a diagnostic and abort with the given argument. |
| `0xb3d00` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t | AL:uint8_t | 7a5bc@7a5e6,82e40@83014,830b8@83184 | Write a whole file; abort when it cannot be created or written. |
| `0xb3f58` | AX:int16_t | EAX:uint32_t | 6b50@6b8e,6b50@6bd0,bc38@bc55 | Look up a 1-based id in the table at [0x41f370] or abort. |
| `0xc32dc` | EAX:int32_t | none | c3430@c345c | Allocate the work block from either pool depending on its size. |
| `0xc34cc` | EAX:uint32_t | EAX:uint32_t | ab74@ae62,278c0@2790a,284ec@28501 | Start the driver unless it is already busy; abort when it fails. |

## runtime_helpers23 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc3714` | EAX:uint32_t,EDX:ef_addr32,EBX:ef_addr32 | AL:uint8_t | abf8c@ac00d,abf8c@ac077 | Query the driver and return its two 16-bit status words. |
| `0xc3f1c` | none | none | cdb20@cdc2b,d1d40@d1d9d,dc250@dc25c | Blit the screen buffer through the driver callback. |
| `0xca780` | EAX:ef_addr32 | EAX:uint32_t | aca14@aca48,acac0@acb26 | Find a name in the table of 25-byte entries at 0x13727e. |
| `0xcc744` | EAX:uint32_t | EAX:uint32_t | 3cb2c@3cb84,3cb2c@3cb97,b3374@b339f | Allocate a block for a non-null request, aborting on failure. |

## runtime_helpers24 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcd2a8` | EAX:uint32_t | none | 9e7bc@9f5ff | Store a half-word offset in the current table entry. |
| `0xcd640` | AX:int16_t | none | cd724@cd745,cd754@cd78b | Allocate the 0xc000-byte text block of a slot when it has none. |
| `0xcd67c` | AX:int16_t,EDX:ef_addr32 | none | cd724@cd74c,cd754@cd792 | Copy a text into the block of a slot; abort when empty or too long. |
| `0xcd724` | AX:int16_t,EDX:ef_addr32 | none | e3e50@e3eda | Store a text in a fixed slot (0..99). |
| `0xcd754` | EAX:ef_addr32 | EAX:uint32_t | e3cac@e3dbb | Store a text in the next dynamic slot and return its id. |
| `0xcd7f0` | AX:int16_t | EAX:uint32_t | 11e7c@11ec3,11e7c@11eda,14d58@14da5 | Return the text block of a slot (0..599). |
| `0xd16d0` | EAX:uint32_t,EDX:uint32_t | none | d16fc@d1702,d175c@d176a,d1860@d188a | Write to the log file and abort if the count is short. |
| `0xd1774` | EAX:ef_addr32 | none | d1c54@d1c77 | Open the log file and write its header. |
| `0xd1da8` | EAX:uint32_t,EDX:uint32_t | none | d1e50@d1ecd,d1e50@d1ede | Pick the 640x400 video mode if either mode is available. |
| `0xd1de8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | d1e50@d1ee7 | Set up the 640x480 video mode; other modes abort but continue. |
| `0xd6fa8` | EAX:ef_addr32 | none | 81230@812ed,81448@814d2,d6f90@d6f9a | Allocate the text buffer of an object and fill it from the default text. |
| `0xd73b8` | AL:uint8_t | none | 7ef14@7f3ae,7ef14@7f3be,7f850@7f96d | Run channel call 0x102858 on the driver channel (0..13). |
| `0xd7458` | AL:uint8_t | none | 7ef14@7f2ed,7ef14@7f376 | Run channel call 0x102824 on the driver channel (0..13). |
| `0xd7490` | AL:uint8_t | none | 7fe18@7fe34 | Run channel call 0x1027a0 on the driver channel (0..13). |
| `0xd74d0` | AL:uint8_t | none | 7ef14@7f43c,7ef14@7f4d6,7fe8c@7fe9e | Start the driver channel and queue its follow-up request. |
| `0xd7530` | AL:uint8_t | none | 7fe4c@7fe71 | Run channel call 0x1027cc on the driver channel (0..13). |
| `0xda190` | EAX:ef_addr32,EDX:ef_addr32 | none | d9e50@da06f | Select the table entry matching a name, store the key and run its handler. |
| `0xdc250` | none | none | 277d8@2780d,27858@2788a,278c0@27afd | Toggle the page flag between 0 and 1 around the page callbacks. |
| `0xdc280` | none | none | 1e480@1e514,1e480@1e58a,1e480@1e602 | Cycle the page flag through 0..2 around the page callbacks. |

## runtime_helpers25 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdead0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | a1d50@a1e67,c5d8f@c5df0,c74bc@c7509 | Find a key in the 1-based id table (stack argument) or abort. |
| `0xe1130` | EAX:uint32_t | none | none direct | Wait for the given ticks or until the abort bit 0x80 is set. |
| `0xe115c` | EAX:uint32_t | none | e0b6c@e0b7b,e1194@e11bc | Wait for the given ticks or until the input poll reports a press. |
| `0xe29a0` | none | none | 80058@8006d,81230@81246,8b274@8b29a | Allocate the arena on first use, register its release hook, then reset it. |
| `0xe29e8` | EAX:uint32_t | EAX:uint32_t | 7ab00@7ab2d,7b3c8@7b3d6,7b3c8@7b3ee | Take bytes from the arena without a header; abort when it runs out. |
| `0xe2a94` | EAX:uint32_t | EAX:uint32_t | 36054@36086,369fc@36aa3,3c410@3c415 | Allocate a block with a 4-byte size header from the arena. |
| `0xe2af4` | EAX:uint32_t | none | 36438@36611,3c434@3c439,3c6c0@3c718 | Free a block of the arena; the arena resets when the last block is freed. |
| `0xe3170` | EAX:uint32_t,DX:uint16_t | none | e330c@e332a,e330c@e333b,e378c@e37b5 | Read from the resource handle and abort when the count is short. |
| `0xe3c20` | none | EAX:ef_addr32 | c4389@c438b | Take the next 36-byte record of the 1000-record pool. |
| `0xe3fbc` | EAX:uint32_t | none | none direct | Translate a value and append the non-zero result to the 32-entry list. |
| `0xe9574` | none | none | 80058@800d0,81230@812a9,82cec@82d3e | Release the buffer at 0x1600a0 once and mark it released. |
| `0xea100` | EAX:uint32_t | EAX:uint32_t | ea270@ea2b9,ea270@ea2e2,ea270@ea34d | Allocate and zero a block, aborting on failure. |
| `0xea1a0` | EAX:ef_addr32,EDX:uint32_t | none | ea270@ea398 | Read a header into an object and take its chunk size when tagged 'RA'. |
| `0xea420` | EAX:uint32_t | none | 9dcf8@9dd1d,de864@de999 | Decode the current chunk either with the 'RA' decoder or a plain copy. |
| `0xea470` | EAX:uint32_t | none | 8b274@8b2a9 | Install the exit handler and restart; abort when called with a mode other than 1. |
| `0xeaab0` | none | none | eac48@eac83 | Open the data file and read its 0x306-byte header. |
| `0xeaaf4` | none | none | eab44@eab6d,eac48@eaccb | Read the data file's size-prefixed block into 0x474368. |

## runtime_helpers26 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xeabe8` | EAX:ef_addr32 | none | eac48@eac7e | Build the data file path from the default path, a name and a suffix. |
| `0xeb4a0` | EAX:uint32_t | EAX:uint32_t | ebb58@ebbe0 | Allocate a block or abort. |
| `0xf1850` | EAX:ef_addr32 | none | f18a4@f1912,f18a4@f1a13,f1a70@f1b94 | Copy an entry's name when its kind is 1 and publish the kind. |
| `0xf2dfc` | none | none | f3b78@f3b99 | Save the current path, restart the driver and abort when it fails. |

## runtime_helpers27 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x510` | EAX:uint32_t | EAX:uint32_t | b04@b15,ddbe0@ddc02,ddbe0@ddc11 | Allocate a paragraph-aligned block and register it in the 12-entry pool. |
| `0x628` | EAX:uint32_t,EDX:uint32_t,EBX:int32_t | EAX:uint32_t | e20@e73,e20@e84,e20@e95 | Take a free 10-byte request slot, fill it and start the request. |
| `0x6b8` | EAX:uint32_t | none | cd554@cd5a1,cd554@cd5ab,118290@1182bd | Finish a request slot and return it to the free list. |
| `0x734` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | 81c@866,cd4d4@cd529,117f68@1181a4 | Take a free 14-byte timer slot, fill it from the source and start it. |
| `0x6b50` | none | none | 5f60@5fba,56d18@56d39 | Copy the pending name into a fresh string slot according to the mode. |
| `0xf198` | none | EAX:uint32_t | f6a8@f7d9 | Read a 14-byte header and its list of 44-byte records. |
| `0xf604` | EAX:ef_addr32 | EAX:uint32_t | f6a8@f7ff | Read a record header and its list of 124-byte sub-records. |
| `0xfb10` | EAX:ef_addr32 | EAX:uint32_t | fbcc@fc39 | Read a list of 25-byte points and clamp the inner ones into 12..387. |
| `0xfbcc` | EAX:ef_addr32 | EAX:uint32_t | fc54@fcd8 | Read a header and its list of point lists. |
| `0xfc54` | EAX:ef_addr32 | EAX:uint32_t | fecc@ff70 | Read a group header and its list of 10-byte polyline records. |
| `0xffb4` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | 1001c@1022b | Find the node whose name matches; abort unless exactly one matches. |

## runtime_helpers28 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x105a4` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32,[ESP+4]:ef_addr32 | EAX:uint32_t | 45a4@45f0,7c574@7c6a3 | Read four numbers from a parser file into the given words. |
| `0x1e3c4` | none | none | 3dd30@3e886 | Re-roll the strength of every entry whose spot is occupied, per list node. |
| `0x285b8` | none | none | 3ea0c@3ec95 | Write the save file header block for the current game file. |
| `0x2863c` | none | none | 3ea0c@3ecac | Read the save file header block of the current game file. |
| `0x34ca4` | none | none | none direct | Start a new session: clear the flags and open the device if needed. |
| `0x34ea4` | none | none | 335c0@335c9,3361c@3361c,336c0@336c0 | Clear the session flags and check the device state in mode 3. |
| `0x34e18` | none | none | none direct | Wait for the device to stop, then shut it down and clear the session. |
| `0x36054` | EAX:ef_addr32,DL:uint8_t | none | 36f84@37066,36f84@37077 | Open the named stream for playback and arm the stream state. |
| `0x3730c` | none | EAX:uint32_t | 37498@374b6 | Take the next 69-byte record of the table, linking it to the previous one. |
| `0x39e0c` | AX:int16_t | none | 8c49c@8c85f | Claim a free slot in the 4-byte slot table for a unit and mark its owner. |

## runtime_helpers29 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3ce70` | none | none | 3cfc0@3d2e1 | Resolve the terrain cell of every 11-byte entry that has none yet. |
| `0xf478c` | none | none | f4a88@f4b2a,f4bdc@f4c06,f4c60@f4c91 | Run the handler of the table entry whose name matches the current one. |
| `0xfddf0` | EAX:ef_addr32,EDX:uint32_t | none | none direct | Store the lower and upper bound of a range as 16.16 values and notify. |

## game_helpers44 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x49d2c` | EAX:ef_addr32 | none | 357ec@35852 | Walk the segment list to the node holding the budget in 0x1a666b. |
| `0x49f28` | AX:int16_t,DX:int16_t,BX:int16_t | AX:uint16_t | 49228@4988b,49228@4990d,49228@49970 | Look up a word of the unit record owning grid cell (a, b); abort on a bad cell. |
| `0x4b0d0` | EAX:ef_addr32 | none | 49228@4981a | Find the segment holding the budget in 0x1357ae and publish its cell word. |
| `0x50f54` | AL:uint8_t | AL:uint8_t | 52164@522ca | Dispatch the current word through the 10-byte command table. |
| `0x50fcc` | EAX:ef_addr32 | none | 52164@522b6 | Skip script lines until the current word equals the given name or the end marker. |

## game_helpers45 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x558b0` | none | none | 581b0@5841b | Finish the pending marker round: mark this player ready and update the scene. |
| `0x56bdc` | none | none | 11820@11d02,19a80@19ac1,19a80@19b12 | Announce readiness (command 0x4d) until everyone answers or 150 ticks pass. |
| `0x57030` | none | none | 3dd30@3e08d | Announce readiness (command 0x36) until everyone answers or 100 ticks pass. |
| `0x7a4a4` | none | none | 7a738@7a73f | Allocate the status text buffer, copying the description when enabled. |
| `0x7c4fc` | EAX:ef_addr32,EDX:uint32_t | none | 4aa0@4aee,4cf4@4d38,4d4c@4dfe | Build a path from a name, remember it and hand a copy to the loader. |
| `0x7e5a0` | none | AL:uint8_t | 7e758@7e75a | Mark unowned players as ours and resend until all players answer or time runs out. |
| `0x7e758` | none | AL:uint8_t | 7e8c0@7e9d6 | Claim the players, then send command 0x1d until the pending counter drains. |
| `0x7e7e0` | none | none | 8bb50@8bc3e | Wait for the pending round to clear, then mark this player ready. |

## game_helpers46 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x7eccc` | none | none | 8bb50@8bc0b | Wait until the counter at 0x127da4 reaches the target at 0x12583a. |
| `0x80058` | none | none | 3ea0c@3ed58 | Load the first sound bank once and register its release hook. |
| `0x81230` | none | none | 3ea0c@3ed46 | Load the second sound bank once, create its voice and register its release hook. |
| `0x82060` | AX:int16_t | none | 49228@495a0 | Allocate the grid table of n rows of 17 bytes and mark every cell empty (0xff). |
| `0x830b8` | none | none | 19a80@19adb,84aec@84aec | Rewrite the settings file with the status text lines appended. |
| `0x83424` | AX:int16_t,DX:int16_t,EBX:uint32_t,ECX:ef_addr32,[ESP+4]:ef_addr32 | AL:uint8_t | 834e8@8357c,834e8@835b4 | Project a point through the view matrix; false when it is behind the viewer. |

## game_helpers47 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x842b4` | none | none | 843b0@843c8,843b0@843ec,843b0@8449e | Center the view on the current camera position and publish its origin. |
| `0x846c8` | EAX:ef_addr32 | none | 848e0@84932 | Reset a unit record to its idle state and rebuild the draw defaults. |
| `0x84cfc` | AX:int16_t,EDX:ef_addr32 | none | 3ea0c@3ecfa | Run the handler whose name matches the first argument of the list. |
| `0x84ff0` | AX:int16_t,EDX:ef_addr32,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:ef_addr32 | none | 4a210@4a93f | Fill one 25-byte entry of a unit's table (stack arguments). |
| `0x850a4` | AX:int16_t,DL:uint8_t | none | 2ab54@2abd2 | Allocate a unit's table of 25-byte entries. |
| `0x8600c` | AL:uint8_t | AX:uint16_t | 86110@8666b | Find the next slot after the current one that holds the given kind. |

## game_helpers48 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x883cc` | EAX:ef_addr32 | none | 8846c@884b3 | Step one object: run its handler when it is alive and has one. |
| `0x8846c` | none | none | 3dcb0@3dcb5 | Step every object of the ring and publish the visible ones. |
| `0x8cc40` | AX:int16_t | none | 8c49c@8c807 | Set a unit's base speed from its type and add 2 when the trigger fires. |
| `0x8d030` | none | none | 8dd48@8de90 | Fire the cell trigger when the player's unit stands on a special terrain value. |

## game_helpers49 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8f3f4` | EAX:ef_addr32,EDX:ef_addr32 | none | 8be3c@8be42 | Make the other object the tracked one if it is at least as close as the current one. |
| `0x8f930` | none | none | 8f644@8f868 | Publish the current entry position of the player's unit through the 0x831a0 block. |
| `0x8fc24` | none | none | 3dd30@3e1f8 | Fade the ten 24-byte effect slots and relink their records. |
| `0x8fcec` | AX:int16_t | none | 4a210@4a806 | Name the units of a player's formation from the name table. |
| `0x8fe58` | none | none | 8bd58@8bd70 | Load the table resource and index its 50 records. |

## game_helpers50 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8ff48` | none | AX:uint16_t | 37498@375af | Create an effect record, link it at the head of the list and number it. |
| `0x90124` | AX:int16_t,DX:int16_t,BX:uint16_t | none | 15b7c@15e7e,16588@167e1,900c0@90111 | Apply a result to an object slot, optionally computing the response value. |
| `0x90378` | EAX:ef_addr32 | none | 901ec@9020c | Drop an object near the camera onto the terrain and register its move. |
| `0x93f44` | none | none | 95e58@96106 | Reset the angle record from the current limits and flag out-of-range limits. |
| `0x94954` | AX:int16_t | none | 95e58@969b0,95e58@96ac9 | Update the navigation target of a unit from its linked object. |

## runtime_helpers30 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x39db0` | AL:uint8_t | none | 39dc4@39dd4 | Allocate the four-byte slot table of the given length. |
| `0x8204c` | EAX:uint32_t | EAX:ef_addr32 | 4ae5c@4b0a5 | Allocate a 12-byte node holding a value. |
| `0x84650` | none | AL:uint8_t | 84678@846bf | Unsupported feature: abort and report failure. |
| `0xa292c` | none | AL:uint8_t | a3eac@a3ebb,a42c4@a42c4 | Abort when the device is not ready; always reports ready. |
| `0xa41a0` | none | none | 3617c@36233,3617c@36244,3617c@3628f | Run the optional device idle callback. |
| `0xa7d34` | none | none | a82f0@a853a | Remember the current clock. |
| `0xab820` | EAX:uint32_t | none | f18a4@f1a02,f18a4@f1a5f,f1a70@f1b43 | Run the optional word callback with the low 16 bits of the argument. |
| `0xe1110` | EAX:uint32_t | none | e0b88@e0bb3,e0bcc@e0bfa,e0bcc@e0c2d | Busy-wait for the given number of clock ticks. |
| `0xe1730` | none | AL:uint8_t | 34390@343a3 | Unsupported input query: abort and report none. |
| `0xe1744` | none | AL:uint8_t | 34140@3415e,34ea4@34f03 | Unsupported input query: abort and report none. |
| `0xe1758` | none | AL:uint8_t | 34298@342e1 | Unsupported input query: abort and report none. |
| `0xe1780` | none | AL:uint8_t | 34140@34150,34ea4@34ef5 | Unsupported input query: abort and report none. |
| `0xe1948` | EAX:uint32_t | none | e1e6c@e1efb,e1e6c@e1fc8,e1e6c@e20c5 | Abort with a message that includes the argument. |
| `0xeb25a` | EAX:uint32_t | none | 9bd20@9bd65,eb448@eb471 | Run the two page callbacks, then the page switch for the argument. |
| `0xeb272` | EAX:uint32_t | none | eb25a@eb26b | Run the page callback pair, then hand the argument to 0x20d. |
| `0xeb35c` | none | none | dc250@dc250,dc280@dc281 | Abort with the stored message when the error flag is set. |
| `0xed6c5` | none | none | ed6cc@ed831 | Forward to the callback at 0x17d498. |

## runtime_helpers31 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x98050` | AL:uint8_t | none | 3ea0c@3ed7e,3ea0c@3ef0e,98198@98266 | Switch the active sound slot (0..3), restarting the driver when it fails. |
| `0x9a624` | none | none | 98bec@98bf1 | Dispatch every active 54-byte record by kind and sum the four counters. |
| `0x9aa78` | EAX:ef_addr32,DX:int16_t | none | 9a624@9a683 | Count an event kind in the counter row of the current slot. |
| `0x9d1e8` | EAX:ef_addr32 | none | f80@1158,f80@1171,f80@11b4 | Read the next non-comment text line of a stream into its buffer. |
| `0x9d990` | none | none | 9da80@9da8d | Load the quoted-string file into an arena block and index its strings. |

## game_helpers51 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa1b54` | EAX:ef_addr32,DX:uint16_t | none | 7a9bc@7a9db | Create the state file and write its 12-byte header. |
| `0xa1bf4` | EAX:ef_addr32,DX:uint16_t | none | 7a844@7a871 | Open the state file and validate its 12-byte header. |
| `0xa2188` | EAX:ef_addr32 | none | none direct | Blend two 3-word keys by a 14-bit weight (linear). |
| `0xa2434` | EAX:ef_addr32 | none | none direct | Blend two 3-word keys by a 9-bit weight (quadratic). |
| `0xa3f74` | EAX:ef_addr32,DX:uint16_t | none | 545b8@545d9,545e4@545f4,54600@54618 | Append a packet to the outgoing buffer, flushing first when it would overflow. |
| `0xa4138` | none | none | 34f94@34f95,35590@355ac,36f84@36f94 | Update the frame delta (clamped to 100) and clear the frame counters. |
| `0xa41dc` | none | AL:uint8_t | 34f94@34fa3 | Start the device; returns true when it is running. |
| `0xa4cb4` | AX:uint16_t | none | 94fc@979a,9820@99aa,9820@99c1 | Activate widget slot idx: run its callback and queue it for redraw. |

## runtime_helpers32 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa6c20` | EAX:ef_addr32 | none | a6d00@a7082 | Draw a widget's text when its rectangle lies inside the clip box. |
| `0xa713c` | EAX:ef_addr32 | none | a6d00@a70be | Draw a widget's frame rectangle when it is visible. |
| `0xa7510` | EAX:ef_addr32,EDX:ef_addr32 | none | a7588@a758f | Store a name in a record, allocating its buffer on first use. |
| `0xa7ec4` | EAX:ef_addr32 | EAX:ef_addr32 | 4738@47ec,4bf8@4c41,5488@55fd | Look up a key name; unknown names return a fallback slot. |
| `0xa7f64` | EAX:ef_addr32 | EAX:ef_addr32 | 27fec@27ffe | Look up a key name that must exist and be of kind 1. |
| `0xa9bfc` | none | none | aaee8@aafb4 | Reset the key table state and its buffer pointers. |
| `0xaa350` | none | none | aad7c@aae42 | Walk every scene group and resolve its child nodes. |

## runtime_helpers33 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xaa4d4` | none | none | aad7c@aae3d | Rewrite the pointer fields of every type 0xe node in each group. |
| `0xab28c` | EAX:ef_addr32 | none | ab47c@ab5a8 | Visit the pointer fields of every type 0xe node, then finish the context. |
| `0xaca14` | none | none | ac854@ac96a,aca94@acab5,acac0@acb0a | Pick the default video mode entry from the configured kind. |
| `0xacac0` | EAX:ef_addr32 | none | ac16c@ac1fd,e1958@e19ee,e1958@e1b51 | Select the video mode named by the argument. |
| `0xac16c` | none | none | ac0e0@ac158,ac780@ac791 | Clear the UI state tables and select the video mode. |
| `0xac0e0` | none | none | 84c84@84c96,84c84@84cac | Reset the UI after a mode change and centre the cursor. |
| `0xac240` | EAX:ef_addr32,DL:uint8_t | none | ac408@ac449,ac408@ac480,ac408@ac4dc | Run the registered page callbacks, then wait for the mouse buttons to clear. |
| `0xb0138` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | 49228@49461 | Allocate and initialise a pool sized from the shape list. |
| `0xb1a68` | EAX:ef_addr32 | none | b1ae4@b1af5 | Free a record and all of its sub-tables. |
| `0xb21ec` | EAX:ef_addr32,EDX:uint32_t | none | b1f80@b21c7,b2758@b27d0 | Print one record to a text handle. |
| `0xb2810` | EAX:ef_addr32,EDX:uint32_t | none | b2b00@b2cb9 | Dump a table entry and its sub-records (variant A names). |
| `0xb28b8` | EAX:ef_addr32,EDX:uint32_t | none | b2b00@b2cd1 | Dump a table entry and its sub-records (variant B names). |
| `0xb29a8` | EAX:ef_addr32,EDX:uint32_t | none | b2560@b26d2,b2560@b26ff,b2560@b272c | Dump a list of sub-lists. |

## runtime_helpers34 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb2d8c` | EAX:ef_addr32,EDX:uint32_t | none | b2e98@b2f28 | Print one shape point record. |
| `0xb2e98` | EAX:ef_addr32,EDX:uint32_t | none | b2e28@b2e55 | Print an object record and its point list. |
| `0xb3b90` | EAX:ef_addr32 | EAX:uint32_t | 6b50@6b78,82e40@82e4e,830b8@830c6 | Return the payload size of a file from its header. |
| `0xb3db0` | EAX:uint32_t | EAX:uint32_t | 6b50@6b88,6b50@6bca,bc38@bc4a | Take a block from the top of the stack pool and clear it. |
| `0xbc1d3` | EAX:ef_addr32 | EAX:uint32_t | bc287@bc291 | Convert an fopen mode string to open flags. |
| `0xbd5f8` | EAX:ef_addr32 | AL:uint8_t | bdbf8@bdc8d,bdbf8@bdcd7,bdbf8@bdd26 | Clear the unusable entries of a record's slot table. |
| `0xbe320` | EAX:ef_addr32,EDX:ef_addr32 | none | b0f5c@b15e4 | Link matching shape entries of two lists to their lookup value. |

## runtime_helpers35 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd1d40` | EAX:uint32_t,EDX:uint32_t | none | d1da8@d1dde,d1de8@d1e31 | Set the screen size and run the video start-up sequence. |
| `0xdeb87` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t | none | a3b48@a3b59,deb50@deb67 | Fill count dwords at dst with value. |
| `0xe284c` | EAX:ef_addr32,DX:uint16_t | EAX:uint32_t | a63c0@a63d4 | Width in pixels of a string in the given font. |
| `0xe994c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 80b14@80b95,83a40@83a66,a04b4@a0511 | Build a rotation matrix and optionally mirror it. |
| `0xe99b8` | EAX:ef_addr32,EDX:int32_t,EBX:int32_t,ECX:int32_t | none | 80b14@80ba5,83734@83737,a04b4@a0526 | Transform a vector by the current matrix (result doubled). |
| `0xf22af` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t,ECX:uint32_t | EAX:int32_t | 11159a@1115af | Set a file's buffer (setvbuf). |
| `0xf25b0` | AL:uint8_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | f261c@f2644 | Draw a 3-column by 6-row glyph into the frame buffer. |
| `0xf36a4` | none | none | f3f84@f4079 | Advance the animation counter and wrap it. |
| `0xfba6c` | EAX:uint32_t,EDX:uint32_t,EBX:ef_addr32 | EAX:uint32_t | bfb88@bfcb6,fba54@fba63 | Count matching type-2 entries across a shape list. |

## float_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3c74` | EAX:int32_t,EDX:int32_t,EBX:int32_t | EAX:int32_t | 20ec@215f,20ec@21b7,20ec@2211 | Integer length of a 3D integer vector, at least 1. |
| `0x66c4` | none | EAX:int32_t | 6924@69a2,625d8@629d6 | Integer result of a table lookup for the current object, or 0. |
| `0x7ca8` | EAX:ef_addr32,DL:uint8_t,BL:uint8_t | none | 7e44@7e7b,70a58@70d8f,70a58@70ed2 | Scaled point from a table entry and one sub-entry, stored as two doubles. |
| `0x8184` | EAX:ef_addr32 | AL:uint8_t | 84b8@84d5 | Index of the table entry with the smallest metric, or 0xff. |
| `0x81ec` | AL:uint8_t,DL:uint8_t | EAX:double | 7ee8@7f74,712e8@71322,7156c@715bb | Double value of a sub-entry, wrapped through a helper unless its kind is 1. |
| `0x1d844` | EAX:ef_addr32 | none | 1d7a4@1d808,1db08@1db10 | Pass the scaled offset of the current waypoint to a helper. |
| `0x1d8e8` | EAX:ef_addr32 | AL:uint8_t | 1d8a8@1d8d1,1d954@1daed | True when the current waypoint is within the limit on either axis. |
| `0x29adc` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 29b14@29c4d | Two scaled components from a record's rotation words. |
| `0x2aa08` | EAX:ef_addr32,EDX:ef_addr32 | none | 2b1f4@2b331 | Convert a packed 16.16 pair to doubles and clear the third component. |
| `0x2dc84` | AX:int16_t | AL:uint8_t | 2dce0@2dcf6 | True when a record is close enough and active. |
| `0x37bc8` | EAX:ef_addr32,DX:int16_t | EAX:double | 2d9a4@2d9f2,39edc@39fac,3a000@3a0e5 | Distance helper over a scaled pair taken from a table entry. |

## float_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x2dbf8` | AX:int16_t,DL:uint8_t | AL:uint8_t | 2dce0@2dd1a | True when a record is near the unit entry and active. |
| `0x37c0c` | EAX:ef_addr32,DX:int16_t | none | 2b1f4@2b2e6,39edc@39f8e,3a000@3a0b3 | Report the offset between a table point and a position. |
| `0x37c64` | AX:int16_t,DL:uint8_t | AL:uint8_t | 37d3c@37d6b | True when a table point is close to a unit's position. |
| `0x37cdc` | AX:int16_t | AL:uint8_t | 37d3c@37d42 | True when a table point is close to the player. |
| `0x37d88` | AX:int16_t,DL:uint8_t | AL:uint8_t | 37e60@37e8f | True unless a table point is far from a unit's position. |
| `0x37e00` | AX:int16_t | AL:uint8_t | 37e60@37e68 | True unless a table point is far from the player. |
| `0x40870` | EAX:uint32_t,EDX:ef_addr32,EBX:uint32_t,ECX:double | EAX:double | 59adc@59d66 | Rate between two clock readings and a distance, minus a divided distance. |
| `0x45b60` | EAX:ef_addr32 | EAX:double | 10e4dd@10e5ac | Double value of an object, reporting an error when its check fails. |
| `0x45fc4` | EAX:double | EAX:int32_t | 6f48@6fb3,7030@70e2,39edc@39f99 | Scale a double to an angle word, wrapping negatives. |
| `0x4600c` | EAX:double | AL:uint8_t | 6f48@6fe4,7030@7115,474b8@47531 | Negated scaled double truncated to a byte. |
| `0x4fdf4` | EAX:ef_addr32,EDX:ef_addr32,EBX:float | none | 4fa64@4fc46,4fa64@4fcda | Add a float of minutes to a minutes/seconds pair, carrying seconds. |
| `0x53330` | EAX:float,EDX:float,EBX:float | EAX:double | 53350@53364,53350@534aa | Length of a vector of three floats. |
| `0x54110` | none | none | 3dd30@3dfa4 | Store the scaled frame step and refresh the dependent tables. |
| `0x5f944` | none | none | 5f60@5f61 | Fill a 90-entry scaled sine table. |

## float_helpers3 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x80f4` | EAX:ef_addr32,EDX:uint32_t | EAX:double | 8184@81af,2d488@2d5be | Distance from a context to a table entry's centre, or a large sentinel. |
| `0x8394` | AL:uint8_t,DL:uint8_t | none | 84b8@85df,8fe4@905d | Flag a sub-entry as ahead (1) or behind (0xff) from a direction test. |
| `0x8424` | AL:uint8_t,DL:uint8_t | EAX:int32_t | 84b8@861a,8cc4@8dbc | Scaled heading of a sub-entry as an integer angle, wrapped positive. |
| `0x18118` | none | none | 7957c@79654 | Toggle the autopilot hint flag when the player is in the right state. |
| `0x1d7a4` | EAX:ef_addr32 | none | 1d700@1d746 | Initialise an autopilot context from its waypoint table. |
| `0x6a5ac` | EAX:ef_addr32,DX:int16_t | EAX:double | 73820@738f8 | Helper result for the offset from a position to a unit, or 0.0. |
| `0x6a66c` | AX:int16_t,DX:int16_t | EAX:double | 6a760@6a7c6 | Distance between two units, or 0.0 when either is missing. |
| `0x6a6e8` | EAX:ef_addr32,DX:int16_t,BX:int16_t | none | 6a760@6a7bd | Difference of two units' weighted axes, stored as three doubles. |
| `0x6bd88` | none | none | 6b6c8@6bbb8 | Copy the camera position and recompute the closing-speed state. |
| `0x84f60` | AX:int16_t,DL:uint8_t | EAX:double | 49228@49b86,55bf8@55c73,55bf8@55ccb | Double stored in a unit's slot table, or 0.0 when the unit is missing. |
| `0x86720` | none | none | 86110@866c6,8cb60@8cc35 | Sum the weighted entries of the player's slot list into two doubles. |
| `0x88d90` | EAX:ef_addr32,EDX:ef_addr32 | none | 89e08@89f41,8a318@8a67d | Scale a three-double vector in place and store its truncated components. |

## float_helpers4 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x88ddc` | EAX:ef_addr32,EDX:double,EBX:double,ECX:double | none | 8a318@8a6ad | Store three scaled doubles as angle words in an output record. |
| `0x88e50` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 89fbc@8a073 | Multiply a vector of three doubles by a 3x3 double matrix (column form). |
| `0x88f6c` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 89fbc@8a184 | Multiply a vector of three doubles by a 3x3 double matrix (row form). |
| `0x8bf58` | AX:int16_t,EDX:ef_addr32 | EAX:int32_t | 26b60@26c80,26b60@26e26,270d0@2726b | Scaled distance from a unit to an integer vector, truncated; 0 if the unit is missing. |
| `0x92f10` | none | none | 95e58@96ce1 | Reset the scene record from the current object and the scene angle record. |
| `0x97534` | EAX:ef_addr32 | AL:uint8_t | 97860@978ad,97860@978c6 | True when an object faces the camera closely enough and is active. |

## float_helpers5 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9ffc8` | AX:int16_t,DX:int16_t | none | 9e7bc@9e8b1 | Convert a table entry's vector and scale a word into a float. |
| `0xa5508` | EAX:ef_addr32,DX:int16_t,BX:int16_t | none | a870@a901,4c6a0@4c6f2,4c718@4c766 | Set a record's offsets from two scaled values and refresh it. |
| `0xa56b0` | EAX:ef_addr32,EDX:double | none | a870@a8fa,4c6a0@4c6eb,4c718@4c75f | Store a positive scale factor in a record and redraw it. |
| `0xb415c` | EAX:ef_addr32,EDX:ef_addr32 | EAX:double | 73ec@7448,78c0@79c5,80f4@815d | Distance between two 2D double points, 0.0 when degenerate. |
| `0xb41bc` | EAX:ef_addr32,EDX:ef_addr32 | EAX:double | 6a66c@6a6d3 | Distance between two 3D double points, 0.0 when degenerate. |
| `0xb4230` | AX:int16_t | EAX:double | 78c0@790d,78c0@7929,7ca8@7cc7 | Convert a signed word to a scaled double. |
| `0xb4250` | EAX:double | EAX:int32_t | 73ec@7426,73ec@743b,d290@d363 | Convert a double to a scaled integer. |
| `0xb427c` | EAX:double | EAX:double | 7d20@7d9f,7d20@7dcf,2461c@24785 | Absolute value of a double. |
| `0xb42b0` | EAX:double | EAX:double | 1db08@1db8b,5f698@5f6e0 | Sign of a double as +1.0 or -1.0 (zero gives -1.0). |
| `0xb42f0` | EAX:double | EAX:double | 7e44@7eac,81ec@824a,8394@83f0 | Wrap an angle in degrees into its canonical range. |
| `0xb4364` | EAX:ef_addr32 | EAX:double | 1d954@1dad2,772d0@77465,8a1dc@8a222 | Length of a 3D vector of doubles, 0.0 when degenerate. |

## float_helpers6 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa3824` | AL:uint8_t | none | a33a4@a33e6 | Dispatch on the balance ratio of a table entry. |
| `0xb0718` | AL:uint8_t,EDX:uint32_t,EBX:ef_addr32 | EAX:double | b0c34@b0c64 | Ratio of listed entries of kind 1 or 2 to all matching entries. |
| `0xb07b4` | AL:uint8_t,EDX:uint32_t,EBX:ef_addr32 | EAX:double | b0c9c@b0ccd | Same ratio using the second matching predicate. |
| `0xb43dc` | EAX:ef_addr32,EDX:ef_addr32 | EAX:double | 88ff8@88ffe,b4468@b4479,119f88@11a2d2 | Dot product of two 3D double vectors. |
| `0xb4424` | EAX:ef_addr32,EDX:ef_addr32 | AL:uint8_t | 28b80@28d50,28b80@28d70,29b14@29c69 | Normalise a 3D vector into out; false when it is too short. |
| `0xb4468` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | b449c@b44a7 | Projection of a vector onto the unit direction of another. |
| `0xb449c` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 119f88@11a2af | Component of a vector perpendicular to a direction. |
| `0xb4620` | EAX:double,EDX:double | EAX:double | 8394@83e1,1d844@1d89b,1dc20@1de39 | Two-argument arctangent (y, x); 0.0 when both are zero. |
| `0xcbfa0` | EAX:ef_addr32 | EAX:int32_t | cbff0@cc091 | Default range of an object: 400 for kinds 1 and 3, else 300. |
| `0xce0b0` | EAX:float,EDX:float | EAX:double | ce268@ce27b,ce268@ce29e,ce268@ce2c2 | Scale a value relative to the horizontal viewport origin and size. |
| `0xce0cc` | EAX:float,EDX:float | EAX:double | ce268@ce28c,ce268@ce2b0,ce268@ce2d9 | Scale a value relative to the vertical viewport origin and size. |

## leaf_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe764` | EAX:uint32_t | none | f918@f976,fecc@ff51,1001c@10316 | Format a string through the allocator log helper. |
| `0x35218` | none | EAX:uint32_t | 34f30@34f70,3527c@352b6 | Constant 0x44. |
| `0x39300` | none | none | db16c@db1af | Store the result of 0x9ddfc as a word. |
| `0x52eb0` | EAX:float | EAX:double | 5375c@537cf,5375c@53816,5375c@5385d | Scale a float to double. |
| `0x56c7c` | EAX:uint32_t | none | 581b0@585e6 | Run 0x56c90 with mode 1 and set the flag word. |
| `0x7a290` | none | AL:uint8_t | 19a80@19a86 | Low byte of 0x7a194's result. |
| `0x7a38c` | none | none | 3ea0c@3ee46 | Register the 0x7a2b0 handler and remember 0x7a338. |
| `0x9d778` | none | AL:uint8_t | a1590@a175b | Return zero. |
| `0xa4104` | none | none | a41dc@a421f | Poll the device. |
| `0xa75b4` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | EAX:ef_addr32 | 345e8@34732,34bcc@34c79,35e68@35e74 | Look up a widget and, when found, apply a value to it. |
| `0xa7ab4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | b3c04@b3c4e,d1bec@d1c08,d1bec@d1c33 | Seek a handle to its start. |
| `0xa7ac8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | 9ccb8@9cdd6,9cee8@9cf24,ebcfc@ebd29 | Seek a handle relative to the current position. |
| `0xa7ae0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | a7b3c@a7b52,b3b90@b3bec,b3c04@b3c67 | Seek a handle to its end (returns the size). |
| `0xabaf0` | EAX:ef_addr32 | none | ca200@ca20b,f819c@f81aa | Build the default path into a buffer. |
| `0xb3fb0` | EAX:ef_addr32,DL:uint8_t,EBX:uint32_t | none | 6df0@6e0e,7780@779e,b1e0@b4de | Fill bytes of a block with one value. |
| `0xb3fd0` | EAX:double | EAX:double | 1dc20@1ddf9,1dc20@1de08,2461c@24676 | Square of a double. |
| `0xb3fdc` | EAX:double | EAX:double | 11b73c@11b7ec,11b73c@11b843,11b73c@11b893 | Cube of a double. |
| `0xc5390` | EAX:uint32_t,EDX:uint32_t,EBX:ef_addr32 | EAX:ef_addr32 | none direct | Byte-swap a word in place; returns the advanced pointer (EBX). |
| `0xc539c` | EAX:uint32_t,EDX:uint32_t,EBX:ef_addr32 | EAX:ef_addr32 | none direct | Byte-swap a word in place; returns the advanced pointer (EBX). |
| `0xc53a8` | EAX:uint32_t,EDX:uint32_t,EBX:ef_addr32 | EAX:ef_addr32 | none direct | Byte-swap a word in place; returns the advanced pointer (EBX). |
| `0xc53df` | EAX:uint32_t,EDX:uint32_t,EBX:ef_addr32 | EAX:ef_addr32 | none direct | Byte-swap a dword in place; returns the advanced pointer (EBX). |
| `0xd175c` | EAX:uint32_t | none | d17d0@d1811,d17d0@d1818,d17d0@d1822 | Log a single value through the 0xd16d0 helper. |
| `0xdcb74` | none | EAX:uint32_t | 233c4@2341e,9bd20@9bd30,c3584@c35a8 | Stack bytes in use relative to the recorded stack base. |
| `0xdd8d0` | none | none | 278c0@27b2d,27d18@27eb1,f3f84@f40f8 | Run the two shutdown helpers in order. |
| `0xe3022` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | EAX:uint32_t | 3d308@3d330,48bb0@48bd1,48c34@48c50 | Format into a buffer through 0xed133 and terminate the result. |
| `0xe3ffc` | EAX:ef_addr32 | none | none direct | Copy a name to the scratch string and run 0x107310. |
| `0xea4a7` | EAX:uint32_t | EAX:uint32_t | 8b39c@8b3dd | Forward a non-zero value to 0xfa8a0, else clear via 0x10ddf7. |
| `0xebe79` | none | none | 9d55d@9d6b8,9d55d@9d6ea | Report error code 14. |
| `0xf9ecd` | none | none | b33ac@b33bd | Call 0x110123. |
| `0x109070` | none | none | e1958@e1b2d | Reset via 0xe6120 then continue with the saved byte. |
| `0x109440` | none | none | e474c@e4763 | Run 0x10944c then 0x118b60. |
| `0x10ddf7` | none | EAX:uint32_t | ea4a7@ea4af | Return -1. |
| `0x10e4d3` | EAX:uint32_t,EDX:uint32_t | none | edb2d@edb92,110130@110160 | Run 0x10e4dd with the second argument also in EBX. |
| `0x1105c3` | none | EAX:uint32_t | fc795@fc843 | Return zero. |
| `0x1105d0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | none | none direct | Stub that discards three stack arguments. |
| `0x1105d4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | none direct | Stub that discards two stack arguments. |
| `0x1115f0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | EAX:uint32_t | none direct | Upper three bytes of a dword. |
| `0x1141a0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32,[ESP+8]:uint32_t | none | 103244@10326e | Store a value at +0x2c of a record when the record exists. |
| `0x11af88` | EAX:ef_addr32 | EAX:double | 10a0d4@10a0e4,11b040@11b0c3 | Absolute width of a bounding box. |
| `0x11af9c` | EAX:ef_addr32 | EAX:double | 10a0d4@10a0ee,11afb4@11afca | Scaled sum of two extents. |
| `0x11afe4` | EAX:ef_addr32 | EAX:double | 10a0d4@10a158 | Ratio of two extents. |
| `0x11b3c0` | none | EAX:double | 11c584@11c6e4 | Constant double at 0x195864. |
| `0x11b694` | EAX:ef_addr32,EDX:double | EAX:double | 10a220@10a31f,11c584@11c750 | Scaled distance below the reference level. |
| `0x11b6a8` | EAX:ef_addr32,EDX:double | EAX:double | 11b6bc@11b6f8,11b6bc@11b729 | Scaled distance above the reference level. |
| `0x11f2d3` | none | none | 117e0c@117e57,117f68@1180b7,118290@1182a2 | Disable interrupts (no-op on the host). |
| `0x11f2d5` | none | none | 117e0c@117e9a,117f68@11826c,118290@11841c | Enable interrupts (no-op on the host). |
| `0x16010e` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:int32_t | 9fb00@9fbe8,9fc00@9fd8c,a0228@a046b | Signed word at +0x10 of the record found by id. |

## leaf_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x324` | EAX:uint32_t | EAX:uint32_t | 734@77a | DOS call 0x2503 with one argument; returns the result word. |
| `0x3d0` | EAX:uint32_t,EDX:uint32_t | none | 7b4@7db | DOS call 0x2505 with two arguments. |
| `0xc24` | AX:int16_t,EDX:uint32_t | none | fc9f8@fca16 | DOS call 0x3503 with a signed word and a second argument. |
| `0xa5bc` | none | none | ba5c@bb3b,bfcc@c284 | Find a named widget and hand it to 0xc868. |
| `0x7cc38` | none | none | none direct | Reset via 0xc768, then run 0x7bc40 on a named widget. |
| `0x84d94` | none | none | aaee8@aafaf | Register the 0x84dbc callback through 0xab838 after 0xabf8c. |
| `0xa29cc` | AL:uint8_t | none | 34ca4@34cde,a3c18@a3c72 | Latch a decoded key into the state table. |
| `0xa32c8` | AL:uint8_t | none | a32e8@a331d,a32e8@a3342 | Issue a request for one byte of state. |
| `0xc469b` | none | none | 39744@3975c | Copy three constant dwords from the code area into the work area. |
| `0xcdc64` | none | none | 8b2d8@8b31a,aaee8@aafdb | Re-apply the stored video mode if it differs from the current one. |
| `0xea3ec` | EAX:ef_addr32 | AL:uint8_t | 9dcf8@9dd06,de864@de97c | Rebuild the cached chunk when the descriptor has one; true if present. |
| `0x11159a` | EAX:ef_addr32,EDX:ef_addr32 | EAX:int32_t | 101c54@101cd3 | Set a file's buffer: fully buffered for a given buffer, else line buffered. |
| `0x118520` | EAX:ef_addr32 | EAX:uint32_t | 106cb0@106cc8,106d34@106d60,106dc4@106dd5 | True when the record has no entry or its entry holds zero. |
| `0x1196a0` | none | none | 1098f0@1098f0 | Run the seven table initialisers, then 0x11c20c. |
| `0x11f538` | EAX:ef_addr32,EDX:uint32_t | none | 118770@1187aa,118770@1187b5 | Store two derived values for a number. |

## leaf_helpers3 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa60` | none | EAX:uint32_t | b04@b06 | First key code with a pending request, else the default 0x3f. |
| `0x1f960` | none | none | 5f60@5f7f | Reset the scroll state. |
| `0x7bc00` | none | none | none direct | Toggle a flag word, then run 0x7bc40 on a named widget. |
| `0x844b0` | none | AL:uint8_t | none direct | Refresh the view when a cursor offset is set, else reset it. |
| `0x9202c` | none | none | 94194@941aa,941bc@941d2 | Recompute the two scene extents. |
| `0x9ab30` | none | none | bfcc@c0bc | Run the sound bank update chain on the object from 0x9a544. |
| `0xa3f30` | none | none | 56bdc@56c55,56c90@56cb0,56ed0@56ef0 | Flush the pending byte buffer through 0xa3eac. |
| `0xabce8` | none | none | none direct | Refresh the named widget's page and draw its label. |
| `0xdeb50` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t | none | 2af20@2af31,2af20@2af45,2af20@2af59 | Fill a block with a repeating four-byte pattern. |
| `0xe474c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 53ecc@53f95,53ecc@53fdd | Store two arguments and run the shutdown call chain. |

## leaf_helpers4 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x5f634` | none | none | 5f60@5f7a,5fd8@5fe8 | Reset the page state and rebuild the menu. |
| `0x6e390` | AL:uint8_t,DL:uint8_t | AL:uint8_t | 6e59c@6e825,6f2bc@6f348,6f3e0@6f47c | Apply the standard step when the request is accepted. |
| `0x81110` | none | none | 8b39c@8b49d | Select the page handler or fall back to the default screens. |
| `0x83b50` | none | none | 84678@84678 | Advance the page counter by 3 or 1 depending on a flag bit. |
| `0x84544` | none | AL:uint8_t | none direct | Refresh the view, re-centre if the cursor left it, and redraw the panel. |
| `0x84c84` | none | none | none direct | Play the queued message strings, then wait for a key. |
| `0xa3280` | AX:uint16_t | none | a32e8@a3327 | Request a status string for an index and restore the frame delta. |
| `0xa334c` | AX:uint16_t | none | a33a4@a33da | Request the details of an entry when its table flag is set. |
| `0xa5abc` | none | none | ac240@ac2cb | Run and clear the pending callbacks of every registered widget. |
| `0xa9490` | EAX:ef_addr32,DX:int16_t | none | a94e4@a9505,a94e4@a9530,a94e4@a959a | Step a list cursor n rows forward and latch the result. |

## leaf_helpers5 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa2948` | AL:uint8_t | AL:uint8_t | a29cc@a29da | Probe the data file; returns a status code (8, 9, 0xc or 0). |
| `0xabc34` | none | none | ac5f8@ac706 | Re-label the named widget when its owner record changed. |
| `0xabc98` | none | none | 84afc@84afc,ac408@ac556 | Draw the named widget's label and activate its page. |
| `0xd7b6c` | none | none | d86e0@d87e6,d8fd8@d911f | Initialise the camera table from the mode handle, or copy the default. |
| `0xd9c30` | none | EAX:uint32_t | e9ac0@e9af3 | Open the stream in either format; 0 on success, 1 on failure. |
| `0xd9dc0` | none | none | d9cb4@d9ccb | Read the stream header and latch the heading and position values. |
| `0xe5088` | none | none | 42b04@42b1c,4bec0@4bed6,4e80c@4e815 | Draw the active unit's marker, then flag it as refreshed. |

## leaf_helpers6 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x39254` | none | none | 3ea0c@3ee0e | Load the numeric settings from their key names. |
| `0x42f7c` | none | none | none direct | Point the page widgets at the current selection. |
| `0x5cff0` | none | none | none direct | Redraw the panel frame, then the selection or the help text. |
| `0x82cec` | none | none | 8b39c@8b472 | Apply the saved options and refresh the display flags. |
| `0xa8958` | EAX:ef_addr32 | none | a9e70@a9f69 | Run the animation loop of a list record until a key press. |
| `0xcd554` | none | none | 8b2d8@8b315,a9878@a98bd,aaee8@aafd6 | Drain the pending key table, release the buffers and restore the page. |

## game_helpers53 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1abbc` | none | none | 1cde8@1cf13 | Run the fire action when the selected weapon kind allows it. |
| `0x8cb60` | none | none | 9114@91c4,9114@9214,9114@926e | Refresh the player's weapon state and the activity display. |
| `0x94000` | none | none | 95e58@96117 | Compute the scene angles from the stored vector and flag out-of-range input. |
| `0x9a8b4` | EAX:ef_addr32,EDX:uint32_t | none | 9a624@9a66e | Count a destroyed ground object in the statistics table by its class. |
| `0x9a998` | EAX:ef_addr32 | none | 9a624@9a677 | Count a destroyed air object in the statistics table by its class. |

## game_helpers54 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x42e8c` | none | none | none direct | Switch the panel pages or finish the pending camera move. |
| `0x832b8` | EAX:int32_t,EDX:int32_t,EBX:int32_t | AL:uint8_t | 834e8@835ff,848e0@8498b | Latch the scaled box corner and test it against the current volume bounds. |
| `0x919d4` | none | none | 3cfc0@3d1cf | Reset the scene angle state and install the default handlers. |
| `0x93e58` | none | none | 95e58@960f5 | Compute the scene angles from the stored vector (variant with heading latch). |
| `0x91f30` | none | none | 9357c@935c6,9357c@935de | Apply the pending yaw and roll input to the scene orientation. |

## game_helpers55 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8654` | AL:uint8_t,DL:uint8_t | none | 8cc4@8d22 | Advance a unit's activity slot, nudging the object it refers to. |

## game_helpers56 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x53c10` | none | none | 53c80@53cbe | Accumulate the frame ticks and clear the idle flag once past the limit. |
| `0x70698` | none | none | 6c488@6c510 | Clear the motion accumulators and set the default scale limits. |
| `0x7feb0` | none | none | 3dd30@3dd91,8b39c@8b48f | Start the page, or fall back to the default control mode. |
| `0xc39ac` | none | none | ab74@af26,11e7c@12133,14d58@14ee3 | Latch the view scroll and leave the pending-scroll mode. |
| `0xc39f0` | EAX:uint32_t | none | 11e7c@11ef1,14d58@14dbc,150d4@15134 | Enter the pending-scroll mode with the given latch value. |

## game_helpers57 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa7900` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:int32_t | 41fc@4228,28224@28290,283c8@284a2 | Open a resource through the first free one of five slots (name A). |
| `0xa7948` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:int32_t | 42a4@42bb,5630@5692,285b8@28609 | Open a resource through the first free one of five slots (name B). |
| `0xeae10` | none | none | 97dcc@97e45,97dcc@97e89,a8bd4@a8e61 | Release the pending handle and the status message. |
| `0xf1d54` | none | none | ac5f8@ac6f8 | Apply the stored selection when the pending-input bit is set. |

## game_helpers58 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x112b1c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 112bbc@112bc2,113c7c@113cdb | Reset a table record: clear the slots, fill the index tables with -1 and set defaults. |

## float_helpers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf83d0` | EAX:int32_t,EDX:int32_t,EBX:int32_t,ECX:int32_t | EAX:double | b04e8@b04fc,b52bc@b5332,b686c@b68c4 | Distance between two integer 2D points, 0.0 when coincident. |

## float_helpers8 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb45a2` | EAX:double | EAX:double | 7d20@7d54,7ee8@8029,1d954@1d9e3 | fcos on ST0, retried while the FPU reports an incomplete reduction. |
| `0xb45ac` | EAX:double | EAX:double | 7d20@7d4c,7ee8@8032,1d954@1d9ee | fsin on ST0, retried while the FPU reports an incomplete reduction. |
| `0x7d20` | EAX:ef_addr32,EDX:ef_addr32 | AL:uint8_t | 7dec@7e34,82b8@8336 | Test whether a 2D point lies inside the oriented box of a record. |
| `0x82b8` | EAX:int32_t,EDX:int32_t | EAX:ef_addr32 | 882b0@88304 | Find the first unmarked entry of the cell containing the scaled point and mark it. |

## float_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc964` | none | none | bfcc@c27f | Recompute the ratio of matching entries and store it as a float. |
| `0x16bac` | EAX:int32_t,EDX:ef_addr32,EBX:ef_addr32 | EAX:double | 168d8@1690e,16e60@16f07,16e60@17025 | Pick the best-rated weapon among a record's carried ones, returned scaled. |

## game_helpers59 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x57480` | EAX:ef_addr32 | none | 581b0@5857d | Copy a decoded descriptor into its 0x30-byte table entry. |
| `0x8bb50` | none | none | 3dd30@3dff0 | Per-frame selection refresh; mirrors the saved camera into the unit table. |
| `0x93060` | none | none | 95e58@96bb3 | Point the scene camera at the current weapon station's object. |
| `0x95ae0` | none | none | 95e58@96717 | Set up the scene camera from the pending target record, or a default view. |

## game_helpers60 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xbeb20` | AL:uint8_t | EAX:uint32_t | bed1c@bed9a | Map a keyboard scan code to the 1..23 index of the matching function key, 0 for none. |

## game_helpers61 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe76c8` | none | none | 49228@49bca,8cd78@8cdd2,8cf10@8cf48 | Clear the 0x4322xx..0x432axx working state and set the 0.6 default scale. |
| `0xf2e74` | none | none | f2fbc@f2fc7,f3b78@f3bd0 | Read a plain entry record and bump the record count. |
| `0xf2fbc` | none | none | f3b78@f3bed | Read an entry record, with a trailing value unless the plain flag is set. |

## game_helpers62 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfd404` | EAX:uint32_t | EAX:uint32_t | c4310@c4333 | Redirect any of a fixed set of code addresses to the shared handler 0xfd52f. |

## game_helpers63 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x509bc` | EAX:ef_addr32 | none | none direct | Select the tactical code from a two-digit field and apply or bind it. |
| `0x3dbf0` | none | none | 3dd30@3dfb3 | Flag the player unit as out of the map when its cell leaves the valid range. |
| `0x7a9bc` | none | none | 7a454@7a45f | Replay the 400x400 byte-pair grid through the plane source when it is enabled. |
| `0x5a998` | none | none | none direct | Draw the tactical coordinates of the player unit from the current table row. |

## game_helpers64 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa32e8` | AX:uint16_t | none | a33a4@a33d2 | Request the state matching the current limit, or a status string for a lookup. |
| `0xd3c50` | none | none | none direct | Redraw when the tracked span is large enough and the two edge values differ by more than 0x800. |

## game_helpers65 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdf65c` | none | AL:uint8_t | dfac8@dfb73,dfac8@dfbf6,dfac8@dfe68 | Latch the selected menu entry's geometry; returns 1 when an entry was latched. |
| `0xe3ad0` | none | AL:uint8_t | 97dcc@97ee7 | Reload the halfword table one step at a time; returns whether more steps remain. |

## game_helpers66 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x69a40` | AX:int16_t | AL:uint8_t | 69b70@69ba4 | Test whether any of the low 13 flag bits of a weapon table entry is set. |
| `0x8ef38` | EAX:ef_addr32 | none | 8c49c@8c8c1,8d11c@8d1a3,8d330@8d3b6 | Clear nine named object references for the object's id. |
| `0x17f10` | none | none | 601c@60d3 | Switch the player unit's mode when no blocking state is active. |

## game_helpers67 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x7222c` | AX:int16_t | none | 8d3d4@8d420 | Copy the player's tracked state block into a unit record and refresh it. |
| `0x6697c` | none | none | 66548@665d7 | Pick the cursor mode from the player's selected station kind and the panel mode. |

## game_helpers68 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdf4f4` | none | AL:uint8_t | dfac8@dfdec,dfac8@dfe2d | Step the menu back one level; returns 1 when an earlier entry was re-latched. |
| `0xe393c` | none | none | e3ad0@e3add | Stream the saved table blocks (sizes scale with the count at 0x146e50). |

## matrix_fixed (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfceb4` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | c6b80@c6bda | out = a * transpose(b); inputs in ESI (a), EBX (b), output EDI. |
| `0xfcffb` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | c5cd3@c5cfa | out = a * b; inputs in ESI (a), EBX (b), output EDI. |

## game_helpers69 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x17754` | none | none | 1abbc@1ac77,5c518@5c79e | Track the trigger hold time for the selected station, firing the release notifications. |
| `0x1cde8` | none | none | 3dd30@3dfdb | One simulation frame: input latch, unit updates and the sub-step loop. |

## game_helpers70 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x7f9e4` | EAX:ef_addr32 | AL:uint8_t | 7ef14@7efeb,7ef14@7f02c,7ef14@7f09a | Pick the icon code for a unit record (0x47 for none). |

## game_helpers71 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcbd30` | AL:uint8_t | AL:uint8_t | caa28@cab5f | Weight for a scan code (0 when unmapped); the original is a binary decision tree. |
| `0xe34e8` | none | AL:uint8_t | 3dd30@3e216 | Read back the saved table blocks through the indirect readers; returns whether a record id is set. |

## game_helpers72 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x845c0` | none | AL:uint8_t | none direct | Forward the cursor vector, redoing it once when the hit test accepts the position. |
| `0x944f4` | none | none | 95e58@96bcb | Latch the camera position and place the scene with the negated view angles. |

## game_helpers73 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11d44` | none | none | 601c@60aa,11de0@11e70 | Advance the letter table walk; emits up to 18 letters per step for the active mode. |
| `0x12210` | AX:int16_t | none | 12d90@12de7 | Refresh a group's entries: its flagged head, then every member from the cx-th list on. |

## game_helpers74 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x941e4` | none | none | 95e58@96182 | Fold the view roll into the scene angles and request a refresh at the limits. |
| `0x9586c` | none | none | 95e58@96d0e | Chase-camera distance control driven by the speed input bits. |
| `0x9bba4` | none | none | none direct | Draw the HUD frame: three vertical rails and four border lines. |

## game_helpers75 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc448b` | none | none | c8c34@c4555 | Install the graphics dispatch pointers (including the patched code words). |
| `0xf75e0` | none | none | none direct | Merge the pending flag word into the active record, or clear it. |
| `0xf1c58` | EAX:ef_addr32 | EAX:uint32_t | ac5f8@ac69b | Run the leave callback of the old object and the refresh steps of the new one. |
| `0x10dc7f` | EAX:int32_t,EDX:int32_t | EAX:uint32_t | ea470@ea48e | Switch the mode of one of 12 channels, running the per-channel transition. |

## game_helpers76 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1f64` | none | none | 1b70@1c4a,1cd8@1eb2,1cd8@1ed7 | Select the active entry: a direct code match first, else the best flagged candidate. |
| `0xa66cc` | EAX:ef_addr32,DL:uint8_t | none | a6998@a69b7,a74bc@a74f1,a7588@a759b | Recompute the left/right and top/bottom padding of a layout box. |

## game_helpers77 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x34f94` | none | none | none direct | Per-frame update of the mission dialog page: reload, timers and dialog switching. |

## float_helpers10 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9d72c` | EAX:double | EAX:double | f80@14eb,f80@1525,f80@1566 | Truncate ST0 toward zero (FPU control word temporarily set to round-to-zero). |
| `0x6ebbc` | none | none | 70914@70a46,712e8@71414 | Store the wrapped difference of two angles scaled by a constant. |
| `0x7b040` | EAX:ef_addr32 | EAX:double | 2e184@2e268,6d38c@6d579,6f504@6f60f | Scale a 2D double point, look up the cell value and return it negated and scaled. |
| `0x88fb4` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:double,[ESP+8]:double | none | 29b14@29c90,89060@890cd,899d8@89a44 | Unit direction vector from a heading (a) and pitch (b): (cos a cos b, sin a cos b, -sin b). |
| `0xb4508` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | e6990@e69ba,e6990@e69ce,11a6e4@11aacb | out = transpose(mat) * vec for a 3x3 double matrix stored by rows. |
| `0xb4550` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 1098f0@1099cb,11a6e4@11a883,11c584@11c63a | out = mat * vec for a 3x3 double matrix stored by rows. |

## float_helpers11 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xea038` | EAX:ef_addr32,EDX:ef_addr32 | none | 119f88@119ff0,119f88@11a07f,11a5c0@11a5e8 | out = M * vec with the 3x3 double matrix at 0x464480 (rows are 0x18 bytes apart). |
| `0xe3120` | none | none | 3dd30@3def1 | Sample the tick counter and store the scaled change since the previous sample. |

## float_helpers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xea884` | EAX:ef_addr32 | ST0:float | 9775c@977d4,979c8@97a03 | Length of an integer 3D vector as a float, 0.0 for a negative sum. |
| `0xf8314` | AX:int16_t,[ESP+4]:double | ST0:double | b52bc@b5348 | Divide a value by a word scaled with 0x1932cc, 0.0 when the divisor is zero. |

## leaf_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9d77c` | none | none | 9e7bc@9f3ac,9e7bc@9f508,9e7bc@9f5b1 | 1 byte. No-op. |
| `0x9dddc` | none | none | 7a844@7a84d,7a844@7a9aa | 1 byte. No-op. |
| `0x39180` | EAX:uint32_t | EAX:uint32_t | f2ac0@f2b18 | tail jump. Forward to 0x7a290. |
| `0x56a44` | EAX:uint32_t | EAX:uint32_t | 581b0@58482 | tail jump. Forward to 0x8fbc8. |
| `0x58094` | EAX:uint32_t | EAX:uint32_t | 3dd30@3e071,3dd30@3e092,7e5a0@7e622 | tail jump. Forward to 0xa3f30. |
| `0x9d088` | EAX:uint32_t | EAX:uint32_t | f80@1ab8,2ff44@301c3,30214@3120d | tail jump. Forward to 0xc3f80. |
| `0xcc768` | EAX:uint32_t | EAX:uint32_t | e710@e728,e710@e734,f8f8@f906 | tail jump. Forward to 0xc3f80. |
| `0xab954` | EAX:uint32_t | EAX:uint32_t | a5ef8@a5fb8,a5ef8@a5fe9 | tail jump. Forward to 0xc9fc4. |
| `0xac7c0` | EAX:uint32_t | EAX:uint32_t | 84c84@84c91,84c84@84ca7,84c84@84cee | tail jump. Forward to 0xabec4. |
| `0xc3f70` | EAX:uint32_t | EAX:uint32_t | d68c@d6df,7a3e8@7a3f8,9ccb8@9cd87 | tail jump. Forward to 0xfc795. |
| `0xc3f78` | EAX:uint32_t | EAX:uint32_t | cc744@cc749 | tail jump. Forward to 0xfc354. |
| `0xc3f80` | EAX:uint32_t | EAX:uint32_t | b1e0@b383,7a434@7a442,9ce84@9cecd | tail jump. Forward to 0xfa9d2. |
| `0xcbc9b` | EAX:uint32_t | EAX:uint32_t | ba5c@bad2,ba5c@baf5,42e8c@42ea1 | tail jump. Forward to 0xfe541. |
| `0x11c0a4` | EAX:uint32_t | EAX:uint32_t | 10a580@10a91d,10a580@10a96b | tail jump. Forward to 0xc3f70. |
| `0xe5bfe` | EAX:uint32_t | EAX:uint32_t | 45b60@45b65 | tail jump. Forward to 0x109e6c. |
| `0x101e70` | EAX:uint32_t | EAX:uint32_t | 1065c8@1065ce,110f6f@110f73,11124c@111257 | tail jump. Forward to 0x110dcc. |
| `0x101e78` | EAX:uint32_t | EAX:uint32_t | 1065c8@10660a,1065c8@106628,110f6f@110fca | tail jump. Forward to 0x110dd9. |
| `0x101f70` | EAX:uint32_t | EAX:uint32_t | 103e0c@103e21,105648@10567c | tail jump. Forward to 0x1111e1. |
| `0x102020` | EAX:uint32_t | EAX:uint32_t | 112808@112872,113d10@113f6d | tail jump. Forward to 0x111359. |
| `0x106be0` | EAX:uint32_t | EAX:uint32_t | 106cb0@106cbd,106d34@106d3d,106d34@106d57 | tail jump. Forward to 0xf8248. |
| `0x115984` | EAX:uint32_t | EAX:uint32_t | 105024@105068,105070@10511c | tail jump. Forward to 0xfa9a0. |
| `0x11d24e` | EAX:uint32_t | EAX:uint32_t | 10dc7f@10dcef | tail jump. Forward to 0x120485. |
| `0xc9ec4` | EAX:uint32_t | EAX:uint32_t | a6038@a606f,ac5f8@ac610 | tail jump. Forward to 0xdc2c0. |
| `0x9c23d` | EAX:uint32_t | EAX:uint32_t | 6b8@6eb | tail jump. Forward to 0x9c224. |

## leaf_helpers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa37c4` | AL:uint8_t | none | a3824@a389d | Send a one-byte request with code 8. |
| `0xa37e4` | AL:uint8_t | none | a3824@a38bd | Send a one-byte request with code 9. |
| `0xa3804` | AL:uint8_t | none | a3824@a3883 | Send a one-byte request with code 10. |
| `0xa3f0c` | none | none | 34f94@3502d | Send a zero byte request with code 5. |
| `0xbbae4` | AL:uint8_t | AL:uint8_t | bbdc4@bbdf3 | Map a code 0..26 to 9 or 10 (0 when out of range). |
| `0xbbb34` | AL:uint8_t | AL:uint8_t | bbdc4@bbde6 | Map a code 0..12 to 5 or 6 (0 when out of range). |
| `0xbbbe4` | AL:uint8_t | AL:uint8_t | bbdc4@bbdd9 | Map a code 0..36 to a flag. |
| `0xbbd8c` | AL:uint8_t | AL:uint8_t | bbdc4@bbe00 | Map a code 1..100 to a class 0..8 (0 when out of range). |
| `0x940e0` | none | AL:uint8_t | 94a74@94cdf | Whether the scene record pointer is unset. |
| `0x9c1bb` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | 9be38@9be69,9be38@9be83,9be38@9be99 | Convert a real-mode segment:offset dword to a linear address. |
| `0x5ca60` | none | none | 598e8@5997a | Read the 9th stream value into a local and apply it when valid. |

## game_helpers78 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1a358` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 95e58@95f15 | Enter the mode once: apply the argument to the driver and refresh its table. |
| `0x1a390` | none | none | 95e58@95f1c | Leave the mode: restore the saved driver parameters and refresh its table. |
| `0x3dccc` | none | none | 3dd30@3de1a | Reset the drivers and step the four channels, then refresh the matching table. |
| `0xbc421` | EAX:uint32_t | EAX:uint32_t | 9ea0@a3ca,a3dc@a5aa,357ec@35a99 | Find the list node with the given key and open it; -1 when absent. |
| `0xa61b0` | none | none | c9fc4@c9fd5 | Run the pending handler for every listed object that has one set. |
| `0x120485` | none | none | none direct | Install the runtime hook once. |

## game_helpers79 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf7518` | none | none | none direct | Merge the pending flag word into the second flag field, or clear it (mode 2 skips the refresh). |

## game_helpers80 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x88cd0` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32 | none | 8a1dc@8a20b | Copy three 8-byte fields of an object's detail block into caller buffers. |
| `0x6e320` | AL:uint8_t,EDX:uint32_t | none | 6e390@6e39b | Gather six table dwords for a (kind, slot) pair and forward them. |
| `0x68efc` | none | none | 69fac@69fde | Resolve the name of every populated weapon entry (from index 2) into its halfword id. |
| `0x569b4` | EAX:ef_addr32 | none | 581b0@58464 | Apply a queued position record to its unit and forward the update. |
| `0x4f388` | EAX:ef_addr32,EDX:ef_addr32 | none | 4b1a4@4b1b8,4b1a4@4b1d7 | Bind two values into the named dialog's widgets, then emit its flag and close it. |
| `0x34a98` | EAX:ef_addr32 | EAX:ef_addr32 | 34b2c@34b55,34b2c@34b70,34b2c@34b7c | Rebuild the child dialog list with the given text and return the dialog. |
| `0x32de8` | EAX:ef_addr32,EDX:ef_addr32 | EAX:int32_t | 7a66c@7a68b | List the files matching a path and mask into the 13-byte name table; returns the count. |

## game_helpers81 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8b328` | EAX:ef_addr32,EDX:int32_t,EBX:int32_t | EAX:ef_addr32 | 8b39c@8b3c5,8b39c@8b3f8,8b39c@8b40c | Format a number right-aligned into a field of the given width, with ',' every 3 digits. |
| `0x8b684` | AX:int16_t,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32 | AL:uint8_t | 8b7e8@8b84d | Start (mode 1) or continue a directory scan; returns 1 when no more entries. |
| `0x9bfe3` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:int32_t | EAX:uint32_t | 9be38@9bebb,9be38@9bf12,9be38@9bf34 | Add an offset to a segment:offset pair, renormalising so the offset stays in 0..0xffff. |
| `0x9fdd4` | none | EAX:int32_t | 9fea8@9feb6 | Select the entry with the smallest rating below 10 and activate it; -1 slot ends early. |
| `0xa9878` | EAX:ef_addr32 | none | a9e70@a9fab | Build a status line from three fragments, store the title, and refresh. |

## game_helpers82 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb2504` | EAX:ef_addr32,DX:int16_t,EBX:uint32_t | none | b2560@b261a,b2810@b2898,b28b8@b2940 | Print a movement record with its kind name. |
| `0xb2960` | EAX:ef_addr32,EDX:uint32_t | none | b29a8@b2a08 | Print a classified record with its class name. |
| `0xb2a2c` | EAX:ef_addr32,EDX:uint32_t | none | b1f80@b21df,b2560@b25d9,b2560@b268b | Print a plain coordinate record. |

## game_helpers83 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb3580` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t | none | b3084@b314b | Hex dump 0x640 bytes through the printf sink, 40 values per line. |
| `0xbc471` | EAX:ef_addr32,EDX:uint32_t | none | bc4da@bc54d | Build a temp file name "t" + four hex digits + "_" + two hex digits + ".tmp". |

## game_helpers84 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x32414` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 316b8@316ca,3170c@3172e,31774@317bd | Rotate an integer vector by the angle triple (yaw/pitch/roll words) with a 1.15 matrix. |
| `0xa69d0` | EAX:ef_addr32,EDX:ef_addr32 | none | a6a64@a6a80,a74bc@a74e8 | Clamp a layout box's origin and size inside the limits of its container. |

## game_helpers85 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xbe3f4` | EAX:ef_addr32,EDX:ef_addr32 | none | b0f5c@b118f,b0f5c@b147c | For each flag-less record of the first list copy the id of every record of the second list that shares its position. |
| `0xbe77c` | EAX:ef_addr32,EDX:ef_addr32 | none | be59c@be74e,f8e44@f908d | Find the record whose id matches the object's and move its entries' weights from field 0x23 to field 0x25. |
| `0xbd2f4` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | b0f5c@b10de,b0f5c@b13de | Instantiate every valid record of a list into a destination list, numbering them; returns the count. |

## game_helpers86 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc1bd0` | EAX:uint32_t | EAX:uint32_t | c1c24@c1c6a | Reverse the byte order of a 32-bit word (xor-swap sequence). |
| `0xcb184` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | none direct | Mark up to count pristine records matching type and position as used. |
| `0xcb614` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | none | b0f5c@b12fb | For type-3 table entries clear a flag byte of the indexed sub-entry of the n-th list record. |
| `0xcb984` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | EAX:uint32_t | cb9f4@cba55 | Return 1 when a list record sits at the same position as the object's last table entry and cc1ac accepts the pair. |

## game_helpers87 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd22d0` | EAX:ef_addr32 | AL:uint8_t | d23d8@d2459 | Return 1 when the three records' 16.16 (x,y) points form a clockwise triangle (negative cross product). |
| `0xd1860` | AX:uint16_t | none | d17d0@d17ec,d17d0@d17f9,d17d0@d1800 | Write a 16-bit word to the log in big-endian byte order. |
| `0xd642c` | EAX:int32_t,EDX:ef_addr32,EBX:int32_t | AL:uint8_t | d6478@d653d | Index of the table entry closest to the given value (first minimum wins, limit 0x100). |
| `0xd6024` | none | none | d5ec0@d5eef | Send two controller messages on each of the 16 MIDI channels (reset sequence). |
| `0xdcc98` | AL:uint8_t,DX:int16_t,BX:int16_t,CX:int16_t | AL:uint8_t | dccd8@dcd27 | Look up a 6-bit code in the table at 0x15e1be and dispatch it to cf7c4; returns the entry's count + 1. |
| `0xdd164` | AL:uint8_t,DX:int16_t,BX:int16_t,CX:int16_t | AL:uint8_t | dd1b0@dd203 | Same lookup as dcc98 with code 0xa5 remapped to 0x4e and the second table set. |

## game_helpers88 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc2f80` | EAX:ef_addr32 | none | c3584@c3608 | Fill a 512-byte table: for each of the 8 bit values and 16 nibble patterns write four bytes (the bit value where the pat |
| `0xce578` | EAX:int32_t,EDX:int32_t,EBX:int32_t | EAX:int32_t | ce5f8@ce60f,ce5f8@ce62a | Approximate 3D distance: 15/16 of the largest absolute component plus 3/8 of the other two. |
| `0xd1fc0` | EAX:ef_addr32 | none | d2130@d2144 | Compute (once) the 2D bounding box of an object's vertex list into fields 0x10..0x1c. |
| `0xcb55c` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | none | b0f5c@b146d | Mark records whose map cell (position / 10, 40 cells per row) passes the cc1ac test as used. |

## game_helpers89 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xde580` | EAX:ef_addr32 | none | 284ec@2851f,39204@39244,97dcc@97e2a | Fade the palette in from black over 40 steps. |
| `0xde648` | EAX:ef_addr32 | none | 284ec@285ac,a8958@a89e5,a8a38@a8ba9 | Fade the palette out to black over 40 steps, then clear and reset it. |
| `0xde6c4` | EAX:ef_addr32 | none | a8a38@a8bba,a8bd4@a8f3a | Same as de648 but over 80 steps. |
| `0xe1df0` | EAX:ef_addr32,DX:int16_t,BL:uint8_t | EAX:int32_t | e1958@e1a87,e1958@e1baa,e1958@e1bee | Pixel width of up to len characters of a string for the given font (spaces use the font's space width). |
| `0xfab80` | EAX:ef_addr32 | EAX:int32_t | fbad8@fbbed | Half the number of non-type-1 sub-entries of an object, clamped to 1..2. |

## game_helpers90 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe83d8` | EAX:ef_addr32,EDX:ef_addr32 | none | e853c@e8545,e853c@e854e | Draw the four edges of the rectangle spanned by two 3-word points in the plane of the first point's y. |
| `0xe853c` | EAX:ef_addr32,EDX:ef_addr32 | none | e8484@e850a | Draw a wireframe box: both rectangle outlines plus the four vertical edges. |

## runtime_helpers30 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1092b0` | EAX:ef_addr32 | EAX:uint32_t | e3e50@e3e6c,e3fbc@e3fc5 | Skip blanks, parse a decimal number at *cursor, skip blanks again and store the new cursor. |
| `0x10934c` | EAX:ef_addr32,EDX:ef_addr32 | none | e3f00@e3f0e | Skip blanks and copy an identifier (class bits 0xe0 or '_') at *cursor into dest, NUL-terminated. |
| `0x110894` | EAX:ef_addr32,EDX:ef_addr32,BX:int16_t,CX:int16_t | none | none direct | Decode run-length coded rows: each row starts with a run count, each run is a signed length (negative = literal bytes, p |

## game_helpers91 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xde5f4` | EAX:ef_addr32 | none | a8a38@a8afa,a8bd4@a8d6d | Fade the palette in from black over 80 steps. |
| `0xe7cec` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32 | none | 69094@691c8,8b2d8@8b2f9,d9e50@d9fad | Build a path from the default directory plus two parts, appending the third part unless it equals the string at 0x190c50 |
| `0xea17c` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | ea270@ea306,ea270@ea37a | Build a path from a record's first name, an extra part and the record's second name. |
| `0xeb008` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:ef_addr32 | none | 98658@986b1,98658@9884d,98658@988da | Draw a NUL-terminated string glyph by glyph (6 pixels apart, 6-bit codes). |

## game_helpers92 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe67a0` | EAX:ef_addr32,DX:int16_t,BX:int16_t,CX:int16_t | none | e67a0@e67e2,e6800@e680e | Quicksort driver: partition [lo,hi] with e6684, recurse on the left part and loop on the right. |
| `0x10162c` | EAX:ef_addr32 | AL:uint8_t | 1016e0@101757 | Return 1 when the minimum and maximum of the vertices' z field have the same integer part (high 16 bits). |

## game_helpers93 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd0ebc` | EAX:ef_addr32,DX:uint16_t | none | 1b4a4@1b5ed,26b60@26f5e,27348@27362 | Rotate the matrix's first and third columns by an angle. |
| `0xd0dbc` | EAX:ef_addr32,DX:uint16_t | none | 1b4a4@1b5fc,26b60@26f6b,27348@2736f | Rotate the matrix's second and third columns by an angle. |
| `0xd0fb8` | EAX:ef_addr32,DX:uint16_t | none | 26b60@26f78,27348@2737c,2a1fc@2a2b9 | Rotate the matrix's first and second columns by an angle. |

## game_helpers94 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc10c4` | AX:int16_t,DX:int16_t,EBX:uint32_t,CL:uint8_t,[ESP+4]:ef_addr32,[ESP+8]:ef_addr32 | EAX:uint32_t | c117c@c11ce | Reactivate up to count disabled (state 7) records at the given cell whose code passes the group and set tests; returns h |

## game_helpers95 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdcaa4` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 9f664@9f702 | Pitch and yaw angles of the vector from position b to position a, stored as two words at out. |

## runtime_helpers31 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x106e2c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | d9c30@d9c8a | Run 106d00 on the object, then 106dc4 on it with the second argument. |
| `0x112bbc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 113d10@113e46,11402c@114045 | Reset a table record and point its field 0x14 8 bytes past field 0x10. |
| `0x113cf4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | none direct | Trace-call a non-null record and mark its field 4 as 1. |
| `0x1113d4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 102384@1023a7 | Between the 101e70/101e78 bracket clear one table dword (index from the stack) unless it is -1. |

## runtime_helpers32 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x110b70` | EAX:ef_addr32,DX:int16_t,BX:int16_t,CX:int16_t | none | none direct | Zero len bytes at the start of each of count rows spaced stride bytes apart. |
| `0x111365` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | 101c54@101d7c,102144@102169 | Claim a free slot of the 15-entry table at 0x17d664, store the argument in 0x17d624 and return the slot offset (-1 when  |
| `0x11141f` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 101c54@101da0,1022dc@1022ff | Promote a table slot from state 1 to state 2. |
| `0x111473` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 102330@102353 | Demote a table slot from state 2 to state 1. |
| `0x1114c7` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 1021f4@10221e | Set a slot's field at 0x17d6e4, clear its field at 0x17d6a4 and run 110f6f. |
| `0x111f20` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | EAX:uint32_t | 112c10@112db4,112c10@112f7b,112c10@1130f0 | Read a MIDI variable-length quantity (up to 4 bytes) and advance the cursor stored at the argument. |

## runtime_helpers33 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x115989` | EAX:uint32_t | EAX:uint32_t | 105024@105050 | Size of an open file: remember the position, seek to the end, seek back; -1 when the position query fails. |
| `0x1141b0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32,[ESP+8]:int32_t,[ESP+12]:int32_t | none | 1031cc@1031fc | Start a ramp of the value at +0x48 to a target over time; sets the per-step delta at +0x54. |
| `0x112bd8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 112c10@113222,1142cc@1142fd | Send controller 7 (volume) for each of the 16 channels whose stored value is not -1. |

## game_helpers96 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe7fc0` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32,[ESP+4]:ef_addr32 | none | 7ab00@7ab5c,de864@de907,de864@de940 | _splitpath: normalise '/' to '\\' in place and split into drive, directory, name and extension (each output optional). |
| `0xea9ec` | EAX:ef_addr32,EDX:ef_addr32 | none | 97534@97595 | Transform a 3-vector in place by a 3x3 1.15 matrix (each product divided by 0x8000 separately). |
| `0xebfd0` | EAX:ef_addr32 | EAX:uint32_t | 9dbf0@9dbfd,9dcf8@9dd9c,a1d50@a1d6d | Hash up to the first 15 characters of a name into 32 bits (length, sum, running sum, mix and ordering bits; '_' is skipp |

## game_helpers97 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe80d0` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32,[ESP+4]:ef_addr32 | none | 7ab00@7ab7b,7ab00@7ab95,de864@de9c0 | _makepath: join drive, directory, name and extension into path, adding the missing separators and normalising '/' to '\\ |

## game_helpers98 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe6684` | EAX:ef_addr32,DX:int16_t,BX:int16_t,ECX:uint32_t | EAX:uint32_t | e67a0@e67ce | Partition records lo..hi around the middle record's key (descending when the flag is set); returns the pivot's final ind |

## game_helpers99 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xce550` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t,ECX:int32_t | AL:uint8_t | none direct | Accumulate constant + z-coefficient * c into the plane's 64-bit result; returns 1 when the result is negative. |
| `0xce4dc` | EAX:ef_addr32,EDX:int32_t,EBX:int32_t,ECX:int32_t | EAX:uint32_t | ceb40@cebc2 | Evaluate a*x + b*y + c*z + constant into the plane's 64-bit result; returns the high dword. |
| `0xce498` | EAX:ef_addr32,EDX:int32_t,EBX:int32_t,ECX:int32_t | AL:uint8_t | none direct | Same evaluation as ce4dc; returns 1 when the result is negative. |
| `0x10901d` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | EAX:int32_t | e0b10@e0b39 | Case-insensitive compare of up to n bytes (does not stop at NUL); returns the first difference of the lower-cased bytes. |
| `0x11082c` | EAX:ef_addr32 | none | none direct | Scale 8-bit palette chunks down to 6 bits in place (count byte, then per chunk a skipped byte, a colour count and 3 byte |

## game_helpers100 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd2040` | EAX:ef_addr32 | none | d2130@d21c1 | Compute (once) the 3D bounding box of an object's points into fields 0x30..0x44. |
| `0xb570c` | EAX:ef_addr32,EDX:ef_addr32,BL:uint8_t | none | b7d4c@b7f8f,b80ac@b8167,b80ac@b842c | Fill the current (last added) 0x19-byte table entry of an object from a source record and a flag byte. |
| `0xa2244` | EAX:ef_addr32 | none | none direct | Blend two sets of three words into rec+8..0xc, weighted by the cosine of the record's angle (1.14 weights). |

## game_helpers101 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa2334` | EAX:ef_addr32 | none | none direct | Like a2244 but the cosine weight is taken at the complementary angle and the weights are swapped. |
| `0x9d278` | EAX:ef_addr32 | none | f80@117f,f80@1187,f80@11d2 | Read the next token (blank/comma separated, optionally quoted, at most 0x50 bytes) from the text at ctx using the cursor |

## game_helpers102 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xbfb2c` | AX:int16_t,DX:int16_t,EBX:ef_addr32 | EAX:uint32_t | bfb88@bfca5,c09e4@c0a35,c09e4@c0a5a | Count the idle records at cell (x,y) that pass bcf18 and whose code is in the cc584 set or 10..15. |
| `0x1086f4` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | 1084d3@108552,1084d3@1086bc | Parse the width and size modifiers of a printf conversion ('*', digits, N/F, h/l/w/L) into the spec record; returns the  |

## game_helpers103 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb3830` | AX:int16_t,DX:int16_t,BX:int16_t | EAX:int32_t | cff64@cff75,cff80@d0209 | Approximate 3D distance from three 16-bit components: 15/16 of the largest absolute value plus 3/8 of the other two. |
| `0xbcc2c` | EAX:ef_addr32 | EAX:uint32_t | bd2f4@bd30b | Check that an object has a ready sub-entry (matching its type's code) and set the flag at +0x36; returns 1 or 0. |

## game_helpers104 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xed3b4` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | EAX:ef_addr32 | ed133@ed163 | Parse flags (ed4e7), width ('*' or digits), precision ('.' then '*' or digits) and size modifier (N F L h l w) into the  |

## game_helpers106 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd65e8` | none | none | 284ec@284f2,39204@39211,3dccc@3dccf | Upload an all-zero (black) 0x300-byte palette. |
| `0xcc7d0` | AX:int16_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | b0f5c@b154b | Forward two stack arguments to cc90c with the 16-bit register argument sign-extended. |
| `0xf8600` | AX:int16_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t | none | b0f5c@b158e | Forward four stack arguments to f9448 with the 16-bit register argument sign-extended. |

## game_helpers107 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x110b30` | EAX:ef_addr32,EDX:ef_addr32,BX:int16_t | none | none direct | Copy 200 lines of 0x140 bytes with the destination advancing by a 16-bit stride per line. |
| `0x1e0ec` | EAX:ef_addr32,DX:int16_t | none | 1df90@1e0e1 | Look up an event code for a record (position >> 8 as a 3-vector) and dispatch it unless the lookup returns -1. |
| `0x11004b` | none | EAX:uint32_t | 1100b1@1100b3 | DOS get-time call (int 21h, AH=2Ch) converted to hundredths of a second since midnight. |

## game_helpers108 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcfa24` | EAX:uint32_t,EDX:ef_addr32,EBX:int32_t,ECX:int32_t | EAX:ef_addr32 | cfa9c@cfb21,cfc68@cfd02 | Return the first of count 0x4a-byte entries whose bounding box contains (x,y) and which passes the cf960 test, else 0. |
| `0xcbf20` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | b0f5c@b1693 | Concatenate the data blocks (length at +9, bytes at *(+0xc)) of a record list into one new buffer; the first block's len |

## game_helpers109 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdcf28` | AL:uint8_t,DX:int16_t,BX:int16_t,CX:int16_t | AL:uint8_t | dd26c@dd2de | Draw the glyph for a character code (0xa5 maps to 0x4e, codes above 0x7f use the extended tables) through cf7c4; returns |

## game_helpers110 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x49144` | AX:int16_t | none | 3a3a8@3a566,646d4@6479b | Draw the outline of widget record idx: its centre (x,y) and size (w,h) become a rectangle; mode 0 when the record's kind |
| `0x22e98` | AX:int16_t | none | 1f960@1f968 | Highlight widget record idx: when its slot byte is set, look the text slot up and enter scroll mode, then draw the recta |

## game_helpers111 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3df8` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | none | 3668@36de,3668@3718,3668@37a2 | Forget the tracked target (*current = 0) when the new position is missing or farther than limit * 32 from it. |
| `0x10f107` | EAX:ef_addr32 | EAX:int32_t | 11580e@115831 | fgetc: read the next byte of a stream (refilling when the count runs out); in text mode skip a CR and treat Ctrl-Z as en |

## game_helpers112 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc08d4` | AL:uint8_t,EDX:ef_addr32,EBX:ef_addr32 | none | c0b04@c0fba,c0b04@c0ff9 | For each record at the object's position with the given type, reactivate state-7 records (counting them in the object's  |
| `0xc09e4` | EAX:ef_addr32,EDX:ef_addr32 | none | c0b04@c0fc3,c0b04@c100c,c0b04@c1020 | Refill an object's sub-entries of kind 0x35/0x37, then reactivate matching non-idle records up to the free capacity of i |

## game_helpers113 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcac4c` | EAX:ef_addr32 | none | b0f5c@b12e5 | Add a random amount (mod 35 for kinds 0x35/0x37, mod 10 for elite records, else mod 5) to every sub-entry counter below  |

## game_helpers114 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xbcda0` | EAX:ef_addr32 | AL:uint8_t | bcf44@bd152 | Count the ready sub-entries of an object (see bcc2c); types 0xb/0x56 also clear the flag at +0x36. |

## game_helpers115 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb0cfc` | EAX:ef_addr32,DX:int16_t,BL:uint8_t,CX:int16_t | none | b0f5c@b12c2 | Shuffle pass over a word array: for each iteration pick two random positions and swap them with the xor trick. |

## game_helpers116 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb39ec` | EAX:int32_t,EDX:int32_t,EBX:int32_t | EAX:int32_t | 23760@23786,237c8@237ee,834e8@8363d | Approximate 3D distance of three components (15/16 of the largest magnitude plus 3/8 of the other two). |
| `0xa4eec` | EAX:ef_addr32 | none | a59d8@a5a08 | Compute the current and maximum load figures of a loadout record from its flag bytes (+0x3a/+0x3b) and raise the stored  |

## game_helpers117 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc07a0` | EAX:ef_addr32,EDX:ef_addr32 | none | c0b04@c0f32,c0b04@c0fa0,c0b04@c0fe1 | Disable (state 7) every record at the object's position with the object's type, decrementing the object's counters. |

## runtime_helpers35 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd170c` | EAX:uint32_t | none | d1774@d17a8,d17d0@d17df,d1894@d18b5 | Reverse the byte order of a dword and pass it to d16fc. |
| `0x117ed4` | EAX:ef_addr32,EDX:ef_addr32,BX:uint16_t | EAX:uint32_t | 117f68@11801d,117f68@118031 | Attach an object to a holder, allocating the holder's block when missing, and reset the object's header words (last word |
| `0x1142cc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32,[ESP+8]:uint32_t | none | 1033c8@1033f2 | When the value at the argument changes, resend the channel volumes of every active (state 4) channel of the player. |

## game_helpers118 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |

## game_helpers118 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb58a8` | EAX:ef_addr32,DX:int16_t,BX:int16_t | EAX:int32_t | 4fa64@4fb14,b5640@b565a,b5800@b587b | Search the object's slot table (9-byte entries, count at +0x48, flag byte at +3 meaning used) from start for a slot whos |

## runtime_helpers34+game_helpers105 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x112048` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32,[ESP+8]:uint32_t,[ESP+12]:uint32_t | EAX:uint32_t | none direct | Read a stored MIDI channel value: program change (0xc0), pitch bend (0xe0, two 7-bit halves) or a controller (0xb0) from |
| `0xf9dcb` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | f9ed2@f9ed8 | asctime: format a broken-down time record into "Www Mmm dd hh:mm:ss yyyy\n"; returns the buffer. |

## game_helpers119 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa4470` | none | AL:uint8_t | 3c74@3cbb,601c@6148,1dc20@1de11 | Return the constant 3. |
| `0x7aa2c` | none | none | 7ab00@7ab9a | Register the status description built for the string at 0x187c00. |
| `0xd5c84` | EAX:int32_t | EAX:uint32_t | c4310@c4341 | Map an address inside the range 0xd3900..0xd5c84 to 0xc454a; any other value is returned unchanged. |

## game_helpers120 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x114170` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 1031a0@1031c3 | Activate a record: trace it, set state 2 and run its field 0x28 callback (cdecl, record on the stack). |
| `0x1115fc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | none direct | Tear down a record: run its field 0x874 callback, trace-call 0x102644 if field 0x88c is positive, then set it to -1 (cde |
| `0xd660c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | EAX:int32_t | eac48@eacbc,eac48@ead32 | Remap the 256-entry palette at the stack argument and compare the result with the 0x300 bytes at 0x420a90 (cdecl; return |
| `0xd6df0` | none | none | d6dc4@d6ddc | Register the stack ranges 0xd7108 (0x48 bytes) and 0x1048b4 (0x400 bytes). |
| `0xdc8d8` | EAX:int32_t,EDX:int32_t | EAX:int32_t | dc91c@dc9a9,dc91c@dc9bf | Angle of the vector (x, y) after halving it until both components fit in 16 bits. |

## game_helpers121 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1068b8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 1027f8@10281b | Deactivate a record that is not yet state 1 or 2: set state 2, run the callbacks at 0x84c and 0x850 (cdecl, record on th |
| `0xeb698` | EAX:ef_addr32,EDX:ef_addr32 | none | eb9f8@eba16,eba64@eba9c,ebb58@ebb92 | Seek the context's handle to the given position; on a position change first reread the 8-byte header into context+0x148  |
| `0xa3424` | AX:uint16_t | none | a348c@a34c4 | Copy the 0x21-byte name of table entry (index << 5) at 0x13587c into the buffer of the slot it names and flag that slot  |

## runtime_helpers36 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x113398` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | none direct | Release a record container: trace each 0x718-byte element, trace field 0xc, free the element array and the 0x1d4-byte co |
| `0x11580e` | EAX:ef_addr32,EDX:int32_t,EBX:ef_addr32 | EAX:ef_addr32 | 103f04@103f5c | fgets: read at most n-1 characters up to and including a newline into buf from stream; returns buf, or 0 on end of file  |

## game_helpers122 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x69b70` | none | none | 69fac@69fe3 | Walk the widget chain starting at the named widget at 0x186320 under the object at 0x186314; for each entry whose weapon |
| `0xfaafc` | EAX:ef_addr32 | EAX:uint32_t | fbad8@fbc73 | Count the active elite 0x15-byte entries of a group whose code is not in the set; the original then always returns 1. |

## game_helpers123 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd1894` | EAX:ef_addr32,DX:int16_t | none | d1c54@d1c83 | Log a 256-entry palette: header text, dword 0x300, then three values per entry (a fixed 10/20/30 when mode == 2, otherwi |
| `0xb5800` | EAX:ef_addr32,EDX:ef_addr32,BX:int16_t | none | b7d4c@b800e,b80ac@b81be,b80ac@b8487 | Recompute the frame index of the current 25-byte slot (object+0x20 table, 1-based index at +0x1c) and then find a free s |

## game_helpers124 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xceb40` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:int32_t,[ESP+8]:int32_t,[ESP+12]:int32_t,[ESP+16]:int32_t | EAX:uint32_t | c46f8@c4710,c473c@c4743 | Test a point (z, y, x on the stack after a margin) against the five planes at 0x15b79c: returns 1 when a coordinate is o |
| `0x111575` | none | none | 110fd4@110feb | Register the two code ranges 0x17d624..0x17e1d4 and 0x110dcc..0x111575 with the stack range tracker. |

## game_helpers125 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1f7d4` | AX:int16_t,DX:int16_t,BX:int16_t,CX:int16_t,[ESP+4]:int16_t | none | 1ff0c@2011c,1ff0c@20491,2065c@20aef | Draw a rectangle outline with four tactical lines of marker a; x spans b..d, y spans c..e (e on the stack). |
| `0xfeec8` | EAX:ef_addr32 | none | d663c@d6652 | Upload a 256-entry RGB palette to the VGA DAC (ports 0x3c8/0x3c9), sending only entries that differ from the shadow copy |
| `0x1052f0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 10585c@105c7f,10683c@106870,106894@1068ac | Once per record: send message 0x401 with the two words at +0x14 and +0x18 (cdecl, record on the stack). |
| `0x111df0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 111e40@111e65,111ed4@111eda,111ed4@111f16 | If a pending count is set at +0x1a8, send message 0x502 with it and clear the count and +0x1ac (cdecl, record on the sta |

## runtime_helpers37 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x117e0c` | EAX:ef_addr32,EDX:int32_t | EAX:int32_t | 106f10@106fcd,118290@11836f | Program the serial port at obj+0x1c with a divisor derived from the rate (clamped 300..115200); returns the actual rate, |
| `0x10f43b` | EAX:uint32_t,EDX:ef_addr32 | EAX:int32_t | f1f5a@f20e3 | fputc: store a character in the stream buffer (expanding newline to CR LF on text streams), flushing when full or line b |
| `0x104464` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 102488@1024ab | Destroy a record: run its field 0x24 callback with field 0x28, trace-call fields 0x20 and 0x1c, send message 0x306, free |

## runtime_helpers38 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfa8a0` | EAX:ef_addr32 | EAX:int32_t | bc4da@bc4fe,f1f5a@f207d,f1f5a@f2112 | fflush: write out a dirty write buffer, or seek back over unread input; resets the buffer position and returns 0 or -1. |

## game_helpers126 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb1bb0` | [ESP+4]:uint32_t,[ESP+8]:ef_addr32 | none | 5630@56e0,5630@56ee,5630@56fc | printf to a handle (100..105): format into a 0x7c-byte buffer and write it (cdecl, variadic; all arguments on the stack) |

## game_helpers127 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb2f48` | EAX:ef_addr32,EDX:uint32_t | none | b3038@b3072 | Dump one group record: its id, type name, extent words and, when any member byte at +5 is not 0x64, the member bytes. |
| `0xb3038` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t | none | b3084@b30ee | Dump a group list: heading with a title, the entry count, then each group via 0xb2f48. |
| `0xb1c34` | EAX:ef_addr32,EDX:uint32_t,EBX:ef_addr32 | none | b3084@b310c | Dump a numbered entry list: the format string, the entry count, then each entry (numbered from 0 in word +0x13) via 0xb1 |
| `0xb2e28` | EAX:uint32_t,EDX:ef_addr32,BX:int16_t,ECX:ef_addr32 | none | b3168@b31d3 | Dump a point list: the format string, a value, then each entry via 0xb2e98. |

## game_helpers128 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb23bc` | EAX:ef_addr32,DX:int16_t,EBX:uint32_t,CX:int16_t | none | b2560@b26a9 | Dump one 0x13-byte record to a handle: an index or fallback value, its kind name, an optional dword and six signed words |
| `0xb059c` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | b0f5c@b11a0,b0f5c@b149f | Classify every entry of the list: entries of kind 0x31 whose type is 2 get state 2 if some other qualifying entry matche |

## game_helpers129 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x109e6f` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | none | e6144@e639c,f4347@f4469,f44b6@f454a | memmove: copy n bytes from src to dst, copying backwards when the ranges overlap with dst above src. |
| `0x111e40` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t | none | 1123a4@1127f7,1133f8@113917,1133f8@113929 | Append a MIDI event (status, data1 and, for three-byte events, data2) to the record's buffer at +8 (offset 0x100, fill c |
| `0x111ed4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32,[ESP+8]:ef_addr32,[ESP+12]:uint32_t | none | 112c10@112fa0 | Queue a raw MIDI block (at most 0x200 bytes) into the record's buffer, flushing before and after (cdecl: record, data, l |
| `0xdc91c` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | 26b60@26f1a,30214@3119b,3122c@314ad | Store the elevation and heading angles of the vector from point b to point a (three dwords each) into two words at out:  |
| `0xc117c` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | EAX:int32_t | c1210@c12a0 | Whether the record may be reactivated: its cell holds more than 12 records and reactivating at most 0x30 - count of them |

## game_helpers130 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb4fb4` | EAX:ef_addr32,EDX:ef_addr32 | none | b47e4@b4829 | Initialise the sub-records (0x7c bytes each, count at +0x12, array at +0x13) of an object from a template: allocate each |
| `0xb31f0` | EAX:ef_addr32,EDX:uint32_t | none | b3168@b31dc | Dump the area list: the number of matching entries (type 0x14 whose first byte pairs with the entry's kind), then each m |

## runtime_helpers39 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10eef0` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | 10eda1@10f055,10eda1@10f065 | Parse a TZ rule ("Jn", "n" or "Mm.w.d", optionally "/h:m:s") into the record at out; returns the text position after the |
| `0x111f84` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32,[ESP+8]:int32_t | EAX:ef_addr32 | 113d10@113d36 | Walk a chunk stream (4-byte tags, big-endian sizes): find the n-th chunk of the tag at 0x1953c8 inside the container tag |

## runtime_helpers39 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10ec62` | EAX:uint32_t,EDX:uint32_t,EBX:int32_t,ECX:ef_addr32 | EAX:ef_addr32 | f118d@f1257,1100d2@1100f0,1100d2@110117 | Break a day count and seconds-of-day (minus a zone offset) into a broken-down time record: +0 sec, +4 min, +8 hour, +0xc |

## runtime_helpers40 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x117aa2` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 105648@105713,10585c@1058b3,10585c@105c60 | Zero the buffer at +0x50 (length at +0x4c) of a record (cdecl, record on the stack). |
| `0x105da0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | none direct | Destroy a sound object: if it was started, stop it, clear its buffer, run 0x117a0f twice, trace 0xa; then trace its fiel |
| `0x114058` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 103148@10316b | Reset the controllers of a state-4 sound record: set state 8, run 0x112808, then for each of its 16 channels send contro |
| `0x10f1bf` | EAX:ef_addr32 | EAX:int32_t | f1d90@f1ec2,f1d90@f1ee2,f1d90@f1f0f | Refill a stream buffer (_filbuf): allocate it if missing, flush stdout-style linked streams, then read from the handle ( |

## runtime_helpers41 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1149e0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:int32_t | none | 103c28@103c6d | Dispatch a MIDI event for a sound record (cdecl: record, port, status, data1, data2): with a non-zero port forward it to |

## runtime_helpers42 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfa815` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | EAX:int32_t | bc400@bc416,cd12e@cd145 | vfprintf: format into a stream through 0xed133 with the putc callback at 0xfa804, temporarily making an unbuffered-flagg |
| `0xbc400` | [ESP+4]:ef_addr32,[ESP+8]:ef_addr32 | EAX:int32_t | 9ea0@a012,9ea0@a0c6,9ea0@a119 | fprintf (cdecl, variadic). |
| `0xcd12e` | [ESP+4]:ef_addr32 | EAX:int32_t | fecc@ff8b,1001c@1034d,1001c@10524 | printf to the standard output stream at 0x16043e (cdecl, variadic). |
| `0xfa485` | [ESP+4]:ef_addr32,[ESP+8]:uint32_t | EAX:int32_t | 105024@105036,105070@1050ce | open: forward name, flags and the optional permission argument to 0xfa4a9 with share mode 0 (cdecl, variadic). |
| `0xa7b3c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | EAX:int32_t | 35cfc@35d6b,35cfc@35da6,36054@360a1 | File size: open the named file in a handle slot, seek to its end, close it and return the end position, or -1 when it ca |

## game_helpers125 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd663c` | EAX:ef_addr32 | none | 1a358@1a385,1a390@1a3c0,1e480@1e4fc | Remap a 256-entry palette, upload it to the VGA DAC and keep a copy of the source at 0x420a90. |
| `0xd710c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | none direct | If a record's mode (+0x30) is 1, register its stack range (+8, length +0x10 minus 1) and set its field to 0x40 (cdecl, r |

## game_helpers131 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x48c34` | [ESP+4]:uint32_t,[ESP+8]:ef_addr32 | EAX:int32_t | 11e7c@11f84,11e7c@12067,150d4@1513c | Width of a formatted string: mode 1 uses the proportional font width, mode 2 six pixels per character, anything else eig |
| `0x3d308` | [ESP+4]:int32_t,[ESP+8]:int32_t,[ESP+12]:ef_addr32 | none | 3d4b0@3d55e,3d4b0@3d576,3d4b0@3d592 | Draw a formatted string at character cell (col, row) with a one-pixel shadow: col * 8, row * 9 pixels (cdecl, variadic). |
| `0xebb10` | EAX:uint32_t,EDX:ef_addr32,EBX:ef_addr32 | AL:uint8_t | 9cc28@9cc4a,9cc28@9cc6e | Read an entry's block: find the directory entry for the key in the context, seek the context's handle to the entry posit |
| `0xbeebc` | EAX:ef_addr32,DX:int16_t,EBX:ef_addr32,ECX:ef_addr32,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:uint32_t | EAX:uint32_t | bc610@bc793 | Run 0xbed1c once for every entry of a list (count from 0xbc5d0), passing the index as a byte in EBX and forwarding the other arguments; returns the last result|

## game_helpers131 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x48bb0` | [ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:int32_t,[ESP+16]:int32_t,[ESP+20]:ef_addr32 | none | 11e7c@1212b,150d4@15175,1e7c0@1eab1 | Draw a formatted string at (x, y) with the renderer chosen by mode (1: 0x15a14, 2: 0xe5db4, otherwise 0xe605c); the firs |
| `0x7e01c` | AX:int16_t,DX:int16_t | none | 7e0a4@7e13f,7e0a4@7e1b5 | Position the numbered child widgets of the container at 0x188034: hide children "0".."8" at (0, 0), then place children  |
| `0x97d10` | AX:int16_t,DX:int16_t,BX:int16_t,CX:int16_t | none | 97dcc@97ea1 | Set the view origin to the centre of the box (x1, x2, y1, y2), latch its corners and run the 0xdc71b / 0xcdf48 box updat |

## runtime_helpers40 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11402c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | none | 10311c@10313f | Start a record that is not yet state 1: trace-call it, reset its table record and link, and set state 4 (cdecl, record o |

## game_helpers131 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x109dd0` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t,ECX:int32_t | none | 109e08@109e33,109e40@109e63 | Copy count rows of len bytes from src to dst, advancing both by the row pitch at 0x1631f4. |
| `0x109e40` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:int32_t | none | e516c@e51b1 | Copy count rows of len bytes at row offset table[idx] + x, from the buffer returned by 0xc382c into the one at 0x15b754. |

## game_helpers132 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd1f88` | none | none | c448b@c4544 | Store the four resource addresses 0xd3c50, 0xff498, 0xd4250 and 0xff81c at 0x15b87c..0x15b888. |
| `0x40e20` | none | none | none direct | Copy the 12-byte record at 0x184358 to 0x41f30c, set the pending name to 0x184364, clear the word at 0x41eeda and store  |
| `0xa4342` | [ESP+4]:ef_addr32,[ESP+8]:ef_addr32 | EAX:int32_t | 20ec@268e,20ec@26c6,20ec@2755 | sprintf: format into buf (callback 0xa432f) and terminate it (cdecl, variadic). |
| `0xd5fc8` | EAX:uint32_t | none | d5d18@d5da9 | When the flag at 0x15b968 is set, trace-call 0x1033c8 with the pointer at 0x420a6c and the value, and remember the low b |

## trace_wrappers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x101ddc` | none | EAX:uint32_t | d5d18@d5d4f,d6e58@d6ef3 | AIL trace wrapper for 0x104694 (cdecl, all arguments on the stack). |
| `0x101e14` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | d6e58@d6eaa,d6e58@d6eb6,d6e58@d6ec2 | AIL trace wrapper for 0x1041e4 with result trace (cdecl, all arguments on the stack). |
| `0x101e80` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | 104200@104373 | AIL trace wrapper for 0x111118 with result trace (cdecl, all arguments on the stack). |
| `0x101f08` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | EAX:uint32_t | none direct | AIL trace wrapper for 0x1111b1 (cdecl, all arguments on the stack). |
| `0x101f88` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t | EAX:uint32_t | 103e0c@103e39,104200@1043b3,104464@1044ad | AIL trace wrapper for 0x11124c with result trace (cdecl, all arguments on the stack). |
| `0x102144` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | e28e8@e2939,104200@1043d8,105e38@10635b | AIL trace wrapper for 0x111365 with result trace (cdecl, all arguments on the stack). |
| `0x102198` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | 104200@104431,105e38@106405,1133f8@1138f0 | AIL trace wrapper for 0x1113ac with result trace (cdecl, all arguments on the stack). |
| `0x1023d8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | 105e38@105f8e,1133f8@1135bf | AIL trace wrapper for 0x104504 with result trace (cdecl, all arguments on the stack). |
| `0x10242c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | 106464@10649b,113a40@113a77 | AIL trace wrapper for 0x104200 with result trace (cdecl, all arguments on the stack). |
| `0x1024b4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | d6e58@d6ee7 | AIL trace wrapper for 0x106514 with result trace (cdecl, all arguments on the stack). |
| `0x102508` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | 106514@10655b | AIL trace wrapper for 0x106464 with result trace (cdecl, all arguments on the stack). |
| `0x102590` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | d6e58@d6f41 | AIL trace wrapper for 0x1065c8 with result trace (cdecl, all arguments on the stack). |
| `0x1026fc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | EAX:uint32_t | d7158@d7209,d72b4@d7388 | AIL trace wrapper for 0x106718 (cdecl, all arguments on the stack). |
| `0x102738` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | EAX:uint32_t | d7158@d71f1,d72b4@d7337 | AIL trace wrapper for 0x106740 (cdecl, all arguments on the stack). |
| `0x1028f4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | d7594@d75ad,d75fc@d7615 | AIL trace wrapper for 0x106708 with result trace (cdecl, all arguments on the stack). |
| `0x102dbc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | d7158@d7288,d72b4@d7373 | AIL trace wrapper for 0x106938 with result trace (cdecl, all arguments on the stack). |
| `0x102f60` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | d5d18@d5d43 | AIL trace wrapper for 0x113af0 with result trace (cdecl, all arguments on the stack). |
| `0x102fb4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | 113af0@113b37 | AIL trace wrapper for 0x113a40 with result trace (cdecl, all arguments on the stack). |
| `0x10303c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | d5d18@d5d7e | AIL trace wrapper for 0x113c7c with result trace (cdecl, all arguments on the stack). |
| `0x1030bc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | EAX:uint32_t | d5e0c@d5e73 | AIL trace wrapper for 0x113d10 with result trace (cdecl, all arguments on the stack). |
| `0x1031cc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | EAX:uint32_t | none direct | AIL trace wrapper for 0x1141b0 (cdecl, all arguments on the stack). |
| `0x1034b0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | d5d18@d5dbf | AIL trace wrapper for 0x113b78 with result trace (cdecl, all arguments on the stack). |
| `0x103778` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | EAX:uint32_t | d5fa8@d5fbc,112c10@113190 | AIL trace wrapper for 0x11451c with result trace (cdecl, all arguments on the stack). |
| `0x103b08` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | 1123a4@112710 | AIL trace wrapper for 0x1146ec with result trace (cdecl, all arguments on the stack). |
| `0x103c28` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:uint32_t | EAX:uint32_t | d605c@d6069 | AIL trace wrapper for 0x1149e0 (cdecl, all arguments on the stack). |
| `0x1023b0` | none | EAX:uint32_t | 104694@1046b5 | AIL trace wrapper for 0x1113fb (no arguments, tail call). |

## hotspot_draw (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x447ec` | EAX:ef_addr32,EDX:ef_addr32 | none | 45574@455aa,4bf1c@4bf78 | Draw the hotspot markers for list type 0x1d. |
| `0x4496c` | EAX:ef_addr32,EDX:ef_addr32 | none | 45574@455fd,4bf1c@4bfb7 | Draw the hotspot markers for list type 0x13. |
| `0x44c68` | EAX:ef_addr32,EDX:ef_addr32 | none | 45574@455b3 | Draw the hotspot markers for list type 0x2b. |
| `0x44de8` | EAX:ef_addr32,EDX:ef_addr32 | none | 45574@45610,4bf1c@4bfcd | Draw the hotspot markers for list type 0x26. |
| `0x450f4` | EAX:ef_addr32,EDX:ef_addr32 | none | 45574@455eb | Draw the hotspot markers for list type 0x5. |
| `0x45274` | EAX:ef_addr32,EDX:ef_addr32 | none | 45574@4563e,4bf1c@4bfe3 | Draw the hotspot markers for list type 0x49. |
| `0x453f4` | EAX:ef_addr32,EDX:ef_addr32 | none | 45574@455bc,4bf1c@4bf8e | Draw the hotspot markers for list type 0x4f. |

## box_rows (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x99ee4` | none | none | none direct | Draw the box rows for the table at 0x40cdf6 (count word at 0x40d02a). |
| `0x99fe0` | none | none | none direct | Draw the box rows for the table at 0x40cefe (count word at 0x40d02c). |
| `0x9a0dc` | none | none | none direct | Draw the box rows for the table at 0x40cd7e (count word at 0x40d026). |
| `0x9a1d8` | none | none | none direct | Draw the box rows for the table at 0x40ce86 (count word at 0x40d02e). |

## hotspot_draw (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x43834` | EAX:ef_addr32,EDX:ef_addr32 | none | 4bf1c@4c051 | Draw the extent markers for list type 0xe. |
| `0x439a4` | EAX:ef_addr32,EDX:ef_addr32 | none | 4bf1c@4c067 | Draw the extent markers for list type 0x17. |
| `0x43c84` | EAX:ef_addr32,EDX:ef_addr32 | none | 45574@45676,4bf1c@4bff9 | Draw the extent markers for list type 0xf. |

## colour_tables (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xba9c4` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key. |
| `0xbaa4c` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key (identical copy). |
| `0xbaad4` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key (identical copy). |

## colour_tables2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xba2c0` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xba460` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xba580` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xba6a0` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xba804` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xba8dc` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xbab5c` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xbabd0` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xbac90` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xbad70` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xbae78` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xbaf48` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xbafe4` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xbb0ac` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |
| `0xbb1ac` | EAX:ef_addr32,DL:uint8_t | none | bb324@bb47d | Fill a colour record for a key 1..7 (other keys use the default shade). |

## code_maps1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3aa8` | EAX:uint32_t | AL:uint8_t | 2e04@2edf,2e04@2f95,3d74@3d94 | Map a code (1-based) to a constant via a jump table (codes outside the range give 26). |
| `0x3b1c` | EAX:uint32_t | AL:uint8_t | 3d74@3dcd | Map a code to a constant via a jump table (codes outside the range give 26). |
| `0x74448` | EAX:uint32_t | EAX:uint32_t | 74718@74878 | Map a code to a constant via a jump table (codes outside the range give 1). |
| `0xba0e4` | EAX:uint32_t | AL:uint8_t | b4ee0@b4f03,b9e9c@b9eb0,bb7c0@bb7dc | Map a code (1-based) to a constant via a jump table (codes outside the range give 0). |

## small_wrappers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd605c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | d6024@d6038,d6024@d604a | Forward three register values to the AIL trace call 0x103c28 with the global handle at 0x420a6c. |

## small_wrappers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa1cb4` | none | none | 7a844@7a9a5,7a9bc@7aa22,a1bf4@a1c82 | Close the log handle at 0x132b18 (writing a -1 marker first when 0x132b1c is 1) and mark it closed. |
| `0x105024` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | EAX:uint32_t | 105070@105080,106464@106491,113a40@113a6d | Open a file and return its size; the error code is left at 0x17d2d8 (3 open failed, 5 size failed). |
| `0xa1a40` | EAX:ef_addr32 | EAX:int32_t | 2fbc4@2fc63,3c730@3c734,74228@74245 | Look a name up in the pointer table at 0x417180 (sentinel stored at the count slot) and return the matching 16-bit value |

## small_wrappers3 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xaa2f8` | EAX:ef_addr32 | EAX:uint32_t | aa350@aa375 | Return 1 when the row (+0xb) or column (+0xc) bytes of a record fall outside the list limits, else 0. |
| `0x10922c` | EAX:ef_addr32 | none | 1093d8@109428 | Lowercase a name, plant it as a sentinel entry of the 8-byte table at 0x4dd898 and store the value of the first matching |

## small_wrappers4 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11288c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32,[ESP+8]:uint32_t | none | 1123a4@11274e | Release every slot of a record whose id (+0x558) matches, then send the pending-count message. |
| `0x77c34` | none | none | 79128@79171 | Shrink the pending line count at 0x1f8d52 to the limit at 0x127d92 and redraw the uppercased message line. |
| `0xeb0b0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:uint32_t,[ESP+24]:ef_addr32 | none | e4c9c@e4f79 | Copy rows of bytes into the framebuffer at (x, y), advancing the destination by the row stride at 0x1631f4. |

## small_wrappers5 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xeb050` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:ef_addr32 | none | 98658@986e3,98658@98877,98658@9890c | Copy a packed buffer of rows to the framebuffer at (x, y). |
| `0xeb1f0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:ef_addr32 | none | 98ac0@98b09,98ac0@98b36,98ac0@98b51 | Copy rows from the framebuffer at (x, y) into a packed buffer. |

## script_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1093d8` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t | none | e4010@e40cf | Run a line-oriented text script: each non-empty line not starting with ';' is passed to the current handler (0x4dd890),  |
| `0x9cbc4` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 6b50@6be1,371b0@37216,3c6c0@3c705 | Try the two optional handlers (0x40d1f0 gated by bit 0 and 0x40d1f4 gated by bit 1 of 0x132ab0); return 1 if one accepts |

## fill_rects1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xceccc` | EAX:uint32_t,EDX:int32_t,EBX:int32_t,ECX:int32_t,[ESP+4]:int32_t | none | 14d58@14e04,1e480@1e54c,1e480@1e5c2 | Fill the rectangle x0..x1, y0..y1 (coordinates swapped into order) with a colour byte, one row at a time. Callee pops th |

## key_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1101e8` | EAX:uint32_t | EAX:int32_t | fa4a9@fa560,fa95f@fa98b,fe541@fe555 | Store a key code, then translate it (via a table at 0x17d5a8 for plain keys, or its high byte for extended codes) and st |

## table_switches1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8c988` | EAX:ef_addr32 | none | 8c49c@8c780,8d11c@8d1aa | Report a tactical action for a record whose mode word (+0x45) selects one of nine action codes. |
| `0xcc51c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | bed1c@bee18,bf444@bf498,bf81c@bf8db | Search a linked list (next at +4) for a node whose id byte (+0xb) equals id and whose state byte (+0x2e) satisfies the t |

## dos_slots1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x7b4` | EAX:uint32_t | none | cd554@cd5b5,e0a90@e0ae3,118290@1182aa | Release hook slot idx (14-byte records at 0x1960c4): restore its vector, clear the pointer and push idx on the free list |

## dos_slots1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x106dc4` | EAX:ef_addr32,EDX:uint32_t | EAX:uint32_t | 106e2c@106e38 | Wait on a ring record until its data arrives (0), the end marker 0x4f (0) or 0x45 (8) is popped, or the timeout expires  |

## colour_tables3 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xba77c` | EAX:ef_addr32,EDX:uint32_t | none | bb324@bb47d | Fill a six-byte colour record chosen by a 1-based key: two leading bytes from the key, four trailing bytes of 4, except  |

## draw_loops1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x461e8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 76e2c@76f47 | Call 0x46254 for each of the seven 16-bit entries (+0x4e) of a 0xca-byte record. |

## unit_events1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x88664` | EAX:ef_addr32,EDX:uint32_t | none | 29dd0@29e00,29dd0@29e84,29dd0@29f3c | Raise unit event 0x20 (code 2) for a unit record. |
| `0x886d0` | EAX:ef_addr32,EDX:uint32_t | none | 29b14@29c1a,29b14@29db6 | Raise unit event 0x1f (code 1) for a unit record. |

## init_tables1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x80d8c` | EAX:ef_addr32 | none | db0b4@db0f9 | Allocate the 0xaf0-byte record pool, fill the 16-bit slot of each of its 200 14-byte records from a lookup on text, and  |

## init_tables1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9cfdc` | EAX:ef_addr32 | none | 3cfc0@3d272,3cfc0@3d2fa,82cec@82cf4 | Select the two handler pairs for a name: defaults when it is empty, otherwise build the path string at 0x40d150 from 0x4 |

## fill_rects2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc91c8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | a5b70@a5c2d,a60a8@a614f,a6d00@a6f05 | Fill a rectangle given as exclusive coordinates when it lies inside the screen (limits 0x15b744/0x15b748); colours up to |
| `0xdd9e4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t | none | 278c0@2792d,27d18@27d50,f2a30@f2a8e | Store a parameter block at 0x15ee80, bracketing the writes with 0xdd870/0xdd8c0 unless 0x15ee94 is -1. |

## page_rotate1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe8368` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 7b3fc@7b4a9 | Look up a page's vertex group and rotate each 0x4a-byte entry's three points, storing the rotation parameters at 0x46437 |

## page_rotate1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe7e30` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | 759b8@75c6c,c8e58@c8f0d,c8f38@c8fc4 | Rebuild the 0x8000-byte remap table at [0x164604] by looking each byte of its upper half up in the 0x433f34 tables, unle |

## list_ops1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa4a94` | EAX:uint32_t | none | a4b08@a4b34,a4b5c@a4b6a,a4b80@a4bb8 | If entry id has flag 8 set, remove it and every entry whose owner byte (+0x11) is id. |
| `0xf3aa8` | EAX:uint32_t | none | f3f84@f409e | For an 22-byte record at 0x485400 + index * 22, run the 0xf3904 call (unless 0x4859db is set), then format its text at + |

## list_ops1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x287bc` | none | none | 284ec@28571 | Build a file name from a format (one of two, chosen by 0x15ee9a), open it, and if that succeeds run 0xab47c and close th |

## list_ops1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa0a2c` | EAX:uint32_t | none | 9e7bc@9f13e | Step a page slot: when its counter (+0) is zero invoke the handler at 0x132adc with the slot's words and the current 0x4 |

## list_ops1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa4a20` | EAX:uint32_t | none | a4b5c@a4b74 | Variant of 0xa4a94: if entry id has flag 8, push it to the front and rewrite every entry whose owner byte is id via 0xa4 |
| `0x9e494` | EAX:uint32_t,EDX:ef_addr32 | none | a0c04@a0d37,cd2e8@cd378 | Run the 0x9e7bc text routine with the current-page globals (0x417174/0x417178/0x41717c/0x41722a) temporarily pointed at  |

## probe_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd9b30` | none | EAX:uint32_t | 919d4@91a87,e9ac0@e9ad9 | Probe base values 0x6000 + n*0x100 (n = 0..16) with the 0x8b8 service 0x33 and return the first value whose reply echoes |
| `0xe9ac0` | none | EAX:uint32_t | 8b39c@8b4c9 | Run the probe selected by the state word at 0x464472 (1, 2 or 3), print a message when it reports failure, and return th |

## probe_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1e214` | EAX:uint32_t,EDX:int32_t,EBX:int32_t,ECX:int32_t | EAX:uint32_t | 1e28c@1e334,1e3c4@1e432 | For an open entry, rotate the query point (x, y, z) by the entry's orientation (0..3) and walk the script cursor until o |

## screen_clip1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc3770` | none | none | cdb20@cdc17,d1d40@d1d89 | Allocate the two work buffers at 0x146e40/0x146e44 (size 0x164600) if needed, complain when either failed, then zero bot |
| `0x8bd58` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 3cfc0@3d2ce,410d0@410e8 | Reset the UI state (record table, four work areas, pool) and then run 0x8bad4 with the caller's two arguments. |
| `0xc92e4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t | none | a6d00@a6e63,a6d00@a6eab,a6d00@a6f49 | Forward a rectangle to 0xeb174 when it lies wholly inside the screen (limits 0x15b744/0x15b748). |

## screen_clip1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc8e8` | none | none | bfcc@c27a,c868@c88f | For the current page (word at 0x121e98) call 0x11560 once for each of its entries, passing per-entry slots of the tables |

## state_reset1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa29f8` | none | none | 34ca4@34d04,a29cc@a29cf | Reset the input/menu state: default words and the 7-byte block at 0x13579b, clear the table, then set the control flags. |
| `0x2ce18` | EAX:uint32_t | none | 2ced4@2cfd8 | Reset a 0xca-byte page record: clear the object limit of each of its six slots (unless 0xff), recompute its grid limit,  |

## draw_loops1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x46168` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 257d0@2584f,257d0@25aee,257d0@25b4e | For the object found by guest_fn_7489c, call 0x46254 for each of its page record's seven 16-bit entries (+0x50) that dif |

## load_planes1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcdc90` | EAX:ef_addr32,EDX:ef_addr32 | none | cde00@cdf14 | Interleave four planes of rows x width bytes from a packed source into every fourth byte of the destination (non-zero so |
| `0xe94ec` | EAX:ef_addr32,EDX:uint32_t | EAX:uint32_t | 80058@80095,81230@8126e,82cec@82d29 | Load a text file through the size/read handlers at 0x132ab4/0x132ab8 into a new buffer at 0x1600a0, trimming a trailing  |

## state_reset1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x103d80` | none | none | 104580@104580 | One-time setup of the runtime's locked ranges: register the code range 0x103d80..0x1046c4 and the six data ranges below, |
| `0x78ac0` | none | none | 79128@791a3 | While the pending-message count at 0x1f8d56 is positive and 0x127da8 is clear, shrink it to the limit at 0x127d92 and re |

## state_reset1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x35b2c` | none | none | 35590@35632 | Commit the chosen game mode: copy the selection (0x1357ae) to the mode/slot bytes, format its name for mode 3, store the |

## tactical_marks1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x5d48c` | EAX:uint32_t,EDX:uint32_t | none | 5cff0@5d074,5d1c4@5d30e,5d1c4@5d47d | Draw a diamond of four lines around (x, y); the half-size is the word at 0x1a6ce8 shifted by the zoom byte 0x12b369 and  |

## tactical_marks1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8873c` | EAX:int32_t,EDX:ef_addr32,EBX:uint32_t,ECX:ef_addr32 | none | 55b80@55bc4,88664@886c4,886d0@88730 | Walk the object ring at 0x20f160: for each object of the current side whose link equals link, whose class byte matches k |

## tactical_marks1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf3610` | EAX:uint32_t | none | f36a4@f36b0 | Draw the segments of path record index (42-byte records at 0x484be0, count in the first word): one 0xe7a34 call per segm |

## tactical_marks1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x85f78` | EAX:uint32_t | EAX:uint32_t | 86110@866a9 | Starting after the current slot (byte +0x1d7) of the player's unit, scan its 20 three-byte slots (+0x198) cyclically for |

## tactical_marks1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9e3f8` | EAX:uint32_t | none | 7775c@77790,87a20@87c57,8c49c@8c55b | Make a page current: record it at 0x41722a/0x417174/0x41717c and, when its table entry has flag 1, clear the eight words |

## tactical_marks1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x110790` | EAX:ef_addr32 | none | none direct | Load run-length palette blocks into the 768-byte table at 0x4dd9b0: a count byte, then per block a start delta and an en |

## tactical_marks1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc9238` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | a6d00@a6f9a | Store a clip rectangle (a, b, c-1, d-1) at 0x15b60e..0x15b61c, build the two 64-entry shade tables at 0x41f3f0 on first  |

## screen_clip1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdd8dc` | none | none | 278c0@27b26,27d18@27eaa,f3480@f3492 | Save the screen area under the parameter block at 0x15ee80 (size clamped against 0x425fc6/0x425fc8) into its buffer and  |

## load_planes1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdbf3c` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | da644@da6a9,da644@da6fe,da7fc@da8a0 | Load two companion files named from part b: the first into dest (complaining when missing), then, if the second exists,  |

## state_reset1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd1e50` | EAX:uint32_t | none | 39204@3920c,3ea0c@3ee75,3ea0c@3ef7b | Switch the display mode (1..5; 3 and 4 count as 2 for the "already current" test against 0x15b7d0), tearing down and reb |

## screen_clip1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa0aa0` | EAX:uint32_t | none | 9e7bc@9eb15,9e7bc@9eb9e,9e7bc@9f14e | Fetch the 6-byte point entry of the current page (table at 0x41717c + 0xa), let the hook at 0x132af4 transform it, add t |

## list_ops1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe2564` | EAX:ef_addr32 | none | a5b70@a5c56 | Walk the node list at obj+0xc7 (next at +0xa) and call 0xe2374 for every node whose position, widened by a per-kind marg |

## load_planes1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11092c` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t,ECX:ef_addr32 | none | none direct | Decode run-length sprite data from src up to end into the byte image at dst with the given row stride. Each block holds  |

## tactical_marks1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x74ed8` | EAX:uint32_t,EDX:uint32_t | none | 2a4c8@2a669,74970@74d28 | Randomly raise the twelve 0..100 counters (+0x18e) of the object found for key by up to n, redrawing its entries first w |

## draw_loops1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x2c600` | EAX:uint32_t,EDX:uint32_t | none | 46454@471bf,46454@47232 | For each of a page's eight linked pages (table at 0x1fcb40) other than itself, call 0x46254 with colour 0x3e; if none we |

## tactical_marks1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x22f28` | EAX:uint32_t | none | none direct | Draw the HUD indicator for slot idx (10-byte records at 0x127a96): if its text slot exists and its enable byte (0x1a81d8 |

## tactical_marks1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x5c8e0` | EAX:ef_addr32 | EAX:uint32_t | 5c518@5c5d3 | Walk the 12-byte point table at 0x1fd098 from the current index (0x20f19a) backwards in steps of n = remaining/50 + 1, p |

## small_wrappers6 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdd8c0` | none | none | dd9e4@dda4a,f34cc@f352e | Run 0xdd8dc when the saved width at 0x15ee94 is -1 (nothing saved yet); otherwise do nothing. |
| `0x101f78` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 103e0c@103e4a,105648@1056a8,105648@1056f0 | Forward a stack argument to 0x1111e5. |

## small_wrappers6 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x108ffa` | EAX:ef_addr32 | EAX:uint32_t | 108888@10892f,108afd@108b5f,108afd@108bd0 | Decrement a use counter (+0xc) and, while it was non-zero, call the object's handler (first dword); returns the handler  |
| `0x109328` | EAX:ef_addr32,EDX:uint32_t | none | e3e50@e3e7c,e3f00@e3f1c | Check that the byte at *cursor equals expected (printing a message through 0x132a74 if not) and advance the cursor. |
| `0x11b1b0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t | none | 11b6bc@11b6eb,11b6bc@11b71c,11bb80@11bb9e | Forward four stack arguments to 0x11b134. |
| `0x9e094` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 8e5f0@8e639 | Run 0x9e0c4 with the hook at 0x132b14 pointing to 0x9e08c and the stack argument saved at 0x41714c, then restore the def |

## small_wrappers6 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb1bf8` | EAX:uint32_t | none | b3084@b3099 | Print the two status lines (0x1264e2; 0x1a364c and 0x1a364b) to a handle. |
| `0xb3374` | EAX:uint32_t | none | b3084@b30e3,b3084@b3101,b3084@b311f | Format a message from the global at 0x197674, the argument and the latch value, and pass it to 0xcc744. |
| `0x65a50` | none | none | 66548@665b7,66548@66689,66548@666b2 | Dispatch on the mode byte at 0x1282c5 (0..8) to the matching refresh routine, then flag the display dirty. |

## small_wrappers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xaaec` | none | EAX:uint32_t | e490@e556,e490@e624 | Pick 3, 6 or 9 from a random number modulo 10. |
| `0x9c1e0` | none | EAX:uint32_t | none direct | Sum 16 times the value of 0xeb2a8 and the +4 field of every node on the list headed at 0x132a70 (node pointers go throug |
| `0xcdfe8` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | none | ac854@ac8c2,f3f84@f4045 | Query service 0x33 sub-function 3 through 0x8b8 and return the low three bits of the status, half the x value and the y  |
| `0xa0060` | none | none | 3dd30@3deec | Reset the time scale at 0x132b20 and, if 0x137a00 is set, run 0xa0174 on every entry of the 0x4e-byte chain starting at  |

## small_wrappers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa9fc` | none | EAX:uint32_t | ab74@ad18,ab74@adf6,ab74@af56 | Pick a delay (0x28, 0x46, 0x78 or 0xc8) from a random number modulo 4. |
| `0xcbcc5` | EAX:uint32_t,EDX:ef_addr32,EBX:uint32_t | EAX:ef_addr32 | cbd0f@cbd21 | Convert an unsigned number to text in the given base (digits 0-9, a-z) at dst, returning dst. |

## small_wrappers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8557c` | EAX:uint32_t | EAX:uint32_t | 857bc@85949 | Return 0 when the owner has no weight of the class implied by its type byte (+0x1b: filter 1 for types 0-2, 9, 13; filte |
| `0xd5ec0` | EAX:ef_addr32 | none | 980e8@98114 | Tear down a loaded block record: trace-call 0x1031a0 with the handle at 0x420a64, release its range (+9 pointer, +5 leng |

## small_wrappers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x84678` | none | none | 92f98@92faa | Advance the page counter, run the transition selected by bits 3..5 of the word at 0x1fcbe2 (1..4), then call the handler |
| `0x912b8` | EAX:ef_addr32 | none | 3dd30@3e344 | While the pending-message count at 0x2144b2 is positive, shrink it by the limit at 0x127d92 and draw the message at row  |
| `0x106cb0` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | 107070@107083 | Read count bytes from a ring record into buf, waiting up to timeout; returns 0 when done or 3 on timeout. |

## small_wrappers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa9640` | EAX:uint32_t,EDX:uint32_t,EBX:int32_t | EAX:uint32_t | a9694@a96ac,a9694@a96da,a9694@a970b | Compare the signed value at 0x418708[index] with value using operator op (1 ==, 2 !=, 3 >, 4 <, 5 >=, 6 <=); returns 1 w |
| `0xdd870` | none | none | dd8d0@dd8d0,dd9e4@dd9ff,f2ac0@f2b13 | If a screen area was saved under the parameter block (0x15ee94 != -1), put it back, then mark it unsaved. |

## small_wrappers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xaa6c` | none | EAX:uint32_t | ab74@ad0f,ab74@aded,ab74@af4d | Pick a code (9..14) from a random number modulo 10. |

## copy_rects1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x109e08` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:int32_t | none | e5114@e5159 | Copy count rows of len bytes at row offset table[idx] + x from the screen (0x15b754) into the buffer returned by 0xc382c |
| `0xe5114` | none | none | a5ef8@a5f81,a60a8@a60da,a6d00@a6d69 | Copy every rectangle of the list at 0x43399e (8-byte entries, count in the word at 0x433eac) from the screen into the ba |
| `0xe516c` | none | none | a5ef8@a6003,a60a8@a61a1,a6d00@a70d6 | Copy every rectangle of the list at 0x43399e from the backing buffer back to the screen. |

## script_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf4c60` | none | none | none direct | Run the mode-1 script section, complaining about each missing part (flags 0x485f72, 0x485f6f, 0x485f73). |
| `0xf4d0c` | none | none | none direct | Run the mode-2 script section (flags 0x485f6d, 0x485f6c, 0x485f6e). |

## script_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfac78` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | fb1a0@fb1cb | Slot count for a selector with a cap of 4. |
| `0xfad54` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | fb1a0@fb1e2 | Same as 0xfac78 with a cap of 6. |

## script_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfaf0c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | fb1a0@fb210 | Slot count for a selector, wide variant (cap 6). |
| `0xfaff0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | fb1a0@fb227 | Identical to 0xfaf0c. |

## page_lookup1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x95660` | EAX:ef_addr32 | none | 9596c@959e6,95e58@96d77,95e58@96de3 | Look up a named entry for the object at 0x214458 and pass it to 0x955c0. |
| `0x957c8` | EAX:ef_addr32 | none | 95ae0@95be5 | Same for the object at 0x20f15c (page word at +0x2e), passing the entry to 0x95704. |

## copy_rects1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe5c30` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | e5db4@e5e4a | Draw a 7x7 glyph at (x, y). |
| `0xf2320` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | f23ac@f23cc | Draw a 7x6 glyph from the second font at (x, y). |

## trace_loaders1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x106464` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | 102508@102534 | Load and parse a resource by name; on a missing file install the 23-byte default block at 0x194a04. |
| `0x113a40` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | 102fb4@102fe0 | Same with parser 0x1133f8 and default block 0x1954bc. |

## text_metrics1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x48d5c` | [ESP+4]:ef_addr32 | EAX:uint32_t | 3b58@3bf1,3b58@3c46,3ea8@3ebd | printf-format a string and return its width: the text width for zoom 0, else its length times 4. Variadic; all arguments |
| `0x48da0` | [ESP+4]:ef_addr32 | EAX:uint32_t | 48e7c@48f35,5c354@5c43d,5c518@5c64d | Same with length times 8 for the nonzero zoom. |

## draw_loops1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x485a8` | EAX:uint32_t,EDX:uint32_t | none | 46454@46e13 | Transfer the unit's link word (+0x158) from a to b. |
| `0x486a0` | EAX:uint32_t,EDX:uint32_t | none | 46454@47097 | Transfer the unit's word at +0x146 from a to b. |

## state_reset1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8b2d8` | none | none | 8b274@8b290 | Reload the game data: tear down the video state, build the data file path, load it to 0x425190 through the handler at 0x |

## screen_clip1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9bca4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 98eac@98ecd,98eac@98eea,98eac@98f07 | Draw a bar of height h (when non-zero) at x: a coloured strip via 0xc99ac and a highlight via 0xc9a98. |

## search_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe0b10` | EAX:ef_addr32 | EAX:uint32_t | e0b88@e0bbd,e115c@e1172,e1260@e1265 | Search the buffer at 0x15f5fd (length byte at 0x15f684) for the text, ignoring case; on a hit remember its position at 0 |
| `0x3a268` | EAX:uint32_t | EAX:uint32_t | 3a148@3a238 | Return 1 when the cell found for the 69-byte record's coordinates (dwords at +7/+0xf divided by 2048) differs from the r |

## search_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3170c` | none | none | 3122c@312ad | Rotate the vector at 0x19e57e by zero angles into 0x19e5c1 and add the offset triple at 0x19e560. |
| `0x85380` | EAX:uint32_t | none | 857bc@85883 | While the owner record has an active chain (byte +0x8e) whose last 0x19-byte entry has kind 2, run 0x85358. |

## code_maps1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xbbdc4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | 8883c@8889b,8e940@8eb37,90438@90606 | Map an index through the mapper chosen by kind (0..3: 0xbbbe4, 0xbbb34, 0xbbae4, 0xbbd8c) and look the result up in the  |

## small_wrappers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa1770` | EAX:uint32_t,EDX:uint32_t | EAX:ef_addr32 | 878c8@878e9 | Return the address of slot idx (16-bit entries after the 29-byte sub-records) in a page object, checking that its type b |

## small_wrappers7 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf8da8` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:ef_addr32,[ESP+4]:uint32_t | none | f8e44@f8f2a | Fill a 0x72-byte record from two source records and an optional third: bytes from src1 (+8, +3, +9), 18 bytes from src2  |

## trace_loaders1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1065c8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | EAX:uint32_t | 102590@1025b5 | Find the first type-1 0x894-byte entry in the list of an object (array pointer +0x5c, count +0x60); if none is type 1 in |

## search_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xac57c` | none | none | ac5f8@ac6d6 | Probe save files numbered from the counter at 0x13794e (name format 0x18dc48): close each one that exists and advance th |

## search_helpers1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa2ef4` | none | EAX:uint32_t | a2f70@a30f5,a410c@a35b2 | Scan the first 0x1357b0 table entries for one without flag 1 (+0x1c); if found, queue request 0x137198 with its index an |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x28744` | none | none | 284ec@285a2,3ea0c@3ecc3,a9724@a9825 | Sibling of 0x287bc: build the same file name, open it with 0xa7948, run 0xab324 with mode 2 and close the handle. |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x36100` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 35c08@35cba,36f84@3700b | (ret 8) Store a request block (0x19f34c.., flag 0x19f5aa) and run 0x35de0; reports errors through the 0x132a74 handler i |
| `0xa8660` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | a8718@a8770,a8718@a879d,a8718@a886b | (ret 4) Dispatch on the mode byte at [[0x4191d0]+4]: 0 passes the arguments straight on to 0xa8578, 1 mirrors x against  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc1e3c` | EAX:uint32_t | none | c3584@c3614,c3584@c362a | Decode a PackBits-style run-length stream at the pointer 0x41f3a0 into dst: rows (byte 0x146e34) of width 0x146e38; cont |
| `0x6b344` | EAX:uint32_t | none | 6c488@6c702 | For unit id, look up its object; if the table entry (0xca bytes at 0x19bb58) of the object's owner still points back at  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xbc4da` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | bc450@bc453 | Close a stream record: flush when flagged, release its handle, optionally run 0xfa9a0, free the buffer and remove a temp |
| `0x5ca88` | EAX:uint32_t,EDX:uint32_t | none | 5a220@5a284,5c354@5c3e6,5c518@5c5f6 | Convert a fixed-point position (words at +0,+4,+8, 8 fractional bits) to be camera relative (origin at 0x21444c/214448/2 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9cc28` | EAX:uint32_t,EDX:uint32_t | AL:uint8_t | ea1a0@ea1ab | Load the resource with key into buffer: try the two archive contexts at 0x40d1f0/0x40d1f4 (enabled by bits 0 and 1 of 0x |
| `0xa9694` | EAX:uint32_t | none | a9e70@a9fe6 | Evaluate up to three variable conditions of a script record (index, operator, value triples at +0, +0xa, +0x14); the fir |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3237c` | none | none | 31e2c@32057 | Update two running minima (0x19e5c5 with its companion word 0x19e5a5, and 0x19e5b9) from the distances computed by 0x7b1 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc8d30` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 69740@69800,7d1f0@7d291,944f4@94557 | (ret 8) Offset a 16.16 vector (eax, edx, ebx) by the table-rotated displacement of length s1: two angles (-ecx and -s0,  |
| `0xc8d10` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | b69c@b948 | (ret 8) Wrapper for 0xc8d30 that first shifts eax, edx and ebx left by 16. |

## float_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf8278` | EAX:double | EAX:double | b59a8@b59fc,b59a8@b5a39 | sin of the argument scaled by the two constants used in 0xf8250 (0x1932a4 and 0x1932ac). |
| `0xf8298` | EAX:double | EAX:double | b59a8@b59e8,b59a8@b5a48 | cos of the same scaled argument. |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xef950` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | ee589@ee59f,ee22c@ee76d,ee22c@ee8fd | Combine two 10-byte records (dword, dword, word) at eax and edx with the core routine 0xef9aa and store the 10-byte resu |
| `0xefbaf` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | ee22c@ee653,ee22c@ee672,ee22c@ee821 | Same wrapper as 0xef950 around the core routine 0xefbd9. |
| `0xefd84` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | ee22c@ee5be,ee22c@ee78c,ee22c@ee920 | Same wrapper as 0xef950 around the core routine 0xefdae. |
| `0xef97a` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | none | f03cc@f045d,f03cc@f0479,f0482@f05b1 | (ret 12) Variant of 0xef950 whose second record comes by value on the stack (dword, dword, word). |
| `0x106d00` | EAX:uint32_t,EDX:uint32_t | none | 106d34@106d52,106e2c@106e2f | Optionally trace the string through printf (when 0x17d364 is set), then feed each byte to the handler 0x118428 of the ob |
| `0x8fdb8` | EAX:uint32_t | EAX:uint32_t | 8fe58@8feb9 | Map the record type (low 7 bits of the first byte, 14..17) of the record at eax to 0x4a, 0x3a, 0x2e or 0x22; anything el |
| `0x1087cd` | EAX:uint32_t | EAX:uint32_t | 1084d3@10851d | Pull characters through the reader at [obj] until one is not in the class-2 skip set (table 0x195af0, indexed by char+1) |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd6ffc` | EAX:uint32_t | none | 80400@8040a | Walk a list of record pointers until one whose first byte is zero; for each, release the range (+0x1c, +0x18) when both  |
| `0x9cee8` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | aaa50@aaa87,eab44@eabb7,eab44@eabc8 | Seek: when flags carry bit 16 or 17 use the archive context at 0x40d1f0 (bit 16) or 0x40d1f4 and advance its cursor by a |
| `0x97914` | EAX:uint32_t | none | 978e8@97906 | Unless 0x122c5c is set, copy the vector at obj+0x30 to a temporary, scale it by the float bits at 0x132b20 and add it to |
| `0x8ba98` | EAX:uint32_t | none | 8bad4@8bb0d | Walk a list of length-prefixed records (word length at +0, zero ends); type byte at +2 of 0x11 links an effect via 0x8ba |
| `0x8bad4` | EAX:uint32_t,EDX:uint32_t | none | 8bd58@8bdc5 | Build the default path with suffix 0x18a86c, append the two names, look up the UI record for it and process its effect l |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa7dac` | none | EAX:uint32_t | a8718@a87a2,a8718@a88bd | Map the mode byte at [[0x4191d0]+0x10] (0..9) to a code through a jump table; out of range gives 0. |
| `0x82b6c` | EAX:uint32_t | none | 82e40@82ed3 | sprintf(buf, format 0x188bd8, seven state bytes and the byte at 0x15b7e6). |
| `0x80374` | EAX:uint32_t | none | a8bd4@a8d03 | Create two entries with 0xd7764 (the first with the caller's EAX and parameters 1, 1, 0x5622, 0x7f, 1, 1; the second for |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x362c4` | none | none | 36438@367b2,36d30@36f45 | Look up the widget named 0x1830f8 and enable it with a label chosen by the state byte 0x19f5aa (2..7); other states clea |
| `0x101688` | EAX:int32_t | none | 1016e0@101826 | Run a short hardware delay loop: n pulses of the two callbacks at 0x486da4/0x486da0, one for the low bit and two per rem |
| `0x7ff0c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | none | none direct | (ret 12) Create an entry through 0xd7764 with the caller's EAX/EDX/EBX/ECX and three stack arguments and append it to th |
| `0xab060` | EAX:uint32_t | none | ab0dc@ab162 | Find the node at 0x4191d4 in the group lists and write its two indices (words from a zeroed pair) plus one byte at 0x419 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x35220` | EAX:uint32_t | none | 367f0@3685d,367f0@368b0,367f0@36906 | Build a 3-byte request (the key byte plus a word: 0x19f592 - 1 for key 0x48, else 0xffff), mirror the word to 0x19f58c,  |
| `0x7ca78` | none | none | 7cc5c@7ccc5 | Walk the widget chain (next at +0x21) starting at the widget found under 0x187f38/0x187dfc; the first n entries (n = 0x1 |
| `0xc9ecc` | none | none | a5b70@a5c97,a60a8@a6179,ac408@ac522 | Set the view origin to the screen size (0x15b744 x 0x15b748) minus one with the half-size corner, and latch the full rec |
| `0x118428` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 106d00@106d2a,107060@107068 | Queue byte ch into the ring buffer of an open device object (buffer at +4, head word at +2, tail word at +4 of the recor |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11d322` | EAX:uint32_t,EDX:uint32_t,EBX:int32_t,ECX:int32_t | EAX:uint32_t | 11d3f0@11d57f,11d3f0@11d5ec | Lay out a decimal digit string as "intdigits.fracdigits" at out: copy up to intdigits digits from src, pad with zeros, a |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11d385` | EAX:uint32_t,EDX:int32_t,EBX:int32_t | none | 11d3f0@11d606 | Right-align the len-byte text at buf inside width bytes: move it to the end, then fix a leading "." or sign+"." by inser |
| `0x11451c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | none | 103778@1037aa | Advance a (column, row) cursor of a text object by 0x4dd804 steps: each step adds +0x68 to the accumulator at +0x6c and  |
| `0xa6038` | none | none | a4b08@a4b43,a4b5c@a4b79,a4b80@a4c79 | Redraw: when a backdrop (0x41eeac) is active draw it over the screen rectangle, otherwise run 0xc9ec4; then re-apply eac |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x97950` | EAX:uint32_t | none | 978e8@9790d | Integrate a record's motion unless 0x122c5c is set: add the vector at +0x18 (scaled by the step 0x132b20) to the positio |
| `0xa374c` | EAX:uint32_t | none | a348c@a34f0 | When at least 8 bytes are pending (0x1357da), send a 13-byte request built from 0x1357ae, 7 bytes of the template at [0x |
| `0xd7764` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | EAX:uint32_t | 7ff0c@7ff3d,80374@80391,80374@803ac | (ret 12) Allocate and clear a 0x25-byte entry, copy the name from EAX into it and fill in the bytes/words at +0x0d, +0x0 |
| `0xdd2f4` | EAX:uint32_t | EAX:uint32_t | 275e0@27658,275e0@276a8,275e0@276e5 | Measure the pixel width of a zero-terminated string: spaces and tabs count 3, characters above 0x7f use the byte table a |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x5cb1c` | EAX:uint32_t,EDX:int32_t,EBX:int32_t,ECX:int32_t | none | 5a2d0@5a3e5,5a468@5a6bb | Variant of 0x5ca88 taking the position in EDX/EBX/ECX (fixed point already shifted) and the extra argument in EAX: make  |
| `0x66b74` | EAX:uint32_t | AL:uint8_t | 59fa4@5a1a7,5cbc8@5cef2,66548@66624 | Return 1 when the metric distance (0x23760) from the 8.8 fixed-point position at pos to the reference point is below the |
| `0x40624` | none | none | 3dd30@3e451 | Latch the full screen rectangle twice, optionally format "ratio" text (50 / 0x127d92) and print it at row 12, column 10, |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x286c0` | none | none | 3ea0c@3ecda | Sibling of 0x28744: build the file name (two formats chosen by 0x15ee9a), open it with 0xa7948, write the 2-byte value 0 |
| `0xa499c` | EAX:uint32_t | none | a4b08@a4b3e,a4cb4@a4d0e,a5a68@a5aa6 | Variant of 0xa4a94 that appends the entry (0xa479c) and rewrites every entry owned by id via 0xa48ac, then refreshes the |
| `0xf4bdc` | none | none | none direct | Reset the command parser state (0x485dfc = 3, 0x485f70/71 = 0), read tokens and run handlers until 0x485e08 clears, repo |
| `0x36340` | none | none | 367f0@36955,36ad8@36cfd | Enable the widget named 0x182fb0 with a label picked from the state byte 0x19f5a9 (2..10): 0x1831dc/0x1831e8 (by 0x19f59 |
| `0x48df8` | none | none | 3dd30@3e759 | Clamp the counter 0x127b1c at zero; while it is non-zero draw the two status strings (0x1a524c) at (17, 17) with modes 0 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x97860` | EAX:uint32_t | EAX:uint32_t | 974b8@9750c | Step the animation record: bump its frame word (+6), restart it through 0x975f0 when the frame reaches the limit table e |
| `0x1046c4` | none | none | none direct | When 0x17d2d4 is set, run 0x1048b4 over the range (0x103d80, 0x1046c4), release the six runtime buffers (0x4dd670, 0x4dd |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xeaf80` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | eb008@eb030 | Draw a 6x5 glyph (index ecx, 5 mask bytes at 0x160648) in colour al at (x = edx, y row = ebx). |
| `0xf2960` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | f29ec@f2a10 | Draw a 6x6 glyph (index ecx, 6 mask bytes at 0x16116c) in colour al at (x = edx, y row = ebx). |
| `0xc38cc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | eac48@ead00 | Copy a planar (4 interleaved planes) image into the screen buffer at 0x146e40: for each plane, every row, every fourth p |
| `0x767f4` | none | none | 58d64@58d73 | Reset the 256-entry colour tables: fill 0x1f71e0 with (0x3f, 0, 0x3f), zero the first entry at 0x464060, copy the 256 tr |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x385cc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 4aa4c@4ad93 | (ret 8) Append a new 0x19-byte node to the list for slot idx (heads at 0x19f5b0, next at +0x15, previous at +0x11) and f |
| `0xd8610` | none | none | d86e0@d898d,d86e0@d89c5,d86e0@d89f6 | Rescale the pair of words at 0x15baf2/0x15baf6: save the old values to 0x15bafe/0x15bafa, then recompute them from the r |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa1500` | EAX:uint32_t | AL:uint8_t | 9e7bc@9eb31,9e7bc@9ebba | Step the timer word entry idx of the current page (entries are 2 bytes wide at 0x417174 + 0xb + 29 * [[0x41717c]+1]); th |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb5640` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | b5fe8@b64d9,b69e4@b7a3b,b7d4c@b7ed1 | (ret 4) Fill the sub-record at [obj+0x20] from the object in edx: find a free slot run (0xb58a8 with start bx and count  |
| `0xf2a30` | none | none | f2ac0@f2b32 | Apply the palette at 0x425c8c, decrement the word 0x1616ee and send the parameter block built from 0x485920/0x48591c and |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdd4c4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | a8578@a862e | Draw a string with the large font (glyphs 0x15e61c, widths 0x15e27e, space 0x11, spacing 2). |
| `0xdd5bc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | a8578@a8642 | Draw a string with the medium font (glyphs 0x15e6ac, widths 0x15e2a2, space 5, spacing 1). |
| `0xdd6a0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 27d18@27dc1,27d18@27e77,27fec@281b6 | Draw a string with the small font (glyphs 0x15e73c, widths 0x15e2c6, space 5, spacing 1; 0xa5 is not remapped). |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xde7d0` | EAX:uint32_t | EAX:uint32_t | de864@de958 | Look up the hash of the name in the binary tree whose keys are at 0x426150 and child word pairs at 0x4270f0 (left/right  |
| `0x9dbf0` | EAX:uint32_t | EAX:uint32_t | 9de08@9de29 | Same lookup/insert as 0xde7d0 over the tables at 0x412fc0 (keys), 0x40f140 (signed child words) and 0x411080 (payload, n |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc1c24` | EAX:uint32_t | EAX:uint32_t | c3584@c35fd | Scan the chunk stream that starts 0xc bytes into the buffer at 0x41f3bc (cursor 0x41f3a0, end 0x41f3c0) for the chunk wi |
| `0xc9f28` | EAX:uint32_t | none | a5b70@a5c71,a60a8@a60e8 | Set the view to the rectangle at 0x41ec08 (x, y, width, height words): origin at the centre, latch the rectangle twice a |
| `0x78934` | none | none | 79128@7918f | Unless 0x127da8 is set: count the timer 0x1f8d58 down by the step 0x127d92 (clearing it above 0xdc), format the hour-lik |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xebc50` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | 9cde8@9ce0e,9cde8@9ce1f | Read count bytes into dst from the archive context ctx (position +0x158, end +0x15c): the part beyond the end is filled  |
| `0x9cde8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | e774@e79d,9d170@9d1a1,aa5c4@aa5e9 | Read count bytes into dst: with flag bits 16/17 from the archive context at 0x40d1f0/0x40d1f4 (0xebc50), with bit 18 fro |

## float_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb45fa` | EAX:double | EAX:double | 5f698@5f763,5f944@5f962,6e3e0@6e536 | tan of ST0 (fptan, popping the pushed 1.0); with bit 0 of 0x1631f0 set the library emulation 0xfa3d4 is used instead. |
| `0xdc1d4` | EAX:double | EAX:double | 1db08@1dba3 | atan of ST0 (fld1; fpatan); the emulation path 0x10848c is used when bit 0 of 0x1631f0 is set. |
| `0x11ac3c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:double | 11acb0@11ad04 | Fetch a double from the table at [obj+0xf]; the index is built from three bytes with the multipliers at obj+1 and obj+2  |
| `0x10d9c6` | EAX:double,EDX:uint32_t | EAX:double | e86f7@e870c,120560@120646 | (ret 12) modf: truncate x toward zero (0x9d72c), store the integral part through ip and return the fractional part. |
| `0x11b588` | EAX:double | EAX:double | 11b5a8@11b608 | (ret 8) Call 0x11b540 with the double argument and scale its result by the constant at 0x1958bc. |
| `0x11bf00` | EAX:uint32_t,EDX:double,EBX:double | EAX:double | 11bf28@11bf64 | (ret 16) Linear combination: f(a0) * v[0] + v[1] * a1 with f computed by 0x11bf00's helper 0xb3fd0 and the pair of doubl |

## float_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdc1e9` | EAX:double,EDX:double | EAX:double | b4620@b4664,deb1a@deb2f,e6990@e6a41 | fpatan after an exchange: atan2(st0, st1) of the two x87 registers (ST0 = y, ST1 = x on entry); the emulation path 0x108 |
| `0x11af60` | EAX:uint32_t | EAX:double | 10a0d4@10a0db,11afb4@11afba,11b040@11b0ce | /((p[3] - p[2]) + (p[7] - p[6])) * ((p[4] - p[0]) * k)/ for the doubles at EAX, with k at 0x195804. |
| `0x11b0ec` | EAX:uint32_t | EAX:double | none direct | atan2(p[1] - p[5], /p[4] - p[0]/) for the doubles at EAX. |
| `0xe86f7` | EAX:double | EAX:double | 7f850@7f87d,b59a8@b5b2e,b59a8@b5b69 | (ret 8) Floor-like: take the integral part with 0x10d9c6 and add the constant at 0x190c8c when the fractional part is ne |

## float_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10a030` | EAX:uint32_t,EDX:uint32_t | none | 109ec0@109edb | Build a record at EAX from the one at EDX: zero the first 8 bytes, store the extent F64[+0x20] - F64[+0], and copy the d |
| `0xea8cc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 97914@9793c,97950@9797b,97950@97996 | (ret 4) Scale the three signed 32-bit components at EAX by the float whose bits are passed on the stack, truncating each |
| `0x11aff4` | EAX:uint32_t,EDX:double | EAX:double | 10a0d4@10a106,10a0d4@10a117,10a0d4@10a128 | (ret 8) Blend two pairs of doubles by the fraction d and return atan2(a - b, /p[4] - p[0]/): a = p[3] * (1 - d) + p[2] * |
| `0x11bfc8` | EAX:uint32_t,EDX:double,EBX:double | EAX:double | 10a220@10a40a | (ret 16) (((p[0] + k1) * k2 * k3 + k4) * (k5 / f(a0))) * k6 with f = 0x11af14 and the constants at 0x1959a4..0x1959cc. |
| `0x11b540` | EAX:double | EAX:double | 11b588@11b595 | (ret 8) f(x * k4) * 0.53 with f = 0x11b3fec's sibling 0xb3fec; the intermediate x * k1 * k2 + k3 is computed and discard |

## float_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |

## float_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1198ec` | none | none | 1196a0@1196aa | Combine the double globals: 0x432a70 = g[432ae0] * g[432948] * g[432908] + g[432c48] * g[432ac8]; 0x432b38 = g[432da0] * |
| `0x11af14` | EAX:double | EAX:double | 11b134@11b146,11b3c8@11b3dc,11bc40@11bc52 | (ret 8) r = f(a) (0xb3fd0); for /a/ > 1 add the constant at 0x1957fc, otherwise use 1 - r; return the 0xa4470 transform  |
| `0x11016c` | none | none | none direct | Pentium FDIV bug probe: when the CPU level 0x13719d is at least 3, compute 4195835 - (4195835 / 3145727) * 3145727 and s |

## float_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10a078` | EAX:uint32_t,EDX:uint32_t | none | 109ec0@109ecc | Build a segment record at EAX from the record at EDX: copy the doubles at +0x20/+0x30/+0x38, zero the first 8 bytes and  |
| `0x10a1c4` | EAX:uint32_t,EDX:uint32_t | none | 109ec0@10a025 | Derive the coefficient record at EAX from the parameters at EDX (doubles s[0]=+0, s[1]=+8, s[2]=+0x10): +0 = s2 * k1, +8 |

## float_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb3fec` | EAX:double,EDX:double | EAX:double | 16b0c@16b78,6cc5c@6cc81,6cf00@6cf27 | (ret 16) pow(x, y) for positive x via exp(y * ln x) (0xfa362 and 0xdcb96); 0.0 for x <= 0. When the product exceeds the  |

## float_helpers9 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11bbdc` | EAX:double,EDX:double | EAX:double | 11bc40@11bc66 | (ret 16) Falloff curve: for a0 above the threshold at 0x19593c return (1 - pow(a0, 2.9) * k2) * k3 (constants 0x195944 a |
| `0x11c484` | EAX:uint32_t,EDX:uint32_t | none | 11c20c@11c2b1,11c350@11c379 | Project the record at EDX into the coordinate triple at EAX: v = (s[0x50] - s[0x48] * tan(s[0x58])) / (1 + tan^2) - k80; |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x78a0c` | none | none | 79128@79194 | For each of the 8 slots (0x7a bytes at 0x1f8930, countdown word at +0x78): count it down by the frame step 0x127d92 (cla |
| `0x34b2c` | none | none | 345e8@3474e,34bcc@34c91,34f94@350da | Dispatch on the state byte 0x13578e (3..19): rebuild the child dialog for the matching label, update the buttons (states |
| `0xbd7cc` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | bd86c@bd8a7 | Try to shrink the counter byte at p+3 by one: the minimum depends on the type byte p+9 (types 1, 2, 10 need more than 2, |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe5e90` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | e605c@e60e7 | Draw a 7x9 glyph in the colour byte 0x433ec0 at (x = eax, y row = edx): code bit 7 selects the table at 0x15feb0 (index  |
| `0x6f6b8` | EAX:uint32_t | AL:uint8_t | 6f758@6f891 | For the unit's 20 three-byte slot records (+0x198: type word area, +0x19b: kind byte) find the slot of rank-2 weapon typ |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe3868` | none | none | e378c@e384a | Load the string table from the resource stream: the entry count word (0x15f902), a 2 * count byte header into the buffer |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfdec0` | EAX:int32_t,EDX:int32_t,EBX:int32_t,ECX:int32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | fdf60@fe0af | (ret 8) Bresenham circle quadrant walk: x = esi decreasing, y = edi increasing until it reaches the end row ebp, err = e |
| `0xfde20` | EAX:int32_t,EDX:int32_t,EBX:int32_t,ECX:int32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | fdf60@fe09a | (ret 8) Mirror image of 0xfdec0: x = esi increasing, y = edi decreasing until it reaches the end row ebp. |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe40e4` | none | none | e4010@e40d4,e4188@e425a | Allocate (once) the pattern buffer of ([0x15f93c] << 8) + 0x200 bytes, align it to 256 bytes (0x15b958) and fill consecu |
| `0xd23d8` | EAX:uint32_t | none | d2500@d2576,d2584@d2626,d2634@d26f3 | Process a render object: count it (0x4209c8), prepare it with 0xd2130 unless 0x158ed8 is set, and when the object is act |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb9e9c` | EAX:uint32_t,EDX:uint32_t | AL:uint8_t | 9820@9bf9 | Map a (class, selector) pair to a code: the class byte comes from the record at [obj+0x51] through 0xba0e4 (4 when absen |
| `0xe3444` | none | none | 3ea0c@3ef4d | Finish the open resource file 0x15f8f8 (when not -1): read two file positions around the call to 0xe36bc, append them (4 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x98ac0` | EAX:uint32_t | none | 98658@98666 | Restore the screen strip saved at 0x40cd72/0x40cd74: when a saved position exists, put the 0x23x0x1e strip at 0x12dda4 b |
| `0x11d27b` | EAX:uint32_t,EDX:int32_t,EBX:int32_t,ECX:int32_t | EAX:uint32_t | 11d3f0@11d5c3 | Format a signed value as a sign character plus zero padded decimal digits (at most 3 digits are produced; no terminator  |
| `0xcd2e8` | EAX:uint32_t | EAX:uint32_t | 161d4@164bf,1e28c@1e383,56084@56196 | Add DELTA to the 16-bit progress value of the current entry (clamped to 100, stored in the table at 0x41f700). When the  |
| `0x103e5c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:int32_t,[ESP+12]:uint32_t | EAX:int32_t | 103f04@1040e9,103f04@104117,103f04@104145 | Parse a number from the string STR in base BASE (digit characters are looked up case insensitively in the table at 0x194 |
| `0x85bc8` | EAX:ef_addr32 | AL:uint8_t | 73ec@74eb,3a268@3a2af | Classify the map cell at POS (two 16-bit coordinates): look up the cell byte in the grid at 0x134d8a (rows of 40, 10 uni |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xddbe0` | none | AL:uint8_t | 982a0@982b0 | One-time mouse/cursor setup: on the first call allocate the cursor buffers (7, 0x1a, 7 and 0x800 bytes) into 0x4260b0..0 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x101a90` | none | none | 101c54@101c5a | One-time runtime init guarded by the flag at 0x17d2cc: register the exit handler pair (0x101a90, 0x103d48) and mark eigh |
| `0xfa6d3` | none | EAX:ef_addr32 | bc34f@bc355 | Allocate a stream slot (26 bytes): reuse a node from the free list at 0x4644e0 (keeping its 0x4000 flag), else take an u |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcd957` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:int32_t,[ESP+20]:int32_t,[ESP+24]:ef_addr32 | none | 98b68@98bc2 | Copy a SRC block of ROWS rows of WIDTH bytes to the frame buffer at (X, Y), skipping zero (transparent) source bytes; SK |
| `0x98b68` | EAX:uint32_t,EDX:uint32_t | none | 98658@987f2,98658@98a19 | Save the 0x23x0x1e screen strip at (X, Y) (negative values clamp to 0) into 0x12dda4, remember the position at 0x40cd74/ |
| `0xd1bec` | none | none | d1c54@d1c8d | Finish the log file at handle 0x420944: write the sizes into the header (offsets 4 and 0x334, size 0x330 rounded up to e |
| `0x5e1b0` | EAX:int32_t,EDX:int32_t,EBX:ef_addr32,ECX:int32_t | none | 5bd8c@5beca | Draw the format string FMT at cell (A, B) (scaled by the shift at 0x12b369, offset by 0x1a6cfc/0x1a6cfa) and continue wi |
| `0x3bcc0` | EAX:int32_t,EDX:int32_t,EBX:ef_addr32,ECX:int32_t,[ESP+4]:uint32_t | none | 3a3a8@3b49f,3a3a8@3b509,3a3a8@3b564 | Optionally (when the last stack argument is 1) draw through 0xc8ccc with the first and second words of PTR added to B an |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10e28a` | EAX:ef_addr32 | none | ebea3@ebece | Convert the positive double at P in place to a 64-bit integer: restore the hidden mantissa bit, shift left for large exp |
| `0x7cc5c` | none | none | none direct | Show the main widget set: release the parser buffers, activate the widget stored at "0x187f74" when the mode byte at 0x1 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd1c54` | EAX:ef_addr32,EDX:ef_addr32,EBX:int32_t | none | ac57c@ac5eb | Start a log: remember [0x15b744] / 8 at 0x15b7cc, open the log file NAME, emit the display list, log the palette SRC (mo |
| `0x110130` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | none | fa362@fa380 | Wrapper for 0x10e4dd on the double (S0, S1): control word (S2 & 0x1f) with 0x1080 when S2 is not 8 and the value is +/-0 |
| `0xf017a` | EAX:uint32_t,EDX:ef_addr32 | none | ee589@ee58f,ee22c@ee5ae,ee22c@ee5cd | Convert the single precision float BITS to an 80-bit extended value at OUT (mantissa at +0/+4, sign and exponent at +8). |
| `0xebdcc` | EAX:ef_addr32,EDX:ef_addr32 | none | 9d55d@9d6a5 | Convert the decimal digit string TEXT (digit characters only, low nibble used) to a double stored at OUT: the digits are |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9be0d` | EAX:ef_addr32 | none | 2b0@2bb,9c446@9c44e,104840@10486d | Store the current segment registers into the 12-byte record at OUT (ES, CS, SS, DS, FS, GS as words, like segread). |
| `0x9c47a` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 9c461@9c46e | The software interrupt dispatcher behind int386x (registers ESI = interrupt number, EDI = input register block, EDX = ou |
| `0x9c461` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | 9c446@9c457,104840@10489a | int386x: perform software interrupt INTNO with the register blocks IN/OUT and the segment block SREGS; returns the EAX t |
| `0x9c446` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | 1047e4@104803 | int386: int386x with a freshly read segment block. |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd6694` | none | EAX:uint32_t | 1c60c@1c60f,82cec@82d03 | Run the step 0xd66c2; when it succeeds (non-zero) run it five more times and return -1, otherwise return 0. |
| `0x107004` | EAX:ef_addr32,EDX:ef_addr32 | AL:uint8_t | d9c30@d9c56 | Try 0x106f10 with the object A and the current slot index [0x464470] - 1: on success (0) store the index through OUT and |
| `0xea20c` | EAX:ef_addr32,EDX:ef_addr32 | none | ea3ec@ea3fd | Hash the name TEXT (0xebfd0), place the hash in a 16-byte block (a callback address 0xea1e8 sits just below it on the st |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x113b78` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:ef_addr32 | EAX:uint32_t | 1034b0@1034d5 | Classify the record pair CTX (cdecl): returns 3 when the name of the first record (+0xba, only when its size at +8 is at |
| `0xf819c` | EAX:ef_addr32 | none | ac2e4@ac3d2 | Load the text file NAME from the default path: build the path in a 144-byte buffer, append NAME and the suffix at 0x1932 |
| `0x9f664` | EAX:int32_t | none | 9e7bc@9f230,9e7bc@9f279,9e7bc@9f2c2 | For the 6-byte point entry INDEX of the current page (table at 0x41717c + 0xa) copy its Y word to 0x417226, let the hook |

## script_helpers2 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfae30` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | fb1a0@fb1f9 | Wide slot-count selector without the special case for class 0x15 (class 2 and 0x19 give 6). |
| `0xfb0d4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | fb1a0@fb23e | Identical to 0xfaf0c. |
| `0xfb1a0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | fbad8@fbe49,fbad8@fbf05,fbad8@fbfd5 | Dispatch to one of the six slot-count selectors by VARIANT (BL): 0 fac78, 1 fad54, 2 fae30, 3 faf0c, 4 faff0, 5 fb0d4, e |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb54d0` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32,ECX:int32_t,[ESP+4]:uint32_t | none | 4fa64@4fb36,b5640@b567f,b9c68@b9d98 | Compute the clock time that lies RATE steps (scaled by the divisor for the rate S0, ret 4) after the stored time hours a |
| `0x598e8` | none | none | none direct | Per-mode refresh of the current object (selected by the signed word at 0x1f71ca; nothing happens when it does not exist) |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa0174` | EAX:ef_addr32 | none | a0060@a008c | Advance the two vector pairs of the object OBJ: copy the vector at +0x3c to 0x132b28, then add the vector at +0x30 scale |
| `0x1a3cc` | none | none | 1cde8@1ce46 | Step the demo script at 0x122c80: when the expected key code 0x41f833 matches the current script byte, clear it and adva |
| `0xaa3dc` | none | none | aad7c@aae38 | Walk the linked list at 0x4191fc for as many entries as the signed word at 0x4194c4 says: copy each entry's name (+9) in |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd3844` | EAX:ef_addr32 | none | none direct | Count how many of the four tiles around the object's cell (x, y)..(x+1, y+1), read with 0xc3f50, equal the value [0x1591 |
| `0x2a7f8` | none | none | 3dd30@3e1f3 | When the larger of the two coordinate distances between (0x125250, 0x125254) and (0x1a5bec, 0x1a5be8) is at most 0x7fff, |
| `0xd5e0c` | EAX:ef_addr32 | EAX:uint32_t | 98050@980af | Register the block REC (size at +5, address at +9) with the trace layer: touch each of its pages, mark the range (0x1049 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcee88` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t,ECX:int32_t | none | a0f84@a11a1 | Add the direction (from the sine/cosine tables at 0x141e08/0x142e08, indexed by the negated angles A and B divided by 8) |
| `0xc94f4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | a5ca4@a5cde,a5ca4@a5d17,a5ca4@a5d54 | Clip the rectangle (X0, Y0)-(X1, Y1) to the screen size 0x15b744 x 0x15b748 (ret 4, Y1 on the stack); a rectangle entire |
| `0xd2320` | EAX:ef_addr32 | none | d23d8@d243e | Prepare the shape list of OBJ (list at +0x4c: count, then entry pointers): shift each entry's three coordinates (+0x24,  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe58a8` | none | none | a5ef8@a5f9a,a5ef8@a5fcb,a6d00@a6dc6 | Clip the rectangle at 0x41ec08 (x, y, width, height words) to the screen: a negative origin moves into the size, a recta |
| `0x4f718` | EAX:uint32_t,EDX:uint32_t | none | 4f3e0@4f481,4f494@4f535,4f548@4f5f5 | Update a row of four button widgets of the group IDX under the root named at 0x184a8c: clear the dirty flag of widgets " |
| `0xe330c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | e393c@e39de,e393c@e39f6,e393c@e3a29 | Decode a run/literal stream from the resource into DST until COUNT bytes are produced: each record is a run length (fill |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x106b1c` | none | none | none direct | One-time runtime teardown guarded by the flag at 0x17d360: register the handler pair (0x105230, 0x106b1c) with 0x1048b4  |
| `0x35f04` | none | none | 3617c@36258,3617c@3628a,36438@364ba | Walk the byte list at [0x136b74] (length byte at [[0x136b78]]): for every entry whose descriptor (32-byte records at 0x1 |
| `0xd5d18` | EAX:uint32_t | EAX:uint32_t | 97f90@97fb1 | Start the trace layer once (returns -1 when already started): set the started flag 0x15b968, configure it with A, initia |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x105230` | none | none | 105e38@105e3e | One-time runtime init guarded by the flag at 0x17d360: register the handler pair (0x105230, 0x106b1c), mark nine ranges  |
| `0x50b68` | EAX:uint32_t,EDX:uint32_t | none | 5089c@508ef,508fc@5094f,5095c@509af | Store the state code for KIND (0..11 -> 0, 1, 2, 3, 4, 5, 0x12, 7, 6, 0x10, 0x11, 0x16) in the byte at 0x1a553b + INDEX; |
| `0xcf960` | EAX:ef_addr32,EDX:int32_t,EBX:int32_t | EAX:uint32_t | cfa24@cfa66,cfde4@cff4b | Test whether the point (PX, PY) lies inside (or on the edge of) the triangle whose three 16-bit (x, y) vertices are stor |
| `0x9f710` | EAX:int32_t,EDX:uint32_t,EBX:int32_t,ECX:int32_t | none | 9e7bc@9f360 | Like 0x9f664 for an explicit point: store Y at 0x417226, let the hook at 0x132af4 transform (A, Z), add the offset 0x417 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe5cf0` | EAX:int32_t,EDX:int32_t,EBX:int32_t | none | e5db4@e5e15,e5db4@e5e74 | Draw the 7x7 bitmap glyph GLYPH (7 row bytes at 0x15fab0 + 7 * GLYPH, bit 7 first) at (X, Y) in the colour [0x433eb0], c |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd1924` | EAX:ef_addr32,EDX:ef_addr32 | EAX:uint32_t | d19ec@d1ad9 | Run-length encode [0x15b7cc] bytes from SRC into DST (PackBits style: a control byte 1-L followed by one byte for a run  |
| `0x5ee80` | none | EAX:ef_addr32 | 9114@9160 | Find the entry (0x54-byte records at 0x1a6b3c, count byte at [[0x1a6b08] + 0x13]) whose position is within 0x7fff of the |
| `0x801e4` | EAX:int32_t,EDX:uint32_t,EBX:uint32_t | none | 14c5c@14ce6,14c5c@14d11,1851c@18574 | Claim a UI control slot for the object INDEX: when the feature byte 0x12b070 is set and the count (0x12b07c when FLAG, e |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x48c90` | [ESP+4]:uint32_t,[ESP+8]:int32_t,[ESP+12]:int32_t,[ESP+16]:ef_addr32 | none | 3b58@3b78,3b58@3c13,3b58@3c66 | Draw the formatted string FMT (cdecl, variadic) at (X, Y) in the colour byte COLOR with the renderer 0xe605c (doubled co |
| `0xf314c` | none | none | f3b78@f3c0a | Read a list of number pairs from the script into the table entry [0x1616e8]: each pair (two parsed integers, stored as w |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcec00` | EAX:uint32_t,EDX:int32_t,EBX:int32_t,ECX:int32_t,[ESP+4]:int32_t | none | 26424@265f0,387e0@38a83,3bc90@3bcb8 | Fill the rectangle (X0, Y0)-(X1, Y1) with the colour byte COLOR, clipped to the rectangle [0x15bdc0..0x15bdc4] x [0x15bd |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x65570` | EAX:ef_addr32 | none | 654b8@654ed,654b8@6551c | Insert the record REC (fixed point position at +0 and +8, ids at +0xc and +0xe) into the list of the six nearest records |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe36bc` | none | none | e3444@e3468 | Write the string table of the resource being built through the writer at [0x42922c]: the 2-byte count word 0x15eea0, the |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3d950` | none | none | 3dd30@3dfae | For each of the [0x1a5a86] 26-byte trigger records at 0x12b268 whose flag test (0xa0e98 on the id word) succeeds and whi |
| `0x6e74` | EAX:uint32_t | none | 8c49c@8c88c | Bind the object ID (looked up with 0x7489c) to the first free entry of the 5-byte table at [0x197260] (0x121e55 entries; |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x66fc` | none | EAX:uint32_t | 6460@669f | Heading from the view origin (0x1a5bec, 0x1a5be8) to the current object: the object (word 0x1f71ca) is looked up, its po |
| `0x7ae88` | EAX:uint32_t,EDX:int32_t,EBX:uint32_t,ECX:int32_t | EAX:uint32_t | 161d4@164a7,1df90@1e040,56084@56141 | Look up the cell of the plane map for the position (X, Z) (both in units of 2048; wrapped to 400 with the original off-b |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x28aa8` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | AL:uint8_t | 28ed8@291b5 | Set up the line-of-sight query between the fixed point positions A and B (both 24.8) for an object of KIND: the clearanc |
| `0x11014` | EAX:ef_addr32 | EAX:uint32_t | 110ec@1119b | Find the entry of OBJ's list (count word at +0x2a, pointer at +0x2c, 21-byte entries with the word pair at +7/+9) that m |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x5ae70` | none | none | 5af50@5af80 | Build the three 24-entry offset rings (radii 25, 20 and 15 at 0x1a69e0, 0x1a6aa0 and 0x1a6a40; two words per entry: cos  |
| `0x7af60` | EAX:uint32_t,EDX:int32_t,EBX:uint32_t,ECX:int32_t | EAX:uint32_t | 1df90@1e096,82478@826a8,86f7c@8718e | Like 0x7ae88 for the query in progress: returns 1 while the endpoint word 0x420908 is non-negative, otherwise looks up t |
| `0x9a544` | none | EAX:uint32_t | 9ab30@9ab31 | Open the next numbered log file: build the name from the template at 0x98bd0, the running number (word 0x12dda8, post-in |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x14c5c` | EAX:int32_t,EDX:uint32_t | none | 589c0@58a1b,589c0@58a2a,589c0@58a39 | Set the state byte of the 19-byte record IDX at 0x1225d4 to STATE; when it changed and the current object is not locked  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdd788` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | a8578@a85a2 | Draw a string with the first extended font (regular glyphs 0x15e7cc / widths 0x15e2ea, extended glyphs 0x15ec70 / widths |
| `0xdd370` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 275e0@27611,275e0@27631,275e0@2766d | Draw a string with the narrow extended font (regular glyphs 0x15e4e8 / widths 0x15e31d, extended glyphs 0x15e898 / width |
| `0xdd26c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | a8578@a85f1 | Draw a string through the glyph routine 0xdcf28 (upper-cased, spaces and tabs advance by 6). ORIGINAL BUG: the character |
| `0xa8578` | EAX:int32_t,EDX:int32_t,EBX:uint32_t,ECX:uint32_t | none | a8660@a86d6 | Draw the text at ECX with the font selected by the byte at [[0x4191d0] + 0x10] (0..9: 0xdd788, 0xf23ac, 0xf261c, 0xdccd8 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9ca57` | EAX:ef_addr32,EDX:ef_addr32 | EAX:ef_addr32 | f80@1127,45a4@45cc,45a4@45dc | strcat: append the string SRC to DST; returns DST. |
| `0x117b0c` | EAX:uint32_t,EDX:uint32_t | none | 117b18@117bc4,117bfc@117d2a,117e0c@117e67 | outp: write the byte VALUE to the I/O port PORT. |
| `0x9c430` | EAX:uint32_t | EAX:uint32_t | 117b18@117b38,117b18@117b67,117bfc@117c39 | inp: read a byte from the I/O port PORT. |
| `0xdc832` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | EAX:ef_addr32 | 1f994@1fbc8,233c4@235f1,233c4@2364f | memcpy: copy COUNT bytes from SRC to DST (dwords first, then the tail bytes); returns DST. |
| `0x9c858` | EAX:ef_addr32 | EAX:ef_addr32 | e20@f2a,28224@28259,283c8@28473 | getenv: search the environment table at [0x160a18] (null terminated array of "NAME=value" strings) for NAME, ignoring ca |
| `0xd099c` | EAX:ef_addr32,EDX:ef_addr32 | none | 91df4@91e04,91df4@91e75,91f30@91f3c | Transpose the 3x3 matrix of dwords at SRC into DST. |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdc2e0` | none | none | 1e480@1e551,1e480@1e5c7,1e480@1e65e | Wait for the vertical retrace: poll the VGA status port 0x3da (at most 100000 reads each) until the display-enable bit ( |
| `0xfa95f` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | bc4da@bc518,f2183@f21d9,f2183@f2216 | DOS lseek (int 21h, AH = 0x42): move the file HANDLE to OFFSET relative to WHENCE. Returns the new position (DX:AX), or  |
| `0x2b0` | EAX:uint32_t,EDX:uint32_t | none | 324@33b,3d0@3ee,880@892 | intdos: run DOS interrupt 0x21 with the registers from the block IN (EAX, EBX, ECX, EDX, ESI, EDI at +0..+0x14) and the  |
| `0x104840` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | 101a90@101aaa,103d80@103d96,104908@104914 | Run DOS service 0x252b / 0x501 over the address range [min(FIRST, LAST), max(...)] (cdecl). Returns 1 when the carry fla |
| `0x1048b4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | 103d48@103d5e,1046c4@1046da,104920@10492c | Like 0x104840 for the service 0x252b / 0x61, issued through intdos (0x2b0). ORIGINAL BUG: 0x2b0 does not store the carry |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9e508` | EAX:int32_t,EDX:uint32_t | none | 9820@9ae3,17754@178ad,1cde8@1ce2a | Run the INDEXth text of the page PAGE: select the page (word 0x41722a, record 0x417174 from the id lookup, table entry 0 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xbef2c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:int32_t | bf444@bf4b2 | Sum, over up to 8 leading characters of the string returned by 0xfb87c (stopping at '%'), of the difference of two per-c |
| `0xeb2c0` | [ESP+4]:ef_addr32 | none | 510@572,628@647,628@65a | Printf-style message (cdecl, FORMAT and varargs on the stack) formatted into a 0x280-byte buffer, then, if the counter b |
| `0xeb174` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:uint32_t,[ESP+24]:uint32_t,[ESP+28]:uint32_t,[ESP+32]:uint32_t | none | c92e4@c9350 | Copy COUNT rows through 0xe9a24: the destination starts at base 0x15b754 + row-offset table 0x163200[Y] + X and advances |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x5c9d4` | EAX:uint32_t,EDX:uint32_t | none | 5c518@5c7cf,5ca60@5ca7b | Draw the horizontal tick marks of a cursor at the point stored in OBJ (x word, y word): refresh through 0x39a0c, plot th |
| `0xca200` | EAX:uint32_t | none | ac0e0@ac108 | Load the record file NAME: build the default path with NAME appended and the suffix 0x18f058, open it through the indire |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb4db0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | c0b04@c0bc2 | Find the first node of type 0x31 whose byte +0xb passes 0xcc1ac for KIND and PARAM. KIND 0, 2 and 5 walk forward from th |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x89ac0` | EAX:uint32_t | none | 8a318@8a54c | When the object's target record (object+0x30 -> +0x4b) is in range (0x89b58), may be swapped in as the object's own targ |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe3b7c` | EAX:uint32_t | AL:uint8_t | 97dcc@97dd9 | Check a data file: open NAME, read a 2-byte header into 0x15f900 and a 4-byte length, and succeed (1) only when the leng |
| `0xe2ba0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | e2c40@e2ceb | Fill a diamond of colour COLOR (byte) into the 8-bit frame buffer: top row at Y - SIZE, starting 1 pixel wide at column  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe8484` | EAX:int32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 7b3fc@7b4c5 | Draw the outline boxes of the scene entry KEY at offset (X, Y, Z): the offset is stored in 0x464370/74/78, the entry kin |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8d330` | EAX:int32_t | none | 8d59c@8d6e7 | Retire the unit INDEX: skipped when its owner byte (+0x18) equals the current player byte 0x1a666b or the owner word is  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xeab44` | none | EAX:uint32_t | 97dcc@97e2f,97dcc@97e73,a8bd4@a8dba | Timed record pump: when the clock 0x10dfe0 has advanced by at least the interval word 0x484466 since 0x484154, re-reads  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe605c` | EAX:uint32_t,EDX:int32_t,EBX:int32_t,ECX:uint32_t | none | 48bb0@48c26,48c90@48cf0,48c90@48d0a | Draw the NUL-terminated string STR at (X, Y) in colour COLOR (stored in 0x433ec0) with 8-pixel cells, clipped against th |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x106644` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 102670@102693 | Reset the stream control block P (cdecl): fixed defaults in the fields up to +0x40 (the clock word 0x4dd7e0 goes into +0 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x7fc34` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 7ef14@7f21a,7fcdc@7fd79 | Remaining share of entry INDEX in the table at 0x1f9078: when the record's size (0xd7730) is 1 the value is the record's |
| `0x101ba8` | none | EAX:uint32_t | 101c54@101db4 | Write the status line to the stream 0x4dd620: with the mode word 0x4dd634 equal to 1 one formatted line from the values  |
| `0xf2ac0` | none | none | f3f84@f4034 | Frame update of the remote view when either activity flag (0x41f7c6, 0x41f7ba) and the enable flag 0x41f7a0 are set: fet |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe5db4` | EAX:uint32_t,EDX:int32_t,EBX:int32_t,ECX:uint32_t | none | 48bb0@48c11 | Draw the NUL-terminated string STR at (X, Y) in colour COLOR (stored in 0x433eb0) with 6-pixel cells (glyph = character  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb4ee0` | EAX:uint32_t | EAX:int32_t | b7d4c@b7fca,b8510@b896a,b8510@b8c2b | Pick the behaviour class of the object OBJ from its type byte (+2) and store the matching handler table in +0x74. Types  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x15a14` | EAX:uint32_t,EDX:int32_t,EBX:int32_t,ECX:uint32_t | none | 3f7fc@3fb6c,3f7fc@3fbf9,3f7fc@3fd32 | Draw STR in colour COLOR with the proportional bitmap font at (X, Y): a space advances 3, characters with bit 7 set use  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc90e0` | none | none | ac408@ac508 | Show the splash picture named by the string at 0x41ef06 (dropped when 0x9d524 rejects the name): allocate the 0x3e800-by |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcdd18` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | cde00@cdf39 | Plot a four-plane sprite (width 0x4201a0, height 0x4201a4) read from SRC into the frame buffer at DST: pass P (0..3) wri |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd8528` | none | none | d86e0@d8912,d86e0@d8953,d86e0@d8c96 | Compute the five projected corner words 0x15baf2/f6/fa/fe (and the constants 0x15baf4, 0x15bb00 = 0, 0x15baf8 = 0x15bafc |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdde80` | EAX:uint32_t | EAX:int32_t | ddce8@ddd13 | Issue the DOS call 0x2f for the date/time request with the low byte of VALUE: fills the request block 0x4260b0 (bytes 0x |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x73734` | EAX:int32_t,EDX:int32_t | none | 46454@4674d | Record that unit A now targets unit B (word +0x143 of A's record). When A is the player unit (word 0x1f71ca) the target' |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcb6fc` | EAX:uint32_t,EDX:uint32_t | none | b0f5c@b139a | Walk the list whose head pointer is at HEAD (links at +4); for every node of kind 0 (byte +0xb) with all four group flag |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x78848` | none | none | 79128@79162 | Draw the numbered entry list kept at 0x1a521c (byte ids terminated by 0xff, at most 0x7f): unless the mode byte 0x127da8 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa48ac` | EAX:uint32_t,EDX:uint32_t | none | a499c@a49f8,a4a20@a4a7c | Mark the item ID (1..count in 0x41eed8, flag 8 set in byte +0x3b of its record in the table 0x41edfc) as ordered with fl |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x39cb4` | EAX:uint32_t,EDX:uint32_t | none | 5c518@5c5ca | Distance between the player unit (word 0x1f71ca) and the unit named by 0x1a823e, from the position longs (+0, +4, +8) sc |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa35bc` | none | none | a410c@a411e | Fold the per-slot samples into running statistics: for each entry of the id list 0x136b74 (count byte at *0x136b78) take |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x49bdc` | EAX:uint32_t | none | 49228@49939,49228@499f9 | Rebuild the group tables from the list node reached by following the next links (+4) of P up to the word 0x125835 times: |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe5f64` | EAX:int32_t,EDX:int32_t,EBX:uint32_t | none | e605c@e60b9,e605c@e6106 | Draw one 7x9 monochrome glyph (CODE bit 7 selects the table 0x15feb0 with index & 0x1f, otherwise 0x15fc70 with index &  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x50658` | none | none | 4fa64@4fadc | Tally the roster kinds: for each of the 0x1a5531 entries (kind byte 0x1a5510[i], 1..4) count the kind in 0x1a5536[kind - |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf2b7c` | none | none | f4178@f419b | Reset the remote cursor state: run 0xcdf60, copy the buffer pointer 0x485928 to 0x485924, clear the words/bytes 0x1616ea |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3739c` | EAX:int32_t | none | 17388@1739c,90074@900b5 | Compact the nine slot words (+0x28, stride 0x45 per group in the table 0x1a14f8) of the group UNIT: a slot is kept when  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8ae6c` | EAX:uint32_t | EAX:uint32_t | 69094@690e8,69094@69190,69094@692c4 | Map the key code 1..36 to its command number through a jump table (0 and anything above 36 report 0x18a7a4 through the e |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa060c` | EAX:int32_t | none | 9e7bc@9e8be | Start the script record number N of the current page. |
| `0x9fb00` | EAX:int32_t | none | 9e7bc@9e854 | Start the script record number N of the current page, shifted down by the offset word 0x417222. |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfb954` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | fc1e8@fc2ac,fc1e8@fc333 | Allocate a 4-byte route record (tag 0x1935bc) and fill it from the class A, sub-class B and slot C: class 0xe with B = 9 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1a47c` | none | none | 1cde8@1ce4b | Dispatch the pending key byte 0x41f833: refresh the flag words 0x19843c (0x41f7ba / 0x41f7c6), 0x198420 (0x41f7c8) and 0 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11de0` | EAX:uint32_t | none | 1a47c@1a4e3 | Look the key KEY up in the letter table of the current mode (mode byte 0x122c53: 1, 2 and 4 select table 0, 1 and 2): sc |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x74480` | EAX:int32_t | none | 74970@74ad6,74970@74bc2 | Credit a hit by the unit A on the unit named by the word 0x197c28 (ignored when either is 0xff/-1, missing, or flagged 4 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x757bc` | EAX:int32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 759b8@75c19,76370@76407 | Blend COUNT palette triples starting at triple START between the palettes FROM and TO into DEST: each 6-bit component be |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x111c90` | none | none | 1133f8@1133fe | One-time registration (flag 0x17e25c) of the data ranges of this module: the code range 0x111c90..0x114ad8 through 0x104 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x779f4` | EAX:uint32_t | none | be48@be5f,be8c@beab,bed8@bef9 | Randomise the pilot record OBJ: a random 1-in-3 (word / 0x3333 equal to 1) picks the first name table (flag +0x34 = 1) i |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa207c` | EAX:uint32_t,EDX:uint32_t | none | a2634@a268f,a2698@a271a | Step the animation script of OBJ (script pointer +0, time +4, wait +6): once the time word is at least 0x4000 the wait i |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd19ec` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | d1afc@d1bb9 | Pack the 8 bit planes of an image row block: SRC holds 0x15b7cc groups of 8 bytes (bit 0 of each is one pixel's plane bi |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x740e4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 7ae4@7b21,7bc0@7c96,2ced4@2d0b7 | Assign the unit A to the slot SLOT (stack byte, 0xff = none) of its owner's roster: the unit's previous slot (byte +0x20 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x104580` | none | none | 101c54@101c7d,101c54@101cb4,101c54@101d58 | Reset the 19 runtime settings (via 0x101e14: index, value) to their defaults after 0x103d80, clear the 16 dwords at 0x4d |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcdb20` | none | none | 8b2d8@8b2de,aaee8@aaf77,d1e50@d1e89 | Set up the 320x200 video mode: on the first call read the environment string at 0x18f620, flag the options named 0x18f62 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9ab6c` | EAX:uint32_t | none | 9ab30@9ab38 | Dump the page header table at 0x40d006..0x40d024 as text lines to the stream STREAM through 0xbc400 (formats 0x13179b..0 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x77a8` | EAX:uint32_t | none | 49228@495b2 | Initialise the roster state for N units: word 0x1972e4 = N, the table 0x1972e0 from 0x7780(N, 0x22), the anchor/limit lo |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x102028` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | EAX:uint32_t | 106514@106526,113af0@113b02 | Traced lookup (cdecl, two stack arguments): calls 0x103f04 and, with tracing on (0x17d2c8 == 1), logs the request (forma |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3b58` | none | none | 3dd30@3e30b | Draw the three status lines of the tactical header: the caption 0x1320d7 (colour 12, x = 0x112, y = 1), the target/mode  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x45574` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | 426a4@426d6 | Run the enabled stages of the cockpit update for the view object A (EAX) with the secondary values S (EDX) and B (EBX):  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa5ef8` | EAX:uint32_t | none | 4aa0@4af8,4cf4@4d42,4d4c@4e08 | Redraw the item ID (1..count, flag 4 set in byte +0x3b of its record): outside a draw (byte 0x13725c clear) the state is |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x78e1c` | none | none | 78b4c@78dc3 | Draw the mission clock at (10, 0xbe) in colour 15: in mode 1 (byte 0x125846) the time is taken from the record at 0x1379 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3a148` | EAX:int32_t | none | 2d488@2d6f8 | Occasionally (1 in 100, or always in mode 3 of 0x125846) advance the follow slot of the unit A's owner target: the targe |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xac2e4` | EAX:uint32_t,EDX:uint32_t | none | ac240@ac274,ac240@ac295 | Register the text file NAME in the recent-file list (21-byte names at 0x41e960, count word 0x41eecc, limit 16 with error |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x56318` | none | none | 3dd30@3e70d | Tick the broadcast ring entry 0x127db8 (55-byte entries from 0x1a63c0, at most 10): its countdown word (+8) falls by the |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x89698` | EAX:uint32_t | AL:uint8_t | 897c4@8989e | Pick the closest enemy for the unit OBJ: for every group of the table 0x1a14f8 (0x45-byte entries, 0x1a1506 of them) who |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9ccb8` | EAX:uint32_t | EAX:uint32_t | f80@112e,f6a8@f71b,fecc@ff0e | Open the resource NAME: first through the archive handles 0x40d1f0 (option bit 1 of 0x132ab0, result / 0x10000) and 0x40 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb0b00` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | b0c34@b0c57 | Count the nodes of the list whose head pointer is at HEAD (links +4) that are of type 0x31 (byte +0xc), pass the filter  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf3a1c` | EAX:uint32_t | none | f3aa8@f3b10,f3b1c@f3b36 | Skip script lines until the token buffer 0x48592c matches NAME (0xa4380 == 0) or the end marker string 0x1919c8; if the  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa6808` | EAX:uint32_t | none | a6998@a69be,a74bc@a74f8 | Load the per-object geometry for REC from two 25-byte table entries at 0x13727e indexed by words +0x55 and +0x57 (nothin |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x4db60` | none | none | 4b6a0@4bb11,4d9cc@4da23,4da2c@4da88 | Fill two text widgets of the dialog named at 0x184b0c (widgets 0x184cf8 and 0x184d38, or 0x184d30 for the second when by |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x91df4` | none | none | 1c780@1cc33,1c780@1cd71,94194@941a5 | Apply the key-driven orientation nudges (byte flags 0x2144aa: 1/2 adjust the scratch matrix 0x21439c by +/-0x800, 4/8 ro |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x117bfc` | EAX:uint32_t | none | none direct | Serial port interrupt service for the channel record REC (port base at +0x1c, state block at +0x2c, receive ring descrip |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x57820` | EAX:uint32_t | none | 577ac@577da,577e8@57818 | Build the status message for the event record REC (bytes: +1 first slot, +2 second slot, signed, -1 = none; +3/+4/+5 sel |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x319ac` | none | none | 3122c@312ee | Update the camera position (dwords 0x19e5c1/5c5/5c9): rotate the offset (0, 0, -dword 0x19e597) by the angle pair at wor |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x76b80` | EAX:uint32_t | none | 8c49c@8c833 | Bind unit ID (16-bit) to the first free 25-byte entry (word +1 == 0xff) of the table at dword 0x1f8918 (count in byte 0x |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa94e4` | EAX:uint32_t | none | a9694@a96ba,a9694@a96eb,a9694@a9717 | Advance the playlist LIST (flag array at +0xe, entry count word +8, played count word +0xa, sequential flag byte +0xd, p |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe6848` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 49228@4939f,b0f5c@b1020,b0f5c@b1220 | Reorder the payload (bytes +9..+0x33) of the list whose head pointer is *HEAD_PTR (link +4) so that the node types (byte |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcde00` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t | none | dd8dc@dd97b | Draw sprite number IDX from the sprite table TABLE (byte count, then word offsets from the table) centred at (X, Y): the |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x6abac` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 6ae30@6aea1 | Pick a free object for the unit ID (16-bit) from candidate list K (word index into the 69-byte table at dword 0x1a14f8,  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe1570` | none | none | a3c18@a3c95 | Install the draw routine table (dwords 0x136ba0..0x136bc4, code addresses) for the renderer mode in byte 0x13579a: 0, 1  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf4a88` | none | none | f478c@f47bb | Start the script section at the top level: push a fresh node (0xf57d4), set its default view words (+0x4f/+0x51 = 0x14,  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x777f4` | EAX:uint32_t | EAX:uint32_t | 8de9c@8e006 | Test the position POS (three dwords) against the entries of the pointer table at dword 0x1f8900 (count word 0x1f891c, sc |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x84788` | EAX:uint32_t | none | 848e0@84939 | Latch the control bits of the flight record REC into the global control word 0x1fcbe2: clear bits 0 and 7 of its low byt |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x436dc` | EAX:uint32_t,EDX:uint32_t | none | 45574@4562c | Draw the map markers for the objects of the list at *LIST (nodes of type 0x0e and 0x17, link +4; group flag tables check |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd7158` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 7fbec@7fc13,803c4@803df,8160c@81672 | Start sample channel CHAN (< 0xe, else report through the callback at 0x132a74 with string 0x18f998) playing the sound r |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1df90` | EAX:uint32_t | none | 1d954@1daf8 | Ground impact test for the projectile REC (owner byte +0x27 must match byte 0x1a666b and the done flag +0x47 must be cle |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x4357c` | EAX:uint32_t,EDX:uint32_t | none | 45574@45651,4bf1c@4c03b | Variant of 0x436dc for the nodes of type 0x14: the position words come from 0x1344f9/fb (positive flag), 0x1344f0/f2 (ne |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xbb324` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | 7d2f8@7d6d1,7d2f8@7d892,7d2f8@7daeb | Clear the 0x14-byte record REC and initialise it for event KIND (0..0x24) by calling the matching initialiser (EAX = REC |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfe700` | none | EAX:uint32_t | cdb20@cdb97,cdc64@cdc6c | BIOS get video mode (int 10h, AH = 0xf): returns the current mode number in AL (EBX, ESI and EDI are preserved). |
| `0xa45e4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 4050@405c,bfcc@c0f1,a9e70@a9e7e | Store the stack argument VALUE into the code-patch dword at 0xa45a4 (stdcall-style frame, ret 4). |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1111ca` | none | none | 101f44@101f67 | Set the dword at 0x17d7c0 to -1 (frame with saved registers, no arguments). |
| `0x1111b1` | none | none | 101f08@101f38 | Compare the dword at 0x17d7c0 with -1 and do nothing either way (an empty check left over in the runtime). |
| `0xeb2a8` | none | EAX:uint32_t | 9c1e0@9c1e3 | DOS allocate memory with the impossible request BX = 0xffff (int 21h, AH = 0x48), which fails and returns the size of th |
| `0x111142` | EAX:uint32_t,EDX:uint32_t | none | 101ed4@101efe | DOS4G set real-mode interrupt vector (int 21h, AX = 0x2505) for interrupt number CL = NUM (low byte) with EBX = VALUE; c |
| `0xdff4c` | none | none | none direct | Poll the serial line status register (port dword 0x15eed0 + 5): when its data-ready bit is set call the receive handler  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcd3c0` | EAX:uint32_t | none | cd414@cd41d,cd414@cd426 | Send the byte VALUE to the keyboard controller (port 0x60) and wait for the 0xfa acknowledge, retrying up to three times |
| `0xcd414` | EAX:uint32_t | none | cd4d4@cd4e5,cd554@cd5d7 | Set the keyboard LEDs: command 0xed followed by the LED mask byte, with interrupts disabled. |
| `0x11d63f` | none | EAX:uint32_t | 10f1bf@10f210 | Return the pending pushed-back character in dword 0x132a50 (clearing it) or, when none is pending, read one character fr |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1101be` | EAX:uint32_t | EAX:uint32_t | fa4a9@fa504,fa4a9@fa605,fa7d2@fa7e0 | DOS IOCTL get device information (int 21h, AX = 0x4400) for HANDLE: true when bit 7 of the device word (a character devi |
| `0xfe541` | EAX:uint32_t | EAX:uint32_t | none direct | DOS delete file (int 21h, AH = 0x41) NAME: 0 on success, otherwise the translated DOS error (0x1101e8) of the failing co |
| `0x10f0e8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | f1d90@f1e7d,10f1bf@10f23c | DOS read (int 21h, AH = 0x3f): read COUNT bytes from HANDLE into BUFFER. Returns the byte count, or the translated DOS e |
| `0xfe710` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | cdb20@cdbae,cdc64@cdc83 | Set the BIOS video mode MODE (int 10h) and read it back (AH = 0xf): returns 1 when the mode now equals the requested low |
| `0xeb288` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | 9be38@9bf6d | DOS allocate memory (int 21h, AH = 0x48) for PARAGRAPHS (16-bit): returns the segment/selector, or 0 when the allocation |
| `0xded1a` | EAX:uint32_t,EDX:uint32_t | none | decde@decf4,decfb@ded10 | When the file-search mode byte 0x132a5e is 9, copy the 0x2b-byte DOS DTA (address from int 21h, AH = 0x2f) to the buffer |
| `0xded3d` | EAX:uint32_t,EDX:uint32_t | none | decfb@ded02 | The reverse of 0xded1a: copy the 0x2b bytes at SRC into the DOS DTA. |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xed542` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | ed6cc@ed872,ed6cc@edaf2 | Length of the string at SEG:OFFSET (the selector SEG is ignored in the flat model), limited to MAX: counts characters un |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xea49a` | EAX:int32_t,EDX:int32_t,EBX:ef_addr32 | none | 8b328@8b35c | Signed divide helper with implicit registers (EAX = dividend, EDX = divisor, ESI = result pointer): store the quotient a |
| `0xfedcc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | fdf60@fdff2 | Plot one pixel with implicit registers (ESI = row, EBX = x offset, CH = colour): address = row table 0x163200[row] + X + |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfeddc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | fdf60@fdfdb | Plot a two-pixel-wide, two-row block with implicit registers (ESI = row, EBX = x offset, CH = colour): both bytes of the |
| `0xc59b7` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | none | c4680@c4690 | Shift three consecutive dword triples left by SHIFT (CL) with implicit registers (ESI = source, EDI = destination): the  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc5409` | EAX:uint32_t,EDX:uint32_t | none | c552e@c5540,c565e@c5670 | Divide the three table entries at index ESI (dword arrays 0x150e58, 0x152e58 and 0x154e58) by the signed byte DIV (AL);  |
| `0xc5441` | EAX:uint32_t,EDX:uint32_t | none | c552e@c5549,c565e@c5679 | Multiply the three table entries at index ESI (arrays 0x150e58, 0x152e58, 0x154e58) by the signed byte MUL (AL, sign ext |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc54ef` | EAX:uint32_t | EAX:uint32_t | c5551@c5562,c5681@c5692 | Fetch the scaled value for table index EDI (implicit register): clear byte 0x158ed8, take the signed word 0x14fe58[index |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc47e0` | EAX:uint32_t | none | none direct | Select the shade table for table index EDI (implicit register): take the signed word 0x14fe58[index] >> 5 clamped to 0.. |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc48dc` | EAX:ef_addr32,EDX:uint32_t | none | c49c4@c4a10,c4a50@c4aaf,c4aeb@c4b4a | Load COUNT 2D vertices (word pairs) starting at SRC (implicit registers ESI = source, ECX = count) into the 48-byte vert |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11afb4` | EAX:uint32_t | EAX:double | 10a0d4@10a0f8 | Box shape ratio for the doubles at BOX: 2 * area (0x11af60) / (extent sum (0x11af9c))^2; the constant 2.0 sits at 0x1958 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc737c` | EAX:ef_addr32,EDX:ef_addr32,EBX:ef_addr32 | EAX:uint32_t | c71dc@c72c3,c71dc@c72d4,c71dc@c72e5 | Project the sum of two 3D points to a 2D screen point (implicit registers ESI = first point, EBX = second point, EDI = d |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x4e7b8` | EAX:uint32_t | none | 4dcd8@4dd77,4dcd8@4ddec,4dcd8@4dea9 | Update the counter widget (0x184d4c) of the dialog named at 0x184b3c: with SHOW (AL) nonzero it shows the decimal value  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1047e4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t | EAX:uint32_t | 104200@104244,105e38@106107 | Try to allocate PARAGRAPHS of DOS memory through int386 (int 21h, AH = 0x48). Cdecl (PARAGRAPHS, BLOCKS_OUT, SEGMENT_OUT |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcd82c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:uint32_t,[ESP+24]:uint32_t,[ESP+28]:uint32_t | none | 11e7c@11f50,38be8@38cd2,ca290@ca42c | Masked blit from a 256-byte-wide source bitmap to the screen (colour 0 is transparent). Stack-only (ret 0x1c): Y, X (des |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe8784` | [ESP+4]:ef_addr32 | EAX:uint32_t | e882c@e888e,e882c@e891d,e882c@e89b1 | Variadic cdecl diagnostic: when byte 0x1600a6 is set, format FMT with its arguments into a 0x140-byte stack buffer and p |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x56268` | EAX:ef_addr32 | none | 55f54@55ff0 | Append a message entry to the ring of ten 55-byte records at 0x1a63c0 (next index in dword 0x127db4): clear the first ei |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10e2f5` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | none | 120560@120715 | Convert the unsigned 64-bit value at VALUE_PTR to exactly N decimal digits (zero padded, high digits truncated) at BUF,  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10d9e3` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | ea20c@ea234 | bsearch: find KEY in the sorted array BASE of N elements of SIZE bytes using the comparison callback CMP (stack argument |
| `0xc486c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | d9e50@d9e6b,da1b4@da211 | Set the scale pair from the stack argument VALUE (ret 4): for VALUE < 10 word 0x158ed0 = VALUE and word 0x158ed4 = 1 <<  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x3519c` | EAX:ef_addr32 | none | 35590@3562d | Unpack the 0x4c-byte settings record at SRC into the game globals: copy the first 44 bytes to 0x125828, scatter the byte |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdf738` | none | none | none direct | Poll the modem status register (port dword 0x15eed0 + 6, saved in byte 0x15eed4): in mode 2 (byte 0x13579a) with the del |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11458c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 1037e0@10380a | Cdecl (REC, KEY): look KEY up in the 6-byte entry table of the animation set at REC+0xc (count: signed word +8, entries  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x31774` | none | none | 3122c@312c3 | Reposition the camera from the angles at 0x19e572/0x19e560 (0xdc91c) by the offset (0, 0, -dword 0x19e597) rotated into  |
| `0xdfeb8` | none | none | none direct | Serial line status handler: store (line status (port 0x15eed0 + 5) >> 1) & 0xf in byte 0x15eed5; when bit 3 of that is s |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x55f54` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:uint32_t,[ESP+24]:uint32_t,[ESP+28]:uint32_t | none | 888cc@88b79 | Build and send the 43-byte command packet of type 0x29: fields A (EAX), B (EDX), C (EBX) and D (ECX) at +1/+5/+9/+0xd, t |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x4d24c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 441fc@443a5 | Draw the stacked-count icon for the record REC at (X, Y) in game modes 1 and 2 (byte 0x1a53dc) when REC word +0x36 is se |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x81394` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | 77c90@77ef3,77c90@77fc6,77c90@77ffa | Reserve cost from the budget dword 0x12b118 for the entries of the word list LIST (first word = count, then entry indice |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8ca24` | EAX:uint32_t,EDX:uint32_t | none | 8c49c@8c770,8cb60@8cb90,8d330@8d3c7 | Initialise the 20 three-byte weapon slots of the unit REC from the loadout record INDEX (202-byte entries at dword 0x19b |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x369fc` | EAX:uint32_t,EDX:uint32_t | none | 35590@3568b,35590@3569c | Open the stream file NAME for the player (flag byte MODE goes to 0x19f5a0): complain through the callback at 0x132a74 wh |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb2758` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | EAX:uint32_t | b2b00@b2c62 | Print the records of the list whose head pointer is at HEAD_PTR (link +4) that are of kind 0 (byte +0xb) and whose coord |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd2584` | EAX:ef_addr32,EDX:uint32_t | none | c49c4@c4a3a | Build the render object at 0x420970 from COUNT vertex indices (word list at LIST, implicit ESI; COUNT in ECX): set dword |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x510d0` | none | none | 52164@522a7 | Load the language text file: build the path (default directory + 0x18515c + the decimal word from the high half of dword |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x150d4` | none | none | 5f60@5fc4,5fd8@600d,6c1c@6c90 | Draw the 0x22 label entries of the table at 0x1225d4 (19 bytes each: kind byte +1, box words +2/+4/+6, height word +8, t |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x95704` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 957c8@9585b | Set up the external view from the angle words A, B, C (EAX, EDX, EBX), D (ECX) and the stack argument E (ret 4): reset t |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1e6f0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 1e7c0@1e9c9,1e7c0@1ea1b | Draw an icon (sprite pair) for COLOR (EAX) with SHADOW flag (EDX) at the position (X, Y) (stack arguments, ret 8) displa |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xbc287` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | bc34f@bc362 | fopen core: open the file NAME with the mode string MODE into the stream record FILE (flags dword +0xc, handle +0x10, bu |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x4dee0` | EAX:uint32_t,EDX:uint32_t | EAX:int32_t | 4dcd8@4dd39,4dcd8@4de13 | Hit test the 24 map icons (index 1..24) against the point (X, Y) (16-bit signed): icon I has its centre at the words 0x1 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1014a4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:int32_t | 1016e0@1017ba,1016e0@1017f4 | Set up the scan-line interpolation between the vertex records P0 (EBX) and P1 (ECX): N = the integer part (>> 16) of the |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfc8cc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | c46f8@c4720,d2110@d2120 | Cdecl-style (COUNT, LIST; ret 8): project COUNT (byte) vertex records whose pointers are listed at LIST: skip those alre |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd2634` | EAX:ef_addr32,EDX:uint32_t | none | c4a50@c4adf | Textured variant of 0xd2584: build the render object at 0x420970 from COUNT vertex indices (word list at LIST, implicit  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe51c4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | e52a4@e56ff,e52a4@e5739,e52a4@e579e | Add a clipped rectangle to the dirty-rectangle list at 0x4339a0 (8-byte entries, running index in dword 0x433ea0, count  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa38f8` | none | none | a410c@a39d9 | Find the lowest rank (byte +5 of the entry record) among the flagged entries of two index lists: with byte 0x135798 set, |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8fe4` | EAX:uint32_t,EDX:uint32_t | none | 90c0@910b | Assign object INDEX (sign-extended word) to a free sub-slot of table TABLE: find a slot with 0x825c, clear its byte +0x2 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xdc43e` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | dc519@dc528 | Implicit-register fragment (EBX, ESI, EDI = three angles; params in that order). Builds eight rotation-matrix words at 0 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xed5ea` | EAX:uint32_t,EDX:int32_t,EBX:uint32_t | none | ed6cc@ed817 | Format the 16.16 fixed-point VALUE as decimal text at BUF with the precision in dword +8 of the format record FMT (-1 se |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10e4dd` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:double | 10e4d3@10e4d6 | Evaluate a binary-operation record: FLAGS (EAX) selects the operation kind (bits 0x40, 0x80, then 0x100, 0x800, 0x200, 0 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x2c454` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | 71fc@7214,7308@7325,7308@7343 | Search the 0xca-byte object records (count word 0x19bb5c, base dword 0x19bb58) for the match nearest POS (distance from  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe378c` | EAX:uint32_t | AL:uint8_t | 97dcc@97eb0 | Open the resource file NAME (handle stored in dword 0x15f8fc), read its count word into 0x15f900 and, when non-zero, all |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x6bca8` | none | none | 6b620@6b666 | Normalise the three angle-like doubles of the current object (dword pointer 0x1f71bc; fields +0x69, +0x71, +0x61 = X, Y, |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x7548c` | EAX:uint32_t,EDX:uint32_t | none | 77c90@77e72,77c90@780c9,78f3c@78fc1 | Build the display name of unit INDEX (16-bit, looked up with 0x7489c) into DEST. When the record's word +0x28 equals wor |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x7b08c` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 239ac@23c42,239ac@23d2f,26b60@27056 | Scale two 3-double vectors (A = EAX, B = EDX; components used in the order +8, +0x10, +0) by the double at 0x187c60, tru |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe6d78` | none | none | e6990@e6c44 | Fuel/energy-style update on the double globals: 0x4327e8 = 0x4327d8*0x432770*0x432da0/0x432da8*0x4327e0 + 0x432808*0x432 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x1132b8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | 1133f8@113592,1133f8@1135df,1133f8@113662 | cdecl (OBJ, REC): copy the 24-byte record REC, fill every part of it that is below 1 (four signed words, then four signe |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x18a68` | none | none | 1d318@1d369,92f10@92f1b | Advance the scene-node scan cursor (word 0x211ae2, starting at 0x2001) for the unit whose index is the signed word at 0x |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc8e58` | none | none | ac408@ac50d | When a data-set name is present (byte 0x41f5f0), build the two paths prefix+name+suffix (0x18eee0/0x18eeec, loaded into  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa81e4` | EAX:uint32_t,EDX:uint32_t | none | a82f0@a83a3 | Open a named script/data file: take the first whitespace-delimited token of NAME (EAX), format "<fmt 0x18d668>" from the |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x80c3c` | none | none | 3dd30@3e1fd | Reset the 16 twelve-byte slots at 0x1fc8a0 (word +0 = 0xffff), then walk the 256 fifty-six-byte effect records at 0x1f90 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x35cfc` | none | none | none direct | Prepare the briefing/mission screen: clear bytes 0x19f5aa and 0x19f5af, apply the key string 0x182f4c, refresh the statu |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x39874` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | 39548@395d6,39548@396fd,39548@39726 | Proximity level of the point (X = EAX word, Y = EDX word signed, Z = EBX word) to the reference position (dwords 0x19e5b |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x87fe4` | none | none | 3dd30@3e1d5,63d5c@6404d | Draw the nearby entries of the linked list whose head is dword 0x20f160 (sentinel 0x20f164, next link at +0x34): with th |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x76620` | none | none | 3dd30@3e01a | Set up the view window for the current viewport record (28-byte records at 0x125ad6, index byte 0x12db9e; X = dword +0 h |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9dcf8` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 9de08@9de3b | Register the resource NAME (EAX) under the key string TEXT (EDX): obtain its data either from the cached chunk (0xea3ec  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x16018a` | none | none | 9f850@9f935,9f940@9fac2 | Implicit-global fragment: append one entry to the table set indexed by word 0x146e52: bytes 0x149e58 (byte 0x1600b8), 0x |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xebee4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:double,[ESP+8]:int32_t | EAX:double | 9d55d@9d704,ebea3@ebec1,11d3f0@11d52a | Stack (ret 0xc): scale the double X by 2^N-style factors from a table. The special values at 0x195bfc / 0x195c04 are fir |
| `0x6eaf4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:double | none | 6d948@6e09a,6ebe0@6ef63,6ef74@6f127 | Stack (ret 8): clamp the double at 0x1f7110 to +-the limit at 0x1f7098, take its difference to the current object's fiel |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x6c1f0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:double | EAX:double | 28b80@28c1f,2ccf0@2cd1d,89c64@89daa | Stack (ret 8): rational estimate of a coefficient from the double X. R = (X + k) / (k - X) * 1.2 with k = double 0x18641 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe7d27` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 69094@69264,c3364@c337d | strstr(HAY = EAX, NEEDLE = EDX): the first occurrence of NEEDLE in HAY, or 0. An empty needle returns HAY; a one-charact |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd4510` | none | none | fddf0@fde0c | Fill one horizontal span with the colour byte 0x159138: from the signed word 0x485fea to the signed word 0x485fd2 (inclu |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf10ca` | EAX:uint32_t | EAX:uint32_t | a4574@a457d | Fill the broken-down time record TM (EAX) from DOS: dwords +0 seconds, +4 minutes, +8 hours, +0xc day, +0x10 month - 1,  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe7ea4` | EAX:int32_t,EDX:int32_t | EAX:uint32_t | da224@da284 | Build the 256-entry brightness ramp at 0x463f60 for the range LO..HI (EAX, EDX; LO >= 0 and HI < 256, else return 0): en |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfe560` | none | EAX:uint32_t | 10640@10640,34ca4@34d3f,34d50@34d6f | High-resolution timer read. The first call (dword 0x17cf98 clear) points 0x485fc4 at the BIOS tick counter (0x120cc6 + 0 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd6cac` | EAX:uint32_t | AL:uint8_t | 1c780@1c936,3d37c@3d3fc | When bit 0 of byte 0x15bac0 is set, store 1 - clamp(r, 0, 1) as a float at OUT (EAX), r being the float ratio dword 0x42 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd08fc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | 93170@9322c,9357c@93603,93a8c@93ae6 | 3x3 fixed-point matrix product OUT = A * B (OUT = EAX, A = EDX, B = EBX; nine dwords each, row major, results arithmetic |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x109a80` | none | none | e474c@e476d | Normalise the double globals: with k = 1 / double 0x433050, 0x432378/0x432380/0x432388 = 0x432a90/98/a0 * k and 0x432360 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd4430` | none | none | none direct | Draw one dithered shaded span: from the signed word 0x485fea to the signed word 0x485fd2 (nothing unless the end is afte |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf0034` | EAX:uint32_t | EAX:uint32_t | ee22c@ee6b9,ee22c@ee6ca | Convert the 80-bit extended value at PTR (EAX) to the bits of a 32-bit float (returned in EAX): round the mantissa to 24 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x118b10` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 108afd@108dc8 | Convert the double whose bits are EDX:EAX (EAX = low dword, EDX = high dword) to the bits of a float (returned in EAX):  |
| `0xd44b0` | none | none | none direct | Draw one shaded span: from the signed word 0x485fea to the signed word 0x485fd2 (nothing unless the end is after the sta |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc46f8` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | c680b@c6820,c73b0@c73b8,c73b0@c73c7 | Implicit-register fragment (EAX = MARGIN, EBX = vertex record pointer; params in that order). Remember the record in dwo |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x103e0c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | none direct | cdecl (OBJ): one-shot guarded notification. When the guard dword 0x17d2d0 is clear, set it, take the handle from 0x101f7 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x10dc1d` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 10da50@10daf0 | Implicit-register fragment (ESI = data pointer, ECX = byte count; params in that order): the two-byte running checksum u |
| `0xded60` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | dedde@dee7e,dedde@defce,dedde@df034 | Implicit-register fragment (ESI = first buffer, EDI = second buffer, ECX = byte count; params in that order): swap COUNT |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfc39c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | c39ac@c39ea,c39f0@c3a1f,cdb20@cdbcb | Stack (ret 8): fill the row-offset table at 0x163200 with COUNT dwords 0, STEP, 2*STEP, ...; store COUNT*STEP in 0x16460 |
| `0x117dd8` | EAX:uint32_t | EAX:uint32_t | 117f68@118196 | Return the code address (EAX; EDX is set to the code segment) of the INDEX-th (0..3) handler 0x117d38, 0x117d5c, 0x117d8 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc89cc` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | c8820@c8931,c8820@c8946 | Implicit-register fragment (params EBX = source point, ESI = output vector, EDI = screen destination): ESI = EBX - camer |
| `0xc89f4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | c8820@c8936,c8820@c8941 | As 0xc89cc but ESI = EBX + camera position. |
| `0xfa2e4` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | fa02c@fa14e | Implicit-register fragment (ESI = data pointer, EDX = byte count; params in that order): the two-byte running checksum ( |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc4654` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | none | b69c@b836,b69c@b8c5,b69c@b961 | Stack (ret 0xc): store the three arguments in dwords 0x1590b0 (first), 0x1590ac (second), 0x1590a8 (third) and run 0xe7a |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc473c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | EAX:uint32_t | 15be10@15be2e,160122@160147,160156@16017b | Implicit-register fragment (EAX = MARGIN word, ESI = x, EDI = y, EBP = z; params in that order): run the view-volume tes |
| `0x10e08d` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | 10dfec@10e017,10dfec@10e039,10dfec@10e05b | Implicit-register fragment (ESI = source words, EDI = destination, ECX = count; params in that order): split each source |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x26670` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 38284@3852f | Set a target position (X = EDX, Y = EBX, Z = ECX) on the scene object INDEX (EAX, signed word, looked up with 0x8ffe8):  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcc6c0` | [ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:uint32_t,[ESP+24]:uint32_t | EAX:uint32_t | e764@e76a,3c790@3c7c9,3cb2c@3cb68 | cdecl, variadic (SIZE, FMT, ...): allocate SIZE bytes (0xc3f70); when that fails report through the pointer at 0x132a74  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x112808` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 114058@114081 | cdecl (OBJ): flush the 32 pending slot dwords at OBJ + 0x558 (one per 4 bytes of OBJ): every slot not equal to -1 is pos |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf4640` | none | none | none direct | Read the next significant script line into the buffer at 0x4859e0: leading spaces/tabs are skipped, characters are store |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd5f0c` | EAX:uint32_t | none | 98000@9800e | Load a list of resource records (13 bytes each: name pointer dword +0, size dword +5, data pointer dword +9; ended by a  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x955c0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 95660@956f3 | Camera set-up (EAX, EDX, EBX, ECX and one stack argument, ret 4; named A, B, C, D, S): rebuild the rotation record at 0x |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc8f38` | none | none | ac408@ac503 | Load the two fixed data files: the path 0x18ef60 is loaded with the hook at 0x132ab8 into 0x425c8c (failure reported thr |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x41314` | EAX:uint32_t | none | a6fc@a718,a6fc@a73c,a6fc@a75e | Show the picture NAME (EAX) in the preview widget: when the name differs from the current one at 0x1a49d8 (0xa4380) or n |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe28e8` | none | none | 8b39c@8b4c4 | One-time timer service start-up (guard byte 0x15f751): init the module (0xd6dc4), lock the handler code range (0xe28c4,  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x49fd4` | EAX:uint32_t | none | 49228@49799 | For every member I (byte +0x12 of the group record P = EAX) look up the object index in the row table at 0x1fcb40 (rows  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x4e5bc` | none | none | 4dcd8@4dd6d | Insert an interpolated entry into the 24-slot waypoint arrays when byte 0x1a568a is 0x11 and the entry kind at 0x1a5673[ |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcef3c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | 26960@26b37,87a20@87c1f,8de9c@8df70 | Advance the 3-dword position V (EAX) by SCALE (EBX) along the direction given by the two angle words at ANG (EDX; a = +0 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xffe78` | none | none | none direct | Draw one texture-displaced span: from x = high word of dword 0x485fe8 to x = high word of dword 0x485fd0 (clipped to dwo |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd2500` | EAX:uint32_t,EDX:uint32_t | none | c4d43@c4da7,c8c78@c8cab | Implicit-register fragment (ESI = list of object word offsets, ECX = count; params in that order): fill the render objec |
| `0xd2704` | EAX:uint32_t,EDX:uint32_t | none | c4aeb@c4b89 | Implicit-register fragment (ESI = list of object word offsets, ECX = count; params in that order): as 0xd2500 but with t |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd2c84` | EAX:uint32_t,EDX:uint32_t | none | none direct | Implicit-register fragment (ESI = list of object word offsets, ECX = count; params in that order): build the render list |
| `0xd2d7c` | EAX:uint32_t,EDX:uint32_t | none | none direct | As 0xd2c84 but the handler value is the dword 0x15b884 when dword 0x158f0c is set, else 0x15b888. |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x91c98` | none | none | 3dd30@3e802 | Advance the camera-preset queue: when the read index (word 0x2144ae) has not caught up with the write index (word 0x2144 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x47764` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t | none | 73ec@7492,2c600@2c6cc,46254@463cf | Queue a message in the 9-slot table at 0x12668e (0x14-byte slots: word +2 = SI id, +4 = DI, dwords +0xa/+0xe/+0x12 = EBX |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x348e4` | none | none | 349ec@349fc | Refresh the 8-row list widget (when byte 0x13578c is set): walk the two parallel widget chains (found under 0xa46cc with |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x440f4` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t | none | 433fc@43560,4357c@436c3,436dc@43819 | Draw a sprite from the table selected by SEL (EAX low byte; EDX, EBX, ECX, then the two stack arguments S0, S1, ret 8; t |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x8b6e0` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | EAX:uint32_t | 8b7e8@8b84d,8b7e8@8b90b | Fetch the next token of a word list into OUT (ECX): with MODE (EAX, signed word) 1 the list file is (re)loaded first (de |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa60a8` | EAX:uint32_t | none | a61b0@a61e2 | Redraw the widget OBJ (EAX): the mode byte comes from 0xe588c (0 when flag 2 of byte +0x3b is set); mode 1 saves the rec |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x5de14` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | 5d1c4@5d2d4,5d1c4@5d430 | Draw a small direction marker: with SIZE (EBX, signed word) the two points (0, 2 * SIZE) and (0, SIZE * 4 / 3) are rotat |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x34298` | none | none | none direct | Update the item-list dialog: without a name match (0xa4380 of 0x125937 against 0x182b34) just activate and redraw the wi |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc3364` | EAX:uint32_t,EDX:uint32_t | none | c3430@c343d | Find the size of the data file NAME (EDX) trying several locations; BUF (EAX) is the work buffer and the result is store |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc768` | EAX:uint32_t | none | bfcc@c264,c868@c88a,7bc00@7bc1d | Lay out the four numbered slot widgets of the dialog 0x180728 for the selection INDEX (EAX, signed word): the widget 0x1 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe4010` | EAX:uint32_t | none | dc094@dc111,e4188@e4255 | Load the text file NAME (EAX) into a local 0x400-byte buffer (default path + 0x190ad4 + name through the hooks 0x132ab4  |
| `0x4f7d0` | none | none | 4b6a0@4ba2c,4ca90@4cada,4f3e0@4f486 | Fill the 16 button widgets "<n>" (format 0x184a9c) of the dialog 0x184a8c: the first byte 0x1a5531 buttons get the label |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xab174` | EAX:uint32_t | none | ab28c@ab312 | Read a (group, item, kind) selector from the stream HANDLE (EAX): word A, word B and byte C (0xa79d4). A kind above 2 or |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xddae0` | none | none | 27d18@27ef4,dd8d0@dd8d5,f3480@f348d | Poll the mouse: add the motion (0xce02c) to the pointer words 0x425fc8 (x) and 0x425fc6 (y), clamp x to the dword range  |
| `0xea50c` | [ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t,[ESP+16]:uint32_t,[ESP+20]:uint32_t | none | 8b39c@8b469 | cdecl, variadic (FMT, ...): increment the nesting counter byte 0x160638; only when it becomes 1 format the message; when |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x4f8a8` | none | none | 4ce4c@4cfd4 | Refresh the button dialog 0x184ae4: label the widgets 0x184e4c and 0x184e54 with the numbers of bytes 0x1a5530 and 0x1a5 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf41dc` | EAX:uint32_t | none | a9bec@a9bf5 | Show the splash picture NAME (EAX): clear byte 0x161e20, set byte 0x161e22, remember the request (0xf41a8), size the fil |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xabe44` | EAX:ef_addr32 | none | 4738@47fa,5630@5652,b1e0@b2eb | Split the string at STR (EAX) into whitespace-separated tokens (character class table 0x195af0 indexed by c+1, flag 2 =  |
| `0xeff04` | EAX:ef_addr32 | EAX:uint32_t | ee22c@eed53 | Convert the 80-bit extended value at P (EAX) to a truncated int32: exponents below 0.5 give 0, magnitudes of 2^32 and ab |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xefb16` | EAX:ef_addr32,EDX:ef_addr32 | EAX:uint32_t | ee22c@ee5d9,ee22c@ee5f5,ee22c@ee7a7 | Compare the 80-bit extended values at A (EAX) and B (EDX): 2 if either is a NaN, otherwise -1, 0 or 1 for A below, equal |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xceda0` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | none | 95020@95093 | Move the 3-dword vector at VEC (EAX) along the direction given by the two angle words at ANG (EDX) by SCALE (EBX, low 16 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x326d8` | none | none | 2fbc4@2ff34,3122c@31363 | Build the label text at 0x19e4da: copy a short constant chosen by byte 0x19e555 (0..7, else a default; 5 to 8 bytes each |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x84e3c` | EAX:ef_addr32,EDX:uint32_t,EBX:uint32_t | none | 601c@621b,66fc@677e,67d4@685f | Fetch two packed words for sub-entry SUB (BL) of the class record owned by unit UNIT (DX, signed) into OUT[0..1] (EAX):  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9f850` | EAX:uint32_t | none | 9e7bc@9e817 | Queue the 29-byte record INDEX (AX, signed; table at [0x417174] + 0xb) as a table entry: run the hook at 0x132af4 with ( |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa348c` | EAX:uint32_t | none | a410c@a3591 | Handle the event record INDEX (EAX; low 16 bits index the 32-byte records at 0x135878) by the type in the low nibble of  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x7a194` | none | EAX:uint32_t | 7a290@7a290 | Full-screen choice prompt: clip to the screen (0xce328, 0xcdf48), draw the page chosen by byte 0x125846 (1: 0x12aeb8, 2: |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xde3b4` | EAX:uint32_t | none | 982d4@982fa,a9cd0@a9d03,f41c4@f41d5 | Send the queued mouse request blocks starting at index START (EAX, low 16 bits signed; dword table at 0x4260d0-0x100): w |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x38be8` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t | none | 22f28@2300f | Draw the framed text panel for slot ID (AX, signed), vertical position Y (EDX) and X (EBX): fetch the panel image of the |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x107310` | none | none | dbff8@dc07f,e3ffc@e4009 | Load the 0x4000-byte colour table from the file named at 0x17d368: on first use allocate 0x10100 bytes (dword 0x4dd860,  |
| `0x114ad8` | none | none | none direct | Release state when dword 0x17e25c is set: unregister the range 0x111c90..0x114ad8 (0x1048b4), unlock the data blocks 0x1 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xa4b80` | EAX:uint32_t | none | 4a90@4a9b,4b54@4b5f,94fc@978a | Close the widget ID (AX; valid 1..word 0x41eed8 in the pointer table 0x41edfc, only when flag 4 of its byte +0x3b is set |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x7bc0` | EAX:uint32_t,EDX:uint32_t | EAX:uint32_t | 7e44@7e65,7104c@71089,7156c@719ce | Pick a roster slot for the unit UNIT (AX, signed) in group GROUP (DL): keep the unit's own slot (byte +0x20b) when it st |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9b668` | none | none | b1e0@b3d7 | Rebuild the per-side tallies: clear the two 0x18-byte tables at 0x40ce70 and 0x40cf78, then for every node of the list a |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb49b4` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t,ECX:ef_addr32 | EAX:uint32_t | b47e4@b48f1 | Search two linked lists for a type-0x31 node of side GROUP (BL, 0..5) that is negative (0xcc1ac) in the list LIST (ECX), |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9b780` | none | none | 9a624@9a6e9 | Remove the contribution of each active entry to the side tallies: for every 0x36-byte entry of the table at [0x1379bc] ( |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xbe208` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:ef_addr32,[ESP+4]:ef_addr32 | none | bd92c@bdb85 | Assign free nodes to the request record REC (ECX; node count in byte +3, kind byte +8): walk the node list whose head po |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xcaea0` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | none | b0f5c@b132a | Refresh the cached 18-byte blocks of a node list from a table of COUNT (EBX, signed 16 bits) 0x1b-byte entries at TABLE  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x6563c` | none | none | 654b8@65564 | Apply the pending 8-byte notices at 0x1a6d2e (count word 0x1a81b2, only while byte 0x1a81e5 is set): look up each node ( |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x110a08` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t | none | none direct | Decode a column-compressed sprite from SRC (EAX, word stream) into DST (EDX) with row stride STRIDE (BX, signed): the fi |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf3904` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t | none | f3aa8@f3af0 | Draw a box outline from the signed 16-bit coordinates A0 (EAX), A1 (EDX), A2 (EBX) and A3 (ECX): after saving the screen |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfe380` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | fe380@fe3bd,fe380@fe3d6,fe49c@fe52d | Draw the arc from angle LO (ECX) to angle HI (stack, ret 8) of radius RADIUS (stack) around (CX = EBX added to the cosin |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x4cff4` | none | none | 4c78c@4c8e5,4c900@4ca6d | Fill in the labels of the dialog 0x184a58. Without a current entry (dword 0x1a54f6 zero) set the first label (0x184cf8)  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfe49c` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t | none | c9680@c9729,c98f8@c999c | Draw an arc from angle LO (ECX) to angle HI (stack, ret 8; both reduced to 16 bits, HI moved a full turn up when it is n |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x654b8` | none | none | 601c@6082 | Per-frame update of the notice list headed by dword 0x1a7c54 (link at +0x16): clear the notice count word 0x1a81b2, then |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd550` | none | none | none direct | Refresh the two status widgets (0x180850, 0x180728) while word 0x1f71d2 is set and byte 0x126576 is clear: refresh the f |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x11f368` | EAX:int32_t,EDX:int32_t | EAX:int32_t | 118770@118830 | Fixed-point arctangent of A (EAX) over B (EDX), angle units of 0x10000 per turn: 0 for (0, 0); with /A/ < /B/ the table  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xebb58` | EAX:uint32_t,EDX:ef_addr32 | EAX:uint32_t | 9ccb8@9ccd8,9ccb8@9cd03 | Start streaming KEY (EAX) into the stream object OBJ (EDX): look the key's record up (0xeb520) and seek it (0xeb698); fo |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x9338` | EAX:uint32_t | none | 9ea0@a0d7 | Choose the mode byte 0x12583b for the kind KIND (AX, signed) and store it in the keyed cell named 0x180548 (0xa7ec4): th |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xd72b4` | EAX:uint32_t,EDX:ef_addr32,EBX:uint32_t | EAX:uint32_t | 815f4@81602 | Start the sample OBJ (EDX; data pointer at +0x1c) of SIZE bytes (EBX) on voice IDX (AL): nothing happens (returns 0) unl |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xfc0ec` | EAX:uint32_t,EDX:uint32_t | EAX:ef_addr32 | bf444@bf5d4,bf81c@bf8f4,bfa58@bfab1 | Allocate the slot-set record for the pair (A = AL, B = DL): a 10-byte header (a at +0, b at +1, the slot count from 0xfc |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xe17f8` | none | none | ac0e0@ac119 | Rebuild the 0x800-byte entry table at 0x4198c8 for the page in byte 0x15f714: initialise every 8-byte entry from index 2 |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x90940` | EAX:ef_addr32 | EAX:uint32_t | 8de9c@8dffb | Find the id of the first object near POS (EAX; three dwords): scan the 0x45-byte records (count word 0x1a1506, table at  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x76e2c` | EAX:uint32_t,EDX:uint32_t | none | 46454@4707f | Refresh the roster of unit B (DX, signed) for class A (AX, signed): look the unit up (0x7489c) and take its roster table |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xac854` | none | none | acbe4@acc07,e1958@e19a7,e1958@e1cab | Poll the pointer device: when byte 0x41f2e1 is set read the motion counters and the button bits (copied to 0x41f2f1, wit |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x5af50` | EAX:uint32_t | none | 5a8d0@5a8ef | Draw the closed outline number N (AL, looked up in the pointer table 0x1281a4; nothing is drawn for N = 0) as 24 line se |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xb4aec` | EAX:ef_addr32,EDX:ef_addr32,EBX:uint32_t,ECX:ef_addr32 | EAX:ef_addr32 | 94fc@9548,b0f5c@b1120,b0f5c@b1430 | Sibling of 0xb49b4 returning a node: for group GROUP (BL, 0..5) walk the list headed by *A (groups 0, 2, 5, links +4) or |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xad440` | EAX:ef_addr32,EDX:ef_addr32 | none | 49228@49390,b0f5c@b1011,b0f5c@b120c | Open the resource NAME (EAX) from the default directory plus the subdirectory 0x18dc8c: build the path (0x9c980, strcat  |
| `0xa1d50` | none | EAX:uint32_t | a1d00@a1d3d | Write the current page record (pointer 0x417174, entry table 0x41717c) to the save stream with 0xa1b28: first the 13-byt |
| `0xa1d00` | EAX:uint32_t | none | 7a9bc@7aa06 | Select the record ID (AX, signed) as the current page: store the id in word 0x41722a, look the record up (0xb3f58), set  |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xf118d` | EAX:ef_addr32 | EAX:uint32_t | a4574@a458e,10e872@10e8e9 | mktime: convert the broken-down time at TM (EAX; sec, min, hour, mday, mon, year since 1900 at +0..+0x14, isdst at +0x20 |
| `0xa4574` | EAX:ef_addr32 | EAX:uint32_t | 4050@4056,bfcc@c0e4,b33ac@b33ae | Get the current time as seconds (0xf118d over the broken-down time from 0xf10ca, whose first field is bumped when the fr |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x2a8ac` | EAX:uint32_t,EDX:uint32_t,EBX:uint32_t,ECX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:ef_addr32 | none | 2a4c8@2a707 | Take a node from the free list at dword 0x19bb40 (refilled by recycling the oldest, 0x2a934) and queue it at the tail of |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0xc868` | none | none | a5bc@a5d1,c838@c85f | Rebuild the current page: clear the word tables (0x92f0), lay out the slots up to the count in word 0x121e98 (0xc768), d |

## misc_drafts1 (2026-11-03)

| Entry | Inputs | Outputs | Callers | Observed effect |
| --- | --- | --- | --- | --- |
| `0x56890` | EAX:ef_addr32 | none | 581b0@58395 | Apply the order record REC (EAX; unit id word +1) to its unit (0x8ffe8) unless the unit's word +0x18 equals byte 0x1a666 |
