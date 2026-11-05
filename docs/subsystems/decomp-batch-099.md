<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 099: guest RAM replay hashes and native patch-hook validation

This batch extends the partial Phase 1 §3.9 golden replay checker from batch
098. It does **not** mark a recovered function verified.

## Change

The portable C++ Unicorn runner now accepts repeatable
`--hash-guest ADDR:LENGTH` options. At guest stop it computes SHA-256 for
selected mapped linear-RAM ranges (maximum 16 MiB per range, 32 ranges).
The default run performs no additional memory reads or hashing.

`tools/replay_suite.py` optionally accepts `guest_hashes` in each scenario.
It checks that original repeated runs and patched runs report the **same**
selected memory hashes, in addition to the framebuffer, stop status and PIT
counters. The scenario validator refuses regions overlapping any patch write,
duplicate/overlapping ranges, oversized ranges and unexpected/missing output
records. The complete compared data need not be written to disk.

The C++ runner also has `--self-test-patch-hits`: a short synthetic guest
calls a `ret` entry at `0x200` twice. The existing patch-entry hook must
report two calls, along with one at the program entry. This is now tested
by the CMake and Nix check expressions, and the native hash is checked against
the known SHA-256 of four zero bytes.

## Evidence obtained locally

The supplied Unicorn 2.1.4 binary archive contains headers and a static
library. This workspace has no Nix, OpenWatcom or system Unicorn package,
but the host C++17 compiler and OpenSSL headers are available. A native
headless runner was built **outside the Git tree's tracked paths** using
the project's `runtime/*.cpp` compilation list from `CMakeLists.txt`, the
archive's `include` and `lib/libunicorn.a`, and `-lcrypto -lm -latomic -ldl
-pthread`. Linking against the archive's `libunicorn.so` failed because it
requires `sqrtf@GLIBC_2.43` on an Ubuntu glibc 2.39 host; using the static
archive plus `-latomic` succeeded.

- Native `--self-test`, `--self-test-bda`, `--self-test-retrace`: pass.
- Native `--self-test-patch-hits --hash-guest 0x300:4`: returns `guest_exit`,
  reports `patch_hit: addr=0x200 count=2` and
  `guest_hash: addr=0x300 length=4
  sha256=df3f619804a92fdb4057192dc43dd748ea778adc52bc498ce80524c014b81119`.
- Two unpatched runs of the supported SHA-pinned `_EF2000.EXE` at 400,000
  instructions with instruction-counted timing return the same PIT counters,
  EIP and 256-byte hash of guest RAM `0x1970a0..0x19719f`. The observed
  original-game hash remains in the ignored local experiment output.
- A disposable **identity** EFPATCH1 (the eight bytes at guest entry `0x86`
  replaced with the exact same eight original bytes) was accepted against
  the original payload SHA-256. It reported `patch_hit: addr=0x86 count=1`
  and exactly the same 256-byte hash and PIT counters. The identity patch is
  not committed, is not a Watcom output, and proves only that patch loading,
  instrumentation and selected memory comparison work through a real game
  executable entry.

The original executable and extracted payload remain in ignored local
workspace storage, unchanged. No guest bytes, patches or screenshot data
are committed. In longer runs here, the original executable crashes the
locally statically-linked Unicorn runner (exit 139, beyond 800,000 guest
instructions), before reaching a useful framebuffer. This run lacks the
complete installed game data; the precise cause of the host crash has **not**
been established, so it must not be interpreted as a game-code regression.

## Still blocked / next

1. Obtain or build `wcc386` and compile the `0x432c` C candidate to an OMF
   object; inspect relocations and code size against its 15-byte entry.
2. Prove a real replacement can be safely spliced. If it is larger than the
   original, the current in-place-only splicer rejects it: no executable
   arena has been validated yet.
3. Run actual call-state differential checks with original game execution,
   then scripted menu/flight replays using the **complete** installation.
4. Add palette and multiple timed intermediate checkpoints, establish a
   golden corpus and cross-check unexpected hash divergences.

**Verified recovered functions: 0.** This batch strengthens the oracle;
it does not claim parity for a C replacement.
