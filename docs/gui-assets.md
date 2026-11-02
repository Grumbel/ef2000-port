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

### Recovered full-screen menu references

The original German installation also contains **already-rasterized menu
screens** among the 59 `DID1ST.DAT` images. The hashed members below are
reproducible image identifiers (not recovered filenames):

| Member | Resolution | Visible content |
| --- | --- | --- |
| `hash_94eb76bc.lbm` | 640×400 | English *In Game Options*, *Realism Menu*, *Fine Tune Options*, *Exit Options* |
| `hash_9dac76bc.lbm` | 640×400 | German *Spieloptionen*, *Realismus-Menü*, *Optionen zur Feineinstellung*, *Optionen verlassen* |
| `hash_950b6052.lbm` | 640×400 | German options and a *Bildschirm-Ausschnitt* window |

These are exceptionally useful **visual references** for the original
widget treatment: single-pixel silver edges, black offset shadows, stepped
olive-green title tabs, gray interiors, compact white bitmap text and tiny
square selection controls. **Most original menu text is baked into the
images**, with a smaller number of dynamic text exceptions. Consequently,
importing `.gui` layout descriptions alone will not yield a fully localized,
editable menu: its button art/text must also be identified and, if desired,
separated or redrawn. The rasterized screen bitmaps are **not proof**
that the runtime draws windows by blitting a matching frame sprite; the
`.gui` scripts include `box`, `bkd`, `boxs`, `bkds`, and `line` attributes
that likely drive a procedural renderer. In particular,
`iff/256/newgui03.lbm` is just the diagonal-textured background with a
diffuse highlight, **not a reusable window border**.

This pass used the English/German image pair to replace the host overlay's
blue/gold placeholder with asset-free gray/silver window chrome and olive
title tabs. It intentionally does **not** claim pixel-perfect fonts, menu
geometry or widget semantics. Original artwork is not committed or shipped.

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

### Original font specimen: `DID1ST/hash_33773001.png`

The original archive also contains a **320×200 font specimen image** at
`DID1ST.DAT` hash **`0x33773001`**, identified from the user's extracted
`DID1ST/hash_33773001.png`. Unlike the earlier options-screen reconstruction,
it shows glyphs **in character order**, with white proportional strokes on a
blue background. The first row runs ASCII `!` through `L` (33..76), the
second `M` through `w` (77..119), and the third begins with `xyz` (120..122)
before switching to accented glyphs. The mapping for the remaining accented
symbols and ASCII `{|}~` is **not established**; do not assign characters to
them by position alone.

`tools/menu_font.py` now takes three line rectangles directly from this
specimen, using empty columns and known sequential character codes to
recover **90 distinct printable glyphs** (ASCII 33..122, including all
digits, A–Z and a–z). A double quote consists of two disconnected strokes;
the extractor explicitly merges those two columns rather than treating them
as independent characters. Typical glyph widths range from one to seven
pixels, with two blank columns between characters. The older 640×400 options
screen samples are retained as independent references for later comparison,
but they are no longer the extraction source. All 41 glyphs shared with those
earlier options-screen samples have matching widths; the masks are **not all
pixel-identical** (only 12 match after ignoring blank rows). The specimen
therefore establishes a real font style, but not necessarily every menu
label's exact rendered raster.

To use the recovered font as an optional SDL3 overlay font:

```sh
ef2000-gui-assets font /path/to/EF2000 --out local/menu-font
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed \
  --overlay-font-bmp local/menu-font/menu-font.bmp
```

The extractor writes a magenta-keyed 128×72 bitmap containing 8×12 cells
for ASCII 32..126, an identical PNG for inspection, a plain text `.widths`
table (95 values), and an evidence/coverage JSON. The SDL3 overlay loads
the bitmap and corresponding widths table, preserves proportional letter
spacing and falls back to debug text for the four unmapped ASCII characters
`{|}~` (space has width zero). The JSON records source hash, coverage and
sample count. All derived pixels remain outside Git and the Nix output.

The specimen is a **rendered image**, not proof of a standalone binary font
format. The underlying glyph storage and rendering routine are still unknown; a
Ghidra analysis of the GUI text blitter would establish glyph metrics,
available character sets, shadowing and color/palette behavior.

### Symbols from the same specimen

This 320×200 test screen also carries **two rows of seven 16×16 symbols**:
up/down/left/right arrows, a cross, overlapping rectangles and corner marks.
The upper row uses light strokes; the lower uses dark ones. A pointer-like
drawing straddles the right-hand cell boundary, and the bottom of the image
contains three small aircraft and another sprite. The latter artwork is
**not evidence of menu cursor art** and is not presently extracted as GUI
controls. Similarly, the two-row symbols have not yet been traced to a GUI
callback or button definition; the command below records only the actual
pixels, plausible visual names, and coordinates.

```sh
nix run .#gui-assets -- symbols /path/to/EF2000 --out local/menu-symbols
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed \
  --overlay-symbols-bmp local/menu-symbols/menu-symbols.bmp
```

`tools/menu_symbols.py` exports a 112×32 BMP with a magenta transparency key,
an RGBA PNG and JSON cell coordinates. It preserves original palette colors
for the 14 grid cells and omits the overlapping pointer rather than slicing
it into misleading fragments. The optional SDL3 overlay uses the dark cross
as its window-close icon, without changing click hit-testing; the ordinary
text `X` remains the default when the BMP is not supplied. Original graphic
bytes and their converted output are **never included in Git or Nix**.

