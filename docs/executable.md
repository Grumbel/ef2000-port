# Executable and startup findings

## Examined installation

Source: `../EF2000/c/EF2000/_EF2000.EXE`, 1,684,561 bytes.
SHA-256: `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
The installed German README describes Tactcom. This identifies the installation's
documentation; the exact executable release still needs independent confirmation.
Machine-readable observations are in [executable-baseline.json](executable-baseline.json).

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
