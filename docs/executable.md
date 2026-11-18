# Executable and startup findings

## Examined installation

Source: `../EF2000/c/EF2000/_EF2000.EXE`, 1,684,561 bytes.
SHA-256: `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
The installed German README describes Tactcom. This identifies the installation's
documentation; the exact executable release still needs independent confirmation.
Machine-readable observations are in [executable-baseline.json](executable-baseline.json).

## English release comparison

The English installation at `../EF2000-rdeprera/` supplies a 1,682,129-byte
`_EF2000.EXE` with SHA-256
`ff245c9f812d4113c4a2a8fe60fb9a0e45687bc6ae73c19e4c23c32bccd2ec49`.
Both executables decode as B23X / FlashTek X-32, with payload file offset
`0x54a1` and protected-mode entry at payload offset `0x86`. Only 20 bytes
before the payload differ. The English payload ends at `0x195630` and its
BSS ends at `0x4dd970`. Both boundaries are `0x980` lower than in German.

Of the English payload's 1,660,464 bytes, 1,401,941 (84.4%) match at the
same offset. In the local German bulk inventory
(`local/ghidra-group-followup-20261118/bulk-gameport-inventory`),
1,265 of 4,360 entries with instruction spans have identical bytes across
their full span at the same offset. Of 278,920 unique instruction spans,
36,567 differ. In 31,614 of those spans, only the trailing four bytes
change and their little-endian value decreases by `0x980`. These counts
use the German boundaries for both payloads and provide candidates for
operand and control-flow review.

| Reference | German address | English address |
| --- | ---: | ---: |
| Append spatial slot entry | `0x263ec` | `0x263ec` |
| Spatial slot array used at `0x26412` | `0x19b2e0` | `0x19a960` |
| Spatial slot count used at `0x26404` | `0x19b376` | `0x19a9f6` |
| Current high score loaded at `0x43e2` | `0x1970a0` | `0x196720` |
| Difficulty loaded at `0x43dd` | `0x121e44` | `0x121e44` |

The native64 launcher recognizes both source hashes. It links the
harness with the initialized and BSS boundaries from the selected X-32
header. The allocation starts at `0x4de000` for English and `0x4df000`
for German. The startup interrupt sites coincide in both builds. The
Watcom command, program, PSP, environment and extender-selector globals
are `0x97c` lower in English. The VGA selector and base fields at
`0x120cc4` and `0x120cc6` retain their addresses.

The hybrid launcher links an English-payload-bound patch for the two
ASCII case helpers at `0xddbc8` and `0xe7835`. Both have the same
entry address, register ABI, and instruction bytes across editions.
Their dedicated OpenWatcom object is relocation-free. The linker checks
the compiled bodies against the English payload before redirecting the
two entries. The rest of the English executable runs with its original
code in the same native64 runtime.
Both Nix apps reached the English main menu, Quick Combat setup, and
the Mission 01 cockpit on private Xvfb displays. Holding Left for four
seconds banked the aircraft in both apps. An isolated differential comparison
executed both redirects and matched the original general-purpose registers,
EFLAGS, stack pointer, and return address for 261 inputs per helper.

## Verified observations

- `EF2000.BAT` sets `DID=.` and `quick_flag=1`, then runs `_ef2000.exe gui`.
- The MZ header declares three 512-byte pages and a 48-byte header. DOS initially
  loads the image ending at file offset `0x600`; most file bytes lie beyond it.
- Initial CS:IP is `0:0x137` relative to the load module: file offset `0x167`.
  SS:SP is `0:0x521`. Two MZ relocations target file offsets `0x42` and `0x38`.
- ASCII `B23X` occurs at file offset `0x28`. The dword at `0x3c` is zero;
  it does not point to a conventional extended executable header.
- An extender copyright naming Doug Huffman starts within a string at `0x755`.
  Watcom C/C++32 runtime text occurs at `0x54c1`; an X386 virtual-memory diagnostic
  occurs at `0x841`. `RATIONAL DOS/4G` also occurs much later, at `0x19a699`.
  Presence of `DOS4GW.EXE` beside the program does not establish its loader format.

## Startup disassembly

These offsets are relative to the load module (file offset minus `0x30`).
Observed instructions, not a complete control-flow reconstruction:

| Stub offset | Instruction/operation |
| --- | --- |
| `0x13b` | Set AH=`0x4a`, followed by INT `0x21` at `0x141` |
| `0x156` | Set AX=`0x1687`, followed by INT `0x2f` at `0x159` |
| `0x1a7` | Set AX=`0xde00`, followed by INT `0x67` at `0x1aa` |
| `0x28a` | Set AX=`0x3d00`, followed by INT `0x21` at `0x28d` |
| `0x29d` | Set DX=`0x451`, CX previously zero; AX=`0x4200`, INT `0x21` |
| `0x3c5` | Set AH=`0x3f`; subsequent setup and far return at `0x3d5` |

The interrupt sequences are consistent with DOS memory/file operations and
DPMI/VCPI detection. Their complete runtime behavior has not yet been traced.
Subsequent analysis identifies FlashTek X-32 through matching startup source and
recovers the second-stage loading path; see [loader.md](loader.md). This does not
yet amount to a complete generic format specification or runtime verification.
Watcom officially supports multiple extender targets, including FlashTek:
[Open Watcom linker guide](https://downloads.openwatcom.org/ftp/manuals/current/lguide.pdf).

## Reproducing the disassembly

With GNU objdump available, from the repository root:

```sh
mkdir -p local
python3 - <<'PY'
from pathlib import Path
image = Path('../EF2000/c/EF2000/_EF2000.EXE').read_bytes()
Path('local/stub.bin').write_bytes(image[0x30:0x600])
PY
objdump -D -b binary -m i386 -M addr16,data16,intel \
  --start-address=0x137 --stop-address=0x3d6 local/stub.bin
```

Linear disassembly can decode embedded data as instructions. Use branch targets
and runtime traces before identifying functions or inferring instruction coverage.
