<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 093: native C high-score logic with a known guest oracle

Five original, previously named high-score functions now have derived C
implementations under `decomp/` rather than only the pre-existing C++ runtime
model. These are **host-tested drafts**, not verified guest replacements.

| Guest entry | Source | Covered behavior |
| --- | --- | --- |
| `4050`, 138 bytes | `hiscore_data_093.c` | 48-name pool selection, 20-byte packed name rows, ten 25-byte records, signed default scores, two unsigned 16-bit random choices per record |
| `433c`, 242 bytes | `hiscore_data_093.c` | Signed score insertion, ties, `INT_MIN` threshold wrap, record shifts with NUL-only string copies, selected difficulty |
| `41fc`, 165 bytes | `hiscore_io_093.c` | Preflight/open failures, checksum validation, default regeneration, GUI refresh only on successful open |
| `42a4`, 114 bytes | `hiscore_io_093.c` | File creation/open failure, checksum then 250 raw record bytes, ignored write counts |
| `40dc`, 288 bytes | `hiscore_display_093.c` | Three independently linked icon chains with 10-row caps, original difficulty labels, `p%d` score text, invalid-level diagnostic and forced-window bypass |

This cluster extends the evidence in
[highscores-reconstruction.md](../highscores-reconstruction.md), which already
compares native C++ implementations of the four data/file operations to
original x86 via Unicorn. The new C implementation shares their intended
state structure but **has not itself been guest-differential tested**. The
host C++ test compares C defaults and insertion to that established native
reference, checks byte-exact save/load including corruption that the weak
checksum fails to detect, and exercises the GUI linked-list order.

## Preserved original quirks

- The insertion comparison decrements each existing score first (with 32-bit
  wraparound) and then compares it signed against the candidate. This is not
  equivalent to a simple `candidate >= old_score` at `INT_MIN`.
- Shifting a record copies its name *through the first NUL*, preserving its
  previous destination padding; the level and unaligned score follow.
- Both `41fc` and `42a4` repeatedly read the **same** first byte of the
  high-score table 250 times. The checksum is therefore `250 * name[0]`,
  intentionally reproducing the original bug.
- The load path returns 0 on preflight/open failure without updating GUI
  columns. A successfully opened but corrupt file regenerates defaults,
  refreshes the GUI, and returns 1.
- The forced display mode skips the window-existence query; each of the
  name, difficulty, and score chains has its own lookup and next pointers at
  icon `+0x21`.

## Validation and limitations

- GCC C11 and G++ C++17 test build use `-Wall -Wextra -Werror -pedantic`.
  C++ reference-vs-C insertion exercises 2,000 deterministic scenarios,
  including ties and `INT_MIN`, with all 254 guest-state bytes compared.
- Defaults exercise all 20 RNG draws, every packed name/level/score and
  untouched padding. File tests compare the 254-byte output to the C++
  reference, load a checksum-undetected mutation, and cover missing and
  failed-open files. Display tests cover missing/forced windows, truncated
  linked lists and invalid difficulty.
- `test_decomp_batch_093_payload.py` checks the SHA-pinned original entry
  sizes, global addresses, prologues and the two exact checksum loops. No
  proprietary payload or disassembly is committed.
- Host adapters represent random seeding, DOS file operations, GUI lookup,
  text assignment, and invalid-level reporting. The host file adapter
  currently supplies complete reads; it does **not** mimic unchecked DOS
  short reads. The original local `sprintf` has a narrower stack buffer
  than our guarded host formatting, so stack scribbles are not modeled.
- `hiscore_insert_433c` assumes NUL-terminated guest strings and does not
  guard against an original unbounded `strcpy` overrun. Its input ABI and
  all five output/guest-call conventions still need caller tracing, Watcom
  compilation, splice and differential replay before `state=verified`.

**Tracker after batch 093:** 4,287 functions, 3,257 old `compiles`, 231
host-tested `drafted`, 91 remaining `named`; zero additional verified
compiled-and-spliced guest replacements.
