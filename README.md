# EF2000 Linux port

Behavior-preserving port of the original DOS game to Linux. Preserve the original
game instructions and data; replace DOS and hardware interfaces incrementally.
Emulation and binary translation are intermediate tools. Full decompilation into
maintainable source is the long-term goal.

## Status

The two-stage FlashTek X-32 loader, segment relocation table, initialized image,
BSS boundaries, and protected-mode entry have been recovered by static analysis.
The extractor reproduces the static relocation passes. A native x86-64 Linux
harness executes unchanged original 32-bit startup and runtime initializer code,
handling the initial selector, DOS-version, stack, and heap interfaces. It now
stops at a DOS time query; the game is not playable through this port yet.
The first target is the installed `_EF2000.EXE` in the sibling
`../EF2000/c/EF2000/` directory. Other releases need separate fingerprints.

## Development

On x86-64 Linux, build and run with Nix:

```sh
nix build -L .                 # Precompile the native and QEMU harness objects
nix flake check                # Unit tests, app/help/version checks, REUSE
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE --output local/nix-startup
nix develop                    # Python, GCC/binutils, QEMU, DOSBox-X, GDB
```

`packages.default` and `packages.ef2000` are the same native package;
`packages.ef2000-startup` is an alias. `apps.default` and `apps.ef2000` run the
`ef2000` launcher; `ef2000-startup` remains available as a command alias.
Builds require no original game files. The launcher accepts your external
executable, creates a new local output directory, and links it with the
precompiled harness. `nix build` does not build a complete playable game yet.

Python 3 is sufficient for the analysis tools. Direct startup development also
requires GCC/binutils; QEMU and DOSBox-X are used by comparison/reference tools:

```sh
python3 tools/inspect_exe.py ../EF2000/c/EF2000/_EF2000.EXE
python3 tools/extract_image.py ../EF2000/c/EF2000/_EF2000.EXE --output local/image-01
python3 -m unittest discover -s tests -v
# Include checks against the fingerprinted original:
EF2000_EXE=../EF2000/c/EF2000/_EF2000.EXE python3 -m unittest discover -s tests -v
# Execute original startup (x86-64 Linux, GCC/binutils; no 32-bit libc required):
python3 tools/run_startup.py ../EF2000/c/EF2000/_EF2000.EXE --output local/startup-01
```

Keep original executables and game data outside this repository. Put generated
memory dumps, traces, and extracted images in ignored `local/` or `build/`.
The native executable, package version, and help flags are checked without game
assets; original-file integration tests run explicitly outside the Nix build.

## License and continuity

New port code and documentation are GPL-3.0-or-later; see
[LICENSES/GPL-3.0-or-later.txt](LICENSES/GPL-3.0-or-later.txt) and `REUSE.toml`.
Original game executables and data remain external and retain their own rights.
Read [TODO.md](TODO.md) for the current stopping point, validation limits, next
steps, and the incremental adoption of the gamepkgstd Linux conventions.

## Documentation

- [Port architecture and milestones](docs/port-plan.md)
- [Executable and startup findings](docs/executable.md)
- [Recovered loader and image layout](docs/loader.md)
- [Startup symbols and interface worklist](docs/startup-symbols.md)
- [Execution probes and reference experiments](docs/execution-probes.md)
- [Native startup harness and validation](docs/startup-runtime.md)
- [Recovered extender services and differential evidence](docs/x32-services.md)
- [Research and evidence conventions](docs/research.md)
