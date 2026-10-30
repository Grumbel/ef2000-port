# Original-code runtime and main menu

## Run and validate

On x86-64 Linux:

```sh
nix run .#ef2000 -- ../EF2000/c/EF2000/_EF2000.EXE --output local/menu-01
```

The original German TACTCOM main menu renders in a 640×400 indexed VBE mode,
with an SDL window and mouse input. The screenshot in a completed run is
`framebuffer.png`. Close the window to stop. Output directories must be new;
the executable's parent directory must contain the original game installation.
Audio is disabled in a local `drive/EF2000/EF2000.CFG` copy. Original assets and
configuration remain unchanged. Gameplay beyond the menu is not validated.

For a bounded capture, or original-file regression checks:

```sh
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/menu-capture --headless --seconds 25
nix develop -c env EF2000_EXE=../EF2000/c/EF2000/_EF2000.EXE \
  python3 -m unittest discover -s tests -v
```

Direct execution inside `nix develop` uses `python3 tools/run_startup.py` with the
same arguments. Outside that environment it needs Python, Unicorn, GCC/binutils,
and an SDL2 library selected by `EF2000_SDL_LIBRARY` (or the system library lookup).
Native64 requires x86-64 Linux with `modify_ldt`; it needs no 32-bit libc.

## Native execution and memory

The ELF64 harness maps the original initialized application at `0x10000000`.
A far return switches into x86 compatibility mode, using 32-bit code/data LDT
descriptors with that base, and starts unchanged instructions at offset `0x86`.
Guest instruction/data offsets remain unchanged. The image reserves 64 MiB,
with initial stack offset `0x3fff000`; the original game's observed heap requests
exceeded the earlier 8 MiB model.

The virtual 1 MiB DOS physical-memory window lives at host `0x01000000`.
The loader's zero-access selector and base-offset patches point to this window,
so BDA, conventional-memory, and VGA accesses work without mapping Linux's
protected low addresses. Conventional paragraph allocation uses physical
`0x10000..0x8ffff`; `0x90000..0x9ffff` is reserved for the real-mode bridge stack.
Both real segments and application-relative pointers are returned to the guest.
PSP/environment selectors use separate host mappings.

Software interrupts and privileged I/O cause hardware exceptions. A 64-bit Linux
signal handler decodes supported requests, updates the saved context, and resumes
original instructions. Host handlers use an alternate signal stack and no libc.
Unknown operations stop explicitly with a register log. The 64-bit signal frame
does not reliably expose FS/GS selectors; native logs mark them `ffffffff`.
The startup's saved PSP/environment selectors are checked in guest memory.

The CPU probe leaves AC enabled before unaligned CPUID-result stores. Linux's
alignment-check behavior raises SIGBUS; an assembly signal veneer clears AC
before entering host C, and the guest resumes the faulting store with AC clear.
This profile does not reflect the original AC value through subsequent PUSHF.

## Implemented interfaces

The original startup still proves its selector limit, copied `gui` command/program
path, and complete BSS clearing of `[0x195fb0,0x4de2f0)`, deliberately seeded with
`0xa5`. Stack/heap translations use a declared no-swap allocation model and are
tested against original-extender state, return values, and arithmetic flags.
See [x32-services.md](x32-services.md) for reference measurements and offsets.

Additional services include dynamic LDT descriptors, vector/exception registration,
conventional allocation, memory residency, DOS time, file open/read/write/seek/close,
console output, and DOS exit. The original game reads both DID archives, config,
clump tables, GUI assets, and its main-menu data. Files are searched in the local
drive overlay before the external installation. First writable access copies an
original file into the overlay; new files are created there. Host filenames are
currently uppercase. C: root writes stay under `drive/`, including `TACTCOM.$$$`.

DOS AH=2Ch uses host UTC `CLOCK_REALTIME`, truncated to hundredths, with raw samples
retained in the report. Polls are paced at no more than 50 Hz using monotonic
deadlines. The original physics loop clamps each frame to at least one 50 Hz
tick; pacing prevents fast native frames from advancing simulation ahead of
wall time. Missed deadlines are discarded rather than followed by catch-up bursts. PIT programming drives Linux interval-timer signals;
IRQ0 enters the installed guest handler through a 32-bit IRET frame and updates
BIOS ticks. PIC acknowledgement permits subsequent interrupts. Masked/coalesced
ticks are approximate. CLI/STI update a virtual interrupt state; PUSHF/POPF
reflection and full interrupt priorities are unfinished.

SDL key events become PC/AT set-1 make/break sequences, including E0/E1
prefixes. A bounded ring in the reserved DOS window queues bytes for port 60h;
port 64h reports pending data. IRQ1 enters the game's installed keyboard handler.
After EOI, handlers that return without STI are traced through their short
epilogue so IRET restores the virtual interrupt state. IRQ1 uses the same PIT
delivery opportunities as IRQ0; it is not a cycle-accurate keyboard controller.
Focus loss queues releases for all held keys. INT16 also exposes BIOS key words
and basic shift flags.

SDL joystick axes/buttons are synchronized alongside framebuffer snapshots.
Writes to port 201h charge four axis counters; subsequent reads discharge each
connected axis after 1–127 polling steps, independently of host syscall speed.
The game's own calibration interprets the measured counts. Buttons are active
low on bits 4–7; absent axes remain high. Only the first four selected axes and
four buttons are represented, matching the legacy game port. Unplugging clears
presence and button state. Loss of window focus centers axes and releases buttons.
The SDL window supplies mouse position/buttons and motion counters through INT 33h.

