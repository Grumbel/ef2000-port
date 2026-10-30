# EF2000 Linux port handoff

## Current state

Read `README.md`, then `docs/startup-runtime.md` and `docs/x32-services.md`.
The current tip is reported by `git log -1`; this repository began without any
commits. Original game binaries/data stay outside Git and the Nix package.

The native x86-64 Linux harness executes original 32-bit instructions through
startup and into runtime initializers. It stops explicitly at DOS AH=2Ch, image
offset `0x11f2a4`. This is not a playable game yet. The target executable and
its SHA-256 are recorded in `docs/executable.md`.

Implemented and verified:

- Two-stage X-32 extraction and both static segment-relocation passes.
- Native compatibility-mode execution with 64-bit Linux signal handlers.
- The first X-32 selector-limit request and DOS 5.0 version query.
- Non-DPMI stack reservation and heap growth, with reference state/flag tests.
- Original PSP/environment copying and clearing of deliberately seeded BSS.
- Hardware expand-down protection. QEMU reaches the same startup boundary but
  did not enforce the tested lower bound; retain it for comparison only.
- A pinned Linux flake that precompiles both harness backends without game data;
  the installed launcher only assembles/links the supplied payload locally.

## Next work

1. Measure the DOS time query and implement an advancing, recordable clock model.
2. Continue initializer tracing; implement only interfaces with evidence. Keep
   unknown calls, privileged instructions, and model exhaustion as explicit stops.
3. Verify x87 initialization and exception behavior, then progress toward game
   `main`, file access, and presentation of the original framebuffer.
4. Document data/mission discoveries separately under `docs/`, with fingerprints,
   offsets, reproducers, and observed/inferred status.

## Reproduce

```sh
nix build -L .
nix flake check
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE --output local/nix-startup
nix develop -c env EF2000_EXE=../EF2000/c/EF2000/_EF2000.EXE python3 -m unittest discover -s tests -v
```

The original-file test suite currently has 34 passing tests. Nix package checks
omit tests requiring the external executable. In the managed environment, the
Nix daemon socket is unavailable: expression/derivation evaluation and direct
execution of the evaluated build/install phases were validated instead of a full
daemon-backed `nix build`. Installed startup and segment-limit checks passed.

## Scope and conventions

Apply `~/projects/gamepkgstd/trunk/gamepkgstd` incrementally. Current adoption:
small focused commits, Ingo Ruhnke as author, Codex co-author attribution,
GPL-3.0-or-later/REUSE, one `VERSION` file, native flake aliases, checks, and this
handoff. Desktop integration and broader runtime conventions follow when there
is a playable Linux application. R36S, Android, and other platform ports are
outside current scope. The source license does not license original game data.

Keep commits on one history. For handoff from this initially empty repository,
create a full-history bundle with `git bundle create <path> HEAD`, without
prerequisites. Use the first root commit as the work-line identifier in its name;
later bundles must continue to include the entire history from this empty base.
