# Execution probes

The current execution path is the [native startup harness](startup-runtime.md).
The small first-entry probe below remains useful for reproducing the initial
backend experiment; it does not describe the latest startup progress.

## Original entry under QEMU

Reproduce from `ef2000/` with Python 3 and `qemu-i386`:

```sh
python3 tools/probe_entry.py ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/qemu-entry-01
```

The output directory must be new. The tool extracts the fingerprinted payload,
wraps it in a diagnostic ELF32 image, and runs QEMU with one instruction per
translation block and register logging. No assembler or C compiler is required.
The application offsets map to guest addresses starting at zero. QEMU's
`-B 0x40000000` chooses a host guest-base offset; this is not the original DOS
allocation base. A small wrapper at guest `0x08000000` initializes a synthetic
stack and jumps to the unchanged application entry at `0x86`.

Observed with `qemu-i386 version 10.2.4`:

| Application offset | Original instruction | Result |
| --- | --- | --- |
| `0x86` | `mov bx, ds` | BX becomes the synthetic data selector `0x2b` |
| `0x89` | `mov ecx, 0x260` | ECX=`0x260` |
| `0x8f` | `mov ax, 0x3505` | AX=`0x3505` |
| `0x93` | `int 0x21` | Stops: no DOS/extender interrupt handler |

The stop is target SIGSEGV, not a successful DOS call. The probe reports success
only if the trace contains the expected instruction boundary, register values,
and signal. It disables core-file generation. `report.json`, `trace.log`, and
`console.log` preserve the evidence locally.

This validates execution of **three original instructions** in QEMU's Linux-user
mode. It does not validate original selector values, extender services, x87
behavior, timing, or game startup. FS/GS, the PSP, the environment, and extender
patches are intentionally not prepared yet. QEMU Linux-user is an available
experimental execution backend; its suitability for the complete game remains
unproven.

`probe.elf` embeds the original payload. Keep it in ignored local/build storage;
do not commit or distribute it as source.

## Disposable DOSBox-X reference experiments

```sh
python3 tools/reference_run.py --game-dir ../EF2000/c/EF2000 \
  --output local/reference-01 --seconds 10 --headless
```

This copies the installation before mounting it, records hashes, requests INT 21h
and file-I/O logging, and runs the original `gui` startup with its environment
variables. Default configuration: 32 MiB, normal CPU core, fixed 60,000 cycles,
surface video, MIDI disabled. Headless mode uses dummy audio/video. These settings
are experiment inputs, not established optimal or faithful game settings.

The script records the emulator process status and compares both the original
and copied installation after the run. A host timeout bounds runs if the
emulator's time limit does not terminate them.

The 10-second experiment using DOSBox-X `2026.05.02`, commit `5817c64`, exited
with code zero, without a host timeout or changes to either installation. Video
logs changed from text output to a 640x400 surface and recorded
`MOUSE:Unhandled videomode 68 on reset`. This is not proof of a displayed menu.
No requested INT 21h/file-I/O trace appeared in this installed build. Use an
instrumented build or another tracing method before treating this as a reference
for loader register state or syscall coverage.

## Unicorn original-menu mouse dispatch

The protected-mode Unicorn probe now reaches the original 640x400 German main
menu with the supplied `_EF2000.EXE` and its unmodified DID archives. Use
`--trace-mouse --timer-instructions 100000` alongside timed `--mouse-event`
options and `--report` to record grouped guest call sites rather than every
individual mouse poll. The addresses below refer to the extracted protected
image, not the on-disk DOS stub.

- `0xCDFE8` reads INT 33h function 03h (position and buttons), including the
  game's historical Y/2 scaling; `0xCE02C` reads relative motion (0Bh), and
  `0xCE064` reads the button mask alone.
- The main menu uses the separate `0xAC854` input path. It updates a software
  cursor from mickeys and passes button state to `0xACBE4` for hit-testing.
  `0xACFB8` dispatches the resulting control event.
- An asset-backed move to mouse `(180,85)` after menu initialization produced
  software cursor `(179,84)` and hit control id 7. A left click registered
  actions 25 (press), 9 (held), and 33 (release) for that same control. This
  coordinate is in the *window title bar*, not the menu icon's clickable body.
- An earlier press during menu initialization instead entered a wait-for-release
  loop at `0xAC569`. A click probe must move and press *after* that handoff;
  an unchanged screenshot by itself does not identify the failing input stage.
