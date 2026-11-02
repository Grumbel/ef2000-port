# Original mission/navigation/campaign readers (bundle #065)

The subsequent [native C++ mission layout reconstruction](mission-layout.md)
adds byte-exact guest structs, attack/target/pilot nesting and two verified
waypoint transformations.

This pass identifies **actual input routines** in the German `_EF2000.EXE`
X-32 image, rather than inferring the input grammar from text-file examples
or from the already-identified output serializers. The guest addresses below
use the pinned extracted payload's zero-based address space. All evidence
comes from the existing local Ghidra decompilation and `calls.tsv` export;
neither game files nor generated pseudo-C are committed.

Ghidra still uses a generic x86 calling convention. Register and return-value
recovery can be wrong; assign names and cross-validate offsets with the
original disassembly or a bounded runtime trace before translating to C++.

## Reader entry points and descendants

| Guest entry | Observed work and directly called reader children |
| --- | --- |
| `0x0000f6a8` | Opens the scenario/configuration input, reads mission count and world status, optionally parses pilot state, then loops over `MISSION` sections using `0x0000f604` and `BASE` sections using `0x0000f260`. Called directly from `0x000357ec`, `0x00049228`, `0x00069094`. |
| `0x0000f604` | Parses one `MISSION` header into an allocated mission record; saves the group count at object offset `+0x12`, a group array pointer at `+0x13`, and calls `0x0000f314` for each group/flight. |
| `0x0000f314` | Parses a `GROUP` or `FLIGHT` payload, including weapon/pylon records via `0x0000eea0`, position via `0x0000ee5c`, waypoint route via `0x0000ed80`, target members via `0x0000eefc`, nested attacks via `0x0000f08c`, and pilots via `0x0000fe4c`. Group entries use a **`0x7c`-byte stride**. |
| `0x0000f260` | Parses a `BASE` header, position and `TIMESLOTS`; each `TIMESLOT` uses `0x0000ee0c`. Header stores the timeslot count at `+0x48` and a pointer at `+0x4a` (both tentative from decompilation). |
| `0x0000fecc` | Opens navigation input and iterates `BASE_NAVIGATION` entries through `0x0000fc54`. Called from `0x000b0f5c` in both initialization paths. |
| `0x0000fc54` | Parses `BASE_NAVIGATION` plus position (`0x0000ee5c`), allocates its corridors and calls `0x0000fbcc` for each. |
| `0x0000fbcc` | Parses a `NAVIG_CORRIDOR`, allocates its routes and invokes `0x0000fb10` per route. |
| `0x0000fb10` | Parses `NAVIG_ROUTE` count, allocates waypoint array, calls `0x0000ed80` for each. For **intermediate points only**, two four-byte coordinates are clamped to `[12, 387]` after parsing. |
| `0x0000ed80` | Common `WAYPOINT` record reader used by mission and navigation routes. Its stores cover a compact **25-byte** record (`0x19` bytes), with coordinates, action code and further parameters; exact field semantics remain unproven. |
| `0x0001001c` | Opens campaign/conflict input, parses `DIFFICULTY`, `PERFORMANCE`, `WORLD_STATUS`, then `NUM_TARGET_AREAS` / `SSNAME`, mobiles and allegiances. Called from `0x00049228` and `0x000b0f5c`. |
| `0x0000fcf4` | Reads a campaign `MOB` record, dispatches class byte 0/1/2 to aircraft/ship/ground readers, stores/advances the ordinal at `+0x13`, then parses position and pilot data. |
| `0x0000f08c` → `0x0000f038` → `0x0000efe8` | Loads `ATTACK_PATTERN` → `ATTACK_ROUTE` → `ATTACK_NODE` into nested 7-, 6-, and 17-byte records. The pattern label is skipped, while each node's action is decoded by `0x0000ef98`. |
| `0x0000eefc`, `0x0000fe4c` | Read a 21-byte target member and 22-byte pilot record, respectively. Names use separately allocated 32-bit guest pointers. |

The roots and each stated child relation have direct `CALL` evidence in the
existing `local/ghidra-entries-recovery/custom-storage-export/calls.tsv`.
The inferred record sizes/field identities rely on pseudo-C and must not
be considered finalized ABI types.

## Shared lexical reader

| Guest entry | Directly observed behavior |
| --- | --- |
| `0x0000e774` | Pulls characters from a buffered file read; skips CR (`0x0d`) and interprets DOS `0x1a` as EOF. |
| `0x0000e810` | Assembles a line into a global buffer, strips initial spaces/tabs, skips *whole* semicolon-comment and blank lines, increments line counter `DAT_00121eac`; checks a 1024-character bound. |
| `0x0000e89c` | Extracts the next token to `DAT_00197abe`: handles quoted text without treating spaces as separators, otherwise separates on space, tab or comma; uses an 80-character token bound. |
| `0x0000e95c` | Token-to-integer helper: calls `0x0000e89c`, checks nonempty text, then calls the numeric conversion routine `0x0009d4cd`. That conversion consumes optional sign and decimal digits. |
| `0x0000ee5c` | Reads a `3D_POSITION` / `3DP` row into three four-byte coordinates followed by three two-byte fields (observed store offsets `+0,+4,+8,+12,+14,+16`). |

