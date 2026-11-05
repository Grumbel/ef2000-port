<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 103: seventeen guest-differential C routines and an ABI correction

Batch 103 tests *existing recovered C* from three modules against the original
native instructions in an isolated Unicorn x86 guest. Unlike a host-only unit
test, both the original and the separately compiled C run at their original
entry addresses and read/write the same addresses in a freshly initialized
guest CPU. **These are diagnostic GCC builds, not OpenWatcom replacements**.

No proprietary game bytes, patched executable, or derived assembly is bundled.

## Original-vs-C evidence

| Recovered function | Guest entry | Cases | Comparison |
| --- | --- | ---: | --- |
| `reference_clock_reaches` | `0x40820` | 96 | AL, preserved regs, input/clock memory |
| `command_code_is_group_1b` | `0x41da4` | 256 | Full EAX and registers; all AL input values |
| `clear_tactical_accumulators` | `0x49054` | 40 | Seven zeroed fields and guards; preserved regs |
| `reset_tactical_slots` | `0x4d8f4` | 40 | Sixteen slots, mode gate, guards; preserved regs |
| `advance_first_tactical_status` | `0x4dc98` | 80 | First matching slot and change latch; preserved regs |
| `count_external_grid_markers` | `0x544c0` | 40 | Packed 27-byte records/count/guards; preserved regs |
| `sample_active_coordinates` | `0x5f1ec` | 80 | Null and live source, overflow, destination/guards, preserved regs |
| `read_event_slot_byte` | `0x14d3c` | 64 | Signed positive/negative indices, AL, preserved regs |
| `reset_dialog_latches` | `0x151a0` | 40 | Three latch bytes, copied source, guards; preserved regs |
| `code_is_10_to_15` | `0xcc5fc` | 256 | Full EAX and preserved regs; all codes |
| `code_is_18_to_1b_1d_1e` | `0xcc628` | 256 | Full EAX and preserved regs; all codes |
| `code_is_18_or_1c_to_1f` | `0xcc654` | 256 | Full EAX and preserved regs; all codes |
| `code_is_20_or_21` | `0xcc67c` | 256 | Full EAX and preserved regs; all codes |
| `code_is_23_or_24` | `0xcc694` | 256 | Full EAX and preserved regs; all codes |
| `code_is_22` | `0xcc6ac` | 256 | Full EAX and preserved regs; all codes |
| `pair_matches_cc7e8` | `0xcc7e8` | 4096 | Full EAX/regs; all codes × 16 selected kinds |
| `pair_matches_cc81c` | `0xcc81c` | 4096 | Full EAX, preserved regs, *original* EDX clobber model |
| **Total** | | **10,464** | **10,464 passed** |

The two opt-in differential harnesses are:

- `tests/guest_runtime_predicates_diff.cpp` (736 executions);
- `tests/guest_code_predicates_diff.cpp` (9,728 executions).

Both compare original versus patched code in separate freshly initialized
Unicorn engines. They reject writes outside the selected expected data spans
and stack, compare all explicitly preserved integer registers and return PC,
and reject a candidate image with changes outside the named function extents
or with a missing replacement. They do not compare EFLAGS, ordering of writes,
or all unused scratch-register values. Passing here says nothing about the
function's actual callers, external jumps into padded NOPs, or a whole-game
replay.

## ABI finding: 0xcc81c clobbers EDX

The earlier signature for `pair_matches_cc81c` incorrectly claimed EDX was
preserved. Original instructions at `0xcc81c` start with `AND EDX,0xff`.
For DL=8 or DL=0x1a, the original then zeroes EDX and moves the incoming AL
to DL before testing it against 2. For other DL values, EDX retains the
zero-extended original DL. It **never restores** EDX. The signature registry
now marks EDX scratch, and the differential harness asserts the exact
original EDX value in all 4,096 test cases. GCC with `-fcall-saved-edx`
leaves EDX unchanged instead; those 4,096 scratch differences are reported
explicitly. Thus the independently compiled code is **not register-for-
register interchangeable** even though its declared return and preserved
register contract passes. An adapter or different compilation strategy is
needed if any real caller relies on the original scratch value.

Other non-contract differences are measured rather than hidden:

- `reference_clock_reaches` writes only AL in the original; GCC zero-extends
  to EAX. All 96 cases preserve the correct AL but change upper EAX; this is
  a genuine barrier to treating the diagnostic splice as a general binary
  drop-in without caller liveness analysis or a small assembly adapter.
