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
playback is implemented; FM synthesis and CD audio remain unsupported.

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
CD audio is deferred. Headless runs record PCM without opening an audio device.
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
