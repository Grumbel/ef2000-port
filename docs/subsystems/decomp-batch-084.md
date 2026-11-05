<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 084: twenty named mission-planning callbacks

This batch recovers **20 distinct, Ghidra-inventoried callback functions**
from the mission-planning GUI. These are not newly guessed addresses or
threaded interpreter jump targets. Their existing original names and body
lengths come from `decomp/symbols/functions.tsv`; their branch structure,
global addresses, and callee sites were independently inspected with i386
disassembly of the original X-32 payload. SHA-256 of that local, uncommitted
payload is `3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`.

They remain **host-tested drafts**, not OpenWatcom-compiled or guest-verified
replacements. A GUI callback table can invoke functions without having two
direct `CALL` instructions; original register outputs and ABI clobber sets
have **not** been confirmed. Consequently `signatures.tsv` remains unchanged,
the new C prototypes are host-level models, and neither file can currently
be used for a safe OMF splice.

| Address | Original symbol | Recovered behavior |
| --- | --- | --- |
| `4bc40` | `mpZoomInMap` | Dispatch mode 1/2/4 to separate map callbacks; re-read mode after each call |
| `4bec0` | `mpRedrawMap` | Conditional map refresh and clear dirty flag |
| `4bef0` | `mpMapSelect` | Map-layer redraw, plus mode-4 waypoint-height widget |
| `4c610` | `mpCycleMap` | Toggle word `12656a` using `1 - value`; dirty latches |
| `4c638` | `mpEWRCoverage` | Cycle byte `1a559d` through 0, 1, 2 |
| `4c66c` | `mpToggleFlags` | Independent cycle of byte `1a559e` |
| `4dae8` | `mpClearWPRoute` | Locked/inactive/0xff guards; clear route, redraw and re-read selection |
| `4e768` | `mpRedrawWPHeights` | Reset 24 waypoint heights and 24 status bytes; preserve index zero |
| `4eb78` | `mpIncSpeed` | Contrary to symbol name, toggle byte `1a55a1` zero/one |
| `4ec14` | `mpDecSpeed` | Contrary to symbol name, add 60 to a 16-bit word and cap at 900 |
| `4ec44` | `mpMission1Click` | Subtract 60 from a 16-bit word and floor at 120 |
| `4f2f8` | `mpClickV8Okay` | Fetch dialog selection at widget +16, close dialog |
| `4f36c` | `mpWarningOkClick` | Same operation on a second dialog |
| `505e4` | `mpReturnToCampaign` | Look up, release and close campaign window |
| `50604` | `mpNoQuit` | Restore three menu bytes as words, leave menu, select mode 6 |
| `50750` | `mpMissPlanTask` | Toggle task enabled, mark two UI state bytes dirty |
| `507f4` | `mpSetRole` | Apply role choice from widget +16 |
| `5080c` | `mpSetACtype` | Set current aircraft choice and update secondary widget |
| `50c2c` | `mpChangeMode` | Branch to map zoom or alternative guest mode |
| `50c40` | `mpRedrawTargetView` | Invert view mode and update its mode-dependent label |

## Exactness points

- Speed arithmetic stores a **16-bit result before an unsigned comparison**.
  For example, incrementing `0xfff0` by 60 yields 44 (not 900), and
  subtracting 60 from zero yields `0xffc4` (not 120).
- EWR and map-flag cycling resets the state on **exactly** 3, not on every
  value greater than two. The original byte arithmetic wraps at 255.
- `mpClearWPRoute` deliberately reloads selected waypoint index `1a5598`
  after its intermediate UI call, which may change it. The test hook
  mutates the selection at that point and checks that the later array lookup
  follows the new index.
- `mpZoomInMap` checks the mode byte before *each* branch, allowing a UI
  callback to alter which subsequent branch is executed. The test covers
  a mode transition inside the first callback.
- `mpMapSelect` passes EBX=`137960`, EDX=`137988`, EAX=`137980` to the
  original `4bf1c` routine. This unusual three-register call uses a
  dedicated test hook; the other UI calls retain EAX/EDX in the host model.

## Host validation and remaining work

`tests/decomp_mission_callbacks_test.c` covers all 20 entry points, the
observable memory writes and argument order, boundary cases for toggle and
speed operations, and reentrancy-sensitive callbacks. The Nix host-check
includes this test. The original game EXE, Ghidra workspace, OpenWatcom and
Unicorn are not committed; **no live guest differential, exact ABI
validation or golden replay** has been performed for this batch.

Before calling them verified: recover complete register preservation for
each caller/callback-table entry, validate the small and indirect UI
callees (notably `4bf1c`, `e5088`, `a46cc`, `a7434`, `a72d8`), build OMF
objects, and compare patched/unpatched guest memory and menu replays.
The existing source currently provides a testable C model with explicit
foreign-call hooks, **not a drop-in binary splice**.
