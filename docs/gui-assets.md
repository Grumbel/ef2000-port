# EF2000 GUI resources and asset extraction (bundle #061)

The game's menu is **not only hard-coded in the executable**. Its layout and
interactions are described in plain-text `.gui` resources inside the original
`DID.DAT` and `DID1ST.DAT` archives. Its background images are standard IFF
ILBM/PBM bitmaps, occasionally compressed by the archives' RA codec. The
existing 3D shape viewer [tfx3view](https://github.com/Grumbel/tfx3view)
contains a separate `tfxdat` archive extractor and can recover named members;
its graphical viewer and bundled 8×8 font are for displaying 3D models, not
for rendering EF2000's interactive menu hierarchy.

## What is confirmed from the supplied German TACTCOM installation

| Input | Count and characteristics | Relevance |
| --- | --- | --- |
| `DID1ST.DAT` | 580 hashed entries, 24 directory prefixes, 39 extension codes | Main menu scripts and 640×400 backgrounds |
| `DID.DAT` | 8,416 hashed entries, 23 prefixes, 59 extension codes | Another revision of GUI scripts, images, 3D/world data |
| Loose `.LBM` | 10 images; several 640×400, one 640×480, one 320×200 | Standalone images, HUD/screens and country panels |

An exhaustive decode of the installed assets recovered **517 PNGs**: 59
from `DID1ST.DAT` (including three PCX), 448 from `DID.DAT`, and ten loose
images. The sizes
include 320×200, 640×400, 640×480 and 800×600 plus several unusual partial
image dimensions. Many otherwise unidentifiable, RA-compressed archive entries
turn out to be IFF artwork; looking only for raw `FORM` headers would miss
most of them.

The archive index uses a four-byte little-endian offset at file position 0;
at that offset is a little-endian header containing a 16-bit file count,
directory-prefix count, extension count, and byte size of the name tables.
Directory and extension strings are stored as one-byte length followed by
ASCII bytes. The remaining index consists of **12-byte entries**:

| Offset within entry | Type | Meaning |
| --- | --- | --- |
| `+0` | signed LE32 | Pathname hash; entries sorted in signed order |
| `+4` | unsigned LE32 | Absolute byte offset in DAT |
| `+8` | unsigned LE32 | Stored byte length |

Original base filenames are **not** present in the index. Instead, paths are
encoded using a one-byte directory ID, ASCII base filename truncated to a
15-byte total, and (if space permits) one-byte extension ID. The original
EF2000/Tactcom rolling hash can then locate an entry; `tools/did_archive.py`
validates directory bounds, sorted hashes, extension tables and member lengths.
The recovered algorithm matches both original archive hash vectors for
`gui/mainmenu.gui` (`0xac04eb66` in `DID1ST.DAT` and `0xa110fd66` in
`DID.DAT`). The TFX/ADF/TAW formats have other version-dependent variants.

**RA** members begin with `RA`, version word `0x0200` and a 32-bit
decompressed size. `tools/ra_decode.py` implements the bounded original
LZ/Huffman byte and back-reference format, based on format research also
represented by tfx3view; it does not execute original code. Uncompressed
members are returned verbatim.

## Recovered screens and GUI definitions

| Archive | Virtual path | Evidence |
| --- | --- | --- |
| `DID1ST.DAT` | `iff/256/newgui03.lbm` | Hash `ab1e4e53`; 179,744 stored bytes; 640×400 ILBM, used as `picture newgui03` |
| `DID1ST.DAT` | `gui/mainmenu.gui` | 8,769-byte text script with 15 windows and 58 icons |
| `DID1ST.DAT` | `gui/archigh.gui` | Arcade/high-score window declarations |
| `DID.DAT` | `gui/mainmenu.gui` | Distinct 4,934-byte script version |
| `DID.DAT` | `iff/256/mainselc.lbm` | 59,063-byte RA member, decodes to 66,378-byte 320×200 ILBM |
| `DID.DAT` | `spr/pitfont.spr` | 385-byte RA member; 45 indexed 4×5 sprites with 16-bit pixel values |

The `gui/mainmenu.gui` script supplies the startup routine and main menu
background, then declares windows and icons by name. The declarative commands
include `window`, `icon`, `xpos`, `ypos`, `xsize`, `ysize`, localized `title`
and `text`, `select`/`unselect`/`call` event actions, `screen` transitions,
`openwindow`/`closewindow`, `key` shortcuts and `routine` callback names.
For example, the Quick Combat item transitions to `screen arcade`; the
campaign item invokes the native callback `gpwClickCampaign`. These named
callbacks overlap with the existing Ghidra function/callback inventory.

This data lets us derive a **window/icon hierarchy, menu navigation graph,
clickable bounds and callback map** without replaying every screen in Unicorn.
The scripts may contain shared declarations, conditions and commands that the
new importer does not yet evaluate. `inspect_script` retains directives in
source order with line numbers; it is not a complete GUI-script interpreter.

The `pitfont.spr` file is a **likely tiny cockpit font**, not yet identified
as the menu font. Its decoded 1,981-byte payload starts with an 8-bit count
and 45 little-endian 16-bit sprite offsets. Each sprite record contains an
8-bit width, 8-bit height, then `width × height` little-endian 16-bit
pixel/index values. `tools/spr_image.py` preserves those indices exactly in
JSON without inventing a font character map or colour palette.

## Extractor

All tools require original game data supplied by the caller; no images, GUI
scripts or archive bytes enter Git or the Nix build source. Extracted files
are deliberately created without overwriting existing output files.

```sh
# Asset inventory (read-only):
nix run .#gui-assets -- inventory /path/to/EF2000

# Named image and GUI script extraction:
nix run .#gui-assets -- extract /path/to/EF2000 \
  gui/mainmenu.gui gui/archigh.gui spr/pitfont.spr \
  iff/256/newgui03.lbm iff/256/mainselc.lbm \
  --out local/gui-assets

# Exhaustive image search, including unnamed hashed images (potentially large):
ef2000-gui-assets extract /path/to/EF2000 --all-images --out local/all-menu-images
```

The extractor creates separate `DID1ST/` and `DID/` subtrees to preserve
resources with the same virtual name in both archives. It writes original
`.gui` scripts and a sibling JSON representation of windows, icons and
directives. For IFF/PBM images it writes PNG with exact indexed pixels and
the image's palette. The same path handles 8-bit, 256-colour PCX images. An
extracted `.spr` collection is retained verbatim and also exported as an
index-preserving JSON sprite catalog. An output `manifest.json` records source archive, hash,
original and decoded sizes, and identified format. The image decoder uses
only the Python standard library: `BMHD`, `CMAP`, ByteRun1 `BODY`, 1–8
bitplanes, planar ILBM and 8-bit chunky PBM, plus transparency and PCX RLE.

For direct archive diagnostics without GUI conversion, run
`python3 tools/did_archive.py /path/to/DID1ST.DAT gui/mainmenu.gui`.
No original files are modified; the default `nix flake check` exercises only
synthetic test fixtures. The separate Nix check is
`nix build .#checks.x86_64-linux.gui-assets`.

## Native overlay using EF2000's visual style (planned)

There is already enough material to prototype a host-side EF2000 skin
without first implementing the whole DOS menu interpreter. The 517 extracted
images and `mainmenu.gui` window/icon geometry give us the visual and
structural starting point, but **not a complete widget theme**. We still
need to identify menu fonts, sprite/button states, reusable frame borders,
cursor imagery and palette rules against actual menu screenshots.

The proposed UI is a separate SDL3 compositor drawn after the guest image
and before presentation. It retains EF2000's bitmap frames, typography,
colours and icons, but provides **movable, overlapping and resizable windows**,
focus/Z-order, scrolling and gamepad navigation. First build an optional
diagnostic/settings panel without changing DOS state; later bind reconstructed
menu callbacks to native actions and replace individual guest menu screens.

Convert local PNG/JSON extractor output into a texture atlas plus skin
manifest under the user's asset directory; **do not commit or ship the
original artwork**. A procedural fallback skin enables testing without
original assets. With local assets available, golden screenshots can check
alignment, palette fidelity and scale against the existing guest renderer.

## Open questions

- **Fonts:** The inspected `.gui` scripts specify text and titles but not a
  separate font file. The directory tables contain bitmap/sprite-like
  extensions such as `.bbm`, `.abm`, `.pmi`, `.pmm` and `.spr`, not a
  conventional `.fnt` extension. Beyond the recovered `pitfont.spr`, we
  have **not yet identified** which glyph data the menu renders, whether
  it is embedded in the executable or another bitmap member,
  or the metrics/kerning behavior. The tfx3view HUD font is not a valid
  substitute without further verification.
- **Widgets:** The scripts describe interaction/layout, but the exact bitmap
  motifs for frame borders, buttons, icons, cursors, and palettes still need
  to be linked to archive resources and GUI renderer routines.
- **Remaining file types:** The extractor supports standard IFF ILBM/PBM and
  8-bit PCX but not every possible DID-specific `.bbm`/`.abm`/`.pmi` format.
  Unknown members remain available as raw bytes. A broader archive extract
  and live GUI-state trace are the next useful cross-checks.

This work is deliberately about recovering **original game content and GUI
semantics** for an eventual native implementation, not making Unicorn faster.
