<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 095: command lexers, OPTIONS dispatch and rectangle descriptors

Three original German TACTCOM functions now have native C host drafts. The
source is reconstructed from the original x86 instructions rather than
copied decompiler output, with a byte audit gated on the pinned payload
SHA-256 `3fd26b...e0fdb`. These are not Watcom-spliced guest replacements.

## Lexer `f46cc` (190 bytes)

`gui_script_next_token_f46cc` reads its character stream at `4859e0` with
a 16-bit signed cursor `161eac`, writes NUL-terminated tokens to `485e08`,
and returns the copied byte length. Leading spaces and tabs are skipped.
Inside quotes, spaces, commas and tabs are copied; an optional closing quote
is consumed. Outside quotes, space, tab, NUL or comma ends the token. Comma
**is not consumed**: reading again at the comma yields an empty token without
moving the cursor. Both forms truncate at 80 bytes, leaving the character
at that limit unread. These small details are checked with synthetic guest
memory and original instruction references.

## Rectangle coordinates `f5484` (531 bytes)

The original parser expects six decimal values with optional separator
keywords:

```
[FROM] x y [SIZE] width height [HOTSPOT] hot_x hot_y
```

It performs three case-insensitive keyword comparisons through `9d524`,
and six calls to decimal conversion `9d4cd`. Unlike the numeric property
handlers in batch 094, the original resets the conversion error word before
**each** integer. Every failing conversion reports a different diagnostic.
Width and height must both be positive; a failure reports an additional
diagnostic without canceling the setter. The six values become little-endian
words at `485de8..485df2` and the callback at `161ea4` becomes `f5eac`,
matching the effect of the existing `f5e6c` setter. The original truncates
each signed integer to its low 16 bits.

The new C reader uses the native `f46cc` host implementation and does not
mock tokenization. Casefold comparison, decimal conversion, diagnostics and
`errno` storage remain explicit host adapters.

## OPTIONS command dispatch `f3b78` (476 bytes)

The OPTIONS file has its **own** token buffer at `48592c` and uses reader
`f2d74`, case-insensitive comparison `a4380`, and these 14 branches:

| Keyword | Action |
| --- | --- |
| `backdrop` | `f2dfc` |
| `anim` | `f2e54` |
| `clickbox` | `f2e74` |
| `lineclickbox` | `f2fbc` |
| `linepoint` | `f314c` |
| `boxcol` | `f35d8` |
| `nobox` | Set byte `4859db=1` |
| `nofade` | Set byte `4859dc=1` |
| `draw_all_boxes` | Set byte `4859dd=1` |
| `display` | Call `f3f84` and exit loop |
| `goto` | `f3b1c` |
| `mouse` | `f3b40` |
| `return` | Exit loop |
| `eof` | Exit loop |

Unknown commands call `f3b58` and parsing continues. The OPTIONS reader,
comparison and command handlers are adapters; the original 14-branch
dispatch and loop control are native C.

## Validation and unresolved boundaries

- `tests/decomp_gui_script_095_test.c`: synthetic 5 MiB guest memory,
  quote/whitespace/comma/80-byte cases, optional and absent marker parsing,
  all six coordinates and conversion errors, every OPTIONS branch and
  fallback, nonterminal chaining and loop termination.
- `tests/test_decomp_batch_095_payload.py`: original instruction references,
  all six conversion sites, 14 keyword comparisons, table/data addresses and
  lexer cursor/bound checks. The original payload is external and optional.
- Strict GCC C11 `-Werror -pedantic` host run passed. This work has not been
  compiled with OpenWatcom or replayed as a guest splice in this environment.
- Existing confirmed ABI notes for `f3b78` and `f46cc` are preserved in the
  symbol tracker. The `f5484` ABI remains unverified; no compiled guest body
  has passed differential replay.

**Tracker after 095:** 4,287 total, 3,257 `compiles`, 241 `drafted`, 84
`named` and zero fully verified guest replacements.