- The title-bar widget runs `0xF1A3F` → `0xE1D14` with a pressed flag of
  `0x10`. The parent window's status `7` fails the focus/activation gate at
  `0xA4B08`. *This is not the game menu action.* The original GUI resource
  declares these windows with `nojump`; the separate `JumpToFront`/`Style`
  handlers do not establish a missing state transition in the emulator.
- The asset's `window arcade` starts at `(150,80)` with size `100×40`; its
  `arcadesel` icon covers the window body below the title. A click at
  `(165,110)` targets that icon, and release produces action `33`, nested
  widget flag `0x20`. The low-level widget's callback at `+0x71` is zero,
  but the game uses separate script masks for `select entering`, `unselect
  leaving`, and `call release` (handlers `0xF6868`, `0xF74B4`, `0xF757C`).
  The menu's event loop stops on release, and the original game begins
  opening/loading `newgui03.lbm` for the next screen. Direct filename opens
  return DOS error 2, then a successful 178,688-byte read from the archive
  follows. A later click cannot be used to test a second menu action because
  the game has already left that event loop.
- The scripted `S` key also takes the screen-loading path; `Escape` opens the
  original "Quit EF2000?" confirmation in the same menu. This establishes
  that keyboard IRQ delivery, GUI command dispatch, and the game-data script
  are functioning. A framebuffer exported while the next screen is loading
  can still show the previous menu; verify the subsequent video handoff before
  reporting navigation failure.
- Extending the *Schnelles Gefecht* click replay to **230 million protected
  instructions** completes the handoff: the guest renders the original 640×400
  Quick Combat screen with difficulty choices, 12 mission buttons, overview,
  high scores, and `Akzeptieren`. At 158–163 million instructions the screenshot
  still showed the previous menu because the game was reading the next screen
  from its archive. This is an instruction-budget artifact, not failed input.

For a repeatable headless Quick Combat screenshot (replace the path with the
original installation location):

```sh
python3 tools/unicorn_backend.py /path/to/_EF2000.EXE \
  --game-dir /path/to --timer-instructions 100000 \
  --instruction-limit 230000000 \
  --mouse-event 1400:165:110:0 \
  --mouse-event 1450:165:110:1 \
  --mouse-event 1470:165:110:0 \
  --framebuffer /tmp/ef2000-quick-combat.png \
  --report /tmp/ef2000-quick-combat.json
```

These observations establish that the mouse driver, software cursor, control
hit-test, GUI event dispatcher, scripted menu activation, and subsequent screen
load are connected. They **do not** yet validate mission start or flight under
Unicorn. Do not clear the guest's window-status bits to make clicks work: the
actual menu icon is already dispatching its action.

## Unicorn Quick Combat mission start

The original GUI script in `DID1ST.DAT` defines `window difficulty` at
`(100,40)`, `window miss_sel` at `(100,135)`, and `window summary` at `(100,240)`.
Their clickable controls lie in the window bodies, below the title bars:

| Action | Mouse target | Original script action |
| --- | --- | --- |
| Rookie difficulty | `(205,74)` | `gpwChangeDifficulty` |
| Mission 01 | `(130,176)` | `gpwSelectLevel` |
| Accept box (right of label) | `(287,368)` | `gpwArcadeAccepted` |

After opening Quick Combat with the first-menu icon click at ticks
`1400/1450/1470`, the new GUI polls events by approximately tick `1678` with
`--timer-instructions 100000`. A follow-up replay selected Rookie at ticks
`1790/1810/1830`, mission 01 at `1850/1870/1890`, and accepted at
`1910/1930/1950` (move/press/release for each). The screen changed to the
original *Simulation wird geladen...* loading status. The guest then read
additional mission and effects assets, reaching the original payload's
decompressor. The initial replay stopped at instruction `325724093`: original
code at `0xFA153` checks the decoder output against word `[0x16279E]`, then
deliberately branches to `INT 3` at `0xFA167` on mismatch. The checksum routine
at `0xFA2E4` uses two rolling 8-bit accumulators over the output buffer given
by `[0x162794]` and `[0x162790]`.

The optional `--trace-decompress` probe records each original checksum compare
in the JSON report, including the last passing block. On a mismatch it includes
the expected and actual sums, output address and length, an independent sum,
SHA-256 and samples of the decompressed bytes. The game breakpoint is preserved
and reported as `guest_integrity_failure`, not ignored. The original failing
block expected `0x6F91`, but the guest (and independent recomputation) returned
`0x3792`. Its 49,152-byte output spanned `0x00FF7C34..0x01003C34`, crossing
the Unicorn backend's former DOS RAM mapping at `0x01000000`. That mapping,
plus the former PSP/environment mappings at `0x02000000`, overlapped the
64 MiB protected application image. The Linux native harness places DOS RAM
outside its protected image; the portable backend must do likewise.

