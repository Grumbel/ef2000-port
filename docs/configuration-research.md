<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Original EF2000 setup and configuration: supplied German Tactcom installation

## Evidence and limits

The user-supplied `EF2000.zip` includes the original data files and executables.
The originals were read from the ZIP (or disposable copies) without modification.
In particular:

- `_EF2000.EXE` SHA256 `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`
- `CONF.EXE` SHA256 `971228001ee969a368133fcd103206e3a6c62f2fc8c92622ce869187fd9e217e`
- `SETSOUND.EXE` SHA256 `b26112af26c694883d01513d71f1600df0862a8105a0d706073e0f4f1b1903b7`

This is a static examination, **not** an execution trace of the original DOS
setup. The distinctions between witnessed file bytes, executable string-table
evidence, and inferred field semantics matter. The ZIP contains German-localized
Tactcom files; other EF2000 releases or patches may differ.

## Programs and files

| Program/file | Observed role |
|---|---|
| `CONFIG.BAT` | Sets `DID=.` and `quick_flag=1`, invokes `CONF.EXE` |
| `CONF.EXE` | Setup UI, diagnostics/joystick calibration, settings writer; invokes `SETSOUND.EXE` and `UVCONFIG` |
| `SETSOUND.EXE` | Miles audio device selection; formats/reads `DIG.INI` and `MDI.INI` |
| `DIAGNOST.EXE` | Tests input, video, audio and CD; has VESA-mode test strings |
| `UVCONFIG.EXE` and `UNIVBE.DRV` | Legacy VESA/UniVBE graphics-driver configuration; not a game preference file |
| `EF2000.BAT` | Sets the same environment and runs `_EF2000.EXE gui` |
| `EF2000.CFG` | Primary human-readable game preferences and joystick calibration |
| `DIG.INI`, `MDI.INI` | Miles digital-effects and MIDI driver selection/hardware parameters |
| `MISSION.CFG` | Separate, human-readable Conflict generator mission records (`NUM_MISSIONS`, `MISSION`, `GROUP`, `WAYPOINT`), **not** an EF2000 preference file; see [mission research](existing-tools-and-releases.md) |
| `CFG/COL` | Separate installation/asset data, not an EF2000 preference file |

The setup UI's embedded labels explicitly offer **320×200 and 640×400** for
resolution (`Auflösung`), and **Low, Medium, High** (`Niedrig`, `Mittel`, `Hoch`)
for detail. It also exposes flight control, speech, music/effects and VR headset
selection. No 1024×768 option appears in this `CONF.EXE` option string group;
the port's additional VESA modes are emulated capabilities, not proof that this
original game version renders in them.

## EF2000.CFG format

The shipped file is ASCII `KEY=value`, one setting per **CRLF** line, uppercase
keys, decimal integers, comma-separated decimal lists and a final CRLF. Its
exact contents are:

```ini
DETAIL=2
REZ=0
CONTROL=1
MUSIC=1,127
STICK=15
JOYSTICK=4000,63,63,63,63,0,126,0,126,63,0
SOUNDFX=1,127
SPEECH=1
VRHELMET=0
JOY_FACTOR=0
```

| Key | Evidence / interpretation | Confidence |
|---|---|---|
| `DETAIL` | Three setup choices; value 2 probably High (0..2) | Strong for UI, ordering inferred |
| `REZ` | Two setup choices 320×200 and 640×400; exact 0/1 mapping unverified | Strong for choices, mapping unknown |
| `CONTROL` | Flight input (0 keyboard, 1 joystick in the existing port) | Established by port tests |
| `MUSIC` | Driver/device selection and volume, here `1,127` | Strong for format; device number unverified |
| `STICK` | Joystick type selection, here 15 | Probable; enumeration unknown |
| `JOYSTICK` | Eleven calibration integers; ranges/semantics not fully recovered | Strong for count/type |
| `SOUNDFX` | Effects device selection and volume, here `1,127` | Strong for format; device number unverified |
| `SPEECH` | Speech enable/selection (1) | Probable |
| `VRHELMET` | VR headset setting; 0 off, 1 VFX1 used in port | Established by port behavior |
| `JOY_FACTOR` | Joystick calibration multiplier; 0 in shipped file, port adjusts to 100 for joystick mode | Established by port tests |

