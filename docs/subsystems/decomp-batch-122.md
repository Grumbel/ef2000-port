<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 122: store runtime control word

## `store_runtime_control_word` (`0x3cd64`, 6 bytes)

The SHA-pinned original executable contains:

```text
0x3cd64  a3 bc 35 1a 00    mov dword [0x1a35bc], eax
0x3cd69  c3                ret
```

This is a Watcall entry with the full input dword in EAX and plain `ret`.
The instruction writes exactly four bytes, leaves EAX and the other general
registers untouched, and does not change EFLAGS. The direct call graph has
six distinct caller functions. Two inspected callers are `0x0c630` at
`0x0c660` (loads EAX with `0xbb8` before the call) and `0x3cfc0` at
`0x3d003` (zeros EAX before the call); the next instructions confirm the
call boundary and do not require any extra callee effects.

The ordinary Watcom global assignment generates an OMF external symbol
`_g_runtime_control_word` that the address resolver cannot canonicalize.
The OpenWatcom branch therefore expresses the exact absolute `A3` store as a
private `#pragma aux` inside the C function. `(void)value` suppresses the
Watcom unused-parameter warning without generating instructions. The GCC
fallback remains the typed global assignment. The production module and
fixture compile with zero warnings, and the standalone fixture OMF resolves
to one in-place write at `0x3cd64`, with no arena allocation. Its six-byte
body is identical to the original.

## Differential and replay evidence

- The 37-case guest spec covers zero, signed boundaries, all bits set, the
  original representative value and seeded random EAX/register states. Each
  case surrounds the target dword with sentinels. All cases pass; exactly
  four target bytes change.
- Two separately captured real cockpit calls pass captured-state
  differential (**2/2**), each comparing ABI registers and the four-byte
  memory write.
- The repeated-baseline 500M menu / 700M setup / 1.5B cockpit replay passes
  all three cases with the nine preceding verified replacements plus this
  candidate. The entry is reached **2,435 times** in cockpit. Original,
  repeated-original and patched outputs agree on selected 64 KiB BSS,
  framebuffer, stop status, interrupt, tick and IRQ0 state. The replay
  summary and captures remain in ignored
  `local/decomp-batch-122-runtime-store/full-replay/`.
- The fixture patch SHA-256 is
  `7e2101eed4573929b08f81d40eb27ec089fb8a306530460d21bc5a25d2d99e45`.
  `python3 tools/decomp_status.py check` passes after the tracker update.

The golden replay compares the selected BSS window and checkpoint outputs,
not every byte of guest RAM. The isolated and captured function differentials
cover the full ABI boundary for this leaf.

The tracker state is `verified`.
