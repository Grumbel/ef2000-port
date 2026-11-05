<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 102: high-score insertion's original-vs-recompiled guest behavior

This batch moves beyond the 15-byte Boolean predicate from batch 101: it
tests a 242-byte original routine that changes a **250-byte packed high-score
table**, using an independently compiled recovered C implementation of its
complete insertion and record-shifting algorithm. The source is already in
`decomp/src/hiscore_data_093.c`, written in batch 093; this batch does not
introduce a second implementation of the algorithm.

## Confirmed ABI and original instruction evidence

`0x433c..0x442d` (`hiscore_insert`) takes the NUL-terminated guest name
address in **EAX** and returns the inserted row index, or `-1`, in **EAX**.
Original prologue `push ebx/ecx/edx/esi/edi/ebp` and both return paths
restore these registers and execute `ret` with **no callee stack cleanup**.
The only direct caller found in the pinned payload is `0x4886`, which loads
EAX with `0x17f6f0`, calls `0x433c`, then stores/tests AX at `0x488b/0x4891`.
The signature tracker is now `confirmed`, but the function remains `drafted`.

In the original code, a signed score comparison decrements the threshold
*before* `JLE`; the `INT_MIN` case wraps to `INT_MAX`. Insertion shifts
records 25 bytes at a time, but calls the guest NUL-terminated string-copy
routine (`0x9d749`), which means the trailing name padding is **not**
overwritten. The recovered C preserves that behavior and avoids undefined
signed overflow when decrementing the comparison threshold.

## Diagnostic GCC guest replacement

The guest-only compilation profile is:

```sh
gcc -m32 -std=c99 -O2 -Wall -Wextra -Werror \
  -mregparm=1 -fcall-saved-ecx -fcall-saved-edx \
  -ffunction-sections -fno-pie -fno-pic -ffreestanding \
  -I decomp/include -c decomp/src/hiscore_data_093.c \
  -o local/build/hiscore-data-gcc32.o

python3 tools/elf_inplace.py local/build/hiscore-data-gcc32.o \
  --payload /path/to/extracted/payload.bin --reviewed-flow 0x433c \
  --function hiscore_insert_433c --allow-draft --allow-nop-pad \
  -o local/build/hiscore-insert-gcc32.efpatch

python3 -c 'from pathlib import Path; from tools.efpatch import apply_patches; p=Path("/path/to/extracted/payload.bin"); image,_=apply_patches(p.read_bytes(), [Path("local/build/hiscore-insert-gcc32.efpatch")]); Path("local/build/hiscore-insert-gcc32-patched.bin").write_bytes(image)'

c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  tests/guest_hiscore_insert_diff.cpp $(pkg-config --cflags --libs unicorn) \
  -o local/build/guest_hiscore_insert_diff
local/build/guest_hiscore_insert_diff \
  /path/to/extracted/payload.bin local/build/hiscore-insert-gcc32-patched.bin
```

`-mregparm=1` provides EAX input; GCC normally expects a stack argument.
`-fcall-saved-ecx -fcall-saved-edx` preserves the two Watcom register-save
obligations that the default GCC i386 ABI treats as caller-clobbered. These
flags apply to the **diagnostic** object, not a recommendation to substitute
GCC for OpenWatcom in the hybrid splice pipeline.

The insertion function compiles to **226 bytes**, shorter than the reviewed
**242-byte** original extent; the explicit `--allow-nop-pad` switch pads the
end with 16 NOPs. The converter can select exactly one `.text.NAME` from a
`-ffunction-sections` object while ignoring unrelated function sections.
External code relocations must still resolve against the tracker, and
private allocatable data or unsupported relocations still fail closed.

**Caveat:** the padded in-place replacement is suitable for this isolated
test only. Before whole-game use, audit all possible jumps into the middle
of `0x433c..0x442d` (including computed dispatches), and run the actual
OpenWatcom patch rather than relying on these GCC diagnostic flags.

## Differential observations

`tests/guest_hiscore_insert_diff.cpp` runs original and patched images in
separate Unicorn 32-bit CPUs with the same game-typical memory layout. It
compares the whole packed 250-byte table, the source-name bytes, current
score, **all eight general-purpose registers**, ESP, and return PC. Its
write-watch rejects guest writes outside the table, current-score word, and
the calling stack. This intentionally excludes irrelevant scratch stack
bytes; write **ordering** and EFLAGS were not proven identical. The caller
immediately tests AX after storing it, so its condition flags are reset.

- **100/100** original-vs-C isolated guest cases passed: 10 hand-picked
  cases (top/middle/last insertion, rejection, ties, negative scores,
  `INT_MIN` rollover, `INT32_MAX`, and a 19-character name) plus **90**
  seeded randomized sorted scoreboards.
- Each accepted case matched all record fields, including the NUL-only copy
  of names and untouched padding. Return registers and incoming preserved
  registers matched even though the compiled instruction sequence differs.
- The original calls its guest `strcpy` helper; the GCC body uses an inline
  copy loop. Both returned the same observed state for the tested cases.
- The source remains a derived, unverified C **draft**, not a completed
  OpenWatcom function replacement. The repository still has **zero**
  fully verified functions under the project definition.

## Next work

The meaningful missing checks are an OpenWatcom OMF build of this function,
an ABI-correct OMF splice, and an end-to-end patched-game golden replay. The
local execution environment has GCC32 and a supplied static Unicorn 2.1.4,
but no `nix` or `wcc386` command, and the supplied Unicorn backend crashes
inside x87 guest instructions on attempted whole-game startup. Therefore
this batch claims **isolated** guest execution only, not a fixed running game.
