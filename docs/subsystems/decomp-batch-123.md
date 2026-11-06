<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 123: initialize mode limits

## `initialize_mode_limits` (`0x58d24`, 29 bytes)

Original disassembly:

```text
53                         push ebx
52                         push edx
ba 08 00 00 00             mov edx, 8
bb 0a 00 00 00             mov ebx, 10
66 89 15 e2 6c 1a 00       mov word [0x1a6ce2], dx
66 89 1d e8 6c 1a 00       mov word [0x1a6ce8], bx
5a                         pop edx
5b                         pop ebx
c3                         ret
```

The no-argument routine writes words 8 and 10 to `0x1a6ce2` and `0x1a6ce8`.
It preserves every GPR and EFLAGS. Caller `0x58d64` invokes it at `0x58efc`;
caller `0x597d8` invokes it at `0x598a6` after setting DL to 1, then reads
DL after return to store it at `0x1a6d1c`. That proves EDX must be preserved
at the call boundary.

The Watcom C body uses two private `#pragma aux` helpers for direct absolute
word stores, plus six flag-neutral NOP helpers. The four leading NOPs keep
the first write at the same instruction ordinal as the original, the second
store remains immediately after the first, and two trailing NOPs preserve
the original nine-instruction count before RET. The original uses stack
pushes and pops for EBX/EDX; the replacement does not touch registers or
stack. Thus it preserves the same register/flag interface and write order;
the only RAM difference is the original's dead eight-byte stack scratch
below incoming ESP, which §2.2 permits.

## Differential and replay evidence

- OpenWatcom 2.0 compiled both production `runtime_control.c` and the focused
  fixture with zero warnings. The OMF splice contains one in-place write at
  `0x58d24`, with bytes
  `90 90 90 90 66 c7 05 e2 6c 1a 00 08 00 66 c7 05 e8 6c 1a 00 0a 00 90 90 c3 cc cc cc cc`.
  The 25-byte body and four INT3 padding bytes fit the 29-byte original
  extent. The emitted body preserves all original nine instruction slots.
- The 37-case isolated differential varies all general registers and both
  target memory neighborhoods. It passes all cases; target words receive the
  original constants and guard bytes remain unchanged.
- A live cockpit call captured from the original executable passes captured
  state differential (**1/1**).
- The repeated-baseline 500M menu / 700M setup / 1.5B cockpit replay passes
  all three cases with the ten previously verified patches plus this
  candidate. The target is not called in menu or setup; it is hit once in
  cockpit. Selected 64 KiB BSS, framebuffer, stop status, interrupts, ticks
  and IRQ0 match the repeated original. The suite summary and original call
  capture remain in ignored `local/decomp-batch-123-mode-limits/`.
- The fixture patch SHA-256 is
  `62af17bfa152a96c9cc44286eb5687c73b3c40bd72dcb0b7163b28dd842fd2ae`.
  `python3 tools/decomp_status.py check` passes after the tracker update.

The gameplay replay compares selected BSS and checkpoint outputs, not all
64 MiB of guest memory. The isolated and captured function differentials
verify the actual ABI-visible behavior.

The tracker state is `verified`.
