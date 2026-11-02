# GFX+ / 3Dfx renderer investigation (bundle #063)

The aim is to find a clean **EF2000 scene/geometry -> hardware renderer**
boundary for an eventual native accelerated renderer. This investigation is
static and read-only. We have **not** obtained a successfully patched, running
DOS GFX+ executable or traced its drawing commands yet. Treat a Voodoo device
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
The embedded data evidently contains distinct upgrade instructions for
**TACTCOM -> 3DFX**, **TACTCOM -> Rendition**, and **TACTCOM -> DOS V2.0**.
The last is supported by installer text, not yet by a diff of installed files.
The historical guide says to apply the update in a Windows 9x VM to an
existing TACTCOM installation. A community [patch discussion](https://simforums.krishty.com/viewtopic.php?p=3547)
reports that installation creates a separate `EF2000.EXE` in some versions;
we have not reproduced that transition. That discussion also distinguishes
English `Tactpat.exe` from a possible German `tacpatg.exe`. **Do not assume
the archived updater matches the supplied German patch level or language**;
keep the installation untouched and use a disposable copy if experimenting.

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

These byte positions are from the exact updater hash above. They are **not**
addresses inside the patched EF2000 executable: the patcher may embed delta
records, compressed content and multiple alternative output fragments.
Six plausible interior MZ headers exist at file offsets `0x30d78`,
`0x211f64`, `0x3a594c`, `0x42525e`, `0x433524` and `0xc57701`. DOS MZ
headers describe the *loadable DOS stub size*, not the total length of an
appended protected-mode executable. None of these candidates has yet been
validated as a complete standalone GFX+ game executable; blindly carving
to the next `MZ` would be incorrect.

The original unpatched German `_EF2000.EXE` has a `B23X` loader marker at
file offset `0x28`, but **none** of the detected 3Dfx/Glide anchors above.
The updater has no `B23X` marker. That is useful for triage, but **does not
prove** what loader, extender, or memory layout the post-patch game uses.
The port's existing X-32 loader and recovered guest addresses therefore
must not be reused for a candidate GFX+ executable without analysis.

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

1. **Get an installed GFX+ executable** from a properly licensed TACTCOM /
   Evolution + 3Dfx patch (or DOS v2.0 3Dfx installer), with SHA-256 and
   complete companion file list. Keep it outside the repository and run the
   probe. The current updater alone does not substitute for it.
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
