<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 104: executable startup coverage and deterministic C-splice replay

This batch turns isolated GCC32 guest comparisons into a **repeatable,
whole-address-space comparison of real EF2000 startup**. It uses the original
privately supplied DOS executable and SHA-bound diagnostic `EFPATCH1` plans;
no original bytes, executable, or prepatched binary enter Git or the bundle.

**Scope: pre-CD startup only.** This environment has no game CD/data files.
The original program exits with a missing-dataset message after about 43
guest ticks. Neither the game's menu/cockpit, later code, nor a complete
mission has been covered. These are GCC32 diagnostic substitutions, not
verified OpenWatcom/OMF production replacements. **Zero functions gain the
`verified` tracker state.**

## A Unicorn linker defect was blocking real execution

The local **static Unicorn 2.1.4** library crashed when an x87 `FLD` read
guest qword memory. In the linked host executable, GNU ld's `GOTPCRELX`
relaxation turned a QEMU-generated indirect helper call into a recursive
`addr32 call` of that helper. The ensuing stack overflow was a host linker
artifact, not a defect in EF2000, the guest ABI, or the recovered x87 C.
Passing **`-Wl,--no-relax`** when linking static Unicorn prevents the invalid
rewrite. The CMake guard covers every Unicorn-linked target on Linux, and
the Nix recipe uses the same option. CMake also links the static library's
`atomic`, `pthread`, and `m` dependencies when needed.

`tests/unicorn_x87_smoke.cpp` exercises five qword `FLD`/`FSTP` guest memory
round trips. All pass with the link guard. The patched runner then completes
the formerly crashing 10-million-instruction benchmark. CMake's test suite
includes this smoke test and a separate deterministic DOS clock test.

This fix is specifically verified for the supplied **static** library and
the local GNU linker; it is not a blanket claim about every Unicorn build.

## Which recovered functions actually execute on startup?

`tools/scan_startup_entries.py` takes all tracked nonempty function entries
inside the original **1,662,896-byte** initialized payload. It instruments
at most 512 entries per runner invocation with an *identity* patch: each
address is written back with its own original byte. The runner reports
per-entry hit counts. It runs with bounded instruction timing, sound off,
and deterministic DOS clock. This records entry visits, **not** proof of
correct function boundaries, unique call sites, branch coverage, or all
executable code paths. The complete entry/call-count metadata is in
`startup-coverage-104.json`.

- Entries surveyed: **4,287**.
- Entries reached: **121** (about **2.8%** of the registry).
- Dynamic entry hits: **43,161** in 20 million emulated guest instructions.
- Most frequent tracked entries: `truncate_9d72c` (20,480),
  `sin_b45ac` (10,240), `reduce_angle_b45b6` (10,240), and
  `step_generator_a45b3` (1,682).

These counts tell us where to spend decompilation effort next. The low
coverage fraction reflects the missing game data and short startup, not a
claim that the rest of the executable is dead code.

## Recovered C and original-guest ABI checks

| Original entry | Size | Actual startup calls | New evidence | Current limitation |
| --- | ---: | ---: | --- | --- |
| `encode_runtime_segment_offset`, `0x604` | 36 | 1 | 1,024/1,024 original-versus-C guest executions agree on all general registers, flags, return, source memory and write-watch | GCC32 candidate, 26 code bytes + explicit diagnostic NOP padding |
| `to_lower_ascii`, `0xe7835` | 14 | 12 | 512/512 isolated guest input/ABI cases; exact-size GCC32 body | Diagnostic GCC, not Watcom |
| `copy_string_9d749`, `0x9d749` | 31 | 11 | 512/512 isolated guest string/return/write-watch cases | **264/512 scratch EDX differences** versus original |
| `step_generator_a45b3`, `0xa45b3` | 49 | 1,682 | 512/512 isolated original-guest model cases, including zero reseed | GCC32 candidate is 65 bytes; **not spliced** |

`encode_runtime_segment_offset` now reads the intentionally misaligned
guest dword with `__builtin_memcpy` on little-endian GCC, which folds into
a single unaligned load; other toolchains retain the existing byte-wise
reader. The original returns a 16-bit segment in the upper half and a
4-bit offset in the lower half.

`copy_string_9d749` returns its original destination in **EAX**. Its old
source declaration incorrectly said `void`, and the signature incorrectly
claimed EDX was preserved. The original uses word-wise string processing,
advancing scratch EDX differently from the scalar GCC replacement; the
declared return and preserved registers match, but a caller that reads EDX
after return could diverge. The **full startup replay** below includes 11
calls and reaches the same final state; this does **not** establish EDX
liveness for every other caller or input. The copy helper remains unverified.

`step_generator_a45b3` also **clobbers DX**; AX receives the new high seed
word and DX the OR of both updated words, preserving their high 16 bits.
The tracker now records this actual ABI instead of claiming EDX preservation.
Because the GCC object overflows its 49-byte original extent, we deliberately
did not include it among the live C replacements.

## Making repeated startup comparable

Instruction-based timer ticks were already deterministic, but DOS
`INT 21h AH=2Ah` (date) and `AH=2Ch` (time) still called the *host wall
clock*. Between otherwise identical replays, the game consequently stored
a different startup clock-baseline dword at guest address **`0x4dd984`**.
That was the only discrepancy seen in the full 64 MiB memory image.

`runtime/dos_clock.hpp` provides the option `--dos-clock guest`, fixing the
calendar origin to **2000-01-01 12:00 UTC** and advancing by the emulated
PIT count/divisor. `--dos-clock host` remains the default for interactive
play. The option changes only the DOS date/time services; deterministic
replays still require `--timing instruction` and controlled external inputs.

