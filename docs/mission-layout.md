# Native C++ reconstruction of packed mission structures (bundle #066)

Building on the recovered [file reader call map](mission-readers.md), this
pass turns **measured guest allocation sizes and field writes** into an
asset-free, host-independent C++ layout module:

- `runtime/ef2000_mission_layout.hpp`: byte-exact 32-bit guest structures
  and `sizeof`/`offsetof` assertions.
- `runtime/ef2000_mission_layout.cpp`: portable little-endian accessors and
  the two recovered waypoint transformations.
- `tests/ef2000_mission_layout_test.cpp`: compile-time and runtime checks,
  included in both `flake.nix` and `CMakeLists.txt`.

The structures model **original memory**, not the on-disk text grammar and
not ordinary native C++ pointer-containing objects. Four-byte pointers are
stored as `Le32` byte arrays and must be resolved through a guest-memory
context; the original x86 code uses unaligned fields extensively. Unknown
bytes remain explicitly named `unknown_...`, and the code is **not yet
connected to the Unicorn backend**. This is a reconstruction milestone,
not a claim that original mission handling has been replaced.

## Measured sizes and allocation evidence

| Guest structure | Size | Original evidence |
| --- | ---: | --- |
| Mission | `0x1b` (27) | `0x0000f6a8` loads `0x1b` immediately before allocating and calling `0x0000f604`. |
| Group/flight | `0x7c` (124) | `0x0000f604` multiplies count by `0x7c` and advances each `0x0000f314` group pointer by `0x7c`. |
| BASE | `0x53` (83) | `0x0000f6a8` supplies `0x53` to its base allocation loop before `0x0000f260`. |
| 3D position | `0x12` (18) | `0x0000ee5c` writes three 32-bit and three 16-bit values at offsets 0–17. |
| Waypoint | `0x19` (25) | `0x0000fb10` allocates count × 25 and advances waypoints by 25; `0x0000ed80` writes action byte at `+0x18`. |
| Timeslot | 9 | `0x0000f260` allocates count × 9 and increments each `0x0000ee0c` item by 9. |
| Base navigation | `0x24` (36) | `0x0000fecc` allocates 36 per `0x0000fc54` entry. |
| Navigation corridor | 10 | `0x0000fc54` allocates count × 10 and advances by 10 for `0x0000fbcc`. |
| Navigation route | 6 | `0x0000fbcc` allocates count × 6 and advances by 6 for `0x0000fb10`. |

All sizes have matching compile-time assertions. They are specific to the
German SHA-pinned X-32 image and must not be assumed valid for 3Dfx/GFX+
or English patch variants without a separate executable analysis.

### Important field offsets

| Structure | Offset | Confirmed reader interpretation |
| --- | --- | --- |
| Mission | `+0x09`–`+0x0c` | Mobile class, mission-type enum, allegiance enum, status enum (bytes). |
| Mission | `+0x12` / `+0x13` | Group count (byte) / 32-bit guest group-array pointer. |
| Group | `+0x00`–`+0x03` | Unit-type index, unit count, flight role and status. |
| Group | `+0x08` | Embedded 18-byte 3D position. |
| Group | `+0x1a`, `+0x1c`, `+0x1e` | Route parameter (unclassified), waypoint count, last-waypoint index. |
| Group | `+0x20` | 32-bit guest waypoint-array pointer. |
| Group | `+0x55`–`+0x68` | Exactly 20 weapon-index bytes (`PYLON` records). |
| Group | `+0x69`, `+0x74` | Conditional pilot-data and attack-data pointers. |
| BASE | `+0x0d`, `+0x11` | Base-name guest pointer and embedded 3D position. |
| BASE | `+0x27` | Eight-byte floating value, written by x87 `FILD`/`FSTP`; its domain meaning is unknown. |
| BASE | `+0x48`, `+0x4a` | Timeslot count and guest timeslot-array pointer. |
| Base navigation | `+0x09`, `+0x1e`, `+0x20` | Embedded position, corridor count, corridor-array pointer. |
| Nav corridor | `+0x04`, `+0x06` | Number of routes and route-array pointer. |
| Nav route | `+0x00`, `+0x02` | Number of waypoints and waypoint-array pointer. |

**Do not name the byte at base-navigation `+0x1b` as a stable target enum
yet.** The original instructions of `0x0000fc54` first store the parsed
target type there, then immediately overwrite that byte with another
numeric field from the same header. This oddity may be intentional packing
or a game bug, but the current decompilation cannot prove its intent.

## Decoded waypoint layout and behavior

The original waypoint reader `0x0000ed80` skips the text keyword and leading
waypoint ordinal, then stores the following values in order:

| Text value | Guest storage | Notes |
| --- | --- | --- |
| X and Y | `+0x00`, `+0x04` | Signed 32-bit integers. |
| Third numeric (probable height) | `+0x0c` | Stored initially as signed 16-bit; see normalization below. |
| Fourth and fifth numerics | `+0x08`, `+0x0a` | Signed 16-bit fields, meaning not established. |
| Action label | `+0x18` | Numeric index from the extracted 19-entry action table. |
| Three trailing numerics | `+0x10`, `+0x12`, `+0x14` | Signed 16-bit fields. |
| `+0x0e` and `+0x16` | — | Not populated by this reader; preserved as unknown bytes. |

The original reader checks the stored **signed 16-bit** third numeric:

```cpp
if (height > 1000) {
    height /= 100;  // signed integer division, after truncation to 16 bits
}
```

Thus a text height of `12000` becomes `120`, while `1000` remains `1000`.
The host helper `guest_waypoint_height()` also models wrap/truncation before
the threshold test. We have **not** established the physical height units.

Navigation routes go through a second transformation at `0x0000fb10`:
for waypoint indexes `1` through `count - 2`, both X and Y are clamped
inclusively to **12–387**. Endpoint coordinates and all other fields are
preserved. `clamp_navigation_route()` matches this behavior on the recovered
25-byte structure, including short routes with no interior points. A
separately proven renderer/navigation coordinate convention is still needed
before naming the numbers as map boundaries.

## Reproduce

```sh
# No original assets, Unicorn or SDL required for these tests:
c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  tests/ef2000_mission_layout_test.cpp runtime/ef2000_mission_layout.cpp \
  -o /tmp/ef2000-mission-layout-test
/tmp/ef2000-mission-layout-test

# Both conventional build systems register mission-layout-reconstruction:
cmake --build /path/to/build --target ef2000-mission-layout-test
nix flake check -L
```

The original observations can be rechecked using the separately supplied
`local/ghidra/payload.bin` from the [Ghidra workflow](ghidra.md) with a
disassembler or the existing `calls.tsv` and pseudo-C exports. The binary
and all game data remain outside version control.

Next: validate the lifetime and ownership of group/waypoint arrays at the
runtime boundary, recover the remaining attack/target and pilot structs,
and only then consider replacing individual guest functions with typed
native C++ code.
