<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# VFX1 headset path

## Activation and limits

```sh
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/vfx1-01 --vr vfx1
```

Enter a flight and press F1. The original headset camera shows `VFX1` in the
upper-right corner. The local configuration overlay sets `VRHELMET=1`; the
source installation stays unchanged. Default `--vr off` sets the local value
to zero. This requires the Unicorn real-mode bridge.

This enables a **monoscopic headset view**, with centered yaw/pitch/roll.
There is no physical tracker input, OpenXR output, or stereoscopic rendering.
A private-Xvfb Quick Combat run verified menu navigation, cockpit entry, and
F1 activation. The original camera continues running with the emulated headset.

DID's 2026 [Head Tracking developer post](https://store.steampowered.com/news/posts/?enddate=1770634969&feed=steam_community_announcements)
explicitly states that EF2000 never had stereoscopic rendering. It explains
that the related ADF renderer uses depth/scale tricks that complicate stereo.
The developers give more detail in this [VR discussion](https://steamcommunity.com/app/3146140/discussions/0/765185574573208094/).
This agrees with the game paths traced here: headset discovery and orientation
polling feed a single camera. Merely enabling VFX1 does not produce eye pairs.
True stereo would require renderer changes and per-eye camera translation,
projection, visibility, and cockpit/HUD validation.

## Original executable evidence

All offsets refer to the fingerprinted German TACTCOM payload, SHA-256 of the
source executable `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.

- `82C34h` parses the numeric VRHELMET setting; value 1 selects VFX1.
- `E9AD9h` calls headset discovery and reports the VFX1-not-found message on failure.
- `D9B30h` scans vendor slots 60h..70h via INT33 AX=`slot:7F`.
  Slot 60h is recognized when AX=607Fh returns AX=7F60h. The routine returns
  6000h on success, zero when no slot responds.
- `8B4F2h` selects internal VFX1 camera action 30h; `1D3C4h` binds its handler
  to the F1 keyboard action. `1A1D8h` queues that camera action.
- `D9BA0h` polls INT33 AX=6070h, BX=0301h. Its caller at `93E58h`
  requests orientation group 3. The bridge supplies yaw in BX, pitch in CX,
  roll in DX, and AX=6000h (selected slot, zero low-byte status).
  The original wrapper extracts BX/CX/DX and status into caller pointers.
- `93E58h` and `94000h` feed orientation into the camera transform.

`tests/test_vfx1.py` executes original discovery and polling routines in
Unicorn's 32-bit engine, intercepting their existing real-mode bridge at 8B8h
and forwarding it to the host INT33 implementation. With `EF2000_EXE` set,
these tests verify absent/present detection and decoded orientation values.
`tests/test_real_mode.py` also checks opt-in isolation, upper register
preservation, vendor-slot isolation, and ordinary mouse independence.

The host currently holds centered orientation in `RealMode.vfx_orientation`;
this is a future input integration point, not a connection to a real tracker.
