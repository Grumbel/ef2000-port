# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

{
  description = "EF2000 Linux original-code main-menu port and reverse-engineering tools";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      inherit (pkgs) lib;
      python = pkgs.python3.withPackages (p: [ p.unicorn ]);
      ghidraHome = "${pkgs.ghidra}/lib/ghidra";
      versionBase = lib.fileContents ./VERSION;
      gitRev = self.shortRev or self.dirtyShortRev or "dirty";
      version =
        if lib.hasInfix "-dev" versionBase
        then "${versionBase}.${toString (self.revCount or 0)}+g${gitRev}"
        else versionBase;
      source = lib.fileset.toSource {
        root = ./.;
        fileset = lib.fileset.unions [
          ./runtime ./tools ./tests ./probes ./docs ./data ./dev ./CMakeLists.txt ./README.md
          ./VERSION ./LICENSES ./REUSE.toml ./TODO.md ./.gitignore ./flake.nix ./flake.lock
        ];
      };
      windowsFor = target: crossPkgs: import ./nix/windows.nix {
        inherit pkgs crossPkgs target version source;
      };
      win64 = windowsFor "win64" pkgs.pkgsCross.mingwW64;
      win32 = windowsFor "win32" pkgs.pkgsCross.mingw32;
      package = pkgs.stdenv.mkDerivation {
        pname = "ef2000";
        inherit version;
        src = source;

        nativeBuildInputs = [ python pkgs.makeWrapper ];
        buildInputs = [ pkgs.unicorn pkgs.openssl pkgs.sdl3 ];
        # Guest transitions do not provide host TLS for stack canaries; the
        # freestanding harness also needs its linker script's fixed addresses.
        hardeningDisable = [ "pic" "stackprotector" "fortify" "fortify3" ];

        buildPhase = ''
          runHook preBuild
          commonFlags="-std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin"
          mkdir -p objects/native64 objects/qemu
          for backend in native64 qemu; do
            if [ "$backend" = native64 ]; then
              width=64
              defines="-D_GNU_SOURCE -DEF_NATIVE64"
              entry=runtime/native64.S
            else
              width=32
              defines=""
              entry=runtime/entry.S
            fi
            $CC -m$width $commonFlags $defines -c runtime/startup.c -o objects/$backend/startup.o
            $CC -m$width $commonFlags $defines -c runtime/x32.c -o objects/$backend/x32.o
            as --$width -I runtime "$entry" -o objects/$backend/entry.o
            as --$width -I runtime --defsym CHECK_SEGMENT_LIMIT=1 "$entry" -o objects/$backend/entry-limit.o
          done
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror -DEF_UNICORN_SDL3 \
            runtime/unicorn_cpp.cpp runtime/dos_drive.cpp runtime/cd_audio.cpp runtime/real_mode_cpu.cpp \
            runtime/sb16_probe.cpp runtime/sound_blaster.cpp runtime/isa_audio.cpp \
            runtime/pcm_audio.cpp runtime/wav_capture.cpp \
            -lunicorn -lcrypto -lSDL3 \
            -o objects/ef2000-unicorn-cpp
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/dos_drive_test.cpp runtime/dos_drive.cpp \
            -o objects/ef2000-dos-drive-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/real_mode_cpu_test.cpp runtime/real_mode_cpu.cpp runtime/sb16_probe.cpp \
            runtime/sound_blaster.cpp runtime/isa_audio.cpp \
            -lunicorn \
            -o objects/ef2000-real-mode-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/original_sound_driver_test.cpp runtime/real_mode_cpu.cpp \
            runtime/sb16_probe.cpp runtime/sound_blaster.cpp runtime/isa_audio.cpp \
            -lunicorn -lcrypto \
            -o objects/ef2000-original-sb16-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/cd_audio_test.cpp runtime/cd_audio.cpp \
            -o objects/ef2000-cd-audio-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/sb16_probe_test.cpp runtime/sb16_probe.cpp \
            -o objects/ef2000-sb16-probe-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/isa_audio_test.cpp runtime/isa_audio.cpp \
            -o objects/ef2000-isa-audio-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/sound_blaster_test.cpp runtime/sound_blaster.cpp runtime/isa_audio.cpp \
            runtime/sb16_probe.cpp \
            -o objects/ef2000-sound-blaster-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/pcm_audio_test.cpp runtime/pcm_audio.cpp \
            -o objects/ef2000-pcm-audio-test
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
            tests/wav_capture_test.cpp runtime/wav_capture.cpp \
            -o objects/ef2000-wav-capture-test
          runHook postBuild
        '';

        doCheck = true;
        checkPhase = ''
          runHook preCheck
          unset EF2000_EXE
          PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -v
          objects/ef2000-unicorn-cpp --self-test | grep -q 'status=guest_exit interrupts=2'
          objects/ef2000-unicorn-cpp --self-test --joystick-event 0:0:0:0 \
            --joystick-axes=0,1 --trace-joystick | grep -q 'gameport: axes=2 reads=1 writes=1'
          objects/ef2000-unicorn-cpp --self-test-bda | grep -q 'status=guest_exit interrupts=5'
          objects/ef2000-unicorn-cpp --self-test-clock | grep -Eq 'status=wall_time_limit interrupts=0 ticks=[1-9].*ax=0x12345678'
          objects/ef2000-unicorn-cpp --self-test-retrace | grep -q 'status=guest_exit'
          objects/ef2000-unicorn-cpp --self-test-retrace --vga-clock-check 1 --block-clock-check 1 | grep -q 'status=guest_exit'
          objects/ef2000-unicorn-cpp --self-test-retrace --profile | grep -Eq 'profile: blocks=[0-9]+.*vga_reads=[1-9]'
          objects/ef2000-unicorn-cpp --self-test-fast-copy | grep -q 'fast_copy: calls=1 bytes=1024 fallback_calls=0'
          objects/ef2000-unicorn-cpp --self-test-fast-copy --profile-copy-callers | \
            grep -q 'copy_caller: ret=0xab calls=1 bytes=1024 max=1024 ge1024=1 ge4096=0 vga_calls=0 vga_bytes=0 accelerated=1 accelerated_bytes=1024'
          objects/ef2000-unicorn-cpp --self-test-fast-copy --fast-copy-vga | \
            grep -q 'fast_copy_vga: calls=1 bytes=1024'
          objects/ef2000-unicorn-cpp --self-test-clock --wall-seconds 0.1 --profile-hot-blocks | grep -Eq 'hot_blocks: samples=[1-9]'
          objects/ef2000-unicorn-cpp --self-test-clock --tcg-cache-mib 128 | grep -q 'status=wall_time_limit'
          objects/ef2000-dos-drive-test
          objects/ef2000-real-mode-test
          objects/ef2000-original-sb16-test
          objects/ef2000-cd-audio-test
          objects/ef2000-sb16-probe-test
          objects/ef2000-isa-audio-test
          objects/ef2000-sound-blaster-test
          objects/ef2000-pcm-audio-test
          objects/ef2000-wav-capture-test
          runHook postCheck
        '';

        installPhase = ''
          runHook preInstall
          mkdir -p "$out/bin" "$out/lib/ef2000" "$out/libexec/ef2000" \
            "$out/share/doc/ef2000" "$out/share/licenses/ef2000" \
            "$out/share/ef2000" "$out/share/applications" "$out/share/metainfo" \
            "$out/share/man/man6" "$out/share/icons/hicolor/scalable/apps"
          cp -r objects/native64 objects/qemu "$out/lib/ef2000/"
          cp objects/ef2000-unicorn-cpp "$out/bin/ef2000-unicorn-cpp"
          cp -r runtime tools probes "$out/libexec/ef2000/"
          cp VERSION "$out/libexec/ef2000/"
          cp -r docs README.md TODO.md VERSION "$out/share/doc/ef2000/"
          cp LICENSES/GPL-3.0-or-later.txt "$out/share/licenses/ef2000/"
          cp LICENSES/CC0-1.0.txt "$out/share/licenses/ef2000/"
          cp data/ef2000.desktop "$out/share/applications/ef2000.desktop"
          cp data/io.github.grumbel.ef2000.metainfo.xml \
            "$out/share/metainfo/io.github.grumbel.ef2000.metainfo.xml"
          cp data/ef2000.6 "$out/share/man/man6/ef2000.6"
          cp data/icons/ef2000.svg "$out/share/icons/hicolor/scalable/apps/ef2000.svg"
          cp data/icons/ef2000.bmp "$out/share/ef2000/ef2000.bmp"
          # Install each expected size explicitly so the layout check validates
          # the same paths the package promises to ship.
          for size in 16x16 32x32 48x48 64x64 128x128 256x256; do
            install -Dm644 "data/icons/hicolor/$size/apps/ef2000.png" \
              "$out/share/icons/hicolor/$size/apps/ef2000.png"
          done

          makeWrapper ${python}/bin/python3 "$out/bin/ef2000" \
            --add-flags "$out/libexec/ef2000/tools/run_startup.py" \
            --add-flags "--runtime-objects $out/lib/ef2000" \
            --set EF2000_VERSION ${lib.escapeShellArg version} \
            --set EF2000_SDL_LIBRARY ${pkgs.sdl3}/lib/libSDL3.so \
            --set EF2000_ICON_BMP "$out/share/ef2000/ef2000.bmp" \
            --prefix PATH : ${lib.makeBinPath [ pkgs.binutils pkgs.qemu-user ]}
          makeWrapper ${python}/bin/python3 "$out/bin/ef2000-unicorn-probe" \
            --add-flags "$out/libexec/ef2000/tools/unicorn_backend.py" \
            --set EF2000_SDL_LIBRARY ${pkgs.sdl3}/lib/libSDL3.so \
            --set EF2000_ICON_BMP "$out/share/ef2000/ef2000.bmp"
          makeWrapper ${python}/bin/python3 "$out/bin/ef2000-unicorn" \
            --add-flags "$out/libexec/ef2000/tools/unicorn_app.py" \
            --set EF2000_VERSION ${lib.escapeShellArg version} \
            --set EF2000_SDL_LIBRARY ${pkgs.sdl3}/lib/libSDL3.so \
            --set EF2000_ICON_BMP "$out/share/ef2000/ef2000.bmp"
          ln -s ef2000 "$out/bin/ef2000-startup"
          makeWrapper ${pkgs.python3}/bin/python3 "$out/bin/ef2000-inspect" \
            --add-flags "$out/libexec/ef2000/tools/inspect_exe.py"
          makeWrapper ${pkgs.python3}/bin/python3 "$out/bin/ef2000-extract" \
            --add-flags "$out/libexec/ef2000/tools/extract_image.py"
          makeWrapper ${pkgs.python3}/bin/python3 "$out/bin/ef2000-probe-entry" \
            --add-flags "$out/libexec/ef2000/tools/probe_entry.py" \
            --prefix PATH : ${lib.makeBinPath [ pkgs.qemu-user ]}
          makeWrapper ${pkgs.python3}/bin/python3 "$out/bin/ef2000-reference" \
            --add-flags "$out/libexec/ef2000/tools/reference_run.py" \
            --prefix PATH : ${lib.makeBinPath [ pkgs.binutils pkgs.dosbox-x ]}
          makeWrapper ${pkgs.python3}/bin/python3 "$out/bin/ef2000-read-x32-probe" \
            --add-flags "$out/libexec/ef2000/tools/read_x32_probe.py"
          runHook postInstall
        '';

        meta = {
          description = "Original-code EF2000 main-menu port";
          platforms = [ system ];
          mainProgram = "ef2000";
          license = lib.licenses.gpl3Plus;
        };
      };
      devCommand = name: script: pkgs.writeShellScriptBin name ''
        set -euo pipefail
        exec ${pkgs.bash}/bin/bash ${script} "$@"
      '';
      devCommands = [
        (devCommand "ef2000-configure" ./dev/configure.sh)
        (devCommand "ef2000-build" ./dev/build.sh)
        (devCommand "ef2000-run" ./dev/run.sh)
        (devCommand "ef2000-run-gdb" ./dev/run-gdb.sh)
      ];
      # The headless workflow is a separate tool: keep Ghidra's large runtime
      # out of the main game package closure. Its scripts are read from the
      # Nix source tree, while projects/exports default to the caller's cwd.
      ghidraWorkflow = pkgs.writeShellScriptBin "ef2000-ghidra" ''
        set -euo pipefail
        if [ -z "''${GHIDRA_HOME:-}" ]; then
          export GHIDRA_HOME=${lib.escapeShellArg ghidraHome}
        fi
        if [ -z "''${EF2000_GHIDRA_WORKSPACE:-}" ]; then
          export EF2000_GHIDRA_WORKSPACE="$PWD/local/ghidra"
        fi
        exec ${pkgs.python3}/bin/python3 ${source}/tools/ghidra_ef2000.py "$@"
      '';
    in {
      packages.${system} = {
        default = package;
        ef2000 = package;
        ef2000-startup = package;
        ef2000-ghidra = ghidraWorkflow;
        ef2000-win64 = win64.dist;
        ef2000-win64-zip = win64.zip;
        ef2000-win32 = win32.dist;
        ef2000-win32-zip = win32.zip;
      };
      apps.${system} = let
        native = {
          type = "app";
          program = "${package}/bin/ef2000";
          meta.description = "Run the original EF2000 game with the native Linux backend";
        };
        unicorn = {
          type = "app";
          program = "${package}/bin/ef2000-unicorn";
          meta.description = "Experiment with original EF2000 menu and flight using Unicorn and SDL3";
        };
        unicorn-cpp = {
          type = "app";
          program = "${package}/bin/ef2000-unicorn-cpp";
          meta.description = "Experiment with original EF2000 menu and flight in native C++ Unicorn/SDL3";
        };
        ghidra = {
          type = "app";
          program = "${pkgs.ghidra}/bin/ghidra";
          meta.description = "Open Ghidra to inspect the extracted EF2000 X-32 image";
        };
      in { default = native; ef2000 = native;
           inherit unicorn;
           inherit unicorn-cpp;
           inherit ghidra;
           ghidra-ef2000 = {
             type = "app";
             program = "${ghidraWorkflow}/bin/ef2000-ghidra";
             meta.description = "Import, audit, repair, and export the EF2000 Ghidra project";
           };
           ghidra-headless = {
             type = "app";
             program = "${ghidraHome}/support/analyzeHeadless";
             meta.description = "Run the Ghidra headless analyzer";
           };
           ef2000-unicorn = unicorn;
           unicorn-probe = {
             type = "app";
             program = "${package}/bin/ef2000-unicorn-probe";
             meta.description = "Probe the original EF2000 executable with portable Unicorn emulation";
           }; };
      checks.${system} = self.packages.${system} // {
        app-default = pkgs.runCommand "ef2000-app-check" {} ''
          test -x ${package}/bin/ef2000
          touch "$out"
        '';
        app-ef2000 = self.checks.${system}.app-default;
        app-unicorn = pkgs.runCommand "ef2000-unicorn-app-check" {} ''
          test -x ${package}/bin/ef2000-unicorn
          ${package}/bin/ef2000-unicorn --help > "$out"
          test "$(${package}/bin/ef2000-unicorn --version)" = ${lib.escapeShellArg "ef2000-unicorn ${version}"}
        '';
        app-unicorn-cpp = pkgs.runCommand "ef2000-unicorn-cpp-app-check" {} ''
          ${package}/bin/ef2000-unicorn-cpp --help > "$out"
          test -s "$out"
          ${package}/bin/ef2000-unicorn-cpp --self-test | grep -q 'status=guest_exit interrupts=2'
        '';
        ghidra-workflow = pkgs.runCommand "ef2000-ghidra-workflow-check" {
          nativeBuildInputs = [ pkgs.python3 ];
        } ''
          ${ghidraWorkflow}/bin/ef2000-ghidra --help > "$out"
          grep -q 'audit-flow-gaps' "$out"
          grep -q 'repair-boundaries' "$out"
          grep -q 'export-all' "$out"
          cd ${source}
          PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests \
            -p test_ghidra_ef2000.py -q
        '';
        windows-layout = pkgs.runCommand "ef2000-windows-layout-check" {
          nativeBuildInputs = [ pkgs.file ];
        } ''
          for target in win64 win32; do
            case "$target" in
              win64) dist=${win64.dist} ;;
              win32) dist=${win32.dist} ;;
            esac
            dir="$dist/ef2000-${version}-$target"
            test -s "$dir/ef2000-unicorn-cpp.exe"
            test -s "$dir/SDL3.dll"
            test -n "$(find "$dir" -maxdepth 1 -iname '*unicorn*.dll' -print -quit)"
            file "$dir/ef2000-unicorn-cpp.exe" | grep -q 'PE32'
          done
          touch "$out"
        '';
        help = pkgs.runCommand "ef2000-help-check" {} ''
          ${package}/bin/ef2000 --help > "$out"
        '';
        version = pkgs.runCommand "ef2000-version-check" {} ''
          test "$(${package}/bin/ef2000 --version)" = ${lib.escapeShellArg "ef2000 ${version}"}
          touch "$out"
        '';
        layout = pkgs.runCommand "ef2000-layout-check" {} ''
          for path in bin/ef2000 share/ef2000 share/applications/ef2000.desktop \
            share/metainfo/io.github.grumbel.ef2000.metainfo.xml share/man/man6/ef2000.6.gz \
            share/icons/hicolor/48x48/apps/ef2000.png; do
            if [ ! -e "${package}/$path" ]; then
              echo "Missing installed EF2000 path: $path" >&2
              exit 1
            fi
          done
          touch "$out"
        '';
        integration = pkgs.runCommand "ef2000-integration-check" {
          nativeBuildInputs = [ pkgs.desktop-file-utils pkgs.appstream ];
        } ''
          desktop-file-validate ${package}/share/applications/ef2000.desktop
          appstreamcli validate --no-net \
            ${package}/share/metainfo/io.github.grumbel.ef2000.metainfo.xml
          touch "$out"
        '';
        man = pkgs.runCommand "ef2000-man-check" {
          nativeBuildInputs = [ pkgs.gzip pkgs.mandoc ];
        } ''
          set -o pipefail
          gzip -cd ${package}/share/man/man6/ef2000.6.gz | mandoc -T lint
          touch "$out"
        '';
        reuse = pkgs.runCommand "ef2000-reuse-check" {
          nativeBuildInputs = [ pkgs.reuse ];
        } ''
          cd ${source}
          reuse lint
          touch "$out"
        '';
      };
      devShells.${system}.default = pkgs.mkShell {
        inputsFrom = [ package ];
        packages = [ python pkgs.ghidra ghidraWorkflow ] ++ devCommands ++ (with pkgs; [
          gcc binutils qemu-user dosbox-x cmake ninja gdb ccache
        ]);
        GHIDRA_HOME = ghidraHome;
        EF2000_SDL_LIBRARY = "${pkgs.sdl3}/lib/libSDL3.so";
        EF2000_VERSION = version;
        shellHook = ''
          export EF2000_SOURCE="$PWD"
          export EF2000_BUILD_DIR="''${EF2000_BUILD_DIR:-/tmp/ef2000-build}"
          export EF2000_ICON_BMP="$EF2000_SOURCE/data/icons/ef2000.bmp"
          printf '\nEF2000 development shell (Debug build, SDL3, Unicorn)\n'
          printf '  Source:    %s\n' "$EF2000_SOURCE"
          printf '  Build:     %s\n' "$EF2000_BUILD_DIR"
          printf '  Build type: %s (set CMAKE_BUILD_TYPE to override)\n\n' "''${CMAKE_BUILD_TYPE:-Debug}"
          printf 'Development commands:\n'
          printf '  ef2000-configure [CMAKE_ARGS]   Configure Ninja and link ./build for clangd\n'
          printf '  ef2000-build [BUILD_ARGS]       Configure if needed, then build\n'
          printf '  ef2000-run [GAME_ARGS]          Build and run the original DOS game on Linux\n'
          printf '  ef2000-run-gdb [GAME_ARGS]      Build and debug the native game ELF in GDB\n\n'
          printf 'Portable original-code emulator (experimental, slower than native):\n'
          printf '  nix run .#unicorn -- --data-dir /path/to/EF2000\n'
          printf '                                  Start windowed Unicorn/SDL3 game frontend\n'
          printf '  nix run .#unicorn-probe -- /path/to/_EF2000.EXE\n'
          printf '                                  Run bounded headless startup diagnostics\n\n'
          printf '  nix run .#unicorn-cpp -- /path/to/_EF2000.EXE --windowed\n'
          printf '                                  Native C++ Unicorn/SDL3 menu and flight frontend\n\n'
          printf 'Windows MinGW cross-builds (original game assets are not included):\n'
          printf '  nix build .#ef2000-win64-zip    Windows x64 executable plus SDL3/Unicorn DLLs\n'
          printf '  nix build .#ef2000-win32-zip    Windows x86 executable plus SDL3/Unicorn DLLs\n\n'
          printf '  python tools/unicorn_backend.py /path/to/_EF2000.EXE --windowed\n'
          printf '                                  Direct Unicorn backend for development\n\n'
          printf 'Ghidra reverse engineering (original executable stays external):\n'
          printf '  ef2000-ghidra import /path/to/_EF2000.EXE\n'
          printf '                                  Import SHA-pinned X-32 image once\n'
          printf '  ef2000-ghidra decompile 0xe9a24 0x8b274\n'
          printf '                                  Export targeted pseudo-C and incoming references\n'
          printf '  ef2000-ghidra export-all --custom-storage-fallback\n'
          printf '                                  Export all recognized functions to pseudo-C\n'
          printf '  ef2000-ghidra audit-flow-gaps\n'
          printf '                                  Audit suspicious fall-through decoding gaps\n'
          printf '  ef2000-ghidra audit-body-holes 0x9820\n'
          printf '                                  Inspect disconnected function-body spans\n'
          printf '  ef2000-ghidra repair-boundaries\n'
          printf '                                  Audit eight pinned function-body repairs (read-only)\n'
          printf '  nix run .#ghidra-ef2000 -- --help\n'
          printf '                                  Same workflow without entering the dev shell\n'
          printf '  ghidra                          Open Ghidra GUI\n\n'
          printf 'Examples:\n'
          printf '  ef2000-run --data-dir /path/to/EF2000 --windowed\n'
          printf '  ef2000-run-gdb --data-dir /path/to/EF2000 --verbose\n'
          printf '  ef2000-run --help               List game, display and diagnostic options\n'
          printf '  nix develop -c ef2000-run --data-dir /path/to/EF2000\n\n'
          printf 'The original game assets are external; all guest writes go to a run overlay.\n'
          printf 'Run logs and extracted startup.elf are saved in each run output directory.\n\n'
        '';
      };
    };
}
