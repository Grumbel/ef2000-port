<!--
SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
SPDX-License-Identifier: GPL-3.0-or-later
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

The MinGW build is checked for PE executables and bundled DLLs by the flake,
but the actual GUI, joystick, sound and startup paths still require validation
under Windows or Wine on a machine with Nix and the original assets. The
legacy Linux native backend is deliberately unchanged.