`CONF.EXE` embeds the literal `printf`-style patterns for all of those fields,
including `JOYSTICK=%ld` repeated eleven times. The main `_EF2000.EXE` also
contains `EF2000.CFG`, `CONTROL`, `DETAIL`, `MODEL`, `FULLSCREEN`, `SOUNDFX`,
`MUSIC`, `STICK`, `JOYSTICK`, `REALISM`, `JOYSCALE`, `HUDCOL`, `NETMODE`, `SPEECH`,
`VRHELMET`, and `JOY_FACTOR` plus corresponding numeric conversion formats.
These are strong static evidence of a game-side parser/serializer, **not** a
complete proof of what each undocumented key does. Notably `REZ` appears in the
configuration program's formatting strings but not as a plain string among
those main-executable key names. How the main game applies `REZ` requires a
trace or further disassembly.

## Miles sound INI files

Both files have semicolon-comment headers and whitespace-separated key/value
fields rather than `KEY=value` syntax. In this installation:

| Field | `DIG.INI` | `MDI.INI` |
|---|---|---|
| `DEVICE` | Creative Labs Sound Blaster 16 or AWE32 | Creative Labs Sound Blaster(TM) 16 |
| `DRIVER` | `SB16.DIG` | `SBPRO2.MDI` |
| `IO_ADDR` | `220h` | `220h` |
| `IRQ` | `-1` | `-1` |
| `DMA_8_BIT`, `DMA_16_BIT` | `-1`, `-1` | `-1`, `-1` |

`SETSOUND.EXE` contains both INI filenames, `DEVICE`, `DRIVER`, `IO_ADDR`,
`IRQ`, `DMA_8_BIT`, `DMA_16_BIT` and corresponding formatting strings.
The runtime already substitutes a local `MDI.INI` for experimental CD audio.
Reproducing the original sound-card setup screens is unnecessary for synthetic
SB16 emulation; the original port flags should remain the authoritative hardware
selection.

## How the port currently reads and writes configuration

1. `tools/run_startup.py` chooses `XDG_CONFIG_HOME/ef2000/EF2000.CFG` if it
   exists; otherwise it reads the original alongside `_EF2000.EXE`.
2. It splits the file into lines, overrides `MUSIC`, `SOUNDFX`, `SPEECH`,
   `VRHELMET`, `CONTROL` and (for joystick) `JOY_FACTOR` as needed, then writes
   a CRLF file to the disposable `OUTPUT/drive/EF2000/EF2000.CFG`.
3. `runtime/startup.c` serves guest file opens from that writable overlay
   first, falling back to the original data directory for reads. Writes are
   copy-on-write into the overlay, so original assets and config stay untouched.
4. `tools/real_mode.py` additionally rewrites `CONTROL` and calibration on
   automatic joystick detection. A game-created change stays in the run overlay
   unless separately persisted by the host; it does not update the user's
   XDG configuration.

## Suggested native replacement for setup

Safe, useful candidates for a host settings menu or CLI:

- **Graphics detail:** a `--detail low|medium|high` option and overlay cycle,
  generating `DETAIL=0|1|2` in the per-run CFG. Test each level against the
  original game before claiming an exact mapping. The original chart also has
  **Alt+D** for selecting detail *in flight*.
- **Original rendering mode:** expose **320×200 vs 640×400**, independently of
  the host window's size/aspect/filtering. First verify whether `REZ` or another
  guest setting determines the mode. **Alt+R** is labeled VGA/SVGA selection on
  the original keyboard chart.
- **Input:** retain the port's joystick selection, axis mapping and calibration
  controls; a raw `STICK` enum UI would be misleading without recovering it.
- **Sound:** keep `--sound`, `--mute`, and synthetic hardware choices; a DOS
  IRQ/DMA wizard is inappropriate for this runtime.
- **VR:** retain `--vr off|vfx1`.

Do **not** rewrite the user's original game CFG in place. Validate and write a
disposable copy or an explicit XDG user config atomically. Do not silently
map arbitrary `REZ` numbers to the port's extra VESA modes.