The read-only Python inspector in [mission-records.md](mission-records.md)
is a **conservative validator**, not a cycle-accurate port of this lexer:
it uses `shlex` for useful quoted-text handling and deliberately rejects
some malformed numeric values that the original's `atoi`-style converter
might instead accept. A practical correction from this code is that semicolons
only suppress **whole lines**, not the rest of a line; the inspector now
retains inline semicolon text as raw tokens rather than discarding it.

## Recovered token-to-enum tables

The original token readers compare each string against a contiguous table
of **32-bit guest pointers** and return the table index. This is visible
in the disassembly, independent of Ghidra's imperfect function signatures.
`tools/mission_enum_probe.py` follows those pointers in the **SHA-verified**
initialized payload and reports the names without committing game data:

| Table | Reader | Guest pointer table | Entries |
| --- | --- | --- | ---: |
| Allegiances | `0x0000e9bc` | `0x132f5c` | 11 |
| Mission types | `0x0000eaf8` | `0x132e6c` | 30 |
| Plane types | `0x0000eb48` | `0x132d34` | 39 |
| Ship types | `0x0000eb9c` | `0x1333f8` | 18 |
| Ground types | `0x0000ebec` | `0x133488` | 27 |
| Flight roles | `0x0000ec3c` | `0x133304` | 13 |
| Target types | `0x0000ec8c` | `0x132fb4` | 106 |
| Statuses | `0x0000ece0` | `0x13336c` | 10 |
| Waypoint actions | `0x0000ed30` | `0x132c2c` | 19 |
| Attack node actions | `0x0000ef98` | `0x132cc4` | 3 |
| Weapons | `0x0000eea0` | `0x1335a0` | 37 |

That gives **313 indexed string labels**. A further hard-coded
comparison sequence at `0x0000ea0c` maps the abbreviated mobile classes
(`PL`, `SH`, `GB`, `ST`) and their expanded synonyms to **0, 1, 2, 3**.
These are numeric parser results, not a proof of how the gameplay engine
uses every value. For example, campaign mobile input currently accepts
classes 0–2 while the common class parser recognizes class 3 as well.

```sh
# Extract from the existing SHA-pinned Ghidra import, not from a random EXE:
python3 tools/mission_enum_probe.py local/ghidra/payload.bin
python3 tools/mission_enum_probe.py local/ghidra/payload.bin \
  --table waypoint_actions
python3 tools/mission_enum_probe.py local/ghidra/payload.bin \
  --table flight_roles --json
python3 -m unittest tests.test_mission_enum_probe -v
```

The default output lists table sizes; `--table` prints every indexed
name and `--json` produces machine-readable mappings for future typed
C++ and data-extraction work. The tool enforces the existing original
payload hash and size; it **must not** apply these table offsets to the
unverified English/3Dfx editions.

## Verified route constraint from the actual input code

In `0x0000f314`, the first `WP_ROUTE` parameter is stored at group offset
`+0x1c` as its waypoint count. The third parameter is stored at `+0x1e`
and used as the *last waypoint index*; the second sits at `+0x1a`.
The original reader checks both:

1. The route must contain **at least one waypoint**.
2. The last waypoint index must be **strictly smaller than the count**.

Its diagnostic strings are `Must Have At Least 1 Waypoint` and
`Last Waypoint Does Not Exist`. `tools/mission_records.py --strict` now
checks those two cases in addition to route child counts. All **807**
`WP_ROUTE` headers in the supplied `MISSION.CFG` satisfy the constraints.

This is a stronger claim than the previous inference from data counts: it
is an actual game-side input validation. Conversely, the second route
parameter's meaning remains unassigned; do not fabricate labels for it.

## Reproduce the static evidence

The original game binary must be supplied separately. After using the
documented [Ghidra workflow](ghidra.md), with a complete exported corpus:

```sh
# Verify both ends of the direct reader call edges:
rg '0x0000f6a8|0x0000f604|0x0000f314|0x0000fecc|0x0000fc54|0x0000fbcc|0x0000fb10|0x0001001c' \
  local/ghidra-entries-recovery/custom-storage-export/calls.tsv

# Inspect exact pseudo-C, with its generic-ABI disclaimer:
sed -n '1,180p' \
  local/ghidra-entries-recovery/custom-storage-export/functions/0000/0x0000f314.c

# Validate the count/last-waypoint checks against supplied original data:
python3 tools/mission_records.py /path/to/EF2000/MISSION.CFG --strict
python3 -m unittest tests.test_mission_records -v
```

## Next decompilation targets

- Resolve the data-object allocator `0x0000e744` and register-passed sizes
  so the mission, group, base, corridor, route and waypoint layouts become
  typed structs with bounded fields rather than guessed parameter lists.
- Correlate the extracted **313 enum labels** to guest struct fields and
  authored mission/campaign rows; test whether repeated labels or sentinel
  values affect game behavior. Preserve unknown edition-specific values.
- Confirm `0x0000fb10` coordinate clamping and the source-side values with
  a disposable navigation overlay and runtime trace; do not edit originals.
- Map the call from mission selection to `0x0000f6a8` and prove which
  configuration filename and scenario mode each branch selects.
