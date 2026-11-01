# Sound initialization research

## Target and evidence

These findings concern the installed German TACTCOM executable, SHA-256
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
The digital driver is the original 2,836-byte `SB16.DIG`, SHA-256
`c8a1e7a0e14c8b696e665520e338711e4c20cc3bb4733f83527add41dd6f6b38`.
The source installation remains unchanged.

The installed DIG.INI selects Sound Blaster 16, base 220h, and automatic
IRQ/8-bit DMA/16-bit DMA discovery (-1). MDI.INI selects SBPRO2.MDI for FM
music. Digital playback and FM synthesis are separate paths. Experimental SB16 digital
playback is implemented; FM synthesis remains unsupported. Experimental CD playback is described in [cd-audio.md](cd-audio.md).

## Original-release CD audio source

The original-release disc supplied in `../EF2000_ISO/EF_DIR.cue` references
`EF_DIR.img`, a 565,961,760-byte raw image containing exactly 240,630 sectors
of 2,352 bytes. Track 1 is MODE1/2352; its sector-16 ISO primary volume
descriptor identifies the volume as `EF_DIR`. Tracks 2–7 are AUDIO.

| Track | INDEX 1 sector | Span to next INDEX 1 or image end |
| --- | ---: | ---: |
| 2 | 59,822 | 484.627 s |
| 3 | 96,169 | 252.213 s |
| 4 | 115,085 | 469.507 s |
| 5 | 150,298 | 484.800 s |
| 6 | 186,658 | 250.933 s |
| 7 | 205,478 | 468.693 s |

Sector positions use 75 sectors per second. These spans include the following
track's INDEX 0 pregap where present; they are not pregap-trimmed song lengths.
The CUE contains explicit INDEX 0 entries for tracks 3–7, which a future disc
reader should preserve. A one-second sample ten seconds into each audio track,
decoded as little-endian signed 16-bit stereo PCM at 44,100 Hz, contained
nonzero samples in every track. This verifies audio data is available; playback
and the game's CD control path have not yet been tested. Inspection was
read-only, and no disc content is included in the repository.

## Driver addressing

All driver offsets below are byte offsets in SB16.DIG, also offsets within
its loaded real-mode segment. The observed native load is at physical 11000h,
with INT66 entering 1100:016Ah. Addresses differ from protected-mode image
offsets and must not be transferred to another driver release.

## Observed driver interface

The header starts with `AIL3DIG` and byte 1Ah. Its little-endian word at +34h
is the real-mode dispatcher entry (016Ah). The dispatcher at 01AEh selects
a seven-entry table at 010Ah for AX=0300h..0306h and a six-entry table at
0118h for AX=0400h..0405h.

| Request | Driver offset | Observed behavior |
| --- | --- | --- |
| 0300h | 079Dh | Initialize driver-local state; returns AX=1 |
| 0301h | 07D1h | Return driver information/data structure pointers |
| 0304h | 0844h | Reset/version/resource discovery, then DMA/IRQ self-test |
| 0305h | 0993h | Install IRQ path and configure the selected mode |
| 0400h | 09DFh | Mixer/volume setting and query |
| 0401h | 09FCh | Program sampling rate and DMA playback |

The names in the last three rows are inferred from the instructions. The original
driver now completes its self-test and produces PCM during startup. In particular, 0304h is more than a presence
check: returning success without its interrupt self-test would conceal missing
hardware behavior.

At 0623h the driver asserts DSP reset, performs 20 reset-port reads, deasserts
reset, and waits for data AAh. At 064Eh it sends E1h and reads major then minor
DSP version bytes. The combined word is stored at driver offset 057Ah.
At 0867h/0896h it reads mixer registers 80h/81h and stores IRQ, DMA8, and DMA16
as little-endian 16-bit values at +18h/+1Ah/+1Ch; the base port is at +16h.

## Independent reference experiment

```sh
python3 tools/reference_run.py --game-dir ../EF2000/c/EF2000 \
  --output local/sb16-reference --sb16-probe --headless --seconds 10
python3 tools/read_sb16_probe.py local/sb16-reference/drive/EF2000/SB16.BIN
```

This executes a bounded read-only DSP probe in a disposable executable copy,
using DOSBox-X with an explicit SB16, 220h, IRQ7, DMA1/5 profile. No DMA is
programmed and no sound is played. `sb16-reference.json` records the decoded
samples, probe/executable/config hashes, and unchanged-source check.

Observed: reset returns AAh; E1h returns version 4.5; ready write status and
empty read status are 7Fh; mixer 80h is 04h (IRQ7), 81h is 22h (DMA1/5),
and 82h is 20h. Only low bits 0..2 of mixer 82h identify pending DSP/MPU
interrupts; the observed 20h does not indicate a pending audio interrupt.
The exact reserved/status values are a DOSBox-X profile, not a claim about
every physical card.