The remaining specimen rows visually contain **48 additional accented
glyphs** after `xyz`, in 42 + 6 segmented columns. Their codepoint order is
not yet established, so the current atlas keeps these separate from the
verified ASCII mapping. Resolving at least the German `äöüÄÖÜß` set is a
useful follow-up before adding native localized overlay labels.

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
# Or create a JavaScript-free contact gallery at extraction time:
ef2000-gui-assets extract /path/to/EF2000 --all-images --gallery \
  --out local/gui-gallery
# For an extraction already on disk:
ef2000-gui-assets gallery local/all-menu-images
```

The extractor creates separate `DID1ST/` and `DID/` subtrees to preserve
resources with the same virtual name in both archives. It writes original
`.gui` scripts and a sibling JSON representation of windows, icons and
directives. For IFF/PBM images it writes PNG with exact indexed pixels and
the image's palette. The same path handles 8-bit, 256-colour PCX images. An
optional `--bmp` additionally exports 24-bit BMP copies for SDL3's built-in
bitmap loader. Masked pixels are flattened onto black in BMPs; the normal
PNG retains palette indices and transparency for archival use. An
extracted `.spr` collection is retained verbatim and also exported as an
index-preserving JSON sprite catalog. An output `manifest.json` records source archive, hash,
original and decoded sizes, and identified format. The image decoder uses
only the Python standard library: `BMHD`, `CMAP`, ByteRun1 `BODY`, 1–8
bitplanes, planar ILBM and 8-bit chunky PBM, plus transparency and PCX RLE.
The offline `gallery.html` lists decoded images smallest-first with image
sizes, archive sources, and original virtual paths or hashes. It uses PNG
thumbnails with links to the full images, without JavaScript or network
access; as with the other exports, an existing gallery is never overwritten.

For direct archive diagnostics without GUI conversion, run
`python3 tools/did_archive.py /path/to/DID1ST.DAT gui/mainmenu.gui`.
No original files are modified; the default `nix flake check` exercises only
synthetic test fixtures. The separate Nix check is
`nix build .#checks.x86_64-linux.gui-assets`.

## Native overlay using EF2000's visual style (in progress)

There is already enough material to prototype a host-side EF2000 skin
without first implementing the whole DOS menu interpreter. The 517 extracted
images and `mainmenu.gui` window/icon geometry give us the visual and
structural starting point, but **not a complete widget theme**. We still
need to identify the source binary font format, sprite/button states, reusable frame borders,
cursor imagery and palette rules against actual menu screenshots.

The host-side overlay now draws after the guest image and before presentation,
with **movable, overlapping, resizable windows**, focus/Z-order, close/reopen
and scroll. The first two panels contain keyboard/gamepad help and a live
performance monitor; F12 toggles them while F11/Alt+Enter changes fullscreen
state. The interaction model lives in `runtime/overlay_windows.hpp` and has
an asset-free test. The procedural default now follows the observed
gray/silver/olive colors and stepped title tabs: the recovered **EF2000
font specimen can now be loaded via `--overlay-font-bmp`**, while original
icon artwork still has to be identified and mapped.
Later bind reconstructed
menu callbacks to native actions and replace individual original GUI screens.

`--overlay-frame-bmp FILE` and optional `--overlay-frame-border N` (1..32)
now let the runtime draw a **caller-selected BMP crop** using nine-slice
geometry, preserving corners while resizing each window. The shared geometry
is tested independently in `runtime/nine_slice.hpp`. This is not automatic
frame discovery: `newgui03.lbm` is a background only, and a clean independent
frame texture has not yet been identified. The original frames may be drawn
by the engine from the `.gui` box and background directives.
Use `ef2000-gui-assets extract ... --bmp --out local/gui-art` to prepare
candidate images and keep all derived artwork outside the repository.

Convert local PNG/JSON extractor output into a texture atlas plus skin
manifest under the user's asset directory; **do not commit or ship the
original artwork**. A screenshot-informed procedural fallback skin enables testing without
original assets. With local assets available, golden screenshots can check
alignment, palette fidelity and scale against the existing guest renderer.

## Open questions

- **Font internals:** The inspected `.gui` scripts specify text and titles but not a
  separate font file. The directory tables contain bitmap/sprite-like
  extensions such as `.bbm`, `.abm`, `.pmi`, `.pmm` and `.spr`, not a
  conventional `.fnt` extension. The `hash_33773001` rendered specimen does
  provide 90 glyph shapes and observed widths, but **not their underlying
  binary storage, complete accented-character mapping, or original glyph
  rendering routines**. The tfx3view HUD font is not a valid substitute
  without further verification.
- **Widgets:** The scripts describe interaction/layout, but the exact bitmap
  motifs for frame borders, buttons, icons, cursors, and palettes still need
  to be linked to archive resources and GUI renderer routines.
- **Remaining file types:** The extractor supports standard IFF ILBM/PBM and
  8-bit PCX but not every possible DID-specific `.bbm`/`.abm`/`.pmi` format.
  Unknown members remain available as raw bytes. A broader archive extract
  and live GUI-state trace are the next useful cross-checks.

This work is deliberately about recovering **original game content and GUI
semantics** for an eventual native implementation, not making Unicorn faster.
