<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 094: GUI script properties, resource names and palette extremes

Seven previously named original script handlers at `f4db8..f53bc` have new
bounded native-C host drafts. Their instructions are grouped as five
property handlers and two asset handlers. The surrounding original script
loop already exists as a separate earlier draft (`f4a88`, `f4bdc`, `f4c60`,
`f4d0c`), but no guest ABI or splice is newly verified here.

## Numeric script properties

The three numeric commands call the lexer at `f46cc` twice, reset the error
word returned by `ebd30` **only before the first** call to integer parser
`9d4cd`, and report a diagnostic if either conversion sees that word set.
One failed first conversion may therefore be reported again after an otherwise
valid second token. Each parsed value is truncated to 16 bits.

| Command | Script mode | First value | Second value | Set flag |
| --- | --- | --- | --- | --- |
| `SIZE` `f4db8` | 1 | Node `+57` | Node `+59` | `485f6f` |
| | 2 | `+63` | `+65` | `485f6c` |
| | 3 | `+4b` | `+4d` | `485f70` |
| `FROM` `f4ed4` | 1 | `+5b` | `+5d` | `485f73` |
| | 2 | `+67` | `+69` | `485f6e` |
| | 3 | `+4f` | `+51` | `485f71` |
| `BACKDROP` `f4ff0` | 1 | `+55` | `+53` | `485f72` |
| | 2 | `+61` | `+5f` | `485f6d` |

The BACKDROP first/second byte offsets really are reversed relative to their
ascending order, and mode 3 is rejected for BACKDROP while permitted for
SIZE and FROM. A missing node is reported first but the original continues
to process tokens; the host tests exercise valid node addresses.

## String properties and original collision check

- `ACTION` (`f50ec`, 145 bytes) copies a lexer token to node `+8` and
  sets byte `485f76=1`.
- `POINTER` (`f5180`, 145 bytes) copies to node `+29` and sets
  `485f78=1`.
- Both reject length **greater than 32** with a diagnostic but then still
  perform original NUL-terminated `strcpy` with no boundary enforcement.
- **ORIGINAL BUG:** both handlers check the *POINTER* presence flag
  (`485f78`) for a duplicate. ACTION does not check its own flag: multiple
  ACTION statements do not trigger the collision until a POINTER was parsed.

## Asset path and 256-entry palette scan

`GADGETRY` (`f5214`, 423 bytes) and `NAME` (`f53bc`, 200 bytes) require script
mode 12, read a token of at most eight characters, and copy its name to
`485f5a` and `485f63`, respectively. If the token is empty, the previous
name is retained while an error is reported. Both construct a path using
the default-path helper at `9c980`, three append calls to `9ca57`, and
`192448`/`192454` prefix/suffix data. They select different driver buffers
via `c3974` and `c3980`, then start the driver (`c34cc`) and exit its pending
scroll mode (`c39ac`). These calls use explicit host adapters, not guessed
Watcom stack pointers.

After calling the driver, GADGETRY examines the 256 RGB triples beginning at
`425c8c` and chooses their greatest and least sum-of-channels values. The
initial reference index is signed word `485dfe`, **not zero**, and ties keep
that existing index. Max is written to `485dfe` and `485e00`; min to
`485e02`. NAME does not run this scan.

## Evidence and limits

- `tests/decomp_gui_script_094_test.c` exercises all eight numeric modes,
  invalid-mode and persistent-conversion-error paths, repeated ACTION,
  POINTER collisions, a 33-character copy, both asset names, empty name,
  invalid section, monotonic RGB sums and palette ties. Its adapters record
  original callee invocation order and use synthetic guest RAM.
- `tests/test_decomp_batch_094_payload.py` validates all seven original
  entry boundaries against the SHA-pinned German payload, scans the exact
  lexer/error/parser call counts, string copy sites, driver calls and palette
  byte references. The external game image is neither copied nor committed.
- GCC C11 host code compiles with `-Wall -Wextra -Werror -pedantic`. Actual
  Ghidra pseudo-C is not available in the sandbox; the original x86
  disassembly was used. Tokenizer, atoi-error state, diagnostics, path
  construction and driver invocations remain unverified host adapters.
- No original DOS/Watcom object was compiled or patched into guest execution;
  `state=drafted` and `sig_status=unverified` are intentional. Overflow and
  malformed-pointer cases are not safe host inputs and may differ in their
  exact stack corruption from the guest.

**Tracker after 094:** 4,287 total functions; 3,257 existing `compiles`,
238 host-tested `drafted`, 84 `named`, and no additional fully verified
guest replacements.
