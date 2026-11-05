<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 085: campaign and aircraft/weapons-panel callbacks

This batch models **twenty distinct original named callback entry points**,
eight in campaign menu control and twelve in aircraft/weapons configuration.
The entry points and sizes were already present in the Ghidra 12.1.4-based
canonical tracker; independent linear and branch-following inspection used
the SHA-256-pinned payload
`3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`.
Do not commit that payload; these files retain only our own C reconstructions.

These are **host-tested drafts**. The Watcom register ABI, particularly at
callback-driven invocation sites and calls whose results are preserved in
secondary registers, has not been confirmed. Model-level `gc_ui_guest_call_085`
and `gpw_ui_guest_call_085` stand for the unchanged original code. The new
sources are **not** splice-ready, and none is marked `verified`.

| Address | Named callback | Recovered operations |
| --- | --- | --- |
| `40e50` | `gcSetDifficulty` | Campaign mode flags, two dialog closures, subwindow selection |
| `40ebc` | `gcStartNewCampaign` | Set campaign flag byte |
| `40ec4` | `gcPerformanceYes` | Clear campaign flag byte |
| `40ed0` | `gcPerformanceNo` | Parse string at widget +63, mirror low byte to two globals |
| `40ee4` | `gcLoadOldCampaign` | Prepare old-campaign dialog, selector word, copy 12-byte key |
| `40f18` | `gcLoadfileAccepted` | Accept old campaign, copy key and start three UI transitions |
| `40f64` | `InitMainMenu` | Initialize selector words, copy key and activate main menu |
| `40f9c` | `gcSetCampaignDifficulty` | Indirect callback selects active-campaign path or fallback |
| `68f40` | `gpwPlaneSideOn` | Three aircraft-view halfword presets |
| `68f6c` | `gpwPlaneUnderneath` | Underside-view presets |
| `68f98` | `gpwPlaneAngled` | Angled-view presets |
| `68fbc` | `gpwUseDefaults` | Separate, identical default-angle entry |
| `68fe0` | `gpwPlaceMissile` | Missile-placement view presets |
| `6931c` | `gpwSetClearMissile` | Missile-select flags (1,0) |
| `69338` | `gpwSetClearAllMissiles` | Missile-select flags (0,1) |
| `69354` | `gpwCheckHitPylon` | Thirteen indexed guest callbacks |
| `69914` | `gpwShowCurrent` | Current-weapon visibility latch |
| `69920` | `gpwTurnYPlane` | Widget bits alter high yaw byte and/or subtract 0x600 |
| `69978` | `gpwArcadeAccepted` | Same operation on second rotation axis |
| `699d4` | `onopenBigCurrentWeapon` | Clear selection word and resolved current-weapon UI text |

## Behavior details

- The campaign key is copied by three consecutive original `MOVSD`
  instructions, **12 bytes**, from either `184358` or `18439c` into
  `41f30c`; the model uses three explicit little-endian dword writes.
- `40f9c` first calls an **indirect function pointer** stored at `132ab4`.
  The zero result enters the dialog-selection path; nonzero enters the
  two-closure fallback. Both paths are exercised by the host test.
- `40e50` and `40f9c` share a dialog-selection sequence which preserves
  an EAX value across a secondary UI callback; the final `DH = 6` replaces
  only bits 8–15, keeping the other EDX bits.
- Aircraft rotations test widget word bit `0x10` or `0x400` to add six to
  the **high byte** of their corresponding 16-bit angle, and independently
  bit `0x10` or `0x4000` to subtract `0x600` from the whole angle. When
  bit `0x10` is set, both operations cancel; when just one of the high-byte
  flags is set, they do not. Both angles use the same logic but different
  destinations.
- The two angle defaults `68f98` and `68fbc` are genuine separate named
  entry points, not a fabricated extra function; both write the same values.

The new `tests/decomp_campaign_weapon_callbacks_test.c` tests all twenty
entries with strict-GCC compilation, including guest-call sequence, both
campaign branches, 12-byte copies, view presets, all thirteen pylon calls,
angle bit combinations and current-weapon text clearing. It is wired into
the flake host check. This is evidence for the **host models**, not proof of
RAM-identical original-machine behavior or original register conventions.

Before promotion: rebuild Ghidra pseudo-C and caller/ref tables, confirm
callback-table ABIs and nonvolatile registers, compile Watcom OMF, isolate
representative guest call captures, and run differential RAM and golden
replay comparisons. No original data is included in this batch.