## Real-mode and video bridge

X-32 AX=2511h marshals its measured 18-byte register block to a bounded Unicorn
16-bit engine. Native and emulated execution exchange the DOS memory window;
drivers execute their own code, including Sound Blaster detection against a
declared absent device. Unsupported nested interrupts or ports stop explicitly.
`--sound probe` retains original audio settings for this diagnostic path;
`--real-mode stop --sound probe --headless` reproduces the old first-driver boundary.

BIOS video and mouse requests use the same bridge. The virtual video adapter
supports VGA 13h and indexed banked VBE modes 100h, 101h, 103h, and 105h, with
controller/mode information, bank changes, scanline length, and display start.
Its layouts follow the [VESA VBE Core specification](https://www.phatcode.net/res/221/files/vbe20.pdf).
The VGA DAC supplies the original six-bit palette. Bank-window changes preserve
the complete VRAM image; SDL displays those pixels with a 4:3 presentation.

`--headless --seconds N` stops at the requested time and exports the final image.
Normal window closure and a requested time limit return success after the startup
checkpoint; unsupported interfaces, guest faults/exits, and exhausted models fail.
There is no service-count cutoff: normal flight performs frequent selector updates.
The descriptor-update log retains the first 128 calls and then samples powers of
two. Use `--seconds` to bound unattended runs.

## Artifacts and verification limits

Each run retains the extracted payload, compiled harness, build/call logs, JSON
report, local drive overlay, and final DOS memory snapshot. Graphics runs also
retain `video.json`, `vram.bin`, and `framebuffer.png`. Driver diagnostics may
add `conventional.bin` (the full physical `[0,0x100000)` window). These artifacts
contain original game bytes and stay in ignored local/build storage.

The main menu was verified visually from native output. Automated original-file
tests check the banked GUI mode and PNG export, alongside the unchanged executable
and asset hashes. Synthetic guests additionally exercise native memory, syscalls,
descriptor loading, timer delivery, alignment recovery, overlay writes, and the
BIOS bridge without original assets. Green asset-free package tests alone do not
prove menu rendering.

Other exception-frame delivery, swapping, audio,
additional device APIs, and gameplay validation remain outstanding.

## Segment diagnostic and QEMU

```sh
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/limits --check-segment-limit
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/qemu-probe --backend qemu --real-mode stop --sound probe --headless
```

The native diagnostic must fault on DS offset `0x40` below the expand-down limit.
That expected #GP is diagnostic success. QEMU reaches the same first-driver call
and startup checkpoint, but previously failed this lower-bound diagnostic; it
remains a comparison backend rather than evidence of segment-fault equivalence.
Both profiles now use the same image base, 64 MiB layout, DOS window, and shared
bootstrap. Some QEMU signal frames lack trap numbers, requiring explicit supported
opcode decoding for I/O faults.

The window captures the mouse for relative movement. Press Escape to release
the desktop pointer; click the window to capture it again.


Mission startup now supports DOS date queries (AH=2Ah), time queries (AH=2Ch)
from arbitrary call sites, and committing open files (AH=68h). Date and time
use UTC. Vendor mouse extension requests leave registers unchanged, matching
the regular [DOSBox-X mouse driver](https://github.com/joncampbell123/dosbox-x/blob/master/src/ints/mouse.cpp);
unimplemented standard mouse functions still stop explicitly.

The PIT signal sends host-only framebuffer snapshots at most 30 times per second,
so direct VGA drawing remains visible after the loading screen. These snapshots
preserve the guest registers and do not invoke a guest interrupt. Quick Combat
was verified under a private Xvfb display through loading into a moving cockpit,
HUD, and terrain. Keyboard make/break and virtual-joystick flight input are now additionally verified;
physical joystick hardware and audio remain unverified.
All automated UI verification must use Xvfb, never the user's desktop display.


The reserved host input mailbox occupies DOS F1400h–F1507h (magic INP1,
16-bit consumer/producer indices, 256-byte ring), with joystick presence/buttons
and four 16-bit discharge counts at F1600h. The parent writes only available
ring slots; the guest controller advances the consumer index. Pending break
codes survive backpressure. These bytes are outside loaded DOS drivers and VGA
memory.

Opt-in SDL integration tests require an explicitly named private display:
`EF2000_TEST_DISPLAY=:193 python -m unittest discover -s tests -p test_sdl_input.py`.
They exercise actual SDL events, a virtual joystick, axis inversion/remapping,
focus loss, and device disconnection. Native synthetic guests verify IRQ1
make/break bytes, IRET-only handlers, controller acknowledgements, and independent
game-port axis discharge/buttons.


Divide exceptions now enter the installed X-32 handler through its measured
IRET/context-pointer ABI. The handler receives the original fault EIP and
register/segment context and supplies the recovery EIP. The host resumes at that
address; it does not replace or skip the game's renderer logic. This covers
the signed 16-bit projection overflow at original EIP C5A7Ch. Nested exceptions,
other exception vectors, and arbitrary stack/segment-context edits remain
unsupported. The reproducible disposable-copy probe is
`tools/reference_run.py --exception-probe`; measured metadata and resumed
register sentinels are in `docs/exception-reference.json`.