Unicorn now maps DOS RAM at `0x05000000..0x05100000`, the PSP at
`0x05200000`, and the environment at `0x05201000`. The ring-3 selector bases,
DOS allocation pointers, BIOS bridge, timer, VGA palette and input mailboxes
use those disjoint mappings. With the **same original assets and timed mouse
replay**, all **68 checksum comparisons pass** and there are **zero failures**.
The run reaches its requested 330-million-instruction limit, 4.3 million
instructions beyond the previous integrity failure. Video remains in VBE mode
`0x100` (640×400) and the frame at this point is black. Extending the exact
same replay to **420 million instructions** reaches **372 passing checksum
comparisons** and renders the original *TACTCOM 2000* loading screen at **20%**
with the legend *Erstelle Umgebung* ("Creating environment"). An earlier frame
at 350 million shows **35%** with *Erstelle Bewaffnung* ("Creating armament"):
the percentage is **stage-local**, not a monotonic overall progress indicator.
EIP samples show continuing asset-loading/decompression work, not a stuck spin
loop. This does **not** yet demonstrate flight rendering; the working native
backend uses 320×200 VGA once flight begins.

A longer **700-million-instruction** replay with timed framebuffer checkpoints
still uses VBE `0x100`, but reaches **572 successful decompression checks**
without an integrity failure. The original loading animation advances as follows:

| Instructions | Screen text | Displayed percent |
| ---: | --- | ---: |
| 350 million | *Erstelle Bewaffnung* | 35% |
| 420 million | *Erstelle Umgebung* | 20% |
| 500 million | *Erstelle Umgebung* | 57% |
| 600 million | *Erstelle Umgebung* | 0% |
| 690 million | *Erstelle Umgebung* | 54% |
| 700 million | *Erstelle Umgebung* | 60% |

The progress number resets even while the same label is visible. The 700M
checkpoint remains in mission environment creation, so it is premature to
claim a flight renderer failure or that the mission has finished loading.
Among the last 1,000 real-mode requests at this boundary, 936 are VBE
`INT 10h AX=4F05h` bank switches, consistent with active 640×400 framebuffer
updates rather than a guest idle loop.

### Original flight reached under Unicorn

The same asset-backed, scripted Quick Combat replay was extended to **1.02
billion guest instructions**, using the original installation and unchanged
menu selection/timer inputs. At **760 million** instructions, the loading screen
shows **97%** (*Erstelle Umgebung*). At instruction **778,005,467**, the
original guest invokes the video-mode transition from VBE `0x100` (640×400)
to VGA `0x13` (320×200). A frame captured at **820 million** shows the
original cockpit, HUD, instruments, and terrain. The **900-million** and
**1-billion** frames show changed instruments, terrain, HUD values, and the
running flight timer, establishing continued original flight simulation and
rendering rather than a single static picture.

The run stops only at its requested instruction limit (`instruction_limit`),
at **1,020,000,000 instructions / 10,200 instruction-paced PIT ticks**. All
**604 original LZ checks pass**, with zero integrity failures. The guest
remains in VGA `0x13` at that boundary. This is a repeatable positive
**rendered-flight milestone**, not yet a guarantee of complete playable
missions, calibrated controls, or sound under Unicorn. Those remain separate
validation tasks, as does the native Windows launcher.

To extend the replay command below to the same flight milestone, set
`--instruction-limit 1020000000`, add `--framebuffer /tmp/ef2000-flight.png`,
and request `--framebuffer-sample 760000000`,
`--framebuffer-sample 820000000`,
`--framebuffer-sample 900000000`, and
`--framebuffer-sample 1000000000`.

### Scripted original flight keyboard input

The original keyboard chart assigns **F4** to player external views. To
verify input through the portable backend's original keyboard handler, replay
the same mission start with these extra options:

```sh
--key-event 8300:61:0:1 --key-event 8305:61:0:0 \
  --instruction-limit 950000000 \
  --framebuffer /tmp/ef2000-f4.png \
  --framebuffer-sample 820000000 --framebuffer-sample 860000000 \
  --framebuffer-sample 900000000 --framebuffer-sample 940000000
```