The register meanings and handshake are also described in Creative's
[Sound Blaster Series Hardware Programming Guide](https://www.ardent-tool.com/sound/Sound_Blaster_HW_Programming_Guide_1st.pdf),
chapters 2 and 6. The probe measurements are the implementation baseline.

## Native diagnostic model and current boundary

```sh
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/sb16-native --sound sb16-probe --headless
```

This mode is experimental discovery support, not working sound. It implements
only the measured reset handshake, DSP version FIFO, and read-only resource
registers. It disables FM music in the local config to isolate digital sound.
Normal runs still default to all audio disabled; `--sound probe` retains the
older absent-card diagnostic behavior.

The unchanged original driver reads DSP 0405h and stores 220h/IRQ7/DMA1/DMA5.
It then installs its real-mode vector 0Fh handler at offset 069Ah and reaches
IN AL,A1h at driver 0372h. That PIC mask access is the explicit stop for `sb16-probe`.
Unicorn's saved IP is 0374h, after the IN instruction; it is not the faulting
instruction's start. The full `sb16` model instead returns 0405h after the original IRQ handler runs.

The failure record in report.json includes CPU state. `conventional.bin` now
contains the failed driver's post-execution memory, not merely the pre-call
snapshot. At the observed 11000h load, verify:

- 11016h: base/IRQ/DMA8/DMA16 words = 0220h, 0007h, 0001h, 0005h.
- 1157Ah: DSP version word = 0405h.

`tests/test_sound_blaster.py` runs this exact original-driver path when
EF2000_EXE and Unicorn are available. It verifies both the parsed fields and
the explicit PIC boundary while checking the source driver's hash.

## Experimental digital playback

Use `--sound sb16` to enable digital effects and speech from the original driver.
Audio remains off by default. FM music is disabled in the local configuration;
CD audio can be enabled separately with `--cd-cue`; see [cd-audio.md](cd-audio.md). Headless runs record PCM without opening an audio device.
Windowed runs additionally send resampled stereo PCM to SDL queued audio.

The model implements PIC masks/acknowledgement, byte/word DMA addressing,
single-cycle and auto-initialized DSP transfers, and original real-mode IRQ7
execution. Overdue blocks continue DMA while pending interrupts coalesce.
The original self-test programs 22050 Hz, signed 16-bit stereo, a one-word DMA
transfer, and returns 0405h only after its handler increments the completion flag.
Streaming observed during startup uses 22050 Hz unsigned 8-bit stereo.

`audio-NNN.wav` files preserve source PCM, segmented at format/reset changes.
`audio.json` records rates, frame counts, nonzero samples, peaks, hashes, IRQ
count, and SDL queue statistics. Capture precedes master/PCM mixer gain;
playback applies those gains. Partial frames from the two-byte self-test are
discarded at format changes. SDL output is 44100 Hz signed 16-bit stereo.

Tests cover frame boundaries, signed/unsigned conversion, DMA/IRQ completion,
and interrupt coalescing. Set `EF2000_TEST_AUDIO=dummy SDL_AUDIODRIVER=dummy`
to exercise the real SDL resampler and queue without using speakers.
Startup and flight PCM plus SDL queueing are verified on private Xvfb with
SDL's dummy audio device. A 180-second original-game run reached Quick Combat,
entered the cockpit, and accepted weapon/gear input. A subsequent 131072-byte
flight interval contained 128378 non-silent samples and 227 distinct byte
values. This verifies the original mixer produces flight PCM and the host
queues it; listening quality, underruns, and long missions remain unverified.
Unknown hardware operations still stop explicitly.

For future rendering research, `../tfx3view` contains the Linux 3View/TFXplorer
port and the libTFX decoder for EF2000 shape bytecode and DAT archives.


## Joystick calibration during digital-sound startup

The original sound initialization at 8B51Eh calls 1C60Ch, which rescales joystick
calibration by `JOY_FACTOR` (stored at 122DC4h). The supplied config has
`JOY_FACTOR=0`; the executable's initialized default is 100. With a connected
joystick and digital sound enabled, a native debugger hardware watchpoint
observed 1C6AEh store zero at 420D96h, replacing the range of 63 loaded by
829C2h. D6969h/D6977h then skip flight joystick polling when either range is zero.
SDL still sees axis/button changes, explaining device registration without
flight response. Sound-off virtual-joystick flight continued to poll normally.

Selecting joystick controls now replaces only the zero scale with 100 in the
writable config; existing nonzero scales and original assets are preserved.
`--controls original` retains original settings. `--verbose` reports SDL
focus/axis/button changes and native `GAMEPORT` samples for comparison.
The correction was verified in original-code SB16 startup (both calibration
ranges remain nonzero) and private-Xvfb flight with a virtual joystick: roll
changes reached native game-port X counts 64/110/64, and button 0 reached the
game. The physical Xbox controller still needs a user retest.

## Native C++ Unicorn sound-driver bring-up

`ef2000-unicorn-cpp` now has a separate 16-bit Unicorn CPU for original DOS
driver vectors installed through the X-32 `2505h` service. The guest's 1 MiB
conventional memory and real-mode interrupt vector table are copied into the
driver CPU; modified memory, segment registers and general-purpose registers
are returned to the protected-mode caller after a real far `IRET`.
Unimplemented nested real-mode interrupts and ports stop with a diagnostic;
both instruction count and wall-clock execution are bounded.
The BIOS Data Area's CRT-controller base at `0040:0063` must be initialized to
`03D4h`: the original driver uses the corresponding `03DAh` retrace input as
a timing source. Without it, the driver polls a nonexistent port until the
real-mode execution bound expires.

`--sound sb16-probe` enables the existing digital-sound configuration while
disabling FM music, and exposes the measured SB16 DSP reset handshake,
version 4.5 and IRQ7/DMA1/DMA5 mixer resources at base 220h. If the original
driver reaches unsupported PIC/DMA ports or DSP playback commands, execution
stops with a diagnostic. This does **not** demonstrate an
original driver self-test pass, PCM capture or SDL audio playback. The default
remains `--sound off` and retains the previously validated menu/flight path.

Standalone C++ tests exercise the 16-bit `IRET` path, DOS memory exchange,
driver code-cache invalidation, bounded execution, nested interrupt failures,
real-mode SB16 `IN`/`OUT` discovery and the measured register responses.
`objects/ef2000-original-sb16-test /path/to/SB16.DIG` after a local Nix
build additionally runs the exact
2,836-byte original driver, checks its known SHA-256, initializes it and
verifies that its hardware self-test detects DSP 4.5, IRQ7 and DMA1/5, runs
the actual driver IRQ handler, and produces the two-byte original PCM test
transfer. Without the proprietary driver argument this test skips cleanly
(including in the Nix build). The full supplied EF2000 archive does contain
`SB16.DIG`, whereas the earlier reduced local fixture did not.

### Native C++ DMA and interrupt modeling

`--sound sb16-dma` adds 8237 DMA channel/page, address/count/flip-flop, mask,
mode and auto-init handling, 8259 interrupt mask/request/in-service/EOI, and
SB16 DSP rate, playback block, pause and resume commands. Sound device state
is independent of SDL3: completed DMA blocks retain the exact original guest
PCM, format, sample rate and mixer gains. The runner reports completed block
and byte counts, and windowed mode can stream normalized PCM through SDL3.

`tests/isa_audio_test.cpp` verifies 8- and 16-bit DMA, page wrapping, reload
and interrupt masking. `tests/sound_blaster_test.cpp` checks deterministic
timed PCM completion, multi-block auto-initialization and IRQ acknowledgement
against the Python reference. `tests/real_mode_cpu_test.cpp` additionally
runs a synthetic *16-bit guest program*: it programs DMA/PIC/DSP using real
`OUT` instructions, waits for completion, enters its own IRQ7 handler,
acknowledges PIC/DSP and returns via `IRET`. This exercises native Unicorn
I/O and interrupt injection without proprietary assets.

The C++ main-loop PIT tick now polls overdue DMA while protected-mode guest
code runs. If IRQ7 becomes unmasked and pending, it copies conventional RAM
into the real-mode CPU, enters the installed 16-bit ISR with a synthetic
hardware interrupt stack and interrupts disabled, runs the handler until
`IRET`, and copies any memory mutations back to the protected-mode DOS map.
A dedicated synthetic test covers this deferred completion path, including
an IRQ handler that loads its own DS. Audio service failures stop the guest
with a diagnostic.
The real SB16 driver also modifies the physical DOS interrupt vector table
directly. Background delivery now observes those live 0000:0000 vectors
instead of relying solely on the last X-32 interrupt-vector registration;
real-mode vector mutations are copied back to the protected guest memory.

Windowed runs now convert unsigned/signed 8/16-bit original PCM to interleaved
stereo float samples with mixer gains and partial-frame carry, then feed SDL3
an audio stream. SDL3 performs sample-rate conversion; queue growth is bounded
and a missing host audio device only disables sound playback. Headless runs
still report modeled PCM blocks without playing them. Optional
`--wav-capture DIR` saves the unmodified guest DMA frames (converted only to
the standard WAV sample signedness), splitting files on DSP generation or
format changes. WAV cannot encode an incomplete multi-channel frame, so a
trailing partial frame is discarded; no empty WAV segments are emitted and
existing capture filenames are not overwritten. The format converter
has deterministic tests for signedness, gains and split-frame boundaries.

With the full original EF2000 archive, sound-enabled C++ replays reached the
original in-flight cockpit after 1.5 billion guest instructions: 287–298 SB16
DMA blocks, 585,730–608,258 PCM bytes (host wall-clock audio timing varies
between runs). A shorter 300M-instruction replay exported 137,216
complete-frame PCM bytes to WAV (68 DMA blocks; two startup self-test bytes
were an incomplete stereo frame). The original driver's initialization and
hardware self-test pass in native C++ as described above. The limited earlier
fixture alone cannot initialize sound because it lacks `SB16.DIG`.
Audible playback on a real SDL3 audio device has **not** yet been validated.
