# EF2000 Linux port

Behavior-preserving port of the original DOS game to Linux. Preserve the original
game instructions and data; replace DOS and hardware interfaces incrementally.
Emulation and binary translation are intermediate tools. Full decompilation into
maintainable source is the long-term goal.

## Status

The two-stage FlashTek X-32 loader, segment relocation table, initialized image,
BSS boundaries, and protected-mode entry have been recovered by static analysis.
The extractor reproduces the static relocation passes. A native x86-64 Linux
harness executes unchanged original 32-bit code through startup and into the
original **main menu and flight**, displayed in an SDL window with mouse,
keyboard, and joystick input. It handles
DOS services, memory allocation, timers, VGA/VBE banked graphics, and BIOS calls.
Unicorn executes real-mode driver requests. Audio is disabled in a local config
copy by default. Broader gameplay and mission completion remain unverified.
An experimental Unicorn backend also runs the original extracted 32-bit
instructions in a portable CPU emulator. It reaches the original main menu
and flight cockpit and supports SDL3 input and rendering, but is much slower
than native execution and is not yet a fully playable Windows replacement.
The first target is the installed `_EF2000.EXE` in the sibling
`../EF2000/c/EF2000/` directory. Other releases need separate fingerprints.

## Development

On x86-64 Linux, build and run with Nix:

```sh
nix build -L .                 # Precompile the native and QEMU harness objects
nix flake check                # Unit tests, app/help/version checks, REUSE
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE --output local/nix-startup
nix develop                    # Python, GCC/binutils, QEMU, DOSBox-X, GDB, Ghidra
```

Inside `nix develop`, the following development commands are available:

```sh
ef2000-configure                       # CMake/Ninja Debug harness in /tmp/ef2000-build
ef2000-build                           # configure if missing/moved, then build objects
ef2000-run /path/to/_EF2000.EXE        # build then run, using those objects
ef2000-run-gdb /path/to/_EF2000.EXE    # build then debug native startup.elf in GDB
nix develop -c ef2000-build            # also works without an interactive shell
nix build .#ef2000-win64-zip           # Windows x64 archive (C++ Unicorn/SDL3)
nix build .#ef2000-win32-zip           # Windows x86 archive
ef2000-ghidra import /path/to/_EF2000.EXE
ef2000-ghidra decompile 0xe9a24 0x8b274
ef2000-ghidra inventory               # all discovered functions and code gaps
ef2000-ghidra export-all --custom-storage-fallback # full pseudo-C corpus
ef2000-ghidra function-facts          # export guest data references and jumps
ef2000-ghidra subsystem-map --seed 0x4aa0 # rank related routines (read-only)
ef2000-ghidra recover-orphans         # review referenced missing functions (dry run)
ef2000-ghidra callback-tables         # extract verified named dispatch tables
ef2000-ghidra name-callbacks          # audit names (dry run)
ef2000-ghidra audit-flow-gaps         # inspect truncated function bodies
ef2000-ghidra audit-body-holes 0x9820 # inspect multi-span function holes
ef2000-ghidra repair-boundaries       # audit nine guarded recipes (dry run)
ef2000-highscores-differential /path/to/_EF2000.EXE --random-cases 300
                                       # original Unicorn guest versus native C++
ef2000-gui-assets inventory /path/to/EF2000
ef2000-gui-assets extract /path/to/EF2000 gui/mainmenu.gui \
  iff/256/newgui03.lbm spr/pitfont.spr --out local/gui-assets
ef2000-gui-assets extract /path/to/EF2000 --all-images --gallery \
  --out local/gui-contact-gallery # open gallery.html locally
ef2000-gui-assets font /path/to/EF2000 --out local/ef2000-menu-font
python3 tools/gfxplus_probe.py /path/to/Tactpat.exe # examine GFX+ updater (read-only)
ghidra                                  # GUI (also: nix run .#ghidra)
```

Without entering the shell, use
`nix run .#ghidra-ef2000 -- import /path/to/_EF2000.EXE`
(or another subcommand). These Nix wrappers default
to the writable `./local/ghidra` directory and keep Ghidra outside the
main game package. Set `EF2000_GHIDRA_WORKSPACE` or pass `--workspace`
to choose a different project. `nix flake check` also runs the asset-free
Ghidra workflow tests; the original executable is never included in Nix
builds.

High-score differential replay is also available without the shell via
`nix run .#highscores-differential -- /path/to/_EF2000.EXE`. It runs selected
original routines in isolated Unicorn guest memory with controlled file and
random-number services, comparing their state and file bytes against the
typed native C++ reconstruction. This workflow requires the caller's original
executable; the asset-free fixture checks run under `nix flake check`.
See [high-score reconstruction](docs/highscores-reconstruction.md) for verified
addresses, behavior and remaining decompilation work.

The original menus also have extractable artwork and declarative layouts:
`DID.DAT` and `DID1ST.DAT` contain hashed game resources, including ILBM/PBM
and PCX graphics, `.gui` window/icon scripts, and sprites. Run
`nix run .#gui-assets -- inventory /path/to/EF2000` to inspect an installation
or `nix run .#gui-assets -- extract ...` to export PNGs and JSON menu
structures. The original files stay untouched, and `nix flake check` uses
only synthetic fixtures. See [GUI resource research](docs/gui-assets.md).

### Related releases, editors and modding research

For historical EF2000 variants and prior art, see
[existing tools, mission files and 3Dfx release research](docs/existing-tools-and-releases.md).
The focused [GFX+ / 3Dfx interface investigation](docs/gfxplus-rendering.md)
fingerprints the historical patch updater, maps embedded Glide diagnostic
anchors and provides a synthetic-tested read-only binary probe. No patched
3Dfx game executable or accelerated renderer is part of the port yet.
In particular:

| Reference | Why it matters |
| --- | --- |
| [tfx3view / tfxdat](https://github.com/Grumbel/tfx3view) | Existing DID archive extraction and 3D geometry viewer; distinct from the menu/mission scripts. |
| [EFUTIL discussion](https://simhq.net/forum/ubbthreads.php/topics/3835020/re-ef2000-reloaded), [ReViVe](https://www.combatsim.com/memb123/archive/htm/htm_arc2/evolute.htm), [Strategic Commander](https://simhq.net/forum/ubbthreads.php/topics/3458818/3) | Legacy campaign aircraft-swapping, restoration and scenario editing tools. **EFUTIL is not a mission extractor.** |
| [EF2000 Reloaded](https://simhq.net/forum/ubbthreads.php/topics/3830314/all/EF2000_Reloaded) and [3Dfx installation notes](https://www.tales-from-darkenedroom.com/post/ef2000-updated-part-3-ef2000-reloaded-dosbox) | Specialized DOSBox Voodoo emulation for DOS **GFX+**; the statically linked graphics library prevents straightforward Glide-wrapper interception. |
| [rdeprera/EF2000](https://github.com/rdeprera/EF2000) | Another DOS v2.0 TACTCOM installation to compare, **not yet confirmed to be English**; its executable and some archive members differ from our German reference. |

Our German installation already includes **plain-text** `MISSION.CFG`
(746 mission blocks), `NAVIG.NAV` (1,118 navigation records), and campaign
files. The [read-only mission parser](docs/mission-records.md) now validates
the record/route counts and can export individual missions and navigation
blocks as JSON while retaining unknown directives and source line numbers.
Try `python3 tools/mission_records.py /path/to/EF2000/MISSION.CFG --block MISSION:516`.
A general mission editor is not necessary yet. The original game files
remain external to the port and its Nix build.

`EF2000_SOURCE` defaults to the directory from which `nix develop` was entered;
`EF2000_BUILD_DIR` defaults to `/tmp/ef2000-build` and can be overridden.
`CMAKE_BUILD_TYPE` defaults to `Debug`; the regular Nix package remains optimized.
The configure command creates a `build` symlink if that path is free. The
development build contains freestanding native64/QEMU harness objects, not the
copyrighted game executable. Run commands accept the same game and diagnostic
options as `ef2000`; they build the harness before launching it.
`ef2000-run-gdb` starts GDB on the generated native `startup.elf` directly,
with Python running outside GDB to service the guest's DOS, BIOS and video
requests. GDB runs the inferior automatically, quits on a normal zero exit,
and remains interactive on crashes or nonzero exits. The ELF and its logs live
in the run's output directory. `--gdb` is also available on the Python runner
for `native64` with Unicorn real-mode services; QEMU is not supported.
The shell greeting lists these commands, usage examples and the active paths.

`packages.default` and `packages.ef2000` are the same native package;
`packages.ef2000-startup` is an alias. `apps.default` and `apps.ef2000` run the
native `ef2000` launcher; `ef2000-startup` remains available as a command
alias. `apps.unicorn` (also `apps.ef2000-unicorn`) launches the experimental
SDL3/Unicorn window, while `apps.unicorn-probe` retains its bounded headless
diagnostic behavior.
`packages.ef2000-win64-zip` and `packages.ef2000-win32-zip` cross-compile the
C++ Unicorn/SDL3 frontend with MinGW-w64 and package Windows EXE/DLLs;
they do not contain the proprietary game files. See the
[Windows build and usage guide](docs/windows.md). The Python and native64 Linux
backends are not part of these Windows archives.
Builds require no original game files. The launcher accepts your external
executable, creates a new local output directory, and links it with the
precompiled harness. The executable's parent directory supplies the original
game assets, opened read-only. Configuration and other writes go under
`OUTPUT/drive/`. Close the window to stop; its final framebuffer is retained as
`OUTPUT/framebuffer.png`. Use a fresh output path for each explicit `--output` run.
Without `--output`, a unique diagnostic run directory is created under
`$XDG_CACHE_HOME/ef2000/runs/`. To launch from the installed desktop entry,
place the original `_EF2000.EXE` and its companion data files in
`$XDG_DATA_HOME/ef2000/`, or specify `--data-dir DIR` on the command line.
The desktop entry does not ship the commercial game files.

The Linux install includes an `ef2000.desktop` launcher, AppStream metadata,
hicolor icons, and a section 6 man page. `--show-paths` works without the game
assets or runtime dependencies and prints the resolved directories and their
sources. `-f`/`-w`, `-g WxH`, `--mute`, and `-d` follow gamepkgstd conventions;
the existing backend, sound, VR, joystick, and diagnostic options remain.
The window defaults to **1280×960**. `--aspect-ratio 4:3` (default) displays
the original image at a 4:3 CRT-like aspect ratio, `native` preserves its
square-pixel dimensions (e.g. 640×400 becomes 8:5), and `stretch` fills the
window. `--smooth-scaling` enables linear filtering instead of the default
nearest-neighbour scaling; `--no-smooth-scaling` selects the sharp setting.
Both options can also be changed live in the F10 overlay.

User configuration is read from `$XDG_CONFIG_HOME/ef2000/EF2000.CFG` if present,
falling back to the original game file. User save states live under
`$XDG_STATE_HOME/ef2000/` (default `~/.local/state/ef2000/`). `--config-dir`,
`--state-dir`, `--user-data-dir`, and `--cache-dir` override their respective
locations independently. `--portable` (or `portable.txt` next to the supplied
game executable) uses its sibling `userdata/{config,state,data,cache}`.
`--no-save` disables persistent save-state access for that run.

For a bounded run without a window:

```sh
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/menu-capture --headless --seconds 25
```

Python 3 is sufficient for the analysis tools. Direct startup development also
requires GCC/binutils, Unicorn, and SDL3 (provided by `nix develop`); QEMU and
DOSBox-X are used by comparison/reference tools:

```sh
python3 tools/inspect_exe.py ../EF2000/c/EF2000/_EF2000.EXE
python3 tools/extract_image.py ../EF2000/c/EF2000/_EF2000.EXE --output local/image-01
python3 -m unittest discover -s tests -v
# Include checks against the fingerprinted original:
EF2000_EXE=../EF2000/c/EF2000/_EF2000.EXE python3 -m unittest discover -s tests -v
# Execute original startup (x86-64 Linux, GCC/binutils; no 32-bit libc required):
python3 tools/run_startup.py ../EF2000/c/EF2000/_EF2000.EXE --output local/startup-01
```

### Experimental portable Unicorn CPU backend

The protected-mode Unicorn backend runs independently of Linux syscalls, ELF
execution, `modify_ldt`, GCC and binutils. It requires Python 3 and Unicorn
2.x. The flake provides a windowed experimental app alongside the original
headless diagnostic probe:

```sh
# Launch the original menu/flight with SDL3 (experimental and slow):
nix run .#unicorn -- --data-dir /path/to/EF2000
# Equivalent: nix run .#ef2000-unicorn -- /path/to/_EF2000.EXE
# Continue without a window for a limited diagnostic run:
nix run .#unicorn -- --data-dir /path/to/EF2000 --headless --instruction-limit 30000000
# Low-level headless probe, unchanged:
nix run .#unicorn-probe -- /path/to/_EF2000.EXE
# The probe can still show a window explicitly:
nix run .#unicorn-probe -- /path/to/_EF2000.EXE --windowed --geometry 1280x960
# Direct Python runner on Windows with `pip install unicorn`:
python tools/unicorn_backend.py C:\EF2000\_EF2000.EXE --report startup.json --overlay-dir C:\EF2000-user\drive --cd-dir D:\
# Deterministic headless mouse press/release while probing the VGA menu:
python tools/unicorn_backend.py C:\EF2000\_EF2000.EXE --mouse-event 70:320:100:1 --mouse-event 78:320:100:0
# Export the final framebuffer without opening a window:
python tools/unicorn_backend.py C:\EF2000\_EF2000.EXE --framebuffer startup.png
```

The new windowed app discovers the executable under `--data-dir`, from its
positional path, or in `$XDG_DATA_HOME/ef2000/`. It uses
`$XDG_STATE_HOME/ef2000/unicorn/drive` as a separate writable DOS overlay
unless `--overlay-dir` is set. Original game data are never modified. Use
`--state-dir`, `--user-data-dir`, `--portable`, or `--show-paths` to manage
locations. Window close returns success; unexpected guest stops remain errors.
The windowed app defaults to 100,000 guest instructions per PIT tick (matching
the original-asset flight replays); the bounded probe retains 250,000.
Expect lengthy startup and slower-than-real-time gameplay: this is still a
development interface, not the default native backend.
The windowed Python Unicorn app now defaults to `--fast-execution`, so the
startup and video event loop do not spend minutes in Python instruction hooks.
The headless diagnostic probe keeps precise execution by default; pass
`--fast-execution` to opt in or `--precise-execution` to turn batching off.
Batching runs up to 1,000 guest instructions between host callbacks,
while keeping PIT, window, progress and framebuffer sampling boundaries.
Interrupt returns are watched at their original resume addresses. The
timing within each batch is approximate; exact mouse/decompressor tracing
cannot be combined with the fast mode. Use `--fast-quantum N` to reduce the
maximum timing error, or omit `--fast-execution` for the original instruction-
hook runner. A 10M-instruction original-asset benchmark dropped from 5.56 to
0.39 seconds, and a 820M replay reached the flight cockpit in 38.3 seconds,
with 8,090 protected timer interrupts. Neither result establishes real-time
playability or equivalence of every flight simulation state.

### Native C++ Unicorn migration

The new `ef2000-unicorn-cpp` binary executes the original X-32 image directly
through Unicorn's **C API**, without launching a Python interpreter. The
original executable remains external and is SHA-256 checked before loading.
The C++ program now owns the ring-3/LDT bootstrap, early X-32 memory and
selector calls, protected vector services and IRQ0/PIT timing, keyboard
controller and BIOS key services, read-only DOS asset access, the mouse and
gameport joystick, and the banked VBE BIOS. It also provides a separate
copy-on-write DOS drive for game-created files; original assets are never
opened for writing. Installed original 16-bit DOS drivers can now run through
a separate bounded C++ Unicorn CPU, with their register and conventional-memory
changes returned to the X-32 caller. It captures and displays the
original VGA 320×200 and VBE 640×400 palette-indexed frames through SDL3.
The initial black screen is replaced with a startup message until the guest
draws its own graphics. Windowed runs have no instruction limit unless one is
explicitly requested; close the window to stop emulation.

```sh
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed --geometry 1280x960 \
  --aspect-ratio 4:3 --smooth-scaling
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed \
  --overlay-dir /path/to/writable-state
# Sound-hardware discovery only; does not yet play digital audio
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --sound sb16-probe \
  --instruction-limit 100000000
# Experimental DMA/PIC/IRQ emulation with SDL3 PCM output in windowed runs
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed --sound sb16-dma
# Optional original-game memory-copy acceleration (experimental)
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed --sound sb16-dma --fast-copy
# Real-time PIT scheduler (automatically selected for unlimited windowed runs)
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed \
  --instruction-limit 0 --timing realtime
# Original CD music alongside SB16 effects (CUE and BIN remain read-only)
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed --sound sb16-dma \
  --cd-dir /path/to/EF2000_ISO --cd-cue /path/to/EF2000_ISO/EF_DIR.cue
# Flight-controller diagnostics; SDL3 gamepads use mapped stick axes (not triggers)
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed --sound sb16-dma \
  --controls joystick --joystick auto --trace-joystick
# Expose a classic two-axis PC joystick instead of four analog axes:
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed --joystick-axes 0,1
# Capture the original guest DMA audio without an SDL3 sound device
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --sound sb16-dma \
  --instruction-limit 300000000 --wav-capture /tmp/ef2000-pcm
# Capture CD music separately as /tmp/ef2000-audio/cd-audio/audio-NNN.wav
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --sound off \
  --cd-cue /path/to/EF2000_ISO/EF_DIR.cue --wav-capture /tmp/ef2000-audio
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --instruction-limit 30000000 --timer-instructions 100000
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --instruction-limit 30000000 --framebuffer /tmp/ef2000-vga.ppm
# Reproducible instruction-counted debugger mode, including in a window:
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed \
  --instruction-limit 30000000 --timing instruction
# Bounded real-time/headless run for wall-clock pacing diagnostics:
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --timing realtime --wall-seconds 10
nix develop -c ef2000-build --target ef2000-unicorn-cpp
# The same executable has a self-contained synthetic CPU test:
nix run .#unicorn-cpp -- --self-test
# Also tests a live X-32 DOS-memory selector and BIOS CRT-base lookup:
nix run .#unicorn-cpp -- --self-test-bda
nix run .#unicorn-cpp -- --self-test-clock
```

The C++ runner was independently replayed from the original assets through
the **Digital Image Design splash**, **main menu**, **Quick Combat mission
selection**, loading animation and **in-flight cockpit**, using timed mouse
events and a 920-million-instruction bound. Its final frame comes directly
from the original game's 320×200 framebuffer; the complete reproduction
command and observed checkpoints are in
[execution-probes.md](docs/execution-probes.md). SDL3 mouse, keyboard and
joystick events are forwarded to the guest, and the CLI supports scripted
`--mouse-event`, `--key-event` and `--joystick-event` inputs. `--fullscreen`,
`--geometry`, `--aspect-ratio` and `--smooth-scaling` control SDL3 output.
With sound enabled, independent 1.5-billion-instruction replays also reached
the in-flight cockpit, completing 287–298 guest DMA blocks
(585,730–608,258 PCM bytes; the DSP service uses host wall-clock timing).

Unlike `--cd-dir`, which only exposes files to the guest DOS drive,
`--cd-cue FILE` also installs the read-only MSCDEX CD-audio interface and
enables the game's Red Book music setting. The original `NULL.MDI` driver
replaces the FM synthesizer in its virtual configuration. The C++ runner
accepts one BINARY CUE image containing indexed MODE1/2352 and AUDIO tracks;
`--cd-dir` can be combined with `--cd-cue` when the installer files reside
alongside the disc image. Music and SB16 effects use two streams bound to one
SDL3 playback device, with independent buffering and rate conversion. Without
an SDL device, `--wav-capture DIR` can save CD samples in `DIR/cd-audio/`,
including when `--sound off`. See [CD audio](docs/cd-audio.md) for request
details and known limitations. Actual original-disc playback on C++ SDL3 still
needs a listening test; the emulator path has synthetic protocol/PCM tests.

The C++ SDL3 frontend recognizes standardized gamepads, avoiding the raw
trigger-axis ordering seen on some controllers. The experimental **flight
profile** maps left stick to roll/pitch, right stick X to the fourth DOS
gameport axis (expected rudder) and right stick Y to a **latched, rate-driven
throttle** on the third DOS axis: push up/down to change thrust; returning
to center keeps its setting, initially 50%. The third/fourth-axis assignments
still need an original-game flight test: they are **not yet proven** to drive
throttle and rudder in every EF2000 control mode. The first two axes and all
four gameport buttons retain their original semantics.

Holding **LB** while pressing A/B/X/Y synthesizes keyboard **G** (gear),
**B** (air brake), **E** (ECM), **R** (radar). Holding **RB** maps A/B/X/Y to
**C** (next target), **Enter** (air weapon), **Backspace** (ground weapon),
**F4** (external view). Unmodified face buttons remain DOS gameport buttons;
LB wins if both shoulders are held. Hold and release events are sent as
real guest keyboard make/break transitions. **Back** on a standardized
gamepad or **F12** toggles the native overlay; it does *not* pause the guest.

The SDL3 overlay now contains two host-side windows: **Flight Controls**
(scrollable keyboard/gamepad command reference) and **System Monitor**
(live SDL/changed-image FPS and guest activity). Drag their titlebars,
resize at the bottom-right corner, click `X` to close, or click a window
to bring it forward. While the overlay is open, mouse/keyboard input is
consumed by it instead of the DOS game, and the physical gamepad is held
neutral. The windows can overlap, remember their position and size while
hidden, and reopen from the overlay's keyboard commands. The procedural
gray/silver frame and stepped olive title tabs are based on actual **English
and German EF2000 menu bitmaps** recovered from `DID1ST.DAT`; this is a
reconstruction, not a direct rendering of the original GUI code.

The `gui-assets` extractor can generate a self-contained, offline HTML
gallery of all recovered images. This is a useful way to find and compare
original menu artwork. Most of the original GUI's text is baked into these
bitmaps; only some text is generated dynamically:

The original green title bars and gray panel interiors have also been
identified as **screen-coordinate crops** of full-screen textures. To use
these in the movable F12 overlay, keeping original copyrighted assets local:

```sh
nix run .#gui-assets -- skin /path/to/EF2000 --out local/gui-skin
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed \
  --overlay-skin-dir local/gui-skin
```

The extractor additionally saves original terrain and a colored unit/flag
atlas for further research. See [GUI resource research](docs/gui-assets.md)
for exact archive hashes and the transparency index caveat.

```sh
nix run .#gui-assets -- extract /path/to/EF2000 \
  --all-images --gallery --out local/gui-art
# Open local/gui-art/gallery.html in a browser (no JavaScript or network).
```

The original `DID1ST.DAT` also contains a **320×200 font specimen**, identified
as `hash_33773001.png` by the extractor. It includes glyphs in character
order: the full ASCII range `!` through `z` (90 characters), including
digits, uppercase and lowercase letters, followed by accented characters.
To generate a proportional, native-resolution font atlas privately from your
original archives:

```sh
nix run .#gui-assets -- font /path/to/EF2000 --out local/menu-font
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed \
  --overlay-font-bmp local/menu-font/menu-font.bmp
```

The SDL3 overlay reads the adjacent `menu-font.widths` file and uses the
original glyph widths and two-pixel letter spacing. Unsupported characters
(`{|}~` have not been identified) fall back to SDL3's built-in debugging font.
The font is recovered directly from the **rendered original specimen**, not
from the menu screenshots, but its underlying binary glyph format remains
unidentified. The extractor records the source hash and coverage in JSON.
It additionally writes `menu-font-extended.png`,
`menu-font-extended.json` and a JavaScript-free
`menu-font-extended.html` gallery for the **48 remaining accented glyphs**.
They are numbered by source position, **not assigned speculative Unicode
values**. Open the HTML locally to identify characters for a later extended
font mapping. Neither the bitmaps nor their derived widths are committed to Git.

The same original font specimen contains small **directional and window-like
symbols** in light/dark variants. An extractor recovers the 14 neatly
aligned cells (without guessing the identity of overlapping drawings) as an
RGBA PNG, a magenta-keyed BMP and a JSON cell catalog. To use the original
dark cross in the movable overlay's window-close buttons:

```sh
nix run .#gui-assets -- symbols /path/to/EF2000 --out local/menu-symbols
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed \
  --overlay-symbols-bmp local/menu-symbols/menu-symbols.bmp
```

This is verified specimen artwork, but its role in the DOS GUI has not yet
been established. No copyrighted image bytes are distributed with the port.

If you can isolate a clean, reusable **frame** region, the extractor can
additionally export BMPs for SDL3 (with `--bmp`). A carefully cropped BMP
can be supplied as a nine-slice texture:

```sh
nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed \
  --overlay-frame-bmp local/ef2000-frame.bmp --overlay-frame-border 12
```

The runtime preserves bitmap corners while stretching edges and centre to
each movable window. The frame crop and border thickness must match the
original artwork; `newgui03.lbm` is a full-screen textured background,
**not** a window-frame source. The original game's frame edges may be drawn
procedurally rather than stored as independent bitmaps.
Neither the BMP nor any derived crop is included in Git or a Nix package.
Without an external frame bitmap, the procedural fallback remains available.

Host shortcuts in the C++ SDL3 frontend:

| Shortcut | Action |
| --- | --- |
| **F12** / gamepad Back | Show/hide the movable overlay |
| **F11** / **Alt+Enter** | Toggle fullscreen/windowed mode |
| **Tab** (overlay open) | Focus next window |
| **H / D** (overlay open) | Reopen help or diagnostics panel |
| **Up/Down, PageUp/PageDown, mouse wheel** | Scroll the focused help panel |
| **Escape** (overlay open) | Close overlay |
| **F10** | **Passed through to EF2000 while overlay is closed**, preserving padlock views |

Raw joysticks keep raw axes and buttons. `--gamepad-raw` restores the previous
direct SDL gamepad mapping and disables the modifier layers. Passing
`--joystick-axes 0,1[,2,3]` also opts out of the new profile, with optional
negative indices to invert axes, as with the Python frontend.
`--joystick auto|off|N` chooses the SDL device. Devices are re-enumerated after a
disconnect/reconnect. `--controls auto|keyboard|joystick` selects the virtual
`EF2000.CFG` flight controls; forcing `joystick` maintains a centered two-axis
gameport even if a physical controller is connected after startup.
`--trace-joystick` reports the selected device, changing axis/button samples
and the number of guest port `201h` reads and writes. The console also reports
guest instructions per wall-clock second plus SDL polling/render counts at
exit, so performance can be compared on the actual target machine. A small
windowed **SDL FPS / CHANGED FPS** HUD now shows the trailing one-second
SDL-presentation rate and the rate of actually changed guest framebuffer
images. The distinction matters when Unicorn repeatedly presents the same
image. Neither number measures the game's internal simulation ticks. Use
`--no-fps` to hide it.

The C++ core is still Unicorn-emulated rather than natively executing the
game. A headless 100M-instruction sound-enabled replay measured about 45M
guest instructions/s on the available test host; this is not a real-device
SDL3 window benchmark. Input/event polling is now limited to approximately
120 Hz, instead of being triggered by every 50K guest instructions, and SDL3
audio stream formats are changed only when the game's PCM rate changes.
Whether this sufficiently improves flight performance needs hardware testing.

Interactive unlimited windowed sessions now default to `--timing realtime`.
In this mode Unicorn executes translated blocks in approximately 10 ms slices
with a basic-block-boundary stop hook but no instruction count: passing an
instruction count would insert a costly implicit per-instruction code hook.
PIT interrupts instead follow the emulated hardware divisor and the host's
monotonic clock; SB16 and CD audio continue to be serviced between interrupts.
`--timing instruction` preserves the original
instruction-accurate debugger and scripted regression timing, including for
bounded headless runs. The new `--self-test-clock` verifies timer progress
using a synthetic busy loop without any game files. A separate 20-million-
instruction Unicorn tight-loop benchmark on the same host measured about
389M instructions/s uncounted versus 41M/s with counting (roughly 9.5×);
this is a microbenchmark, **not** a measured 9.5× game speedup. Real-time mode
does not know the number of guest instructions executed, so the console reports
`guest_ips=n/a` with emulation slices and PIT divisor instead. A bounded
real-time probe finishes with `status=wall_time_limit`. The old native
Linux backend still has a fundamental performance advantage because it executes
the game's protected-mode code directly, without CPU translation. Gameplay
performance with real SDL3 graphics and input still needs testing.

The default real-time path now samples the block deadline every 256 translated
blocks and VGA vertical-retrace status every 32 reads of port `3DAh`, avoiding
most of the host clock lookups caused by busy loops. Set
`--block-clock-check 1 --vga-clock-check 1` to restore per-access clock queries
for comparisons. `--profile` adds opt-in counts and host-time measurements for
guest execution, VGA polling, background audio and window updates. The
`--self-test-retrace` check verifies that retrace wait loops still finish; a
repeatable asset-free CPU sample is `--self-test-clock --wall-seconds 5 --profile`.
These options do not eliminate the remaining Unicorn block callback overhead;
actual in-game frame pacing must still be compared on hardware.

For deeper diagnosis, `--profile-hot-blocks` samples the 16 hottest guest
basic-block addresses, `--profile-copy` counts call sizes at the original
guest memory-copy routine, and `--profile-copy-callers` groups copies by the
guest return address and reports byte counts, VGA writes and accelerated calls.
In a sound-enabled startup replay that routine accounted for roughly 23% of
sampled block executions and copied hundreds of
megabytes. `--fast-copy` is an **opt-in experiment** that handles only large,
non-overlapping copies into guest data memory in C++; the native guest routine
continues handling smaller or potentially sensitive copies. A 75-second
headless original-game replay with the option enabled reached the 320×200
cockpit while SB16 and CD playback remained active, but gameplay correctness
and speed on Windows still need testing. `--self-test-fast-copy` verifies the
host fast path with an original-asset-free synthetic program. A separate
**opt-in** `--fast-copy-vga` also allows large copies into the 64 KiB banked
VGA RAM window, which accounts for much of the remaining startup copy traffic;
`--self-test-fast-copy --fast-copy-vga` checks the expanded destination and
register/stack behavior. A matching original-game headless test rendered the
same 640×400 menu as the old path, but in-flight FPS, timing and Windows SDL3
behavior have not been verified. With `--windowed --profile-video-frames`,
the frontend also reports `image_presentations`, `changed_images` and
`changed_hz` by fingerprinting the actual displayed pixels. This separates
identical repeated SDL presentations from visible guest updates; it is an
optional accumulated diagnostic, **not** simulation FPS. A controlled SDL3 dummy-driver
startup comparison saw 93 changed images in ten seconds with either copy
path despite much higher guest-copy throughput with `--fast-copy-vga`.
See [Ghidra findings](docs/ghidra-findings.md) and
[execution probes](docs/execution-probes.md).
The first standalone typed original-game C++ model now covers
[high-score records, insertion, defaults and file checksums](docs/highscores-reconstruction.md).
An opt-in differential harness also checks those reconstructed operations
against original x86 guest execution, including checksum-failure fallback.
A separate `--tcg-cache-mib N` option allows testing smaller Unicorn translated-code
caches (32..1024 MiB), especially for 32-bit Windows; size alone did not
produce a repeatable speed improvement on the Linux reference host.

DOS writes in windowed mode default to
`$XDG_STATE_HOME/ef2000/unicorn/drive` (or
`~/.local/state/ef2000/unicorn/drive`). Override this with `--overlay-dir`
to keep a specific profile or use it in headless mode; without an overlay,
headless mode rejects all guest writes. The overlay preserves the source
installation, supports files created by the game, and copies original files
only when opened for writing. `EF2000.CFG` is still adjusted virtually for
the connected controls and to disable unimplemented sound devices (except
CD music when an image is supplied). The
experimental `--sound sb16-probe` mode instead enables the original digital
driver and models the measured SB16 reset/version/resource interface at
port 220h, without enabling FM music. Reached DMA/PIC requests that have
not yet been ported stop with a diagnostic; this mode does **not** play audio.
The experimental `--sound sb16-dma` mode instead runs guest port I/O through
native C++ 8237 DMA, 8259 PIC and SB16 DSP models. A synthetic 16-bit test
program has programmed the sound hardware, transferred guest PCM, received
IRQ7, acknowledged it from guest code and returned via `IRET`. Modeled DMA
block and byte counts are reported at exit. Protected-mode timer ticks can
also finish DMA blocks and invoke installed real-mode IRQ handlers without
an outstanding X-32 BIOS call. Windowed runs feed gain-adjusted stereo PCM
into an SDL3 audio device, using the stream's input-format conversion to
handle the game's sample rates. Headless runs can optionally save the source
PCM as regular WAV files with `--wav-capture DIR` (requires `--sound sb16-dma`
or `--cd-cue`); CD captures use a separate `cd-audio` subdirectory;
format changes open a new file, and existing captures are never overwritten.
Only complete sample frames can be represented in WAV, so an incomplete
trailing frame is omitted. By default headless runs only count PCM blocks. Audio
device initialization failures disable playback without stopping the guest.
The original `SB16.DIG` from the supplied full EF2000 archive passed its
native C++ IRQ7/DMA self-test, including DSP 4.5 and DMA1/5 detection, and
the sound-enabled cockpit replay. The smaller reduced testing fixture alone
was missing that file. Real-device SDL3 audio playback remains untested here.

The native C++ BIOS bridge now responds to real-mode `INT 13h` disk probes:
there is **no BIOS sector device** behind the DOS-file virtual drive, so
requests return carry set with AH=`01h` (invalid function or drive)
instead of aborting emulation. The first request logs its function (`AH`)
and drive (`DL`) to stderr. No host disk or original game file is exposed
for raw sector I/O; a BIOS disk-image backend remains future work. This
fallback is covered by the asset-free `bios-disk-no-media` check, but the
original long-running gameplay path still needs a fresh integration run.

This is still an **experimental migration**, not yet a drop-in replacement
for the Python app. Real SDL3 window/gamepad operation has not been exercised
in this build environment, and real-device sound playback, original-driver
integration beyond the tested path, additional DOS calls and
extensive flight-control/playability validation remain open. The working
Python backend remains available at `nix run .#unicorn` for comparison.

The original `_EF2000.EXE` remains external and SHA-256 validated. The Python probe
implements the observed X-32 stack/heap allocator, selectors, vector services,
DOS clock, conventional-memory allocations, and DOS file open/read/write/
create/seek/close/IOCTL/flush. It records guest console output in the JSON
report and never writes to the original game files. Original assets and CD
data are always read-only. Supply `--overlay-dir DIR` to enable game-created
files and copy-on-first-write under `DIR/EF2000/` (relative game paths) or
`DIR/` (absolute DOS paths). Without that option, writes fail with DOS error 5.
An alternate asset directory can be supplied with `--game-dir DIR`.

The supplied ZIP **does contain** the original `DID1ST.DAT` and `DID.DAT`
data archives; the Linux port loads them successfully. An earlier portable
probe incorrectly reported missing `\\CHECK.CD` and `\\VIEWS\\VIEWS2.DAT`
because it omitted the DOS PSP command line and environment used by the Linux
harness. The portable probe now initializes `gui`, `DID=.` and `quick_flag=1`
equivalently, models the keyboard-controller ACK handshake, and resolves DOS
paths containing `./`. It disables sound in a virtual, read-only configuration
copy by default (`--sound original` uses the original settings). On the same
installed assets it reads the archives, starts `TACTCOM`, and executes
X-32 AX=`2511h` real-mode driver requests through the same bounded 16-bit
Unicorn engine used on Linux. Direct INT `10h`, `33h`, `16h`, and `1Ah` BIOS
calls use that bridge too. The game reaches VGA mode `13h` (320×200) and
continues through the 30-million-instruction diagnostic budget. The portable
CPU now models PIT channel 0 programming, BIOS tick accumulation, protected
IRQ0 injection, PIC EOI/IRET, and the original synthetic BIOS timer fallback.
Ticks are instruction-paced (`--timer-instructions N`, default 250000 in the
probe), not
wall-clock synchronized. The `--mouse-event TICK:X:Y:BUTTONS` option can
inject reproducible mouse states (button 1=left, 0=release); events appear in
the JSON report. Scripted movement updates both absolute coordinates and
relative mouse-mickey counters, matching live SDL input for original menu
polling (`INT 33h` functions `03h` and `0Bh`). The original menu maintains
its own cursor position from relative movement, so the scripted X:Y values
are not necessarily the on-screen cursor coordinates. The virtual DOS mouse
also re-centers and adopts the correct bounds when VGA/VBE modes change (for
example, the menu's 640x400 mode), so BIOS coordinates and the software cursor
start from matching screen centers.

The `--key-event TICK:SDL_SCAN:ASCII:DOWN` option injects
keyboard make/break events, e.g. `--key-event 600:41:27:1` and
`--key-event 605:41:27:0` press and release Escape. Both work in headless probes.
For game-port diagnostics, repeat `--joystick-event TICK:X:Y:BUTTONS` with
signed SDL axis values (-32768..32767) and a four-bit button mask; optionally
use `TICK:X:Y:Z:W:BUTTONS` for four axes. A scripted joystick is advertised
from startup with centered axes, and the virtual config selects joystick
controls and substitutes `JOY_FACTOR=100` for the original zero calibration
factor. Original files are never modified. For the supplied `STICK=15`
configuration, the four-axis form is needed for the verified in-flight roll
response: `--joystick-event 0:0:0:0:0:0` followed by
`--joystick-event 8300:32767:0:0:0:0` visibly banks the original cockpit,
whereas a two-axis-only replay did not show an unambiguous response.
The report records video-mode changes with instruction counts, making splash
and VBE transitions traceable. `--sample-instructions N` adds periodic guest
instruction-pointer/register snapshots, and DOS file requests report the
returned error flag and result value; this helps locate post-splash stalls.
`--trace-mouse` additionally groups the original guest's mouse-driver call
sites, internal GUI cursor positions, control hit-tests, generated events, and
dispatch codes by timer tick, without dumping every individual poll.
Experimental Python `--windowed` mode now uses the shared SDL3
renderer and input layer; headless remains the default for the probe and the
window is the default for the new `unicorn` app. Windowed sessions
now run until closed (or the guest stops), instead of terminating at the
headless 30-million-instruction budget. Use `--instruction-limit N` to set
an explicit budget (0 = unlimited). SDL3 events are
polled every 50,000 guest instructions (tunable with `--display-instructions N`),
with rendering capped at 30 FPS. VGA DAC writes update the DOS color palette.
When the original video mode is absent or renders an entirely black frame,
the SDL window presents a startup banner until the first nonblack original
frame arrives. SDL clear/present failures now surface as errors.
The window accepts keyboard, captured relative mouse, and joystick/gamepad
input through the Linux backend's host input mailboxes. Registered IRQ1
keyboard handlers receive pending scan codes at PIT tick opportunities;
port `201h` emulates joystick discharge and active-low buttons. F10 opens the
gamepad-controllable debug menu and F11 toggles fullscreen. Window options
include `--fullscreen`, `--geometry WIDTHxHEIGHT`,
`--aspect-ratio {4:3,native,stretch}` and `--smooth-scaling`. The optional
`--framebuffer PATH` writes the latest VGA or VBE frame as a PNG in either
headless or windowed mode. SDL3 must be installed (`EF2000_SDL_LIBRARY` can
specify its shared library), and windowed mode has so far been checked using
an injected test display, not a live X server. The Python backend still
lacks a standalone Windows launcher; the C++ runner now has MinGW/SDL3
archive outputs, though Windows gameplay remains unverified. An
asset-backed 100-million-instruction probe produced the original 320×200
Pentium splash screen with its palette; the 30-million-instruction checkpoint
is still black. The original code then switches into 640×400 VBE mode and
renders the Digital Image Design splash. Initially it stalled there, even
after 280 million instructions, because the portable backend returned zero
unconditionally for VGA status port `03DAh`. The game waits for vertical
retrace there, bounded by a large polling loop at `0xDC302`. Mirroring the
native backend's 60 Hz/1.67 ms retrace signal fixes the stall: an original-
asset probe with `--instruction-limit 200000000 --timer-instructions 100000`
reached and rendered the original **640×400 German main menu**, including
the mouse pointer and buttons. Broader gameplay remains unvalidated.
No CD marker or additional unpacking is needed for that startup path. This
does not establish that all game features work under the portable backend.

The CPU enters ring 3 through an IRETD trampoline with the original X-32
selector values (`CS=0x2f`, `DS/SS=7`) and a synthetic LDT. This corrects the
early ring-0 shortcut. The data segment's expand-down limit is updated on the
first 3505 request, and dynamically allocated descriptors are written into the
guest LDT. It uses IOPL=3 for hooked port operations, and **does not yet
implement** complete privilege-fault semantics, exceptions, or all hardware
IRQs. The diagnostic stops on unsupported requests (exit code 20), guest exit
(exit code 26), or the instruction budget; it prints the original guest call
transcript and BSS-clearing checkpoint. The default 30-million-instruction
budget can be changed with `--instruction-limit`. The legacy ring-0 model is
available for comparison via `--flat-ring0`.

Absolute DOS paths such as `\\CHECK.CD` are resolved against `--cd-dir`, while
relative game paths stay under `--game-dir`. No game or CD assets are included
in the port or its git bundle.

This is a first verified CPU-execution milestone, not a new `--backend` choice
for the playable launcher. The existing Linux native backend remains unchanged.

The host window, joystick and PCM audio backends use SDL3 (3.2+), loaded with
`ctypes`. `EF2000_SDL_LIBRARY` overrides the system SDL3 lookup; the Nix
launcher and dev shell point it at nixpkgs' `sdl3`. Unlike the original SDL2
port, window events use SDL3's typed structures, joystick selection enumerates
instance IDs, and audio playback uses SDL3 audio streams. SDL3 migration does
not change the Linux-specific original-code execution harness. **Native
Windows execution of the original code remains a separate backend task**;
the new Windows flake outputs use the portable C++ Unicorn frontend.

Keep original executables and game data outside this repository. Put generated
memory dumps, traces, and extracted images in ignored `local/` or `build/`.
The native executable, package version, and help flags are checked without game
assets; original-file integration tests run explicitly outside the Nix build.

## License and continuity

New port code and documentation are GPL-3.0-or-later; see
[LICENSES/GPL-3.0-or-later.txt](LICENSES/GPL-3.0-or-later.txt) and `REUSE.toml`.
Original game executables and data remain external and retain their own rights.
Read [TODO.md](TODO.md) for the current stopping point, validation limits, next
steps, and the incremental adoption of the gamepkgstd Linux conventions.

## Documentation

- [Port architecture and milestones](docs/port-plan.md)
- [Executable and startup findings](docs/executable.md)
- [Recovered loader and image layout](docs/loader.md)
- [Headless Ghidra import and decompilation workflow](docs/ghidra.md)
- [Startup symbols and interface worklist](docs/startup-symbols.md)
- [Execution probes and reference experiments](docs/execution-probes.md)
- [Native startup harness and validation](docs/startup-runtime.md)
- [Recovered extender services and differential evidence](docs/x32-services.md)
- [Sound driver research and diagnostic probe](docs/sound-research.md)
- [Original setup programs, CFG/INI formats and host configuration options](docs/configuration-research.md)
- [Original keyboard chart and host input interception reference](docs/keyboard-reference.md)
- [Research and evidence conventions](docs/research.md)
- [Existing modding tools, mission file formats, release variants and 3Dfx research](docs/existing-tools-and-releases.md)
- [GFX+ 3Dfx patch and rendering interface investigation](docs/gfxplus-rendering.md)
- [Mission, navigation and campaign text-format reconstruction](docs/mission-records.md)
- [Original mission, navigation and campaign reader call map](docs/mission-readers.md)
- [Recovered native C++ mission, waypoint, attack, target and pilot memory layouts](docs/mission-layout.md)

The read-only `tools/mission_enum_probe.py` additionally exports the
original X-32 reader's indexed names for waypoint actions, weapons,
aircraft, ground units, roles and status codes from a SHA-pinned
`payload.bin` (see the reader map for commands). The game binary remains
external to this repository.

The mouse starts free and is captured only by a left-click inside the game
image. That first click captures the pointer without activating a game button.
Hold Escape for three seconds to release the desktop pointer; a short Escape
press still reaches the game normally. Losing focus also releases capture, and
regaining focus never captures it again. The window title shows the available
capture/release action. Relative input remains necessary for reliable menu
cursor positioning; uncaptured absolute input is not implemented yet.


Flight controls default to the first connected SDL joystick, or keyboard when no
joystick is connected. Use `--controls keyboard` or `--controls joystick` to force
a mode. Keyboard input includes held keys, releases, modifiers, function keys,
and the numeric keypad; losing focus releases held keys.

The joystick exposes up to four axes and four buttons through the DOS game port.
Use `--joystick N` to select an SDL device index or `--joystick off` to disable it.
`--joystick-axes=0,1,3,-2` remaps the four axes and inverts SDL axis 2. Defaults
are axes 0,1,2,3 and buttons 0–3. Use the original game's joystick calibration
for the selected stick; only the output directory's config is changed.
Joystick hotplug is supported; automatic flight-control selection happens at
startup. Keyboard and joystick flight input were verified under Xvfb, using an
SDL virtual joystick. When joystick controls are selected, an inherited
`JOY_FACTOR=0` is replaced with the executable default of 100 in the writable
config: digital-sound startup otherwise reduces joystick calibration to zero.
Add `--verbose` to compare `input: joystick` focus/axes/buttons
with `trace: GAMEPORT` samples received by the game; moving the stick should change
both. Focus loss centers axes and releases buttons.

Press **F10** to pause the native game and open the debug overlay. Use the
arrow keys and Enter, or click an entry. Escape goes back; F10 resumes. The
selected joystick also controls the overlay: **Start** (button 7) opens/closes
it, the D-pad or vertical stick navigates, **A** (button 0) selects, and **B**
(button 1) goes back. Hold a direction to repeat; menu button presses are not
passed into the game when resuming. These are SDL button indices, so mappings
may differ on non-Xbox-style joysticks. The menu provides Resume, borderless
fullscreen, separate Save State / Load State
submenus, and Quit. **F11** also toggles fullscreen. The save submenu offers
**New Save** and existing slots to overwrite; both lists scroll with the keyboard.
The overlay shows the current loaded/saved slot.

Save states use the native64 backend, including with `--sound sb16`.
They include CPU/x87 context, guest and DOS memory, emulated video/input state,
open-file positions, and the writable drive overlay. Original assets stay
read-only. Loading replaces this run's drive overlay with the saved copy.
SB16 states also preserve DSP, DMA, and interrupt state so audio resumes on load.
States are tied to the executable, runtime, and device configuration; incompatible
states are rejected. QEMU checkpoints are not supported.
Names include the date, observed menu/loading/flight phase, resolution, and
campaign/mission/navigation filenames when observed. Exact player location and
mission type are not yet recovered, so names do not guess those fields.

The previous default `$XDG_DATA_HOME/ef2000/states` remains readable if the
new state directory is empty; existing saves are not moved or deleted. Older
saves in `OUTPUT/states` can be opened using `--state-dir OUTPUT/states`.
Use `--state-dir DIR` for another library, or load a state directly with:

```sh
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE --output local/restored-run \
  --state-dir local/states --load-state local/states/SAVE.efstate
```

`--fullscreen` starts in desktop fullscreen. Normal stdout reports paths,
configuration, renderer/window settings, observed game/video phase changes,
file creation, and state operations. `--verbose` adds detailed guest file,
console, PIT, and I/O activity. The complete native transcript remains in
`OUTPUT/calls.log` regardless of verbosity.