Here `61` is the SDL F4 scancode, mapped to the original PC set-1
`0x3E` make / `0xBE` break bytes. Both events are injected after flight
startup (tick 8300). The **820M** pre-input cockpit image is **byte-identical**
(SHA-256 `2dc4236620235c62a413da89b71e311dddc75eebebb6d4c8187592f0dec4647f`)
to the earlier unmodified no-input replay. By **860M**, the guest renders an
external view of its aircraft with the original *Aussen beweglich 1* label;
the **900M** and **940M** checkpoints retain that view and show continuing
simulation updates. The report records both applied key events and **two
protected keyboard IRQ1 deliveries**, compared with zero in the no-input run.

The guest reaches its requested **950M-instruction** limit, remains in
320×200 VGA, and passes all **604 original LZ checks** without failures.
This establishes the in-flight keyboard make/break → IRQ1 → original camera
command → renderer path. It does not yet validate physical keyboard focus,
joystick axes, throttle, pitch/roll, or mission completion under Unicorn.

### Scripted joystick input and game-port measurements

The headless Unicorn runner accepts deterministic two-axis or four-axis SDL
joystick samples at PIT ticks. A two-axis, one-button-mask sample has the form
`--joystick-event TICK:X:Y:BUTTONS`; use
`--joystick-event TICK:X:Y:Z:W:BUTTONS` for four axes. Axis values are signed
16-bit SDL values and the button mask selects the low four bits. The first
scripted event also advertises a centered joystick from startup, so the
original `EF2000.CFG` is presented with `CONTROL=1`. A shipped
`JOY_FACTOR=0` is replaced by the executable's default of `100` in the
in-memory virtual config. The files on disk are unchanged.

For an original-asset flight replay, append these options to the same
Quick Combat / Rookie / mission 01 mouse script described above:

```sh
--joystick-event 0:0:0:0 \
--joystick-event 8300:32767:0:0 \
--joystick-event 8800:0:0:0 \
--instruction-limit 950000000 \
--framebuffer-sample 820000000 \
--framebuffer-sample 850000000 \
--framebuffer-sample 880000000 \
--framebuffer-sample 920000000
```

The headless replay applied all three joystick states, reached the 950M
instruction limit and passed 604 original LZ checks without failure. The
320×200 cockpit continued rendering, but the sampled frames did **not**
demonstrate a clear roll response to the X-axis deflection. Synthetic guest
tests establish correct game-port 201h button bits, axis discharge counts,
and the OUT/IN sequence, but do not by themselves prove original-game flight
control. Reports now include `gameport.reads`, `gameport.writes`, and
`gameport.sampled_states` for direct diagnosis of guest port consumption.

A second original-asset replay, with the same scripted inputs and guest port
instrumentation, completed 900M instructions with **120,120 IN 201h reads**
and **30 OUT 201h writes**. Of those 30 game-port charge cycles, **22** sampled
the centered X/Y discharge values `[64, 64]`, and **eight** sampled the
full-X deflection `[127, 64]` (axis mask `3`, no pressed buttons). All
604 decompressor checks passed, and the guest remained in VGA mode `0x13`.
This directly confirms delivery and consumption of the scripted stick
position by the *original executable*, but **not** an observable aircraft
control response. Diagnosing its joystick-control interpretation and the
physics/camera effect remains open.

To reproduce the mission-start integrity check, run the same timed actions
against the original, unmodified game files:

```sh
python3 tools/unicorn_backend.py /path/to/_EF2000.EXE \
  --game-dir /path/to --timer-instructions 100000 \
  --instruction-limit 330000000 \
  --mouse-event 1400:165:110:0 --mouse-event 1450:165:110:1 \
  --mouse-event 1470:165:110:0 \
  --mouse-event 1790:205:74:0 --mouse-event 1810:205:74:1 \
  --mouse-event 1830:205:74:0 \
  --mouse-event 1850:130:176:0 --mouse-event 1870:130:176:1 \
  --mouse-event 1890:130:176:0 \
  --mouse-event 1910:287:368:0 --mouse-event 1930:287:368:1 \
  --mouse-event 1950:287:368:0 --trace-decompress \
  --report /tmp/ef2000-unicorn-mission.json
```

For longer headless replays, add `--framebuffer /tmp/ef2000-flight.png` and
repeat `--framebuffer-sample N` for selected guest instruction counts (e.g.
`350000000`, `420000000`, `500000000`). These checkpoints write numbered PNGs
next to the final-frame path and list their actual video modes in the JSON
report. They reveal intermediate loading screens without separate full replays.
Add `--progress-instructions 50000000` to print guest instruction/tick counts,
EIP, current video mode, and decompression-check counter to stderr while the run
is still executing. Both diagnostics are disabled unless requested.
