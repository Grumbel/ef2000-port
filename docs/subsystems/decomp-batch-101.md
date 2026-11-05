<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 101: first compiled C body executed against original guest code

The previous SHA-bound replay work tested an *identity* patch. This batch
exercises a genuinely different, independently compiled C implementation in
the same guest CPU against the unmodified original function. The scope is
intentionally narrow: one high-score predicate, not a full game replacement.

## Reviewed original and candidate

`hiscore_qualifies` at payload `0x432c` has exactly **15 bytes**. Its callers
`0x49b2` and `0x4b64` consume **AL**, and the known original signed-int32
comparison tests `g_hiscore_current` (`0x1970a0`) against the tenth packed
entry's score (`g_hiscore_table + 9*25 + 21 = 0x19719a`). A tie is false.

`decomp/src/hiscore.c` already contains the corresponding C expression. Here
the host GCC 32-bit x86 build, without PIC or a hosted runtime, emitted
exactly 15 bytes of `.text` and two `R_386_32` relocations. The compiled code
reverses the CMP operands and uses SETL instead of the original SETG: its AL
result is equivalent for signed 32-bit values. The remaining GPRs are checked
by the guest probe rather than assumed from disassembly.

`tools/elf_inplace.py` deliberately accepts only a single confirmed function
which fills its `.text` section and matches an explicitly reviewed original
extent. It permits external absolute 32-bit code relocations resolved against
the existing function/global symbol trackers; it rejects private allocated
data, extra executable sections, arbitrary relocation kinds, ambiguous
symbols, overlapping relocations and wrong-sized functions. The result is a
SHA-bound `EFPATCH1` generated using the established patch format. This
converter is for **diagnostic GCC32 C** objects only; it does not replace the
OpenWatcom `tools/omf_inplace.py` production path.

## Reproduce with a privately supplied payload

These examples create only ignored local build artifacts. Use the extracted
1,662,896-byte payload from the original game, never commit the payload, the
generated object, or the patched image. On NixOS, the GCC compilation shown
requires a toolchain capable of generating ELF32 i386 objects; a native 32-bit
linker or 32-bit glibc is unnecessary.

```sh
mkdir -p local/build
gcc -m32 -std=c99 -O2 -Wall -Wextra -Werror \
  -fno-pie -fno-pic -ffreestanding -I decomp/include \
  -c decomp/src/hiscore.c -o local/build/hiscore-gcc32.o

python3 tools/elf_inplace.py local/build/hiscore-gcc32.o \
  --payload /path/to/extracted/payload.bin --reviewed-flow 0x432c \
  -o local/build/hiscore-gcc32.efpatch

python3 -c 'from pathlib import Path; from tools.efpatch import apply_patches; p=Path("/path/to/extracted/payload.bin"); image,_=apply_patches(p.read_bytes(), [Path("local/build/hiscore-gcc32.efpatch")]); Path("local/build/hiscore-gcc32-patched.bin").write_bytes(image)'

# Compile the opt-in isolated guest oracle with Unicorn's C API and C++17:
c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  tests/guest_hiscore_c_diff.cpp $(pkg-config --cflags --libs unicorn) \
  -o local/build/guest_hiscore_c_diff
local/build/guest_hiscore_c_diff \
  /path/to/extracted/payload.bin local/build/hiscore-gcc32-patched.bin
```

The local session used the supplied Unicorn 2.1.4 *static* library and a
standalone C++ guest oracle; `pkg-config` above is the equivalent route with
a normal Unicorn installation. The static library was necessary because the
supplied shared object required newer host glibc symbols. Do not mistake the
local test runner for the game's SDL3/Unicorn frontend.

## Measured results

- **10/10** guest original-vs-C comparisons passed: zero and negative ties,
  positive greater/less, negative greater/less, signed INT32_MIN/INT32_MAX
  in both directions, and both signs around zero.
- Compared **all eight 32-bit general-purpose registers**, the return PC,
  restored stack position, and the expected signed predicate in AL.
- A guest-memory write hook reported **zero writes** in either implementation
  for each case. The suite does not claim that all 64 MiB of RAM was compared.
- **4/4** asset-free Python ELF parser/patcher tests passed, including a
  real `gcc -m32` test when that compiler is available.
- The tested compiled body is **different** from the original. This is
  stronger evidence than a no-op/identity replay but still a bounded oracle.

## What this does and does not unblock

This establishes that an independently compiled C body can be inserted at a
reviewed original address, have its symbols relocated and execute with the
same observed guest result. It is the beginning of function replacement,
**not** the completion of decompilation plan step 3.8/3.9: there has been no
OpenWatcom OMF compile of this exact body, no OMF splice, no integrated patched
game run and no multi-checkpoint differential replay of modified code.

Next, in the proper OpenWatcom environment, build and inspect the candidate
with `ef2000-wcc`, generate a real OMF splice with `omf_inplace.py` and run
both the isolated function oracle and the whole-game `replay_suite.py` with
the code changed. Only after those checks pass may the function be marked
`verified`. Current tracker still has **zero verified functions**.
