# Code overview and progress, 2026-11-12

Static survey of the whole function inventory (`decomp/symbols/functions.tsv`,
4,290 functions, 909,123 bytes between `0x86` and `0x1603f1`). Numbers are
bytes unless stated. Nothing here is original code; only counts and addresses.

## Classification of the former `unclassified` functions

738 functions / 507,391 bytes were `kind=unclassified`. A static pass over the
Ghidra export (`local/ghidra/full-export/calls.tsv`), the disassembly and
pointer references in the initialised image sorted them:

| Evidence | Functions | Bytes | New kind |
|---|---|---|---|
| Reachable by direct calls from already recovered code | 552 | 407,343 | game |
| Referenced from data tables (callback/dispatch targets) | 78 | 28,395 | game |
| Called only from other unreached code | 35 | 17,201 | game |
| Address taken by an immediate inside code | 6 | 5,173 | game |
| Uses `int 21h/31h/2fh` or port I/O | 26 | 11,640 | asm |
| No static reference at all (computed dispatch or dead) | 41 | 37,639 | unclassified |

No library-like routines were found among them: none of the 738 matches the
size/call-fan-in profile of the Watcom runtime, and only 26 touch DOS or
hardware. The "unclassified" bytes are ordinary, not yet recovered game logic.
The classification is heuristic (reachability, not proof of authorship); the
tracker `kind` is a working label, not a verification claim.

## Progress in bytes

| Stage | Functions | Bytes | % of 909,123 |
|---|---|---|---|
| All functions | 4,290 | 909,123 | 100 |
| Library, X-32, thunk, hardware asm | 86 | about 2,800 | 0.3 |
| Game code (incl. reclassified) | 4,163 | 858,067 | 94 |
| Recovered C that compiles | 3,249 | 363,851 | 40.0 |
| of those, excluded (original kept) | 826 | 79,944 | 8.8 |
| Linked into the hybrid image | 2,423 | 283,907 | 31.2 |
| Differential PASS (harness or fuzz) | 1,276 | 140,742 | 15.5 |
| Differential SOFT (preserved registers only) | 282 | 40,270 | 4.4 |
| Linked, no usable check yet | 865 | 102,895 | 11.3 |
| Game code not yet recovered | | 494,216 | 54.4 |

"Linked" is computed as compiled minus the union of the hybrid exclusion list
and the last link report's exclusions, so it is lower than the 2,847 of the
earlier report. Verification is behavioural (original-vs-patched under the
emulator), not byte-exact. Byte-exact matching has not been measured at scale.

Not yet recovered, by state: unknown 430,170; named 35,048; drafted 20,507;
typed 6,619; blocked 1,012; compiles-bad 860.

## Where the remaining code is

Per 128 KiB of the image (compiled = recovered C that compiles):

| Range | Total | Compiled | Remaining |
|---|---|---|---|
| 0x000000 | 100,290 | 32% | 67,688 |
| 0x020000 | 111,387 | 32% | 74,822 |
| 0x040000 | 107,565 | 36% | 67,940 |
| 0x060000 | 106,717 | 32% | 72,165 |
| 0x080000 | 109,838 | 45% | 60,090 |
| 0x0a0000 | 106,841 | 49% | 54,292 |
| 0x0c0000 | 93,639 | 44% | 51,591 |
| 0x0e0000 | 87,984 | 46% | 46,841 |
| 0x100000 | 81,964 | 41% | 47,827 |
| 0x120000 and above | 2,898 | 29% | 2,016 |

The low half of the image (0x0 to 0x80000) is the least recovered at about a
third. Remaining functions by size: under 128 bytes 13,450 (218 functions);
128 to 511 bytes 119,325 (364); 512 to 2,047 bytes 309,706 (346); 2 KiB and
larger 89,374 (27). 232 of them with 204,449 bytes use x87 floating point
heavily, which the current C/ABI tooling handles poorly (see the x87
exclusions). Largest: `0x3a3a8` (6,328), `0xb69e4`, `0x95e58`, `0x46454`,
`0xc1ee8`, `0x30214`.

## Open verification work

- 215 isolated fuzz failures, 35 SOFT, 2 weak after the corrected rerun
  (see `decomp-log.md`); the 1,428 harness errors left 879 functions that
  random fuzzing cannot judge (DOS, interrupt and x87 style code).
- Excluded for ABI reasons: stack/x87-convention functions, 247 unwrapped
  stack-argument functions, 77 module-level link exclusions.
