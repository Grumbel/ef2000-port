<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 088: environment selectors and campaign GUI

Twenty **new original, named entry points** have host-tested C drafts,
covering eight environment/configuration functions and twelve GUI callbacks.
The 4,287-entry tracker now has 3,257 `compiles` entries and 127 `drafted`
entries. **No new functions have a proven Watcom ABI, compiled-and-spliced
equivalence, or guest-state differential replay.** The host callbacks are
interception points for guest dependencies, not replacement implementations
of those dependencies.

The original source of truth is the German X-32 executable payload with
SHA-256 `3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`.
`objdump -D -b binary -m i386 -M intel` was used for bounded disassembly;
no original bytes or generated Ghidra pseudo-C are present in the patch.

## Environment configuration (eight functions)

| Entry | Game symbol | Host operation |
| --- | --- | --- |
| `dbdd0` | `normal` | Advance input cursor `425af0` across literal spaces; report only non-`;`, non-LF, non-NUL lines using unresolved guest formatter/sink hooks |
| `dbe5c` | `spacecorr` | OR `425bc4` with `08`, `159130` with `84`; return new first byte in AH |
| `dbe80` | `rings` | Same two masks as `spacecorr`; set `425c89=1`; return new first byte in AH |
| `dbeac` | `moon` | OR *word* `425bc4` with `0108`, OR `159130` with `80`, set `425c89=1`; return new second mask in AH |
| `dbedc` | `inferno` | OR byte `425bc4` with `10` |
| `dbee4` | `shield` | OR byte `425bc4` with `01` |
| `dbeec` | `city` | OR byte `425bc4` with `22` |
| `dbef8` | `day` | Zero `15bb5f`, OR byte `159130` with `80` and byte `425bc5` with `08`; return new second mask in AH |

`normal` is the name of a parser/diagnostic handler, **not** the normal
terrain-shading algorithm. Its two original calls are `a4342` (formatter)
and the pointer at `[132a74]` (diagnostic sink). A host hook receives the
source cursor, format pointer `190448`, and sink pointer, without claiming
that the unresolved stack-local message formatting is byte-identical.

## GUI callbacks (twelve functions)

| Entry | Game symbol | Recovered behavior |
| --- | --- | --- |
| `4318` | `mdMainloop` | Pass incoming widget through `41fc` and `40dc` |
| `4cf4` | `gpwChangeDifficulty` | Parse text at widget+`3f`, store AX at `121e46`, select 76-byte row in table at `121e40`, notify widget |
| `a960` | `gcChangeMode` | Tail branch to `a870` if `121e8e != 0`, else to `4595c` |
| `a974` | `gcZoomMapInOut` | Store 16-bit `1 - 121e8e`, then update widget text via `a7588` |
| `b574` | `gpwCloseTargetView` | Compare widget text, set `121e95` to 2/1/0, notify selected widget |
| `bde8` | `gpwClickPilot1` | Look up four widgets and pass their child IDs through `a4b80` |
| `be48` | `gpwClickPilot2` | Compare row label; select row 0, otherwise activate fallback widget |
| `be8c` | `gpwClickPilot3` | Same selector, row offset `43`, slot 1 |
| `bed8` | `gpwClickPilot4` | Same selector, row offset `86`, slot 2 |
| `bf24` | `gpwSaveConflict` | Same selector, row offset `c9`, slot 3 |
| `c838` | `gpwShowDebrief` | Increment word counter `121e98`, wrap to zero on signed comparison to `1a53c0`, redraw via `c868` |
| `ca40` | `gpwAlterSize` | Copy two sizing words from `1a365e/1a3660` to `1a4ba2/1a4ba0` |

### Details not to simplify away

- The difficulty callback stores parsed AX at `121e46` **before** reading
  the signed high half of dword `121e44`. Those memory locations overlap,
  so the row calculation uses the *newly parsed value*, not the previous
  upper half. A dedicated host test catches this dependency.
- The target-view callback contains **two separate guest comparisons with
  the same text pointer `180834`**. The second result is not assumed to
  equal the first; the exact two-call sequence and all three paths are
  preserved. Replacing the second pointer with a guessed label would be an
  unjustified semantic change.
- Pilot selector callbacks use the original row stride `0x43` and keep
  their branch-dependent guest calls. For the first row (`be48`), the EDX
  value reaching guest `779f4` depends on a preceding unresolved guest
  comparison. It is not yet a verified part of the host adapter's ABI;
  the selected-row model uses zero for that guest-call argument, and the
  guest-state comparison must resolve it.
- GUI callbacks that end in tail jumps are represented as host sink calls.
  This is sufficient to test dispatch ordering, but **not** a guest splice.
- Mode handlers explicitly preserve the rest of EAX when AH alone is
  written; 16-bit subtraction and comparison preserve wrap and sign.

## Validation and next steps

`tests/decomp_environment_gui_088_test.c` runs isolated synthetic-RAM
tests for all twenty original entries, including corner cases, callback
sequence and branch-choice assertions. The new test is part of the Nix
host-check build, and can also be run with GCC using
`-DEF_DECOMP_HOST_TEST=1` and all three implementation sources.

`tests/test_decomp_batch_088_payload.py` is an **opt-in** audit of
original disassembly, guarded by `EF2000_PAYLOAD` and the pinned SHA-256;
it checks all twenty entries and the duplicate comparison at `b574`.

Remaining: map guest hook register side effects (especially EDX in `be48`),
check actual UI hierarchy and string pointers against live snapshots,
compile with the pinned Open Watcom toolchain, and compare guest memory,
registers and events at original return/tail-jump boundaries. No
substantial emulation performance gain is claimed from these host drafts.