- `clear_tactical_accumulators`, `reset_tactical_slots`,
  `count_external_grid_markers`, and `reset_dialog_latches` also have
  diagnostic scratch EAX differences. Their signatures declare no return
  value, so all tested memory output and preserved registers still agree.

The recovered source bodies are unchanged in this batch; their existing
host-side models remain independently available.

## Reproduction (private input, ignored outputs)

Requires a privately extracted `payload.bin` of **1,662,896 bytes**, matching
SHA-256 `3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`.
`tools/elf_inplace.py` makes SHA-bound single-function `EFPATCH1` plans and
requires the reviewed address and explicit `--allow-nop-pad`; no modified
payload is committed. Set `EF2000_PAYLOAD` to the private path and ensure
`local/build/` exists.

```sh
gcc -m32 -std=c99 -Os -Wall -Wextra -Werror \
  -mregparm=2 -fcall-saved-ecx -fcall-saved-edx \
  -ffunction-sections -fno-pie -fno-pic -ffreestanding \
  -I decomp/include -c decomp/src/runtime_predicates.c \
  -o local/build/runtime_predicates-gcc32.o

gcc -m32 -std=c99 -Os -Wall -Wextra -Werror \
  -mregparm=2 -fcall-saved-ecx -fcall-saved-edx \
  -ffunction-sections -fno-pie -fno-pic -ffreestanding \
  -I decomp/include -c decomp/src/low_helpers.c \
  -o local/build/low_helpers-gcc32.o

gcc -m32 -std=c99 -Os -Wall -Wextra -Werror \
  -mregparm=2 -fcall-saved-ecx -fcall-saved-edx \
  -ffunction-sections -fno-pie -fno-pic -ffreestanding \
  -I decomp/include -c decomp/src/code_predicates.c \
  -o local/build/code_predicates-gcc32.o
```

For each selected function, run the diagnostic converter, e.g.:

```sh
python3 tools/elf_inplace.py local/build/code_predicates-gcc32.o \
  --payload "$EF2000_PAYLOAD" --reviewed-flow 0xcc5fc \
  --function code_is_10_to_15 --allow-nop-pad \
  -o local/build/code_is_10_to_15.efpatch
```

The other entry/size mappings are in the table above. Apply the nine
`runtime_predicates`/`low_helpers` patches as one group and the eight
`code_predicates` patches as another, each against the original unmodified
payload. The API `tools.efpatch.apply_patches(image, patch_paths)` accepts
multiple `EFPATCH1` plans, checking their base SHA and overlapping writes;
write each combined result under `local/build/`. Run the C++ differential
programs against original and their corresponding combined images:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  tests/guest_runtime_predicates_diff.cpp \
  $(pkg-config --cflags --libs unicorn) \
  -o local/build/guest_runtime_predicates_diff

c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  tests/guest_code_predicates_diff.cpp \
  $(pkg-config --cflags --libs unicorn) \
  -o local/build/guest_code_predicates_diff

local/build/guest_runtime_predicates_diff "$EF2000_PAYLOAD" \
  local/build/runtime_predicates-gcc32-Os-patched.bin
local/build/guest_code_predicates_diff "$EF2000_PAYLOAD" \
  local/build/code_predicates-gcc32-Os-patched.bin
```

For a static Unicorn archive, replace `pkg-config` with its include/lib paths
plus `-lpthread -lm -latomic` as required. The local test environment used a
supplied static Unicorn 2.1.4 build with those options. The diagnostic ELF
parser now permits up to 256 checked section headers (formerly 64), because
the unchanged `low_helpers.c` compilation contains 73 sections. A regression
test generates a real 70-extra-function GCC object and selects one function;
malformed or excessive section headers remain rejected.

## Remaining work and completion criterion

The selected C bodies are semantically supported by significant isolated
guest evidence, but **no new function meets the project's `verified` state**:
the OpenWatcom-generated OMF bytes, ABI-correct in-place splice, whole-game
startup, patched replay and differential golden trace are still outstanding.
This environment has no `nix`/`wcc386`, and the supplied static Unicorn has
crashed on whole-game startup in guest x87 execution. Neither fact was worked
around by claiming a full replay from isolated tests. The next meaningful
step is testing these specific candidates in the native Windows/Nix toolchain
with original callers, then extending the same differential methodology to
more complex state-transition routines.
