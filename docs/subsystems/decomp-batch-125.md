<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 125: control mode A

## `set_control_mode_a` (`0x7eea0`, 25 bytes)

Original disassembly:

```text
52                         push edx
8a 15 c6 ba 15 00          mov dl, [0x15bac6]
84 d2                      test dl, dl
74 02                      je +2
88 c2                      mov dl, al
31 c0                      xor eax, eax
88 d0                      mov al, dl
88 15 70 b0 12 00          mov [0x12b070], dl
5a                         pop edx
c3                         ret
```

AL is the input. If byte `0x15bac6` is zero, the routine selects zero;
otherwise it selects AL. It stores the selected byte at `0x12b070`, returns
it zero-extended in EAX, and preserves EDX and the other GPRs. The final
EFLAGS are those from `xor eax,eax`; the replacement is byte-for-byte, so
this flag result is preserved as well.

The callers inspected are `0x3dd30` at `0x3dd98` and `0x7feb0` at `0x7fee9`.
The first clears EAX before calling, while the second sets EAX to 1. Both
continue with a subsequent flag/register operation. The public C declaration
in `ef_abi.h` now returns `uint32_t`, matching the full EAX result.

A straightforward high-level Watcom C lowering was 60 bytes and required an
unsupported arena redirect. The production C wrapper instead uses a private
Watcom aux helper containing the reviewed original sequence, with a C
fallback that expresses the gate and byte latch using typed globals. This
preserves the full original 25-byte extent and ABI exactly.

## Differential and replay evidence

- OpenWatcom 2.0 compiled production `runtime_control.c` and the focused
  fixture with zero warnings. The OMF splice is one in-place 25-byte write
  exactly equal to the original.
- The 37-case isolated test varies AL, upper EAX bits, all preserved
  registers, enabled/disabled gate values and both memory neighborhoods.
  All cases pass, with only the output latch byte changed.
- Two actual calls captured independently from repeated original cockpit
  runs pass captured-state differentials (**2/2**); both captures have the
  gate disabled and AL zero. The enabled-gate/nonzero-AL behavior is covered
  by the isolated boundary and seeded cases.
- The repeated-baseline 500M menu / 700M setup / 1.5B cockpit replay passes
  all three cases with the twelve previous verified replacements plus this
  candidate. `0x7eea0` is reached once in cockpit. Selected 64 KiB BSS,
  framebuffer, stop status, interrupts, ticks and IRQ0 match the repeated
  original. Summary and captures remain under ignored
  `local/decomp-batch-125-control-a/`.
- The fixture patch SHA-256 is
  `ff7d197760a9a7354ffef22fe34210509fc70ef1e2902ba0e164fb0532875bf2`.
  `python3 tools/decomp_status.py check` passes after the tracker update.

The golden suite hashes selected BSS and checkpoint outputs, not all guest
RAM. The exact in-place body and isolated/captured differentials cover this
function's ABI boundary.

The tracker state is `verified`.