`tools/compare_guest_replay.py` validates private patch fingerprints and
overlap, excludes **only the exact changed code bytes** from the 64 MiB
image, partitions the remaining bytes into at most 32 runner-supported
SHA-256 ranges of at most 16 MiB each, and rejects:

- nondeterminism among the original baseline runs;
- changed termination status, instruction/timer/interrupt statistics, or
  any compared guest-memory SHA-256;
- any replacement entry never reached, or inconsistent hit totals;
- executable failures, patch mismatches and missing hash spans.

### Measured result

With the three compiled C replacements at `0x604`, `0xe7835`, and
`0x9d749` **simultaneously**:

| Metric | Observation |
| --- | --- |
| Original unmodified replays | **3 identical** |
| Replays with all three C patches | **2 identical to originals** |
| Compared guest memory | **67,108,783 / 67,108,864 bytes** |
| Excluded | **81 bytes** of the three replaced code bodies, nothing else |
| Hash spans | 7 SHA-256 ranges covering all remaining guest RAM |
| C function entries actually reached | 24 calls total: 1 + 12 + 11 |
| Termination | `guest_exit`, interrupts 56, PIT ticks 43, IRQ0 0 |

This is much stronger than testing C functions in isolation: the patched
startup actually returns from those functions, keeps running through its
normal early-exit sequence, and ends with identical **compared guest RAM and
reported exit/timer/interrupt statistics**. The runner does not yet hash all
general registers or x87 state at termination. **Equal final memory does not
prove identical transient states, outputs, flags at every intermediate
instruction, or full-game equivalence**. In particular, the diagnostic copy
helper's different scratch EDX remains a known concern.

### Reproduction (private files, never commit generated patches)

Set `EF2000_EXE` to the original DOS executable and `EF2000_PAYLOAD` to the
extracted **1,662,896-byte** initialized X-32 payload with SHA-256
`3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`.
Build a Unicorn runner with the link guard (`cmake` or the Nix flake), then:

```sh
python3 tools/scan_startup_entries.py \
  --executable "$EF2000_EXE" --payload "$EF2000_PAYLOAD" \
  --runner local/build/ef2000-unicorn-cpp \
  --report local/build/startup-entries.json

# Compile reviewed C bodies to separate ELF32 sections with their observed
# diagnostic calling conventions. These flag sets produced 26, 14 and 19
# instruction bytes, respectively, on the tested GCC32 toolchain:
gcc -m32 -std=c99 -Os -mregparm=1 -fcall-saved-ecx -fcall-saved-edx \
  -ffunction-sections -fno-pie -fno-pic -ffreestanding \
  -I decomp/include -c decomp/src/runtime_indexed.c \
  -o local/build/runtime-indexed-gcc32.o
gcc -m32 -std=c99 -Os -mregparm=1 -fcall-saved-ecx -fcall-saved-edx \
  -fomit-frame-pointer -ffunction-sections -fno-pie -fno-pic -ffreestanding \
  -I decomp/include -c decomp/src/math_string.c \
  -o local/build/math-string-gcc32-preserved.o
gcc -m32 -std=c99 -Os -mregparm=2 -fcall-saved-ecx \
  -fomit-frame-pointer -ffunction-sections -fno-pie -fno-pic -ffreestanding \
  -I decomp/include -c decomp/src/ui_widget_helpers.c \
  -o local/build/ui-widget-helpers-gcc32-return.o

# Generate SHA-pinned diagnostic-only patch plans:
python3 tools/elf_inplace.py local/build/runtime-indexed-gcc32.o \
  --payload "$EF2000_PAYLOAD" --reviewed-flow 0x604 \
  --function encode_runtime_segment_offset --allow-nop-pad \
  -o local/build/startup-encode-604.efpatch

python3 tools/elf_inplace.py local/build/math-string-gcc32-preserved.o \
  --payload "$EF2000_PAYLOAD" --reviewed-flow 0xe7835 \
  --function to_lower_ascii \
  -o local/build/startup-lowercase-e7835.efpatch

python3 tools/elf_inplace.py local/build/ui-widget-helpers-gcc32-return.o \
  --payload "$EF2000_PAYLOAD" --reviewed-flow 0x9d749 \
  --function copy_string_9d749 --allow-nop-pad \
  -o local/build/startup-copy-string-return.efpatch

python3 tools/compare_guest_replay.py \
  --executable "$EF2000_EXE" --payload "$EF2000_PAYLOAD" \
  --runner local/build/ef2000-unicorn-cpp \
  --patch local/build/startup-encode-604.efpatch \
  --patch local/build/startup-lowercase-e7835.efpatch \
  --patch local/build/startup-copy-string-return.efpatch
```

The object names denote local GCC32 diagnostic builds; they are not committed
artifacts. The converter checks actual ELF sections, expected code extents
and original payload hashes; it rejects a different object rather than
silently splicing it. Compiler-version changes can also alter body sizes.

Asset-free checks: `python3 -m unittest discover -s tests -q` (388 tests,
81 skipped); static-Unicorn CMake/CTest includes x87 and clock regressions.
`nix flake check` could not run on this host because Nix is unavailable.

## Next high-leverage work

1. Supply/mount the private EF2000 game data so startup reaches the menus
   and ultimately cockpit. Record golden checkpoints and input scripts.
2. Use the coverage ranking to recover high-frequency floating-point and
   random-number helpers, with exact x87/flag/scratch-register semantics.
3. Introduce intermediate call-return snapshots for patched candidates,
   especially `copy_string_9d749`'s nonmatching scratch EDX.
4. Switch successful GCC32 diagnostics to reviewed Watcom/OMF ABI-compatible
   implementations and only then advance individual tracker verification.
