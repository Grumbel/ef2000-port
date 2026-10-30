# Startup symbols and interface worklist

All addresses are offsets in the initialized application image extracted from
the executable fingerprint in [executable.md](executable.md). Names below are
research annotations, not symbols recovered from a debug table.

## Recognized runtime routines

| Offset | Annotation | Evidence/confidence |
| --- | --- | --- |
| `0x86` | `__x386_init` | Strong: matches FlashTek Watcom startup source sequence |
| `0x9bd70` | `__InitRtns` | Strong: startup calls it with EAX=`0xff`, matching source |
| `0x9bd20` | `__CMain` | Strong: startup passes command buffer in EAX and clears EBP |
| `0x8b274` | Candidate game `main` | Inferred: called by `__CMain` after argument setup |

Verify the candidate main by tracing argument construction and its execution;
do not assume that its entire reachable code is gameplay rather than runtime.

## Recognized data locations

| Offset | Annotation | Evidence |
| --- | --- | --- |
| `0x132a28` | `__x32_stack_size` | Startup loads it before AX=`0x3509`; initial value `0x4000` |
| `0x132a2c` | `_dynend` | Receives `_end` in the source-matched sequence |
| `0x132a30` | `_curbrk` | Receives page-rounded `_end`, then result of stack setup |
| `0x132a34` | `_LpCmdLine` | Receives copied command buffer address |
| `0x132a38` | `_LpPgmName` | Receives copied program-name address |
| `0x132a3c` | `_psp` | Receives FS at entry |
| `0x132a61` | `_Envptr` | Fword pointer loaded by LDS before scanning environment |
| `0x132a65` | `_Envseg` | Receives GS at entry; overlaps selector part of `_Envptr` |

## First compatibility-layer boundaries

1. Establish the selector table, PSP command tail (`gui`), and double-NUL DOS
   environment including program-name trailer. Preserve `DID=.` and `quick_flag=1`.
2. Handle X-32 AX=`0x3505` descriptor limits and AX=`0x3509` stack/memory setup.
   Recover exact return values and side effects before implementing them.
3. Handle DOS AH=`0x30` version query at entry. Record the actual reference value.
4. Trace runtime initializers before adding game-level hooks; they may initialize
   x87, exception handling, allocation, drivers, and device access.

The initial selector-limit request, DOS version query, and non-DPMI stack/heap
success calculations now have implementations. PSP/environment setup and the
original BSS clearing are checked by the [startup harness](startup-runtime.md).
DOS AH=`0x2c` at application `0x11f2a4` now returns a recorded host UTC sample.
Game initialization now reads the archives/configuration and prints TACTCOM.
AX=`0x2511` at application `0x1112b7` marshals AIL3DIG driver requests to the
real-mode engine. The default local no-audio configuration continues through
timer/input initialization, VGA loading, and the original 640×400 VBE main menu.
Gameplay and full exception-frame delivery remain outstanding. Keep unknown
behavior as an explicit stop.
