<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 086: multiplayer settings and network UI callbacks

Twenty **previously named but unmodeled** original entries are reconstructed
in `decomp/src/network_settings_callbacks.c` (nine) and
`decomp/src/network_status_callbacks.c` (eleven). These are actual callback
bodies, not the neighboring 5-byte dispatch-table thunks. The original
entry boundaries and branches were inspected with 32-bit x86 disassembly of
the pinned 1,662,896-byte X-32 payload (SHA-256
`3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`).
No original bytes, assets, or decompiler-generated pseudo-C are included.

All twenty remain **`drafted`**: their C-level behavior is exercised in a
strict-GCC host harness, but they have **not** passed OpenWatcom build,
original stack/register ABI verification, injection, or guest RAM
differentials. The guest dependencies are explicit `net_ui_guest_call_086`
hooks. Decimal scanning and option formatting have separate hooks to model
output into the original stack-local scratch buffers. The model callbacks
are *not* an assertion about the real Watcom callee signatures.

| Address | Original named entry | Size | Recovered state/dispatch behavior |
| --- | --- | ---: | --- |
| `335c0` | `gnNewBaud` | 89 | Parse widget value as integer 1–4; store index 0–3, or `ff` on error; refresh |
| `3361c` | `gnNewParity` | 20 | Notify parity-change handlers; fall through to shared network refresh |
| `336c0` | `gnNewFifoLen` | 45 | Guarded XOR of FIFO option `15f099` bit 0; refresh |
| `336f0` | `onopenIpxSettings` | 47 | If option enabled, advance `15f09a` modulo four; otherwise return |
| `33720` | `gnNewNumber` | 21 | Copy widget value to `15f57d`, notify |
| `33738` | `gnNewPt` | 21 | Copy widget value to `15f53d`, notify |
| `33760` | `gnNewInitString` | 74 | Toggle selector `15f688`, format indexed choice, update widget |
| `337ac` | `onopenModemStatus` | 21 | Copy widget value to `15f4fd`, notify |
| `337c4` | `gnNewPort` | 37 | Read byte `13579a`, transform via `e1540`, write AL back; refresh |
| `33ad8` | `gnOpenLoops` | 129 | Populate four related modem/loop widgets, one formatted from selected option |
| `341d8` | `gpwDirectPlayGameSelected` | 62 | Walk `+0x21` widget chain, matching successive 64-byte DirectPlay records |
| `34480` | `gnStartNetArcadeGame` | 29 | Clear socket word `1357b8`; initialize, notify, refresh |
| `344a0` | `gnNewChannel` | 27 | Invert active-channel flag `1357c8` bit zero; notify, refresh |
| `345d4` | `gnNewEndString` | 19 | If network flag `13578c` set, open ending-string window |
| `347ac` | `onopenLoops` | 13 | Zero-extend widget byte `+0x10`, dispatch UI selection |
| `349ec` | `gnMainLoop` | 23 | Resolve named widget into global `19f320`, refresh list |
| `35128` | `onopenInitialise` | 27 | Set pending initialization byte `19f59b=ff`, enter initialization UI |
| `35e68` | `onopenRxstatus` | 37 | Open status widget and invoke `35de0` with two zero stack arguments |
| `35e90` | `gnSetNonEntryFlag` | 37 | Distinct entry, identical original body to `35e68` |
| `35eb8` | `gnNewType` | 75 | Set complementary UI controls from live type flag `13578d` |

## Cross-boundary control flow

Some entry points do not have standalone x86 `RET` instructions. Treating
their tracker size as a closed body would be incorrect:

- `gnNewParity` at `3361c` executes its four short calls, then **falls
  through** to the multi-hundred-byte shared body at `33630`. The C draft
  uses an explicit guest dependency marker to represent the shared tail.
- `gnNewFifoLen`, `onopenIpxSettings`, `gnNewPort`, and `gnNewChannel` use
  `JMP 33630` rather than a call/return pair. `gnStartNetArcadeGame`
  instead calls `33630` and returns normally. Both currently use a host
  callback; the original stack behavior differs.
- The `onopenIpxSettings` disabled path jumps to `33618`, the `RET` at the
  end of `gnNewBaud`; it must **not** run network-refresh logic.
- The `gnNewEndString` disabled path similarly jumps into the `345d2`
  `RET` belonging to the preceding function, while the enabled path
  tail-jumps to the dialog activator at `a4d90`.
- The `onopenInitialise` last instruction is a `JMP ac7a8`, not a normal
  call; `gnNewInitString` and `gnOpenLoops` format into transient local
  buffers. These are exact semantic boundaries still requiring a guest
  stack-lifetime and register-preservation investigation.

The existing `33750` network-abort entry was *not* reclassified or counted
because it is already recorded as a compiled host model named
`forward_if_flag_1357c8`. The nearby 5-byte trampoline names at `84afc`
and above remain separately inventoried and unmodified.

## Host checks

`tests/decomp_network_callbacks_test.c` runs all twenty original entries
under strict GCC warnings, covers both boolean paths and boundary values,
checks ordered original-guest-entry dispatch, validates wraparound for
baud/IPX/channel options, follows the linked DirectPlay widget chain,
checks the four widget fields in `gnOpenLoops`, and checks the duplicated
receive-status body. The host check is part of the Nix flake suite.

Before promoting any entry to `compiles` or `verified`:

1. Confirm live callback-table entry arguments and nonvolatile registers
   at the call sites in Ghidra; record stack cleanup at each ABI boundary.
2. Implement or splice shared refresh `33630`, keeping cross-entry jumps
   and return-address ownership correct; verify stack scratch lifetime for
   `df26f`/`a4342` and the original pointer tables.
3. Build with OpenWatcom, capture guest-side input/output register and RAM
   windows, and compare original vs. replacement with the same executable
   payload, clocks, config, and input replay.

The host harness does not replace those checks and no guest verification is
claimed in this batch.
