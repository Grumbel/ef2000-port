<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# OpenWatcom library fingerprinting (Phase 1, §3.6)

The current library matcher is an **offline, proposal-only** tool. It does
not rewrite symbols, and its input signatures are not evidence of a specific
Watcom version until checked against the SHA-pinned EF2000 payload and its
calling convention. The original binary used Watcom 10.x-era components;
OpenWatcom 1.9/2.0 library code may be different.

## Prepare fingerprints

Build or obtain the corresponding 32-bit OMF runtime libraries (clib3r,
clib3s, math387r) under each OpenWatcom version. Extract their individual
object modules to **separate** directories; do not mix versions or source
libraries. The official WLIB manual documents extraction with
WLIB -x -d=DIRECTORY LIBRARY.lib, which does not remove the objects from
the library. For example, given an existing clib3r.lib:

~~~sh
mkdir -p local/ow-libs/v2/clib3r
wlib -x -d=local/ow-libs/v2/clib3r /path/to/clib3r.lib
python3 tools/flirt_match.py build local/ow-libs/v2/clib3r \
  -o local/ow-libs/v2-clib3r-signatures.json
~~~

Replace /path/to/clib3r.lib with the actual installed library path. The
flake provides OpenWatcom, but not a checked-in copy of its libraries.
Nix's open-watcom-v2 may have a different library layout than the historical
product. Verify architecture and code model before interpreting a match.

The build command accepts one or more .obj paths or directories (including
uppercase .OBJ filenames) and uses the first 64 bytes of each public
function in a CODE/_TEXT segment. It parses 16/32-bit SEGDEF, PUBDEF,
LEDATA and FIXUPP; address bytes named by relocation records are masked.
The minimum is 16 non-relocated bytes. Unsupported OMF module features
such as LIDATA and COMDAT are **skipped with a diagnostic**, not guessed;
use --strict to abort instead. The generated JSON includes code bytes
derived from the library, so it belongs in ignored local/, never in git.

## Compare against the actual payload

~~~sh
python3 tools/flirt_match.py match \
  --signatures local/ow-libs/v2-clib3r-signatures.json \
  --image local/ghidra/payload.bin \
  --functions decomp/symbols/functions.tsv \
  -o local/ow-libs/v2-clib3r-proposals.json
~~~

The matcher checks *known function entry points only*. It requires an exact
match of all non-relocated prefix bytes and a tracked function size large
enough to contain that prefix. This avoids treating arbitrary text or data
as a function. A unique result means a one-to-one match **within that
specific signature set**, not verified runtime identity. Overlapping
signatures, short functions, missed relocation kinds, different compilers,
link-time optimization and post-link edits all create false negatives or
ambiguity. In particular, a matching code prefix does not establish that
the whole function body or its return ABI is equivalent.

For each proposal:

1. Compare original instruction boundaries, length, callees, globals and
   at least two callers (including observable register postconditions).
2. Cross-check matches from 1.9 and 2.0 and investigate collisions; where
   possible compare independent 10.x symbol evidence.
3. Only after manual review, put the confirmed name, address and kind=clib
   in decomp/symbols/functions.tsv with an evidence note.
4. Keep the unchanged emulator as the runtime oracle and continue the
   function-level differential/replay requirements for any replacement.

The implementation is tested with synthetic OMF modules and 32-bit fixups,
but **has not yet been tested against actual WLIB-extracted Watcom runtime
objects** in this environment. No runtime library functions were
automatically classified by this tool.

## Independently recognized startup routines

Three routines are already identified from the original application's
disassembly and startup source/call-sequence comparison, *not* by OMF
fingerprints:

| Address | Tracker name | Kind | Evidence |
| --- | --- | --- | --- |
| `0x00000086` | `x386_startup` | `x32` | Entry sequence agrees with FlashTek's Watcom `cstrtx32.asm`; see `docs/loader.md` |
| `0x0009bd20` | `watcom_cmain` | `clib` | Startup command-line/EAX and EBP setup before the candidate game `main`; see `docs/startup-symbols.md` |
| `0x0009bd70` | `watcom_init_routines` | `clib` | Watcom initializer-table walk invoked with EAX=`0xff`; see `docs/startup-symbols.md` |

These are marked `state=library` in `decomp/symbols/functions.tsv` with
`sig_status=unverified`: the identities are strong, but they have not been
cross-checked against a matching Watcom 10.x object library. Do not inflate
the OMF matching statistics with these source-backed identifications.

Format details: [TIS OMF Specification v1.1](https://openwatcom.org/ftp/devel/docs/omf.pdf)
and [OpenWatcom WLIB manual](https://open-watcom.github.io/open-watcom-v2-wikidocs/ctools.html).
