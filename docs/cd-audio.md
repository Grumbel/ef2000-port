# CD audio

Run the installed game with the original-release mixed-mode disc:

```sh
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/cd-session --sound sb16 --cd-cue ../EF2000_ISO/EF_DIR.cue
```

`--cd-cue` enables CD music (`MUSIC=2,127`) and selects the supplied Miles
`NULL.MDI` in the writable overlay. The game initializes Miles even in CD mode;
its original FM configuration would otherwise attempt unsupported hardware.
The disc image is opened read-only. Currently the reader supports consecutive
tracks in one BINARY image with AUDIO and MODE1/2352 tracks. CD PCM is captured
under `cd-audio/`; `requests.json` records command/status and play ranges.
Windowed playback mixes CD music and SB16 effects into one SDL device. Headless
runs capture WAV files without opening audio hardware.

## Native C++ Unicorn backend

The C++ runner now independently implements the read-only MSCDEX and CUE/BIN
reader. Enable it explicitly (a directory by itself is not sufficient):

```sh
nix run .#unicorn -- ../EF2000/c/EF2000/_EF2000.EXE \
  --windowed --sound sb16-dma \
  --cd-dir ../EF2000_ISO/ --cd-cue ../EF2000_ISO/EF_DIR.cue
```

`--cd-dir` supplies disc *files* to the DOS drive. `--cd-cue` supplies the
mixed-mode audio sectors, discovers a simulated D: MSCDEX drive, and presents
`MUSIC=2,127` plus `NULL.MDI` in the virtual `EF2000.CFG` and `MDI.INI` reads
without modifying the original installation. One CUE `FILE ... BINARY` with
consecutive indexed MODE1/2352 and AUDIO tracks is supported, using relative
paths from the CUE. Requests use the same X-32 real-mode bridge used for the
game's original SB16 driver. SDL3 automatically mixes two independently
bound audio streams into the same playback device; stopping/restarting a CD
track clears queued music without cutting off effects.

For a headless diagnostic, add `--wav-capture DIR`. CD stereo 16-bit 44100 Hz
samples are saved under `DIR/cd-audio/`, separate from any captured SB16
effects. Captures never overwrite existing files, and no disc sectors are
written. CD audio also works with `--sound off` and a cue file. The console
reports MSCDEX request count, decoded tracks, delivered PCM blocks and bytes.

`tests/cd_audio_test.cpp` tests drive discovery, version, ISO volume descriptor,
disc/track/device IOCTLs, rejection of a data-track play, a valid play range,
elapsed-sector PCM bytes, stop, and reset using a generated test disc. The
C++ integration and SDL3 playback with the real disc have not been exercised
in this development environment (SDL3 development files and the user's CUE/BIN
are unavailable here), so the real game's music should be checked on the
target machine.

### CRT retrace read during Miles initialization

Enabling CD music makes the original game initialize the Miles `NULL.MDI`
driver even if FM music is disabled. Its delay routine calls X-32 selector
services 3501h/3503h/3505h to map guest physical address 0400h, then reads the
BIOS Data Area's CRT controller word at `0040:0063` (03D4h) and polls port
03DAh. The C++ emulator initially mapped that descriptor into the game image
instead of the separate 1 MiB DOS-memory window. Merely redirecting the
descriptor was insufficient: the game loads DS *before* setting the descriptor
base, so Unicorn's cached DS base must also be refreshed when 3503h/3505h
change a live selector. Otherwise, the game reads a spurious port such as
2034h or 0006h and stops at `unsupported port IN`.

`--self-test-bda` exercises that precise order using a synthetic protected-mode
x86 program. An incorrect mapping or stale segment cache triggers an explicit
test failure; a successful run exits after five X-32/DOS interrupts.

## Interface recovered from the installed executable

Offsets refer to the extracted protected-mode payload of the German TACTCOM
executable fingerprinted in [executable.md](executable.md). These are static
instruction observations, with startup disc queries and the original play
routine also executed through the native bridge.

The game calls the real-mode wrapper at `0x8b8`, which invokes X-32 INT 21h
AX=2511h. This already reaches `RealMode.execute`; no game patch is needed for
MSCDEX interception. The simulated drive is D: (DOS drive number 3).

| Routine | Request |
| --- | --- |
| `0xddc88` | INT 2Fh AX=1500h, discover drive count and first drive |
| `0xddd48` | AX=1510h, driver command 03h, IOCTL input 0Ah, disc info |
| `0xdde80` | command 03h, IOCTL input 0Bh, track start/control |
| `0xddf68` | command 03h, IOCTL input 06h, device status |
| `0xde004` | command 0Ch, IOCTL output 00h, eject |
| `0xde094` | command 0Ch, IOCTL output 05h, close tray |
| `0xde12c` | command 0Ch, IOCTL output 02h, reset |
| `0xde1bc` | command 03h, IOCTL input 09h, media change |
| `0xde258` | AX=1505h, volume descriptor read |
| `0xde2d8` | command 03h, IOCTL input 0Ch, Q-channel info |
| `0xde3b4` | command 84h, play audio |
| `0xde4b0` | command 85h, stop audio |

The IOCTL request is 26 bytes: command at +2, status at +3, transfer far pointer
at +14, transfer length at +18. Track starts are returned as binary frame,
second, minute, zero bytes; the game converts them to sectors and subtracts 150.
The play packet is 22 bytes, with address mode at +13, start sector at +14,
and length at +18. The original code zeroes this packet, so mode is sector
addressing (0). Length is the disc lead-out minus start, rather than the next
track minus start. It expects status 0300h after successful play (done/busy).

The implementation handles discovery, version, drive check, volume descriptor,
disc info, track info, device status, media change, reset, play, and stop.
Unsupported driver commands return an error status. Eject/close and full
Q-channel reporting remain unsupported.

A disposable entry probe calling original routines `0xddbe0` (initialization)
and `0xde3b4` (track 2) produced play range 59822–240630 and status 0300h.
A 15-second headless run captured 659736 stereo frames at 44100 Hz, including
1315181 nonzero samples, peak 25302. Original game assets were not patched;
only a local extracted payload was instrumented for this probe.

Miles' initialization also reads the CRT base port at BIOS address 0463h via
an X-32 selector with physical base 0400h. User descriptors into the first MiB
now map to the harness DOS window, preserving their guest-visible bases.

A subsequent normal Quick Combat run on private Xvfb with SDL dummy audio
entered the cockpit and issued the same successful play request. Its CD capture
contained 1,092,504 frames, 2,180,619 nonzero samples, and peak 26,564.
SDL accepted 4,370,016 converted CD bytes while SB16 also queued 5,001,216 bytes;
neither source reported a queue reset. This verifies game-driven music and
shared-device output, without a listening-quality assessment. The game first
attempted the data track, received an error, then advanced to track 2 through
its existing retry logic.
