# GFX+ / 3Dfx renderer investigation (bundle #063)

The aim is to find a clean **EF2000 scene/geometry -> hardware renderer**
boundary for an eventual native accelerated renderer. This investigation is
static and read-only. `tools/didpatch.py` now rebuilds the patched DOS V2.0,
GFX+ 3Dfx and GFX+ Rendition executables from the English TACTCOM install
(see "Rebuilding the patched installs" below). None has been run or traced
yet. Treat a Voodoo device
model, the game-to-Glide interface and the eventual SDL3/GPU backend as three
different layers.

## Historical update located and fingerprinted

The [original GFX+ installation guide](https://www.tales-from-darkenedroom.com/post/ef2000-updated-part-2-installation)
links a [preserved patch collection](https://archive.org/details/ef2000-patches).
It includes this archive path (each layer is an actual ZIP entry):

```text
EF2000 patch collection
  TACTCOM patches.zip
    gfxplus.zip
      Tactpat.exe
```

Our local analysis used **only this updater and the original German game
files supplied by the user**, without modifying or redistributing either:

| Input | Size | SHA-256 |
| --- | ---: | --- |
| `TACTCOM patches.zip` | 9,702,046 bytes | `a713d2a37ab51ef03983085c20d9cf0941e23b633bcfe17542df7c1e3491bb15` |
| `gfxplus.zip` (inner archive) | 9,538,883 bytes | `733a4a28f756235eb28c7d4869d35be0b7d604b40829981046f9fcad3f0543f1` |
| `Tactpat.exe` | 14,131,200 bytes | `305d5ff54dec28e1826548edd7760bcd9f69cd7bc9a1b4e36d94272860a0b918` |

`Tactpat.exe` is a **PE32 i386 Windows executable**, not a DOS game executable.
Its PE sections contain an unusually large `DATA` region: raw file offset
`0x30800`, length `0xd3e600`, RVA `0x32000`, image base `0x400000`.
The historical guide says to apply the update in a Windows 9x VM to an
existing TACTCOM installation. A community [patch discussion](https://simforums.krishty.com/viewtopic.php?p=3547)
distinguishes English `Tactpat.exe` from a possible German `tacpatg.exe`.
The archived updater is the English one: it accepts only the English
`../EF2000-rdeprera` `_EF2000.EXE` (1,682,129 bytes) and rejects the German
reference.

## Rebuilding the patched installs

`DATA` holds three 16-bit Borland DOS patchers. The Windows front end writes
each one out as `_update.exe` and runs it. Each patcher's MZ image is
followed by its own patch data:

| Stage | Patcher MZ | Patch data | Input `_ef2000.exe` | Output `_ef2000.exe` |
| --- | --- | --- | --- | --- |
| `update to EF2000 V2.0` | `0x3a594c` | `0x3a8d6c`..`0xd6a7f9` | English TACTCOM, 1,682,129 bytes, CRC `13f406bc` | DOS V2.0, 1,675,537 bytes, CRC `7f0d6d3c` |
| `Tactcom 2.0 to 3DFX update` | `0x30d78` | `0x3331c`..`0x211f5f` | DOS V2.0 | GFX+ 3Dfx, 2,569,210 bytes, CRC `23587ec1` |
| `Tactcom 2.0 to Rendition update` | `0x211f64` | `0x214508`..`0x3a5942` | DOS V2.0 | GFX+ Rendition, 2,349,186 bytes, CRC `38bffb1a` |

The format was recovered from the 3DFX patcher's code. An 8-byte trailer
holds two u32 lengths: the opcode stream sits just before the trailer and
the argument stream just before that. There are 32 opcodes: open, close and
verify the target or source, rename, check for files, copy literal or source
bytes, and seek relative to the source. Every file has an expected size and
CRC-32 (polynomial `0xedb88320`, initial value 0, no final xor) before and
after patching.

`tools/didpatch.py` reimplements this format without running the original
patcher. It never writes into the source install, and it checks every
output against the patcher's own size and CRC. All three stages pass:

```sh
T=/path/to/Tactpat.exe
python3 -I tools/didpatch.py list "$T"
python3 -I tools/didpatch.py apply "$T" v20  ../EF2000-rdeprera local/gfxplus/3dfx
python3 -I tools/didpatch.py apply "$T" 3dfx ../EF2000-rdeprera local/gfxplus/3dfx
```

Use `rendition` in place of `3dfx` for the Vérité build, in a separate
output directory. The output directory holds only the files the update
creates or changes; unchanged files remain in the source install.

- **V2.0** rewrites `_ef2000.exe`, `diagnost.exe`, `did.dat` (53,124,034 ->
  40,484,862 bytes), `navig.nav`, `iff/256/gamopts.lbm` and `readme.txt`.
  It adds `config.dat` and new `cfg/`, `joystick/` and `missions/` files.
  The executable still uses X-32 (`B23X` at `0x28`) and identifies itself as
  `EF2000 V2.0 for DOS`, `$Revision: 3.32 $ by Russ on May 02 1997`.
- **GFX+ 3Dfx** replaces `_ef2000.exe`, `config.exe`, `diagnost.exe` and the
  `ef2000.exe` launcher, and adds `ef2000.pif` and two icons in `cfg/`. Its
  executable is a **Watcom LE binary under the CauseWay DOS extender** (MZ
  stub `0xb610` bytes, LE header at `0xb610`), not X-32. It identifies itself
  as `GFX+ 3DFX version $Revision: 7.13 $ by Russ on May 16 1997` and
  contains a statically linked Glide 2.1 (`grSstOpen`, the `FX_GLIDE_*`
  environment switches, `fxmemmap.vxd`).
- **GFX+ Rendition** has the same layout and adds the Vérité microcode
  `spd3d.uc`.

Rebuilt files belong in the ignored `local/` directory only. The front end
also writes `did1st.dat` and `ef2000.bat` from its own data; `didpatch.py`
does not reproduce those two files.

The rest of the patch collection uses the same patcher layout, and
`didpatch.py apply PATCH file SRC OUT` handles it directly:

- `TACTCOM patches/TACTPAT1.ZIP` (`tactpat1.exe`, serial/modem settings in
  `comms.cfg`) applies to the English `_ef2000.exe`.
- `TACTCOM patches/tactpgf.zip` (French/German modem fix) applies to the
  German reference install's `did1st.dat` only. The German `_EF2000.EXE`
  is unaffected.
- `EF2000 patches/` holds updates for the pre-TACTCOM release: `efpat24`
  (to v2.4, plus an already patched `_EF2000.EXE`) and `PATCHEF` (Midlife
  Upgrade 2.02). They don't apply to either TACTCOM install.

## Confirmed 3Dfx / Glide anchors inside the updater

This is **genuine string evidence of linked graphics code in patch data**,
not proof that a contiguous standalone game EXE can be carved. Two copies of
the Glide 2.1 version string occur in `DATA`, alongside original diagnostics
for 3Dfx card detection, hardware initialization, texture transfers and
GFX+ text rendering:

| String or diagnostic anchor | PE file offset | PE virtual address (for Ghidra) |
| --- | --- | --- |
| `Tactcom 2.0 to 3DFX update` | `0x3331c` | `0x434b1c` |
| `Tactcom 2.0 to Rendition update` | `0x214508` | `0x615d08` |
| `Glide(tm) Version 2.1` | `0x1512dc`, `0x1d0fd9` | `0x552adc`, `0x5d27d9` |
| `VOODOO_FILE` | `0x14a80b`, `0x1cf831` | `0x54c00b`, `0x5d1031` |
| `fxPCI for DOS` | `0x1ee620` | `0x5efe20` |

These byte positions are offsets inside the updater, which stores literal
runs from the patch data. They are **not** addresses in the patched
executable. Of the interior MZ headers, `0x30d78`, `0x211f64` and `0x3a594c`
are the three patchers. The others (`0x42525e`, `0x433524`, `0xc57701`) are
fragments inside the V2.0 patch data.

The port's X-32 loader and recovered guest addresses do not apply to the
CauseWay/LE GFX+ executables. Those need their own loader and a separate
Ghidra project. The DOS V2.0 executable is still X-32 and is close to the
English build, so it can serve as another cross-version reference.

## Reproducible read-only probe

`tools/gfxplus_probe.py` scans arbitrary user-provided MZ/PE binaries for
the specific known anchors, reports PE section mappings and checks possible
interior DOS MZ headers. It does **not** apply the update, execute any binary,
extract proprietary bytes or claim that text strings are callable functions.

```sh
# Caller supplies the archived historical updater, separately extracted:
python3 tools/gfxplus_probe.py /path/to/Tactpat.exe
python3 tools/gfxplus_probe.py /path/to/Tactpat.exe --json

# Control: the supplied, known unaccelerated German executable:
python3 tools/gfxplus_probe.py /path/to/EF2000/_EF2000.EXE

# Synthetic-only tests (also discovered by the Python suite):
python3 -m unittest tests.test_gfxplus_probe -v
```

Ghidra can import the installer as **Windows PE32** and navigate to those
virtual addresses as raw evidence. Its large `DATA` section contains patch
payloads, not automatically recognized x86 program functions. Do **not**
label the nearby bytes as the original `grSstOpen` function merely because
a diagnostic string mentions it. The final GFX+ game executable, once
available, needs a **separate version-specific Ghidra project**, independent
of the existing German X-32 decompilation corpus.

## Hardware interface and route to native 3D

The [EF2000 Reloaded technical guide](https://www.tales-from-darkenedroom.com/post/ef2000-updated-part-3-ef2000-reloaded-dosbox)
reports **statically linked Glide**, so the guest writes the original
hardware-facing commands itself instead of calling a replaceable
`GLIDE2X.OVL`. We can now corroborate the *presence of Glide 2.1 and Voodoo
diagnostic code in the updater*, but actual GFX+ guest callsites remain to
be measured. Glide calls, PCI configuration/VRAM mapping, 3Dfx register
writes, and the host SDL3 renderer are separate interfaces.

Two useful upstream implementation references are:

- [DOSBox-X 3Dfx configuration and low-level versus passthrough modes](https://dosbox-x.com/wiki/Guide%3ASetting-up-3dfx-Voodoo-in-DOSBox%E2%80%90X): the low-level device model is what the EF2000 path reportedly needs.
- [DOSBox-X Voodoo emulator source](https://dosbox-x.com/doxygen/html/voodoo__emu_8cpp_source.html) and [OpenGL backend source](https://dosbox-x.com/doxygen/html/voodoo__opengl_8cpp_source.html): register, triangle, framebuffer and texture operation semantics. These are **reference code**, not copied into this GPL port; observe their license when considering reuse.

The high-confidence next experiments are:

1. **Rebuild the GFX+ install** with `tools/didpatch.py` (done; see above).
   Keep it under `local/`. Load it in a separate Ghidra project as an LE
   executable.
2. **Trace first hardware access** in low-level Voodoo DOSBox-X: PCI vendor/
   device discovery, BAR assignments, memory-mapped register writes and reads,
   framebuffer and TMU writes. Capture addresses, widths, values, IPs and
   timestamps; cross-check against DOSBox-X's documented register semantics.
3. **Locate game-to-graphics calls** in Ghidra on the *actual patched output*:
   callers of card setup, triangle submission, buffer swaps, palette/texture
   upload, and overlay UI. Separate fixed Glide library internals from
   game-specific scene preparation. Correlate with the software renderer.
4. **Choose an implementation boundary** based on evidence. Higher-level
   reconstructed geometry/texture submission would enable a native SDL3 GPU
   / OpenGL / Vulkan backend; a full Voodoo MMIO emulator is a distinct and
   larger compatibility project. Test image fidelity and frame pacing in
   equivalent scenes before deciding.

No GPU acceleration, Voodoo device support or Windows runtime change is
included in bundle #063. The deliverable is the verified patch inventory,
stable analysis anchors, safe probe and bounded next-step plan.
