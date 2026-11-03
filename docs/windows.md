<!--
SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
-->

# Windows C++ Unicorn / SDL3 build

The Windows port runs the original protected-mode EF2000 instructions in
Unicorn, with SDL3 for the window, gamepad and sound. It does **not** use the
Linux-only native64 harness, Linux syscalls, a Python runtime, or DOSBox.
It is still slower than the original-code native Linux backend.

Cross-build from x86-64 Linux (MinGW-w64, using the pinned Nix dependencies):

```sh
nix build .#ef2000-win64-zip
# Alternatively, for 32-bit Windows:
nix build .#ef2000-win32-zip
```

The `result` symlink contains a ZIP with a single
`ef2000-VERSION-win64/` or `ef2000-VERSION-win32/` folder. The archive
contains `ef2000-unicorn-cpp.exe`, SDL3 and Unicorn DLLs, required MinGW
runtime DLLs, licenses and instructions. **No proprietary game or CD files**
are bundled. Extract the ZIP into a writable directory on Windows.

First, without any game files, run the synthetic CPU and clock tests in
PowerShell (or `cmd.exe`):

```powershell
.\ef2000-unicorn-cpp.exe --self-test
.\ef2000-unicorn-cpp.exe --self-test-bda
.\ef2000-unicorn-cpp.exe --self-test-clock
.\ef2000-unicorn-cpp.exe --self-test-retrace
.\ef2000-unicorn-cpp.exe --self-test-fast-copy
```

Then point the executable at an original EF2000 installation:

```powershell
.\ef2000-unicorn-cpp.exe "C:\Games\EF2000\_EF2000.EXE" --windowed --sound sb16-dma --instruction-limit 0
```

For CD audio, also specify `--cd-dir` (the game-visible mounted disc data)
and `--cd-cue` (a readable CUE sheet and its referenced BIN):

```powershell
.\ef2000-unicorn-cpp.exe "C:\Games\EF2000\_EF2000.EXE" --windowed --sound sb16-dma --instruction-limit 0 --cd-dir D:\ --cd-cue "C:\Games\EF2000_ISO\EF_DIR.cue"
```

The program finds the other game assets alongside `_EF2000.EXE`, unless
`--game-dir DIR` overrides that location. Original game/CD files are opened
read-only. Guest-created settings, pilot files and other DOS writes go to
`%LOCALAPPDATA%\ef2000\state\unicorn\drive` by default in windowed mode.
Use `--overlay-dir DIR` to choose a different writable directory. This is
separate from the source game and CD paths.

The same options as on Linux apply, including `--geometry 1280x960`,
`--aspect-ratio 4:3|native|stretch`, `--smooth-scaling`, `--fullscreen`,
`--joystick`, `--controls`, `--trace-joystick`, and `--timing`.
Unlimited windowed sessions default to `--timing realtime`, avoiding
Unicorn's expensive per-instruction count hook. Deterministic headless probes
can use `--timing instruction --instruction-limit N`.

The real-time runner checks its deadline every 256 translated basic blocks
(`--block-clock-check N`) rather than querying the host clock for every block.
It also samples VGA retrace status every 32 port reads (`--vga-clock-check N`).
Both options accept powers of two from 1 to 4096; setting both to `1` restores
the original per-access clock checks for A/B comparisons. `--self-test-retrace`
checks that the guest can still detect VGA retrace transitions without game
files.

For performance comparisons, `--profile` prints translated-block and VGA
poll counts, clock samples, guest execution time, audio-service time and
frame-presentation time. Normal play does not collect these extra statistics.
Compare a longer asset-free clock loop under the default sampling and the old
per-access behavior:

```powershell
.\ef2000-unicorn-cpp.exe --self-test-clock --wall-seconds 5 --profile
.\ef2000-unicorn-cpp.exe --self-test-clock --wall-seconds 5 --profile --block-clock-check 1 --vga-clock-check 1
```

`profile: blocks` is the basic-block count, **not** the number of guest
instructions. The game runs with `--profile` as well; use a fixed
`--wall-seconds N` and repeat comparisons at the same in-game stage. The
display itself is capped near 30 presentations per second, so the reported
`rendered_frames` alone is not a gameplay speed benchmark.

`--profile-hot-blocks` samples the guest basic-block addresses and reports
the 16 most frequently executed ones. `--profile-copy` counts calls, bytes
and size buckets for the original game's 32-byte-at-a-time memory copier.
The optional `--tcg-cache-mib N` controls Unicorn's translated-code cache
(powers of two from 32 to 1024 MiB). The tested Unicorn 2.1.4 build defaults
to a 1024 MiB code cache; a smaller cache may help constrained 32-bit address
spaces, but the Linux tests did not show a reliable speed improvement.

`--fast-copy` is an **experimental** optimization for the original game:
it handles large, non-overlapping flat-memory copies in C++ and leaves other
calls on Unicorn's original x86 path. This is optional because the complete
Windows graphics/audio/gamepad path needs hardware validation. The synthetic
`--self-test-fast-copy` checks the return address, stack cleanup, EAX/EDX,
callee-saved registers and copied data without original game files.
Try it on a normal game run with:

```powershell
.\ef2000-unicorn-cpp.exe "C:\Games\EF2000\_EF2000.EXE" --windowed --sound sb16-dma --fast-copy
```

The MinGW build is checked for PE executables and bundled DLLs by the flake,
but the actual GUI, joystick, sound and startup paths still require validation
under Windows or Wine on a machine with Nix and the original assets. The
legacy Linux native backend is deliberately unchanged.
