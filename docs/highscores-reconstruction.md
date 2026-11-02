# EF2000 native high-score reconstruction (bundles #058–#060)

This is the first **typed original-game logic module** from the Ghidra corpus
and x86 instruction stream: `runtime/ef2000_highscores.hpp/.cpp`, tested by
`tests/ef2000_highscores_test.cpp`. It is standalone and **does not yet replace
the original guest code**. No original executable or generated pseudo-C is
committed.

## Verified guest data layout

| Guest address | Extent | Meaning | Evidence |
| --- | --- | --- | --- |
| `0x1970a0` | 4 | Current signed score, LE | `0x432c`, `0x433c` |
| `0x1970a4` | 250 | Ten packed 25-byte records | `0x4050`, `0x41fc`, `0x42a4`, `0x433c` |
| Record `+0` | 20 | NUL-terminated name, including unused padding | `0x9d749`, `0x433c` |
| Record `+20` | 1 | Difficulty, normally 1..3 | `0x40ba`, `0x43e8` |
| Record `+21` | 4 | Signed score, LE, unaligned | `0x40c0`, `0x43f2` |
| `0x19719a` | 4 | Last record's score | `0x432c` |
| `0x121e44` | 1 | Active difficulty on insertion | `0x43dd` |

The native code uses byte arrays for unaligned LE dwords and statically
checks sizes/offsets. `State` is 254 bytes: the current score followed by the
250-byte table.

## Original routines reconstructed

- **`0x4050`** creates ten default entries, each with a name selected by an
  unsigned 16-bit RNG result modulo 48 from the name pool at `0x121292`
  (20 bytes per name). A second random result modulo 3 plus one chooses
  difficulty. Scores come from template dwords at `0x1211ad + 25*i`,
  originally 10,000, 9,000, …, 1,000. The native function accepts an
  injected RNG and external source data; the initialization calls to
  `0xa4574`/`0xa45e4` and their seeding are not modeled yet.
- **`0x432c`** tests the current score *strictly greater* than slot nine's.
- **`0x433c`** inserts before the first stored score less than **or equal**
  to the current score. It computes stored-score-minus-one first and performs
  signed `JLE`, so `INT_MIN` exhibits guest 32-bit wraparound. When shifting
  entries it uses `strcpy` at `0x9d749`, not a 20-byte `memcpy`; padding
  following the first NUL in the destination remains unchanged.
- **`0x41fc`, `0x42a4`** read and write the high-score file: 4-byte LE
  checksum followed by the 250 record bytes. The current score is not stored.

## #059: Display lists and arcade name acceptance

The display routine `0x40dc` has three complete, independent passes over
GUI icon lists. Each list links to its next icon through a 32-bit pointer at
object offset **`+0x21`**; traversal ends after ten rows or a null pointer.
The window is named `hiscores`. The exact pointers identify these icon IDs:

| Pass | Icon ID (guest string pointer) | Original value assigned |
| --- | --- | --- |
| Player | `name1` (`0x17f654`) | Record `+0` NUL-terminated name |
| Difficulty | `level1` (`0x17f65c`) | `Rookie`, `Profi`, `Topgun` (level 1, 2, 3) |
| Score | `score1` (`0x17f688`) | `p%d` formatted signed score (`0x17f690`) |

The code first checks whether the `hiscores` window exists **only when input
register EAX is zero**. If absent, it returns without touching icons;
nonzero EAX bypasses that check. A difficulty outside 1..3 invokes an error
callback rather than silently choosing a label. The native `refresh_display`
models the three lists through a GUI-independent `DisplaySink`, preserving
traversal order, independent short-list termination, guarded missing-window
behavior and invalid-difficulty diagnostics. It does not implement guest GUI
objects or their allocation.

The named `InitArcade` callback at `0x4c50` gets an edit-field string, copies
it into the record at `0x1970a4 + selected_slot * 25`, and uses guest `strlen`
at `0xb1b8b` to check for an empty result. Empty names are replaced by
`Jemand` (`0x130be8`, German for “someone”), without trimming whitespace.
It then saves through `0x42a4`, refreshes through `0x40dc`, and invokes a
menu helper at `0xac7a8`. The native `accept_arcade_name` models only the
record update and fallback; save, refresh and menu effects remain explicit
outside this helper.

### Confirmed original checksum bug

Both the load loop (`0x426d..0x427a`) and save loop (`0x42d4..0x42e1`)
initialize `EBX` to `0x1970a4` and read `[EBX]` 250 times. The counter
increases but `EBX` **never increments**, so the effective checksum is:

```text
250 * unsigned_first_byte_of_first_player_name
```

