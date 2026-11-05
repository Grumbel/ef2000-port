<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 096: original OPTIONS line reader and indirect formatting wrapper

The earlier `f3b78` OPTIONS dispatcher (batch 095) used a callback to supply
one pre-tokenized command at a time. This batch recovers its actual source
reader, `f2d74`, and joins it to the pre-existing native C implementations
of the line reader `f2c78`, token copier `f2cd0`, and label test `f2d54`.
Host tests now execute *one script buffer through the original-style reader,
tokenizer and all fourteen dispatcher comparisons*, including skipped lines
and the `display` terminating command. These are host drafts, not game-tested
replacement routines.

## `f2d74` (86 original bytes)

The function loops over lines in the stream at guest dword `485924`:

1. Calls `f2c78`, which skips leading spaces and tabs, copies through LF
   into `485956`, resets the token cursor `1616ea` and increments the line
   counter byte `1616ec`.
2. Ignores lines beginning with a semicolon (`;`) and lines whose copied
   string is shorter than three bytes.
3. Calls `f2cd0`, which copies the initial token to `48592c` and leaves
   its cursor immediately after the terminating delimiter.
4. Rejects a token ending in `:` using `f2d54`, treating it as a label.
5. For a command token, execution falls through from the NOP at `f2da3`
   into the block at `f2da4`; it maps every byte through `e7835` (ASCII
   `A`–`Z` to `a`–`z`) and returns through the shared epilogue at `f2d50`.

The fallthrough is part of **one** original function; creating a separate
`f2da4` entry would double-count the logic. There is no new EOF recovery
behavior: the original line reader reads until LF, and the host test supplies
terminated lines followed by a terminating OPTIONS command. Scripts without
a terminal newline or a `display`/`return`/`eof` command are not guaranteed
to terminate. The `f2d74` helper must not be treated as a generic text-file
parser.

## `f6124` (38 original bytes)

This wrapper saves EDX, obtains a 16-bit value from `16007c`, a 32-bit
value from `464390`, and passes those together with literal pointers
`192c4c` and `464396` to the four-argument function pointer at `132a74`.
The host model uses the explicit `gui_options_format_096` callback rather
than guessing the format function's guest ABI. It returns the callback's
EAX; its register preservation is not guest-verified.

## Checks and boundaries

- `tests/decomp_gui_options_096_test.c` compiles with the real host C
  implementations of `f2c78`, `f2cd0`, `f2d54` and `f3b78`. Five-megabyte
  synthetic guest RAM tests comment/blank/label filtering, mixed-case
  command folding, line count and cursor movement, original keyword data,
  `nobox` and `nofade`, the `display` handler, and formatter arguments.
  Explicit host adapters remain for the script comparison function, the
  display operation, and the indirect formatter.
- `tests/test_decomp_batch_096_payload.py` checks the two tracker entries
  even without the original executable. When `EF2000_PAYLOAD` supplies the
  SHA-256-pinned 1,662,896-byte payload, it verifies the exact reader
  call targets, skip tests, token buffer address, fallthrough and formatter
  call-site constants against the original x86 bytes.
- Both host C files compile under GCC C11 with `-Wall -Wextra -Werror
  -pedantic`; the full Python suite and `decomp_status.py check` are run
  before packaging. OpenWatcom is unavailable in this execution environment.
  No guest splice or differential verification has been claimed.

**Tracker after 096:** 4,287 total, 3,257 `compiles`, 245 `drafted`, 83
`named`, zero guest-verified replacements. The three pre-existing reader
helpers remain `compiles`; they have not been artificially promoted by their
use in the new end-to-end test.
