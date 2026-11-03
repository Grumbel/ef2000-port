<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# EF2000 compiler and ABI evidence (in progress)

This is a working evidence log, **not** a verified global calling convention.
The image is the SHA-pinned German TACTCOM X-32 payload. Ground signatures
in original disassembly and caller behavior, not in Ghidra's guesses.

## Provisional compiler flags

`ef2000-wcc` currently uses `-bt=dos -3r -zp1 -fpi87 -s -wx`. These are
bootstrap flags for producing OMF, **not evidence** that the original binary
used these switches. Step 3.5 must establish the actual register ABI, x87
precision/control word, struct packing and optimizer constraints before
compiling recovered game logic. Binary matching is not a goal.

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
| `0x24480` x87 spatial comparison | Inputs from EAX (sign-extended AX) and EDX (pointer to three qwords); reads an eight-byte stack argument; saves/restores EBX, ECX, ESI, EDI; returns with `ret 8`, low-byte result in AL | Exact semantic types, other caller-visible clobbers, x87 control/status requirements |

Evidence: `objdump -b binary -m i386 -M intel -D --start-address=...`
against `local/ghidra/payload.bin`, and `docs/highscores-reconstruction.md`.
Except for the separate `0x432c` row below, these observations are not
complete signatures and remain unverified.

For `0x24480`, two different caller functions show the same argument setup:
`0x2479c` at callsite `0x248b9` and `0x6aeb4` at `0x6b1aa` both store a
computed x87 value as a qword on the stack, put a scalar/index into EAX and
a pointer into EDX, then call `0x24480`. The second caller consumes AL with
`test al,al` at `0x6b1af`. The callee epilogue at `0x2460d` writes AL,
restores registers, and executes `ret 8` at `0x24616`. This establishes
stack ownership and a register+stack calling *pattern*, but still needs a
fully typed signature and x87 differential evidence.

Packing evidence: the high-score record occupies 25 bytes with a signed
32-bit score at unaligned offset `+21`. The GUI icon `next` pointer at `+0x21`
is another candidate for packed layout. These observations do not prove
every structure in the original program was packed to one byte.

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
2. Confirm inputs, outputs, preserved registers, stack cleanup, x87 state
   and segment assumptions from disassembly and at least two callers each.
3. Record confirmed signatures in `decomp/symbols/signatures.tsv`, adding
   `#pragma aux` declarations only when justified by evidence.
4. Compare `-3r`/`-5r`, packing, optimizer and FP options, then update
   `ef2000-wcc` and this document. No game C replacement is verified yet.