The rest of the file can change without invalidating it. This is not merely
Ghidra pseudo-C ambiguity: the same pointer bug is visible in both original
instruction streams. `original_checksum`, `encode_file`, and `decode_file`
intentionally reproduce it. On a checksum mismatch, `decode_file` has already
copied the table, matching the guest loader before its default fallback.

### Explicit limitations

The native insert function safely rejects malformed, unterminated 20-byte
names with host-only `-2` instead of allowing the guest `strcpy` to overrun
memory. Truncated file inputs are also rejected without modifying the table,
unlike the DOS code's unchecked short reads. The `0x40dc` model reports
invalid names and difficulty levels instead of performing unbounded memory
reads or invoking the potentially fatal guest diagnostic. The original
file-open error paths, GUI object internals, RNG seed management and other
menu callbacks are not reconstructed yet. The Watcom calling convention and
runtime integration still require guest-trace verification.

## Tests

The asset-free C++ suite checks structure packing, signed score and byte
order, ranking and equal scores, shift and padding semantics, full-table
failure, `INT_MIN` wraparound, defaults with an injected RNG, and the weak
checksum—including corruption which goes undetected and truncated input.
The #059 tests additionally cover all three independent GUI lists, update
ordering, missing-window cases, bad difficulty values, and arcade name
acceptance including the original empty-name fallback.

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror \
  tests/ef2000_highscores_test.cpp runtime/ef2000_highscores.cpp \
  -o /tmp/ef2000-highscores-test
/tmp/ef2000-highscores-test
```

Also included in CMake/CTest and the main `nix flake check` package tests.
For a quick Nix-only check without building the Unicorn/SDL3 game, use
`nix build .#checks.x86_64-linux.highscores-reconstruction`.

## #060: Byte-for-byte original/native differential replay

`tools/highscores_differential.py` runs five **real original x86 functions**
in an isolated 32-bit Unicorn guest, alongside a temporary shared library
built from `runtime/ef2000_highscores.cpp` and the test-only C ABI at
`tests/highscores_reference_abi.cpp`. It requires either the original
`_EF2000.EXE` or a locally extracted matching `payload.bin`; the extracted
image must match the recorded SHA-256. The executable, extracted image,
Ghidra output and generated native library remain outside Git and Nix store
source inputs.

| Original routine | Compared effects | Controlled external calls |
| --- | --- | --- |
| `0x433c` insert | All 254 state bytes; returned 16-bit slot | None; original `strcpy` executes |
| `0x432c` threshold | State unchanged; returned low byte | None |
| `0x4050` defaults | All 254 bytes with 20 injected RNG values | Time, RNG seed and 15-bit random output |
| `0x42a4` save | Exact 4+250 file bytes; state; return byte; failed-open path | DOS create/write/close |
| `0x41fc` load | Valid-file state, bad-checksum/default regeneration and return byte | DOS preflight/open/read/close; GUI refresh |

The runner uses real original code from address zero through its 32-bit
machine, with a bounded instruction count and mapped guest stack. It does
**not** emulate the whole DOS operating system, file system, GUI or original
RNG implementation. Those interfaces are explicitly controlled; regular
`strcpy` and all reconstructed guest arithmetic remain real instructions.
Native code is not injected into the original process.

With the pinned original image, **308 insertions, 308 threshold checks, six
default generations, and six complete save/valid-load/corrupt-checksum
fallback cycles** passed, comparing the state after every operation and the
precise saved bytes. Fixed edge cases include score ties, lower-than-last
scores, signed negative scores, `INT_MIN` wraparound, maximum valid name
length and differing record-name padding. Randomized cases use a fixed seed
by default for reproducibility. The write-open failure returns zero without
touching the state or producing file bytes. Corrupt-checksum loads reset the
table through the original `0x4050` path and match native reinitialization.

Run the optional guest replay when you have the game installed:

```sh
nix run .#highscores-differential -- /path/to/_EF2000.EXE --random-cases 300
# Or from nix develop:
ef2000-highscores-differential /path/to/_EF2000.EXE --random-cases 300
```

For already-extracted local research input,
`--payload local/ghidra-entries-056/payload.bin` skips DOS extraction but enforces
the same payload SHA. `tests/test_highscores_differential.py` checks fixture
shape and hash rejection without any original asset. Its full guest replay
test runs only when `EF2000_EXE` is set and Unicorn plus a C++ compiler are
available; normal `nix flake check` does **not** require proprietary files.
The separate fast check is
`nix build .#checks.x86_64-linux.highscores-differential`.

These comparisons establish equivalence for specific bounded routines and
inputs, **not** a complete gameplay decompilation or replacement. Remaining
work includes GUI object and callback reconstruction, original RNG seeding,
other DOS open/read failure branches, and integration of typed code into a
native runtime backend.

Next is to verify guest-vs-native UI state traces, finish the remaining DOS
file-open/read branches and GUI object interfaces, and then replace a bounded
guest path.
