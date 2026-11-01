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
An experimental Unicorn **protected-mode startup probe** now runs the original
extracted 32-bit instructions on a portable CPU emulator. It passes the
initial X-32 allocation, descriptor and DOS services, reads original game data,
and progresses through the game's data-archive startup. It is not yet a
Windows-playable replacement for the native Linux execution backend.
The first target is the installed `_EF2000.EXE` in the sibling
`../EF2000/c/EF2000/` directory. Other releases need separate fingerprints.

## Development

On x86-64 Linux, build and run with Nix:

```sh
nix build -L .                 # Precompile the native and QEMU harness objects
nix flake check                # Unit tests, app/help/version checks, REUSE
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE --output local/nix-startup
nix develop                    # Python, GCC/binutils, QEMU, DOSBox-X, GDB
```

Inside `nix develop`, the following development commands are available:

```sh
ef2000-configure                       # CMake/Ninja Debug harness in /tmp/ef2000-build
ef2000-build                           # configure if missing/moved, then build objects
ef2000-run /path/to/_EF2000.EXE        # build then run, using those objects
ef2000-run-gdb /path/to/_EF2000.EXE    # build then debug native startup.elf in GDB
nix develop -c ef2000-build            # also works without an interactive shell
```

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
`ef2000` launcher; `ef2000-startup` remains available as a command alias.
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

The protected-mode Unicorn probe runs independently of Linux syscalls, ELF
execution, `modify_ldt`, GCC and binutils. It requires Python 3 and Unicorn
2.x and is designed to run on Windows as well as Linux:

```sh
nix run .#unicorn-probe -- /path/to/_EF2000.EXE
# With SDL3, show indexed video and forward live input:
nix run .#unicorn-probe -- /path/to/_EF2000.EXE --windowed --geometry 1280x960
# Or on Windows, with `pip install unicorn`:
python tools/unicorn_backend.py C:\EF2000\_EF2000.EXE --report startup.json --overlay-dir C:\EF2000-user\drive --cd-dir D:\
# Deterministic headless mouse press/release while probing the VGA menu:
python tools/unicorn_backend.py C:\EF2000\_EF2000.EXE --mouse-event 70:320:100:1 --mouse-event 78:320:100:0
# Export the final framebuffer without opening a window:
python tools/unicorn_backend.py C:\EF2000\_EF2000.EXE --framebuffer startup.png
```

The original `_EF2000.EXE` remains external and SHA-256 validated. The probe
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
Ticks are instruction-paced (`--timer-instructions N`, default 250000), not
wall-clock synchronized. The `--mouse-event TICK:X:Y:BUTTONS` option can
inject reproducible mouse states (button 1=left, 0=release); events appear in
the JSON report. Similarly `--key-event TICK:SDL_SCAN:ASCII:DOWN` injects
keyboard make/break events, e.g. `--key-event 600:41:27:1` and
`--key-event 605:41:27:0` press and release Escape. Both work in headless probes.
The report records video-mode changes with instruction counts, making splash
and VBE transitions traceable. `--sample-instructions N` adds periodic guest
instruction-pointer/register snapshots, and DOS file requests report the
returned error flag and result value; this helps locate post-splash stalls.
Experimental `--windowed` mode now uses the shared SDL3
renderer and input layer; headless remains the default. Windowed sessions
now run until closed (or the guest stops), instead of terminating at the
headless 30-million-instruction budget. Use `--instruction-limit N` to set
an explicit budget (0 = unlimited). SDL3 events are
polled every 50,000 guest instructions (tunable with `--display-instructions N`),
with rendering capped at 30 FPS. VGA DAC writes update the DOS color palette.
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
an injected test display, not a live X server. The portable backend still
lacks a native Windows launcher and confirmed playable execution. An
asset-backed 100-million-instruction probe produced the original 320×200
Pentium splash screen with its palette; the 30-million-instruction checkpoint
is still black. The original code then switches into 640×400 VBE mode and
renders the Digital Image Design splash. That splash remained unchanged
through a 280-million-instruction probe with 2,800 instruction-paced PIT
ticks; scripted mouse presses and an Escape make/break (both delivered, with
two protected IRQ1s for Escape) did not dismiss it. Reaching the interactive
main menu under Unicorn remains an open diagnostic boundary. The native Linux
backend does reach it with the same assets.
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
Windows builds remain a separate backend porting task**, not a flake output.

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
- [Startup symbols and interface worklist](docs/startup-symbols.md)
- [Execution probes and reference experiments](docs/execution-probes.md)
- [Native startup harness and validation](docs/startup-runtime.md)
- [Recovered extender services and differential evidence](docs/x32-services.md)
- [Sound driver research and diagnostic probe](docs/sound-research.md)
- [Original setup programs, CFG/INI formats and host configuration options](docs/configuration-research.md)
- [Original keyboard chart and host input interception reference](docs/keyboard-reference.md)
- [Research and evidence conventions](docs/research.md)

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
