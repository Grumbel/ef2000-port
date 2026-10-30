# Recovered loader and application layout

## Scope and evidence

Applies only to the executable fingerprint in [executable.md](executable.md).
These results come from static disassembly, cross-checked against the locally
installed Open Watcom 1.9 `src/startup/386/cstrtx32.asm`. That source identifies
itself as FlashTek X-32 startup code modified by Doug Huffman. Its SHA-256 is
`dc66bc152bb53d116ffe75af7f23d213aaa6536b41eef2b6b0f2be1698a71675`.
The application entry's selector handling, command-line copying, environment
scan, BSS clearing, and extender calls match that startup sequence. This is
substantially stronger identification than an isolated embedded string.

This is a version-specific loader recovery, not a complete specification of
`B23X`, and not yet a runtime trace.

## Address conventions

`L` is the real-mode load-module segment chosen by DOS (not the PSP segment).
`S` is the second-stage data segment, `L + 0x10`. `F` is an absolute file offset.
Application offsets below are relative to the 32-bit code/data selectors, whose
linear base is chosen by the extender; they are not host virtual addresses.

## First-stage fields and transfer

The MZ load module starts at `F=0x30`. It contains these little-endian fields:

| File offset | Width | Value | Observed use |
| --- | --- | --- | --- |
| `0x30` | 2 | `0x451` | File offset passed to DOS seek |
| `0x32` | 2 | `0x5050` | Byte count of the second-stage read |
| `0x34` | 4 | `0x195fb0` | Passed in ESI to the next stage |
| `0x38` | 2 | `0x95` | Second-stage entry segment relative to L |
| `0x3a` | 2 | `0x10` | Added to CS to obtain second-stage data segment |
| `0x40` | 2 | `0x5830` | New SP loaded with LSS |
| `0x42` | 2 | `0x10` | New SS relative to L, relocated by DOS |

DOS also relocates the word at `0x38`. The stub obtains the INT 21h handler
through AX=`0x3521`; later it builds a return frame and transfers directly to
that handler for AH=`0x3f`. This reads `[0x451, 0x54a1)` into `L:0`, overwriting
the original stub. On return, execution continues at `(L+0x95):0`, corresponding
to `F=0xda1`, where a jump leads to `F=0xeca`.

That distinction matters: the loader's `0x451` is a **file offset**, whereas its
segment offsets refer to the destination of this new read.

## Second-stage relocations

The new block begins with 32 unsigned 16-bit relocation offsets followed by a
`0xffff` terminator (66 bytes total). At `F=0xee1..0xeea`, the loader adds `S`
to the word at `S:offset`, modulo 65536, for every table entry. Offset zero is
valid; it is not a terminator. Targets are relative to the data segment,
`0x100` bytes into the newly read block.

The data header at `F=0x551` contains the Watcom startup fields:

| Relative to S | Width | Initial value | Interpretation |
| --- | --- | --- | --- |
| `0x0` | 2 | `0x4f5` | `_TEXT` segment, adjusted by the relocation pass |
| `0x2` | 2 | `0x128` | Debug hook offset, from startup source structure |
| `0x4` | 4 | `0x195fb0` | `_edata`, end of initialized application bytes |
| `0x8` | 4 | `0x4de2f0` | `_end`, end of BSS |

## Protected-mode image

The first read leaves the executable file position at `0x54a1`. The next-stage
setup queries that position at `F=0xf42..0xf4d`. The application read path at
`F=0x4d0b..0x4d39` uses the declared image length and the buffered read/copy loop
at `F=0x176f..0x180c`. Its destination uses selectors whose base is established
by the descriptor setup at `F=0x4979..0x49cd`.

| Property | Value |
| --- | --- |
| Initialized file bytes | `[0x54a1, 0x19b451)` |
| Initialized application offsets | `[0, 0x195fb0)` |
| Startup BSS clearing | `[0x195fb0, 0x4de2f0)` |
| Protected-mode entry | Application offset `0x86`, file offset `0x5527` |
| Initialized payload SHA-256 | `3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb` |

The DPMI path transfers to offset `0x86` via a 32-bit far return at `F=0x52b4`.
The other inspected path also constructs an entry at `0x86` (`F=0x4ee6`).
No application relocation stream is consumed in the inspected read path;
absolute application operands refer to offsets within the selectors. That does
not establish the absence of all later runtime patching or self-modification.

## Startup state still required

The extracted payload is not sufficient to launch the game. The extender patches
application memory, including a zero-base selector at application `0x120cc4`,
a base-related value at `0x120cc6`, and a 16-bit data selector at `0x132a24`.
Entry requires DS/ES for application data, FS for the PSP, and GS for the DOS
environment. SS and ESP also depend on extender setup and memory allocation.

The startup calls INT 21h AX=`0x3505` and AX=`0x3509`; these are extender
interfaces, not ordinary DOS vector queries with arbitrary interrupt numbers.
Preserve their X-32 semantics when implementing the compatibility layer.

## Reproduce extraction

From `ef2000/`:

```sh
python3 tools/extract_image.py ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/image-01 --load-segment 0x1000
objdump -D -b binary -m i386 -M intel \
  --start-address=0x86 --stop-address=0x1de local/image-01/payload.bin
```

The output directory must be new. It contains the raw payload, both statically
relocated loader stages, and a manifest with coordinate conventions and remaining
runtime requirements. The tool refuses unknown executable hashes.
