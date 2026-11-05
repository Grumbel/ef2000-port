<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 089: GUI entry aliases and mission controls

This batch accounts for **63 distinct original, named function entries**
from the German EF2000 X-32 binary. These fall into two very different
categories:

- **48 five-byte GUI `JMP rel32` thunks** with a verified destination each;
  they contain no gameplay algorithm and are classified as `kind=thunk`.
- **Two 15-byte Quit Yes/No wrappers** with two original guest call/jump
  targets each, plus **13 actual callback/state bodies** spanning 8–99
  bytes. These carry state updates, branching and/or data parsing.

This is a *large inventory and host-reconstruction batch*, **not 63
completed native replacements**. The tracker moves these 63 from `named`
to `drafted`, for a total of 190 host drafts, 3,257 existing `compiles`
and 4,287 inventoried function entries. Nothing new is `spliced` or
`verified`, and the Open Watcom count is unchanged. The amount of newly
understood gameplay code is closer to the 13 substantive bodies than to
the headline entry count.

## Dispatch jump table

The 48 tracked jump aliases live at `0x84afc` and `0x84b04..0x84c7c`.
Their original names, exact entry points and exact destinations appear in
the declarative `EF_UI_JUMPS_089` mapping in
`decomp/include/ef_ui_thunks_089.h`. The host models in
`decomp/src/gui_jump_thunks_089.c` forward EAX, EDX, EBX and ECX without
local mutation, using an injected guest-tailcall adapter.

Each original opcode at these 48 entries is `E9` plus a signed rel32
displacement, proven by an original-payload audit that calculates the
target from the bytes. The surrounding machine code is omitted from this
repository; the audit reads it only when `EF2000_PAYLOAD` is set to the
raw payload of SHA-256
`3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`.

One adjacent opcode at `0x84c4c` also looks like a relative jump to
`0x9bcf8`, but it is **not in the authoritative function inventory**.
It has not been counted or given a made-up name; a future caller audit
should decide whether to add it as another function entry.

The two nearby 15-byte original entry points are not pure thunks:

| Entry | Original name | Guest sequence |
| --- | --- | --- |
| `84ac4` | `QuitEF2000Yes` | EAX=`189b78`, CALL `a4d90`, JMP `ac9a0` |
| `84aec` | `QuitEF2000No` | CALL `830b8`, EAX=`189b5c`, JMP `ac7c8` |

## Thirteen substantive original entry points

The remaining C models live in `decomp/src/gui_callbacks_089.c`:

| Original entry | Game symbol | Recovered control and state behavior |
| --- | --- | --- |
| `4b54` | `kotsInitKOTS` | Select clicked child's ID, check KOTS state, jump to one of two original GUI paths |
| `4b1a4` | `cgWarningOkClick` | Network guard; compare signed fuel against minimum; error or screen transition |
| `50638` | `mpDrawFuelBar` | Two guest calls with a specific menu widget argument |
| `5089c` | `mpType2Click` | Parse an encoded widget digit into mission type group 3, check legality, update or warn |
| `508fc` | `mpType3Click` | Same original parser, group 4 |
| `5095c` | `mpType4Click` | Same original parser, group 1 |
| `4d9cc` | `mpPrevAssign` | Increment and wrap index, scanning for category 3/4; redraw |
| `4da2c` | `mpMakeAssign` | Decrement index, wrap from `0xfe`/`0xff`, scan category 3/4; redraw |
| `4da90` | `mpViewAssign` | Save selected aircraft assignment into parallel arrays, call previous-assignment and redraw again |
| `84ddc` | `gpwshowsummaryinfo` | Extract low-byte child ID and jump to selector |
| `da2b0` | `clouds` | Set cloud enable byte `425c88=1` |
| `37260` | `onopenCredits` | Choose among three guest tails depending on state at `125846`, network flag and signed high word of `19769e` |
| `37254` | `gnNewSocket` | Set `125846=1`, clear EAX, and **fall through** into `onopenCredits` (no RET) |

The repeated mission-type callback bodies use a very specific parser:
they initially decode ASCII at widget+`42` and only use widget+`43` if
the first digit is `1` and the second is ASCII `0..9`. The resulting
second-digit value is ASCII code minus `0x26`; a generic `atoi` would
not preserve it.

The assignment scans intentionally **have no bound on iterations**. If
no item of type 3 or 4 is present they do not terminate in the original
code, so our drafts retain that behavior. Host tests use valid tables.

For `gnNewSocket`, the original 12-byte entry flows directly into code
starting at `37260`. Those are separate *entry addresses* in Ghidra but
not two independently returning algorithm bodies; that distinction is
recorded rather than obscured.

## Tests and limits

- `tests/decomp_ui_thunks_089_test.c` exercises all 48 original jump
  entries with nontrivial register tuples and both Quit Yes/No call pairs.
- `tests/decomp_gui_callbacks_089_test.c` exercises every substantive
  body with synthetic guest RAM and injected guest-call return values,
  including both mission category directions, edge/wrap states and
  conditional UI flows.
- `tests/test_decomp_batch_089_payload.py` verifies 48 original signed
  jump targets byte by byte, the two Quit CALL/JMP sequences, thirteen
  additional disassembly patterns and exact correspondence to the
  authoritative tracker. It skips the payload-dependent parts when the
  original executable is absent.
- Both GCC test programs are wired into `nix flake check` host checks.
  No original assets are committed.

### Still missing

The host adapters do not validate original Watcom register clobbers,
non-returning jumps, indirect callback effects, the complete x87 state or
guest-visible stack state. Unresolved guest calls such as `0x50a1c`,
`0x4db60` and jumps into code *inside* other original bodies still require
native guest tracing, Watcom compilation, patching and differential replay.
For the mission selection callbacks, EDX preservation across guest calls
is currently an explicit unverified assumption.

Future large batches should prioritize clusters with a common verified
runtime ABI or repeated instruction template, not just raw entry counts.
