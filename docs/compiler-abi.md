<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# EF2000 compiler and ABI evidence (in progress)

This is a working evidence log, **not** a verified global calling convention.
The image is the SHA-pinned German TACTCOM X-32 payload. Ground signatures
in original disassembly and caller behavior, not in Ghidra's guesses.

## Provisional compiler flags

`ef2000-wcc` currently uses `-bt=dos -3r -zp1 -fpi87 -wx`. These are
bootstrap flags for producing OMF, **not evidence** that the original binary
used these switches. Step 3.5 must establish the actual register ABI, x87
precision/control word, struct packing and optimizer constraints before
compiling recovered game logic. Binary matching is not a goal.

The Nixpkgs wrapped compiler supplies `WATCOM`, `INCLUDE` and the host
`binl64`/`binl` search path. The v2 wrapper is explicitly selected even when
the 1.9 compatibility compiler is also in the development shell.

## Confirmed local instruction observations

| Guest entry | Original instruction evidence | Still unknown |
| --- | --- | --- |
| `0x4050` high-score defaults | Saves EBX, ECX, EDX, ESI; restores them before `ret` on both exits; calls `0x9c960` with EDX zeroed; uses a 25-byte record stride | Caller expectations, return/clobber registers, called runtime ABI |
| `0x41fc` high-score load | Saves EBX, ECX, EDX, ESI; reserves four stack bytes; passes EAX=`0x17f694` and EDX=0 to indirect call `[0x132ab4]` | Target ABI, stack cleanup, outputs |
| `0x432c` score threshold | Loads score in EAX, compares against last record, executes `setg al`, then `ret` | Whether callers consume only AL; upper EAX bits retain score bytes |
| `0x433c` high-score insert | Saves EBX, ECX, EDX, ESI, EDI, EBP and reserves eight stack bytes | Input ABI and full return behavior |

Evidence: `objdump -b binary -m i386 -M intel -D --start-address=...`
against `local/ghidra/payload.bin`, and `docs/highscores-reconstruction.md`.
These are not complete signatures; no row has been promoted to `confirmed`.

Packing evidence: the high-score record occupies 25 bytes with a signed
32-bit score at unaligned offset `+21`. The GUI icon `next` pointer at `+0x21`
is another candidate for packed layout. These observations do not prove
every structure in the original program was packed to one byte.

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
2. Confirm inputs, outputs, preserved registers, stack cleanup, x87 state
   and segment assumptions from disassembly and at least two callers each.
3. Record confirmed signatures in `decomp/symbols/signatures.tsv`, adding
   `#pragma aux` declarations only when justified by evidence.
4. Compare `-3r`/`-5r`, packing, optimizer and FP options, then update
   `ef2000-wcc` and this document. No game C replacement is verified yet.
